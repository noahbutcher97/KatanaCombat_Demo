// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/CombatMathEnums.h"
#include "GeometryMathLibrary.generated.h"

/**
 * Bounding volume result
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FBoundingVolumeResult
{
    GENERATED_BODY()

    /** Center of the bounding volume */
    UPROPERTY(BlueprintReadOnly, Category = "Geometry")
    FVector Center = FVector::ZeroVector;

    /** Extents (half-sizes) for box volumes */
    UPROPERTY(BlueprintReadOnly, Category = "Geometry")
    FVector Extents = FVector::ZeroVector;

    /** Radius for sphere volumes */
    UPROPERTY(BlueprintReadOnly, Category = "Geometry")
    float Radius = 0.0f;

    /** Rotation for oriented volumes */
    UPROPERTY(BlueprintReadOnly, Category = "Geometry")
    FQuat Rotation = FQuat::Identity;

    /** Type of bounding volume */
    UPROPERTY(BlueprintReadOnly, Category = "Geometry")
    EBoundingVolumeType Type = EBoundingVolumeType::AABB;

    /** Is this volume valid? */
    bool IsValid() const { return Radius > 0.0f || !Extents.IsNearlyZero(); }
};

/**
 * Result of a closest point query
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FClosestPointResult
{
    GENERATED_BODY()

    /** The closest point found */
    UPROPERTY(BlueprintReadOnly, Category = "Geometry")
    FVector ClosestPoint = FVector::ZeroVector;

    /** Distance from query point to closest point */
    UPROPERTY(BlueprintReadOnly, Category = "Geometry")
    float Distance = 0.0f;

    /** Parameter along line/segment (0-1 for segments) */
    UPROPERTY(BlueprintReadOnly, Category = "Geometry")
    float Parameter = 0.0f;

    /** Was a valid closest point found? */
    UPROPERTY(BlueprintReadOnly, Category = "Geometry")
    bool bValid = false;
};

/**
 * Geometry Math Utility Library
 *
 * Static utility functions for geometric calculations:
 * - Multiple distance formulas (Euclidean, Manhattan, Chebyshev, etc.)
 * - Bounding volume calculations (AABB, OBB, Sphere, Capsule)
 * - Closest point queries
 * - Angle calculations and normalization
 * - Projection utilities
 *
 * Design: Foundation for spatial analysis in paired animations
 * and combat targeting systems.
 */
UCLASS()
class KATANACOMBAT_API UGeometryMathLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ========================================================================
    // DISTANCE CALCULATIONS
    // ========================================================================

    /**
     * Calculate distance using specified formula
     *
     * @param A - First point
     * @param B - Second point
     * @param Formula - Distance formula to use
     * @return Distance between points using specified formula
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Distance", meta = (DisplayName = "Calculate Distance"))
    static float CalculateDistance(
        const FVector& A,
        const FVector& B,
        EDistanceFormula Formula = EDistanceFormula::Euclidean);

    /**
     * Standard 3D Euclidean distance: sqrt(dx² + dy² + dz²)
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Distance")
    static float EuclideanDistance(const FVector& A, const FVector& B);

    /**
     * 2D Euclidean distance (ignores Z): sqrt(dx² + dy²)
     * Useful for ground-based distance checks
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Distance")
    static float EuclideanDistance2D(const FVector& A, const FVector& B);

    /**
     * Manhattan distance: |dx| + |dy| + |dz|
     * Useful for grid-based or tile-based calculations
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Distance")
    static float ManhattanDistance(const FVector& A, const FVector& B);

    /**
     * Chebyshev distance: max(|dx|, |dy|, |dz|)
     * Useful for box-shaped distance checks
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Distance")
    static float ChebyshevDistance(const FVector& A, const FVector& B);

    /**
     * Squared Euclidean distance: dx² + dy² + dz²
     * Faster than Euclidean (no sqrt), use for distance comparisons
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Distance")
    static float SquaredEuclideanDistance(const FVector& A, const FVector& B);

    /**
     * Check if distance is within threshold using specified formula
     *
     * @param A - First point
     * @param B - Second point
     * @param Threshold - Maximum distance
     * @param Formula - Distance formula to use
     * @return True if distance <= threshold
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Distance")
    static bool IsWithinDistance(
        const FVector& A,
        const FVector& B,
        float Threshold,
        EDistanceFormula Formula = EDistanceFormula::Euclidean);

    // ========================================================================
    // BOUNDING VOLUMES
    // ========================================================================

    /**
     * Calculate axis-aligned bounding box (AABB) from points
     *
     * @param Points - Array of points to bound
     * @return AABB bounding volume
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Bounds")
    static FBoundingVolumeResult CalculateAABB(const TArray<FVector>& Points);

    /**
     * Calculate bounding sphere from points (Ritter's algorithm)
     *
     * @param Points - Array of points to bound
     * @return Bounding sphere
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Bounds")
    static FBoundingVolumeResult CalculateBoundingSphere(const TArray<FVector>& Points);

    /**
     * Calculate minimum enclosing sphere (slower but tighter)
     *
     * @param Points - Array of points to bound
     * @return Minimum bounding sphere
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Bounds")
    static FBoundingVolumeResult CalculateMinimumBoundingSphere(const TArray<FVector>& Points);

    /**
     * Check if point is inside bounding volume
     *
     * @param Point - Point to test
     * @param Volume - Bounding volume to test against
     * @return True if point is inside volume
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Bounds")
    static bool IsPointInBoundingVolume(
        const FVector& Point,
        const FBoundingVolumeResult& Volume);

    /**
     * Check if two bounding volumes intersect
     *
     * @param A - First volume
     * @param B - Second volume
     * @return True if volumes intersect
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Bounds")
    static bool DoBoundingVolumesIntersect(
        const FBoundingVolumeResult& A,
        const FBoundingVolumeResult& B);

    /**
     * Expand bounding volume by margin
     *
     * @param Volume - Original volume
     * @param Margin - Amount to expand
     * @return Expanded volume
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Bounds")
    static FBoundingVolumeResult ExpandBoundingVolume(
        const FBoundingVolumeResult& Volume,
        float Margin);

    // ========================================================================
    // CLOSEST POINT QUERIES
    // ========================================================================

    /**
     * Find closest point on line segment to query point
     *
     * @param Point - Query point
     * @param SegmentStart - Start of line segment
     * @param SegmentEnd - End of line segment
     * @return Closest point result
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|ClosestPoint")
    static FClosestPointResult ClosestPointOnSegment(
        const FVector& Point,
        const FVector& SegmentStart,
        const FVector& SegmentEnd);

    /**
     * Find closest point on infinite line to query point
     *
     * @param Point - Query point
     * @param LineOrigin - Point on the line
     * @param LineDirection - Direction of the line (will be normalized)
     * @return Closest point result
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|ClosestPoint")
    static FClosestPointResult ClosestPointOnLine(
        const FVector& Point,
        const FVector& LineOrigin,
        const FVector& LineDirection);

    /**
     * Find closest point on box surface to query point
     *
     * @param Point - Query point
     * @param BoxCenter - Center of box
     * @param BoxExtents - Half-extents of box
     * @param BoxRotation - Box rotation
     * @return Closest point on box surface
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|ClosestPoint")
    static FClosestPointResult ClosestPointOnBox(
        const FVector& Point,
        const FVector& BoxCenter,
        const FVector& BoxExtents,
        const FQuat& BoxRotation);

    /**
     * Find closest point on sphere surface to query point
     *
     * @param Point - Query point
     * @param SphereCenter - Center of sphere
     * @param SphereRadius - Radius of sphere
     * @return Closest point on sphere surface
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|ClosestPoint")
    static FClosestPointResult ClosestPointOnSphere(
        const FVector& Point,
        const FVector& SphereCenter,
        float SphereRadius);

    /**
     * Find closest point on capsule surface to query point
     *
     * @param Point - Query point
     * @param CapsuleBase - Base of capsule (center of bottom hemisphere)
     * @param CapsuleTip - Tip of capsule (center of top hemisphere)
     * @param CapsuleRadius - Radius of capsule
     * @return Closest point on capsule surface
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|ClosestPoint")
    static FClosestPointResult ClosestPointOnCapsule(
        const FVector& Point,
        const FVector& CapsuleBase,
        const FVector& CapsuleTip,
        float CapsuleRadius);

    // ========================================================================
    // ANGLE UTILITIES
    // ========================================================================

    /**
     * Calculate signed angle between two vectors on specified plane
     *
     * @param From - Source direction
     * @param To - Target direction
     * @param Axis - Axis perpendicular to measurement plane (typically up vector)
     * @return Signed angle in degrees (-180 to 180)
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Angles")
    static float SignedAngleBetweenVectors(
        const FVector& From,
        const FVector& To,
        const FVector& Axis);

    /**
     * Calculate unsigned angle between two vectors
     *
     * @param A - First vector
     * @param B - Second vector
     * @return Angle in degrees (0 to 180)
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Angles")
    static float AngleBetweenVectors(const FVector& A, const FVector& B);

    /**
     * Normalize angle to range (-180, 180]
     *
     * @param Angle - Angle in degrees
     * @return Normalized angle
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Angles")
    static float NormalizeAngle180(float Angle);

    /**
     * Normalize angle to range [0, 360)
     *
     * @param Angle - Angle in degrees
     * @return Normalized angle
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Angles")
    static float NormalizeAngle360(float Angle);

    /**
     * Check if angle is within arc (handles wrap-around)
     *
     * @param Angle - Angle to check (degrees)
     * @param ArcCenter - Center of arc (degrees)
     * @param ArcHalfAngle - Half-width of arc (degrees)
     * @return True if angle is within arc
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Angles")
    static bool IsAngleInArc(float Angle, float ArcCenter, float ArcHalfAngle);

    // ========================================================================
    // PROJECTION UTILITIES
    // ========================================================================

    /**
     * Project point onto plane
     *
     * @param Point - Point to project
     * @param PlaneOrigin - Point on the plane
     * @param PlaneNormal - Normal of the plane (will be normalized)
     * @return Projected point on plane
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Projection")
    static FVector ProjectPointOntoPlane(
        const FVector& Point,
        const FVector& PlaneOrigin,
        const FVector& PlaneNormal);

    /**
     * Project vector onto another vector
     *
     * @param Vector - Vector to project
     * @param Target - Vector to project onto
     * @return Projected vector
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Projection")
    static FVector ProjectVectorOntoVector(
        const FVector& Vector,
        const FVector& Target);

    /**
     * Decompose vector into components parallel and perpendicular to direction
     *
     * @param Vector - Vector to decompose
     * @param Direction - Direction to decompose along (will be normalized)
     * @param OutParallel - Component parallel to direction
     * @param OutPerpendicular - Component perpendicular to direction
     */
    UFUNCTION(BlueprintCallable, Category = "Geometry|Projection")
    static void DecomposeVector(
        const FVector& Vector,
        const FVector& Direction,
        FVector& OutParallel,
        FVector& OutPerpendicular);

    // ========================================================================
    // INTERPOLATION
    // ========================================================================

    /**
     * Spherical linear interpolation between directions
     *
     * @param From - Starting direction (will be normalized)
     * @param To - Ending direction (will be normalized)
     * @param Alpha - Interpolation factor (0-1)
     * @return Interpolated direction
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Interpolation")
    static FVector SlerpDirection(
        const FVector& From,
        const FVector& To,
        float Alpha);

    /**
     * Smooth step interpolation (Hermite curve)
     *
     * @param Edge0 - Lower edge
     * @param Edge1 - Upper edge
     * @param X - Value to interpolate
     * @return Smoothly interpolated value
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Interpolation")
    static float SmoothStep(float Edge0, float Edge1, float X);

    /**
     * Smoother step interpolation (5th degree polynomial)
     *
     * @param Edge0 - Lower edge
     * @param Edge1 - Upper edge
     * @param X - Value to interpolate
     * @return Smoother interpolated value
     */
    UFUNCTION(BlueprintPure, Category = "Geometry|Interpolation")
    static float SmootherStep(float Edge0, float Edge1, float X);
};
