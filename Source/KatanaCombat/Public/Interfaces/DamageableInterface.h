// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatTypes.h"
#include "DamageableInterface.generated.h"

class UAttackData;

// This class does not need to be modified.
UINTERFACE(MinimalAPI, Blueprintable)
class UDamageableInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for actors that can receive damage and combat effects
 * Provides contract for damage application, finishers, parry reactions, etc.
 */
class KATANACOMBAT_API IDamageableInterface
{
    GENERATED_BODY()

public:
    /**
     * Apply damage to this actor
     * @param HitInfo - Complete information about the hit
     * @return Actual damage dealt (after resistances, etc.)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
    float ApplyDamage(const FHitReactionInfo& HitInfo);

    /**
     * DEPRECATED: Posture system removed. Use contextual stagger via HitReactionComponent::ApplyStagger() instead.
     * @param PostureDamage - Amount of posture to remove
     * @param Attacker - Who is attacking
     * @return Always returns false (posture system removed)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat", meta = (DeprecatedFunction, DeprecationMessage = "Posture system removed. Use HitReactionComponent::ApplyStagger() instead."))
    bool ApplyPostureDamage(float PostureDamage, AActor* Attacker);

    /**
     * Check if this actor can be damaged right now
     * @return True if vulnerable to damage
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
    bool CanBeDamaged() const;

    /**
     * Check if this actor is currently blocking
     * @return True if actively blocking
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
    bool IsBlocking() const;

    /**
     * DEPRECATED: Posture-based guard break removed. Use IsStaggered() instead.
     * @return True if staggered (forwards to IsStaggered)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat", meta = (DeprecatedFunction, DeprecationMessage = "Posture system removed. Use IsStaggered() instead."))
    bool IsGuardBroken() const;

    /**
     * Check if this actor is in a staggered state.
     * Stagger is a contextual vulnerability triggered by heavy hits, counters, or special moves.
     * Unlike the old posture system, stagger is event-driven (not a persistent bar).
     * @return True if currently staggered
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
    bool IsStaggered() const;

    /**
     * Execute a finisher on this actor after explicit low-health, stagger, or contextual eligibility.
     * Deprecated posture guard break is not a new-content eligibility source.
     * @param Attacker - Who is performing the finisher
     * @param FinisherData - Attack data for the finisher
     * @return True if finisher was successfully started
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
    bool ExecuteFinisher(AActor* Attacker, UAttackData* FinisherData);

    /**
     * React to a successful parry (this actor's attack was parried)
     * @param Parrier - Who parried the attack
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
    void OnAttackParried(AActor* Parrier);

    /**
     * Open counter window (after being parried or perfect evaded)
     * @param Duration - How long the counter window stays open
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
    void OpenCounterWindow(float Duration);

    /**
     * DEPRECATED: Posture system removed. Returns 0.
     * @return Always 0 (posture system removed)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat", meta = (DeprecatedFunction, DeprecationMessage = "Posture system removed."))
    float GetCurrentPosture() const;

    /**
     * DEPRECATED: Posture system removed. Returns 100.
     * @return Always 100 (posture system removed)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat", meta = (DeprecatedFunction, DeprecationMessage = "Posture system removed."))
    float GetMaxPosture() const;

    /**
     * Check if currently in a counter window (vulnerable to counter attacks)
     * @return True if in counter window
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
    bool IsInCounterWindow() const;

    // ========================================================================
    // HEALTH QUERIES (for targeting system)
    // ========================================================================

    /**
     * Get current health value
     * @return Current health
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Health")
    float GetCurrentHealth() const;

    /**
     * Get maximum health value
     * @return Max health
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Health")
    float GetMaxHealth() const;

    /**
     * Check if this actor is alive
     * @return True if health > 0
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Health")
    bool IsAlive() const;
};
