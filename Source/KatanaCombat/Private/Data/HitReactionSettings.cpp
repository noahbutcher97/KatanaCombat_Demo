// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/HitReactionSettings.h"
#include "Data/HitReactionData.h"
#include "Data/AttackData.h"

DEFINE_LOG_CATEGORY_STATIC(LogHitReactionSettings, Log, All);

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

UHitReactionData* UHitReactionSettings::GetSpecialReaction(ESpecialReactionType SpecialType) const
{
    switch (SpecialType)
    {
        case ESpecialReactionType::GuardBroken:
            return GuardBrokenReaction;

        case ESpecialReactionType::Knockdown:
            return KnockdownReaction;

        case ESpecialReactionType::Launch:
            return LaunchReaction;

        case ESpecialReactionType::Death:
            return DeathReaction;

        default:
            return nullptr;
    }
}

UHitReactionData* UHitReactionSettings::GetPairedReaction(
    EPairedReactionType PairedType,
    FName ReactionName) const
{
    if (ReactionName == NAME_None)
    {
        return nullptr;
    }

    switch (PairedType)
    {
        case EPairedReactionType::Counter:
        {
            const TObjectPtr<UHitReactionData>* Found = CounterReactions.Find(ReactionName);
            return Found ? Found->Get() : nullptr;
        }

        case EPairedReactionType::Finisher:
        {
            const TObjectPtr<UHitReactionData>* Found = FinisherVictimReactions.Find(ReactionName);
            return Found ? Found->Get() : nullptr;
        }

        case EPairedReactionType::Parry:
        case EPairedReactionType::Throw:
            // Extension point: Add maps for these when needed
            UE_LOG(LogHitReactionSettings, Warning,
                TEXT("GetPairedReaction: PairedType %d not yet implemented"),
                static_cast<int32>(PairedType));
            return nullptr;

        default:
            return nullptr;
    }
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
