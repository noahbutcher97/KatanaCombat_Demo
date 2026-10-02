#include "Core/TargetingComponent.h"
#include "Animation/RootMotionModifier_ProceduralDisplacement.h"
#include "Animation/RootMotionSource_ProceduralDisplacement.h"
#include "Utilities/DisplacementMath.h"
#include "MotionWarpingComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
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
// Blocked: this much consecutive request time under the progress fraction. Time, not ticks, so a graze
// ends a push the same way at any frame rate; 0.05 s is the three 60 Hz ticks the rule was tuned with.
constexpr double DisplacementBlockedSecondsLimit = 0.05;
// Float sums of frame times can land a hair under the limit (three 1/60 s steps); do not let that add a tick.
constexpr double DisplacementBlockedSecondsTolerance = 1e-6;
constexpr double DisplacementBlockedProgressFraction = 0.1;
constexpr double DisplacementMinimumExpectedStep = 0.1;
// An override exactly as long as the push it has left is not longer than it, but float sums of frame times land a
// hair either side: at 60 Hz one overridden step is 0.0166666675 s against 0.0166666657 s left on the last step.
constexpr double DisplacementOverrideSecondsTolerance = 1e-6;
const FName DisplacementSourceName(TEXT("KatanaProceduralDisplacement"));
// The executor cancels a push for two causes of its own, so each terminal row names its cause.
const TCHAR* const StaleSuspensionReason = TEXT("StaleSuspension");
const TCHAR* const AnimationOverrideReason = TEXT("AnimationOverride");

/** Movement along the push direction from one location to another. */
double ProgressAlong(const FVector& From, const FVector& To, const FVector& Direction)
{
	return FVector::DotProduct(To - From, Direction);
}

/** The channel that can carry the push this frame: animation only while the root-motion montage advances. */
EDisplacementChannel SelectLiveDisplacementChannel(const ACharacter& Character, const bool bHasMotionWarping)
{
	const FAnimMontageInstance* Instance = Character.GetRootMotionAnimMontageInstance();
	return DisplacementMath::SelectChannel(Character.IsPlayingRootMotion(), bHasMotionWarping, Instance && Instance->IsPlaying());
}

const TCHAR* DisplacementOutcomeReason(const EAlignmentMotionOutcome Outcome)
{
	switch (Outcome)
	{
	case EAlignmentMotionOutcome::Reached: return TEXT("DurationReached");
	case EAlignmentMotionOutcome::Blocked: return TEXT("ProgressStalled");
	case EAlignmentMotionOutcome::Invalid: return TEXT("NoDeliverableChannel");
	// Cancelled has several causes (StaleSuspension, AnimationOverride, a release's reason); its callers name it.
	default: return TEXT("");
	}
}
}

void UTargetingComponent::AppendDisplacementTelemetry(const FAlignmentRequestRecord& Record, const FName Disposition, const FString& Detail) const
{
	const ABaseCombatCharacter* Character = Cast<ABaseCombatCharacter>(OwnerCharacter);
	if (UCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr)
	{
		// Within the existing schema: the owner's started row plus suspension, resume and terminal rows.
		FActionReactionTelemetryRecord Row;
		Row.Event = EActionReactionTelemetryEvent::AlignmentChanged;
		Row.Actor = OwnerCharacter.Get();
		Row.AlignmentOwner = Record.Spec.OwnerId;
		Row.AlignmentDisposition = Disposition;
		Row.MovementMagnitude = static_cast<float>(Record.MotionState.Travel);
		Row.Detail = Detail;
		Combat->AppendActionReactionTelemetry(MoveTemp(Row));
	}
	if (CombatDebug::IsKnockbackDebugEnabled())
	{
		// The push's own travel, and all movement along the push (which includes the reaction's kept root motion).
		UE_LOG(LogTemp, Log, TEXT("[DISPLACEMENT] %s %s: %s after %.3f s, push %.1f of %.1f cm (total along push %.1f cm) (%s)"),
			*GetNameSafe(GetOwner()), *Record.Spec.OwnerId.ToString(), *Disposition.ToString(), Record.DisplacementElapsed,
			Record.MotionState.PushTravel, Record.Spec.Displacement.Distance, Record.MotionState.Travel, *Detail);
	}
}

void UTargetingComponent::ReportDisplacementOutcome(const FAlignmentRequestRecord& Record, const EAlignmentMotionOutcome Outcome, const TCHAR* Reason) const
{
	const FName OutcomeName(*StaticEnum<EAlignmentMotionOutcome>()->GetNameStringByValue(static_cast<int64>(Outcome)));
	AppendDisplacementTelemetry(Record, OutcomeName, FString(Reason));
	const UWorld* World = GetWorld();
	if (CombatDebug::IsKnockbackDebugEnabled() && OwnerCharacter && World)
	{
		// Where the push ended: its outcome and its own travel against the requested distance, at the feet, on top.
		const FColor Color = Outcome == EAlignmentMotionOutcome::Reached ? FColor::Green
			: Outcome == EAlignmentMotionOutcome::Blocked ? FColor::Red
			: Outcome == EAlignmentMotionOutcome::Cancelled ? FColor::Yellow
			: FColor::Magenta;
		const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
		const FVector Foot = CombatDebug::GetKnockbackDebugFootLocation(
			OwnerCharacter->GetActorLocation(), Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f);
		const float Duration = CombatDebug::GetKnockbackDebugDrawDuration();
		DrawDebugPoint(World, Foot, 14.0f, Color, false, Duration, SDPG_Foreground);
		DrawDebugString(World, Foot + FVector(0.0, 0.0, 15.0),
			FString::Printf(TEXT("%s (%s): push %.1f of %.1f cm"), *OutcomeName.ToString(), Reason ? Reason : TEXT(""),
				Record.MotionState.PushTravel, Record.Spec.Displacement.Distance),
			nullptr, Color, Duration, true);
	}
}

void UTargetingComponent::CancelRunningDisplacement(FAlignmentRequestRecord& Record, const TCHAR* Reason)
{
	// Every record's outcome defaults to Running, so only a displacement that has not ended reports here.
	if (Record.Spec.Executor != EAlignmentExecutor::ProceduralDisplacement
		|| Record.MotionState.Outcome != EAlignmentMotionOutcome::Running)
	{
		return;
	}
	// The channel may have applied a step since the last advance (character movement ticked, this component has not):
	// take that step's clock and travel, as the advance would have, before the channel goes.
	SyncDisplacementElapsed(Record);
	double KeptAnimationTravel = 0.0;
	if (Record.DisplacementChannel == EDisplacementChannel::Animation)
	{
		if (URootMotionModifier_ProceduralDisplacement* Modifier = Record.DisplacementModifier.Get())
		{
			KeptAnimationTravel = Modifier->ConsumeAnimationTravel();
		}
	}
	if (OwnerCharacter)
	{
		AccrueDisplacementTravel(Record, OwnerCharacter->GetActorLocation(), KeptAnimationTravel);
	}
	RemoveDisplacementChannel(Record);
	// A release that lands after the push's last step reports the push it delivered, not a cancellation.
	const EAlignmentMotionOutcome Outcome = Record.DisplacementElapsed >= Record.Spec.Displacement.Duration
		? EAlignmentMotionOutcome::Reached
		: EAlignmentMotionOutcome::Cancelled;
	Record.MotionState.Outcome = Outcome;
	Record.MotionState.Elapsed = Record.DisplacementElapsed;
	ReportDisplacementOutcome(Record, Outcome,
		Outcome == EAlignmentMotionOutcome::Reached ? DisplacementOutcomeReason(Outcome) : Reason);
}

double UTargetingComponent::AccrueDisplacementTravel(FAlignmentRequestRecord& Record, const FVector& Location,
	const double KeptAnimationTravel)
{
	double Progress = 0.0;
	if (Record.bDisplacementHasLastLocation)
	{
		Progress = ProgressAlong(Record.DisplacementLastLocation, Location, Record.Spec.Displacement.Direction);
		Record.MotionState.Travel += FMath::Max(0.0, Progress);
		Record.MotionState.PushTravel += DisplacementMath::PushStep(Progress, KeptAnimationTravel);
	}
	Record.DisplacementLastLocation = Location;
	Record.bDisplacementHasLastLocation = true;
	return Progress;
}

void UTargetingComponent::AccumulateDisplacementSuspension(const float DeltaTime)
{
	// A running displacement that is not the active request is suspended. Measure it on the owner's dilated
	// time (component ticks are scaled by CustomTimeDilation), so hitstop does not count toward staleness.
	for (TPair<FAlignmentRequestHandle, FAlignmentRequestRecord>& Pair : AlignmentRequests)
	{
		if (Pair.Key != ActiveAlignmentRequest
			&& Pair.Value.Spec.Executor == EAlignmentExecutor::ProceduralDisplacement
			&& Pair.Value.MotionState.Outcome == EAlignmentMotionOutcome::Running)
		{
			Pair.Value.DisplacementSuspendedSeconds += DeltaTime;
		}
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

bool UTargetingComponent::CanDeliverDisplacement() const
{
	// Both channels advance only inside character movement's own update, which does not run
	// (or drops its root motion) while movement is disabled or the capsule simulates physics.
	const UCharacterMovementComponent* Movement = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	return Movement && Movement->MovementMode != MOVE_None
		&& Movement->UpdatedComponent && !Movement->UpdatedComponent->IsSimulatingPhysics();
}

bool UTargetingComponent::InstallDisplacementChannel(FAlignmentRequestRecord& Record)
{
	if (!CanDeliverDisplacement())
	{
		return false;
	}
	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	const FProceduralDisplacement& Displacement = Record.Spec.Displacement;
	const double Remaining = Displacement.Duration - Record.DisplacementElapsed;
	if (Remaining <= 0.0)
	{
		return false;
	}

	const EDisplacementChannel Channel = SelectLiveDisplacementChannel(*OwnerCharacter, MotionWarpingComponent != nullptr);
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

	auto Source = MakeShared<FRootMotionSource_ProceduralDisplacement>();
	Source->InstanceName = DisplacementSourceName;
	Source->AccumulateMode = ERootMotionAccumulateMode::Override;
	Source->Priority = 500;
	Source->Duration = -1.0f; // never times out; the executor removes it
	Source->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
	// On removal: horizontal speed to zero, a fall keeps its downward speed.
	Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
	Source->FinishVelocityParams.ClampVelocity = 0.0f;
	// The source evaluates the curve itself from each movement step's simulation time.
	Source->Direction = Displacement.Direction;
	Source->Distance = Displacement.Distance;
	Source->CurveDuration = Displacement.Duration;
	Source->SpeedProfile = Displacement.SpeedProfile;
	Source->StartElapsed = static_cast<float>(Record.DisplacementElapsed);
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
			// Its overridden time is the dilated time of the steps animation root motion overrode since then.
			if (Source->GetScriptStruct() == FRootMotionSource_ProceduralDisplacement::StaticStruct())
			{
				Record.DisplacementOverriddenSeconds =
					static_cast<const FRootMotionSource_ProceduralDisplacement&>(*Source).OverriddenTime;
			}
		}
	}
}

void UTargetingComponent::RemoveDisplacementChannel(FAlignmentRequestRecord& Record)
{
	SyncDisplacementElapsed(Record); // a suspension between movement and this tick keeps the applied step
	if (URootMotionModifier_ProceduralDisplacement* Modifier = Record.DisplacementModifier.Get())
	{
		// MarkedForRemoval lets UMotionWarpingComponent purge the modifier on its next root-motion update
		// (UpdateWithContext), so one removed while no root motion plays stays inert in its list until then.
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
	Record.DisplacementBlockedSeconds = 0.0;
	Record.DisplacementOverriddenSeconds = 0.0;
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

	// Active again after a suspension: resume from the request clock only if the suspension was shorter
	// than the push it had left. A longer one would deliver the rest long after the hit that caused it.
	bool bStaleSuspension = false;
	if (Record->bDisplacementSuspended || Record->DisplacementSuspendedSeconds > 0.0)
	{
		const double Remaining = Displacement.Duration - Record->DisplacementElapsed;
		bStaleSuspension = Remaining > 0.0 && Record->DisplacementSuspendedSeconds > Remaining;
		if (!bStaleSuspension && Record->bDisplacementSuspended)
		{
			AppendDisplacementTelemetry(*Record, TEXT("Resumed"),
				FString::Printf(TEXT("%.3f s suspended"), Record->DisplacementSuspendedSeconds));
		}
		Record->DisplacementSuspendedSeconds = 0.0;
		Record->bDisplacementSuspended = false;
	}

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
		Record->DisplacementOverriddenSeconds = 0.0;
	}

	// Re-select the channel every tick: animation root motion overrides root-motion sources, so a
	// root-motion montage that starts mid-push must take the push over.
	// A montage holding its last pose (auto blend-out disabled) or paused extracts no root motion, so the push
	// moves to the movement channel rather than freezing with the montage.
	const EDisplacementChannel LiveChannel = SelectLiveDisplacementChannel(*OwnerCharacter, MotionWarpingComponent != nullptr);
	if (Record->DisplacementChannel != EDisplacementChannel::None && Record->DisplacementChannel != LiveChannel)
	{
		// The removal clears the measured location, so the travel measurement below skips this frame (Travel keeps
		// that gap). The step the outgoing channel delivered is still the push's own, so count it toward PushTravel:
		// the animation channel's step less the animation it kept, or the movement source's step unless animation
		// root motion overrode it (then the step was the animation's).
		if (Record->bDisplacementHasLastLocation && Record->DisplacementOverriddenSeconds <= 0.0)
		{
			Record->MotionState.PushTravel += DisplacementMath::PushStep(ProgressAlong(
				Record->DisplacementLastLocation, OwnerCharacter->GetActorLocation(), Displacement.Direction), AnimationTravel);
		}
		RemoveDisplacementChannel(*Record);
	}
	// A channel installed before the owner stopped being movable would freeze mid-push (its clock
	// stops with character movement); drop it so the install below reports Invalid.
	if (Record->DisplacementChannel != EDisplacementChannel::None && !CanDeliverDisplacement())
	{
		RemoveDisplacementChannel(*Record);
	}

	// Progress along the push direction, measured from actual movement against the expected
	// push plus any animation root motion that was kept.
	const FVector Location = OwnerCharacter->GetActorLocation();
	const bool bMeasured = Record->bDisplacementHasLastLocation;
	const double Actual = AccrueDisplacementTravel(*Record, Location, AnimationTravel);
	if (bMeasured)
	{
		const double Expected = AnimationTravel + DisplacementMath::DistanceBetween(
			Displacement.SpeedProfile, Displacement.Distance, Displacement.Duration, PreviousElapsed, Record->DisplacementElapsed);
		// Blocked time is request time, so the rule ends a push after the same contact at any frame rate.
		if (Expected > DisplacementMinimumExpectedStep && Actual < DisplacementBlockedProgressFraction * Expected)
		{
			Record->DisplacementBlockedSeconds += Record->DisplacementElapsed - PreviousElapsed;
		}
		else if (Expected > DisplacementMinimumExpectedStep)
		{
			Record->DisplacementBlockedSeconds = 0.0;
		}
	}

	EAlignmentMotionOutcome Outcome = EAlignmentMotionOutcome::Running;
	const TCHAR* CancelReason = nullptr;
	if (bStaleSuspension)
	{
		Outcome = EAlignmentMotionOutcome::Cancelled;
		CancelReason = StaleSuspensionReason;
	}
	else if (Record->DisplacementElapsed >= Displacement.Duration)
	{
		Outcome = EAlignmentMotionOutcome::Reached;
	}
	else if (Record->DisplacementBlockedSeconds >= DisplacementBlockedSecondsLimit - DisplacementBlockedSecondsTolerance)
	{
		Outcome = EAlignmentMotionOutcome::Blocked;
	}
	else if (Record->DisplacementOverriddenSeconds
		> Displacement.Duration - Record->DisplacementElapsed + DisplacementOverrideSecondsTolerance)
	{
		// Animation root motion has held the movement channel longer than the push it had left (the suspension
		// rule): the rest would arrive after the push should have ended. With no motion warping component, or in
		// RootMotionFromEverything, it would otherwise hold the push for the whole animation.
		Outcome = EAlignmentMotionOutcome::Cancelled;
		CancelReason = AnimationOverrideReason;
	}
	else if (Record->DisplacementChannel == EDisplacementChannel::None && !InstallDisplacementChannel(*Record))
	{
		Outcome = EAlignmentMotionOutcome::Invalid;
	}

	// Drawn after the install, so the first point of a push shows the channel it actually took. A trail at the feet,
	// on top of the mesh, that outlives the push.
	if (CombatDebug::IsKnockbackDebugEnabled())
	{
		const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
		DrawDebugPoint(GetWorld(),
			CombatDebug::GetKnockbackDebugFootLocation(Location, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f), 8.0f,
			Record->DisplacementChannel == EDisplacementChannel::Animation ? FColor::Cyan : FColor::Orange,
			false, CombatDebug::GetKnockbackDebugDrawDuration(), SDPG_Foreground);
	}

	Record->MotionState.Outcome = Outcome;
	Record->MotionState.Elapsed = Record->DisplacementElapsed;

	if (Outcome != EAlignmentMotionOutcome::Running)
	{
		RemoveDisplacementChannel(*Record);
		ReportDisplacementOutcome(*Record, Outcome, CancelReason ? CancelReason : DisplacementOutcomeReason(Outcome));
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
