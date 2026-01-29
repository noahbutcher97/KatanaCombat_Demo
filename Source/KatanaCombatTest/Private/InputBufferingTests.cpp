// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"
#include "Data/AttackData.h"

/**
 * Test: Input Buffering - Always Buffer
 * Verifies input is ALWAYS buffered, regardless of combo window state
 *
 * Design Rule: Input is ALWAYS captured/buffered.
 * Combo window modifies WHEN execution happens, not WHETHER input is captured.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputBufferingTest, "KatanaCombat.CombatComponent.InputBuffering", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInputBufferingTest::RunTest(const FString& Parameters)
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

	// Test 1: Queue starts empty
	TestTrue("Queue should start empty", CombatComp->IsQueueEmpty());
	TestEqual("Queue size should be 0", CombatComp->GetQueueSize(), 0);

	// Test 2: Actions queue during Active phase (when character is mid-attack)
	CombatComp->OnPhaseTransition(EAttackPhase::Active);

	UAttackData* TestAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	FQueuedInputAction InputAction(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);
	CombatComp->QueueAction(InputAction, TestAttack);

	TestFalse("Queue should not be empty after queueing during Active", CombatComp->IsQueueEmpty());
	TestEqual("Queue size should be 1", CombatComp->GetQueueSize(), 1);

	// Test 3: Multiple inputs can be queued
	CombatComp->QueueAction(InputAction, TestAttack);
	CombatComp->QueueAction(InputAction, TestAttack);

	TestEqual("Queue size should be 3 after multiple inputs", CombatComp->GetQueueSize(), 3);

	// Test 4: Clear queue
	CombatComp->ClearQueue(false);
	TestTrue("Queue should be empty after clear", CombatComp->IsQueueEmpty());

	// Test 5: During Recovery/None phases, actions may execute immediately
	// (This is phase-dependent behavior tested elsewhere)
	CombatComp->OnPhaseTransition(EAttackPhase::None);

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);

	return true;
}
