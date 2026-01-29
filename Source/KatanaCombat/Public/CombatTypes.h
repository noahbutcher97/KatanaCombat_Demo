
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.generated.h"

// Forward declarations
class UAttackData;
class UAnimMontage;
class AActor;

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
 */
UENUM(BlueprintType)
enum class EHitReactionType : uint8
{
    None            UMETA(DisplayName = "No Reaction"),
    Flinch          UMETA(DisplayName = "Flinch"),
    Light           UMETA(DisplayName = "Light Stagger"),
    Medium          UMETA(DisplayName = "Medium Stagger"),
    Heavy           UMETA(DisplayName = "Heavy Stagger"),
    Knockback       UMETA(DisplayName = "Knockback"),
    Knockdown       UMETA(DisplayName = "Knockdown"),
    Launch          UMETA(DisplayName = "Launch"),
    Custom          UMETA(DisplayName = "Custom Reaction")
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
 * Hit reaction configuration data
 */
USTRUCT(BlueprintType)
struct FHitReactionData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
    EHitReactionType ReactionType = EHitReactionType::Light;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
    float StunDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float KnockbackForce = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float LaunchForce = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", 
              meta = (EditCondition = "ReactionType == EHitReactionType::Custom"))
    TObjectPtr<UAnimMontage> CustomReactionMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
    bool bForceInterruptCurrentAction = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
    bool bCanBeBlocked = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
    bool bCanBeParried = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
    bool bUnblockable = false;
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