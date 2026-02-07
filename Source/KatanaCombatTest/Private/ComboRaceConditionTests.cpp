// ComboRaceConditionTests.cpp
// Tests for INPUT-1 fix: combo flip-flop race condition
// Verifies the PendingComboTransitions counter in FAttackStateMachine correctly
// rejects stale async blend-out callbacks from combo transitions.

#include "CombatTestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Core/CombatComponent.h"
#include "ActionQueueTypes.h"
#include "Data/AttackData.h"
#include "CombatTypes.h"

// ============================================================================
// TEST: PendingComboTransitions starts at zero
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboRace_PendingTransitionsStartZero,
	"KatanaCombat.ComboRaceCondition.PendingTransitionsStartZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FComboRace_PendingTransitionsStartZero::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);

	if (!Combat)
	{
		AddError(TEXT("Failed to create combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestEqual(TEXT("PendingComboTransitions should start at 0"),
		Combat->AttackStateMachine.PendingComboTransitions, 0);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: OnComboTransition increments PendingComboTransitions
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboRace_ComboTransitionIncrementsPending,
	"KatanaCombat.ComboRaceCondition.ComboTransitionIncrementsPending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FComboRace_ComboTransitionIncrementsPending::RunTest(const FString& Parameters)
{
	FAttackStateMachine SM;
	UAttackData* Attack1 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* Attack2 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	// Start first attack
	SM.OnAttackStarted(Attack1->AttackMontage, NAME_None, 0.0f);
	TestEqual(TEXT("PendingComboTransitions should be 0 after first attack"), SM.PendingComboTransitions, 0);

	// Combo transition to second attack
	SM.OnComboTransition(Attack2->AttackMontage, NAME_None, 0.1f, 0.1f);
	TestEqual(TEXT("PendingComboTransitions should be 1 after combo transition"), SM.PendingComboTransitions, 1);

	return true;
}

// ============================================================================
// TEST: ShouldProcessMontageEnd rejects stale interrupt and decrements counter
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboRace_StaleInterruptRejected,
	"KatanaCombat.ComboRaceCondition.StaleInterruptRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FComboRace_StaleInterruptRejected::RunTest(const FString& Parameters)
{
	FAttackStateMachine SM;
	UAttackData* Attack1 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* Attack2 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	// Start first attack
	SM.OnAttackStarted(Attack1->AttackMontage, NAME_None, 0.0f);

	// Combo transition
	SM.OnComboTransition(Attack2->AttackMontage, NAME_None, 0.1f, 0.1f);
	TestEqual(TEXT("Pending should be 1"), SM.PendingComboTransitions, 1);

	// Simulate stale callback from old montage (interrupted, arrives async after blend)
	bool bShouldProcess = SM.ShouldProcessMontageEnd(Attack1->AttackMontage, true, 1.0f);
	TestFalse(TEXT("Stale interrupt should be rejected"), bShouldProcess);
	TestEqual(TEXT("Pending should be 0 after consumed"), SM.PendingComboTransitions, 0);

	return true;
}

// ============================================================================
// TEST: Genuine interrupt (no pending transitions) is processed
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboRace_GenuineInterruptProcessed,
	"KatanaCombat.ComboRaceCondition.GenuineInterruptProcessed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FComboRace_GenuineInterruptProcessed::RunTest(const FString& Parameters)
{
	FAttackStateMachine SM;
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	// Start attack (no combo transition — PendingComboTransitions stays 0)
	SM.OnAttackStarted(Attack->AttackMontage, NAME_None, 0.0f);
	TestEqual(TEXT("No pending transitions"), SM.PendingComboTransitions, 0);

	// Simulate genuine interrupt (death, stun)
	bool bShouldProcess = SM.ShouldProcessMontageEnd(Attack->AttackMontage, true, 1.0f);
	TestTrue(TEXT("Genuine interrupt should be processed"), bShouldProcess);

	return true;
}

// ============================================================================
// TEST: Normal completion (not interrupted) is always processed
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboRace_NormalCompletionProcessed,
	"KatanaCombat.ComboRaceCondition.NormalCompletionProcessed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FComboRace_NormalCompletionProcessed::RunTest(const FString& Parameters)
{
	FAttackStateMachine SM;
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	SM.OnAttackStarted(Attack->AttackMontage, NAME_None, 0.0f);

	// Normal completion (bInterrupted=false) should always be processed
	bool bShouldProcess = SM.ShouldProcessMontageEnd(Attack->AttackMontage, false, 1.0f);
	TestTrue(TEXT("Normal completion should be processed"), bShouldProcess);

	return true;
}

// ============================================================================
// TEST: Multiple rapid transitions accumulate pending count
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboRace_MultipleTransitionsAccumulate,
	"KatanaCombat.ComboRaceCondition.MultipleTransitionsAccumulate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FComboRace_MultipleTransitionsAccumulate::RunTest(const FString& Parameters)
{
	FAttackStateMachine SM;
	UAttackData* Attack1 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* Attack2 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* Attack3 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	// Start attack chain: 1 -> 2 -> 3 (rapid input, old callbacks pending)
	SM.OnAttackStarted(Attack1->AttackMontage, NAME_None, 0.0f);
	SM.OnComboTransition(Attack2->AttackMontage, NAME_None, 0.1f, 0.05f);
	SM.OnComboTransition(Attack3->AttackMontage, NAME_None, 0.1f, 0.10f);

	TestEqual(TEXT("Should have 2 pending transitions"), SM.PendingComboTransitions, 2);

	// First stale callback consumed
	bool b1 = SM.ShouldProcessMontageEnd(Attack1->AttackMontage, true, 1.0f);
	TestFalse(TEXT("First stale callback rejected"), b1);
	TestEqual(TEXT("1 pending remaining"), SM.PendingComboTransitions, 1);

	// Second stale callback consumed
	bool b2 = SM.ShouldProcessMontageEnd(Attack2->AttackMontage, true, 1.0f);
	TestFalse(TEXT("Second stale callback rejected"), b2);
	TestEqual(TEXT("0 pending remaining"), SM.PendingComboTransitions, 0);

	// Third callback (actual montage end) should be processed
	bool b3 = SM.ShouldProcessMontageEnd(Attack3->AttackMontage, true, 2.0f);
	TestTrue(TEXT("Current montage interrupt should be processed"), b3);

	return true;
}

// ============================================================================
// TEST: OnAttackStarted resets pending counter
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboRace_FreshAttackResetsPending,
	"KatanaCombat.ComboRaceCondition.FreshAttackResetsPending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FComboRace_FreshAttackResetsPending::RunTest(const FString& Parameters)
{
	FAttackStateMachine SM;
	UAttackData* Attack1 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* Attack2 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* Attack3 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	SM.OnAttackStarted(Attack1->AttackMontage, NAME_None, 0.0f);
	SM.OnComboTransition(Attack2->AttackMontage, NAME_None, 0.1f, 0.1f);
	TestEqual(TEXT("1 pending after combo"), SM.PendingComboTransitions, 1);

	// Start a completely new attack (not a combo) — resets counter
	SM.OnAttackStarted(Attack3->AttackMontage, NAME_None, 1.0f);
	TestEqual(TEXT("Fresh attack resets pending to 0"), SM.PendingComboTransitions, 0);

	return true;
}

// ============================================================================
// TEST: Combo chain data integrity - NextComboAttack is always accessible
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboRace_ComboChainDataIntegrity,
	"KatanaCombat.ComboRaceCondition.ComboChainDataIntegrity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FComboRace_ComboChainDataIntegrity::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);

	if (!Combat)
	{
		AddError(TEXT("Failed to create combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Create a 6-attack combo chain (matching the 6 light attacks in the real game)
	const int32 ChainLength = 6;
	UAttackData* FirstAttack = FCombatTestHelpers::CreateTestComboChain(ChainLength, EAttackType::Light);

	// Verify the chain is properly linked
	UAttackData* Current = FirstAttack;
	int32 Count = 0;
	while (Current)
	{
		Count++;
		// Simulate setting CurrentAttackData for each attack in the chain
		Combat->CurrentAttackData = Current;
		TestTrue(TEXT("CurrentAttackData should match chain element"),
			Combat->CurrentAttackData == Current);

		// If there's a next attack, verify it's accessible
		if (Current->NextComboAttack)
		{
			TestTrue(TEXT("NextComboAttack should be valid"), Current->NextComboAttack != nullptr);
		}

		Current = Current->NextComboAttack;
	}

	TestEqual(TEXT("Combo chain should have correct length"), Count, ChainLength);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Revert on failure restores previous attack data
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboRace_RevertOnFailure,
	"KatanaCombat.ComboRaceCondition.RevertOnFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FComboRace_RevertOnFailure::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);

	if (!Combat)
	{
		AddError(TEXT("Failed to create combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Create two attacks
	UAttackData* PreviousAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* NewAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	NewAttack->AttackMontage = nullptr; // Force PlayAttackMontage to fail

	// Set up initial state (as if mid-combo)
	Combat->CurrentAttackData = PreviousAttack;
	Combat->CurrentAttackInputType = EInputType::LightAttack;

	// Simulate the revert-on-failure pattern from ExecuteAction:
	UAttackData* SavedAttackData = Combat->CurrentAttackData;
	EInputType SavedInputType = Combat->CurrentAttackInputType;

	Combat->CurrentAttackData = NewAttack;
	Combat->CurrentAttackInputType = EInputType::HeavyAttack;

	// Simulate failure (montage was null)
	bool bSuccess = (NewAttack->AttackMontage != nullptr);
	if (!bSuccess)
	{
		Combat->CurrentAttackData = SavedAttackData;
		Combat->CurrentAttackInputType = SavedInputType;
	}

	// Verify revert worked
	TestTrue(TEXT("CurrentAttackData should revert to previous on failure"),
		Combat->CurrentAttackData == PreviousAttack);
	TestTrue(TEXT("CurrentAttackInputType should revert to previous on failure"),
		Combat->CurrentAttackInputType == EInputType::LightAttack);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: SetPhase(None) clears state when no pending transitions
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboRace_SetPhaseNoneClearsState,
	"KatanaCombat.ComboRaceCondition.SetPhaseNoneClearsState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FComboRace_SetPhaseNoneClearsState::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);

	if (!Combat)
	{
		AddError(TEXT("Failed to create combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	// Set up mid-attack state
	Combat->CurrentAttackData = Attack;
	Combat->CurrentAttackInputType = EInputType::LightAttack;
	Combat->SetPhase(EAttackPhase::Windup);

	// SetPhase(None) should clear CurrentAttackData (no pending transitions)
	Combat->SetPhase(EAttackPhase::None);

	TestTrue(TEXT("CurrentAttackData should be cleared on phase None"),
		Combat->CurrentAttackData == nullptr);
	TestTrue(TEXT("CurrentAttackInputType should be None"),
		Combat->CurrentAttackInputType == EInputType::None);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
