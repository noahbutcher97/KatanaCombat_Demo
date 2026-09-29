// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatTypes.h"
#include "HitReactionSettings.generated.h"

/**
 * Container for hit reaction configuration
 *
 * Architecture:
 * - Directional reactions: inline FHitReactionEntry structs (Intensity x Direction)
 * - Death reactions: inline FHitReactionEntry structs keyed by direction
 * - Paired victim animation (counters, finishers) is owned by UPairedAnimationData
 *
 * Swapping reaction sets per character is done at the asset level: assign a different
 * settings asset through UHitReactionComponent::HitReactionSettingsOverride or
 * CombatSettings->HitReactionSettings.
 *
 * Usage:
 * 1. Create HitReactionSettings asset
 * 2. Configure directional and death reactions inline
 * 3. Reference from CombatSettings or HitReactionComponent override
 */
UCLASS(BlueprintType)
class KATANACOMBAT_API UHitReactionSettings : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UHitReactionSettings();

    // ========================================================================
    // DIRECTIONAL REACTIONS (intensity × direction lookup)
    // ========================================================================

    /**
     * Directional reactions organized by intensity then direction
     * Modular: Adding new intensity/direction doesn't require enum explosion
     *
     * Example setup:
     *   Light → {Forward: LightFront, Back: LightBack, Left: LightLeft, Right: LightRight}
     *   Heavy → {Forward: HeavyFront, Back: HeavyBack, Left: HeavyLeft, Right: HeavyRight}
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions|Directional")
    TMap<EHitIntensity, FDirectionalReactionSet> DirectionalReactions;

    // ========================================================================
    // DEATH REACTIONS (directional)
    // ========================================================================

    /**
     * Directional death reactions
     * Key: Direction the killing blow came from (relative to victim)
     * Value: Death animation config (should have Outcome = Death or Ragdoll)
     * Falls back to Forward direction if specific direction not configured
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions|Death")
    TMap<EAttackDirection, FHitReactionEntry> DeathReactions;

    /**
     * Get death reaction for a direction
     * Falls back to Forward if specific direction not configured
     * @param Direction - Direction of killing blow (relative to victim)
     * @return Death reaction entry, or nullptr if no deaths configured
     */
    const FHitReactionEntry* GetDeathReaction(EAttackDirection Direction) const;

    // ========================================================================
    // SELECTION PARAMETERS
    // ========================================================================

    /**
     * Damage percentage threshold for forced heavy reaction
     * If (damage / currentHealth) >= this value, play heavy reaction regardless of attack type
     * Example: 0.25 = if hit does 25% or more of current health → heavy reaction
     * Set to 1.0+ to disable (only attack type determines intensity)
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HeavyDamageHealthPercent = 0.25f;

    /** [NOT WIRED] No knockback physics is applied; this multiplier is never read (pending wire-or-delete). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float GlobalKnockbackMultiplier = 1.0f;

    // ========================================================================
    // SELECTION API
    // ========================================================================

    /**
     * Get directional reaction entry by intensity and direction
     * @param Intensity - Light or Heavy
     * @param Direction - Front, Back, Left, or Right
     * @return Pointer to reaction entry, or nullptr if intensity not configured
     */
    const FHitReactionEntry* GetDirectionalReaction(EHitIntensity Intensity, EAttackDirection Direction) const;

    /**
     * Determine intensity from attack type and damage percentage
     * Heavy/Special attack → Heavy intensity
     * Light attack with (damage/currentHealth) >= HeavyDamageHealthPercent → Heavy intensity
     * Otherwise → Light intensity
     * @param AttackType - Attack type from AttackData
     * @param Damage - Damage amount dealt
     * @param CurrentHealth - Victim's current health (for percentage calc)
     * @return Calculated intensity
     */
    UFUNCTION(BlueprintPure, Category = "Selection")
    EHitIntensity GetIntensityFromAttack(EAttackType AttackType, float Damage, float CurrentHealth) const;
};
