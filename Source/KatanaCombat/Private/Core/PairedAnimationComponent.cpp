// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/PairedAnimationComponent.h"
#include "Utilities/AlignmentMotionLibrary.h"
#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Core/HitReactionComponent.h"
#include "Characters/BaseCombatCharacter.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/DamageableInterface.h"
#include "Interfaces/TeamMemberInterface.h"
#include "Data/PairedAnimationData.h"
#include "Data/AttackData.h"
#include "Data/CombatFXData.h"
#include "Data/DefenseConfiguration.h"
#include "Data/TargetingSettings.h"
#include "Defense/DefensePresentationSelector.h"
#include "Debug/ActionReactionTelemetry.h"
#include "Debug/DebugConfig.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "Utilities/CombatGameplayTags.h"
#include "Utilities/PairedAnimationUtilityLibrary.h"
#include "Subsystems/CombatEffectsWorldSubsystem.h"
#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "Animation/AnimNotify_ChainStageTransition.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraSystem.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"
#include "Utilities/CombatMath.h"

// ============================================================================
// LOG CATEGORY DEFINITION
// ============================================================================

DEFINE_LOG_CATEGORY(LogPairedAnim);

namespace
{
FDefensePresentationSelectionContext BuildDefenseBridgeSelectionContext(
	const FDefenseResolution& Resolution)
{
	FDefensePresentationSelectionContext Context;
	Context.Outcome = Resolution.Decision.Outcome;
	Context.AttackerResponse = Resolution.Decision.AttackerResponse;
	Context.Height = Resolution.Decision.Height;
	Context.Lane = Resolution.Decision.Lane;
	Context.SwingShape = Resolution.Decision.SwingShape;
	Context.bPairedBridgeUsable = true;
	if (Resolution.Decision.SelectedAttack)
	{
		Context.AttackTags = Resolution.Decision.SelectedAttack->AttackTags;
	}
	return Context;
}

bool HasValidPairedRuntimeNumerics(const UPairedAnimationData& Data)
{
	if (Data.Entry.bEnabled && (!AlignmentMotion::IsValid(Data.Entry.Limits) || !AlignmentMotion::IsValidGoal(Data.Entry.VictimRelativeTransform))) { return false; }
	const bool bFinitePlayback = FMath::IsFinite(Data.SyncPointTime)
		&& Data.SyncPointTime >= 0.0f
		&& FMath::IsFinite(Data.VictimStartOffset)
		&& FMath::IsFinite(Data.AttackerBlendIn)
		&& Data.AttackerBlendIn >= 0.0f
		&& FMath::IsFinite(Data.AttackerBlendOut)
		&& Data.AttackerBlendOut >= 0.0f
		&& FMath::IsFinite(Data.VictimBlendIn)
		&& Data.VictimBlendIn >= 0.0f
		&& FMath::IsFinite(Data.VictimBlendOut)
		&& Data.VictimBlendOut >= 0.0f
		&& FMath::IsFinite(Data.RagdollBlendTime)
		&& Data.RagdollBlendTime >= 0.0f;
	const bool bFiniteDamage = FMath::IsFinite(Data.BaseDamage)
		&& Data.BaseDamage >= 0.0f
		&& FMath::IsFinite(Data.DamageMultiplier)
		&& Data.DamageMultiplier >= 0.0f
		&& static_cast<double>(Data.BaseDamage) * static_cast<double>(Data.DamageMultiplier)
			<= static_cast<double>(TNumericLimits<float>::Max());
	const bool bFinitePositioning = !Data.VictimRelativePosition.ContainsNaN()
		&& !Data.VictimRelativeRotation.ContainsNaN()
		&& Data.VictimFacingMode >= -1
		&& Data.VictimFacingMode <= 1
		&& FMath::IsFinite(Data.MaxWarpDistance)
		&& Data.MaxWarpDistance >= 0.0f
		&& FMath::IsFinite(Data.MinTriggerDistance)
		&& Data.MinTriggerDistance >= 0.0f
		&& FMath::IsFinite(Data.MaxTriggerDistance)
		&& Data.MaxTriggerDistance > Data.MinTriggerDistance
		&& FMath::IsFinite(Data.AttackerWarpConfig.MaxWarpDistance)
		&& Data.AttackerWarpConfig.MaxWarpDistance >= 0.0f
		&& !Data.AttackerWarpConfig.RelativeOffset.ContainsNaN()
		&& UPairedAnimationUtilityLibrary::IsValidFacingPolicy(Data.AttackerWarpConfig.FacingPolicy)
		&& FMath::IsFinite(Data.VictimWarpConfig.MaxWarpDistance)
		&& Data.VictimWarpConfig.MaxWarpDistance >= 0.0f
		&& !Data.VictimWarpConfig.RelativeOffset.ContainsNaN()
		&& UPairedAnimationUtilityLibrary::IsValidFacingPolicy(Data.VictimWarpConfig.FacingPolicy)
		&& FMath::IsFinite(Data.ChainTransitionPolicy.ResponseWindowOverride)
		&& Data.ChainTransitionPolicy.ResponseWindowOverride >= 0.0f;
	const bool bFiniteEffects = !Data.bApplySlowMotion
		|| (FMath::IsFinite(Data.SlowMotionScale)
			&& Data.SlowMotionScale >= 0.0f
			&& Data.SlowMotionScale <= 1.0f
			&& FMath::IsFinite(Data.SlowMotionDuration)
			&& Data.SlowMotionDuration >= 0.0f);
	return bFinitePlayback && bFiniteDamage && bFinitePositioning && bFiniteEffects;
}

float GetAbsolutePairedYaw(const AActor* Actor, const AActor* Target, EPairedFacingPolicy Policy)
{
	if (!Actor || !Target)
	{
		return TNumericLimits<float>::Max();
	}

	const float DesiredYaw = UPairedAnimationUtilityLibrary::ResolvePairedFacingRotation(
		Actor->GetActorLocation(), Actor->GetActorRotation(), Target->GetActorTransform(), Policy).Yaw;
	return FMath::Abs(FMath::FindDeltaAngleDegrees(Actor->GetActorRotation().Yaw, DesiredYaw));
}

struct FDefenseStageAlignmentLimits
{
	float MaximumTurnRate = 0.0f;
	float RemainingTurnBudget = 0.0f;
};

FDefenseStageAlignmentLimits ResolveDefenseStageAlignmentLimits(
	const UCombatComponent* Combat,
	const UTargetingComponent* Targeting,
	const FAlignmentRequestHandle ExistingHandle,
	const float InitialBudgetCap)
{
	const UDefenseConfiguration* Configuration = Combat
		? Combat->GetEffectiveDefenseConfiguration()
		: GetDefault<UDefenseConfiguration>();
	const float ConfiguredRate = Configuration
		&& FMath::IsFinite(Configuration->DefenseTurnRate)
		? FMath::Max(0.0f, Configuration->DefenseTurnRate)
		: 180.0f;
	const float ConfiguredBudget = Configuration
		&& FMath::IsFinite(Configuration->MaximumAutomaticTurn)
		? FMath::Max(0.0f, Configuration->MaximumAutomaticTurn)
		: 70.0f;

	FDefenseStageAlignmentLimits Limits;
	Limits.MaximumTurnRate = ConfiguredRate;
	Limits.RemainingTurnBudget = FMath::Min(
		ConfiguredBudget,
		FMath::IsFinite(InitialBudgetCap)
			? FMath::Max(0.0f, InitialBudgetCap)
			: ConfiguredBudget);

	FAlignmentRequestSpec ExistingSpec;
	if (ExistingHandle.IsValid()
		&& Targeting
		&& Targeting->GetAlignmentRequestSpec(ExistingHandle, ExistingSpec))
	{
		Limits.MaximumTurnRate = FMath::Min(
			Limits.MaximumTurnRate,
			ExistingSpec.MaximumTurnRate);
		Limits.RemainingTurnBudget = FMath::Min(
			Limits.RemainingTurnBudget,
			ExistingSpec.RemainingTurnBudget);
	}
	return Limits;
}

FName DefenseStageName(const EChainCounterState State)
{
	const UEnum* Enum = StaticEnum<EChainCounterState>();
	return Enum
		? FName(*Enum->GetNameStringByValue(static_cast<int64>(State)))
		: NAME_None;
}

void AppendPairedStageActionReactionTelemetry(
	UCombatComponent* Sink,
	const FDefenseSequenceContext& Sequence,
	const EActionReactionTelemetryEvent Event,
	const EActionReactionTelemetryReason Reason,
	const UPairedAnimationData* StageData,
	AActor* ReportingActor,
	const int32 MontageInstanceId,
	const FAnimNotifyRuntimeSourceId* NotifySource,
	FString Detail)
{
	if (!Sink && Sequence.Defender.IsValid())
	{
		Sink = Sequence.Defender->FindComponentByClass<UCombatComponent>();
	}
	if (!Sink)
	{
		return;
	}

	FActionReactionTelemetryRecord Record;
	Record.Event = Event;
	Record.Reason = Reason;
	Record.Actor = Sequence.Defender.IsValid() ? Sequence.Defender.Get() : Sink->GetOwner();
	Record.Counterpart = Sequence.SourceAttacker.Get();
	Record.AttackGeneration = Sequence.OriginatingAttack.AttackInstance.AttackGeneration;
	Record.PrimaryActionGeneration = Sequence.StageGeneration;
	Record.MontageInstanceId = MontageInstanceId;
	Record.ActionName = StageData
		? StageData->ChainTransitionPolicy.RequiredMarker
		: NAME_None;
	Record.AnimationLane = TEXT("DefenseChain");
	Record.ReactionClass = DefenseStageName(Sequence.ChainState);
	if (StageData)
	{
		const UEnum* ReactionEnum = StaticEnum<EPairedReactionType>();
		Record.ReactionDisposition = ReactionEnum
			? FName(*ReactionEnum->GetNameStringByValue(
				static_cast<int64>(StageData->ReactionType)))
			: NAME_None;
		Record.MontagePath = StageData->AttackerMontage
			? FSoftObjectPath(StageData->AttackerMontage)
			: FSoftObjectPath();
	}
	if (NotifySource)
	{
		Record.NotifySourcePath = NotifySource->SourceAnimation;
		if (NotifySource->SourceAnimation.IsValid())
		{
			Record.MontagePath = NotifySource->SourceAnimation;
		}
	}
	const bool bMarkerEvent = Event == EActionReactionTelemetryEvent::PairedStageMarkerAccepted
		|| Event == EActionReactionTelemetryEvent::PairedStageMarkerRejected;
	if (bMarkerEvent && ReportingActor)
	{
		Detail = FString::Printf(
			TEXT("reporter=%s;%s"),
			*ReportingActor->GetPathName(),
			*Detail);
	}
	Record.Detail = MoveTemp(Detail);
	Sink->AppendActionReactionTelemetry(MoveTemp(Record));
}

void AppendDefenseSequenceTelemetry(
	UCombatComponent* Sink,
	const FDefenseSequenceContext& Sequence,
	const EDefenseTelemetryEvent Event,
	const EChainCounterState Stage,
	const FName CleanupReason = NAME_None)
{
	if (!Sink || !Sequence.OriginatingInteraction.IsValid())
	{
		return;
	}
	FDefenseTelemetryRecord Record = DefenseTelemetry::FromResolution(
		Sequence.OriginatingResolution,
		Event);
	Record.AttackInstance = Sequence.OriginatingAttack.AttackInstance;
	Record.AttackWindow = Sequence.OriginatingAttack.ActiveParryWindow;
	Record.StageGeneration = Sequence.StageGeneration;
	Record.StageName = DefenseStageName(Stage);
	Record.Defender = Sequence.Defender;
	Record.Attacker = Sequence.SourceAttacker;
	Record.Candidate = Sequence.SourceAttacker;
	Record.CandidateDisposition = TEXT("RetainedSequence");
	Record.CleanupReason = CleanupReason;
	Record.TimeToDeadline = Sequence.ResponseDeadlineUnscaled > 0.0
		? static_cast<float>(FMath::Max(
			0.0,
			Sequence.ResponseDeadlineUnscaled - FPlatformTime::Seconds()))
		: -1.0f;
	if (Sequence.Defender.IsValid())
	{
		Record.OwnerTransform = Sequence.Defender->GetActorTransform();
	}
	if (Sequence.SourceAttacker.IsValid())
	{
		Record.CounterpartTransform = Sequence.SourceAttacker->GetActorTransform();
	}
	Sink->AppendDefenseTelemetry(MoveTemp(Record));
}

/**
 * Link a stage montage's played section chain into its authored ready section.
 *
 * The engine stops an instance whose current section has no successor as soon as the remaining play time
 * falls inside the montage's blend-out, and a stopping instance is no longer reachable through the
 * active-instance section API. A bridge whose window marker lies inside that tail (the shipped parry bridge
 * AM_ParryBridge_Defender: blend-out from 0.45 s, marker at 0.65 s) therefore lost its pose before the
 * marker could ask for it. Linking the played chain's terminal section into the ready section removes that
 * early tail: the bridge plays through to its ready pose. bHoldReadyPose then sets what the ready section
 * does:
 * - true, for the parried attacker: the ready section loops, holding the pose until a successor stage or
 *   terminal cleanup stops it;
 * - false, for the parrying defender: the ready section is the montage's terminal section, so the bridge
 *   ends after one pass and returns the defender to its AnimBP and the player, even if the authored
 *   section loops.
 * Idempotent. Returns whether the instance reaches the ready section.
 *
 * When the played chain can never reach the ready section (an authored cycle), it is entered by a jump
 * only if bJumpWhenUnreachable: the stage start must not skip its bridge, the opened window may.
 */
bool LinkStageIntoReadySection(
	UAnimInstance* AnimInstance,
	const UAnimMontage* Montage,
	const FName ReadySection,
	const bool bHoldReadyPose,
	const bool bJumpWhenUnreachable)
{
	FAnimMontageInstance* Instance = AnimInstance && Montage && !ReadySection.IsNone()
		? AnimInstance->GetActiveInstanceForMontage(Montage)
		: nullptr;
	const int32 ReadyIndex = Instance ? Montage->GetSectionIndex(ReadySection) : INDEX_NONE;
	if (ReadyIndex == INDEX_NONE)
	{
		return false;
	}

	bool bReachesReady = false;
	int32 SectionIndex = Montage->GetSectionIndex(Instance->GetCurrentSection());
	TSet<int32, DefaultKeyFuncs<int32>, TInlineSetAllocator<8>> Visited;
	while (SectionIndex != INDEX_NONE && !Visited.Contains(SectionIndex))
	{
		if (SectionIndex == ReadyIndex)
		{
			bReachesReady = true;
			break;
		}
		Visited.Add(SectionIndex);
		const int32 NextIndex = Instance->GetNextSectionID(SectionIndex);
		if (NextIndex == INDEX_NONE)
		{
			bReachesReady = Instance->SetNextSectionID(SectionIndex, ReadyIndex);
			break;
		}
		SectionIndex = NextIndex;
	}
	if (!Instance->SetNextSectionID(ReadyIndex, bHoldReadyPose ? ReadyIndex : INDEX_NONE))
	{
		return false;
	}
	if (!bReachesReady && bJumpWhenUnreachable)
	{
		bReachesReady = Instance->JumpToSectionName(ReadySection);
	}
	return bReachesReady;
}

FName ResolveChainResponseExpiryReason(const EChainCounterState ResponseState)
{
	return ResponseState == EChainCounterState::FinisherReady
		? FName(TEXT("FinisherReadyExpired"))
		: FName(TEXT("CounterWindowExpired"));
}
}

// ============================================================================
// CONSTRUCTION
// ============================================================================

UPairedAnimationComponent::UPairedAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void UPairedAnimationComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedOwnerCharacter = Cast<ABaseCombatCharacter>(GetOwner());
	if (CachedOwnerCharacter)
	{
		CachedCombatComponent = CachedOwnerCharacter->FindComponentByClass<UCombatComponent>();
	}
}

void UPairedAnimationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// A no-montage defense bridge still owns tags, input, and an async deadline.
	if (ChainState != EChainCounterState::None || IsPairedAnimationActive())
	{
		CancelPairedAnimation(0.0f);
	}
	if (ActiveLegacyPairedGeneration != 0)
	{
		const int32 EndingLegacyGeneration = ActiveLegacyPairedGeneration;
		ReleaseLegacyPairedParticipationForPartners();
		EndLegacyPairedParticipation(EndingLegacyGeneration);
	}

	ClearPairedPartners();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlowMotionRestoreHandle);
		for (TPair<FDefenseAsyncHandle, FTimerHandle>& Pair : DefenseSimulationTimers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	ReleaseLegacyPairedTimeDilation();
	for (const TPair<FDefenseAsyncHandle, FTSTicker::FDelegateHandle>& Pair : DefenseResponseTickers)
	{
		if (Pair.Value.IsValid())
		{
			FTSTicker::RemoveTicker(Pair.Value);
		}
	}
	DefenseSimulationTimers.Reset();
	DefenseResponseTickers.Reset();
	RetiredOwnerMontageCallbacks.Reset();
	ReleaseAllPairedStateLeases();
	ReleaseAllInputOwnership();

	ActivePairedAnimData = nullptr;
	CurrentFinisherVictim.Reset();
	bCompletingPairedAnimation = false;
	bCounterWindowActive = false;
	bParryWindowActive = false;
	ClearChainContext();
	CounterWindowData.Reset();

	Super::EndPlay(EndPlayReason);
}

// ============================================================================
// CONFIGURATION / CACHED REFERENCES
// ============================================================================

ABaseCombatCharacter* UPairedAnimationComponent::GetOwnerCharacter() const
{
	return CachedOwnerCharacter
		? CachedOwnerCharacter.Get()
		: Cast<ABaseCombatCharacter>(GetOwner());
}

bool UPairedAnimationComponent::GetDebugDraw() const
{
	return CombatDebug::IsPairedAnimDebugEnabled();
}

bool UPairedAnimationComponent::IsValidPairedTarget(AActor* TargetActor) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !TargetActor || OwnerActor == TargetActor)
	{
		return false;
	}

	if (OwnerActor->Implements<UTeamMemberInterface>() && TargetActor->Implements<UTeamMemberInterface>())
	{
		return ITeamMemberInterface::Execute_IsHostileTo(OwnerActor, TargetActor);
	}

	return true;
}

UPairedAnimationComponent* UPairedAnimationComponent::FindDefenseSequenceOwner() const
{
	if (ActiveDefenseSequence.OriginatingInteraction.IsValid()
		&& ChainState != EChainCounterState::None)
	{
		return const_cast<UPairedAnimationComponent*>(this);
	}

	AActor* OwnerActor = GetOwner();
	UPairedAnimationComponent* ResolvedOwner = nullptr;
	for (const TWeakObjectPtr<AActor>& PartnerRef : PairedAnimationPartners)
	{
		AActor* Partner = PartnerRef.Get();
		UPairedAnimationComponent* Candidate = Partner
			? Partner->FindComponentByClass<UPairedAnimationComponent>()
			: nullptr;
		if (Candidate
			&& Candidate->ChainState != EChainCounterState::None
			&& Candidate->ActiveDefenseSequence.OriginatingInteraction.IsValid()
			&& (Candidate->ActiveDefenseSequence.Defender.Get() == OwnerActor
				|| Candidate->ActiveDefenseSequence.SourceAttacker.Get() == OwnerActor))
		{
			if (ResolvedOwner && ResolvedOwner != Candidate)
			{
				return nullptr;
			}
			ResolvedOwner = Candidate;
		}
	}
	return ResolvedOwner;
}

void UPairedAnimationComponent::HandleChainStageTransition(
	const EChainStageTransitionType Transition,
	const int32 MontageInstanceId,
	const FAnimNotifyRuntimeSourceId NotifySourceId)
{
	if (UPairedAnimationComponent* SequenceOwner = FindDefenseSequenceOwner())
	{
		SequenceOwner->HandleChainStageTransitionFromActor(
			GetOwner(),
			Transition,
			MontageInstanceId,
			NotifySourceId);
	}
}

void UPairedAnimationComponent::HandleChainStageTransitionFromActor(
	AActor* ReportingActor,
	const EChainStageTransitionType Transition,
	const int32 MontageInstanceId,
	const FAnimNotifyRuntimeSourceId& NotifySourceId)
{
	ABaseCombatCharacter* Defender = Cast<ABaseCombatCharacter>(ActiveDefenseSequence.Defender.Get());
	ABaseCombatCharacter* SourceAttacker = Cast<ABaseCombatCharacter>(ActiveDefenseSequence.SourceAttacker.Get());
	UPairedAnimationData* StageData = ActiveDefenseSequence.ActivePairedData.Get();
	const FDefenseSequenceContext MarkerSequence = ActiveDefenseSequence;
	UCombatComponent* MarkerTelemetrySink = CachedCombatComponent.Get();
	if (!MarkerTelemetrySink && Defender)
	{
		MarkerTelemetrySink = Defender->CombatComponent.Get();
	}
	if (!ReportingActor
		|| !StageData
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid()
		|| ActiveDefenseSequence.StageGeneration <= 0
		|| !NotifySourceId.IsValid()
		|| MontageInstanceId < 0)
	{
		AppendPairedStageActionReactionTelemetry(
			MarkerTelemetrySink,
			MarkerSequence,
			EActionReactionTelemetryEvent::PairedStageMarkerRejected,
			EActionReactionTelemetryReason::MarkerContextInvalid,
			StageData,
			ReportingActor,
			MontageInstanceId,
			&NotifySourceId,
			TEXT("marker context is incomplete or invalid"));
		return;
	}
	if (!Defender
		|| !SourceAttacker
		|| Defender->IsDeadOrDying()
		|| SourceAttacker->IsDeadOrDying())
	{
		AppendPairedStageActionReactionTelemetry(
			MarkerTelemetrySink,
			MarkerSequence,
			EActionReactionTelemetryEvent::PairedStageMarkerRejected,
			EActionReactionTelemetryReason::MarkerParticipantInvalid,
			StageData,
			ReportingActor,
			MontageInstanceId,
			&NotifySourceId,
			FString::Printf(
				TEXT("defender_valid=%s source_valid=%s defender_terminal=%s source_terminal=%s"),
				Defender ? TEXT("true") : TEXT("false"),
				SourceAttacker ? TEXT("true") : TEXT("false"),
				Defender && Defender->IsDeadOrDying() ? TEXT("true") : TEXT("false"),
				SourceAttacker && SourceAttacker->IsDeadOrDying() ? TEXT("true") : TEXT("false")));
		CleanupDefenseSequence(
			ActiveDefenseSequence.StageGeneration,
			0.1f,
			TEXT("MarkerParticipantInvalid"));
		return;
	}

	EPairedAnimationRole ReportingRole;
	UAnimMontage* ExpectedMontage = nullptr;
	int32 ExpectedMontageInstanceId = INDEX_NONE;
	if (ReportingActor == Defender)
	{
		ReportingRole = EPairedAnimationRole::Attacker;
		ExpectedMontage = StageData->AttackerMontage;
		ExpectedMontageInstanceId = ActiveDefenseSequence.AttackerMontageInstanceId;
	}
	else if (ReportingActor == SourceAttacker)
	{
		ReportingRole = EPairedAnimationRole::Victim;
		ExpectedMontage = StageData->VictimMontage;
		ExpectedMontageInstanceId = ActiveDefenseSequence.VictimMontageInstanceId;
	}
	else
	{
		AppendPairedStageActionReactionTelemetry(
			MarkerTelemetrySink,
			MarkerSequence,
			EActionReactionTelemetryEvent::PairedStageMarkerRejected,
			EActionReactionTelemetryReason::MarkerReporterMismatch,
			StageData,
			ReportingActor,
			MontageInstanceId,
			&NotifySourceId,
			TEXT("reporting actor is not a participant in the active defense sequence"));
		return;
	}

	const FPairedChainTransitionPolicy& Policy = StageData->ChainTransitionPolicy;
	const bool bDriverRoleMatches = ReportingRole == Policy.DriverRole;
	const bool bMontageInstanceMatches = MontageInstanceId == ExpectedMontageInstanceId;
	const bool bSourceMontageMatches = ExpectedMontage
		&& NotifySourceId.SourceAnimation == FSoftObjectPath(ExpectedMontage);
	const bool bNotifyIndexValid = ExpectedMontage
		&& ExpectedMontage->Notifies.IsValidIndex(NotifySourceId.NotifyEventIndex);
	if (!bDriverRoleMatches
		|| !bMontageInstanceMatches
		|| !bSourceMontageMatches
		|| !bNotifyIndexValid)
	{
		const EActionReactionTelemetryReason RejectionReason = !bDriverRoleMatches
			? EActionReactionTelemetryReason::MarkerDriverRoleMismatch
			: !bMontageInstanceMatches
				? EActionReactionTelemetryReason::MarkerMontageInstanceMismatch
				: !bSourceMontageMatches
					? EActionReactionTelemetryReason::MarkerNotifySourceMismatch
					: EActionReactionTelemetryReason::MarkerNotifyIndexInvalid;
		const FString Detail = FString::Printf(
			TEXT("role=%d expected_role=%d instance=%d expected_instance=%d source=%s expected_source=%s notify_index=%d notify_count=%d"),
			static_cast<int32>(ReportingRole),
			static_cast<int32>(Policy.DriverRole),
			MontageInstanceId,
			ExpectedMontageInstanceId,
			*NotifySourceId.SourceAnimation.ToString(),
			ExpectedMontage ? *FSoftObjectPath(ExpectedMontage).ToString() : TEXT("None"),
			NotifySourceId.NotifyEventIndex,
			ExpectedMontage ? ExpectedMontage->Notifies.Num() : 0);
		UE_LOG(LogPairedAnim, Verbose,
			TEXT("[COUNTER-CHAIN] Ignored stage marker generation=%d %s"),
			ActiveDefenseSequence.StageGeneration,
			*Detail);
		AppendPairedStageActionReactionTelemetry(
			MarkerTelemetrySink,
			MarkerSequence,
			EActionReactionTelemetryEvent::PairedStageMarkerRejected,
			RejectionReason,
			StageData,
			ReportingActor,
			MontageInstanceId,
			&NotifySourceId,
			Detail);
		return;
	}

	const UAnimNotify_ChainStageTransition* AuthoredNotify = Cast<UAnimNotify_ChainStageTransition>(
		ExpectedMontage->Notifies[NotifySourceId.NotifyEventIndex].Notify);
	if (!AuthoredNotify
		|| AuthoredNotify->Transition != Transition
		|| Policy.RequiredMarker.IsNone()
		|| AuthoredNotify->MarkerName != Policy.RequiredMarker)
	{
		AppendPairedStageActionReactionTelemetry(
			MarkerTelemetrySink,
			MarkerSequence,
			EActionReactionTelemetryEvent::PairedStageMarkerRejected,
			EActionReactionTelemetryReason::MarkerPolicyMismatch,
			StageData,
			ReportingActor,
			MontageInstanceId,
			&NotifySourceId,
			TEXT("authored marker payload does not match the active transition policy"));
		return;
	}

	const int32 ExpectedGeneration = ActiveDefenseSequence.StageGeneration;
	const EChainCounterState PreviousState = ChainState;
	bool bTransitionApplied = false;
	if (Transition == EChainStageTransitionType::OpenCounterWindow)
	{
		bTransitionApplied = EnterDefenseCounterWindow(ExpectedGeneration);
	}
	else
	{
		bTransitionApplied = HandleDefenseAutoContinueMarker(ExpectedGeneration);
	}
	const bool bMarkerConsumed = bTransitionApplied
		|| ChainState != PreviousState
		|| ActiveDefenseSequence.StageGeneration != ExpectedGeneration;
	AppendPairedStageActionReactionTelemetry(
		MarkerTelemetrySink,
		MarkerSequence,
		bMarkerConsumed
			? EActionReactionTelemetryEvent::PairedStageMarkerAccepted
			: EActionReactionTelemetryEvent::PairedStageMarkerRejected,
		bMarkerConsumed
			? EActionReactionTelemetryReason::MarkerAccepted
			: EActionReactionTelemetryReason::MarkerStateMismatch,
		StageData,
		ReportingActor,
		MontageInstanceId,
		&NotifySourceId,
		bMarkerConsumed
			? TEXT("exact driver marker consumed")
			: TEXT("identity-valid marker rejected by the active Chain state"));
}

FPairedSequenceLeaseHandle UPairedAnimationComponent::AcquirePairedStateLease(
	const FName Owner,
	const int32 StageGeneration,
	const bool bUseTrackedPartnersOnly,
	const bool bDisablePawnCollision,
	const bool bDisableCapsulePhysics,
	const bool bDisableMovement,
	const bool bScanForDynamicObstructions,
	const float DynamicObstructionRadius)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character
		|| !Capsule
		|| Owner.IsNone()
		|| !FMath::IsFinite(DynamicObstructionRadius)
		|| DynamicObstructionRadius < 0.0f)
	{
		return {};
	}

	do
	{
		++NextPairedStateLeaseId;
	}
	while (NextPairedStateLeaseId == 0
		|| PairedStateLeases.Contains(FPairedSequenceLeaseHandle(NextPairedStateLeaseId)));
	const FPairedSequenceLeaseHandle Handle(NextPairedStateLeaseId);
	FPairedStateLeaseRecord& Record = PairedStateLeases.Add(Handle);
	Record.Owner = Owner;
	Record.StageGeneration = StageGeneration;
	Record.bUseTrackedPartnersOnly = bUseTrackedPartnersOnly;
	Record.bDisablePawnCollision = bDisablePawnCollision;
	Record.bDisableCapsulePhysics = bDisableCapsulePhysics;
	Record.bDisableMovement = bDisableMovement && Movement != nullptr;
	Record.bScanForDynamicObstructions = bScanForDynamicObstructions;
	Record.DynamicObstructionRadius = DynamicObstructionRadius;
	if (bDisablePawnCollision && bUseTrackedPartnersOnly)
	{
		for (const TWeakObjectPtr<AActor>& Partner : PairedAnimationPartners)
		{
			if (Partner.IsValid())
			{
				Record.IgnoredActors.Add(Partner);
			}
		}
	}
	RecomputePairedState();
	return Handle;
}

void UPairedAnimationComponent::ReleasePairedStateLease(
	const FPairedSequenceLeaseHandle Handle)
{
	if (!Handle.IsValid() || PairedStateLeases.Remove(Handle) == 0)
	{
		return;
	}
	RecomputePairedState();
}

void UPairedAnimationComponent::ReleasePairedStateLeasesForGeneration(
	const int32 StageGeneration)
{
	if (StageGeneration == 0)
	{
		return;
	}
	TSet<FPairedSequenceLeaseHandle> ReleasedHandles;
	for (auto It = PairedStateLeases.CreateIterator(); It; ++It)
	{
		if (It.Value().StageGeneration == StageGeneration)
		{
			ReleasedHandles.Add(It.Key());
			It.RemoveCurrent();
		}
	}
	if (ReleasedHandles.IsEmpty())
	{
		return;
	}

	for (auto It = PairedNotifyLeases.CreateIterator(); It; ++It)
	{
		if (ReleasedHandles.Contains(It.Value()))
		{
			It.RemoveCurrent();
		}
	}
	RecomputePairedState();
}

void UPairedAnimationComponent::RekeyPairedStateLeasesGeneration(
	const int32 PreviousGeneration,
	const int32 SuccessorGeneration)
{
	if (PreviousGeneration <= 0 || SuccessorGeneration <= 0
		|| PreviousGeneration == SuccessorGeneration)
	{
		return;
	}
	for (TPair<FPairedSequenceLeaseHandle, FPairedStateLeaseRecord>& Pair : PairedStateLeases)
	{
		if (Pair.Value.StageGeneration == PreviousGeneration)
		{
			Pair.Value.StageGeneration = SuccessorGeneration;
		}
	}
}

void UPairedAnimationComponent::ReleaseAllPairedStateLeases()
{
	PairedNotifyLeases.Reset();
	PairedStateLeases.Reset();
	RecomputePairedState();
}

void UPairedAnimationComponent::RecomputePairedState()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !Capsule)
	{
		return;
	}
	const ABaseCombatCharacter* CombatCharacter = Cast<ABaseCombatCharacter>(Character);
	const bool bTerminalCharacterState = CombatCharacter && CombatCharacter->IsDeadOrDying();

	TSet<TWeakObjectPtr<AActor>> DesiredIgnoredActors;
	bool bIgnoreAllPawns = false;
	bool bDisableCapsule = false;
	bool bDisableCharacterMovement = false;
	for (const TPair<FPairedSequenceLeaseHandle, FPairedStateLeaseRecord>& Pair : PairedStateLeases)
	{
		const FPairedStateLeaseRecord& Record = Pair.Value;
		if (Record.bDisablePawnCollision)
		{
			if (Record.bUseTrackedPartnersOnly)
			{
				DesiredIgnoredActors.Append(Record.IgnoredActors);
			}
			else
			{
				bIgnoreAllPawns = true;
			}
		}
		bDisableCapsule |= Record.bDisableCapsulePhysics;
		bDisableCharacterMovement |= Record.bDisableMovement;
	}

	if (!DesiredIgnoredActors.IsEmpty() && !bMoveIgnoreBaselineCaptured)
	{
		BaselineMoveIgnoredActors.Reset();
		for (AActor* IgnoredActor : Capsule->GetMoveIgnoreActors())
		{
			if (IgnoredActor)
			{
				BaselineMoveIgnoredActors.Add(IgnoredActor);
			}
		}
		bMoveIgnoreBaselineCaptured = true;
	}

	for (auto It = AppliedIgnoredActors.CreateIterator(); It; ++It)
	{
		const TWeakObjectPtr<AActor> Existing = *It;
		if (!DesiredIgnoredActors.Contains(Existing))
		{
			if (AActor* Actor = Existing.Get())
			{
				Capsule->IgnoreActorWhenMoving(Actor, false);
			}
			It.RemoveCurrent();
		}
	}
	for (const TWeakObjectPtr<AActor>& Desired : DesiredIgnoredActors)
	{
		if (!BaselineMoveIgnoredActors.Contains(Desired)
			&& !AppliedIgnoredActors.Contains(Desired))
		{
			if (AActor* Actor = Desired.Get())
			{
				Capsule->IgnoreActorWhenMoving(Actor, true);
				AppliedIgnoredActors.Add(Desired);
			}
		}
	}
	if (DesiredIgnoredActors.IsEmpty() && bMoveIgnoreBaselineCaptured)
	{
		BaselineMoveIgnoredActors.Reset();
		bMoveIgnoreBaselineCaptured = false;
	}

	if (bIgnoreAllPawns)
	{
		if (!bPawnCollisionBaselineCaptured)
		{
			BaselinePawnCollisionResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
			bPawnCollisionBaselineCaptured = true;
		}
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	else if (bPawnCollisionBaselineCaptured)
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, BaselinePawnCollisionResponse.GetValue());
		bPawnCollisionBaselineCaptured = false;
	}

	if (bDisableCapsule)
	{
		if (!bCapsuleCollisionBaselineCaptured)
		{
			BaselineCollisionEnabled = Capsule->GetCollisionEnabled();
			bCapsuleCollisionBaselineCaptured = true;
		}
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else if (bCapsuleCollisionBaselineCaptured)
	{
		if (!bTerminalCharacterState)
		{
			Capsule->SetCollisionEnabled(BaselineCollisionEnabled.GetValue());
		}
		bCapsuleCollisionBaselineCaptured = false;
	}

	if (Movement)
	{
		if (bDisableCharacterMovement)
		{
			if (!bMovementBaselineCaptured)
			{
				BaselineMovementMode = Movement->MovementMode.GetValue();
				bMovementBaselineCaptured = true;
			}
			Movement->Velocity = FVector::ZeroVector;
			Movement->DisableMovement();
		}
		else if (bMovementBaselineCaptured)
		{
			if (!bTerminalCharacterState)
			{
				Movement->SetMovementMode(BaselineMovementMode.GetValue());
			}
			bMovementBaselineCaptured = false;
		}
	}

	if (PairedStateLeases.IsEmpty())
	{
		AppliedIgnoredActors.Reset();
		BaselineMoveIgnoredActors.Reset();
		bMoveIgnoreBaselineCaptured = false;
	}
}

void UPairedAnimationComponent::ScanPairedStateLease(
	const FPairedSequenceLeaseHandle Handle)
{
	FPairedStateLeaseRecord* Record = PairedStateLeases.Find(Handle);
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Record
		|| !Character
		|| !Record->bScanForDynamicObstructions
		|| !Record->bUseTrackedPartnersOnly
		|| !Record->bDisablePawnCollision)
	{
		return;
	}

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(Character);
	for (const TWeakObjectPtr<AActor>& Ignored : Record->IgnoredActors)
	{
		if (AActor* Actor = Ignored.Get())
		{
			IgnoreActors.Add(Actor);
		}
	}
	const TArray<AActor*> Obstructions =
		UPairedAnimationUtilityLibrary::FindObstructingActorsInRadius(
			GetWorld(),
			Character->GetActorLocation(),
			Record->DynamicObstructionRadius,
			IgnoreActors);
	for (AActor* Obstruction : Obstructions)
	{
		if (Obstruction && Obstruction != Character)
		{
			Record->IgnoredActors.Add(Obstruction);
		}
	}
	RecomputePairedState();
}

bool UPairedAnimationComponent::BeginPairedCollisionNotify(
	const FAnimNotifyRuntimeSourceId& NotifySource,
	const int32 MontageInstanceId,
	const bool bUseTrackedPartnersOnly,
	const bool bDisablePawnCollision,
	const bool bDisableCapsulePhysics,
	const bool bDisableMovement,
	const bool bScanForDynamicObstructions,
	const float DynamicObstructionRadius)
{
	const FPairedNotifyLeaseKey Key{NotifySource, MontageInstanceId};
	if (!NotifySource.IsValid() || MontageInstanceId < 0 || PairedNotifyLeases.Contains(Key))
	{
		return false;
	}
	const UPairedAnimationComponent* SequenceOwner = FindDefenseSequenceOwner();
	const int32 StageGeneration = SequenceOwner
		? SequenceOwner->ActiveDefenseSequence.StageGeneration
		: ActiveLegacyPairedGeneration;
	const FPairedSequenceLeaseHandle Handle = AcquirePairedStateLease(
		TEXT("PairedCollisionNotify"),
		StageGeneration,
		bUseTrackedPartnersOnly,
		bDisablePawnCollision,
		bDisableCapsulePhysics,
		bDisableMovement,
		bScanForDynamicObstructions,
		DynamicObstructionRadius);
	if (!Handle.IsValid())
	{
		return false;
	}
	PairedNotifyLeases.Add(Key, Handle);
	return true;
}

void UPairedAnimationComponent::TickPairedCollisionNotify(
	const FAnimNotifyRuntimeSourceId& NotifySource,
	const int32 MontageInstanceId)
{
	if (const FPairedSequenceLeaseHandle* Handle = PairedNotifyLeases.Find({NotifySource, MontageInstanceId}))
	{
		ScanPairedStateLease(*Handle);
	}
}

void UPairedAnimationComponent::EndPairedCollisionNotify(
	const FAnimNotifyRuntimeSourceId& NotifySource,
	const int32 MontageInstanceId)
{
	FPairedSequenceLeaseHandle Handle;
	if (PairedNotifyLeases.RemoveAndCopyValue({NotifySource, MontageInstanceId}, Handle))
	{
		ReleasePairedStateLease(Handle);
	}
}

FPairedSequenceLeaseHandle UPairedAnimationComponent::AcquireInputOwnership(
	const FName Owner,
	const int32 StageGeneration)
{
	if (Owner.IsNone())
	{
		return {};
	}
	do
	{
		++NextPairedInputLeaseId;
	}
	while (NextPairedInputLeaseId == 0
		|| PairedInputLeases.Contains(FPairedSequenceLeaseHandle(NextPairedInputLeaseId)));
	const FPairedSequenceLeaseHandle Handle(NextPairedInputLeaseId);
	FPairedInputLeaseRecord& Record = PairedInputLeases.Add(Handle);
	Record.Owner = Owner;
	Record.StageGeneration = StageGeneration;
	RecomputeInputOwnership();
	return Handle;
}

void UPairedAnimationComponent::ReleaseInputOwnership(
	const FPairedSequenceLeaseHandle Handle)
{
	if (Handle.IsValid())
	{
		PairedInputLeases.Remove(Handle);
	}
	RecomputeInputOwnership();
}

void UPairedAnimationComponent::ReleaseAllInputOwnership()
{
	PairedInputLeases.Reset();
	LegacyPairedInputLease = {};
	RecomputeInputOwnership();
}

void UPairedAnimationComponent::RecomputeInputOwnership()
{
	bBlockCombatInput = !PairedInputLeases.IsEmpty();
}

void UPairedAnimationComponent::RetireOwnerMontageCallback(UAnimMontage* Montage)
{
	if (Montage)
	{
		++RetiredOwnerMontageCallbacks.FindOrAdd(Montage);
	}
}

void UPairedAnimationComponent::CancelRetiredOwnerMontageCallback(UAnimMontage* Montage)
{
	if (int32* Count = Montage ? RetiredOwnerMontageCallbacks.Find(Montage) : nullptr)
	{
		if (--(*Count) <= 0)
		{
			RetiredOwnerMontageCallbacks.Remove(Montage);
		}
	}
}

bool UPairedAnimationComponent::ConsumeRetiredOwnerMontageCallback(UAnimMontage* Montage)
{
	if (!Montage || !RetiredOwnerMontageCallbacks.Contains(Montage))
	{
		return false;
	}
	CancelRetiredOwnerMontageCallback(Montage);
	return true;
}

FDefenseAsyncHandle UPairedAnimationComponent::AllocateDefenseAsyncHandle()
{
	do
	{
		++NextDefenseAsyncId;
	}
	while (NextDefenseAsyncId == 0
		|| DefenseResponseTickers.Contains(FDefenseAsyncHandle(NextDefenseAsyncId))
		|| DefenseSimulationTimers.Contains(FDefenseAsyncHandle(NextDefenseAsyncId)));
	return FDefenseAsyncHandle(NextDefenseAsyncId);
}

void UPairedAnimationComponent::CancelDefenseAsyncHandle(
	const FDefenseAsyncHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}
	FTSTicker::FDelegateHandle TickerHandle;
	if (DefenseResponseTickers.RemoveAndCopyValue(Handle, TickerHandle)
		&& TickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TickerHandle);
	}
	FTimerHandle TimerHandle;
	if (DefenseSimulationTimers.RemoveAndCopyValue(Handle, TimerHandle))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(TimerHandle);
		}
	}
}

void UPairedAnimationComponent::ScheduleChainResponseDeadline(
	const EChainCounterState ResponseState,
	const float Duration,
	const int32 ExpectedStageGeneration,
	const double PreservedDeadline)
{
	CancelDefenseAsyncHandle(ActiveDefenseSequence.ResponseTimeoutHandle);
	ActiveDefenseSequence.ResponseTimeoutHandle = {};
	if ((ResponseState != EChainCounterState::CounterWindow
			&& ResponseState != EChainCounterState::FinisherReady)
		|| !FMath::IsFinite(Duration)
		|| Duration < 0.0f
		|| ActiveDefenseSequence.StageGeneration != ExpectedStageGeneration
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	const double Deadline = PreservedDeadline > Now
		? PreservedDeadline
		: Now + static_cast<double>(Duration);
	const float Delay = static_cast<float>(FMath::Max(0.0, Deadline - Now));
	const FDefenseAsyncHandle AsyncHandle = AllocateDefenseAsyncHandle();
	const FDefenseInteractionId Interaction = ActiveDefenseSequence.OriginatingInteraction;
	const TWeakObjectPtr<UPairedAnimationComponent> WeakThis(this);
	const TWeakObjectPtr<UWorld> WeakWorld(GetWorld());
	const FTSTicker::FDelegateHandle TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakThis, WeakWorld, Interaction, ResponseState, ExpectedStageGeneration, AsyncHandle](const float DeltaTime)
			{
				UPairedAnimationComponent* Component = WeakThis.Get();
				UWorld* World = WeakWorld.Get();
				return Component && World && Component->GetWorld() == World
					? Component->HandleChainResponseDeadline(
						Interaction,
						ResponseState,
						ExpectedStageGeneration,
						AsyncHandle,
						DeltaTime)
					: false;
			}),
		Delay);
	DefenseResponseTickers.Add(AsyncHandle, TickerHandle);
	ActiveDefenseSequence.ResponseTimeoutHandle = AsyncHandle;
	ActiveDefenseSequence.ResponseDeadlineUnscaled = Deadline;
}

bool UPairedAnimationComponent::HandleChainResponseDeadline(
	const FDefenseInteractionId Interaction,
	const EChainCounterState ExpectedState,
	const int32 ExpectedStageGeneration,
	const FDefenseAsyncHandle AsyncHandle,
	const float DeltaTime)
{
	DefenseResponseTickers.Remove(AsyncHandle);
	if (ActiveDefenseSequence.ResponseTimeoutHandle == AsyncHandle)
	{
		ActiveDefenseSequence.ResponseTimeoutHandle = {};
	}
	if (ActiveDefenseSequence.OriginatingInteraction != Interaction
		|| ActiveDefenseSequence.StageGeneration != ExpectedStageGeneration
		|| ActiveDefenseSequence.ChainState != ExpectedState
		|| ChainState != ExpectedState)
	{
		return false;
	}
	if (!ActiveDefenseSequence.Defender.IsValid()
		|| !ActiveDefenseSequence.SourceAttacker.IsValid())
	{
		CleanupDefenseSequence(ExpectedStageGeneration, 0.1f, TEXT("DeadlineParticipantInvalid"));
		return false;
	}
	const ABaseCombatCharacter* Defender = Cast<ABaseCombatCharacter>(
		ActiveDefenseSequence.Defender.Get());
	const ABaseCombatCharacter* SourceAttacker = Cast<ABaseCombatCharacter>(
		ActiveDefenseSequence.SourceAttacker.Get());
	if (!Defender
		|| !SourceAttacker
		|| Defender->IsDeadOrDying()
		|| SourceAttacker->IsDeadOrDying())
	{
		CleanupDefenseSequence(ExpectedStageGeneration, 0.1f, TEXT("DeadlineParticipantUnavailable"));
		return false;
	}
	// The response deadline is the only normal end of a waiting window; name it by the window it closed.
	CleanupDefenseSequence(
		ExpectedStageGeneration,
		0.1f,
		ResolveChainResponseExpiryReason(ExpectedState));
	return false;
}

bool UPairedAnimationComponent::ApplyActivePairedDamageOnce()
{
	const int32 StageGeneration = ActiveDefenseSequence.StageGeneration;
	if (StageGeneration <= 0
		|| ActiveDefenseSequence.LastDamageAppliedStageGeneration == StageGeneration
		|| ActivePairedReactionType == EPairedReactionType::Parry)
	{
		return false;
	}

	AActor* Victim = ActiveDefenseSequence.SourceAttacker.Get();
	UPairedAnimationData* Data = ActiveDefenseSequence.ActivePairedData.Get();
	AActor* DamageSource = ActiveDefenseSequence.Defender.Get();
	if (!Victim || !DamageSource || !Data || !Victim->Implements<UDamageableInterface>())
	{
		return false;
	}

	const double RequestedDamageDouble =
		static_cast<double>(Data->BaseDamage) * static_cast<double>(Data->DamageMultiplier);
	if (!FMath::IsFinite(Data->BaseDamage)
		|| Data->BaseDamage < 0.0f
		|| !FMath::IsFinite(Data->DamageMultiplier)
		|| Data->DamageMultiplier < 0.0f
		|| RequestedDamageDouble > static_cast<double>(TNumericLimits<float>::Max()))
	{
		return false;
	}
	float RequestedDamage = static_cast<float>(RequestedDamageDouble);
	const float CurrentHealth = IDamageableInterface::Execute_GetCurrentHealth(Victim);
	if (!FMath::IsFinite(CurrentHealth) || CurrentHealth < 0.0f)
	{
		return false;
	}
	const bool bTreatAsLethal = ShouldTreatPairedAnimationAsLethal(
		ActivePairedReactionType,
		Data);
	if (ActivePairedReactionType == EPairedReactionType::Counter && !bTreatAsLethal)
	{
		RequestedDamage = FMath::Min(RequestedDamage, FMath::Max(0.0f, CurrentHealth - 1.0f));
	}
	FHitReactionInfo HitInfo;
	HitInfo.Attacker = DamageSource;
	HitInfo.DirectionToAttacker = CombatMath::DirectionToAttacker(Victim->GetActorLocation(), DamageSource->GetActorLocation());
	HitInfo.ImpactPoint = Victim->GetActorLocation();
	HitInfo.bWasCounter = ActivePairedReactionType == EPairedReactionType::Counter;
	HitInfo.PhaseWhenHit = EAttackPhase::Active;
	HitInfo.Damage = RequestedDamage;
	if (bTreatAsLethal)
	{
		HitInfo.Damage = FMath::Max(
			RequestedDamage,
			CurrentHealth + 1.0f);
	}
	// Install the marker immediately before external damage code so reentry is
	// harmless while invalid preflight data remains retryable and diagnosable.
	ActiveDefenseSequence.LastDamageAppliedStageGeneration = StageGeneration;
	AppendDefenseSequenceTelemetry(
		CachedCombatComponent.Get(),
		ActiveDefenseSequence,
		EDefenseTelemetryEvent::StageDamage,
		ActiveDefenseSequence.ChainState);
	IDamageableInterface::Execute_ApplyDamage(Victim, HitInfo);
	return true;
}

bool UPairedAnimationComponent::ApplyLegacyPairedDamageOnce()
{
	if (IsPreparingPairedEntry()) { return false; }
	const int32 LegacyGeneration = ActiveLegacyPairedGeneration;
	if (!bOwnsLegacyPairedGeneration
		|| LegacyGeneration >= 0
		|| LastLegacyDamageAppliedGeneration == LegacyGeneration
		|| ActivePairedReactionType == EPairedReactionType::Parry)
	{
		return false;
	}

	AActor* Victim = CurrentFinisherVictim.Get();
	AActor* DamageSource = GetOwner();
	UPairedAnimationData* Data = ActivePairedAnimData.Get();
	if (!HasAcceptedLegacyPairedParticipant(Victim)
		|| !DamageSource
		|| !Data
		|| !Victim->Implements<UDamageableInterface>())
	{
		return false;
	}

	const double RequestedDamageDouble =
		static_cast<double>(Data->BaseDamage) * static_cast<double>(Data->DamageMultiplier);
	if (!FMath::IsFinite(Data->BaseDamage)
		|| Data->BaseDamage < 0.0f
		|| !FMath::IsFinite(Data->DamageMultiplier)
		|| Data->DamageMultiplier < 0.0f
		|| RequestedDamageDouble > static_cast<double>(TNumericLimits<float>::Max()))
	{
		return false;
	}

	float RequestedDamage = static_cast<float>(RequestedDamageDouble);
	const float CurrentHealth = IDamageableInterface::Execute_GetCurrentHealth(Victim);
	if (!FMath::IsFinite(CurrentHealth) || CurrentHealth < 0.0f)
	{
		return false;
	}

	const bool bTreatAsLethal = ShouldTreatPairedAnimationAsLethal(
		ActivePairedReactionType,
		Data);
	if (ActivePairedReactionType == EPairedReactionType::Counter && !bTreatAsLethal)
	{
		RequestedDamage = FMath::Min(RequestedDamage, FMath::Max(0.0f, CurrentHealth - 1.0f));
	}

	FHitReactionInfo HitInfo;
	HitInfo.Attacker = DamageSource;
	HitInfo.DirectionToAttacker = CombatMath::DirectionToAttacker(Victim->GetActorLocation(), DamageSource->GetActorLocation());
	HitInfo.ImpactPoint = Victim->GetActorLocation();
	HitInfo.bWasCounter = ActivePairedReactionType == EPairedReactionType::Counter;
	HitInfo.PhaseWhenHit = EAttackPhase::Active;
	HitInfo.Damage = bTreatAsLethal
		? FMath::Max(RequestedDamage, CurrentHealth + 1.0f)
		: RequestedDamage;

	// Commit ownership before invoking external damage code so synchronous death
	// callbacks recognize the victim death as the expected paired outcome.
	const int32 PreviousDamageGeneration = LastLegacyDamageAppliedGeneration;
	LastLegacyDamageAppliedGeneration = LegacyGeneration;
	const float ActualDamage = IDamageableInterface::Execute_ApplyDamage(Victim, HitInfo);
	if (HitInfo.Damage > 0.0f
		&& ActualDamage <= 0.0f
		&& IDamageableInterface::Execute_IsAlive(Victim))
	{
		LastLegacyDamageAppliedGeneration = PreviousDamageGeneration;
		UE_LOG(LogPairedAnim, Warning,
			TEXT("[PAIRED DAMAGE] Legacy generation %d damage was rejected by %s; completion fallback remains armed"),
			LegacyGeneration,
			*GetNameSafe(Victim));
		return false;
	}
	UE_LOG(LogPairedAnim, Log,
		TEXT("[PAIRED DAMAGE] Legacy generation %d committed %.1f actual damage (%.1f requested) to %s"),
		LegacyGeneration,
		ActualDamage,
		HitInfo.Damage,
		*GetNameSafe(Victim));
	return true;
}

void UPairedAnimationComponent::HandleDefenseOwnerDying(AActor* Killer)
{
	(void)Killer;
	CleanupDefenseSequence(
		ActiveDefenseSequence.StageGeneration,
		0.0f,
		TEXT("DefenseOwnerDeath"));
}

bool UPairedAnimationComponent::IsExpectedDefenseFinisherSourceDeath(
	const AActor* Source) const
{
	const ABaseCombatCharacter* SourceCharacter = Cast<ABaseCombatCharacter>(Source);
	UPairedAnimationData* ActiveData = ActiveDefenseSequence.ActivePairedData.Get();
	return Source
		&& Source == ActiveDefenseSequence.SourceAttacker.Get()
		&& ChainState == EChainCounterState::FinisherActive
		&& ActiveDefenseSequence.ChainState == EChainCounterState::FinisherActive
		&& ActivePairedReactionType == EPairedReactionType::Finisher
		&& ActiveDefenseSequence.StageGeneration > 0
		&& ActiveDefenseSequence.LastDamageAppliedStageGeneration
			== ActiveDefenseSequence.StageGeneration
		&& ActiveData
		&& ShouldTreatPairedAnimationAsLethal(EPairedReactionType::Finisher, ActiveData)
		&& SourceCharacter
		&& SourceCharacter->IsDeadOrDying();
}

bool UPairedAnimationComponent::IsExpectedLegacyPairedVictimDeath(
	const AActor* Victim) const
{
	const ABaseCombatCharacter* VictimCharacter = Cast<ABaseCombatCharacter>(Victim);
	return Victim
		&& Victim == CurrentFinisherVictim.Get()
		&& bOwnsLegacyPairedGeneration
		&& ActiveLegacyPairedGeneration < 0
		&& HasAcceptedLegacyPairedParticipant(Victim)
		&& LastLegacyDamageAppliedGeneration == ActiveLegacyPairedGeneration
		&& ActivePairedReactionType != EPairedReactionType::Parry
		&& ActivePairedAnimData
		&& ShouldTreatPairedAnimationAsLethal(
			ActivePairedReactionType,
			ActivePairedAnimData)
		&& VictimCharacter
		&& VictimCharacter->IsDeadOrDying();
}

bool UPairedAnimationComponent::IsExpectedPairedVictimDeath() const
{
	if (const UPairedAnimationComponent* SequenceOwner = FindDefenseSequenceOwner())
	{
		return SequenceOwner->IsExpectedDefenseFinisherSourceDeath(GetOwner());
	}
	const UPairedAnimationComponent* SequenceOwner = LegacyPairedSequenceOwner.Get();
	return SequenceOwner && SequenceOwner->IsExpectedLegacyPairedVictimDeath(GetOwner());
}

void UPairedAnimationComponent::HandleDefenseSourceDying(AActor* Killer)
{
	const AActor* Defender = ActiveDefenseSequence.Defender.Get();
	const bool bExpectedFinisherDeath = IsExpectedDefenseFinisherSourceDeath(
		ActiveDefenseSequence.SourceAttacker.Get())
		&& (Killer == Defender || Killer == nullptr);
	if (bExpectedFinisherDeath)
	{
		return;
	}

	CleanupDefenseSequence(
		ActiveDefenseSequence.StageGeneration,
		0.0f,
		TEXT("DefenseSourceDeath"));
}

void UPairedAnimationComponent::HandleDefenseOwnerDestroyed(AActor* DestroyedActor)
{
	(void)DestroyedActor;
	CleanupDefenseSequence(
		ActiveDefenseSequence.StageGeneration,
		0.0f,
		TEXT("DefenseOwnerDestroyed"));
}

void UPairedAnimationComponent::HandleDefenseSourceDestroyed(AActor* DestroyedActor)
{
	(void)DestroyedActor;
	CleanupDefenseSequence(
		ActiveDefenseSequence.StageGeneration,
		0.0f,
		TEXT("DefenseSourceDestroyed"));
}

void UPairedAnimationComponent::HandleDefenderHitReactionStarted(
	const EAttackDirection Direction,
	const bool bIsHeavyHit)
{
	(void)Direction;
	(void)bIsHeavyHit;
	CleanupDefenseSequenceForDefenderReaction();
}

void UPairedAnimationComponent::HandleDefenderStunBegin(const float Duration)
{
	(void)Duration;
	CleanupDefenseSequenceForDefenderReaction();
}

void UPairedAnimationComponent::HandleDefenderStaggered(AActor* StaggeredActor, const float Duration)
{
	(void)Duration;
	if (StaggeredActor == ActiveDefenseSequence.Defender.Get())
	{
		CleanupDefenseSequenceForDefenderReaction();
	}
}

void UPairedAnimationComponent::CleanupDefenseSequenceForDefenderReaction()
{
	// A defender interrupted by a hit (reaction montage, stun or stagger) no longer owns the counter:
	// the sequence ends here and terminal cleanup releases the held attacker. While the defender is
	// committed to a montage stage it is a paired participant whose third-party contacts do not land, so
	// in practice this fires while a released defender waits in a response window, or during a
	// no-montage bridge.
	if (ChainState == EChainCounterState::None
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		return;
	}
	CleanupDefenseSequence(
		ActiveDefenseSequence.StageGeneration,
		0.1f,
		TEXT("DefenderHitReaction"));
}

void UPairedAnimationComponent::ReleaseDefenderForResponseWindow()
{
	if (ActiveDefenseSequence.bDefenderReleased
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid()
		|| !IsChainWaitingForResponse())
	{
		return;
	}
	ABaseCombatCharacter* Defender = Cast<ABaseCombatCharacter>(ActiveDefenseSequence.Defender.Get());
	UCombatComponent* DefenderCombat = CachedCombatComponent
		? CachedCombatComponent.Get()
		: Defender ? Defender->CombatComponent.Get() : nullptr;
	ActiveDefenseSequence.bDefenderReleased = true;

	// The player is free between committed stages: input, movement, partner collision, the stage's
	// alignment request, stage slow motion and paired status (so other attackers' contacts land and AI may
	// engage) are all released. The source attacker keeps its AI suppression, paired-victim state, held
	// pose and movement lease until the window resolves, and Light/Heavy still route only to this window.
	// The window keeps a single owner, its deadline: moving, guarding and hits that cause no reaction leave
	// it open; a response starts the next stage; a defender reaction, death or cancel ends it.
	ReleaseInputOwnership(ActiveDefenseSequence.InputOwnershipLease);
	ActiveDefenseSequence.InputOwnershipLease = {};
	ReleasePairedStateLeasesForGeneration(ActiveDefenseSequence.StageGeneration);
	ActiveDefenseSequence.AttackerCollisionLease = {};
	if (UTargetingComponent* DefenderTargeting = Defender ? Defender->TargetingComponent.Get() : nullptr)
	{
		DefenderTargeting->ReleaseAlignmentRequest(ActiveDefenseSequence.AttackerAlignmentLease);
	}
	ActiveDefenseSequence.AttackerAlignmentLease = {};
	if (ActiveDefenseSequence.TimeDilationLease.IsValid())
	{
		if (UCombatEffectsWorldSubsystem* Effects = GetWorld()
			? GetWorld()->GetSubsystem<UCombatEffectsWorldSubsystem>()
			: nullptr)
		{
			Effects->ReleaseLease(ActiveDefenseSequence.TimeDilationLease);
		}
		ActiveDefenseSequence.TimeDilationLease = {};
	}
	ActivePairedAnimData = nullptr;
	if (DefenderCombat)
	{
		// A completed counter leaves the Active phase (and its weapon hit detection) behind; a free player
		// starts from no attack, as after terminal cleanup.
		if (DefenderCombat->GetCurrentPhase() != EAttackPhase::None)
		{
			DefenderCombat->SetPhase(EAttackPhase::None);
		}
		DefenderCombat->RefreshGuardThreat(EThreatRefreshReason::ManualRevalidation);
	}
	AppendPairedStageActionReactionTelemetry(
		CachedCombatComponent.Get(),
		ActiveDefenseSequence,
		EActionReactionTelemetryEvent::MovementStateChanged,
		EActionReactionTelemetryReason::MovementRestored,
		ActiveDefenseSequence.ActivePairedData.Get(),
		Defender,
		INDEX_NONE,
		nullptr,
		TEXT("defender released for the response window; source attacker stays held"));
	OnDefenseSequenceParticipationChanged.Broadcast(false);
}

void UPairedAnimationComponent::CleanupDefenseSequence(
	const int32 ExpectedStageGeneration,
	const float BlendOutTime,
	const FName Reason)
{
	if (bDefenseSequenceCleanupInProgress
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid()
		|| (ExpectedStageGeneration > 0
			&& ActiveDefenseSequence.StageGeneration != ExpectedStageGeneration))
	{
		return;
	}
	bDefenseSequenceCleanupInProgress = true;

	const FDefenseSequenceContext Sequence = ActiveDefenseSequence;
	const EPairedReactionType EndedReaction = ActivePairedReactionType;
	ABaseCombatCharacter* Defender = Cast<ABaseCombatCharacter>(Sequence.Defender.Get());
	ABaseCombatCharacter* SourceAttacker = Cast<ABaseCombatCharacter>(Sequence.SourceAttacker.Get());
	const bool bExpectedCommittedSourceDeath =
		IsExpectedDefenseFinisherSourceDeath(SourceAttacker);
	UPairedAnimationComponent* SourcePaired = SourceAttacker
		? SourceAttacker->PairedAnimationComponent.Get()
		: nullptr;
	UTargetingComponent* DefenderTargeting = Defender ? Defender->TargetingComponent.Get() : nullptr;
	UTargetingComponent* SourceTargeting = SourceAttacker ? SourceAttacker->TargetingComponent.Get() : nullptr;
	UCombatComponent* DefenderCombat = CachedCombatComponent
		? CachedCombatComponent.Get()
		: Defender ? Defender->CombatComponent.Get() : nullptr;
	AppendDefenseSequenceTelemetry(
		DefenderCombat,
		Sequence,
		EDefenseTelemetryEvent::Cleanup,
		Sequence.ChainState,
		Reason);
	if (Defender)
	{
		Defender->OnCharacterDying.RemoveDynamic(
			this,
			&UPairedAnimationComponent::HandleDefenseOwnerDying);
		Defender->OnCharacterDeath.RemoveDynamic(
			this,
			&UPairedAnimationComponent::HandleDefenseOwnerDying);
		Defender->OnDestroyed.RemoveDynamic(
			this,
			&UPairedAnimationComponent::HandleDefenseOwnerDestroyed);
		if (UHitReactionComponent* DefenderHitReaction = Defender->HitReactionComponent.Get())
		{
			DefenderHitReaction->OnHitReactionStarted.RemoveDynamic(
				this,
				&UPairedAnimationComponent::HandleDefenderHitReactionStarted);
			DefenderHitReaction->OnStunBegin.RemoveDynamic(
				this,
				&UPairedAnimationComponent::HandleDefenderStunBegin);
			DefenderHitReaction->OnStaggered.RemoveDynamic(
				this,
				&UPairedAnimationComponent::HandleDefenderStaggered);
		}
	}
	if (SourceAttacker)
	{
		SourceAttacker->OnCharacterDying.RemoveDynamic(
			this,
			&UPairedAnimationComponent::HandleDefenseSourceDying);
		SourceAttacker->OnCharacterDeath.RemoveDynamic(
			this,
			&UPairedAnimationComponent::HandleDefenseSourceDying);
		SourceAttacker->OnDestroyed.RemoveDynamic(
			this,
			&UPairedAnimationComponent::HandleDefenseSourceDestroyed);
	}

	CancelDefenseAsyncHandle(Sequence.ResponseTimeoutHandle);
	CancelDefenseAsyncHandle(Sequence.BridgeFallbackHandle);
	for (const TPair<FDefenseAsyncHandle, FTSTicker::FDelegateHandle>& Pair : DefenseResponseTickers)
	{
		if (Pair.Value.IsValid())
		{
			FTSTicker::RemoveTicker(Pair.Value);
		}
	}
	DefenseResponseTickers.Reset();
	if (UWorld* World = GetWorld())
	{
		for (TPair<FDefenseAsyncHandle, FTimerHandle>& Pair : DefenseSimulationTimers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	DefenseSimulationTimers.Reset();

	// Retire gameplay identity before stopping montages; synchronous end callbacks are stale.
	ChainState = EChainCounterState::None;
	ActiveChainContext.Reset();
	ActiveChainTarget.Reset();
	ActiveChainAttackData = nullptr;
	ActiveDefenseSequence = {};
	ActivePairedAnimData = nullptr;
	ActivePairedReactionType = EPairedReactionType::None;
	CurrentFinisherVictim.Reset();
	bCompletingPairedAnimation = false;

	if (DefenderCombat)
	{
		DefenderCombat->ReleaseContextTagLease(Sequence.ContextTagLease);
	}
	ReleaseInputOwnership(Sequence.InputOwnershipLease);
	ReleasePairedStateLeasesForGeneration(Sequence.StageGeneration);
	if (SourcePaired)
	{
		SourcePaired->ReleasePairedStateLeasesForGeneration(Sequence.StageGeneration);
	}
	if (DefenderTargeting)
	{
		DefenderTargeting->ReleaseAlignmentRequest(Sequence.AttackerAlignmentLease);
	}
	if (SourceTargeting)
	{
		SourceTargeting->ReleaseAlignmentRequest(Sequence.VictimAlignmentLease);
	}
	if (Sequence.TimeDilationLease.IsValid())
	{
		if (UCombatEffectsWorldSubsystem* Effects = GetWorld()
			? GetWorld()->GetSubsystem<UCombatEffectsWorldSubsystem>()
			: nullptr)
		{
			Effects->ReleaseLease(Sequence.TimeDilationLease);
		}
	}

	if (Sequence.ActivePairedData)
	{
		if (Defender && Defender->GetMesh())
		{
			if (UAnimInstance* Anim = Defender->GetMesh()->GetAnimInstance())
			{
				if (Anim->Montage_IsPlaying(Sequence.ActivePairedData->AttackerMontage))
				{
					RetireOwnerMontageCallback(Sequence.ActivePairedData->AttackerMontage);
				}
				Anim->Montage_Stop(
					FMath::Max(0.0f, BlendOutTime),
					Sequence.ActivePairedData->AttackerMontage);
			}
		}
		if (SourceAttacker && SourceAttacker->GetMesh())
		{
			if (UAnimInstance* Anim = SourceAttacker->GetMesh()->GetAnimInstance())
			{
				if (SourcePaired
					&& Anim->Montage_IsPlaying(Sequence.ActivePairedData->VictimMontage))
				{
					SourcePaired->RetireOwnerMontageCallback(
						Sequence.ActivePairedData->VictimMontage);
				}
				Anim->Montage_Stop(
					FMath::Max(0.0f, BlendOutTime),
					Sequence.ActivePairedData->VictimMontage);
			}
		}
	}
	if (SourceAttacker && SourceAttacker->HitReactionComponent)
	{
		if (bExpectedCommittedSourceDeath)
		{
			SourceAttacker->HitReactionComponent->CompletePairedAnimationState();
		}
		else
		{
			SourceAttacker->HitReactionComponent->ExitPairedAnimationState();
		}
	}
	if (SourcePaired)
	{
		SourcePaired->RemovePairedPartner(Defender);
		SourcePaired->PairedAnimationPartners.RemoveAll(
			[](const TWeakObjectPtr<AActor>& Partner)
			{
				return !Partner.IsValid();
			});
	}
	RemovePairedPartner(SourceAttacker);
	PairedAnimationPartners.RemoveAll(
		[](const TWeakObjectPtr<AActor>& Partner)
		{
			return !Partner.IsValid();
		});
	if (UEnemyCombatAIComponent* DefenderAI = Defender
		? Defender->FindComponentByClass<UEnemyCombatAIComponent>()
		: nullptr)
	{
		DefenderAI->ReleaseDefenseChainSuppression(Sequence.OriginatingInteraction);
	}
	if (UEnemyCombatAIComponent* SourceAI = SourceAttacker
		? SourceAttacker->FindComponentByClass<UEnemyCombatAIComponent>()
		: nullptr)
	{
		SourceAI->ReleaseDefenseChainSuppression(Sequence.OriginatingInteraction);
	}

	if (DefenderCombat)
	{
		// A response buffered for a window this sequence never opened must not reach a later sequence.
		DefenderCombat->DiscardBufferedChainResponse();
		DefenderCombat->SetPhase(EAttackPhase::None);
		DefenderCombat->ClearQueue(false);
		DefenderCombat->RefreshGuardThreat(EThreatRefreshReason::ManualRevalidation);
	}
	OnDefenseSequenceParticipationChanged.Broadcast(false);
	if (SourcePaired)
	{
		SourcePaired->OnDefenseSequenceParticipationChanged.Broadcast(false);
	}

	UE_LOG(LogPairedAnim, Log,
		TEXT("[COUNTER-CHAIN] Terminal cleanup generation %d (%s)"),
		Sequence.StageGeneration,
		*Reason.ToString());
	bDefenseSequenceCleanupInProgress = false;
	OnPairedAnimationEnded.Broadcast(EndedReaction);
}

bool UPairedAnimationComponent::BeginDefenseSequence(const FDefenseResolution& Resolution)
{
	ABaseCombatCharacter* Defender = GetOwnerCharacter();
	const FAttackInstanceId& AttackInstance = Resolution.InteractionId.Key.AttackInstance;
	ABaseCombatCharacter* SourceAttacker = Cast<ABaseCombatCharacter>(AttackInstance.Attacker.Get());
	UCombatComponent* SourceCombat = SourceAttacker
		? SourceAttacker->CombatComponent.Get()
		: nullptr;
	UPairedAnimationComponent* SourcePaired = SourceAttacker
		? SourceAttacker->PairedAnimationComponent.Get()
		: nullptr;
	if (!Defender
		|| !SourceAttacker
		|| !SourceCombat
		|| Defender->IsDeadOrDying()
		|| SourceAttacker->IsDeadOrDying()
		|| Resolution.Stage != EDefenseQueryStage::InputIntent
		|| Resolution.Decision.Outcome != EDefenseOutcome::PerfectParry
		|| !Resolution.InteractionId.IsValid()
		|| Resolution.InteractionId.Key.Defender.Get() != Defender
		|| Resolution.Decision.AttackInstance != AttackInstance
		|| !SourceCombat->IsAttackConsumed(AttackInstance)
		|| ChainState != EChainCounterState::None
		|| IsPairedAnimationActive()
		|| (SourcePaired
			&& (SourcePaired->IsPairedAnimationActive()
				|| SourcePaired->GetChainState() != EChainCounterState::None)))
	{
		UE_LOG(LogPairedAnim, Warning,
			TEXT("[DEFENSE BRIDGE] Rejected committed-sequence entry: Defender=%s Source=%s Stage=%d Outcome=%d Interaction=%s DefenderMatch=%s IdentityMatch=%s Consumed=%s Chain=%d Paired=%s"),
			*GetNameSafe(Defender),
			*GetNameSafe(SourceAttacker),
			static_cast<int32>(Resolution.Stage),
			static_cast<int32>(Resolution.Decision.Outcome),
			Resolution.InteractionId.IsValid() ? TEXT("yes") : TEXT("no"),
			Resolution.InteractionId.Key.Defender.Get() == Defender ? TEXT("yes") : TEXT("no"),
			Resolution.Decision.AttackInstance == AttackInstance ? TEXT("yes") : TEXT("no"),
			SourceCombat && SourceCombat->IsAttackConsumed(AttackInstance) ? TEXT("yes") : TEXT("no"),
			static_cast<int32>(ChainState),
			IsPairedAnimationActive() ? TEXT("yes") : TEXT("no"));
		return false;
	}

	FDefensePresentationPayload SelectedPresentation = Resolution.Presentation;
	bool bUsePairedBridge = false;
	FString ExactFailureReason;
	if (SelectedPresentation.PairedBridgeData)
	{
		bUsePairedBridge = PreflightDefenseBridge(
			Resolution,
			SelectedPresentation,
			ExactFailureReason);
	}

	if (!bUsePairedBridge)
	{
		UCombatComponent* DefenderCombat = CachedCombatComponent
			? CachedCombatComponent.Get()
			: Defender->CombatComponent.Get();
		const UDefenseConfiguration* Configuration = DefenderCombat
			? DefenderCombat->GetEffectiveDefenseConfiguration()
			: GetDefault<UDefenseConfiguration>();
		const FTableDefensePresentationSelector Selector;
		FDefensePresentationSelectionContext SelectionContext =
			BuildDefenseBridgeSelectionContext(Resolution);
		const FDefensePresentationSelectionResult GenericSelection =
			Selector.SelectGenericDefender(
				SelectionContext,
				Configuration);
		if (GenericSelection.bFound)
		{
			FString GenericFailureReason;
			if (!GenericSelection.Payload.PairedBridgeData
				|| PreflightDefenseBridge(
					Resolution,
					GenericSelection.Payload,
					GenericFailureReason))
			{
				SelectedPresentation = GenericSelection.Payload;
				bUsePairedBridge = SelectedPresentation.PairedBridgeData != nullptr;
			}
		}
		if (!bUsePairedBridge)
		{
			SelectionContext.bPairedBridgeUsable = false;
			const FDefensePresentationSelectionResult NoBridgeSelection =
				Selector.SelectGenericDefender(SelectionContext, Configuration);
			if (NoBridgeSelection.bFound)
			{
				SelectedPresentation = NoBridgeSelection.Payload;
			}
		}
	}

	if (!bUsePairedBridge)
	{
		SelectedPresentation.PairedBridgeData = nullptr;
		SelectedPresentation.ReviewedDeflectionMarker = NAME_None;
	}

	NextDefenseStageGeneration = NextDefenseStageGeneration == MAX_int32
		? 1
		: NextDefenseStageGeneration + 1;
	ActiveDefenseSequence = {};
	ActiveDefenseSequence.OriginatingResolution = Resolution;
	ActiveDefenseSequence.OriginatingInteraction = Resolution.InteractionId;
	ActiveDefenseSequence.OriginatingAttack = SourceCombat->BuildAttackExecutionSnapshot();
	ActiveDefenseSequence.OriginatingAttack.AttackInstance = AttackInstance;
	ActiveDefenseSequence.Defender = Defender;
	ActiveDefenseSequence.SourceAttacker = SourceAttacker;
	ActiveDefenseSequence.SelectedCounterAttack = nullptr;
	ActiveDefenseSequence.CounterData = nullptr;
	ActiveDefenseSequence.FinisherData = nullptr;
	ActiveDefenseSequence.ChainState = EChainCounterState::ParryActive;
	ActiveDefenseSequence.StageGeneration = NextDefenseStageGeneration;
	ActiveDefenseSequence.ActivePresentation = SelectedPresentation;
	UCombatComponent* DefenderCombat = CachedCombatComponent
		? CachedCombatComponent.Get()
		: Defender->CombatComponent.Get();
	ActiveDefenseSequence.ContextTagLease = DefenderCombat
		? DefenderCombat->AcquireContextTagLease(
			KatanaCombatGameplayTags::ContextParryCounter(),
			TEXT("DefenseSequence"))
		: FCombatContextLeaseHandle{};
	ActiveDefenseSequence.InputOwnershipLease = AcquireInputOwnership(
		TEXT("DefenseSequence"),
		ActiveDefenseSequence.StageGeneration);
	if (!ActiveDefenseSequence.ContextTagLease.IsValid()
		|| !ActiveDefenseSequence.InputOwnershipLease.IsValid())
	{
		if (DefenderCombat)
		{
			DefenderCombat->ReleaseContextTagLease(ActiveDefenseSequence.ContextTagLease);
		}
		ReleaseInputOwnership(ActiveDefenseSequence.InputOwnershipLease);
		ActiveDefenseSequence = {};
		return false;
	}
	Defender->OnCharacterDying.AddUniqueDynamic(
		this,
		&UPairedAnimationComponent::HandleDefenseOwnerDying);
	Defender->OnCharacterDeath.AddUniqueDynamic(
		this,
		&UPairedAnimationComponent::HandleDefenseOwnerDying);
	Defender->OnDestroyed.AddUniqueDynamic(
		this,
		&UPairedAnimationComponent::HandleDefenseOwnerDestroyed);
	if (UHitReactionComponent* DefenderHitReaction = Defender->HitReactionComponent.Get())
	{
		DefenderHitReaction->OnHitReactionStarted.AddUniqueDynamic(
			this,
			&UPairedAnimationComponent::HandleDefenderHitReactionStarted);
		DefenderHitReaction->OnStunBegin.AddUniqueDynamic(
			this,
			&UPairedAnimationComponent::HandleDefenderStunBegin);
		DefenderHitReaction->OnStaggered.AddUniqueDynamic(
			this,
			&UPairedAnimationComponent::HandleDefenderStaggered);
	}
	SourceAttacker->OnCharacterDying.AddUniqueDynamic(
		this,
		&UPairedAnimationComponent::HandleDefenseSourceDying);
	SourceAttacker->OnCharacterDeath.AddUniqueDynamic(
		this,
		&UPairedAnimationComponent::HandleDefenseSourceDying);
	SourceAttacker->OnDestroyed.AddUniqueDynamic(
		this,
		&UPairedAnimationComponent::HandleDefenseSourceDestroyed);
	AddPairedPartner(SourceAttacker);
	if (SourcePaired)
	{
		SourcePaired->AddPairedPartner(Defender);
	}
	UEnemyCombatAIComponent* DefenderAI =
		Defender->FindComponentByClass<UEnemyCombatAIComponent>();
	UEnemyCombatAIComponent* SourceAI =
		SourceAttacker->FindComponentByClass<UEnemyCombatAIComponent>();
	if ((DefenderAI
			&& !DefenderAI->AcquireDefenseChainSuppression(Resolution.InteractionId))
		|| (SourceAI
			&& !SourceAI->AcquireDefenseChainSuppression(Resolution.InteractionId)))
	{
		CleanupDefenseSequence(
			ActiveDefenseSequence.StageGeneration,
			0.0f,
			TEXT("AISuppressionAcquireFailed"));
		return false;
	}

	ActiveChainContext.Reset();
	ActiveChainContext.Attacker = SourceAttacker;
	if (Resolution.Decision.SelectedAttack)
	{
		ActiveChainContext.AttackType = Resolution.Decision.SelectedAttack->AttackType;
		ActiveChainContext.SwingDirection =
			Resolution.Decision.SelectedAttack->DefenseProfile.SwingShape;
		ActiveChainContext.SpecificCounterData = Resolution.Decision.SelectedAttack->CounterData;
	}
	ActiveChainTarget = SourceAttacker;
	ActiveChainAttackData = nullptr;
	ChainState = EChainCounterState::ParryActive;
	// The chain now owns the defender's body. The no-montage parry bridge starts no stage and holds no bridge
	// request, so release the defender's own knockback push here; a paired-bridge parry's stage start releases
	// it again, which is a harmless miss.
	if (UHitReactionComponent* DefenderHitReaction = Defender->HitReactionComponent.Get())
	{
		DefenderHitReaction->ReleaseKnockback(TEXT("ChainStart"));
	}
	OnDefenseSequenceParticipationChanged.Broadcast(true);
	if (SourcePaired)
	{
		SourcePaired->OnDefenseSequenceParticipationChanged.Broadcast(true);
	}

	if (bUsePairedBridge)
	{
		if (TryStartPairedAnimationWithTarget(
			SourceAttacker,
			SelectedPresentation.PairedBridgeData,
			EPairedReactionType::Parry))
		{
			return true;
		}

		UE_LOG(LogPairedAnim, Warning,
			TEXT("[DEFENSE BRIDGE] Two-role start failed after preflight for interaction epoch %llu; closing Chain presentation only"),
			Resolution.InteractionId.Epoch);
		CleanupDefenseSequence(
			ActiveDefenseSequence.StageGeneration,
			0.1f,
			TEXT("BridgeStartFailed"));
		return false;
	}

	if (!ExactFailureReason.IsEmpty())
	{
		UE_LOG(LogPairedAnim, Verbose,
			TEXT("[DEFENSE BRIDGE] Falling back to no montage: %s"),
			*ExactFailureReason);
	}
	if (!ScheduleNoMontageDefenseBridge(ActiveDefenseSequence.StageGeneration))
	{
		CleanupDefenseSequence(
			ActiveDefenseSequence.StageGeneration,
			0.0f,
			TEXT("BridgeFallbackScheduleFailed"));
		return false;
	}
	AppendDefenseSequenceTelemetry(
		DefenderCombat,
		ActiveDefenseSequence,
		EDefenseTelemetryEvent::StageStart,
		EChainCounterState::ParryActive);
	return true;
}

bool UPairedAnimationComponent::PreflightDefenseBridge(
	const FDefenseResolution& Resolution,
	const FDefensePresentationPayload& Presentation,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	const UPairedAnimationData* BridgeData = Presentation.PairedBridgeData;
	ABaseCombatCharacter* Defender = GetOwnerCharacter();
	const FAttackInstanceId& AttackInstance = Resolution.InteractionId.Key.AttackInstance;
	ABaseCombatCharacter* SourceAttacker = Cast<ABaseCombatCharacter>(AttackInstance.Attacker.Get());
	UCombatComponent* SourceCombat = SourceAttacker
		? SourceAttacker->CombatComponent.Get()
		: nullptr;
	UPairedAnimationComponent* SourcePaired = SourceAttacker
		? SourceAttacker->PairedAnimationComponent.Get()
		: nullptr;
	UCombatComponent* DefenderCombat = CachedCombatComponent
		? CachedCombatComponent.Get()
		: Defender ? Defender->CombatComponent.Get() : nullptr;
	UTargetingComponent* DefenderTargeting = Defender
		? Defender->TargetingComponent.Get()
		: nullptr;
	UTargetingComponent* SourceTargeting = SourceAttacker
		? SourceAttacker->TargetingComponent.Get()
		: nullptr;
	UHitReactionComponent* SourceHitReaction = SourceAttacker
		? SourceAttacker->HitReactionComponent.Get()
		: nullptr;
	if (!BridgeData || !Defender || !SourceAttacker || !SourceCombat
		|| !SourcePaired || !DefenderCombat || !DefenderTargeting
		|| !SourceTargeting || !SourceHitReaction)
	{
		OutFailureReason = TEXT("missing bridge data or required participant component");
		return false;
	}
	if (Defender->IsDeadOrDying()
		|| SourceAttacker->IsDeadOrDying()
		|| !IsValidPairedTarget(SourceAttacker)
		|| (Defender->HitReactionComponent
			&& Defender->HitReactionComponent->IsInPairedAnimationState())
		|| (SourceAttacker->HitReactionComponent
			&& SourceAttacker->HitReactionComponent->IsInPairedAnimationState())
		|| (SourcePaired
			&& (SourcePaired->IsPairedAnimationActive()
				|| SourcePaired->GetChainState() != EChainCounterState::None)))
	{
		OutFailureReason = TEXT("participant is dead, friendly, or already paired");
		return false;
	}
	if (Resolution.Stage != EDefenseQueryStage::InputIntent
		|| Resolution.Decision.Outcome != EDefenseOutcome::PerfectParry
		|| !Resolution.InteractionId.IsValid()
		|| Resolution.InteractionId.Key.Defender.Get() != Defender
		|| Resolution.Decision.AttackInstance != AttackInstance
		|| !SourceCombat->IsAttackConsumed(AttackInstance))
	{
		OutFailureReason = TEXT("resolution does not own the exact consumed attack");
		return false;
	}
	if (BridgeData->ReactionType != EPairedReactionType::Parry
		|| !BridgeData->AttackerMontage
		|| !BridgeData->VictimMontage
		|| (!BridgeData->AttackerMontageSection.IsNone()
			&& !BridgeData->AttackerMontage->IsValidSectionName(BridgeData->AttackerMontageSection))
		|| (!BridgeData->VictimMontageSection.IsNone()
			&& !BridgeData->VictimMontage->IsValidSectionName(BridgeData->VictimMontageSection)))
	{
		OutFailureReason = TEXT("bridge montage, section, or reaction role is invalid");
		return false;
	}
	if (RetiredOwnerMontageCallbacks.Contains(BridgeData->AttackerMontage)
		|| SourcePaired->RetiredOwnerMontageCallbacks.Contains(BridgeData->VictimMontage))
	{
		OutFailureReason = TEXT("a bridge role montage still has an unresolved prior callback");
		return false;
	}
	if (!HasValidPairedRuntimeNumerics(*BridgeData))
	{
		OutFailureReason = TEXT("bridge playback, damage, or effects numeric configuration is invalid");
		return false;
	}
	const FPairedChainTransitionPolicy& BridgePolicy = BridgeData->ChainTransitionPolicy;
	const UAnimMontage* DriverMontage =
		BridgePolicy.DriverRole == EPairedAnimationRole::Attacker
		? BridgeData->AttackerMontage.Get()
		: BridgeData->VictimMontage.Get();
	const FName DriverSection =
		BridgePolicy.DriverRole == EPairedAnimationRole::Attacker
		? BridgeData->AttackerMontageSection
		: BridgeData->VictimMontageSection;
	float DriverMarkerOffsetSeconds = 0.0f;
	if (Presentation.ReviewedDeflectionMarker.IsNone()
		|| BridgePolicy.bAutoContinue
		|| !BridgePolicy.HasRetainableReadyPose()
		|| (!BridgePolicy.AttackerReadySection.IsNone()
			&& !BridgeData->AttackerMontage->IsValidSectionName(
				BridgePolicy.AttackerReadySection))
		|| (!BridgePolicy.VictimReadySection.IsNone()
			&& !BridgeData->VictimMontage->IsValidSectionName(
				BridgePolicy.VictimReadySection))
		|| Presentation.ReviewedDeflectionMarker
			!= BridgePolicy.RequiredMarker
		|| !UAnimNotify_ChainStageTransition::TryGetSinglePlayableMarkerOffset(
			DriverMontage,
			Presentation.ReviewedDeflectionMarker,
			EChainStageTransitionType::OpenCounterWindow,
			DriverSection,
			DriverMarkerOffsetSeconds))
	{
		OutFailureReason = TEXT("driver montage lacks one reviewed Chain marker inside its played section or a retainable ready pose");
		return false;
	}
	if (!BridgeData->AttackerWarpConfig.bWarpRotation
		|| !BridgeData->VictimWarpConfig.bWarpRotation
		|| BridgeData->AttackerWarpConfig.WarpTargetName.IsNone()
		|| BridgeData->VictimWarpConfig.WarpTargetName.IsNone())
	{
		OutFailureReason = TEXT("canonical defense roles require named rotation warp targets");
		return false;
	}

	ACharacter* DefenderCharacter = Cast<ACharacter>(Defender);
	ACharacter* SourceCharacter = Cast<ACharacter>(SourceAttacker);
	bool bCanUsePlaybackOverride = false;
#if WITH_AUTOMATION_TESTS
	bCanUsePlaybackOverride = static_cast<bool>(DefenseStagePlaybackOverrideForTesting);
#endif
	if (!DefenderCharacter
		|| !SourceCharacter
		|| !DefenderCharacter->GetMesh()
		|| !SourceCharacter->GetMesh()
		|| ((!DefenderCharacter->GetMesh()->GetAnimInstance()
				|| !SourceCharacter->GetMesh()->GetAnimInstance())
			&& !bCanUsePlaybackOverride))
	{
		OutFailureReason = TEXT("one or both animation instances are unavailable");
		return false;
	}

	const float PairDistance = FVector::Dist(
		Defender->GetActorLocation(),
		SourceAttacker->GetActorLocation());
	if (!FMath::IsFinite(PairDistance)
		|| !FMath::IsFinite(BridgeData->MinTriggerDistance)
		|| !FMath::IsFinite(BridgeData->MaxTriggerDistance)
		|| BridgeData->MinTriggerDistance < 0.0f
		|| BridgeData->MaxTriggerDistance <= BridgeData->MinTriggerDistance
		|| PairDistance < BridgeData->MinTriggerDistance
		|| PairDistance > BridgeData->MaxTriggerDistance)
	{
		OutFailureReason = TEXT("participant distance is outside the bridge trigger range");
		return false;
	}

	const UDefenseConfiguration* DefenderConfiguration = DefenderCombat
		? DefenderCombat->GetEffectiveDefenseConfiguration()
		: GetDefault<UDefenseConfiguration>();
	const UDefenseConfiguration* SourceConfiguration =
		SourceCombat->GetEffectiveDefenseConfiguration();
	const float ConfiguredTranslationAllowance = DefenderConfiguration
		? DefenderConfiguration->PerfectParryTranslationAllowancePerRole
		: 0.0f;
	if (!FMath::IsFinite(ConfiguredTranslationAllowance)
		|| ConfiguredTranslationAllowance < 0.0f
		|| !FMath::IsFinite(BridgeData->MaxWarpDistance)
		|| BridgeData->MaxWarpDistance < 0.0f
		|| !FMath::IsFinite(BridgeData->AttackerWarpConfig.MaxWarpDistance)
		|| BridgeData->AttackerWarpConfig.MaxWarpDistance < 0.0f
		|| !FMath::IsFinite(BridgeData->VictimWarpConfig.MaxWarpDistance)
		|| BridgeData->VictimWarpConfig.MaxWarpDistance < 0.0f
		|| !FMath::IsFinite(Presentation.MaximumTranslation)
		|| Presentation.MaximumTranslation < 0.0f
		|| BridgeData->AttackerWarpConfig.RelativeOffset.ContainsNaN()
		|| BridgeData->VictimWarpConfig.RelativeOffset.ContainsNaN()
		|| !UPairedAnimationUtilityLibrary::IsValidFacingPolicy(BridgeData->AttackerWarpConfig.FacingPolicy)
		|| !UPairedAnimationUtilityLibrary::IsValidFacingPolicy(BridgeData->VictimWarpConfig.FacingPolicy))
	{
		OutFailureReason = TEXT("a role has an invalid translation budget");
		return false;
	}
	const float TranslationAllowance = ConfiguredTranslationAllowance;
	const FVector DefenderDestination = SourceAttacker->GetActorLocation()
		+ SourceAttacker->GetActorRotation().RotateVector(
			BridgeData->AttackerWarpConfig.RelativeOffset);
	const FVector SourceDestination = Defender->GetActorLocation()
		+ Defender->GetActorRotation().RotateVector(
			BridgeData->VictimWarpConfig.RelativeOffset);
	auto IsRoleTranslationValid = [&](const AActor* Role, const FVector& Destination,
		const FPairedWarpConfig& WarpConfig)
	{
		if (!WarpConfig.bWarpTranslation)
		{
			return true;
		}
		float Allowed = FMath::Min(
			TranslationAllowance,
			FMath::Max(0.0f, BridgeData->MaxWarpDistance));
		Allowed = FMath::Min(Allowed, FMath::Max(0.0f, WarpConfig.MaxWarpDistance));
		if (Presentation.MaximumTranslation > 0.0f)
		{
			Allowed = FMath::Min(Allowed, Presentation.MaximumTranslation);
		}
		const float Required = FVector::Dist(Role->GetActorLocation(), Destination);
		return FMath::IsFinite(Required) && Required <= Allowed + KINDA_SMALL_NUMBER;
	};
	if (!IsRoleTranslationValid(
			Defender,
			DefenderDestination,
			BridgeData->AttackerWarpConfig)
		|| !IsRoleTranslationValid(
			SourceAttacker,
			SourceDestination,
			BridgeData->VictimWarpConfig))
	{
		OutFailureReason = TEXT("a role exceeds its perfect-parry translation budget");
		return false;
	}

	const double RemainingContactSeconds =
		Resolution.PredictedContact.ContactSimulationTime
		- (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
	const ACharacter* DriverCharacter =
		BridgePolicy.DriverRole == EPairedAnimationRole::Attacker
		? DefenderCharacter
		: SourceCharacter;
	const USkeletalMeshComponent* DriverMesh = DriverCharacter
		? DriverCharacter->GetMesh()
		: nullptr;
	const double DriverPlaybackRate = DriverMontage && DriverMesh && DriverCharacter
		? static_cast<double>(DriverMontage->RateScale)
			* static_cast<double>(DriverMesh->GlobalAnimRateScale)
			* static_cast<double>(DriverCharacter->CustomTimeDilation)
		: 0.0;
	const double MarkerDeadlineSeconds = DriverPlaybackRate > UE_DOUBLE_SMALL_NUMBER
		? static_cast<double>(DriverMarkerOffsetSeconds) / DriverPlaybackRate
		: 0.0;
	if (!Resolution.PredictedContact.bIsValid
		|| !FMath::IsFinite(RemainingContactSeconds)
		|| RemainingContactSeconds <= 0.0
		|| !FMath::IsFinite(DriverPlaybackRate)
		|| DriverPlaybackRate <= UE_DOUBLE_SMALL_NUMBER
		|| !FMath::IsFinite(MarkerDeadlineSeconds)
		|| MarkerDeadlineSeconds <= 0.0)
	{
		OutFailureReason = TEXT("remaining predicted alignment or marker time is unavailable");
		return false;
	}
	const double RemainingAlignmentSeconds = FMath::Min(
		RemainingContactSeconds,
		MarkerDeadlineSeconds);

	const float RequiredDefenderTurn = FMath::Max(
		0.0f,
		(BridgeData->AttackerWarpConfig.FacingPolicy == EPairedFacingPolicy::FacePartner
			? FMath::Abs(Resolution.Decision.MeasuredYawDegrees)
			: GetAbsolutePairedYaw(Defender, SourceAttacker, BridgeData->AttackerWarpConfig.FacingPolicy))
			- Resolution.Decision.RequiredFinalTolerance);
	const float ConfiguredDefenderTurnBudget = DefenderConfiguration
		? DefenderConfiguration->MaximumAutomaticTurn
		: 0.0f;
	const float ConfiguredDefenderTurnRate = DefenderConfiguration
		? DefenderConfiguration->DefenseTurnRate
		: 0.0f;
	const float ConfiguredSourceTurnBudget = SourceConfiguration
		? SourceConfiguration->MaximumAutomaticTurn
		: 0.0f;
	const float ConfiguredSourceTurnRate = SourceConfiguration
		? SourceConfiguration->DefenseTurnRate
		: 0.0f;
	const float SourceYawToDefender = GetAbsolutePairedYaw(SourceAttacker, Defender, BridgeData->VictimWarpConfig.FacingPolicy);
	const float RequiredSourceTurn = FMath::Max(
		0.0f,
		SourceYawToDefender - Resolution.Decision.RequiredFinalTolerance);
	const float DefenderSimulationRate = DefenderCharacter->CustomTimeDilation;
	const float SourceSimulationRate = SourceCharacter->CustomTimeDilation;
	const float AvailableDefenderTurn = FMath::Min3(
		Resolution.Decision.AvailableTurnDegrees,
		ConfiguredDefenderTurnBudget,
		ConfiguredDefenderTurnRate
			* static_cast<float>(RemainingAlignmentSeconds)
			* DefenderSimulationRate);
	const float AvailableSourceTurn = FMath::Min(
		ConfiguredSourceTurnBudget,
		ConfiguredSourceTurnRate
			* static_cast<float>(RemainingAlignmentSeconds)
			* SourceSimulationRate);
	if (!FMath::IsFinite(RequiredDefenderTurn)
		|| !FMath::IsFinite(Resolution.Decision.MeasuredYawDegrees)
		|| !FMath::IsFinite(Resolution.Decision.RequiredFinalTolerance)
		|| Resolution.Decision.RequiredFinalTolerance < 0.0f
		|| !FMath::IsFinite(Resolution.Decision.AvailableTurnDegrees)
		|| Resolution.Decision.AvailableTurnDegrees < 0.0f
		|| !FMath::IsFinite(ConfiguredDefenderTurnBudget)
		|| ConfiguredDefenderTurnBudget < 0.0f
		|| !FMath::IsFinite(ConfiguredDefenderTurnRate)
		|| ConfiguredDefenderTurnRate < 0.0f
		|| !FMath::IsFinite(ConfiguredSourceTurnBudget)
		|| ConfiguredSourceTurnBudget < 0.0f
		|| !FMath::IsFinite(ConfiguredSourceTurnRate)
		|| ConfiguredSourceTurnRate < 0.0f
		|| !FMath::IsFinite(DefenderSimulationRate)
		|| DefenderSimulationRate <= 0.0f
		|| !FMath::IsFinite(SourceSimulationRate)
		|| SourceSimulationRate <= 0.0f
		|| !FMath::IsFinite(SourceYawToDefender)
		|| !FMath::IsFinite(AvailableDefenderTurn)
		|| !FMath::IsFinite(AvailableSourceTurn)
		|| RequiredDefenderTurn > AvailableDefenderTurn + KINDA_SMALL_NUMBER
		|| (BridgeData->VictimWarpConfig.bWarpRotation
			&& RequiredSourceTurn > AvailableSourceTurn + KINDA_SMALL_NUMBER))
	{
		OutFailureReason = TEXT("a role exceeds its perfect-parry rotation budget");
		return false;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Defender);
	ActorsToIgnore.Add(SourceAttacker);
	constexpr float PathClearanceRadius = 30.0f;
	const bool bPairPathClear = UPairedAnimationUtilityLibrary::IsPathClear(
		GetWorld(),
		Defender->GetActorLocation(),
		SourceAttacker->GetActorLocation(),
		PathClearanceRadius,
		ActorsToIgnore);
	const bool bDefenderWarpClear = !BridgeData->AttackerWarpConfig.bWarpTranslation
		|| UPairedAnimationUtilityLibrary::IsPathClear(
			GetWorld(),
			Defender->GetActorLocation(),
			DefenderDestination,
			PathClearanceRadius,
			ActorsToIgnore);
	const bool bSourceWarpClear = !BridgeData->VictimWarpConfig.bWarpTranslation
		|| UPairedAnimationUtilityLibrary::IsPathClear(
			GetWorld(),
			SourceAttacker->GetActorLocation(),
			SourceDestination,
			PathClearanceRadius,
			ActorsToIgnore);
	if (!bPairPathClear || !bDefenderWarpClear || !bSourceWarpClear)
	{
		OutFailureReason = TEXT("bridge path or role warp sweep is blocked");
		return false;
	}

	return true;
}

bool UPairedAnimationComponent::ScheduleNoMontageDefenseBridge(
	const int32 ExpectedStageGeneration)
{
	UWorld* World = GetWorld();
	if (!World
		|| ChainState != EChainCounterState::ParryActive
		|| ActiveDefenseSequence.StageGeneration != ExpectedStageGeneration)
	{
		return false;
	}

	ABaseCombatCharacter* Defender = GetOwnerCharacter();
	UCombatComponent* DefenderCombat = CachedCombatComponent
		? CachedCombatComponent.Get()
		: Defender ? Defender->CombatComponent.Get() : nullptr;
	const UDefenseConfiguration* Configuration = DefenderCombat
		? DefenderCombat->GetEffectiveDefenseConfiguration()
		: GetDefault<UDefenseConfiguration>();
	const float ConfiguredDelay = Configuration
		? Configuration->NoMontageParryBridgeSeconds
		: 0.15f;
	const float Delay = FMath::IsFinite(ConfiguredDelay) && ConfiguredDelay >= 0.0f
		? ConfiguredDelay
		: 0.15f;
	CancelDefenseAsyncHandle(ActiveDefenseSequence.BridgeFallbackHandle);
	const FDefenseAsyncHandle AsyncHandle = AllocateDefenseAsyncHandle();
	FTimerDelegate Delegate = FTimerDelegate::CreateUObject(
		this,
		&UPairedAnimationComponent::HandleNoMontageDefenseBridgeElapsed,
		ExpectedStageGeneration,
		AsyncHandle);
	FTimerHandle TimerHandle;
	World->GetTimerManager().SetTimer(
		TimerHandle,
		Delegate,
		FMath::Max(UE_SMALL_NUMBER, Delay),
		false);
	DefenseSimulationTimers.Add(AsyncHandle, TimerHandle);
	ActiveDefenseSequence.BridgeFallbackHandle = AsyncHandle;
	return true;
}

void UPairedAnimationComponent::HandleNoMontageDefenseBridgeElapsed(
	const int32 ExpectedStageGeneration,
	const FDefenseAsyncHandle AsyncHandle)
{
	DefenseSimulationTimers.Remove(AsyncHandle);
	if (ActiveDefenseSequence.BridgeFallbackHandle != AsyncHandle)
	{
		return;
	}
	ActiveDefenseSequence.BridgeFallbackHandle = {};
	EnterDefenseCounterWindow(ExpectedStageGeneration);
}

bool UPairedAnimationComponent::EnterDefenseCounterWindow(
	const int32 ExpectedStageGeneration)
{
	if (ChainState != EChainCounterState::ParryActive
		|| ActiveDefenseSequence.ChainState != EChainCounterState::ParryActive
		|| ActiveDefenseSequence.StageGeneration != ExpectedStageGeneration
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		return false;
	}
	if (!ActiveDefenseSequence.Defender.IsValid()
		|| !ActiveDefenseSequence.SourceAttacker.IsValid())
	{
		CleanupDefenseSequence(ExpectedStageGeneration, 0.1f, TEXT("BridgeParticipantInvalid"));
		return false;
	}

	ABaseCombatCharacter* Defender = Cast<ABaseCombatCharacter>(
		ActiveDefenseSequence.Defender.Get());
	ABaseCombatCharacter* SourceAttacker = Cast<ABaseCombatCharacter>(
		ActiveDefenseSequence.SourceAttacker.Get());
	UCombatComponent* SourceCombat = SourceAttacker
		? SourceAttacker->CombatComponent.Get()
		: nullptr;
	const FAttackInstanceId& AttackInstance =
		ActiveDefenseSequence.OriginatingInteraction.Key.AttackInstance;
	if (!Defender
		|| !SourceAttacker
		|| Defender->IsDeadOrDying()
		|| SourceAttacker->IsDeadOrDying()
		|| !SourceCombat
		|| !SourceCombat->IsAttackConsumed(AttackInstance))
	{
		CleanupDefenseSequence(ExpectedStageGeneration, 0.1f, TEXT("BridgeOwnershipInvalid"));
		return false;
	}

	CancelDefenseAsyncHandle(ActiveDefenseSequence.BridgeFallbackHandle);
	ActiveDefenseSequence.BridgeFallbackHandle = {};
	ChainState = EChainCounterState::CounterWindow;
	ActiveDefenseSequence.ChainState = EChainCounterState::CounterWindow;
	AppendDefenseSequenceTelemetry(
		CachedCombatComponent.Get(),
		ActiveDefenseSequence,
		EDefenseTelemetryEvent::StageTransition,
		EChainCounterState::CounterWindow);
	UPairedAnimationData* StageData = ActiveDefenseSequence.ActivePairedData.Get();
	if (StageData)
	{
		// The parried attacker stays held for the whole window. The stage start already linked it into its
		// ready loop; reassert it. It plays its remaining bridge frames into the hold rather than skipping
		// them; only a role whose authored sections can never reach the hold jumps to it. The defender's
		// bridge is left to play out: the player is free once it ends.
		LinkStageIntoReadySection(
			SourceAttacker->GetMesh() ? SourceAttacker->GetMesh()->GetAnimInstance() : nullptr,
			StageData->VictimMontage,
			StageData->ChainTransitionPolicy.VictimReadySection,
			true,
			true);
	}

	UCombatComponent* DefenderCombat = CachedCombatComponent
		? CachedCombatComponent.Get()
		: Defender->CombatComponent.Get();
	const UDefenseConfiguration* Configuration = DefenderCombat
		? DefenderCombat->GetEffectiveDefenseConfiguration()
		: GetDefault<UDefenseConfiguration>();
	const float PolicyOverride = ActiveDefenseSequence.ActivePairedData
		? ActiveDefenseSequence.ActivePairedData->ChainTransitionPolicy.ResponseWindowOverride
		: 0.0f;
	const float ConfiguredWindowDuration = FMath::IsFinite(PolicyOverride)
		&& PolicyOverride > 0.0f
		? PolicyOverride
		: Configuration
		? Configuration->CounterWindowSeconds
		: 2.0f;
	const float WindowDuration = FMath::IsFinite(ConfiguredWindowDuration)
		&& ConfiguredWindowDuration >= 0.0f
		? ConfiguredWindowDuration
		: 2.0f;
	ActiveChainContext.TimeInWindow = 0.0f;
	ActiveChainContext.WindowDuration = WindowDuration;
	ScheduleChainResponseDeadline(
		EChainCounterState::CounterWindow,
		WindowDuration,
		ExpectedStageGeneration);

	// A defender with no bridge montage left (the no-montage bridge, or a bridge whose end was already
	// handled before the source attacker's marker) is free from the moment the window opens. A defender
	// still in its bridge, including its blend-out, is released when that montage's end is handled.
	if (!StageData
		|| ActiveDefenseSequence.LastOwnerMontageEndHandledStageGeneration
			== ActiveDefenseSequence.StageGeneration)
	{
		ReleaseDefenderForResponseWindow();
	}

	// A response pressed during the bridge executes now that the window and its deadline exist, so a
	// failed counter start rolls back into this same window rather than an unscheduled one.
	if (DefenderCombat)
	{
		DefenderCombat->ReleaseBufferedChainResponse(ActiveDefenseSequence.OriginatingInteraction);
	}
	return true;
}

bool UPairedAnimationComponent::HandleDefenseAutoContinueMarker(
	const int32 ExpectedStageGeneration)
{
	if (ChainState != EChainCounterState::CounterActive
		|| ActiveDefenseSequence.ChainState != EChainCounterState::CounterActive
		|| ActiveDefenseSequence.StageGeneration != ExpectedStageGeneration)
	{
		return false;
	}
	UPairedAnimationData* CounterData = ActiveDefenseSequence.ActivePairedData.Get();
	if (!CounterData || !CounterData->ChainTransitionPolicy.bAutoContinue)
	{
		return false;
	}

	ApplyActivePairedDamageOnce();
	if (ActiveDefenseSequence.StageGeneration != ExpectedStageGeneration
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		return false;
	}
	ABaseCombatCharacter* Defender = Cast<ABaseCombatCharacter>(
		ActiveDefenseSequence.Defender.Get());
	ABaseCombatCharacter* SourceAttacker = Cast<ABaseCombatCharacter>(
		ActiveDefenseSequence.SourceAttacker.Get());
	if (!Defender || !SourceAttacker
		|| Defender->IsDeadOrDying()
		|| SourceAttacker->IsDeadOrDying())
	{
		CleanupDefenseSequence(
			ExpectedStageGeneration,
			0.0f,
			TEXT("AutoContinueParticipantInvalid"));
		return false;
	}
	UPairedAnimationData* FinisherData = ActiveDefenseSequence.FinisherData.Get();
	if (FinisherData
		&& TryStartDefenseChainStage(
			FinisherData,
			EPairedReactionType::Finisher,
			EChainCounterState::FinisherActive))
	{
		return true;
	}

	const bool bRetryable = CounterData->ChainTransitionPolicy.bFinisherRetryable
		&& FinisherData
		&& ActiveDefenseSequence.Defender.IsValid()
		&& ActiveDefenseSequence.SourceAttacker.IsValid();
	if (!bRetryable)
	{
		CleanupDefenseSequence(
			ActiveDefenseSequence.StageGeneration,
			0.1f,
			TEXT("AutoFinisherStartFailed"));
		return false;
	}

	ChainState = EChainCounterState::FinisherReady;
	ActiveDefenseSequence.ChainState = EChainCounterState::FinisherReady;
	AppendDefenseSequenceTelemetry(
		CachedCombatComponent.Get(),
		ActiveDefenseSequence,
		EDefenseTelemetryEvent::StageTransition,
		EChainCounterState::FinisherReady);
	// Resolve the defender's configuration as CounterWindow does, so the window's length never depends on
	// whether the component cache has been populated.
	UCombatComponent* DefenderCombat = CachedCombatComponent
		? CachedCombatComponent.Get()
		: Defender->CombatComponent.Get();
	const UDefenseConfiguration* Configuration = DefenderCombat
		? DefenderCombat->GetEffectiveDefenseConfiguration()
		: GetDefault<UDefenseConfiguration>();
	const float ConfiguredDuration = Configuration
		? Configuration->FinisherReadySeconds
		: 2.0f;
	const float Duration = FMath::IsFinite(ConfiguredDuration) && ConfiguredDuration >= 0.0f
		? ConfiguredDuration
		: 2.0f;
	ScheduleChainResponseDeadline(
		EChainCounterState::FinisherReady,
		Duration,
		ActiveDefenseSequence.StageGeneration);
	return false;
}

bool UPairedAnimationComponent::HandleOwnerPairedMontageBlendingOut(
	UAnimMontage* Montage,
	const bool bInterrupted)
{
	if (!Montage)
	{
		return false;
	}

	if (ChainState != EChainCounterState::None
		&& ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		if (!ActiveDefenseSequence.ActivePairedData
			|| Montage != ActiveDefenseSequence.ActivePairedData->AttackerMontage)
		{
			return false;
		}
		if (!bInterrupted && ChainState == EChainCounterState::FinisherActive)
		{
			ApplyActivePairedDamageOnce();
		}
		return true;
	}

	if (UPairedAnimationComponent* SequenceOwner = FindDefenseSequenceOwner())
	{
		return SequenceOwner->HandleSourcePairedMontageBlendingOut(
			GetOwner(), Montage, bInterrupted);
	}
	return false;
}

bool UPairedAnimationComponent::HandleOwnerPairedMontageEnded(
	UAnimMontage* Montage,
	const bool bInterrupted)
{
	if (ConsumeRetiredOwnerMontageCallback(Montage))
	{
		return true;
	}
	if (ChainState == EChainCounterState::None
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		if (UPairedAnimationComponent* SequenceOwner = FindDefenseSequenceOwner())
		{
			if (SequenceOwner->HandleSourcePairedMontageEnded(
				GetOwner(), Montage, bInterrupted))
			{
				return true;
			}
		}
		if (!ActivePairedAnimData || Montage != ActivePairedAnimData->AttackerMontage)
		{
			return false;
		}
		if (bInterrupted)
		{
			CancelPairedAnimation(0.0f);
		}
		else
		{
			CompletePairedAnimation();
		}
		return true;
	}

	// Any outgoing paired callback during a successor start is stale but still consumed.
	if (!ActiveDefenseSequence.ActivePairedData
		|| Montage != ActiveDefenseSequence.ActivePairedData->AttackerMontage)
	{
		return true;
	}
	const int32 Generation = ActiveDefenseSequence.StageGeneration;
	if (ActiveDefenseSequence.LastOwnerMontageEndHandledStageGeneration == Generation)
	{
		return true;
	}
	ActiveDefenseSequence.LastOwnerMontageEndHandledStageGeneration = Generation;
	if (bInterrupted)
	{
		CleanupDefenseSequence(Generation, 0.0f, TEXT("ActiveMontageInterrupted"));
		return true;
	}

	if (ChainState == EChainCounterState::FinisherActive)
	{
		ApplyActivePairedDamageOnce();
		CleanupDefenseSequence(Generation, 0.0f, TEXT("FinisherCompleted"));
		return true;
	}
	if (ChainState == EChainCounterState::CounterActive)
	{
		ApplyActivePairedDamageOnce();
		if (ActiveDefenseSequence.StageGeneration != Generation
			|| !ActiveDefenseSequence.OriginatingInteraction.IsValid())
		{
			return true;
		}
		ABaseCombatCharacter* Defender = Cast<ABaseCombatCharacter>(
			ActiveDefenseSequence.Defender.Get());
		ABaseCombatCharacter* SourceAttacker = Cast<ABaseCombatCharacter>(
			ActiveDefenseSequence.SourceAttacker.Get());
		if (!Defender || !SourceAttacker
			|| Defender->IsDeadOrDying()
			|| SourceAttacker->IsDeadOrDying())
		{
			CleanupDefenseSequence(
				Generation,
				0.0f,
				TEXT("CounterCompletionParticipantInvalid"));
			return true;
		}
		UPairedAnimationData* CounterData = ActiveDefenseSequence.ActivePairedData.Get();
		const bool bCanWaitForFinisher = ActiveDefenseSequence.FinisherData
			&& (!CounterData
				|| !CounterData->ChainTransitionPolicy.bAutoContinue
				|| CounterData->ChainTransitionPolicy.bFinisherRetryable);
		if (!bCanWaitForFinisher)
		{
			CleanupDefenseSequence(Generation, 0.0f, TEXT("CounterEndedWithoutSuccessor"));
			return true;
		}

		ChainState = EChainCounterState::FinisherReady;
		ActiveDefenseSequence.ChainState = EChainCounterState::FinisherReady;
		AppendDefenseSequenceTelemetry(
			CachedCombatComponent.Get(),
			ActiveDefenseSequence,
			EDefenseTelemetryEvent::StageTransition,
			EChainCounterState::FinisherReady);
		ActiveDefenseSequence.AttackerMontageInstanceId = INDEX_NONE;
		ActiveDefenseSequence.VictimMontageInstanceId = INDEX_NONE;
		const UCombatComponent* DefenderCombat = CachedCombatComponent
			? CachedCombatComponent.Get()
			: Defender->CombatComponent.Get();
		const UDefenseConfiguration* Configuration = DefenderCombat
			? DefenderCombat->GetEffectiveDefenseConfiguration()
			: GetDefault<UDefenseConfiguration>();
		const float ConfiguredDuration = Configuration
			? Configuration->FinisherReadySeconds
			: 2.0f;
		ScheduleChainResponseDeadline(
			EChainCounterState::FinisherReady,
			FMath::IsFinite(ConfiguredDuration) && ConfiguredDuration >= 0.0f
				? ConfiguredDuration
				: 2.0f,
			Generation);
		// The counter montage has ended: the player is free while the finisher prompt waits.
		ReleaseDefenderForResponseWindow();
		return true;
	}
	if (IsChainWaitingForResponse())
	{
		// An open response window is owned by its deadline, not by the stage montage that opened it. A
		// stage montage that finishes while the window waits (the bridge that opened CounterWindow) hands
		// the defender back to the AnimBP and the player; the source attacker stays held.
		AppendPairedStageActionReactionTelemetry(
			CachedCombatComponent.Get(),
			ActiveDefenseSequence,
			EActionReactionTelemetryEvent::MontageCallbackAccepted,
			EActionReactionTelemetryReason::MontageCompleted,
			ActiveDefenseSequence.ActivePairedData.Get(),
			GetOwner(),
			ActiveDefenseSequence.AttackerMontageInstanceId,
			nullptr,
			TEXT("stage montage completed while the response window stays open until its deadline"));
		ReleaseDefenderForResponseWindow();
		return true;
	}
	if (ChainState == EChainCounterState::ParryActive
		&& ActiveDefenseSequence.ActivePairedData->ChainTransitionPolicy.DriverRole
			!= EPairedAnimationRole::Attacker)
	{
		// The source attacker's marker drives this bridge: the defender stays committed, with no montage,
		// until that marker opens the window and releases it. The driver's hold entry is the failure net.
		AppendPairedStageActionReactionTelemetry(
			CachedCombatComponent.Get(),
			ActiveDefenseSequence,
			EActionReactionTelemetryEvent::MontageCallbackAccepted,
			EActionReactionTelemetryReason::MontageCompleted,
			ActiveDefenseSequence.ActivePairedData.Get(),
			GetOwner(),
			ActiveDefenseSequence.AttackerMontageInstanceId,
			nullptr,
			TEXT("defender bridge completed before the source attacker's marker opened the window"));
		return true;
	}

	// Only a defender-driven bridge that ends before its marker opened CounterWindow lands here.
	CleanupDefenseSequence(Generation, 0.0f, TEXT("BridgeEndedBeforeCounter"));
	return true;
}

bool UPairedAnimationComponent::HandleSourcePairedMontageBlendingOut(
	AActor* ReportingSource,
	UAnimMontage* Montage,
	const bool bInterrupted)
{
	if (!ReportingSource
		|| ChainState == EChainCounterState::None
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid()
		|| ActiveDefenseSequence.SourceAttacker.Get() != ReportingSource
		|| !ActiveDefenseSequence.ActivePairedData
		|| Montage != ActiveDefenseSequence.ActivePairedData->VictimMontage)
	{
		return false;
	}

	if (!bInterrupted && ChainState == EChainCounterState::FinisherActive)
	{
		ApplyActivePairedDamageOnce();
	}
	return true;
}

bool UPairedAnimationComponent::HandleSourcePairedMontageEnded(
	AActor* ReportingSource,
	UAnimMontage* Montage,
	const bool bInterrupted)
{
	if (!ReportingSource
		|| ChainState == EChainCounterState::None
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid()
		|| ActiveDefenseSequence.SourceAttacker.Get() != ReportingSource
		|| !ActiveDefenseSequence.ActivePairedData
		|| Montage != ActiveDefenseSequence.ActivePairedData->VictimMontage)
	{
		return false;
	}

	if (bInterrupted)
	{
		CleanupDefenseSequence(
			ActiveDefenseSequence.StageGeneration,
			0.0f,
			TEXT("SourceMontageInterrupted"));
	}
	else if (!IsChainWaitingForResponse())
	{
		// As for the owner role, an open response window ends only at its deadline or on response input,
		// so only a stage that is still playing verifies a natural source-role end.
		ScheduleSourceMontageEndVerification(
			Montage,
			ChainState,
			ActiveDefenseSequence.StageGeneration);
	}
	return true;
}

void UPairedAnimationComponent::ScheduleSourceMontageEndVerification(
	UAnimMontage* Montage,
	const EChainCounterState ExpectedState,
	const int32 ExpectedStageGeneration)
{
	if (!Montage
		|| ExpectedState == EChainCounterState::None
		|| ExpectedStageGeneration <= 0
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		return;
	}

	const FDefenseAsyncHandle AsyncHandle = AllocateDefenseAsyncHandle();
	const FDefenseInteractionId Interaction = ActiveDefenseSequence.OriginatingInteraction;
	const TWeakObjectPtr<UPairedAnimationComponent> WeakThis(this);
	const TWeakObjectPtr<UWorld> WeakWorld(GetWorld());
	const TWeakObjectPtr<UAnimMontage> WeakMontage(Montage);
	const FTSTicker::FDelegateHandle TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakThis, WeakWorld, Interaction, WeakMontage, ExpectedState,
				ExpectedStageGeneration, AsyncHandle](const float DeltaTime)
			{
				UPairedAnimationComponent* Component = WeakThis.Get();
				UWorld* World = WeakWorld.Get();
				return Component && World && Component->GetWorld() == World
					? Component->HandleSourceMontageEndVerification(
						Interaction,
						WeakMontage,
						ExpectedState,
						ExpectedStageGeneration,
						AsyncHandle,
						DeltaTime)
					: false;
			}),
		0.0f);
	DefenseResponseTickers.Add(AsyncHandle, TickerHandle);
}

bool UPairedAnimationComponent::HandleSourceMontageEndVerification(
	const FDefenseInteractionId Interaction,
	const TWeakObjectPtr<UAnimMontage> Montage,
	const EChainCounterState ExpectedState,
	const int32 ExpectedStageGeneration,
	const FDefenseAsyncHandle AsyncHandle,
	const float DeltaTime)
{
	(void)DeltaTime;
	DefenseResponseTickers.Remove(AsyncHandle);
	if (ActiveDefenseSequence.OriginatingInteraction != Interaction
		|| ActiveDefenseSequence.StageGeneration != ExpectedStageGeneration
		|| ActiveDefenseSequence.ChainState != ExpectedState
		|| ChainState != ExpectedState
		|| !ActiveDefenseSequence.ActivePairedData
		|| ActiveDefenseSequence.ActivePairedData->VictimMontage != Montage.Get())
	{
		return false;
	}

	if (ExpectedState == EChainCounterState::FinisherActive)
	{
		ApplyActivePairedDamageOnce();
		if (ActiveDefenseSequence.OriginatingInteraction == Interaction
			&& ActiveDefenseSequence.StageGeneration == ExpectedStageGeneration)
		{
			CleanupDefenseSequence(
				ExpectedStageGeneration,
				0.0f,
				TEXT("FinisherCompleted"));
		}
	}
	else
	{
		CleanupDefenseSequence(
			ExpectedStageGeneration,
			0.0f,
			TEXT("SourceMontageEndedFirst"));
	}
	return false;
}

// ============================================================================
// FINISHER EXECUTION
// ============================================================================

bool UPairedAnimationComponent::TryExecuteFinisher(UAttackData* AttackData)
{
	// Validate attack has finisher data
	if (!AttackData || !AttackData->FinisherData)
	{
		return false;
	}

	// Get owner character
	ABaseCombatCharacter* AttackerCharacter = GetOwnerCharacter();
	if (!AttackerCharacter)
	{
		return false;
	}

	// Get targeting component to find current target
	UTargetingComponent* TargetingComp = AttackerCharacter->GetTargetingComponent();
	if (!TargetingComp)
	{
		return false;
	}

	// Get current target - try hard-lock first, then fall back to soft-aim
	AActor* TargetActor = TargetingComp->GetCurrentTarget();
	if (!TargetActor)
	{
		// No hard-locked target - try soft-aim to find nearest enemy in facing direction
		const FVector FacingDirection = AttackerCharacter->GetActorForwardVector();
		TargetingComp->FindBestTargetForDirection(
			FacingDirection,
			TargetActor,
			-1.0f, -1.0f, -1.0f, -1.0f, -1.0f
		);

		if (!TargetActor)
		{
			return false;
		}

		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[FINISHER] No hard-lock, using soft-aim target: %s"),
				*TargetActor->GetName());
		}
	}

	if (!IsValidPairedTarget(TargetActor))
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Verbose, TEXT("[FINISHER] Rejecting non-hostile target %s"),
				*GetNameSafe(TargetActor));
		}
		return false;
	}

	// ========================================================================
	// FINISHER DISTANCE VALIDATION (Gap 16.2)
	// ========================================================================
	const float DistanceToTarget = FVector::Dist(
		AttackerCharacter->GetActorLocation(),
		TargetActor->GetActorLocation()
	);

	float MaxFinisherRange = 500.0f;  // Fallback value
	if (const UTargetingSettings* TargetingSettings = TargetingComp->GetEffectiveSettings())
	{
		MaxFinisherRange = TargetingSettings->SoftAimRange;
	}

	if (DistanceToTarget > MaxFinisherRange)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[FINISHER] Target %s too far: %.1f > %.1f (max range)"),
				*TargetActor->GetName(), DistanceToTarget, MaxFinisherRange);
		}
		return false;
	}

	// ========================================================================
	// GAP 19.6 FIX: Validate path is clear before executing finisher
	// ========================================================================
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(GetOwner());
	ActorsToIgnore.Add(TargetActor);

	const float PathClearanceRadius = 30.0f;
	if (!UPairedAnimationUtilityLibrary::IsPathClear(
		GetWorld(),
		AttackerCharacter->GetActorLocation(),
		TargetActor->GetActorLocation(),
		PathClearanceRadius,
		ActorsToIgnore))
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[FINISHER] Path to target %s is blocked by obstacle"),
				*TargetActor->GetName());
		}
		return false;
	}

	// Get target's hit reaction component
	UHitReactionComponent* TargetHitReaction = TargetActor->FindComponentByClass<UHitReactionComponent>();
	if (!TargetHitReaction)
	{
		return false;
	}

	// Check if target is vulnerable to finisher
	if (!TargetHitReaction->IsVulnerableToFinisher())
	{
		return false;
	}

	// Get finisher trigger reason for logging/context
	EFinisherTriggerReason TriggerReason = TargetHitReaction->GetFinisherTriggerReason();

	// Always log finisher execution for diagnostics (this is a major combat event)
	{
		ABaseCombatCharacter* TargetCombatChar = Cast<ABaseCombatCharacter>(TargetActor);
		UE_LOG(LogPairedAnim, Warning, TEXT("[FINISHER] EXECUTING on %s — Reason: %s, Health: %.1f/%.1f, Stunned: %s, Staggered: %s, IsDying: %s"),
			*TargetActor->GetName(),
			*UEnum::GetValueAsString(TriggerReason),
			TargetCombatChar ? TargetCombatChar->CurrentHealth : -1.0f,
			TargetCombatChar ? TargetCombatChar->MaxHealth : -1.0f,
			TargetHitReaction->IsStunned() ? TEXT("YES") : TEXT("NO"),
			TargetHitReaction->IsStaggered() ? TEXT("YES") : TEXT("NO"),
			TargetCombatChar ? (TargetCombatChar->IsDeadOrDying() ? TEXT("YES") : TEXT("NO")) : TEXT("N/A"));
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[FINISHER] Executing finisher: %s"), *AttackData->FinisherData->GetDisplayName());
		UE_LOG(LogPairedAnim, Log, TEXT("[FINISHER] Target: %s"), *TargetActor->GetName());
		UE_LOG(LogPairedAnim, Log, TEXT("[FINISHER] Trigger Reason: %s"), *UEnum::GetValueAsString(TriggerReason));
	}

	return TryStartPairedAnimationWithTarget(TargetActor, AttackData->FinisherData, EPairedReactionType::Finisher);
}

int32 UPairedAnimationComponent::AllocateDefenseStageGeneration()
{
	NextDefenseStageGeneration = NextDefenseStageGeneration == MAX_int32
		? 1
		: NextDefenseStageGeneration + 1;
	return NextDefenseStageGeneration;
}

int32 UPairedAnimationComponent::AllocateLegacyPairedGeneration()
{
	NextLegacyPairedGeneration = NextLegacyPairedGeneration == MIN_int32
		? -1
		: NextLegacyPairedGeneration - 1;
	return NextLegacyPairedGeneration;
}

bool UPairedAnimationComponent::BeginLegacyPairedParticipation(
	const int32 Generation,
	UPairedAnimationComponent* const SequenceOwner)
{
	if (Generation >= 0
		|| !SequenceOwner
		|| SequenceOwner == this
		|| ChainState != EChainCounterState::None
		|| ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		return false;
	}
	if (ActiveLegacyPairedGeneration == Generation
		&& !bOwnsLegacyPairedGeneration
		&& LegacyPairedSequenceOwner.Get() == SequenceOwner)
	{
		return true;
	}
	if (ActiveLegacyPairedGeneration != 0 || IsPairedAnimationActive())
	{
		return false;
	}

	ActiveLegacyPairedGeneration = Generation;
	bOwnsLegacyPairedGeneration = false;
	LegacyPairedSequenceOwner = SequenceOwner;
	return true;
}

void UPairedAnimationComponent::EndLegacyPairedParticipation(
	const int32 ExpectedGeneration,
	const UPairedAnimationComponent* const ExpectedOwner)
{
	if (ExpectedGeneration >= 0 || ActiveLegacyPairedGeneration != ExpectedGeneration)
	{
		return;
	}
	if (ExpectedOwner && LegacyPairedSequenceOwner.Get() != ExpectedOwner)
	{
		return;
	}

	ReleasePairedStateLeasesForGeneration(ExpectedGeneration);
	if (!bOwnsLegacyPairedGeneration)
	{
		// Expected lethal damage preserves this link through montage playback.
		// Release only the retiring sequence's partner when participation ends.
		if (const UPairedAnimationComponent* SequenceOwner = LegacyPairedSequenceOwner.Get())
		{
			RemovePairedPartner(SequenceOwner->GetOwner());
		}
	}
	if (bOwnsLegacyPairedGeneration)
	{
		AcceptedLegacyPairedParticipants.Reset();
	}
	ActiveLegacyPairedGeneration = 0;
	bOwnsLegacyPairedGeneration = false;
	LegacyPairedSequenceOwner.Reset();
}

void UPairedAnimationComponent::RegisterLegacyPairedParticipationForPartners()
{
	if (!bOwnsLegacyPairedGeneration || ActiveLegacyPairedGeneration >= 0)
	{
		return;
	}

	AcceptedLegacyPairedParticipants.RemoveAll(
		[](const TWeakObjectPtr<UPairedAnimationComponent>& ParticipantRef)
		{
			return !ParticipantRef.IsValid();
		});
	for (const TWeakObjectPtr<AActor>& PartnerRef : PairedAnimationPartners)
	{
		AActor* Partner = PartnerRef.Get();
		UPairedAnimationComponent* PartnerPaired = Partner
			? Partner->FindComponentByClass<UPairedAnimationComponent>()
			: nullptr;
		if (PartnerPaired
			&& PartnerPaired->BeginLegacyPairedParticipation(
				ActiveLegacyPairedGeneration,
				this))
		{
			AcceptedLegacyPairedParticipants.AddUnique(PartnerPaired);
		}
		else if (PartnerPaired)
		{
			UE_LOG(LogPairedAnim, Warning,
				TEXT("[PAIRED] %s could not acquire legacy participation generation %d"),
				*GetNameSafe(Partner),
				ActiveLegacyPairedGeneration);
		}
	}
}

void UPairedAnimationComponent::ReleaseLegacyPairedParticipationForPartners()
{
	if (!bOwnsLegacyPairedGeneration || ActiveLegacyPairedGeneration >= 0)
	{
		return;
	}

	const int32 EndingGeneration = ActiveLegacyPairedGeneration;
	for (const TWeakObjectPtr<UPairedAnimationComponent>& ParticipantRef :
		AcceptedLegacyPairedParticipants)
	{
		if (UPairedAnimationComponent* const Participant = ParticipantRef.Get())
		{
			Participant->EndLegacyPairedParticipation(EndingGeneration, this);
		}
	}
	AcceptedLegacyPairedParticipants.Reset();
}

bool UPairedAnimationComponent::HasAcceptedLegacyPairedParticipant(const AActor* Partner) const
{
	if (!Partner || !bOwnsLegacyPairedGeneration || ActiveLegacyPairedGeneration >= 0)
	{
		return false;
	}

	const UPairedAnimationComponent* const PartnerPaired =
		Partner->FindComponentByClass<UPairedAnimationComponent>();
	const bool bOwnerRecordedParticipant = AcceptedLegacyPairedParticipants.ContainsByPredicate(
		[PartnerPaired](const TWeakObjectPtr<UPairedAnimationComponent>& ParticipantRef)
		{
			return ParticipantRef.Get() == PartnerPaired;
		});
	return PartnerPaired
		&& bOwnerRecordedParticipant
		&& PartnerPaired->ActiveLegacyPairedGeneration == ActiveLegacyPairedGeneration
		&& !PartnerPaired->bOwnsLegacyPairedGeneration
		&& PartnerPaired->LegacyPairedSequenceOwner.Get() == this;
}

bool UPairedAnimationComponent::PreflightDefenseChainStage(
	UPairedAnimationData* PairedAnimData,
	const EPairedReactionType ReactionType,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	ABaseCombatCharacter* Defender = Cast<ABaseCombatCharacter>(ActiveDefenseSequence.Defender.Get());
	ABaseCombatCharacter* SourceAttacker = Cast<ABaseCombatCharacter>(ActiveDefenseSequence.SourceAttacker.Get());
	UPairedAnimationComponent* SourcePaired = SourceAttacker
		? SourceAttacker->PairedAnimationComponent.Get()
		: nullptr;
	UCombatComponent* DefenderCombat = CachedCombatComponent
		? CachedCombatComponent.Get()
		: Defender
			? Defender->CombatComponent.Get()
			: nullptr;
	UCombatComponent* SourceCombat = SourceAttacker
		? SourceAttacker->CombatComponent.Get()
		: nullptr;
	UTargetingComponent* DefenderTargeting = Defender
		? Defender->TargetingComponent.Get()
		: nullptr;
	UTargetingComponent* SourceTargeting = SourceAttacker
		? SourceAttacker->TargetingComponent.Get()
		: nullptr;
	ACharacter* DefenderCharacter = Cast<ACharacter>(Defender);
	ACharacter* SourceCharacter = Cast<ACharacter>(SourceAttacker);
	UAnimInstance* DefenderAnim = DefenderCharacter && DefenderCharacter->GetMesh()
		? DefenderCharacter->GetMesh()->GetAnimInstance()
		: nullptr;
	UAnimInstance* SourceAnim = SourceCharacter && SourceCharacter->GetMesh()
		? SourceCharacter->GetMesh()->GetAnimInstance()
		: nullptr;
	bool bCanUsePlaybackOverride = false;
#if WITH_AUTOMATION_TESTS
	bCanUsePlaybackOverride = static_cast<bool>(DefenseStagePlaybackOverrideForTesting);
#endif
	if (!PairedAnimData
		|| !Defender
		|| !SourceAttacker
		|| !SourcePaired
		|| !DefenderCombat
		|| !SourceCombat
		|| !DefenderTargeting
		|| !SourceTargeting
		|| !SourceAttacker->HitReactionComponent
		|| ((!DefenderAnim || !SourceAnim) && !bCanUsePlaybackOverride)
		|| Defender->IsDeadOrDying()
		|| SourceAttacker->IsDeadOrDying()
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		OutFailureReason = TEXT("missing or invalid retained participant");
		return false;
	}
	if (PairedAnimData->ReactionType != ReactionType
		|| !PairedAnimData->AttackerMontage
		|| !PairedAnimData->VictimMontage
		|| (!PairedAnimData->AttackerMontageSection.IsNone()
			&& !PairedAnimData->AttackerMontage->IsValidSectionName(PairedAnimData->AttackerMontageSection))
		|| (!PairedAnimData->VictimMontageSection.IsNone()
			&& !PairedAnimData->VictimMontage->IsValidSectionName(PairedAnimData->VictimMontageSection)))
	{
		OutFailureReason = TEXT("paired role montage, section, or reaction is invalid");
		return false;
	}
	if (!HasValidPairedRuntimeNumerics(*PairedAnimData))
	{
		OutFailureReason = TEXT("retained stage playback, damage, or effects numeric configuration is invalid");
		return false;
	}
	if (const UPairedAnimationData* PreviousStage =
		ActiveDefenseSequence.ActivePairedData.Get())
	{
		if (PreviousStage->AttackerMontage == PairedAnimData->AttackerMontage
			|| PreviousStage->VictimMontage == PairedAnimData->VictimMontage)
		{
			OutFailureReason = TEXT("adjacent retained stages require distinct role montages for callback identity");
			return false;
		}
	}
	if (RetiredOwnerMontageCallbacks.Contains(PairedAnimData->AttackerMontage)
		|| SourcePaired->RetiredOwnerMontageCallbacks.Contains(PairedAnimData->VictimMontage))
	{
		OutFailureReason = TEXT("a retained role montage still has an unresolved prior callback");
		return false;
	}
	if (!PairedAnimData->AttackerWarpConfig.bWarpRotation
		|| !PairedAnimData->VictimWarpConfig.bWarpRotation
		|| PairedAnimData->AttackerWarpConfig.WarpTargetName.IsNone()
		|| PairedAnimData->VictimWarpConfig.WarpTargetName.IsNone())
	{
		OutFailureReason = TEXT("canonical defense roles require named rotation warp targets");
		return false;
	}

	const FVector DefenderLocation = Defender->GetActorLocation();
	const FVector SourceLocation = SourceAttacker->GetActorLocation();
	const float PairDistance = FVector::Dist(DefenderLocation, SourceLocation);
	if (!IsValidPairedTarget(SourceAttacker)
		|| !FMath::IsFinite(PairDistance)
		|| !FMath::IsFinite(PairedAnimData->MinTriggerDistance)
		|| !FMath::IsFinite(PairedAnimData->MaxTriggerDistance)
		|| PairedAnimData->MinTriggerDistance < 0.0f
		|| PairedAnimData->MaxTriggerDistance <= PairedAnimData->MinTriggerDistance
		|| PairDistance > PairedAnimData->MaxTriggerDistance)
	{
		OutFailureReason = TEXT("retained participants are invalid or outside the stage trigger range");
		return false;
	}

	const FPairedWarpConfig& DefenderWarp = PairedAnimData->AttackerWarpConfig;
	const FPairedWarpConfig& SourceWarp = PairedAnimData->VictimWarpConfig;
	if (!UPairedAnimationUtilityLibrary::IsValidFacingPolicy(DefenderWarp.FacingPolicy)
		|| !UPairedAnimationUtilityLibrary::IsValidFacingPolicy(SourceWarp.FacingPolicy))
	{
		OutFailureReason = TEXT("retained stage has an invalid facing policy");
		return false;
	}
	if (!FMath::IsFinite(PairedAnimData->MaxWarpDistance)
		|| PairedAnimData->MaxWarpDistance < 0.0f
		|| !FMath::IsFinite(DefenderWarp.MaxWarpDistance)
		|| DefenderWarp.MaxWarpDistance < 0.0f
		|| !FMath::IsFinite(SourceWarp.MaxWarpDistance)
		|| SourceWarp.MaxWarpDistance < 0.0f
		|| DefenderWarp.RelativeOffset.ContainsNaN()
		|| SourceWarp.RelativeOffset.ContainsNaN())
	{
		OutFailureReason = TEXT("retained stage has an invalid role translation budget");
		return false;
	}

	const FVector DefenderDestination = SourceLocation
		+ SourceAttacker->GetActorRotation().RotateVector(DefenderWarp.RelativeOffset);
	const FVector SourceDestination = DefenderLocation
		+ Defender->GetActorRotation().RotateVector(SourceWarp.RelativeOffset);
	auto IsRoleTranslationValid = [PairedAnimData](
		const AActor* Role,
		const FVector& Destination,
		const FPairedWarpConfig& WarpConfig)
	{
		if (!WarpConfig.bWarpTranslation)
		{
			return true;
		}
		const float Allowed = FMath::Min(
			PairedAnimData->MaxWarpDistance,
			WarpConfig.MaxWarpDistance);
		const float Required = FVector::Dist(Role->GetActorLocation(), Destination);
		return FMath::IsFinite(Required) && Required <= Allowed + KINDA_SMALL_NUMBER;
	};
	if (!IsRoleTranslationValid(Defender, DefenderDestination, DefenderWarp)
		|| !IsRoleTranslationValid(SourceAttacker, SourceDestination, SourceWarp))
	{
		OutFailureReason = TEXT("a retained role exceeds its stage translation budget");
		return false;
	}

	const UDefenseConfiguration* SourceConfiguration =
		SourceCombat->GetEffectiveDefenseConfiguration();
	const float SourceInitialBudget = SourceConfiguration
		? SourceConfiguration->MaximumAutomaticTurn
		: 70.0f;
	const float DefenderInitialBudget =
		ActiveDefenseSequence.OriginatingResolution.Decision.AvailableTurnDegrees;
	const FDefenseStageAlignmentLimits DefenderAlignmentLimits =
		ResolveDefenseStageAlignmentLimits(
			DefenderCombat,
			DefenderTargeting,
			ActiveDefenseSequence.AttackerAlignmentLease,
			DefenderInitialBudget);
	const FDefenseStageAlignmentLimits SourceAlignmentLimits =
		ResolveDefenseStageAlignmentLimits(
			SourceCombat,
			SourceTargeting,
			ActiveDefenseSequence.VictimAlignmentLease,
			SourceInitialBudget);
	const float RequiredTolerance =
		ActiveDefenseSequence.OriginatingResolution.Decision.RequiredFinalTolerance;
	auto IsRoleRotationValid = [RequiredTolerance](
		const AActor* Role,
		const AActor* Target,
		EPairedFacingPolicy Policy,
		const FDefenseStageAlignmentLimits& Limits)
	{
		const float RequiredYaw = GetAbsolutePairedYaw(Role, Target, Policy);
		if (!FMath::IsFinite(RequiredYaw)
			|| !FMath::IsFinite(RequiredTolerance)
			|| RequiredTolerance < 0.0f
			|| !FMath::IsFinite(Limits.MaximumTurnRate)
			|| !FMath::IsFinite(Limits.RemainingTurnBudget))
		{
			return false;
		}
		const float RequiredCorrection = FMath::Max(0.0f, RequiredYaw - RequiredTolerance);
		return RequiredCorrection <= Limits.RemainingTurnBudget + KINDA_SMALL_NUMBER
			&& (RequiredCorrection <= KINDA_SMALL_NUMBER
				|| Limits.MaximumTurnRate > KINDA_SMALL_NUMBER);
	};
	if (!IsRoleRotationValid(Defender, SourceAttacker, DefenderWarp.FacingPolicy, DefenderAlignmentLimits)
		|| !IsRoleRotationValid(SourceAttacker, Defender, SourceWarp.FacingPolicy, SourceAlignmentLimits))
	{
		OutFailureReason = TEXT("a retained role exceeds its remaining rotation budget");
		return false;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Defender);
	ActorsToIgnore.Add(SourceAttacker);
	constexpr float PathClearanceRadius = 30.0f;
	const bool bPairPathClear = UPairedAnimationUtilityLibrary::IsPathClear(
		GetWorld(), DefenderLocation, SourceLocation, PathClearanceRadius, ActorsToIgnore);
	const bool bDefenderWarpClear = !DefenderWarp.bWarpTranslation
		|| UPairedAnimationUtilityLibrary::IsPathClear(
			GetWorld(), DefenderLocation, DefenderDestination, PathClearanceRadius, ActorsToIgnore);
	const bool bSourceWarpClear = !SourceWarp.bWarpTranslation
		|| UPairedAnimationUtilityLibrary::IsPathClear(
			GetWorld(), SourceLocation, SourceDestination, PathClearanceRadius, ActorsToIgnore);
	if (!bPairPathClear || !bDefenderWarpClear || !bSourceWarpClear)
	{
		OutFailureReason = TEXT("retained stage path or role warp sweep is blocked");
		return false;
	}

	const FPairedChainTransitionPolicy& Policy = PairedAnimData->ChainTransitionPolicy;
	const UAnimMontage* DriverMontage = Policy.DriverRole == EPairedAnimationRole::Attacker
		? PairedAnimData->AttackerMontage.Get()
		: PairedAnimData->VictimMontage.Get();
	const FName DriverSection = Policy.DriverRole == EPairedAnimationRole::Attacker
		? PairedAnimData->AttackerMontageSection
		: PairedAnimData->VictimMontageSection;
	if ((!Policy.AttackerReadySection.IsNone()
			&& !PairedAnimData->AttackerMontage->IsValidSectionName(
				Policy.AttackerReadySection))
		|| (!Policy.VictimReadySection.IsNone()
			&& !PairedAnimData->VictimMontage->IsValidSectionName(
				Policy.VictimReadySection)))
	{
		OutFailureReason = TEXT("retained stage references a missing role ready section");
		return false;
	}
	if (ReactionType == EPairedReactionType::Parry
		&& (Policy.bAutoContinue
			|| Policy.RequiredMarker.IsNone()
			|| !Policy.HasRetainableReadyPose()
			|| !UAnimNotify_ChainStageTransition::HasExactlyOnePlayableMarker(
				DriverMontage,
				Policy.RequiredMarker,
				EChainStageTransitionType::OpenCounterWindow,
				DriverSection)))
	{
		OutFailureReason = TEXT("parry bridge lacks an unambiguous retained-pose marker policy");
		return false;
	}
	if (ReactionType == EPairedReactionType::Counter && Policy.bAutoContinue)
	{
		if (!UAnimNotify_ChainStageTransition::HasExactlyOnePlayableMarker(
			DriverMontage,
			Policy.RequiredMarker,
			EChainStageTransitionType::AutoContinue,
			DriverSection))
		{
			OutFailureReason = TEXT("auto-continuing counter lacks one driver marker inside its played section");
			return false;
		}
	}
	return true;
}

bool UPairedAnimationComponent::TryStartDefenseChainStage(
	UPairedAnimationData* PairedAnimData,
	const EPairedReactionType ReactionType,
	const EChainCounterState SuccessState)
{
	FString FailureReason;
	if (!PreflightDefenseChainStage(PairedAnimData, ReactionType, FailureReason))
	{
		UE_LOG(LogPairedAnim, Warning,
			TEXT("[COUNTER-CHAIN] Stage preflight failed: %s"),
			*FailureReason);
		AppendPairedStageActionReactionTelemetry(
			CachedCombatComponent.Get(),
			ActiveDefenseSequence,
			EActionReactionTelemetryEvent::PairedStageStartFailed,
			EActionReactionTelemetryReason::StagePreflightFailed,
			PairedAnimData,
			ActiveDefenseSequence.SourceAttacker.Get(),
			INDEX_NONE,
			nullptr,
			FailureReason);
		return false;
	}

	ABaseCombatCharacter* Defender = Cast<ABaseCombatCharacter>(ActiveDefenseSequence.Defender.Get());
	ABaseCombatCharacter* SourceAttacker = Cast<ABaseCombatCharacter>(ActiveDefenseSequence.SourceAttacker.Get());
	UPairedAnimationComponent* SourcePaired = SourceAttacker->PairedAnimationComponent.Get();
	UTargetingComponent* DefenderTargeting = Defender->TargetingComponent.Get();
	UTargetingComponent* SourceTargeting = SourceAttacker->TargetingComponent.Get();
	UHitReactionComponent* SourceHitReaction = SourceAttacker->HitReactionComponent.Get();
	UCombatComponent* DefenderCombat = Defender->CombatComponent.Get();
	UCombatComponent* SourceCombat = SourceAttacker->CombatComponent.Get();
	UAnimInstance* DefenderAnim = Defender->GetMesh()->GetAnimInstance();
	UAnimInstance* SourceAnim = SourceAttacker->GetMesh()->GetAnimInstance();
	if (!SourcePaired || !DefenderTargeting || !SourceTargeting || !SourceHitReaction
		|| !DefenderCombat || !SourceCombat)
	{
		AppendPairedStageActionReactionTelemetry(
			CachedCombatComponent.Get(),
			ActiveDefenseSequence,
			EActionReactionTelemetryEvent::PairedStageStartFailed,
			EActionReactionTelemetryReason::StageDependenciesMissing,
			PairedAnimData,
			SourceAttacker,
			INDEX_NONE,
			nullptr,
			FString::Printf(
				TEXT("source_paired=%s defender_targeting=%s source_targeting=%s source_reaction=%s defender_combat=%s source_combat=%s"),
				SourcePaired ? TEXT("true") : TEXT("false"),
				DefenderTargeting ? TEXT("true") : TEXT("false"),
				SourceTargeting ? TEXT("true") : TEXT("false"),
				SourceHitReaction ? TEXT("true") : TEXT("false"),
				DefenderCombat ? TEXT("true") : TEXT("false"),
				SourceCombat ? TEXT("true") : TEXT("false")));
		return false;
	}

	const FDefenseSequenceContext Previous = ActiveDefenseSequence;
	// The outgoing stage's montages, and the source attacker's held victim state, follow the retained stage
	// data. ActivePairedAnimData is the defender's own paired status, which a response window that released
	// the defender has already cleared.
	UPairedAnimationData* PreviousStageData = Previous.ActivePairedData.Get();
	UPairedAnimationData* PreviousOwnerData = ActivePairedAnimData.Get();
	const EPairedReactionType PreviousReaction = ActivePairedReactionType;
	const TWeakObjectPtr<AActor> PreviousVictim = CurrentFinisherVictim;
	const EChainCounterState PreviousChainState = ChainState;
	const double PreservedDeadline = Previous.ResponseDeadlineUnscaled;
	const int32 SuccessorGeneration = AllocateDefenseStageGeneration();

	ActiveDefenseSequence.StageGeneration = SuccessorGeneration;
	ActiveDefenseSequence.ActivePairedData = PairedAnimData;
	ActiveDefenseSequence.AttackerMontageInstanceId = INDEX_NONE;
	ActiveDefenseSequence.VictimMontageInstanceId = INDEX_NONE;
	ActivePairedAnimData = PairedAnimData;
	ActivePairedReactionType = ReactionType;
	CurrentFinisherVictim = SourceAttacker;

	AddPairedPartner(SourceAttacker);
	SourcePaired->AddPairedPartner(Defender);

	const FPairedSequenceLeaseHandle NewDefenderCollision = AcquirePairedStateLease(
		TEXT("DefenseChainStage"), SuccessorGeneration,
		true, true, false, true, false, 150.0f);
	const FPairedSequenceLeaseHandle NewSourceCollision = SourcePaired->AcquirePairedStateLease(
		TEXT("DefenseChainStage"), SuccessorGeneration,
		true, true, false, true, false, 150.0f);

	const UDefenseConfiguration* SourceConfiguration =
		SourceCombat->GetEffectiveDefenseConfiguration();
	const float SourceInitialBudget = SourceConfiguration
		? SourceConfiguration->MaximumAutomaticTurn
		: 70.0f;
	const FDefenseStageAlignmentLimits DefenderAlignmentLimits =
		ResolveDefenseStageAlignmentLimits(
			DefenderCombat,
			DefenderTargeting,
			Previous.AttackerAlignmentLease,
			Previous.OriginatingResolution.Decision.AvailableTurnDegrees);
	const FDefenseStageAlignmentLimits SourceAlignmentLimits =
		ResolveDefenseStageAlignmentLimits(
			SourceCombat,
			SourceTargeting,
			Previous.VictimAlignmentLease,
			SourceInitialBudget);
	const UDefenseConfiguration* DefenderConfiguration =
		DefenderCombat->GetEffectiveDefenseConfiguration();
	auto ResolveTranslationBudget = [PairedAnimData, ReactionType, DefenderConfiguration, &Previous](
		const FPairedWarpConfig& Warp)
	{
		if (!Warp.bWarpTranslation)
		{
			return 0.0f;
		}
		float Allowed = FMath::Min(
			FMath::Max(0.0f, PairedAnimData->MaxWarpDistance),
			FMath::Max(0.0f, Warp.MaxWarpDistance));
		if (ReactionType == EPairedReactionType::Parry)
		{
			const float ConfiguredAllowance = DefenderConfiguration
				? FMath::Max(
					0.0f,
					DefenderConfiguration->PerfectParryTranslationAllowancePerRole)
				: 0.0f;
			Allowed = FMath::Min(Allowed, ConfiguredAllowance);
			if (Previous.ActivePresentation.MaximumTranslation > 0.0f)
			{
				Allowed = FMath::Min(
					Allowed,
					Previous.ActivePresentation.MaximumTranslation);
			}
		}
		return Allowed;
	};

	auto BuildAlignmentSpec = [SuccessorGeneration](
		AActor* Owner,
		AActor* Target,
		const FPairedWarpConfig& Warp,
		const FName OwnerId,
		const FDefenseStageAlignmentLimits& Limits,
		const float MaximumTranslation)
	{
		FAlignmentRequestSpec Spec;
		Spec.OwnerId = OwnerId;
		Spec.OwnerGeneration = SuccessorGeneration;
		Spec.Priority = EDefenseAlignmentPriority::PairedOrParryBridge;
		Spec.Executor = EAlignmentExecutor::MotionWarping;
		Spec.Target = Target;
		Spec.TargetRelativeOffset = Warp.RelativeOffset;
		Spec.FacingPolicy = Warp.FacingPolicy;
		Spec.DesiredRotation = Target
			? UPairedAnimationUtilityLibrary::ResolvePairedFacingRotation(
				Owner->GetActorLocation(), Owner->GetActorRotation(), Target->GetActorTransform(), Warp.FacingPolicy)
			: Owner->GetActorRotation();
		Spec.MaximumTurnRate = Limits.MaximumTurnRate;
		Spec.RemainingTurnBudget = Limits.RemainingTurnBudget;
		Spec.MaximumTranslation = MaximumTranslation;
		Spec.WarpTargetName = Warp.WarpTargetName;
		Spec.bTrackTargetRotation = Warp.bWarpRotation;
		Spec.bWarpTranslation = Warp.bWarpTranslation;
		return Spec;
	};

	FAlignmentRequestHandle NewDefenderAlignment;
	FAlignmentRequestHandle NewSourceAlignment;
	FAlignmentRequestSpec PreviousDefenderAlignmentSpec;
	FAlignmentRequestSpec PreviousSourceAlignmentSpec;
	bool bUpdatedDefenderAlignment = false;
	bool bUpdatedSourceAlignment = false;
	auto AcquireOrUpdateAlignment = [](
		UTargetingComponent* Targeting,
		const FAlignmentRequestHandle Existing,
		const FAlignmentRequestSpec& Desired,
		FAlignmentRequestSpec& OutPrevious,
		bool& bOutUpdated)
	{
		if (Existing.IsValid()
			&& Targeting->GetAlignmentRequestSpec(Existing, OutPrevious)
			&& OutPrevious.WarpTargetName == Desired.WarpTargetName)
		{
			FAlignmentRequestSpec Updated = Desired;
			Updated.OwnerId = OutPrevious.OwnerId;
			Updated.OwnerGeneration = OutPrevious.OwnerGeneration;
			Updated.Priority = OutPrevious.Priority;
			Updated.Executor = OutPrevious.Executor;
			Updated.WarpTargetName = OutPrevious.WarpTargetName;
			bOutUpdated = Targeting->UpdateAlignmentRequest(Existing, Updated);
			return bOutUpdated ? Existing : FAlignmentRequestHandle{};
		}
		return Targeting->AcquireAlignmentRequest(Desired);
	};

	const FAlignmentRequestSpec DefenderAlignmentSpec = BuildAlignmentSpec(
		Defender,
		SourceAttacker,
		PairedAnimData->AttackerWarpConfig,
		TEXT("DefenseChainAttacker"),
		DefenderAlignmentLimits,
		ResolveTranslationBudget(PairedAnimData->AttackerWarpConfig));
	const FAlignmentRequestSpec SourceAlignmentSpec = BuildAlignmentSpec(
		SourceAttacker,
		Defender,
		PairedAnimData->VictimWarpConfig,
		TEXT("DefenseChainVictim"),
		SourceAlignmentLimits,
		ResolveTranslationBudget(PairedAnimData->VictimWarpConfig));
	NewDefenderAlignment = AcquireOrUpdateAlignment(
		DefenderTargeting,
		Previous.AttackerAlignmentLease,
		DefenderAlignmentSpec,
		PreviousDefenderAlignmentSpec,
		bUpdatedDefenderAlignment);
	NewSourceAlignment = AcquireOrUpdateAlignment(
		SourceTargeting,
		Previous.VictimAlignmentLease,
		SourceAlignmentSpec,
		PreviousSourceAlignmentSpec,
		bUpdatedSourceAlignment);

	FTimeDilationLeaseHandle NewTimeLease = Previous.TimeDilationLease;
	bool bAcquiredNewTimeLease = false;
	if (PairedAnimData->bApplySlowMotion)
	{
		const UDefenseConfiguration* Configuration = CachedCombatComponent
			? CachedCombatComponent->GetEffectiveDefenseConfiguration()
			: GetDefault<UDefenseConfiguration>();
		const double Watchdog = Configuration
			? static_cast<double>(Configuration->TimeDilationLeaseWatchdogSeconds)
			: 10.0;
		if (UCombatEffectsWorldSubsystem* Effects = GetWorld()
			? GetWorld()->GetSubsystem<UCombatEffectsWorldSubsystem>()
			: nullptr)
		{
			NewTimeLease = Effects->AcquireWorldLease(
				TEXT("DefenseChainStage"),
				FMath::Clamp(PairedAnimData->SlowMotionScale, 0.0001f, 1.0f),
				FMath::IsFinite(Watchdog) && Watchdog > 0.0 ? Watchdog : 10.0);
			bAcquiredNewTimeLease = NewTimeLease.IsValid();
		}
	}

	const bool bOwnershipReady = NewDefenderCollision.IsValid()
		&& NewSourceCollision.IsValid()
		&& NewDefenderAlignment.IsValid()
		&& NewSourceAlignment.IsValid()
		&& (!PairedAnimData->bApplySlowMotion || bAcquiredNewTimeLease);
	bool bDefenderStarted = false;
	bool bSourceStarted = false;
	bool bUsedPlaybackOverride = false;
	int32 DefenderMontageInstanceId = INDEX_NONE;
	int32 SourceMontageInstanceId = INDEX_NONE;
	if (bOwnershipReady)
	{
		const bool bHadOutgoingOwnerMontage = PreviousStageData
			&& DefenderAnim
			&& DefenderAnim->Montage_IsPlaying(PreviousStageData->AttackerMontage);
		const bool bHadOutgoingSourceMontage = PreviousStageData
			&& SourceAnim
			&& SourceAnim->Montage_IsPlaying(PreviousStageData->VictimMontage);
		if (bHadOutgoingOwnerMontage)
		{
			RetireOwnerMontageCallback(PreviousStageData->AttackerMontage);
		}
		if (bHadOutgoingSourceMontage)
		{
			SourcePaired->RetireOwnerMontageCallback(PreviousStageData->VictimMontage);
		}
		// The defender's own knockback push ends when its stage starts. Its stage bridge outranks the push, so
		// without this the push would only be suspended and could resume once the chain releases the bridge.
		if (UHitReactionComponent* DefenderHitReaction = Defender->HitReactionComponent.Get())
		{
			DefenderHitReaction->ReleaseKnockback(TEXT("ChainStart"));
		}
		SourceHitReaction->EnterPairedAnimationState(
			PairedAnimData->VictimMontage,
			PairedAnimData->VictimDeathOutcome,
			PairedAnimData->RagdollBlendTime,
			ShouldTreatPairedAnimationAsLethal(ReactionType, PairedAnimData),
			Defender);

#if WITH_AUTOMATION_TESTS
		if (DefenseStagePlaybackOverrideForTesting)
		{
			bUsedPlaybackOverride = true;
			bDefenderStarted = DefenseStagePlaybackOverrideForTesting(
				EPairedAnimationRole::Attacker,
				PairedAnimData,
				DefenderMontageInstanceId);
		}
		else
#endif
		{
			const float DefenderLength = DefenderAnim->Montage_PlayWithBlendIn(
				PairedAnimData->AttackerMontage,
				FAlphaBlendArgs(FMath::Max(0.0f, PairedAnimData->AttackerBlendIn)),
				1.0f,
				EMontagePlayReturnType::MontageLength,
				0.0f,
				true);
			bDefenderStarted = DefenderLength > 0.0f;
		}
		if (!bDefenderStarted && bHadOutgoingOwnerMontage
			&& DefenderAnim->Montage_IsPlaying(PreviousStageData->AttackerMontage))
		{
			CancelRetiredOwnerMontageCallback(PreviousStageData->AttackerMontage);
		}
		if (!bDefenderStarted && bHadOutgoingSourceMontage
			&& SourceAnim->Montage_IsPlaying(PreviousStageData->VictimMontage))
		{
			SourcePaired->CancelRetiredOwnerMontageCallback(PreviousStageData->VictimMontage);
		}
		if (bDefenderStarted && !bUsedPlaybackOverride)
		{
			if (!PairedAnimData->AttackerMontageSection.IsNone())
			{
				DefenderAnim->Montage_JumpToSection(
					PairedAnimData->AttackerMontageSection,
					PairedAnimData->AttackerMontage);
			}
			if (FAnimMontageInstance* Instance =
				DefenderAnim->GetActiveInstanceForMontage(PairedAnimData->AttackerMontage))
			{
				DefenderMontageInstanceId = Instance->GetInstanceID();
			}
		}

		if (bDefenderStarted && DefenderMontageInstanceId >= 0)
		{
#if WITH_AUTOMATION_TESTS
			if (bUsedPlaybackOverride)
			{
				bSourceStarted = DefenseStagePlaybackOverrideForTesting(
					EPairedAnimationRole::Victim,
					PairedAnimData,
					SourceMontageInstanceId);
			}
			else
#endif
			{
				const float SourceLength = SourceAnim->Montage_PlayWithBlendIn(
					PairedAnimData->VictimMontage,
					FAlphaBlendArgs(FMath::Max(0.0f, PairedAnimData->VictimBlendIn)),
					1.0f,
					EMontagePlayReturnType::MontageLength,
					FMath::Max(0.0f, -PairedAnimData->VictimStartOffset),
					true);
				bSourceStarted = SourceLength > 0.0f;
			}
			if (bSourceStarted && !bUsedPlaybackOverride)
			{
				if (!PairedAnimData->VictimMontageSection.IsNone())
				{
					SourceAnim->Montage_JumpToSection(
						PairedAnimData->VictimMontageSection,
						PairedAnimData->VictimMontage);
				}
				if (FAnimMontageInstance* Instance =
					SourceAnim->GetActiveInstanceForMontage(PairedAnimData->VictimMontage))
				{
					SourceMontageInstanceId = Instance->GetInstanceID();
				}
			}
		}
	}

	const bool bStarted = bOwnershipReady
		&& bDefenderStarted
		&& bSourceStarted
		&& DefenderMontageInstanceId >= 0
		&& SourceMontageInstanceId >= 0;
	if (!bStarted)
	{
		AppendPairedStageActionReactionTelemetry(
			DefenderCombat,
			ActiveDefenseSequence,
			EActionReactionTelemetryEvent::PairedStageStartFailed,
			bOwnershipReady
				? EActionReactionTelemetryReason::StagePlaybackFailed
				: EActionReactionTelemetryReason::StageOwnershipFailed,
			PairedAnimData,
			SourceAttacker,
			DefenderMontageInstanceId,
			nullptr,
			FString::Printf(
				TEXT("ownership=%s defender_lease=%s source_lease=%s defender_alignment=%s source_alignment=%s time_lease=%s defender_started=%s source_started=%s defender_instance=%d source_instance=%d"),
				bOwnershipReady ? TEXT("true") : TEXT("false"),
				NewDefenderCollision.IsValid() ? TEXT("true") : TEXT("false"),
				NewSourceCollision.IsValid() ? TEXT("true") : TEXT("false"),
				NewDefenderAlignment.IsValid() ? TEXT("true") : TEXT("false"),
				NewSourceAlignment.IsValid() ? TEXT("true") : TEXT("false"),
				(!PairedAnimData->bApplySlowMotion || bAcquiredNewTimeLease)
					? TEXT("true") : TEXT("false"),
				bDefenderStarted ? TEXT("true") : TEXT("false"),
				bSourceStarted ? TEXT("true") : TEXT("false"),
				DefenderMontageInstanceId,
				SourceMontageInstanceId));
		ActiveDefenseSequence = Previous;
		ActiveDefenseSequence.StageGeneration = SuccessorGeneration;
		if (Previous.LastDamageAppliedStageGeneration == Previous.StageGeneration)
		{
			ActiveDefenseSequence.LastDamageAppliedStageGeneration = SuccessorGeneration;
		}
		if (Previous.LastOwnerMontageEndHandledStageGeneration == Previous.StageGeneration)
		{
			ActiveDefenseSequence.LastOwnerMontageEndHandledStageGeneration =
				SuccessorGeneration;
		}
		ActiveDefenseSequence.AttackerMontageInstanceId = INDEX_NONE;
		ActiveDefenseSequence.VictimMontageInstanceId = INDEX_NONE;
		ActivePairedAnimData = PreviousOwnerData;
		ActivePairedReactionType = PreviousReaction;
		CurrentFinisherVictim = PreviousVictim;
		ChainState = PreviousChainState;
		RekeyPairedStateLeasesGeneration(Previous.StageGeneration, SuccessorGeneration);
		SourcePaired->RekeyPairedStateLeasesGeneration(
			Previous.StageGeneration,
			SuccessorGeneration);
		if (bDefenderStarted && !bUsedPlaybackOverride && DefenderAnim)
		{
			if (DefenderAnim->Montage_IsPlaying(PairedAnimData->AttackerMontage))
			{
				RetireOwnerMontageCallback(PairedAnimData->AttackerMontage);
			}
			DefenderAnim->Montage_Stop(
				FMath::Max(0.0f, PairedAnimData->AttackerBlendOut),
				PairedAnimData->AttackerMontage);
		}
		if (bSourceStarted && !bUsedPlaybackOverride && SourceAnim)
		{
			if (SourceAnim->Montage_IsPlaying(PairedAnimData->VictimMontage))
			{
				SourcePaired->RetireOwnerMontageCallback(PairedAnimData->VictimMontage);
			}
			SourceAnim->Montage_Stop(
				FMath::Max(0.0f, PairedAnimData->VictimBlendOut),
				PairedAnimData->VictimMontage);
		}
		ReleasePairedStateLease(NewDefenderCollision);
		SourcePaired->ReleasePairedStateLease(NewSourceCollision);
		if (bUpdatedDefenderAlignment)
		{
			DefenderTargeting->UpdateAlignmentRequest(NewDefenderAlignment, PreviousDefenderAlignmentSpec);
		}
		else if (NewDefenderAlignment != Previous.AttackerAlignmentLease)
		{
			DefenderTargeting->ReleaseAlignmentRequest(NewDefenderAlignment);
		}
		if (bUpdatedSourceAlignment)
		{
			SourceTargeting->UpdateAlignmentRequest(NewSourceAlignment, PreviousSourceAlignmentSpec);
		}
		else if (NewSourceAlignment != Previous.VictimAlignmentLease)
		{
			SourceTargeting->ReleaseAlignmentRequest(NewSourceAlignment);
		}
		if (bAcquiredNewTimeLease)
		{
			if (UCombatEffectsWorldSubsystem* Effects = GetWorld()
				? GetWorld()->GetSubsystem<UCombatEffectsWorldSubsystem>()
				: nullptr)
			{
				Effects->ReleaseLease(NewTimeLease);
			}
		}
		if (!PreviousStageData)
		{
			SourceHitReaction->ExitPairedAnimationState();
		}
		else
		{
			SourceHitReaction->EnterPairedAnimationState(
				PreviousStageData->VictimMontage,
				PreviousStageData->VictimDeathOutcome,
				PreviousStageData->RagdollBlendTime,
				ShouldTreatPairedAnimationAsLethal(PreviousReaction, PreviousStageData),
				Defender);
		}
		if (PreviousChainState == EChainCounterState::CounterWindow
			|| PreviousChainState == EChainCounterState::FinisherReady)
		{
			const double Now = FPlatformTime::Seconds();
			ScheduleChainResponseDeadline(
				PreviousChainState,
				static_cast<float>(FMath::Max(0.0, PreservedDeadline - Now)),
				SuccessorGeneration,
				PreservedDeadline);
		}
		return false;
	}

	CancelDefenseAsyncHandle(Previous.ResponseTimeoutHandle);
	ActiveDefenseSequence.ResponseTimeoutHandle = {};
	ActiveDefenseSequence.ResponseDeadlineUnscaled = 0.0;
	ActiveDefenseSequence.AttackerMontageInstanceId = DefenderMontageInstanceId;
	ActiveDefenseSequence.VictimMontageInstanceId = SourceMontageInstanceId;
	ActiveDefenseSequence.AttackerCollisionLease = NewDefenderCollision;
	ActiveDefenseSequence.VictimCollisionLease = NewSourceCollision;
	ActiveDefenseSequence.AttackerAlignmentLease = NewDefenderAlignment;
	ActiveDefenseSequence.VictimAlignmentLease = NewSourceAlignment;
	ActiveDefenseSequence.TimeDilationLease = NewTimeLease;
	ActiveDefenseSequence.ChainState = SuccessState;
	ChainState = SuccessState;
	AppendDefenseSequenceTelemetry(
		DefenderCombat,
		ActiveDefenseSequence,
		EDefenseTelemetryEvent::StageStart,
		SuccessState);
	AppendPairedStageActionReactionTelemetry(
		DefenderCombat,
		ActiveDefenseSequence,
		EActionReactionTelemetryEvent::PairedStageStartSucceeded,
		EActionReactionTelemetryReason::StageStarted,
		PairedAnimData,
		SourceAttacker,
		DefenderMontageInstanceId,
		nullptr,
		TEXT("both paired roles started with owned leases"));

	ReleasePairedStateLeasesForGeneration(Previous.StageGeneration);
	SourcePaired->ReleasePairedStateLeasesForGeneration(Previous.StageGeneration);
	if (!bUpdatedDefenderAlignment
		&& Previous.AttackerAlignmentLease.IsValid()
		&& Previous.AttackerAlignmentLease != NewDefenderAlignment)
	{
		DefenderTargeting->ReleaseAlignmentRequest(Previous.AttackerAlignmentLease);
	}
	if (!bUpdatedSourceAlignment
		&& Previous.VictimAlignmentLease.IsValid()
		&& Previous.VictimAlignmentLease != NewSourceAlignment)
	{
		SourceTargeting->ReleaseAlignmentRequest(Previous.VictimAlignmentLease);
	}
	if (bAcquiredNewTimeLease && Previous.TimeDilationLease.IsValid())
	{
		if (UCombatEffectsWorldSubsystem* Effects = GetWorld()
			? GetWorld()->GetSubsystem<UCombatEffectsWorldSubsystem>()
			: nullptr)
		{
			Effects->ReleaseLease(Previous.TimeDilationLease);
		}
	}

	// A successor only stops the outgoing montages in its own montage group. Stop any outgoing role montage
	// still playing in another group (a held ready loop would otherwise never end); its callback was
	// retired above, so the stop is consumed as stale.
	if (PreviousStageData && !bUsedPlaybackOverride)
	{
		if (DefenderAnim && DefenderAnim->Montage_IsPlaying(PreviousStageData->AttackerMontage))
		{
			DefenderAnim->Montage_Stop(
				FMath::Max(0.0f, PairedAnimData->AttackerBlendIn),
				PreviousStageData->AttackerMontage);
		}
		if (SourceAnim && SourceAnim->Montage_IsPlaying(PreviousStageData->VictimMontage))
		{
			SourceAnim->Montage_Stop(
				FMath::Max(0.0f, PairedAnimData->VictimBlendIn),
				PreviousStageData->VictimMontage);
		}
	}

	// A defender released into a response window commits to the started stage again: input is owned by
	// the sequence and the defender is once more a held participant.
	const bool bRecommittedDefender = ActiveDefenseSequence.bDefenderReleased;
	if (!ActiveDefenseSequence.InputOwnershipLease.IsValid())
	{
		ActiveDefenseSequence.InputOwnershipLease = AcquireInputOwnership(
			TEXT("DefenseSequence"),
			SuccessorGeneration);
	}
	ActiveDefenseSequence.bDefenderReleased = false;

	if (ReactionType == EPairedReactionType::Parry && !bUsedPlaybackOverride)
	{
		ArmBridgeReadyPoseHold(PairedAnimData, DefenderAnim, SourceAnim, SuccessorGeneration);
	}
	if (CachedCombatComponent && ReactionType != EPairedReactionType::Parry)
	{
		CachedCombatComponent->SetPhase(EAttackPhase::Active);
	}
	if (bRecommittedDefender)
	{
		OnDefenseSequenceParticipationChanged.Broadcast(true);
	}
	OnPairedAnimationStarted.Broadcast(ReactionType, true);
	return true;
}

void UPairedAnimationComponent::ArmBridgeReadyPoseHold(
	const UPairedAnimationData* BridgeData,
	UAnimInstance* DefenderAnim,
	UAnimInstance* SourceAnim,
	const int32 StageGeneration)
{
	if (!BridgeData)
	{
		return;
	}
	const FPairedChainTransitionPolicy& Policy = BridgeData->ChainTransitionPolicy;
	// The defender's bridge plays through its ready pose once and ends, so its marker is never cut off by
	// an early blend-out and the player is free when the bridge is over. The source attacker holds its
	// ready pose until the window resolves.
	LinkStageIntoReadySection(
		DefenderAnim,
		BridgeData->AttackerMontage,
		Policy.AttackerReadySection,
		false,
		false);
	const bool bSourceHolds = LinkStageIntoReadySection(
		SourceAnim,
		BridgeData->VictimMontage,
		Policy.VictimReadySection,
		true,
		false);

	// A defender driver keeps its natural end, which is the signal that its marker never opened
	// CounterWindow. Holding the source attacker's ready pose removes that signal for a source driver, so
	// watch its entry into the hold instead: reaching it while still ParryActive means the bridge played out
	// without opening the window. The engine dispatches notifies before montage section events, so a marker
	// and the hold entry inside one long frame still open the window first and the watch then ignores the
	// entry.
	if (Policy.DriverRole == EPairedAnimationRole::Attacker || !bSourceHolds)
	{
		return;
	}
	FOnMontageSectionChanged HoldEntered = FOnMontageSectionChanged::CreateUObject(
		this,
		&UPairedAnimationComponent::HandleBridgeReadyPoseEntered,
		StageGeneration,
		Policy.VictimReadySection);
	SourceAnim->Montage_SetSectionChangedDelegate(HoldEntered, BridgeData->VictimMontage);
}

void UPairedAnimationComponent::HandleBridgeReadyPoseEntered(
	UAnimMontage* Montage,
	const FName SectionName,
	const bool bLooped,
	const int32 ExpectedStageGeneration,
	const FName ReadySection)
{
	(void)Montage;
	(void)bLooped;
	if (SectionName != ReadySection
		|| ChainState != EChainCounterState::ParryActive
		|| ActiveDefenseSequence.ChainState != EChainCounterState::ParryActive
		|| ActiveDefenseSequence.StageGeneration != ExpectedStageGeneration
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		return;
	}
	CleanupDefenseSequence(ExpectedStageGeneration, 0.1f, TEXT("BridgeEndedBeforeCounter"));
}

bool UPairedAnimationComponent::TryStartPairedAnimationWithTarget(AActor* TargetActor, UPairedAnimationData* PairedAnimData, EPairedReactionType ReactionType)
{
	if (!TargetActor || !PairedAnimData)
	{
		return false;
	}
	if (!HasValidPairedRuntimeNumerics(*PairedAnimData))
	{
		UE_LOG(LogPairedAnim, Warning,
			TEXT("[PAIRED START] Rejecting paired data with invalid runtime numeric configuration: %s"),
			*GetNameSafe(PairedAnimData));
		return false;
	}

	const bool bHasDefenseSequence =
		ActiveDefenseSequence.OriginatingInteraction.IsValid()
		|| ChainState != EChainCounterState::None;
	if (bHasDefenseSequence)
	{
		if (ActiveDefenseSequence.OriginatingInteraction.IsValid()
			&& ChainState != EChainCounterState::None
			&& ActiveDefenseSequence.SourceAttacker.Get() == TargetActor)
		{
			const EChainCounterState SuccessState = ReactionType == EPairedReactionType::Parry
				? EChainCounterState::ParryActive
				: ReactionType == EPairedReactionType::Counter
					? EChainCounterState::CounterActive
					: EChainCounterState::FinisherActive;
			return TryStartDefenseChainStage(PairedAnimData, ReactionType, SuccessState);
		}

		UE_LOG(LogPairedAnim, Verbose,
			TEXT("[PAIRED START] Rejected competing start while a defense sequence owns the component"));
		return false;
	}
	if (IsPairedAnimationActive())
	{
		return false;
	}

	ABaseCombatCharacter* AttackerCharacter = GetOwnerCharacter();
	if (!AttackerCharacter
		|| (AttackerCharacter->HitReactionComponent
			&& AttackerCharacter->HitReactionComponent->IsInPairedAnimationState()))
	{
		return false;
	}

	if (!IsValidPairedTarget(TargetActor))
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Verbose, TEXT("[PAIRED START] Rejecting non-hostile target %s"),
				*GetNameSafe(TargetActor));
		}
		return false;
	}

	UTargetingComponent* TargetingComp = AttackerCharacter->GetTargetingComponent();
	if (!TargetingComp)
	{
		return false;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(GetOwner());
	ActorsToIgnore.Add(TargetActor);

	const float PathClearanceRadius = 30.0f;
	if (!UPairedAnimationUtilityLibrary::IsPathClear(
		GetWorld(),
		AttackerCharacter->GetActorLocation(),
		TargetActor->GetActorLocation(),
		PathClearanceRadius,
		ActorsToIgnore))
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED START] Path to target %s is blocked by obstacle"),
				*TargetActor->GetName());
		}
		return false;
	}

	UHitReactionComponent* TargetHitReaction = TargetActor->FindComponentByClass<UHitReactionComponent>();
	UPairedAnimationComponent* TargetPairedComp =
		TargetActor->FindComponentByClass<UPairedAnimationComponent>();
	if (!TargetHitReaction
		|| TargetHitReaction->IsInPairedAnimationState()
		|| (TargetPairedComp
			&& (TargetPairedComp->IsPairedAnimationActive()
				|| TargetPairedComp->GetChainState() != EChainCounterState::None)))
	{
		return false;
	}

	if (PairedAnimData->Entry.bEnabled && !PreflightPairedEntry(TargetActor, PairedAnimData))
	{
		LastEntryOutcome = EAlignmentMotionOutcome::Exhausted;
		return false;
	}
	bEntryPending = PairedAnimData->Entry.bEnabled;
	LastEntryOutcome = EAlignmentMotionOutcome::Running;

	const bool bTreatAsLethal = ShouldTreatPairedAnimationAsLethal(ReactionType, PairedAnimData);
	if (ReactionType == EPairedReactionType::Counter && PairedAnimData->bIsLethal && !bTreatAsLethal)
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-CHAIN] Counter paired data is authored lethal but runtime policy treats counter steps as nonlethal"));
	}

	TargetHitReaction->EnterPairedAnimationState(
		PairedAnimData->VictimMontage,
		PairedAnimData->VictimDeathOutcome,
		PairedAnimData->RagdollBlendTime,
		bTreatAsLethal,
		GetOwner());

	CurrentFinisherVictim = TargetActor;

	AddPairedPartner(TargetActor);
	if (TargetPairedComp)
	{
		TargetPairedComp->AddPairedPartner(GetOwner());
	}

	BeginPairedAnimation(PairedAnimData, ReactionType, true);

	if (PairedAnimData->Entry.bEnabled)
	{
		if (!ActivePairedAnimData || !HasAcceptedLegacyPairedParticipant(TargetActor)) { return false; }
		if (PreparePairedEntry(TargetActor, PairedAnimData)) { return true; }
		LastEntryOutcome = EAlignmentMotionOutcome::Invalid;
		CancelPairedAnimation(0); return false;
	}
	return StartLegacyPairedMontages(TargetActor, PairedAnimData, ReactionType);
}

bool UPairedAnimationComponent::StartLegacyPairedMontages(AActor* TargetActor, UPairedAnimationData* PairedAnimData, EPairedReactionType ReactionType)
{
	ABaseCombatCharacter* AttackerCharacter = GetOwnerCharacter();
	UTargetingComponent* TargetingComp = AttackerCharacter ? AttackerCharacter->GetTargetingComponent() : nullptr;
	UHitReactionComponent* TargetHitReaction = TargetActor ? TargetActor->FindComponentByClass<UHitReactionComponent>() : nullptr;
	UPairedAnimationComponent* TargetPairedComp = TargetActor ? TargetActor->FindComponentByClass<UPairedAnimationComponent>() : nullptr;
	if (!TargetingComp || !TargetHitReaction || !PairedAnimData) { return false; }
	bool bAttackerMontageSuccess = false;
	bool bVictimMontageSuccess = false;

	ACharacter* AttackerChar = Cast<ACharacter>(AttackerCharacter);
	if (AttackerChar && PairedAnimData->AttackerMontage)
	{
		UAnimInstance* AttackerAnimInstance = AttackerChar->GetMesh() ? AttackerChar->GetMesh()->GetAnimInstance() : nullptr;
		if (AttackerAnimInstance)
		{
			const float MontageLength = AttackerAnimInstance->Montage_Play(
				PairedAnimData->AttackerMontage,
				1.0f,
				EMontagePlayReturnType::MontageLength,
				0.0f,
				true
			);

			bAttackerMontageSuccess = (MontageLength > 0.0f);

			if (bAttackerMontageSuccess)
			{
				if (!PairedAnimData->AttackerMontageSection.IsNone())
				{
					AttackerAnimInstance->Montage_JumpToSection(
						PairedAnimData->AttackerMontageSection,
						PairedAnimData->AttackerMontage
					);
					AttackerAnimInstance->Montage_SetNextSection(
						PairedAnimData->AttackerMontageSection,
						NAME_None,
						PairedAnimData->AttackerMontage
					);
				}

				TargetingComp->SetupAttackerPairedWarp(TargetActor, PairedAnimData->AttackerWarpConfig);

				if (CachedCombatComponent && ReactionType != EPairedReactionType::Parry)
				{
					CachedCombatComponent->SetPhase(EAttackPhase::Active);
				}

				if (GetDebugDraw())
				{
					FString SectionInfo = PairedAnimData->AttackerMontageSection.IsNone()
						? TEXT("(full)")
						: *PairedAnimData->AttackerMontageSection.ToString();
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED START] Attacker montage playing: %s Section: %s"),
						*PairedAnimData->AttackerMontage->GetName(), *SectionInfo);
				}
			}
		}
	}

	ACharacter* VictimChar = Cast<ACharacter>(TargetActor);
	if (VictimChar && PairedAnimData->VictimMontage)
	{
		UAnimInstance* VictimAnimInstance = VictimChar->GetMesh() ? VictimChar->GetMesh()->GetAnimInstance() : nullptr;
		if (VictimAnimInstance)
		{
			const float StartPosition = FMath::Max(0.0f, -PairedAnimData->VictimStartOffset);

			const float MontageLength = VictimAnimInstance->Montage_Play(
				PairedAnimData->VictimMontage,
				1.0f,
				EMontagePlayReturnType::MontageLength,
				StartPosition,
				true
			);

			bVictimMontageSuccess = (MontageLength > 0.0f);

			if (bVictimMontageSuccess)
			{
				if (!PairedAnimData->VictimMontageSection.IsNone())
				{
					VictimAnimInstance->Montage_JumpToSection(
						PairedAnimData->VictimMontageSection,
						PairedAnimData->VictimMontage
					);
					VictimAnimInstance->Montage_SetNextSection(
						PairedAnimData->VictimMontageSection,
						NAME_None,
						PairedAnimData->VictimMontage
					);
				}

				if (UTargetingComponent* VictimTargeting = TargetActor->FindComponentByClass<UTargetingComponent>())
				{
					VictimTargeting->SetupVictimWarp(GetOwner(), PairedAnimData->VictimWarpConfig);
				}

				if (GetDebugDraw())
				{
					FString SectionInfo = PairedAnimData->VictimMontageSection.IsNone()
						? TEXT("(full)")
						: *PairedAnimData->VictimMontageSection.ToString();
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED START] Victim montage playing: %s Section: %s (StartPos: %.2f)"),
						*PairedAnimData->VictimMontage->GetName(), *SectionInfo, StartPosition);
				}
			}
		}
	}

	if (!bAttackerMontageSuccess || !bVictimMontageSuccess)
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED START] Execution failed for %s - rolling back (Attacker: %s, Victim: %s)"),
			*PairedAnimData->GetDisplayName(),
			bAttackerMontageSuccess ? TEXT("OK") : TEXT("FAILED"),
			bVictimMontageSuccess ? TEXT("OK") : TEXT("FAILED"));

		TargetHitReaction->ExitPairedAnimationState();
		CurrentFinisherVictim.Reset();

		ReleaseLegacyPairedParticipationForPartners();
		ClearPairedPartners();
		if (TargetPairedComp)
		{
			TargetPairedComp->ClearPairedPartners();
		}

		EndPairedAnimation();

		if (bAttackerMontageSuccess && AttackerChar && AttackerChar->GetMesh())
		{
			if (UAnimInstance* AnimInst = AttackerChar->GetMesh()->GetAnimInstance())
			{
				AnimInst->Montage_Stop(0.1f);
			}
		}
		if (bVictimMontageSuccess && VictimChar && VictimChar->GetMesh())
		{
			if (UAnimInstance* AnimInst = VictimChar->GetMesh()->GetAnimInstance())
			{
				AnimInst->Montage_Stop(0.1f);
			}
		}

		TargetingComp->ClearAttackerPairedWarp();
		if (UTargetingComponent* VictimTargeting = TargetActor->FindComponentByClass<UTargetingComponent>())
		{
			VictimTargeting->ClearVictimWarp();
		}

		return false;
	}

	return true;
}

bool UPairedAnimationComponent::ShouldTreatPairedAnimationAsLethal(
	EPairedReactionType ReactionType,
	const UPairedAnimationData* PairedAnimData) const
{
	if (!PairedAnimData)
	{
		return false;
	}

	if (ReactionType == EPairedReactionType::Parry)
	{
		return false;
	}

	if (ReactionType == EPairedReactionType::Counter && !bAllowLethalCounterPairedData)
	{
		return false;
	}

	return PairedAnimData->bIsLethal;
}

// ============================================================================
// COUNTER WINDOW STATE
// ============================================================================

void UPairedAnimationComponent::SetCounterWindowData(EAttackType InAttackType, ESwingDirection InSwingDirection,
                                             UPairedAnimationData* InCounterData, float InWindowDuration)
{
	bCounterWindowActive = true;

	CounterWindowData.Attacker = GetOwner();
	CounterWindowData.AttackType = InAttackType;
	CounterWindowData.SwingDirection = InSwingDirection;
	CounterWindowData.SpecificCounterData = InCounterData;
	CounterWindowData.TimeInWindow = 0.0f;
	CounterWindowData.WindowDuration = InWindowDuration;

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER] Counter window opened: Type=%s, Swing=%s, Duration=%.2f"),
			*UEnum::GetValueAsString(InAttackType),
			*UEnum::GetValueAsString(InSwingDirection),
			InWindowDuration);
	}
}

void UPairedAnimationComponent::ClearCounterWindowData()
{
	if (bCounterWindowActive && GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER] Counter window closed"));
	}

	bCounterWindowActive = false;
	CounterWindowData.Reset();
}

void UPairedAnimationComponent::SetParryWindowActive(bool bActive)
{
	if (bParryWindowActive == bActive)
	{
		return;
	}

	bParryWindowActive = bActive;

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PARRY] Parry window %s on %s"),
			bActive ? TEXT("OPENED") : TEXT("CLOSED"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("None"));
	}
}

// ============================================================================
// COUNTER SYSTEM API
// ============================================================================

FCounterContext UPairedAnimationComponent::GetEnemyParryContext(AActor* Enemy) const
{
	FCounterContext Context;

	if (!Enemy)
	{
		return Context;
	}

	const UPairedAnimationComponent* EnemyPaired = Enemy->FindComponentByClass<UPairedAnimationComponent>();
	const UCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UCombatComponent>();
	const bool bEnemyInParryWindow = EnemyPaired
		? EnemyPaired->IsInParryWindow()
		: (EnemyCombat && EnemyCombat->IsInParryWindow());
	if (!bEnemyInParryWindow)
	{
		return Context;
	}

	Context.Attacker = Enemy;

	if (const UAttackData* CurrentAttack = EnemyCombat ? EnemyCombat->GetCurrentAttack() : nullptr)
	{
		Context.AttackType = CurrentAttack->AttackType;
	}

	return Context;
}

// ============================================================================
// COUNTER SYSTEM IMPLEMENTATIONS
// ============================================================================

bool UPairedAnimationComponent::TryAdvanceChainCounter(UAttackData* SelectedAttackData)
{
	if (ChainState == EChainCounterState::FinisherReady)
	{
		return ExecuteChainFinisher();
	}
	if (ChainState != EChainCounterState::CounterWindow)
	{
		return false;
	}

	if (!SelectedAttackData)
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-CHAIN] Cannot advance: selected attack data is null"));
		return false;
	}

	return ExecuteChainCounterAttack(SelectedAttackData);
}

bool UPairedAnimationComponent::ExecuteChainCounterAttack(UAttackData* ChainAttackData)
{
	if (ChainState != EChainCounterState::CounterWindow)
	{
		return false;
	}

	if (!ChainAttackData)
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-CHAIN] Cannot execute counter attack: selected attack data is null"));
		return false;
	}

	UPairedAnimationData* CounterPairedData = ChainAttackData->CounterData;
	if (!CounterPairedData && bAllowNotifyCounterDataFallback)
	{
		CounterPairedData = ActiveChainContext.SpecificCounterData;
	}

	if (!CounterPairedData || !ActiveChainTarget.IsValid())
	{
		UE_LOG(LogPairedAnim, Warning,
			TEXT("[COUNTER-CHAIN] Selected attack lacks usable paired counter data"));
		return false;
	}

	UAttackData* PreviousAttack = ActiveChainAttackData.Get();
	UAttackData* PreviousSelected = ActiveDefenseSequence.SelectedCounterAttack.Get();
	UPairedAnimationData* PreviousCounter = ActiveDefenseSequence.CounterData.Get();
	UPairedAnimationData* PreviousFinisher = ActiveDefenseSequence.FinisherData.Get();
	ActiveChainAttackData = ChainAttackData;
	ActiveDefenseSequence.SelectedCounterAttack = ChainAttackData;
	ActiveDefenseSequence.CounterData = CounterPairedData;
	ActiveDefenseSequence.FinisherData = ChainAttackData->FinisherData;
	if (TryStartDefenseChainStage(
		CounterPairedData,
		EPairedReactionType::Counter,
		EChainCounterState::CounterActive))
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER-CHAIN] Counter stage started."));
		return true;
	}

	ActiveChainAttackData = PreviousAttack;
	ActiveDefenseSequence.SelectedCounterAttack = PreviousSelected;
	ActiveDefenseSequence.CounterData = PreviousCounter;
	ActiveDefenseSequence.FinisherData = PreviousFinisher;
	return false;
}

bool UPairedAnimationComponent::ExecuteChainFinisher()
{
	if (ChainState != EChainCounterState::FinisherReady)
	{
		return false;
	}

	UPairedAnimationData* FinisherData = ActiveDefenseSequence.FinisherData.Get();
	if (!FinisherData || !ActiveChainTarget.IsValid())
	{
		return false;
	}

	const bool bSuccess = TryStartDefenseChainStage(
		FinisherData,
		EPairedReactionType::Finisher,
		EChainCounterState::FinisherActive);
	if (bSuccess)
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER-CHAIN] Chain finisher executed successfully!"));
	}
	else
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-CHAIN] Chain finisher failed - no valid target or animation"));
	}

	return bSuccess;
}

void UPairedAnimationComponent::CancelChainCounter()
{
	if (ChainState == EChainCounterState::None)
	{
		return;
	}

	const EChainCounterState PrevState = ChainState;
	if (ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		CleanupDefenseSequence(
			ActiveDefenseSequence.StageGeneration,
			0.1f,
			TEXT("Cancelled"));
	}
	else
	{
		ClearChainContext();
	}

	UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER-CHAIN] Chain cancelled from state %s"),
		*UEnum::GetValueAsString(PrevState));
}

void UPairedAnimationComponent::ClearChainContext()
{
	if (ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		CleanupDefenseSequence(
			ActiveDefenseSequence.StageGeneration,
			0.0f,
			TEXT("ClearChainContext"));
		return;
	}
	ChainState = EChainCounterState::None;
	ActiveChainContext.Reset();
	ActiveChainTarget.Reset();
	ActiveChainAttackData = nullptr;
	ActiveDefenseSequence = {};
}

// ============================================================================
// PAIRED ANIMATION PARTNER TRACKING
// ============================================================================

void UPairedAnimationComponent::AddPairedPartner(AActor* Partner)
{
	if (!Partner)
	{
		return;
	}

	for (const TWeakObjectPtr<AActor>& Existing : PairedAnimationPartners)
	{
		if (Existing.Get() == Partner)
		{
			return;
		}
	}

	PairedAnimationPartners.Add(Partner);
	if (bOwnsLegacyPairedGeneration && ActiveLegacyPairedGeneration < 0)
	{
		if (UPairedAnimationComponent* PartnerPaired =
			Partner->FindComponentByClass<UPairedAnimationComponent>())
		{
			if (PartnerPaired->BeginLegacyPairedParticipation(
				ActiveLegacyPairedGeneration,
				this))
			{
				AcceptedLegacyPairedParticipants.AddUnique(PartnerPaired);
			}
		}
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED] Added partner: %s (Total: %d)"),
			*Partner->GetName(), PairedAnimationPartners.Num());
	}
}

void UPairedAnimationComponent::RemovePairedPartner(AActor* Partner)
{
	if (!Partner)
	{
		return;
	}

	for (int32 i = PairedAnimationPartners.Num() - 1; i >= 0; --i)
	{
		if (PairedAnimationPartners[i].Get() == Partner)
		{
			PairedAnimationPartners.RemoveAt(i);

			if (GetDebugDraw())
			{
				UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED] Removed partner: %s (Remaining: %d)"),
					*Partner->GetName(), PairedAnimationPartners.Num());
			}
			return;
		}
	}
}

void UPairedAnimationComponent::ClearPairedPartners()
{
	const int32 Count = PairedAnimationPartners.Num();
	PairedAnimationPartners.Empty();

	if (GetDebugDraw() && Count > 0)
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED] Cleared all partners (was %d)"), Count);
	}
}

bool UPairedAnimationComponent::IsPairedPartner(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& Partner : PairedAnimationPartners)
	{
		if (Partner.Get() == Actor)
		{
			return true;
		}
	}

	return false;
}

bool UPairedAnimationComponent::IsPairedSequenceOwnerFor(const AActor* Actor) const
{
	if (!Actor || CurrentFinisherVictim.Get() != Actor || !IsPairedAnimationActive())
	{
		return false;
	}

	const bool bOwnsLegacySequence = HasAcceptedLegacyPairedParticipant(Actor);
	const bool bOwnsDefenseSequence = ChainState != EChainCounterState::None
		&& ActiveDefenseSequence.OriginatingInteraction.IsValid()
		&& ActiveDefenseSequence.Defender.Get() == GetOwner()
		&& ActiveDefenseSequence.SourceAttacker.Get() == Actor;
	return bOwnsLegacySequence || bOwnsDefenseSequence;
}

bool UPairedAnimationComponent::IsDefenseSequenceParticipant() const
{
	const UPairedAnimationComponent* const SequenceOwner = FindDefenseSequenceOwner();
	if (!SequenceOwner)
	{
		return false;
	}

	const AActor* const OwnerActor = GetOwner();
	if (SequenceOwner->ActiveDefenseSequence.SourceAttacker.Get() == OwnerActor)
	{
		return true;
	}
	return SequenceOwner->ActiveDefenseSequence.Defender.Get() == OwnerActor
		&& !SequenceOwner->ActiveDefenseSequence.bDefenderReleased;
}

// ============================================================================
// PAIRED ANIMATION EFFECT HANDLING
// ============================================================================

void UPairedAnimationComponent::BeginPairedAnimation(UPairedAnimationData* PairedAnimData, EPairedReactionType ReactionType, bool bIsCriticalMoment)
{
	if (ChainState != EChainCounterState::None
		&& ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		UE_LOG(LogPairedAnim, Verbose,
			TEXT("[PAIRED EFFECTS] Legacy BeginPairedAnimation cannot replace an owned defense sequence"));
		return;
	}
	if (!PairedAnimData)
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED EFFECTS] BeginPairedAnimation called with null PairedAnimData"));
		return;
	}
	if (ActiveLegacyPairedGeneration != 0 && !bOwnsLegacyPairedGeneration)
	{
		UE_LOG(LogPairedAnim, Warning,
			TEXT("[PAIRED EFFECTS] Legacy participant cannot become a competing sequence owner"));
		return;
	}
	if (ActiveLegacyPairedGeneration == 0)
	{
		ActiveLegacyPairedGeneration = AllocateLegacyPairedGeneration();
		bOwnsLegacyPairedGeneration = true;
		LegacyPairedSequenceOwner = this;
		AcceptedLegacyPairedParticipants.Reset();
	}
	RegisterLegacyPairedParticipationForPartners();
	if (!CurrentFinisherVictim.IsValid())
	{
		AActor* SolePartner = nullptr;
		for (const TWeakObjectPtr<AActor>& PartnerRef : PairedAnimationPartners)
		{
			AActor* const Partner = PartnerRef.Get();
			if (!Partner)
			{
				continue;
			}
			if (SolePartner && SolePartner != Partner)
			{
				SolePartner = nullptr;
				break;
			}
			SolePartner = Partner;
		}
		if (HasAcceptedLegacyPairedParticipant(SolePartner))
		{
			CurrentFinisherVictim = SolePartner;
		}
	}
	if (UCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UCombatComponent>() : nullptr)
	{
		Combat->PrepareForPairedTakeover();
	}

	ActivePairedAnimData = PairedAnimData;
	ActivePairedReactionType = ReactionType;
	if (!LegacyPairedInputLease.IsValid())
	{
		LegacyPairedInputLease = AcquireInputOwnership(
			TEXT("LegacyPairedAnimation"),
			ActiveLegacyPairedGeneration);
	}
	if (UCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UCombatComponent>() : nullptr)
	{
		Combat->ClearGuardThreat(EThreatClearReason::PairedTakeover);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlowMotionRestoreHandle);
	}

	if (bIsCriticalMoment
		&& PairedAnimData->bApplySlowMotion
		&& FMath::IsFinite(PairedAnimData->SlowMotionScale)
		&& FMath::IsFinite(PairedAnimData->SlowMotionDuration)
		&& PairedAnimData->SlowMotionDuration > 0.0f)
	{
		UWorld* World = GetWorld();
		UCombatEffectsWorldSubsystem* Effects = World
			? World->GetSubsystem<UCombatEffectsWorldSubsystem>()
			: nullptr;
		const UDefenseConfiguration* Configuration = CachedCombatComponent
			? CachedCombatComponent->GetEffectiveDefenseConfiguration()
			: GetDefault<UDefenseConfiguration>();
		const double ConfiguredWatchdog = Configuration
			? static_cast<double>(Configuration->TimeDilationLeaseWatchdogSeconds)
			: 10.0;
		const double Watchdog = FMath::Max(
			static_cast<double>(PairedAnimData->SlowMotionDuration),
			FMath::IsFinite(ConfiguredWatchdog) && ConfiguredWatchdog > 0.0
				? ConfiguredWatchdog
				: 10.0);
		const FTimeDilationLeaseHandle Successor = Effects
			? Effects->AcquireWorldLease(
				TEXT("PairedAnimation.Legacy"),
				FMath::Clamp(PairedAnimData->SlowMotionScale, 0.0001f, 1.0f),
				Watchdog)
			: FTimeDilationLeaseHandle{};
		if (Successor.IsValid())
		{
			ReleaseLegacyPairedTimeDilation();
			LegacyPairedTimeDilationLease = Successor;
			World->GetTimerManager().SetTimer(
				SlowMotionRestoreHandle,
				this,
				&UPairedAnimationComponent::OnSlowMotionTimerExpired,
				PairedAnimData->SlowMotionDuration,
				false
			);

			if (GetDebugDraw())
			{
				UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Slow motion applied: Scale=%.2f, Duration=%.2fs"),
					PairedAnimData->SlowMotionScale, PairedAnimData->SlowMotionDuration);
			}
		}
		else
		{
			ReleaseLegacyPairedTimeDilation();
		}
	}
	else
	{
		ReleaseLegacyPairedTimeDilation();
	}

	OnPairedAnimationStarted.Broadcast(ReactionType, bIsCriticalMoment);

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Started paired animation: %s (Type: %d, Critical: %d, SlowMo: %d)"),
			*PairedAnimData->GetDisplayName(),
			static_cast<int32>(ReactionType),
			bIsCriticalMoment,
			PairedAnimData->bApplySlowMotion);
	}
}

void UPairedAnimationComponent::EndPairedAnimation()
{
	ReleasePairedEntry();
	if (ChainState != EChainCounterState::None
		&& ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		CleanupDefenseSequence(
			ActiveDefenseSequence.StageGeneration,
			0.0f,
			TEXT("LegacyPairedEnd"));
		return;
	}
	const EPairedReactionType ReactionType = ActivePairedReactionType;

	if (SlowMotionRestoreHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SlowMotionRestoreHandle);
		}
	}

	ReleaseLegacyPairedTimeDilation();
	const int32 EndingLegacyGeneration = ActiveLegacyPairedGeneration;
	ReleaseLegacyPairedParticipationForPartners();
	EndLegacyPairedParticipation(EndingLegacyGeneration, this);

	ActivePairedAnimData = nullptr;
	ActivePairedReactionType = EPairedReactionType::None;
	CurrentFinisherVictim.Reset();
	ReleaseInputOwnership(LegacyPairedInputLease);
	LegacyPairedInputLease = {};
	if (UCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UCombatComponent>() : nullptr)
	{
		Combat->RefreshGuardThreat(EThreatRefreshReason::ManualRevalidation);
	}

	if (!PairedStateLeases.IsEmpty())
	{
		RecomputePairedState();
	}
	else
	{
		bMovementCurrentlyDisabled = false;
	}

	OnPairedAnimationEnded.Broadcast(ReactionType);

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Ended paired animation (Type: %d)"),
			static_cast<int32>(ReactionType));
	}
}

AActor* UPairedAnimationComponent::GetOwnedSyncCorrectionTarget() const
{
	// Resolve from sequence identity, not the order of collision/partner tracking.
	// Retained defense alignment already has its own scoped executor and budgets.
	AActor* Target = CurrentFinisherVictim.Get();
	if (!ActivePairedAnimData || ActivePairedAnimData->Entry.bEnabled || ChainState != EChainCounterState::None
		|| ActiveDefenseSequence.OriginatingInteraction.IsValid()
		|| !HasAcceptedLegacyPairedParticipant(Target) || !IsPairedPartner(Target))
	{
		return nullptr;
	}
	return Target;
}

void UPairedAnimationComponent::HandlePairedSyncPoint(
	const FName SyncPointName,
	const bool bApplyDamage)
{
	if (IsPreparingPairedEntry() || (LegacyPairedSequenceOwner.IsValid() && LegacyPairedSequenceOwner->IsPreparingPairedEntry())) { return; }
	TriggerSyncPointEffects(SyncPointName);
	if (!bApplyDamage)
	{
		return;
	}

	if (ChainState != EChainCounterState::None
		&& ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		ApplyActivePairedDamageOnce();
		return;
	}

	ApplyLegacyPairedDamageOnce();
}

void UPairedAnimationComponent::TriggerSyncPointEffects(FName SyncPointName)
{
	// Play camera shake if configured
	if (ActivePairedAnimData && ActivePairedAnimData->ImpactCameraShake)
	{
		UCinematicEffectsUtilityLibrary::PlayCameraShakeOnActor(GetOwner(), ActivePairedAnimData->ImpactCameraShake);

		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Camera shake played: %s"),
				*ActivePairedAnimData->ImpactCameraShake->GetName());
		}
	}

	if (!ActivePairedAnimData)
	{
		OnPairedAnimationSyncPoint.Broadcast(ActivePairedReactionType, SyncPointName);
		return;
	}

	AActor* Owner = GetOwner();
	AActor* Partner = PairedAnimationPartners.Num() > 0
		? PairedAnimationPartners[0].Get()
		: nullptr;

	// Calculate contact point for VFX (midpoint between attacker and victim)
	FVector ContactPoint = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	FVector ImpactNormal = FVector::UpVector;
	if (Owner && Partner)
	{
		ContactPoint = (Owner->GetActorLocation() + Partner->GetActorLocation()) * 0.5f;
		ImpactNormal = (Partner->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
		if (ImpactNormal.IsNearlyZero())
		{
			ImpactNormal = FVector::UpVector;
		}
	}

	// ================================================================
	// PAIRED ANIMATION AUDIO
	// ================================================================
	if (ActivePairedAnimData->ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(), ActivePairedAnimData->ImpactSound,
			ContactPoint, FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f,
			nullptr, nullptr, Owner);

		UE_LOG(LogCombatFX, Verbose, TEXT("[PAIRED FX] Impact sound: %s at %s"),
			*ActivePairedAnimData->ImpactSound->GetName(), *ContactPoint.ToString());
	}

	if (ActivePairedAnimData->VictimReactionSound && Partner)
	{
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(), ActivePairedAnimData->VictimReactionSound,
			Partner->GetActorLocation(), FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f,
			nullptr, nullptr, Partner);

		UE_LOG(LogCombatFX, Verbose, TEXT("[PAIRED FX] Victim reaction sound: %s"),
			*ActivePairedAnimData->VictimReactionSound->GetName());
	}

	if (ActivePairedAnimData->AttackerVoiceLine && Owner)
	{
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(), ActivePairedAnimData->AttackerVoiceLine,
			Owner->GetActorLocation(), FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f,
			nullptr, nullptr, Owner);

		UE_LOG(LogCombatFX, Verbose, TEXT("[PAIRED FX] Attacker voice line: %s"),
			*ActivePairedAnimData->AttackerVoiceLine->GetName());
	}

	// ================================================================
	// PAIRED ANIMATION VFX
	// ================================================================
	if (ActivePairedAnimData->ImpactVFX)
	{
		FImpactVFXConfig VFXConfig;
		VFXConfig.ImpactVFX = ActivePairedAnimData->ImpactVFX;
		VFXConfig.ScaleMultiplier = 1.0f;
		VFXConfig.bAlignToSurface = true;
		VFXConfig.bUseWeaponFallback = false;

		UCinematicEffectsUtilityLibrary::SpawnImpactVFX(
			GetWorld(),
			VFXConfig,
			nullptr,
			ContactPoint,
			ImpactNormal,
			NAME_None);

		UE_LOG(LogCombatFX, Verbose, TEXT("[PAIRED FX] Impact VFX: %s at %s"),
			*ActivePairedAnimData->ImpactVFX->GetName(), *ContactPoint.ToString());
	}

	OnPairedAnimationSyncPoint.Broadcast(ActivePairedReactionType, SyncPointName);

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Sync point: %s (Type: %d, Audio: %s/%s/%s, VFX: %s)"),
			*SyncPointName.ToString(),
			static_cast<int32>(ActivePairedReactionType),
			ActivePairedAnimData->ImpactSound ? TEXT("Impact") : TEXT("-"),
			ActivePairedAnimData->VictimReactionSound ? TEXT("Victim") : TEXT("-"),
			ActivePairedAnimData->AttackerVoiceLine ? TEXT("Voice") : TEXT("-"),
			ActivePairedAnimData->ImpactVFX ? TEXT("Yes") : TEXT("No"));
	}
}

void UPairedAnimationComponent::OnSlowMotionTimerExpired()
{
	ReleaseLegacyPairedTimeDilation();

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Slow motion timer expired - time dilation restored"));
	}
}

void UPairedAnimationComponent::ReleaseLegacyPairedTimeDilation()
{
	if (!LegacyPairedTimeDilationLease.IsValid())
	{
		return;
	}
	if (UCombatEffectsWorldSubsystem* Effects = GetWorld()
		? GetWorld()->GetSubsystem<UCombatEffectsWorldSubsystem>()
		: nullptr)
	{
		Effects->ReleaseLease(LegacyPairedTimeDilationLease);
	}
	LegacyPairedTimeDilationLease = {};
}

// ============================================================================
// PAIRED ANIMATION INTERRUPT HANDLING
// ============================================================================

void UPairedAnimationComponent::OnPairedPartnerDeath(AActor* DeadPartner)
{
	if (!DeadPartner)
	{
		return;
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED INTERRUPT] Partner %s died during paired animation"),
			*DeadPartner->GetName());
	}

	if (!IsPairedPartner(DeadPartner))
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED INTERRUPT] %s was not a tracked partner, ignoring"),
				*DeadPartner->GetName());
		}
		return;
	}

	// Resolve before removing the link: the non-owning participant uses it to
	// find the component that owns the retained defense sequence.
	if (UPairedAnimationComponent* SequenceOwner = FindDefenseSequenceOwner();
		SequenceOwner
		&& SequenceOwner->ChainState != EChainCounterState::None)
	{
		if (SequenceOwner->IsExpectedDefenseFinisherSourceDeath(DeadPartner))
		{
			return;
		}
		SequenceOwner->CancelPairedAnimation();
		return;
	}
	if (IsExpectedLegacyPairedVictimDeath(DeadPartner))
	{
		return;
	}
	if (bCompletingPairedAnimation && CurrentFinisherVictim.Get() == DeadPartner)
	{
		return;
	}
	if (ActiveLegacyPairedGeneration < 0)
	{
		const int32 EndingGeneration = ActiveLegacyPairedGeneration;
		if (bOwnsLegacyPairedGeneration)
		{
			if (UPairedAnimationComponent* DeadPartnerPaired =
				DeadPartner->FindComponentByClass<UPairedAnimationComponent>())
			{
				DeadPartnerPaired->EndLegacyPairedParticipation(EndingGeneration, this);
				AcceptedLegacyPairedParticipants.RemoveAll(
					[DeadPartnerPaired](const TWeakObjectPtr<UPairedAnimationComponent>& ParticipantRef)
					{
						return ParticipantRef.Get() == DeadPartnerPaired;
					});
			}
		}
		else
		{
			EndLegacyPairedParticipation(
				EndingGeneration,
				LegacyPairedSequenceOwner.Get());
		}
	}

	RemovePairedPartner(DeadPartner);
	if (IsPairedAnimationActive())
	{
		CancelPairedAnimation();
	}
}

void UPairedAnimationComponent::CancelPairedAnimation(float BlendOutTime)
{
	if (ChainState != EChainCounterState::None
		&& ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		CleanupDefenseSequence(
			ActiveDefenseSequence.StageGeneration,
			BlendOutTime,
			TEXT("PairedCancelled"));
		return;
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED INTERRUPT] Cancelling paired animation (BlendOutTime: %.2fs)"),
			BlendOutTime);
	}

	// GAP 18.10 FIX: Clear victim warp tracking on all partners BEFORE clearing partners
	// GAP 18.7 FIX: Clear bIsFinisherTarget flag on all partners
	for (const TWeakObjectPtr<AActor>& PartnerRef : PairedAnimationPartners)
	{
		if (AActor* Partner = PartnerRef.Get())
		{
			if (UTargetingComponent* PartnerTargeting = Partner->FindComponentByClass<UTargetingComponent>())
			{
				PartnerTargeting->ClearVictimWarp();
				PartnerTargeting->ClearAttackerPairedWarp();

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED INTERRUPT] Cleared warp tracking on partner %s"),
						*Partner->GetName());
				}
			}

			if (UHitReactionComponent* PartnerHitReaction = Partner->FindComponentByClass<UHitReactionComponent>())
			{
				if (IsExpectedLegacyPairedVictimDeath(Partner))
				{
					PartnerHitReaction->CompletePairedAnimationState();
				}
				else
				{
					PartnerHitReaction->ExitPairedAnimationState();
				}

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED INTERRUPT] Exited paired animation state on %s"),
						*Partner->GetName());
				}
			}
		}
	}

	// Entry owns only its movement instance; a replacement must survive cancellation.
	// ReleasePairedEntry retires that instance during EndPairedAnimation below.
	if (AActor* Owner = IsPreparingPairedEntry() ? nullptr : GetOwner())
	{
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			if (Character->GetMesh())
			if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
			{
				if (ActivePairedAnimData
					&& AnimInstance->Montage_IsPlaying(ActivePairedAnimData->AttackerMontage))
				{
					RetireOwnerMontageCallback(ActivePairedAnimData->AttackerMontage);
				}
				AnimInstance->Montage_Stop(BlendOutTime);

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED INTERRUPT] Montage stopped on %s"),
						*Owner->GetName());
				}
			}
		}
	}

	CurrentFinisherVictim.Reset();
	bCompletingPairedAnimation = false;
	ReleaseLegacyPairedParticipationForPartners();
	ClearPairedPartners();
	EndPairedAnimation();

	// Reset to idle phase via CombatComponent
	if (CachedCombatComponent)
	{
		CachedCombatComponent->SetPhase(EAttackPhase::None);
		CachedCombatComponent->ClearQueue(false);
	}

	if (ChainState != EChainCounterState::None)
	{
		ClearChainContext();
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED INTERRUPT] Paired animation cancelled - state reset"));
	}
}

void UPairedAnimationComponent::CompletePairedAnimation()
{
	if (IsPreparingPairedEntry()) { return; }
	if (ChainState != EChainCounterState::None
		&& ActiveDefenseSequence.OriginatingInteraction.IsValid())
	{
		UAnimMontage* ActiveMontage = ActiveDefenseSequence.ActivePairedData
			? ActiveDefenseSequence.ActivePairedData->AttackerMontage.Get()
			: nullptr;
		if (ActiveMontage)
		{
			HandleOwnerPairedMontageEnded(ActiveMontage, false);
		}
		else
		{
			CleanupDefenseSequence(
				ActiveDefenseSequence.StageGeneration,
				0.0f,
				TEXT("CompletionWithoutActiveMontage"));
		}
		return;
	}

	// GUARD: PREVENT DOUBLE EXECUTION (Gap 20.4)
	if (bCompletingPairedAnimation)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED COMPLETE] Already completing - ignoring duplicate call"));
		}
		return;
	}
	bCompletingPairedAnimation = true;

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] Completing paired animation successfully"));
	}

	// Compatibility fallback: correctly authored montages commit at their primary
	// damage sync point. Older or malformed montages still resolve at completion.
	if (ActivePairedReactionType != EPairedReactionType::Parry
		&& LastLegacyDamageAppliedGeneration != ActiveLegacyPairedGeneration)
	{
		if (ApplyLegacyPairedDamageOnce())
		{
			UE_LOG(LogPairedAnim, Warning,
				TEXT("[PAIRED COMPLETE] Generation %d reached montage completion without a damage sync point; applied compatibility fallback"),
				ActiveLegacyPairedGeneration);
		}
		else
		{
			UE_LOG(LogPairedAnim, Warning,
				TEXT("[PAIRED COMPLETE] Generation %d ended without a valid paired damage commit"),
				ActiveLegacyPairedGeneration);
		}
	}

	// ========================================================================
	// CLEANUP STATE
	// ========================================================================

	// Clear victim's finisher target flag and warp tracking
	for (const TWeakObjectPtr<AActor>& PartnerRef : PairedAnimationPartners)
	{
		if (AActor* Partner = PartnerRef.Get())
		{
			if (UTargetingComponent* PartnerTargeting = Partner->FindComponentByClass<UTargetingComponent>())
			{
				PartnerTargeting->ClearVictimWarp();
				PartnerTargeting->ClearAttackerPairedWarp();

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] Cleared warp tracking on partner %s"),
						*Partner->GetName());
				}
			}

			if (UHitReactionComponent* PartnerHitReaction = Partner->FindComponentByClass<UHitReactionComponent>())
			{
				PartnerHitReaction->CompletePairedAnimationState();

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] Exited paired animation state on %s"),
						*Partner->GetName());
				}
			}
		}
	}

	// Clear our own warp tracking
	if (ABaseCombatCharacter* Character = GetOwnerCharacter())
	{
		if (UTargetingComponent* TargetingComp = Character->GetTargetingComponent())
		{
			TargetingComp->ClearAttackerPairedWarp();
		}
	}

	CurrentFinisherVictim.Reset();
	ReleaseLegacyPairedParticipationForPartners();
	ClearPairedPartners();
	EndPairedAnimation();

	// Reset to idle phase via CombatComponent
	if (CachedCombatComponent)
	{
		CachedCombatComponent->SetPhase(EAttackPhase::None);
		CachedCombatComponent->ClearQueue(false);
	}

	// Clear guard flag now that completion is finished
	bCompletingPairedAnimation = false;

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] Paired animation completed - state reset"));
	}
}
