// Copyright Epic Games, Inc. All Rights Reserved.

#include "PairedAnimationAnalysisLibrary.h"

// ============================================================================
// SPATIAL RELATIONSHIP CONSTRAINTS
// ============================================================================

void UPairedAnimationAnalysisLibrary::GetRelationshipConstraints(
	ESpatialRelationship Relationship,
	float& OutMinYaw,
	float& OutMaxYaw,
	float& OutTolerance)
{
	switch (Relationship)
	{
	case ESpatialRelationship::Facing:
		// Victim faces attacker (180 degrees relative)
		OutMinYaw = 150.0f;
		OutMaxYaw = 210.0f;
		OutTolerance = 30.0f;
		break;

	case ESpatialRelationship::Behind:
		// Attacker behind victim (0 degrees - same direction)
		OutMinYaw = -30.0f;
		OutMaxYaw = 30.0f;
		OutTolerance = 30.0f;
		break;

	case ESpatialRelationship::LeftSide:
		// Attacker on victim's left (90 degrees)
		OutMinYaw = 60.0f;
		OutMaxYaw = 120.0f;
		OutTolerance = 30.0f;
		break;

	case ESpatialRelationship::RightSide:
		// Attacker on victim's right (-90 degrees)
		OutMinYaw = -120.0f;
		OutMaxYaw = -60.0f;
		OutTolerance = 30.0f;
		break;

	case ESpatialRelationship::Custom:
	case ESpatialRelationship::Inferred:
	default:
		// No constraints - full 360
		OutMinYaw = -180.0f;
		OutMaxYaw = 180.0f;
		OutTolerance = 180.0f;
		break;
	}
}

FRotator UPairedAnimationAnalysisLibrary::ConstrainRotationToRelationship(
	FRotator InputRotation,
	ESpatialRelationship Relationship)
{
	if (Relationship == ESpatialRelationship::Custom ||
		Relationship == ESpatialRelationship::Inferred)
	{
		return InputRotation;
	}

	float MinYaw, MaxYaw, Tolerance;
	GetRelationshipConstraints(Relationship, MinYaw, MaxYaw, Tolerance);

	float NormalizedYaw = NormalizeAngle180(InputRotation.Yaw);
	float ClampedYaw = FMath::Clamp(NormalizedYaw, MinYaw, MaxYaw);

	return FRotator(InputRotation.Pitch, ClampedYaw, InputRotation.Roll);
}

bool UPairedAnimationAnalysisLibrary::IsRotationValidForRelationship(
	FRotator Rotation,
	ESpatialRelationship Relationship)
{
	if (Relationship == ESpatialRelationship::Custom ||
		Relationship == ESpatialRelationship::Inferred)
	{
		return true;
	}

	float MinYaw, MaxYaw, Tolerance;
	GetRelationshipConstraints(Relationship, MinYaw, MaxYaw, Tolerance);

	float NormalizedYaw = NormalizeAngle180(Rotation.Yaw);
	return NormalizedYaw >= MinYaw && NormalizedYaw <= MaxYaw;
}

// ============================================================================
// SCORE CALCULATIONS
// ============================================================================

float UPairedAnimationAnalysisLibrary::CalculateContactScore(float Distance, float Threshold)
{
	if (Threshold <= 0.0f)
	{
		return 0.0f;
	}

	if (Distance <= Threshold)
	{
		// Perfect contact
		return 1.0f;
	}
	else if (Distance <= Threshold * 3.0f)
	{
		// Linear falloff zone
		return 1.0f - ((Distance - Threshold) / (Threshold * 2.0f));
	}

	return 0.0f;
}

float UPairedAnimationAnalysisLibrary::ComputeWeightedScore(
	const TArray<float>& Scores,
	const TArray<float>& Weights)
{
	if (Scores.Num() == 0 || Scores.Num() != Weights.Num())
	{
		return 0.0f;
	}

	float WeightedSum = 0.0f;
	float TotalWeight = 0.0f;

	for (int32 i = 0; i < Scores.Num(); ++i)
	{
		WeightedSum += Scores[i] * Weights[i];
		TotalWeight += Weights[i];
	}

	return TotalWeight > 0.0f ? WeightedSum / TotalWeight : 0.0f;
}

float UPairedAnimationAnalysisLibrary::CalculateConsistency(const TArray<float>& Scores)
{
	if (Scores.Num() < 2)
	{
		return 1.0f; // Single value is perfectly consistent
	}

	float Variance = CalculateVariance(Scores);
	float StdDev = FMath::Sqrt(Variance);

	// Convert standard deviation to consistency score
	// StdDev of 0 = consistency of 1
	// StdDev of 1 = consistency of 0
	return FMath::Clamp(1.0f - StdDev, 0.0f, 1.0f);
}

float UPairedAnimationAnalysisLibrary::CalculateActivityWeight(float Velocity, float MaxVelocity)
{
	if (MaxVelocity <= 0.0f)
	{
		return 0.1f;
	}

	float NormalizedVelocity = FMath::Clamp(Velocity / MaxVelocity, 0.0f, 1.0f);

	// Minimum weight of 0.1 to never completely ignore static poses
	return 0.1f + (0.9f * NormalizedVelocity);
}

// ============================================================================
// ANGLE CALCULATIONS
// ============================================================================

float UPairedAnimationAnalysisLibrary::CalculateRelativeAngle(
	FVector AttackerForward,
	FVector VictimForward)
{
	// Normalize inputs
	AttackerForward = AttackerForward.GetSafeNormal2D();
	VictimForward = VictimForward.GetSafeNormal2D();

	float DotProduct = FVector::DotProduct(AttackerForward, VictimForward);
	DotProduct = FMath::Clamp(DotProduct, -1.0f, 1.0f);

	return FMath::RadiansToDegrees(FMath::Acos(DotProduct));
}

ESpatialRelationship UPairedAnimationAnalysisLibrary::InferRelationshipFromAngle(
	float RelativeAngle,
	FVector AttackDirection,
	FVector VictimForward)
{
	// Normalize inputs
	AttackDirection = AttackDirection.GetSafeNormal2D();
	VictimForward = VictimForward.GetSafeNormal2D();

	// Calculate angle between attack direction and victim forward
	float DotProduct = FVector::DotProduct(AttackDirection, VictimForward);
	float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f)));

	if (AngleDegrees < 45.0f)
	{
		// Attacker approaching from behind (same direction as victim facing)
		return ESpatialRelationship::Behind;
	}
	else if (AngleDegrees > 135.0f)
	{
		// Attacker approaching from front (opposite to victim facing)
		return ESpatialRelationship::Facing;
	}
	else
	{
		// Side approach - determine left or right
		FVector CrossProduct = FVector::CrossProduct(VictimForward, AttackDirection);
		if (CrossProduct.Z > 0)
		{
			return ESpatialRelationship::LeftSide;
		}
		else
		{
			return ESpatialRelationship::RightSide;
		}
	}
}

float UPairedAnimationAnalysisLibrary::NormalizeAngle180(float Angle)
{
	while (Angle > 180.0f) Angle -= 360.0f;
	while (Angle < -180.0f) Angle += 360.0f;
	return Angle;
}

float UPairedAnimationAnalysisLibrary::NormalizeAngle360(float Angle)
{
	while (Angle >= 360.0f) Angle -= 360.0f;
	while (Angle < 0.0f) Angle += 360.0f;
	return Angle;
}

float UPairedAnimationAnalysisLibrary::CalculateSpatialInferenceConfidence(float AngleDegrees)
{
	// Higher confidence when angle is clearly in a relationship zone
	// Front (180°) and Behind (0°) zones have highest confidence
	// Side zones (90°, -90°) have moderate confidence
	// Transition zones have lower confidence

	float Confidence = 0.5f;

	if (AngleDegrees < 30.0f || AngleDegrees > 150.0f)
	{
		// Front or Behind - high confidence zone
		float IdealAngle = (AngleDegrees < 90.0f) ? 0.0f : 180.0f;
		Confidence = 1.0f - FMath::Abs(AngleDegrees - IdealAngle) / 45.0f;
		Confidence = FMath::Clamp(Confidence, 0.5f, 1.0f);
	}
	else
	{
		// Side zones - moderate confidence
		Confidence = 0.5f + 0.3f * FMath::Abs(FMath::Sin(FMath::DegreesToRadians(AngleDegrees)));
	}

	return FMath::Clamp(Confidence, 0.5f, 1.0f);
}

bool UPairedAnimationAnalysisLibrary::IsYawWithinConstraint(float TargetYaw, float Tolerance, float TestYaw)
{
	float Diff = FMath::Abs(FMath::FindDeltaAngleDegrees(TargetYaw, TestYaw));
	return Diff <= Tolerance;
}

// ============================================================================
// STATISTICAL HELPERS
// ============================================================================

float UPairedAnimationAnalysisLibrary::CalculateMean(const TArray<float>& Values)
{
	if (Values.Num() == 0)
	{
		return 0.0f;
	}

	float Sum = 0.0f;
	for (float Value : Values)
	{
		Sum += Value;
	}

	return Sum / static_cast<float>(Values.Num());
}

float UPairedAnimationAnalysisLibrary::CalculateVariance(const TArray<float>& Values)
{
	if (Values.Num() < 2)
	{
		return 0.0f;
	}

	float Mean = CalculateMean(Values);
	float SumSquaredDiff = 0.0f;

	for (float Value : Values)
	{
		float Diff = Value - Mean;
		SumSquaredDiff += Diff * Diff;
	}

	// Sample variance (n-1)
	return SumSquaredDiff / static_cast<float>(Values.Num() - 1);
}

float UPairedAnimationAnalysisLibrary::CalculateStandardDeviation(const TArray<float>& Values)
{
	return FMath::Sqrt(CalculateVariance(Values));
}

float UPairedAnimationAnalysisLibrary::FindMinimum(const TArray<float>& Values)
{
	if (Values.Num() == 0)
	{
		return 0.0f;
	}

	float Min = Values[0];
	for (int32 i = 1; i < Values.Num(); ++i)
	{
		if (Values[i] < Min)
		{
			Min = Values[i];
		}
	}
	return Min;
}

float UPairedAnimationAnalysisLibrary::FindMaximum(const TArray<float>& Values)
{
	if (Values.Num() == 0)
	{
		return 0.0f;
	}

	float Max = Values[0];
	for (int32 i = 1; i < Values.Num(); ++i)
	{
		if (Values[i] > Max)
		{
			Max = Values[i];
		}
	}
	return Max;
}

// ============================================================================
// DISTANCE CALCULATIONS
// ============================================================================

FVector UPairedAnimationAnalysisLibrary::CalculateVictimPosition(
	FVector AttackerLocation,
	FRotator AttackerRotation,
	float Distance)
{
	FVector Forward = AttackerRotation.Vector();
	return AttackerLocation + Forward * Distance;
}

FVector UPairedAnimationAnalysisLibrary::CalculateMidpoint(FVector LocationA, FVector LocationB)
{
	return (LocationA + LocationB) * 0.5f;
}

float UPairedAnimationAnalysisLibrary::CalculateDistance2D(FVector LocationA, FVector LocationB)
{
	return FVector::Dist2D(LocationA, LocationB);
}
