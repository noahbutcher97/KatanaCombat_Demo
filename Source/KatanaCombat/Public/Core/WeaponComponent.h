// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponComponent.generated.h"

class UAttackData;
class ACharacter;
class USkeletalMeshComponent;

/**
 * Handles weapon-based hit detection via socket tracing
 * Tracks which actors have been hit to prevent multiple hits per attack
 * 
 * Usage:
 * 1. Add component to character
 * 2. Create sockets on weapon mesh (weapon_start at base, weapon_end at tip)
 * 3. AnimNotify_ToggleHitDetection enables/disables during Active phase
 * 4. Bind to OnWeaponHit event to process hits
 * 
 * Hit Detection Flow:
 * - EnableHitDetection() called by AnimNotify at start of Active phase
 * - Every tick: Swept sphere trace from weapon_start to weapon_end
 * - Hit actors tracked to prevent double-hitting
 * - DisableHitDetection() called by AnimNotify at end of Active phase
 * - ResetHitActors() called at start of new attack
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class KATANACOMBAT_API UWeaponComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWeaponComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ============================================================================
    // CONFIGURATION
    // ============================================================================

    /** Socket name for weapon start (usually weapon base/handle) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FName WeaponStartSocket = "weapon_start";

    /** Socket name for weapon end (usually weapon tip) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FName WeaponEndSocket = "weapon_end";

    /** Trace radius for swept sphere (adjust based on weapon size) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float TraceRadius = 5.0f;

    /** Trace channel for hit detection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

    /** Enable debug visualization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Debug")
    bool bDebugDraw = false;

    /** Debug draw duration (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Debug")
    float DebugDrawDuration = 2.0f;

    // ============================================================================
    // HIT DETECTION CONTROL
    // ============================================================================

    /**
     * Enable hit detection (called by AnimNotify_ToggleHitDetection)
     * Begins swept sphere tracing on tick
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void EnableHitDetection();

    /**
     * Disable hit detection (called by AnimNotify_ToggleHitDetection)
     * Stops tracing and preserves hit actor list for current attack
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void DisableHitDetection();

    /**
     * Is hit detection currently enabled?
     * @return True if actively tracing for hits
     */
    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool IsHitDetectionEnabled() const { return bHitDetectionEnabled; }

    /**
     * Reset hit actors list (called at start of new attack)
     * Allows previously hit actors to be hit again by new attack
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void ResetHitActors();

    // ============================================================================
    // SOCKET CONFIGURATION
    // ============================================================================

    /**
     * Set weapon sockets for tracing
     * @param StartSocket - Socket at weapon base
     * @param EndSocket - Socket at weapon tip
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SetWeaponSockets(FName StartSocket, FName EndSocket);

    /**
     * Get weapon socket location in world space
     * @param SocketName - Socket to query
     * @return Socket location in world space, or character location if socket not found
     */
    UFUNCTION(BlueprintPure, Category = "Weapon")
    FVector GetSocketLocation(FName SocketName) const;

    // ============================================================================
    // HIT QUERIES
    // ============================================================================

    /**
     * Check if actor was already hit by current attack
     * @param Actor - Actor to check
     * @return True if actor has been hit
     */
    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool WasActorAlreadyHit(AActor* Actor) const;

    /**
     * Get list of all actors hit by current attack
     * @return Array of hit actors
     */
    UFUNCTION(BlueprintPure, Category = "Weapon")
    TArray<AActor*> GetHitActors() const { return HitActors; }

    /**
     * Get count of actors hit by current attack
     * @return Number of unique actors hit
     */
    UFUNCTION(BlueprintPure, Category = "Weapon")
    int32 GetHitActorCount() const { return HitActors.Num(); }

    // ============================================================================
    // HIT EVENTS
    // ============================================================================

    /**
     * Event broadcast when weapon hits something
     * Listeners can process hit, apply damage, spawn VFX, etc.
     */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeaponHit, AActor*, HitActor, const FHitResult&, HitResult, UAttackData*, AttackData);

    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnWeaponHit OnWeaponHit;

protected:
    virtual void BeginPlay() override;

private:
    // ============================================================================
    // STATE
    // ============================================================================

    /** Is hit detection currently active? */
    UPROPERTY()
    bool bHitDetectionEnabled = false;

    /** Actors already hit by current attack (prevents double-hitting) */
    UPROPERTY()
    TArray<TObjectPtr<AActor>> HitActors;

    /** Previous frame's weapon tip location (for swept trace) */
    FVector PreviousTipLocation;

    /** Previous frame's weapon start location (for swept trace) */
    FVector PreviousStartLocation;

    /** Is this the first trace since hit detection enabled? */
    bool bFirstTrace = true;

    // ============================================================================
    // CACHED REFERENCES
    // ============================================================================

    /** Owner character (cached for performance) */
    UPROPERTY()
    TObjectPtr<ACharacter> OwnerCharacter;

    /** Owner's skeletal mesh component (where sockets are) */
    UPROPERTY()
    TObjectPtr<USkeletalMeshComponent> OwnerMesh;

    // ============================================================================
    // INTERNAL HELPERS
    // ============================================================================

    /**
     * Perform swept sphere trace from weapon start to end
     * Called every tick when hit detection is enabled
     */
    void PerformWeaponTrace();

    /**
     * Process a hit result
     * Checks if actor was already hit, adds to list, broadcasts event
     * @param Hit - Hit result from trace
     */
    void ProcessHit(const FHitResult& Hit);

    /**
     * Add actor to hit list
     * @param Actor - Actor to add
     */
    void AddHitActor(AActor* Actor);

    /**
     * Get current attack data from combat component
     * @return Current attack data, or nullptr if not attacking
     */
    UAttackData* GetCurrentAttackData() const;

    /**
     * Draw debug visualization for trace
     * @param Start - Trace start location
     * @param End - Trace end location
     * @param bHit - Did trace hit something?
     * @param Hit - Hit result (if bHit is true)
     */
    void DrawDebugTrace(const FVector& Start, const FVector& End, bool bHit, const FHitResult& Hit) const;
};