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

    /** Enable V2 combat system (timer-based action queue) instead of V1 (immediate execution) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
    bool bUseV2System = false;

    /** Enable debug visualization for combat system state */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "System")
    bool bDebugDraw = false;

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
