#include "Utilities/CombatMath.h"

namespace CombatMath
{
double AngleBetweenDegrees(const FVector& A, const FVector& B)
{
	const FVector UnitA = A.GetSafeNormal();
	const FVector UnitB = B.GetSafeNormal();
	if (UnitA.IsNearlyZero() || UnitB.IsNearlyZero())
	{
		return 0.0;
	}

	// Rounding can push a unit dot product just outside [-1, 1]; Acos would return NaN.
	const double Dot = FMath::Clamp(FVector::DotProduct(UnitA, UnitB), -1.0, 1.0);
	return FMath::RadiansToDegrees(FMath::Acos(Dot));
}

bool IsWithinCone(const FVector& Forward, const FVector& Direction, double HalfAngleDegrees)
{
	return AngleBetweenDegrees(Forward, Direction) <= HalfAngleDegrees;
}

FVector DirectionToAttacker(const FVector& VictimLocation, const FVector& AttackerLocation)
{
	return (AttackerLocation - VictimLocation).GetSafeNormal();
}

FVector FlatDirection(const FVector& From, const FVector& To)
{
	return (To - From).GetSafeNormal2D();
}

double SignedYawDegrees(const FVector& Forward, const FVector& Direction)
{
	const FVector FlatForward = Forward.GetSafeNormal2D();
	const FVector FlatDirectionVector = Direction.GetSafeNormal2D();
	if (FlatForward.IsNearlyZero() || FlatDirectionVector.IsNearlyZero())
	{
		return 0.0;
	}

	// Atan2(cross.Z, dot) is stable at 0 and 180 degrees, unlike Acos(dot).
	const double Cross = FlatForward.X * FlatDirectionVector.Y - FlatForward.Y * FlatDirectionVector.X;
	const double Dot = FlatForward.X * FlatDirectionVector.X + FlatForward.Y * FlatDirectionVector.Y;
	const double Degrees = FMath::RadiansToDegrees(FMath::Atan2(Cross, Dot));
	return Degrees <= -180.0 ? 180.0 : Degrees;
}

double BearingDegrees(const FVector& Direction)
{
	return SignedYawDegrees(FVector::ForwardVector, Direction);
}

EAttackDirection ClassifyLocalDirection(const FVector& LocalDirection)
{
	const FVector Flat = LocalDirection.GetSafeNormal2D();
	if (Flat.IsNearlyZero())
	{
		return EAttackDirection::Forward;
	}

	// Exact diagonals (|X| == |Y|) resolve to the side sectors. This preserves the policy of
	// the targeting and hit-reaction classifiers this function replaced.
	if (FMath::Abs(Flat.X) > FMath::Abs(Flat.Y))
	{
		return Flat.X > 0.0 ? EAttackDirection::Forward : EAttackDirection::Backward;
	}
	return Flat.Y > 0.0 ? EAttackDirection::Right : EAttackDirection::Left;
}

EAttackDirection ClassifyRelativeToFacing(const FTransform& Facing, const FVector& WorldDirection)
{
	return ClassifyLocalDirection(Facing.InverseTransformVectorNoScale(WorldDirection));
}
}
