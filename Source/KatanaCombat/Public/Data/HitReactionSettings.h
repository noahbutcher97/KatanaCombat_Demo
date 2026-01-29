// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatTypes.h"
#include "HitReactionSettings.generated.h"

class UHitReactionData;

/**
 * Container for hit reaction configuration
 *
 * Architecture:
 * - Directional reactions: Inline FHitReactionEntry structs (no separate assets needed)
 * - Special reactions: UHitReactionData assets (for complex config, reusability)
 * - Paired reactions: UHitReactionData assets (named lookup for counter/finisher)
 *
 * Selection Priority:
 * 1. Special reactions (GuardBroken, Knockdown, Launch, Death) - checked first
 * 2. Paired reactions (Counter, Finisher) - when triggered explicitly
 * 3. Directional reactions (Intensity × Direction) - default fallback
 *
 * Usage:
 * 1. Create HitReactionSettings asset
 * 2. Configure directional reactions inline (Light/Heavy × Front/Back/Left/Right)
 * 3. Add special reaction assets as needed
 * 4. Reference from CombatSettings or HitReactionComponent override
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
    // SPECIAL REACTIONS (non-directional)
    // ========================================================================

    /** Guard broken reaction (posture depleted) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions|Special")
    TObjectPtr<UHitReactionData> GuardBrokenReaction;

    /** Knockdown reaction */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions|Special")
    TObjectPtr<UHitReactionData> KnockdownReaction;

    /** Launch reaction (for launchers) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions|Special")
    TObjectPtr<UHitReactionData> LaunchReaction;

    /** Death reaction */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions|Special")
    TObjectPtr<UHitReactionData> DeathReaction;

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
    // PAIRED REACTIONS (keyed by name) - Extension Point
    // Built now, wired when AttackData extended with counter/finisher names
    // ========================================================================

    /** Counter reactions (keyed by counter attack name) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions|Paired")
    TMap<FName, TObjectPtr<UHitReactionData>> CounterReactions;

    /** Finisher victim reactions (keyed by finisher name) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions|Paired")
    TMap<FName, TObjectPtr<UHitReactionData>> FinisherVictimReactions;

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

    /** Global knockback multiplier applied to all reactions */
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
     * Get special reaction by type (returns asset reference)
     * @param SpecialType - GuardBroken, Knockdown, Launch, or Death
     * @return HitReactionData or nullptr if not configured
     */
    UFUNCTION(BlueprintPure, Category = "Selection")
    UHitReactionData* GetSpecialReaction(ESpecialReactionType SpecialType) const;

    /**
     * Get paired reaction by type and name (returns asset reference)
     * Extension point: Called when AttackData has counter/finisher name
     * @param PairedType - Counter or Finisher
     * @param ReactionName - Name identifier matching attacker's attack
     * @return HitReactionData or nullptr if not found
     */
    UFUNCTION(BlueprintPure, Category = "Selection")
    UHitReactionData* GetPairedReaction(EPairedReactionType PairedType, FName ReactionName) const;

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
