// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PhysicsIntegrationLibrary.generated.h"

/**
 * Trajectory sample point for physics prediction
 * Note: Named FPhysicsTrajectorySample to avoid conflict with engine's FTrajectorySample
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FPhysicsTrajectorySample
{
    GENERATED_BODY()

    /** World position at this sample */
    UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
    FVector Position = FVector::ZeroVector;

    /** Velocity at this sample */
    UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
    FVector Velocity = FVector::ZeroVector;

    /** Time from trajectory start */
    UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
    float Time = 0.0f;

    /** Did trajectory hit something at this point? */
    UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
    bool bIsCollision = false;
};

/**
 * Result of a trajectory prediction
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FTrajectoryPrediction
{
    GENERATED_BODY()

    /** All sample points along trajectory */
    UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
    TArray<FPhysicsTrajectorySample> Samples;

    /** Final position (may be collision point) */
    UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
    FVector EndPosition = FVector::ZeroVector;

    /** Total trajectory time */
    UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
    float TotalTime = 0.0f;

    /** Did trajectory collide with something? */
    UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
    bool bHitSomething = false;

    /** Hit result if collision occurred */
    UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
    FHitResult HitResult;

    /** Estimated landing position (for projectiles) */
    UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
    FVector LandingPosition = FVector::ZeroVector;
};

/**
 * Result of collision prediction between two moving objects
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FCollisionPrediction
{
    GENERATED_BODY()

    /** Will objects collide within prediction window? */
    UPROPERTY(BlueprintReadOnly, Category = "Collision")
    bool bWillCollide = false;

    /** Time until collision */
    UPROPERTY(BlueprintReadOnly, Category = "Collision")
    float TimeToCollision = 0.0f;

    /** Predicted collision point */
    UPROPERTY(BlueprintReadOnly, Category = "Collision")
    FVector CollisionPoint = FVector::ZeroVector;

    /** Position of object A at collision time */
    UPROPERTY(BlueprintReadOnly, Category = "Collision")
    FVector ObjectAPosition = FVector::ZeroVector;

    /** Position of object B at collision time */
    UPROPERTY(BlueprintReadOnly, Category = "Collision")
    FVector ObjectBPosition = FVector::ZeroVector;

    /** Relative velocity at collision */
    UPROPERTY(BlueprintReadOnly, Category = "Collision")
    FVector RelativeVelocity = FVector::ZeroVector;

    /** Confidence of prediction (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Collision")
    float Confidence = 0.0f;
};

/**
 * Intercept prediction for moving target
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FInterceptPrediction
{
    GENERATED_BODY()

    /** Can intercept be achieved? */
    UPROPERTY(BlueprintReadOnly, Category = "Intercept")
    bool bCanIntercept = false;

    /** Point to aim at for intercept */
    UPROPERTY(BlueprintReadOnly, Category = "Intercept")
    FVector InterceptPoint = FVector::ZeroVector;

    /** Direction to travel for intercept */
    UPROPERTY(BlueprintReadOnly, Category = "Intercept")
    FVector InterceptDirection = FVector::ZeroVector;

    /** Time until intercept */
    UPROPERTY(BlueprintReadOnly, Category = "Intercept")
    float TimeToIntercept = 0.0f;

    /** Distance to travel */
    UPROPERTY(BlueprintReadOnly, Category = "Intercept")
    float InterceptDistance = 0.0f;
};

/**
 * Physics Integration Utility Library
 *
 * Static utility functions for physics-based predictions:
 * - Trajectory prediction (ballistic, linear)
 * - Collision prediction between moving objects
 * - Intercept calculation for moving targets
 * - Simple physics simulation (Verlet integration)
 * - Ground prediction for falling characters
 *
 * Design: Foundation for paired animation positioning,
 * attack prediction, and AI threat assessment.
 */
UCLASS()
class KATANACOMBAT_API UPhysicsIntegrationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ========================================================================
    // TRAJECTORY PREDICTION
    // ========================================================================

    /**
     * Predict ballistic trajectory (affected by gravity)
     *
     * @param World - World context
     * @param StartPosition - Initial position
     * @param InitialVelocity - Initial velocity
     * @param GravityScale - Multiplier for world gravity (1.0 = normal)
     * @param MaxTime - Maximum prediction time
     * @param SampleInterval - Time between samples
     * @param CollisionRadius - Radius for collision detection (0 = no collision check)
     * @param IgnoreActors - Actors to ignore in collision
     * @return Trajectory prediction with samples
     */
    UFUNCTION(BlueprintCallable, Category = "Physics|Trajectory", meta = (WorldContext = "WorldContextObject"))
    static FTrajectoryPrediction PredictBallisticTrajectory(
        UObject* WorldContextObject,
        const FVector& StartPosition,
        const FVector& InitialVelocity,
        float GravityScale,
        float MaxTime,
        float SampleInterval,
        float CollisionRadius,
        const TArray<AActor*>& IgnoreActors);

    /**
     * Predict linear trajectory (no gravity)
     *
     * @param World - World context
     * @param StartPosition - Initial position
     * @param Velocity - Constant velocity
     * @param MaxDistance - Maximum prediction distance
     * @param CollisionRadius - Radius for collision detection
     * @param IgnoreActors - Actors to ignore
     * @return Trajectory prediction
     */
    UFUNCTION(BlueprintCallable, Category = "Physics|Trajectory", meta = (WorldContext = "WorldContextObject"))
    static FTrajectoryPrediction PredictLinearTrajectory(
        UObject* WorldContextObject,
        const FVector& StartPosition,
        const FVector& Velocity,
        float MaxDistance,
        float CollisionRadius,
        const TArray<AActor*>& IgnoreActors);

    /**
     * Predict where a falling character will land
     *
     * @param World - World context
     * @param StartPosition - Current position
     * @param CurrentVelocity - Current velocity
     * @param CapsuleRadius - Character capsule radius
     * @param CapsuleHalfHeight - Character capsule half-height
     * @param MaxFallTime - Maximum prediction time
     * @param IgnoreActor - Character actor to ignore
     * @return Landing position or current position if no landing found
     */
    UFUNCTION(BlueprintCallable, Category = "Physics|Trajectory", meta = (WorldContext = "WorldContextObject"))
    static FVector PredictLandingPosition(
        UObject* WorldContextObject,
        const FVector& StartPosition,
        const FVector& CurrentVelocity,
        float CapsuleRadius,
        float CapsuleHalfHeight,
        float MaxFallTime = 5.0f,
        AActor* IgnoreActor = nullptr);

    // ========================================================================
    // COLLISION PREDICTION
    // ========================================================================

    /**
     * Predict if two moving spheres will collide
     *
     * @param PosA - Position of sphere A
     * @param VelA - Velocity of sphere A
     * @param RadiusA - Radius of sphere A
     * @param PosB - Position of sphere B
     * @param VelB - Velocity of sphere B
     * @param RadiusB - Radius of sphere B
     * @param MaxTime - Maximum prediction window
     * @return Collision prediction
     */
    UFUNCTION(BlueprintPure, Category = "Physics|Collision")
    static FCollisionPrediction PredictSphereCollision(
        const FVector& PosA,
        const FVector& VelA,
        float RadiusA,
        const FVector& PosB,
        const FVector& VelB,
        float RadiusB,
        float MaxTime = 2.0f);

    /**
     * Predict if a moving sphere will hit a static sphere
     *
     * @param MovingPos - Position of moving sphere
     * @param MovingVel - Velocity of moving sphere
     * @param MovingRadius - Radius of moving sphere
     * @param StaticPos - Position of static sphere
     * @param StaticRadius - Radius of static sphere
     * @param MaxTime - Maximum prediction window
     * @return Collision prediction
     */
    UFUNCTION(BlueprintPure, Category = "Physics|Collision")
    static FCollisionPrediction PredictSphereToStaticCollision(
        const FVector& MovingPos,
        const FVector& MovingVel,
        float MovingRadius,
        const FVector& StaticPos,
        float StaticRadius,
        float MaxTime = 2.0f);

    /**
     * Predict closest approach between two moving objects
     *
     * @param PosA - Position of object A
     * @param VelA - Velocity of object A
     * @param PosB - Position of object B
     * @param VelB - Velocity of object B
     * @param OutTime - Time of closest approach
     * @param OutDistanceSq - Squared distance at closest approach
     * @return True if closest approach is in the future
     */
    UFUNCTION(BlueprintPure, Category = "Physics|Collision")
    static bool PredictClosestApproach(
        const FVector& PosA,
        const FVector& VelA,
        const FVector& PosB,
        const FVector& VelB,
        float& OutTime,
        float& OutDistanceSq);

    // ========================================================================
    // INTERCEPT CALCULATION
    // ========================================================================

    /**
     * Calculate intercept point for a moving target
     * Useful for leading shots or movement prediction
     *
     * @param ShooterPosition - Position of the shooter/chaser
     * @param ProjectileSpeed - Speed of the projectile/chaser
     * @param TargetPosition - Current target position
     * @param TargetVelocity - Target velocity
     * @param MaxInterceptTime - Maximum time to consider
     * @return Intercept prediction
     */
    UFUNCTION(BlueprintPure, Category = "Physics|Intercept")
    static FInterceptPrediction CalculateIntercept(
        const FVector& ShooterPosition,
        float ProjectileSpeed,
        const FVector& TargetPosition,
        const FVector& TargetVelocity,
        float MaxInterceptTime = 5.0f);

    /**
     * Calculate where to aim to hit a moving target with a projectile
     *
     * @param ShooterPosition - Shooter position
     * @param ProjectileSpeed - Projectile speed
     * @param TargetPosition - Target current position
     * @param TargetVelocity - Target velocity
     * @param OutAimDirection - Direction to aim
     * @return True if a valid aim direction was found
     */
    UFUNCTION(BlueprintPure, Category = "Physics|Intercept")
    static bool CalculateLeadAim(
        const FVector& ShooterPosition,
        float ProjectileSpeed,
        const FVector& TargetPosition,
        const FVector& TargetVelocity,
        FVector& OutAimDirection);

    /**
     * Predict future position of an object moving at constant velocity
     *
     * @param CurrentPosition - Current position
     * @param Velocity - Constant velocity
     * @param TimeAhead - Time to predict ahead
     * @return Predicted position
     */
    UFUNCTION(BlueprintPure, Category = "Physics|Intercept")
    static FVector PredictFuturePosition(
        const FVector& CurrentPosition,
        const FVector& Velocity,
        float TimeAhead);

    // ========================================================================
    // SIMPLE PHYSICS SIMULATION
    // ========================================================================

    /**
     * Single step of Verlet integration
     * Useful for simple physics without full simulation
     *
     * @param CurrentPosition - Current position
     * @param PreviousPosition - Previous position
     * @param Acceleration - Current acceleration
     * @param DeltaTime - Time step
     * @return New position
     */
    UFUNCTION(BlueprintPure, Category = "Physics|Simulation")
    static FVector VerletIntegrationStep(
        const FVector& CurrentPosition,
        const FVector& PreviousPosition,
        const FVector& Acceleration,
        float DeltaTime);

    /**
     * Calculate velocity from Verlet positions
     *
     * @param CurrentPosition - Current position
     * @param PreviousPosition - Previous position
     * @param DeltaTime - Time between positions
     * @return Estimated velocity
     */
    UFUNCTION(BlueprintPure, Category = "Physics|Simulation")
    static FVector VerletVelocity(
        const FVector& CurrentPosition,
        const FVector& PreviousPosition,
        float DeltaTime);

    /**
     * Apply drag to velocity
     *
     * @param Velocity - Current velocity
     * @param DragCoefficient - Drag amount (0-1 per second)
     * @param DeltaTime - Time step
     * @return Velocity after drag
     */
    UFUNCTION(BlueprintPure, Category = "Physics|Simulation")
    static FVector ApplyDrag(
        const FVector& Velocity,
        float DragCoefficient,
        float DeltaTime);

    /**
     * Reflect velocity off a surface
     *
     * @param Velocity - Incoming velocity
     * @param SurfaceNormal - Surface normal
     * @param Bounciness - Coefficient of restitution (0 = no bounce, 1 = perfect bounce)
     * @return Reflected velocity
     */
    UFUNCTION(BlueprintPure, Category = "Physics|Simulation")
    static FVector ReflectVelocity(
        const FVector& Velocity,
        const FVector& SurfaceNormal,
        float Bounciness = 0.5f);

    // ========================================================================
    // GROUND/TERRAIN
    // ========================================================================

    /**
     * Find ground position below a point
     *
     * @param World - World context
     * @param Position - Starting position
     * @param MaxDistance - Maximum downward trace distance
     * @param CapsuleRadius - Optional capsule radius for sweep
     * @param IgnoreActor - Actor to ignore
     * @return Ground position or original position if no ground found
     */
    UFUNCTION(BlueprintCallable, Category = "Physics|Ground", meta = (WorldContext = "WorldContextObject"))
    static FVector FindGroundPosition(
        UObject* WorldContextObject,
        const FVector& Position,
        float MaxDistance = 500.0f,
        float CapsuleRadius = 0.0f,
        AActor* IgnoreActor = nullptr);

    /**
     * Get ground normal at position
     *
     * @param World - World context
     * @param Position - Position to check
     * @param MaxDistance - Maximum trace distance
     * @param IgnoreActor - Actor to ignore
     * @return Ground normal or up vector if no ground
     */
    UFUNCTION(BlueprintCallable, Category = "Physics|Ground", meta = (WorldContext = "WorldContextObject"))
    static FVector GetGroundNormal(
        UObject* WorldContextObject,
        const FVector& Position,
        float MaxDistance = 500.0f,
        AActor* IgnoreActor = nullptr);

    /**
     * Calculate slope angle at position
     *
     * @param World - World context
     * @param Position - Position to check
     * @param MaxDistance - Maximum trace distance
     * @param IgnoreActor - Actor to ignore
     * @return Slope angle in degrees (0 = flat)
     */
    UFUNCTION(BlueprintCallable, Category = "Physics|Ground", meta = (WorldContext = "WorldContextObject"))
    static float GetSlopeAngle(
        UObject* WorldContextObject,
        const FVector& Position,
        float MaxDistance = 500.0f,
        AActor* IgnoreActor = nullptr);
};
