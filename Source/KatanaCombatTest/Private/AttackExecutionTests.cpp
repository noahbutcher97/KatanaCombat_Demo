// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"
#include "Data/AttackData.h"

/**
 * Test: Queue Starts Empty
 * Verifies the action queue starts empty
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueStartsEmptyTest, "KatanaCombat.CombatComponent.Queue.StartsEmpty", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueueStartsEmptyTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Queue should start empty
	TestTrue("Queue should start empty", CombatComp->IsQueueEmpty());
	TestEqual("Queue size should be 0", CombatComp->GetQueueSize(), 0);

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: QueueAction - Adds Entry to Queue
 * Verifies QueueAction() correctly adds an entry to the action queue
 * Note: Actions are only queued during Windup/Active phases; otherwise they execute immediately
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueActionAddsEntryTest, "KatanaCombat.CombatComponent.Queue.AddEntry", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQueueActionAddsEntryTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Put component into Active phase so actions get queued instead of executing immediately
	// (During None/Recovery, actions execute immediately; during Windup/Active, they queue)
	CombatComp->OnPhaseTransition(EAttackPhase::Active);

	// Create test attack
	UAttackData* TestAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	// Queue an action using the constructor
	FQueuedInputAction InputAction(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);

	CombatComp->QueueAction(InputAction, TestAttack);

	// Verify queue is not empty after adding
	TestFalse("Queue should not be empty after adding action", CombatComp->IsQueueEmpty());
	TestEqual("Queue size should be 1", CombatComp->GetQueueSize(), 1);

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: ClearQueue - Empties Queue
 * Verifies ClearQueue() empties the action queue
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClearQueueEmptiesTest, "KatanaCombat.CombatComponent.Queue.ClearEmpties", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FClearQueueEmptiesTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Put component into Active phase so actions get queued instead of executing immediately
	CombatComp->OnPhaseTransition(EAttackPhase::Active);

	// Queue multiple actions
	UAttackData* TestAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	FQueuedInputAction InputAction(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);

	CombatComp->QueueAction(InputAction, TestAttack);
	CombatComp->QueueAction(InputAction, TestAttack);
	CombatComp->QueueAction(InputAction, TestAttack);

	TestFalse("Queue should not be empty after adding actions", CombatComp->IsQueueEmpty());
	TestEqual("Queue size should be 3", CombatComp->GetQueueSize(), 3);

	// Clear queue
	CombatComp->ClearQueue(false);

	TestTrue("Queue should be empty after clear", CombatComp->IsQueueEmpty());
	TestEqual("Queue size should be 0 after clear", CombatComp->GetQueueSize(), 0);

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: GetCurrentPhase - Default None
 * Verifies attack phase starts as None
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefaultPhaseNoneTest, "KatanaCombat.CombatComponent.Phase.DefaultNone", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefaultPhaseNoneTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Default phase should be None
	TestEqual("Default attack phase should be None",
		CombatComp->GetCurrentPhase(), EAttackPhase::None);

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: OnPhaseTransition - Updates Phase
 * Verifies phase transitions work correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhaseTransitionTest, "KatanaCombat.CombatComponent.Phase.Transition", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPhaseTransitionTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Test phase transitions
	CombatComp->OnPhaseTransition(EAttackPhase::Windup);
	TestEqual("Should transition to Windup", CombatComp->GetCurrentPhase(), EAttackPhase::Windup);

	CombatComp->OnPhaseTransition(EAttackPhase::Active);
	TestEqual("Should transition to Active", CombatComp->GetCurrentPhase(), EAttackPhase::Active);

	CombatComp->OnPhaseTransition(EAttackPhase::Recovery);
	TestEqual("Should transition to Recovery", CombatComp->GetCurrentPhase(), EAttackPhase::Recovery);

	CombatComp->OnPhaseTransition(EAttackPhase::None);
	TestEqual("Should transition to None", CombatComp->GetCurrentPhase(), EAttackPhase::None);

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: GetCurrentAttack - Null When Not Attacking
 * Verifies current attack is null when idle
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoCurrentAttackWhenIdleTest, "KatanaCombat.CombatComponent.Attack.NullWhenIdle", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FNoCurrentAttackWhenIdleTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Current attack should be null when idle
	TestNull("Current attack should be null when idle", CombatComp->GetCurrentAttack());
	TestFalse("IsAttacking should be false when idle", CombatComp->IsAttacking());

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: IsInComboWindow - Default False
 * Verifies combo window is not active by default
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboWindowDefaultFalseTest, "KatanaCombat.CombatComponent.ComboWindow.DefaultFalse", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FComboWindowDefaultFalseTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Combo window should not be active by default
	TestFalse("Combo window should not be active by default", CombatComp->IsInComboWindow());

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: RegisterCheckpoint - Activates Combo Window
 * Verifies checkpoint registration activates combo window
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRegisterCheckpointActivatesComboTest, "KatanaCombat.CombatComponent.Checkpoint.ActivatesCombo", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRegisterCheckpointActivatesComboTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Register a combo checkpoint
	CombatComp->RegisterCheckpoint(EActionWindowType::Combo, 0.5f, 0.3f);

	// Combo window should now be active
	TestTrue("Combo window should be active after registering combo checkpoint", CombatComp->IsInComboWindow());

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: GetActiveWindows - Returns Correct Windows
 * Verifies GetActiveWindows filters windows by time
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGetActiveWindowsTest, "KatanaCombat.CombatComponent.Checkpoint.GetActiveWindows", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGetActiveWindowsTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Register a checkpoint at 0.5s with 0.3s duration (active from 0.5 to 0.8)
	CombatComp->RegisterCheckpoint(EActionWindowType::Combo, 0.5f, 0.3f);

	// At 0.3s - should have no active windows (before checkpoint starts)
	TArray<FTimerCheckpoint> ActiveAt03 = CombatComp->GetActiveWindows(0.3f);
	TestEqual("Should have no active windows at 0.3s", ActiveAt03.Num(), 0);

	// At 0.6s - should have active combo window
	TArray<FTimerCheckpoint> ActiveAt06 = CombatComp->GetActiveWindows(0.6f);
	TestTrue("Should have active window at 0.6s", ActiveAt06.Num() > 0);

	// At 0.9s - should have no active windows (after checkpoint ends)
	TArray<FTimerCheckpoint> ActiveAt09 = CombatComp->GetActiveWindows(0.9f);
	TestEqual("Should have no active windows at 0.9s", ActiveAt09.Num(), 0);

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Pending Action Count - Starts at Zero
 * Verifies GetPendingActionCount returns zero initially
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPendingCountStartsZeroTest, "KatanaCombat.CombatComponent.Queue.PendingCountStartsZero", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPendingCountStartsZeroTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Pending action count should be 0 initially
	TestEqual("Pending action count should start at 0",
		CombatComp->GetPendingActionCount(), 0);

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: GetActionQueue - Returns Queue Reference
 * Verifies GetActionQueue returns accessible queue
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGetActionQueueTest, "KatanaCombat.CombatComponent.Queue.GetActionQueue", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGetActionQueueTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Queue starts empty
	const TArray<FActionQueueEntry>& Queue = CombatComp->GetActionQueue();
	TestEqual("Queue should start empty", Queue.Num(), 0);

	// Put component into Active phase so actions get queued instead of executing immediately
	CombatComp->OnPhaseTransition(EAttackPhase::Active);

	// Add an action
	UAttackData* TestAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	FQueuedInputAction InputAction(EInputType::LightAttack, EInputEventType::Press, 0.0f, false);
	CombatComp->QueueAction(InputAction, TestAttack);

	// Queue should now have one entry
	TestEqual("Queue should have 1 entry after adding", CombatComp->GetActionQueue().Num(), 1);

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
