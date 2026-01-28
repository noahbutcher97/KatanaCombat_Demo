// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatSettings.generated.h"

/**
 * Global combat tuning values
 * Use this data asset to configure posture, timing, and other combat parameters
 */
UCLASS(BlueprintType)
class KATANACOMBAT_API UCombatSettings : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UCombatSettings();

    // ============================================================================
    // SYSTEM CONFIGURATION
    // ============================================================================

    // Debug visualization is now controlled via CVars (see DebugConfig.h):
    // Combat.Debug.All 1         - Enable all debug visualization
    // Combat.Debug.Direction 1   - Direction transformation arrows
    // Combat.Debug.Targeting 1   - Targeting cones and targets
    // Combat.Debug.Weapon 1      - Weapon trace visualization

    // ============================================================================
    // POSTURE SYSTEM
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture")
    float MaxPosture = 100.0f;

    /** Posture regeneration while attacking (rewards aggression) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture")
    float PostureRegenRate_Attacking = 50.0f;

    /** Posture regeneration while not blocking (neutral stance) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture")
    float PostureRegenRate_NotBlocking = 30.0f;

    /** Posture regeneration while idle (passive recovery) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture")
    float PostureRegenRate_Idle = 20.0f;

    /** How long to stun when posture breaks */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture")
    float GuardBreakStunDuration = 2.0f;

    /** Percentage of max posture recovered after guard break (0.5 = 50%) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture", 
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GuardBreakRecoveryPercent = 0.5f;

    // ============================================================================
    // ATTACK CONFIGURATION
    // ============================================================================

    /** Attack moveset configuration (default attacks, movement attacks) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attacks")
    TObjectPtr<class UAttackConfiguration> AttackConfiguration;

    // ============================================================================
    // COUNTER SYSTEM
    // ============================================================================

    /** How long counter window stays open after parry/evade */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counter")
    float CounterWindowDuration = 1.5f;

    /** Damage multiplier during counter window */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counter")
    float CounterDamageMultiplier = 1.5f;

    // ============================================================================
    // MOTION WARPING DEFAULTS
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
    float DefaultMaxWarpDistance = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
    float DefaultMinWarpDistance = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
    float DefaultDirectionalConeAngle = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
    float DefaultWarpRotationSpeed = 720.0f;

    // ============================================================================
    // DIRECTIONAL TARGETING (Soft Target / Aim Assist)
    // ============================================================================

    /** Max range to consider enemies for directional targeting */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional Targeting")
    float DirectionalTargetingRange = 500.0f;

    /** Angle threshold (degrees) - enemies within this angle of input direction are candidates */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional Targeting", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float GradientAngleThreshold = 45.0f;

    /** Angle threshold (degrees) - enemies beyond this are considered "opposite" and ignored */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional Targeting", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float OppositeAngleThreshold = 120.0f;

    /** Weight for angle alignment in scoring (0-1). Higher = prefer enemies more aligned with input direction */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional Targeting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DirectionalAngleWeight = 0.7f;

    /** Weight for distance in scoring (0-1). Higher = prefer closer enemies */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional Targeting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DirectionalDistanceWeight = 0.3f;

    // ============================================================================
    // HIT REACTION DEFAULTS
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Reactions")
    float LightAttackStunDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Reactions")
    float HeavyAttackStunDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Reactions")
    float ChargedAttackStunDuration = 1.0f;

    // ============================================================================
    // DAMAGE DEFAULTS
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float LightBaseDamage = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float HeavyBaseDamage = 50.0f;

    // ============================================================================
    // POSTURE DAMAGE DEFAULTS (when blocked)
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture Damage")
    float LightPostureDamage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture Damage")
    float HeavyPostureDamage = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture Damage")
    float ChargedPostureDamage = 40.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture Damage")
    float ParryPostureDamage = 40.0f;
};
