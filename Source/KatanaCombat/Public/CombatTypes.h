
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
class USoundBase;
class UNiagaraSystem;
class UPairedAnimationData;

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
    GuardBroken     UMETA(DisplayName = "Guard Broken (Deprecated)"),
    Staggered       UMETA(DisplayName = "Staggered"),
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

/**
 * Counter system mode for AB testing
 * Determines the flow and feel of the counter mechanic
 */
UENUM(BlueprintType)
enum class ECounterSystemMode : uint8
{
    /** AC3/Arkham style: One-step pose-matched counter-kills */
    AC3             UMETA(DisplayName = "AC3 (One-Step Counter-Kill)"),

    /** Chain style: Three-step parry → counter → finisher flow */
    Chain           UMETA(DisplayName = "Chain (Parry → Counter → Finisher)")
};

/**
 * Chain counter state machine states
 * Only used when ECounterSystemMode::Chain is active
 */
UENUM(BlueprintType)
enum class EChainCounterState : uint8
{
    /** Not in counter chain */
    None            UMETA(DisplayName = "None"),

    /** Playing parry (deflection) animation */
    ParryActive     UMETA(DisplayName = "Parry Active"),

    /** Slow-mo decision window - waiting for Light/Heavy/Nothing input */
    CounterWindow   UMETA(DisplayName = "Counter Window"),

    /** Playing counter (riposte) animation */
    CounterActive   UMETA(DisplayName = "Counter Active"),

    /** Enemy vulnerable, finisher available */
    FinisherReady   UMETA(DisplayName = "Finisher Ready")
};

/**
 * Swing direction for procedural pose-matching
 * Different from EAttackDirection (which is movement/targeting direction)
 * Used to select appropriate counter animation based on attack trajectory
 */
UENUM(BlueprintType)
enum class ESwingDirection : uint8
{
    /** Side-to-side swings (slashes) */
    Horizontal      UMETA(DisplayName = "Horizontal"),

    /** Overhead or uppercut attacks */
    Vertical        UMETA(DisplayName = "Vertical"),

    /** Stabs, lunges, piercing attacks */
    Thrust          UMETA(DisplayName = "Thrust"),

    /** Low sweeping attacks */
    Sweep           UMETA(DisplayName = "Sweep"),

    /** Unarmed grabs, grapples */
    Grab            UMETA(DisplayName = "Grab")
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

    /** Impact surface normal (for VFX alignment, blood decals) */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    FVector ImpactNormal = FVector::UpVector;

    /** Bone hit by the attack (for attached effects, ragdoll impulse) */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction")
    FName BoneName = NAME_None;

    // ========================================================================
    // HIT-1: Extended hit metadata (for VFX, knockback, analytics)
    // ========================================================================

    /** Attacker's montage position at moment of hit (for synchronized reactions) */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction|Metadata")
    float AnimationTime = 0.0f;

    /** Weapon velocity at moment of impact (for directional VFX, knockback scaling) */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction|Metadata")
    FVector WeaponVelocity = FVector::ZeroVector;

    /** Attack phase when hit connected (Active, Recovery, etc.) */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction|Metadata")
    EAttackPhase PhaseWhenHit = EAttackPhase::None;

    /** Distance from attacker to target at moment of hit */
    UPROPERTY(BlueprintReadWrite, Category = "Hit Reaction|Metadata")
    float DistanceToTarget = 0.0f;

    FHitReactionInfo()
        : Attacker(nullptr)
        , HitDirection(FVector::ForwardVector)
        , AttackData(nullptr)
        , Damage(0.0f)
        , StunDuration(0.0f)
        , bWasCounter(false)
        , ImpactPoint(FVector::ZeroVector)
        , ImpactNormal(FVector::UpVector)
        , BoneName(NAME_None)
        , AnimationTime(0.0f)
        , WeaponVelocity(FVector::ZeroVector)
        , PhaseWhenHit(EAttackPhase::None)
        , DistanceToTarget(0.0f)
    {
    }
};

/**
 * Counter context - information about an incoming attack that can be countered
 * Passed to counter animation selection and procedural adjustment systems
 */
USTRUCT(BlueprintType)
struct FCounterContext
{
    GENERATED_BODY()

    /** The enemy currently attacking (owner of the counter window) */
    UPROPERTY(BlueprintReadOnly, Category = "Counter")
    TObjectPtr<AActor> Attacker = nullptr;

    /** Attack type for animation pool selection */
    UPROPERTY(BlueprintReadOnly, Category = "Counter")
    EAttackType AttackType = EAttackType::Light;

    /** Swing direction for procedural pose-matching */
    UPROPERTY(BlueprintReadOnly, Category = "Counter")
    ESwingDirection SwingDirection = ESwingDirection::Horizontal;

    /** Specific counter animation for this attack (if configured on AnimNotifyState) */
    UPROPERTY(BlueprintReadOnly, Category = "Counter")
    TObjectPtr<UPairedAnimationData> SpecificCounterData = nullptr;

    /** Time spent in counter window (for perfect counter detection) */
    UPROPERTY(BlueprintReadOnly, Category = "Counter")
    float TimeInWindow = 0.0f;

    /** Total duration of the counter window */
    UPROPERTY(BlueprintReadOnly, Category = "Counter")
    float WindowDuration = 0.5f;

    /** Is this context currently valid (has active attacker in window)? */
    bool IsValid() const { return Attacker != nullptr; }

    /** Calculate percentage through window (0.0 = start, 1.0 = end) */
    float GetWindowProgress() const
    {
        return WindowDuration > 0.0f ? FMath::Clamp(TimeInWindow / WindowDuration, 0.0f, 1.0f) : 0.0f;
    }

    /** Check if currently in perfect counter timing (last 20% of window) */
    bool IsPerfectTiming() const
    {
        return GetWindowProgress() >= 0.8f;
    }

    void Reset()
    {
        Attacker = nullptr;
        AttackType = EAttackType::Light;
        SwingDirection = ESwingDirection::Horizontal;
        SpecificCounterData = nullptr;
        TimeInWindow = 0.0f;
        WindowDuration = 0.5f;
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

    /** Duration of hitstun (character cannot act). Default 0 = no stun.
     * Only configure stun for specific scenarios (guard break, heavy attacks, etc.)
     * Non-zero stun makes target vulnerable to finishers via IsVulnerableToFinisher(). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float StunDuration = 0.0f;

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

// ============================================================================
// IMPACT AUDIO CONFIG
// ============================================================================

/**
 * Per-attack impact audio configuration.
 * Supports primary sound with optional weapon fallback, pitch variation.
 *
 * Resolution order: ImpactSound → WeaponData::HitSound → nothing.
 * Pitch variation prevents repetition (industry standard: ±5%).
 */
USTRUCT(BlueprintType)
struct FImpactAudioConfig
{
	GENERATED_BODY()

	/** Primary impact sound (plays at hit location via spatial audio) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> ImpactSound = nullptr;

	/** Volume multiplier for impact sound */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio",
		meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float VolumeMultiplier = 1.0f;

	/** Base pitch multiplier for impact sound */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio",
		meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float PitchMultiplier = 1.0f;

	/** Random pitch variation (±range from base pitch, prevents repetition) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio",
		meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PitchVariation = 0.05f;

	/** If true and ImpactSound is null, use WeaponData::HitSound as fallback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	bool bUseWeaponFallback = true;

	/** Check if this config will produce audio (has sound or allows fallback) */
	bool IsActive() const { return ImpactSound != nullptr || bUseWeaponFallback; }

	/** Get pitch with random variation applied */
	float GetRandomizedPitch() const
	{
		const float Variation = FMath::FRandRange(-PitchVariation, PitchVariation);
		return FMath::Clamp(PitchMultiplier + Variation, 0.1f, 4.0f);
	}
};

// ============================================================================
// IMPACT VFX CONFIG (U-16)
// ============================================================================

/**
 * Per-attack impact VFX configuration.
 * Supports Niagara effects with surface alignment and scale options.
 */
USTRUCT(BlueprintType)
struct FImpactVFXConfig
{
	GENERATED_BODY()

	/** Niagara system to spawn at impact point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TObjectPtr<UNiagaraSystem> ImpactVFX = nullptr;

	/** Scale multiplier for spawned VFX */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX",
		meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float ScaleMultiplier = 1.0f;

	/** Align VFX rotation to impact surface normal (vs. always world up) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	bool bAlignToSurface = true;

	/** If true and ImpactVFX is null, use WeaponData::HitVFX as fallback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	bool bUseWeaponFallback = true;

	/** Check if this config will produce VFX (has system or allows fallback) */
	bool IsActive() const { return ImpactVFX != nullptr || bUseWeaponFallback; }
};

// ============================================================================
// COMBAT SURFACE TYPE (Scaffold - not wired until bReturnPhysicalMaterial enabled)
// ============================================================================

/**
 * Physical surface type for surface-specific FX.
 * [SCAFFOLD] - Not wired. WeaponComponent trace has bReturnPhysicalMaterial = false.
 * Wire when PhysMaterial -> ECombatSurfaceType mapping is implemented.
 */
UENUM(BlueprintType)
enum class ECombatSurfaceType : uint8
{
	Default		UMETA(DisplayName = "Default"),
	Flesh		UMETA(DisplayName = "Flesh"),
	Armor		UMETA(DisplayName = "Armor"),
	Wood		UMETA(DisplayName = "Wood"),
	Stone		UMETA(DisplayName = "Stone"),
	Metal		UMETA(DisplayName = "Metal")
};

// ============================================================================
// IMPACT FX POOL ENTRIES (Single items in a pool)
// ============================================================================

/**
 * Single sound entry in an FX pool.
 * Pooled sounds allow random selection for audio variety.
 */
USTRUCT(BlueprintType)
struct FImpactSoundEntry
{
	GENERATED_BODY()

	/** Sound to play at impact location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> Sound = nullptr;

	/** Volume multiplier for this sound */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio",
		meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float VolumeMultiplier = 1.0f;

	/** Base pitch multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio",
		meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float PitchMultiplier = 1.0f;

	/** Is this entry valid (has a sound)? */
	bool IsValid() const { return Sound != nullptr; }
};

/**
 * Single VFX entry in an FX pool.
 * Pooled VFX allows random selection for visual variety.
 */
USTRUCT(BlueprintType)
struct FImpactVFXEntry
{
	GENERATED_BODY()

	/** Niagara system to spawn at impact point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TObjectPtr<UNiagaraSystem> VFX = nullptr;

	/** Scale multiplier for this VFX */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX",
		meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float ScaleMultiplier = 1.0f;

	/** Is this entry valid (has VFX)? */
	bool IsValid() const { return VFX != nullptr; }
};

// ============================================================================
// IMPACT FX POOL (Collection of sounds/VFX for one attack type)
// ============================================================================

/**
 * Pool of impact sounds and VFX for random selection.
 * One pool per EAttackType allows type-specific audio character.
 *
 * Usage:
 * - Configure Light pool with 3-5 light slash sounds
 * - Configure Heavy pool with 2-3 meaty impact sounds
 * - System selects randomly at runtime for variety
 */
USTRUCT(BlueprintType)
struct FImpactFXPool
{
	GENERATED_BODY()

	/** Pool of impact sounds (random selection at runtime) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TArray<FImpactSoundEntry> ImpactSounds;

	/** Pool of impact VFX (random selection at runtime) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TArray<FImpactVFXEntry> ImpactVFX;

	/** Pitch variation applied on top of selected entry's pitch (±range) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio",
		meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PitchVariation = 0.05f;

	/** Align VFX rotation to impact surface normal (pool default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	bool bAlignVFXToSurface = true;

	/** Get a random valid sound entry (nullptr if pool empty or no valid entries) */
	const FImpactSoundEntry* GetRandomSound() const
	{
		TArray<int32> ValidIndices;
		for (int32 i = 0; i < ImpactSounds.Num(); ++i)
		{
			if (ImpactSounds[i].IsValid())
			{
				ValidIndices.Add(i);
			}
		}
		if (ValidIndices.Num() == 0)
		{
			return nullptr;
		}
		return &ImpactSounds[ValidIndices[FMath::RandRange(0, ValidIndices.Num() - 1)]];
	}

	/** Get a random valid VFX entry (nullptr if pool empty) [SCAFFOLD] */
	const FImpactVFXEntry* GetRandomVFX() const
	{
		TArray<int32> ValidIndices;
		for (int32 i = 0; i < ImpactVFX.Num(); ++i)
		{
			if (ImpactVFX[i].IsValid())
			{
				ValidIndices.Add(i);
			}
		}
		if (ValidIndices.Num() == 0)
		{
			return nullptr;
		}
		return &ImpactVFX[ValidIndices[FMath::RandRange(0, ValidIndices.Num() - 1)]];
	}

	/** Check if pool has any valid sounds */
	bool HasSounds() const
	{
		for (const auto& Entry : ImpactSounds)
		{
			if (Entry.IsValid())
			{
				return true;
			}
		}
		return false;
	}

	/** Check if pool has any valid VFX [SCAFFOLD] */
	bool HasVFX() const
	{
		for (const auto& Entry : ImpactVFX)
		{
			if (Entry.IsValid())
			{
				return true;
			}
		}
		return false;
	}
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
// ATTACK STATE MACHINE
// ============================================================================

/**
 * Attack lifecycle state - tracks where we are in an attack's execution
 * This is INTERNAL to the state machine, not the same as EAttackPhase
 */
UENUM(BlueprintType)
enum class EAttackLifecycleState : uint8
{
	/** No attack in progress */
	Idle,
	/** Attack initiated, montage starting */
	Starting,
	/** Attack in progress (montage playing) */
	InProgress,
	/** Old montage blending out to combo attack */
	ComboBlending,
	/** Attack was interrupted (death, stun, paired anim) */
	Interrupted
};

/**
 * Centralized state machine for attack lifecycle management
 *
 * Replaces scattered flags (bInComboBlend, BlendTransitionEndTime, etc.)
 * with clean, unified state tracking. Key responsibility: determine whether
 * a montage callback should be processed or ignored.
 *
 * DESIGN PRINCIPLE: Each attack has an "owner" montage AND section. Callbacks from
 * non-owner montages (old combos blending out) are ignored.
 *
 * CRITICAL COMPLICATION:
 * All light attacks share the SAME montage (AM_Light_Combo_1) with different sections.
 * Pointer comparison alone fails - we must also use TIME-BASED grace periods
 * to handle section-to-section transitions within the same montage.
 *
 * Usage:
 * - Call OnAttackStarted() when beginning new attack
 * - Call OnComboTransition() when transitioning to combo
 * - Call ShouldProcessMontageEnd() in OnMontageEnded to filter callbacks
 * - Call OnMontageEndProcessed() after handling the callback
 */
USTRUCT(BlueprintType)
struct FAttackStateMachine
{
	GENERATED_BODY()

	// ========================================================================
	// STATE
	// ========================================================================

	/** Current lifecycle state */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EAttackLifecycleState LifecycleState = EAttackLifecycleState::Idle;

	/** Current attack phase (from notify system) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EAttackPhase CurrentPhase = EAttackPhase::None;

	/** The montage this state machine is tracking (owner montage) */
	UPROPERTY()
	TWeakObjectPtr<UAnimMontage> ActiveMontage;

	/** The section name we're playing (for logging/debugging) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	FName ActiveSectionName = NAME_None;

	/** Previous montage (during combo blend) */
	UPROPERTY()
	TWeakObjectPtr<UAnimMontage> PreviousMontage;

	/** Generation ID - incremented on each new attack, used for staleness detection */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int32 AttackGeneration = 0;

	/**
	 * Number of combo transitions that haven't had their OnMontageEnded callback
	 * processed yet. Incremented in OnComboTransition(), decremented when an
	 * interrupted callback is rejected. Used by ShouldProcessMontageEnd() to know
	 * that an interrupted callback is from a previous attack in the combo chain.
	 *
	 * For same-montage section transitions (Attack_1→2→3 in AM_Light_Combo_1),
	 * the montage pointer is the same for old and new — this counter is the only
	 * reliable way to detect staleness regardless of timing.
	 */
	int32 PendingComboTransitions = 0;

	/** Time when combo blend will complete (world time seconds) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float ComboBlendEndTime = 0.0f;

	/** Time when current section started (for grace period protection) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float SectionStartTime = 0.0f;

	/** Grace period after section start - ignore interrupted callbacks during this time (seconds) */
	static constexpr float SectionTransitionGracePeriod = 0.15f;

	// ========================================================================
	// API - ATTACK LIFECYCLE
	// ========================================================================

	/**
	 * Called when a new attack starts (not a combo)
	 * Resets state machine and sets the new montage as owner
	 */
	void OnAttackStarted(UAnimMontage* Montage, FName SectionName, float CurrentWorldTime)
	{
		LifecycleState = EAttackLifecycleState::Starting;
		CurrentPhase = EAttackPhase::None;
		ActiveMontage = Montage;
		ActiveSectionName = SectionName;
		PreviousMontage = nullptr;
		AttackGeneration++;
		PendingComboTransitions = 0; // Fresh attack — no pending transitions
		ComboBlendEndTime = 0.0f;
		SectionStartTime = CurrentWorldTime;
	}

	/**
	 * Called when transitioning from one attack to a combo attack
	 * Tracks the old montage so we can ignore its callbacks
	 *
	 * @param NewMontage The new combo montage starting
	 * @param NewSectionName The section we're transitioning to
	 * @param BlendDuration How long the blend will take
	 * @param CurrentWorldTime Current world time for timeout calculation
	 */
	void OnComboTransition(UAnimMontage* NewMontage, FName NewSectionName, float BlendDuration, float CurrentWorldTime)
	{
		PreviousMontage = ActiveMontage;
		ActiveMontage = NewMontage;
		ActiveSectionName = NewSectionName;
		AttackGeneration++;
		PendingComboTransitions++; // Old montage will fire OnMontageEnded later — expect it
		LifecycleState = EAttackLifecycleState::ComboBlending;
		ComboBlendEndTime = CurrentWorldTime + BlendDuration;
		SectionStartTime = CurrentWorldTime;
	}

	/**
	 * Called when combo blend completes (new montage fully started)
	 */
	void OnComboBlendComplete()
	{
		PreviousMontage = nullptr;
		if (LifecycleState == EAttackLifecycleState::ComboBlending)
		{
			LifecycleState = EAttackLifecycleState::InProgress;
		}
	}

	/**
	 * Called when phase transitions (from AnimNotify)
	 */
	void OnPhaseChanged(EAttackPhase NewPhase)
	{
		CurrentPhase = NewPhase;

		// Windup start means attack is now in progress
		if (NewPhase == EAttackPhase::Windup && LifecycleState == EAttackLifecycleState::Starting)
		{
			LifecycleState = EAttackLifecycleState::InProgress;
		}
	}

	// ========================================================================
	// API - MONTAGE CALLBACK FILTERING
	// ========================================================================

	/**
	 * CRITICAL: Determines if a montage end callback should be processed.
	 *
	 * This is the SINGLE AUTHORITY for rejecting stale async callbacks.
	 * No boolean guards or ad-hoc checks elsewhere in OnMontageEnded —
	 * if this returns false, the callback is completely ignored.
	 *
	 * RULES (in priority order):
	 * 0. Pending transition counter: Each OnComboTransition() increments
	 *    PendingComboTransitions. Each rejected stale callback decrements it.
	 *    If > 0 on an interrupted callback, an old montage's async blend-out
	 *    callback just arrived — consume it and reject. This handles ALL cases:
	 *    same-montage sections, cross-montage combos, any blend timing.
	 * 1. Grace period: During first 150ms after section start, IGNORE interrupted
	 *    callbacks (same-montage section transitions within AM_Light_Combo_1, etc.)
	 * 2. Different montage: If not our active montage pointer, ignore
	 * 3. Combo blend time window: Additional protection during blend
	 *
	 * @param EndedMontage The montage that just ended
	 * @param bInterrupted Was it interrupted?
	 * @param CurrentWorldTime Current world time for timeout check
	 * @return true if this callback should be processed, false to ignore
	 */
	bool ShouldProcessMontageEnd(UAnimMontage* EndedMontage, bool bInterrupted, float CurrentWorldTime)
	{
		// RULE 0: Pending combo transition check (INPUT-1 systemic fix)
		// Each combo transition (OnComboTransition) increments a counter.
		// When an interrupted callback arrives, if we have pending transitions,
		// this callback is from an old montage that was intentionally stopped.
		// Consume one pending transition and reject the callback.
		// This works for BOTH same-montage section transitions AND cross-montage
		// combos, regardless of async timing — no heuristics needed.
		if (bInterrupted && PendingComboTransitions > 0)
		{
			PendingComboTransitions--;
			return false;  // Consumed stale combo-transition callback
		}

		// RULE 1: Grace period protection after section/attack change
		// Handles same-montage section transitions (all light attacks share AM_Light_Combo_1)
		// Old section's callback arrives after new section starts - grace period catches this
		if (bInterrupted && CurrentWorldTime < SectionStartTime + SectionTransitionGracePeriod)
		{
			return false;  // Interrupted during grace period = stale callback from old section
		}

		// RULE 2: Montage pointer check (handles different-montage transitions)
		if (EndedMontage != ActiveMontage.Get())
		{
			return false;  // Not our montage - ignore completely
		}

		// RULE 3: Combo blend time window protection (belt-and-suspenders)
		// Even if same montage, if we're in blend window with interrupt, ignore
		if (bInterrupted && CurrentWorldTime < ComboBlendEndTime)
		{
			return false;
		}

		// This is a valid callback from our active montage - process it
		return true;
	}

	/**
	 * Called after processing a montage end callback
	 * Updates state machine to reflect the end
	 */
	void OnMontageEndProcessed(UAnimMontage* EndedMontage, bool bInterrupted)
	{
		if (bInterrupted)
		{
			LifecycleState = EAttackLifecycleState::Interrupted;
		}
		else
		{
			LifecycleState = EAttackLifecycleState::Idle;
		}
		CurrentPhase = EAttackPhase::None;
	}

	/**
	 * Force reset to idle (used by external systems like death)
	 */
	void ForceReset()
	{
		LifecycleState = EAttackLifecycleState::Idle;
		CurrentPhase = EAttackPhase::None;
		ActiveMontage = nullptr;
		ActiveSectionName = NAME_None;
		PreviousMontage = nullptr;
		PendingComboTransitions = 0;
		ComboBlendEndTime = 0.0f;
		SectionStartTime = 0.0f;
	}

	// ========================================================================
	// QUERIES
	// ========================================================================

	/** Is an attack currently in progress? */
	bool IsAttackActive() const
	{
		return LifecycleState == EAttackLifecycleState::Starting ||
			   LifecycleState == EAttackLifecycleState::InProgress ||
			   LifecycleState == EAttackLifecycleState::ComboBlending;
	}

	/** Are we in the middle of a combo blend? */
	bool IsComboBlending() const
	{
		return LifecycleState == EAttackLifecycleState::ComboBlending;
	}

	/** Get the current owner montage */
	UAnimMontage* GetActiveMontage() const
	{
		return ActiveMontage.Get();
	}

	/** Check if a specific montage is the current owner */
	bool IsOwnerMontage(UAnimMontage* Montage) const
	{
		return ActiveMontage.Get() == Montage;
	}
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, ECombatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttackHit, AActor*, HitActor, const FHitReactionInfo&, HitInfo);
// DEPRECATED: Posture system removed - use health-based contextual stagger instead
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPostureChanged, float, NewPosture);

// Combat System Event Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAttackStarted, UAttackData*, AttackData, EInputType, InputType, bool, bIsCombo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPhaseChanged, EAttackPhase, OldPhase, EAttackPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboWindowChanged, bool, bActive, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHoldActivated, EInputType, InputType, float, HoldDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMontageEvent, UAnimMontage*, Montage, bool, bInterrupted, FName, EventName);
// DEPRECATED: Guard break removed - use FOnStaggered instead
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGuardBroken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaggered, AActor*, StaggeredActor, float, Duration);
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