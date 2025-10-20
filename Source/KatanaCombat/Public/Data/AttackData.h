
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatTypes.h"
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
    // HEAVY ATTACK CHARGING
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy Attack", 
        meta = (EditCondition = "AttackType == EAttackType::Heavy", EditConditionHides))
    float MaxChargeTime = 2.0f;

    /** Animation playback speed during charge windup (< 1.0 = slower) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy Attack", 
        meta = (EditCondition = "AttackType == EAttackType::Heavy", EditConditionHides))
    float ChargeTimeScale = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy Attack", 
        meta = (EditCondition = "AttackType == EAttackType::Heavy", EditConditionHides))
    float MaxChargeDamageMultiplier = 2.5f;

    /** Posture damage at full charge */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy Attack", 
        meta = (EditCondition = "AttackType == EAttackType::Heavy", EditConditionHides))
    float ChargedPostureDamage = 40.0f;

    // ============================================================================
    // LIGHT ATTACK HOLD & FOLLOW-UP
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Attack",
        meta = (EditCondition = "AttackType == EAttackType::Light", EditConditionHides))
    bool bCanHold = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Attack",
        meta = (EditCondition = "AttackType == EAttackType::Light && bCanHold", EditConditionHides))
    bool bEnforceMaxHoldTime = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Attack",
        meta = (EditCondition = "AttackType == EAttackType::Light && bCanHold && bEnforceMaxHoldTime", EditConditionHides))
    float MaxHoldTime = 1.5f;

    // ============================================================================
    // TIMING SYSTEM (AnimNotify-Driven with Fallbacks)
    // ============================================================================

    /** Primary: Use timing from AnimNotifyState_AttackPhase in montage */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
    bool bUseAnimNotifyTiming = true;

    /** What to do if AnimNotifyStates are missing from montage/section */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", 
        meta = (EditCondition = "bUseAnimNotifyTiming", EditConditionHides))
    ETimingFallbackMode TimingFallbackMode = ETimingFallbackMode::AutoCalculate;

    /** Manual timing values (used when bUseAnimNotifyTiming = false) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", 
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

    /** Check if this attack has valid AnimNotifyStates in its target section/montage */
    UFUNCTION(BlueprintPure, Category = "Attack Data")
    bool HasValidNotifyTimingInSection() const;

    /** Get calculated timing based on current settings (notifies or fallback) */
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