
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatTypes.h"
#include "Utilities/MontageUtilityLibrary.h"
#include "AttackData.generated.h"

class UAnimMontage;

/**
 * Defines a single attack's properties and behavior
 * Extended with combo chains, posture damage, and montage section support
 */
UCLASS(BlueprintType)
class KATANACOMBAT_API UAttackData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UAttackData();

    // ============================================================================
    // BASIC ATTACK PROPERTIES
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Basic")
    EAttackType AttackType = EAttackType::Light;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Basic")
    EAttackDirection Direction = EAttackDirection::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Basic")
    TObjectPtr<UAnimMontage> AttackMontage = nullptr;

    // ============================================================================
    // MONTAGE SECTION SUPPORT
    // ============================================================================

    /** Which section of the montage to use for this attack (NAME_None = use entire montage) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Montage Section")
    FName MontageSection = NAME_None;

    /** If true, only this section plays. If false, montage continues normally after section */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Montage Section")
    bool bUseSectionOnly = false;

    /** If true, automatically jump to the section start when playing this attack */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Montage Section")
    bool bJumpToSectionStart = true;

    // ============================================================================
    // DAMAGE & POSTURE
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float BaseDamage = 25.0f;

    /** Posture damage dealt when this attack is blocked (not parried) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float PostureDamage = 10.0f;

    /** Multiplier applied during counter window (after successful parry/evade) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float CounterDamageMultiplier = 1.5f;

    /** Hitstun duration inflicted on hit (0 = no stun, can be countered immediately) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float HitStunDuration = 0.0f;

    // ============================================================================
    // COMBO SYSTEM
    // ============================================================================

    /** Next attack in light combo chain (tap light during recovery) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos")
    TObjectPtr<UAttackData> NextComboAttack = nullptr;

    /** Heavy attack branch from this attack (tap heavy during recovery) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos")
    TObjectPtr<UAttackData> HeavyComboAttack = nullptr;

    /** Directional follow-ups after hold-and-release */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos")
    TMap<EAttackDirection, TObjectPtr<UAttackData>> DirectionalFollowUps;

    /** Heavy attacks available after directional follow-ups */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos")
    TMap<EAttackDirection, TObjectPtr<UAttackData>> HeavyDirectionalFollowUps;

    /** Time window for combo input (after this attack starts recovery) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos")
    float ComboInputWindow = 0.6f;

    // ============================================================================
    // HEAVY ATTACK CHARGING (Only visible when AttackType == Heavy)
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Heavy Attack",
        meta = (EditCondition = "AttackType == EAttackType::Heavy", EditConditionHides))
    float MaxChargeTime = 2.0f;

    /** Animation playback speed during charge windup (< 1.0 = slower) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Heavy Attack",
        meta = (EditCondition = "AttackType == EAttackType::Heavy", EditConditionHides))
    float ChargeTimeScale = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Heavy Attack",
        meta = (EditCondition = "AttackType == EAttackType::Heavy", EditConditionHides))
    float MaxChargeDamageMultiplier = 2.5f;

    /** Posture damage at full charge */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Heavy Attack",
        meta = (EditCondition = "AttackType == EAttackType::Heavy", EditConditionHides))
    float ChargedPostureDamage = 40.0f;

    /** Montage section that loops during charge (NAME_None = use default animation) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Heavy Attack",
        meta = (EditCondition = "AttackType == EAttackType::Heavy", EditConditionHides))
    FName ChargeLoopSection = NAME_None;

    /** Montage section to play on release (the actual attack after charging). If NAME_None, blends to idle instead. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Heavy Attack",
        meta = (EditCondition = "AttackType == EAttackType::Heavy", EditConditionHides))
    FName ChargeReleaseSection = NAME_None;

    /** Blend time when transitioning from initial attack animation INTO charge loop section (0 = instant jump) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Heavy Attack",
        meta = (EditCondition = "AttackType == EAttackType::Heavy", EditConditionHides))
    float ChargeLoopBlendTime = 0.3f;

    /** Blend time when transitioning OUT of charge loop to release section (0 = instant jump) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Heavy Attack",
        meta = (EditCondition = "AttackType == EAttackType::Heavy", EditConditionHides))
    float ChargeReleaseBlendTime = 0.2f;

    // ============================================================================
    // LIGHT ATTACK HOLD & FOLLOW-UP (Only visible when AttackType == Light)
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Light Attack",
        meta = (EditCondition = "AttackType == EAttackType::Light", EditConditionHides))
    bool bCanHold = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Light Attack",
        meta = (EditCondition = "AttackType == EAttackType::Light && bCanHold", EditConditionHides))
    bool bEnforceMaxHoldTime = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Light Attack",
        meta = (EditCondition = "AttackType == EAttackType::Light && bCanHold && bEnforceMaxHoldTime", EditConditionHides))
    float MaxHoldTime = 1.5f;

    /** Duration to ease animation to stop when hold activates (0 = instant stop) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Light Attack",
        meta = (EditCondition = "AttackType == EAttackType::Light && bCanHold", EditConditionHides))
    float HoldEaseInDuration = 0.5f;

    /** Easing curve for hold slow-down transition (EaseOutQuad recommended) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Light Attack",
        meta = (EditCondition = "AttackType == EAttackType::Light && bCanHold", EditConditionHides))
    EEasingType HoldEaseInType = EEasingType::EaseOutQuad;

    /** Target playrate when hold is fully activated (0.0 = freeze, 0.2 = very slow) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Light Attack",
        meta = (EditCondition = "AttackType == EAttackType::Light && bCanHold", EditConditionHides))
    float HoldTargetPlayRate = 0.0f;

    /** Duration to ease animation back to normal speed on release (0 = instant restore) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Light Attack",
        meta = (EditCondition = "AttackType == EAttackType::Light && bCanHold", EditConditionHides))
    float HoldEaseOutDuration = 0.3f;

    /** Easing curve for hold speed-up transition on release (EaseInQuad recommended) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Type|Light Attack",
        meta = (EditCondition = "AttackType == EAttackType::Light && bCanHold", EditConditionHides))
    EEasingType HoldEaseOutType = EEasingType::EaseInQuad;

    // ============================================================================
    // TIMING SYSTEM (Event-Based Phase Transitions)
    // ============================================================================
    // NEW SYSTEM: Phases use AnimNotify_AttackPhaseTransition events (2 per attack)
    // - Windup → Active transition (at end of windup)
    // - Active → Recovery transition (at end of active/hit detection)
    // - Windup start is implicit (montage start)
    // - Recovery end is implicit (montage end)
    //
    // DEPRECATED: Old AnimNotifyState_AttackPhase system (6 events per attack)
    // These properties are kept for backward compatibility but are no longer used

    /** DEPRECATED: Timing is now always event-driven (AnimNotify_AttackPhaseTransition) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing (Deprecated)")
    bool bUseAnimNotifyTiming = true;

    /** DEPRECATED: No longer used with event-based phase system */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing (Deprecated)",
        meta = (EditCondition = "bUseAnimNotifyTiming", EditConditionHides))
    ETimingFallbackMode TimingFallbackMode = ETimingFallbackMode::AutoCalculate;

    /** DEPRECATED: Manual timing not supported with event-based system */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing (Deprecated)",
        meta = (EditCondition = "!bUseAnimNotifyTiming", EditConditionHides))
    FAttackPhaseTimingOverride ManualTiming;

    // ============================================================================
    // MOTION WARPING
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
    FMotionWarpingConfig MotionWarpingConfig;

    // ============================================================================
    // RUNTIME QUERIES (used by CombatComponent)
    // ============================================================================

    /** Get the time range of the specified montage section (section-relative or absolute) */
    UFUNCTION(BlueprintCallable, Category = "Attack Data")
    void GetSectionTimeRange(float& OutStart, float& OutEnd) const;

    /** Get the length of the target section (or entire montage if no section specified) */
    UFUNCTION(BlueprintPure, Category = "Attack Data")
    float GetSectionLength() const;

    /** DEPRECATED: Check if attack has valid phase transitions (always returns true for new system) */
    UFUNCTION(BlueprintPure, Category = "Attack Data")
    bool HasValidNotifyTimingInSection() const;

    /** DEPRECATED: Get calculated timing (not used with event-based system) */
    UFUNCTION(BlueprintCallable, Category = "Attack Data")
    void GetEffectiveTiming(float& OutWindup, float& OutActive, float& OutRecovery) const;

#if WITH_EDITOR
    // ============================================================================
    // EDITOR-ONLY FUNCTIONALITY
    // ============================================================================

    /** Auto-calculate reasonable timing from montage/section length and attack type */
    void AutoCalculateTimingFromSection();

    /** Generate AnimNotifyState_AttackPhase notifies in the target section */
    bool GenerateNotifiesInSection();

    /** Find other AttackData assets using the same montage section (conflict detection) */
    TArray<UAttackData*> FindOtherUsersOfSection() const;

    /** Validate montage section exists and is properly configured */
    bool ValidateMontageSection(FText& OutErrorMessage) const;

    /** Get a preview string showing the timing layout */
    FString GetTimingPreviewString() const;

    // Post-edit hooks for editor validation
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};