// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"

/**
 * Test: Phase Transition Validation
 * Verifies phase transitions in the combat system
 *
 * NOTE: The original state machine (ECombatState) has been removed.
 * The combat system now uses phases (EAttackPhase) for attack progression:
 * - None → Windup → Active → Recovery → None
 *
 * See AttackExecutionTests.cpp for phase transition tests.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStateTransitionTest, "KatanaCombat.CombatComponent.StateTransitions", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FStateTransitionTest::RunTest(const FString& Parameters)
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

	// Combat system uses phases, not state machine transitions
	// See AttackExecutionTests.cpp for phase transition coverage:
	// - FDefaultPhaseNoneTest: Verifies default phase is None
	// - FPhaseTransitionTest: Tests Windup → Active → Recovery → None
	AddInfo("State machine tests deprecated - phase transition tests in AttackExecutionTests.cpp");

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);

	return true;
}
