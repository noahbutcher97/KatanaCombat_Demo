
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "Data/PairedAnimationTypes.h"
#include "TargetingComponent.generated.h"

class ACharacter;
class AActor;
class UMotionWarpingComponent;
class UCombatSettings;
class UTargetingSettings;

/**
 * Handles directional cone-based targeting and motion warping setup
 * Reusable by player and AI for consistent targeting behavior
 *
 * Settings Hierarchy:
 * 1. TargetingSettingsOverride (if set on this component)
 * 2. CombatSettings->TargetingSettings (character's default)
 * 3. Hardcoded fallback (should never be reached)
 *
 * Key features:
 * - Cone-based directional targeting
 * - Soft aim assist with angle/distance scoring
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
    // SETTINGS
    // ============================================================================

    /**
     * Optional per-instance settings override
     * If set, uses this instead of CombatSettings->TargetingSettings
     * Useful for special characters that need different targeting behavior
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    TObjectPtr<UTargetingSettings> TargetingSettingsOverride;

    /**
     * Actor classes to consider as targets (empty = all actors implementing IDamageableInterface)
     * This is per-component since different components might target different things
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    TArray<TSubclassOf<AActor>> TargetableClasses;

    /**
     * Get the effective targeting settings (override or from CombatSettings)
     * @return TargetingSettings to use, or nullptr if none available
     */
    UFUNCTION(BlueprintPure, Category = "Settings")
    UTargetingSettings* GetEffectiveSettings() const;

    // Debug visualization controlled via CVars:
    // Combat.Debug.Targeting 1 - Enable targeting visualization
    // Combat.Debug.DrawDuration 2.0 - Set debug draw duration

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
    // COUNTER LOCK (Stickiness after parry/counter)
    // ============================================================================

    /**
     * Lock targeting to a specific enemy after initiating a counter.
     * Counter lock has priority over normal targeting - prevents target switching
     * until the parry/counter chain completes or is explicitly released.
     *
     * Chain Mode: Called when player parries, remains until finisher or disengage
     * AC3 Mode: Called briefly during counter-kill execution
     *
     * @param Target - Enemy to lock onto
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting|Counter")
    void LockToCounterTarget(AActor* Target);

    /**
     * Release counter lock, allowing normal target switching.
     * Called when counter chain completes, is interrupted, or player disengages.
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting|Counter")
    void ReleaseCounterLock();

    /** Check if counter lock is active */
    UFUNCTION(BlueprintPure, Category = "Targeting|Counter")
    bool IsCounterLocked() const { return bIsCounterLocked; }

    /** Get the counter-locked target (if any) */
    UFUNCTION(BlueprintPure, Category = "Targeting|Counter")
    AActor* GetCounterLockedTarget() const { return CounterLockedTarget.Get(); }

    /**
     * Get effective target - returns counter locked target if active, otherwise current target.
     * Use this instead of GetCurrentTarget() when executing attacks.
     */
    UFUNCTION(BlueprintPure, Category = "Targeting|Counter")
    AActor* GetEffectiveTarget() const { return bIsCounterLocked ? CounterLockedTarget.Get() : CurrentTarget.Get(); }

    // ============================================================================
    // SOFT AIM ASSIST (Directional Attack Targeting)
    // ============================================================================

    /**
     * Find best target in given direction using soft aim assist scoring
     * Uses gradient angle threshold and distance weighting from CombatSettings
     *
     * @param InputDirection - World space direction to search
     * @param MaxRange - Maximum search range (uses CombatSettings default if <= 0)
     * @param GradientAngle - Enemies within this angle are candidates (uses CombatSettings if <= 0)
     * @param OppositeAngle - Enemies beyond this angle are ignored (uses CombatSettings if <= 0)
     * @param AngleWeight - Weight for angle alignment in scoring (uses CombatSettings if < 0)
     * @param DistanceWeight - Weight for distance in scoring (uses CombatSettings if < 0)
     * @param OutBestTarget - Output: best target actor (nullptr if none)
     * @return Rotation to face best target, or rotation toward InputDirection if no target
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting|Soft Aim Assist")
    FRotator FindBestTargetForDirection(
        const FVector& InputDirection,
        AActor*& OutBestTarget,
        float MaxRange = -1.0f,
        float GradientAngle = -1.0f,
        float OppositeAngle = -1.0f,
        float AngleWeight = -1.0f,
        float DistanceWeight = -1.0f);

    /**
     * Find nearest valid target within facing cone
     * Used as fallback when no movement input is provided
     *
     * @param MaxRange - Maximum search range (uses CombatSettings default if <= 0)
     * @param FacingConeAngle - Only consider targets within this angle of forward facing (degrees, 180 = any direction)
     * @return Nearest valid target within cone, or nullptr if none
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting|Soft Aim Assist")
    AActor* FindNearestTarget(float MaxRange = -1.0f, float FacingConeAngle = 180.0f);

    // ============================================================================
    // MOTION WARPING INTEGRATION
    // ============================================================================

    /**
     * Setup attack warp based on context (unified function)
     *
     * Determines warp type automatically:
     * - If Target is valid: Uses TargetWarpName with translation+rotation toward target
     * - If Target is null: Uses RotationWarpName with rotation-only toward TargetRotation
     *
     * Animation Setup Required:
     * - Add AnimNotifyState_MotionWarping with name matching Config.TargetWarpName (bWarpTranslation=true)
     * - Add AnimNotifyState_MotionWarping with name matching Config.RotationWarpName (bWarpTranslation=false)
     *
     * @param Target - Target actor (null = rotation-only warp)
     * @param TargetRotation - Rotation to face (used when Target is null)
     * @param Config - Warp configuration from AttackData
     * @return True if warp was set up successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting|Motion Warping")
    bool SetupAttackWarp(AActor* Target, const FRotator& TargetRotation, const struct FAttackWarpConfig& Config);

    /**
     * Clear motion warp targets
     * @param WarpTargetName - Name of warp target to clear (NAME_None = all)
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting|Motion Warping")
    void ClearMotionWarp(FName WarpTargetName = NAME_None);

    // ========================================================================
    // VICTIM WARP (for paired animations - victim warps to attacker)
    // ========================================================================

    /**
     * Set up continuous warp tracking as the ATTACKER of a paired animation.
     * Attacker warps toward victim's position during finisher/counter execution.
     * Symmetric with SetupVictimWarp - both characters track each other.
     *
     * Call this when initiating a paired animation where this character is the attacker.
     * Automatically registers victim as paired partner for collision ignore.
     *
     * Unlike standard SetupAttackWarp (which uses FAttackWarpConfig for regular attacks),
     * this uses FPairedWarpConfig for consistency with paired animation infrastructure.
     *
     * @param Victim - Target of the paired animation (we warp toward their position)
     * @param Config - Paired warp configuration (terrain adjustment, max distance, etc.)
     * @return True if warp tracking was set up successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting|Paired Animation")
    bool SetupAttackerPairedWarp(AActor* Victim, const FPairedWarpConfig& Config);

    /**
     * Clear attacker paired warp tracking.
     * Call when paired animation ends or is interrupted.
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting|Paired Animation")
    void ClearAttackerPairedWarp();

    /**
     * Check if this component is currently tracking as a paired animation attacker.
     */
    UFUNCTION(BlueprintPure, Category = "Targeting|Paired Animation")
    bool IsTrackingAsAttacker() const { return bIsTrackingAsAttacker; }

    /**
     * Set up continuous warp tracking as the VICTIM of a paired animation.
     * Victim warps to maintain position relative to attacker's ACTUAL location.
     * Mirror of SetupAttackerPairedWarp - attacker tracks victim, victim tracks attacker.
     *
     * Call this when initiating a paired animation where this character is the victim.
     * Automatically registers attacker as paired partner for collision ignore.
     *
     * @param Attacker - Actor performing the attack (we warp relative to their position)
     * @param Config - Warp configuration (offset, terrain adjustment, etc.)
     * @return True if warp tracking was set up successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting|Paired Animation")
    bool SetupVictimWarp(AActor* Attacker, const FPairedWarpConfig& Config);

    /**
     * Clear victim warp tracking.
     * Call when paired animation ends or is interrupted.
     */
    UFUNCTION(BlueprintCallable, Category = "Targeting|Paired Animation")
    void ClearVictimWarp();

    /**
     * Check if this component is currently tracking as a paired animation victim.
     */
    UFUNCTION(BlueprintPure, Category = "Targeting|Paired Animation")
    bool IsTrackingAsVictim() const { return bIsTrackingAsVictim; }

    // Legacy functions - kept for backwards compatibility, prefer SetupAttackWarp
    UFUNCTION(BlueprintCallable, Category = "Targeting|Motion Warping", meta = (DeprecatedFunction, DeprecationMessage = "Use SetupAttackWarp instead"))
    bool SetupMotionWarp(AActor* Target, FName WarpTargetName = "AttackTarget", float MaxDistance = -1.0f);

    UFUNCTION(BlueprintCallable, Category = "Targeting|Motion Warping", meta = (DeprecatedFunction, DeprecationMessage = "Use SetupAttackWarp instead"))
    bool SetupDirectionalWarp(const FVector& InputDirection, const struct FAttackWarpConfig& Config);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // ============================================================================
    // STATE
    // ============================================================================

    /** Currently locked target (for lock-on systems) */
    UPROPERTY()
    TObjectPtr<AActor> CurrentTarget = nullptr;

    // ============================================================================
    // COUNTER LOCK STATE
    // ============================================================================

    /** Target locked via counter (weak to handle destruction during counter chain) */
    TWeakObjectPtr<AActor> CounterLockedTarget;

    /** Whether counter lock is active (takes priority over normal targeting) */
    bool bIsCounterLocked = false;

    // ============================================================================
    // CONTINUOUS WARP TRACKING STATE (ATTACKER MODE)
    // ============================================================================

    /** Target being tracked for continuous warp updates (weak to handle destruction) */
    TWeakObjectPtr<AActor> TrackedWarpTarget;

    /** Active warp configuration for continuous updates */
    FAttackWarpConfig ActiveWarpConfig;

    /** Whether we're actively tracking a target for warp updates */
    bool bIsTrackingWarpTarget = false;

    /** Callback for continuous warp updates - called each frame by MotionWarpingComponent */
    UFUNCTION()
    void OnMotionWarpingPreUpdate(UMotionWarpingComponent* MotionWarpingComp);

    /** Stop tracking and unbind from updates */
    void StopWarpTracking();

    // ============================================================================
    // ATTACKER PAIRED WARP TRACKING STATE (PAIRED ANIMATION ATTACKER MODE)
    // ============================================================================

    /** Victim being tracked for attacker paired warp updates (we warp toward them) */
    TWeakObjectPtr<AActor> TrackedVictim;

    /** Attacker paired warp configuration for continuous updates */
    FPairedWarpConfig AttackerPairedWarpConfig;

    /** Whether we're actively tracking as a paired animation attacker */
    bool bIsTrackingAsAttacker = false;

    /** Callback for attacker paired warp updates - called each frame by MotionWarpingComponent */
    UFUNCTION()
    void OnAttackerPairedWarpPreUpdate(UMotionWarpingComponent* MotionWarpingComp);

    /** Stop attacker paired tracking and unbind from updates */
    void StopAttackerPairedWarpTracking();

    // ============================================================================
    // VICTIM WARP TRACKING STATE (PAIRED ANIMATION VICTIM MODE)
    // ============================================================================

    /** Attacker being tracked for victim warp updates (we position relative to them) */
    TWeakObjectPtr<AActor> TrackedAttacker;

    /** Victim warp configuration for continuous updates */
    FPairedWarpConfig VictimWarpConfig;

    /** Whether we're actively tracking as a paired animation victim */
    bool bIsTrackingAsVictim = false;

    /** Callback for victim warp updates - called each frame by MotionWarpingComponent */
    UFUNCTION()
    void OnVictimMotionWarpingPreUpdate(UMotionWarpingComponent* MotionWarpingComp);

    /** Stop victim tracking and unbind from updates */
    void StopVictimWarpTracking();

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