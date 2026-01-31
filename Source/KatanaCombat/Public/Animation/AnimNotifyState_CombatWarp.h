// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNotifyState_MotionWarping.h"
#include "AnimNotifyState_CombatWarp.generated.h"

class UMotionWarpingComponent;
class URootMotionModifier;
class UAnimSequenceBase;

/**
 * Combat-aware motion warp notify state
 *
 * Extends AnimNotifyState_MotionWarping to automatically choose between:
 * - TARGET mode (translation+rotation) when enemy target exists
 * - ROTATION mode (rotation-only) when no target, just input direction
 *
 * This eliminates the need for two separate notifies per attack montage.
 *
 * Usage:
 * 1. Add this notify to your attack montage (replaces two MotionWarping notifies)
 * 2. Configure TargetWarpName and RotationWarpName (defaults usually work)
 * 3. At runtime, CombatComponent::SetupAttackWarp() sets up ONE of the targets
 * 4. This notify detects which exists and configures itself appropriately
 *
 * How it works:
 * - When AddRootMotionModifier is called, we check which target exists
 * - If TargetWarpName exists: Enable translation+rotation toward enemy
 * - If RotationWarpName exists: Disable translation, only rotate toward direction
 * - If neither exists: Skip warping entirely (return nullptr)
 */
UCLASS(DisplayName = "Combat Warp", meta = (DisplayName = "Combat Warp"))
class KATANACOMBAT_API UAnimNotifyState_CombatWarp : public UAnimNotifyState_MotionWarping
{
    GENERATED_BODY()

public:
    UAnimNotifyState_CombatWarp(const FObjectInitializer& ObjectInitializer);

    // ========================================================================
    // COMBAT WARP SETTINGS
    // ========================================================================

    /**
     * Warp target name for translation+rotation (toward enemy)
     * Used when SetupAttackWarp() found a valid target via soft aim assist
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Warp")
    FName TargetWarpName = "AttackTarget";

    /**
     * Warp target name for rotation-only (face direction)
     * Used when SetupAttackWarp() found no target, only input direction
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Warp")
    FName RotationWarpName = "RotationTarget";

    /** Enable translation warp when using TargetWarpName (toward enemy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Warp")
    bool bEnableTranslationForTarget = true;

    // ========================================================================
    // MOTION WARPING INTERFACE OVERRIDES
    // ========================================================================

    /**
     * Override to dynamically configure warp settings based on which target exists.
     * Called when this notify becomes relevant during animation playback.
     */
    virtual URootMotionModifier* AddRootMotionModifier_Implementation(
        UMotionWarpingComponent* MotionWarpingComp,
        const UAnimSequenceBase* Animation,
        float StartTime,
        float EndTime) const override;

    virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
    virtual FLinearColor GetEditorColor() override { return FLinearColor(0.9f, 0.5f, 0.1f, 1.0f); } // Orange
    
    /**
     * Override editor validation to prevent false warnings about WarpTargetName
     * The parent class validates WarpTargetName at edit time, but we set it dynamically at runtime
     * We validate that at least ONE of our target names is set instead
     */
    virtual void ValidateAssociatedAssets() override;
#endif

private:
    /** Check if a warp target exists in the component */
    bool HasWarpTarget(UMotionWarpingComponent* MotionWarping, FName InTargetName) const;
};
