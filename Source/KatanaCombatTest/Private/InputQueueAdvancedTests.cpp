// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"
#include "Data/AttackData.h"
#include "ActionQueueTypes.h"

/**
 * Advanced Input Queue and Buffering Tests
 *
 * Test Coverage:
 * - Press/Release event pairing
 * - Queue operations (add, cancel, clear, priority)
 * - Hold state management and transitions
 * - Directional input buffering
 * - Execution mode determination by phase
 * - Phase transition queue processing
 * - Edge cases and concurrent input handling
 * - Queue statistics and debugging
 */

// ============================================================================
// PRESS/RELEASE EVENT TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputEventPressReleaseTest,
	"KatanaCombat.InputQueue.PressRelease.BasicPairing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInputEventPressReleaseTest::RunTest(const FString& Parameters)
{
	// Test FQueuedInputAction press/release identification
	FQueuedInputAction PressEvent(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);
	FQueuedInputAction ReleaseEvent(EInputType::LightAttack, EInputEventType::Release, 0.5f, false);

	TestTrue("Press event IsPress() should return true", PressEvent.IsPress());
	TestFalse("Press event IsRelease() should return false", PressEvent.IsRelease());
	TestFalse("Release event IsPress() should return false", ReleaseEvent.IsPress());
	TestTrue("Release event IsRelease() should return true", ReleaseEvent.IsRelease());

	// Test timestamp ordering
	TestTrue("Release should come after press", ReleaseEvent.Timestamp > PressEvent.Timestamp);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputEventComboWindowFlagTest,
	"KatanaCombat.InputQueue.PressRelease.ComboWindowFlag",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInputEventComboWindowFlagTest::RunTest(const FString& Parameters)
{
	// Test combo window flag propagation
	FQueuedInputAction InCombo(EInputType::LightAttack, EInputEventType::Press, 1.0f, true);
	FQueuedInputAction OutOfCombo(EInputType::LightAttack, EInputEventType::Press, 2.0f, false);

	TestTrue("In combo window flag should be true", InCombo.bInComboWindow);
	TestFalse("Out of combo window flag should be false", OutOfCombo.bInComboWindow);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputEventInputTypeTest,
	"KatanaCombat.InputQueue.PressRelease.InputTypes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInputEventInputTypeTest::RunTest(const FString& Parameters)
{
	// Test all input types
	FQueuedInputAction LightInput(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);
	FQueuedInputAction HeavyInput(EInputType::HeavyAttack, EInputEventType::Press, 0.0f, false);
	FQueuedInputAction EvadeInput(EInputType::Evade, EInputEventType::Press, 0.0f, false);
	FQueuedInputAction BlockInput(EInputType::Block, EInputEventType::Press, 0.0f, false);

	TestEqual("Light attack input type", LightInput.InputType, EInputType::LightAttack);
	TestEqual("Heavy attack input type", HeavyInput.InputType, EInputType::HeavyAttack);
	TestEqual("Evade input type", EvadeInput.InputType, EInputType::Evade);
	TestEqual("Block input type", BlockInput.InputType, EInputType::Block);

	return true;
}

// ============================================================================
// QUEUE OPERATIONS TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueBasicOperationsTest,
	"KatanaCombat.InputQueue.Operations.BasicAddClear",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueueBasicOperationsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Set to Active phase so actions get queued
	CombatComp->OnPhaseTransition(EAttackPhase::Active);

	UAttackData* TestAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	// Test empty state
	TestTrue("Queue should start empty", CombatComp->IsQueueEmpty());
	TestEqual("Empty queue size should be 0", CombatComp->GetQueueSize(), 0);

	// Test add
	FQueuedInputAction Action1(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);
	CombatComp->QueueAction(Action1, TestAttack);

	TestFalse("Queue should not be empty after add", CombatComp->IsQueueEmpty());
	TestEqual("Queue size should be 1", CombatComp->GetQueueSize(), 1);

	// Test multiple adds
	FQueuedInputAction Action2(EInputType::HeavyAttack, EInputEventType::Press, 0.1f, false);
	FQueuedInputAction Action3(EInputType::LightAttack, EInputEventType::Press, 0.2f, false);
	CombatComp->QueueAction(Action2, TestAttack);
	CombatComp->QueueAction(Action3, TestAttack);

	TestEqual("Queue size should be 3", CombatComp->GetQueueSize(), 3);

	// Test clear
	CombatComp->ClearQueue(false);
	TestTrue("Queue should be empty after clear", CombatComp->IsQueueEmpty());
	TestEqual("Queue size should be 0 after clear", CombatComp->GetQueueSize(), 0);

	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueuePriorityTest,
	"KatanaCombat.InputQueue.Operations.Priority",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueuePriorityTest::RunTest(const FString& Parameters)
{
	// Test FActionQueueEntry priority cancellation
	FQueuedInputAction Input(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);

	FActionQueueEntry LowPriority(Input, nullptr, EActionExecutionMode::Queued, 1);
	FActionQueueEntry HighPriority(Input, nullptr, EActionExecutionMode::Queued, 5);
	FActionQueueEntry SamePriority(Input, nullptr, EActionExecutionMode::Queued, 1);

	// Test cancellation rules
	TestTrue("Higher priority can cancel lower", LowPriority.CanBeCancelledBy(HighPriority));
	TestTrue("Same priority can cancel (replaces)", LowPriority.CanBeCancelledBy(SamePriority));
	TestFalse("Lower priority cannot cancel higher", HighPriority.CanBeCancelledBy(LowPriority));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueActionStateTest,
	"KatanaCombat.InputQueue.Operations.ActionState",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueueActionStateTest::RunTest(const FString& Parameters)
{
	FQueuedInputAction Input(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);
	FActionQueueEntry Entry(Input, nullptr, EActionExecutionMode::Queued, 0);

	// Default state
	TestEqual("Default state should be Pending", Entry.State, EActionState::Pending);
	TestTrue("IsPending should return true", Entry.IsPending());
	TestFalse("IsExecuting should return false initially", Entry.IsExecuting());

	// State transitions
	Entry.State = EActionState::Executing;
	TestFalse("IsPending should return false when Executing", Entry.IsPending());
	TestTrue("IsExecuting should return true", Entry.IsExecuting());

	Entry.State = EActionState::Completed;
	TestFalse("IsPending should return false when Completed", Entry.IsPending());
	TestFalse("IsExecuting should return false when Completed", Entry.IsExecuting());

	Entry.State = EActionState::Cancelled;
	TestFalse("IsPending should return false when Cancelled", Entry.IsPending());
	TestFalse("IsExecuting should return false when Cancelled", Entry.IsExecuting());

	return true;
}

// ============================================================================
// HOLD STATE TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldStateActivationTest,
	"KatanaCombat.InputQueue.Hold.Activation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldStateActivationTest::RunTest(const FString& Parameters)
{
	FHoldState HoldState;

	// Initial state
	TestFalse("Should not be holding initially", HoldState.IsHolding());
	TestFalse("Should not be completed initially", HoldState.IsHoldCompleted());
	TestEqual("Initial playrate should be 1.0", HoldState.CurrentPlayRate, 1.0f);

	// Activate hold
	HoldState.Activate(EInputType::LightAttack, 1.0f, 0.5f);

	TestTrue("Should be holding after activation", HoldState.IsHolding());
	TestFalse("Should not be completed immediately", HoldState.IsHoldCompleted());
	TestEqual("Held input type should match", HoldState.GetHeldInputType(), EInputType::LightAttack);
	TestEqual("Playrate should be set", HoldState.CurrentPlayRate, 0.5f);
	TestTrue("bActivatedThisAttack should be true", HoldState.bActivatedThisAttack);

	// Hold ID should be assigned
	TestTrue("Hold ID should be > 0", HoldState.CurrentHold.HoldID > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldStateCompletionTest,
	"KatanaCombat.InputQueue.Hold.Completion",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldStateCompletionTest::RunTest(const FString& Parameters)
{
	FHoldState HoldState;
	HoldState.Activate(EInputType::HeavyAttack, 0.0f, 1.0f);

	// Mark completed
	HoldState.MarkHoldCompleted();

	TestTrue("IsHoldCompleted should return true", HoldState.IsHoldCompleted());
	TestTrue("CurrentHold.bCompleted should be true", HoldState.CurrentHold.bCompleted);
	TestTrue("Should still be holding", HoldState.IsHolding());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldStateDeactivationTest,
	"KatanaCombat.InputQueue.Hold.Deactivation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldStateDeactivationTest::RunTest(const FString& Parameters)
{
	FHoldState HoldState;
	HoldState.Activate(EInputType::LightAttack, 0.0f, 0.25f);
	HoldState.bIsEasing = true;
	HoldState.EaseStartTime = 0.5f;

	// Deactivate
	HoldState.Deactivate();

	TestFalse("Should not be holding after deactivate", HoldState.IsHolding());
	TestEqual("Playrate should reset to 1.0", HoldState.CurrentPlayRate, 1.0f);
	TestFalse("bIsEasing should be false", HoldState.bIsEasing);
	TestEqual("EaseStartTime should be 0", HoldState.EaseStartTime, 0.0f);

	// bActivatedThisAttack persists until Reset()
	TestTrue("bActivatedThisAttack should persist after Deactivate", HoldState.bActivatedThisAttack);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldStateResetTest,
	"KatanaCombat.InputQueue.Hold.Reset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldStateResetTest::RunTest(const FString& Parameters)
{
	FHoldState HoldState;
	HoldState.Activate(EInputType::LightAttack, 0.0f, 0.5f);

	// Reset for new attack chain
	HoldState.Reset();

	TestFalse("Should not be holding after reset", HoldState.IsHolding());
	TestFalse("bActivatedThisAttack should be false after Reset", HoldState.bActivatedThisAttack);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldStateDurationTest,
	"KatanaCombat.InputQueue.Hold.Duration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldStateDurationTest::RunTest(const FString& Parameters)
{
	FHoldState HoldState;

	// No hold active
	TestEqual("Duration should be 0 when not holding", HoldState.GetHoldDuration(5.0f), 0.0f);

	// Activate at time 2.0
	HoldState.Activate(EInputType::LightAttack, 2.0f, 1.0f);

	// Check duration at various times
	TestEqual("Duration at start", HoldState.GetHoldDuration(2.0f), 0.0f);
	TestEqual("Duration after 1 second", HoldState.GetHoldDuration(3.0f), 1.0f);
	TestEqual("Duration after 2.5 seconds", HoldState.GetHoldDuration(4.5f), 2.5f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldEventValidityTest,
	"KatanaCombat.InputQueue.Hold.EventValidity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldEventValidityTest::RunTest(const FString& Parameters)
{
	FHoldState HoldState;

	// Activate - should get HoldID = 1
	HoldState.Activate(EInputType::LightAttack, 0.0f, 1.0f);
	int32 FirstID = HoldState.CurrentHold.HoldID;

	TestTrue("First hold should be valid with correct ID", HoldState.CurrentHold.IsValid(FirstID));
	TestFalse("First hold should not be valid with wrong ID", HoldState.CurrentHold.IsValid(999));

	// Deactivate and reactivate - should get new ID
	HoldState.Deactivate();
	HoldState.Activate(EInputType::HeavyAttack, 1.0f, 1.0f);
	int32 SecondID = HoldState.CurrentHold.HoldID;

	TestTrue("Second ID should be different", SecondID != FirstID);
	TestTrue("Second hold should be valid with new ID", HoldState.CurrentHold.IsValid(SecondID));
	TestFalse("Second hold should not be valid with old ID", HoldState.CurrentHold.IsValid(FirstID));

	return true;
}

// ============================================================================
// DIRECTIONAL INPUT BUFFER TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectionalBufferCaptureTest,
	"KatanaCombat.InputQueue.Directional.Capture",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDirectionalBufferCaptureTest::RunTest(const FString& Parameters)
{
	FDirectionalInputBuffer Buffer;

	// Initial state
	TestFalse("Should not have valid input initially", Buffer.HasValidInput());
	TestEqual("Initial direction should be None", Buffer.DirectionAtRelease, EInputDirection::None);

	// Capture direction
	Buffer.CaptureDirection(EInputDirection::Forward, 1.5f);

	TestTrue("Should have valid input after capture", Buffer.HasValidInput());
	TestEqual("Direction should be Forward", Buffer.DirectionAtRelease, EInputDirection::Forward);
	TestEqual("Capture time should be set", Buffer.CaptureTime, 1.5f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectionalBufferResetTest,
	"KatanaCombat.InputQueue.Directional.Reset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDirectionalBufferResetTest::RunTest(const FString& Parameters)
{
	FDirectionalInputBuffer Buffer;
	Buffer.CaptureDirection(EInputDirection::Left, 2.0f);

	// Reset
	Buffer.Reset();

	TestFalse("Should not have valid input after reset", Buffer.HasValidInput());
	TestEqual("Direction should be None after reset", Buffer.DirectionAtRelease, EInputDirection::None);
	TestEqual("Capture time should be 0 after reset", Buffer.CaptureTime, 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectionalBufferAllDirectionsTest,
	"KatanaCombat.InputQueue.Directional.AllDirections",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDirectionalBufferAllDirectionsTest::RunTest(const FString& Parameters)
{
	FDirectionalInputBuffer Buffer;

	// Test all directions
	TArray<EInputDirection> Directions = {
		EInputDirection::None,
		EInputDirection::Forward,
		EInputDirection::Backward,
		EInputDirection::Left,
		EInputDirection::Right
	};

	for (EInputDirection Dir : Directions)
	{
		Buffer.CaptureDirection(Dir, 0.0f);
		TestEqual(FString::Printf(TEXT("Direction %d should match"), (int32)Dir),
			Buffer.DirectionAtRelease, Dir);

		if (Dir != EInputDirection::None)
		{
			TestTrue(FString::Printf(TEXT("Direction %d should be valid"), (int32)Dir),
				Buffer.HasValidInput());
		}
	}

	return true;
}

// ============================================================================
// EXECUTION MODE TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExecutionModeQueuedTest,
	"KatanaCombat.InputQueue.ExecutionMode.Queued",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FExecutionModeQueuedTest::RunTest(const FString& Parameters)
{
	FQueuedInputAction Input(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);
	FActionQueueEntry Entry(Input, nullptr, EActionExecutionMode::Queued, 0);

	TestEqual("Execution mode should be Queued", Entry.ExecutionMode, EActionExecutionMode::Queued);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExecutionModeImmediateTest,
	"KatanaCombat.InputQueue.ExecutionMode.Immediate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FExecutionModeImmediateTest::RunTest(const FString& Parameters)
{
	FQueuedInputAction Input(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);
	FActionQueueEntry Entry(Input, nullptr, EActionExecutionMode::Immediate, 0);

	TestEqual("Execution mode should be Immediate", Entry.ExecutionMode, EActionExecutionMode::Immediate);

	return true;
}

// ============================================================================
// TIMER CHECKPOINT TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimerCheckpointCreationTest,
	"KatanaCombat.InputQueue.Checkpoint.Creation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTimerCheckpointCreationTest::RunTest(const FString& Parameters)
{
	FTimerCheckpoint Combo(EActionWindowType::Combo, 0.5f, 0.3f);

	TestEqual("Window type should be Combo", Combo.WindowType, EActionWindowType::Combo);
	TestEqual("Montage time should be 0.5", Combo.MontageTime, 0.5f);
	TestEqual("Duration should be 0.3", Combo.Duration, 0.3f);
	TestFalse("Should not be active by default", Combo.bActive);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimerCheckpointWindowTypesTest,
	"KatanaCombat.InputQueue.Checkpoint.WindowTypes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTimerCheckpointWindowTypesTest::RunTest(const FString& Parameters)
{
	// Test all window types can be created
	FTimerCheckpoint Combo(EActionWindowType::Combo, 0.0f, 0.1f);
	FTimerCheckpoint Parry(EActionWindowType::Parry, 0.0f, 0.1f);
	FTimerCheckpoint Counter(EActionWindowType::Counter, 0.0f, 0.1f);
	FTimerCheckpoint Cancel(EActionWindowType::Cancel, 0.0f, 0.1f);
	FTimerCheckpoint Hold(EActionWindowType::Hold, 0.0f, 0.1f);
	FTimerCheckpoint Recovery(EActionWindowType::Recovery, 0.0f, 0.1f);

	TestEqual("Combo type", Combo.WindowType, EActionWindowType::Combo);
	TestEqual("Parry type", Parry.WindowType, EActionWindowType::Parry);
	TestEqual("Counter type", Counter.WindowType, EActionWindowType::Counter);
	TestEqual("Cancel type", Cancel.WindowType, EActionWindowType::Cancel);
	TestEqual("Hold type", Hold.WindowType, EActionWindowType::Hold);
	TestEqual("Recovery type", Recovery.WindowType, EActionWindowType::Recovery);

	return true;
}

// ============================================================================
// QUEUE STATISTICS TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueStatsResetTest,
	"KatanaCombat.InputQueue.Stats.Reset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueueStatsResetTest::RunTest(const FString& Parameters)
{
	FQueueStats Stats;
	Stats.TotalInputs = 10;
	Stats.ActionsExecuted = 5;
	Stats.ActionsCancelled = 2;
	Stats.QueuedExecutions = 6;
	Stats.ImmediateExecutions = 4;

	Stats.Reset();

	TestEqual("TotalInputs should be 0", Stats.TotalInputs, 0);
	TestEqual("ActionsExecuted should be 0", Stats.ActionsExecuted, 0);
	TestEqual("ActionsCancelled should be 0", Stats.ActionsCancelled, 0);
	TestEqual("QueuedExecutions should be 0", Stats.QueuedExecutions, 0);
	TestEqual("ImmediateExecutions should be 0", Stats.ImmediateExecutions, 0);

	return true;
}

// ============================================================================
// PHASE TRANSITION QUEUE PROCESSING TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueProcessingNonePhaseTest,
	"KatanaCombat.InputQueue.Phase.NonePhase",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueueProcessingNonePhaseTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Phase None - actions should execute immediately, not queue
	CombatComp->OnPhaseTransition(EAttackPhase::None);

	// Note: In None phase, input goes through full execution path
	// For this test we just verify the phase is set correctly
	TestEqual("Phase should be None", CombatComp->GetCurrentPhase(), EAttackPhase::None);

	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueProcessingActivePhaseTest,
	"KatanaCombat.InputQueue.Phase.ActivePhase",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueueProcessingActivePhaseTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Phase Active - actions should be queued
	CombatComp->OnPhaseTransition(EAttackPhase::Active);

	UAttackData* TestAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	FQueuedInputAction Action(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);
	CombatComp->QueueAction(Action, TestAttack);

	TestFalse("Queue should not be empty during Active", CombatComp->IsQueueEmpty());
	TestEqual("Queue size should be 1", CombatComp->GetQueueSize(), 1);

	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueProcessingRecoveryPhaseTest,
	"KatanaCombat.InputQueue.Phase.RecoveryPhase",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueueProcessingRecoveryPhaseTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Phase Recovery - verify transition
	CombatComp->OnPhaseTransition(EAttackPhase::Recovery);
	TestEqual("Phase should be Recovery", CombatComp->GetCurrentPhase(), EAttackPhase::Recovery);

	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// CONCURRENT INPUT HANDLING TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueLastInputWinsTest,
	"KatanaCombat.InputQueue.Concurrent.LastInputWins",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueueLastInputWinsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Set to Active so inputs queue
	CombatComp->OnPhaseTransition(EAttackPhase::Active);

	UAttackData* LightAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* HeavyAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);

	// Queue multiple inputs in rapid succession
	FQueuedInputAction Light1(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);
	FQueuedInputAction Heavy1(EInputType::HeavyAttack, EInputEventType::Press, 0.01f, false);
	FQueuedInputAction Light2(EInputType::LightAttack, EInputEventType::Press, 0.02f, false);

	CombatComp->QueueAction(Light1, LightAttack);
	CombatComp->QueueAction(Heavy1, HeavyAttack);
	CombatComp->QueueAction(Light2, LightAttack);

	// All should be queued (last-input-wins happens at execution)
	TestEqual("Queue should have all inputs", CombatComp->GetQueueSize(), 3);

	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueRapidInputTest,
	"KatanaCombat.InputQueue.Concurrent.RapidInput",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueueRapidInputTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	CombatComp->OnPhaseTransition(EAttackPhase::Active);
	UAttackData* TestAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	// Simulate button mashing - many inputs in short time
	for (int32 i = 0; i < 10; ++i)
	{
		FQueuedInputAction Action(EInputType::LightAttack, EInputEventType::Press,
			static_cast<float>(i) * 0.01f, false);
		CombatComp->QueueAction(Action, TestAttack);
	}

	// Queue should handle rapid input without crash
	TestTrue("Queue size should be > 0 after rapid input", CombatComp->GetQueueSize() > 0);

	// Clear should handle full queue
	CombatComp->ClearQueue(false);
	TestTrue("Queue should be empty after clear", CombatComp->IsQueueEmpty());

	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueNullAttackDataTest,
	"KatanaCombat.InputQueue.EdgeCase.NullAttackData",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueueNullAttackDataTest::RunTest(const FString& Parameters)
{
	// Test FActionQueueEntry with null attack data
	FQueuedInputAction Input(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);
	FActionQueueEntry Entry(Input, nullptr, EActionExecutionMode::Queued, 0);

	TestTrue("Entry with null AttackData should be valid", Entry.InputAction.InputType == EInputType::LightAttack);
	TestTrue("AttackData should be nullptr", Entry.AttackData == nullptr);
	TestTrue("Entry should still be pending", Entry.IsPending());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueZeroTimestampTest,
	"KatanaCombat.InputQueue.EdgeCase.ZeroTimestamp",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueueZeroTimestampTest::RunTest(const FString& Parameters)
{
	// Test input at time 0
	FQueuedInputAction Action(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);

	TestEqual("Timestamp should be 0", Action.Timestamp, 0.0f);
	TestTrue("Action should still be valid", Action.InputType == EInputType::LightAttack);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueNegativeTimestampTest,
	"KatanaCombat.InputQueue.EdgeCase.NegativeTimestamp",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueueNegativeTimestampTest::RunTest(const FString& Parameters)
{
	// Test input with negative timestamp (edge case)
	FQueuedInputAction Action(EInputType::LightAttack, EInputEventType::Press, -1.0f, false);

	TestEqual("Negative timestamp should be preserved", Action.Timestamp, -1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldStateNoHoldDurationTest,
	"KatanaCombat.InputQueue.EdgeCase.NoHoldDuration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldStateNoHoldDurationTest::RunTest(const FString& Parameters)
{
	FHoldState HoldState;

	// Duration when not holding
	float Duration = HoldState.GetHoldDuration(10.0f);
	TestEqual("Duration should be 0 when not holding", Duration, 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldEventInvalidIDTest,
	"KatanaCombat.InputQueue.EdgeCase.HoldInvalidID",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldEventInvalidIDTest::RunTest(const FString& Parameters)
{
	FHoldEvent Event;

	// Default event has ID 0 - should be invalid
	TestFalse("Default event should not be valid with ID 0", Event.IsValid(0));
	TestFalse("Default event should not be valid with any ID", Event.IsValid(1));

	return true;
}

// ============================================================================
// INPUT CONTEXT TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputContextEnumTest,
	"KatanaCombat.InputQueue.Context.EnumValues",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInputContextEnumTest::RunTest(const FString& Parameters)
{
	// Test all input context values exist
	EInputContext Movement = EInputContext::Movement;
	EInputContext Directional = EInputContext::DirectionalInput;
	EInputContext Disabled = EInputContext::Disabled;

	TestTrue("Movement context should be distinct",
		Movement != Directional && Movement != Disabled);
	TestTrue("DirectionalInput context should be distinct",
		Directional != Movement && Directional != Disabled);
	TestTrue("Disabled context should be distinct",
		Disabled != Movement && Disabled != Directional);

	return true;
}
