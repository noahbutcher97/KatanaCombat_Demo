
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.generated.h"

// Forward declarations
class UAttackData;
class UHitReactionData;
class UAnimMontage;
class AActor;
class UCameraShakeBase;

// ============================================================================
// ENUMS
// ============================================================================

/**
 * Combat state for the state machine
 */
UENUM(BlueprintType)
enum class ECombatState : uint8
{
    Idle                UMETA(DisplayName = "Idle"),
    Attacking           UMETA(DisplayName = "Attacking"),
    HoldingLightAttack  UMETA(DisplayName = "Holding Light Attack"),
    ChargingHeavyAttack UMETA(DisplayName = "Charging Heavy Attack"),
    Blocking            UMETA(DisplayName = "Blocking"),
    Parrying            UMETA(DisplayName = "Parrying"),
    GuardBroken         UMETA(DisplayName = "Guard Broken"),
    Finishing           UMETA(DisplayName = "Finishing"),
    HitStunned          UMETA(DisplayName = "Hit Stunned"),
    Evading             UMETA(DisplayName = "Evading"),
    Dead                UMETA(DisplayName = "Dead")
};

/**
 * Attack type classification
 */
UENUM(BlueprintType)
enum class EAttackType : uint8
{
    None            UMETA(DisplayName = "None"),
    Light           UMETA(DisplayName = "Light"),
    Heavy           UMETA(DisplayName = "Heavy"),
    Special         UMETA(DisplayName = "Special")
};

/**
 * Attack phase within animation
 * Phases are MUTUALLY EXCLUSIVE - only one active at a time
 * Controlled by AnimNotifyState_AttackPhase in montages
 *
 * NOTE: Hold/Combo/Parry/Cancel are WINDOWS (not phases)
 * Windows are tracked independently via booleans and can overlap
 */
UENUM(BlueprintType)
enum class EAttackPhase : uint8
{
    None            UMETA(DisplayName = "None"),
    Windup          UMETA(DisplayName = "Windup"),
    Active          UMETA(DisplayName = "Active"),
    Recovery        UMETA(DisplayName = "Recovery")
};

/**
 * Directional input for attacks and targeting (4-way for data configuration)
 */
UENUM(BlueprintType)
enum class EAttackDirection : uint8
{
    None            UMETA(DisplayName = "None"),
    Forward         UMETA(DisplayName = "Forward"),
    Backward        UMETA(DisplayName = "Backward"),
    Left            UMETA(DisplayName = "Left"),
    Right           UMETA(DisplayName = "Right")
};

/**
 * Input direction captured from movement stick/keys (8-way for gameplay)
 * Used for directional attacks, evades, targeting, hold follow-ups
 */
UENUM(BlueprintType)
enum class EInputDirection : uint8
{
    None            UMETA(DisplayName = "None"),
    Forward         UMETA(DisplayName = "Forward"),
    ForwardRight    UMETA(DisplayName = "Forward-Right"),
    Right           UMETA(DisplayName = "Right"),
    BackwardRight   UMETA(DisplayName = "Backward-Right"),
    Backward        UMETA(DisplayName = "Backward"),
    BackwardLeft    UMETA(DisplayName = "Backward-Left"),
    Left            UMETA(DisplayName = "Left"),
    ForwardLeft     UMETA(DisplayName = "Forward-Left")
};

/**
 * Hit reaction type classification
 * Light/Heavy: Directional reactions selected via EHitIntensity × EAttackDirection
 * Special: Non-directional reactions selected via ESpecialReactionType
 */
UENUM(BlueprintType)
enum class EHitReactionType : uint8
{
    Light           UMETA(DisplayName = "Light"),
    Heavy           UMETA(DisplayName = "Heavy"),
    Special         UMETA(DisplayName = "Special")
};

/**
 * Input type for buffering system
 */
UENUM(BlueprintType)
enum class EInputType : uint8
{
    None            UMETA(DisplayName = "None"),
    LightAttack     UMETA(DisplayName = "Light Attack"),
    HeavyAttack     UMETA(DisplayName = "Heavy Attack"),
    Block           UMETA(DisplayName = "Block"),
    Evade           UMETA(DisplayName = "Evade"),
    Special         UMETA(DisplayName = "Special")
};

/**
 * Editor-only: Timing fallback strategy for AttackDataTools notify generation
 * NOT used at runtime - combat system uses AnimNotify_AttackPhaseTransition events
 */
UENUM(BlueprintType)
enum class ETimingFallbackMode : uint8
{
    AutoCalculate           UMETA(DisplayName = "Auto Calculate"),
    RequireManualOverride   UMETA(DisplayName = "Require Manual Override"),
    UseSafeDefaults         UMETA(DisplayName = "Use Safe Defaults"),
    DisallowMontage         UMETA(DisplayName = "Disallow Montage")
};

/**
 * Hit intensity level - determines reaction severity
 * Separate from direction for modularity (intensity × direction = reaction)
 */
UENUM(BlueprintType)
enum class EHitIntensity : uint8
{
    Light           UMETA(DisplayName = "Light"),
    Heavy           UMETA(DisplayName = "Heavy")
    // Future: Medium, Critical, etc.
};

/**
 * Paired reaction type for synchronized attacker/victim animations
 * Extension point: Built now, wired when AttackData extended with counter/finisher names
 */
UENUM(BlueprintType)
enum class EPairedReactionType : uint8
{
    None            UMETA(DisplayName = "None"),
    Counter         UMETA(DisplayName = "Counter Reaction"),
    Finisher        UMETA(DisplayName = "Finisher Victim"),
    Parry           UMETA(DisplayName = "Parry Stagger"),
    Throw           UMETA(DisplayName = "Throw Victim")
};


/**
 * Special hit reaction categories (non-directional)
 * Directional reactions use EHitIntensity × EAttackDirection lookup instead
 */
UENUM(BlueprintType)
enum class ESpecialReactionType : uint8
{
    GuardBroken     UMETA(DisplayName = "Guard Broken"),
    Knockdown       UMETA(DisplayName = "Knockdown"),
    Launch          UMETA(DisplayName = "Launch"),
    Death           UMETA(DisplayName = "Death")
};

/**
 * Outcome of a hit reaction - what happens after animation completes
 * Separates "what animation plays" from "what state results"
 * Used to determine post-animation behavior (recovery, ragdoll, etc.)
 */
UENUM(BlueprintType)
enum class EReactionOutcome : uint8
{
    /** Return to idle after stun duration (standard hits) */
    StandardRecovery    UMETA(DisplayName = "Standard Recovery"),

    /** Hold final animation pose permanently (death without ragdoll) */
    Death               UMETA(DisplayName = "Death"),

    /** Blend to ragdoll physics simulation (death with ragdoll) */
    Ragdoll             UMETA(DisplayName = "Ragdoll")

    // Future outcomes:
    // Knockdown - Fall, stay grounded, play get-up
    // Vulnerable - Open to finisher/counter window
};

// ============================================================================
// STRUCTS
// ============================================================================

/**
 * Editor-only: Manual timing values for AttackDataTools notify generation
 * NOT used at runtime - combat system uses AnimNotify_AttackPhaseTransition events
 */
USTRUCT(BlueprintType)
struct FAttackPhaseTimingOverride
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float WindupDuration = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float ActiveDuration = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float RecoveryDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float HoldWindowStart = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float HoldWindowDuration = 0.3f;
};

/**
 * Buffered input for combo system
 */
USTRUCT(BlueprintType)
struct FBufferedInput
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    EInputType Type = EInputType::None;

    UPROPERTY(BlueprintReadWrite)
    FVector2D Direction = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    bool bConsumed = false;

    FBufferedInput() {}
};

/**
 * Attack phase timing configuration
 */
USTRUCT(BlueprintType)
struct FAttackPhaseTiming
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float WindupStart = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float WindupEnd = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float ActiveStart = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float ActiveEnd = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float RecoveryStart = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float RecoveryEnd = 1.0f;

    // Optional phases
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    bool bHasHoldWindow = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", 
              meta = (EditCondition = "bHasHoldWindow"))
    float HoldWindowStart = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", 
              meta = (EditCondition = "bHasHoldWindow"))
    float HoldWindowEnd = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    bool bHasCancelWindow = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", 
              meta = (EditCondition = "bHasCancelWindow"))
    float CancelWindowStart = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", 
              meta = (EditCondition = "bHasCancelWindow"))
    float CancelWindowEnd = 0.6f;
};

/**
 * Target selection scoring data
 */
USTRUCT(BlueprintType)
struct FTargetScore
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<AActor> Target = nullptr;

    UPROPERTY(BlueprintReadOnly)
    float TotalScore = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float DistanceScore = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float DirectionScore = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float FacingScore = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float ThreatScore = 0.0f;
};

/**
 * Hit reaction information passed when applying damage
 */
USTRUCT(BlueprintType)
struct FHitReactionInfo
{
    GENERATED_BODY()

    /** Attacker who dealt the damage */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    TObjectPtr<AActor> Attacker = nullptr;

    /** Direction of the hit (normalized, in world space) */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    FVector HitDirection = FVector::ForwardVector;

    /** Attack data that caused this hit */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    TObjectPtr<UAttackData> AttackData = nullptr;

    /** Final damage amount (after all modifiers) */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    float Damage = 0.0f;

    /** Hitstun duration to apply */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    float StunDuration = 0.0f;

    /** Was this a counter attack (during counter window)? */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    bool bWasCounter = false;

    /** Impact location in world space */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    FVector ImpactPoint = FVector::ZeroVector;

    FHitReactionInfo()
        : Attacker(nullptr)
        , HitDirection(FVector::ForwardVector)
        , AttackData(nullptr)
        , Damage(0.0f)
        , StunDuration(0.0f)
        , bWasCounter(false)
        , ImpactPoint(FVector::ZeroVector)
    {
    }
};

/**
 * Hit reaction animation set based on direction
 * Legacy: Used by HitReactionComponent before HitReactionSettings migration
 */
USTRUCT(BlueprintType)
struct FHitReactionAnimSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions")
    TObjectPtr<UAnimMontage> FrontHit = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions")
    TObjectPtr<UAnimMontage> BackHit = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions")
    TObjectPtr<UAnimMontage> LeftHit = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions")
    TObjectPtr<UAnimMontage> RightHit = nullptr;
};

/**
 * Single montage variant for reaction variety system
 * Pairs a montage with its optional section for per-animation section selection
 */
USTRUCT(BlueprintType)
struct FReactionMontageVariant
{
    GENERATED_BODY()

    /** Animation montage for this variant */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    TObjectPtr<UAnimMontage> Montage = nullptr;

    /** Which section of this montage to use (NAME_None = use entire montage) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    FName MontageSection = NAME_None;

    /** Is this variant configured with a valid montage? */
    bool IsValid() const { return Montage != nullptr; }
};

/**
 * Single hit reaction entry - inline configuration for one reaction
 * Contains all data needed to play a reaction without requiring a separate asset
 * Used inline in FDirectionalReactionSet for directional reactions
 */
USTRUCT(BlueprintType)
struct FHitReactionEntry
{
    GENERATED_BODY()

    // ========================================================================
    // ANIMATION
    // ========================================================================

    /** Animation montage for this reaction (use ReactionMontages array for variety) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    TObjectPtr<UAnimMontage> ReactionMontage = nullptr;

    /** Which section of the single montage to use (NAME_None = use entire montage) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    FName MontageSection = NAME_None;

    /** Array of montage variants for variety (each with its own section selection) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    TArray<FReactionMontageVariant> ReactionMontages;

    /** If true, only this section plays. If false, montage continues after section */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    bool bUseSectionOnly = true;

    /** If true, automatically jump to the section start when playing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    bool bJumpToSectionStart = true;

    /** Montage play rate (1.0 = normal speed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation",
        meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float PlayRate = 1.0f;

    // ========================================================================
    // TIMING
    // ========================================================================

    /** Duration of hitstun (character cannot act) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float StunDuration = 0.3f;

    /** If true, cannot be hit during i-frame window */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    bool bHasIFrames = false;

    /** I-frame start time (relative to montage/section start) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
        meta = (EditCondition = "bHasIFrames", ClampMin = "0.0"))
    float IFrameStart = 0.0f;

    /** I-frame end time (relative to montage/section start) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
        meta = (EditCondition = "bHasIFrames", ClampMin = "0.0"))
    float IFrameEnd = 0.5f;

    // ========================================================================
    // PHYSICS
    // ========================================================================

    /** Knockback force applied to victim */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics",
        meta = (ClampMin = "0.0"))
    float KnockbackForce = 200.0f;

    // ========================================================================
    // OUTCOME (what happens after animation completes)
    // ========================================================================

    /** What happens when this reaction completes */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
    EReactionOutcome Outcome = EReactionOutcome::StandardRecovery;

    /** Blend time from animation to ragdoll (only used when Outcome == Ragdoll) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome",
        meta = (EditCondition = "Outcome == EReactionOutcome::Ragdoll", ClampMin = "0.0", ClampMax = "1.0"))
    float RagdollBlendTime = 0.2f;

    // ========================================================================
    // HELPERS
    // ========================================================================

    /** Is this entry configured with a valid montage? */
    bool IsValid() const { return ReactionMontage != nullptr || ReactionMontages.Num() > 0; }

    /** Check if currently in i-frame window at given time */
    bool IsInIFrameWindow(float CurrentTime) const
    {
        if (!bHasIFrames) return false;
        return CurrentTime >= IFrameStart && CurrentTime <= IFrameEnd;
    }

    /** Get section time range (returns montage length if no section specified) */
    void GetSectionTimeRange(float& OutStart, float& OutEnd) const;

    /** Get section length */
    float GetSectionLength() const;

    // ========================================================================
    // VARIETY HELPERS
    // ========================================================================

    /** Get all available montage variants (combines single + array for compatibility) */
    TArray<FReactionMontageVariant> GetAllVariants() const
    {
        TArray<FReactionMontageVariant> Result;

        // Add array variants first (preferred)
        for (const FReactionMontageVariant& Variant : ReactionMontages)
        {
            if (Variant.IsValid())
            {
                Result.Add(Variant);
            }
        }

        // Add single montage as variant if array is empty (backwards compat)
        if (Result.Num() == 0 && ReactionMontage)
        {
            FReactionMontageVariant SingleVariant;
            SingleVariant.Montage = ReactionMontage;
            SingleVariant.MontageSection = MontageSection;
            Result.Add(SingleVariant);
        }

        return Result;
    }

    /** Get count of available montage variants */
    int32 GetMontageCount() const
    {
        int32 Count = 0;
        for (const FReactionMontageVariant& Variant : ReactionMontages)
        {
            if (Variant.IsValid()) Count++;
        }
        if (Count == 0 && ReactionMontage)
        {
            Count = 1;
        }
        return Count;
    }
};

/**
 * History of played reaction indices for n-2 randomization
 * Tracks last N played montages to exclude from selection
 */
USTRUCT()
struct FReactionHistory
{
    GENERATED_BODY()

    /** Recently played montage indices (most recent at end) */
    TArray<int32> RecentIndices;

    /** Maximum history entries to keep */
    static constexpr int32 MaxHistory = 2;

    /** Record that a montage index was played */
    void RecordPlayed(int32 Index)
    {
        RecentIndices.Add(Index);
        while (RecentIndices.Num() > MaxHistory)
        {
            RecentIndices.RemoveAt(0);
        }
    }

    /** Get the most recently played index (-1 if none) */
    int32 GetLastPlayed() const
    {
        return RecentIndices.Num() > 0 ? RecentIndices.Last() : -1;
    }

    /** Clear history (e.g., on death/respawn) */
    void Clear() { RecentIndices.Empty(); }
};

/**
 * Directional reaction set - reactions for each direction at one intensity level
 * Contains inline FHitReactionEntry for Front/Back/Left/Right
 * No separate assets needed for standard directional reactions
 */
USTRUCT(BlueprintType)
struct FDirectionalReactionSet
{
    GENERATED_BODY()

    /** Front hit reaction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactions")
    FHitReactionEntry Front;

    /** Back hit reaction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactions")
    FHitReactionEntry Back;

    /** Left hit reaction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactions")
    FHitReactionEntry Left;

    /** Right hit reaction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactions")
    FHitReactionEntry Right;

    /** Get reaction entry for direction */
    const FHitReactionEntry* GetReaction(EAttackDirection Direction) const
    {
        switch (Direction)
        {
            case EAttackDirection::Forward: return &Front;
            case EAttackDirection::Backward: return &Back;
            case EAttackDirection::Left: return &Left;
            case EAttackDirection::Right: return &Right;
            default: return &Front; // Default to front for None
        }
    }
};

/**
 * Unified motion warping configuration for attacks
 *
 * Handles both scenarios:
 * 1. TARGET-BASED: When enemy found via soft aim assist → translation + rotation warp
 * 2. DIRECTION-BASED: When no target, just input direction → rotation-only warp
 *
 * Animation Setup:
 * - Add TWO AnimNotifyState_MotionWarping to your montage:
 *   1. "AttackTarget" with bWarpTranslation=true (for enemy targeting)
 *   2. "RotationTarget" with bWarpTranslation=false (for directional rotation)
 * - The system will set up the appropriate target at runtime based on context
 */
USTRUCT(BlueprintType)
struct FAttackWarpConfig
{
    GENERATED_BODY()

    // ========================================================================
    // GENERAL
    // ========================================================================

    /** Enable motion warping for this attack */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp")
    bool bEnableWarp = true;

    /**
     * Skip warp if already facing within this angle of target/direction (degrees)
     * Prevents micro-adjustments when already aligned
     * Set to 0 to always warp regardless of current facing
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp",
        meta = (EditCondition = "bEnableWarp", ClampMin = "0.0", ClampMax = "45.0"))
    float AlreadyFacingThreshold = 15.0f;

    /**
     * When no input direction, only auto-target enemies within this angle of current facing (degrees)
     * Prevents jarring 180° snaps to enemies behind you
     * Set to 180 to allow targeting enemies in any direction
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp",
        meta = (EditCondition = "bEnableWarp", ClampMin = "30.0", ClampMax = "180.0"))
    float NoInputFacingCone = 90.0f;

    // ========================================================================
    // TARGET-BASED WARPING (translation + rotation toward enemy)
    // Used when soft aim assist finds a valid target
    // ========================================================================

    /** Warp target name for translation+rotation (montage notify should have bWarpTranslation=true) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp|Target",
        meta = (EditCondition = "bEnableWarp"))
    FName TargetWarpName = "AttackTarget";

    /** Maximum distance to warp toward target */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp|Target",
        meta = (EditCondition = "bEnableWarp", ClampMin = "0.0", ClampMax = "1000.0"))
    float MaxWarpDistance = 400.0f;

    /** Minimum distance before warping kicks in (prevents micro-warps when already close) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp|Target",
        meta = (EditCondition = "bEnableWarp", ClampMin = "0.0", ClampMax = "200.0"))
    float MinWarpDistance = 50.0f;

    // ========================================================================
    // ROTATION-ONLY WARPING (no translation, just face direction)
    // Used when no target found, only input direction available
    // ========================================================================

    /** Warp target name for rotation-only (montage notify should have bWarpTranslation=false) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp|Rotation",
        meta = (EditCondition = "bEnableWarp"))
    FName RotationWarpName = "RotationTarget";

    /** Rotation speed for directional warps (degrees per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp|Rotation",
        meta = (EditCondition = "bEnableWarp", ClampMin = "90.0", ClampMax = "1800.0"))
    float RotationSpeed = 720.0f;
};

// Backwards compatibility typedef - remove after updating all references
using FDirectionalWarpConfig = FAttackWarpConfig;
using FMotionWarpingConfig = FAttackWarpConfig;

// ============================================================================
// HITSTOP CONFIGURATION
// ============================================================================

/**
 * Per-attack hitstop configuration.
 * Controls the momentary freeze on hit impact that sells attack weight.
 *
 * Industry standard (Sakurai-style): Both attacker and defender freeze via
 * per-actor CustomTimeDilation. Camera, particles, and background actors
 * continue during hitstop.
 *
 * Duration guidelines (60fps reference):
 * - Light:   2-3 frames (0.033-0.050s)
 * - Heavy:   4-6 frames (0.067-0.100s)
 * - Special: 6-10 frames (0.100-0.167s)
 */
USTRUCT(BlueprintType)
struct FHitstopConfig
{
	GENERATED_BODY()

	/** Enable hitstop on hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitstop")
	bool bEnabled = true;

	/** Duration of hitstop freeze in seconds (real wall-clock time, unaffected by time dilation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitstop",
		meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "0.3", UIMin = "0.0", UIMax = "0.2"))
	float Duration = 0.05f;

	/** Camera shake to play on the player during hitstop (nullptr = no shake) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitstop",
		meta = (EditCondition = "bEnabled"))
	TSubclassOf<UCameraShakeBase> CameraShake;

	/** Camera shake intensity scale (1.0 = full intensity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitstop",
		meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "3.0"))
	float CameraShakeScale = 1.0f;

	/** Whether to apply hitstop when the attack is blocked (reduced duration) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitstop|Block",
		meta = (EditCondition = "bEnabled"))
	bool bApplyOnBlock = true;

	/** Duration multiplier when attack is blocked (0.5 = half the normal hitstop) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitstop|Block",
		meta = (EditCondition = "bEnabled && bApplyOnBlock", ClampMin = "0.0", ClampMax = "1.0"))
	float BlockedDurationMultiplier = 0.5f;

	/** Create default hitstop config based on attack type */
	static FHitstopConfig CreateDefault(EAttackType AttackType)
	{
		FHitstopConfig Config;
		Config.bEnabled = true;
		switch (AttackType)
		{
		case EAttackType::Light:
			Config.Duration = 0.04f;    // ~2.4 frames at 60fps
			Config.CameraShakeScale = 0.5f;
			break;
		case EAttackType::Heavy:
			Config.Duration = 0.083f;   // ~5 frames at 60fps
			Config.CameraShakeScale = 1.0f;
			break;
		case EAttackType::Special:
			Config.Duration = 0.1f;     // ~6 frames at 60fps
			Config.CameraShakeScale = 1.5f;
			break;
		default:
			Config.Duration = 0.05f;    // ~3 frames at 60fps
			Config.CameraShakeScale = 0.5f;
			break;
		}
		return Config;
	}

	/** Is this config active (enabled with positive duration)? */
	bool IsActive() const { return bEnabled && Duration > 0.0f; }
};

#if WITH_AUTOMATION_TESTS
/**
 * Debug arrow information for testing
 * Contains all data needed to verify an arrow's position, style, and appearance
 */
struct FDebugArrowInfo
{
	FVector StartPosition;
	FVector EndPosition;
	FVector LabelPosition;
	FString Label;
	FColor Color;
	float Thickness;
	bool bIsDashed;
	float Length;

	FDebugArrowInfo()
		: StartPosition(FVector::ZeroVector)
		, EndPosition(FVector::ZeroVector)
		, LabelPosition(FVector::ZeroVector)
		, Label(TEXT(""))
		, Color(FColor::White)
		, Thickness(1.0f)
		, bIsDashed(false)
		, Length(0.0f)
	{}
};

/**
 * Complete debug visualization data for testing
 * Allows unit tests to verify positioning, coloring, and visibility logic
 * without requiring actual rendering
 */
struct FDebugVisualizationData
{
	TArray<FDebugArrowInfo> Arrows;
	TArray<FVector> ArcPoints;
	FString HoldStateLabel;
	bool bShowHoldIndicator;
	FVector ChestOffset;
	float YawDelta;

	FDebugVisualizationData()
		: HoldStateLabel(TEXT(""))
		, bShowHoldIndicator(false)
		, ChestOffset(FVector::ZeroVector)
		, YawDelta(0.0f)
	{}
};
#endif // WITH_AUTOMATION_TESTS

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, ECombatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttackHit, AActor*, HitActor, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPostureChanged, float, NewPosture);

// Combat System Event Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAttackStarted, UAttackData*, AttackData, EInputType, InputType, bool, bIsCombo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPhaseChanged, EAttackPhase, OldPhase, EAttackPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboWindowChanged, bool, bActive, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHoldActivated, EInputType, InputType, float, HoldDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMontageEvent, UAnimMontage*, Montage, bool, bInterrupted, FName, EventName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGuardBroken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerfectParry, AActor*, ParriedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerfectEvade, AActor*, EvadedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFinisherAvailable, AActor*, Target);

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
// Direction conversion and rotation helpers have been moved to:
// - UCombatUtils (Utilities/CombatUtils.h) - Core combat utility functions
// - UDebugUtils (Debug/DebugUtils.h) - Debug-specific utilities
// ============================================================================