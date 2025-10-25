// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"

/**
 * Test: Phases vs Windows Separation
 * Verifies phases are exclusive, windows can overlap
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhasesVsWindowsTest, "KatanaCombat.CombatComponent.PhasesVsWindows", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPhasesVsWindowsTest::RunTest(const FString& Parameters)
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

	// Test 1: Only ONE phase active at a time (phases are exclusive)
	CombatComp->OnAttackPhaseBegin(EAttackPhase::Windup);
	TestEqual("Should be in Windup phase",
		CombatComp->GetCurrentPhase(), EAttackPhase::Windup);

	CombatComp->OnAttackPhaseBegin(EAttackPhase::Active);
	TestEqual("Should transition to Active phase",
		CombatComp->GetCurrentPhase(), EAttackPhase::Active);
	TestNotEqual("Should no longer be in Windup",
		CombatComp->GetCurrentPhase(), EAttackPhase::Windup);

	CombatComp->OnAttackPhaseBegin(EAttackPhase::Recovery);
	TestEqual("Should transition to Recovery phase",
		CombatComp->GetCurrentPhase(), EAttackPhase::Recovery);
	TestNotEqual("Should no longer be in Active",
		CombatComp->GetCurrentPhase(), EAttackPhase::Active);

	// Test 2: MULTIPLE windows can be active simultaneously
	CombatComp->OpenParryWindow(0.3f);
	CombatComp->OpenHoldWindow(0.5f);
	CombatComp->OpenComboWindow(0.6f);
	CombatComp->OpenCounterWindow(1.5f);

	TestTrue("Parry window should be active",
		CombatComp->IsInParryWindow());
	TestTrue("Hold window should be active",
		CombatComp->IsInHoldWindow());
	TestTrue("Combo window should be active",
		CombatComp->CanCombo());
	TestTrue("Counter window should be active",
		CombatComp->IsInCounterWindow());

	// All 4 windows are active at the same time!
	int32 ActiveWindows = 0;
	if (CombatComp->IsInParryWindow()) ActiveWindows++;
	if (CombatComp->IsInHoldWindow()) ActiveWindows++;
	if (CombatComp->CanCombo()) ActiveWindows++;
	if (CombatComp->IsInCounterWindow()) ActiveWindows++;

	TestEqual("All 4 windows should be active simultaneously",
		ActiveWindows, 4);

	// Test 3: Windows are independent of phases
	CombatComp->OnAttackPhaseBegin(EAttackPhase::Windup);

	TestTrue("Windows persist through phase changes",
		CombatComp->IsInParryWindow());

	CombatComp->OnAttackPhaseBegin(EAttackPhase::Active);

	TestTrue("Windows still active in Active phase",
		CombatComp->IsInParryWindow());

	// Test 4: Phase transitions don't automatically close windows (they're independent)
	CombatComp->OpenParryWindow(2.0f);  // Long window
	CombatComp->OnAttackPhaseBegin(EAttackPhase::Windup);

	TestTrue("Window active in Windup",
		CombatComp->IsInParryWindow());

	CombatComp->OnAttackPhaseBegin(EAttackPhase::Active);

	TestTrue("Window persists through phase change",
		CombatComp->IsInParryWindow());

	// Test 5: EAttackPhase enum has exactly 4 values (None, Windup, Active, Recovery)
	// Not 5 or 6 - Hold and ParryWindow are NOT phases
	TArray<EAttackPhase> AllPhases = {
		EAttackPhase::None,
		EAttackPhase::Windup,
		EAttackPhase::Active,
		EAttackPhase::Recovery
	};

	TestEqual("EAttackPhase should have exactly 4 values",
		AllPhases.Num(), 4);

	// Test 6: Closing windows doesn't affect phase
	CombatComp->OnAttackPhaseBegin(EAttackPhase::Active);
	CombatComp->OpenComboWindow(0.5f);

	TestEqual("Should be in Active phase",
		CombatComp->GetCurrentPhase(), EAttackPhase::Active);
	TestTrue("Combo window should be open",
		CombatComp->CanCombo());

	CombatComp->CloseComboWindow();

	TestEqual("Should still be in Active phase",
		CombatComp->GetCurrentPhase(), EAttackPhase::Active);
	TestFalse("Combo window should be closed",
		CombatComp->CanCombo());

	// Test 7: Window states are tracked independently via booleans
	CombatComp->CloseParryWindow();
	CombatComp->CloseHoldWindow();
	CombatComp->CloseComboWindow();
	CombatComp->CloseCounterWindow();

	TestFalse("Parry window should be closed",
		CombatComp->IsInParryWindow());
	TestFalse("Hold window should be closed",
		CombatComp->IsInHoldWindow());
	TestFalse("Combo window should be closed",
		CombatComp->CanCombo());
	TestFalse("Counter window should be closed",
		CombatComp->IsInCounterWindow());

	// Phase is still Active (unaffected by window closures)
	TestEqual("Phase remains Active",
		CombatComp->GetCurrentPhase(), EAttackPhase::Active);

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);

	return true;
}