#include "Core/PairedAnimationComponent.h"
#include "Core/TargetingComponent.h"
#include "Characters/BaseCombatCharacter.h"
#include "Data/PairedAnimationData.h"
#include "Utilities/AlignmentMotionLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

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
	const FTransform Anchor(Owner->GetActorRotation(), Owner->GetActorLocation());
	const FTransform Current(Victim->GetActorRotation(), Victim->GetActorLocation());
	return AlignmentMotion::CanReach(Current, Data->Entry.VictimRelativeTransform * Anchor, Data->Entry.Limits);
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
	Spec.BoundedGoal = FTransform(GetOwner()->GetActorRotation(), GetOwner()->GetActorLocation());
	EntryOwnerAlignment = EntryOwnerTargeting->AcquireAlignmentRequest(Spec);
	Spec.Target = GetOwner(); Spec.BoundedGoal = EntryConfig.VictimRelativeTransform;
	EntryVictimAlignment = EntryVictimTargeting->AcquireAlignmentRequest(Spec);
	if (!EntryOwnerState.IsValid() || !EntryVictimState.IsValid()
		|| !EntryOwnerAlignment.IsValid() || !EntryVictimAlignment.IsValid()
		|| EntryOwnerTargeting->GetActiveAlignmentRequest() != EntryOwnerAlignment
		|| EntryVictimTargeting->GetActiveAlignmentRequest() != EntryVictimAlignment)
	{
		return false;
	}
	LastEntryOutcome = EAlignmentMotionOutcome::Running;
	SetComponentTickEnabled(true);
	UE_LOG(LogTemp, Log, TEXT("[PAIRED ENTRY] Preparing %s and %s (Generation: %d, Deadline: %.3f s)"),
		*GetOwner()->GetName(), *Target->GetName(), EntryGeneration, EntryConfig.Limits.Duration);
	return true;
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
	if (!bOwned || SimulationSeconds < 0
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
		OwnerState.Outcome = AlignmentMotion::CalculateStep(OwnerTransform, OwnerSpec.BoundedGoal, EntryConfig.Limits, OwnerState, 0).Outcome;
		VictimState.Outcome = AlignmentMotion::CalculateStep(VictimTransform, VictimSpec.BoundedGoal * OwnerTransform, EntryConfig.Limits, VictimState, 0).Outcome;
		const auto Failed = [](EAlignmentMotionOutcome O) { return O != EAlignmentMotionOutcome::Running && O != EAlignmentMotionOutcome::Reached; };
		if (Failed(OwnerState.Outcome)) { LastEntryOutcome = OwnerState.Outcome; }
		else if (Failed(VictimState.Outcome)) { LastEntryOutcome = VictimState.Outcome; }
		else if (OwnerState.Outcome == EAlignmentMotionOutcome::Reached && VictimState.Outcome == EAlignmentMotionOutcome::Reached)
		{
			LastEntryOutcome = EAlignmentMotionOutcome::Reached;
			UE_LOG(LogTemp, Log, TEXT("[PAIRED ENTRY] Ready %s (Generation: %d, Time: %.3f s, Victim travel: %.3f cm, Turn: %.3f deg)"),
				*GetOwner()->GetName(), EntryGeneration, EntryElapsed, VictimState.Travel, VictimState.Turn);
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
