// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WeaponTraceLibrary.generated.h"

class UPhysicalMaterial;

/**
 * Stateless utility functions for weapon hit detection trace math.
 *
 * Handles:
 * - Blade segmentation: computing trace points along a weapon's length
 * - Adaptive substeps: velocity-based interpolation count for accurate sweeps
 * - Trace point velocity: max movement speed across blade sample points
 *
 * All functions are pure static — no UObject references, no side effects.
 * UObject-dependent operations (socket queries, world sweeps) stay in WeaponComponent.
 */
UCLASS()
class KATANACOMBAT_API UWeaponTraceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Compute evenly-spaced trace points along a blade from base to tip.
     * @param BladeBase - World-space position of weapon base (start socket)
     * @param BladeTip - World-space position of weapon tip (end socket)
     * @param NumPoints - Number of points to generate (1=tip only, 2=base+tip, 3=base+mid+tip)
     * @return Array of world-space positions from base to tip
     */
    UFUNCTION(BlueprintPure, Category = "Weapon Trace|Blade")
    static TArray<FVector> ComputeBladeTracePoints(
        const FVector& BladeBase,
        const FVector& BladeTip,
        int32 NumPoints);

    /**
     * Compute velocity-adaptive substep count for hit detection.
     * Scales linearly from MinSubsteps at zero velocity to MaxSubsteps at VelocityThreshold.
     * @param MaxPointVelocity - Fastest trace point velocity this frame (units/sec)
     * @param MinSubsteps - Minimum substeps (used at low velocity)
     * @param MaxSubsteps - Maximum substeps (used at high velocity)
     * @param VelocityThreshold - Velocity at which MaxSubsteps is reached (units/sec)
     * @return Substep count for this frame
     */
    UFUNCTION(BlueprintPure, Category = "Weapon Trace|Substeps")
    static int32 ComputeAdaptiveSubstepCount(
        float MaxPointVelocity,
        int32 MinSubsteps,
        int32 MaxSubsteps,
        float VelocityThreshold);

    /**
     * Compute the maximum velocity across all trace points.
     * Uses the fastest-moving point (typically the tip) to drive substep adaptation.
     * @param PreviousPoints - Previous frame positions for each trace point
     * @param CurrentPoints - Current frame positions for each trace point
     * @param DeltaTime - Frame delta time in seconds
     * @return Maximum velocity magnitude across all points (units/sec)
     */
    UFUNCTION(BlueprintPure, Category = "Weapon Trace|Velocity")
    static float ComputeMaxTracePointVelocity(
        const TArray<FVector>& PreviousPoints,
        const TArray<FVector>& CurrentPoints,
        float DeltaTime);

    /**
     * Compute the velocity vector for a specific trace point (typically the tip).
     * @param PreviousPosition - Previous frame position
     * @param CurrentPosition - Current frame position
     * @param DeltaTime - Frame delta time in seconds
     * @return Velocity vector (units/sec)
     */
    UFUNCTION(BlueprintPure, Category = "Weapon Trace|Velocity")
    static FVector ComputeTracePointVelocity(
        const FVector& PreviousPosition,
        const FVector& CurrentPosition,
        float DeltaTime);

    // ========================================================================
    // SURFACE TYPE MAPPING
    // ========================================================================

    /**
     * Map UE PhysicalMaterial to combat surface type for material-dependent FX.
     * Uses SurfaceType from PhysicalMaterial (EPhysicalSurface) to map to ECombatSurfaceType.
     * @param PhysMaterial - Physical material from hit result (can be null)
     * @return Combat surface type (Default if no material or unmapped type)
     */
    UFUNCTION(BlueprintPure, Category = "Weapon Trace|Surface")
    static ECombatSurfaceType MapPhysicalMaterialToSurfaceType(const UPhysicalMaterial* PhysMaterial);

    // ========================================================================
    // HIT CONFIDENCE SCORING
    // ========================================================================

    /**
     * Compute hit confidence/quality score based on weapon state at impact.
     * Higher confidence = cleaner hit = more impactful effects.
     * @param WeaponVelocity - Weapon tip velocity at impact (units/sec)
     * @param ImpactPoint - World-space impact location
     * @param BladeBase - World-space blade base position at impact
     * @param BladeTip - World-space blade tip position at impact
     * @param WeaponVelocityThreshold - Velocity at which confidence maxes out (units/sec)
     * @return Confidence score 0.0 (glancing/weak) to 1.0 (clean center-blade hit at speed)
     */
    UFUNCTION(BlueprintPure, Category = "Weapon Trace|Confidence")
    static float ComputeHitConfidence(
        const FVector& WeaponVelocity,
        const FVector& ImpactPoint,
        const FVector& BladeBase,
        const FVector& BladeTip,
        float WeaponVelocityThreshold = 1000.0f);

    // ========================================================================
    // TRAJECTORY PREDICTION
    // ========================================================================

    /**
     * Estimate likelihood that a weapon swing will connect with a moving target.
     * Useful for AI attack timing and optional trace-ahead bias.
     * @param BladeBase - Current blade base position
     * @param BladeTip - Current blade tip position
     * @param BladeVelocity - Blade tip velocity (units/sec)
     * @param TargetPosition - Target's current position
     * @param TargetVelocity - Target's movement velocity (units/sec)
     * @param TraceRadius - Sweep sphere radius
     * @param LookAheadTime - How far ahead to predict (seconds)
     * @return 0.0 (no chance) to 1.0 (guaranteed hit) based on predicted proximity
     */
    UFUNCTION(BlueprintPure, Category = "Weapon Trace|Prediction")
    static float PredictHitLikelihood(
        const FVector& BladeBase,
        const FVector& BladeTip,
        const FVector& BladeVelocity,
        const FVector& TargetPosition,
        const FVector& TargetVelocity,
        float TraceRadius,
        float LookAheadTime = 0.1f);
};
