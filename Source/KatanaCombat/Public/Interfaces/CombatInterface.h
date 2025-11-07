// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatTypes.h"
#include "CombatInterface.generated.h"

class UAttackData;

// This class does not need to be modified.
UINTERFACE(MinimalAPI, Blueprintable)
class UCombatInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for actors that can perform attacks
 * Provides contract for combat actions and state queries
 */
class KATANACOMBAT_API ICombatInterface
{
	GENERATED_BODY()

public:
	/**
	 * Can this actor perform an attack right now?
	 * @return True if able to attack
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	bool CanPerformAttack() const;

	/**
	 * Get current combat state
	 * @return Current state
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	ECombatState GetCombatState() const;

	/**
	 * Is currently executing an attack?
	 * @return True if attacking
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	bool IsAttacking() const;

	/**
	 * Get the current attack being performed
	 * @return Current attack data, or nullptr if not attacking
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	UAttackData* GetCurrentAttack() const;

	/**
	 * Get current attack phase
	 * @return Current phase, or None if not attacking
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	EAttackPhase GetCurrentPhase() const;

	/**
 * Called when hit detection should be enabled (from AnimNotify_ToggleHitDetection)
 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	void OnEnableHitDetection();

	/**
	 * Called when hit detection should be disabled (from AnimNotify_ToggleHitDetection)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	void OnDisableHitDetection();


	/**
 * Called when an attack phase begins (from AnimNotifyState_AttackPhase)
 * @param Phase - Which phase is beginning
 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	void OnAttackPhaseBegin(EAttackPhase Phase);

	/**
	 * Called when an attack phase ends (from AnimNotifyState_AttackPhase)
	 * @param Phase - Which phase is ending
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	void OnAttackPhaseEnd(EAttackPhase Phase);

	/**
	 * Called when transitioning to a new attack phase (from AnimNotify_AttackPhaseTransition)
	 * NEW PHASE SYSTEM: Replaces NotifyState approach with single-event transitions
	 *
	 * Phases are contiguous and implicitly defined:
	 * - Windup: Montage start → Active transition
	 * - Active: Active transition → Recovery transition
	 * - Recovery: Recovery transition → Montage end
	 *
	 * @param NewPhase - The phase we're transitioning TO (Active or Recovery)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	void OnAttackPhaseTransition(EAttackPhase NewPhase);

	/**
	 * Is this actor currently in parry window? (Attacker-side state)
	 * Defender checks this on nearby attackers to determine if parry is possible
	 * @return True if this actor is vulnerable to being parried
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	bool IsInParryWindow() const;
};
