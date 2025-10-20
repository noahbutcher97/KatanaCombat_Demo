
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "TargetingComponent.generated.h"

class ACharacter;
class AActor;
class UMotionWarpingComponent;

/**
 * Handles directional cone-based targeting and motion warping setup
 * Reusable by player and AI for consistent targeting behavior
 * 
 * Key features:
 * - Cone-based directional targeting (120° default)
 * - Distance filtering and line of sight checks
 * - Motion warping setup for cinematic attacks
 * - Optional target locking for lock-on systems
 * - Configurable target class filtering
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class KATANACOMBAT_API UTargetingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTargetingComponent();

    // ============================================================================
    // CONFIGURATION
    // ============================================================================

    /** Angle of directional cone for targeting (degrees, half-angle each side) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    float DirectionalConeAngle = 60.0f;

    /** Maximum distance to search for targets */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    float MaxTargetDistance = 1000.0f;

    /** Require line of sight to target */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    bool bRequireLineOfSight = true;

    /** Trace channel for line of sight checks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    TEnumAsByte<ECollisionChannel> LineOfSightChannel = ECC_Visibility;

    /** Actor classes to consider as targets (empty = all actors) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    TArray<TSubclassOf<AActor>> TargetableClasses;

    /** Enable debug visualization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Debug")
    bool bDebugDraw = false;

    // ============================================================================
    // TARGETING - PRIMARY API
    // ============================================================================

    /**
     * Find nearest target in directional cone based on attack direction enum
     * @param Direction - Attack direction (None = use character forward)
     * @return Target actor, or nullptr if none found
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting")
    AActor* FindTarget(EAttackDirection Direction = EAttackDirection::None);

    /**
     * Find nearest target using a specific world space direction vector
     * @param DirectionVector - World space direction (normalized)
     * @return Target actor, or nullptr if none found
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting")
    AActor* FindTargetInDirection(const FVector& DirectionVector);

    /**
     * Get all potential targets in range (no direction filter)
     * @param OutTargets - Array to fill with found targets
     * @return Number of targets found
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting")
    int32 GetAllTargetsInRange(TArray<AActor*>& OutTargets);

    // ============================================================================
    // TARGETING - UTILITY QUERIES
    // ============================================================================

    /**
     * Check if target is in directional cone
     * @param Target - Actor to check
     * @param Direction - World space direction
     * @param AngleTolerance - Cone half-angle (uses component default if <= 0)
     * @return True if target is in cone
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting")
    bool IsTargetInCone(AActor* Target, const FVector& Direction, float AngleTolerance = -1.0f) const;

    /**
     * Check if has line of sight to target
     * @param Target - Actor to check
     * @return True if can see target
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting")
    bool HasLineOfSightTo(AActor* Target) const;

    /**
     * Convert attack direction enum to world space vector
     * @param Direction - Direction enum
     * @param bUseCamera - Use camera forward instead of actor forward for None direction
     * @return World space direction vector (normalized)
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting")
    FVector GetDirectionVector(EAttackDirection Direction, bool bUseCamera = false) const;

    /**
     * Get angle to target from facing direction
     * @param Target - Target actor
     * @return Angle in degrees (0 = directly ahead, positive = right, negative = left)
     */
    UFUNCTION(BlueprintPure, Category = "Targeting")
    float GetAngleToTarget(AActor* Target) const;

    /**
     * Get distance to target
     * @param Target - Target actor
     * @return Distance in units
     */
    UFUNCTION(BlueprintPure, Category = "Targeting")
    float GetDistanceToTarget(AActor* Target) const;

    // ============================================================================
    // CURRENT TARGET MANAGEMENT (for lock-on systems)
    // ============================================================================

    /** Get current locked target */
    UFUNCTION(BlueprintPure, Category = "Targeting")
    AActor* GetCurrentTarget() const { return CurrentTarget; }

    /** Set current target */
    UFUNCTION(BlueprintCallable, Category = "Targeting")
    void SetCurrentTarget(AActor* NewTarget);

    /** Clear current target */
    UFUNCTION(BlueprintCallable, Category = "Targeting")
    void ClearCurrentTarget();

    /** Is currently locked onto a target? */
    UFUNCTION(BlueprintPure, Category = "Targeting")
    bool HasTarget() const { return CurrentTarget != nullptr; }

    // ============================================================================
    // MOTION WARPING INTEGRATION
    // ============================================================================

    /**
     * Setup motion warping for attack toward target
     * @param Target - Target to warp toward
     * @param WarpTargetName - Name of warp target in animation (default: "AttackTarget")
     * @param MaxDistance - Maximum warp distance (uses AttackData config if <= 0)
     * @return True if warp was set up successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting|Motion Warping")
    bool SetupMotionWarp(AActor* Target, FName WarpTargetName = "AttackTarget", float MaxDistance = -1.0f);

    /**
     * Clear motion warp targets
     * @param WarpTargetName - Name of warp target to clear (NAME_None = all)
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting|Motion Warping")
    void ClearMotionWarp(FName WarpTargetName = NAME_None);

protected:
    virtual void BeginPlay() override;

private:
    // ============================================================================
    // STATE
    // ============================================================================

    /** Currently locked target (for lock-on systems) */
    UPROPERTY()
    TObjectPtr<AActor> CurrentTarget = nullptr;

    // ============================================================================
    // CACHED REFERENCES
    // ============================================================================

    UPROPERTY()
    TObjectPtr<ACharacter> OwnerCharacter;

    UPROPERTY()
    TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

    // ============================================================================
    // INTERNAL HELPERS - TARGET FINDING
    // ============================================================================

    /** Get all actors in sphere around owner */
    void GetActorsInRange(TArray<AActor*>& OutActors) const;

    /** Filter actors by targetable class */
    void FilterByTargetableClass(TArray<AActor*>& InOutActors) const;

    /** Filter actors by directional cone */
    void FilterByCone(TArray<AActor*>& InOutActors, const FVector& Direction) const;

    /** Filter actors by line of sight */
    void FilterByLineOfSight(TArray<AActor*>& InOutActors) const;

    /** Sort actors by distance (nearest first) */
    void SortByDistance(TArray<AActor*>& InOutActors) const;

    /**
     * Find best target using filtering pipeline
     * @param Direction - World space search direction
     * @return Best target, or nullptr
     */
    AActor* FindBestTarget(const FVector& Direction) const;

    // ============================================================================
    // INTERNAL HELPERS - MOTION WARPING
    // ============================================================================

    /** Calculate warp target location based on distance constraints */
    FVector CalculateWarpLocation(AActor* Target, float MaxDistance) const;

    // ============================================================================
    // DEBUG VISUALIZATION
    // ============================================================================

    /** Draw debug visualization for targeting */
    void DrawDebugTargeting(const TArray<AActor*>& PotentialTargets, AActor* SelectedTarget, const FVector& SearchDirection) const;
    EAttackDirection GetAttackDirectionFromInput(FVector InputDirection) const;
};