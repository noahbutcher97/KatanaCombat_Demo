
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
 * Directional input for attacks and targeting
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
 * Timing fallback strategy when AnimNotifyStates are missing
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
 * Manual timing override when not using AnimNotifyStates
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
 * Motion warping configuration for an attack
 */
USTRUCT(BlueprintType)
struct FMotionWarpingConfig
{
    GENERATED_BODY()

    /** Use motion warping for this attack */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    bool bUseMotionWarping = true;

    /** Name of the warp target (must match in animation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    FName MotionWarpingTargetName = "AttackTarget";

    /** Minimum distance to target before warping kicks in */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    float MinWarpDistance = 50.0f;

    /** Maximum distance we'll chase the target */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    float MaxWarpDistance = 400.0f;

    /** Speed of rotation toward target (degrees per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    float WarpRotationSpeed = 720.0f;

    /** Warp position toward target */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    bool bWarpTranslation = true;

    /** Require line of sight to target */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warping")
    bool bRequireLineOfSight = true;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, ECombatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttackHit, AActor*, HitActor, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPostureChanged, float, NewPosture);

// V2 Combat System Event Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnV2AttackStarted, UAttackData*, AttackData, EInputType, InputType, bool, bIsCombo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnV2PhaseChanged, EAttackPhase, OldPhase, EAttackPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnV2ComboWindowChanged, bool, bActive, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnV2HoldActivated, EInputType, InputType, float, HoldDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnV2MontageEvent, UAnimMontage*, Montage, bool, bInterrupted, FName, EventName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGuardBroken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerfectParry, AActor*, ParriedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerfectEvade, AActor*, EvadedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFinisherAvailable, AActor*, Target);