// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"

/**
 * Test: Memory Safety - Null CurrentAttackData
 * Verifies system handles null CurrentAttackData gracefully
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemorySafetyTest, "KatanaCombat.CombatComponent.MemorySafety", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMemorySafetyTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ACharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Test 1: ReleaseHeldLight with null CurrentAttackData (should not crash)
	CombatComp->bIsHolding = true;
	CombatComp->CurrentAttackData = nullptr;

	CombatComp->ReleaseHeldLight(false);

	TestEqual("Should return to Idle safely",
		CombatComp->GetCombatState(), ECombatState::Idle);
	TestFalse("Should exit hold state",
		CombatComp->IsHolding());

	// Test 2: ReleaseHeldHeavy with null CurrentAttackData (should not crash)
	CombatComp->bIsHolding = true;
	CombatComp->CurrentAttackData = nullptr;

	CombatComp->ReleaseHeldHeavy(false);

	TestEqual("Should return to Idle safely",
		CombatComp->GetCombatState(), ECombatState::Idle);
	TestFalse("Should exit hold state",
		CombatComp->IsHolding());

	// Test 3: OpenHoldWindow with null CurrentAttackData (should not crash)
	CombatComp->SetCombatState(ECombatState::Attacking);
	CombatComp->CurrentAttackData = nullptr;

	CombatComp->OpenHoldWindow(0.5f);

	TestFalse("Should not enter hold with null attack",
		CombatComp->IsHolding());

	// Test 4: UpdateHoldTime with null CurrentAttackData (should not crash)
	CombatComp->bIsHolding = true;
	CombatComp->CurrentAttackData = nullptr;

	// Should not crash
	CombatComp->UpdateHoldTime(0.016f);

	TestTrue("UpdateHoldTime should handle null gracefully", true);

	// Test 5: GetCurrentAttack returns null safely
	CombatComp->CurrentAttackData = nullptr;

	UAttackData* CurrentAttack = CombatComp->GetCurrentAttack();

	TestNull("GetCurrentAttack should return null when no attack",
		CurrentAttack);

	// Test 6: Combo execution with valid attack data (baseline - should work)
	CombatComp->SetCombatState(ECombatState::Idle);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	bool bSuccess = CombatComp->ExecuteAttack(Attack);

	TestTrue("Should execute attack successfully", bSuccess);
	TestNotNull("CurrentAttackData should be set",
		CombatComp->GetCurrentAttack());

	// Test 7: Clearing attack data mid-combo doesn't crash hold release
	CombatComp->SetCombatState(ECombatState::Idle);
	UAttackData* HoldableAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	HoldableAttack->bCanHold = true;

	CombatComp->OnLightAttackPressed();
	CombatComp->ExecuteAttack(HoldableAttack);
	CombatComp->CurrentAttackInputType = EInputType::LightAttack;
	CombatComp->OpenHoldWindow(0.5f);

	// Simulate montage ending during hold (clears CurrentAttackData)
	CombatComp->CurrentAttackData = nullptr;

	// Should not crash when releasing
	CombatComp->OnLightAttackReleased();

	TestEqual("Should handle null gracefully and return to Idle",
		CombatComp->GetCombatState(), ECombatState::Idle);

	// Test 8: Multiple null checks in sequence
	CombatComp->CurrentAttackData = nullptr;
	CombatComp->bIsHolding = true;

	CombatComp->ReleaseHeldLight(false);
	CombatComp->ReleaseHeldHeavy(false);
	CombatComp->UpdateHoldTime(0.016f);

	TestTrue("Multiple operations with null should not crash", true);

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);

	return true;
}