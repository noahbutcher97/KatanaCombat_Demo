#include "Utilities/DisplacementMath.h"

namespace DisplacementMath
{
double Progress(const EDisplacementSpeedProfile Profile, const double U)
{
	const double Clamped = FMath::Clamp(U, 0.0, 1.0);
	return Profile == EDisplacementSpeedProfile::EaseOut
		? 1.0 - FMath::Square(1.0 - Clamped)
		: Clamped;
}

double DistanceBetween(const EDisplacementSpeedProfile Profile, const double Distance, const double Duration,
	const double T0, const double T1)
{
	if (Duration <= 0.0)
	{
		return T0 <= 0.0 && T1 > T0 ? Distance : 0.0;
	}
	return Distance * (Progress(Profile, T1 / Duration) - Progress(Profile, T0 / Duration));
}

double PushStep(const double Progress, const double KeptAnimationTravel)
{
	return FMath::Max(0.0, Progress - KeptAnimationTravel);
}

EDisplacementChannel SelectChannel(const bool bPlayingRootMotion, const bool bHasMotionWarping, const bool bRootMotionMontageAdvancing)
{
	return bPlayingRootMotion && bHasMotionWarping && bRootMotionMontageAdvancing
		? EDisplacementChannel::Animation
		: EDisplacementChannel::Movement;
}

bool IsValid(const FProceduralDisplacement& Displacement)
{
	return !Displacement.Direction.ContainsNaN()
		&& FMath::IsNearlyEqual(Displacement.Direction.Size(), 1.0, 1e-3)
		&& FMath::Abs(Displacement.Direction.Z) <= 1e-3
		&& FMath::IsFinite(Displacement.Distance) && Displacement.Distance > 0.0f
		&& FMath::IsFinite(Displacement.Duration) && Displacement.Duration > 0.0f
		&& Displacement.Clock == EDisplacementClock::ActorTime;
}
}
