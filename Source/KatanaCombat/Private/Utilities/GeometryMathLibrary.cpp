// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/GeometryMathLibrary.h"

// ============================================================================
// DISTANCE CALCULATIONS
// ============================================================================

float UGeometryMathLibrary::CalculateDistance(const FVector& A, const FVector& B, EDistanceFormula Formula)
{
    switch (Formula)
    {
    case EDistanceFormula::Euclidean:
        return EuclideanDistance(A, B);
    case EDistanceFormula::Euclidean2D:
        return EuclideanDistance2D(A, B);
    case EDistanceFormula::Manhattan:
        return ManhattanDistance(A, B);
    case EDistanceFormula::Chebyshev:
        return ChebyshevDistance(A, B);
    case EDistanceFormula::SquaredEuclidean:
        return SquaredEuclideanDistance(A, B);
    default:
        return EuclideanDistance(A, B);
    }
}

float UGeometryMathLibrary::EuclideanDistance(const FVector& A, const FVector& B)
{
    return FVector::Dist(A, B);
}

float UGeometryMathLibrary::EuclideanDistance2D(const FVector& A, const FVector& B)
{
    return FVector::Dist2D(A, B);
}

float UGeometryMathLibrary::ManhattanDistance(const FVector& A, const FVector& B)
{
    const FVector Delta = A - B;
    return FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) + FMath::Abs(Delta.Z);
}

float UGeometryMathLibrary::ChebyshevDistance(const FVector& A, const FVector& B)
{
    const FVector Delta = A - B;
    return FMath::Max3(FMath::Abs(Delta.X), FMath::Abs(Delta.Y), FMath::Abs(Delta.Z));
}

float UGeometryMathLibrary::SquaredEuclideanDistance(const FVector& A, const FVector& B)
{
    return FVector::DistSquared(A, B);
}

bool UGeometryMathLibrary::IsWithinDistance(const FVector& A, const FVector& B, float Threshold, EDistanceFormula Formula)
{
    // For squared euclidean, compare against squared threshold
    if (Formula == EDistanceFormula::SquaredEuclidean)
    {
        return SquaredEuclideanDistance(A, B) <= (Threshold * Threshold);
    }

    return CalculateDistance(A, B, Formula) <= Threshold;
}

// ============================================================================
// BOUNDING VOLUMES
// ============================================================================

FBoundingVolumeResult UGeometryMathLibrary::CalculateAABB(const TArray<FVector>& Points)
{
    FBoundingVolumeResult Result;
    Result.Type = EBoundingVolumeType::AABB;

    if (Points.Num() == 0)
    {
        return Result;
    }

    FVector Min = Points[0];
    FVector Max = Points[0];

    for (const FVector& Point : Points)
    {
        Min.X = FMath::Min(Min.X, Point.X);
        Min.Y = FMath::Min(Min.Y, Point.Y);
        Min.Z = FMath::Min(Min.Z, Point.Z);

        Max.X = FMath::Max(Max.X, Point.X);
        Max.Y = FMath::Max(Max.Y, Point.Y);
        Max.Z = FMath::Max(Max.Z, Point.Z);
    }

    Result.Center = (Min + Max) * 0.5f;
    Result.Extents = (Max - Min) * 0.5f;
    Result.Radius = Result.Extents.Size();

    return Result;
}

FBoundingVolumeResult UGeometryMathLibrary::CalculateBoundingSphere(const TArray<FVector>& Points)
{
    FBoundingVolumeResult Result;
    Result.Type = EBoundingVolumeType::Sphere;

    if (Points.Num() == 0)
    {
        return Result;
    }

    if (Points.Num() == 1)
    {
        Result.Center = Points[0];
        Result.Radius = 0.0f;
        return Result;
    }

    // Ritter's algorithm - fast but not optimal
    // Step 1: Find two most distant points along each axis
    int32 MinX = 0, MaxX = 0, MinY = 0, MaxY = 0, MinZ = 0, MaxZ = 0;

    for (int32 i = 1; i < Points.Num(); ++i)
    {
        if (Points[i].X < Points[MinX].X) MinX = i;
        if (Points[i].X > Points[MaxX].X) MaxX = i;
        if (Points[i].Y < Points[MinY].Y) MinY = i;
        if (Points[i].Y > Points[MaxY].Y) MaxY = i;
        if (Points[i].Z < Points[MinZ].Z) MinZ = i;
        if (Points[i].Z > Points[MaxZ].Z) MaxZ = i;
    }

    // Find the pair with maximum span
    float SpanX = FVector::DistSquared(Points[MinX], Points[MaxX]);
    float SpanY = FVector::DistSquared(Points[MinY], Points[MaxY]);
    float SpanZ = FVector::DistSquared(Points[MinZ], Points[MaxZ]);

    int32 P1, P2;
    if (SpanX >= SpanY && SpanX >= SpanZ)
    {
        P1 = MinX;
        P2 = MaxX;
    }
    else if (SpanY >= SpanX && SpanY >= SpanZ)
    {
        P1 = MinY;
        P2 = MaxY;
    }
    else
    {
        P1 = MinZ;
        P2 = MaxZ;
    }

    // Initial sphere from most distant pair
    Result.Center = (Points[P1] + Points[P2]) * 0.5f;
    Result.Radius = FVector::Dist(Points[P1], Result.Center);

    // Step 2: Grow sphere to include all points
    for (const FVector& Point : Points)
    {
        float Dist = FVector::Dist(Point, Result.Center);
        if (Dist > Result.Radius)
        {
            // Point is outside - grow sphere
            float NewRadius = (Result.Radius + Dist) * 0.5f;
            float k = (NewRadius - Result.Radius) / Dist;
            Result.Center += (Point - Result.Center) * k;
            Result.Radius = NewRadius;
        }
    }

    return Result;
}

FBoundingVolumeResult UGeometryMathLibrary::CalculateMinimumBoundingSphere(const TArray<FVector>& Points)
{
    // For now, use Ritter's algorithm + one pass refinement
    // A true minimum bounding sphere requires Welzl's algorithm which is more complex
    FBoundingVolumeResult Result = CalculateBoundingSphere(Points);

    // Refinement pass - shrink toward centroid if possible
    if (Points.Num() > 3)
    {
        // Calculate centroid
        FVector Centroid = FVector::ZeroVector;
        for (const FVector& Point : Points)
        {
            Centroid += Point;
        }
        Centroid /= Points.Num();

        // Find max distance from centroid
        float MaxDist = 0.0f;
        for (const FVector& Point : Points)
        {
            MaxDist = FMath::Max(MaxDist, FVector::Dist(Point, Centroid));
        }

        // If centroid-based sphere is smaller, use it
        if (MaxDist < Result.Radius)
        {
            Result.Center = Centroid;
            Result.Radius = MaxDist;
        }
    }

    return Result;
}

bool UGeometryMathLibrary::IsPointInBoundingVolume(const FVector& Point, const FBoundingVolumeResult& Volume)
{
    switch (Volume.Type)
    {
    case EBoundingVolumeType::AABB:
        {
            FVector LocalPoint = Point - Volume.Center;
            return FMath::Abs(LocalPoint.X) <= Volume.Extents.X &&
                   FMath::Abs(LocalPoint.Y) <= Volume.Extents.Y &&
                   FMath::Abs(LocalPoint.Z) <= Volume.Extents.Z;
        }

    case EBoundingVolumeType::OBB:
        {
            // Transform point to OBB local space
            FVector LocalPoint = Volume.Rotation.UnrotateVector(Point - Volume.Center);
            return FMath::Abs(LocalPoint.X) <= Volume.Extents.X &&
                   FMath::Abs(LocalPoint.Y) <= Volume.Extents.Y &&
                   FMath::Abs(LocalPoint.Z) <= Volume.Extents.Z;
        }

    case EBoundingVolumeType::Sphere:
        return FVector::DistSquared(Point, Volume.Center) <= (Volume.Radius * Volume.Radius);

    default:
        return false;
    }
}

bool UGeometryMathLibrary::DoBoundingVolumesIntersect(const FBoundingVolumeResult& A, const FBoundingVolumeResult& B)
{
    // Simplified sphere-sphere check for all types
    // Uses bounding spheres of the volumes
    float RadiusA = (A.Type == EBoundingVolumeType::Sphere) ? A.Radius : A.Extents.Size();
    float RadiusB = (B.Type == EBoundingVolumeType::Sphere) ? B.Radius : B.Extents.Size();

    float CombinedRadius = RadiusA + RadiusB;
    return FVector::DistSquared(A.Center, B.Center) <= (CombinedRadius * CombinedRadius);
}

FBoundingVolumeResult UGeometryMathLibrary::ExpandBoundingVolume(const FBoundingVolumeResult& Volume, float Margin)
{
    FBoundingVolumeResult Result = Volume;
    Result.Extents += FVector(Margin);
    Result.Radius += Margin;
    return Result;
}

// ============================================================================
// CLOSEST POINT QUERIES
// ============================================================================

FClosestPointResult UGeometryMathLibrary::ClosestPointOnSegment(const FVector& Point, const FVector& SegmentStart, const FVector& SegmentEnd)
{
    FClosestPointResult Result;
    Result.bValid = true;

    const FVector Segment = SegmentEnd - SegmentStart;
    const float SegmentLengthSq = Segment.SizeSquared();

    if (SegmentLengthSq < SMALL_NUMBER)
    {
        // Degenerate segment (point)
        Result.ClosestPoint = SegmentStart;
        Result.Parameter = 0.0f;
        Result.Distance = FVector::Dist(Point, SegmentStart);
        return Result;
    }

    // Calculate parameter t for closest point: t = dot(Point - Start, Segment) / |Segment|^2
    Result.Parameter = FMath::Clamp(FVector::DotProduct(Point - SegmentStart, Segment) / SegmentLengthSq, 0.0f, 1.0f);
    Result.ClosestPoint = SegmentStart + Segment * Result.Parameter;
    Result.Distance = FVector::Dist(Point, Result.ClosestPoint);

    return Result;
}

FClosestPointResult UGeometryMathLibrary::ClosestPointOnLine(const FVector& Point, const FVector& LineOrigin, const FVector& LineDirection)
{
    FClosestPointResult Result;
    Result.bValid = true;

    const FVector NormalizedDir = LineDirection.GetSafeNormal();
    if (NormalizedDir.IsNearlyZero())
    {
        Result.ClosestPoint = LineOrigin;
        Result.Parameter = 0.0f;
        Result.Distance = FVector::Dist(Point, LineOrigin);
        return Result;
    }

    Result.Parameter = FVector::DotProduct(Point - LineOrigin, NormalizedDir);
    Result.ClosestPoint = LineOrigin + NormalizedDir * Result.Parameter;
    Result.Distance = FVector::Dist(Point, Result.ClosestPoint);

    return Result;
}

FClosestPointResult UGeometryMathLibrary::ClosestPointOnBox(const FVector& Point, const FVector& BoxCenter, const FVector& BoxExtents, const FQuat& BoxRotation)
{
    FClosestPointResult Result;
    Result.bValid = true;

    // Transform point to box local space
    FVector LocalPoint = BoxRotation.UnrotateVector(Point - BoxCenter);

    // Clamp to box extents
    FVector ClampedLocal;
    ClampedLocal.X = FMath::Clamp(LocalPoint.X, -BoxExtents.X, BoxExtents.X);
    ClampedLocal.Y = FMath::Clamp(LocalPoint.Y, -BoxExtents.Y, BoxExtents.Y);
    ClampedLocal.Z = FMath::Clamp(LocalPoint.Z, -BoxExtents.Z, BoxExtents.Z);

    // Transform back to world space
    Result.ClosestPoint = BoxCenter + BoxRotation.RotateVector(ClampedLocal);
    Result.Distance = FVector::Dist(Point, Result.ClosestPoint);

    return Result;
}

FClosestPointResult UGeometryMathLibrary::ClosestPointOnSphere(const FVector& Point, const FVector& SphereCenter, float SphereRadius)
{
    FClosestPointResult Result;
    Result.bValid = true;

    FVector ToPoint = Point - SphereCenter;
    float Distance = ToPoint.Size();

    if (Distance < SMALL_NUMBER)
    {
        // Point at center - return any point on surface
        Result.ClosestPoint = SphereCenter + FVector(SphereRadius, 0.0f, 0.0f);
        Result.Distance = SphereRadius;
    }
    else
    {
        Result.ClosestPoint = SphereCenter + ToPoint * (SphereRadius / Distance);
        Result.Distance = FMath::Abs(Distance - SphereRadius);
    }

    return Result;
}

FClosestPointResult UGeometryMathLibrary::ClosestPointOnCapsule(const FVector& Point, const FVector& CapsuleBase, const FVector& CapsuleTip, float CapsuleRadius)
{
    FClosestPointResult Result;
    Result.bValid = true;

    // Find closest point on capsule axis
    FClosestPointResult AxisResult = ClosestPointOnSegment(Point, CapsuleBase, CapsuleTip);

    // Then closest point on sphere centered at that point
    return ClosestPointOnSphere(Point, AxisResult.ClosestPoint, CapsuleRadius);
}

// ============================================================================
// ANGLE UTILITIES
// ============================================================================

float UGeometryMathLibrary::SignedAngleBetweenVectors(const FVector& From, const FVector& To, const FVector& Axis)
{
    FVector FromNorm = From.GetSafeNormal();
    FVector ToNorm = To.GetSafeNormal();
    FVector AxisNorm = Axis.GetSafeNormal();

    if (FromNorm.IsNearlyZero() || ToNorm.IsNearlyZero() || AxisNorm.IsNearlyZero())
    {
        return 0.0f;
    }

    float Dot = FVector::DotProduct(FromNorm, ToNorm);
    float Angle = FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f));

    // Determine sign using cross product
    FVector Cross = FVector::CrossProduct(FromNorm, ToNorm);
    if (FVector::DotProduct(Cross, AxisNorm) < 0.0f)
    {
        Angle = -Angle;
    }

    return FMath::RadiansToDegrees(Angle);
}

float UGeometryMathLibrary::AngleBetweenVectors(const FVector& A, const FVector& B)
{
    FVector ANorm = A.GetSafeNormal();
    FVector BNorm = B.GetSafeNormal();

    if (ANorm.IsNearlyZero() || BNorm.IsNearlyZero())
    {
        return 0.0f;
    }

    float Dot = FVector::DotProduct(ANorm, BNorm);
    return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
}

float UGeometryMathLibrary::NormalizeAngle180(float Angle)
{
    // Normalize to (-180, 180]
    while (Angle > 180.0f) Angle -= 360.0f;
    while (Angle <= -180.0f) Angle += 360.0f;
    return Angle;
}

float UGeometryMathLibrary::NormalizeAngle360(float Angle)
{
    // Normalize to [0, 360)
    while (Angle >= 360.0f) Angle -= 360.0f;
    while (Angle < 0.0f) Angle += 360.0f;
    return Angle;
}

bool UGeometryMathLibrary::IsAngleInArc(float Angle, float ArcCenter, float ArcHalfAngle)
{
    float Delta = NormalizeAngle180(Angle - ArcCenter);
    return FMath::Abs(Delta) <= ArcHalfAngle;
}

// ============================================================================
// PROJECTION UTILITIES
// ============================================================================

FVector UGeometryMathLibrary::ProjectPointOntoPlane(const FVector& Point, const FVector& PlaneOrigin, const FVector& PlaneNormal)
{
    FVector Normal = PlaneNormal.GetSafeNormal();
    if (Normal.IsNearlyZero())
    {
        return Point;
    }

    float Distance = FVector::DotProduct(Point - PlaneOrigin, Normal);
    return Point - Normal * Distance;
}

FVector UGeometryMathLibrary::ProjectVectorOntoVector(const FVector& Vector, const FVector& Target)
{
    return Vector.ProjectOnTo(Target);
}

void UGeometryMathLibrary::DecomposeVector(const FVector& Vector, const FVector& Direction, FVector& OutParallel, FVector& OutPerpendicular)
{
    FVector DirNorm = Direction.GetSafeNormal();
    if (DirNorm.IsNearlyZero())
    {
        OutParallel = FVector::ZeroVector;
        OutPerpendicular = Vector;
        return;
    }

    OutParallel = DirNorm * FVector::DotProduct(Vector, DirNorm);
    OutPerpendicular = Vector - OutParallel;
}

// ============================================================================
// INTERPOLATION
// ============================================================================

FVector UGeometryMathLibrary::SlerpDirection(const FVector& From, const FVector& To, float Alpha)
{
    FVector FromNorm = From.GetSafeNormal();
    FVector ToNorm = To.GetSafeNormal();

    if (FromNorm.IsNearlyZero() || ToNorm.IsNearlyZero())
    {
        return FMath::Lerp(From, To, Alpha).GetSafeNormal();
    }

    // Use quaternion slerp for directions
    FQuat FromQuat = FQuat::FindBetweenNormals(FVector::ForwardVector, FromNorm);
    FQuat ToQuat = FQuat::FindBetweenNormals(FVector::ForwardVector, ToNorm);
    FQuat ResultQuat = FQuat::Slerp(FromQuat, ToQuat, Alpha);

    return ResultQuat.RotateVector(FVector::ForwardVector);
}

float UGeometryMathLibrary::SmoothStep(float Edge0, float Edge1, float X)
{
    // Clamp and normalize x
    float t = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);
    // Hermite interpolation: 3t^2 - 2t^3
    return t * t * (3.0f - 2.0f * t);
}

float UGeometryMathLibrary::SmootherStep(float Edge0, float Edge1, float X)
{
    // Clamp and normalize x
    float t = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);
    // 5th degree polynomial: 6t^5 - 15t^4 + 10t^3
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}
