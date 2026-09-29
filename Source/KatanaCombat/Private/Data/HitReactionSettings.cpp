// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/HitReactionSettings.h"
#include "Data/AttackData.h"

UHitReactionSettings::UHitReactionSettings()
{
    // Default values are set via inline initialization in header
}

// ============================================================================
// SELECTION API
// ============================================================================

const FHitReactionEntry* UHitReactionSettings::GetDirectionalReaction(
    EHitIntensity Intensity,
    EAttackDirection Direction) const
{
    // Find the reaction set for this intensity
    const FDirectionalReactionSet* ReactionSet = DirectionalReactions.Find(Intensity);
    if (!ReactionSet)
    {
        return nullptr;
    }

    // Get reaction entry for direction
    const FHitReactionEntry* Entry = ReactionSet->GetReaction(Direction);

    // Only return if it has a valid montage configured
    if (Entry && Entry->IsValid())
    {
        return Entry;
    }

    return nullptr;
}

const FHitReactionEntry* UHitReactionSettings::GetDeathReaction(EAttackDirection Direction) const
{
    // Try exact direction first
    if (const FHitReactionEntry* Entry = DeathReactions.Find(Direction))
    {
        if (Entry->IsValid())
        {
            return Entry;
        }
    }

    // Fall back to Forward if different direction requested
    if (Direction != EAttackDirection::Forward)
    {
        if (const FHitReactionEntry* Entry = DeathReactions.Find(EAttackDirection::Forward))
        {
            if (Entry->IsValid())
            {
                return Entry;
            }
        }
    }

    // No death reactions configured
    return nullptr;
}

EHitIntensity UHitReactionSettings::GetIntensityFromAttack(
    EAttackType AttackType,
    float Damage,
    float CurrentHealth) const
{
    // Heavy/Special attacks always trigger heavy reaction
    if (AttackType == EAttackType::Heavy || AttackType == EAttackType::Special)
    {
        return EHitIntensity::Heavy;
    }

    // Light attacks: check damage percentage threshold
    if (CurrentHealth > 0.0f && HeavyDamageHealthPercent < 1.0f)
    {
        const float DamagePercent = Damage / CurrentHealth;
        if (DamagePercent >= HeavyDamageHealthPercent)
        {
            return EHitIntensity::Heavy;
        }
    }

    return EHitIntensity::Light;
}
