// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TargetingSettings.generated.h"

/**
 * Configuration for targeting system behavior
 *
 * Usage:
 * - Referenced by CombatSettings as class-level default
 * - Can be overridden per-instance on TargetingComponent
 * - Create different assets for different character types (player vs AI, melee vs ranged)
 */
UCLASS(BlueprintType)
class KATANACOMBAT_API UTargetingSettings : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UTargetingSettings();

    // ============================================================================
    // BASIC TARGETING
    // ============================================================================

    /** Maximum distance to search for targets */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Basic Targeting")
    float MaxTargetDistance = 1000.0f;

    /** Angle of directional cone for basic targeting (degrees, half-angle each side) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Basic Targeting",
        meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float DirectionalConeAngle = 60.0f;

    /** Require line of sight to target */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Basic Targeting")
    bool bRequireLineOfSight = true;

    /** Trace channel for line of sight checks */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Basic Targeting")
    TEnumAsByte<ECollisionChannel> LineOfSightChannel = ECC_Visibility;

    // ============================================================================
    // SOFT AIM ASSIST (Directional Attack Targeting)
    // Used by: TargetingComponent::FindBestTargetForDirection()
    // ============================================================================

    /** Max range to consider enemies for soft aim assist */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Aim Assist")
    float SoftAimRange = 500.0f;

    /** Maximum angle (degrees) from input direction for an enemy to be a candidate */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Aim Assist",
        meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float SoftAimCandidateAngle = 45.0f;

    /** Angle threshold (degrees) - enemies beyond this are considered "opposite" and ignored */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Aim Assist",
        meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float OppositeAngleThreshold = 120.0f;

    /** Weight for angle alignment in scoring (0-1). Higher = prefer enemies more aligned with input */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Aim Assist",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AngleWeight = 0.7f;

    /** Weight for distance in scoring (0-1). Higher = prefer closer enemies */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Aim Assist",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DistanceWeight = 0.3f;
};
