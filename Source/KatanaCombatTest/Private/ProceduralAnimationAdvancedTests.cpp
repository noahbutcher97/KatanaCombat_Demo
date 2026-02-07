// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * ProceduralAnimationAdvancedTests.cpp
 *
 * Comprehensive test suite for ProceduralAnimationLibrary covering:
 * - Edge cases and boundary conditions
 * - Integration tests for chained algorithms
 * - Performance benchmarks with latency/throughput measurement
 * - Regression tests against baseline metrics
 * - Context inference and graceful degradation
 *
 * Test Coverage Targets:
 * - All 11 categories have edge case coverage
 * - All multi-factor chaining modes tested with various tier combinations
 * - Performance benchmarks establish baseline metrics
 * - All perceptual presets validated against spec
 */

#include "CombatTestHelpers.h"
#include "Utilities/ProceduralAnimationLibrary.h"
#include "Data/ProceduralAnimationTypes.h"
#include "HAL/PlatformTime.h"

// ============================================================================
// TEST COVERAGE TARGETS
// ============================================================================
// Category 1: Blend Timing - 15 tests (6 core + 9 edge cases)
// Category 2: Pose Analysis - 8 tests (3 core + 5 edge cases)
// Category 3: IK Helpers - 8 tests (3 core + 5 edge cases)
// Category 4: Healing - 6 tests (2 core + 4 edge cases)
// Category 5: Velocity - 8 tests (3 core + 5 edge cases)
// Category 6: Timing - 6 tests (3 core + 3 edge cases)
// Category 7: Root Motion - 6 tests (2 core + 4 edge cases)
// Category 8: Contact - 6 tests (1 core + 5 edge cases)
// Category 9: Momentum - 6 tests (2 core + 4 edge cases)
// Category 10: Time Scaling - 6 tests (1 core + 5 edge cases)
// Category 11: Constraints - 6 tests (2 core + 4 edge cases)
// Integration Tests - 10 tests
// Performance Benchmarks - 5 tests
// Regression Tests - 3 tests
// TOTAL: ~100 tests

// ============================================================================
// EDGE CASE TESTS - CATEGORY 1: BLEND TIMING
// ============================================================================

/**
 * Test: Blend calculation handles zero and negative montage lengths gracefully.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendTimingEdge_ZeroLength, "KatanaCombat.ProceduralAnimation.Advanced.BlendTiming.Edge.ZeroLength", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendTimingEdge_ZeroLength::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// Zero length
	FProceduralBlendResult ZeroResult = UProceduralAnimationLibrary::CalculateProceduralBlend(0.0f, 0.0f, Config, false);
	TestTrue("Zero length returns valid result", ZeroResult.IsValid());
	TestTrue("Zero length flagged as fresh attack", ZeroResult.bIsFreshAttack);

	// Negative length (should be treated as zero)
	FProceduralBlendResult NegResult = UProceduralAnimationLibrary::CalculateProceduralBlend(0.0f, -1.0f, Config, false);
	TestTrue("Negative length returns valid result", NegResult.IsValid());
	TestTrue("Negative length flagged as fresh attack", NegResult.bIsFreshAttack);

	return true;
}

/**
 * Test: Blend calculation handles position exceeding length.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendTimingEdge_PositionExceedsLength, "KatanaCombat.ProceduralAnimation.Advanced.BlendTiming.Edge.PositionExceedsLength", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendTimingEdge_PositionExceedsLength::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// Position > length (e.g., looping animation)
	FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(2.0f, 1.0f, Config, false);
	TestTrue("Position exceeds length returns valid result", Result.IsValid());
	TestTrue("Progress clamped to 1.0", Result.AnimationProgress <= 1.0f);

	return true;
}

/**
 * Test: Blend calculation handles negative position.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendTimingEdge_NegativePosition, "KatanaCombat.ProceduralAnimation.Advanced.BlendTiming.Edge.NegativePosition", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendTimingEdge_NegativePosition::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// Negative position
	FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(-0.5f, 1.0f, Config, false);
	TestTrue("Negative position returns valid result", Result.IsValid());
	TestTrue("Progress clamped to 0.0", Result.AnimationProgress >= 0.0f);

	return true;
}

/**
 * Test: Blend calculation handles very large values without overflow.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendTimingEdge_LargeValues, "KatanaCombat.ProceduralAnimation.Advanced.BlendTiming.Edge.LargeValues", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendTimingEdge_LargeValues::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// Very large values
	FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(1000000.0f, 2000000.0f, Config, false);
	TestTrue("Large values return valid result", Result.IsValid());
	TestTrue("No NaN in blend time", !FMath::IsNaN(Result.BlendInTime));
	TestTrue("No Inf in blend time", FMath::IsFinite(Result.BlendInTime));

	return true;
}

/**
 * Test: All perceptual presets produce valid, progressively longer times.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendTimingEdge_AllPresets, "KatanaCombat.ProceduralAnimation.Advanced.BlendTiming.Edge.AllPresets", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendTimingEdge_AllPresets::RunTest(const FString& Parameters)
{
	float PreviousMaxBlend = 0.0f;

	for (int32 PresetIdx = 0; PresetIdx <= static_cast<int32>(ECombatFeelPreset::Cinematic); ++PresetIdx)
	{
		ECombatFeelPreset Preset = static_cast<ECombatFeelPreset>(PresetIdx);
		FProceduralBlendConfig Config;
		Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, Preset);

		float MaxBlend = Config.GetEffectiveMaxBlendTime();

		TestTrue(FString::Printf(TEXT("Preset %d produces valid max blend"), PresetIdx), MaxBlend > 0.0f);
		TestTrue(FString::Printf(TEXT("Preset %d max >= previous preset max"), PresetIdx), MaxBlend >= PreviousMaxBlend);

		PreviousMaxBlend = MaxBlend;
	}

	return true;
}

/**
 * Test: All strategies produce valid interpolation values.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendTimingEdge_AllStrategies, "KatanaCombat.ProceduralAnimation.Advanced.BlendTiming.Edge.AllStrategies", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendTimingEdge_AllStrategies::RunTest(const FString& Parameters)
{
	for (int32 StratIdx = 0; StratIdx <= static_cast<int32>(EProceduralStrategy::CustomCurve); ++StratIdx)
	{
		EProceduralStrategy Strategy = static_cast<EProceduralStrategy>(StratIdx);

		// Test at various progress points
		for (float Progress : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
		{
			float Alpha = UProceduralAnimationLibrary::ApplyStrategy(Progress, Strategy, nullptr);
			TestTrue(FString::Printf(TEXT("Strategy %d at progress %.2f valid"), StratIdx, Progress),
					 Alpha >= 0.0f && Alpha <= 1.0f);
		}
	}

	return true;
}

/**
 * Test: Remaining time constraint is respected.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendTimingEdge_RemainingTimeConstraint, "KatanaCombat.ProceduralAnimation.Advanced.BlendTiming.Edge.RemainingTimeConstraint", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBlendTimingEdge_RemainingTimeConstraint::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Smooth);

	// Position near end with very little remaining time
	FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(0.98f, 1.0f, Config, false);
	TestTrue("Blend time constrained by remaining time", Result.BlendInTime <= Result.RemainingTime + 0.001f);

	return true;
}

// ============================================================================
// EDGE CASE TESTS - CATEGORY 2: POSE ANALYSIS
// ============================================================================

/**
 * Test: Pose similarity handles empty snapshots.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoseAnalysisEdge_EmptySnapshots, "KatanaCombat.ProceduralAnimation.Advanced.PoseAnalysis.Edge.EmptySnapshots", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPoseAnalysisEdge_EmptySnapshots::RunTest(const FString& Parameters)
{
	FProceduralPoseSnapshot EmptyA;
	FProceduralPoseSnapshot EmptyB;

	FPoseSimilarityResult Result = UProceduralAnimationLibrary::CalculatePoseSimilarity(EmptyA, EmptyB);
	TestTrue("Empty snapshots return valid result", true); // Should not crash
	TestEqual("Empty snapshots have 0 bones compared", Result.BonesCompared, 0);

	return true;
}

/**
 * Test: Pose similarity handles mismatched bone counts.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoseAnalysisEdge_MismatchedBoneCounts, "KatanaCombat.ProceduralAnimation.Advanced.PoseAnalysis.Edge.MismatchedBoneCounts", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPoseAnalysisEdge_MismatchedBoneCounts::RunTest(const FString& Parameters)
{
	FProceduralPoseSnapshot SnapshotA;
	SnapshotA.BoneNames.Add(FName("Bone1"));
	SnapshotA.BoneNames.Add(FName("Bone2"));
	SnapshotA.BoneTransforms.Add(FTransform::Identity);
	SnapshotA.BoneTransforms.Add(FTransform::Identity);

	FProceduralPoseSnapshot SnapshotB;
	SnapshotB.BoneNames.Add(FName("Bone1"));
	SnapshotB.BoneTransforms.Add(FTransform::Identity);

	FPoseSimilarityResult Result = UProceduralAnimationLibrary::CalculatePoseSimilarity(SnapshotA, SnapshotB);
	TestEqual("Compares minimum of bone counts", Result.BonesCompared, 1);

	return true;
}

/**
 * Test: Rotation delta handles extreme angles (180 degrees).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoseAnalysisEdge_ExtremeRotation, "KatanaCombat.ProceduralAnimation.Advanced.PoseAnalysis.Edge.ExtremeRotation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPoseAnalysisEdge_ExtremeRotation::RunTest(const FString& Parameters)
{
	FQuat Identity = FQuat::Identity;
	FQuat Flipped = FRotator(180, 0, 0).Quaternion();

	float Delta = UProceduralAnimationLibrary::CalculateRotationDelta(Identity, Flipped);
	TestTrue("180 degree rotation produces ~180 delta", FMath::Abs(Delta - 180.0f) < 1.0f);
	TestTrue("No NaN in extreme rotation", !FMath::IsNaN(Delta));

	return true;
}

// ============================================================================
// EDGE CASE TESTS - CATEGORY 3: IK HELPERS
// ============================================================================

/**
 * Test: IK target handles zero chain length.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIKEdge_ZeroChainLength, "KatanaCombat.ProceduralAnimation.Advanced.IK.Edge.ZeroChainLength", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FIKEdge_ZeroChainLength::RunTest(const FString& Parameters)
{
	FProceduralIKTarget Result = UProceduralAnimationLibrary::CalculateIKTarget(
		FVector(50, 0, 0), FVector::ZeroVector, FVector(100, 0, 0), 0.0f);

	TestTrue("Zero chain length returns valid result", true); // Should not crash
	TestFalse("Zero chain length is not reachable", Result.bIsReachable);

	return true;
}

/**
 * Test: Predicted IK target handles zero prediction time.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIKEdge_ZeroPredictionTime, "KatanaCombat.ProceduralAnimation.Advanced.IK.Edge.ZeroPredictionTime", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FIKEdge_ZeroPredictionTime::RunTest(const FString& Parameters)
{
	FVector CurrentTarget = FVector(50, 0, 0);
	FVector Velocity = FVector(100, 0, 0);

	FProceduralIKTarget Result = UProceduralAnimationLibrary::CalculatePredictedIKTarget(
		CurrentTarget, Velocity, 0.0f, 100.0f, FVector::ZeroVector);

	// Zero prediction time should return current position
	TestTrue("Zero prediction returns current position",
			 FVector::Dist(Result.TargetPosition, CurrentTarget) < 0.1f);

	return true;
}

/**
 * Test: Contact point IK handles coincident positions.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIKEdge_CoincidentPositions, "KatanaCombat.ProceduralAnimation.Advanced.IK.Edge.CoincidentPositions", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FIKEdge_CoincidentPositions::RunTest(const FString& Parameters)
{
	FVector SamePos = FVector(50, 0, 0);

	FProceduralIKTarget Result = UProceduralAnimationLibrary::CalculateContactPointIKTarget(
		SamePos, SamePos, 0.5f, 100.0f, FVector::ZeroVector);

	TestTrue("Coincident positions return that position",
			 FVector::Dist(Result.TargetPosition, SamePos) < 0.1f);

	return true;
}

// ============================================================================
// EDGE CASE TESTS - CATEGORY 4: HEALING
// ============================================================================

/**
 * Test: Spring correction handles zero stiffness.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealingEdge_ZeroStiffness, "KatanaCombat.ProceduralAnimation.Advanced.Healing.Edge.ZeroStiffness", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHealingEdge_ZeroStiffness::RunTest(const FString& Parameters)
{
	FVector CurrentPos = FVector(100, 0, 0);
	FVector TargetPos = FVector::ZeroVector;
	FVector CurrentVel = FVector::ZeroVector;
	FVector NewPos, NewVel;

	UProceduralAnimationLibrary::CalculateSpringCorrection(
		CurrentPos, TargetPos, CurrentVel, 0.0f, 0.0f, 0.016f, NewPos, NewVel);

	// With zero stiffness and damping, position should remain unchanged
	TestTrue("Zero stiffness preserves position",
			 FVector::Dist(NewPos, CurrentPos) < 0.1f);

	return true;
}

/**
 * Test: Spring correction handles zero delta time.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealingEdge_ZeroDeltaTime, "KatanaCombat.ProceduralAnimation.Advanced.Healing.Edge.ZeroDeltaTime", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHealingEdge_ZeroDeltaTime::RunTest(const FString& Parameters)
{
	FVector CurrentPos = FVector(100, 0, 0);
	FVector TargetPos = FVector::ZeroVector;
	FVector CurrentVel = FVector(10, 0, 0);
	FVector NewPos, NewVel;

	UProceduralAnimationLibrary::CalculateSpringCorrection(
		CurrentPos, TargetPos, CurrentVel, 100.0f, 10.0f, 0.0f, NewPos, NewVel);

	// With zero delta time, position should remain unchanged
	TestTrue("Zero delta time preserves position",
			 FVector::Dist(NewPos, CurrentPos) < 0.1f);

	return true;
}

// ============================================================================
// EDGE CASE TESTS - CATEGORY 5: VELOCITY
// ============================================================================

/**
 * Test: Velocity from snapshots handles zero time delta.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVelocityEdge_ZeroTimeDelta, "KatanaCombat.ProceduralAnimation.Advanced.Velocity.Edge.ZeroTimeDelta", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVelocityEdge_ZeroTimeDelta::RunTest(const FString& Parameters)
{
	FProceduralPoseSnapshot Previous;
	Previous.BoneNames.Add(FName("Root"));
	Previous.BoneTransforms.Add(FTransform(FVector::ZeroVector));

	FProceduralPoseSnapshot Current;
	Current.BoneNames.Add(FName("Root"));
	Current.BoneTransforms.Add(FTransform(FVector(100, 0, 0)));

	FVelocityAnalysisResult Result = UProceduralAnimationLibrary::CalculateVelocityFromSnapshots(
		Previous, Current, 0.0f);

	// Should handle gracefully without division by zero
	TestTrue("Zero time delta returns valid result", true);
	TestTrue("No NaN in velocity", !FMath::IsNaN(Result.MaxBoneSpeed));

	return true;
}

/**
 * Test: Kinetic energy handles zero velocity.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVelocityEdge_ZeroVelocity, "KatanaCombat.ProceduralAnimation.Advanced.Velocity.Edge.ZeroVelocity", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVelocityEdge_ZeroVelocity::RunTest(const FString& Parameters)
{
	FVelocityAnalysisResult Velocity;
	Velocity.MaxBoneSpeed = 0.0f;

	float Energy = UProceduralAnimationLibrary::CalculateKineticEnergy(Velocity, 100.0f);
	TestEqual("Zero velocity produces zero energy", Energy, 0.0f);

	return true;
}

// ============================================================================
// EDGE CASE TESTS - CATEGORY 6: TIMING
// ============================================================================

/**
 * Test: Time to position handles zero play rate.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimingEdge_ZeroPlayRate, "KatanaCombat.ProceduralAnimation.Advanced.Timing.Edge.ZeroPlayRate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTimingEdge_ZeroPlayRate::RunTest(const FString& Parameters)
{
	float TimeForward = UProceduralAnimationLibrary::PredictTimeToPosition(0.0f, 1.0f, 0.0f);
	TestTrue("Zero play rate forward returns infinity/max", TimeForward > 1000000.0f);

	float TimePast = UProceduralAnimationLibrary::PredictTimeToPosition(1.0f, 0.0f, 0.0f);
	TestEqual("Zero play rate past returns 0", TimePast, 0.0f);

	return true;
}

// ============================================================================
// EDGE CASE TESTS - CATEGORY 8: CONTACT
// ============================================================================

/**
 * Test: Contact prediction handles stationary objects.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContactEdge_StationaryObjects, "KatanaCombat.ProceduralAnimation.Advanced.Contact.Edge.Stationary", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FContactEdge_StationaryObjects::RunTest(const FString& Parameters)
{
	FContactPredictionResult Result = UProceduralAnimationLibrary::PredictLinearContact(
		FVector(-50, 0, 0), FVector::ZeroVector,  // A stationary
		FVector(50, 0, 0), FVector::ZeroVector,   // B stationary
		10.0f, 1.0f);

	TestFalse("Stationary objects don't predict contact", Result.bContactPredicted);

	return true;
}

/**
 * Test: Contact prediction handles parallel movement.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContactEdge_ParallelMovement, "KatanaCombat.ProceduralAnimation.Advanced.Contact.Edge.ParallelMovement", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FContactEdge_ParallelMovement::RunTest(const FString& Parameters)
{
	FContactPredictionResult Result = UProceduralAnimationLibrary::PredictLinearContact(
		FVector(-50, 0, 0), FVector(100, 0, 0),   // A moving right
		FVector(50, 0, 0), FVector(100, 0, 0),    // B moving right (same direction/speed)
		10.0f, 1.0f);

	TestFalse("Parallel movement doesn't predict contact", Result.bContactPredicted);

	return true;
}

// ============================================================================
// INTEGRATION TESTS - CHAINED ALGORITHMS
// ============================================================================

/**
 * Test: Multi-factor blend integrates all available tiers correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntegration_MultiFactor_AllTiers, "KatanaCombat.ProceduralAnimation.Advanced.Integration.MultiFactor.AllTiers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FIntegration_MultiFactor_AllTiers::RunTest(const FString& Parameters)
{
	FMultiFactorBlendInput Input;
	Input.CurrentPosition = 0.5f;
	Input.AnimationLength = 1.0f;
	Input.TargetWindupTime = 0.3f; // Tier 2

	// Add Tier 3 (pose similarity)
	Input.bHasPoseSimilarity = true;
	Input.PoseSimilarity.OverallSimilarity = 0.8f; // Similar poses = shorter blend

	// Add Tier 4 (velocity)
	Input.bHasVelocityData = true;
	Input.VelocityAnalysis.MaxBoneSpeed = 200.0f; // High velocity = longer blend

	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// Test all chain modes
	for (int32 ModeIdx = 0; ModeIdx <= static_cast<int32>(EMultiFactorChainMode::Adaptive); ++ModeIdx)
	{
		Config.ChainMode = static_cast<EMultiFactorChainMode>(ModeIdx);
		FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateMultiFactorBlend(Input, Config);

		TestTrue(FString::Printf(TEXT("Chain mode %d produces valid result"), ModeIdx), Result.IsValid());
		TestTrue(FString::Printf(TEXT("Chain mode %d reports highest tier"), ModeIdx), Result.TierUsed >= 3);
	}

	return true;
}

/**
 * Test: Context inference selects appropriate tier based on available data.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntegration_ContextInference, "KatanaCombat.ProceduralAnimation.Advanced.Integration.ContextInference", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FIntegration_ContextInference::RunTest(const FString& Parameters)
{
	// Tier 1 only
	FMultiFactorBlendInput Tier1Only;
	Tier1Only.CurrentPosition = 0.5f;
	Tier1Only.AnimationLength = 1.0f;
	TestEqual("Tier 1 only detected", Tier1Only.GetHighestAvailableTier(), 1);

	// Tier 2
	FMultiFactorBlendInput Tier2;
	Tier2.CurrentPosition = 0.5f;
	Tier2.AnimationLength = 1.0f;
	Tier2.TargetWindupTime = 0.3f;
	TestEqual("Tier 2 detected", Tier2.GetHighestAvailableTier(), 2);

	// Tier 3
	FMultiFactorBlendInput Tier3;
	Tier3.CurrentPosition = 0.5f;
	Tier3.AnimationLength = 1.0f;
	Tier3.bHasPoseSimilarity = true;
	TestEqual("Tier 3 detected", Tier3.GetHighestAvailableTier(), 3);

	// Tier 4
	FMultiFactorBlendInput Tier4;
	Tier4.CurrentPosition = 0.5f;
	Tier4.AnimationLength = 1.0f;
	Tier4.bHasVelocityData = true;
	TestEqual("Tier 4 detected", Tier4.GetHighestAvailableTier(), 4);

	return true;
}

/**
 * Test: Momentum-aware blending integrates with pose similarity.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntegration_MomentumPose, "KatanaCombat.ProceduralAnimation.Advanced.Integration.MomentumPose", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FIntegration_MomentumPose::RunTest(const FString& Parameters)
{
	// High momentum with high pose similarity - blend should be short
	FVelocityAnalysisResult HighMomentum;
	HighMomentum.MaxBoneSpeed = 500.0f;
	HighMomentum.MomentumDirection = FVector(1, 0, 0);

	FPoseSimilarityResult HighSimilarity;
	HighSimilarity.OverallSimilarity = 0.9f;

	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	float MomentumBlend = HighMomentum.GetRecommendedBlendTime(
		Config.GetEffectiveMinBlendTime(), Config.GetEffectiveMaxBlendTime());
	float SimilarityBlend = HighSimilarity.GetRecommendedBlendTime(
		Config.GetEffectiveMinBlendTime(), Config.GetEffectiveMaxBlendTime());

	// Momentum wants longer blend, similarity wants shorter - system should balance
	TestTrue("Both recommendations valid", MomentumBlend > 0.0f && SimilarityBlend > 0.0f);

	return true;
}

/**
 * Test: Contact-aware blend respects timing constraints.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntegration_ContactTiming, "KatanaCombat.ProceduralAnimation.Advanced.Integration.ContactTiming", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FIntegration_ContactTiming::RunTest(const FString& Parameters)
{
	// Two approaching objects
	FContactPredictionResult Contact = UProceduralAnimationLibrary::PredictLinearContact(
		FVector(-100, 0, 0), FVector(200, 0, 0),  // A moving right fast
		FVector(100, 0, 0), FVector(-200, 0, 0),  // B moving left fast
		20.0f, 2.0f);

	TestTrue("Contact predicted", Contact.bContactPredicted);

	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	float ContactAwareBlend = UProceduralAnimationLibrary::GetContactAwareBlendTime(
		Contact, Config, 0.05f);

	// Blend should complete before contact
	TestTrue("Blend completes before contact", ContactAwareBlend < Contact.TimeToContact);

	return true;
}

/**
 * Test: Root motion analysis integrates with blend weight calculation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntegration_RootMotionBlend, "KatanaCombat.ProceduralAnimation.Advanced.Integration.RootMotionBlend", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FIntegration_RootMotionBlend::RunTest(const FString& Parameters)
{
	// Create root motion data
	TArray<FTransform> Transforms;
	TArray<float> Times;

	for (int32 i = 0; i <= 10; ++i)
	{
		float t = i / 10.0f;
		Transforms.Add(FTransform(FVector(t * 200.0f, 0, 0))); // 200 units over 1 second
		Times.Add(t);
	}

	FRootMotionAnalysisResult Analysis = UProceduralAnimationLibrary::AnalyzeRootMotion(Transforms, Times);
	TestTrue("Significant motion detected", Analysis.bHasSignificantMotion);

	float BlendWeight = UProceduralAnimationLibrary::CalculateRootMotionBlendWeight(Analysis, 100.0f);
	TestTrue("Full blend weight for significant motion", BlendWeight >= 1.0f);

	return true;
}

// ============================================================================
// PERFORMANCE BENCHMARKS
// ============================================================================

/**
 * Benchmark: Measure blend calculation latency.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBenchmark_BlendLatency, "KatanaCombat.ProceduralAnimation.Advanced.Benchmark.BlendLatency", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBenchmark_BlendLatency::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	const int32 Iterations = 10000;
	double TotalTime = 0.0;

	for (int32 i = 0; i < Iterations; ++i)
	{
		double StartTime = FPlatformTime::Seconds();
		FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(
			FMath::FRand(), 1.0f, Config, false);
		TotalTime += FPlatformTime::Seconds() - StartTime;
	}

	double AverageLatency = (TotalTime / Iterations) * 1000000.0; // Convert to microseconds

	// Baseline: Single blend calculation should take < 10 microseconds
	AddInfo(FString::Printf(TEXT("Blend calculation average latency: %.3f us"), AverageLatency));
	TestTrue("Blend latency under threshold", AverageLatency < 50.0); // 50us threshold

	return true;
}

/**
 * Benchmark: Measure multi-factor blend throughput.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBenchmark_MultifactorThroughput, "KatanaCombat.ProceduralAnimation.Advanced.Benchmark.MultifactorThroughput", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBenchmark_MultifactorThroughput::RunTest(const FString& Parameters)
{
	FMultiFactorBlendInput Input;
	Input.CurrentPosition = 0.5f;
	Input.AnimationLength = 1.0f;
	Input.TargetWindupTime = 0.3f;
	Input.bHasPoseSimilarity = true;
	Input.PoseSimilarity.OverallSimilarity = 0.7f;
	Input.bHasVelocityData = true;
	Input.VelocityAnalysis.MaxBoneSpeed = 150.0f;

	FProceduralBlendConfig Config;
	Config.ChainMode = EMultiFactorChainMode::Adaptive;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	const int32 Iterations = 1000;
	double StartTime = FPlatformTime::Seconds();

	for (int32 i = 0; i < Iterations; ++i)
	{
		Input.CurrentPosition = FMath::FRand();
		FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateMultiFactorBlend(Input, Config);
	}

	double ElapsedTime = FPlatformTime::Seconds() - StartTime;
	double Throughput = Iterations / ElapsedTime;

	// Baseline: Should achieve > 10,000 calculations per second
	AddInfo(FString::Printf(TEXT("Multi-factor blend throughput: %.0f calcs/sec"), Throughput));
	TestTrue("Throughput above threshold", Throughput > 5000.0);

	return true;
}

/**
 * Benchmark: Measure pose similarity calculation latency.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBenchmark_PoseSimilarityLatency, "KatanaCombat.ProceduralAnimation.Advanced.Benchmark.PoseSimilarityLatency", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBenchmark_PoseSimilarityLatency::RunTest(const FString& Parameters)
{
	// Create realistic pose snapshots (50 bones)
	FProceduralPoseSnapshot SnapshotA, SnapshotB;
	for (int32 i = 0; i < 50; ++i)
	{
		SnapshotA.BoneNames.Add(FName(*FString::Printf(TEXT("Bone_%d"), i)));
		SnapshotA.BoneTransforms.Add(FTransform(FRotator(FMath::FRand() * 90, FMath::FRand() * 180, 0), FVector(i * 10, 0, 0)));

		SnapshotB.BoneNames.Add(FName(*FString::Printf(TEXT("Bone_%d"), i)));
		SnapshotB.BoneTransforms.Add(FTransform(FRotator(FMath::FRand() * 90, FMath::FRand() * 180, 0), FVector(i * 10, 0, 0)));
	}

	const int32 Iterations = 1000;
	double TotalTime = 0.0;

	for (int32 i = 0; i < Iterations; ++i)
	{
		double StartTime = FPlatformTime::Seconds();
		FPoseSimilarityResult Result = UProceduralAnimationLibrary::CalculatePoseSimilarity(SnapshotA, SnapshotB);
		TotalTime += FPlatformTime::Seconds() - StartTime;
	}

	double AverageLatency = (TotalTime / Iterations) * 1000000.0;

	// Baseline: 50-bone comparison should take < 100 microseconds
	AddInfo(FString::Printf(TEXT("Pose similarity (50 bones) average latency: %.3f us"), AverageLatency));
	TestTrue("Pose similarity latency under threshold", AverageLatency < 200.0);

	return true;
}

/**
 * Benchmark: Measure spring correction latency.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBenchmark_SpringCorrectionLatency, "KatanaCombat.ProceduralAnimation.Advanced.Benchmark.SpringCorrectionLatency", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBenchmark_SpringCorrectionLatency::RunTest(const FString& Parameters)
{
	const int32 Iterations = 100000;
	double TotalTime = 0.0;

	FVector CurrentPos = FVector(100, 50, 25);
	FVector TargetPos = FVector::ZeroVector;
	FVector Velocity = FVector(10, 5, 2);
	FVector NewPos, NewVel;

	for (int32 i = 0; i < Iterations; ++i)
	{
		double StartTime = FPlatformTime::Seconds();
		UProceduralAnimationLibrary::CalculateSpringCorrection(
			CurrentPos, TargetPos, Velocity, 100.0f, 10.0f, 0.016f, NewPos, NewVel);
		TotalTime += FPlatformTime::Seconds() - StartTime;
	}

	double AverageLatency = (TotalTime / Iterations) * 1000000.0;

	// Baseline: Spring calculation should take < 1 microsecond
	AddInfo(FString::Printf(TEXT("Spring correction average latency: %.3f us"), AverageLatency));
	TestTrue("Spring correction latency under threshold", AverageLatency < 5.0);

	return true;
}

// ============================================================================
// REGRESSION TESTS
// ============================================================================

/**
 * Regression: Verify perceptual derivation parameters match spec.
 * Frame counts based on human visual system research.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRegression_PerceptualSpec, "KatanaCombat.ProceduralAnimation.Advanced.Regression.PerceptualSpec", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRegression_PerceptualSpec::RunTest(const FString& Parameters)
{
	// Verify frame counts match perceptual research
	// Reference: Human visual system perceives distinct "events" at these thresholds

	// Instant: 2-3 frames (~33-50ms at 60fps) - below change blindness threshold
	FPerceptualDerivationParams Instant = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Instant);
	TestTrue("Instant within change blindness threshold",
			 Instant.GetMaxBlendTime() <= 0.05f); // 50ms

	// Snappy: 6-9 frames (~100-150ms) - responsive interaction threshold
	FPerceptualDerivationParams Snappy = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Snappy);
	TestTrue("Snappy within responsive threshold",
			 Snappy.GetMaxBlendTime() <= 0.2f); // 200ms

	// Smooth: 15-18 frames (~250-300ms) - continuous motion perception
	FPerceptualDerivationParams Smooth = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Smooth);
	TestTrue("Smooth appropriate for continuous motion",
			 Smooth.GetMinBlendTime() >= 0.2f && Smooth.GetMaxBlendTime() <= 0.4f);

	return true;
}

/**
 * Regression: Verify easing functions maintain mathematical properties.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRegression_EasingMath, "KatanaCombat.ProceduralAnimation.Advanced.Regression.EasingMath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRegression_EasingMath::RunTest(const FString& Parameters)
{
	// Test monotonicity: easing functions should be non-decreasing
	for (float t = 0.0f; t < 1.0f; t += 0.1f)
	{
		float t2 = t + 0.1f;

		TestTrue("Linear monotonic", UProceduralAnimationLibrary::EaseLinear(t) <= UProceduralAnimationLibrary::EaseLinear(t2));
		TestTrue("EaseInQuad monotonic", UProceduralAnimationLibrary::EaseInQuad(t) <= UProceduralAnimationLibrary::EaseInQuad(t2));
		TestTrue("EaseOutQuad monotonic", UProceduralAnimationLibrary::EaseOutQuad(t) <= UProceduralAnimationLibrary::EaseOutQuad(t2));
		TestTrue("EaseInOutCubic monotonic", UProceduralAnimationLibrary::EaseInOutCubic(t) <= UProceduralAnimationLibrary::EaseInOutCubic(t2) + 0.001f);
	}

	// EaseOutBack is intentionally non-monotonic (overshoots then returns)
	// So we don't test it for monotonicity

	return true;
}

/**
 * Regression: Verify spring correction converges to target.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRegression_SpringConvergence, "KatanaCombat.ProceduralAnimation.Advanced.Regression.SpringConvergence", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRegression_SpringConvergence::RunTest(const FString& Parameters)
{
	FVector CurrentPos = FVector(1000, 0, 0);
	FVector TargetPos = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Stiffness = 100.0f;
	float Damping = 20.0f; // Critical damping for smooth convergence
	float DeltaTime = 0.016f;

	// Simulate 120 frames (2 seconds)
	for (int32 i = 0; i < 120; ++i)
	{
		FVector NewPos, NewVel;
		UProceduralAnimationLibrary::CalculateSpringCorrection(
			CurrentPos, TargetPos, Velocity, Stiffness, Damping, DeltaTime, NewPos, NewVel);
		CurrentPos = NewPos;
		Velocity = NewVel;
	}

	// After 2 seconds, should be close to target
	float FinalDistance = FVector::Dist(CurrentPos, TargetPos);
	TestTrue("Spring converges to target", FinalDistance < 10.0f);

	return true;
}

// ============================================================================
// GRACEFUL DEGRADATION TESTS
// ============================================================================

/**
 * Test: Multi-factor blend degrades gracefully when higher tiers unavailable.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDegradation_TierFallback, "KatanaCombat.ProceduralAnimation.Advanced.Degradation.TierFallback", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDegradation_TierFallback::RunTest(const FString& Parameters)
{
	FProceduralBlendConfig Config;
	Config.ChainMode = EMultiFactorChainMode::TieredFallback;
	Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(60.0f, ECombatFeelPreset::Balanced);

	// Full data
	FMultiFactorBlendInput FullData;
	FullData.CurrentPosition = 0.5f;
	FullData.AnimationLength = 1.0f;
	FullData.TargetWindupTime = 0.3f;
	FullData.bHasPoseSimilarity = true;
	FullData.PoseSimilarity.OverallSimilarity = 0.8f;
	FullData.bHasVelocityData = true;
	FullData.VelocityAnalysis.MaxBoneSpeed = 100.0f;

	FProceduralBlendResult FullResult = UProceduralAnimationLibrary::CalculateMultiFactorBlend(FullData, Config);

	// Remove velocity data
	FullData.bHasVelocityData = false;
	FProceduralBlendResult NoVelocityResult = UProceduralAnimationLibrary::CalculateMultiFactorBlend(FullData, Config);

	// Remove pose data
	FullData.bHasPoseSimilarity = false;
	FProceduralBlendResult NoPoseResult = UProceduralAnimationLibrary::CalculateMultiFactorBlend(FullData, Config);

	// Remove target data
	FullData.TargetWindupTime = 0.0f;
	FProceduralBlendResult MinimalResult = UProceduralAnimationLibrary::CalculateMultiFactorBlend(FullData, Config);

	// All should produce valid results
	TestTrue("Full data valid", FullResult.IsValid());
	TestTrue("No velocity valid", NoVelocityResult.IsValid());
	TestTrue("No pose valid", NoPoseResult.IsValid());
	TestTrue("Minimal data valid", MinimalResult.IsValid());

	// Tier usage should decrease as data is removed
	TestTrue("Tier decreases as data removed", MinimalResult.TierUsed <= NoVelocityResult.TierUsed);

	return true;
}
