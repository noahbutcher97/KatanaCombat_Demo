// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/CombatMathEnums.h"
#include "Math/CombatMathTypes.h"
#include "SpatialQueryLibrary.generated.h"

/**
 * Configuration for spatial queries
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FSpatialQueryParams
{
    GENERATED_BODY()

    /** Object types to query */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;

    /** Actors to ignore */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
    TArray<AActor*> IgnoreActors;

    /** Collision channel for trace queries */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

    /** Draw debug shapes */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bDebugDraw = false;

    /** Debug draw duration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (EditCondition = "bDebugDraw"))
    float DebugDrawDuration = 0.0f;

    /** Debug draw color */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (EditCondition = "bDebugDraw"))
    FColor DebugColor = FColor::Yellow;
};

/**
 * Result of a cone/FOV query
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FConeQueryResult
{
    GENERATED_BODY()

    /** Actors found in cone */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    TArray<AActor*> Actors;

    /** Distances from cone origin */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    TArray<float> Distances;

    /** Angles from cone axis (degrees) */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    TArray<float> Angles;

    /** Were any actors found? */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bFoundAny = false;

    /** Get closest actor */
    AActor* GetClosest() const { return Actors.Num() > 0 ? Actors[0] : nullptr; }
};

/**
 * Spatial Query Utility Library
 *
 * Static utility functions for spatial queries:
 * - Sphere/box/capsule overlap queries
 * - Cone/FOV queries for combat targeting
 * - Line of sight checks
 * - Filtered queries (by class, interface, tag)
 * - Sorted results (by distance, angle, priority)
 *
 * Design: Foundation for combat targeting, finisher victim selection,
 * and AI threat assessment.
 */
UCLASS()
class KATANACOMBAT_API USpatialQueryLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ========================================================================
    // SPHERE QUERIES
    // ========================================================================

    /**
     * Find all actors in sphere
     *
     * @param World - World context
     * @param Center - Sphere center
     * @param Radius - Sphere radius
     * @param Params - Query parameters
     * @return Query result with found actors
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|Sphere", meta = (WorldContext = "WorldContextObject"))
    static FSpatialQueryResult SphereQuery(
        UObject* WorldContextObject,
        const FVector& Center,
        float Radius,
        const FSpatialQueryParams& Params);

    /**
     * Find actors of specific class in sphere
     *
     * @param World - World context
     * @param Center - Sphere center
     * @param Radius - Sphere radius
     * @param ActorClass - Class to filter by
     * @param IgnoreActors - Actors to ignore
     * @return Found actors sorted by distance
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|Sphere", meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "ActorClass"))
    static TArray<AActor*> SphereQueryByClass(
        UObject* WorldContextObject,
        const FVector& Center,
        float Radius,
        TSubclassOf<AActor> ActorClass,
        const TArray<AActor*>& IgnoreActors);

    /**
     * Find closest actor in sphere
     *
     * @param World - World context
     * @param Center - Sphere center
     * @param Radius - Sphere radius
     * @param Params - Query parameters
     * @return Closest actor or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|Sphere", meta = (WorldContext = "WorldContextObject"))
    static AActor* FindClosestInSphere(
        UObject* WorldContextObject,
        const FVector& Center,
        float Radius,
        const FSpatialQueryParams& Params);

    // ========================================================================
    // BOX QUERIES
    // ========================================================================

    /**
     * Find all actors in axis-aligned box
     *
     * @param World - World context
     * @param Center - Box center
     * @param HalfExtents - Box half-extents
     * @param Params - Query parameters
     * @return Query result with found actors
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|Box", meta = (WorldContext = "WorldContextObject"))
    static FSpatialQueryResult BoxQuery(
        UObject* WorldContextObject,
        const FVector& Center,
        const FVector& HalfExtents,
        const FSpatialQueryParams& Params);

    /**
     * Find all actors in oriented box
     *
     * @param World - World context
     * @param Center - Box center
     * @param HalfExtents - Box half-extents
     * @param Rotation - Box rotation
     * @param Params - Query parameters
     * @return Query result with found actors
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|Box", meta = (WorldContext = "WorldContextObject"))
    static FSpatialQueryResult OrientedBoxQuery(
        UObject* WorldContextObject,
        const FVector& Center,
        const FVector& HalfExtents,
        const FQuat& Rotation,
        const FSpatialQueryParams& Params);

    // ========================================================================
    // CAPSULE QUERIES
    // ========================================================================

    /**
     * Find all actors in capsule
     *
     * @param World - World context
     * @param Start - Capsule start (center of bottom hemisphere)
     * @param End - Capsule end (center of top hemisphere)
     * @param Radius - Capsule radius
     * @param Params - Query parameters
     * @return Query result with found actors
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|Capsule", meta = (WorldContext = "WorldContextObject"))
    static FSpatialQueryResult CapsuleQuery(
        UObject* WorldContextObject,
        const FVector& Start,
        const FVector& End,
        float Radius,
        const FSpatialQueryParams& Params);

    // ========================================================================
    // CONE/FOV QUERIES
    // ========================================================================

    /**
     * Find all actors in cone (FOV check)
     *
     * @param World - World context
     * @param Origin - Cone origin
     * @param Direction - Cone direction
     * @param Length - Cone length
     * @param HalfAngle - Cone half-angle in degrees
     * @param Params - Query parameters
     * @return Cone query result with angles
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|Cone", meta = (WorldContext = "WorldContextObject"))
    static FConeQueryResult ConeQuery(
        UObject* WorldContextObject,
        const FVector& Origin,
        const FVector& Direction,
        float Length,
        float HalfAngle,
        const FSpatialQueryParams& Params);

    /**
     * Check if actor is in field of view
     *
     * @param Observer - Actor doing the looking
     * @param Target - Actor being looked at
     * @param FOVAngle - Total field of view angle in degrees
     * @param MaxDistance - Maximum distance (0 = infinite)
     * @param bRequireLOS - Require line of sight
     * @return True if target is in FOV
     */
    UFUNCTION(BlueprintPure, Category = "Spatial|Cone")
    static bool IsInFieldOfView(
        AActor* Observer,
        AActor* Target,
        float FOVAngle = 90.0f,
        float MaxDistance = 0.0f,
        bool bRequireLOS = false);

    /**
     * Check if point is in cone
     *
     * @param Point - Point to check
     * @param ConeOrigin - Cone origin
     * @param ConeDirection - Cone direction
     * @param ConeLength - Cone length
     * @param ConeHalfAngle - Cone half-angle in degrees
     * @return True if point is inside cone
     */
    UFUNCTION(BlueprintPure, Category = "Spatial|Cone")
    static bool IsPointInCone(
        const FVector& Point,
        const FVector& ConeOrigin,
        const FVector& ConeDirection,
        float ConeLength,
        float ConeHalfAngle);

    /**
     * Find best target in cone (for soft-lock targeting)
     * Prioritizes by angle to center, with distance as tiebreaker
     *
     * @param World - World context
     * @param Origin - Cone origin
     * @param Direction - Preferred direction
     * @param MaxDistance - Maximum targeting distance
     * @param HalfAngle - Cone half-angle
     * @param Params - Query parameters
     * @param AngleWeight - Weight for angle vs distance (0=distance only, 1=angle only)
     * @return Best target actor
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|Cone", meta = (WorldContext = "WorldContextObject"))
    static AActor* FindBestTargetInCone(
        UObject* WorldContextObject,
        const FVector& Origin,
        const FVector& Direction,
        float MaxDistance,
        float HalfAngle,
        const FSpatialQueryParams& Params,
        float AngleWeight = 0.7f);

    // ========================================================================
    // LINE OF SIGHT
    // ========================================================================

    /**
     * Check if there's line of sight between two points
     *
     * @param World - World context
     * @param Start - Start point
     * @param End - End point
     * @param IgnoreActors - Actors to ignore
     * @param TraceChannel - Channel to trace
     * @return True if line of sight exists
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|LOS", meta = (WorldContext = "WorldContextObject"))
    static bool HasLineOfSight(
        UObject* WorldContextObject,
        const FVector& Start,
        const FVector& End,
        const TArray<AActor*>& IgnoreActors,
        ECollisionChannel TraceChannel = ECC_Visibility);

    /**
     * Check if actor can see another actor
     *
     * @param Observer - Observing actor
     * @param Target - Target actor
     * @param bFromEyes - Trace from eye viewpoint (vs actor location)
     * @return True if observer can see target
     */
    UFUNCTION(BlueprintPure, Category = "Spatial|LOS")
    static bool CanActorSee(
        AActor* Observer,
        AActor* Target,
        bool bFromEyes = true);

    /**
     * Find visible actors from list
     *
     * @param World - World context
     * @param Observer - Observing point
     * @param Candidates - Actors to check
     * @param TraceChannel - Collision channel for traces
     * @return Visible actors
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|LOS", meta = (WorldContext = "WorldContextObject"))
    static TArray<AActor*> FilterVisibleActors(
        UObject* WorldContextObject,
        const FVector& Observer,
        const TArray<AActor*>& Candidates,
        ECollisionChannel TraceChannel = ECC_Visibility);

    // ========================================================================
    // FILTERING & SORTING
    // ========================================================================

    /**
     * Filter actors by interface
     *
     * @param Actors - Actors to filter
     * @param InterfaceClass - Interface to check for
     * @return Actors implementing the interface
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|Filter")
    static TArray<AActor*> FilterByInterface(
        const TArray<AActor*>& Actors,
        TSubclassOf<UInterface> InterfaceClass);

    /**
     * Filter actors by gameplay tag
     *
     * @param Actors - Actors to filter
     * @param RequiredTag - Tag that actors must have
     * @return Actors with the tag
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|Filter")
    static TArray<AActor*> FilterByTag(
        const TArray<AActor*>& Actors,
        FName RequiredTag);

    /**
     * Sort actors by distance to point
     *
     * @param Actors - Actors to sort
     * @param Origin - Point to measure from
     * @param bAscending - True for closest first
     * @return Sorted actors
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|Sort")
    static TArray<AActor*> SortByDistance(
        const TArray<AActor*>& Actors,
        const FVector& Origin,
        bool bAscending = true);

    /**
     * Sort actors by angle from direction
     *
     * @param Actors - Actors to sort
     * @param Origin - Origin point
     * @param Direction - Reference direction
     * @param bAscending - True for smallest angle first
     * @return Sorted actors
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|Sort")
    static TArray<AActor*> SortByAngle(
        const TArray<AActor*>& Actors,
        const FVector& Origin,
        const FVector& Direction,
        bool bAscending = true);

    // ========================================================================
    // UTILITY
    // ========================================================================

    /**
     * Get eye viewpoint for actor
     * Falls back to actor location if no viewpoint available
     *
     * @param Actor - Actor to get viewpoint for
     * @param OutLocation - Eye location
     * @param OutRotation - Eye rotation
     */
    UFUNCTION(BlueprintCallable, Category = "Spatial|Utility")
    static void GetActorEyeViewpoint(
        AActor* Actor,
        FVector& OutLocation,
        FRotator& OutRotation);

    /**
     * Calculate signed angle from forward to target (on horizontal plane)
     *
     * @param Origin - Observer location
     * @param Forward - Observer forward direction
     * @param TargetLocation - Target location
     * @return Signed angle in degrees (-180 to 180)
     */
    UFUNCTION(BlueprintPure, Category = "Spatial|Utility")
    static float GetSignedAngleToTarget(
        const FVector& Origin,
        const FVector& Forward,
        const FVector& TargetLocation);

    /**
     * Get direction and distance from origin to target
     *
     * @param Origin - Starting point
     * @param Target - Target point
     * @param OutDirection - Normalized direction
     * @param OutDistance - Distance
     */
    UFUNCTION(BlueprintPure, Category = "Spatial|Utility")
    static void GetDirectionAndDistance(
        const FVector& Origin,
        const FVector& Target,
        FVector& OutDirection,
        float& OutDistance);
};
