// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Animation/AnimMontage.h"
#include "Core/CombatComponent.h"
#include "CombatTestHelpers.h"
#include "Data/AttackConfiguration.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Data/WeaponData.h"
#include "Debug/ActionReactionTelemetry.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"

namespace ActionReactionTelemetryTests
{
class FScopedConsoleInt
{
public:
	FScopedConsoleInt(const TCHAR* Name, const int32 NewValue)
	{
		Variable = IConsoleManager::Get().FindConsoleVariable(Name);
		if (Variable)
		{
			PreviousValue = Variable->GetInt();
			Variable->SetWithCurrentPriority(NewValue);
		}
	}

	~FScopedConsoleInt()
	{
		if (Variable)
		{
			Variable->SetWithCurrentPriority(PreviousValue);
		}
	}

	bool IsValid() const { return Variable != nullptr; }

private:
	IConsoleVariable* Variable = nullptr;
	int32 PreviousValue = 0;
};

int32 CountCsvFields(const FString& Line)
{
	bool bQuoted = false;
	int32 FieldCount = 1;
	for (int32 Index = 0; Index < Line.Len(); ++Index)
	{
		if (Line[Index] == TEXT('"'))
		{
			if (bQuoted && Index + 1 < Line.Len() && Line[Index + 1] == TEXT('"'))
			{
				++Index;
				continue;
			}
			bQuoted = !bQuoted;
		}
		else if (Line[Index] == TEXT(',') && !bQuoted)
		{
			++FieldCount;
		}
	}
	return FieldCount;
}

int32 CountMovementDecisionRecords(
	const TArray<FActionReactionTelemetryRecord>& Records)
{
	int32 Count = 0;
	for (const FActionReactionTelemetryRecord& Record : Records)
	{
		Count += Record.Event ==
			EActionReactionTelemetryEvent::MovementInputDecisionChanged ? 1 : 0;
	}
	return Count;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryBoundedBufferTest,
	"KatanaCombat.ActionReaction.Telemetry.BoundedBuffer",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryBoundedBufferTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FActionReactionTelemetryBuffer Buffer(3);
	FActionReactionTelemetryRecord Source;
	Source.Event = EActionReactionTelemetryEvent::InputCaptured;
	Source.Reason = EActionReactionTelemetryReason::Captured;
	Source.InputSerial = 41;

	for (int32 Index = 0; Index < 5; ++Index)
	{
		Source.SimulationTimestamp = Index;
		Buffer.Append(Source);
	}

	TestEqual(TEXT("Buffer should retain its configured capacity"), Buffer.GetRecords().Num(), 3);
	TestEqual(TEXT("Source record should not be mutated"), Source.Sequence, uint64(0));
	if (Buffer.GetRecords().Num() == 3)
	{
		TestEqual(TEXT("Oldest retained sequence should reflect eviction"),
			Buffer.GetRecords()[0].Sequence, uint64(3));
		TestEqual(TEXT("Newest sequence should remain monotonic"),
			Buffer.GetRecords().Last().Sequence, uint64(5));
		TestEqual(TEXT("Newest payload should be retained"),
			Buffer.GetRecords().Last().SimulationTimestamp, 4.0);
	}

	Buffer.Reset();
	TestEqual(TEXT("Reset should clear retained records"), Buffer.GetRecords().Num(), 0);
	Buffer.Append(Source);
	TestEqual(TEXT("Reset should restart the local sequence"),
		Buffer.GetRecords().Last().Sequence, uint64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryComponentStorageTest,
	"KatanaCombat.ActionReaction.Telemetry.ComponentStorage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryComponentStorageTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ActionReactionTelemetryTests;

	UCombatComponent* Combat = NewObject<UCombatComponent>();
	TestNotNull(TEXT("Combat component should be created"), Combat);
	if (!Combat)
	{
		return false;
	}

	FScopedConsoleInt ActionDebug(TEXT("Combat.ActionReaction.Debug"), 0);
	FScopedConsoleInt MasterDebug(TEXT("Combat.Debug.All"), 0);
	FActionReactionTelemetryRecord Record;
	Record.Event = EActionReactionTelemetryEvent::InputCaptured;
	Combat->AppendActionReactionTelemetry(Record);
	TestEqual(TEXT("Disabled telemetry should not retain records"),
		Combat->GetActionReactionTelemetry().Num(), 0);

	{
		FScopedConsoleInt Enabled(TEXT("Combat.ActionReaction.Debug"), 1);
		Combat->AppendActionReactionTelemetry(Record);
		TestEqual(TEXT("Enabled telemetry should retain a record"),
			Combat->GetActionReactionTelemetry().Num(), 1);
		if (Combat->GetActionReactionTelemetry().Num() == 1)
		{
			TestTrue(TEXT("Component storage should assign actor identity"),
				Combat->GetActionReactionTelemetry()[0].ActorStableId.IsValid());
		}
	}

	Combat->ClearActionReactionTelemetry();
	TestEqual(TEXT("Clear should empty component storage"),
		Combat->GetActionReactionTelemetry().Num(), 0);

	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* OwnerCombat = nullptr;
	APlayerCharacter* Owner = FCombatTestHelpers::CreateTestCharacterWithCombat(World, OwnerCombat);
	AEnemyCharacter* OtherActor = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UCombatComponent* OtherCombat = OtherActor ? OtherActor->CombatComponent.Get() : nullptr;
	if (OwnerCombat && Owner && OtherActor && OtherCombat)
	{
		FScopedConsoleInt Enabled(TEXT("Combat.ActionReaction.Debug"), 1);
		FActionReactionTelemetryRecord CrossActorRecord;
		CrossActorRecord.Actor = OtherActor;
		CrossActorRecord.Counterpart = Owner;
		OwnerCombat->AppendActionReactionTelemetry(CrossActorRecord);
	}
	if (OwnerCombat && OtherCombat && OwnerCombat->GetActionReactionTelemetry().Num() == 1)
	{
		const FActionReactionTelemetryRecord& Stored =
			OwnerCombat->GetActionReactionTelemetry()[0];
		TestEqual(TEXT("Explicit actor identity should not inherit the sink identity"),
			Stored.ActorStableId, OtherCombat->GetCombatantStableId());
		TestEqual(TEXT("Counterpart identity should resolve independently"),
			Stored.CounterpartStableId, OwnerCombat->GetCombatantStableId());
	}
	else
	{
		AddError(TEXT("Cross-actor telemetry fixture was not created"));
	}
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryRejectedInputCorrelationTest,
	"KatanaCombat.ActionReaction.Telemetry.RejectedInputCorrelation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryRejectedInputCorrelationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ActionReactionTelemetryTests;

	FScopedConsoleInt ActionDebug(TEXT("Combat.ActionReaction.Debug"), 1);
	FScopedConsoleInt MasterDebug(TEXT("Combat.Debug.All"), 0);
	UCombatComponent* Combat = NewObject<UCombatComponent>();
	Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press, EInputDirection::None);

	const TArray<FActionReactionTelemetryRecord>& Records = Combat->GetActionReactionTelemetry();
	TestEqual(TEXT("Rejected input should emit capture and terminal decision records"), Records.Num(), 2);
	if (Records.Num() == 2)
	{
		TestEqual(TEXT("First record should capture the physical edge"),
			Records[0].Event, EActionReactionTelemetryEvent::InputCaptured);
		TestEqual(TEXT("Second record should finalize the edge"),
			Records[1].Event, EActionReactionTelemetryEvent::InputFinalized);
		TestTrue(TEXT("Input identity should be nonzero and correlated"),
			Records[0].InputSerial != 0 && Records[0].InputSerial == Records[1].InputSerial);
		TestEqual(TEXT("Missing settings should have a closed rejection reason"),
			Records[1].Reason, EActionReactionTelemetryReason::MissingCombatSettings);
		TestEqual(TEXT("Terminal decision should preserve rejection"),
			Records[1].InputDisposition, ECombatInputDisposition::Rejected);
		TestTrue(TEXT("Both records should identify the combatant"),
			Records[0].ActorStableId.IsValid()
			&& Records[0].ActorStableId == Records[1].ActorStableId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryQueueIdentityTest,
	"KatanaCombat.ActionReaction.Telemetry.QueueIdentity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryQueueIdentityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ActionReactionTelemetryTests;

	FScopedConsoleInt ActionDebug(TEXT("Combat.ActionReaction.Debug"), 1);
	FScopedConsoleInt MasterDebug(TEXT("Combat.Debug.All"), 0);
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	TestNotNull(TEXT("Player should be created"), Player);
	TestNotNull(TEXT("Combat component should be created"), Combat);

	UAttackData* Attack = NewObject<UAttackData>();
	Player->CombatSettings->DefaultWeaponData->AttackConfiguration->DefaultLightAttack = Attack;
	Combat->SetPhase(EAttackPhase::Windup);
	Combat->ClearActionReactionTelemetry();
	Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press, EInputDirection::Forward);

	TestEqual(TEXT("Windup input should produce one pending queue entry"), Combat->GetActionQueue().Num(), 1);
	if (Combat->GetActionQueue().Num() == 1 && !Combat->GetCombatInputHistory().IsEmpty())
	{
		const FActionQueueEntry& Entry = Combat->GetActionQueue()[0];
		const uint64 CapturedSerial = Combat->GetCombatInputHistory().Last().Serial;
		TestTrue(TEXT("Queue entry identity should be assigned"), Entry.QueueEntryId != 0);
		TestEqual(TEXT("Physical input identity should survive queue construction"),
			Entry.InputAction.InputSerial, CapturedSerial);

		const FActionReactionTelemetryRecord* QueueRecord =
			Combat->GetActionReactionTelemetry().FindByPredicate(
				[&Entry](const FActionReactionTelemetryRecord& Record)
				{
					return Record.Event == EActionReactionTelemetryEvent::QueueAccepted
						&& Record.QueueEntryId == Entry.QueueEntryId;
				});
		TestNotNull(TEXT("Accepted queue entry should have a correlated telemetry record"), QueueRecord);
		if (QueueRecord)
		{
			TestEqual(TEXT("Queue record should retain physical input identity"),
				QueueRecord->InputSerial, CapturedSerial);
			TestEqual(TEXT("Queue record should identify queued execution"),
				QueueRecord->ExecutionMode, EActionExecutionMode::Queued);
		}
		const FActionReactionTelemetryRecord* Finalized =
			Combat->GetActionReactionTelemetry().FindByPredicate(
				[CapturedSerial](const FActionReactionTelemetryRecord& Record)
				{
					return Record.Event == EActionReactionTelemetryEvent::InputFinalized
						&& Record.InputSerial == CapturedSerial;
				});
		TestNotNull(TEXT("Queued input should have a terminal routing record"), Finalized);
		if (Finalized)
		{
			TestEqual(TEXT("Queued input should retain its exact routing reason"),
				Finalized->Reason, EActionReactionTelemetryReason::Queued);
		}
	}

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryLifecycleCorrelationTest,
	"KatanaCombat.ActionReaction.Telemetry.LifecycleCorrelation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryLifecycleCorrelationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ActionReactionTelemetryTests;

	FScopedConsoleInt ActionDebug(TEXT("Combat.ActionReaction.Debug"), 1);
	FScopedConsoleInt MasterDebug(TEXT("Combat.Debug.All"), 0);
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	TestNotNull(TEXT("Player should be created"), Player);
	TestNotNull(TEXT("Combat component should be created"), Combat);
	if (!Player || !Combat)
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* Attack = NewObject<UAttackData>();
	Attack->AttackType = EAttackType::Light;
	Attack->AttackMontage = NewObject<UAnimMontage>(Attack);
	Attack->HoldTargetPlayRate = 0.0f;
	Attack->HoldEaseInDuration = 1.0f;
	Combat->SeedAttackWindowStateForTesting(
		Attack,
		EAttackPhase::None,
		17,
		Attack->AttackMontage,
		91);
	Combat->HeldInputs.Add(EInputType::LightAttack, World->GetTimeSeconds());
	constexpr uint64 PressInputSerial = 77;
	Combat->HeldInputSerials.Add(EInputType::LightAttack, PressInputSerial);
	Combat->ClearActionReactionTelemetry();

	FAnimNotifyRuntimeSourceId HoldNotifySource;
	HoldNotifySource.SourceAnimation = FSoftObjectPath(Attack->AttackMontage.Get());
	HoldNotifySource.NotifyEventIndex = 0;
	TestTrue(TEXT("Exact hold context should commit"),
		Combat->OnHoldWindowStartWithContext(
			EInputType::LightAttack,
			HoldNotifySource,
			91));
	Combat->OnPhaseTransition(EAttackPhase::Windup);
	const int32 HoldGeneration = Combat->HoldState.CurrentHold.HoldID;
	Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Release);
	const uint64 ReleaseInputSerial = Combat->GetCombatInputHistory().Last().Serial;
	Combat->OnPhaseTransition(EAttackPhase::None);

	const TArray<FActionReactionTelemetryRecord>& Records = Combat->GetActionReactionTelemetry();
	const FActionReactionTelemetryRecord* HoldActivated = nullptr;
	const FActionReactionTelemetryRecord* HoldReleased = nullptr;
	const FActionReactionTelemetryRecord* TerminalReset = nullptr;
	int32 ContextChanges = 0;
	int32 PhaseChanges = 0;
	int32 MovementStateChanges = 0;
	for (const FActionReactionTelemetryRecord& Record : Records)
	{
		if (Record.Event == EActionReactionTelemetryEvent::HoldStateChanged
			&& Record.Reason == EActionReactionTelemetryReason::HoldActivated)
		{
			HoldActivated = &Record;
		}
		else if (Record.Event == EActionReactionTelemetryEvent::HoldStateChanged
			&& Record.Reason == EActionReactionTelemetryReason::HoldReleased)
		{
			HoldReleased = &Record;
		}
		else if (Record.Event == EActionReactionTelemetryEvent::TerminalReset)
		{
			TerminalReset = &Record;
		}

		ContextChanges += Record.Event == EActionReactionTelemetryEvent::InputContextChanged ? 1 : 0;
		PhaseChanges += Record.Event == EActionReactionTelemetryEvent::PhaseChanged ? 1 : 0;
		MovementStateChanges += Record.Event == EActionReactionTelemetryEvent::MovementStateChanged ? 1 : 0;
	}

	TestTrue(TEXT("Real hold path should assign a generation"), HoldGeneration > 0);
	TestNotNull(TEXT("Hold activation should be observable"), HoldActivated);
	TestNotNull(TEXT("Terminal cleanup should preserve the released hold generation"), HoldReleased);
	if (HoldActivated && HoldReleased)
	{
		TestEqual(TEXT("Hold lifecycle records should correlate"),
			HoldReleased->HoldGeneration, HoldActivated->HoldGeneration);
		TestEqual(TEXT("Hold telemetry should use the runtime generation"),
			HoldActivated->HoldGeneration, HoldGeneration);
		TestEqual(TEXT("Hold activation should correlate to the physical press"),
			HoldActivated->InputSerial, PressInputSerial);
		TestEqual(TEXT("Hold release should correlate to the physical release"),
			HoldReleased->InputSerial, ReleaseInputSerial);
	}
	TestEqual(TEXT("Ordinary hold lifecycle must not claim character movement mode"),
		MovementStateChanges, 0);
	TestNotNull(TEXT("Terminal attack cleanup should be observable"), TerminalReset);
	TestEqual(TEXT("Directional hold entry and terminal exit should both change input context"), ContextChanges, 2);
	TestEqual(TEXT("Windup entry and terminal exit should both change phase"), PhaseChanges, 2);
	TestEqual(TEXT("Terminal cleanup should restore movement input context"),
		Combat->CurrentInputContext, EInputContext::Movement);
	TestFalse(TEXT("Terminal cleanup should not leave movement suppressed"),
		Combat->IsMovementInputSuppressed());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryExecutionCorrelationTest,
	"KatanaCombat.ActionReaction.Telemetry.ExecutionCorrelation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryExecutionCorrelationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ActionReactionTelemetryTests;

	FScopedConsoleInt ActionDebug(TEXT("Combat.ActionReaction.Debug"), 1);
	FScopedConsoleInt MasterDebug(TEXT("Combat.Debug.All"), 0);
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	Combat->ClearActionReactionTelemetry();

	const bool bExecuted = Combat->ExecuteAttackData(Attack, nullptr, EInputType::LightAttack);
	const FActionReactionTelemetryRecord* Started = nullptr;
	const FActionReactionTelemetryRecord* Finished = nullptr;
	for (const FActionReactionTelemetryRecord& Record : Combat->GetActionReactionTelemetry())
	{
		if (Record.Event == EActionReactionTelemetryEvent::ActionExecutionStarted)
		{
			Started = &Record;
		}
		else if (Record.Event == EActionReactionTelemetryEvent::ActionExecutionFinished)
		{
			Finished = &Record;
		}
	}

	TestNotNull(TEXT("Execution attempt should have a start record"), Started);
	TestNotNull(TEXT("Execution attempt should have a terminal record"), Finished);
	if (Started && Finished)
	{
		TestTrue(TEXT("Execution attempt should allocate a queue identity"), Started->QueueEntryId != 0);
		TestEqual(TEXT("Execution records should correlate by queue identity"),
			Finished->QueueEntryId, Started->QueueEntryId);
		TestEqual(TEXT("Execution records should preserve attack data"),
			Finished->AttackDataPath, FSoftObjectPath(Attack));
		TestEqual(TEXT("Terminal reason should match the returned execution result"),
			Finished->Reason,
			bExecuted ? EActionReactionTelemetryReason::Executed : EActionReactionTelemetryReason::ExecutionFailed);
	}

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryImmediateFailureReasonTest,
	"KatanaCombat.ActionReaction.Telemetry.ImmediateFailureReason",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryImmediateFailureReasonTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ActionReactionTelemetryTests;

	FScopedConsoleInt ActionDebug(TEXT("Combat.ActionReaction.Debug"), 1);
	FScopedConsoleInt MasterDebug(TEXT("Combat.Debug.All"), 0);
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UAttackData* UnplayableAttack = NewObject<UAttackData>();
	UnplayableAttack->AttackType = EAttackType::Light;
	Player->CombatSettings->DefaultWeaponData->AttackConfiguration->DefaultLightAttack =
		UnplayableAttack;
	Combat->ClearActionReactionTelemetry();

	Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);

	const FActionReactionTelemetryRecord* Finalized =
		Combat->GetActionReactionTelemetry().FindByPredicate(
			[](const FActionReactionTelemetryRecord& Record)
			{
				return Record.Event == EActionReactionTelemetryEvent::InputFinalized;
			});
	TestNotNull(TEXT("Failed immediate execution should finalize its physical input"), Finalized);
	if (Finalized)
	{
		TestEqual(TEXT("Resolved attack playback failure should not be reported as resolution failure"),
			Finalized->Reason, EActionReactionTelemetryReason::ImmediateExecutionFailed);
		TestEqual(TEXT("Failed immediate execution should reject the input application"),
			Finalized->InputDisposition, ECombatInputDisposition::Rejected);
	}

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryStaleMontageCallbackTest,
	"KatanaCombat.ActionReaction.Telemetry.StaleMontageCallback",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryStaleMontageCallbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ActionReactionTelemetryTests;

	FScopedConsoleInt ActionDebug(TEXT("Combat.ActionReaction.Debug"), 1);
	FScopedConsoleInt MasterDebug(TEXT("Combat.Debug.All"), 0);
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UAttackData* ActiveAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* StaleAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	Combat->CurrentAttackData = ActiveAttack;
	Combat->CurrentPhase = EAttackPhase::Windup;
	Combat->AttackStateMachine.OnAttackStarted(
		ActiveAttack->AttackMontage,
		NAME_None,
		World->GetTimeSeconds());
	const int32 ActiveGeneration = Combat->AttackStateMachine.AttackGeneration;
	Combat->ClearActionReactionTelemetry();

	Combat->OnMontageEnded(StaleAttack->AttackMontage, false);

	const FActionReactionTelemetryRecord* Rejected =
		Combat->GetActionReactionTelemetry().FindByPredicate(
			[](const FActionReactionTelemetryRecord& Record)
			{
				return Record.Event == EActionReactionTelemetryEvent::MontageCallbackRejected;
			});
	TestNotNull(TEXT("Stale montage callback should be observable"), Rejected);
	if (Rejected)
	{
		TestEqual(TEXT("Stale callback should have a closed reason"),
			Rejected->Reason, EActionReactionTelemetryReason::StaleMontageCallback);
		TestEqual(TEXT("Rejected callback should identify the ended montage"),
			Rejected->MontagePath, FSoftObjectPath(StaleAttack->AttackMontage));
		TestEqual(TEXT("Rejected callback should preserve the active generation"),
			Rejected->AttackGeneration, ActiveGeneration);
	}
	TestEqual(TEXT("Observing a stale callback must not replace current attack data"),
		Combat->CurrentAttackData.Get(), ActiveAttack);
	TestEqual(TEXT("Observing a stale callback must not reset phase"),
		Combat->CurrentPhase, EAttackPhase::Windup);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryFreshChainCancellationTest,
	"KatanaCombat.ActionReaction.Telemetry.FreshChainCancellation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryFreshChainCancellationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ActionReactionTelemetryTests;

	FScopedConsoleInt ActionDebug(TEXT("Combat.ActionReaction.Debug"), 1);
	FScopedConsoleInt MasterDebug(TEXT("Combat.Debug.All"), 0);
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UAttackData* LightAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* HeavyAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	Player->CombatSettings->DefaultWeaponData->AttackConfiguration->DefaultLightAttack = LightAttack;
	Player->CombatSettings->DefaultWeaponData->AttackConfiguration->DefaultHeavyAttack = HeavyAttack;

	Combat->SetPhase(EAttackPhase::Windup);
	Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press, EInputDirection::Forward);
	TestEqual(TEXT("Setup should leave one pending action"), Combat->GetActionQueue().Num(), 1);
	const uint64 PendingQueueId = Combat->GetActionQueue().IsEmpty()
		? 0
		: Combat->GetActionQueue()[0].QueueEntryId;
	const uint64 PendingInputSerial = Combat->GetActionQueue().IsEmpty()
		? 0
		: Combat->GetActionQueue()[0].InputAction.InputSerial;
	Combat->SetPhase(EAttackPhase::None);
	Combat->ClearActionReactionTelemetry();

	Combat->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Press, EInputDirection::Forward);

	const FActionReactionTelemetryRecord* Cancelled =
		Combat->GetActionReactionTelemetry().FindByPredicate(
			[PendingQueueId](const FActionReactionTelemetryRecord& Record)
			{
				return Record.Event == EActionReactionTelemetryEvent::QueueCancelled
					&& Record.QueueEntryId == PendingQueueId;
			});
	TestTrue(TEXT("Setup queue identity should be nonzero"), PendingQueueId != 0);
	TestNotNull(TEXT("Fresh immediate chain should explain pending cancellation"), Cancelled);
	if (Cancelled)
	{
		TestEqual(TEXT("Fresh-chain cancellation should have a closed reason"),
			Cancelled->Reason, EActionReactionTelemetryReason::FreshChainReset);
		TestEqual(TEXT("Cancellation should retain originating input identity"),
			Cancelled->InputSerial, PendingInputSerial);
	}

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryExplicitQueueClearTest,
	"KatanaCombat.ActionReaction.Telemetry.ExplicitQueueClear",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryExplicitQueueClearTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ActionReactionTelemetryTests;

	FScopedConsoleInt ActionDebug(TEXT("Combat.ActionReaction.Debug"), 1);
	FScopedConsoleInt MasterDebug(TEXT("Combat.Debug.All"), 0);
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	Player->CombatSettings->DefaultWeaponData->AttackConfiguration->DefaultLightAttack = Attack;
	Combat->SetPhase(EAttackPhase::Windup);
	Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press, EInputDirection::Forward);
	TestEqual(TEXT("Setup should leave one pending action"), Combat->GetActionQueue().Num(), 1);
	const uint64 QueueEntryId = Combat->GetActionQueue().IsEmpty()
		? 0
		: Combat->GetActionQueue()[0].QueueEntryId;
	Combat->ClearActionReactionTelemetry();

	Combat->ClearQueue(false);

	const FActionReactionTelemetryRecord* Cancelled =
		Combat->GetActionReactionTelemetry().FindByPredicate(
			[QueueEntryId](const FActionReactionTelemetryRecord& Record)
			{
				return Record.Event == EActionReactionTelemetryEvent::QueueCancelled
					&& Record.QueueEntryId == QueueEntryId;
			});
	TestNotNull(TEXT("Explicit queue clear should explain each discarded pending action"), Cancelled);
	if (Cancelled)
	{
		TestEqual(TEXT("Explicit queue clear should have a closed reason"),
			Cancelled->Reason, EActionReactionTelemetryReason::ExplicitQueueClear);
	}
	TestTrue(TEXT("Queue should retain its existing clear behavior"), Combat->GetActionQueue().IsEmpty());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryRejectedHoldReleaseTest,
	"KatanaCombat.ActionReaction.Telemetry.RejectedHoldRelease",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryRejectedHoldReleaseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ActionReactionTelemetryTests;

	FScopedConsoleInt ActionDebug(TEXT("Combat.ActionReaction.Debug"), 1);
	FScopedConsoleInt MasterDebug(TEXT("Combat.Debug.All"), 0);
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	Combat->ActivateHold(EInputType::LightAttack, 1.0f);
	const int32 HoldGeneration = Combat->HoldState.CurrentHold.HoldID;
	Combat->ClearActionReactionTelemetry();

	Combat->DeactivateHold();

	const FActionReactionTelemetryRecord* Rejected =
		Combat->GetActionReactionTelemetry().FindByPredicate(
			[](const FActionReactionTelemetryRecord& Record)
			{
				return Record.Event == EActionReactionTelemetryEvent::HoldReleaseRejected;
			});
	TestNotNull(TEXT("Missing attack context should explain a rejected hold release"), Rejected);
	if (Rejected)
	{
		TestEqual(TEXT("Rejected release should have a closed reason"),
			Rejected->Reason, EActionReactionTelemetryReason::MissingAttackContext);
		TestEqual(TEXT("Rejected release should retain hold identity"),
			Rejected->HoldGeneration, HoldGeneration);
	}
	TestFalse(TEXT("Missing attack context should terminate the hold without a follow-up"),
		Combat->IsHolding());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryMovementDecisionTest,
	"KatanaCombat.ActionReaction.Telemetry.MovementDecisionChanges",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryMovementDecisionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ActionReactionTelemetryTests;

	FScopedConsoleInt ActionDebug(TEXT("Combat.ActionReaction.Debug"), 1);
	FScopedConsoleInt MasterDebug(TEXT("Combat.Debug.All"), 0);
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player =
		FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UCharacterMovementComponent* Movement =
		Player ? Player->GetCharacterMovement() : nullptr;
	if (!TestNotNull(TEXT("Player should be created"), Player)
		|| !TestNotNull(TEXT("Combat component should be created"), Combat)
		|| !TestNotNull(TEXT("Movement component should be created"), Movement))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Movement->SetMovementMode(MOVE_Flying);
	Combat->ClearActionReactionTelemetry();
	TestTrue(TEXT("Unsuppressed movement should be applied"),
		Combat->SubmitMovementInput(FVector2D(0.0, 1.0), FRotator::ZeroRotator));
	TestEqual(TEXT("Dead-zone entry should emit one movement decision"),
		CountMovementDecisionRecords(Combat->GetActionReactionTelemetry()), 1);

	const FActionReactionTelemetryRecord* Allowed =
		Combat->GetActionReactionTelemetry().FindByPredicate(
			[](const FActionReactionTelemetryRecord& Record)
			{
				return Record.Event ==
						EActionReactionTelemetryEvent::MovementInputDecisionChanged
					&& Record.Reason ==
						EActionReactionTelemetryReason::MovementInputAllowed;
			});
	TestNotNull(TEXT("Allowed movement should have a reason-coded record"), Allowed);
	if (Allowed)
	{
		TestEqual(TEXT("Movement record should retain the sample serial"),
			Allowed->InputSerial, Combat->GetMovementInputSample().Serial);
		TestEqual(TEXT("Movement record should identify the primary owner generation"),
			Allowed->PrimaryActionGeneration, Allowed->AttackGeneration);
		TestEqual(TEXT("Movement record should include CMC mode"),
			Allowed->CharacterMovementMode,
			FName(TEXT("MOVE_Flying")));
		TestEqual(TEXT("Movement record should include normalized magnitude"),
			Allowed->MovementMagnitude, 1.0f);
		TestFalse(TEXT("Idle movement should report no root motion"),
			Allowed->bRootMotionActive);
	}

	Combat->SubmitMovementInput(FVector2D(0.5, 0.5), FRotator::ZeroRotator);
	TestEqual(TEXT("Repeated samples with the same decision must not spam history"),
		CountMovementDecisionRecords(Combat->GetActionReactionTelemetry()), 1);

	Combat->ActivateHold(EInputType::LightAttack, 0.0f);
	TestFalse(TEXT("Committed hold should suppress movement application"),
		Combat->SubmitMovementInput(FVector2D(0.0, 1.0), FRotator::ZeroRotator));
	TestEqual(TEXT("Hold policy transition should emit one additional decision"),
		CountMovementDecisionRecords(Combat->GetActionReactionTelemetry()), 2);
	const FActionReactionTelemetryRecord* Suppressed =
		Combat->GetActionReactionTelemetry().FindByPredicate(
			[](const FActionReactionTelemetryRecord& Record)
			{
				return Record.Event ==
						EActionReactionTelemetryEvent::MovementInputDecisionChanged
					&& Record.Reason ==
						EActionReactionTelemetryReason::MovementInputSuppressedByHold;
			});
	TestNotNull(TEXT("Hold suppression should have a closed reason"), Suppressed);

	Combat->ClearActionReactionTelemetry();
	Combat->SubmitMovementInput(FVector2D(0.0, 1.0), FRotator::ZeroRotator);
	TestEqual(TEXT("Clearing telemetry should re-arm the current movement decision"),
		CountMovementDecisionRecords(Combat->GetActionReactionTelemetry()), 1);

	Combat->DeactivateHold();
	Combat->SubmitMovementInput(FVector2D(0.0, 1.0), FRotator::ZeroRotator);
	const int32 BeforeTerminal =
		CountMovementDecisionRecords(Combat->GetActionReactionTelemetry());
	Combat->ClearMovementInputSample();
	TestEqual(TEXT("Terminal move edge should emit one inactive decision"),
		CountMovementDecisionRecords(Combat->GetActionReactionTelemetry()),
		BeforeTerminal + 1);
	Combat->ClearMovementInputSample();
	TestEqual(TEXT("Repeated terminal clears must be idempotent"),
		CountMovementDecisionRecords(Combat->GetActionReactionTelemetry()),
		BeforeTerminal + 1);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryStableCsvTest,
	"KatanaCombat.ActionReaction.Telemetry.StableCsv",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryStableCsvTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FActionReactionTelemetryRecord Later;
	Later.Sequence = 2;
	Later.Event = EActionReactionTelemetryEvent::TerminalReset;
	Later.Reason = EActionReactionTelemetryReason::MontageInterrupted;
	Later.UnscaledTimestamp = 20.0;
	Later.ActorStableId.Value = 22;
	Later.Detail = TEXT("Interrupted, owner \"Player\"");

	FActionReactionTelemetryRecord Earlier;
	Earlier.Sequence = 9;
	Earlier.Event = EActionReactionTelemetryEvent::InputFinalized;
	Earlier.Reason = EActionReactionTelemetryReason::DuplicatePendingInput;
	Earlier.ActorPathSnapshot = TEXT("/Game/Test/DestroyedPlayer.DestroyedPlayer");
	Earlier.UnscaledTimestamp = 10.0;
	Earlier.ActorStableId.Value = 11;
	Earlier.InputSerial = 77;
	Earlier.QueueEntryId = 12;
	Earlier.InputDisposition = ECombatInputDisposition::Rejected;
	Earlier.AttackGeneration = 4;
	Earlier.HoldGeneration = 8;

	const FString Csv = ActionReactionTelemetry::BuildCsv({Later, Earlier});
	TestTrue(TEXT("CSV should use the versioned stable header"),
		Csv.StartsWith(TEXT("schema_version,sequence,event,reason,simulation_timestamp")));
	TestTrue(TEXT("CSV should expose correlation identities"),
		Csv.Contains(TEXT("input_serial,queue_entry_id,attack_generation,primary_action_generation,hold_generation")));
	TestTrue(TEXT("CSV should expose movement observation context"),
		Csv.Contains(TEXT("movement_disposition,character_movement_mode,character_custom_movement_mode,movement_magnitude,root_motion_active")));
	TestTrue(TEXT("CSV should emit stable event and reason names"),
		Csv.Contains(TEXT("InputFinalized")) && Csv.Contains(TEXT("DuplicatePendingInput")));
	TestTrue(TEXT("CSV should retain snapshotted identity after actor invalidation"),
		Csv.Contains(TEXT("/Game/Test/DestroyedPlayer.DestroyedPlayer")));
	TestTrue(TEXT("CSV should escape commas and embedded quotes"),
		Csv.Contains(TEXT("\"Interrupted, owner \"\"Player\"\"\"")));
	TestTrue(TEXT("CSV should sort by timestamps before local sequence"),
		Csv.Find(TEXT("DuplicatePendingInput")) < Csv.Find(TEXT("MontageInterrupted")));

	TArray<FString> Lines;
	Csv.ParseIntoArrayLines(Lines, true);
	TestEqual(TEXT("CSV should contain one header and two records"), Lines.Num(), 3);
	if (Lines.Num() == 3)
	{
		const int32 HeaderFields = ActionReactionTelemetryTests::CountCsvFields(Lines[0]);
		TestEqual(TEXT("First row should match header cardinality"),
			ActionReactionTelemetryTests::CountCsvFields(Lines[1]), HeaderFields);
		TestEqual(TEXT("Second row should match header cardinality"),
			ActionReactionTelemetryTests::CountCsvFields(Lines[2]), HeaderFields);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FActionReactionTelemetryConsoleContractTest,
	"KatanaCombat.ActionReaction.Telemetry.ConsoleContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActionReactionTelemetryConsoleContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ActionReactionTelemetryTests;

	FScopedConsoleInt ActionDebug(TEXT("Combat.ActionReaction.Debug"), 0);
	FScopedConsoleInt MasterDebug(TEXT("Combat.Debug.All"), 1);
	TestTrue(TEXT("Action-reaction telemetry CVar should be registered"), ActionDebug.IsValid());
	TestTrue(TEXT("Master debug CVar should be registered"), MasterDebug.IsValid());
	TestTrue(TEXT("Master debug should enable action-reaction telemetry"),
		ActionReactionTelemetry::IsEnabled());
	TestNotNull(TEXT("Telemetry dump command should be registered"),
		IConsoleManager::Get().FindConsoleObject(TEXT("Combat.ActionReaction.DumpTelemetry")));
	TestNotNull(TEXT("Telemetry clear command should be registered"),
		IConsoleManager::Get().FindConsoleObject(TEXT("Combat.ActionReaction.ClearTelemetry")));
	return true;
}
