// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"

#include "Animation/AnimMontage.h"
#include "Core/CombatComponent.h"
#include "Data/AttackData.h"

namespace
{
FAnimNotifyRuntimeSourceId MakeNotifySource(UAnimMontage* Montage, int32 NotifyIndex = 0)
{
	FAnimNotifyRuntimeSourceId Source;
	Source.SourceAnimation = FSoftObjectPath(Montage);
	Source.NotifyEventIndex = NotifyIndex;
	return Source;
}

void SeedExactAttack(
	UCombatComponent* Combat,
	UAttackData* Attack,
	UAnimMontage* Montage,
	int32 AttackGeneration,
	int32 MontageInstanceId)
{
	Combat->SeedAttackWindowStateForTesting(
		Attack,
		EAttackPhase::Recovery,
		AttackGeneration,
		Montage,
		MontageInstanceId);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHoldCommitmentExactContextTest,
	"KatanaCombat.CombatInput.HoldCommitment.ExactNotifyContext",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldCommitmentExactContextTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	if (!TestNotNull(TEXT("Player should exist"), Player)
		|| !TestNotNull(TEXT("Combat component should exist"), Combat))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAnimMontage* Montage = NewObject<UAnimMontage>(Attack);
	Attack->AttackMontage = Montage;
	SeedExactAttack(Combat, Attack, Montage, 12, 44);
	Combat->HeldInputs.Add(EInputType::LightAttack, World->GetTimeSeconds());
	Combat->HeldInputSerials.Add(EInputType::LightAttack, 1001);
	const FAnimNotifyRuntimeSourceId NotifySource = MakeNotifySource(Montage);

	TestFalse(TEXT("Stale montage instance must not commit a hold"),
		Combat->OnHoldWindowStartWithContext(
			EInputType::LightAttack,
			NotifySource,
			43));
	TestFalse(TEXT("Rejected stale notify should leave hold inactive"), Combat->HoldState.IsHolding());

	TestTrue(TEXT("Exact active context should commit a hold"),
		Combat->OnHoldWindowStartWithContext(
			EInputType::LightAttack,
			NotifySource,
			44));
	TestEqual(TEXT("Hold should capture the active attack generation"),
		Combat->HoldState.CurrentHold.SourceAttackInstance.AttackGeneration, 12);
	TestEqual(TEXT("Hold should capture the montage instance"),
		Combat->HoldState.CurrentHold.MontageInstanceId, 44);
	TestEqual(TEXT("Hold should capture source attack data"),
		Combat->HoldState.CurrentHold.SourceAttackData.Get(), Attack);

	Combat->ClearHoldState();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHoldCommitmentRecoveryInputBuffersTest,
	"KatanaCombat.CombatInput.HoldCommitment.RecoveryInputCannotPreempt",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldCommitmentRecoveryInputBuffersTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	if (!TestNotNull(TEXT("Player should exist"), Player)
		|| !TestNotNull(TEXT("Combat component should exist"), Combat))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Combat->CurrentPhase = EAttackPhase::Recovery;
	Combat->HoldState.Activate(EInputType::LightAttack, World->GetTimeSeconds(), 0.1f);
	const int32 HoldGeneration = Combat->HoldState.CurrentHold.HoldID;
	UAttackData* PendingAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	FQueuedInputAction Input(EInputType::HeavyAttack, EInputEventType::Press, 1.0f, false);
	Combat->QueueAction(Input, PendingAttack);

	TestEqual(TEXT("Committed hold should prevent immediate execution"),
		Combat->ExecuteActionCallCountForTesting, 0);
	TestEqual(TEXT("Attack should occupy the one pending slot"), Combat->ActionQueue.Num(), 1);
	TestEqual(TEXT("Pending attack should retain its authored data"),
		Combat->ActionQueue[0].AttackData, PendingAttack);
	TestEqual(TEXT("Normal input must not replace the hold generation"),
		Combat->HoldState.CurrentHold.HoldID, HoldGeneration);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNormalAttackSlotReplacementTest,
	"KatanaCombat.CombatInput.NormalSlot.NewestEligibleInputWins",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FNormalAttackSlotReplacementTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	if (!TestNotNull(TEXT("Player should exist"), Player)
		|| !TestNotNull(TEXT("Combat component should exist"), Combat))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Combat->CurrentPhase = EAttackPhase::Active;
	UAttackData* FirstAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* WinningAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	FQueuedInputAction First(EInputType::LightAttack, EInputEventType::Press, 1.0f, false);
	First.FacingIntent.DesiredYaw = -45.0f;
	FQueuedInputAction Winner(EInputType::HeavyAttack, EInputEventType::Press, 2.0f, false);
	Winner.FacingIntent.DesiredYaw = 80.0f;
	Winner.FacingIntent.WorldDirection = FVector::RightVector;
	Winner.FacingIntent.bHasWorldDirection = true;
	Combat->QueueAction(First, FirstAttack);
	Combat->QueueAction(Winner, WinningAttack);

	TestEqual(TEXT("Normal slot should contain exactly one entry"), Combat->ActionQueue.Num(), 1);
	TestEqual(TEXT("Newest attack data should replace the old slot"),
		Combat->ActionQueue[0].AttackData, WinningAttack);
	TestEqual(TEXT("Newest facing yaw should remain immutable"),
		Combat->ActionQueue[0].InputAction.FacingIntent.DesiredYaw, 80.0f);
	TestTrue(TEXT("Newest world-facing direction should remain immutable"),
		Combat->ActionQueue[0].InputAction.FacingIntent.WorldDirection.Equals(FVector::RightVector));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNormalAttackSingleWinnerBoundaryTest,
	"KatanaCombat.CombatInput.NormalSlot.OneExecutionAttemptPerBoundary",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FNormalAttackSingleWinnerBoundaryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	if (!TestNotNull(TEXT("Player should exist"), Player)
		|| !TestNotNull(TEXT("Combat component should exist"), Combat))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	Attack->AttackMontage = nullptr;
	FQueuedInputAction First(EInputType::LightAttack, EInputEventType::Press, 1.0f, false);
	FQueuedInputAction Second(EInputType::HeavyAttack, EInputEventType::Press, 2.0f, false);
	FActionQueueEntry FirstEntry(First, Attack, EActionExecutionMode::Queued);
	FActionQueueEntry SecondEntry(Second, Attack, EActionExecutionMode::Queued);
	FirstEntry.TargetPhase = EAttackPhase::Recovery;
	SecondEntry.TargetPhase = EAttackPhase::Recovery;
	Combat->ActionQueue = {FirstEntry, SecondEntry};

	Combat->ProcessQueuedActions(EAttackPhase::Recovery);
	TestEqual(TEXT("One boundary should make exactly one execution attempt"),
		Combat->ExecuteActionCallCountForTesting, 1);
	TestEqual(TEXT("A reentrant or corrupt second entry must wait for another boundary"),
		Combat->ActionQueue.Num(), 1);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHoldOwnedHandoffPrecedesNormalSlotTest,
	"KatanaCombat.CombatInput.HoldCommitment.HoldOwnedHandoffPrecedesNormalSlot",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldOwnedHandoffPrecedesNormalSlotTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	if (!TestNotNull(TEXT("Player should exist"), Player)
		|| !TestNotNull(TEXT("Combat component should exist"), Combat))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* SourceAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* FollowUpAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* PendingAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	UAnimMontage* Montage = NewObject<UAnimMontage>(SourceAttack);
	SourceAttack->AttackMontage = Montage;
	FollowUpAttack->AttackMontage = nullptr;
	SourceAttack->DirectionalFollowUps.Add(EAttackDirection::Right, FollowUpAttack);
	SeedExactAttack(Combat, SourceAttack, Montage, 21, 77);

	FAttackInstanceId SourceInstance;
	SourceInstance.Attacker = Player;
	SourceInstance.AttackGeneration = 21;
	Combat->HoldState.Activate(
		EInputType::LightAttack,
		World->GetTimeSeconds(),
		0.1f,
		SourceInstance,
		SourceAttack,
		MakeNotifySource(Montage),
		77);
	Combat->HoldState.MarkHoldCompleted();
	FAttackFacingIntent ReleaseIntent;
	ReleaseIntent.BranchDirection = EInputDirection::Right;
	ReleaseIntent.WorldDirection = FVector::RightVector;
	ReleaseIntent.DesiredYaw = 90.0f;
	ReleaseIntent.bHasWorldDirection = true;
	const int32 HoldGeneration = Combat->HoldState.CurrentHold.HoldID;
	Combat->HoldState.BeginReleaseBlend(
		HoldGeneration,
		ReleaseIntent,
		EAttackDirection::Right);

	FQueuedInputAction PendingInput(EInputType::HeavyAttack, EInputEventType::Press, 3.0f, false);
	Combat->QueueAction(PendingInput, PendingAttack);
	TestEqual(TEXT("Normal pending slot should be populated before handoff"), Combat->ActionQueue.Num(), 1);

	TestFalse(TEXT("Fixture follow-up intentionally fails montage startup"),
		Combat->DispatchHoldOwnedFollowUp(HoldGeneration));
	TestEqual(TEXT("Hold-owned route should receive the first execution attempt"),
		Combat->HoldOwnedDispatchCountForTesting, 1);
	TestEqual(TEXT("Only the hold-owned continuation should be attempted"),
		Combat->ExecuteActionCallCountForTesting, 1);
	TestEqual(TEXT("Hold-owned dispatch must not consume the normal pending slot"),
		Combat->ActionQueue.Num(), 1);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHoldStaleEaseGenerationRejectedTest,
	"KatanaCombat.CombatInput.HoldCommitment.StaleEaseGenerationRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldStaleEaseGenerationRejectedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	if (!TestNotNull(TEXT("Player should exist"), Player)
		|| !TestNotNull(TEXT("Combat component should exist"), Combat))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Combat->HoldState.Activate(EInputType::LightAttack, World->GetTimeSeconds(), 0.5f);
	const int32 StaleGeneration = Combat->HoldState.CurrentHold.HoldID;
	Combat->HoldState.Deactivate();
	Combat->HoldState.bActivatedThisAttack = false;
	Combat->HoldState.Activate(EInputType::LightAttack, World->GetTimeSeconds(), 0.25f);
	Combat->HoldState.bIsEasing = true;
	const int32 CurrentGeneration = Combat->HoldState.CurrentHold.HoldID;

	Combat->OnEaseTimerTick(StaleGeneration);
	TestEqual(TEXT("Stale timer must not terminate the current hold"),
		Combat->HoldState.CurrentHold.HoldID, CurrentGeneration);
	TestTrue(TEXT("Current hold should remain active after stale timer callback"),
		Combat->HoldState.IsHolding());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
