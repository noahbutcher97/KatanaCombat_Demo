// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/ProceduralAnimationLibrary.h"
#include "Curves/CurveFloat.h"

// ============================================================================
// CATEGORY 1: BLEND TIMING
// ============================================================================

float UProceduralAnimationLibrary::ApplyStrategy(
	float Progress,
	EProceduralStrategy Strategy,
	UCurveFloat* CustomCurve)
{
	const float T = FMath::Clamp(Progress, 0.0f, 1.0f);

	switch (Strategy)
	{
	case EProceduralStrategy::Linear:
		return EaseLinear(T);

	case EProceduralStrategy::EaseOut:
		return EaseOutQuad(T);

	case EProceduralStrategy::EaseIn:
		return EaseInQuad(T);

	case EProceduralStrategy::EaseInOut:
		return EaseInOutCubic(T);

	case EProceduralStrategy::Step:
		// Step returns 1.0 for any non-zero progress
		return T > 0.0f ? 1.0f : 0.0f;

	case EProceduralStrategy::CustomCurve:
		if (CustomCurve)
		{
			return FMath::Clamp(CustomCurve->GetFloatValue(T), 0.0f, 1.0f);
		}
		return EaseLinear(T);

	default:
		return EaseLinear(T);
	}
}

float UProceduralAnimationLibrary::InterpolateWithStrategy(
	float Progress,
	float MinValue,
	float MaxValue,
	EProceduralStrategy Strategy,
	UCurveFloat* CustomCurve)
{
	const float Alpha = ApplyStrategy(Progress, Strategy, CustomCurve);
	// Lerp from Max to Min as progress increases
	// Progress 0 (animation start) → MaxValue
	// Progress 1 (animation end) → MinValue
	return FMath::Lerp(MaxValue, MinValue, Alpha);
}

FProceduralBlendResult UProceduralAnimationLibrary::CalculateProceduralBlend(
	float CurrentPosition,
	float MontageLength,
	const FProceduralBlendConfig& Config,
	bool bIsRapidInput)
{
	FProceduralBlendResult Result;
	Result.UsedStrategy = Config.Strategy;
	Result.UsedChainMode = Config.ChainMode;
	Result.bRapidInputDetected = bIsRapidInput;
	Result.TierUsed = 1;

	// Get derived blend bounds from perceptual params
	const float MinBlend = Config.GetEffectiveMinBlendTime();
	const float MaxBlend = Config.GetEffectiveMaxBlendTime();

	// Fresh attack (no previous montage)
	if (MontageLength <= 0.0f)
	{
		Result.bIsFreshAttack = true;
		Result.bUseInstantBlend = true;
		Result.BlendInTime = MinBlend;
		Result.BlendOutTime = 0.0f;
		Result.AnimationProgress = 0.0f;
		Result.RemainingTime = 0.0f;
		return Result;
	}

	// Calculate progress and remaining time
	Result.AnimationProgress = GetNormalizedProgress(CurrentPosition, MontageLength);
	Result.RemainingTime = GetRemainingTime(CurrentPosition, MontageLength);

	// Handle rapid input based on mode
	if (bIsRapidInput)
	{
		switch (Config.RapidInputMode)
		{
		case ERapidInputBlendMode::ForceInstant:
			Result.bUseInstantBlend = true;
			Result.BlendInTime = 0.0f;
			Result.BlendOutTime = 0.0f;
			return Result;

		case ERapidInputBlendMode::ContinueCurrent:
			// Caller handles - continue with normal calculation
			break;

		case ERapidInputBlendMode::QueueUntilComplete:
			// Signal to queue
			Result.BlendInTime = -1.0f;
			return Result;

		case ERapidInputBlendMode::Accelerate:
			// Calculate normal then apply multiplier below
			break;
		}
	}

	// Step strategy: instant if above derived threshold
	if (Config.Strategy == EProceduralStrategy::Step)
	{
		const float Threshold = Config.GetEffectiveInstantThreshold(MontageLength);
		if (Result.AnimationProgress >= Threshold)
		{
			Result.bUseInstantBlend = true;
			Result.BlendInTime = MinBlend;
			Result.BlendOutTime = MinBlend;
		}
		else
		{
			Result.BlendInTime = MaxBlend;
			Result.BlendOutTime = MaxBlend;
		}
		return Result;
	}

	// Standard interpolation
	Result.RawInterpolationAlpha = ApplyStrategy(Result.AnimationProgress, Config.Strategy, Config.CustomBlendCurve);
	float BlendTime = FMath::Lerp(MaxBlend, MinBlend, Result.RawInterpolationAlpha);

	// Apply acceleration if rapid input in Accelerate mode
	if (bIsRapidInput && Config.RapidInputMode == ERapidInputBlendMode::Accelerate)
	{
		const float AccelMultiplier = Config.GetEffectiveAccelerationMultiplier();
		BlendTime /= AccelMultiplier;
		BlendTime = FMath::Max(BlendTime, MinBlend * 0.5f);
	}

	// Constrain blend to remaining time - blend cannot exceed available time
	if (BlendTime > Result.RemainingTime)
	{
		BlendTime = Result.RemainingTime;
		Result.bRemainingTimeConstrained = true;
	}

	Result.BlendInTime = BlendTime;
	Result.BlendOutTime = BlendTime;
	Result.bUseInstantBlend = false;

	return Result;
}

FProceduralBlendResult UProceduralAnimationLibrary::CalculateProceduralBlendTargetAware(
	float CurrentPosition,
	float MontageLength,
	float TargetWindupTime,
	const FProceduralBlendConfig& Config,
	bool bIsRapidInput)
{
	// Start with Tier 1 calculation
	FProceduralBlendResult Result = CalculateProceduralBlend(CurrentPosition, MontageLength, Config, bIsRapidInput);
	Result.TierUsed = 2;

	// If target windup time is known, constrain blend to complete before impact
	if (TargetWindupTime > 0.0f && !Result.bUseInstantBlend)
	{
		// Safety factor: complete blend at 80% of windup to have buffer
		const float SafetyFactor = 0.8f;
		const float MaxAllowedBlend = TargetWindupTime * SafetyFactor;

		if (Result.BlendInTime > MaxAllowedBlend)
		{
			Result.BlendInTime = MaxAllowedBlend;
			Result.BlendOutTime = MaxAllowedBlend;
			Result.bTargetTimingConstrained = true;
		}
	}

	return Result;
}

FProceduralBlendResult UProceduralAnimationLibrary::CalculateMultiFactorBlend(
	const FMultiFactorBlendInput& Input,
	const FProceduralBlendConfig& Config)
{
	FProceduralBlendResult Result;
	Result.UsedStrategy = Config.Strategy;
	Result.UsedChainMode = Config.ChainMode;
	Result.bRapidInputDetected = Input.bIsRapidInput;

	const int32 HighestTier = Input.GetHighestAvailableTier();
	Result.TierUsed = HighestTier;

	const float MinBlend = Config.GetEffectiveMinBlendTime();
	const float MaxBlend = Config.GetEffectiveMaxBlendTime();

	// Calculate results from each available tier
	TArray<float> TierBlendTimes;
	TArray<float> TierWeights;

	// Tier 1: Progress-based (always available)
	{
		FProceduralBlendResult Tier1 = CalculateProceduralBlend(
			Input.CurrentPosition, Input.AnimationLength, Config, Input.bIsRapidInput);
		TierBlendTimes.Add(Tier1.BlendInTime);
		TierWeights.Add(1.0f);
	}

	// Tier 2: Target-aware
	if (Input.TargetWindupTime > 0.0f)
	{
		FProceduralBlendResult Tier2 = CalculateProceduralBlendTargetAware(
			Input.CurrentPosition, Input.AnimationLength, Input.TargetWindupTime, Config, Input.bIsRapidInput);
		TierBlendTimes.Add(Tier2.BlendInTime);
		TierWeights.Add(1.5f); // Higher weight for more informed calculation
	}

	// Tier 3: Pose-aware
	if (Input.bHasPoseSimilarity)
	{
		float PoseBlend = Input.PoseSimilarity.GetRecommendedBlendTime(MinBlend, MaxBlend);
		TierBlendTimes.Add(PoseBlend);
		TierWeights.Add(2.0f); // Highest weight for pose similarity
	}

	// Tier 4: Velocity-aware
	if (Input.bHasVelocityData)
	{
		float VelocityBlend = Input.VelocityAnalysis.GetRecommendedBlendTime(MinBlend, MaxBlend);
		TierBlendTimes.Add(VelocityBlend);
		TierWeights.Add(1.5f);
	}

	// Combine based on chain mode
	float FinalBlendTime = MinBlend;

	switch (Config.ChainMode)
	{
	case EMultiFactorChainMode::WeightedCombination:
		{
			float WeightedSum = 0.0f;
			float TotalWeight = 0.0f;
			for (int32 i = 0; i < TierBlendTimes.Num(); ++i)
			{
				if (TierBlendTimes[i] >= 0.0f)
				{
					WeightedSum += TierBlendTimes[i] * TierWeights[i];
					TotalWeight += TierWeights[i];
				}
			}
			FinalBlendTime = (TotalWeight > 0.0f) ? (WeightedSum / TotalWeight) : MinBlend;
		}
		break;

	case EMultiFactorChainMode::ConstraintCascade:
		{
			// Start with max, each tier can only constrain (reduce)
			FinalBlendTime = MaxBlend;
			for (float Blend : TierBlendTimes)
			{
				if (Blend >= 0.0f)
				{
					FinalBlendTime = FMath::Min(FinalBlendTime, Blend);
				}
			}
		}
		break;

	case EMultiFactorChainMode::TieredFallback:
		{
			// Use highest available tier
			if (TierBlendTimes.Num() > 0)
			{
				FinalBlendTime = TierBlendTimes.Last();
			}
		}
		break;

	case EMultiFactorChainMode::Adaptive:
		{
			// Combine all three approaches
			// 1. Get weighted average
			float Weighted = 0.0f;
			float TotalWeight = 0.0f;
			for (int32 i = 0; i < TierBlendTimes.Num(); ++i)
			{
				if (TierBlendTimes[i] >= 0.0f)
				{
					Weighted += TierBlendTimes[i] * TierWeights[i];
					TotalWeight += TierWeights[i];
				}
			}
			Weighted = (TotalWeight > 0.0f) ? (Weighted / TotalWeight) : MinBlend;

			// 2. Get constraint cascade
			float Constrained = MaxBlend;
			for (float Blend : TierBlendTimes)
			{
				if (Blend >= 0.0f)
				{
					Constrained = FMath::Min(Constrained, Blend);
				}
			}

			// 3. Get tiered
			float Tiered = TierBlendTimes.Num() > 0 ? TierBlendTimes.Last() : MinBlend;

			// Adaptive: use the most conservative (shortest) when we have high-tier data,
			// otherwise use weighted average
			if (HighestTier >= 3)
			{
				// High confidence - use constraint cascade
				FinalBlendTime = Constrained;
			}
			else
			{
				// Lower confidence - use weighted average
				FinalBlendTime = Weighted;
			}
		}
		break;
	}

	// Clamp final result
	FinalBlendTime = FMath::Clamp(FinalBlendTime, MinBlend, MaxBlend);

	// Apply constraint satisfaction if available (Tier 5)
	if (Input.SpatialConstraints.Num() > 0)
	{
		// This would need position data to properly evaluate
		// For now, just note that constraints are available
	}

	Result.AnimationProgress = Input.GetProgress();
	Result.RemainingTime = Input.GetRemainingTime();
	Result.BlendInTime = FinalBlendTime;
	Result.BlendOutTime = FinalBlendTime;
	Result.bUseInstantBlend = (FinalBlendTime <= MinBlend * 0.5f);

	return Result;
}

void UProceduralAnimationLibrary::DeriveBlendBoundsFromFramerate(
	float TargetFPS,
	ECombatFeelPreset Preset,
	float& OutMinBlend,
	float& OutMaxBlend)
{
	FPerceptualDerivationParams Params = FPerceptualDerivationParams::FromPreset(TargetFPS, Preset);
	OutMinBlend = Params.GetMinBlendTime();
	OutMaxBlend = Params.GetMaxBlendTime();
}

void UProceduralAnimationLibrary::DeriveBlendBoundsFromMontageLengths(
	float SourceMontageLength,
	float TargetMontageLength,
	const FPerceptualDerivationParams& PerceptualParams,
	float& OutMinBlend,
	float& OutMaxBlend)
{
	// Get base bounds from perceptual params
	float BaseMin = PerceptualParams.GetMinBlendTime();
	float BaseMax = PerceptualParams.GetMaxBlendTime();

	// Reference length: 1 second is "standard"
	const float ReferenceLength = 1.0f;

	// Scale based on source length
	if (SourceMontageLength > 0.0f)
	{
		float LengthRatio = FMath::Sqrt(SourceMontageLength / ReferenceLength); // Square root for gentler scaling
		LengthRatio = FMath::Clamp(LengthRatio, 0.5f, 2.0f);

		OutMinBlend = BaseMin * LengthRatio;
		OutMaxBlend = BaseMax * LengthRatio;
	}
	else
	{
		OutMinBlend = BaseMin;
		OutMaxBlend = BaseMax;
	}

	// If target is known, further constrain
	if (TargetMontageLength > 0.0f)
	{
		// Don't exceed half the target length
		OutMaxBlend = FMath::Min(OutMaxBlend, TargetMontageLength * 0.5f);
	}

	// Ensure valid range
	OutMinBlend = FMath::Max(OutMinBlend, PerceptualParams.GetFrameDuration());
	OutMaxBlend = FMath::Max(OutMaxBlend, OutMinBlend);
}

// ============================================================================
// CATEGORY 2: POSE ANALYSIS & MATCHING
// ============================================================================

FPoseSimilarityResult UProceduralAnimationLibrary::CalculatePoseSimilarity(
	const FProceduralPoseSnapshot& SnapshotA,
	const FProceduralPoseSnapshot& SnapshotB)
{
	FPoseSimilarityResult Result;

	if (!SnapshotA.IsValid() || !SnapshotB.IsValid())
	{
		return Result;
	}

	const int32 BoneCount = FMath::Min(SnapshotA.BoneTransforms.Num(), SnapshotB.BoneTransforms.Num());
	if (BoneCount == 0)
	{
		return Result;
	}

	Result.BonesCompared = BoneCount;

	// Compare root (first bone typically)
	if (BoneCount > 0)
	{
		Result.RootPositionDelta = FVector::Dist(
			SnapshotA.BoneTransforms[0].GetLocation(),
			SnapshotB.BoneTransforms[0].GetLocation());

		Result.RootRotationDelta = CalculateRotationDelta(
			SnapshotA.BoneTransforms[0].GetRotation(),
			SnapshotB.BoneTransforms[0].GetRotation());
	}

	// Compare all bone rotations
	float TotalRotationDelta = 0.0f;
	float MaxDelta = 0.0f;
	int32 MaxDeltaIndex = 0;

	for (int32 i = 0; i < BoneCount; ++i)
	{
		float Delta = CalculateRotationDelta(
			SnapshotA.BoneTransforms[i].GetRotation(),
			SnapshotB.BoneTransforms[i].GetRotation());

		TotalRotationDelta += Delta;

		if (Delta > MaxDelta)
		{
			MaxDelta = Delta;
			MaxDeltaIndex = i;
		}
	}

	Result.AverageBoneRotationDelta = TotalRotationDelta / BoneCount;
	Result.MaxBoneRotationDelta = MaxDelta;

	if (MaxDeltaIndex < SnapshotA.BoneNames.Num())
	{
		Result.MaxDifferenceBone = SnapshotA.BoneNames[MaxDeltaIndex];
	}

	// Calculate overall similarity (0-1)
	// Use rotation delta as primary metric
	// 0 degrees = 1.0 similarity, 90 degrees = 0.5, 180 degrees = 0.0
	const float MaxRotationForZeroSimilarity = 180.0f;
	Result.OverallSimilarity = 1.0f - FMath::Clamp(
		Result.AverageBoneRotationDelta / MaxRotationForZeroSimilarity, 0.0f, 1.0f);

	return Result;
}

float UProceduralAnimationLibrary::GetBlendTimeFromSimilarity(
	const FPoseSimilarityResult& Similarity,
	const FProceduralBlendConfig& Config)
{
	return Similarity.GetRecommendedBlendTime(
		Config.GetEffectiveMinBlendTime(),
		Config.GetEffectiveMaxBlendTime());
}

float UProceduralAnimationLibrary::CalculateRotationDelta(
	const FQuat& RotationA,
	const FQuat& RotationB)
{
	// Angular distance between two quaternions
	const float Dot = FMath::Abs(RotationA | RotationB);
	// Clamp to valid range to avoid NaN from acos
	const float ClampedDot = FMath::Clamp(Dot, 0.0f, 1.0f);
	// Convert to degrees
	return FMath::RadiansToDegrees(2.0f * FMath::Acos(ClampedDot));
}

// ============================================================================
// CATEGORY 3: IK HELPERS
// ============================================================================

FProceduralIKTarget UProceduralAnimationLibrary::CalculateIKTarget(
	const FVector& EffectorLocation,
	const FVector& RootLocation,
	const FVector& TargetPoint,
	float ChainLength)
{
	FProceduralIKTarget Result;
	Result.TargetPosition = TargetPoint;
	Result.TargetType = EIKTargetType::WorldPosition;
	Result.CalculationTime = 0.0f;

	// Calculate distance from root to target
	Result.DistanceToTarget = FVector::Dist(RootLocation, TargetPoint);

	// Check reachability
	Result.bIsReachable = (Result.DistanceToTarget <= ChainLength);

	// Calculate confidence based on distance
	if (ChainLength > 0.0f)
	{
		const float DistanceRatio = Result.DistanceToTarget / ChainLength;
		// Full confidence at 80% reach, drops off beyond
		Result.Confidence = FMath::Clamp(1.0f - FMath::Max(0.0f, DistanceRatio - 0.8f) * 5.0f, 0.0f, 1.0f);
	}

	// Calculate target rotation (pointing toward target)
	FVector Direction = (TargetPoint - EffectorLocation).GetSafeNormal();
	if (!Direction.IsNearlyZero())
	{
		Result.TargetRotation = Direction.Rotation();
	}

	return Result;
}

FProceduralIKTarget UProceduralAnimationLibrary::CalculatePredictedIKTarget(
	const FVector& CurrentTarget,
	const FVector& Velocity,
	float PredictionTime,
	float ChainLength,
	const FVector& RootLocation)
{
	// Project target forward
	FVector PredictedPosition = CurrentTarget + Velocity * PredictionTime;

	FProceduralIKTarget Result = CalculateIKTarget(
		CurrentTarget, RootLocation, PredictedPosition, ChainLength);

	Result.TargetType = EIKTargetType::PredictedPosition;
	Result.CalculationTime = PredictionTime;

	// Reduce confidence for longer predictions (uncertainty increases)
	const float PredictionConfidence = FMath::Exp(-PredictionTime * 2.0f); // Exponential decay
	Result.Confidence *= PredictionConfidence;

	return Result;
}

FProceduralIKTarget UProceduralAnimationLibrary::CalculateContactPointIKTarget(
	const FVector& ActorALocation,
	const FVector& ActorBLocation,
	float BlendWeight,
	float ChainLength,
	const FVector& RootLocation)
{
	// Calculate contact point as weighted average
	FVector ContactPoint = FMath::Lerp(ActorALocation, ActorBLocation, BlendWeight);

	FProceduralIKTarget Result = CalculateIKTarget(
		ActorALocation, RootLocation, ContactPoint, ChainLength);

	Result.TargetType = EIKTargetType::ContactPoint;

	return Result;
}

// ============================================================================
// CATEGORY 4: POST-BLEND HEALING
// ============================================================================

bool UProceduralAnimationLibrary::CalculateBoneHealingCorrection(
	const FTransform& CurrentTransform,
	const FTransform& ReferenceTransform,
	const FPoseHealingConfig& Config,
	float DeltaTime,
	const FVector& PreviousVelocity,
	FTransform& OutCorrectedTransform,
	float& OutCorrectionAmount)
{
	OutCorrectedTransform = CurrentTransform;
	OutCorrectionAmount = 0.0f;

	if (Config.HealingStrength <= 0.0f)
	{
		return false;
	}

	switch (Config.Strategy)
	{
	case EPoseHealingStrategy::InterpolateToReference:
		{
			// Simple lerp toward reference
			const float Alpha = FMath::Clamp(Config.HealingStrength * DeltaTime * 10.0f, 0.0f, 1.0f);
			OutCorrectedTransform.Blend(CurrentTransform, ReferenceTransform, Alpha);

			// Calculate how much we corrected
			FVector PositionDelta = ReferenceTransform.GetLocation() - CurrentTransform.GetLocation();
			OutCorrectionAmount = PositionDelta.Size() * Alpha;
		}
		break;

	case EPoseHealingStrategy::SpringCorrection:
		{
			FVector NewPosition, NewVelocity;
			CalculateSpringCorrection(
				CurrentTransform.GetLocation(),
				ReferenceTransform.GetLocation(),
				PreviousVelocity,
				Config.SpringStiffness,
				Config.SpringDamping,
				DeltaTime,
				NewPosition,
				NewVelocity);

			OutCorrectedTransform.SetLocation(NewPosition);

			// Simple rotation spring
			FQuat CurrentRot = CurrentTransform.GetRotation();
			FQuat TargetRot = ReferenceTransform.GetRotation();
			FQuat NewRot = FQuat::Slerp(CurrentRot, TargetRot, Config.HealingStrength * DeltaTime * 5.0f);
			OutCorrectedTransform.SetRotation(NewRot);

			OutCorrectionAmount = FVector::Dist(CurrentTransform.GetLocation(), NewPosition);
		}
		break;

	case EPoseHealingStrategy::SnapshotBlend:
	case EPoseHealingStrategy::ConstraintSolver:
		// These would require additional state/context - use simple interpolation as fallback
		{
			const float Alpha = FMath::Clamp(Config.HealingStrength * DeltaTime * 10.0f, 0.0f, 1.0f);
			OutCorrectedTransform.Blend(CurrentTransform, ReferenceTransform, Alpha);
			OutCorrectionAmount = FVector::Dist(CurrentTransform.GetLocation(), OutCorrectedTransform.GetLocation());
		}
		break;
	}

	// Clamp correction amount
	if (OutCorrectionAmount > Config.MaxCorrectionPerFrame)
	{
		const float Scale = Config.MaxCorrectionPerFrame / OutCorrectionAmount;
		OutCorrectedTransform.Blend(CurrentTransform, OutCorrectedTransform, Scale);
		OutCorrectionAmount = Config.MaxCorrectionPerFrame;
	}

	return OutCorrectionAmount > KINDA_SMALL_NUMBER;
}

void UProceduralAnimationLibrary::CalculateSpringCorrection(
	const FVector& CurrentPosition,
	const FVector& TargetPosition,
	const FVector& CurrentVelocity,
	float Stiffness,
	float Damping,
	float DeltaTime,
	FVector& OutNewPosition,
	FVector& OutNewVelocity)
{
	// Hooke's law: F = -kx - cv
	// Where x = displacement from target, v = velocity
	FVector Displacement = CurrentPosition - TargetPosition;

	// F = -k*x - c*v
	FVector Force = -Stiffness * Displacement - Damping * CurrentVelocity;

	// v' = v + F*dt (assuming unit mass)
	OutNewVelocity = CurrentVelocity + Force * DeltaTime;

	// x' = x + v'*dt
	OutNewPosition = CurrentPosition + OutNewVelocity * DeltaTime;
}

// ============================================================================
// CATEGORY 5: VELOCITY & MOTION ANALYSIS
// ============================================================================

FVelocityAnalysisResult UProceduralAnimationLibrary::CalculateVelocityFromSnapshots(
	const FProceduralPoseSnapshot& PreviousSnapshot,
	const FProceduralPoseSnapshot& CurrentSnapshot,
	float TimeDelta)
{
	FVelocityAnalysisResult Result;

	if (!PreviousSnapshot.IsValid() || !CurrentSnapshot.IsValid() || TimeDelta <= 0.0f)
	{
		return Result;
	}

	const int32 BoneCount = FMath::Min(PreviousSnapshot.BoneTransforms.Num(), CurrentSnapshot.BoneTransforms.Num());
	if (BoneCount == 0)
	{
		return Result;
	}

	// Root velocity (first bone)
	if (BoneCount > 0)
	{
		FVector RootDelta = CurrentSnapshot.BoneTransforms[0].GetLocation() -
							PreviousSnapshot.BoneTransforms[0].GetLocation();
		Result.RootVelocity = RootDelta / TimeDelta;

		FQuat RotDelta = CurrentSnapshot.BoneTransforms[0].GetRotation() *
						 PreviousSnapshot.BoneTransforms[0].GetRotation().Inverse();
		FVector Axis;
		float Angle;
		RotDelta.ToAxisAndAngle(Axis, Angle);
		Result.RootAngularVelocity = FRotator(0, FMath::RadiansToDegrees(Angle) / TimeDelta, 0);
	}

	// Analyze all bones
	float TotalSpeed = 0.0f;
	float MaxSpeed = 0.0f;
	int32 FastestIndex = 0;
	FVector TotalMomentum = FVector::ZeroVector;

	for (int32 i = 0; i < BoneCount; ++i)
	{
		FVector BoneDelta = CurrentSnapshot.BoneTransforms[i].GetLocation() -
							PreviousSnapshot.BoneTransforms[i].GetLocation();
		FVector BoneVelocity = BoneDelta / TimeDelta;
		float Speed = BoneVelocity.Size();

		TotalSpeed += Speed;
		TotalMomentum += BoneVelocity;

		if (Speed > MaxSpeed)
		{
			MaxSpeed = Speed;
			FastestIndex = i;
		}
	}

	Result.AverageBoneSpeed = TotalSpeed / BoneCount;
	Result.MaxBoneSpeed = MaxSpeed;

	if (FastestIndex < PreviousSnapshot.BoneNames.Num())
	{
		Result.FastestBone = PreviousSnapshot.BoneNames[FastestIndex];
	}

	Result.MomentumDirection = TotalMomentum.GetSafeNormal();

	// Estimate kinetic energy (using average speed and assumed unit mass)
	Result.KineticEnergyEstimate = 0.5f * Result.AverageBoneSpeed * Result.AverageBoneSpeed;

	return Result;
}

float UProceduralAnimationLibrary::CalculateKineticEnergy(
	const FVelocityAnalysisResult& Velocity,
	float EstimatedMass)
{
	// E = 0.5 * m * v^2
	return 0.5f * EstimatedMass * Velocity.MaxBoneSpeed * Velocity.MaxBoneSpeed;
}

float UProceduralAnimationLibrary::GetMomentumAdjustedBlendTime(
	const FVelocityAnalysisResult& Velocity,
	const FProceduralBlendConfig& Config,
	float ReferenceEnergy)
{
	const float MinBlend = Config.GetEffectiveMinBlendTime();
	const float MaxBlend = Config.GetEffectiveMaxBlendTime();

	// Calculate energy-based blend
	return Velocity.GetRecommendedBlendTime(MinBlend, MaxBlend, ReferenceEnergy);
}

// ============================================================================
// CATEGORY 6: ANIMATION TIMING PREDICTION
// ============================================================================

float UProceduralAnimationLibrary::CalculateProceduralTiming(
	float InputValue,
	const FProceduralTimingConfig& Config)
{
	return InterpolateWithStrategy(
		InputValue,
		Config.GetEffectiveMinDuration(),
		Config.GetEffectiveMaxDuration(),
		Config.Strategy,
		Config.CustomCurve);
}

float UProceduralAnimationLibrary::PredictTimeToPosition(
	float CurrentPosition,
	float TargetPosition,
	float PlayRate)
{
	if (PlayRate <= 0.0f)
	{
		return (TargetPosition > CurrentPosition) ? FLT_MAX : 0.0f;
	}

	float Distance = TargetPosition - CurrentPosition;
	return Distance / PlayRate;
}

float UProceduralAnimationLibrary::CalculateOptimalInterruptTime(
	float CurrentPosition,
	float AnimationLength,
	float BlendDuration,
	float PlayRate)
{
	if (PlayRate <= 0.0f || AnimationLength <= 0.0f)
	{
		return CurrentPosition;
	}

	// Find position where: remaining_time == blend_duration
	// remaining = (length - position) / playrate
	// remaining == blend_duration
	// (length - position) / playrate == blend_duration
	// position = length - (blend_duration * playrate)
	float OptimalPosition = AnimationLength - (BlendDuration * PlayRate);

	// Clamp to valid range
	return FMath::Clamp(OptimalPosition, CurrentPosition, AnimationLength);
}

// ============================================================================
// CATEGORY 7: ROOT MOTION ANALYSIS
// ============================================================================

FRootMotionAnalysisResult UProceduralAnimationLibrary::AnalyzeRootMotion(
	const TArray<FTransform>& RootTransforms,
	const TArray<float>& TimeStamps)
{
	FRootMotionAnalysisResult Result;

	if (RootTransforms.Num() < 2 || RootTransforms.Num() != TimeStamps.Num())
	{
		return Result;
	}

	Result.AnalysisWindowDuration = TimeStamps.Last() - TimeStamps[0];
	if (Result.AnalysisWindowDuration <= 0.0f)
	{
		return Result;
	}

	// Total translation and rotation
	Result.TotalTranslation = RootTransforms.Last().GetLocation() - RootTransforms[0].GetLocation();
	Result.TotalRotation = (RootTransforms.Last().GetRotation() * RootTransforms[0].GetRotation().Inverse()).Rotator();

	// Average velocity
	Result.AverageVelocity = Result.TotalTranslation / Result.AnalysisWindowDuration;

	// Find peak speed
	for (int32 i = 1; i < RootTransforms.Num(); ++i)
	{
		float DeltaTime = TimeStamps[i] - TimeStamps[i - 1];
		if (DeltaTime > 0.0f)
		{
			FVector Velocity = (RootTransforms[i].GetLocation() - RootTransforms[i - 1].GetLocation()) / DeltaTime;
			float Speed = Velocity.Size();
			if (Speed > Result.PeakSpeed)
			{
				Result.PeakSpeed = Speed;
				Result.PeakSpeedTime = TimeStamps[i];
			}
		}
	}

	// Dominant axis
	Result.DominantAxis = Result.TotalTranslation.GetSafeNormal();
	if (Result.DominantAxis.IsNearlyZero())
	{
		Result.DominantAxis = FVector::ForwardVector;
	}

	// Significance check (arbitrary threshold of 10 units/sec)
	Result.bHasSignificantMotion = (Result.PeakSpeed > 10.0f);

	return Result;
}

float UProceduralAnimationLibrary::CalculateRootMotionBlendWeight(
	const FRootMotionAnalysisResult& Analysis,
	float SignificanceThreshold)
{
	if (!Analysis.bHasSignificantMotion || SignificanceThreshold <= 0.0f)
	{
		return 0.0f;
	}

	// Scale blend weight based on motion significance
	float Ratio = Analysis.PeakSpeed / SignificanceThreshold;
	return FMath::Clamp(Ratio, 0.0f, 1.0f);
}

// ============================================================================
// CATEGORY 8: CONTACT PREDICTION
// ============================================================================

FContactPredictionResult UProceduralAnimationLibrary::PredictLinearContact(
	const FVector& PositionA,
	const FVector& VelocityA,
	const FVector& PositionB,
	const FVector& VelocityB,
	float ContactRadius,
	float MaxPredictionTime)
{
	FContactPredictionResult Result;

	// Relative position and velocity
	FVector RelPos = PositionB - PositionA;
	FVector RelVel = VelocityB - VelocityA;

	float RelSpeed = RelVel.Size();
	if (RelSpeed < KINDA_SMALL_NUMBER)
	{
		// Not approaching each other
		Result.bContactPredicted = false;
		return Result;
	}

	// Time to closest approach
	float TimeToClosest = -FVector::DotProduct(RelPos, RelVel) / (RelSpeed * RelSpeed);

	if (TimeToClosest < 0.0f || TimeToClosest > MaxPredictionTime)
	{
		// Contact in the past or too far in future
		Result.bContactPredicted = false;
		return Result;
	}

	// Position at closest approach
	FVector ClosestA = PositionA + VelocityA * TimeToClosest;
	FVector ClosestB = PositionB + VelocityB * TimeToClosest;
	float ClosestDistance = FVector::Dist(ClosestA, ClosestB);

	if (ClosestDistance <= ContactRadius)
	{
		Result.bContactPredicted = true;
		Result.TimeToContact = TimeToClosest;
		Result.ContactPosition = (ClosestA + ClosestB) * 0.5f;
		Result.ContactNormal = (ClosestB - ClosestA).GetSafeNormal();
		Result.ImpactVelocity = RelSpeed;

		// Confidence based on how close the approach is
		Result.Confidence = 1.0f - (ClosestDistance / ContactRadius);
	}
	else
	{
		Result.bContactPredicted = false;
		Result.Confidence = 0.0f;
	}

	return Result;
}

float UProceduralAnimationLibrary::GetContactAwareBlendTime(
	const FContactPredictionResult& ContactPrediction,
	const FProceduralBlendConfig& Config,
	float SafetyMargin)
{
	const float MinBlend = Config.GetEffectiveMinBlendTime();
	const float MaxBlend = Config.GetEffectiveMaxBlendTime();

	if (!ContactPrediction.bContactPredicted)
	{
		return MaxBlend;
	}

	// Blend must complete before contact
	float MaxAllowedBlend = ContactPrediction.TimeToContact - SafetyMargin;
	MaxAllowedBlend = FMath::Max(MaxAllowedBlend, MinBlend);

	return FMath::Min(MaxBlend, MaxAllowedBlend);
}

// ============================================================================
// CATEGORY 9: MOMENTUM-AWARE BLENDING
// ============================================================================

float UProceduralAnimationLibrary::CalculateMomentumPreservingBlend(
	const FVector& IncomingMomentum,
	const FVector& OutgoingMomentum,
	float BlendProgress)
{
	float InSpeed = IncomingMomentum.Size();
	float OutSpeed = OutgoingMomentum.Size();

	if (InSpeed < KINDA_SMALL_NUMBER && OutSpeed < KINDA_SMALL_NUMBER)
	{
		return BlendProgress;
	}

	// Check momentum alignment
	FVector InDir = IncomingMomentum.GetSafeNormal();
	FVector OutDir = OutgoingMomentum.GetSafeNormal();
	float Alignment = FVector::DotProduct(InDir, OutDir);

	// If momenta are aligned, use ease-out curve (maintain momentum longer)
	// If opposing, use ease-in curve (dissipate momentum faster)
	if (Alignment > 0.5f)
	{
		return EaseOutQuad(BlendProgress);
	}
	else if (Alignment < -0.5f)
	{
		return EaseInQuad(BlendProgress);
	}
	else
	{
		return EaseInOutCubic(BlendProgress);
	}
}

EProceduralStrategy UProceduralAnimationLibrary::GetMomentumDerivedStrategy(
	float IncomingSpeed,
	float OutgoingSpeed)
{
	const float SpeedDelta = OutgoingSpeed - IncomingSpeed;
	const float MaxSpeed = FMath::Max(IncomingSpeed, OutgoingSpeed);

	if (MaxSpeed < KINDA_SMALL_NUMBER)
	{
		return EProceduralStrategy::Linear;
	}

	const float RelativeDelta = SpeedDelta / MaxSpeed;

	// High deceleration: ease-out (maintain incoming momentum longer)
	if (RelativeDelta < -0.3f)
	{
		return EProceduralStrategy::EaseOut;
	}
	// High acceleration: ease-in (quickly pick up outgoing momentum)
	else if (RelativeDelta > 0.3f)
	{
		return EProceduralStrategy::EaseIn;
	}
	// Similar speeds: smooth transition
	else
	{
		return EProceduralStrategy::EaseInOut;
	}
}

// ============================================================================
// CATEGORY 10: ANIMATION TIME SCALING
// ============================================================================

FTimeScalingResult UProceduralAnimationLibrary::CalculatePlayRateForDuration(
	float OriginalDuration,
	float TargetDuration,
	float MinPlayRate,
	float MaxPlayRate)
{
	FTimeScalingResult Result;
	Result.OriginalDuration = OriginalDuration;

	if (OriginalDuration <= 0.0f || TargetDuration <= 0.0f)
	{
		Result.PlayRateMultiplier = 1.0f;
		Result.ScaledDuration = OriginalDuration;
		return Result;
	}

	// Calculate ideal play rate
	float IdealRate = OriginalDuration / TargetDuration;

	// Clamp to perceptual bounds
	Result.PlayRateMultiplier = FMath::Clamp(IdealRate, MinPlayRate, MaxPlayRate);
	Result.ScaledDuration = OriginalDuration / Result.PlayRateMultiplier;

	Result.bIsCompressed = (Result.PlayRateMultiplier > 1.0f);
	Result.bIsExpanded = (Result.PlayRateMultiplier < 1.0f);

	return Result;
}

void UProceduralAnimationLibrary::DerivePlayRateBounds(
	const FPerceptualDerivationParams& PerceptualParams,
	float& OutMinRate,
	float& OutMaxRate)
{
	// Base rates on perceptual feel
	// At 60fps, "snappy" animations should be able to play faster
	// Slower framerates need more conservative bounds to avoid jitter

	float BaseFrameMultiplier = PerceptualParams.TargetFramerate / 60.0f;

	// Minimum rate: don't slow down more than 50% (would feel sluggish)
	OutMinRate = 0.5f;

	// Maximum rate: scale with framerate capability
	// Higher framerate can display faster animations smoothly
	OutMaxRate = FMath::Clamp(1.5f * BaseFrameMultiplier, 1.5f, 3.0f);
}

// ============================================================================
// CATEGORY 11: CONSTRAINT SATISFACTION
// ============================================================================

FConstraintSatisfactionResult UProceduralAnimationLibrary::CheckConstraintSatisfaction(
	const FVector& Position,
	const TArray<FSpatialConstraint>& Constraints)
{
	FConstraintSatisfactionResult Result;
	Result.ConstraintsChecked = Constraints.Num();
	Result.ConstraintsSatisfied = 0;
	Result.MostViolatedIndex = -1;
	Result.MostViolatedScore = 1.0f;

	if (Constraints.Num() == 0)
	{
		Result.OverallScore = 1.0f;
		return Result;
	}

	float TotalScore = 0.0f;

	for (int32 i = 0; i < Constraints.Num(); ++i)
	{
		float Score = Constraints[i].GetSatisfactionScore(Position);
		TotalScore += Score;

		if (Constraints[i].IsSatisfied(Position))
		{
			Result.ConstraintsSatisfied++;
		}

		if (Score < Result.MostViolatedScore)
		{
			Result.MostViolatedScore = Score;
			Result.MostViolatedIndex = i;
		}
	}

	Result.OverallScore = TotalScore / Constraints.Num();

	// Calculate suggested correction direction
	if (Result.MostViolatedIndex >= 0)
	{
		const FSpatialConstraint& Violated = Constraints[Result.MostViolatedIndex];
		// Move toward constraint center
		Result.SuggestedCorrection = (Violated.Center - Position).GetSafeNormal();
	}

	return Result;
}

FVector UProceduralAnimationLibrary::CalculateConstraintSatisfyingPosition(
	const FVector& CurrentPosition,
	const TArray<FSpatialConstraint>& Constraints)
{
	if (Constraints.Num() == 0)
	{
		return CurrentPosition;
	}

	// Simple approach: move toward the average constraint center
	// More sophisticated algorithms could use gradient descent or convex optimization

	FVector AverageCenter = FVector::ZeroVector;
	int32 ActiveCount = 0;

	for (const FSpatialConstraint& Constraint : Constraints)
	{
		if (Constraint.bIsActive)
		{
			AverageCenter += Constraint.Center;
			ActiveCount++;
		}
	}

	if (ActiveCount == 0)
	{
		return CurrentPosition;
	}

	AverageCenter /= ActiveCount;

	// Move toward average center, but respect individual constraints
	FVector Direction = (AverageCenter - CurrentPosition).GetSafeNormal();
	FVector NewPosition = CurrentPosition;

	// Iteratively move toward valid position
	for (int32 Iteration = 0; Iteration < 10; ++Iteration)
	{
		FConstraintSatisfactionResult Check = CheckConstraintSatisfaction(NewPosition, Constraints);
		if (Check.AllSatisfied())
		{
			break;
		}

		// Move in suggested direction
		NewPosition += Check.SuggestedCorrection * 10.0f; // 10 units per iteration
	}

	return NewPosition;
}

float UProceduralAnimationLibrary::GetConstraintAdjustedBlendTime(
	const FConstraintSatisfactionResult& ConstraintResult,
	float BaseBlendTime,
	float CorrectionDistance,
	float CorrectionSpeed)
{
	if (ConstraintResult.AllSatisfied() || CorrectionDistance <= 0.0f)
	{
		return BaseBlendTime;
	}

	// Time needed for correction
	float CorrectionTime = (CorrectionSpeed > 0.0f) ? (CorrectionDistance / CorrectionSpeed) : 0.0f;

	// Add correction time to blend time
	return BaseBlendTime + CorrectionTime;
}

// ============================================================================
// CATEGORY 12: CONTROL RIG PARAMETER HELPERS
// ============================================================================

FLookAtResult UProceduralAnimationLibrary::CalculateLookAtParams(
	const FVector& HeadLocation,
	const FVector& HeadForward,
	const FProceduralLookAtTarget& Target)
{
	FLookAtResult Result;

	// Calculate direction to target
	FVector ToTarget = Target.TargetPosition - HeadLocation;
	Result.DistanceToTarget = ToTarget.Size();

	if (Result.DistanceToTarget < KINDA_SMALL_NUMBER)
	{
		return Result;
	}

	FVector ToTargetNorm = ToTarget / Result.DistanceToTarget;

	// Calculate angle to target
	Result.AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(HeadForward, ToTargetNorm)));

	// Check if within valid range
	Result.bTargetInRange = (Result.AngleToTarget <= Target.MaxHeadYaw);

	// Calculate rotation to face target
	FRotator FullRotation = FRotationMatrix::MakeFromX(ToTargetNorm).Rotator();
	FRotator CurrentRotation = FRotationMatrix::MakeFromX(HeadForward).Rotator();
	FRotator DeltaRotation = (FullRotation - CurrentRotation).GetNormalized();

	// Clamp to max angles
	float ClampedYaw = FMath::Clamp(DeltaRotation.Yaw, -Target.MaxHeadYaw, Target.MaxHeadYaw);
	float ClampedPitch = FMath::Clamp(DeltaRotation.Pitch, -Target.MaxHeadPitch, Target.MaxHeadPitch);

	// Distribute rotation across head, chest, eyes based on weights
	Result.HeadRotation = FRotator(ClampedPitch * Target.HeadWeight, ClampedYaw * Target.HeadWeight, 0.0f);
	Result.ChestRotation = FRotator(ClampedPitch * Target.ChestWeight * 0.5f, ClampedYaw * Target.ChestWeight, 0.0f);
	Result.EyeRotation = FRotator(ClampedPitch * Target.EyeWeight, ClampedYaw * Target.EyeWeight * 0.5f, 0.0f);

	return Result;
}

void UProceduralAnimationLibrary::CalculateAimOffsetParams(
	const FVector& ActorForward,
	const FVector& AimDirection,
	float MaxYaw,
	float MaxPitch,
	float& OutYaw,
	float& OutPitch)
{
	FVector Forward2D = FVector(ActorForward.X, ActorForward.Y, 0.0f).GetSafeNormal();
	FVector Right2D = FVector::CrossProduct(FVector::UpVector, Forward2D);

	FVector AimDir = AimDirection.GetSafeNormal();

	// Calculate yaw (left/right)
	float YawAngle = FMath::RadiansToDegrees(FMath::Atan2(
		FVector::DotProduct(AimDir, Right2D),
		FVector::DotProduct(AimDir, Forward2D)));

	// Calculate pitch (up/down)
	float PitchAngle = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(AimDir.Z, -1.0f, 1.0f)));

	// Normalize to -1 to 1 range
	OutYaw = FMath::Clamp(YawAngle / MaxYaw, -1.0f, 1.0f);
	OutPitch = FMath::Clamp(PitchAngle / MaxPitch, -1.0f, 1.0f);
}

TArray<FControlRigParam> UProceduralAnimationLibrary::CalculateWeaponGripParams(
	const FTransform& WeaponGripTransform,
	const FTransform& CurrentHandTransform,
	float ChainLength)
{
	TArray<FControlRigParam> Params;

	// Calculate IK target for hand
	FProceduralIKTarget IKTarget = CalculateIKTarget(
		CurrentHandTransform.GetLocation(),
		CurrentHandTransform.GetLocation() - FVector(0, 0, ChainLength * 0.5f), // Approximate shoulder
		WeaponGripTransform.GetLocation(),
		ChainLength);

	// Position parameter
	Params.Add(FControlRigParam::CreateVector(
		FName("HandIKTarget"),
		IKTarget.TargetPosition,
		IKTarget.Confidence));

	// Rotation parameter
	FControlRigParam RotParam;
	RotParam.ParamName = FName("HandIKRotation");
	RotParam.ParamType = EControlRigParamType::Rotator;
	RotParam.RotatorValue = WeaponGripTransform.GetRotation().Rotator();
	RotParam.BlendWeight = IKTarget.Confidence;
	Params.Add(RotParam);

	// Blend weight based on reachability
	Params.Add(FControlRigParam::CreateScalar(
		FName("HandIKBlend"),
		IKTarget.bIsReachable ? IKTarget.Confidence : 0.0f));

	return Params;
}

void UProceduralAnimationLibrary::CalculateFootIKParams(
	const FVector& FootLocation,
	const FVector& GroundHitLocation,
	const FVector& GroundNormal,
	float MaxAdjustment,
	FVector& OutIKTarget,
	FRotator& OutRotationOffset,
	float& OutBlendWeight)
{
	// Calculate Z adjustment
	float ZDelta = GroundHitLocation.Z - FootLocation.Z;
	float ClampedZ = FMath::Clamp(ZDelta, -MaxAdjustment, MaxAdjustment);

	OutIKTarget = FVector(FootLocation.X, FootLocation.Y, FootLocation.Z + ClampedZ);

	// Calculate rotation to match ground slope
	FVector FootForward = FVector::ForwardVector;
	FVector GroundRight = FVector::CrossProduct(GroundNormal, FootForward).GetSafeNormal();
	FVector GroundForward = FVector::CrossProduct(GroundRight, GroundNormal);
	OutRotationOffset = FRotationMatrix::MakeFromXZ(GroundForward, GroundNormal).Rotator();

	// Blend weight based on how valid the adjustment is
	float AdjustmentRatio = FMath::Abs(ZDelta) / MaxAdjustment;
	OutBlendWeight = FMath::Clamp(1.0f - AdjustmentRatio * 0.5f, 0.0f, 1.0f);
}

TArray<float> UProceduralAnimationLibrary::CalculateSpineTwistDistribution(
	float DesiredRotation,
	int32 SpineBoneCount,
	UCurveFloat* TwistDistribution)
{
	TArray<float> Distribution;

	if (SpineBoneCount <= 0)
	{
		return Distribution;
	}

	Distribution.SetNum(SpineBoneCount);

	for (int32 i = 0; i < SpineBoneCount; ++i)
	{
		float NormalizedIndex = static_cast<float>(i) / static_cast<float>(SpineBoneCount - 1);

		float Weight;
		if (TwistDistribution)
		{
			Weight = TwistDistribution->GetFloatValue(NormalizedIndex);
		}
		else
		{
			// Default: more rotation in upper spine (quadratic distribution)
			Weight = NormalizedIndex * NormalizedIndex;
		}

		Distribution[i] = DesiredRotation * Weight / SpineBoneCount;
	}

	return Distribution;
}

// ============================================================================
// CATEGORY 13: ANIMATION SELECTION & MATCHING
// ============================================================================

float UProceduralAnimationLibrary::ScoreAnimationCandidate(
	const FAnimationCandidate& Candidate,
	const FAnimationSelectionContext& Context)
{
	float Score = Candidate.BasePriority;

	// Distance scoring
	if (Candidate.IdealDistanceRange.X < Candidate.IdealDistanceRange.Y)
	{
		float DistMid = (Candidate.IdealDistanceRange.X + Candidate.IdealDistanceRange.Y) * 0.5f;
		float DistRange = (Candidate.IdealDistanceRange.Y - Candidate.IdealDistanceRange.X) * 0.5f;
		float DistScore = 1.0f - FMath::Clamp(FMath::Abs(Context.DistanceToTarget - DistMid) / DistRange, 0.0f, 1.0f);
		Score *= DistScore;
	}

	// Angle scoring
	if (Candidate.IdealAngleRange.X < Candidate.IdealAngleRange.Y)
	{
		float AngleMid = (Candidate.IdealAngleRange.X + Candidate.IdealAngleRange.Y) * 0.5f;
		float AngleRange = (Candidate.IdealAngleRange.Y - Candidate.IdealAngleRange.X) * 0.5f;
		float AngleScore = 1.0f - FMath::Clamp(FMath::Abs(Context.AngleToTarget - AngleMid) / FMath::Max(AngleRange, 1.0f), 0.0f, 1.0f);
		Score *= AngleScore;
	}

	// Input direction scoring
	if (!Candidate.RequiredInputDirection.IsNearlyZero())
	{
		float InputMatch = FVector::DotProduct(Context.InputDirection.GetSafeNormal(), Candidate.RequiredInputDirection.GetSafeNormal());
		Score *= FMath::Max(0.0f, InputMatch);
	}

	// Heavy attack matching
	if (Candidate.bIsHeavy != Context.bIsHeavyAttack)
	{
		Score *= 0.1f; // Heavily penalize mismatch
	}

	// Gap closer bonus when far
	if (Candidate.bIsGapCloser)
	{
		float GapNeed = CalculateGapCloserNeed(Context.DistanceToTarget, Context.GetSpeed(), Candidate.IdealDistanceRange.Y);
		if (GapNeed > 0.0f)
		{
			Score *= 1.5f; // Bonus for gap closer when needed
		}
	}

	// Combo position validation
	if (!Candidate.IsValidForComboPosition(Context.ComboCount))
	{
		Score = 0.0f;
	}

	return FMath::Max(0.0f, Score);
}

FAnimationSelectionResult UProceduralAnimationLibrary::SelectBestAnimation(
	const TArray<FAnimationCandidate>& Candidates,
	const FAnimationSelectionContext& Context)
{
	FAnimationSelectionResult Result;
	Result.AllScores.SetNum(Candidates.Num());

	float BestScore = -1.0f;

	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		float Score = ScoreAnimationCandidate(Candidates[i], Context);
		Result.AllScores[i] = Score;

		if (Score > BestScore)
		{
			BestScore = Score;
			Result.SelectedIndex = i;
			Result.SelectedAnimationId = Candidates[i].AnimationId;
			Result.SelectedScore = Score;
		}
	}

	Result.bValidSelection = (Result.SelectedIndex >= 0 && Result.SelectedScore > 0.0f);

	if (Result.bValidSelection)
	{
		Result.SelectionReason = FString::Printf(TEXT("Best match with score %.2f"), Result.SelectedScore);
	}
	else
	{
		Result.SelectionReason = TEXT("No valid candidates");
	}

	return Result;
}

int32 UProceduralAnimationLibrary::GetDirectionalAnimationIndex(
	const FVector& InputDirection,
	const FVector& FacingDirection)
{
	if (InputDirection.IsNearlyZero(0.1f))
	{
		return -1; // Neutral
	}

	// Project to 2D
	FVector Input2D = FVector(InputDirection.X, InputDirection.Y, 0.0f).GetSafeNormal();
	FVector Forward2D = FVector(FacingDirection.X, FacingDirection.Y, 0.0f).GetSafeNormal();
	FVector Right2D = FVector::CrossProduct(FVector::UpVector, Forward2D);

	float ForwardDot = FVector::DotProduct(Input2D, Forward2D);
	float RightDot = FVector::DotProduct(Input2D, Right2D);

	// Determine angle
	float Angle = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));

	// Map to 8 directions
	// 0=forward, 1=forward-right, 2=right, 3=back-right, 4=back, 5=back-left, 6=left, 7=forward-left
	float NormalizedAngle = FMath::Fmod(Angle + 360.0f + 22.5f, 360.0f);
	return static_cast<int32>(NormalizedAngle / 45.0f);
}

float UProceduralAnimationLibrary::CalculateGapCloserNeed(
	float DistanceToTarget,
	float CharacterSpeed,
	float AttackRange)
{
	// Gap = distance we can't cover with normal attack range
	float Gap = DistanceToTarget - AttackRange;

	// If already in range, no gap closer needed
	if (Gap <= 0.0f)
	{
		return 0.0f;
	}

	return Gap;
}

float UProceduralAnimationLibrary::GetComboPositionWeight(
	int32 CurrentComboCount,
	float TimeSinceLastAttack,
	float ComboWindowDuration)
{
	// Base weight
	float Weight = 1.0f;

	// Decay based on time since last attack
	if (ComboWindowDuration > 0.0f && TimeSinceLastAttack > 0.0f)
	{
		float TimeRatio = TimeSinceLastAttack / ComboWindowDuration;
		Weight *= (1.0f - FMath::Clamp(TimeRatio, 0.0f, 1.0f));
	}

	// Higher combo positions get slight priority (encourages completing combos)
	Weight *= (1.0f + CurrentComboCount * 0.1f);

	return Weight;
}

// ============================================================================
// CATEGORY 14: LAYERED BLENDING UTILITIES
// ============================================================================

TArray<float> UProceduralAnimationLibrary::CalculateLayeredBlendWeights(
	const FLayeredBlendConfig& Config,
	const TArray<FName>& AllBoneNames,
	const TArray<int32>& BoneParentIndices)
{
	TArray<float> Weights;
	Weights.SetNumZeroed(AllBoneNames.Num());

	// Process each configured bone weight
	for (const FProceduralBoneBlendWeight& BoneWeight : Config.BoneWeights)
	{
		int32 BoneIndex = AllBoneNames.IndexOfByKey(BoneWeight.BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			continue;
		}

		// Set weight for this bone
		Weights[BoneIndex] = BoneWeight.BlendWeight * Config.GlobalWeight;

		// Propagate to children if requested
		if (BoneWeight.bIncludeChildren)
		{
			int32 CurrentDepth = 0;
			for (int32 i = BoneIndex + 1; i < AllBoneNames.Num(); ++i)
			{
				// Check if this bone is a child of our bone
				int32 ParentIndex = BoneParentIndices[i];
				bool bIsChild = false;
				int32 Depth = 0;

				while (ParentIndex != INDEX_NONE)
				{
					Depth++;
					if (ParentIndex == BoneIndex)
					{
						bIsChild = true;
						break;
					}
					ParentIndex = BoneParentIndices[ParentIndex];
				}

				if (bIsChild && (BoneWeight.BlendDepth < 0 || Depth <= BoneWeight.BlendDepth))
				{
					Weights[i] = BoneWeight.BlendWeight * Config.GlobalWeight;
				}
			}
		}
	}

	return Weights;
}

float UProceduralAnimationLibrary::CalculateAdditiveBlendWeight(
	float BaseWeight,
	const FVector& Velocity,
	float AnimProgress,
	float FadeInDuration,
	float FadeOutDuration)
{
	float Weight = BaseWeight;

	// Modulate by velocity (faster movement = less additive)
	float Speed = Velocity.Size();
	const float SpeedThreshold = 300.0f; // Configurable
	if (Speed > SpeedThreshold)
	{
		float SpeedFactor = 1.0f - FMath::Clamp((Speed - SpeedThreshold) / SpeedThreshold, 0.0f, 1.0f);
		Weight *= SpeedFactor;
	}

	// Fade in/out based on animation progress
	if (AnimProgress < FadeInDuration)
	{
		Weight *= AnimProgress / FadeInDuration;
	}
	else if (AnimProgress > (1.0f - FadeOutDuration))
	{
		Weight *= (1.0f - AnimProgress) / FadeOutDuration;
	}

	return FMath::Clamp(Weight, 0.0f, 1.0f);
}

float UProceduralAnimationLibrary::CalculateMaskedBlendAlpha(
	int32 BoneIndex,
	int32 MaskRootBoneIndex,
	int32 HierarchyDepth,
	int32 FalloffDepth)
{
	if (BoneIndex == MaskRootBoneIndex)
	{
		return 1.0f;
	}

	if (FalloffDepth <= 0)
	{
		return 1.0f;
	}

	// Linear falloff based on hierarchy depth
	float Alpha = 1.0f - FMath::Clamp(static_cast<float>(HierarchyDepth) / static_cast<float>(FalloffDepth), 0.0f, 1.0f);
	return Alpha;
}

// ============================================================================
// CATEGORY 15: ANIMATION WARPING HELPERS
// ============================================================================

FProceduralWarpTarget UProceduralAnimationLibrary::CalculateAttackWarpTarget(
	const FVector& AttackerLocation,
	const FVector& AttackerForward,
	const FVector& TargetLocation,
	float IdealAttackDistance,
	float MaxWarpDistance)
{
	FProceduralWarpTarget Result;

	FVector ToTarget = TargetLocation - AttackerLocation;
	float DistanceToTarget = ToTarget.Size2D();

	if (DistanceToTarget < KINDA_SMALL_NUMBER)
	{
		Result.bIsValid = false;
		return Result;
	}

	FVector DirectionToTarget = ToTarget.GetSafeNormal2D();

	// Calculate ideal position (IdealAttackDistance from target)
	Result.TargetPosition = TargetLocation - DirectionToTarget * IdealAttackDistance;
	Result.TargetPosition.Z = AttackerLocation.Z; // Maintain height

	// Calculate warp distance
	Result.WarpDistance = FVector::Dist2D(AttackerLocation, Result.TargetPosition);

	// Check if warp is valid
	if (Result.WarpDistance > MaxWarpDistance)
	{
		// Limit warp to max distance
		FVector WarpDirection = (Result.TargetPosition - AttackerLocation).GetSafeNormal2D();
		Result.TargetPosition = AttackerLocation + WarpDirection * MaxWarpDistance;
		Result.WarpDistance = MaxWarpDistance;
		Result.Confidence = 0.5f;
	}
	else
	{
		Result.Confidence = 1.0f;
	}

	// Calculate rotation to face target
	Result.TargetRotation = DirectionToTarget.Rotation();
	Result.WarpAngle = FMath::Abs(FMath::FindDeltaAngleDegrees(
		AttackerForward.Rotation().Yaw,
		Result.TargetRotation.Yaw));

	Result.WarpAlpha = 1.0f;
	Result.bIsValid = true;

	return Result;
}

FDistanceMatchResult UProceduralAnimationLibrary::CalculateDistanceMatch(
	float CurrentDistance,
	const FDistanceMatchConfig& Config)
{
	FDistanceMatchResult Result;

	if (!Config.DistanceCurve)
	{
		// Without a distance curve, can't match
		Result.bMatchSuccessful = false;
		return Result;
	}

	// Find time on curve where distance matches target
	// This requires sampling the curve to find the best match
	float BestPosition = 0.0f;
	float BestError = FLT_MAX;

	const int32 SampleCount = 20;
	for (int32 i = 0; i <= SampleCount; ++i)
	{
		float Position = static_cast<float>(i) / static_cast<float>(SampleCount);
		float CurveDistance = Config.DistanceCurve->GetFloatValue(Position);
		float DistanceRemaining = Config.TargetDistance - CurveDistance;

		float Error = FMath::Abs(DistanceRemaining - CurrentDistance);
		if (Error < BestError)
		{
			BestError = Error;
			BestPosition = Position;
		}
	}

	Result.StartPosition = BestPosition;
	Result.DistanceError = BestError;
	Result.bMatchSuccessful = (BestError <= Config.DistanceTolerance);

	// Calculate play rate to fine-tune match
	if (Result.bMatchSuccessful && Config.TargetDistance > 0.0f)
	{
		float CurveDistance = Config.DistanceCurve->GetFloatValue(BestPosition);
		float RemainingCurve = Config.TargetDistance - CurveDistance;

		if (RemainingCurve > 0.0f && CurrentDistance > 0.0f)
		{
			float IdealRate = RemainingCurve / CurrentDistance;
			Result.PlayRate = FMath::Clamp(IdealRate, Config.PlayRateBounds.X, Config.PlayRateBounds.Y);
		}
		else
		{
			Result.PlayRate = 1.0f;
		}
	}
	else
	{
		Result.PlayRate = 1.0f;
	}

	return Result;
}

FRotator UProceduralAnimationLibrary::CalculateOrientationWarp(
	const FRotator& CurrentRotation,
	const FVector& TargetDirection,
	float MaxWarpAngle,
	UCurveFloat* WarpCurve)
{
	FRotator TargetRotation = TargetDirection.Rotation();
	float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw);

	// Clamp to max warp angle
	float ClampedYaw = FMath::Clamp(DeltaYaw, -MaxWarpAngle, MaxWarpAngle);

	// Apply curve if provided
	if (WarpCurve)
	{
		float NormalizedInput = FMath::Abs(ClampedYaw) / MaxWarpAngle;
		float CurvedValue = WarpCurve->GetFloatValue(NormalizedInput);
		ClampedYaw = FMath::Sign(ClampedYaw) * CurvedValue * MaxWarpAngle;
	}

	return FRotator(0.0f, ClampedYaw, 0.0f);
}

float UProceduralAnimationLibrary::CalculateStrideWarpScale(
	float DesiredSpeed,
	float AnimationBaseSpeed,
	float MinScale,
	float MaxScale)
{
	if (AnimationBaseSpeed <= 0.0f)
	{
		return 1.0f;
	}

	float IdealScale = DesiredSpeed / AnimationBaseSpeed;
	return FMath::Clamp(IdealScale, MinScale, MaxScale);
}

// ============================================================================
// CATEGORY 16: GAMEPLAY CONTEXT MAPPING
// ============================================================================

int32 UProceduralAnimationLibrary::MapInputToQuadrant(
	const FVector& InputDirection,
	const FVector& FacingDirection,
	float DeadZone)
{
	if (InputDirection.Size2D() < DeadZone)
	{
		return 0; // Neutral
	}

	FVector Input2D = FVector(InputDirection.X, InputDirection.Y, 0.0f).GetSafeNormal();
	FVector Forward2D = FVector(FacingDirection.X, FacingDirection.Y, 0.0f).GetSafeNormal();
	FVector Right2D = FVector::CrossProduct(FVector::UpVector, Forward2D);

	float ForwardDot = FVector::DotProduct(Input2D, Forward2D);
	float RightDot = FVector::DotProduct(Input2D, Right2D);

	// Determine primary direction
	if (FMath::Abs(ForwardDot) > FMath::Abs(RightDot))
	{
		return (ForwardDot > 0.0f) ? 1 : 2; // Forward or Back
	}
	else
	{
		return (RightDot > 0.0f) ? 4 : 3; // Right or Left
	}
}

float UProceduralAnimationLibrary::CalculateAttackIntensity(
	float HoldDuration,
	float MinHoldForHeavy,
	float MaxHoldForCharged)
{
	if (HoldDuration < MinHoldForHeavy)
	{
		return 0.0f; // Light
	}
	else if (HoldDuration < MaxHoldForCharged)
	{
		// Interpolate between heavy (0.5) and charged (1.0)
		float Progress = (HoldDuration - MinHoldForHeavy) / (MaxHoldForCharged - MinHoldForHeavy);
		return 0.5f + Progress * 0.5f;
	}
	else
	{
		return 1.0f; // Fully charged
	}
}

float UProceduralAnimationLibrary::CalculateDefensiveStanceBlend(
	float ThreatDistance,
	int32 ThreatCount,
	float MaxBlendDistance,
	float MinBlendDistance)
{
	if (ThreatCount <= 0)
	{
		return 0.0f;
	}

	// Distance-based blend
	float DistanceBlend = 0.0f;
	if (ThreatDistance <= MinBlendDistance)
	{
		DistanceBlend = 1.0f;
	}
	else if (ThreatDistance < MaxBlendDistance)
	{
		DistanceBlend = 1.0f - (ThreatDistance - MinBlendDistance) / (MaxBlendDistance - MinBlendDistance);
	}

	// Multiply by threat count factor (more threats = more defensive)
	float ThreatFactor = FMath::Min(static_cast<float>(ThreatCount) * 0.33f, 1.0f);
	DistanceBlend *= (0.5f + ThreatFactor * 0.5f);

	return FMath::Clamp(DistanceBlend, 0.0f, 1.0f);
}

void UProceduralAnimationLibrary::MapVelocityToBlendSpace(
	const FVector& Velocity,
	const FVector& FacingDirection,
	float MaxSpeed,
	float& OutForward,
	float& OutRight,
	float& OutSpeed)
{
	FVector Forward2D = FVector(FacingDirection.X, FacingDirection.Y, 0.0f).GetSafeNormal();
	FVector Right2D = FVector::CrossProduct(FVector::UpVector, Forward2D);
	FVector Vel2D = FVector(Velocity.X, Velocity.Y, 0.0f);

	float Speed = Vel2D.Size();
	OutSpeed = (MaxSpeed > 0.0f) ? FMath::Clamp(Speed / MaxSpeed, 0.0f, 1.0f) : 0.0f;

	if (Speed > KINDA_SMALL_NUMBER)
	{
		FVector VelDir = Vel2D / Speed;
		OutForward = FVector::DotProduct(VelDir, Forward2D);
		OutRight = FVector::DotProduct(VelDir, Right2D);
	}
	else
	{
		OutForward = 0.0f;
		OutRight = 0.0f;
	}
}

int32 UProceduralAnimationLibrary::CalculateHitReactionDirection(
	const FVector& DamageDirection,
	const FVector& VictimForward)
{
	FVector DamageDir2D = FVector(DamageDirection.X, DamageDirection.Y, 0.0f).GetSafeNormal();
	FVector Forward2D = FVector(VictimForward.X, VictimForward.Y, 0.0f).GetSafeNormal();
	FVector Right2D = FVector::CrossProduct(FVector::UpVector, Forward2D);

	float ForwardDot = FVector::DotProduct(DamageDir2D, Forward2D);
	float RightDot = FVector::DotProduct(DamageDir2D, Right2D);

	// Determine primary direction (damage comes FROM this direction)
	if (FMath::Abs(ForwardDot) > FMath::Abs(RightDot))
	{
		return (ForwardDot > 0.0f) ? 0 : 1; // Front or Back
	}
	else
	{
		return (RightDot > 0.0f) ? 3 : 2; // Right or Left
	}
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

float UProceduralAnimationLibrary::GetNormalizedProgress(float Position, float Length)
{
	if (Length <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(Position / Length, 0.0f, 1.0f);
}

float UProceduralAnimationLibrary::GetRemainingTime(float CurrentPosition, float MontageLength)
{
	return FMath::Max(0.0f, MontageLength - CurrentPosition);
}

FString UProceduralAnimationLibrary::GetStrategyDisplayName(EProceduralStrategy Strategy)
{
	switch (Strategy)
	{
	case EProceduralStrategy::Linear:
		return TEXT("Linear");
	case EProceduralStrategy::EaseOut:
		return TEXT("Ease Out (Quadratic)");
	case EProceduralStrategy::EaseIn:
		return TEXT("Ease In (Quadratic)");
	case EProceduralStrategy::EaseInOut:
		return TEXT("Ease In-Out (Cubic)");
	case EProceduralStrategy::Step:
		return TEXT("Step (Threshold-based)");
	case EProceduralStrategy::CustomCurve:
		return TEXT("Custom Curve");
	default:
		return TEXT("Unknown");
	}
}

FString UProceduralAnimationLibrary::GetChainModeDisplayName(EMultiFactorChainMode ChainMode)
{
	switch (ChainMode)
	{
	case EMultiFactorChainMode::WeightedCombination:
		return TEXT("Weighted Combination");
	case EMultiFactorChainMode::ConstraintCascade:
		return TEXT("Constraint Cascade");
	case EMultiFactorChainMode::TieredFallback:
		return TEXT("Tiered Fallback");
	case EMultiFactorChainMode::Adaptive:
		return TEXT("Adaptive (All Methods)");
	default:
		return TEXT("Unknown");
	}
}
