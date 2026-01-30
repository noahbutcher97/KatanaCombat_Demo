// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/SpatialQueryLibrary.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameplayTagAssetInterface.h"

// ============================================================================
// SPHERE QUERIES
// ============================================================================

FSpatialQueryResult USpatialQueryLibrary::SphereQuery(
    UObject* WorldContextObject,
    const FVector& Center,
    float Radius,
    const FSpatialQueryParams& Params)
{
    FSpatialQueryResult Result;

    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World || Radius <= 0.0f)
    {
        return Result;
    }

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActors(Params.IgnoreActors);

    FCollisionObjectQueryParams ObjectParams;
    for (const auto& ObjectType : Params.ObjectTypes)
    {
        ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
    }

    // If no object types specified, use pawn by default
    if (Params.ObjectTypes.Num() == 0)
    {
        ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    }

    if (World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams,
        FCollisionShape::MakeSphere(Radius), QueryParams))
    {
        Result.bSuccess = true;

        for (const FOverlapResult& Overlap : Overlaps)
        {
            if (AActor* Actor = Overlap.GetActor())
            {
                if (!Result.Actors.Contains(Actor))
                {
                    Result.Actors.Add(Actor);
                    Result.Components.Add(Overlap.GetComponent());
                    Result.Distances.Add(FVector::Dist(Center, Actor->GetActorLocation()));
                }
            }
        }

        // Sort by distance
        if (Result.Actors.Num() > 1)
        {
            TArray<int32> Indices;
            for (int32 i = 0; i < Result.Actors.Num(); ++i)
            {
                Indices.Add(i);
            }

            Indices.Sort([&Result](int32 A, int32 B)
            {
                return Result.Distances[A] < Result.Distances[B];
            });

            TArray<AActor*> SortedActors;
            TArray<UPrimitiveComponent*> SortedComponents;
            TArray<float> SortedDistances;

            for (int32 Index : Indices)
            {
                SortedActors.Add(Result.Actors[Index]);
                SortedComponents.Add(Result.Components[Index]);
                SortedDistances.Add(Result.Distances[Index]);
            }

            Result.Actors = MoveTemp(SortedActors);
            Result.Components = MoveTemp(SortedComponents);
            Result.Distances = MoveTemp(SortedDistances);
        }
    }

    // Debug draw
    if (Params.bDebugDraw)
    {
        DrawDebugSphere(World, Center, Radius, 16, Params.DebugColor, false, Params.DebugDrawDuration);
    }

    return Result;
}

TArray<AActor*> USpatialQueryLibrary::SphereQueryByClass(
    UObject* WorldContextObject,
    const FVector& Center,
    float Radius,
    TSubclassOf<AActor> ActorClass,
    const TArray<AActor*>& IgnoreActors)
{
    TArray<AActor*> Results;

    FSpatialQueryParams Params;
    Params.IgnoreActors = IgnoreActors;

    FSpatialQueryResult QueryResult = SphereQuery(WorldContextObject, Center, Radius, Params);

    for (AActor* Actor : QueryResult.Actors)
    {
        if (Actor && Actor->IsA(ActorClass))
        {
            Results.Add(Actor);
        }
    }

    return Results;
}

AActor* USpatialQueryLibrary::FindClosestInSphere(
    UObject* WorldContextObject,
    const FVector& Center,
    float Radius,
    const FSpatialQueryParams& Params)
{
    FSpatialQueryResult Result = SphereQuery(WorldContextObject, Center, Radius, Params);
    return Result.Actors.Num() > 0 ? Result.Actors[0] : nullptr;
}

// ============================================================================
// BOX QUERIES
// ============================================================================

FSpatialQueryResult USpatialQueryLibrary::BoxQuery(
    UObject* WorldContextObject,
    const FVector& Center,
    const FVector& HalfExtents,
    const FSpatialQueryParams& Params)
{
    return OrientedBoxQuery(WorldContextObject, Center, HalfExtents, FQuat::Identity, Params);
}

FSpatialQueryResult USpatialQueryLibrary::OrientedBoxQuery(
    UObject* WorldContextObject,
    const FVector& Center,
    const FVector& HalfExtents,
    const FQuat& Rotation,
    const FSpatialQueryParams& Params)
{
    FSpatialQueryResult Result;

    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World)
    {
        return Result;
    }

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActors(Params.IgnoreActors);

    FCollisionObjectQueryParams ObjectParams;
    for (const auto& ObjectType : Params.ObjectTypes)
    {
        ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
    }

    if (Params.ObjectTypes.Num() == 0)
    {
        ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    }

    if (World->OverlapMultiByObjectType(Overlaps, Center, Rotation, ObjectParams,
        FCollisionShape::MakeBox(HalfExtents), QueryParams))
    {
        Result.bSuccess = true;

        for (const FOverlapResult& Overlap : Overlaps)
        {
            if (AActor* Actor = Overlap.GetActor())
            {
                if (!Result.Actors.Contains(Actor))
                {
                    Result.Actors.Add(Actor);
                    Result.Components.Add(Overlap.GetComponent());
                    Result.Distances.Add(FVector::Dist(Center, Actor->GetActorLocation()));
                }
            }
        }
    }

    // Debug draw
    if (Params.bDebugDraw)
    {
        DrawDebugBox(World, Center, HalfExtents, Rotation, Params.DebugColor, false, Params.DebugDrawDuration);
    }

    return Result;
}

// ============================================================================
// CAPSULE QUERIES
// ============================================================================

FSpatialQueryResult USpatialQueryLibrary::CapsuleQuery(
    UObject* WorldContextObject,
    const FVector& Start,
    const FVector& End,
    float Radius,
    const FSpatialQueryParams& Params)
{
    FSpatialQueryResult Result;

    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World || Radius <= 0.0f)
    {
        return Result;
    }

    FVector Center = (Start + End) * 0.5f;
    float HalfHeight = FVector::Dist(Start, End) * 0.5f + Radius;
    FQuat Rotation = FRotationMatrix::MakeFromZ(End - Start).ToQuat();

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActors(Params.IgnoreActors);

    FCollisionObjectQueryParams ObjectParams;
    for (const auto& ObjectType : Params.ObjectTypes)
    {
        ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
    }

    if (Params.ObjectTypes.Num() == 0)
    {
        ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    }

    if (World->OverlapMultiByObjectType(Overlaps, Center, Rotation, ObjectParams,
        FCollisionShape::MakeCapsule(Radius, HalfHeight), QueryParams))
    {
        Result.bSuccess = true;

        for (const FOverlapResult& Overlap : Overlaps)
        {
            if (AActor* Actor = Overlap.GetActor())
            {
                if (!Result.Actors.Contains(Actor))
                {
                    Result.Actors.Add(Actor);
                    Result.Components.Add(Overlap.GetComponent());
                    Result.Distances.Add(FVector::Dist(Center, Actor->GetActorLocation()));
                }
            }
        }
    }

    // Debug draw
    if (Params.bDebugDraw)
    {
        DrawDebugCapsule(World, Center, HalfHeight, Radius, Rotation, Params.DebugColor, false, Params.DebugDrawDuration);
    }

    return Result;
}

// ============================================================================
// CONE/FOV QUERIES
// ============================================================================

FConeQueryResult USpatialQueryLibrary::ConeQuery(
    UObject* WorldContextObject,
    const FVector& Origin,
    const FVector& Direction,
    float Length,
    float HalfAngle,
    const FSpatialQueryParams& Params)
{
    FConeQueryResult Result;

    // First do a sphere query with the cone's length as radius
    FSpatialQueryResult SphereResult = SphereQuery(WorldContextObject, Origin, Length, Params);

    if (!SphereResult.bSuccess)
    {
        return Result;
    }

    FVector DirNorm = Direction.GetSafeNormal();
    float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(HalfAngle));

    // Filter to only actors in the cone
    for (int32 i = 0; i < SphereResult.Actors.Num(); ++i)
    {
        AActor* Actor = SphereResult.Actors[i];
        FVector ToActor = (Actor->GetActorLocation() - Origin).GetSafeNormal();

        float DotProduct = FVector::DotProduct(DirNorm, ToActor);
        if (DotProduct >= CosHalfAngle)
        {
            Result.Actors.Add(Actor);
            Result.Distances.Add(SphereResult.Distances[i]);
            Result.Angles.Add(FMath::RadiansToDegrees(FMath::Acos(DotProduct)));
        }
    }

    Result.bFoundAny = Result.Actors.Num() > 0;

    // Sort by angle (closest to center first)
    if (Result.Actors.Num() > 1)
    {
        TArray<int32> Indices;
        for (int32 i = 0; i < Result.Actors.Num(); ++i)
        {
            Indices.Add(i);
        }

        Indices.Sort([&Result](int32 A, int32 B)
        {
            return Result.Angles[A] < Result.Angles[B];
        });

        TArray<AActor*> SortedActors;
        TArray<float> SortedDistances;
        TArray<float> SortedAngles;

        for (int32 Index : Indices)
        {
            SortedActors.Add(Result.Actors[Index]);
            SortedDistances.Add(Result.Distances[Index]);
            SortedAngles.Add(Result.Angles[Index]);
        }

        Result.Actors = MoveTemp(SortedActors);
        Result.Distances = MoveTemp(SortedDistances);
        Result.Angles = MoveTemp(SortedAngles);
    }

    // Debug draw
    if (Params.bDebugDraw)
    {
        UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
        if (World)
        {
            // Draw cone lines
            FVector Right = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal() * Length * FMath::Sin(FMath::DegreesToRadians(HalfAngle));
            FVector Up = FVector::CrossProduct(Right, Direction).GetSafeNormal() * Length * FMath::Sin(FMath::DegreesToRadians(HalfAngle));
            FVector EndCenter = Origin + DirNorm * Length;

            DrawDebugLine(World, Origin, EndCenter + Right, Params.DebugColor, false, Params.DebugDrawDuration);
            DrawDebugLine(World, Origin, EndCenter - Right, Params.DebugColor, false, Params.DebugDrawDuration);
            DrawDebugLine(World, Origin, EndCenter + Up, Params.DebugColor, false, Params.DebugDrawDuration);
            DrawDebugLine(World, Origin, EndCenter - Up, Params.DebugColor, false, Params.DebugDrawDuration);
        }
    }

    return Result;
}

bool USpatialQueryLibrary::IsInFieldOfView(
    AActor* Observer,
    AActor* Target,
    float FOVAngle,
    float MaxDistance,
    bool bRequireLOS)
{
    if (!Observer || !Target)
    {
        return false;
    }

    FVector EyeLocation;
    FRotator EyeRotation;
    GetActorEyeViewpoint(Observer, EyeLocation, EyeRotation);

    FVector TargetLocation = Target->GetActorLocation();
    FVector ToTarget = TargetLocation - EyeLocation;
    float Distance = ToTarget.Size();

    // Distance check
    if (MaxDistance > 0.0f && Distance > MaxDistance)
    {
        return false;
    }

    // Angle check
    FVector Forward = EyeRotation.Vector();
    float DotProduct = FVector::DotProduct(Forward, ToTarget.GetSafeNormal());
    float HalfAngleRad = FMath::DegreesToRadians(FOVAngle * 0.5f);

    if (DotProduct < FMath::Cos(HalfAngleRad))
    {
        return false;
    }

    // LOS check
    if (bRequireLOS)
    {
        return CanActorSee(Observer, Target, true);
    }

    return true;
}

bool USpatialQueryLibrary::IsPointInCone(
    const FVector& Point,
    const FVector& ConeOrigin,
    const FVector& ConeDirection,
    float ConeLength,
    float ConeHalfAngle)
{
    FVector ToPoint = Point - ConeOrigin;
    float Distance = ToPoint.Size();

    // Distance check
    if (Distance > ConeLength)
    {
        return false;
    }

    // Angle check
    FVector DirNorm = ConeDirection.GetSafeNormal();
    float DotProduct = FVector::DotProduct(DirNorm, ToPoint.GetSafeNormal());
    float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngle));

    return DotProduct >= CosHalfAngle;
}

AActor* USpatialQueryLibrary::FindBestTargetInCone(
    UObject* WorldContextObject,
    const FVector& Origin,
    const FVector& Direction,
    float MaxDistance,
    float HalfAngle,
    const FSpatialQueryParams& Params,
    float AngleWeight)
{
    FConeQueryResult Result = ConeQuery(WorldContextObject, Origin, Direction, MaxDistance, HalfAngle, Params);

    if (!Result.bFoundAny)
    {
        return nullptr;
    }

    // Score each target
    float BestScore = MAX_FLT;
    AActor* BestTarget = nullptr;

    for (int32 i = 0; i < Result.Actors.Num(); ++i)
    {
        // Normalize angle and distance to 0-1 range
        float NormalizedAngle = Result.Angles[i] / HalfAngle;
        float NormalizedDistance = Result.Distances[i] / MaxDistance;

        // Combined score (lower is better)
        float Score = NormalizedAngle * AngleWeight + NormalizedDistance * (1.0f - AngleWeight);

        if (Score < BestScore)
        {
            BestScore = Score;
            BestTarget = Result.Actors[i];
        }
    }

    return BestTarget;
}

// ============================================================================
// LINE OF SIGHT
// ============================================================================

bool USpatialQueryLibrary::HasLineOfSight(
    UObject* WorldContextObject,
    const FVector& Start,
    const FVector& End,
    const TArray<AActor*>& IgnoreActors,
    ECollisionChannel TraceChannel)
{
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World)
    {
        return false;
    }

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActors(IgnoreActors);

    FHitResult Hit;
    return !World->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, QueryParams);
}

bool USpatialQueryLibrary::CanActorSee(AActor* Observer, AActor* Target, bool bFromEyes)
{
    if (!Observer || !Target)
    {
        return false;
    }

    FVector StartLocation;
    if (bFromEyes)
    {
        FRotator EyeRotation;
        GetActorEyeViewpoint(Observer, StartLocation, EyeRotation);
    }
    else
    {
        StartLocation = Observer->GetActorLocation();
    }

    TArray<AActor*> IgnoreActors = { Observer, Target };
    return HasLineOfSight(Observer, StartLocation, Target->GetActorLocation(), IgnoreActors);
}

TArray<AActor*> USpatialQueryLibrary::FilterVisibleActors(
    UObject* WorldContextObject,
    const FVector& Observer,
    const TArray<AActor*>& Candidates,
    ECollisionChannel TraceChannel)
{
    TArray<AActor*> VisibleActors;

    for (AActor* Actor : Candidates)
    {
        if (Actor)
        {
            TArray<AActor*> Ignore = { Actor };
            if (HasLineOfSight(WorldContextObject, Observer, Actor->GetActorLocation(), Ignore, TraceChannel))
            {
                VisibleActors.Add(Actor);
            }
        }
    }

    return VisibleActors;
}

// ============================================================================
// FILTERING & SORTING
// ============================================================================

TArray<AActor*> USpatialQueryLibrary::FilterByInterface(
    const TArray<AActor*>& Actors,
    TSubclassOf<UInterface> InterfaceClass)
{
    TArray<AActor*> Filtered;

    if (!InterfaceClass)
    {
        return Filtered;
    }

    for (AActor* Actor : Actors)
    {
        if (Actor && Actor->GetClass()->ImplementsInterface(InterfaceClass))
        {
            Filtered.Add(Actor);
        }
    }

    return Filtered;
}

TArray<AActor*> USpatialQueryLibrary::FilterByTag(
    const TArray<AActor*>& Actors,
    FName RequiredTag)
{
    TArray<AActor*> Filtered;

    for (AActor* Actor : Actors)
    {
        if (Actor && Actor->ActorHasTag(RequiredTag))
        {
            Filtered.Add(Actor);
        }
    }

    return Filtered;
}

TArray<AActor*> USpatialQueryLibrary::SortByDistance(
    const TArray<AActor*>& Actors,
    const FVector& Origin,
    bool bAscending)
{
    TArray<AActor*> Sorted = Actors;

    // UE5.6 TArray::Sort auto-dereferences pointers, so predicate receives AActor& not AActor*
    Sorted.Sort([&Origin, bAscending](const AActor& A, const AActor& B)
    {
        float DistA = FVector::DistSquared(A.GetActorLocation(), Origin);
        float DistB = FVector::DistSquared(B.GetActorLocation(), Origin);

        return bAscending ? (DistA < DistB) : (DistA > DistB);
    });

    return Sorted;
}

TArray<AActor*> USpatialQueryLibrary::SortByAngle(
    const TArray<AActor*>& Actors,
    const FVector& Origin,
    const FVector& Direction,
    bool bAscending)
{
    TArray<AActor*> Sorted = Actors;
    FVector DirNorm = Direction.GetSafeNormal();

    // UE5.6 TArray::Sort auto-dereferences pointers, so predicate receives AActor& not AActor*
    Sorted.Sort([&Origin, &DirNorm, bAscending](const AActor& A, const AActor& B)
    {
        FVector ToA = (A.GetActorLocation() - Origin).GetSafeNormal();
        FVector ToB = (B.GetActorLocation() - Origin).GetSafeNormal();

        float AngleA = FMath::Acos(FVector::DotProduct(DirNorm, ToA));
        float AngleB = FMath::Acos(FVector::DotProduct(DirNorm, ToB));

        return bAscending ? (AngleA < AngleB) : (AngleA > AngleB);
    });

    return Sorted;
}

// ============================================================================
// UTILITY
// ============================================================================

void USpatialQueryLibrary::GetActorEyeViewpoint(AActor* Actor, FVector& OutLocation, FRotator& OutRotation)
{
    if (!Actor)
    {
        OutLocation = FVector::ZeroVector;
        OutRotation = FRotator::ZeroRotator;
        return;
    }

    // Try to get from pawn/controller first
    if (APawn* Pawn = Cast<APawn>(Actor))
    {
        if (AController* Controller = Pawn->GetController())
        {
            Controller->GetPlayerViewPoint(OutLocation, OutRotation);
            return;
        }
    }

    // Fall back to actor location/rotation
    OutLocation = Actor->GetActorLocation();
    OutRotation = Actor->GetActorRotation();

    // Offset for typical eye height
    OutLocation.Z += 70.0f;
}

float USpatialQueryLibrary::GetSignedAngleToTarget(
    const FVector& Origin,
    const FVector& Forward,
    const FVector& TargetLocation)
{
    FVector ToTarget = (TargetLocation - Origin).GetSafeNormal();
    FVector ForwardNorm = Forward.GetSafeNormal();

    // Project to horizontal plane
    ToTarget.Z = 0.0f;
    ToTarget.Normalize();
    ForwardNorm.Z = 0.0f;
    ForwardNorm.Normalize();

    if (ToTarget.IsNearlyZero() || ForwardNorm.IsNearlyZero())
    {
        return 0.0f;
    }

    float Dot = FVector::DotProduct(ForwardNorm, ToTarget);
    float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));

    // Determine sign
    FVector Cross = FVector::CrossProduct(ForwardNorm, ToTarget);
    if (Cross.Z < 0.0f)
    {
        Angle = -Angle;
    }

    return Angle;
}

void USpatialQueryLibrary::GetDirectionAndDistance(
    const FVector& Origin,
    const FVector& Target,
    FVector& OutDirection,
    float& OutDistance)
{
    FVector Delta = Target - Origin;
    OutDistance = Delta.Size();
    OutDirection = OutDistance > SMALL_NUMBER ? (Delta / OutDistance) : FVector::ForwardVector;
}
