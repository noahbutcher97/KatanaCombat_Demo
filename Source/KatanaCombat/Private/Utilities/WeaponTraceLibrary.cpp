// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/WeaponTraceLibrary.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

TArray<FVector> UWeaponTraceLibrary::ComputeBladeTracePoints(
    const FVector& BladeBase,
    const FVector& BladeTip,
    int32 NumPoints)
{
    TArray<FVector> Points;
    NumPoints = FMath::Max(1, NumPoints);
    Points.Reserve(NumPoints);

    if (NumPoints == 1)
    {
        // Single point at blade center
        Points.Add((BladeBase + BladeTip) * 0.5f);
    }
    else
    {
        // Evenly spaced from base to tip
        for (int32 i = 0; i < NumPoints; ++i)
        {
            const float Alpha = static_cast<float>(i) / static_cast<float>(NumPoints - 1);
            Points.Add(FMath::Lerp(BladeBase, BladeTip, Alpha));
        }
    }

    return Points;
}

int32 UWeaponTraceLibrary::ComputeAdaptiveSubstepCount(
    float MaxPointVelocity,
    int32 MinSubsteps,
    int32 MaxSubsteps,
    float VelocityThreshold)
{
    MinSubsteps = FMath::Max(1, MinSubsteps);
    MaxSubsteps = FMath::Max(MinSubsteps, MaxSubsteps);

    if (VelocityThreshold <= KINDA_SMALL_NUMBER)
    {
        return MaxSubsteps;
    }

    const float NormalizedVelocity = FMath::Clamp(MaxPointVelocity / VelocityThreshold, 0.0f, 1.0f);
    return FMath::RoundToInt(FMath::Lerp(
        static_cast<float>(MinSubsteps),
        static_cast<float>(MaxSubsteps),
        NormalizedVelocity));
}

float UWeaponTraceLibrary::ComputeMaxTracePointVelocity(
    const TArray<FVector>& PreviousPoints,
    const TArray<FVector>& CurrentPoints,
    float DeltaTime)
{
    if (DeltaTime <= KINDA_SMALL_NUMBER || PreviousPoints.Num() != CurrentPoints.Num())
    {
        return 0.0f;
    }

    float MaxVelocity = 0.0f;
    for (int32 i = 0; i < PreviousPoints.Num(); ++i)
    {
        const float PointVelocity = FVector::Dist(PreviousPoints[i], CurrentPoints[i]) / DeltaTime;
        MaxVelocity = FMath::Max(MaxVelocity, PointVelocity);
    }

    return MaxVelocity;
}

FVector UWeaponTraceLibrary::ComputeTracePointVelocity(
    const FVector& PreviousPosition,
    const FVector& CurrentPosition,
    float DeltaTime)
{
    if (DeltaTime <= KINDA_SMALL_NUMBER)
    {
        return FVector::ZeroVector;
    }

    return (CurrentPosition - PreviousPosition) / DeltaTime;
}

ECombatSurfaceType UWeaponTraceLibrary::MapPhysicalMaterialToSurfaceType(const UPhysicalMaterial* PhysMaterial)
{
    if (!PhysMaterial)
    {
        return ECombatSurfaceType::Default;
    }

    // Map UE physical surface types to combat surface types.
    // These must match Project Settings → Physics → Physical Surface:
    //   SurfaceType1 = Flesh, SurfaceType2 = Armor, SurfaceType3 = Wood,
    //   SurfaceType4 = Stone, SurfaceType5 = Metal
    // Assign the matching PhysicalMaterial asset to each mesh/body in the editor.
    switch (PhysMaterial->SurfaceType)
    {
    case SurfaceType1:  return ECombatSurfaceType::Flesh;
    case SurfaceType2:  return ECombatSurfaceType::Armor;
    case SurfaceType3:  return ECombatSurfaceType::Wood;
    case SurfaceType4:  return ECombatSurfaceType::Stone;
    case SurfaceType5:  return ECombatSurfaceType::Metal;
    default:            return ECombatSurfaceType::Default;
    }
}

float UWeaponTraceLibrary::ComputeHitConfidence(
    const FVector& WeaponVelocity,
    const FVector& ImpactPoint,
    const FVector& BladeBase,
    const FVector& BladeTip,
    float WeaponVelocityThreshold)
{
    // Velocity factor: faster weapon = cleaner hit
    const float Speed = WeaponVelocity.Size();
    const float VelocityFactor = (WeaponVelocityThreshold > KINDA_SMALL_NUMBER)
        ? FMath::Clamp(Speed / WeaponVelocityThreshold, 0.0f, 1.0f)
        : 1.0f;

    // Blade position factor: hits near blade center score highest
    // Project impact point onto blade line segment to find closest point parameter
    const FVector BladeDirection = BladeTip - BladeBase;
    const float BladeLength = BladeDirection.Size();

    float BladeFactor = 1.0f;
    if (BladeLength > KINDA_SMALL_NUMBER)
    {
        const FVector BladeNormalized = BladeDirection / BladeLength;
        const float ProjectedT = FMath::Clamp(
            FVector::DotProduct(ImpactPoint - BladeBase, BladeNormalized) / BladeLength,
            0.0f, 1.0f);

        // Optimal hit zone: 0.3-0.7 along blade (center region)
        // Score falls off toward base (0.0) and tip (1.0) edges
        const float DistFromCenter = FMath::Abs(ProjectedT - 0.5f);
        BladeFactor = FMath::Clamp(1.0f - (DistFromCenter * 2.0f), 0.2f, 1.0f);
    }

    // Combined: 60% velocity, 40% blade position
    return FMath::Clamp(VelocityFactor * 0.6f + BladeFactor * 0.4f, 0.0f, 1.0f);
}

float UWeaponTraceLibrary::PredictHitLikelihood(
    const FVector& BladeBase,
    const FVector& BladeTip,
    const FVector& BladeVelocity,
    const FVector& TargetPosition,
    const FVector& TargetVelocity,
    float TraceRadius,
    float LookAheadTime)
{
    if (LookAheadTime <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    // Predict future positions
    const FVector FutureBladeCenter = ((BladeBase + BladeTip) * 0.5f) + (BladeVelocity * LookAheadTime);
    const FVector FutureTarget = TargetPosition + (TargetVelocity * LookAheadTime);

    // Blade length defines the effective reach zone
    const float BladeHalfLength = FVector::Dist(BladeBase, BladeTip) * 0.5f;
    const float EffectiveReach = BladeHalfLength + TraceRadius;

    // Distance from predicted blade center to predicted target
    const float PredictedDist = FVector::Dist(FutureBladeCenter, FutureTarget);

    if (PredictedDist >= EffectiveReach * 2.0f)
    {
        return 0.0f;
    }

    // Linear falloff: 1.0 at center, 0.0 at 2x effective reach
    return FMath::Clamp(1.0f - (PredictedDist / (EffectiveReach * 2.0f)), 0.0f, 1.0f);
}
