// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"

/**
 * Test: State Transition Validation
 * Verifies all valid and invalid state transitions in the combat state machine
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateTransitionTest, "KatanaCombat.CombatComponent.StateTransitions", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FStateTransitionTest::RunTest(const FString& Parameters)
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

	// Test 1: Idle → Attacking (valid)
	TestTrue("Can transition Idle → Attacking",
		CombatComp->CanTransitionTo(ECombatState::Attacking));

	// Test 2: Idle → Attacking → Idle (valid chain)
	CombatComp->SetCombatState(ECombatState::Attacking);
	TestEqual("Should be in Attacking state",
		CombatComp->GetCombatState(), ECombatState::Attacking);
	TestTrue("Can transition Attacking → Idle",
		CombatComp->CanTransitionTo(ECombatState::Idle));

	// Test 3: Dead → Attacking (invalid - terminal state)
	CombatComp->SetCombatState(ECombatState::Idle);
	CombatComp->SetCombatState(ECombatState::Dead);
	TestEqual("Should be in Dead state",
		CombatComp->GetCombatState(), ECombatState::Dead);
	TestFalse("Cannot transition Dead → Attacking (terminal state)",
		CombatComp->CanTransitionTo(ECombatState::Attacking));

	// Test 4: Blocking → Parrying (valid)
	CombatComp->ForceSetStateForTest(ECombatState::Idle);  // Force reset after Dead
	CombatComp->SetCombatState(ECombatState::Blocking);
	TestTrue("Can transition Blocking → Parrying",
		CombatComp->CanTransitionTo(ECombatState::Parrying));

	// Test 5: Dead is truly terminal (cannot transition to any other state)
	CombatComp->ForceSetStateForTest(ECombatState::Idle);  // Force reset
	CombatComp->SetCombatState(ECombatState::Dead);
	TestFalse("Cannot transition Dead → Idle",
		CombatComp->CanTransitionTo(ECombatState::Idle));
	TestFalse("Cannot transition Dead → Blocking",
		CombatComp->CanTransitionTo(ECombatState::Blocking));
	TestFalse("Cannot transition Dead → Evading",
		CombatComp->CanTransitionTo(ECombatState::Evading));

	// Test 6: Attacking → HoldingLightAttack (valid)
	CombatComp->ForceSetStateForTest(ECombatState::Idle);  // Force reset after Dead
	CombatComp->SetCombatState(ECombatState::Attacking);
	TestTrue("Can transition Attacking → HoldingLightAttack",
		CombatComp->CanTransitionTo(ECombatState::HoldingLightAttack));

	// Test 7: GuardBroken → Idle (valid recovery)
	CombatComp->ForceSetStateForTest(ECombatState::Idle);  // Force reset after Dead
	CombatComp->SetCombatState(ECombatState::GuardBroken);
	TestTrue("Can transition GuardBroken → Idle",
		CombatComp->CanTransitionTo(ECombatState::Idle));

	// Test 8: Cannot transition to same state
	CombatComp->ForceSetStateForTest(ECombatState::Idle);  // Force reset after GuardBroken
	TestFalse("Cannot transition to same state (Idle → Idle)",
		CombatComp->CanTransitionTo(ECombatState::Idle));

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);

	return true;
}