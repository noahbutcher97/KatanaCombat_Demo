// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/PairedAnimationComponent.h"
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
#include "Debug/DebugConfig.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "Utilities/CombatGameplayTags.h"
#include "Utilities/PairedAnimationUtilityLibrary.h"
#include "Subsystems/CombatEffectsWorldSubsystem.h"
#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "Animation/AnimNotify_ChainStageTransition.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraSystem.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"

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

bool MontageContainsExactlyOneReviewedParryMarker(
	const UAnimMontage* Montage,
	const FName MarkerName)
{
	if (!Montage || MarkerName.IsNone())
	{
		return false;
	}

	int32 MatchingMarkerCount = 0;
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const UAnimNotify_ChainStageTransition* ChainNotify =
			Cast<UAnimNotify_ChainStageTransition>(NotifyEvent.Notify);
		if (ChainNotify
			&& ChainNotify->Transition == EChainStageTransitionType::OpenCounterWindow
			&& ChainNotify->MarkerName == MarkerName)
		{
			++MatchingMarkerCount;
		}
	}
	return MatchingMarkerCount == 1;
}

bool HasValidPairedRuntimeNumerics(const UPairedAnimationData& Data)
{
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
		&& FMath::IsFinite(Data.VictimWarpConfig.MaxWarpDistance)
		&& Data.VictimWarpConfig.MaxWarpDistance >= 0.0f
		&& !Data.VictimWarpConfig.RelativeOffset.ContainsNaN()
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

float GetAbsoluteYawToTarget(const AActor* Actor, const AActor* Target)
{
	if (!Actor || !Target)
	{
		return TNumericLimits<float>::Max();
	}

	const FVector ToTarget = Target->GetActorLocation() - Actor->GetActorLocation();
	if (ToTarget.IsNearlyZero())
	{
		return 0.0f;
	}
	const float DesiredYaw = ToTarget.Rotation().Yaw;
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
}

// ============================================================================
// CONSTRUCTION
// ============================================================================

UPairedAnimationComponent::UPairedAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
	if (!ReportingActor
		|| !StageData
		|| !ActiveDefenseSequence.OriginatingInteraction.IsValid()
		|| ActiveDefenseSequence.StageGeneration <= 0
		|| !NotifySourceId.IsValid()
		|| MontageInstanceId < 0)
	{
		return;
	}
	if (!Defender
		|| !SourceAttacker
		|| Defender->IsDeadOrDying()
		|| SourceAttacker->IsDeadOrDying())
	{
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
		return;
	}

	const FPairedChainTransitionPolicy& Policy = StageData->ChainTransitionPolicy;
	if (ReportingRole != Policy.DriverRole
		|| MontageInstanceId != ExpectedMontageInstanceId
		|| !ExpectedMontage
		|| NotifySourceId.SourceAnimation != FSoftObjectPath(ExpectedMontage)
		|| !ExpectedMontage->Notifies.IsValidIndex(NotifySourceId.NotifyEventIndex))
	{
		UE_LOG(LogPairedAnim, Verbose,
			TEXT("[COUNTER-CHAIN] Ignored stale or partner stage marker (generation %d)"),
			ActiveDefenseSequence.StageGeneration);
		return;
	}

	const UAnimNotify_ChainStageTransition* AuthoredNotify = Cast<UAnimNotify_ChainStageTransition>(
		ExpectedMontage->Notifies[NotifySourceId.NotifyEventIndex].Notify);
	if (!AuthoredNotify
		|| AuthoredNotify->Transition != Transition
		|| Policy.RequiredMarker.IsNone()
		|| AuthoredNotify->MarkerName != Policy.RequiredMarker)
	{
		return;
	}

	const int32 ExpectedGeneration = ActiveDefenseSequence.StageGeneration;
	if (Transition == EChainStageTransitionType::OpenCounterWindow)
	{
		EnterDefenseCounterWindow(ExpectedGeneration);
	}
	else
	{
		HandleDefenseAutoContinueMarker(ExpectedGeneration);
	}
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
	if (StageGeneration <= 0)
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
		Capsule->SetCollisionEnabled(BaselineCollisionEnabled.GetValue());
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
			Movement->SetMovementMode(BaselineMovementMode.GetValue());
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
		: 0;
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
	CleanupDefenseSequence(ExpectedStageGeneration, 0.1f, TEXT("ResponseTimeout"));
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

	// Install the marker before calling external damage code so reentry is harmless.
	ActiveDefenseSequence.LastDamageAppliedStageGeneration = StageGeneration;
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
	HitInfo.HitDirection = (Victim->GetActorLocation() - DamageSource->GetActorLocation()).GetSafeNormal();
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
	IDamageableInterface::Execute_ApplyDamage(Victim, HitInfo);
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

void UPairedAnimationComponent::HandleDefenseSourceDying(AActor* Killer)
{
	(void)Killer;
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
	UPairedAnimationComponent* SourcePaired = SourceAttacker
		? SourceAttacker->PairedAnimationComponent.Get()
		: nullptr;
	UTargetingComponent* DefenderTargeting = Defender ? Defender->TargetingComponent.Get() : nullptr;
	UTargetingComponent* SourceTargeting = SourceAttacker ? SourceAttacker->TargetingComponent.Get() : nullptr;
	UCombatComponent* DefenderCombat = CachedCombatComponent
		? CachedCombatComponent.Get()
		: Defender ? Defender->CombatComponent.Get() : nullptr;
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
		SourceAttacker->HitReactionComponent->ExitPairedAnimationState();
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

	if (DefenderCombat)
	{
		DefenderCombat->SetPhase(EAttackPhase::None);
		DefenderCombat->ClearQueue(false);
		DefenderCombat->RefreshGuardThreat(EThreatRefreshReason::ManualRevalidation);
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
		|| !MontageContainsExactlyOneReviewedParryMarker(
			DriverMontage,
			Presentation.ReviewedDeflectionMarker))
	{
		OutFailureReason = TEXT("driver montage lacks one reviewed Chain marker or retainable ready pose");
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
		|| BridgeData->VictimWarpConfig.RelativeOffset.ContainsNaN())
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

	const float RequiredDefenderTurn = FMath::Max(
		0.0f,
		FMath::Abs(Resolution.Decision.MeasuredYawDegrees)
			- Resolution.Decision.RequiredFinalTolerance);
	const float ConfiguredSourceTurnBudget = SourceConfiguration
		? SourceConfiguration->MaximumAutomaticTurn
		: 0.0f;
	const float SourceYawToDefender = GetAbsoluteYawToTarget(SourceAttacker, Defender);
	if (!FMath::IsFinite(RequiredDefenderTurn)
		|| !FMath::IsFinite(Resolution.Decision.MeasuredYawDegrees)
		|| !FMath::IsFinite(Resolution.Decision.RequiredFinalTolerance)
		|| !FMath::IsFinite(Resolution.Decision.AvailableTurnDegrees)
		|| !FMath::IsFinite(ConfiguredSourceTurnBudget)
		|| ConfiguredSourceTurnBudget < 0.0f
		|| !FMath::IsFinite(SourceYawToDefender)
		|| RequiredDefenderTurn > Resolution.Decision.AvailableTurnDegrees + KINDA_SMALL_NUMBER
		|| (BridgeData->VictimWarpConfig.bWarpRotation
			&& SourceYawToDefender
				> ConfiguredSourceTurnBudget + Resolution.Decision.RequiredFinalTolerance))
	{
		OutFailureReason = TEXT("a role exceeds its perfect-parry rotation budget");
		return false;
	}
	const double RemainingAlignmentSeconds =
		Resolution.PredictedContact.ContactSimulationTime
		- (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
	if (!Resolution.PredictedContact.bIsValid
		|| !FMath::IsFinite(RemainingAlignmentSeconds)
		|| RemainingAlignmentSeconds <= 0.0)
	{
		OutFailureReason = TEXT("remaining predicted alignment time is unavailable");
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
	if (UPairedAnimationData* StageData = ActiveDefenseSequence.ActivePairedData.Get())
	{
		const FPairedChainTransitionPolicy& Policy = StageData->ChainTransitionPolicy;
		if (!Policy.AttackerReadySection.IsNone())
		{
			if (UAnimInstance* Anim = Defender->GetMesh()
				? Defender->GetMesh()->GetAnimInstance()
				: nullptr)
			{
				Anim->Montage_JumpToSection(Policy.AttackerReadySection, StageData->AttackerMontage);
				Anim->Montage_SetNextSection(
					Policy.AttackerReadySection,
					Policy.AttackerReadySection,
					StageData->AttackerMontage);
			}
		}
		if (!Policy.VictimReadySection.IsNone())
		{
			if (UAnimInstance* Anim = SourceAttacker->GetMesh()
				? SourceAttacker->GetMesh()->GetAnimInstance()
				: nullptr)
			{
				Anim->Montage_JumpToSection(Policy.VictimReadySection, StageData->VictimMontage);
				Anim->Montage_SetNextSection(
					Policy.VictimReadySection,
					Policy.VictimReadySection,
					StageData->VictimMontage);
			}
		}
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
	UCombatComponent* DefenderCombat = CachedCombatComponent.Get();
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
		ActiveDefenseSequence.AttackerMontageInstanceId = INDEX_NONE;
		ActiveDefenseSequence.VictimMontageInstanceId = INDEX_NONE;
		const UDefenseConfiguration* Configuration = CachedCombatComponent
			? CachedCombatComponent->GetEffectiveDefenseConfiguration()
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
		return true;
	}

	CleanupDefenseSequence(Generation, 0.0f, TEXT("BridgeEndedBeforeCounter"));
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
	else
	{
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

	CleanupDefenseSequence(
		ExpectedStageGeneration,
		0.0f,
		TEXT("SourceMontageEndedFirst"));
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
		const FDefenseStageAlignmentLimits& Limits)
	{
		const float RequiredYaw = GetAbsoluteYawToTarget(Role, Target);
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
	if (!IsRoleRotationValid(Defender, SourceAttacker, DefenderAlignmentLimits)
		|| !IsRoleRotationValid(SourceAttacker, Defender, SourceAlignmentLimits))
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
			|| !MontageContainsExactlyOneReviewedParryMarker(
				Policy.DriverRole == EPairedAnimationRole::Attacker
					? PairedAnimData->AttackerMontage
					: PairedAnimData->VictimMontage,
				Policy.RequiredMarker)))
	{
		OutFailureReason = TEXT("parry bridge lacks an unambiguous retained-pose marker policy");
		return false;
	}
	if (ReactionType == EPairedReactionType::Counter && Policy.bAutoContinue)
	{
		const UAnimMontage* DriverMontage = Policy.DriverRole == EPairedAnimationRole::Attacker
			? PairedAnimData->AttackerMontage.Get()
			: PairedAnimData->VictimMontage.Get();
		int32 MarkerCount = 0;
		for (const FAnimNotifyEvent& Event : DriverMontage->Notifies)
		{
			const UAnimNotify_ChainStageTransition* Notify =
				Cast<UAnimNotify_ChainStageTransition>(Event.Notify);
			if (Notify
				&& Notify->Transition == EChainStageTransitionType::AutoContinue
				&& Notify->MarkerName == Policy.RequiredMarker)
			{
				++MarkerCount;
			}
		}
		if (Policy.RequiredMarker.IsNone() || MarkerCount != 1)
		{
			OutFailureReason = TEXT("auto-continuing counter lacks one driver marker");
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
		return false;
	}

	const FDefenseSequenceContext Previous = ActiveDefenseSequence;
	UPairedAnimationData* PreviousData = ActivePairedAnimData.Get();
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
		Spec.DesiredRotation = Target
			? (Target->GetActorLocation() - Owner->GetActorLocation()).Rotation()
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
		const bool bHadOutgoingOwnerMontage = PreviousData
			&& DefenderAnim
			&& DefenderAnim->Montage_IsPlaying(PreviousData->AttackerMontage);
		const bool bHadOutgoingSourceMontage = PreviousData
			&& SourceAnim
			&& SourceAnim->Montage_IsPlaying(PreviousData->VictimMontage);
		if (bHadOutgoingOwnerMontage)
		{
			RetireOwnerMontageCallback(PreviousData->AttackerMontage);
		}
		if (bHadOutgoingSourceMontage)
		{
			SourcePaired->RetireOwnerMontageCallback(PreviousData->VictimMontage);
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
			&& DefenderAnim->Montage_IsPlaying(PreviousData->AttackerMontage))
		{
			CancelRetiredOwnerMontageCallback(PreviousData->AttackerMontage);
		}
		if (!bDefenderStarted && bHadOutgoingSourceMontage
			&& SourceAnim->Montage_IsPlaying(PreviousData->VictimMontage))
		{
			SourcePaired->CancelRetiredOwnerMontageCallback(PreviousData->VictimMontage);
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
		ActivePairedAnimData = PreviousData;
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
		if (!PreviousData)
		{
			SourceHitReaction->ExitPairedAnimationState();
		}
		else
		{
			SourceHitReaction->EnterPairedAnimationState(
				PreviousData->VictimMontage,
				PreviousData->VictimDeathOutcome,
				PreviousData->RagdollBlendTime,
				ShouldTreatPairedAnimationAsLethal(PreviousReaction, PreviousData),
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

	if (CachedCombatComponent && ReactionType != EPairedReactionType::Parry)
	{
		CachedCombatComponent->SetPhase(EAttackPhase::Active);
	}
	OnPairedAnimationStarted.Broadcast(ReactionType, true);
	return true;
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

	bool bAttackerMontageSuccess = false;
	bool bVictimMontageSuccess = false;

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

bool UPairedAnimationComponent::TryCounter()
{
	if (CounterMode == ECounterSystemMode::Chain)
	{
		UE_LOG(LogPairedAnim, Verbose,
			TEXT("[COUNTER] Direct Chain initiation is retired; Block must commit through the defense resolver"));
		return false;
	}
	if (!CanCounter())
	{
		return false;
	}

	AActor* Target = FindCounterableEnemy();
	if (!Target)
	{
		UE_LOG(LogPairedAnim, Verbose, TEXT("[COUNTER] TryCounter failed: No counterable enemy found"));
		return false;
	}

	FCounterContext Context = GetEnemyCounterContext(Target);
	if (!Context.Attacker)
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER] TryCounter failed: Invalid counter context"));
		return false;
	}

	switch (CounterMode)
	{
	case ECounterSystemMode::AC3:
		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER] Executing AC3 counter-kill against %s"), *Target->GetName());
		return TryCounter_AC3Mode(Context);

	case ECounterSystemMode::Chain:
		return false;

	default:
		return false;
	}
}

bool UPairedAnimationComponent::CanCounter() const
{
	// Must be in a state that allows countering
	ECombatState State = ICombatInterface::Execute_GetCombatState(GetOwner());
	if (State != ECombatState::Idle && State != ECombatState::Blocking)
	{
		return false;
	}

	// Must not be in a paired animation
	if (bCompletingPairedAnimation || bBlockCombatInput)
	{
		return false;
	}

	// Chain mode: Check chain state allows countering
	if (CounterMode == ECounterSystemMode::Chain)
	{
		return false;
	}

	// AC3 mode: Must have a counterable enemy nearby
	return FindCounterableEnemy() != nullptr;
}

AActor* UPairedAnimationComponent::FindCounterableEnemy() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	UTargetingComponent* Targeting = Owner->FindComponentByClass<UTargetingComponent>();
	float SearchRange = 400.0f;
	if (Targeting)
	{
		if (const UTargetingSettings* Settings = Targeting->GetEffectiveSettings())
		{
			SearchRange = Settings->SoftAimRange;
		}
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);

	Owner->GetWorld()->OverlapMultiByChannel(
		Overlaps,
		Owner->GetActorLocation(),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(SearchRange),
		QueryParams
	);

	AActor* BestTarget = nullptr;
	float BestDistance = FLT_MAX;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OtherActor = Overlap.GetActor();
		if (!OtherActor)
		{
			continue;
		}

		// Check if this is an enemy (different team)
		if (OtherActor->Implements<UTeamMemberInterface>() && Owner->Implements<UTeamMemberInterface>())
		{
			ETeamId OtherTeam = ITeamMemberInterface::Execute_GetTeamId(OtherActor);
			ETeamId MyTeam = ITeamMemberInterface::Execute_GetTeamId(Owner);
			if (OtherTeam == MyTeam)
			{
				continue;
			}
		}

		// Check if enemy has an active counter window. PairedAnimationComponent owns
		// the state after CP-2; CombatComponent fallback keeps older delegates safe.
		const UPairedAnimationComponent* EnemyPaired = OtherActor->FindComponentByClass<UPairedAnimationComponent>();
		const UCombatComponent* EnemyCombat = OtherActor->FindComponentByClass<UCombatComponent>();
		const bool bEnemyInCounterWindow = EnemyPaired
			? EnemyPaired->IsInCounterWindow()
			: (EnemyCombat && EnemyCombat->IsInCounterWindow());
		if (!bEnemyInCounterWindow)
		{
			continue;
		}

		float Distance = FVector::Dist(Owner->GetActorLocation(), OtherActor->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestTarget = OtherActor;
		}
	}

	return BestTarget;
}

FCounterContext UPairedAnimationComponent::GetEnemyCounterContext(AActor* Enemy) const
{
	FCounterContext Context;

	if (!Enemy)
	{
		return Context;
	}

	if (const UPairedAnimationComponent* EnemyPaired = Enemy->FindComponentByClass<UPairedAnimationComponent>())
	{
		if (!EnemyPaired->IsInCounterWindow())
		{
			return Context;
		}

		Context = EnemyPaired->GetCounterWindowData();
		return Context;
	}

	const UCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UCombatComponent>();
	if (!EnemyCombat || !EnemyCombat->IsInCounterWindow())
	{
		return Context;
	}

	Context = EnemyCombat->GetCounterWindowData();
	return Context;
}

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

AActor* UPairedAnimationComponent::FindParryableEnemy() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	UTargetingComponent* Targeting = Owner->FindComponentByClass<UTargetingComponent>();
	float SearchRange = 400.0f;
	if (Targeting)
	{
		if (const UTargetingSettings* Settings = Targeting->GetEffectiveSettings())
		{
			SearchRange = Settings->SoftAimRange;
		}
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);

	Owner->GetWorld()->OverlapMultiByChannel(
		Overlaps,
		Owner->GetActorLocation(),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(SearchRange),
		QueryParams
	);

	AActor* BestTarget = nullptr;
	float BestDistance = FLT_MAX;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OtherActor = Overlap.GetActor();
		if (!OtherActor)
		{
			continue;
		}

		if (OtherActor->Implements<UTeamMemberInterface>() && Owner->Implements<UTeamMemberInterface>())
		{
			ETeamId OtherTeam = ITeamMemberInterface::Execute_GetTeamId(OtherActor);
			ETeamId MyTeam = ITeamMemberInterface::Execute_GetTeamId(Owner);
			if (OtherTeam == MyTeam)
			{
				continue;
			}
		}

		// Check if enemy has an active PARRY window (not counter window).
		const UPairedAnimationComponent* EnemyPaired = OtherActor->FindComponentByClass<UPairedAnimationComponent>();
		const UCombatComponent* EnemyCombat = OtherActor->FindComponentByClass<UCombatComponent>();
		const bool bEnemyInParryWindow = EnemyPaired
			? EnemyPaired->IsInParryWindow()
			: (EnemyCombat && EnemyCombat->IsInParryWindow());
		if (!bEnemyInParryWindow)
		{
			continue;
		}

		float Distance = FVector::Dist(Owner->GetActorLocation(), OtherActor->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestTarget = OtherActor;
		}
	}

	return BestTarget;
}

// ============================================================================
// COUNTER SYSTEM IMPLEMENTATIONS
// ============================================================================

bool UPairedAnimationComponent::TryCounter_AC3Mode(const FCounterContext& Context)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Context.Attacker)
	{
		return false;
	}

	// If counter data is specified on the notify, use that animation
	if (Context.SpecificCounterData)
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER-AC3] Using specific counter animation: %s"),
			*Context.SpecificCounterData->GetName());

		if (TryStartPairedAnimationWithTarget(Context.Attacker.Get(), Context.SpecificCounterData, EPairedReactionType::Counter))
		{
			return true;
		}

		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-AC3] Specific counter animation failed to start; falling back to direct counter damage"));
	}

	// AC3 Mode fallback: instant counter-kill via slow-mo and direct lethal damage.
	UWorld* World = GetWorld();
	if (World)
	{
		UCinematicEffectsUtilityLibrary::ApplySlowMotion(World, 0.2f);
	}

	// No specific counter data — stagger enemy and apply lethal damage directly
	if (ABaseCombatCharacter* EnemyChar = Cast<ABaseCombatCharacter>(Context.Attacker.Get()))
	{
		if (UHitReactionComponent* EnemyHitReact = EnemyChar->FindComponentByClass<UHitReactionComponent>())
		{
			EnemyHitReact->ApplyStagger(2.0f);
		}

		// Apply lethal damage — direction is FROM attacker TO victim (hit travels toward enemy)
		FHitReactionInfo HitInfo;
		HitInfo.Attacker = Owner;
		HitInfo.HitDirection = (Context.Attacker->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
		HitInfo.Damage = IDamageableInterface::Execute_GetCurrentHealth(Context.Attacker.Get()) + 1.0f;
		HitInfo.bWasCounter = true;
		HitInfo.PhaseWhenHit = EAttackPhase::Active;
		HitInfo.ImpactPoint = Context.Attacker->GetActorLocation();

		IDamageableInterface::Execute_ApplyDamage(Context.Attacker.Get(), HitInfo);

		// Restore time dilation after direct counter-kill (no paired animation to manage it)
		if (World)
		{
			UCinematicEffectsUtilityLibrary::RestoreTimeDilation(World);
		}

		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER-AC3] Counter-kill applied to %s"), *Context.Attacker->GetName());
		return true;
	}

	// Failed to find valid target — restore time dilation
	if (World)
	{
		UCinematicEffectsUtilityLibrary::RestoreTimeDilation(World);
	}
	return false;
}

bool UPairedAnimationComponent::TryCounter_ChainMode(const FCounterContext& Context)
{
	(void)Context;
	UE_LOG(LogPairedAnim, Verbose,
		TEXT("[COUNTER-CHAIN] Direct Chain initiation is retired; use a committed defense resolution"));
	return false;
}

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

	ActivePairedAnimData = PairedAnimData;
	ActivePairedReactionType = ReactionType;
	if (!LegacyPairedInputLease.IsValid())
	{
		LegacyPairedInputLease = AcquireInputOwnership(TEXT("LegacyPairedAnimation"), 0);
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

	ActivePairedAnimData = nullptr;
	ActivePairedReactionType = EPairedReactionType::None;
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
	else if (ABaseCombatCharacter* Character = GetOwnerCharacter())
	{
		if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
		{
			if (MovementComp->MovementMode == MOVE_None)
			{
				MovementComp->SetMovementMode(MOVE_Walking);

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Restored movement mode (was MOVE_None)"));
				}
			}
		}
		bMovementCurrentlyDisabled = false;
	}

	OnPairedAnimationEnded.Broadcast(ReactionType);

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Ended paired animation (Type: %d)"),
			static_cast<int32>(ReactionType));
	}
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
		SequenceOwner->CancelPairedAnimation();
		return;
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
				PartnerHitReaction->ExitPairedAnimationState();

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED INTERRUPT] Exited paired animation state on %s"),
						*Partner->GetName());
				}
			}
		}
	}

	// Stop any playing montage on the owner
	if (AActor* Owner = GetOwner())
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
	const bool bTreatAsLethal = ShouldTreatPairedAnimationAsLethal(ActivePairedReactionType, ActivePairedAnimData);

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] Completing paired animation successfully"));
	}

	// ========================================================================
	// APPLY FINISHER DAMAGE TO VICTIM
	// ========================================================================
	AActor* Victim = CurrentFinisherVictim.Get();
	const bool bShouldApplyPairedDamage =
		ActivePairedReactionType != EPairedReactionType::Parry;
	if (Victim && ActivePairedAnimData && bShouldApplyPairedDamage)
	{
		const float FinalDamage = ActivePairedAnimData->BaseDamage * ActivePairedAnimData->DamageMultiplier;

		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] Applying damage to %s: %.1f (Base: %.1f x Mult: %.2f, Lethal: %s)"),
				*Victim->GetName(),
				FinalDamage,
				ActivePairedAnimData->BaseDamage,
				ActivePairedAnimData->DamageMultiplier,
				bTreatAsLethal ? TEXT("YES") : TEXT("NO"));
		}

		if (Victim->Implements<UDamageableInterface>())
		{
			FHitReactionInfo HitInfo;
			HitInfo.Attacker = GetOwner();
			HitInfo.HitDirection = (Victim->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
			HitInfo.AttackData = nullptr;
			HitInfo.ImpactPoint = Victim->GetActorLocation();
			HitInfo.bWasCounter = (ActivePairedReactionType == EPairedReactionType::Counter);
			HitInfo.StunDuration = 0.0f;

			if (bTreatAsLethal)
			{
				const float MaxHealth = IDamageableInterface::Execute_GetMaxHealth(Victim);
				const float CurrentHealth = IDamageableInterface::Execute_GetCurrentHealth(Victim);
				HitInfo.Damage = FMath::Max(FinalDamage, CurrentHealth + 1.0f);

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] LETHAL finisher: Applying %.1f damage (victim has %.1f/%.1f health)"),
						HitInfo.Damage, CurrentHealth, MaxHealth);
				}
			}
			else
			{
				HitInfo.Damage = FinalDamage;
			}

			const float ActualDamage = IDamageableInterface::Execute_ApplyDamage(Victim, HitInfo);

			if (GetDebugDraw())
			{
				UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] Damage applied: %.1f actual (%.1f requested)"),
					ActualDamage, HitInfo.Damage);
			}
		}
		else
		{
			UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED COMPLETE] Victim %s does not implement IDamageableInterface - no damage applied"),
				*Victim->GetName());
		}
	}
	else if (GetDebugDraw())
	{
		if (!Victim)
		{
			UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED COMPLETE] No victim tracked - cannot apply damage"));
		}
		if (!ActivePairedAnimData)
		{
			UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED COMPLETE] No ActivePairedAnimData - cannot apply damage"));
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
				PartnerHitReaction->ExitPairedAnimationState();

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
