
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CombatTypes.h"
#include "Utilities/MontageUtilityLibrary.h"
#include "AttackData.generated.h"

class UAnimMontage;
class UPairedAnimationData;

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

    /** Base damage dealt on hit (used by BaseCombatCharacter::OnWeaponHit) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float BaseDamage = 25.0f;

    /** Hitstun duration inflicted on hit (used by BaseCombatCharacter::OnWeaponHit) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float HitStunDuration = 0.0f;

    // ============================================================================
    // HITSTOP (Per-Attack Impact Freeze)
    // ============================================================================

    /** Hitstop configuration for this attack (freeze on hit impact) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|Hitstop")
    FHitstopConfig HitstopConfig;

    // ============================================================================
    // AUDIO/VFX (Impact Effects)
    // ============================================================================

    /** Audio configuration for hit impacts (sound, volume, pitch variation) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|Audio")
    FImpactAudioConfig ImpactAudioConfig;

    /** VFX configuration for hit impacts [SCAFFOLD FOR U-16] */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|VFX")
    FImpactVFXConfig ImpactVFXConfig;

    /** DEPRECATED: Posture system removed. Use StaggerPower instead. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deprecated", meta = (DeprecatedProperty, DeprecationMessage = "Use StaggerPower instead"))
    float PostureDamage = 10.0f;

    /**
     * Stagger power (0-1). Controls the chance/strength of contextual stagger.
     * Heavy attacks have higher stagger power. Counter attacks can force stagger.
     * 0 = never staggers, 1 = always staggers on hit
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float StaggerPower = 0.0f;

    /** [NOT YET IMPLEMENTED] Multiplier applied during counter window */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Future|Damage")
    float CounterDamageMultiplier = 1.5f;

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
    // COMBO TRANSITION BLENDING
    // ============================================================================

    /** Blend-out time when transitioning FROM this attack to any combo follow-up (0 = instant) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos|Blending",
        meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.5"))
    float ComboBlendOutTime = 0.1f;

    /** Blend-in time when this attack is the TARGET of a combo transition (0 = instant) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos|Blending",
        meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.5"))
    float ComboBlendInTime = 0.1f;

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

    /** DEPRECATED: Posture system removed. Heavy attacks use StaggerPower instead. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deprecated",
        meta = (DeprecatedProperty, DeprecationMessage = "Posture system removed. Use StaggerPower instead."))
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
    // LEGACY TIMING (Editor Tools Only - DO NOT USE AT RUNTIME)
    // ============================================================================
    // The combat system uses AnimNotify_AttackPhaseTransition events for phase changes.
    // These properties exist ONLY for the editor's "Generate Notifies" tooling.
    // At runtime, timing comes from animation notify events, NOT these values.
    //
    // If you need to configure timing, add AnimNotify_AttackPhaseTransition events
    // to your montage at the appropriate frames.

    /** Editor-only: Used by AttackDataTools to generate notifies (runtime ignores this) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Editor Tools|Timing Generation",
        meta = (DisplayName = "Use Notify Timing (Editor Only)"))
    bool bUseAnimNotifyTiming = true;

    /** Editor-only: Fallback when notifies are missing (runtime ignores this) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Editor Tools|Timing Generation",
        meta = (EditCondition = "bUseAnimNotifyTiming", EditConditionHides, DisplayName = "Fallback Mode"))
    ETimingFallbackMode TimingFallbackMode = ETimingFallbackMode::AutoCalculate;

    /** Editor-only: Manual timing for notify generation (runtime ignores this) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Editor Tools|Timing Generation",
        meta = (EditCondition = "!bUseAnimNotifyTiming", EditConditionHides, DisplayName = "Manual Timing"))
    FAttackPhaseTimingOverride ManualTiming;

    // ============================================================================
    // MOTION WARPING
    // Unified config for both target-based and direction-based warping
    // Used by: CombatComponent::SetupAttackWarp()
    // ============================================================================

    /** Motion warping configuration (handles both target and rotation-only scenarios) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warp")
    FAttackWarpConfig WarpConfig;

    // ============================================================================
    // PAIRED ANIMATIONS (Finishers, Counters)
    // ============================================================================

    /**
     * Paired animation data for this attack's finisher (if any)
     * When this attack triggers a finisher on a vulnerable enemy, this data defines
     * the synced animation sequence between attacker and victim.
     * Null = no finisher version of this attack
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paired Animation")
    TObjectPtr<UPairedAnimationData> FinisherData = nullptr;

    /**
     * Paired animation data for this attack's counter version
     * Used when this attack is performed as a parry counter.
     * Null = use standard attack animation for counters
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paired Animation")
    TObjectPtr<UPairedAnimationData> CounterData = nullptr;

    /**
     * Which hand performs this attack (for procedural IK and animation selection)
     * Used by paired animation system for contact point calculation
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paired Animation|Context")
    FName AttackHand = "RightHand";

    /**
     * Default victim bone for contact (chest, head, etc.)
     * Used for IK adjustment and effect placement during paired animations
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paired Animation|Context")
    FName DefaultContactBone = "spine_03";

    /**
     * Whether this attack can trigger finishers on vulnerable enemies
     * Only checked when enemy is in finisher-eligible state (low health, guard broken, stunned)
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paired Animation|Triggers")
    bool bCanTriggerFinisher = false;

    /**
     * Whether this attack has a counter variant (used after parry)
     * When true and CounterData is set, uses paired counter animation
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paired Animation|Triggers")
    bool bHasCounterVariant = false;

    // ============================================================================
    // CONTEXT & TAGS
    // ============================================================================

    /**
     * Capabilities and properties of this attack (metadata for resolution system)
     * Examples:
     * - Attack.Capability.CanCombo: Can chain into another attack
     * - Attack.Capability.CanDirectional: Can have directional follow-ups
     * - Attack.Capability.Terminal: Ends combo chain (forces reset)
     * - Attack.Capability.CanHold: Supports hold mechanics
     * - Attack.Type.Light / Attack.Type.Heavy: Attack classification
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context|Tags",
        meta = (Categories = "Attack"))
    FGameplayTagContainer AttackTags;

    /**
     * Required context for this attack to be usable (situational attacks)
     * Examples:
     * - Context.ParryCounter: Only available after successful parry
     * - Context.LowHealthFinisher: Only available when enemy health < 20%
     * - Context.DirectionalFollowUp: Only available after hold-and-release
     * Empty = always available (most attacks)
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context|Tags",
        meta = (Categories = "Context"))
    FGameplayTagContainer RequiredContextTags;

    // ============================================================================
    // RUNTIME QUERIES (used by CombatComponent)
    // ============================================================================

    /** Get the time range of the specified montage section (section-relative or absolute) */
    UFUNCTION(BlueprintCallable, Category = "Attack Data")
    void GetSectionTimeRange(float& OutStart, float& OutEnd) const;

    /** Get the length of the target section (or entire montage if no section specified) */
    UFUNCTION(BlueprintPure, Category = "Attack Data")
    float GetSectionLength() const;

    /** Check if montage section has valid AnimNotify_AttackPhaseTransition events */
    UFUNCTION(BlueprintPure, Category = "Attack Data")
    bool HasValidNotifyTimingInSection() const;

    /** Editor-only: Get timing from ManualTiming struct for notify generation tools */
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

    // ============================================================================
    // CONTEXT SYSTEM VALIDATION (Phase 1)
    // ============================================================================

    /**
     * Validate entire attack data asset for context system compliance
     * Checks for cycles, orphaned attacks, invalid directional follow-ups
     * Called automatically by asset validation framework
     */
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;

    /**
     * Detect circular references in combo chains using depth-first search
     * @param Visited - Set of already-visited attacks (prevents infinite loops)
     * @param Errors - Accumulated error messages
     * @return True if cycle detected
     */
    bool DetectCycles(TSet<const UAttackData*>& Visited, TArray<FText>& Errors) const;

    /**
     * Validate directional follow-up configuration
     * Ensures DirectionalFollowUps are marked with proper tags
     * @param Errors - Accumulated error messages
     * @return True if validation passed
     */
    bool ValidateDirectionalFollowUps(TArray<FText>& Errors) const;

    /**
     * Validate Terminal tag consistency
     * Terminal attacks must NOT have NextComboAttack/HeavyComboAttack/DirectionalFollowUps
     * @param Errors - Accumulated error messages
     * @return True if validation passed
     */
    bool ValidateTerminalTag(TArray<FText>& Errors) const;
#endif
};