#include "Core/TargetingComponent.h"
#include "Animation/RootMotionModifier_ProceduralDisplacement.h"
#include "Utilities/DisplacementMath.h"
#include "MotionWarpingComponent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Characters/BaseCombatCharacter.h"
#include "Core/CombatComponent.h"
#include "Debug/ActionReactionTelemetry.h"
#include "Debug/DebugConfig.h"
#include "DrawDebugHelpers.h"

namespace
{
constexpr int32 DisplacementBlockedTickLimit = 3;
constexpr double DisplacementBlockedProgressFraction = 0.1;
constexpr double DisplacementMinimumExpectedStep = 0.1;
const FName DisplacementSourceName(TEXT("KatanaProceduralDisplacement"));
}

void UTargetingComponent::ReportDisplacementOutcome(const FAlignmentRequestRecord& Record, const EAlignmentMotionOutcome Outcome) const
{
	const FName OutcomeName(*StaticEnum<EAlignmentMotionOutcome>()->GetNameStringByValue(static_cast<int64>(Outcome)));
	const ABaseCombatCharacter* Character = Cast<ABaseCombatCharacter>(OwnerCharacter);
	if (UCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr)
	{
		// Within the existing schema: the owner's started row plus this terminal row.
		FActionReactionTelemetryRecord Row;
		Row.Event = EActionReactionTelemetryEvent::AlignmentChanged;
		Row.Actor = OwnerCharacter.Get();
		Row.AlignmentOwner = Record.Spec.OwnerId;
		Row.AlignmentDisposition = OutcomeName;
		Row.MovementMagnitude = static_cast<float>(Record.MotionState.Travel);
		Combat->AppendActionReactionTelemetry(MoveTemp(Row));
	}
	if (CombatDebug::IsKnockbackDebugEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("[DISPLACEMENT] %s %s: %s after %.3f s, %.1f of %.1f cm"),
			*GetNameSafe(GetOwner()), *Record.Spec.OwnerId.ToString(), *OutcomeName.ToString(),
			Record.DisplacementElapsed, Record.MotionState.Travel, Record.Spec.Displacement.Distance);
	}
}

bool UTargetingComponent::RequestCanRotate(const FAlignmentRequestSpec& Spec)
{
	return Spec.Executor != EAlignmentExecutor::ProceduralDisplacement;
}

bool UTargetingComponent::HasRotatingAlignmentRequest() const
{
	for (const TPair<FAlignmentRequestHandle, FAlignmentRequestRecord>& Pair : AlignmentRequests)
	{
		if (RequestCanRotate(Pair.Value.Spec))
		{
			return true;
		}
	}
	return false;
}

void UTargetingComponent::SteerDisplacementMovement(FAlignmentRequestRecord& Record, const float StepEstimate)
{
	UCharacterMovementComponent* Movement = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	const TSharedPtr<FRootMotionSource> Source = Movement ? Movement->GetRootMotionSourceByID(Record.DisplacementSourceId) : nullptr;
	if (!Source.IsValid())
	{
		return;
	}
	// Average velocity over the next step, assuming it lasts as long as this one: exact for both
	// profiles at a steady frame rate, and it lands on the curve's end instead of overshooting.
	const FProceduralDisplacement& Displacement = Record.Spec.Displacement;
	StaticCastSharedPtr<FRootMotionSource_ConstantForce>(Source)->Force = Displacement.Direction * DisplacementMath::StepSpeed(
		Displacement.SpeedProfile, Displacement.Distance, Displacement.Duration, Record.DisplacementElapsed, StepEstimate);
}

bool UTargetingComponent::InstallDisplacementChannel(FAlignmentRequestRecord& Record, const float StepEstimate)
{
	UCharacterMovementComponent* Movement = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	if (!Movement || Movement->MovementMode == MOVE_None)
	{
		return false;
	}
	const FProceduralDisplacement& Displacement = Record.Spec.Displacement;
	const double Remaining = Displacement.Duration - Record.DisplacementElapsed;
	if (Remaining <= 0.0)
	{
		return false;
	}

	const EDisplacementChannel Channel = DisplacementMath::SelectChannel(
		OwnerCharacter->IsPlayingRootMotion(), MotionWarpingComponent != nullptr);
	if (Channel == EDisplacementChannel::Animation)
	{
		const FAnimMontageInstance* Instance = OwnerCharacter->GetRootMotionAnimMontageInstance();
		if (Instance && Instance->Montage)
		{
			auto* Modifier = NewObject<URootMotionModifier_ProceduralDisplacement>(MotionWarpingComponent);
			Modifier->Animation = Instance->Montage.Get();
			Modifier->StartTime = Instance->GetPosition();
			const float Rate = FMath::Max(0.01f, FMath::Abs(Instance->Montage->RateScale * Instance->GetPlayRate()));
			// Backstop only: completion is tracked by the modifier's own clock.
			Modifier->EndTime = Modifier->StartTime + static_cast<float>(Remaining) * Rate * 1.5f + 0.1f;
			Modifier->Configure(Displacement, Record.DisplacementElapsed);
			MotionWarpingComponent->AddModifier(Modifier);
			Record.DisplacementModifier = Modifier;
			Record.DisplacementChannel = EDisplacementChannel::Animation;
			return true;
		}
	}

	auto Source = MakeShared<FRootMotionSource_ConstantForce>();
	Source->InstanceName = DisplacementSourceName;
	Source->AccumulateMode = ERootMotionAccumulateMode::Override;
	Source->Priority = 500;
	Source->Duration = -1.0f; // never times out; the executor removes it
	Source->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
	// On removal: horizontal speed to zero, a fall keeps its downward speed.
	Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
	Source->FinishVelocityParams.ClampVelocity = 0.0f;
	Source->Force = Displacement.Direction * DisplacementMath::StepSpeed(
		Displacement.SpeedProfile, Displacement.Distance, Displacement.Duration, Record.DisplacementElapsed, StepEstimate);
	Record.DisplacementSourceId = Movement->ApplyRootMotionSource(Source);
	if (Record.DisplacementSourceId == static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		return false;
	}
	Record.DisplacementChannelStartElapsed = Record.DisplacementElapsed;
	Record.DisplacementChannel = EDisplacementChannel::Movement;
	return true;
}

void UTargetingComponent::SyncDisplacementElapsed(FAlignmentRequestRecord& Record)
{
	// Each channel keeps its own exact clock; read it rather than re-integrating our tick delta.
	if (Record.DisplacementChannel == EDisplacementChannel::Animation)
	{
		if (const URootMotionModifier_ProceduralDisplacement* Modifier = Record.DisplacementModifier.Get())
		{
			Record.DisplacementElapsed = Modifier->GetElapsed();
		}
	}
	else if (Record.DisplacementChannel == EDisplacementChannel::Movement && OwnerCharacter)
	{
		UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
		const TSharedPtr<FRootMotionSource> Source = Movement ? Movement->GetRootMotionSourceByID(Record.DisplacementSourceId) : nullptr;
		if (Source.IsValid())
		{
			// The source's time advances by the dilated simulation time it was applied for.
			Record.DisplacementElapsed = FMath::Min(static_cast<double>(Record.Spec.Displacement.Duration),
				Record.DisplacementChannelStartElapsed + Source->GetTime());
		}
	}
}

void UTargetingComponent::RemoveDisplacementChannel(FAlignmentRequestRecord& Record)
{
	SyncDisplacementElapsed(Record); // a suspension between movement and this tick keeps the applied step
	if (URootMotionModifier_ProceduralDisplacement* Modifier = Record.DisplacementModifier.Get())
	{
		Modifier->SetState(ERootMotionModifierState::MarkedForRemoval);
	}
	if (Record.DisplacementSourceId != 0 && OwnerCharacter)
	{
		if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
		{
			Movement->RemoveRootMotionSourceByID(Record.DisplacementSourceId);
		}
	}
	Record.DisplacementModifier.Reset();
	Record.DisplacementSourceId = 0;
	Record.DisplacementChannel = EDisplacementChannel::None;
	Record.bDisplacementHasLastLocation = false;
}

void UTargetingComponent::AdvanceProceduralDisplacement(const float DeltaTime)
{
	if (LastAlignmentExecutionFrame == GFrameCounter || !EnsureAlignmentDependencies())
	{
		return;
	}
	const FAlignmentRequestHandle Handle = ActiveAlignmentRequest;
	FAlignmentRequestRecord* Record = AlignmentRequests.Find(Handle);
	if (!Record || Record->MotionState.Outcome != EAlignmentMotionOutcome::Running)
	{
		return; // a finished owner-released request waits for its owner; it never reports or moves again
	}
	LastAlignmentExecutionFrame = GFrameCounter;
	LastAlignmentExecutor = EAlignmentExecutor::ProceduralDisplacement;

	const FProceduralDisplacement& Displacement = Record->Spec.Displacement;
	const double PreviousElapsed = Record->DisplacementElapsed;
	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();

	// Advance the request clock from whichever channel delivered this frame.
	SyncDisplacementElapsed(*Record);
	double AnimationTravel = 0.0;
	if (Record->DisplacementChannel == EDisplacementChannel::Animation)
	{
		URootMotionModifier_ProceduralDisplacement* Modifier = Record->DisplacementModifier.Get();
		AnimationTravel = Modifier ? Modifier->ConsumeAnimationTravel() : 0.0;
		if (!Modifier || Modifier->GetState() == ERootMotionModifierState::MarkedForRemoval)
		{
			// The montage ended or was replaced; the remainder is reinstalled below on the live channel.
			Record->DisplacementModifier.Reset();
			Record->DisplacementChannel = EDisplacementChannel::None;
		}
	}
	else if (Record->DisplacementChannel == EDisplacementChannel::Movement
		&& !(Movement && Movement->GetRootMotionSourceByID(Record->DisplacementSourceId).IsValid()))
	{
		Record->DisplacementSourceId = 0;
		Record->DisplacementChannel = EDisplacementChannel::None;
	}

	// Re-select the channel every tick: animation root motion overrides root-motion sources, so a
	// root-motion montage that starts mid-push must take the push over.
	const EDisplacementChannel LiveChannel = DisplacementMath::SelectChannel(
		OwnerCharacter->IsPlayingRootMotion(), MotionWarpingComponent != nullptr);
	if (Record->DisplacementChannel != EDisplacementChannel::None && Record->DisplacementChannel != LiveChannel)
	{
		RemoveDisplacementChannel(*Record);
	}

	// Progress along the push direction, measured from actual movement against the expected
	// push plus any animation root motion that was kept.
	const FVector Location = OwnerCharacter->GetActorLocation();
	double Actual = 0.0;
	if (Record->bDisplacementHasLastLocation)
	{
		Actual = FVector::DotProduct(Location - Record->DisplacementLastLocation, Displacement.Direction);
		const double Expected = AnimationTravel + DisplacementMath::DistanceBetween(
			Displacement.SpeedProfile, Displacement.Distance, Displacement.Duration, PreviousElapsed, Record->DisplacementElapsed);
		if (Expected > DisplacementMinimumExpectedStep && Actual < DisplacementBlockedProgressFraction * Expected)
		{
			++Record->DisplacementBlockedTicks;
		}
		else if (Expected > DisplacementMinimumExpectedStep)
		{
			Record->DisplacementBlockedTicks = 0;
		}
	}
	Record->DisplacementLastLocation = Location;
	Record->bDisplacementHasLastLocation = true;
	if (CombatDebug::IsKnockbackDebugEnabled())
	{
		DrawDebugPoint(GetWorld(), Location, 8.0f,
			Record->DisplacementChannel == EDisplacementChannel::Animation ? FColor::Cyan : FColor::Orange,
			false, CombatDebug::GetDebugDrawDuration());
	}

	EAlignmentMotionOutcome Outcome = EAlignmentMotionOutcome::Running;
	if (Record->DisplacementElapsed >= Displacement.Duration)
	{
		Outcome = EAlignmentMotionOutcome::Reached;
	}
	else if (Record->DisplacementBlockedTicks >= DisplacementBlockedTickLimit)
	{
		Outcome = EAlignmentMotionOutcome::Blocked;
	}
	else if (Record->DisplacementChannel == EDisplacementChannel::None && !InstallDisplacementChannel(*Record, DeltaTime))
	{
		Outcome = EAlignmentMotionOutcome::Invalid;
	}
	else if (Record->DisplacementChannel == EDisplacementChannel::Movement)
	{
		// ActorTime: the component delta is already scaled by the owner's time dilation.
		SteerDisplacementMovement(*Record, DeltaTime);
	}

	Record->MotionState.Outcome = Outcome;
	Record->MotionState.Elapsed = Record->DisplacementElapsed;
	Record->MotionState.Travel += FMath::Max(0.0, Actual);

	if (Outcome != EAlignmentMotionOutcome::Running)
	{
		RemoveDisplacementChannel(*Record);
		ReportDisplacementOutcome(*Record, Outcome);
		if (Record->Spec.bReleaseWhenFinished)
		{
			ReleaseAlignmentRequest(Handle);
		}
		else
		{
			SetComponentTickEnabled(HasSmoothAlignmentRequest());
		}
	}
}
