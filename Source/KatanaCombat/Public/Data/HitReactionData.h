// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CombatTypes.h"
#include "HitReactionData.generated.h"

class UAnimMontage;

/**
 * Defines a single hit reaction's properties and behavior
 * Similar to AttackData - supports section selection, context tags, and paired reactions
 *
 * Usage:
 * 1. Create HitReactionData asset in Content Browser
 * 2. Assign montage and configure section (optional)
 * 3. Set reaction parameters (stun, knockback, i-frames)
 * 4. Reference from HitReactionSettings
 *
 * Section Selection:
 * - Multiple reactions can share one montage using different sections
 * - MontageSection = NAME_None uses entire montage
 * - bUseSectionOnly = true stops at section end
 *
 * I-Frames:
 * - When bHasIFrames = true, character ignores hits during IFrameStart to IFrameEnd
 * - Times are relative to montage/section start
 */
UCLASS(BlueprintType)
class KATANACOMBAT_API UHitReactionData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UHitReactionData();

    // ========================================================================
    // ANIMATION
    // ========================================================================

    /** Animation montage for this reaction */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    TObjectPtr<UAnimMontage> ReactionMontage = nullptr;

    /** Which section of the montage to use (NAME_None = use entire montage) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Section")
    FName MontageSection = NAME_None;

    /** If true, only this section plays. If false, montage continues after section */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Section")
    bool bUseSectionOnly = true;

    /** If true, automatically jump to the section start when playing */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Section")
    bool bJumpToSectionStart = true;

    /** Montage play rate (1.0 = normal speed) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation",
        meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float PlayRate = 1.0f;

    // ========================================================================
    // REACTION TYPE
    // ========================================================================

    /** Classification of this reaction */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
    EHitReactionType ReactionType = EHitReactionType::Light;

    /** Direction this reaction is designed for (None = omnidirectional) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
    EAttackDirection Direction = EAttackDirection::None;

    // ========================================================================
    // TIMING & INTERRUPTS
    // ========================================================================

    /** Duration of hitstun (character cannot act) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float StunDuration = 0.3f;

    /** If true, cannot be interrupted by further attacks (i-frames) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
    bool bHasIFrames = false;

    /** I-frame start time (relative to montage/section start) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing",
        meta = (EditCondition = "bHasIFrames", EditConditionHides, ClampMin = "0.0"))
    float IFrameStart = 0.0f;

    /** I-frame end time (relative to montage/section start) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing",
        meta = (EditCondition = "bHasIFrames", EditConditionHides, ClampMin = "0.0"))
    float IFrameEnd = 0.5f;

    // ========================================================================
    // PHYSICS
    // ========================================================================

    /** Knockback force applied to victim */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics",
        meta = (ClampMin = "0.0"))
    float KnockbackForce = 200.0f;

    /** Vertical launch force (0 = no launch) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics",
        meta = (ClampMin = "0.0"))
    float LaunchForce = 0.0f;

    // ========================================================================
    // PAIRED REACTIONS (Counter/Finisher) - Extension Point
    // Built now, wired when AttackData extended with counter/finisher names
    // ========================================================================

    /** Is this a paired reaction that syncs with attacker? */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paired Reaction")
    bool bIsPairedReaction = false;

    /** Type of paired reaction */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paired Reaction",
        meta = (EditCondition = "bIsPairedReaction", EditConditionHides))
    EPairedReactionType PairedType = EPairedReactionType::None;

    /** Name identifier for pairing (matches attacker's finisher/counter name) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paired Reaction",
        meta = (EditCondition = "bIsPairedReaction", EditConditionHides))
    FName PairedReactionName = NAME_None;

    /** Sync point for paired animations (attacker and victim sync at this time) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paired Reaction",
        meta = (EditCondition = "bIsPairedReaction", EditConditionHides, ClampMin = "0.0"))
    float SyncPointTime = 0.0f;

    // ========================================================================
    // CONTEXT TAGS - Extension Point
    // Built now for future context-based reaction selection
    // ========================================================================

    /**
     * Properties of this reaction (for selection/filtering)
     * Examples:
     * - Reaction.Intensity.Light / Reaction.Intensity.Heavy
     * - Reaction.Type.Stagger / Reaction.Type.Knockdown
     * - Reaction.Paired.Counter / Reaction.Paired.Finisher
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context|Tags",
        meta = (Categories = "Reaction"))
    FGameplayTagContainer ReactionTags;

    /**
     * Required context for this reaction to be selected
     * Examples:
     * - Context.Counter: Only after successful counter
     * - Context.GuardBroken: Only when posture depleted
     * - Context.LowHealth: Only when victim health < 20%
     * Empty = default reaction (always selectable)
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context|Tags",
        meta = (Categories = "Context"))
    FGameplayTagContainer RequiredContextTags;

    // ========================================================================
    // RUNTIME QUERIES
    // ========================================================================

    /** Get the time range of the specified montage section */
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction Data")
    void GetSectionTimeRange(float& OutStart, float& OutEnd) const;

    /** Get the length of the target section (or entire montage if no section) */
    UFUNCTION(BlueprintPure, Category = "Hit Reaction Data")
    float GetSectionLength() const;

    /** Check if currently in i-frame window at given time */
    UFUNCTION(BlueprintPure, Category = "Hit Reaction Data")
    bool IsInIFrameWindow(float CurrentTime) const;

#if WITH_EDITOR
    /** Validate montage section exists */
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
