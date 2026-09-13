#include "Utilities/AlignmentMotionLibrary.h"

namespace AlignmentMotion
{
double SimulationDelta(double ComponentDelta, double ActorTimeScale)
{
	if (!FMath::IsFinite(ComponentDelta) || ComponentDelta < 0 || !FMath::IsFinite(ActorTimeScale) || ActorTimeScale <= 0) { return -1; }
	const double Delta = ComponentDelta / ActorTimeScale;
	return FMath::IsFinite(Delta) ? Delta : -1;
}

bool IsValid(const FAlignmentMotionLimits& L)
{
	return FMath::IsFinite(L.Duration) && L.Duration > 0
		&& FMath::IsFinite(L.TranslationSpeed) && L.TranslationSpeed >= 0
		&& FMath::IsFinite(L.TravelBudget) && L.TravelBudget >= 0
		&& FMath::IsFinite(L.TurnRate) && L.TurnRate >= 0
		&& FMath::IsFinite(L.TurnBudget) && L.TurnBudget >= 0
		&& FMath::IsFinite(L.PositionTolerance) && L.PositionTolerance >= 0
		&& FMath::IsFinite(L.YawTolerance) && L.YawTolerance >= 0 && L.YawTolerance <= 180;
}

bool IsValidGoal(const FTransform& T)
{
	if (T.ContainsNaN() || !T.GetRotation().IsNormalized() || !T.GetScale3D().Equals(FVector::OneVector)) { return false; }
	const FRotator R = T.Rotator();
	return FMath::Abs(R.Pitch) <= KINDA_SMALL_NUMBER && FMath::Abs(R.Roll) <= KINDA_SMALL_NUMBER;
}

bool CanReach(const FTransform& Current, const FTransform& Goal, const FAlignmentMotionLimits& L)
{
	if (!IsValid(L) || !IsValidGoal(Current) || !IsValidGoal(Goal)) { return false; }
	const double Distance = FMath::Max(0.0, FVector::Distance(Current.GetLocation(), Goal.GetLocation()) - L.PositionTolerance);
	const double Turn = FMath::Max(0.0, FMath::Abs(FMath::FindDeltaAngleDegrees(Current.Rotator().Yaw, Goal.Rotator().Yaw)) - L.YawTolerance);
	return Distance <= L.TravelBudget && Distance <= static_cast<double>(L.TranslationSpeed) * L.Duration
		&& Turn <= L.TurnBudget && Turn <= static_cast<double>(L.TurnRate) * L.Duration;
}

FStep CalculateStep(const FTransform& Current, const FTransform& Goal,
	const FAlignmentMotionLimits& L, const FAlignmentMotionState& S, double DeltaSeconds)
{
	FStep Step;
	if (!IsValid(L) || !IsValidGoal(Current) || !IsValidGoal(Goal) || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0
		|| !FMath::IsFinite(S.Elapsed) || S.Elapsed < 0 || !FMath::IsFinite(S.Travel) || S.Travel < 0
		|| !FMath::IsFinite(S.Turn) || S.Turn < 0)
	{
		Step.Outcome = EAlignmentMotionOutcome::Invalid; return Step;
	}
	if (S.Outcome != EAlignmentMotionOutcome::Running && S.Outcome != EAlignmentMotionOutcome::Reached)
	{
		Step.Outcome = S.Outcome; return Step;
	}
	const FVector Delta = Goal.GetLocation() - Current.GetLocation();
	const double Distance = Delta.Size();
	const double Yaw = FMath::FindDeltaAngleDegrees(Current.Rotator().Yaw, Goal.Rotator().Yaw);
	if (!FMath::IsFinite(Distance) || !FMath::IsFinite(Yaw)) { Step.Outcome = EAlignmentMotionOutcome::Invalid; return Step; }
	Step.Seconds = FMath::Min(DeltaSeconds, FMath::Max(0.0, L.Duration - S.Elapsed));
	if (Distance <= L.PositionTolerance && FMath::Abs(Yaw) <= L.YawTolerance)
	{
		Step.Outcome = EAlignmentMotionOutcome::Reached; return Step;
	}
	if (S.Elapsed >= L.Duration
		|| (Distance > L.PositionTolerance && (S.Travel >= L.TravelBudget || L.TranslationSpeed <= 0))
		|| (FMath::Abs(Yaw) > L.YawTolerance && (S.Turn >= L.TurnBudget || L.TurnRate <= 0)))
	{
		Step.Outcome = EAlignmentMotionOutcome::Exhausted; return Step;
	}
	if (Distance > L.PositionTolerance)
	{
		Step.Translation = Delta.GetSafeNormal() * FMath::Min3(Distance,
			static_cast<double>(L.TranslationSpeed) * Step.Seconds, FMath::Max(0.0, L.TravelBudget - S.Travel));
	}
	if (FMath::Abs(Yaw) > L.YawTolerance)
	{
		Step.Yaw = FMath::Sign(Yaw) * FMath::Min3(FMath::Abs(Yaw),
			static_cast<double>(L.TurnRate) * Step.Seconds, FMath::Max(0.0, L.TurnBudget - S.Turn));
	}
	return Step;
}
}
