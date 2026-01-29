// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"

/**
 * Test: Memory Safety - Null CurrentAttack
 * Verifies system handles null CurrentAttack gracefully
 *
 * Tests:
 * - GetCurrentAttack() returns null when idle
 * - Hold state operations don't crash with null attack
 * - Phase transitions are safe when no attack is active
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemorySafetyTest, "KatanaCombat.CombatComponent.MemorySafety", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMemorySafetyTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Test 1: GetCurrentAttack returns null when idle
	TestNull("GetCurrentAttack should be null when idle", CombatComp->GetCurrentAttack());

	// Test 2: IsAttacking returns false when idle
	TestFalse("IsAttacking should be false when idle", CombatComp->IsAttacking());

	// Test 3: Hold operations don't crash without active attack
	CombatComp->ActivateHold(EInputType::HeavyAttack, 0.5f);
	TestTrue("ActivateHold should succeed even without active attack", CombatComp->IsHolding());

	// Test 4: Phase transitions don't crash without attack
	CombatComp->OnPhaseTransition(EAttackPhase::Active);
	TestEqual("Phase should transition safely", CombatComp->GetCurrentPhase(), EAttackPhase::Active);

	CombatComp->OnPhaseTransition(EAttackPhase::None);
	TestEqual("Phase should return to None", CombatComp->GetCurrentPhase(), EAttackPhase::None);

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);

	return true;
}
