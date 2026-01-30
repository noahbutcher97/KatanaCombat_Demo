// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"
#include "PairedAnimationTypes.generated.h"

// Forward declarations
class UAnimMontage;
class UPairedAnimationData;

// ============================================================================
// ENUMS
// ============================================================================

/**
 * Reason why an enemy is vulnerable to a finisher
 * Used for UI feedback and determining finisher type
 */
UENUM(BlueprintType)
enum class EFinisherTriggerReason : uint8
{
    None            UMETA(DisplayName = "None"),
    LowHealth       UMETA(DisplayName = "Low Health"),
    GuardBroken     UMETA(DisplayName = "Guard Broken"),
    Stunned         UMETA(DisplayName = "Stunned")
};

// ============================================================================
// STRUCTS
// ============================================================================

/**
 * Motion warping configuration for paired animations (finishers, counters)
 * Used for both attacker and victim positioning during synced animations
 * Simpler than FAttackWarpConfig - focused on paired sync rather than targeting
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FPairedWarpConfig
{
    GENERATED_BODY()

    /** Name of the warp target in the montage's MotionWarping notify */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paired Animation")
    FName WarpTargetName = "PairedTarget";

    /** Maximum distance to warp (prevents warping through walls) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paired Animation",
        meta = (ClampMin = "0.0", ClampMax = "500.0"))
    float MaxWarpDistance = 300.0f;

    /** Enable translation warping (move to position) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paired Animation")
    bool bWarpTranslation = true;

    /** Enable rotation warping (face toward/away from partner) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paired Animation")
    bool bWarpRotation = true;

    /** Adjust warp target Z to match terrain height (prevents floating) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paired Animation")
    bool bAdjustToTerrain = true;
};

/**
 * Configuration for finisher trigger conditions
 * Multiple conditions can be enabled - any matching condition triggers vulnerability
 * Design goal: Easy to trigger, flashy to execute (AC3 style)
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FFinisherTriggerConfig
{
    GENERATED_BODY()

    // ========================================================================
    // LOW HEALTH TRIGGER
    // ========================================================================

    /** Enable finisher when enemy health drops below threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Triggers|Health")
    bool bTriggerOnLowHealth = true;

    /** Health percentage threshold (0.0-1.0, 0.25 = 25%) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Triggers|Health",
        meta = (EditCondition = "bTriggerOnLowHealth", ClampMin = "0.0", ClampMax = "1.0"))
    float HealthThreshold = 0.25f;

    // ========================================================================
    // GUARD BREAK TRIGGER
    // ========================================================================

    /** Enable finisher when enemy's posture is depleted */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Triggers|Posture")
    bool bTriggerOnGuardBreak = true;

    // ========================================================================
    // STUN TRIGGER
    // ========================================================================

    /** Enable finisher on enemies in hitstun from heavy attacks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Triggers|Stun")
    bool bTriggerOnStun = true;

    /** Minimum stun duration remaining for finisher eligibility */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Triggers|Stun",
        meta = (EditCondition = "bTriggerOnStun", ClampMin = "0.0", ClampMax = "2.0"))
    float MinStunTimeRemaining = 0.3f;

    // ========================================================================
    // FEEDBACK
    // ========================================================================

    /** Show UI prompt when finisher is available */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
    bool bShowFinisherPrompt = true;

    /** Slow motion scale when entering finisher (0 = disabled) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SlowMotionScale = 0.3f;

    /** Duration of slow motion effect */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback",
        meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float SlowMotionDuration = 0.5f;
};

// ============================================================================
// DELEGATES
// ============================================================================

/** Broadcast when a paired animation sequence begins */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPairedAnimationStarted, EPairedReactionType, Type, bool, bIsCriticalMoment);

/** Broadcast when a sync point is reached during paired animation */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPairedAnimationSyncPoint, EPairedReactionType, Type, FName, SyncPointName);

/** Broadcast when a paired animation sequence ends */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPairedAnimationEnded, EPairedReactionType, Type);
