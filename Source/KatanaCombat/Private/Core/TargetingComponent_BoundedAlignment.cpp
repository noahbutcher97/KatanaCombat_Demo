#include "Core/TargetingComponent.h"
#include "Utilities/AlignmentMotionLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

bool UTargetingComponent::GetAlignmentMotionState(FAlignmentRequestHandle Handle, FAlignmentMotionState& OutState) const
{
	const FAlignmentRequestRecord* Record = AlignmentRequests.Find(Handle);
	if (!Record || (Record->Spec.Executor != EAlignmentExecutor::BoundedMovement
		&& Record->Spec.Executor != EAlignmentExecutor::ProceduralDisplacement))
	{
		OutState = {}; OutState.Outcome = EAlignmentMotionOutcome::Invalid; return false;
	}
	OutState = Record->MotionState; return true;
}

void UTargetingComponent::AdvanceBoundedAlignment(float DeltaTime)
{
	if (LastAlignmentExecutionFrame == GFrameCounter || !EnsureAlignmentDependencies()) { return; }
	const FAlignmentRequestHandle Handle = ActiveAlignmentRequest;
	FAlignmentRequestRecord* Record = AlignmentRequests.Find(Handle);
	if (!Record) { return; }
	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (!Movement || !Movement->UpdatedComponent || Record->Spec.Target.IsStale(true))
	{
		Record->MotionState.Outcome = EAlignmentMotionOutcome::Invalid; return;
	}
	FTransform Goal = Record->Spec.BoundedGoal;
	if (AActor* Target = Record->Spec.Target.Get())
	{
		Goal = Goal * FTransform(Target->GetActorRotation(), Target->GetActorLocation());
	}
	const FTransform Before(OwnerCharacter->GetActorRotation(), OwnerCharacter->GetActorLocation());
	const double SimulationSeconds = AlignmentMotion::SimulationDelta(DeltaTime, OwnerCharacter->CustomTimeDilation);
	const AlignmentMotion::FStep Step = AlignmentMotion::CalculateStep(Before, Goal, Record->Spec.MotionLimits, Record->MotionState, SimulationSeconds);
	Record->MotionState.Outcome = Step.Outcome;
	Record->MotionState.Elapsed += Step.Seconds;
	LastAlignmentExecutionFrame = GFrameCounter;
	LastAlignmentExecutor = EAlignmentExecutor::BoundedMovement;
	if (Step.Outcome != EAlignmentMotionOutcome::Running) { return; }
	FRotator Rotation = Before.Rotator(); Rotation.Yaw += Step.Yaw;
	FHitResult Hit;
	Movement->MoveUpdatedComponent(Step.Translation, Rotation.Quaternion(), true, &Hit);
	// Sweeps can invoke gameplay callbacks that release or replace this request.
	Record = AlignmentRequests.Find(Handle);
	if (!Record || !OwnerCharacter) { return; }
	const FTransform After(OwnerCharacter->GetActorRotation(), OwnerCharacter->GetActorLocation());
	const double Travel = FVector::Distance(Before.GetLocation(), After.GetLocation());
	const double Turn = FMath::Abs(FMath::FindDeltaAngleDegrees(Before.Rotator().Yaw, After.Rotator().Yaw));
	Record->MotionState.Travel += Travel;
	Record->MotionState.Turn += Turn;
	Record->MotionState.Outcome = Hit.bBlockingHit ? EAlignmentMotionOutcome::Blocked
		: AlignmentMotion::CalculateStep(After, Goal, Record->Spec.MotionLimits, Record->MotionState, 0).Outcome;
	if (Travel > Step.Translation.Size() + 0.1 || Turn > FMath::Abs(Step.Yaw) + 0.1)
	{
		Record->MotionState.Outcome = EAlignmentMotionOutcome::Invalid;
	}
}
