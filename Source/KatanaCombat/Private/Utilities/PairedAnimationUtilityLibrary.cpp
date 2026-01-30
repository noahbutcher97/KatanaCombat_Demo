// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/PairedAnimationUtilityLibrary.h"
#include "Data/PairedAnimationData.h"
#include "Debug/DebugUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

// ============================================================================
// POSITION CALCULATION
// ============================================================================

FTransform UPairedAnimationUtilityLibrary::CalculateVictimTransform(
    const FTransform& AttackerTransform,
    const FVector& RelativePosition,
    int32 VictimFacingMode,
    const FRotator& RelativeRotation)
{
    // Calculate victim world position from attacker-relative offset
    const FVector WorldPosition = AttackerTransform.TransformPosition(RelativePosition);

    // Calculate victim rotation based on facing mode
    FRotator VictimRotation;

    if (VictimFacingMode == -1)
    {
        // Face toward attacker
        const FVector ToAttacker = AttackerTransform.GetLocation() - WorldPosition;
        VictimRotation = ToAttacker.Rotation();
    }
    else if (VictimFacingMode == 1)
    {
        // Face away from attacker (same direction as attacker faces)
        VictimRotation = AttackerTransform.GetRotation().Rotator();
    }
    else
    {
        // Use fixed relative rotation
        VictimRotation = AttackerTransform.GetRotation().Rotator() + RelativeRotation;
    }

    return FTransform(VictimRotation, WorldPosition, FVector::OneVector);
}

FTransform UPairedAnimationUtilityLibrary::CalculateVictimTransformFromData(
    const FTransform& AttackerTransform,
    const UPairedAnimationData* AnimationData)
{
    if (!AnimationData)
    {
        return AttackerTransform;
    }

    return CalculateVictimTransform(
        AttackerTransform,
        AnimationData->VictimRelativePosition,
        AnimationData->VictimFacingMode,
        AnimationData->VictimRelativeRotation);
}

FTransform UPairedAnimationUtilityLibrary::InterpolateTransform(
    const FTransform& CurrentTransform,
    const FTransform& TargetTransform,
    float Alpha)
{
    FTransform Result;

    // Lerp location
    Result.SetLocation(FMath::Lerp(
        CurrentTransform.GetLocation(),
        TargetTransform.GetLocation(),
        Alpha));

    // Slerp rotation for smooth interpolation
    Result.SetRotation(FQuat::Slerp(
        CurrentTransform.GetRotation(),
        TargetTransform.GetRotation(),
        Alpha));

    // Lerp scale (typically 1,1,1)
    Result.SetScale3D(FMath::Lerp(
        CurrentTransform.GetScale3D(),
        TargetTransform.GetScale3D(),
        Alpha));

    return Result;
}

// ============================================================================
// VALIDATION
// ============================================================================

FPairedAnimationValidation UPairedAnimationUtilityLibrary::ValidatePairedAnimation(
    UWorld* World,
    const FVector& AttackerLocation,
    const FVector& VictimLocation,
    const UPairedAnimationData* AnimationData,
    float ClearanceRadius)
{
    FPairedAnimationValidation Result;

    if (!AnimationData)
    {
        Result.FailureReason = TEXT("Animation data is null");
        return Result;
    }

    // Calculate distance
    Result.Distance = GetHorizontalDistance(AttackerLocation, VictimLocation);

    // Check trigger distance
    if (Result.Distance < AnimationData->MinTriggerDistance)
    {
        Result.FailureReason = FString::Printf(TEXT("Too close (%.1f < %.1f min)"),
            Result.Distance, AnimationData->MinTriggerDistance);
        return Result;
    }

    if (Result.Distance > AnimationData->MaxTriggerDistance)
    {
        Result.FailureReason = FString::Printf(TEXT("Too far (%.1f > %.1f max)"),
            Result.Distance, AnimationData->MaxTriggerDistance);
        return Result;
    }

    // Calculate angle from attacker to victim
    const FVector ToVictim = (VictimLocation - AttackerLocation).GetSafeNormal2D();
    Result.Angle = FMath::RadiansToDegrees(FMath::Acos(ToVictim.X));  // Simplified, assumes forward is +X

    // Check path is clear
    TArray<AActor*> EmptyIgnoreList;
    if (World && !IsPathClear(World, AttackerLocation, VictimLocation, ClearanceRadius, EmptyIgnoreList))
    {
        Result.FailureReason = TEXT("Path blocked by obstacle");
        return Result;
    }

    // Calculate target victim transform
    FTransform AttackerTransform(FRotator::ZeroRotator, AttackerLocation);
    Result.SuggestedTransform = CalculateVictimTransformFromData(AttackerTransform, AnimationData);

    // Check if warp distance is acceptable
    const float WarpDistance = FVector::Dist(VictimLocation, Result.SuggestedTransform.GetLocation());
    if (WarpDistance > AnimationData->MaxWarpDistance)
    {
        Result.FailureReason = FString::Printf(TEXT("Warp distance too large (%.1f > %.1f max)"),
            WarpDistance, AnimationData->MaxWarpDistance);
        return Result;
    }

    Result.bIsValid = true;
    return Result;
}

bool UPairedAnimationUtilityLibrary::IsPathClear(
    UWorld* World,
    const FVector& Start,
    const FVector& End,
    float ClearanceRadius,
    const TArray<AActor*>& ActorsToIgnore)
{
    if (!World)
    {
        return true;  // Assume clear if no world
    }

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActors(ActorsToIgnore);

    FHitResult Hit;
    const bool bHit = World->SweepSingleByChannel(
        Hit,
        Start,
        End,
        FQuat::Identity,
        ECC_Visibility,
        FCollisionShape::MakeSphere(ClearanceRadius),
        QueryParams);

    return !bHit;
}

bool UPairedAnimationUtilityLibrary::IsPositionClear(
    UWorld* World,
    const FVector& Location,
    float CapsuleRadius,
    float CapsuleHalfHeight,
    AActor* ActorToIgnore)
{
    if (!World)
    {
        return true;
    }

    FCollisionQueryParams QueryParams;
    if (ActorToIgnore)
    {
        QueryParams.AddIgnoredActor(ActorToIgnore);
    }

    TArray<FOverlapResult> Overlaps;
    const bool bHasOverlap = World->OverlapMultiByChannel(
        Overlaps,
        Location,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
        QueryParams);

    return !bHasOverlap;
}

bool UPairedAnimationUtilityLibrary::IsVictimInAngleRange(
    const FTransform& AttackerTransform,
    const FVector& VictimLocation,
    float MaxAngle)
{
    const FVector AttackerForward = AttackerTransform.GetRotation().GetForwardVector();
    const FVector ToVictim = (VictimLocation - AttackerTransform.GetLocation()).GetSafeNormal();

    const float DotProduct = FVector::DotProduct(AttackerForward, ToVictim);
    const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f)));

    return AngleDegrees <= MaxAngle;
}

// ============================================================================
// TERRAIN ADJUSTMENT
// ============================================================================

FTransform UPairedAnimationUtilityLibrary::AdjustTransformToTerrain(
    UWorld* World,
    const FTransform& Transform,
    float HeightOffset,
    AActor* ActorToIgnore)
{
    if (!World)
    {
        return Transform;
    }

    // Use existing DebugUtils function for ground sampling
    const FVector AdjustedLocation = UDebugUtils::AdjustLocationToGround(
        World,
        Transform.GetLocation(),
        HeightOffset,
        ActorToIgnore,
        false  // No debug draw
    );

    FTransform Result = Transform;
    Result.SetLocation(AdjustedLocation);
    return Result;
}

FTransform UPairedAnimationUtilityLibrary::GetTerrainAdjustedVictimTransform(
    UWorld* World,
    const FTransform& AttackerTransform,
    const UPairedAnimationData* AnimationData,
    float VictimCapsuleHalfHeight,
    AActor* VictimActor)
{
    // Calculate base victim transform
    FTransform VictimTransform = CalculateVictimTransformFromData(AttackerTransform, AnimationData);

    // Adjust for terrain if warping is enabled
    if (AnimationData && AnimationData->VictimWarpConfig.bAdjustToTerrain && World)
    {
        VictimTransform = AdjustTransformToTerrain(
            World,
            VictimTransform,
            VictimCapsuleHalfHeight,
            VictimActor);
    }

    return VictimTransform;
}

// ============================================================================
// CONTACT POINTS
// ============================================================================

FVector UPairedAnimationUtilityLibrary::GetBoneWorldLocation(
    USkeletalMeshComponent* Mesh,
    FName BoneName)
{
    if (!Mesh)
    {
        return FVector::ZeroVector;
    }

    const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
    if (BoneIndex == INDEX_NONE)
    {
        return FVector::ZeroVector;
    }

    return Mesh->GetBoneLocation(BoneName);
}

FVector UPairedAnimationUtilityLibrary::CalculateContactPoint(
    USkeletalMeshComponent* AttackerMesh,
    FName AttackerBoneName,
    USkeletalMeshComponent* VictimMesh,
    FName VictimBoneName)
{
    const FVector AttackerBone = GetBoneWorldLocation(AttackerMesh, AttackerBoneName);
    const FVector VictimBone = GetBoneWorldLocation(VictimMesh, VictimBoneName);

    // If either bone is invalid, return the valid one or zero
    if (AttackerBone.IsZero())
    {
        return VictimBone;
    }
    if (VictimBone.IsZero())
    {
        return AttackerBone;
    }

    // Return midpoint between contact bones
    return (AttackerBone + VictimBone) * 0.5f;
}

// ============================================================================
// DISTANCE CALCULATIONS
// ============================================================================

float UPairedAnimationUtilityLibrary::GetHorizontalDistance(
    const FVector& Location1,
    const FVector& Location2)
{
    return FVector::Dist2D(Location1, Location2);
}

bool UPairedAnimationUtilityLibrary::IsInTriggerRange(
    const FVector& AttackerLocation,
    const FVector& VictimLocation,
    const UPairedAnimationData* AnimationData)
{
    if (!AnimationData)
    {
        return false;
    }

    const float Distance = GetHorizontalDistance(AttackerLocation, VictimLocation);
    return Distance >= AnimationData->MinTriggerDistance &&
           Distance <= AnimationData->MaxTriggerDistance;
}

bool UPairedAnimationUtilityLibrary::IsWithinWarpDistance(
    const FVector& VictimLocation,
    const FVector& TargetLocation,
    float MaxWarpDistance)
{
    return FVector::Dist(VictimLocation, TargetLocation) <= MaxWarpDistance;
}

// ============================================================================
// WARP TARGET SETUP
// ============================================================================

FTransform UPairedAnimationUtilityLibrary::CalculateWarpTarget(
    AActor* TargetActor,
    const FPairedWarpConfig& WarpConfig,
    AActor* SourceActor)
{
    if (!TargetActor)
    {
        return FTransform::Identity;
    }

    FTransform Result;
    Result.SetLocation(TargetActor->GetActorLocation());
    Result.SetRotation(TargetActor->GetActorQuat());

    // If source actor provided, calculate facing rotation
    if (SourceActor && WarpConfig.bWarpRotation)
    {
        const FVector ToTarget = TargetActor->GetActorLocation() - SourceActor->GetActorLocation();
        Result.SetRotation(ToTarget.Rotation().Quaternion());
    }

    return Result;
}

FTransform UPairedAnimationUtilityLibrary::CalculateAttackerWarpTarget(
    const FVector& VictimLocation,
    const FTransform& AttackerTransform,
    const FPairedWarpConfig& WarpConfig)
{
    FTransform Result = AttackerTransform;

    // Attacker typically only rotates to face victim
    if (WarpConfig.bWarpRotation)
    {
        const FVector ToVictim = VictimLocation - AttackerTransform.GetLocation();
        Result.SetRotation(ToVictim.Rotation().Quaternion());
    }

    // Optionally translate (usually disabled for attacker)
    if (WarpConfig.bWarpTranslation)
    {
        // Move toward victim by a fraction if needed
        const FVector Direction = (VictimLocation - AttackerTransform.GetLocation()).GetSafeNormal();
        const float CurrentDistance = FVector::Dist(AttackerTransform.GetLocation(), VictimLocation);

        // Don't move if already within max warp distance
        if (CurrentDistance > WarpConfig.MaxWarpDistance)
        {
            const FVector NewLocation = AttackerTransform.GetLocation() +
                Direction * (CurrentDistance - WarpConfig.MaxWarpDistance);
            Result.SetLocation(NewLocation);
        }
    }

    return Result;
}
