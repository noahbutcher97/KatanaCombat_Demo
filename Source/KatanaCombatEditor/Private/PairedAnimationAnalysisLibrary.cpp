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
// Explicit contact intent shared by preview and automated evaluation.
FPairedContactEvaluation UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(
	const FPairedContactRule& Rule, TConstArrayView<FPairedContactPose> Poses)
{
	FPairedContactEvaluation Result;
	const double Numbers[] = {Rule.StartSeconds, Rule.EndSeconds, Rule.TargetRadiusCm,
		Rule.MinimumGapCm, Rule.MaximumGapCm, Rule.MaximumSampleGapSeconds,
		Rule.ExpectedAngleDegrees, Rule.AngleToleranceDegrees};
	for (double Value : Numbers)
	{
		if (!FMath::IsFinite(Value)) { Result.Reason = TEXT("Non-finite contact criterion"); return Result; }
	}
	if (Rule.Name.IsEmpty() || Rule.SourcePoint.IsEmpty() || Rule.TargetPoint.IsEmpty()
		|| Rule.StartSeconds > Rule.EndSeconds || Rule.TargetRadiusCm < 0
		|| Rule.MinimumGapCm > Rule.MaximumGapCm || Rule.MaximumSampleGapSeconds <= 0
		|| Rule.AngleToleranceDegrees < 0 || Rule.ExpectedAngleDegrees < 0 || Rule.ExpectedAngleDegrees > 180
		|| (Rule.bMeasureOrientation && (Rule.TargetLocalNormal.ContainsNaN() || Rule.TargetLocalNormal.IsNearlyZero())))
	{
		Result.Reason = TEXT("Invalid contact geometry, timing or tolerance"); return Result;
	}
	int32 Before = INDEX_NONE, After = INDEX_NONE;
	for (int32 I = 0; I < Poses.Num(); ++I)
	{
		if (!FMath::IsFinite(Poses[I].TimeSeconds) || !FMath::IsFinite(Poses[I].OriginalTimeSeconds)
			|| (I && Poses[I].TimeSeconds <= Poses[I - 1].TimeSeconds))
		{
			Result.Reason = TEXT("Pose times must be finite and strictly increasing"); return Result;
		}
		if (Poses[I].TimeSeconds <= Rule.StartSeconds) { Before = I; }
		if (After == INDEX_NONE && Poses[I].TimeSeconds >= Rule.EndSeconds) { After = I; }
	}
	if (Before == INDEX_NONE || After == INDEX_NONE)
	{
		Result.Reason = TEXT("Samples do not bracket the intended contact interval"); return Result;
	}
	for (int32 I = Before; I <= After; ++I)
	{
		const FPairedContactPose& Pose = Poses[I];
		if (!Pose.bEligible) { Result.Reason = Pose.IneligibilityReason; return Result; }
		if (I > Before)
		{
			Result.MaximumSampleGapSeconds = FMath::Max(Result.MaximumSampleGapSeconds, Pose.TimeSeconds - Poses[I - 1].TimeSeconds);
			if (Result.MaximumSampleGapSeconds > Rule.MaximumSampleGapSeconds)
			{
				Result.Reason = TEXT("Contact interval exceeds the declared sampling uncertainty"); return Result;
			}
		}
		const FTransform* Source = Pose.Points.Find(Rule.SourcePoint);
		const FTransform* End = Rule.SourceEndPoint.IsEmpty() ? Source : Pose.Points.Find(Rule.SourceEndPoint);
		const FTransform* Target = Pose.Points.Find(Rule.TargetPoint);
		if (!Source || !End || !Target || !Source->IsValid() || !End->IsValid() || !Target->IsValid())
		{
			Result.Reason = TEXT("Missing or invalid intended contact point; no origin or nearest-bone fallback"); return Result;
		}
		if (Pose.TimeSeconds < Rule.StartSeconds || Pose.TimeSeconds > Rule.EndSeconds) { continue; }
		if (!Rule.SourceEndPoint.IsEmpty() && Source->GetLocation().Equals(End->GetLocation(), UE_SMALL_NUMBER))
		{
			Result.Reason = TEXT("A declared segment has coincident endpoints; use explicit point intent instead"); return Result;
		}
		const FVector Closest = FMath::ClosestPointOnSegment(Target->GetLocation(), Source->GetLocation(), End->GetLocation());
		FPairedContactObservation Observation;
		Observation.TimeSeconds = Pose.TimeSeconds; Observation.OriginalTimeSeconds = Pose.OriginalTimeSeconds;
		Observation.SignedGapCm = FVector::Distance(Closest, Target->GetLocation()) - Rule.TargetRadiusCm;
		if (Rule.bMeasureOrientation)
		{
			const FVector Axis = Rule.SourceEndPoint.IsEmpty() ? Source->GetUnitAxis(EAxis::X) : (End->GetLocation() - Source->GetLocation()).GetSafeNormal();
			if (Axis.IsNearlyZero()) { Result.Reason = TEXT("Degenerate source axis cannot establish orientation"); return Result; }
			const FVector Normal = Target->TransformVectorNoScale(Rule.TargetLocalNormal).GetSafeNormal();
			const double Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Axis, Normal), -1.0, 1.0)));
			Observation.AngleErrorDegrees = FMath::Abs(Angle - Rule.ExpectedAngleDegrees);
		}
		Observation.bWithinCriteria = Observation.SignedGapCm >= Rule.MinimumGapCm && Observation.SignedGapCm <= Rule.MaximumGapCm
			&& (!Rule.bMeasureOrientation || Observation.AngleErrorDegrees <= Rule.AngleToleranceDegrees);
		Result.MatchingSamples += Observation.bWithinCriteria ? 1 : 0;
		Result.Observations.Add(Observation);
	}
	if (Result.Observations.IsEmpty()) { Result.Reason = TEXT("No observed pose inside the contact interval"); return Result; }
	const bool bPass = Rule.bSustained ? Result.MatchingSamples == Result.Observations.Num() : Result.MatchingSamples > 0;
	Result.Status = bPass ? EPairedContactResult::Pass : EPairedContactResult::Fail;
	Result.Reason = Rule.bSustained ? TEXT("All observed poses must satisfy the sustained contact intent") : TEXT("At least one observed pose must satisfy contact within the intended interval");
	return Result;
}

FVector UPairedAnimationAnalysisLibrary::CalculateWeightedCenter(const TArray<FVector>& Locations, const TArray<float>& Weights)
{
	FVector WeightedSum = FVector::ZeroVector;
	double TotalWeight = 0.0;
	for (int32 Index = 0; Index < Locations.Num(); ++Index)
	{
		const double Weight = Weights.IsValidIndex(Index) ? Weights[Index] : 1.0;
		WeightedSum += Locations[Index] * Weight;
		TotalWeight += Weight;
	}
	return TotalWeight > 0.0 ? WeightedSum / TotalWeight : FVector::ZeroVector;
}

void UPairedAnimationAnalysisLibrary::GetHumanoidMassWeights(TArray<FName>& OutBoneNames, TArray<float>& OutWeights)
{
	// Approximate body-segment mass fractions (biomechanics tables); arms and legs are per side.
	static const TPair<const TCHAR*, float> Segments[] = {
		{ TEXT("head"), 0.08f }, { TEXT("neck_01"), 0.02f },
		{ TEXT("spine_03"), 0.20f }, { TEXT("spine_01"), 0.15f }, { TEXT("pelvis"), 0.10f },
		{ TEXT("upperarm_l"), 0.03f }, { TEXT("lowerarm_l"), 0.02f }, { TEXT("hand_l"), 0.01f },
		{ TEXT("upperarm_r"), 0.03f }, { TEXT("lowerarm_r"), 0.02f }, { TEXT("hand_r"), 0.01f },
		{ TEXT("thigh_l"), 0.10f }, { TEXT("calf_l"), 0.05f }, { TEXT("foot_l"), 0.02f },
		{ TEXT("thigh_r"), 0.10f }, { TEXT("calf_r"), 0.05f }, { TEXT("foot_r"), 0.02f },
	};

	OutBoneNames.Reset(UE_ARRAY_COUNT(Segments));
	OutWeights.Reset(UE_ARRAY_COUNT(Segments));
	for (const TPair<const TCHAR*, float>& Segment : Segments)
	{
		OutBoneNames.Add(FName(Segment.Key));
		OutWeights.Add(Segment.Value);
	}
}

bool UPairedAnimationAnalysisLibrary::CalculateSegmentCenterOfMass(const TArray<FVector>& Locations, const TArray<float>& Weights,
	float TableTotalWeight, float MinimumCoverage, FVector& OutCenter)
{
	OutCenter = FVector::ZeroVector;
	if (Locations.Num() == 0 || Weights.Num() != Locations.Num() || !(TableTotalWeight > 0.0f))
	{
		return false;
	}

	double MatchedWeight = 0.0;
	for (const float Weight : Weights)
	{
		MatchedWeight += Weight;
	}
	if (MatchedWeight / TableTotalWeight < MinimumCoverage)
	{
		return false;
	}

	OutCenter = CalculateWeightedCenter(Locations, Weights);
	return true;
}
