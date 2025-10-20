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
     * Apply posture damage when this actor blocks an attack
     * @param PostureDamage - Amount of posture to remove
     * @param Attacker - Who is attacking
     * @return True if guard was broken (posture reached 0)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
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
     * Check if this actor is in a guard broken state
     * @return True if posture depleted
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
    bool IsGuardBroken() const;

    /**
     * Execute a finisher on this actor (must be guard broken or stunned)
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
     * Get current posture value (0-100, 0 = guard broken)
     * @return Current posture
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
    float GetCurrentPosture() const;

    /**
     * Get maximum posture value
     * @return Max posture
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
    float GetMaxPosture() const;

    /**
     * Check if currently in a counter window (vulnerable to counter attacks)
     * @return True if in counter window
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
    bool IsInCounterWindow() const;
};
