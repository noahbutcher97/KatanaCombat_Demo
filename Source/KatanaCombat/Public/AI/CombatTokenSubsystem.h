// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CombatTokenSubsystem.generated.h"

/**
 * Combat Token Subsystem
 *
 * Manages "attack permits" to coordinate enemy attacks on the player.
 * Only enemies holding a token can initiate attacks, preventing spam
 * and creating readable, counterable attack patterns (AC3/Arkham style).
 *
 * Design:
 * - Limited pool of concurrent attack tokens (default: 2)
 * - Enemies request tokens before attacking
 * - Token holders execute attacks with counter windows
 * - Tokens released after attack completes or enemy is interrupted
 * - Queue system for waiting enemies (FIFO)
 *
 * Usage:
 * - Enemy AI calls RequestAttackToken() before attacking
 * - If granted, enemy executes attack montage with AnimNotifyState_CounterWindow
 * - On attack completion/interruption, call ReleaseAttackToken()
 * - Waiting enemies automatically notified when tokens become available
 */
UCLASS()
class KATANACOMBAT_API UCombatTokenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Maximum number of enemies that can attack simultaneously */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Tokens")
	int32 MaxConcurrentAttackers = 2;

	/** Minimum time between granting tokens to the same enemy (prevents rapid re-requests) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Tokens")
	float TokenCooldownPerEnemy = 1.0f;

	// ============================================================================
	// TOKEN API
	// ============================================================================

	/**
	 * Request an attack token for an enemy
	 * @param Requester The enemy actor requesting permission to attack
	 * @return True if token granted immediately, false if queued or denied
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Tokens")
	bool RequestAttackToken(AActor* Requester);

	/**
	 * Release an attack token held by an enemy
	 * Call this when attack completes, is interrupted, or enemy dies
	 * @param Holder The enemy actor releasing the token
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Tokens")
	void ReleaseAttackToken(AActor* Holder);

	/**
	 * Check if an enemy currently holds an attack token
	 * @param Actor The enemy to check
	 * @return True if this enemy has permission to attack
	 */
	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	bool HasAttackToken(AActor* Actor) const;

	/**
	 * Check if an enemy is waiting in the token queue
	 * @param Actor The enemy to check
	 * @return True if this enemy is queued for a token
	 */
	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	bool IsInTokenQueue(AActor* Actor) const;

	/**
	 * Remove an enemy from the token queue (e.g., if they die while waiting)
	 * @param Actor The enemy to remove from queue
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Tokens")
	void RemoveFromQueue(AActor* Actor);

	/**
	 * Force release all tokens and clear queue (e.g., combat ended, cutscene)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Tokens")
	void ResetAllTokens();

	// ============================================================================
	// QUERIES
	// ============================================================================

	/** Get current number of active attackers */
	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	int32 GetActiveAttackerCount() const { return ActiveAttackers.Num(); }

	/** Get current queue length */
	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	int32 GetQueueLength() const { return TokenQueue.Num(); }

	/** Get all currently active attackers */
	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	TArray<AActor*> GetActiveAttackers() const;

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when a token is granted to an enemy */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTokenGranted, AActor*, Attacker);
	UPROPERTY(BlueprintAssignable, Category = "Combat Tokens")
	FOnTokenGranted OnTokenGranted;

	/** Broadcast when a token is released */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTokenReleased, AActor*, Attacker);
	UPROPERTY(BlueprintAssignable, Category = "Combat Tokens")
	FOnTokenReleased OnTokenReleased;

	// ============================================================================
	// SUBSYSTEM LIFECYCLE
	// ============================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

protected:
	/** Enemies currently holding attack tokens */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> ActiveAttackers;

	/** Enemies waiting for attack tokens (FIFO queue) */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> TokenQueue;

	/** Cooldown tracking - when each enemy last held a token */
	TMap<TWeakObjectPtr<AActor>, float> LastTokenTime;

	/** Try to grant a token from the queue (called when a token is released) */
	void TryGrantQueuedToken();

	/** Check if enemy is on cooldown */
	bool IsOnCooldown(AActor* Actor) const;

	/** Clean up any invalid (destroyed) actors from tracking arrays */
	void CleanupInvalidActors();
};
