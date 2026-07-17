// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "CombatEventRecorder.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_ChainStageTransition.h"
#include "Containers/Ticker.h"
#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/AttackConfiguration.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Data/DefenseConfiguration.h"
#include "Data/PairedAnimationData.h"
#include "Data/TargetingSettings.h"
#include "Subsystems/CombatEffectsWorldSubsystem.h"
#include "Utilities/CombatGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

#include <limits>

struct FDefenseChainFixture
{
	UWorld* World = nullptr;
	APlayerCharacter* Defender = nullptr;
	UCombatComponent* DefenderCombat = nullptr;
	AEnemyCharacter* SourceAttacker = nullptr;
	UCombatComponent* SourceCombat = nullptr;
	UPairedAnimationComponent* Paired = nullptr;
	UPairedAnimationComponent* SourcePaired = nullptr;
	UAttackData* SourceAttack = nullptr;
	UAttackData* CounterAttack = nullptr;
	UDefenseConfiguration* DefenseConfig = nullptr;
	FAttackInstanceId AttackInstance;

	bool Initialize()
	{
		World = FCombatTestHelpers::CreateTestWorld();
		Defender = FCombatTestHelpers::CreateTestCharacterWithCombat(World, DefenderCombat);
		SourceAttacker = FCombatTestHelpers::CreateTestEnemyCharacter(
			World, FVector(250.0f, 0.0f, 0.0f));
		SourceCombat = SourceAttacker ? SourceAttacker->CombatComponent.Get() : nullptr;
		Paired = Defender ? Defender->PairedAnimationComponent.Get() : nullptr;
		SourcePaired = SourceAttacker ? SourceAttacker->PairedAnimationComponent.Get() : nullptr;
		if (!World || !Defender || !DefenderCombat || !SourceAttacker
			|| !SourceCombat || !Paired || !SourcePaired)
		{
			return false;
		}
		SourceAttacker->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));

		UCombatSettings* Settings = FCombatTestHelpers::CreateTestCombatSettings();
		CounterAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
		if (UAttackConfiguration* Attacks = Settings ? Settings->GetAttackConfiguration() : nullptr)
		{
			Attacks->DefaultLightAttack = CounterAttack;
			Attacks->DefaultHeavyAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
		}
		Defender->CombatSettings = Settings;
		DefenderCombat->CombatSettings = Settings;

		UTargetingSettings* TargetingSettings = NewObject<UTargetingSettings>();
		TargetingSettings->MaxTargetDistance = 1500.0f;
		TargetingSettings->bRequireLineOfSight = false;
		Defender->TargetingComponent->TargetingSettingsOverride = TargetingSettings;

		DefenseConfig = NewObject<UDefenseConfiguration>();
		DefenseConfig->DefenseThreatRange = 1000.0f;
		DefenseConfig->MaximumHighConfidencePredictionAge = 1.0f;
		DefenseConfig->HardGuardConeHalfAngle = 70.0f;
		DefenseConfig->MaximumAutomaticTurn = 70.0f;
		DefenseConfig->DefenseTurnRate = 360.0f;
		DefenseConfig->PerfectParryFinalTolerance = 10.0f;
		DefenseConfig->NoMontageParryBridgeSeconds = 0.05f;
		DefenseConfig->CounterWindowSeconds = 5.0f;
		DefenseConfig->FinisherReadySeconds = 5.0f;
		DefenseConfig->TimeDilationLeaseWatchdogSeconds = 10.0f;
		DefenderCombat->DefenseConfigurationOverride = DefenseConfig;

		SourceAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
		SourceAttack->AttackTags.AddTag(KatanaCombatGameplayTags::AttackDefenseParryable());
		SourceCombat->SeedAttackWindowStateForTesting(SourceAttack, EAttackPhase::Windup, 41);
		SourceCombat->SetAttackIntentTarget(Defender);

		const double Now = World->GetTimeSeconds();
		FAttackThreatPrediction Prediction;
		Prediction.IntendedTarget = Defender;
		Prediction.PathOrigin = SourceAttacker->GetActorLocation();
		Prediction.PathDirection =
			(Defender->GetActorLocation() - SourceAttacker->GetActorLocation()).GetSafeNormal();
		Prediction.PredictedContactPoint = Defender->GetActorLocation();
		Prediction.SourceSocket = TEXT("weapon_tip");
		Prediction.DefenderTargetBone = TEXT("spine_03");
		Prediction.PredictionSimulationTimestamp = Now;
		Prediction.PredictedContactSimulationTime = Now + 0.20;
		Prediction.Lane = EIncomingAttackLane::Center;
		Prediction.Height = EAttackHeight::Middle;
		Prediction.Confidence = EDefensePredictionConfidence::High;
		Prediction.bPathIntersectsThreatVolume = true;
		SourceCombat->PublishAttackThreatPrediction(Prediction);

		FAnimNotifyRuntimeSourceId HitSource;
		HitSource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Chain/HitWindow"));
		HitSource.NotifyEventIndex = 1;
		FAnimNotifyRuntimeSourceId ParrySource;
		ParrySource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Chain/ParryWindow"));
		ParrySource.NotifyEventIndex = 2;
		const FAttackWindowInstanceId HitWindow = SourceCombat->OpenAttackWindow(
			EAttackWindowKind::Hit, HitSource, 401, 0.40f);
		const FAttackWindowInstanceId ParryWindow = SourceCombat->OpenAttackWindow(
			EAttackWindowKind::Parry, ParrySource, 401, 0.40f);
		AttackInstance = ParryWindow.AttackInstance;
		return HitWindow.IsValid() && ParryWindow.IsValid();
	}

	bool StartCommittedParry() const
	{
		DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
		return Paired->GetChainState() == EChainCounterState::ParryActive
			&& SourceCombat->IsAttackConsumed(AttackInstance);
	}

	bool OpenCounterWindow() const
	{
		const int32 Generation = Paired->ActiveDefenseSequence.StageGeneration;
		const FDefenseAsyncHandle AsyncHandle =
			Paired->ActiveDefenseSequence.BridgeFallbackHandle;
		Paired->HandleNoMontageDefenseBridgeElapsed(Generation, AsyncHandle);
		return Paired->GetChainState() == EChainCounterState::CounterWindow;
	}

	bool PreflightStage(
		UPairedAnimationData* Data,
		const EPairedReactionType ReactionType,
		FString& OutFailureReason) const
	{
		return Paired
			&& Paired->PreflightDefenseChainStage(Data, ReactionType, OutFailureReason);
	}

	bool PreflightBridge(
		UPairedAnimationData* Data,
		const FName ReviewedMarker,
		FString& OutFailureReason) const
	{
		return PreflightBridgeWithResolution(
			Data,
			ReviewedMarker,
			DefenderCombat->GetLastInputDefenseResolutionForTesting(),
			OutFailureReason);
	}

	bool PreflightBridgeWithResolution(
		UPairedAnimationData* Data,
		const FName ReviewedMarker,
		const FDefenseResolution& Resolution,
		FString& OutFailureReason) const
	{
		FDefensePresentationPayload Presentation;
		Presentation.PairedBridgeData = Data;
		Presentation.ReviewedDeflectionMarker = ReviewedMarker;
		return Paired
			&& Paired->PreflightDefenseBridge(
				Resolution,
				Presentation,
				OutFailureReason);
	}

	void SetPlaybackOverride(
		TFunction<bool(EPairedAnimationRole, const UPairedAnimationData*, int32&)> Override) const
	{
		Paired->DefenseStagePlaybackOverrideForTesting = MoveTemp(Override);
	}

	void EnableLethalCounterData() const
	{
		Paired->bAllowLethalCounterPairedData = true;
	}

	bool TryCompetingPairedStart(
		AActor* Target,
		UPairedAnimationData* Data,
		const EPairedReactionType ReactionType) const
	{
		return Paired
			&& Paired->TryStartPairedAnimationWithTarget(Target, Data, ReactionType);
	}

	void SetPendingRoleMontageCallback(
		UAnimMontage* Montage,
		const EPairedAnimationRole Role,
		const bool bPending) const
	{
		UPairedAnimationComponent* RoleComponent =
			Role == EPairedAnimationRole::Attacker ? Paired : SourcePaired;
		if (bPending)
		{
			RoleComponent->RetireOwnerMontageCallback(Montage);
		}
		else
		{
			RoleComponent->CancelRetiredOwnerMontageCallback(Montage);
		}
	}

	void Destroy() const
	{
		if (Paired && Paired->GetChainState() != EChainCounterState::None)
		{
			Paired->CancelPairedAnimation(0.0f);
		}
		FCombatTestHelpers::DestroyTestWorld(World);
	}
};

namespace
{

UPairedAnimationData* CreateChainStageData(
	const EPairedReactionType ReactionType,
	const EChainStageTransitionType Transition = EChainStageTransitionType::OpenCounterWindow,
	const FName MarkerName = NAME_None,
	const bool bAutoContinue = false)
{
	UPairedAnimationData* Data = NewObject<UPairedAnimationData>();
	Data->ReactionType = ReactionType;
	Data->AttackerMontage = NewObject<UAnimMontage>(Data);
	Data->VictimMontage = NewObject<UAnimMontage>(Data);
	Data->bIsLethal = false;
	Data->BaseDamage = 7.0f;
	Data->DamageMultiplier = 1.0f;
	Data->ChainTransitionPolicy.RequiredMarker = MarkerName;
	Data->ChainTransitionPolicy.bAutoContinue = bAutoContinue;
	Data->ChainTransitionPolicy.bFinisherRetryable = true;
	if (!MarkerName.IsNone())
	{
		UAnimNotify_ChainStageTransition* Notify =
			NewObject<UAnimNotify_ChainStageTransition>(Data->AttackerMontage);
		Notify->Transition = Transition;
		Notify->MarkerName = MarkerName;
		FAnimNotifyEvent Event;
		Event.Notify = Notify;
		Data->AttackerMontage->Notifies.Add(Event);
	}
	return Data;
}

FAnimNotifyRuntimeSourceId MakeMarkerSource(const UAnimMontage* Montage, const int32 Index = 0)
{
	FAnimNotifyRuntimeSourceId Source;
	Source.SourceAnimation = FSoftObjectPath(Montage);
	Source.NotifyEventIndex = Index;
	return Source;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainContextLeaseOwnershipTest,
	"KatanaCombat.Defense.Chain.ContextLeaseOwnership",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainContextLeaseOwnershipTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	const FGameplayTag Tag = KatanaCombatGameplayTags::ContextParryCounter();

	if (!TestNotNull(TEXT("Character exists"), Character)
		|| !TestNotNull(TEXT("Combat component exists"), Combat)
		|| !TestTrue(TEXT("Canonical context tag exists"), Tag.IsValid()))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FCombatContextLeaseHandle First = Combat->AcquireContextTagLease(Tag, TEXT("DefenseSequenceA"));
	const FCombatContextLeaseHandle Second = Combat->AcquireContextTagLease(Tag, TEXT("DefenseSequenceB"));
	TestTrue(TEXT("First owner receives a valid handle"), First.IsValid());
	TestTrue(TEXT("Second owner receives a valid handle"), Second.IsValid());
	TestTrue(TEXT("Tag is active with both owners"), Combat->HasActiveContextTag(Tag));

	Combat->ReleaseContextTagLease(First);
	TestTrue(TEXT("One owner cannot remove another owner's tag contribution"), Combat->HasActiveContextTag(Tag));
	Combat->ReleaseContextTagLease(First);
	TestTrue(TEXT("Duplicate release is side-effect free"), Combat->HasActiveContextTag(Tag));
	Combat->ReleaseContextTagLease(Second);
	TestFalse(TEXT("Last release removes the tag"), Combat->HasActiveContextTag(Tag));

	const FCombatContextLeaseHandle InvalidTag = Combat->AcquireContextTagLease({}, TEXT("Invalid"));
	const FCombatContextLeaseHandle InvalidOwner = Combat->AcquireContextTagLease(Tag, NAME_None);
	TestFalse(TEXT("Invalid tag fails closed"), InvalidTag.IsValid());
	TestFalse(TEXT("Unnamed owner fails closed"), InvalidOwner.IsValid());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainAuthoringContractTest,
	"KatanaCombat.Defense.Chain.AuthoringContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainAuthoringContractTest::RunTest(const FString& Parameters)
{
	UPairedAnimationData* Data = NewObject<UPairedAnimationData>();
	TestNotNull(TEXT("Paired data can own Chain policy"), Data);
	TestEqual(TEXT("Driver defaults to initiating montage role"),
		Data->ChainTransitionPolicy.DriverRole, EPairedAnimationRole::Attacker);
	TestFalse(TEXT("Unreviewed default does not claim a retainable pose"),
		Data->ChainTransitionPolicy.HasRetainableReadyPose());

	Data->ChainTransitionPolicy.AttackerReadySection = TEXT("CounterReady");
	Data->ChainTransitionPolicy.bVictimTerminalPoseCompatible = true;
	TestTrue(TEXT("Each role can satisfy readiness by section or reviewed terminal pose"),
		Data->ChainTransitionPolicy.HasRetainableReadyPose());
	TestTrue(TEXT("FinisherActive is distinct from FinisherReady"),
		EChainCounterState::FinisherActive != EChainCounterState::FinisherReady);

	UPairedAnimationData* InvalidReadySection = CreateChainStageData(
		EPairedReactionType::Parry,
		EChainStageTransitionType::OpenCounterWindow,
		TEXT("CounterReady"));
	InvalidReadySection->ChainTransitionPolicy.AttackerReadySection = TEXT("MissingReadySection");
	InvalidReadySection->ChainTransitionPolicy.bVictimTerminalPoseCompatible = true;
	TestFalse(TEXT("A nonexistent ready section cannot satisfy the authoring contract"),
		InvalidReadySection->IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainCollisionLeaseIdentityTest,
	"KatanaCombat.Defense.Chain.CollisionLeaseIdentity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainCollisionLeaseIdentityTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Owner = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Partner = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(100.0f, 0.0f, 0.0f));
	UPairedAnimationComponent* Paired = Owner ? Owner->PairedAnimationComponent.Get() : nullptr;
	if (!TestNotNull(TEXT("Owner paired component exists"), Paired)
		|| !TestNotNull(TEXT("Partner exists"), Partner))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Paired->AddPairedPartner(Partner);
	FAnimNotifyRuntimeSourceId FirstSource;
	FirstSource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Chain/CollisionMontage"));
	FirstSource.NotifyEventIndex = 2;
	FAnimNotifyRuntimeSourceId SecondSource = FirstSource;
	SecondSource.NotifyEventIndex = 3;
	const EMovementMode BaselineMovementMode =
		Owner->GetCharacterMovement()->MovementMode.GetValue();

	TestTrue(TEXT("First exact notify window acquires state"), Paired->BeginPairedCollisionNotify(
		FirstSource, 41, true, true, false, true, false, 150.0f));
	TestTrue(TEXT("Overlapping exact notify window acquires independent state"), Paired->BeginPairedCollisionNotify(
		SecondSource, 41, true, true, false, true, false, 150.0f));
	TestEqual(TEXT("Both windows are independently owned"), Paired->GetActivePairedStateLeaseCount(), 2);
	TestEqual(TEXT("Movement is disabled while either owner is active"),
		Owner->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
	TestTrue(TEXT("Tracked partner collision is ignored"),
		Owner->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Partner));

	Paired->EndPairedCollisionNotify(FirstSource, 999);
	TestEqual(TEXT("Stale montage End cannot release a live window"),
		Paired->GetActivePairedStateLeaseCount(), 2);
	Paired->EndPairedCollisionNotify(FirstSource, 41);
	TestEqual(TEXT("Exact End releases only its own window"),
		Paired->GetActivePairedStateLeaseCount(), 1);
	Paired->EndPairedAnimation();
	TestEqual(TEXT("Legacy paired cleanup cannot restore movement owned by another lease"),
		Owner->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
	TestEqual(TEXT("Overlap keeps movement disabled"),
		Owner->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
	Paired->EndPairedCollisionNotify(SecondSource, 41);
	TestEqual(TEXT("Last exact End releases all notify-owned state"),
		Paired->GetActivePairedStateLeaseCount(), 0);
	TestEqual(TEXT("Last release restores the captured movement mode"),
		Owner->GetCharacterMovement()->MovementMode.GetValue(), BaselineMovementMode);
	TestFalse(TEXT("Last release restores partner collision"),
		Owner->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Partner));

	FAnimNotifyRuntimeSourceId BaselineSource = FirstSource;
	BaselineSource.NotifyEventIndex = 4;
	Owner->GetCapsuleComponent()->IgnoreActorWhenMoving(Partner, true);
	TestTrue(TEXT("A new lease captures a pre-existing move-ignore baseline"),
		Paired->BeginPairedCollisionNotify(
			BaselineSource, 42, true, true, false, true, false, 150.0f));
	Paired->EndPairedCollisionNotify(BaselineSource, 42);
	TestTrue(TEXT("Lease release preserves a pre-existing move-ignore relationship"),
		Owner->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Partner));
	Owner->GetCapsuleComponent()->IgnoreActorWhenMoving(Partner, false);

	FAnimNotifyRuntimeSourceId CollisionOnlySource = FirstSource;
	CollisionOnlySource.NotifyEventIndex = 5;
	TestTrue(TEXT("Collision-only lease acquires partner-ignore ownership"),
		Paired->BeginPairedCollisionNotify(
			CollisionOnlySource, 43, true, true, false, false, true, 150.0f));
	Owner->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Owner->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Owner->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Paired->TickPairedCollisionNotify(CollisionOnlySource, 43);
	TestEqual(TEXT("Collision-only recompute cannot rewrite unowned movement"),
		Owner->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Falling);
	TestEqual(TEXT("Collision-only recompute cannot rewrite unowned capsule mode"),
		Owner->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("Tracked-partner recompute cannot rewrite unowned pawn response"),
		Owner->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Overlap);
	Paired->EndPairedCollisionNotify(CollisionOnlySource, 43);
	TestEqual(TEXT("Collision-only release preserves external movement state"),
		Owner->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Falling);
	TestEqual(TEXT("Collision-only release preserves external capsule state"),
		Owner->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("Collision-only release preserves external pawn response"),
		Owner->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Overlap);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainSequenceOwnershipTimeoutTest,
	"KatanaCombat.Defense.Chain.SequenceOwnershipTimeout",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainSequenceOwnershipTimeoutTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create defense Chain fixture"));
		Fixture.Destroy();
		return false;
	}
	Fixture.DefenseConfig->CounterWindowSeconds = 0.01f;
	if (!Fixture.StartCommittedParry())
	{
		AddError(TEXT("Failed to start committed perfect parry"));
		Fixture.Destroy();
		return false;
	}
	AActor* DefenderUnrelatedPartner = Fixture.World->SpawnActor<AActor>();
	AActor* SourceUnrelatedPartner = Fixture.World->SpawnActor<AActor>();
	Fixture.Paired->AddPairedPartner(DefenderUnrelatedPartner);
	Fixture.SourcePaired->AddPairedPartner(SourceUnrelatedPartner);

	const FDefenseSequenceContext& Parry = Fixture.Paired->GetActiveDefenseSequenceContext();
	TestEqual(TEXT("Committed sequence starts in ParryActive"),
		Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);
	TestTrue(TEXT("Sequence retains the defender"), Parry.Defender.Get() == Fixture.Defender);
	TestTrue(TEXT("Sequence retains the source attacker"),
		Parry.SourceAttacker.Get() == Fixture.SourceAttacker);
	TestEqual(TEXT("Sequence retains the immutable committed interaction"),
		Parry.OriginatingInteraction,
		Fixture.DefenderCombat->GetLastInputDefenseResolutionForTesting().InteractionId);
	TestTrue(TEXT("Sequence owns input before a response window"), Fixture.Paired->IsInputBlocked());
	TestTrue(TEXT("Sequence owns Context.ParryCounter"), Fixture.DefenderCombat->HasActiveContextTag(
		KatanaCombatGameplayTags::ContextParryCounter()));
	TestEqual(TEXT("No-montage fallback still links the source participant"),
		Fixture.Paired->GetPairedPartnerCount(), 2);
	TestEqual(TEXT("No-montage fallback links the defender on the source"),
		Fixture.SourcePaired->GetPairedPartnerCount(), 2);
	FAnimNotifyRuntimeSourceId NotifySource;
	NotifySource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Chain/TerminalCollision"));
	NotifySource.NotifyEventIndex = 7;
	TestTrue(TEXT("A compatibility notify can overlap canonical sequence ownership"),
		Fixture.Paired->BeginPairedCollisionNotify(
			NotifySource, 77, true, true, false, true, false, 150.0f));

	TestTrue(TEXT("Simulation bridge reaches CounterWindow"), Fixture.OpenCounterWindow());
	const FDefenseSequenceContext& Waiting = Fixture.Paired->GetActiveDefenseSequenceContext();
	TestTrue(TEXT("CounterWindow owns an unscaled response deadline"),
		Waiting.ResponseDeadlineUnscaled > FPlatformTime::Seconds());
	FTSTicker::GetCoreTicker().Tick(0.02f);

	TestEqual(TEXT("Unscaled deadline performs terminal cleanup"),
		Fixture.Paired->GetChainState(), EChainCounterState::None);
	TestFalse(TEXT("Timeout releases retained interaction"),
		Fixture.Paired->GetActiveDefenseSequenceContext().OriginatingInteraction.IsValid());
	TestFalse(TEXT("Timeout releases input ownership"), Fixture.Paired->IsInputBlocked());
	TestFalse(TEXT("Timeout releases the context tag"), Fixture.DefenderCombat->HasActiveContextTag(
		KatanaCombatGameplayTags::ContextParryCounter()));
	TestEqual(TEXT("Timeout removes only the sequence's defender registration"),
		Fixture.Paired->GetPairedPartnerCount(), 1);
	TestTrue(TEXT("Timeout preserves unrelated defender-side partner ownership"),
		Fixture.Paired->IsPairedPartner(DefenderUnrelatedPartner));
	TestEqual(TEXT("Timeout removes only the sequence's source registration"),
		Fixture.SourcePaired->GetPairedPartnerCount(), 1);
	TestTrue(TEXT("Timeout preserves unrelated source-side partner ownership"),
		Fixture.SourcePaired->IsPairedPartner(SourceUnrelatedPartner));
	TestEqual(TEXT("Terminal cleanup releases surviving notify-owned state"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
	Fixture.Paired->EndPairedCollisionNotify(NotifySource, 77);
	TestEqual(TEXT("A stale notify End after terminal cleanup is harmless"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
	TestTrue(TEXT("Terminal cleanup preserves held guard"), Fixture.DefenderCombat->IsBlocking());

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainMarkerIdentityTest,
	"KatanaCombat.Defense.Chain.MarkerIdentity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainMarkerIdentityTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
	{
		AddError(TEXT("Failed to create marker identity fixture"));
		Fixture.Destroy();
		return false;
	}

	Fixture.Paired->CancelDefenseAsyncHandle(
		Fixture.Paired->ActiveDefenseSequence.BridgeFallbackHandle);
	Fixture.Paired->ActiveDefenseSequence.BridgeFallbackHandle = {};
	UPairedAnimationData* BridgeData = CreateChainStageData(
		EPairedReactionType::Parry,
		EChainStageTransitionType::OpenCounterWindow,
		TEXT("CounterReady"));
	BridgeData->ChainTransitionPolicy.bAttackerTerminalPoseCompatible = true;
	BridgeData->ChainTransitionPolicy.bVictimTerminalPoseCompatible = true;
	Fixture.Paired->ActiveDefenseSequence.ActivePairedData = BridgeData;
	Fixture.Paired->ActiveDefenseSequence.AttackerMontageInstanceId = 101;
	Fixture.Paired->ActiveDefenseSequence.VictimMontageInstanceId = 202;
	Fixture.Paired->ActivePairedAnimData = BridgeData;
	Fixture.Paired->ActivePairedReactionType = EPairedReactionType::Parry;
	const FAnimNotifyRuntimeSourceId Source = MakeMarkerSource(BridgeData->AttackerMontage);

	Fixture.SourcePaired->HandleChainStageTransition(
		EChainStageTransitionType::OpenCounterWindow, 202, Source);
	TestEqual(TEXT("Partner role cannot drive the bridge marker"),
		Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);
	Fixture.Paired->HandleChainStageTransition(
		EChainStageTransitionType::OpenCounterWindow, 999, Source);
	TestEqual(TEXT("Stale montage instance cannot drive the marker"),
		Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);
	Fixture.Paired->HandleChainStageTransition(
		EChainStageTransitionType::OpenCounterWindow,
		101,
		MakeMarkerSource(BridgeData->AttackerMontage, 3));
	TestEqual(TEXT("Wrong runtime notify source cannot drive the marker"),
		Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);

	Fixture.Paired->HandleChainStageTransition(
		EChainStageTransitionType::OpenCounterWindow, 101, Source);
	TestEqual(TEXT("Exact driver marker opens CounterWindow"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterWindow);
	TestEqual(TEXT("One exact marker creates one response deadline"),
		Fixture.Paired->DefenseResponseTickers.Num(), 1);
	Fixture.Paired->HandleChainStageTransition(
		EChainStageTransitionType::OpenCounterWindow, 101, Source);
	TestEqual(TEXT("Duplicate marker cannot create another deadline"),
		Fixture.Paired->DefenseResponseTickers.Num(), 1);
	TestTrue(TEXT("Marker handoff retains paired presentation ownership"),
		Fixture.Paired->IsPairedAnimationActive());

	Fixture.Destroy();

	FDefenseChainFixture VictimDriver;
	if (!VictimDriver.Initialize() || !VictimDriver.StartCommittedParry())
	{
		AddError(TEXT("Failed to create victim-driver marker fixture"));
		VictimDriver.Destroy();
		return false;
	}
	VictimDriver.Paired->CancelDefenseAsyncHandle(
		VictimDriver.Paired->ActiveDefenseSequence.BridgeFallbackHandle);
	VictimDriver.Paired->ActiveDefenseSequence.BridgeFallbackHandle = {};
	UPairedAnimationData* VictimBridgeData = CreateChainStageData(
		EPairedReactionType::Parry,
		EChainStageTransitionType::OpenCounterWindow,
		TEXT("VictimCounterReady"));
	VictimBridgeData->ChainTransitionPolicy.DriverRole = EPairedAnimationRole::Victim;
	VictimBridgeData->ChainTransitionPolicy.bAttackerTerminalPoseCompatible = true;
	VictimBridgeData->ChainTransitionPolicy.bVictimTerminalPoseCompatible = true;
	VictimBridgeData->VictimMontage->Notifies = MoveTemp(
	VictimBridgeData->AttackerMontage->Notifies);
	VictimDriver.SourceAttacker->SetActorLocationAndRotation(
		FVector(175.0f, 0.0f, 0.0f),
		FRotator(0.0f, 180.0f, 0.0f));
	VictimDriver.SetPlaybackOverride([](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = Role == EPairedAnimationRole::Attacker ? 303 : 404;
		return Data != nullptr;
	});
	FString VictimPreflightFailure;
	VictimBridgeData->AttackerBlendIn = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Bridge preflight rejects a nonfinite playback value"),
		VictimDriver.PreflightBridge(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			VictimPreflightFailure));
	TestTrue(TEXT("Bridge numeric refusal reports an actionable reason"),
		VictimPreflightFailure.Contains(TEXT("numeric")));
	VictimBridgeData->AttackerBlendIn = 0.1f;
	VictimBridgeData->VictimWarpConfig.bWarpRotation = false;
	VictimPreflightFailure.Reset();
	TestFalse(TEXT("Bridge preflight rejects a role without rotation warping"),
		VictimDriver.PreflightBridge(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			VictimPreflightFailure));
	TestTrue(TEXT("Rotation-warp refusal reports the canonical role contract"),
		VictimPreflightFailure.Contains(TEXT("rotation warp")));
	VictimBridgeData->VictimWarpConfig.bWarpRotation = true;
	VictimDriver.SetPendingRoleMontageCallback(
		VictimBridgeData->VictimMontage,
		EPairedAnimationRole::Victim,
		true);
	VictimPreflightFailure.Reset();
	TestFalse(TEXT("Bridge preflight rejects an unresolved prior role callback"),
		VictimDriver.PreflightBridge(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			VictimPreflightFailure));
	TestTrue(TEXT("Pending-callback refusal reports the ownership ambiguity"),
		VictimPreflightFailure.Contains(TEXT("unresolved prior callback")));
	VictimDriver.SetPendingRoleMontageCallback(
		VictimBridgeData->VictimMontage,
		EPairedAnimationRole::Victim,
		false);
	const FAnimNotifyEvent DuplicateDriverMarker =
		VictimBridgeData->VictimMontage->Notifies[0];
	VictimBridgeData->VictimMontage->Notifies.Add(DuplicateDriverMarker);
	VictimPreflightFailure.Reset();
	TestFalse(TEXT("Bridge preflight rejects duplicate matching driver markers"),
		VictimDriver.PreflightBridge(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			VictimPreflightFailure));
	TestTrue(TEXT("Duplicate marker refusal reports the marker contract"),
		VictimPreflightFailure.Contains(TEXT("one reviewed Chain marker")));
	VictimBridgeData->VictimMontage->Notifies.RemoveAt(1);
	VictimPreflightFailure.Reset();
	FDefenseResolution AlreadyAligned =
		VictimDriver.DefenderCombat->GetLastInputDefenseResolutionForTesting();
	AlreadyAligned.Decision.MeasuredYawDegrees = 5.0f;
	AlreadyAligned.Decision.RequiredFinalTolerance = 10.0f;
	AlreadyAligned.Decision.AvailableTurnDegrees = 0.0f;
	TestTrue(TEXT("A bridge inside final tolerance requires no invented turn budget"),
		VictimDriver.PreflightBridgeWithResolution(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			AlreadyAligned,
			VictimPreflightFailure));
	VictimPreflightFailure.Reset();
	const bool bVictimPreflightPassed = VictimDriver.PreflightBridge(
		VictimBridgeData,
		TEXT("VictimCounterReady"),
		VictimPreflightFailure);
	TestTrue(*FString::Printf(
		TEXT("Bridge preflight accepts the explicitly authored victim driver role: %s"),
		*VictimPreflightFailure),
		bVictimPreflightPassed);
	VictimDriver.Paired->ActiveDefenseSequence.ActivePairedData = VictimBridgeData;
	VictimDriver.Paired->ActiveDefenseSequence.AttackerMontageInstanceId = 303;
	VictimDriver.Paired->ActiveDefenseSequence.VictimMontageInstanceId = 404;
	VictimDriver.Paired->ActivePairedAnimData = VictimBridgeData;
	VictimDriver.Paired->ActivePairedReactionType = EPairedReactionType::Parry;
	VictimDriver.SourcePaired->HandleChainStageTransition(
		EChainStageTransitionType::OpenCounterWindow,
		404,
		MakeMarkerSource(VictimBridgeData->VictimMontage));
	TestEqual(TEXT("The authored victim driver marker opens CounterWindow"),
		VictimDriver.Paired->GetChainState(), EChainCounterState::CounterWindow);
	VictimDriver.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainPartialStartRollbackTest,
	"KatanaCombat.Defense.Chain.PartialStartRollback",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainPartialStartRollbackTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create partial-start fixture"));
		Fixture.Destroy();
		return false;
	}
	UPairedAnimationData* CounterData = CreateChainStageData(EPairedReactionType::Counter);
	Fixture.CounterAttack->CounterData = CounterData;
	const int32 OriginalGeneration =
		Fixture.Paired->GetActiveDefenseSequenceContext().StageGeneration;
	const double OriginalDeadline =
		Fixture.Paired->GetActiveDefenseSequenceContext().ResponseDeadlineUnscaled;
	Fixture.Paired->DefenseStagePlaybackOverrideForTesting = [](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = Role == EPairedAnimationRole::Attacker ? 301 : INDEX_NONE;
		return Role == EPairedAnimationRole::Attacker;
	};

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	const FDefenseSequenceContext& RolledBack =
		Fixture.Paired->GetActiveDefenseSequenceContext();
	TestEqual(TEXT("One-role failure keeps CounterWindow retryable"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterWindow);
	TestTrue(TEXT("Rollback retires the attempted generation"),
		RolledBack.StageGeneration > OriginalGeneration);
	TestEqual(TEXT("Rollback preserves the original real-time deadline"),
		RolledBack.ResponseDeadlineUnscaled, OriginalDeadline, 0.001);
	TestFalse(TEXT("Rollback does not claim an active paired stage"),
		Fixture.Paired->IsPairedAnimationActive());
	TestEqual(TEXT("Rollback releases successor defender collision"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
	TestEqual(TEXT("Rollback releases successor source collision"),
		Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 0);
	TestFalse(TEXT("Rollback restores source hit-reaction ownership"),
		Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
	TestTrue(TEXT("Rollback retains sequence input ownership"), Fixture.Paired->IsInputBlocked());
	TestTrue(TEXT("Rollback retains sequence context ownership"),
		Fixture.DefenderCombat->HasActiveContextTag(
			KatanaCombatGameplayTags::ContextParryCounter()));
	const FCombatInputRecord& Record = Fixture.DefenderCombat->GetCombatInputHistory().Last();
	TestEqual(TEXT("Failed stage input remains ChainOnly"), Record.Route, ECombatInputRoute::ChainOnly);
	TestEqual(TEXT("Failed stage input expires"), Record.Disposition, ECombatInputDisposition::Expired);
	TestTrue(TEXT("Stale failed-stage end callback is consumed"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(CounterData->AttackerMontage, true));
	TestEqual(TEXT("Stale failed-stage callback cannot clean the response window"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterWindow);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainRetainedStageLifecycleTest,
	"KatanaCombat.Defense.Chain.RetainedStageLifecycle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainRetainedStageLifecycleTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create retained-stage fixture"));
		Fixture.Destroy();
		return false;
	}
	UPairedAnimationData* CounterData = CreateChainStageData(
		EPairedReactionType::Counter,
		EChainStageTransitionType::AutoContinue,
		TEXT("CounterImpact"),
		true);
	UPairedAnimationData* FinisherData = CreateChainStageData(EPairedReactionType::Finisher);
	FinisherData->AttackerWarpConfig.WarpTargetName = TEXT("DefenseFinisherAttacker");
	FinisherData->VictimWarpConfig.WarpTargetName = TEXT("DefenseFinisherVictim");
	CounterData->bApplySlowMotion = true;
	CounterData->SlowMotionScale = 0.5f;
	FinisherData->bApplySlowMotion = true;
	FinisherData->SlowMotionScale = 0.3f;
	Fixture.CounterAttack->CounterData = CounterData;
	Fixture.CounterAttack->FinisherData = FinisherData;
	int32 NextInstanceId = 500;
	Fixture.Paired->DefenseStagePlaybackOverrideForTesting = [&NextInstanceId](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = ++NextInstanceId;
		return true;
	};

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	TestEqual(TEXT("ChainOnly attack starts CounterActive"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterActive);
	TestEqual(TEXT("Counter stage owns one defender collision lease"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 1);
	TestEqual(TEXT("Counter stage owns one source collision lease"),
		Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 1);
	TestTrue(TEXT("Counter stage retains sequence input ownership"), Fixture.Paired->IsInputBlocked());
	TestTrue(TEXT("Counter stage retains the context tag"),
		Fixture.DefenderCombat->HasActiveContextTag(
			KatanaCombatGameplayTags::ContextParryCounter()));
	FAlignmentRequestSpec CounterDefenderAlignment;
	FAlignmentRequestSpec CounterSourceAlignment;
	const FDefenseSequenceContext& CounterSequence =
		Fixture.Paired->GetActiveDefenseSequenceContext();
	TestTrue(TEXT("Counter stage retains defender alignment ownership"),
		Fixture.Defender->TargetingComponent->GetAlignmentRequestSpec(
			CounterSequence.AttackerAlignmentLease,
			CounterDefenderAlignment));
	TestTrue(TEXT("Counter stage retains source alignment ownership"),
		Fixture.SourceAttacker->TargetingComponent->GetAlignmentRequestSpec(
			CounterSequence.VictimAlignmentLease,
			CounterSourceAlignment));
	UPairedAnimationData* ReusedRoleMontage =
		CreateChainStageData(EPairedReactionType::Finisher);
	ReusedRoleMontage->AttackerMontage = CounterData->AttackerMontage;
	FString ReusedMontageFailure;
	TestFalse(TEXT("An adjacent stage cannot reuse a same-role montage without instance identity"),
		Fixture.PreflightStage(
			ReusedRoleMontage,
			EPairedReactionType::Finisher,
			ReusedMontageFailure));
	TestTrue(TEXT("Reused-montage refusal reports an actionable reason"),
		ReusedMontageFailure.Contains(TEXT("distinct role montages")));
	const UDefenseConfiguration* SourceDefenseConfig =
		Fixture.SourceCombat->GetEffectiveDefenseConfiguration();
	TestEqual(TEXT("Counter defender alignment uses the effective defense turn rate"),
		CounterDefenderAlignment.MaximumTurnRate,
		Fixture.DefenseConfig->DefenseTurnRate);
	TestEqual(TEXT("Counter source alignment uses the effective defense turn rate"),
		CounterSourceAlignment.MaximumTurnRate,
		SourceDefenseConfig ? SourceDefenseConfig->DefenseTurnRate : 0.0f);
	constexpr float SpentDefenderBudget = 25.0f;
	constexpr float SpentSourceBudget = 30.0f;
	CounterDefenderAlignment.RemainingTurnBudget = SpentDefenderBudget;
	CounterSourceAlignment.RemainingTurnBudget = SpentSourceBudget;
	TestTrue(TEXT("Fixture can account for defender yaw already spent by the stage"),
		Fixture.Defender->TargetingComponent->UpdateAlignmentRequest(
			CounterSequence.AttackerAlignmentLease,
			CounterDefenderAlignment));
	TestTrue(TEXT("Fixture can account for source yaw already spent by the stage"),
		Fixture.SourceAttacker->TargetingComponent->UpdateAlignmentRequest(
			CounterSequence.VictimAlignmentLease,
			CounterSourceAlignment));
	UCombatEffectsWorldSubsystem* Effects =
		Fixture.World->GetSubsystem<UCombatEffectsWorldSubsystem>();
	const FTimeDilationLeaseHandle CounterTimeLease = CounterSequence.TimeDilationLease;
	TestTrue(TEXT("Counter stage owns its exact world-time lease"),
		Effects && Effects->IsLeaseActive(CounterTimeLease));
	TestEqual(TEXT("Counter stage applies its requested time scale"),
		Fixture.World->GetWorldSettings()->TimeDilation, 0.5f);
	const FCombatInputRecord& CounterInput = Fixture.DefenderCombat->GetCombatInputHistory().Last();
	TestEqual(TEXT("Successful response uses ChainOnly route"),
		CounterInput.Route, ECombatInputRoute::ChainOnly);
	TestEqual(TEXT("Successful response consumes its edge"),
		CounterInput.Disposition, ECombatInputDisposition::Consumed);
	const int32 ClearQueueCallsBeforeSuccessor =
		Fixture.DefenderCombat->GetClearQueueCallCountForTesting();

	const float HealthBeforeCounter = Fixture.SourceAttacker->CurrentHealth;
	const int32 CounterMontageId =
		Fixture.Paired->GetActiveDefenseSequenceContext().AttackerMontageInstanceId;
	Fixture.Paired->HandleChainStageTransition(
		EChainStageTransitionType::AutoContinue,
		CounterMontageId,
		MakeMarkerSource(CounterData->AttackerMontage));
	TestEqual(TEXT("Exact auto marker starts FinisherActive without exposing None"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherActive);
	TestEqual(TEXT("Counter damage commits once before the successor"),
		Fixture.SourceAttacker->CurrentHealth,
		HealthBeforeCounter - CounterData->BaseDamage,
		KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Successor replaces, rather than overlaps, defender stage lease"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 1);
	TestEqual(TEXT("Successor replaces, rather than overlaps, source stage lease"),
		Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 1);
	TestTrue(TEXT("Successor retains input ownership"), Fixture.Paired->IsInputBlocked());
	TestTrue(TEXT("Successor retains context ownership"),
		Fixture.DefenderCombat->HasActiveContextTag(
			KatanaCombatGameplayTags::ContextParryCounter()));
	TestEqual(TEXT("Successor does not clear the action queue"),
		Fixture.DefenderCombat->GetClearQueueCallCountForTesting(),
		ClearQueueCallsBeforeSuccessor);
	const FDefenseSequenceContext& FinisherSequence =
		Fixture.Paired->GetActiveDefenseSequenceContext();
	FAlignmentRequestSpec FinisherDefenderAlignment;
	FAlignmentRequestSpec FinisherSourceAlignment;
	TestTrue(TEXT("Successor retains defender alignment ownership"),
		Fixture.Defender->TargetingComponent->GetAlignmentRequestSpec(
			FinisherSequence.AttackerAlignmentLease,
			FinisherDefenderAlignment));
	TestTrue(TEXT("Successor retains source alignment ownership"),
		Fixture.SourceAttacker->TargetingComponent->GetAlignmentRequestSpec(
			FinisherSequence.VictimAlignmentLease,
			FinisherSourceAlignment));
	TestEqual(TEXT("Successor cannot raise the defender's configured turn rate"),
		FinisherDefenderAlignment.MaximumTurnRate,
		Fixture.DefenseConfig->DefenseTurnRate);
	TestEqual(TEXT("Successor cannot raise the source's configured turn rate"),
		FinisherSourceAlignment.MaximumTurnRate,
		SourceDefenseConfig ? SourceDefenseConfig->DefenseTurnRate : 0.0f);
	TestEqual(TEXT("A new defender warp target inherits the remaining yaw budget"),
		FinisherDefenderAlignment.RemainingTurnBudget,
		SpentDefenderBudget);
	TestEqual(TEXT("A new source warp target inherits the remaining yaw budget"),
		FinisherSourceAlignment.RemainingTurnBudget,
		SpentSourceBudget);
	const FAlignmentRequestHandle FinalDefenderAlignment =
		FinisherSequence.AttackerAlignmentLease;
	const FAlignmentRequestHandle FinalSourceAlignment =
		FinisherSequence.VictimAlignmentLease;
	const FTimeDilationLeaseHandle FinalTimeLease = FinisherSequence.TimeDilationLease;
	TestTrue(TEXT("Successor owns its exact world-time lease"),
		Effects && Effects->IsLeaseActive(FinalTimeLease));
	TestFalse(TEXT("Successor retires the outgoing world-time lease"),
		Effects && Effects->IsLeaseActive(CounterTimeLease));
	TestEqual(TEXT("Successor applies its own requested time scale"),
		Fixture.World->GetWorldSettings()->TimeDilation, 0.3f);
	TestTrue(TEXT("Outgoing counter end callback is consumed"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(CounterData->AttackerMontage, true));
	TestEqual(TEXT("Outgoing callback cannot clean the finisher"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherActive);

	const float HealthBeforeFinisher = Fixture.SourceAttacker->CurrentHealth;
	TestTrue(TEXT("Active finisher completion is owned by the Chain"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(FinisherData->AttackerMontage, false));
	TestEqual(TEXT("Final completion reaches terminal None"),
		Fixture.Paired->GetChainState(), EChainCounterState::None);
	TestEqual(TEXT("Finisher damage commits once"),
		Fixture.SourceAttacker->CurrentHealth,
		HealthBeforeFinisher - FinisherData->BaseDamage,
		KINDA_SMALL_NUMBER);
	TestFalse(TEXT("Terminal completion releases input"), Fixture.Paired->IsInputBlocked());
	TestFalse(TEXT("Terminal completion releases context"),
		Fixture.DefenderCombat->HasActiveContextTag(
			KatanaCombatGameplayTags::ContextParryCounter()));
	TestEqual(TEXT("Terminal completion releases defender collision"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
	TestEqual(TEXT("Terminal completion releases source collision"),
		Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 0);
	TestEqual(TEXT("Terminal completion clears defender partners"),
		Fixture.Paired->GetPairedPartnerCount(), 0);
	TestEqual(TEXT("Terminal completion clears source partners"),
		Fixture.SourcePaired->GetPairedPartnerCount(), 0);
	TestEqual(TEXT("Terminal completion clears the action queue exactly once"),
		Fixture.DefenderCombat->GetClearQueueCallCountForTesting(),
		ClearQueueCallsBeforeSuccessor + 1);
	TestFalse(TEXT("Terminal completion releases its exact world-time ownership"),
		Effects && Effects->IsLeaseActive(FinalTimeLease));
	TestEqual(TEXT("Terminal completion restores world-time baseline"),
		Fixture.World->GetWorldSettings()->TimeDilation, 1.0f);
	FAlignmentRequestSpec ReleasedAlignment;
	TestFalse(TEXT("Terminal completion releases defender alignment ownership"),
		Fixture.Defender->TargetingComponent->GetAlignmentRequestSpec(
			FinalDefenderAlignment,
			ReleasedAlignment));
	TestFalse(TEXT("Terminal completion releases source alignment ownership"),
		Fixture.SourceAttacker->TargetingComponent->GetAlignmentRequestSpec(
			FinalSourceAlignment,
			ReleasedAlignment));
	const float HealthAfterCompletion = Fixture.SourceAttacker->CurrentHealth;
	TestFalse(TEXT("Duplicate terminal callback is no longer owned"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(FinisherData->AttackerMontage, false));
	TestEqual(TEXT("Duplicate terminal callback cannot replay damage"),
		Fixture.SourceAttacker->CurrentHealth, HealthAfterCompletion);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainDuplicateIntermediateCallbackTest,
	"KatanaCombat.Defense.Chain.DuplicateIntermediateCallback",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainDuplicateIntermediateCallbackTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create duplicate-intermediate-callback fixture"));
		Fixture.Destroy();
		return false;
	}
	UPairedAnimationData* CounterData = CreateChainStageData(EPairedReactionType::Counter);
	UPairedAnimationData* FinisherData = CreateChainStageData(EPairedReactionType::Finisher);
	CounterData->bIsLethal = true;
	Fixture.CounterAttack->CounterData = CounterData;
	Fixture.CounterAttack->FinisherData = FinisherData;
	int32 NextInstanceId = 650;
	Fixture.SetPlaybackOverride([&NextInstanceId, FinisherData](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = ++NextInstanceId;
		return Data != FinisherData || Role != EPairedAnimationRole::Victim;
	});

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	TestEqual(TEXT("Counter starts before its normal completion callback"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterActive);
	Fixture.SourceAttacker->CurrentHealth = 5.0f;
	const int32 QueueClearsBeforeCompletion =
		Fixture.DefenderCombat->GetClearQueueCallCountForTesting();
	TestTrue(TEXT("First counter completion callback is sequence-owned"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(
			CounterData->AttackerMontage, false));
	TestEqual(TEXT("First counter completion enters FinisherReady"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherReady);
	TestEqual(TEXT("Default Chain counter damage cannot kill a low-health source"),
		Fixture.SourceAttacker->CurrentHealth,
		1.0f,
		KINDA_SMALL_NUMBER);
	TestFalse(TEXT("Authored-lethal counter data remains nonlethal without the explicit opt-in"),
		Fixture.SourceAttacker->IsDeadOrDying());

	TestTrue(TEXT("Duplicate counter completion callback remains sequence-owned"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(
			CounterData->AttackerMontage, false));
	TestEqual(TEXT("Duplicate completion cannot terminate FinisherReady"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherReady);
	TestEqual(TEXT("Duplicate completion cannot replay counter damage"),
		Fixture.SourceAttacker->CurrentHealth,
		1.0f,
		KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Duplicate completion retains sequence input ownership"),
		Fixture.Paired->IsInputBlocked());
	TestEqual(TEXT("Duplicate completion cannot clear the action queue"),
		Fixture.DefenderCombat->GetClearQueueCallCountForTesting(),
		QueueClearsBeforeCompletion);

	const int32 GenerationBeforeFailedFinisher =
		Fixture.Paired->GetActiveDefenseSequenceContext().StageGeneration;
	Fixture.DefenderCombat->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Press);
	TestEqual(TEXT("A partial finisher start rolls back to FinisherReady"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherReady);
	TestNotEqual(TEXT("Rollback advances the stage generation"),
		Fixture.Paired->GetActiveDefenseSequenceContext().StageGeneration,
		GenerationBeforeFailedFinisher);
	TestTrue(TEXT("The stale counter callback remains owned after rollback rekeying"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(
			CounterData->AttackerMontage, false));
	TestEqual(TEXT("The rekeyed stale callback cannot terminate FinisherReady"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherReady);
	TestEqual(TEXT("The rekeyed stale callback cannot replay counter damage"),
		Fixture.SourceAttacker->CurrentHealth,
		1.0f,
		KINDA_SMALL_NUMBER);
	TestEqual(TEXT("The rekeyed stale callback cannot clear the action queue"),
		Fixture.DefenderCombat->GetClearQueueCallCountForTesting(),
		QueueClearsBeforeCompletion);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainLethalCounterOptInTest,
	"KatanaCombat.Defense.Chain.LethalCounterOptIn",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainLethalCounterOptInTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create lethal-counter fixture"));
		Fixture.Destroy();
		return false;
	}

	UPairedAnimationData* CounterData = CreateChainStageData(EPairedReactionType::Counter);
	CounterData->bIsLethal = true;
	Fixture.CounterAttack->CounterData = CounterData;
	Fixture.EnableLethalCounterData();
	Fixture.SetPlaybackOverride([](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = Role == EPairedAnimationRole::Attacker ? 681 : 682;
		return true;
	});

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	TestEqual(TEXT("Opted-in lethal counter reaches CounterActive"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterActive);
	Fixture.SourceAttacker->CurrentHealth = 5.0f;
	TestTrue(TEXT("Lethal counter completion callback is sequence-owned"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(
			CounterData->AttackerMontage, false));
	TestTrue(TEXT("Explicit lethal opt-in permits authored counter data to kill"),
		Fixture.SourceAttacker->IsDeadOrDying());
	TestEqual(TEXT("Death during damage performs terminal Chain cleanup"),
		Fixture.Paired->GetChainState(), EChainCounterState::None);
	TestFalse(TEXT("Death cleanup releases sequence input ownership"),
		Fixture.Paired->IsInputBlocked());
	TestFalse(TEXT("Death cleanup releases sequence context ownership"),
		Fixture.DefenderCombat->HasActiveContextTag(
			KatanaCombatGameplayTags::ContextParryCounter()));
	TestFalse(TEXT("Death cleanup releases the retained interaction"),
		Fixture.Paired->GetActiveDefenseSequenceContext().OriginatingInteraction.IsValid());

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainRetryableFinisherTest,
	"KatanaCombat.Defense.Chain.RetryableFinisher",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainRetryableFinisherTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create retryable-finisher fixture"));
		Fixture.Destroy();
		return false;
	}
	UPairedAnimationData* CounterData = CreateChainStageData(
		EPairedReactionType::Counter,
		EChainStageTransitionType::AutoContinue,
		TEXT("AutoFinisher"),
		true);
	UPairedAnimationData* FinisherData = CreateChainStageData(EPairedReactionType::Finisher);
	Fixture.CounterAttack->CounterData = CounterData;
	Fixture.CounterAttack->FinisherData = FinisherData;
	bool bFailFinisherVictim = true;
	int32 NextInstanceId = 700;
	Fixture.Paired->DefenseStagePlaybackOverrideForTesting = [
		&bFailFinisherVictim,
		&NextInstanceId,
		FinisherData](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = ++NextInstanceId;
		return !(Data == FinisherData
			&& Role == EPairedAnimationRole::Victim
			&& bFailFinisherVictim);
	};

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	const int32 CounterMontageId =
		Fixture.Paired->GetActiveDefenseSequenceContext().AttackerMontageInstanceId;
	Fixture.Paired->HandleChainStageTransition(
		EChainStageTransitionType::AutoContinue,
		CounterMontageId,
		MakeMarkerSource(CounterData->AttackerMontage));
	TestEqual(TEXT("Retryable partial finisher failure enters FinisherReady"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherReady);
	TestEqual(TEXT("Rollback retains counter defender collision ownership"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 1);
	TestEqual(TEXT("Rollback retains counter source collision ownership"),
		Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 1);
	TestTrue(TEXT("FinisherReady retains sequence input ownership"), Fixture.Paired->IsInputBlocked());
	TestTrue(TEXT("FinisherReady owns a fresh real-time deadline"),
		Fixture.Paired->GetActiveDefenseSequenceContext().ResponseDeadlineUnscaled
			> FPlatformTime::Seconds());

	bFailFinisherVictim = false;
	Fixture.DefenderCombat->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Press);
	TestEqual(TEXT("A later physical input can retry the finisher"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherActive);
	const FCombatInputRecord& RetryInput = Fixture.DefenderCombat->GetCombatInputHistory().Last();
	TestEqual(TEXT("Finisher retry remains ChainOnly"),
		RetryInput.Route, ECombatInputRoute::ChainOnly);
	TestEqual(TEXT("Successful finisher retry consumes only its own edge"),
		RetryInput.Disposition, ECombatInputDisposition::Consumed);
	Fixture.Paired->HandleOwnerPairedMontageEnded(FinisherData->AttackerMontage, false);
	TestEqual(TEXT("Retried finisher completes terminally"),
		Fixture.Paired->GetChainState(), EChainCounterState::None);

	Fixture.Destroy();

	FDefenseChainFixture NonRetryable;
	if (!NonRetryable.Initialize()
		|| !NonRetryable.StartCommittedParry()
		|| !NonRetryable.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create non-retryable finisher fixture"));
		NonRetryable.Destroy();
		return false;
	}
	UPairedAnimationData* NonRetryCounter = CreateChainStageData(
		EPairedReactionType::Counter,
		EChainStageTransitionType::AutoContinue,
		TEXT("NonRetryableFinisher"),
		true);
	NonRetryCounter->ChainTransitionPolicy.bFinisherRetryable = false;
	UPairedAnimationData* InvalidFinisher = CreateChainStageData(EPairedReactionType::Finisher);
	NonRetryable.CounterAttack->CounterData = NonRetryCounter;
	NonRetryable.CounterAttack->FinisherData = InvalidFinisher;
	int32 NonRetryInstanceId = 800;
	NonRetryable.SetPlaybackOverride([InvalidFinisher, &NonRetryInstanceId](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = ++NonRetryInstanceId;
		return Data != InvalidFinisher || Role != EPairedAnimationRole::Victim;
	});
	NonRetryable.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	NonRetryable.Paired->HandleChainStageTransition(
		EChainStageTransitionType::AutoContinue,
		NonRetryable.Paired->GetActiveDefenseSequenceContext().AttackerMontageInstanceId,
		MakeMarkerSource(NonRetryCounter->AttackerMontage));
	TestEqual(TEXT("A non-retryable automatic finisher failure cleans up terminally"),
		NonRetryable.Paired->GetChainState(), EChainCounterState::None);
	TestFalse(TEXT("Non-retryable failure releases input ownership"),
		NonRetryable.Paired->IsInputBlocked());
	TestFalse(TEXT("Non-retryable failure releases context ownership"),
		NonRetryable.DefenderCombat->HasActiveContextTag(
			KatanaCombatGameplayTags::ContextParryCounter()));
	NonRetryable.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainParticipantDeathTest,
	"KatanaCombat.Defense.Chain.ParticipantDeath",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainParticipantDeathTest::RunTest(const FString& Parameters)
{
	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create partner-death fixture"));
			Fixture.Destroy();
			return false;
		}
		Fixture.SourceAttacker->bIsDying = true;
		Fixture.SourceAttacker->FinalizeDeath();
		TestEqual(TEXT("Source death cancels a no-montage retained sequence"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("Source death releases input"), Fixture.Paired->IsInputBlocked());
		TestFalse(TEXT("Source death releases context"),
			Fixture.DefenderCombat->HasActiveContextTag(
				KatanaCombatGameplayTags::ContextParryCounter()));
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create owner-death fixture"));
			Fixture.Destroy();
			return false;
		}
		Fixture.Defender->bIsDying = true;
		Fixture.Defender->FinalizeDeath();
		TestEqual(TEXT("Owner death cancels the retained sequence"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("Owner death releases input"), Fixture.Paired->IsInputBlocked());
		TestEqual(TEXT("Owner death clears source partner registration"),
			Fixture.SourcePaired->GetPairedPartnerCount(), 0);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create raw participant-destruction fixture"));
			Fixture.Destroy();
			return false;
		}
		Fixture.SourceAttacker->Destroy();
		TestEqual(TEXT("Raw source destruction cancels the retained sequence"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("Raw source destruction releases input"), Fixture.Paired->IsInputBlocked());
		TestFalse(TEXT("Raw source destruction releases context"),
			Fixture.DefenderCombat->HasActiveContextTag(
				KatanaCombatGameplayTags::ContextParryCounter()));
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create raw owner-destruction fixture"));
			Fixture.Destroy();
			return false;
		}
		Fixture.Defender->Destroy();
		TestEqual(TEXT("Raw owner destruction cancels the retained sequence"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("Raw owner destruction releases input"), Fixture.Paired->IsInputBlocked());
		TestEqual(TEXT("Raw owner destruction clears source partner registration"),
			Fixture.SourcePaired->GetPairedPartnerCount(), 0);
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainStagePreflightGeometryTest,
	"KatanaCombat.Defense.Chain.StagePreflightGeometry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainStagePreflightGeometryTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create stage-preflight fixture"));
		Fixture.Destroy();
		return false;
	}
	Fixture.SetPlaybackOverride([](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = Role == EPairedAnimationRole::Attacker ? 901 : 902;
		return true;
	});
	UPairedAnimationData* CounterData = CreateChainStageData(EPairedReactionType::Counter);
	FString FailureReason;
	UAnimMontage* VictimMontage = CounterData->VictimMontage;
	CounterData->VictimMontage = nullptr;
	TestFalse(TEXT("A successor with a missing role montage fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Montage refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("montage")));
	CounterData->VictimMontage = VictimMontage;
	CounterData->VictimBlendOut = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("A successor with a nonfinite playback value fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Numeric refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("numeric")));
	CounterData->VictimBlendOut = 0.2f;
	CounterData->BaseDamage = TNumericLimits<float>::Max();
	CounterData->DamageMultiplier = 10.0f;
	TestFalse(TEXT("A successor with an unrepresentable damage product fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Damage overflow refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("numeric")));
	CounterData->BaseDamage = 100.0f;
	CounterData->DamageMultiplier = 1.0f;
	CounterData->AttackerWarpConfig.bWarpRotation = false;
	FailureReason.Reset();
	TestFalse(TEXT("A successor role without rotation warping fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Stage rotation-warp refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("rotation warp")));
	CounterData->AttackerWarpConfig.bWarpRotation = true;
	Fixture.SetPendingRoleMontageCallback(
		CounterData->AttackerMontage,
		EPairedAnimationRole::Attacker,
		true);
	FailureReason.Reset();
	TestFalse(TEXT("A successor with an unresolved prior callback fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Stage callback refusal reports the ownership ambiguity"),
		FailureReason.Contains(TEXT("unresolved prior callback")));
	Fixture.SetPendingRoleMontageCallback(
		CounterData->AttackerMontage,
		EPairedAnimationRole::Attacker,
		false);

	CounterData->MaxTriggerDistance = 200.0f;
	TestFalse(TEXT("A successor outside its maximum trigger range fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Range refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("trigger range")));

	CounterData->MaxTriggerDistance = 300.0f;
	CounterData->VictimWarpConfig.MaxWarpDistance = 100.0f;
	TestFalse(TEXT("A role exceeding its authored warp budget fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Warp refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("translation budget")));

	CounterData->VictimWarpConfig.MaxWarpDistance = 300.0f;
	AActor* Obstacle = Fixture.World->SpawnActor<AActor>();
	UBoxComponent* BlockingBox = Obstacle ? NewObject<UBoxComponent>(Obstacle) : nullptr;
	if (Obstacle && BlockingBox)
	{
		Obstacle->SetRootComponent(BlockingBox);
		Obstacle->AddInstanceComponent(BlockingBox);
		BlockingBox->SetBoxExtent(FVector(25.0f, 100.0f, 100.0f));
		BlockingBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BlockingBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		BlockingBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		BlockingBox->RegisterComponent();
		Obstacle->SetActorLocation(FVector(175.0f, 0.0f, 0.0f));
	}
	TestFalse(TEXT("A blocked participant or warp sweep fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Sweep refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("blocked")));

	if (BlockingBox)
	{
		BlockingBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Fixture.SourceAttacker->SetActorLocation(FVector(10.0f, 0.0f, 0.0f));
	FailureReason.Reset();
	TestTrue(TEXT("A retained successor may start below the initial minimum range"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));

	Fixture.SourceAttacker->SetActorLocationAndRotation(
		FVector(250.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator);
	CounterData->AttackerWarpConfig.bWarpTranslation = false;
	CounterData->VictimWarpConfig.bWarpTranslation = false;
	FailureReason.Reset();
	TestFalse(TEXT("A role that cannot face its partner within the remaining yaw budget fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Rotation refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("rotation budget")));

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainSourceMontageOwnershipTest,
	"KatanaCombat.Defense.Chain.SourceMontageOwnership",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainSourceMontageOwnershipTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create source-montage fixture"));
		Fixture.Destroy();
		return false;
	}
	UPairedAnimationData* CounterData = CreateChainStageData(EPairedReactionType::Counter);
	Fixture.CounterAttack->CounterData = CounterData;
	Fixture.SetPlaybackOverride([](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = Role == EPairedAnimationRole::Attacker ? 911 : 912;
		return true;
	});

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	TestEqual(TEXT("Counter stage starts before the source callback"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterActive);
	TestTrue(TEXT("The source component forwards its victim-role callback to the sequence owner"),
		Fixture.SourcePaired->HandleOwnerPairedMontageEnded(
			CounterData->VictimMontage, true));
	TestEqual(TEXT("An interrupted source montage performs terminal sequence cleanup"),
		Fixture.Paired->GetChainState(), EChainCounterState::None);
	TestFalse(TEXT("Source interruption releases sequence input"), Fixture.Paired->IsInputBlocked());
	TestEqual(TEXT("Source interruption releases source collision ownership"),
		Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 0);

	Fixture.Destroy();

	FDefenseChainFixture NormalCompletion;
	if (!NormalCompletion.Initialize()
		|| !NormalCompletion.StartCommittedParry()
		|| !NormalCompletion.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create source normal-completion fixture"));
		NormalCompletion.Destroy();
		return false;
	}
	UPairedAnimationData* NormalCounterData = CreateChainStageData(EPairedReactionType::Counter);
	NormalCompletion.CounterAttack->CounterData = NormalCounterData;
	NormalCompletion.SetPlaybackOverride([](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = Role == EPairedAnimationRole::Attacker ? 921 : 922;
		return true;
	});
	NormalCompletion.DefenderCombat->OnInputEvent(
		EInputType::LightAttack,
		EInputEventType::Press);
	const float HealthBefore = NormalCompletion.SourceAttacker->CurrentHealth;
	TestTrue(TEXT("A normal source-role end is deferred for same-frame owner ordering"),
		NormalCompletion.SourcePaired->HandleOwnerPairedMontageEnded(
			NormalCounterData->VictimMontage, false));
	TestEqual(TEXT("The source callback alone does not cancel the active counter"),
		NormalCompletion.Paired->GetChainState(), EChainCounterState::CounterActive);
	TestTrue(TEXT("The authoritative owner callback completes the counter"),
		NormalCompletion.Paired->HandleOwnerPairedMontageEnded(
			NormalCounterData->AttackerMontage, false));
	TestTrue(TEXT("Same-frame callback ordering still applies counter damage"),
		NormalCompletion.SourceAttacker->CurrentHealth < HealthBefore);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Deferred source verification cannot reopen or recancel completion"),
		NormalCompletion.Paired->GetChainState(), EChainCounterState::None);
	NormalCompletion.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainCleanupListenerReentryTest,
	"KatanaCombat.Defense.Chain.CleanupListenerReentry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainCleanupListenerReentryTest::RunTest(const FString& Parameters)
{
	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create listener-action fixture"));
			Fixture.Destroy();
			return false;
		}
		UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
		Recorder->bBeginBlockOnPairedEnded = true;
		Recorder->PairedActionTarget = Fixture.Defender;
		Fixture.Paired->OnPairedAnimationEnded.AddDynamic(
			Recorder, &UCombatEventRecorder::HandlePairedAnimationEnded);
		Fixture.DefenderCombat->EndBlock();
		Fixture.Paired->CancelPairedAnimation(0.0f);
		TestEqual(TEXT("Terminal cleanup broadcasts exactly once"),
			Recorder->PairedAnimationEndedCount, 1);
		TestTrue(TEXT("A listener may start a new action after cleanup released ownership"),
			Recorder->bBeginBlockOnPairedEndedResult);
		TestTrue(TEXT("The listener-started block remains active after broadcast returns"),
			Fixture.DefenderCombat->IsBlocking());
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create listener-destruction fixture"));
			Fixture.Destroy();
			return false;
		}
		UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
		Recorder->bDestroyOnPairedEnded = true;
		Recorder->ActorToDestroy = Fixture.Defender;
		Fixture.Paired->OnPairedAnimationEnded.AddDynamic(
			Recorder, &UCombatEventRecorder::HandlePairedAnimationEnded);
		Fixture.Paired->CancelPairedAnimation(0.0f);
		TestEqual(TEXT("Destructive listener still observes one terminal event"),
			Recorder->PairedAnimationEndedCount, 1);
		TestTrue(TEXT("Destroying the sequence owner from the terminal listener is safe"),
			Fixture.Defender->IsActorBeingDestroyed());
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainCompetingPairedStartRejectedTest,
	"KatanaCombat.Defense.Chain.CompetingPairedStartRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainCompetingPairedStartRejectedTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
	{
		AddError(TEXT("Failed to create competing-paired-start fixture"));
		Fixture.Destroy();
		return false;
	}

	AEnemyCharacter* CompetingTarget = FCombatTestHelpers::CreateTestEnemyCharacter(
		Fixture.World,
		FVector(0.0f, 250.0f, 0.0f));
	UPairedAnimationData* CompetingData = CreateChainStageData(
		EPairedReactionType::Finisher);
	const FDefenseInteractionId InteractionBefore =
		Fixture.Paired->GetActiveDefenseSequenceContext().OriginatingInteraction;
	TestFalse(TEXT("A generic paired start cannot replace a retained defense sequence"),
		Fixture.TryCompetingPairedStart(
			CompetingTarget,
			CompetingData,
			EPairedReactionType::Finisher));
	TestEqual(TEXT("Rejected competing start preserves the active Chain state"),
		Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);
	TestTrue(TEXT("Rejected competing start preserves the exact interaction"),
		Fixture.Paired->GetActiveDefenseSequenceContext().OriginatingInteraction
			== InteractionBefore);
	TestTrue(TEXT("Rejected competing start preserves the source partner"),
		Fixture.Paired->IsPairedPartner(Fixture.SourceAttacker));
	TestFalse(TEXT("Rejected competing start does not register another target"),
		Fixture.Paired->IsPairedPartner(CompetingTarget));

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainCancellationMatrixTest,
	"KatanaCombat.Defense.Chain.CancellationMatrix",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainCancellationMatrixTest::RunTest(const FString& Parameters)
{
	auto AssertTerminalCancellation = [this](
		FDefenseChainFixture& Fixture,
		const EChainCounterState ExpectedState)
	{
		TestEqual(TEXT("Fixture reached the requested cancellable state"),
			Fixture.Paired->GetChainState(), ExpectedState);
		Fixture.Paired->CancelPairedAnimation(0.0f);
		TestEqual(TEXT("Cancellation reaches terminal None"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("Cancellation releases retained interaction"),
			Fixture.Paired->GetActiveDefenseSequenceContext().OriginatingInteraction.IsValid());
		TestFalse(TEXT("Cancellation releases input ownership"), Fixture.Paired->IsInputBlocked());
		TestFalse(TEXT("Cancellation releases context ownership"),
			Fixture.DefenderCombat->HasActiveContextTag(
				KatanaCombatGameplayTags::ContextParryCounter()));
		TestEqual(TEXT("Cancellation releases defender stage leases"),
			Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
		TestEqual(TEXT("Cancellation releases source stage leases"),
			Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 0);
		Fixture.Paired->CancelPairedAnimation(0.0f);
		TestEqual(TEXT("Duplicate cancellation is idempotent"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
	};

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create ParryActive cancellation fixture"));
			Fixture.Destroy();
			return false;
		}
		AssertTerminalCancellation(Fixture, EChainCounterState::ParryActive);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
		{
			AddError(TEXT("Failed to create CounterWindow cancellation fixture"));
			Fixture.Destroy();
			return false;
		}
		AssertTerminalCancellation(Fixture, EChainCounterState::CounterWindow);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
		{
			AddError(TEXT("Failed to create CounterActive cancellation fixture"));
			Fixture.Destroy();
			return false;
		}
		UPairedAnimationData* CounterData = CreateChainStageData(EPairedReactionType::Counter);
		Fixture.CounterAttack->CounterData = CounterData;
		Fixture.SetPlaybackOverride([](
			const EPairedAnimationRole Role,
			const UPairedAnimationData* Data,
			int32& OutInstanceId)
		{
			OutInstanceId = Role == EPairedAnimationRole::Attacker ? 921 : 922;
			return true;
		});
		Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		AssertTerminalCancellation(Fixture, EChainCounterState::CounterActive);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
		{
			AddError(TEXT("Failed to create FinisherReady cancellation fixture"));
			Fixture.Destroy();
			return false;
		}
		UPairedAnimationData* CounterData = CreateChainStageData(
			EPairedReactionType::Counter,
			EChainStageTransitionType::AutoContinue,
			TEXT("ReadyCancel"),
			true);
		UPairedAnimationData* FinisherData = CreateChainStageData(EPairedReactionType::Finisher);
		Fixture.CounterAttack->CounterData = CounterData;
		Fixture.CounterAttack->FinisherData = FinisherData;
		Fixture.SetPlaybackOverride([FinisherData](
			const EPairedAnimationRole Role,
			const UPairedAnimationData* Data,
			int32& OutInstanceId)
		{
			OutInstanceId = Role == EPairedAnimationRole::Attacker ? 931 : 932;
			return Data != FinisherData || Role != EPairedAnimationRole::Victim;
		});
		Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		Fixture.Paired->HandleChainStageTransition(
			EChainStageTransitionType::AutoContinue,
			Fixture.Paired->GetActiveDefenseSequenceContext().AttackerMontageInstanceId,
			MakeMarkerSource(CounterData->AttackerMontage));
		AssertTerminalCancellation(Fixture, EChainCounterState::FinisherReady);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
		{
			AddError(TEXT("Failed to create FinisherActive cancellation fixture"));
			Fixture.Destroy();
			return false;
		}
		UPairedAnimationData* CounterData = CreateChainStageData(
			EPairedReactionType::Counter,
			EChainStageTransitionType::AutoContinue,
			TEXT("ActiveCancel"),
			true);
		UPairedAnimationData* FinisherData = CreateChainStageData(EPairedReactionType::Finisher);
		Fixture.CounterAttack->CounterData = CounterData;
		Fixture.CounterAttack->FinisherData = FinisherData;
		int32 NextInstanceId = 940;
		Fixture.SetPlaybackOverride([&NextInstanceId](
			const EPairedAnimationRole Role,
			const UPairedAnimationData* Data,
			int32& OutInstanceId)
		{
			OutInstanceId = ++NextInstanceId;
			return true;
		});
		Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		Fixture.Paired->HandleChainStageTransition(
			EChainStageTransitionType::AutoContinue,
			Fixture.Paired->GetActiveDefenseSequenceContext().AttackerMontageInstanceId,
			MakeMarkerSource(CounterData->AttackerMontage));
		AssertTerminalCancellation(Fixture, EChainCounterState::FinisherActive);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create legacy API isolation fixture"));
			Fixture.Destroy();
			return false;
		}
		UPairedAnimationData* UnrelatedData = CreateChainStageData(
			EPairedReactionType::Finisher);
		Fixture.Paired->BeginPairedAnimation(
			UnrelatedData,
			EPairedReactionType::Finisher,
			true);
		TestEqual(TEXT("Legacy begin cannot replace an active defense sequence"),
			Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);
		TestNull(TEXT("Legacy begin cannot install unrelated paired data"),
			Fixture.Paired->ActivePairedAnimData.Get());
		Fixture.Paired->EndPairedAnimation();
		TestEqual(TEXT("Legacy end routes active defense ownership through terminal cleanup"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("Legacy end releases defense input ownership"),
			Fixture.Paired->IsInputBlocked());
		TestFalse(TEXT("Legacy end releases defense context ownership"),
			Fixture.DefenderCombat->HasActiveContextTag(
				KatanaCombatGameplayTags::ContextParryCounter()));
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainSharedNotifyAcrossActorsTest,
	"KatanaCombat.Defense.Chain.SharedNotifyAcrossActors",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainSharedNotifyAcrossActorsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* First = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* FirstPartner = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(100.0f, 0.0f, 0.0f));
	APlayerCharacter* Second = FCombatTestHelpers::CreateTestPlayerCharacter(
		World, FVector(0.0f, 500.0f, 0.0f));
	AEnemyCharacter* SecondPartner = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(100.0f, 500.0f, 0.0f));
	UPairedAnimationComponent* FirstPaired = First ? First->PairedAnimationComponent.Get() : nullptr;
	UPairedAnimationComponent* SecondPaired = Second ? Second->PairedAnimationComponent.Get() : nullptr;
	if (!FirstPaired || !SecondPaired || !FirstPartner || !SecondPartner)
	{
		AddError(TEXT("Failed to create shared-notify actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	FirstPaired->AddPairedPartner(FirstPartner);
	SecondPaired->AddPairedPartner(SecondPartner);
	FAnimNotifyRuntimeSourceId SharedSource;
	SharedSource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Chain/SharedNotify"));
	SharedSource.NotifyEventIndex = 4;
	const EMovementMode FirstBaseline = First->GetCharacterMovement()->MovementMode;
	const EMovementMode SecondBaseline = Second->GetCharacterMovement()->MovementMode;

	TestTrue(TEXT("First actor acquires the shared notify identity"),
		FirstPaired->BeginPairedCollisionNotify(
			SharedSource, 77, true, true, false, true, false, 150.0f));
	TestTrue(TEXT("Second actor independently acquires the same notify identity"),
		SecondPaired->BeginPairedCollisionNotify(
			SharedSource, 77, true, true, false, true, false, 150.0f));
	FirstPaired->EndPairedCollisionNotify(SharedSource, 77);
	TestEqual(TEXT("First End restores only the first actor"),
		First->GetCharacterMovement()->MovementMode.GetValue(), FirstBaseline);
	TestEqual(TEXT("First End cannot restore the second actor"),
		Second->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
	SecondPaired->EndPairedCollisionNotify(SharedSource, 77);
	TestEqual(TEXT("Second exact End restores its own baseline"),
		Second->GetCharacterMovement()->MovementMode.GetValue(), SecondBaseline);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
