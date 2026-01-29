// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MotionWarpingSettings.generated.h"

/**
 * Configuration for motion warping behavior (character-level defaults)
 *
 * Usage:
 * - Referenced by CombatSettings as class-level default
 * - Per-attack overrides available via AttackData::WarpConfig (FAttackWarpConfig)
 * - Create different assets for different character types (player with more generous warping vs AI)
 */
UCLASS(BlueprintType)
class KATANACOMBAT_API UMotionWarpingSettings : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UMotionWarpingSettings();

    // ============================================================================
    // TARGET-BASED WARPING
    // Used when warping toward an enemy target
    // ============================================================================

    /** Maximum distance to warp toward a target */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target Warping")
    float MaxWarpDistance = 400.0f;

    /** Minimum distance before warping kicks in (prevents micro-warps) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target Warping")
    float MinWarpDistance = 50.0f;

    /** Require line of sight for target warping */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target Warping")
    bool bRequireLineOfSight = true;

    // ============================================================================
    // DIRECTIONAL WARPING
    // Used when warping based on input direction (no target)
    // ============================================================================

    /** Default rotation warp speed (degrees per second) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional Warping",
        meta = (ClampMin = "90.0", ClampMax = "1800.0"))
    float DirectionalWarpRotationSpeed = 720.0f;

    /** Default angle threshold to skip warp if already facing direction (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional Warping",
        meta = (ClampMin = "0.0", ClampMax = "45.0"))
    float AlreadyFacingThreshold = 15.0f;

    /** Default facing cone for auto-targeting when no input direction (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional Warping",
        meta = (ClampMin = "30.0", ClampMax = "180.0"))
    float NoInputFacingCone = 90.0f;
};
