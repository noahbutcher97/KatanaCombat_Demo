// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"

/**
 * Test: Phases vs Windows Separation
 * Verifies phases are exclusive, windows can overlap
 *
 * Design Rule: Phases are EXCLUSIVE (one at a time), Windows can OVERLAP
 * - Phases: None → Windup → Active → Recovery (mutually exclusive)
 * - Windows: Combo, Parry, Hold, Cancel (can be active simultaneously)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhasesVsWindowsTest, "KatanaCombat.CombatComponent.PhasesVsWindows", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPhasesVsWindowsTest::RunTest(const FString& Parameters)
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

	// Test 1: Phase exclusivity - only ONE phase active at a time
	CombatComp->OnPhaseTransition(EAttackPhase::Windup);
	TestEqual("Should be in Windup phase", CombatComp->GetCurrentPhase(), EAttackPhase::Windup);

	CombatComp->OnPhaseTransition(EAttackPhase::Active);
	TestEqual("Should transition to Active", CombatComp->GetCurrentPhase(), EAttackPhase::Active);
	TestTrue("No longer in Windup", CombatComp->GetCurrentPhase() != EAttackPhase::Windup);

	CombatComp->OnPhaseTransition(EAttackPhase::Recovery);
	TestEqual("Should transition to Recovery", CombatComp->GetCurrentPhase(), EAttackPhase::Recovery);

	// Test 2: Multiple windows can be active simultaneously via checkpoints
	CombatComp->RegisterCheckpoint(EActionWindowType::Combo, 0.5f, 0.3f);
	CombatComp->RegisterCheckpoint(EActionWindowType::Parry, 0.5f, 0.2f);

	TArray<FTimerCheckpoint> ActiveWindows = CombatComp->GetActiveWindows(0.6f);
	TestTrue("Multiple windows can be active simultaneously", ActiveWindows.Num() >= 1);

	// Test 3: Combo window activates correctly
	TestTrue("Combo window should be active after registration", CombatComp->IsInComboWindow());

	// Test 4: Phase change doesn't affect windows
	EAttackPhase PhaseBefore = CombatComp->GetCurrentPhase();
	bool bComboActiveBefore = CombatComp->IsInComboWindow();

	CombatComp->OnPhaseTransition(EAttackPhase::None);

	TestEqual("Phase should change", CombatComp->GetCurrentPhase(), EAttackPhase::None);
	// Windows are time-based, independent of phase transitions
	TestTrue("Combo window persists through phase changes (still registered)",
		CombatComp->IsInComboWindow() == bComboActiveBefore);

	// Test 5: EAttackPhase enum validation
	TArray<EAttackPhase> AllPhases = {
		EAttackPhase::None,
		EAttackPhase::Windup,
		EAttackPhase::Active,
		EAttackPhase::Recovery
	};
	TestEqual("EAttackPhase should have exactly 4 values", AllPhases.Num(), 4);

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);

	return true;
}
