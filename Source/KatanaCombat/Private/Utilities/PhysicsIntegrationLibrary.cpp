// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/PhysicsIntegrationLibrary.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/WorldSettings.h"

// ============================================================================
// TRAJECTORY PREDICTION
// ============================================================================

FTrajectoryPrediction UPhysicsIntegrationLibrary::PredictBallisticTrajectory(
    UObject* WorldContextObject,
    const FVector& StartPosition,
    const FVector& InitialVelocity,
    float GravityScale,
    float MaxTime,
    float SampleInterval,
    float CollisionRadius,
    const TArray<AActor*>& IgnoreActors)
{
    FTrajectoryPrediction Prediction;

    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World)
    {
        Prediction.EndPosition = StartPosition;
        return Prediction;
    }

    // Get world gravity
    float GravityZ = World->GetGravityZ() * GravityScale;
    FVector Gravity(0.0f, 0.0f, GravityZ);

    FVector CurrentPos = StartPosition;
    FVector CurrentVel = InitialVelocity;
    float CurrentTime = 0.0f;

    // Collision params
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActors(IgnoreActors);

    while (CurrentTime < MaxTime)
    {
        // Store sample
        FPhysicsTrajectorySample Sample;
        Sample.Position = CurrentPos;
        Sample.Velocity = CurrentVel;
        Sample.Time = CurrentTime;
        Prediction.Samples.Add(Sample);

        // Next position using kinematic equation
        FVector NextPos = CurrentPos + CurrentVel * SampleInterval + 0.5f * Gravity * SampleInterval * SampleInterval;
        FVector NextVel = CurrentVel + Gravity * SampleInterval;

        // Collision check
        if (CollisionRadius > 0.0f)
        {
            FHitResult Hit;
            if (World->SweepSingleByChannel(Hit, CurrentPos, NextPos, FQuat::Identity,
                ECC_WorldStatic, FCollisionShape::MakeSphere(CollisionRadius), QueryParams))
            {
                Prediction.bHitSomething = true;
                Prediction.HitResult = Hit;
                Prediction.EndPosition = Hit.Location;
                Prediction.LandingPosition = Hit.Location;

                // Add final collision sample
                FPhysicsTrajectorySample CollisionSample;
                CollisionSample.Position = Hit.Location;
                CollisionSample.Velocity = CurrentVel;
                CollisionSample.Time = CurrentTime + SampleInterval * Hit.Time;
                CollisionSample.bIsCollision = true;
                Prediction.Samples.Add(CollisionSample);

                Prediction.TotalTime = CollisionSample.Time;
                return Prediction;
            }
        }

        CurrentPos = NextPos;
        CurrentVel = NextVel;
        CurrentTime += SampleInterval;

        // Check if below ground level (simple landing check)
        if (CurrentPos.Z < StartPosition.Z - 10000.0f)
        {
            break;
        }
    }

    Prediction.EndPosition = CurrentPos;
    Prediction.TotalTime = CurrentTime;
    Prediction.LandingPosition = CurrentPos;

    return Prediction;
}

FTrajectoryPrediction UPhysicsIntegrationLibrary::PredictLinearTrajectory(
    UObject* WorldContextObject,
    const FVector& StartPosition,
    const FVector& Velocity,
    float MaxDistance,
    float CollisionRadius,
    const TArray<AActor*>& IgnoreActors)
{
    FTrajectoryPrediction Prediction;

    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World || Velocity.IsNearlyZero())
    {
        Prediction.EndPosition = StartPosition;
        return Prediction;
    }

    FVector Direction = Velocity.GetSafeNormal();
    float Speed = Velocity.Size();
    FVector EndPosition = StartPosition + Direction * MaxDistance;

    // Initial sample
    FPhysicsTrajectorySample StartSample;
    StartSample.Position = StartPosition;
    StartSample.Velocity = Velocity;
    StartSample.Time = 0.0f;
    Prediction.Samples.Add(StartSample);

    // Collision check
    if (CollisionRadius > 0.0f)
    {
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActors(IgnoreActors);

        FHitResult Hit;
        if (World->SweepSingleByChannel(Hit, StartPosition, EndPosition, FQuat::Identity,
            ECC_WorldStatic, FCollisionShape::MakeSphere(CollisionRadius), QueryParams))
        {
            Prediction.bHitSomething = true;
            Prediction.HitResult = Hit;
            Prediction.EndPosition = Hit.Location;

            float HitDistance = FVector::Dist(StartPosition, Hit.Location);
            Prediction.TotalTime = HitDistance / Speed;

            FPhysicsTrajectorySample HitSample;
            HitSample.Position = Hit.Location;
            HitSample.Velocity = Velocity;
            HitSample.Time = Prediction.TotalTime;
            HitSample.bIsCollision = true;
            Prediction.Samples.Add(HitSample);

            return Prediction;
        }
    }

    Prediction.EndPosition = EndPosition;
    Prediction.TotalTime = MaxDistance / Speed;

    FPhysicsTrajectorySample EndSample;
    EndSample.Position = EndPosition;
    EndSample.Velocity = Velocity;
    EndSample.Time = Prediction.TotalTime;
    Prediction.Samples.Add(EndSample);

    return Prediction;
}

FVector UPhysicsIntegrationLibrary::PredictLandingPosition(
    UObject* WorldContextObject,
    const FVector& StartPosition,
    const FVector& CurrentVelocity,
    float CapsuleRadius,
    float CapsuleHalfHeight,
    float MaxFallTime,
    AActor* IgnoreActor)
{
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World)
    {
        return StartPosition;
    }

    float GravityZ = World->GetGravityZ();
    FVector Gravity(0.0f, 0.0f, GravityZ);

    FVector Pos = StartPosition;
    FVector Vel = CurrentVelocity;
    float DeltaTime = 0.05f;
    float TotalTime = 0.0f;

    FCollisionQueryParams QueryParams;
    if (IgnoreActor)
    {
        QueryParams.AddIgnoredActor(IgnoreActor);
    }

    FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);

    while (TotalTime < MaxFallTime)
    {
        FVector NextPos = Pos + Vel * DeltaTime + 0.5f * Gravity * DeltaTime * DeltaTime;
        FVector NextVel = Vel + Gravity * DeltaTime;

        FHitResult Hit;
        if (World->SweepSingleByChannel(Hit, Pos, NextPos, FQuat::Identity,
            ECC_WorldStatic, CapsuleShape, QueryParams))
        {
            return Hit.Location;
        }

        Pos = NextPos;
        Vel = NextVel;
        TotalTime += DeltaTime;

        // Safety: if we've fallen very far, give up
        if (Pos.Z < StartPosition.Z - 50000.0f)
        {
            break;
        }
    }

    return Pos;
}

// ============================================================================
// COLLISION PREDICTION
// ============================================================================

FCollisionPrediction UPhysicsIntegrationLibrary::PredictSphereCollision(
    const FVector& PosA,
    const FVector& VelA,
    float RadiusA,
    const FVector& PosB,
    const FVector& VelB,
    float RadiusB,
    float MaxTime)
{
    FCollisionPrediction Result;

    // Relative position and velocity
    FVector RelPos = PosB - PosA;
    FVector RelVel = VelB - VelA;
    float CombinedRadius = RadiusA + RadiusB;

    // If relative velocity is zero, objects maintain constant distance
    float RelVelSq = RelVel.SizeSquared();
    if (RelVelSq < SMALL_NUMBER)
    {
        Result.bWillCollide = RelPos.Size() <= CombinedRadius;
        if (Result.bWillCollide)
        {
            Result.TimeToCollision = 0.0f;
            Result.CollisionPoint = (PosA + PosB) * 0.5f;
            Result.ObjectAPosition = PosA;
            Result.ObjectBPosition = PosB;
            Result.Confidence = 1.0f;
        }
        return Result;
    }

    // Solve quadratic: |RelPos + t*RelVel|^2 = CombinedRadius^2
    // a*t^2 + b*t + c = 0
    float a = RelVelSq;
    float b = 2.0f * FVector::DotProduct(RelPos, RelVel);
    float c = RelPos.SizeSquared() - CombinedRadius * CombinedRadius;

    float Discriminant = b * b - 4.0f * a * c;

    if (Discriminant < 0.0f)
    {
        // No collision
        return Result;
    }

    float SqrtDisc = FMath::Sqrt(Discriminant);
    float t1 = (-b - SqrtDisc) / (2.0f * a);
    float t2 = (-b + SqrtDisc) / (2.0f * a);

    // Find earliest positive collision time
    float CollisionTime = -1.0f;
    if (t1 >= 0.0f && t1 <= MaxTime)
    {
        CollisionTime = t1;
    }
    else if (t2 >= 0.0f && t2 <= MaxTime)
    {
        CollisionTime = t2;
    }

    if (CollisionTime < 0.0f)
    {
        return Result;
    }

    Result.bWillCollide = true;
    Result.TimeToCollision = CollisionTime;
    Result.ObjectAPosition = PosA + VelA * CollisionTime;
    Result.ObjectBPosition = PosB + VelB * CollisionTime;
    Result.CollisionPoint = (Result.ObjectAPosition + Result.ObjectBPosition) * 0.5f;
    Result.RelativeVelocity = RelVel;

    // Confidence based on how soon collision occurs
    Result.Confidence = 1.0f - (CollisionTime / MaxTime);

    return Result;
}

FCollisionPrediction UPhysicsIntegrationLibrary::PredictSphereToStaticCollision(
    const FVector& MovingPos,
    const FVector& MovingVel,
    float MovingRadius,
    const FVector& StaticPos,
    float StaticRadius,
    float MaxTime)
{
    return PredictSphereCollision(MovingPos, MovingVel, MovingRadius, StaticPos, FVector::ZeroVector, StaticRadius, MaxTime);
}

bool UPhysicsIntegrationLibrary::PredictClosestApproach(
    const FVector& PosA,
    const FVector& VelA,
    const FVector& PosB,
    const FVector& VelB,
    float& OutTime,
    float& OutDistanceSq)
{
    FVector RelPos = PosB - PosA;
    FVector RelVel = VelB - VelA;

    float RelVelSq = RelVel.SizeSquared();
    if (RelVelSq < SMALL_NUMBER)
    {
        // No relative motion - closest approach is now
        OutTime = 0.0f;
        OutDistanceSq = RelPos.SizeSquared();
        return true;
    }

    // Time of closest approach: t = -dot(RelPos, RelVel) / |RelVel|^2
    OutTime = -FVector::DotProduct(RelPos, RelVel) / RelVelSq;

    if (OutTime < 0.0f)
    {
        // Closest approach was in the past
        OutTime = 0.0f;
        OutDistanceSq = RelPos.SizeSquared();
        return false;
    }

    // Distance at closest approach
    FVector ClosestRelPos = RelPos + RelVel * OutTime;
    OutDistanceSq = ClosestRelPos.SizeSquared();

    return true;
}

// ============================================================================
// INTERCEPT CALCULATION
// ============================================================================

FInterceptPrediction UPhysicsIntegrationLibrary::CalculateIntercept(
    const FVector& ShooterPosition,
    float ProjectileSpeed,
    const FVector& TargetPosition,
    const FVector& TargetVelocity,
    float MaxInterceptTime)
{
    FInterceptPrediction Result;

    if (ProjectileSpeed <= 0.0f)
    {
        return Result;
    }

    FVector ToTarget = TargetPosition - ShooterPosition;
    float Distance = ToTarget.Size();

    // If target is stationary, simple case
    if (TargetVelocity.IsNearlyZero())
    {
        Result.bCanIntercept = true;
        Result.InterceptPoint = TargetPosition;
        Result.InterceptDirection = ToTarget.GetSafeNormal();
        Result.TimeToIntercept = Distance / ProjectileSpeed;
        Result.InterceptDistance = Distance;
        return Result;
    }

    // Solve quadratic for time to intercept
    // |TargetPos + TargetVel*t - ShooterPos|^2 = (ProjectileSpeed*t)^2
    float a = TargetVelocity.SizeSquared() - ProjectileSpeed * ProjectileSpeed;
    float b = 2.0f * FVector::DotProduct(ToTarget, TargetVelocity);
    float c = ToTarget.SizeSquared();

    // Handle special case where speeds are equal
    if (FMath::Abs(a) < SMALL_NUMBER)
    {
        if (FMath::Abs(b) < SMALL_NUMBER)
        {
            // Can only intercept if already at target
            Result.bCanIntercept = c < SMALL_NUMBER;
            return Result;
        }

        float t = -c / b;
        if (t > 0.0f && t <= MaxInterceptTime)
        {
            Result.bCanIntercept = true;
            Result.TimeToIntercept = t;
            Result.InterceptPoint = TargetPosition + TargetVelocity * t;
            Result.InterceptDirection = (Result.InterceptPoint - ShooterPosition).GetSafeNormal();
            Result.InterceptDistance = FVector::Dist(ShooterPosition, Result.InterceptPoint);
        }
        return Result;
    }

    float Discriminant = b * b - 4.0f * a * c;
    if (Discriminant < 0.0f)
    {
        return Result;
    }

    float SqrtDisc = FMath::Sqrt(Discriminant);
    float t1 = (-b - SqrtDisc) / (2.0f * a);
    float t2 = (-b + SqrtDisc) / (2.0f * a);

    // Find smallest positive time
    float InterceptTime = -1.0f;
    if (t1 > 0.0f && (t1 < t2 || t2 <= 0.0f))
    {
        InterceptTime = t1;
    }
    else if (t2 > 0.0f)
    {
        InterceptTime = t2;
    }

    if (InterceptTime > 0.0f && InterceptTime <= MaxInterceptTime)
    {
        Result.bCanIntercept = true;
        Result.TimeToIntercept = InterceptTime;
        Result.InterceptPoint = TargetPosition + TargetVelocity * InterceptTime;
        Result.InterceptDirection = (Result.InterceptPoint - ShooterPosition).GetSafeNormal();
        Result.InterceptDistance = FVector::Dist(ShooterPosition, Result.InterceptPoint);
    }

    return Result;
}

bool UPhysicsIntegrationLibrary::CalculateLeadAim(
    const FVector& ShooterPosition,
    float ProjectileSpeed,
    const FVector& TargetPosition,
    const FVector& TargetVelocity,
    FVector& OutAimDirection)
{
    FInterceptPrediction Intercept = CalculateIntercept(ShooterPosition, ProjectileSpeed, TargetPosition, TargetVelocity);

    if (Intercept.bCanIntercept)
    {
        OutAimDirection = Intercept.InterceptDirection;
        return true;
    }

    // Fall back to aiming directly at target
    OutAimDirection = (TargetPosition - ShooterPosition).GetSafeNormal();
    return false;
}

FVector UPhysicsIntegrationLibrary::PredictFuturePosition(
    const FVector& CurrentPosition,
    const FVector& Velocity,
    float TimeAhead)
{
    return CurrentPosition + Velocity * TimeAhead;
}

// ============================================================================
// SIMPLE PHYSICS SIMULATION
// ============================================================================

FVector UPhysicsIntegrationLibrary::VerletIntegrationStep(
    const FVector& CurrentPosition,
    const FVector& PreviousPosition,
    const FVector& Acceleration,
    float DeltaTime)
{
    // Verlet: NextPos = 2*CurrentPos - PrevPos + Acc*dt^2
    return 2.0f * CurrentPosition - PreviousPosition + Acceleration * DeltaTime * DeltaTime;
}

FVector UPhysicsIntegrationLibrary::VerletVelocity(
    const FVector& CurrentPosition,
    const FVector& PreviousPosition,
    float DeltaTime)
{
    if (DeltaTime > SMALL_NUMBER)
    {
        return (CurrentPosition - PreviousPosition) / DeltaTime;
    }
    return FVector::ZeroVector;
}

FVector UPhysicsIntegrationLibrary::ApplyDrag(
    const FVector& Velocity,
    float DragCoefficient,
    float DeltaTime)
{
    // Exponential drag: V_new = V_old * e^(-drag*dt)
    // Approximation for small dt: V_new = V_old * (1 - drag*dt)
    float DragFactor = FMath::Clamp(1.0f - DragCoefficient * DeltaTime, 0.0f, 1.0f);
    return Velocity * DragFactor;
}

FVector UPhysicsIntegrationLibrary::ReflectVelocity(
    const FVector& Velocity,
    const FVector& SurfaceNormal,
    float Bounciness)
{
    FVector Normal = SurfaceNormal.GetSafeNormal();
    FVector Reflected = Velocity - 2.0f * FVector::DotProduct(Velocity, Normal) * Normal;
    return Reflected * Bounciness;
}

// ============================================================================
// GROUND/TERRAIN
// ============================================================================

FVector UPhysicsIntegrationLibrary::FindGroundPosition(
    UObject* WorldContextObject,
    const FVector& Position,
    float MaxDistance,
    float CapsuleRadius,
    AActor* IgnoreActor)
{
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World)
    {
        return Position;
    }

    FCollisionQueryParams QueryParams;
    if (IgnoreActor)
    {
        QueryParams.AddIgnoredActor(IgnoreActor);
    }

    FVector Start = Position;
    FVector End = Position - FVector(0.0f, 0.0f, MaxDistance);

    FHitResult Hit;
    bool bHit;

    if (CapsuleRadius > 0.0f)
    {
        bHit = World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
            ECC_WorldStatic, FCollisionShape::MakeSphere(CapsuleRadius), QueryParams);
    }
    else
    {
        bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, QueryParams);
    }

    if (bHit)
    {
        return Hit.Location;
    }

    return Position;
}

FVector UPhysicsIntegrationLibrary::GetGroundNormal(
    UObject* WorldContextObject,
    const FVector& Position,
    float MaxDistance,
    AActor* IgnoreActor)
{
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World)
    {
        return FVector::UpVector;
    }

    FCollisionQueryParams QueryParams;
    if (IgnoreActor)
    {
        QueryParams.AddIgnoredActor(IgnoreActor);
    }

    FVector Start = Position;
    FVector End = Position - FVector(0.0f, 0.0f, MaxDistance);

    FHitResult Hit;
    if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, QueryParams))
    {
        return Hit.Normal;
    }

    return FVector::UpVector;
}

float UPhysicsIntegrationLibrary::GetSlopeAngle(
    UObject* WorldContextObject,
    const FVector& Position,
    float MaxDistance,
    AActor* IgnoreActor)
{
    FVector Normal = GetGroundNormal(WorldContextObject, Position, MaxDistance, IgnoreActor);
    float DotWithUp = FVector::DotProduct(Normal, FVector::UpVector);
    return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotWithUp, -1.0f, 1.0f)));
}
