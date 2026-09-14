#include "Core/PairedAnimationComponent.h"
#include "Core/TargetingComponent.h"
#include "Characters/BaseCombatCharacter.h"
#include "Data/PairedAnimationData.h"
#include "Utilities/AlignmentMotionLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
bool ValidPresentation(const FPairedEntryConfig& Entry, const ACharacter* Mover)
{
	if (Entry.MovingRole != EPairedEntryMovingRole::Victim && Entry.MovingRole != EPairedEntryMovingRole::Initiator) { return false; }
	if (!FMath::IsFinite(Entry.MovementPlayRate) || Entry.MovementPlayRate < .01f || Entry.MovementPlayRate > 10
		|| !FMath::IsFinite(Entry.MovementStartTime) || Entry.MovementStartTime < 0
		|| !FMath::IsFinite(Entry.MovementBlendIn) || Entry.MovementBlendIn < 0 || Entry.MovementBlendIn > 1
		|| !FMath::IsFinite(Entry.MovementBlendOut) || Entry.MovementBlendOut < 0 || Entry.MovementBlendOut > 1
		|| Entry.MovementSlot.IsNone()) { return false; }
	const UAnimSequence* Animation = Entry.MovementAnimation;
	if (!Animation) { return Entry.MovementStartTime == 0; }
	const auto* Mesh = Mover && Mover->GetMesh() ? Mover->GetMesh()->GetSkeletalMeshAsset() : nullptr;
	// This presentation path is deliberately in-place and single-cycle. No second
	// root-motion authority or silent looping is introduced beside the swept executor.
	return Mesh && Animation->GetSkeleton() && Animation->GetSkeleton() == Mesh->GetSkeleton()
		&& Animation->GetSkeleton()->ContainsSlotName(Entry.MovementSlot)
		&& Animation->CanBeUsedInComposition() && !Animation->HasRootMotion()
		&& FMath::IsFinite(Animation->RateScale) && Animation->RateScale > 0
		&& Entry.MovementStartTime < Animation->GetPlayLength()
		&& (Animation->GetPlayLength() - Entry.MovementStartTime) / (Animation->RateScale * Entry.MovementPlayRate)
			>= Entry.Limits.Duration * Mover->CustomTimeDilation + Entry.MovementBlendOut;
}

FTransform WorldPose(const AActor* Actor)
{
	return FTransform(Actor->GetActorRotation(), Actor->GetActorLocation());
}

FTransform ResolveGoal(const FAlignmentRequestSpec& Spec)
{
	return Spec.Target.IsValid() ? Spec.BoundedGoal * WorldPose(Spec.Target.Get()) : Spec.BoundedGoal;
}
}

bool UPairedAnimationComponent::PreflightPairedEntry(AActor* Target, const UPairedAnimationData* Data) const
{
	const ACharacter* Owner = Cast<ACharacter>(GetOwner());
	const ACharacter* Victim = Cast<ACharacter>(Target);
	if (!Owner || !Victim || !Data || !AlignmentMotion::IsValid(Data->Entry.Limits)
		|| AlignmentMotion::SimulationDelta(0, Owner->CustomTimeDilation) < 0
		|| AlignmentMotion::SimulationDelta(0, Victim->CustomTimeDilation) < 0
		|| !AlignmentMotion::IsValidGoal(Data->Entry.VictimRelativeTransform)
		|| !Victim->FindComponentByClass<UPairedAnimationComponent>()
		|| !Victim->FindComponentByClass<UTargetingComponent>()
		|| !Owner->GetMesh() || !Victim->GetMesh()
		|| !Owner->GetMesh()->GetAnimInstance() || !Victim->GetMesh()->GetAnimInstance()
		|| !Data->AttackerMontage || !Data->VictimMontage
		|| Data->AttackerMontage->GetPlayLength() <= 0 || Data->VictimMontage->GetPlayLength() <= 0)
	{
		return false;
	}
	const bool bMoveInitiator = Data->Entry.MovingRole == EPairedEntryMovingRole::Initiator;
	const ACharacter* Mover = bMoveInitiator ? Owner : Victim;
	const ACharacter* Anchor = bMoveInitiator ? Victim : Owner;
	const FTransform Relative = bMoveInitiator ? Data->Entry.VictimRelativeTransform.Inverse() : Data->Entry.VictimRelativeTransform;
	return ValidPresentation(Data->Entry, Mover)
		&& AlignmentMotion::CanReach(WorldPose(Mover), Relative * WorldPose(Anchor), Data->Entry.Limits);
}

bool UPairedAnimationComponent::PreparePairedEntry(AActor* Target, const UPairedAnimationData* Data)
{
	EntryGeneration = ActiveLegacyPairedGeneration;
	EntryElapsed = 0;
	EntryConfig = Data->Entry;
	EntryOwnerTargeting = GetOwner()->FindComponentByClass<UTargetingComponent>();
	EntryVictimTargeting = Target->FindComponentByClass<UTargetingComponent>();
	EntryVictimComponent = Target->FindComponentByClass<UPairedAnimationComponent>();
	if (!EntryOwnerTargeting.IsValid() || !EntryVictimTargeting.IsValid() || !EntryVictimComponent.IsValid()
		|| EntryGeneration >= 0 || !bOwnsLegacyPairedGeneration) { return false; }

	// Stop locomotion and ignore only accepted partners; environment collision stays active.
	EntryOwnerState = AcquirePairedStateLease(TEXT("PairedEntry"), EntryGeneration, true, true, false, true, false, 0);
	EntryVictimState = EntryVictimComponent->AcquirePairedStateLease(TEXT("PairedEntry"), EntryGeneration, true, true, false, true, false, 0);
	FAlignmentRequestSpec Spec;
	Spec.OwnerId = TEXT("PairedEntry"); Spec.OwnerGeneration = EntryGeneration;
	Spec.Priority = EDefenseAlignmentPriority::PairedOrParryBridge;
	Spec.Executor = EAlignmentExecutor::BoundedMovement;
	Spec.MotionLimits = EntryConfig.Limits;
	const bool bMoveInitiator = EntryConfig.MovingRole == EPairedEntryMovingRole::Initiator;
	Spec.Target = bMoveInitiator ? Target : nullptr;
	Spec.BoundedGoal = bMoveInitiator ? EntryConfig.VictimRelativeTransform.Inverse() : WorldPose(GetOwner());
	EntryOwnerAlignment = EntryOwnerTargeting->AcquireAlignmentRequest(Spec);
	Spec.Target = bMoveInitiator ? nullptr : GetOwner();
	Spec.BoundedGoal = bMoveInitiator ? WorldPose(Target) : EntryConfig.VictimRelativeTransform;
	EntryVictimAlignment = EntryVictimTargeting->AcquireAlignmentRequest(Spec);
	if (!EntryOwnerState.IsValid() || !EntryVictimState.IsValid()
		|| !EntryOwnerAlignment.IsValid() || !EntryVictimAlignment.IsValid()
		|| EntryOwnerTargeting->GetActiveAlignmentRequest() != EntryOwnerAlignment
		|| EntryVictimTargeting->GetActiveAlignmentRequest() != EntryVictimAlignment)
	{
		return false;
	}
	ACharacter* Mover = Cast<ACharacter>(bMoveInitiator ? GetOwner() : Target);
	const FTransform Goal = bMoveInitiator ? EntryConfig.VictimRelativeTransform.Inverse() * WorldPose(Target)
		: EntryConfig.VictimRelativeTransform * WorldPose(GetOwner());
	const bool bReady = AlignmentMotion::CalculateStep(WorldPose(Mover), Goal, EntryConfig.Limits, {}, 0).Outcome == EAlignmentMotionOutcome::Reached;
	if (!bReady && EntryConfig.MovementAnimation && !StartEntryMovementPresentation(Mover)) { return false; }
	LastEntryOutcome = EAlignmentMotionOutcome::Running;
	SetComponentTickEnabled(true);
	UE_LOG(LogTemp, Log, TEXT("[PAIRED ENTRY] Preparing %s and %s (Generation: %d, Deadline: %.3f s)"),
		*GetOwner()->GetName(), *Target->GetName(), EntryGeneration, EntryConfig.Limits.Duration);
	return true;
}

bool UPairedAnimationComponent::StartEntryMovementPresentation(ACharacter* Mover)
{
	UAnimInstance* Instance = Mover->GetMesh()->GetAnimInstance();
	EntryMovementMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(EntryConfig.MovementAnimation,
		EntryConfig.MovementSlot, EntryConfig.MovementBlendIn, EntryConfig.MovementBlendOut);
	if (!EntryMovementMontage) { return false; }
	const int32 Generation = EntryGeneration;
	// Dynamic montage tracks incorporate asset RateScale. The factory's start-time
	// argument is unused in UE 5.6; start the actual instance at this track time.
	const float TrackStart = EntryConfig.MovementStartTime / EntryConfig.MovementAnimation->RateScale;
	if (Instance->Montage_Play(EntryMovementMontage, EntryConfig.MovementPlayRate,
		EMontagePlayReturnType::MontageLength, TrackStart) <= 0) { return false; }
	FAnimMontageInstance* Playback = Instance->GetActiveInstanceForMontage(EntryMovementMontage);
	if (!Playback) { return false; }
	EntryMovementAnimInstance = Instance;
	EntryMovementInstanceId = Playback->GetInstanceID();
	EntryMovementGeneration = Generation;
	return IsPreparingPairedEntry() && Generation == EntryGeneration;
}

bool UPairedAnimationComponent::IsEntryMovementPresentationValid() const
{
	if (EntryMovementGeneration == 0) { return true; } // No clip, or already ready at acceptance.
	UAnimInstance* Instance = EntryMovementAnimInstance.Get();
	const ACharacter* Mover = Cast<ACharacter>(EntryConfig.MovingRole == EPairedEntryMovingRole::Initiator ? GetOwner() : CurrentFinisherVictim.Get());
	const FAnimMontageInstance* Playback = Instance ? Instance->GetMontageInstanceForID(EntryMovementInstanceId) : nullptr;
	return EntryMovementGeneration == EntryGeneration && Mover && Mover->GetMesh()->GetAnimInstance() == Instance
		&& Playback && Playback->Montage == EntryMovementMontage && Playback->IsActive() && Playback->IsPlaying();
}

void UPairedAnimationComponent::StopEntryMovementPresentation()
{
	UAnimInstance* Instance = EntryMovementAnimInstance.Get();
	FAnimMontageInstance* Playback = Instance ? Instance->GetMontageInstanceForID(EntryMovementInstanceId) : nullptr;
	const bool bOwned = EntryMovementGeneration != 0 && EntryMovementGeneration == EntryGeneration
		&& Playback && Playback->Montage == EntryMovementMontage;
	// Retire identity before Stop can invoke external blend-out callbacks.
	EntryMovementGeneration = 0; EntryMovementInstanceId = INDEX_NONE;
	EntryMovementAnimInstance.Reset(); EntryMovementMontage = nullptr;
	if (bOwned) { Playback->Stop(FAlphaBlend(EntryConfig.MovementBlendOut)); }
}

void UPairedAnimationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsPreparingPairedEntry()) { return; }
	const bool bOwned = EntryGeneration == ActiveLegacyPairedGeneration && EntryGeneration < 0
		&& bOwnsLegacyPairedGeneration && ActivePairedAnimData
		&& HasAcceptedLegacyPairedParticipant(CurrentFinisherVictim.Get())
		&& IsPairedPartner(CurrentFinisherVictim.Get())
		&& EntryOwnerTargeting.IsValid() && EntryVictimTargeting.IsValid()
		&& EntryOwnerTargeting->GetActiveAlignmentRequest() == EntryOwnerAlignment
		&& EntryVictimTargeting->GetActiveAlignmentRequest() == EntryVictimAlignment;
	FAlignmentMotionState OwnerState, VictimState;
	const double SimulationSeconds = AlignmentMotion::SimulationDelta(DeltaTime, GetOwner()->CustomTimeDilation);
	if (!bOwned || !IsEntryMovementPresentationValid() || SimulationSeconds < 0
		|| !EntryOwnerTargeting->GetAlignmentMotionState(EntryOwnerAlignment, OwnerState)
		|| !EntryVictimTargeting->GetAlignmentMotionState(EntryVictimAlignment, VictimState))
	{
		LastEntryOutcome = EAlignmentMotionOutcome::Invalid;
	}
	else
	{
		EntryElapsed += SimulationSeconds;
		// Target/physics callbacks can move actors after an executor reports readiness.
		// Recheck the live transforms before starting either montage.
		FAlignmentRequestSpec OwnerSpec, VictimSpec;
		EntryOwnerTargeting->GetAlignmentRequestSpec(EntryOwnerAlignment, OwnerSpec);
		EntryVictimTargeting->GetAlignmentRequestSpec(EntryVictimAlignment, VictimSpec);
		const FTransform OwnerTransform(GetOwner()->GetActorRotation(), GetOwner()->GetActorLocation());
		AActor* Victim = CurrentFinisherVictim.Get();
		const FTransform VictimTransform(Victim->GetActorRotation(), Victim->GetActorLocation());
		OwnerState.Outcome = AlignmentMotion::CalculateStep(OwnerTransform, ResolveGoal(OwnerSpec), EntryConfig.Limits, OwnerState, 0).Outcome;
		VictimState.Outcome = AlignmentMotion::CalculateStep(VictimTransform, ResolveGoal(VictimSpec), EntryConfig.Limits, VictimState, 0).Outcome;
		const auto Failed = [](EAlignmentMotionOutcome O) { return O != EAlignmentMotionOutcome::Running && O != EAlignmentMotionOutcome::Reached; };
		if (Failed(OwnerState.Outcome)) { LastEntryOutcome = OwnerState.Outcome; }
		else if (Failed(VictimState.Outcome)) { LastEntryOutcome = VictimState.Outcome; }
		else if (OwnerState.Outcome == EAlignmentMotionOutcome::Reached && VictimState.Outcome == EAlignmentMotionOutcome::Reached)
		{
			LastEntryOutcome = EAlignmentMotionOutcome::Reached;
			UE_LOG(LogTemp, Log, TEXT("[PAIRED ENTRY] Ready %s (Generation: %d, Time: %.3f s, Initiator travel/turn: %.3f cm/%.3f deg, Victim: %.3f cm/%.3f deg)"),
				*GetOwner()->GetName(), EntryGeneration, EntryElapsed, OwnerState.Travel, OwnerState.Turn, VictimState.Travel, VictimState.Turn);
			ReleasePairedEntry();
			if (!StartLegacyPairedMontages(CurrentFinisherVictim.Get(), ActivePairedAnimData, ActivePairedReactionType))
			{
				LastEntryOutcome = EAlignmentMotionOutcome::Invalid;
				CancelPairedAnimation(0);
			}
			return;
		}
		else if (EntryElapsed >= EntryConfig.Limits.Duration) { LastEntryOutcome = EAlignmentMotionOutcome::Exhausted; }
		else { return; }
	}
	UE_LOG(LogTemp, Log, TEXT("[PAIRED ENTRY] Failed %s (Generation: %d, Outcome: %s)"),
		*GetOwner()->GetName(), EntryGeneration, *UEnum::GetValueAsString(LastEntryOutcome));
	CancelPairedAnimation(0);
}

void UPairedAnimationComponent::ReleasePairedEntry()
{
	if (bEntryPending && LastEntryOutcome == EAlignmentMotionOutcome::Running) { LastEntryOutcome = EAlignmentMotionOutcome::Cancelled; }
	bEntryPending = false;
	StopEntryMovementPresentation();
	EntryGeneration = 0;
	SetComponentTickEnabled(false);
	if (EntryOwnerTargeting.IsValid()) { EntryOwnerTargeting->ReleaseAlignmentRequest(EntryOwnerAlignment); }
	if (EntryVictimTargeting.IsValid()) { EntryVictimTargeting->ReleaseAlignmentRequest(EntryVictimAlignment); }
	ReleasePairedStateLease(EntryOwnerState);
	if (EntryVictimComponent.IsValid()) { EntryVictimComponent->ReleasePairedStateLease(EntryVictimState); }
	EntryOwnerAlignment = {}; EntryVictimAlignment = {};
	EntryOwnerState = {}; EntryVictimState = {};
	EntryOwnerTargeting.Reset(); EntryVictimTargeting.Reset(); EntryVictimComponent.Reset();
}
