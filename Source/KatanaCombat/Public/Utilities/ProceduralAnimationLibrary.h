// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/ProceduralAnimationTypes.h"
#include "ProceduralAnimationLibrary.generated.h"

// Forward declarations
class UCurveFloat;
class UAnimMontage;
class USkeletalMeshComponent;

/**
 * Procedural Animation Library
 *
 * Comprehensive stateless utility functions for procedural animation calculations.
 * All functions are pure (no side effects) and operate on primitives.
 *
 * Architecture: LIBRARY LAYER in three-layer pattern:
 *   Types (Data/ProceduralAnimationTypes.h) → Library (this) → Component (CombatComponent)
 *
 * Design Principles:
 * - NO MAGIC NUMBERS: All constants derived from inputs (framerate, duration, velocity, etc.)
 * - Graceful degradation: Functions work with partial information
 * - Multi-factor chaining: Weighted, Cascade, Tiered, and Adaptive combination modes
 * - Testable: Pure functions with deterministic outputs
 *
 * Categories (11 total):
 * 1. Blend Timing - Calculate blend durations from context
 * 2. Pose Analysis - Compare poses, find similarity scores
 * 3. IK Helpers - Target calculation, reach validation
 * 4. Post-Blend Healing - Fix pose artifacts after transitions
 * 5. Velocity & Motion Analysis - Analyze bone/character velocities
 * 6. Animation Timing Prediction - Predict event timing, phase durations
 * 7. Root Motion Analysis - Analyze/adjust root motion for transitions
 * 8. Contact Prediction - Predict weapon/character contact timing
 * 9. Momentum-Aware Blending - Physics-informed blend weights
 * 10. Animation Time Scaling - Compress/expand timing preserving feel
 * 11. Constraint Satisfaction - Ensure animations meet spatial constraints
 */
UCLASS()
class KATANACOMBAT_API UProceduralAnimationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================================
	// CATEGORY 1: BLEND TIMING
	// Calculate blend durations from context with multi-tier sophistication
	// ============================================================================

	/**
	 * Apply interpolation strategy to normalize progress (0-1) → alpha (0-1).
	 * Core primitive used by all higher-level functions.
	 *
	 * @param Progress Normalized progress (0-1)
	 * @param Strategy Interpolation strategy to apply
	 * @param CustomCurve Optional curve for CustomCurve strategy
	 * @return Interpolated alpha (0-1)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Blend",
	          meta = (DisplayName = "Apply Interpolation Strategy"))
	static float ApplyStrategy(
		float Progress,
		EProceduralStrategy Strategy,
		UCurveFloat* CustomCurve = nullptr);

	/**
	 * Interpolate between two values using strategy.
	 * Progress 0 (animation start) → MaxValue
	 * Progress 1 (animation end) → MinValue
	 *
	 * @param Progress Normalized progress (0-1)
	 * @param MinValue Value at progress=1 (end of animation)
	 * @param MaxValue Value at progress=0 (start of animation)
	 * @param Strategy Interpolation strategy
	 * @param CustomCurve Optional curve for CustomCurve strategy
	 * @return Interpolated value
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Blend",
	          meta = (DisplayName = "Interpolate With Strategy"))
	static float InterpolateWithStrategy(
		float Progress,
		float MinValue,
		float MaxValue,
		EProceduralStrategy Strategy,
		UCurveFloat* CustomCurve = nullptr);

	/**
	 * TIER 1: Progress-Based Blend Calculation (Baseline)
	 *
	 * Uses animation progress to interpolate between derived perceptual bounds.
	 * Always available - requires only source montage position and length.
	 *
	 * Algorithm:
	 *   1. Compute progress = position / length
	 *   2. Compute remaining = length - position
	 *   3. Derive blend bounds from perceptual params (framerate + frame counts)
	 *   4. Scale between bounds based on progress
	 *   5. Clamp to never exceed remaining time
	 *
	 * @param CurrentPosition Current position in source montage (seconds)
	 * @param MontageLength Total length of source montage (seconds)
	 * @param Config Blend configuration with perceptual derivation params
	 * @param bIsRapidInput Was this triggered during an existing blend?
	 * @return FProceduralBlendResult with calculated blend times and debug info
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Blend",
	          meta = (DisplayName = "Calculate Blend (Tier 1 - Progress)"))
	static FProceduralBlendResult CalculateProceduralBlend(
		float CurrentPosition,
		float MontageLength,
		const FProceduralBlendConfig& Config,
		bool bIsRapidInput = false);

	/**
	 * TIER 2: Target-Aware Blend Calculation
	 *
	 * Adds constraint: blend must complete before target's impact frame.
	 * Requires knowing target montage's first Active phase time.
	 *
	 * Algorithm:
	 *   1. Start with Tier 1 calculation
	 *   2. If TargetWindupTime > 0, clamp blend to TargetWindupTime * SafetyFactor
	 *   3. This ensures blend finishes before attack's impact frame
	 *
	 * @param CurrentPosition Current position in source montage (seconds)
	 * @param MontageLength Total length of source montage (seconds)
	 * @param TargetWindupTime Time until target montage's Active phase (seconds), 0 if unknown
	 * @param Config Blend configuration
	 * @param bIsRapidInput Was this triggered during an existing blend?
	 * @return FProceduralBlendResult with calculated blend times
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Blend",
	          meta = (DisplayName = "Calculate Blend (Tier 2 - Target Aware)"))
	static FProceduralBlendResult CalculateProceduralBlendTargetAware(
		float CurrentPosition,
		float MontageLength,
		float TargetWindupTime,
		const FProceduralBlendConfig& Config,
		bool bIsRapidInput = false);

	/**
	 * MULTI-FACTOR: Combined Blend Calculation
	 *
	 * Uses all available tiers with specified chaining mode.
	 * Gracefully degrades when higher-tier data is unavailable.
	 *
	 * Chaining Modes:
	 * - Weighted: Average all available factors with derived weights
	 * - Cascade: Each factor constrains the result (intersection)
	 * - Tiered: Use highest available tier, fallback to simpler
	 * - Adaptive: Intelligently combine based on context
	 *
	 * @param Input Multi-factor input with all available data
	 * @param Config Blend configuration with chain mode
	 * @return FProceduralBlendResult with combined calculation
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Blend",
	          meta = (DisplayName = "Calculate Blend (Multi-Factor)"))
	static FProceduralBlendResult CalculateMultiFactorBlend(
		const FMultiFactorBlendInput& Input,
		const FProceduralBlendConfig& Config);

	/**
	 * Derive blend bounds from target framerate and feel preference.
	 * Uses perceptual research to compute frame-count-based thresholds.
	 *
	 * @param TargetFPS Target framerate (e.g., 60, 30, 120)
	 * @param Preset Desired combat feel preset
	 * @param OutMinBlend Output minimum blend time (seconds)
	 * @param OutMaxBlend Output maximum blend time (seconds)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Blend")
	static void DeriveBlendBoundsFromFramerate(
		float TargetFPS,
		ECombatFeelPreset Preset,
		float& OutMinBlend,
		float& OutMaxBlend);

	/**
	 * Derive blend bounds from montage durations.
	 * Scales bounds proportionally to animation duration.
	 *
	 * Algorithm:
	 *   - Shorter animations → proportionally shorter blend bounds
	 *   - ReferenceLength derived from target framerate and frame counts
	 *   - Clamped to perceptual thresholds
	 *
	 * @param SourceMontageLength Length of source montage (seconds)
	 * @param TargetMontageLength Length of target montage (seconds), 0 if unknown
	 * @param PerceptualParams Perceptual parameters for derivation
	 * @param OutMinBlend Output minimum blend time
	 * @param OutMaxBlend Output maximum blend time
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Blend")
	static void DeriveBlendBoundsFromMontageLengths(
		float SourceMontageLength,
		float TargetMontageLength,
		const FPerceptualDerivationParams& PerceptualParams,
		float& OutMinBlend,
		float& OutMaxBlend);

	// ============================================================================
	// CATEGORY 2: POSE ANALYSIS & MATCHING
	// Compare poses, calculate similarity scores for blend optimization
	// ============================================================================

	/**
	 * Calculate pose similarity between two pose snapshots.
	 * Higher similarity → shorter blend needed.
	 *
	 * Algorithm:
	 *   1. Compare root transforms (position + rotation)
	 *   2. Compare each bone's rotation delta
	 *   3. Weight bones by importance (spine > extremities)
	 *   4. Combine into overall similarity score
	 *
	 * @param SnapshotA First pose snapshot
	 * @param SnapshotB Second pose snapshot
	 * @return FPoseSimilarityResult with detailed comparison
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Pose")
	static FPoseSimilarityResult CalculatePoseSimilarity(
		const FProceduralPoseSnapshot& SnapshotA,
		const FProceduralPoseSnapshot& SnapshotB);

	/**
	 * Calculate recommended blend time from pose similarity.
	 * Maps similarity score to blend time within perceptual bounds.
	 *
	 * @param Similarity Pose similarity result
	 * @param Config Blend config with perceptual params
	 * @return Recommended blend time (seconds)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Pose")
	static float GetBlendTimeFromSimilarity(
		const FPoseSimilarityResult& Similarity,
		const FProceduralBlendConfig& Config);

	/**
	 * Calculate rotation delta between two quaternions (degrees).
	 * Pure math function for bone comparison.
	 *
	 * @param RotationA First rotation
	 * @param RotationB Second rotation
	 * @return Angle difference in degrees
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Pose")
	static float CalculateRotationDelta(
		const FQuat& RotationA,
		const FQuat& RotationB);

	// ============================================================================
	// CATEGORY 3: IK HELPERS
	// Calculate IK targets, validate reachability
	// ============================================================================

	/**
	 * Calculate IK target for effector reaching toward a point.
	 *
	 * Algorithm:
	 *   1. Compute direction from root to target
	 *   2. Check against chain length (reachability)
	 *   3. Compute confidence based on distance/angle constraints
	 *
	 * @param EffectorLocation Current effector position (world space)
	 * @param RootLocation IK chain root position (world space)
	 * @param TargetPoint Desired target position (world space)
	 * @param ChainLength Total length of IK chain (derived from skeleton)
	 * @return FProceduralIKTarget with target info and reachability
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|IK")
	static FProceduralIKTarget CalculateIKTarget(
		const FVector& EffectorLocation,
		const FVector& RootLocation,
		const FVector& TargetPoint,
		float ChainLength);

	/**
	 * Calculate predicted IK target based on velocity.
	 * Projects current position forward by prediction time.
	 *
	 * @param CurrentTarget Current target position
	 * @param Velocity Target velocity (units per second)
	 * @param PredictionTime Time to predict ahead (seconds)
	 * @param ChainLength IK chain length for reachability check
	 * @param RootLocation IK root for reachability check
	 * @return FProceduralIKTarget with predicted position
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|IK")
	static FProceduralIKTarget CalculatePredictedIKTarget(
		const FVector& CurrentTarget,
		const FVector& Velocity,
		float PredictionTime,
		float ChainLength,
		const FVector& RootLocation);

	/**
	 * Calculate contact point IK target between two actors.
	 * Used for paired animations where actors need to meet at a point.
	 *
	 * @param ActorALocation First actor's relevant bone location
	 * @param ActorBLocation Second actor's relevant bone location
	 * @param BlendWeight Weight toward ActorB (0 = at A, 1 = at B)
	 * @param ChainLength IK chain length for reachability
	 * @param RootLocation IK root location
	 * @return FProceduralIKTarget at contact point
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|IK")
	static FProceduralIKTarget CalculateContactPointIKTarget(
		const FVector& ActorALocation,
		const FVector& ActorBLocation,
		float BlendWeight,
		float ChainLength,
		const FVector& RootLocation);

	// ============================================================================
	// CATEGORY 4: POST-BLEND HEALING
	// Fix pose artifacts after blend transitions
	// ============================================================================

	/**
	 * Calculate pose healing correction for a single bone.
	 * Applies specified strategy to move bone toward reference.
	 *
	 * @param CurrentTransform Current bone transform
	 * @param ReferenceTransform Reference/target transform
	 * @param Config Healing configuration
	 * @param DeltaTime Frame delta time (for spring/velocity)
	 * @param PreviousVelocity Previous frame's velocity (for spring damping)
	 * @param OutCorrectedTransform Output corrected transform
	 * @param OutCorrectionAmount Output amount of correction applied
	 * @return True if correction was applied
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Healing")
	static bool CalculateBoneHealingCorrection(
		const FTransform& CurrentTransform,
		const FTransform& ReferenceTransform,
		const FPoseHealingConfig& Config,
		float DeltaTime,
		const FVector& PreviousVelocity,
		FTransform& OutCorrectedTransform,
		float& OutCorrectionAmount);

	/**
	 * Calculate spring-based correction for physics-style healing.
	 * Uses Hooke's law: F = -kx - cv
	 *
	 * @param CurrentPosition Current position
	 * @param TargetPosition Target/reference position
	 * @param CurrentVelocity Current velocity
	 * @param Stiffness Spring stiffness (k)
	 * @param Damping Damping coefficient (c)
	 * @param DeltaTime Frame delta time
	 * @param OutNewPosition Output corrected position
	 * @param OutNewVelocity Output new velocity
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Healing")
	static void CalculateSpringCorrection(
		const FVector& CurrentPosition,
		const FVector& TargetPosition,
		const FVector& CurrentVelocity,
		float Stiffness,
		float Damping,
		float DeltaTime,
		FVector& OutNewPosition,
		FVector& OutNewVelocity);

	// ============================================================================
	// CATEGORY 5: VELOCITY & MOTION ANALYSIS
	// Analyze bone/character velocities for momentum-aware blending
	// ============================================================================

	/**
	 * Calculate velocity analysis from two pose snapshots.
	 * Derives velocity by comparing bone positions over time delta.
	 *
	 * @param PreviousSnapshot Earlier pose snapshot
	 * @param CurrentSnapshot Later pose snapshot
	 * @param TimeDelta Time between snapshots (seconds)
	 * @return FVelocityAnalysisResult with velocity metrics
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Velocity")
	static FVelocityAnalysisResult CalculateVelocityFromSnapshots(
		const FProceduralPoseSnapshot& PreviousSnapshot,
		const FProceduralPoseSnapshot& CurrentSnapshot,
		float TimeDelta);

	/**
	 * Calculate kinetic energy estimate from velocity analysis.
	 * E = 0.5 * m * v^2, mass derived from skeleton bounds.
	 *
	 * @param Velocity Velocity analysis result
	 * @param EstimatedMass Estimated character mass (kg)
	 * @return Kinetic energy estimate (joules)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Velocity")
	static float CalculateKineticEnergy(
		const FVelocityAnalysisResult& Velocity,
		float EstimatedMass);

	/**
	 * Get momentum-adjusted blend time.
	 * Higher momentum → longer blend to avoid jarring stop.
	 *
	 * Algorithm:
	 *   - Calculate kinetic energy from velocity
	 *   - Map energy to blend time within perceptual bounds
	 *   - Higher energy = longer blend (dissipate momentum)
	 *
	 * @param Velocity Velocity analysis
	 * @param Config Blend config with perceptual params
	 * @param ReferenceEnergy Reference energy for 1.0 blend (derived from config)
	 * @return Momentum-adjusted blend time (seconds)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Velocity")
	static float GetMomentumAdjustedBlendTime(
		const FVelocityAnalysisResult& Velocity,
		const FProceduralBlendConfig& Config,
		float ReferenceEnergy = 0.0f);

	// ============================================================================
	// CATEGORY 6: ANIMATION TIMING PREDICTION
	// Predict event timing, phase durations
	// ============================================================================

	/**
	 * Calculate procedural timing value from normalized input.
	 * Generic function usable for any timing-based procedural system.
	 *
	 * @param InputValue Normalized input (0-1)
	 * @param Config Timing configuration with perceptual params
	 * @return Calculated duration in seconds
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Timing",
	          meta = (DisplayName = "Calculate Procedural Timing"))
	static float CalculateProceduralTiming(
		float InputValue,
		const FProceduralTimingConfig& Config);

	/**
	 * Predict time to reach a specific animation position.
	 * Accounts for current play rate.
	 *
	 * @param CurrentPosition Current position (seconds)
	 * @param TargetPosition Target position (seconds)
	 * @param PlayRate Current play rate multiplier
	 * @return Time to reach target (seconds), negative if already past
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Timing")
	static float PredictTimeToPosition(
		float CurrentPosition,
		float TargetPosition,
		float PlayRate);

	/**
	 * Calculate optimal interrupt time for seamless transition.
	 * Finds position where blend will complete exactly at target arrival.
	 *
	 * @param CurrentPosition Current animation position
	 * @param AnimationLength Total animation length
	 * @param BlendDuration Expected blend duration
	 * @param PlayRate Current play rate
	 * @return Optimal interrupt position (seconds)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Timing")
	static float CalculateOptimalInterruptTime(
		float CurrentPosition,
		float AnimationLength,
		float BlendDuration,
		float PlayRate);

	// ============================================================================
	// CATEGORY 7: ROOT MOTION ANALYSIS
	// Analyze/adjust root motion for transitions
	// ============================================================================

	/**
	 * Analyze root motion over a time window.
	 * Extracts velocity, peak speed, dominant axis.
	 *
	 * @param RootTransforms Array of root transforms over time
	 * @param TimeStamps Corresponding timestamps for each transform
	 * @return FRootMotionAnalysisResult with motion metrics
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|RootMotion")
	static FRootMotionAnalysisResult AnalyzeRootMotion(
		const TArray<FTransform>& RootTransforms,
		const TArray<float>& TimeStamps);

	/**
	 * Calculate root motion blend weight based on motion analysis.
	 * High motion → preserve more root motion, low motion → blend out.
	 *
	 * @param Analysis Root motion analysis result
	 * @param SignificanceThreshold Speed threshold for "significant" motion
	 * @return Blend weight (0 = ignore root motion, 1 = full root motion)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|RootMotion")
	static float CalculateRootMotionBlendWeight(
		const FRootMotionAnalysisResult& Analysis,
		float SignificanceThreshold);

	// ============================================================================
	// CATEGORY 8: CONTACT PREDICTION
	// Predict weapon/character contact timing
	// ============================================================================

	/**
	 * Predict contact between two moving points.
	 * Uses linear trajectory intersection.
	 *
	 * @param PositionA First point position
	 * @param VelocityA First point velocity
	 * @param PositionB Second point position
	 * @param VelocityB Second point velocity
	 * @param ContactRadius Radius for contact detection
	 * @param MaxPredictionTime Maximum time to predict ahead
	 * @return FContactPredictionResult with contact info
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Contact")
	static FContactPredictionResult PredictLinearContact(
		const FVector& PositionA,
		const FVector& VelocityA,
		const FVector& PositionB,
		const FVector& VelocityB,
		float ContactRadius,
		float MaxPredictionTime);

	/**
	 * Calculate contact-aware blend timing.
	 * Ensures blend completes before predicted contact.
	 *
	 * @param ContactPrediction Contact prediction result
	 * @param Config Blend configuration
	 * @param SafetyMargin Extra time before contact to complete blend
	 * @return Adjusted blend time (seconds)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Contact")
	static float GetContactAwareBlendTime(
		const FContactPredictionResult& ContactPrediction,
		const FProceduralBlendConfig& Config,
		float SafetyMargin);

	// ============================================================================
	// CATEGORY 9: MOMENTUM-AWARE BLENDING
	// Physics-informed blend weights
	// ============================================================================

	/**
	 * Calculate momentum-preserving blend curve.
	 * Adjusts blend profile to conserve perceived momentum.
	 *
	 * @param IncomingMomentum Momentum from source animation
	 * @param OutgoingMomentum Momentum in target animation
	 * @param BlendProgress Current blend progress (0-1)
	 * @return Adjusted blend alpha that preserves momentum feel
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Momentum")
	static float CalculateMomentumPreservingBlend(
		const FVector& IncomingMomentum,
		const FVector& OutgoingMomentum,
		float BlendProgress);

	/**
	 * Get momentum-derived blend strategy.
	 * Recommends best strategy based on momentum difference.
	 *
	 * @param IncomingSpeed Incoming animation's peak speed
	 * @param OutgoingSpeed Target animation's initial speed
	 * @return Recommended interpolation strategy
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Momentum")
	static EProceduralStrategy GetMomentumDerivedStrategy(
		float IncomingSpeed,
		float OutgoingSpeed);

	// ============================================================================
	// CATEGORY 10: ANIMATION TIME SCALING
	// Compress/expand timing while preserving feel
	// ============================================================================

	/**
	 * Calculate play rate to fit animation within target duration.
	 * Clamps to perceptual limits to preserve feel.
	 *
	 * @param OriginalDuration Original animation duration
	 * @param TargetDuration Desired duration
	 * @param MinPlayRate Minimum allowed play rate (0.5 default)
	 * @param MaxPlayRate Maximum allowed play rate (2.0 default)
	 * @return FTimeScalingResult with play rate and actual duration
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|TimeScale")
	static FTimeScalingResult CalculatePlayRateForDuration(
		float OriginalDuration,
		float TargetDuration,
		float MinPlayRate = 0.5f,
		float MaxPlayRate = 2.0f);

	/**
	 * Calculate perceptually-valid play rate bounds.
	 * Derives limits from frame rate and feel preset.
	 *
	 * @param PerceptualParams Perceptual parameters
	 * @param OutMinRate Output minimum play rate
	 * @param OutMaxRate Output maximum play rate
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|TimeScale")
	static void DerivePlayRateBounds(
		const FPerceptualDerivationParams& PerceptualParams,
		float& OutMinRate,
		float& OutMaxRate);

	// ============================================================================
	// CATEGORY 11: CONSTRAINT SATISFACTION
	// Ensure animations meet spatial constraints
	// ============================================================================

	/**
	 * Check if animation endpoint satisfies spatial constraints.
	 *
	 * @param Position Position to check
	 * @param Constraints Array of spatial constraints
	 * @return FConstraintSatisfactionResult with detailed check
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Constraint")
	static FConstraintSatisfactionResult CheckConstraintSatisfaction(
		const FVector& Position,
		const TArray<FSpatialConstraint>& Constraints);

	/**
	 * Calculate suggested position to satisfy constraints.
	 * Finds nearest valid position if current is invalid.
	 *
	 * @param CurrentPosition Current position (potentially invalid)
	 * @param Constraints Spatial constraints to satisfy
	 * @return Suggested valid position
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Constraint")
	static FVector CalculateConstraintSatisfyingPosition(
		const FVector& CurrentPosition,
		const TArray<FSpatialConstraint>& Constraints);

	/**
	 * Get constraint-adjusted blend time.
	 * Longer blend if position correction needed.
	 *
	 * @param ConstraintResult Constraint satisfaction result
	 * @param BaseBlendTime Base blend time before adjustment
	 * @param CorrectionDistance Distance to valid position
	 * @param CorrectionSpeed Speed at which to correct (units/sec)
	 * @return Adjusted blend time accounting for correction
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Constraint")
	static float GetConstraintAdjustedBlendTime(
		const FConstraintSatisfactionResult& ConstraintResult,
		float BaseBlendTime,
		float CorrectionDistance,
		float CorrectionSpeed);

	// ============================================================================
	// EASING FUNCTIONS
	// Mathematical easing - no magic numbers, just math
	// ============================================================================

	/** Linear: f(t) = t */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Easing")
	static float EaseLinear(float T) { return T; }

	/** Quadratic ease-in: f(t) = t² (slow start, fast end) */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Easing")
	static float EaseInQuad(float T) { return T * T; }

	/** Quadratic ease-out: f(t) = 1-(1-t)² (fast start, slow end) */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Easing")
	static float EaseOutQuad(float T) { return 1.0f - (1.0f - T) * (1.0f - T); }

	/** Cubic ease-in-out: smooth S-curve */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Easing")
	static float EaseInOutCubic(float T)
	{
		return T < 0.5f
			? 4.0f * T * T * T
			: 1.0f - FMath::Pow(-2.0f * T + 2.0f, 3.0f) / 2.0f;
	}

	/** Exponential ease-out: f(t) = 1 - 2^(-10t) (very fast start) */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Easing")
	static float EaseOutExpo(float T)
	{
		return T >= 1.0f ? 1.0f : 1.0f - FMath::Pow(2.0f, -10.0f * T);
	}

	/** Sine ease-out: f(t) = sin(t * π/2) (gentle deceleration) */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Easing")
	static float EaseOutSine(float T)
	{
		return FMath::Sin(T * PI / 2.0f);
	}

	/** Back ease-out: slight overshoot then settle (standard overshoot constant 1.70158) */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Easing")
	static float EaseOutBack(float T)
	{
		// Standard overshoot constant from Robert Penner's easing equations
		// c1 ≈ 1.70158 produces ~10% overshoot, derived from aesthetic testing
		const float C1 = 1.70158f;
		const float C3 = C1 + 1.0f;
		return 1.0f + C3 * FMath::Pow(T - 1.0f, 3.0f) + C1 * FMath::Pow(T - 1.0f, 2.0f);
	}

	// ============================================================================
	// CATEGORY 12: CONTROL RIG PARAMETER HELPERS
	// Calculate parameters for UE5 Control Rig integration
	// ============================================================================

	/**
	 * Calculate look-at parameters for Control Rig.
	 * Derives head, chest, and eye rotations from target.
	 *
	 * @param HeadLocation Current head bone world location
	 * @param HeadForward Current head forward direction
	 * @param Target Look-at target configuration
	 * @return FLookAtResult with calculated rotations
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|ControlRig")
	static FLookAtResult CalculateLookAtParams(
		const FVector& HeadLocation,
		const FVector& HeadForward,
		const FProceduralLookAtTarget& Target);

	/**
	 * Calculate aim offset parameters for Control Rig.
	 * Returns yaw/pitch blend values for aim offset blendspace.
	 *
	 * @param ActorForward Actor's forward direction
	 * @param AimDirection Desired aim direction
	 * @param MaxYaw Maximum yaw angle (degrees)
	 * @param MaxPitch Maximum pitch angle (degrees)
	 * @param OutYaw Output yaw blend value (-1 to 1)
	 * @param OutPitch Output pitch blend value (-1 to 1)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|ControlRig")
	static void CalculateAimOffsetParams(
		const FVector& ActorForward,
		const FVector& AimDirection,
		float MaxYaw,
		float MaxPitch,
		float& OutYaw,
		float& OutPitch);

	/**
	 * Calculate hand IK parameters for weapon gripping.
	 * Adjusts hand position/rotation for different weapon holds.
	 *
	 * @param WeaponGripTransform Weapon's grip socket transform
	 * @param CurrentHandTransform Current hand bone transform
	 * @param ChainLength Arm chain length for reach validation
	 * @return FControlRigParam array with IK parameters
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|ControlRig")
	static TArray<FControlRigParam> CalculateWeaponGripParams(
		const FTransform& WeaponGripTransform,
		const FTransform& CurrentHandTransform,
		float ChainLength);

	/**
	 * Calculate foot IK parameters for ground adaptation.
	 * Returns IK targets and blend weights for each foot.
	 *
	 * @param FootLocation Current foot bone location
	 * @param GroundHitLocation Ground trace hit location
	 * @param GroundNormal Ground surface normal
	 * @param MaxAdjustment Maximum Z adjustment allowed
	 * @param OutIKTarget Output IK target position
	 * @param OutRotationOffset Output rotation offset for slope adaptation
	 * @param OutBlendWeight Output blend weight based on validity
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|ControlRig")
	static void CalculateFootIKParams(
		const FVector& FootLocation,
		const FVector& GroundHitLocation,
		const FVector& GroundNormal,
		float MaxAdjustment,
		FVector& OutIKTarget,
		FRotator& OutRotationOffset,
		float& OutBlendWeight);

	/**
	 * Calculate spine twist parameters for torso rotation.
	 * Distributes rotation across spine bones for natural twist.
	 *
	 * @param DesiredRotation Total rotation to apply (degrees)
	 * @param SpineBoneCount Number of spine bones to distribute across
	 * @param TwistDistribution Distribution curve (0=lower spine, 1=upper spine)
	 * @return Array of rotation offsets per bone (bottom to top)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|ControlRig")
	static TArray<float> CalculateSpineTwistDistribution(
		float DesiredRotation,
		int32 SpineBoneCount,
		UCurveFloat* TwistDistribution = nullptr);

	// ============================================================================
	// CATEGORY 13: ANIMATION SELECTION & MATCHING
	// Score and select animations based on gameplay context
	// ============================================================================

	/**
	 * Score a single animation candidate against gameplay context.
	 * Returns normalized score (0-1) based on how well candidate matches context.
	 *
	 * Scoring Factors:
	 * - Distance match: How close target is to ideal range
	 * - Angle match: How aligned with ideal angle
	 * - Input match: Does input direction match requirements
	 * - State match: Stamina, combo position, etc.
	 *
	 * @param Candidate Animation candidate to score
	 * @param Context Current gameplay context
	 * @return Score (0-1), higher is better match
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Selection")
	static float ScoreAnimationCandidate(
		const FAnimationCandidate& Candidate,
		const FAnimationSelectionContext& Context);

	/**
	 * Select best animation from candidates based on context.
	 * Scores all candidates and returns the best match.
	 *
	 * @param Candidates Array of animation candidates
	 * @param Context Current gameplay context
	 * @return FAnimationSelectionResult with selected animation
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Selection")
	static FAnimationSelectionResult SelectBestAnimation(
		const TArray<FAnimationCandidate>& Candidates,
		const FAnimationSelectionContext& Context);

	/**
	 * Get directional animation based on input.
	 * Maps 8-way input to appropriate animation index.
	 *
	 * @param InputDirection Input direction vector
	 * @param FacingDirection Character's facing direction
	 * @return Directional index (0=forward, 1=forward-right, 2=right, etc., -1=neutral)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Selection")
	static int32 GetDirectionalAnimationIndex(
		const FVector& InputDirection,
		const FVector& FacingDirection);

	/**
	 * Calculate gap closer requirement.
	 * Determines if a gap-closing animation is appropriate.
	 *
	 * @param DistanceToTarget Current distance to target
	 * @param CharacterSpeed Character's current movement speed
	 * @param AttackRange Maximum attack range
	 * @return Gap to close (positive = need gap closer, 0 = in range)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Selection")
	static float CalculateGapCloserNeed(
		float DistanceToTarget,
		float CharacterSpeed,
		float AttackRange);

	/**
	 * Get combo position weight modifier.
	 * Adjusts animation priority based on combo state.
	 *
	 * @param CurrentComboCount Current combo position
	 * @param TimeSinceLastAttack Time since last attack completed
	 * @param ComboWindowDuration Duration of combo window
	 * @return Weight modifier (1.0 = neutral, >1 = prioritize, <1 = deprioritize)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Selection")
	static float GetComboPositionWeight(
		int32 CurrentComboCount,
		float TimeSinceLastAttack,
		float ComboWindowDuration);

	// ============================================================================
	// CATEGORY 14: LAYERED BLENDING UTILITIES
	// Per-bone blending and additive pose management
	// ============================================================================

	/**
	 * Calculate per-bone blend weights from configuration.
	 * Expands bone hierarchy to get weights for all affected bones.
	 *
	 * @param Config Layered blend configuration
	 * @param AllBoneNames All bone names in skeleton (ordered by hierarchy)
	 * @param BoneParentIndices Parent index for each bone (-1 = root)
	 * @return Array of blend weights indexed by bone index
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|LayeredBlend")
	static TArray<float> CalculateLayeredBlendWeights(
		const FLayeredBlendConfig& Config,
		const TArray<FName>& AllBoneNames,
		const TArray<int32>& BoneParentIndices);

	/**
	 * Calculate additive pose blend factor.
	 * Determines how much additive pose to apply based on context.
	 *
	 * Algorithm:
	 *   - Base weight from configuration
	 *   - Modulated by velocity (faster = less additive)
	 *   - Modulated by animation progress (fade in/out)
	 *
	 * @param BaseWeight Configuration base weight
	 * @param Velocity Current character velocity
	 * @param AnimProgress Current animation progress (0-1)
	 * @param FadeInDuration Fade in duration (normalized)
	 * @param FadeOutDuration Fade out duration (normalized)
	 * @return Modulated blend weight
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|LayeredBlend")
	static float CalculateAdditiveBlendWeight(
		float BaseWeight,
		const FVector& Velocity,
		float AnimProgress,
		float FadeInDuration = 0.1f,
		float FadeOutDuration = 0.1f);

	/**
	 * Calculate masked blend alpha for smooth transitions.
	 * Uses bone mask to create smooth spatial falloff.
	 *
	 * @param BoneIndex Current bone index
	 * @param MaskRootBoneIndex Root of masked region
	 * @param HierarchyDepth Depth of this bone from mask root
	 * @param FalloffDepth How many levels to fade over
	 * @return Blend alpha for this bone (0-1)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|LayeredBlend")
	static float CalculateMaskedBlendAlpha(
		int32 BoneIndex,
		int32 MaskRootBoneIndex,
		int32 HierarchyDepth,
		int32 FalloffDepth);

	// ============================================================================
	// CATEGORY 15: ANIMATION WARPING HELPERS
	// Procedural warp target calculation
	// ============================================================================

	/**
	 * Calculate procedural warp target for attack animations.
	 * Determines optimal position/rotation to reach target.
	 *
	 * @param AttackerLocation Current attacker location
	 * @param AttackerForward Attacker facing direction
	 * @param TargetLocation Target location
	 * @param IdealAttackDistance Ideal distance for attack
	 * @param MaxWarpDistance Maximum allowed warp distance
	 * @return FProceduralWarpTarget with warp parameters
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Warp")
	static FProceduralWarpTarget CalculateAttackWarpTarget(
		const FVector& AttackerLocation,
		const FVector& AttackerForward,
		const FVector& TargetLocation,
		float IdealAttackDistance,
		float MaxWarpDistance);

	/**
	 * Calculate distance-matched animation start position.
	 * Finds optimal start frame to cover exact distance.
	 *
	 * @param CurrentDistance Current distance to target
	 * @param Config Distance match configuration
	 * @return FDistanceMatchResult with start position and play rate
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Warp")
	static FDistanceMatchResult CalculateDistanceMatch(
		float CurrentDistance,
		const FDistanceMatchConfig& Config);

	/**
	 * Calculate orientation warp parameters.
	 * Determines rotation adjustment to face target during animation.
	 *
	 * @param CurrentRotation Current actor rotation
	 * @param TargetDirection Direction to target
	 * @param MaxWarpAngle Maximum warp angle (degrees)
	 * @param WarpCurve Optional curve for non-linear warp
	 * @return Rotation offset to apply
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Warp")
	static FRotator CalculateOrientationWarp(
		const FRotator& CurrentRotation,
		const FVector& TargetDirection,
		float MaxWarpAngle,
		UCurveFloat* WarpCurve = nullptr);

	/**
	 * Calculate stride warp scale for locomotion.
	 * Adjusts stride length to match desired speed.
	 *
	 * @param DesiredSpeed Desired movement speed
	 * @param AnimationBaseSpeed Animation's base movement speed
	 * @param MinScale Minimum stride scale allowed
	 * @param MaxScale Maximum stride scale allowed
	 * @return Stride scale multiplier
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Warp")
	static float CalculateStrideWarpScale(
		float DesiredSpeed,
		float AnimationBaseSpeed,
		float MinScale = 0.8f,
		float MaxScale = 1.2f);

	// ============================================================================
	// CATEGORY 16: GAMEPLAY CONTEXT MAPPING
	// Map gameplay state to animation parameters
	// ============================================================================

	/**
	 * Map input direction to animation quadrant.
	 * Converts input vector to discrete quadrant for animation selection.
	 *
	 * @param InputDirection Raw input direction
	 * @param FacingDirection Character facing direction
	 * @param DeadZone Input magnitude below which is neutral
	 * @return Quadrant index: 0=neutral, 1=forward, 2=back, 3=left, 4=right
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Context")
	static int32 MapInputToQuadrant(
		const FVector& InputDirection,
		const FVector& FacingDirection,
		float DeadZone = 0.1f);

	/**
	 * Calculate attack intensity from input.
	 * Determines attack "power" based on hold time and input.
	 *
	 * @param HoldDuration How long input was held
	 * @param MinHoldForHeavy Minimum hold for heavy attack
	 * @param MaxHoldForCharged Maximum hold for charged attack
	 * @return Intensity (0=light, 0.5=heavy, 1=charged)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Context")
	static float CalculateAttackIntensity(
		float HoldDuration,
		float MinHoldForHeavy,
		float MaxHoldForCharged);

	/**
	 * Calculate defensive stance blend.
	 * Determines how much to blend defensive pose based on threat.
	 *
	 * @param ThreatDistance Distance to nearest threat
	 * @param ThreatCount Number of active threats
	 * @param MaxBlendDistance Distance at which blend starts
	 * @param MinBlendDistance Distance at which blend is full
	 * @return Defensive stance blend weight (0-1)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Context")
	static float CalculateDefensiveStanceBlend(
		float ThreatDistance,
		int32 ThreatCount,
		float MaxBlendDistance,
		float MinBlendDistance);

	/**
	 * Map velocity to locomotion blend space.
	 * Converts velocity to animation blendspace coordinates.
	 *
	 * @param Velocity Current velocity
	 * @param FacingDirection Character facing direction
	 * @param MaxSpeed Maximum expected speed
	 * @param OutForward Forward component (-1 to 1)
	 * @param OutRight Right component (-1 to 1)
	 * @param OutSpeed Speed component (0 to 1)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Context")
	static void MapVelocityToBlendSpace(
		const FVector& Velocity,
		const FVector& FacingDirection,
		float MaxSpeed,
		float& OutForward,
		float& OutRight,
		float& OutSpeed);

	/**
	 * Calculate hit reaction direction.
	 * Maps damage direction to hit reaction animation selection.
	 *
	 * @param DamageDirection Direction damage came from
	 * @param VictimForward Victim's facing direction
	 * @return Reaction direction index (0=front, 1=back, 2=left, 3=right)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Context")
	static int32 CalculateHitReactionDirection(
		const FVector& DamageDirection,
		const FVector& VictimForward);

	// ============================================================================
	// UTILITY FUNCTIONS
	// ============================================================================

	/**
	 * Get normalized animation progress from position and length.
	 * Handles edge cases (zero length, negative values).
	 *
	 * @param Position Current position (seconds)
	 * @param Length Total length (seconds)
	 * @return Normalized progress [0,1]
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Utility")
	static float GetNormalizedProgress(float Position, float Length);

	/**
	 * Compute remaining time in montage.
	 *
	 * @param CurrentPosition Current position (seconds)
	 * @param MontageLength Total length (seconds)
	 * @return Remaining time (seconds), minimum 0
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Utility")
	static float GetRemainingTime(float CurrentPosition, float MontageLength);

	/**
	 * Get human-readable name for strategy enum.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Utility")
	static FString GetStrategyDisplayName(EProceduralStrategy Strategy);

	/**
	 * Get human-readable name for chain mode enum.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Utility")
	static FString GetChainModeDisplayName(EMultiFactorChainMode ChainMode);
};
