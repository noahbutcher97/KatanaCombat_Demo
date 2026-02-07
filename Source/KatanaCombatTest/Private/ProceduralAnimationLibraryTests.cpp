// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Utilities/ProceduralAnimationLibrary.h"
#include "Data/ProceduralAnimationTypes.h"

// ============================================================================
// CATEGORY 1: BLEND TIMING TESTS
// ============================================================================

/**
 * Test: CalculateProceduralBlend produces progress-based blend times derived from perceptual params.
 * Validates NO MAGIC NUMBERS - all values derived from framerate and frame counts.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralBlendProgressBasedTest, "KatanaCombat.ProceduralAnimation.BlendTiming.ProgressBased", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FProceduralBlendProgressBasedTest::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// Animation start (progress = 0): should get longer blend (MaxBlend)
	FProceduralBlendResult StartResult = UProceduralAnimationLibrary::CalculateProceduralBlend(0.0f, 1.0f, Config, false);
	TestTrue("Start result is valid", StartResult.IsValid());
	TestEqual("Tier 1 used", StartResult.TierUsed, 1);

	// Animation end (progress = 0.9, 100ms remaining): should get shorter blend (MinBlend)
	// Using 0.9 instead of 0.99 to ensure remaining time (100ms) > typical DerivedMin
	FProceduralBlendResult EndResult = UProceduralAnimationLibrary::CalculateProceduralBlend(0.9f, 1.0f, Config, false);

	// End blend should be shorter than start blend
	TestTrue("End blend shorter than start blend", EndResult.BlendInTime <= StartResult.BlendInTime);

	// Verify blend times are derived from perceptual params, not hardcoded
	const float DerivedMin = Config.PerceptualParams.GetMinBlendTime();
	const float DerivedMax = Config.PerceptualParams.GetMaxBlendTime();
	TestTrue("Start blend within derived bounds", StartResult.BlendInTime >= DerivedMin && StartResult.BlendInTime <= DerivedMax);

	// End blend should be within bounds OR constrained by remaining time (0.1s here)
	const float RemainingTime = 0.1f;  // 1.0 - 0.9
	const bool bWithinBounds = EndResult.BlendInTime >= DerivedMin && EndResult.BlendInTime <= DerivedMax;
	const bool bConstrainedByRemaining = EndResult.BlendInTime <= RemainingTime + 0.001f;
	TestTrue("End blend within derived bounds or constrained by remaining time", bWithinBounds || bConstrainedByRemaining);

	return true;
}

/**
 * Test: Fresh attack (no previous montage) produces instant blend.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralBlendFreshAttackTest, "KatanaCombat.ProceduralAnimation.BlendTiming.FreshAttack", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FProceduralBlendFreshAttackTest::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Snappy);

	// Zero-length montage = fresh attack
	FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(0.0f, 0.0f, Config, false);

	TestTrue("Fresh attack flagged", Result.bIsFreshAttack);
	TestTrue("Instant blend flagged", Result.bUseInstantBlend);
	TestEqual("Zero blend out time", Result.BlendOutTime, 0.0f);

	return true;
}

/**
 * Test: Rapid input handling modes work correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralBlendRapidInputTest, "KatanaCombat.ProceduralAnimation.BlendTiming.RapidInput", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FProceduralBlendRapidInputTest::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// ForceInstant mode
	Config.RapidInputMode = ERapidInputBlendMode::ForceInstant;
	FProceduralBlendResult ForceInstantResult = UProceduralAnimationLibrary::CalculateProceduralBlend(0.5f, 1.0f, Config, true);
	TestTrue("ForceInstant flags instant blend", ForceInstantResult.bUseInstantBlend);
	TestEqual("ForceInstant zero blend in", ForceInstantResult.BlendInTime, 0.0f);

	// QueueUntilComplete mode
	Config.RapidInputMode = ERapidInputBlendMode::QueueUntilComplete;
	FProceduralBlendResult QueueResult = UProceduralAnimationLibrary::CalculateProceduralBlend(0.5f, 1.0f, Config, true);
	TestTrue("Queue signals negative blend in", QueueResult.BlendInTime < 0.0f);

	// Accelerate mode should produce shorter blend than normal
	Config.RapidInputMode = ERapidInputBlendMode::Accelerate;
	Config.AccelerationMultiplier = 2.0f;
	FProceduralBlendResult AccelResult = UProceduralAnimationLibrary::CalculateProceduralBlend(0.5f, 1.0f, Config, true);
	FProceduralBlendResult NormalResult = UProceduralAnimationLibrary::CalculateProceduralBlend(0.5f, 1.0f, Config, false);
	TestTrue("Accelerated blend shorter than normal", AccelResult.BlendInTime < NormalResult.BlendInTime);

	return true;
}

/**
 * Test: Step strategy produces instant blend above threshold.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralBlendStepStrategyTest, "KatanaCombat.ProceduralAnimation.BlendTiming.StepStrategy", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FProceduralBlendStepStrategyTest::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.Strategy = EProceduralStrategy::Step;
	Config.InstantBlendThreshold = 0.9f;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Snappy);

	// Below threshold: should use max blend
	FProceduralBlendResult BelowThreshold = UProceduralAnimationLibrary::CalculateProceduralBlend(0.5f, 1.0f, Config, false);
	TestFalse("Below threshold not instant", BelowThreshold.bUseInstantBlend);

	// Above threshold: should use min blend (near instant)
	FProceduralBlendResult AboveThreshold = UProceduralAnimationLibrary::CalculateProceduralBlend(0.95f, 1.0f, Config, false);
	TestTrue("Above threshold is instant", AboveThreshold.bUseInstantBlend);

	return true;
}

/**
 * Test: DeriveBlendBoundsFromFramerate produces frame-count-based timing.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeriveBlendBoundsFramerateTest, "KatanaCombat.ProceduralAnimation.BlendTiming.DeriveFromFramerate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeriveBlendBoundsFramerateTest::RunTest(const FString& Parameters)
{
	float MinBlend60, MaxBlend60;
	float MinBlend30, MaxBlend30;

	// 60 FPS Snappy preset
	UProceduralAnimationLibrary::DeriveBlendBoundsFromFramerate(60.0f, ECombatFeelPreset::Snappy, MinBlend60, MaxBlend60);

	// 30 FPS Snappy preset - should have longer timing (fewer frames per second)
	UProceduralAnimationLibrary::DeriveBlendBoundsFromFramerate(30.0f, ECombatFeelPreset::Snappy, MinBlend30, MaxBlend30);

	// At 30 FPS, same frame count = longer time
	TestTrue("30 FPS min longer than 60 FPS min", MinBlend30 > MinBlend60);
	TestTrue("30 FPS max longer than 60 FPS max", MaxBlend30 > MaxBlend60);

	// Verify frame count is constant across framerates (time changes, frames don't)
	// At 60 FPS, Snappy = 6-9 frames → 0.1-0.15s
	// At 30 FPS, Snappy = 6-9 frames → 0.2-0.3s
	const float MinFrames60 = MinBlend60 * 60.0f;
	const float MinFrames30 = MinBlend30 * 30.0f;
	TestTrue("Frame counts approximately equal", FMath::Abs(MinFrames60 - MinFrames30) < 1.0f);

	return true;
}

/**
 * Test: MultiFactorBlend combines tiers correctly with different chain modes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMultiFactorBlendChainModesTest, "KatanaCombat.ProceduralAnimation.BlendTiming.MultiFactorChainModes", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMultiFactorBlendChainModesTest::RunTest(const FString& Parameters)
{
	FMultiFactorBlendInput Input;
	Input.CurrentPosition = 0.5f;
	Input.AnimationLength = 1.0f;
	Input.TargetWindupTime = 0.3f; // Adds Tier 2 constraint

	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// Weighted combination
	Config.ChainMode = EMultiFactorChainMode::WeightedCombination;
	FProceduralBlendResult WeightedResult = UProceduralAnimationLibrary::CalculateMultiFactorBlend(Input, Config);
	TestTrue("Weighted result valid", WeightedResult.IsValid());

	// Constraint cascade (should be most conservative)
	Config.ChainMode = EMultiFactorChainMode::ConstraintCascade;
	FProceduralBlendResult CascadeResult = UProceduralAnimationLibrary::CalculateMultiFactorBlend(Input, Config);
	TestTrue("Cascade respects target timing constraint", CascadeResult.BlendInTime <= Input.TargetWindupTime);

	// Tiered fallback (uses highest tier)
	Config.ChainMode = EMultiFactorChainMode::TieredFallback;
	FProceduralBlendResult TieredResult = UProceduralAnimationLibrary::CalculateMultiFactorBlend(Input, Config);
	TestEqual("Tiered uses highest available tier", TieredResult.TierUsed, Input.GetHighestAvailableTier());

	return true;
}

// ============================================================================
// CATEGORY 2: POSE ANALYSIS TESTS
// ============================================================================

/**
 * Test: CalculatePoseSimilarity returns correct similarity for identical poses.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoseSimilarityIdenticalTest, "KatanaCombat.ProceduralAnimation.PoseAnalysis.IdenticalPoses", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPoseSimilarityIdenticalTest::RunTest(const FString& Parameters)
{
	FProceduralPoseSnapshot SnapshotA;
	SnapshotA.BoneNames.Add(FName("Root"));
	SnapshotA.BoneNames.Add(FName("Spine"));
	SnapshotA.BoneTransforms.Add(FTransform::Identity);
	SnapshotA.BoneTransforms.Add(FTransform(FRotator(0, 45, 0), FVector(0, 0, 100)));

	// Identical pose
	FProceduralPoseSnapshot SnapshotB = SnapshotA;

	FPoseSimilarityResult Result = UProceduralAnimationLibrary::CalculatePoseSimilarity(SnapshotA, SnapshotB);

	TestEqual("Identical poses have 1.0 similarity", Result.OverallSimilarity, 1.0f);
	TestEqual("Zero rotation delta", Result.AverageBoneRotationDelta, 0.0f);
	TestEqual("Both bones compared", Result.BonesCompared, 2);

	return true;
}

/**
 * Test: CalculatePoseSimilarity returns lower similarity for different poses.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoseSimilarityDifferentTest, "KatanaCombat.ProceduralAnimation.PoseAnalysis.DifferentPoses", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPoseSimilarityDifferentTest::RunTest(const FString& Parameters)
{
	FProceduralPoseSnapshot SnapshotA;
	SnapshotA.BoneNames.Add(FName("Root"));
	SnapshotA.BoneTransforms.Add(FTransform::Identity);

	FProceduralPoseSnapshot SnapshotB;
	SnapshotB.BoneNames.Add(FName("Root"));
	// 90 degree rotation difference
	SnapshotB.BoneTransforms.Add(FTransform(FRotator(0, 90, 0), FVector::ZeroVector));

	FPoseSimilarityResult Result = UProceduralAnimationLibrary::CalculatePoseSimilarity(SnapshotA, SnapshotB);

	TestTrue("Different poses have lower similarity", Result.OverallSimilarity < 1.0f);
	TestTrue("Rotation delta approximately 90 degrees", FMath::Abs(Result.AverageBoneRotationDelta - 90.0f) < 1.0f);

	return true;
}

/**
 * Test: CalculateRotationDelta returns correct angular distance.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRotationDeltaTest, "KatanaCombat.ProceduralAnimation.PoseAnalysis.RotationDelta", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRotationDeltaTest::RunTest(const FString& Parameters)
{
	FQuat Identity = FQuat::Identity;
	FQuat Rotated90 = FRotator(0, 90, 0).Quaternion();
	FQuat Rotated180 = FRotator(0, 180, 0).Quaternion();

	float Delta0 = UProceduralAnimationLibrary::CalculateRotationDelta(Identity, Identity);
	float Delta90 = UProceduralAnimationLibrary::CalculateRotationDelta(Identity, Rotated90);
	float Delta180 = UProceduralAnimationLibrary::CalculateRotationDelta(Identity, Rotated180);

	TestTrue("Identity to Identity is 0", FMath::Abs(Delta0) < 0.01f);
	TestTrue("Identity to 90deg is ~90", FMath::Abs(Delta90 - 90.0f) < 1.0f);
	TestTrue("Identity to 180deg is ~180", FMath::Abs(Delta180 - 180.0f) < 1.0f);

	return true;
}

// ============================================================================
// CATEGORY 3: IK HELPERS TESTS
// ============================================================================

/**
 * Test: CalculateIKTarget correctly determines reachability.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIKTargetReachabilityTest, "KatanaCombat.ProceduralAnimation.IK.Reachability", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FIKTargetReachabilityTest::RunTest(const FString& Parameters)
{
	FVector RootLocation = FVector::ZeroVector;
	FVector EffectorLocation = FVector(50, 0, 0);
	float ChainLength = 100.0f;

	// Target within reach
	FVector ReachableTarget = FVector(80, 0, 0);
	FProceduralIKTarget ReachableResult = UProceduralAnimationLibrary::CalculateIKTarget(
		EffectorLocation, RootLocation, ReachableTarget, ChainLength);
	TestTrue("Target within chain length is reachable", ReachableResult.bIsReachable);
	TestTrue("Reachable target has high confidence", ReachableResult.Confidence > 0.5f);

	// Target beyond reach
	FVector UnreachableTarget = FVector(200, 0, 0);
	FProceduralIKTarget UnreachableResult = UProceduralAnimationLibrary::CalculateIKTarget(
		EffectorLocation, RootLocation, UnreachableTarget, ChainLength);
	TestFalse("Target beyond chain length is not reachable", UnreachableResult.bIsReachable);
	TestTrue("Unreachable target has low confidence", UnreachableResult.Confidence < 0.5f);

	return true;
}

/**
 * Test: CalculatePredictedIKTarget projects position forward correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIKTargetPredictionTest, "KatanaCombat.ProceduralAnimation.IK.Prediction", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FIKTargetPredictionTest::RunTest(const FString& Parameters)
{
	FVector CurrentTarget = FVector(50, 0, 0);
	FVector Velocity = FVector(100, 0, 0); // Moving right at 100 units/sec
	float PredictionTime = 0.5f;
	float ChainLength = 200.0f;
	FVector RootLocation = FVector::ZeroVector;

	FProceduralIKTarget Result = UProceduralAnimationLibrary::CalculatePredictedIKTarget(
		CurrentTarget, Velocity, PredictionTime, ChainLength, RootLocation);

	// Position should be projected: 50 + (100 * 0.5) = 100
	TestTrue("Predicted position is ahead", Result.TargetPosition.X > CurrentTarget.X);
	TestEqual("Predicted target type", Result.TargetType, EIKTargetType::PredictedPosition);

	// Longer prediction = lower confidence
	FProceduralIKTarget LongResult = UProceduralAnimationLibrary::CalculatePredictedIKTarget(
		CurrentTarget, Velocity, 2.0f, ChainLength, RootLocation);
	TestTrue("Longer prediction has lower confidence", LongResult.Confidence < Result.Confidence);

	return true;
}

// ============================================================================
// CATEGORY 4: POST-BLEND HEALING TESTS
// ============================================================================

/**
 * Test: CalculateSpringCorrection applies Hooke's law correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpringCorrectionTest, "KatanaCombat.ProceduralAnimation.Healing.SpringCorrection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSpringCorrectionTest::RunTest(const FString& Parameters)
{
	FVector CurrentPos = FVector(100, 0, 0);
	FVector TargetPos = FVector::ZeroVector;
	FVector CurrentVel = FVector::ZeroVector;
	float Stiffness = 100.0f;
	float Damping = 10.0f;
	float DeltaTime = 0.016f; // ~60fps

	FVector NewPos, NewVel;
	UProceduralAnimationLibrary::CalculateSpringCorrection(
		CurrentPos, TargetPos, CurrentVel, Stiffness, Damping, DeltaTime, NewPos, NewVel);

	// Spring should pull position toward target
	TestTrue("Position moved toward target", NewPos.X < CurrentPos.X);
	// Velocity should be in direction of target
	TestTrue("Velocity toward target", NewVel.X < 0.0f);

	// With high damping, oscillation should be minimal
	FVector NewPos2, NewVel2;
	UProceduralAnimationLibrary::CalculateSpringCorrection(
		NewPos, TargetPos, NewVel, Stiffness, Damping, DeltaTime, NewPos2, NewVel2);
	TestTrue("Continues toward target", NewPos2.X < NewPos.X);

	return true;
}

/**
 * Test: CalculateBoneHealingCorrection clamps to max correction per frame.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoneHealingMaxCorrectionTest, "KatanaCombat.ProceduralAnimation.Healing.MaxCorrection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBoneHealingMaxCorrectionTest::RunTest(const FString& Parameters)
{
	FTransform CurrentTransform = FTransform(FVector(1000, 0, 0)); // Far from reference
	FTransform ReferenceTransform = FTransform::Identity;

	FPoseHealingConfig Config;
	Config.Strategy = EPoseHealingStrategy::InterpolateToReference;
	Config.HealingStrength = 1.0f;
	Config.MaxCorrectionPerFrame = 10.0f; // Limit to 10 units per frame

	FTransform CorrectedTransform;
	float CorrectionAmount;

	bool bCorrected = UProceduralAnimationLibrary::CalculateBoneHealingCorrection(
		CurrentTransform, ReferenceTransform, Config, 0.016f, FVector::ZeroVector,
		CorrectedTransform, CorrectionAmount);

	TestTrue("Correction was applied", bCorrected);
	TestTrue("Correction clamped to max", CorrectionAmount <= Config.MaxCorrectionPerFrame);

	return true;
}

// ============================================================================
// CATEGORY 5: VELOCITY ANALYSIS TESTS
// ============================================================================

/**
 * Test: CalculateVelocityFromSnapshots derives correct velocities.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVelocityFromSnapshotsTest, "KatanaCombat.ProceduralAnimation.Velocity.FromSnapshots", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVelocityFromSnapshotsTest::RunTest(const FString& Parameters)
{
	FProceduralPoseSnapshot Previous;
	Previous.BoneNames.Add(FName("Root"));
	Previous.BoneTransforms.Add(FTransform(FVector::ZeroVector));

	FProceduralPoseSnapshot Current;
	Current.BoneNames.Add(FName("Root"));
	Current.BoneTransforms.Add(FTransform(FVector(100, 0, 0))); // Moved 100 units

	float TimeDelta = 0.5f; // Half second

	FVelocityAnalysisResult Result = UProceduralAnimationLibrary::CalculateVelocityFromSnapshots(
		Previous, Current, TimeDelta);

	// Velocity should be 100 / 0.5 = 200 units/sec
	TestTrue("Root velocity X approximately 200", FMath::Abs(Result.RootVelocity.X - 200.0f) < 1.0f);
	TestTrue("Max bone speed approximately 200", FMath::Abs(Result.MaxBoneSpeed - 200.0f) < 1.0f);

	return true;
}

/**
 * Test: CalculateKineticEnergy uses correct E = 0.5 * m * v^2 formula.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKineticEnergyTest, "KatanaCombat.ProceduralAnimation.Velocity.KineticEnergy", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKineticEnergyTest::RunTest(const FString& Parameters)
{
	FVelocityAnalysisResult Velocity;
	Velocity.MaxBoneSpeed = 10.0f; // 10 units/sec
	float Mass = 100.0f; // 100 kg

	float Energy = UProceduralAnimationLibrary::CalculateKineticEnergy(Velocity, Mass);

	// E = 0.5 * 100 * 10^2 = 5000
	TestTrue("Kinetic energy correct", FMath::Abs(Energy - 5000.0f) < 0.01f);

	return true;
}

// ============================================================================
// CATEGORY 6: TIMING PREDICTION TESTS
// ============================================================================

/**
 * Test: PredictTimeToPosition calculates correct time accounting for play rate.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPredictTimeToPositionTest, "KatanaCombat.ProceduralAnimation.Timing.TimeToPosition", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPredictTimeToPositionTest::RunTest(const FString& Parameters)
{
	// Normal play rate
	float Time1 = UProceduralAnimationLibrary::PredictTimeToPosition(0.0f, 1.0f, 1.0f);
	TestTrue("Time at 1x rate", FMath::Abs(Time1 - 1.0f) < 0.01f);

	// Double play rate
	float Time2 = UProceduralAnimationLibrary::PredictTimeToPosition(0.0f, 1.0f, 2.0f);
	TestTrue("Time at 2x rate is half", FMath::Abs(Time2 - 0.5f) < 0.01f);

	// Half play rate
	float Time3 = UProceduralAnimationLibrary::PredictTimeToPosition(0.0f, 1.0f, 0.5f);
	TestTrue("Time at 0.5x rate is double", FMath::Abs(Time3 - 2.0f) < 0.01f);

	// Already past target
	float Time4 = UProceduralAnimationLibrary::PredictTimeToPosition(0.5f, 0.25f, 1.0f);
	TestTrue("Negative time if past target", Time4 < 0.0f);

	return true;
}

/**
 * Test: CalculateOptimalInterruptTime finds position where blend completes at animation end.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOptimalInterruptTimeTest, "KatanaCombat.ProceduralAnimation.Timing.OptimalInterrupt", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOptimalInterruptTimeTest::RunTest(const FString& Parameters)
{
	float AnimLength = 1.0f;
	float BlendDuration = 0.2f;
	float PlayRate = 1.0f;

	float OptimalPos = UProceduralAnimationLibrary::CalculateOptimalInterruptTime(
		0.0f, AnimLength, BlendDuration, PlayRate);

	// At 1x rate, optimal position = length - (blend * rate) = 1.0 - 0.2 = 0.8
	TestTrue("Optimal position correct", FMath::Abs(OptimalPos - 0.8f) < 0.01f);

	// At 2x rate, optimal = 1.0 - (0.2 * 2.0) = 0.6
	float OptimalPos2x = UProceduralAnimationLibrary::CalculateOptimalInterruptTime(
		0.0f, AnimLength, BlendDuration, 2.0f);
	TestTrue("Optimal position at 2x rate", FMath::Abs(OptimalPos2x - 0.6f) < 0.01f);

	return true;
}

// ============================================================================
// CATEGORY 7: ROOT MOTION ANALYSIS TESTS
// ============================================================================

/**
 * Test: AnalyzeRootMotion extracts correct metrics from transform array.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRootMotionAnalysisTest, "KatanaCombat.ProceduralAnimation.RootMotion.Analysis", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRootMotionAnalysisTest::RunTest(const FString& Parameters)
{
	TArray<FTransform> Transforms;
	TArray<float> Times;

	// Create simple linear motion: 0 → 100 over 1 second
	Transforms.Add(FTransform(FVector(0, 0, 0)));
	Times.Add(0.0f);
	Transforms.Add(FTransform(FVector(50, 0, 0)));
	Times.Add(0.5f);
	Transforms.Add(FTransform(FVector(100, 0, 0)));
	Times.Add(1.0f);

	FRootMotionAnalysisResult Result = UProceduralAnimationLibrary::AnalyzeRootMotion(Transforms, Times);

	TestTrue("Total translation X is 100", FMath::Abs(Result.TotalTranslation.X - 100.0f) < 0.01f);
	TestTrue("Average velocity X is 100", FMath::Abs(Result.AverageVelocity.X - 100.0f) < 1.0f);
	TestTrue("Has significant motion", Result.bHasSignificantMotion);

	return true;
}

/**
 * Test: CalculateRootMotionBlendWeight scales correctly with significance.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRootMotionBlendWeightTest, "KatanaCombat.ProceduralAnimation.RootMotion.BlendWeight", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRootMotionBlendWeightTest::RunTest(const FString& Parameters)
{
	FRootMotionAnalysisResult HighMotion;
	HighMotion.PeakSpeed = 500.0f;
	HighMotion.bHasSignificantMotion = true;

	FRootMotionAnalysisResult LowMotion;
	LowMotion.PeakSpeed = 50.0f;
	LowMotion.bHasSignificantMotion = true;

	float HighWeight = UProceduralAnimationLibrary::CalculateRootMotionBlendWeight(HighMotion, 100.0f);
	float LowWeight = UProceduralAnimationLibrary::CalculateRootMotionBlendWeight(LowMotion, 100.0f);

	TestTrue("High motion has full weight", HighWeight >= 1.0f);
	TestTrue("Low motion has partial weight", LowWeight < 1.0f && LowWeight > 0.0f);

	return true;
}

// ============================================================================
// CATEGORY 8: CONTACT PREDICTION TESTS
// ============================================================================

/**
 * Test: PredictLinearContact detects approaching objects.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContactPredictionTest, "KatanaCombat.ProceduralAnimation.Contact.LinearPrediction", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FContactPredictionTest::RunTest(const FString& Parameters)
{
	// Two objects approaching each other
	FVector PosA = FVector(-50, 0, 0);
	FVector VelA = FVector(100, 0, 0); // Moving right

	FVector PosB = FVector(50, 0, 0);
	FVector VelB = FVector(-100, 0, 0); // Moving left

	float ContactRadius = 10.0f;
	float MaxPrediction = 1.0f;

	FContactPredictionResult Result = UProceduralAnimationLibrary::PredictLinearContact(
		PosA, VelA, PosB, VelB, ContactRadius, MaxPrediction);

	TestTrue("Contact predicted", Result.bContactPredicted);
	TestTrue("Time to contact is positive", Result.TimeToContact > 0.0f);
	TestTrue("Contact position near origin", Result.ContactPosition.Size() < ContactRadius);

	// Objects moving apart should not predict contact
	FVector VelAway = FVector(-100, 0, 0); // Both moving away
	FContactPredictionResult NoContactResult = UProceduralAnimationLibrary::PredictLinearContact(
		PosA, VelAway, PosB, VelB, ContactRadius, MaxPrediction);
	TestFalse("No contact when moving apart", NoContactResult.bContactPredicted);

	return true;
}

// ============================================================================
// CATEGORY 9: MOMENTUM-AWARE BLENDING TESTS
// ============================================================================

/**
 * Test: CalculateMomentumPreservingBlend adjusts blend based on alignment.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMomentumPreservingBlendTest, "KatanaCombat.ProceduralAnimation.Momentum.PreservingBlend", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMomentumPreservingBlendTest::RunTest(const FString& Parameters)
{
	// Aligned momenta (same direction)
	FVector AlignedIn = FVector(100, 0, 0);
	FVector AlignedOut = FVector(100, 0, 0);

	// Opposing momenta
	FVector OpposingIn = FVector(100, 0, 0);
	FVector OpposingOut = FVector(-100, 0, 0);

	float Progress = 0.5f;

	float AlignedBlend = UProceduralAnimationLibrary::CalculateMomentumPreservingBlend(
		AlignedIn, AlignedOut, Progress);
	float OpposingBlend = UProceduralAnimationLibrary::CalculateMomentumPreservingBlend(
		OpposingIn, OpposingOut, Progress);

	// Aligned should use ease-out (maintain momentum), opposing should use ease-in (dissipate)
	// Both should produce valid blend values
	TestTrue("Aligned blend valid", AlignedBlend >= 0.0f && AlignedBlend <= 1.0f);
	TestTrue("Opposing blend valid", OpposingBlend >= 0.0f && OpposingBlend <= 1.0f);

	// Aligned blend at 0.5 progress with ease-out should be > 0.5
	// Opposing blend at 0.5 progress with ease-in should be < 0.5
	TestTrue("Aligned momentum preserved longer", AlignedBlend > 0.5f);
	TestTrue("Opposing momentum dissipates faster", OpposingBlend < 0.5f);

	return true;
}

/**
 * Test: GetMomentumDerivedStrategy recommends appropriate strategy.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMomentumDerivedStrategyTest, "KatanaCombat.ProceduralAnimation.Momentum.DerivedStrategy", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMomentumDerivedStrategyTest::RunTest(const FString& Parameters)
{
	// High deceleration: incoming fast, outgoing slow
	EProceduralStrategy DecelStrategy = UProceduralAnimationLibrary::GetMomentumDerivedStrategy(100.0f, 20.0f);
	TestEqual("Deceleration uses EaseOut", DecelStrategy, EProceduralStrategy::EaseOut);

	// High acceleration: incoming slow, outgoing fast
	EProceduralStrategy AccelStrategy = UProceduralAnimationLibrary::GetMomentumDerivedStrategy(20.0f, 100.0f);
	TestEqual("Acceleration uses EaseIn", AccelStrategy, EProceduralStrategy::EaseIn);

	// Similar speeds: smooth transition
	EProceduralStrategy SimilarStrategy = UProceduralAnimationLibrary::GetMomentumDerivedStrategy(100.0f, 110.0f);
	TestEqual("Similar speeds use EaseInOut", SimilarStrategy, EProceduralStrategy::EaseInOut);

	return true;
}

// ============================================================================
// CATEGORY 10: TIME SCALING TESTS
// ============================================================================

/**
 * Test: CalculatePlayRateForDuration calculates correct rate within bounds.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayRateForDurationTest, "KatanaCombat.ProceduralAnimation.TimeScale.PlayRate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlayRateForDurationTest::RunTest(const FString& Parameters)
{
	// Compress: fit 2 second animation into 1 second
	FTimeScalingResult Compressed = UProceduralAnimationLibrary::CalculatePlayRateForDuration(
		2.0f, 1.0f, 0.5f, 2.0f);
	TestTrue("Compressed is flagged", Compressed.bIsCompressed);
	TestTrue("Play rate is 2.0", FMath::Abs(Compressed.PlayRateMultiplier - 2.0f) < 0.01f);

	// Expand: fit 0.5 second animation into 1 second
	FTimeScalingResult Expanded = UProceduralAnimationLibrary::CalculatePlayRateForDuration(
		0.5f, 1.0f, 0.5f, 2.0f);
	TestTrue("Expanded is flagged", Expanded.bIsExpanded);
	TestTrue("Play rate is 0.5", FMath::Abs(Expanded.PlayRateMultiplier - 0.5f) < 0.01f);

	// Rate clamped to bounds
	FTimeScalingResult Clamped = UProceduralAnimationLibrary::CalculatePlayRateForDuration(
		4.0f, 1.0f, 0.5f, 2.0f);
	TestTrue("Rate clamped to max", Clamped.PlayRateMultiplier <= 2.0f);

	return true;
}

// ============================================================================
// CATEGORY 11: CONSTRAINT SATISFACTION TESTS
// ============================================================================

/**
 * Test: CheckConstraintSatisfaction correctly evaluates spatial constraints.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstraintSatisfactionTest, "KatanaCombat.ProceduralAnimation.Constraint.Satisfaction", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FConstraintSatisfactionTest::RunTest(const FString& Parameters)
{
	TArray<FSpatialConstraint> Constraints;

	// Distance constraint at origin, max distance 100
	// Height constraint centered at 50 (0-100 range)
	FSpatialConstraint DistanceConstraint;
	DistanceConstraint.Center = FVector::ZeroVector;
	DistanceConstraint.MaxDistance = 100.0f;
	DistanceConstraint.MinDistance = 0.0f;
	DistanceConstraint.MinHeight = 0.0f;
	DistanceConstraint.MaxHeight = 100.0f;
	DistanceConstraint.bIsActive = true;
	Constraints.Add(DistanceConstraint);

	// Position at center of allowed ranges (XY distance = 50, Z = 50)
	// This should score high on both distance and height
	FVector InsidePos = FVector(50, 0, 50);
	FConstraintSatisfactionResult InsideResult = UProceduralAnimationLibrary::CheckConstraintSatisfaction(
		InsidePos, Constraints);
	TestTrue("Inside position satisfies constraint", InsideResult.AllSatisfied());
	TestTrue("High satisfaction score", InsideResult.OverallScore > 0.3f); // Adjusted threshold

	// Position outside constraint (XY distance > 100)
	FVector OutsidePos = FVector(200, 0, 50);
	FConstraintSatisfactionResult OutsideResult = UProceduralAnimationLibrary::CheckConstraintSatisfaction(
		OutsidePos, Constraints);
	TestFalse("Outside position violates constraint", OutsideResult.AllSatisfied());
	TestTrue("Low satisfaction score", OutsideResult.OverallScore < 0.5f);

	return true;
}

/**
 * Test: CalculateConstraintSatisfyingPosition moves toward valid position.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstraintSatisfyingPositionTest, "KatanaCombat.ProceduralAnimation.Constraint.SatisfyingPosition", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FConstraintSatisfyingPositionTest::RunTest(const FString& Parameters)
{
	TArray<FSpatialConstraint> Constraints;

	FSpatialConstraint Constraint;
	Constraint.Center = FVector::ZeroVector;
	Constraint.MaxDistance = 100.0f;
	Constraint.MinDistance = 0.0f;
	Constraint.bIsActive = true;
	Constraints.Add(Constraint);

	// Start outside constraint
	FVector OutsidePos = FVector(200, 0, 0);
	FVector SuggestedPos = UProceduralAnimationLibrary::CalculateConstraintSatisfyingPosition(
		OutsidePos, Constraints);

	// Suggested position should be closer to constraint center
	float OriginalDist = FVector::Dist(OutsidePos, Constraint.Center);
	float SuggestedDist = FVector::Dist(SuggestedPos, Constraint.Center);
	TestTrue("Suggested position closer to center", SuggestedDist < OriginalDist);

	return true;
}

// ============================================================================
// EASING FUNCTION TESTS
// ============================================================================

/**
 * Test: Easing functions produce correct values at boundaries.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEasingFunctionBoundaryTest, "KatanaCombat.ProceduralAnimation.Easing.Boundaries", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEasingFunctionBoundaryTest::RunTest(const FString& Parameters)
{
	// All easing functions should return 0 at t=0 and 1 at t=1
	TestTrue("Linear(0) = 0", FMath::Abs(UProceduralAnimationLibrary::EaseLinear(0.0f)) < 0.001f);
	TestTrue("Linear(1) = 1", FMath::Abs(UProceduralAnimationLibrary::EaseLinear(1.0f) - 1.0f) < 0.001f);

	TestTrue("EaseInQuad(0) = 0", FMath::Abs(UProceduralAnimationLibrary::EaseInQuad(0.0f)) < 0.001f);
	TestTrue("EaseInQuad(1) = 1", FMath::Abs(UProceduralAnimationLibrary::EaseInQuad(1.0f) - 1.0f) < 0.001f);

	TestTrue("EaseOutQuad(0) = 0", FMath::Abs(UProceduralAnimationLibrary::EaseOutQuad(0.0f)) < 0.001f);
	TestTrue("EaseOutQuad(1) = 1", FMath::Abs(UProceduralAnimationLibrary::EaseOutQuad(1.0f) - 1.0f) < 0.001f);

	TestTrue("EaseInOutCubic(0) = 0", FMath::Abs(UProceduralAnimationLibrary::EaseInOutCubic(0.0f)) < 0.001f);
	TestTrue("EaseInOutCubic(1) = 1", FMath::Abs(UProceduralAnimationLibrary::EaseInOutCubic(1.0f) - 1.0f) < 0.001f);

	TestTrue("EaseOutExpo(0) = 0", FMath::Abs(UProceduralAnimationLibrary::EaseOutExpo(0.0f)) < 0.001f);
	TestTrue("EaseOutExpo(1) = 1", FMath::Abs(UProceduralAnimationLibrary::EaseOutExpo(1.0f) - 1.0f) < 0.001f);

	TestTrue("EaseOutSine(0) = 0", FMath::Abs(UProceduralAnimationLibrary::EaseOutSine(0.0f)) < 0.001f);
	TestTrue("EaseOutSine(1) = 1", FMath::Abs(UProceduralAnimationLibrary::EaseOutSine(1.0f) - 1.0f) < 0.001f);

	return true;
}

/**
 * Test: Easing functions produce expected curve shapes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEasingFunctionShapeTest, "KatanaCombat.ProceduralAnimation.Easing.CurveShapes", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEasingFunctionShapeTest::RunTest(const FString& Parameters)
{
	float t = 0.5f;

	// Linear should be exactly 0.5 at midpoint
	TestTrue("Linear(0.5) = 0.5", FMath::Abs(UProceduralAnimationLibrary::EaseLinear(t) - 0.5f) < 0.001f);

	// EaseIn (slow start) should be < 0.5 at midpoint
	TestTrue("EaseInQuad(0.5) < 0.5", UProceduralAnimationLibrary::EaseInQuad(t) < 0.5f);

	// EaseOut (fast start) should be > 0.5 at midpoint
	TestTrue("EaseOutQuad(0.5) > 0.5", UProceduralAnimationLibrary::EaseOutQuad(t) > 0.5f);

	// EaseInOut should be exactly 0.5 at midpoint (symmetric S-curve)
	TestTrue("EaseInOutCubic(0.5) = 0.5", FMath::Abs(UProceduralAnimationLibrary::EaseInOutCubic(t) - 0.5f) < 0.001f);

	return true;
}

// ============================================================================
// UTILITY FUNCTION TESTS
// ============================================================================

/**
 * Test: GetNormalizedProgress handles edge cases correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNormalizedProgressTest, "KatanaCombat.ProceduralAnimation.Utility.NormalizedProgress", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FNormalizedProgressTest::RunTest(const FString& Parameters)
{
	// Normal case
	TestTrue("50% progress", FMath::Abs(UProceduralAnimationLibrary::GetNormalizedProgress(0.5f, 1.0f) - 0.5f) < 0.001f);

	// Zero length should return 0
	TestTrue("Zero length returns 0", UProceduralAnimationLibrary::GetNormalizedProgress(0.5f, 0.0f) == 0.0f);

	// Negative values clamped
	TestTrue("Negative position clamped to 0", UProceduralAnimationLibrary::GetNormalizedProgress(-1.0f, 1.0f) == 0.0f);

	// Position > length clamped to 1
	TestTrue("Position > length clamped to 1", UProceduralAnimationLibrary::GetNormalizedProgress(2.0f, 1.0f) == 1.0f);

	return true;
}

/**
 * Test: GetRemainingTime calculates correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemainingTimeTest, "KatanaCombat.ProceduralAnimation.Utility.RemainingTime", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRemainingTimeTest::RunTest(const FString& Parameters)
{
	TestTrue("Remaining at start", FMath::Abs(UProceduralAnimationLibrary::GetRemainingTime(0.0f, 1.0f) - 1.0f) < 0.001f);
	TestTrue("Remaining at halfway", FMath::Abs(UProceduralAnimationLibrary::GetRemainingTime(0.5f, 1.0f) - 0.5f) < 0.001f);
	TestTrue("Remaining at end", UProceduralAnimationLibrary::GetRemainingTime(1.0f, 1.0f) == 0.0f);
	TestTrue("Past end clamped to 0", UProceduralAnimationLibrary::GetRemainingTime(2.0f, 1.0f) == 0.0f);

	return true;
}

// ============================================================================
// PERCEPTUAL DERIVATION TESTS
// ============================================================================

/**
 * Test: FPerceptualDerivationParams derives all values from framerate and frame counts.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerceptualDerivationNoMagicNumbersTest, "KatanaCombat.ProceduralAnimation.Perceptual.NoMagicNumbers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPerceptualDerivationNoMagicNumbersTest::RunTest(const FString& Parameters)
{
	// Test that all presets derive values purely from framerate and frame counts
	for (int32 PresetIdx = 0; PresetIdx <= static_cast<int32>(ECombatFeelPreset::Cinematic); ++PresetIdx)
	{
		ECombatFeelPreset Preset = static_cast<ECombatFeelPreset>(PresetIdx);

		FPerceptualDerivationParams Params60 = FPerceptualDerivationParams::FromPreset(60.0f, Preset);
		FPerceptualDerivationParams Params30 = FPerceptualDerivationParams::FromPreset(30.0f, Preset);

		// Frame counts should be the same across framerates for same preset
		TestEqual("Min frame count consistent", Params60.MinFrameCount, Params30.MinFrameCount);
		TestEqual("Max frame count consistent", Params60.MaxFrameCount, Params30.MaxFrameCount);

		// Times should scale with framerate
		// At half framerate, times should be double
		float ExpectedMin30 = Params60.GetMinBlendTime() * 2.0f;
		float ExpectedMax30 = Params60.GetMaxBlendTime() * 2.0f;

		TestTrue("Min time scales with framerate", FMath::Abs(Params30.GetMinBlendTime() - ExpectedMin30) < 0.001f);
		TestTrue("Max time scales with framerate", FMath::Abs(Params30.GetMaxBlendTime() - ExpectedMax30) < 0.001f);
	}

	return true;
}

/**
 * Test: ECombatFeelPreset frame counts match perceptual research.
 * Based on human visual system research: 2-3 frames = instant, 6-9 = snappy, etc.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerceptualPresetFrameCountsTest, "KatanaCombat.ProceduralAnimation.Perceptual.PresetFrameCounts", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPerceptualPresetFrameCountsTest::RunTest(const FString& Parameters)
{
	// Instant: 2-3 frames
	FPerceptualDerivationParams Instant = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Instant);
	TestEqual("Instant min frames", Instant.MinFrameCount, 2);
	TestEqual("Instant max frames", Instant.MaxFrameCount, 3);

	// UltraSnappy: 3-6 frames
	FPerceptualDerivationParams UltraSnappy = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::UltraSnappy);
	TestEqual("UltraSnappy min frames", UltraSnappy.MinFrameCount, 3);
	TestEqual("UltraSnappy max frames", UltraSnappy.MaxFrameCount, 6);

	// Snappy: 6-9 frames
	FPerceptualDerivationParams Snappy = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Snappy);
	TestEqual("Snappy min frames", Snappy.MinFrameCount, 6);
	TestEqual("Snappy max frames", Snappy.MaxFrameCount, 9);

	// Balanced: 9-15 frames
	FPerceptualDerivationParams Balanced = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);
	TestEqual("Balanced min frames", Balanced.MinFrameCount, 9);
	TestEqual("Balanced max frames", Balanced.MaxFrameCount, 15);

	// Smooth: 15-18 frames
	FPerceptualDerivationParams Smooth = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Smooth);
	TestEqual("Smooth min frames", Smooth.MinFrameCount, 15);
	TestEqual("Smooth max frames", Smooth.MaxFrameCount, 18);

	// Cinematic: 18-30 frames (half second at 60fps)
	FPerceptualDerivationParams Cinematic = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Cinematic);
	TestEqual("Cinematic min frames", Cinematic.MinFrameCount, 18);
	TestEqual("Cinematic max frames", Cinematic.MaxFrameCount, 30);

	return true;
}
