// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CombatTypes.h"
#include "Data/PairedAnimationTypes.h"
#include "AnimNotifyState_PairedAnimationSync.generated.h"

/**
 * Animation notify state for paired animation synchronization points
 *
 * Add to ATTACKER's montage at the key moment when:
 * - Damage should be applied
 * - Effects should trigger (camera shake, slow-mo, etc.)
 * - Victim animation should reach its corresponding sync point
 *
 * Timeline Example (Finisher):
 * [──Wind-up──][─Swing─][─SYNC─][──Follow-through──]
 *                        ▲ Apply damage here
 *                        ▲ Trigger camera shake
 *                        ▲ End slow-mo if applicable
 *
 * Paired Animation Context:
 * This notify broadcasts FOnPairedAnimationSyncPoint which can be used by:
 * - Damage system (apply finisher damage)
 * - Effects system (camera shake, hit pause, blood VFX)
 * - Audio system (impact sounds)
 * - Time dilation (restore normal speed after slow-mo)
 *
 * Gameplay Context Support:
 * - AttackHand: Which hand is performing the attack (for IK/animation selection)
 * - BlockHand: Which hand should block (for counter animations)
 * - WeaponType: Type of weapon being used
 * - ContactBoneName: Target bone for impact effects
 *
 * Usage:
 * 1. Add to attacker's montage at impact frame
 * 2. Set SyncPointName for event identification
 * 3. Configure gameplay context fields for procedural adjustments
 * 4. Listen to OnPairedAnimationSyncPoint delegate
 */
UCLASS(meta = (DisplayName = "Paired Animation Sync Point"))
class KATANACOMBAT_API UAnimNotifyState_PairedAnimationSync : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    UAnimNotifyState_PairedAnimationSync();

    // ========================================================================
    // SYNC POINT IDENTIFICATION
    // ========================================================================

    /** Unique name for this sync point (e.g., "Impact", "FinisherHit", "CounterStrike") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sync Point")
    FName SyncPointName = "Impact";

    /** Type of paired animation this sync belongs to */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sync Point")
    EPairedReactionType ReactionType = EPairedReactionType::Finisher;

    /** Is this the primary sync point where damage/effects trigger? */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sync Point")
    bool bIsPrimarySyncPoint = true;

    // ========================================================================
    // GAMEPLAY CONTEXT (Procedural Selection)
    // ========================================================================

    /** Which hand is attacking (for IK/animation selection) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay Context")
    FName AttackHand = "RightHand";

    /** Which hand should block/parry (for counter animations) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay Context")
    FName BlockHand = "LeftHand";

    /** Target bone on victim for contact/impact (for IK adjustment) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay Context")
    FName VictimContactBone = "spine_03";

    /** Attack direction for directional selection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay Context")
    EAttackDirection AttackDirection = EAttackDirection::Forward;

    // ========================================================================
    // EFFECTS
    // ========================================================================

    /** Apply damage at this sync point */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    bool bApplyDamage = true;

    /** Trigger camera shake at this sync point */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    bool bTriggerCameraShake = true;

    /** Apply hit pause/stop at this sync point */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    bool bApplyHitPause = false;

    /** Hit pause duration in seconds (actual time, not game time) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects",
        meta = (EditCondition = "bApplyHitPause", ClampMin = "0.0", ClampMax = "0.5"))
    float HitPauseDuration = 0.05f;

    /** End slow motion at this sync point (if slow-mo was active) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    bool bEndSlowMotion = false;

    // ========================================================================
    // ALIGNMENT VALIDATION
    // ========================================================================

    /** Enable alignment validation at sync point (checks distance between attacker and victim) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
    bool bValidateAlignment = true;

    /** Maximum acceptable distance between attacker and victim at sync point (units) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation",
        meta = (EditCondition = "bValidateAlignment", ClampMin = "50.0", ClampMax = "500.0"))
    float MaxContactDistance = 150.0f;

    /** Log warning when misalignment detected (for debugging/tuning animations) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation",
        meta = (EditCondition = "bValidateAlignment"))
    bool bLogMisalignment = true;

    /** Auto-correct minor misalignment by nudging victim position */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation",
        meta = (EditCondition = "bValidateAlignment"))
    bool bNudgeOnMinorMisalignment = false;

    /** Distance threshold for nudge correction (must be < MaxContactDistance) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation",
        meta = (EditCondition = "bValidateAlignment && bNudgeOnMinorMisalignment", ClampMin = "10.0", ClampMax = "100.0"))
    float NudgeThreshold = 50.0f;

    // ========================================================================
    // ANIMNOTIFYSTATE INTERFACE
    // ========================================================================

    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

    virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
    virtual FLinearColor GetEditorColor() override { return FLinearColor(1.0f, 0.2f, 0.2f); }  // Red for impact
    virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override { return true; }
#endif
};
