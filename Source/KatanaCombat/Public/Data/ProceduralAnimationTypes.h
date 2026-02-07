// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralAnimationTypes.generated.h"

// Forward declarations
class UCurveFloat;

// ============================================================================
// INTERPOLATION STRATEGIES
// ============================================================================

/**
 * Strategy for calculating procedural values.
 * Different algorithms for different use cases.
 */
UENUM(BlueprintType)
enum class EProceduralStrategy : uint8
{
	/** Linear interpolation (Progress → MinBlend to MaxBlend) */
	Linear                  UMETA(DisplayName = "Linear"),

	/** Quadratic ease-out (fast start, slow end) */
	EaseOut                 UMETA(DisplayName = "Ease Out (Quadratic)"),

	/** Quadratic ease-in (slow start, fast end) */
	EaseIn                  UMETA(DisplayName = "Ease In (Quadratic)"),

	/** Cubic ease-in-out (smooth S-curve) */
	EaseInOut               UMETA(DisplayName = "Ease In-Out (Cubic)"),

	/** Step function (instant at threshold) */
	Step                    UMETA(DisplayName = "Step (Threshold-based)"),

	/** Custom curve (requires UCurveFloat reference) */
	CustomCurve             UMETA(DisplayName = "Custom Curve")
};

/**
 * Mode for blend behavior during rapid input.
 * Controls how system handles mashing during transitions.
 */
UENUM(BlueprintType)
enum class ERapidInputBlendMode : uint8
{
	/** Force instant blend (clear everything, start fresh) */
	ForceInstant            UMETA(DisplayName = "Force Instant"),

	/** Continue current blend (ignore rapid input) */
	ContinueCurrent         UMETA(DisplayName = "Continue Current"),

	/** Queue for later execution */
	QueueUntilComplete      UMETA(DisplayName = "Queue Until Complete"),

	/** Blend faster than normal (accelerate) */
	Accelerate              UMETA(DisplayName = "Accelerate Blend")
};

/**
 * Combat feel preset derived from perceptual research.
 * Each preset maps to a range of frame counts at reference framerate.
 */
UENUM(BlueprintType)
enum class ECombatFeelPreset : uint8
{
	/** 2-3 frames: Perceived as instantaneous/popping */
	Instant                 UMETA(DisplayName = "Instant (2-3 frames)"),

	/** 3-6 frames: Very responsive, minimal visual blend */
	UltraSnappy             UMETA(DisplayName = "Ultra Snappy (3-6 frames)"),

	/** 6-9 frames: Snappy, good for reactive combat */
	Snappy                  UMETA(DisplayName = "Snappy (6-9 frames)"),

	/** 9-15 frames: Balanced, smooth transitions with responsiveness */
	Balanced                UMETA(DisplayName = "Balanced (9-15 frames)"),

	/** 15-18 frames: Smooth, flowing combat feel */
	Smooth                  UMETA(DisplayName = "Smooth (15-18 frames)"),

	/** 18+ frames: Cinematic, deliberate transitions */
	Cinematic               UMETA(DisplayName = "Cinematic (18+ frames)")
};

/**
 * Multi-factor chaining mode for combined calculations.
 * Determines how multiple factors are combined.
 */
UENUM(BlueprintType)
enum class EMultiFactorChainMode : uint8
{
	/** Weighted average of all factors */
	WeightedCombination     UMETA(DisplayName = "Weighted Combination"),

	/** Each factor constrains the result (intersection) */
	ConstraintCascade       UMETA(DisplayName = "Constraint Cascade"),

	/** Use most sophisticated available factor, fallback to simpler */
	TieredFallback          UMETA(DisplayName = "Tiered Fallback"),

	/** Combine all three approaches intelligently based on context */
	Adaptive                UMETA(DisplayName = "Adaptive (All Methods)")
};

/**
 * IK target type for procedural IK helpers.
 */
UENUM(BlueprintType)
enum class EIKTargetType : uint8
{
	/** World-space position target */
	WorldPosition           UMETA(DisplayName = "World Position"),

	/** Relative to another bone */
	BoneRelative            UMETA(DisplayName = "Bone Relative"),

	/** Predicted future position based on velocity */
	PredictedPosition       UMETA(DisplayName = "Predicted Position"),

	/** Contact point between two actors */
	ContactPoint            UMETA(DisplayName = "Contact Point")
};

/**
 * Pose healing strategy for fixing blend artifacts.
 */
UENUM(BlueprintType)
enum class EPoseHealingStrategy : uint8
{
	/** Interpolate toward reference pose */
	InterpolateToReference  UMETA(DisplayName = "Interpolate To Reference"),

	/** Apply physics-based spring correction */
	SpringCorrection        UMETA(DisplayName = "Spring Correction"),

	/** Blend toward last valid pose snapshot */
	SnapshotBlend           UMETA(DisplayName = "Snapshot Blend"),

	/** Use constraint-based pose solver */
	ConstraintSolver        UMETA(DisplayName = "Constraint Solver")
};

// ============================================================================
// PERCEPTUAL DERIVATION PARAMETERS
// ============================================================================

/**
 * Parameters for deriving perceptually-correct timing values.
 * All timing constants are derived from these inputs - NO MAGIC NUMBERS.
 *
 * Derivation Formula:
 *   FrameDuration = 1.0 / TargetFramerate
 *   MinBlend = FrameDuration * MinFrameCount
 *   MaxBlend = FrameDuration * MaxFrameCount
 *
 * Frame counts come from human visual perception research:
 * - Below 2-3 frames: perceived as instantaneous/popping
 * - 3-9 frames: perceived as "snappy" - good for reactive combat
 * - 9-18 frames: perceived as "smooth" - good for flowing combat
 * - Above 18 frames: perceived as "sluggish" - loses responsiveness
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FPerceptualDerivationParams
{
	GENERATED_BODY()

	/** Target framerate for frame-to-time conversion (e.g., 60, 30, 120) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Derivation",
	          meta = (ClampMin = "24.0", ClampMax = "240.0"))
	float TargetFramerate = 60.0f;

	/** Minimum frame count for blend (perceptual instant threshold) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Derivation",
	          meta = (ClampMin = "1", ClampMax = "30"))
	int32 MinFrameCount = 3;

	/** Maximum frame count for blend (perceptual smoothness upper bound) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Derivation",
	          meta = (ClampMin = "1", ClampMax = "60"))
	int32 MaxFrameCount = 12;

	/** Default constructor with 60fps balanced values */
	FPerceptualDerivationParams()
		: TargetFramerate(60.0f)
		, MinFrameCount(3)
		, MaxFrameCount(12)
	{
	}

	/** Construct from framerate and feel preset */
	static FPerceptualDerivationParams FromPreset(float Framerate, ECombatFeelPreset Preset)
	{
		FPerceptualDerivationParams Params;
		Params.TargetFramerate = Framerate;

		// Frame ranges derived from perceptual research on animation discontinuities
		switch (Preset)
		{
		case ECombatFeelPreset::Instant:
			Params.MinFrameCount = 2;
			Params.MaxFrameCount = 3;
			break;
		case ECombatFeelPreset::UltraSnappy:
			Params.MinFrameCount = 3;
			Params.MaxFrameCount = 6;
			break;
		case ECombatFeelPreset::Snappy:
			Params.MinFrameCount = 6;
			Params.MaxFrameCount = 9;
			break;
		case ECombatFeelPreset::Balanced:
			Params.MinFrameCount = 9;
			Params.MaxFrameCount = 15;
			break;
		case ECombatFeelPreset::Smooth:
			Params.MinFrameCount = 15;
			Params.MaxFrameCount = 18;
			break;
		case ECombatFeelPreset::Cinematic:
			Params.MinFrameCount = 18;
			Params.MaxFrameCount = 30;
			break;
		}

		return Params;
	}

	/** Convert frame count to seconds at target framerate */
	FORCEINLINE float FramesToSeconds(int32 Frames) const
	{
		return (TargetFramerate > 0.0f) ? (static_cast<float>(Frames) / TargetFramerate) : 0.0f;
	}

	/** Convert seconds to frame count at target framerate */
	FORCEINLINE int32 SecondsToFrames(float Seconds) const
	{
		return (TargetFramerate > 0.0f) ? FMath::RoundToInt(Seconds * TargetFramerate) : 0;
	}

	/** Get derived minimum blend time in seconds */
	FORCEINLINE float GetMinBlendTime() const
	{
		return FramesToSeconds(MinFrameCount);
	}

	/** Get derived maximum blend time in seconds */
	FORCEINLINE float GetMaxBlendTime() const
	{
		return FramesToSeconds(MaxFrameCount);
	}

	/** Get single frame duration in seconds */
	FORCEINLINE float GetFrameDuration() const
	{
		return (TargetFramerate > 0.0f) ? (1.0f / TargetFramerate) : 0.0f;
	}
};

// ============================================================================
// PROCEDURAL BLEND CONFIGURATION
// ============================================================================

/**
 * Configuration for procedural blend time calculation.
 * All timing values are DERIVED from perceptual parameters - no magic numbers.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FProceduralBlendConfig
{
	GENERATED_BODY()

	/** Strategy for calculating blend time from animation progress */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Strategy")
	EProceduralStrategy Strategy = EProceduralStrategy::Linear;

	/** How to handle rapid input during blend transitions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Strategy")
	ERapidInputBlendMode RapidInputMode = ERapidInputBlendMode::ForceInstant;

	/** How to combine multiple factors when using multi-factor calculation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Strategy")
	EMultiFactorChainMode ChainMode = EMultiFactorChainMode::TieredFallback;

	/** Perceptual parameters for deriving timing values */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Derivation")
	FPerceptualDerivationParams PerceptualParams;

	/**
	 * Minimum blend time in seconds (derived from PerceptualParams.MinFrameCount).
	 * Can be overridden manually if needed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Timing",
	          meta = (ClampMin = "0.001", ClampMax = "2.0"))
	float MinBlendTime = 0.0f;  // 0 = use derived value

	/**
	 * Maximum blend time in seconds (derived from PerceptualParams.MaxFrameCount).
	 * Can be overridden manually if needed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Timing",
	          meta = (ClampMin = "0.001", ClampMax = "2.0"))
	float MaxBlendTime = 0.0f;  // 0 = use derived value

	/** Progress threshold for instant blend (Step strategy) - derived from remaining time ratio */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Timing",
	          meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "Strategy == EProceduralStrategy::Step"))
	float InstantBlendThreshold = 0.0f;  // 0 = derive from remaining time

	/** Custom blend curve (CustomCurve strategy only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Curve",
	          meta = (EditCondition = "Strategy == EProceduralStrategy::CustomCurve"))
	TObjectPtr<UCurveFloat> CustomBlendCurve = nullptr;

	/** Multiplier for accelerated blend mode - derived from input frequency if 0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|RapidInput",
	          meta = (ClampMin = "0.0", ClampMax = "10.0", EditCondition = "RapidInputMode == ERapidInputBlendMode::Accelerate"))
	float AccelerationMultiplier = 0.0f;  // 0 = derive from input timing

	/** Default constructor - values derived at runtime */
	FProceduralBlendConfig()
		: Strategy(EProceduralStrategy::Linear)
		, RapidInputMode(ERapidInputBlendMode::ForceInstant)
		, ChainMode(EMultiFactorChainMode::TieredFallback)
		, PerceptualParams()
		, MinBlendTime(0.0f)
		, MaxBlendTime(0.0f)
		, InstantBlendThreshold(0.0f)
		, CustomBlendCurve(nullptr)
		, AccelerationMultiplier(0.0f)
	{
	}

	/** Get effective minimum blend time (derived or overridden) */
	FORCEINLINE float GetEffectiveMinBlendTime() const
	{
		return (MinBlendTime > 0.0f) ? MinBlendTime : PerceptualParams.GetMinBlendTime();
	}

	/** Get effective maximum blend time (derived or overridden) */
	FORCEINLINE float GetEffectiveMaxBlendTime() const
	{
		return (MaxBlendTime > 0.0f) ? MaxBlendTime : PerceptualParams.GetMaxBlendTime();
	}

	/** Get effective instant blend threshold (derived from max blend / animation length ratio) */
	float GetEffectiveInstantThreshold(float AnimationLength) const
	{
		if (InstantBlendThreshold > 0.0f)
		{
			return InstantBlendThreshold;
		}
		// Derive: threshold where remaining time equals minimum blend time
		// This ensures we have enough time to complete the blend
		if (AnimationLength > 0.0f)
		{
			const float MinTime = GetEffectiveMinBlendTime();
			return FMath::Clamp(1.0f - (MinTime / AnimationLength), 0.5f, 0.99f);
		}
		return 0.95f;  // Fallback if no animation length provided
	}

	/** Get effective acceleration multiplier (derived from rapid input frequency) */
	float GetEffectiveAccelerationMultiplier(float InputFrequency = 0.0f) const
	{
		if (AccelerationMultiplier > 0.0f)
		{
			return AccelerationMultiplier;
		}
		// Derive from input frequency: faster mashing = more acceleration
		// InputFrequency is inputs per second
		if (InputFrequency > 0.0f)
		{
			// Scale: 2 inputs/sec = 1.5x, 4 inputs/sec = 2.0x, 8+ inputs/sec = 3.0x
			return FMath::Clamp(1.0f + (InputFrequency / 4.0f), 1.0f, 3.0f);
		}
		return 2.0f;  // Default fallback
	}

	/** Create config from framerate and feel preset */
	static FProceduralBlendConfig FromPreset(float Framerate, ECombatFeelPreset Preset)
	{
		FProceduralBlendConfig Config;
		Config.PerceptualParams = FPerceptualDerivationParams::FromPreset(Framerate, Preset);
		return Config;
	}

	/** Create config for snappy reactive combat (6-9 frames at target FPS) */
	static FProceduralBlendConfig CreateSnappy(float Framerate = 60.0f)
	{
		return FromPreset(Framerate, ECombatFeelPreset::Snappy);
	}

	/** Create config for balanced combat (9-15 frames at target FPS) */
	static FProceduralBlendConfig CreateBalanced(float Framerate = 60.0f)
	{
		return FromPreset(Framerate, ECombatFeelPreset::Balanced);
	}

	/** Create config for smooth cinematic combat (15-18 frames at target FPS) */
	static FProceduralBlendConfig CreateSmooth(float Framerate = 60.0f)
	{
		return FromPreset(Framerate, ECombatFeelPreset::Smooth);
	}
};

// ============================================================================
// PROCEDURAL BLEND RESULT
// ============================================================================

/**
 * Result of procedural blend calculation.
 * Rich return type for debugging, analytics, and flexibility.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FProceduralBlendResult
{
	GENERATED_BODY()

	/** Calculated blend-in time for new montage */
	UPROPERTY(BlueprintReadOnly, Category = "Blend")
	float BlendInTime = 0.0f;

	/** Calculated blend-out time for current montage */
	UPROPERTY(BlueprintReadOnly, Category = "Blend")
	float BlendOutTime = 0.0f;

	/** Current animation progress (0-1) */
	UPROPERTY(BlueprintReadOnly, Category = "Blend")
	float AnimationProgress = 0.0f;

	/** Remaining time in current animation (seconds) */
	UPROPERTY(BlueprintReadOnly, Category = "Blend")
	float RemainingTime = 0.0f;

	/** Raw interpolation alpha before clamping (for debug) */
	UPROPERTY(BlueprintReadOnly, Category = "Blend|Debug")
	float RawInterpolationAlpha = 0.0f;

	/** Strategy that was used for calculation */
	UPROPERTY(BlueprintReadOnly, Category = "Blend|Debug")
	EProceduralStrategy UsedStrategy = EProceduralStrategy::Linear;

	/** Chain mode used for multi-factor calculation */
	UPROPERTY(BlueprintReadOnly, Category = "Blend|Debug")
	EMultiFactorChainMode UsedChainMode = EMultiFactorChainMode::TieredFallback;

	/** Which tier was used (for tiered fallback mode) */
	UPROPERTY(BlueprintReadOnly, Category = "Blend|Debug")
	int32 TierUsed = 0;

	/** Should use instant blend (rapid input or near end) */
	UPROPERTY(BlueprintReadOnly, Category = "Blend")
	bool bUseInstantBlend = false;

	/** Was this a fresh attack (no previous montage) */
	UPROPERTY(BlueprintReadOnly, Category = "Blend")
	bool bIsFreshAttack = false;

	/** Was rapid input detected? */
	UPROPERTY(BlueprintReadOnly, Category = "Blend")
	bool bRapidInputDetected = false;

	/** Was remaining time constraint active? */
	UPROPERTY(BlueprintReadOnly, Category = "Blend|Debug")
	bool bRemainingTimeConstrained = false;

	/** Was target timing constraint active? */
	UPROPERTY(BlueprintReadOnly, Category = "Blend|Debug")
	bool bTargetTimingConstrained = false;

	/** Default constructor */
	FProceduralBlendResult()
		: BlendInTime(0.0f)
		, BlendOutTime(0.0f)
		, AnimationProgress(0.0f)
		, RemainingTime(0.0f)
		, RawInterpolationAlpha(0.0f)
		, UsedStrategy(EProceduralStrategy::Linear)
		, UsedChainMode(EMultiFactorChainMode::TieredFallback)
		, TierUsed(0)
		, bUseInstantBlend(false)
		, bIsFreshAttack(false)
		, bRapidInputDetected(false)
		, bRemainingTimeConstrained(false)
		, bTargetTimingConstrained(false)
	{
	}

	/** Is blend valid? */
	bool IsValid() const { return BlendInTime >= 0.0f && BlendOutTime >= 0.0f; }

	/** Get effective blend duration (max of in/out) */
	float GetEffectiveDuration() const { return FMath::Max(BlendInTime, BlendOutTime); }

	/** Check if blend can complete before animation ends */
	bool CanCompleteBeforeEnd() const { return GetEffectiveDuration() <= RemainingTime; }

	/** Get debug string for logging */
	FString ToDebugString() const
	{
		return FString::Printf(TEXT("BlendIn=%.3f, BlendOut=%.3f, Progress=%.1f%%, Remaining=%.3f, Strategy=%d, Chain=%d, Tier=%d, Instant=%s, Fresh=%s, Rapid=%s, TimeConstr=%s, TargetConstr=%s"),
			BlendInTime, BlendOutTime, AnimationProgress * 100.0f, RemainingTime,
			static_cast<int32>(UsedStrategy),
			static_cast<int32>(UsedChainMode),
			TierUsed,
			bUseInstantBlend ? TEXT("Y") : TEXT("N"),
			bIsFreshAttack ? TEXT("Y") : TEXT("N"),
			bRapidInputDetected ? TEXT("Y") : TEXT("N"),
			bRemainingTimeConstrained ? TEXT("Y") : TEXT("N"),
			bTargetTimingConstrained ? TEXT("Y") : TEXT("N"));
	}
};

// ============================================================================
// POSE ANALYSIS TYPES
// ============================================================================

/**
 * Result of pose similarity calculation between two animation states.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FPoseSimilarityResult
{
	GENERATED_BODY()

	/** Overall similarity score (0 = completely different, 1 = identical) */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	float OverallSimilarity = 0.0f;

	/** Root bone position difference (world units) */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	float RootPositionDelta = 0.0f;

	/** Root bone rotation difference (degrees) */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	float RootRotationDelta = 0.0f;

	/** Average bone rotation difference across skeleton (degrees) */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	float AverageBoneRotationDelta = 0.0f;

	/** Maximum bone rotation difference (degrees) */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	float MaxBoneRotationDelta = 0.0f;

	/** Name of bone with maximum difference */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	FName MaxDifferenceBone = NAME_None;

	/** Number of bones compared */
	UPROPERTY(BlueprintReadOnly, Category = "Pose|Debug")
	int32 BonesCompared = 0;

	/** Derived blend time recommendation based on similarity */
	float GetRecommendedBlendTime(float MinBlend, float MaxBlend) const
	{
		// More similar = shorter blend needed
		// Similarity 1.0 → MinBlend, Similarity 0.0 → MaxBlend
		return FMath::Lerp(MaxBlend, MinBlend, OverallSimilarity);
	}
};

/**
 * Snapshot of skeletal pose at a specific time.
 * Named FProceduralPoseSnapshot to avoid collision with Engine's FPoseSnapshot.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FProceduralPoseSnapshot
{
	GENERATED_BODY()

	/** Bone transforms in component space */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	TArray<FTransform> BoneTransforms;

	/** Bone names for reference */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	TArray<FName> BoneNames;

	/** Time at which snapshot was taken */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	float SnapshotTime = 0.0f;

	/** Animation position when snapshot was taken */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	float AnimationPosition = 0.0f;

	/** Is this snapshot valid? */
	bool IsValid() const { return BoneTransforms.Num() > 0 && BoneTransforms.Num() == BoneNames.Num(); }
};

// ============================================================================
// VELOCITY ANALYSIS TYPES
// ============================================================================

/**
 * Result of velocity analysis for animation blending.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FVelocityAnalysisResult
{
	GENERATED_BODY()

	/** Root motion velocity (world units per second) */
	UPROPERTY(BlueprintReadOnly, Category = "Velocity")
	FVector RootVelocity = FVector::ZeroVector;

	/** Root motion angular velocity (degrees per second) */
	UPROPERTY(BlueprintReadOnly, Category = "Velocity")
	FRotator RootAngularVelocity = FRotator::ZeroRotator;

	/** Average bone linear velocity magnitude (world units per second) */
	UPROPERTY(BlueprintReadOnly, Category = "Velocity")
	float AverageBoneSpeed = 0.0f;

	/** Maximum bone linear velocity (world units per second) */
	UPROPERTY(BlueprintReadOnly, Category = "Velocity")
	float MaxBoneSpeed = 0.0f;

	/** Name of bone with maximum velocity */
	UPROPERTY(BlueprintReadOnly, Category = "Velocity")
	FName FastestBone = NAME_None;

	/** Kinetic energy estimate (for momentum-aware blending) */
	UPROPERTY(BlueprintReadOnly, Category = "Velocity")
	float KineticEnergyEstimate = 0.0f;

	/** Momentum direction (normalized) */
	UPROPERTY(BlueprintReadOnly, Category = "Velocity")
	FVector MomentumDirection = FVector::ForwardVector;

	/** Derived blend time recommendation based on velocity */
	float GetRecommendedBlendTime(float MinBlend, float MaxBlend, float ReferenceSpeed = 500.0f) const
	{
		// Higher velocity = longer blend to avoid jarring stop
		// Velocity 0 → MinBlend, Velocity >= ReferenceSpeed → MaxBlend
		const float SpeedRatio = FMath::Clamp(MaxBoneSpeed / ReferenceSpeed, 0.0f, 1.0f);
		return FMath::Lerp(MinBlend, MaxBlend, SpeedRatio);
	}
};

// ============================================================================
// IK TARGET TYPES
// ============================================================================

/**
 * Procedurally calculated IK target.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FProceduralIKTarget
{
	GENERATED_BODY()

	/** Target position in world space */
	UPROPERTY(BlueprintReadWrite, Category = "IK")
	FVector TargetPosition = FVector::ZeroVector;

	/** Target rotation in world space */
	UPROPERTY(BlueprintReadWrite, Category = "IK")
	FRotator TargetRotation = FRotator::ZeroRotator;

	/** Target type that was calculated */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	EIKTargetType TargetType = EIKTargetType::WorldPosition;

	/** Confidence in target validity (0-1) */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	float Confidence = 0.0f;

	/** Is target within reachable range? */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	bool bIsReachable = false;

	/** Distance to target from effector bone */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	float DistanceToTarget = 0.0f;

	/** Time at which target was calculated (for prediction) */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	float CalculationTime = 0.0f;

	/** Default constructor */
	FProceduralIKTarget()
		: TargetPosition(FVector::ZeroVector)
		, TargetRotation(FRotator::ZeroRotator)
		, TargetType(EIKTargetType::WorldPosition)
		, Confidence(0.0f)
		, bIsReachable(false)
		, DistanceToTarget(0.0f)
		, CalculationTime(0.0f)
	{
	}

	/** Is this target valid for use? */
	bool IsValid() const { return Confidence > 0.0f && bIsReachable; }
};

// ============================================================================
// POSE HEALING TYPES
// ============================================================================

/**
 * Configuration for post-blend pose healing.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FPoseHealingConfig
{
	GENERATED_BODY()

	/** Strategy for healing pose artifacts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing")
	EPoseHealingStrategy Strategy = EPoseHealingStrategy::InterpolateToReference;

	/** Healing strength (0 = no healing, 1 = full correction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealingStrength = 0.5f;

	/** Spring stiffness for SpringCorrection strategy */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing",
	          meta = (ClampMin = "0.0", EditCondition = "Strategy == EPoseHealingStrategy::SpringCorrection"))
	float SpringStiffness = 100.0f;

	/** Spring damping for SpringCorrection strategy */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing",
	          meta = (ClampMin = "0.0", EditCondition = "Strategy == EPoseHealingStrategy::SpringCorrection"))
	float SpringDamping = 10.0f;

	/** Maximum correction per frame (degrees for rotation, units for position) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing",
	          meta = (ClampMin = "0.0"))
	float MaxCorrectionPerFrame = 5.0f;

	/** Bones to prioritize for healing (empty = all bones) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing")
	TArray<FName> PriorityBones;

	/** Default constructor */
	FPoseHealingConfig()
		: Strategy(EPoseHealingStrategy::InterpolateToReference)
		, HealingStrength(0.5f)
		, SpringStiffness(100.0f)
		, SpringDamping(10.0f)
		, MaxCorrectionPerFrame(5.0f)
	{
	}
};

/**
 * Result of pose healing calculation.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FPoseHealingResult
{
	GENERATED_BODY()

	/** Corrected bone transforms (component space) */
	UPROPERTY(BlueprintReadOnly, Category = "Healing")
	TArray<FTransform> CorrectedTransforms;

	/** Amount of correction applied per bone (degrees/units) */
	UPROPERTY(BlueprintReadOnly, Category = "Healing")
	TArray<float> CorrectionAmounts;

	/** Total correction applied this frame */
	UPROPERTY(BlueprintReadOnly, Category = "Healing")
	float TotalCorrection = 0.0f;

	/** Is healing complete (pose within tolerance)? */
	UPROPERTY(BlueprintReadOnly, Category = "Healing")
	bool bHealingComplete = false;

	/** Remaining error after healing */
	UPROPERTY(BlueprintReadOnly, Category = "Healing")
	float RemainingError = 0.0f;
};

// ============================================================================
// CONTACT PREDICTION TYPES
// ============================================================================

/**
 * Result of contact point prediction.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FContactPredictionResult
{
	GENERATED_BODY()

	/** Predicted contact position (world space) */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	FVector ContactPosition = FVector::ZeroVector;

	/** Predicted contact normal */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	FVector ContactNormal = FVector::UpVector;

	/** Predicted time until contact (seconds) */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	float TimeToContact = 0.0f;

	/** Relative velocity at contact (world units per second) */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	float ImpactVelocity = 0.0f;

	/** Confidence in prediction (0-1) */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	float Confidence = 0.0f;

	/** Will contact occur within prediction window? */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	bool bContactPredicted = false;

	/** Bone/socket making contact (if applicable) */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	FName ContactBone = NAME_None;

	/** Default constructor */
	FContactPredictionResult()
		: ContactPosition(FVector::ZeroVector)
		, ContactNormal(FVector::UpVector)
		, TimeToContact(0.0f)
		, ImpactVelocity(0.0f)
		, Confidence(0.0f)
		, bContactPredicted(false)
		, ContactBone(NAME_None)
	{
	}

	/** Is this a valid, confident prediction? */
	bool IsValid() const { return bContactPredicted && Confidence > 0.5f; }
};

// ============================================================================
// PROCEDURAL TIMING TYPES
// ============================================================================

/**
 * Configuration for procedural timing calculations.
 * Can extend procedural system beyond just blend timing.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FProceduralTimingConfig
{
	GENERATED_BODY()

	/** Strategy for timing calculation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	EProceduralStrategy Strategy = EProceduralStrategy::Linear;

	/** Perceptual parameters for deriving timing bounds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	FPerceptualDerivationParams PerceptualParams;

	/** Minimum duration override (0 = use derived from PerceptualParams) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float MinDuration = 0.0f;

	/** Maximum duration override (0 = use derived from PerceptualParams) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float MaxDuration = 0.0f;

	/** Custom curve for timing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
	          meta = (EditCondition = "Strategy == EProceduralStrategy::CustomCurve"))
	TObjectPtr<UCurveFloat> CustomCurve = nullptr;

	FProceduralTimingConfig()
		: Strategy(EProceduralStrategy::Linear)
		, PerceptualParams()
		, MinDuration(0.0f)
		, MaxDuration(0.0f)
		, CustomCurve(nullptr)
	{
	}

	/** Get effective minimum duration */
	float GetEffectiveMinDuration() const
	{
		return (MinDuration > 0.0f) ? MinDuration : PerceptualParams.GetMinBlendTime();
	}

	/** Get effective maximum duration */
	float GetEffectiveMaxDuration() const
	{
		return (MaxDuration > 0.0f) ? MaxDuration : PerceptualParams.GetMaxBlendTime();
	}
};

// ============================================================================
// ANIMATION TIME SCALING TYPES
// ============================================================================

/**
 * Result of animation time scaling calculation.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FTimeScalingResult
{
	GENERATED_BODY()

	/** Calculated play rate multiplier */
	UPROPERTY(BlueprintReadOnly, Category = "TimeScale")
	float PlayRateMultiplier = 1.0f;

	/** Original animation duration */
	UPROPERTY(BlueprintReadOnly, Category = "TimeScale")
	float OriginalDuration = 0.0f;

	/** Scaled animation duration */
	UPROPERTY(BlueprintReadOnly, Category = "TimeScale")
	float ScaledDuration = 0.0f;

	/** Was compression applied (rate > 1)? */
	UPROPERTY(BlueprintReadOnly, Category = "TimeScale")
	bool bIsCompressed = false;

	/** Was expansion applied (rate < 1)? */
	UPROPERTY(BlueprintReadOnly, Category = "TimeScale")
	bool bIsExpanded = false;

	/** Scale ratio (ScaledDuration / OriginalDuration) */
	float GetScaleRatio() const
	{
		return (OriginalDuration > 0.0f) ? (ScaledDuration / OriginalDuration) : 1.0f;
	}
};

// ============================================================================
// ROOT MOTION ANALYSIS TYPES
// ============================================================================

/**
 * Result of root motion analysis.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FRootMotionAnalysisResult
{
	GENERATED_BODY()

	/** Total root motion translation over analysis window */
	UPROPERTY(BlueprintReadOnly, Category = "RootMotion")
	FVector TotalTranslation = FVector::ZeroVector;

	/** Total root motion rotation over analysis window */
	UPROPERTY(BlueprintReadOnly, Category = "RootMotion")
	FRotator TotalRotation = FRotator::ZeroRotator;

	/** Average velocity during analysis window */
	UPROPERTY(BlueprintReadOnly, Category = "RootMotion")
	FVector AverageVelocity = FVector::ZeroVector;

	/** Peak velocity during analysis window */
	UPROPERTY(BlueprintReadOnly, Category = "RootMotion")
	float PeakSpeed = 0.0f;

	/** Time of peak velocity */
	UPROPERTY(BlueprintReadOnly, Category = "RootMotion")
	float PeakSpeedTime = 0.0f;

	/** Is root motion significant (above threshold)? */
	UPROPERTY(BlueprintReadOnly, Category = "RootMotion")
	bool bHasSignificantMotion = false;

	/** Dominant motion axis */
	UPROPERTY(BlueprintReadOnly, Category = "RootMotion")
	FVector DominantAxis = FVector::ForwardVector;

	/** Analysis window duration */
	UPROPERTY(BlueprintReadOnly, Category = "RootMotion|Debug")
	float AnalysisWindowDuration = 0.0f;
};

// ============================================================================
// CONSTRAINT SATISFACTION TYPES
// ============================================================================

/**
 * Spatial constraint for animation validation.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FSpatialConstraint
{
	GENERATED_BODY()

	/** Constraint center (world space) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
	FVector Center = FVector::ZeroVector;

	/** Maximum allowed distance from center */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint",
	          meta = (ClampMin = "0.0"))
	float MaxDistance = 100.0f;

	/** Minimum allowed distance from center */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint",
	          meta = (ClampMin = "0.0"))
	float MinDistance = 0.0f;

	/** Maximum allowed height (Z) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
	float MaxHeight = 1000.0f;

	/** Minimum allowed height (Z) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
	float MinHeight = 0.0f;

	/** Is constraint active? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
	bool bIsActive = true;

	/** Check if a point satisfies this constraint */
	bool IsSatisfied(const FVector& Point) const
	{
		if (!bIsActive) return true;

		const float DistXY = FVector::Dist2D(Point, Center);
		if (DistXY < MinDistance || DistXY > MaxDistance) return false;
		if (Point.Z < MinHeight || Point.Z > MaxHeight) return false;
		return true;
	}

	/** Get satisfaction score (1.0 = fully satisfied, 0.0 = fully violated) */
	float GetSatisfactionScore(const FVector& Point) const
	{
		if (!bIsActive) return 1.0f;

		float Score = 1.0f;

		// Distance score
		const float DistXY = FVector::Dist2D(Point, Center);
		if (MaxDistance > MinDistance)
		{
			const float MidDist = (MinDistance + MaxDistance) * 0.5f;
			const float DistRange = (MaxDistance - MinDistance) * 0.5f;
			const float DistScore = 1.0f - FMath::Clamp(FMath::Abs(DistXY - MidDist) / DistRange, 0.0f, 1.0f);
			Score *= DistScore;
		}

		// Height score
		if (MaxHeight > MinHeight)
		{
			const float MidHeight = (MinHeight + MaxHeight) * 0.5f;
			const float HeightRange = (MaxHeight - MinHeight) * 0.5f;
			const float HeightScore = 1.0f - FMath::Clamp(FMath::Abs(Point.Z - MidHeight) / HeightRange, 0.0f, 1.0f);
			Score *= HeightScore;
		}

		return Score;
	}
};

/**
 * Result of constraint satisfaction check.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FConstraintSatisfactionResult
{
	GENERATED_BODY()

	/** Overall satisfaction score (0-1) */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	float OverallScore = 0.0f;

	/** Number of constraints checked */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	int32 ConstraintsChecked = 0;

	/** Number of constraints satisfied */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	int32 ConstraintsSatisfied = 0;

	/** Most violated constraint index (-1 if all satisfied) */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	int32 MostViolatedIndex = -1;

	/** Score of most violated constraint */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	float MostViolatedScore = 1.0f;

	/** Suggested correction direction */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	FVector SuggestedCorrection = FVector::ZeroVector;

	/** Are all constraints satisfied? */
	bool AllSatisfied() const { return ConstraintsSatisfied == ConstraintsChecked; }
};

// ============================================================================
// CONTROL RIG PARAMETER TYPES
// ============================================================================

/**
 * Type of Control Rig parameter for procedural calculation.
 */
UENUM(BlueprintType)
enum class EControlRigParamType : uint8
{
	/** Scalar value (float) - blend weights, multipliers */
	Scalar              UMETA(DisplayName = "Scalar (Float)"),

	/** Vector3 - positions, directions, offsets */
	Vector              UMETA(DisplayName = "Vector (FVector)"),

	/** Rotator - orientation adjustments */
	Rotator             UMETA(DisplayName = "Rotator (FRotator)"),

	/** Transform - full bone adjustment */
	Transform           UMETA(DisplayName = "Transform (FTransform)"),

	/** Boolean - toggle/enable flags */
	Boolean             UMETA(DisplayName = "Boolean (bool)")
};

/**
 * Control Rig parameter slot for procedural animation.
 * Used to pass calculated values to Control Rig at runtime.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FControlRigParam
{
	GENERATED_BODY()

	/** Parameter name (matches Control Rig control name) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ControlRig")
	FName ParamName = NAME_None;

	/** Parameter type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ControlRig")
	EControlRigParamType ParamType = EControlRigParamType::Scalar;

	/** Scalar value (for Scalar/Boolean types) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ControlRig")
	float ScalarValue = 0.0f;

	/** Vector value (for Vector type) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ControlRig")
	FVector VectorValue = FVector::ZeroVector;

	/** Rotator value (for Rotator type) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ControlRig")
	FRotator RotatorValue = FRotator::ZeroRotator;

	/** Transform value (for Transform type) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ControlRig")
	FTransform TransformValue = FTransform::Identity;

	/** Blend weight for this parameter (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ControlRig",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlendWeight = 1.0f;

	FControlRigParam() = default;

	/** Create scalar parameter */
	static FControlRigParam CreateScalar(FName Name, float Value, float Weight = 1.0f)
	{
		FControlRigParam Param;
		Param.ParamName = Name;
		Param.ParamType = EControlRigParamType::Scalar;
		Param.ScalarValue = Value;
		Param.BlendWeight = Weight;
		return Param;
	}

	/** Create vector parameter */
	static FControlRigParam CreateVector(FName Name, const FVector& Value, float Weight = 1.0f)
	{
		FControlRigParam Param;
		Param.ParamName = Name;
		Param.ParamType = EControlRigParamType::Vector;
		Param.VectorValue = Value;
		Param.BlendWeight = Weight;
		return Param;
	}

	/** Create transform parameter */
	static FControlRigParam CreateTransform(FName Name, const FTransform& Value, float Weight = 1.0f)
	{
		FControlRigParam Param;
		Param.ParamName = Name;
		Param.ParamType = EControlRigParamType::Transform;
		Param.TransformValue = Value;
		Param.BlendWeight = Weight;
		return Param;
	}
};

/**
 * Look-at target for procedural head/torso tracking.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FProceduralLookAtTarget
{
	GENERATED_BODY()

	/** World-space position to look at */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LookAt")
	FVector TargetPosition = FVector::ZeroVector;

	/** Weight for head rotation (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LookAt",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeadWeight = 1.0f;

	/** Weight for chest/spine rotation (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LookAt",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChestWeight = 0.3f;

	/** Weight for eyes (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LookAt",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EyeWeight = 1.0f;

	/** Maximum head yaw angle (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LookAt",
	          meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxHeadYaw = 70.0f;

	/** Maximum head pitch angle (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LookAt",
	          meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxHeadPitch = 35.0f;

	/** Interpolation speed for smooth tracking */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LookAt",
	          meta = (ClampMin = "0.0"))
	float InterpSpeed = 5.0f;

	FProceduralLookAtTarget() = default;
};

/**
 * Result of look-at calculation for Control Rig.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FLookAtResult
{
	GENERATED_BODY()

	/** Calculated head rotation offset */
	UPROPERTY(BlueprintReadOnly, Category = "LookAt")
	FRotator HeadRotation = FRotator::ZeroRotator;

	/** Calculated chest rotation offset */
	UPROPERTY(BlueprintReadOnly, Category = "LookAt")
	FRotator ChestRotation = FRotator::ZeroRotator;

	/** Calculated eye rotation offset */
	UPROPERTY(BlueprintReadOnly, Category = "LookAt")
	FRotator EyeRotation = FRotator::ZeroRotator;

	/** Was target within valid angle range? */
	UPROPERTY(BlueprintReadOnly, Category = "LookAt")
	bool bTargetInRange = false;

	/** Angle to target (degrees) */
	UPROPERTY(BlueprintReadOnly, Category = "LookAt")
	float AngleToTarget = 0.0f;

	/** Distance to target */
	UPROPERTY(BlueprintReadOnly, Category = "LookAt")
	float DistanceToTarget = 0.0f;
};

// ============================================================================
// ANIMATION SELECTION TYPES
// ============================================================================

/**
 * Gameplay context for animation selection.
 * Captures all relevant state for choosing appropriate animation.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FAnimationSelectionContext
{
	GENERATED_BODY()

	// === Movement Context ===

	/** Character's current velocity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Movement")
	FVector Velocity = FVector::ZeroVector;

	/** Character's facing direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Movement")
	FVector FacingDirection = FVector::ForwardVector;

	/** Is character grounded? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Movement")
	bool bIsGrounded = true;

	/** Is character in air? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Movement")
	bool bIsInAir = false;

	// === Combat Context ===

	/** Direction to target (normalized) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Combat")
	FVector DirectionToTarget = FVector::ForwardVector;

	/** Distance to target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Combat")
	float DistanceToTarget = 0.0f;

	/** Relative angle to target (degrees, -180 to 180) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Combat")
	float AngleToTarget = 0.0f;

	/** Target's velocity (for prediction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Combat")
	FVector TargetVelocity = FVector::ZeroVector;

	/** Is target blocking? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Combat")
	bool bTargetBlocking = false;

	/** Is target staggered? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Combat")
	bool bTargetStaggered = false;

	// === Input Context ===

	/** Current input direction (normalized) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Input")
	FVector InputDirection = FVector::ZeroVector;

	/** Is heavy attack input? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Input")
	bool bIsHeavyAttack = false;

	/** Is directional attack input? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Input")
	bool bIsDirectional = false;

	/** Current combo count (0 = first attack) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|Input")
	int32 ComboCount = 0;

	// === State Context ===

	/** Current animation position (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|State")
	float CurrentAnimProgress = 0.0f;

	/** Time since last attack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|State")
	float TimeSinceLastAttack = 0.0f;

	/** Character's current health (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context|State")
	float HealthPercent = 1.0f;

	FAnimationSelectionContext() = default;

	/** Get movement speed */
	float GetSpeed() const { return Velocity.Size(); }

	/** Get movement direction (normalized) */
	FVector GetMoveDirection() const { return Velocity.GetSafeNormal(); }

	/** Is character moving? */
	bool IsMoving() const { return Velocity.SizeSquared() > 100.0f; }

	/** Get angle between facing and movement */
	float GetMoveFacingAngle() const
	{
		if (!IsMoving()) return 0.0f;
		return FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(FacingDirection, GetMoveDirection())));
	}
};

/**
 * Animation candidate for selection scoring.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FAnimationCandidate
{
	GENERATED_BODY()

	/** Animation identifier (montage name or slot) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candidate")
	FName AnimationId = NAME_None;

	/** Soft reference to montage (optional) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candidate")
	TSoftObjectPtr<class UAnimMontage> Montage;

	// === Animation Properties ===

	/** Ideal distance range for this animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candidate|Properties")
	FVector2D IdealDistanceRange = FVector2D(100.0f, 200.0f);

	/** Ideal angle range for this animation (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candidate|Properties")
	FVector2D IdealAngleRange = FVector2D(-45.0f, 45.0f);

	/** Required input direction (forward, back, left, right, none) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candidate|Properties")
	FVector RequiredInputDirection = FVector::ZeroVector;

	/** Is this a heavy attack animation? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candidate|Properties")
	bool bIsHeavy = false;

	/** Is this a gap-closer animation? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candidate|Properties")
	bool bIsGapCloser = false;

	/** Valid combo positions (empty = any) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candidate|Properties")
	TArray<int32> ValidComboPositions;

	/** Base priority (higher = preferred) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candidate|Properties")
	float BasePriority = 1.0f;

	FAnimationCandidate() = default;

	/** Check if this candidate is valid for given combo position */
	bool IsValidForComboPosition(int32 ComboPos) const
	{
		return ValidComboPositions.Num() == 0 || ValidComboPositions.Contains(ComboPos);
	}
};

/**
 * Result of animation selection scoring.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FAnimationSelectionResult
{
	GENERATED_BODY()

	/** Index of selected animation in candidate list */
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	int32 SelectedIndex = -1;

	/** Name of selected animation */
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	FName SelectedAnimationId = NAME_None;

	/** Final score of selected animation */
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	float SelectedScore = 0.0f;

	/** Scores for all candidates (for debugging) */
	UPROPERTY(BlueprintReadOnly, Category = "Selection|Debug")
	TArray<float> AllScores;

	/** Was a valid animation found? */
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	bool bValidSelection = false;

	/** Reason for selection (debug) */
	UPROPERTY(BlueprintReadOnly, Category = "Selection|Debug")
	FString SelectionReason;

	FAnimationSelectionResult() = default;
};

// ============================================================================
// LAYERED BLENDING TYPES
// ============================================================================

/**
 * Per-bone blend weight configuration for procedural animation.
 * Named to avoid collision with engine's FPerBoneBlendWeight.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FProceduralBoneBlendWeight
{
	GENERATED_BODY()

	/** Bone name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	FName BoneName = NAME_None;

	/** Blend weight for this bone (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlendWeight = 1.0f;

	/** Should children inherit this weight? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	bool bIncludeChildren = true;

	/** Blend depth (how many levels of children to affect) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend",
	          meta = (ClampMin = "-1", EditCondition = "bIncludeChildren"))
	int32 BlendDepth = -1;  // -1 = all children

	FProceduralBoneBlendWeight() = default;

	static FProceduralBoneBlendWeight Create(FName Bone, float Weight, bool bChildren = true)
	{
		FProceduralBoneBlendWeight Result;
		Result.BoneName = Bone;
		Result.BlendWeight = Weight;
		Result.bIncludeChildren = bChildren;
		return Result;
	}
};

/**
 * Layered blend configuration for combining multiple animations.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FLayeredBlendConfig
{
	GENERATED_BODY()

	/** Per-bone blend weights */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LayeredBlend")
	TArray<FProceduralBoneBlendWeight> BoneWeights;

	/** Global blend weight multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LayeredBlend",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GlobalWeight = 1.0f;

	/** Is this additive blend? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LayeredBlend")
	bool bIsAdditive = false;

	/** Blend space (local/mesh/component) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LayeredBlend")
	TEnumAsByte<EBoneControlSpace> BlendSpace = BCS_ComponentSpace;

	FLayeredBlendConfig() = default;

	/** Create upper body only config */
	static FLayeredBlendConfig CreateUpperBody(float Weight = 1.0f)
	{
		FLayeredBlendConfig Config;
		Config.BoneWeights.Add(FProceduralBoneBlendWeight::Create(FName("spine_01"), Weight, true));
		Config.GlobalWeight = Weight;
		return Config;
	}

	/** Create lower body only config */
	static FLayeredBlendConfig CreateLowerBody(float Weight = 1.0f)
	{
		FLayeredBlendConfig Config;
		Config.BoneWeights.Add(FProceduralBoneBlendWeight::Create(FName("pelvis"), Weight, false));
		Config.BoneWeights.Add(FProceduralBoneBlendWeight::Create(FName("thigh_l"), Weight, true));
		Config.BoneWeights.Add(FProceduralBoneBlendWeight::Create(FName("thigh_r"), Weight, true));
		Config.GlobalWeight = Weight;
		return Config;
	}

	/** Create arms only config */
	static FLayeredBlendConfig CreateArmsOnly(float Weight = 1.0f)
	{
		FLayeredBlendConfig Config;
		Config.BoneWeights.Add(FProceduralBoneBlendWeight::Create(FName("clavicle_l"), Weight, true));
		Config.BoneWeights.Add(FProceduralBoneBlendWeight::Create(FName("clavicle_r"), Weight, true));
		Config.GlobalWeight = Weight;
		return Config;
	}
};

// ============================================================================
// ANIMATION WARPING TYPES
// ============================================================================

/**
 * Procedural warp target calculation result.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FProceduralWarpTarget
{
	GENERATED_BODY()

	/** Target position for warp */
	UPROPERTY(BlueprintReadWrite, Category = "Warp")
	FVector TargetPosition = FVector::ZeroVector;

	/** Target rotation for warp */
	UPROPERTY(BlueprintReadWrite, Category = "Warp")
	FRotator TargetRotation = FRotator::ZeroRotator;

	/** Warp alpha (how much to apply) */
	UPROPERTY(BlueprintReadWrite, Category = "Warp",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WarpAlpha = 1.0f;

	/** Is warp target valid? */
	UPROPERTY(BlueprintReadOnly, Category = "Warp")
	bool bIsValid = false;

	/** Distance from current to target */
	UPROPERTY(BlueprintReadOnly, Category = "Warp")
	float WarpDistance = 0.0f;

	/** Angle from current to target (degrees) */
	UPROPERTY(BlueprintReadOnly, Category = "Warp")
	float WarpAngle = 0.0f;

	/** Confidence in warp calculation */
	UPROPERTY(BlueprintReadOnly, Category = "Warp",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Confidence = 0.0f;

	FProceduralWarpTarget() = default;
};

/**
 * Configuration for procedural distance matching.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FDistanceMatchConfig
{
	GENERATED_BODY()

	/** Target distance to match */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatch")
	float TargetDistance = 0.0f;

	/** Tolerance for distance matching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatch",
	          meta = (ClampMin = "0.0"))
	float DistanceTolerance = 10.0f;

	/** Animation curve to sample for distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatch")
	TObjectPtr<UCurveFloat> DistanceCurve = nullptr;

	/** Play rate bounds for distance matching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatch")
	FVector2D PlayRateBounds = FVector2D(0.8f, 1.2f);

	FDistanceMatchConfig() = default;
};

/**
 * Result of distance matching calculation.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FDistanceMatchResult
{
	GENERATED_BODY()

	/** Calculated start position in animation (0-1) */
	UPROPERTY(BlueprintReadOnly, Category = "DistanceMatch")
	float StartPosition = 0.0f;

	/** Calculated play rate multiplier */
	UPROPERTY(BlueprintReadOnly, Category = "DistanceMatch")
	float PlayRate = 1.0f;

	/** Predicted end distance after animation */
	UPROPERTY(BlueprintReadOnly, Category = "DistanceMatch")
	float PredictedEndDistance = 0.0f;

	/** Distance error (predicted - target) */
	UPROPERTY(BlueprintReadOnly, Category = "DistanceMatch")
	float DistanceError = 0.0f;

	/** Is match within tolerance? */
	UPROPERTY(BlueprintReadOnly, Category = "DistanceMatch")
	bool bMatchSuccessful = false;

	FDistanceMatchResult() = default;
};

// ============================================================================
// MULTI-FACTOR INPUT TYPES
// ============================================================================

/**
 * Combined input for multi-factor blend calculation.
 * Supports all three chaining modes: weighted, cascade, tiered.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FMultiFactorBlendInput
{
	GENERATED_BODY()

	// === Tier 1: Basic (always available) ===

	/** Current position in source animation (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Tier1")
	float CurrentPosition = 0.0f;

	/** Total length of source animation (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Tier1")
	float AnimationLength = 0.0f;

	// === Tier 2: Target-aware (optional) ===

	/** Target animation's windup time (seconds), 0 if unknown */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Tier2")
	float TargetWindupTime = 0.0f;

	/** Target animation's total length (seconds), 0 if unknown */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Tier2")
	float TargetAnimationLength = 0.0f;

	// === Tier 3: Pose-aware (optional) ===

	/** Pose similarity result, if available */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Tier3")
	FPoseSimilarityResult PoseSimilarity;

	/** Is pose similarity data valid? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Tier3")
	bool bHasPoseSimilarity = false;

	// === Tier 4: Velocity-aware (optional) ===

	/** Velocity analysis result, if available */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Tier4")
	FVelocityAnalysisResult VelocityAnalysis;

	/** Is velocity data valid? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Tier4")
	bool bHasVelocityData = false;

	// === Tier 5: Constraint-aware (optional) ===

	/** Spatial constraints to satisfy */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Tier5")
	TArray<FSpatialConstraint> SpatialConstraints;

	// === Context ===

	/** Is this rapid input during existing blend? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Context")
	bool bIsRapidInput = false;

	/** Rapid input frequency (inputs per second), for acceleration calculation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Context")
	float InputFrequency = 0.0f;

	/** Get highest available tier */
	int32 GetHighestAvailableTier() const
	{
		if (SpatialConstraints.Num() > 0) return 5;
		if (bHasVelocityData) return 4;
		if (bHasPoseSimilarity) return 3;
		if (TargetWindupTime > 0.0f || TargetAnimationLength > 0.0f) return 2;
		return 1;
	}

	/** Get normalized animation progress */
	float GetProgress() const
	{
		return (AnimationLength > 0.0f) ? FMath::Clamp(CurrentPosition / AnimationLength, 0.0f, 1.0f) : 0.0f;
	}

	/** Get remaining time in animation */
	float GetRemainingTime() const
	{
		return FMath::Max(0.0f, AnimationLength - CurrentPosition);
	}
};

