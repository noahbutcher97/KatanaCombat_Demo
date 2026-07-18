// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/EnemyAITypes.h"
#include "CombatTypes.h"
#include "EnemyCombatAIComponent.generated.h"

class UCombatTokenSubsystem;
class UAttackData;
class UAnimMontage;

/**
 * Enemy Combat AI Component
 *
 * Manages enemy combat decision-making and attack coordination.
 * Works with CombatTokenSubsystem to ensure readable, counterable attack patterns.
 *
 * Design Philosophy:
 * - Enemies request attack tokens before attacking (prevents spam)
 * - While waiting for token, enemies circle the player
 * - Token holders approach and execute attacks with counter windows
 * - Tokens released after attack completes or enemy is interrupted
 *
 * Usage:
 * - Add to enemy character alongside CombatComponent
 * - Configure circling/approach behavior via exposed properties
 * - Call TryInitiateAttack() from StateTree when enemy wants to attack
 * - Component handles token management and state transitions
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class KATANACOMBAT_API UEnemyCombatAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyCombatAIComponent();

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Circling behavior configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Circling")
	FEnemyCirclingConfig CirclingConfig;

	/** Approach behavior configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Approach")
	FEnemyApproachConfig ApproachConfig;

	/** Available attacks for this enemy */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Attacks")
	TArray<FEnemyAttackConfig> AvailableAttacks;

	/** How this enemy selects which attack to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Attacks")
	EEnemyAttackSelection AttackSelectionMode = EEnemyAttackSelection::Random;

	/** Recovery time after completing an attack before can attack again */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Timing", meta = (ClampMin = 0.0))
	float PostAttackRecoveryTime = 1.0f;

	/** Recovery time after being staggered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Timing", meta = (ClampMin = 0.0))
	float StaggerRecoveryTime = 1.5f;

	// ============================================================================
	// STATE
	// ============================================================================

	/** Current AI state */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
	EEnemyAIState CurrentState = EEnemyAIState::Idle;

	/** The player/target we're fighting */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
	TWeakObjectPtr<AActor> CombatTarget;

	/** Currently selected attack (valid during Approaching/Attacking states) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
	TObjectPtr<UAttackData> SelectedAttack;

	/** Current circling direction (1 = clockwise, -1 = counter-clockwise) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
	int32 CirclingDirection = 1;

	// ============================================================================
	// COMBAT API
	// ============================================================================

	/**
	 * Attempt to initiate an attack sequence
	 * Requests token from subsystem and transitions to Approaching if granted
	 * @return True if attack sequence started (token granted), false if queued/denied
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	bool TryInitiateAttack();

	/**
	 * Cancel a pending queued attack request without releasing an active token.
	 * Used when a StateTree request task times out or exits before a grant.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void CancelQueuedAttackRequest();

	/**
	 * Abort queued, approaching, active, or recovering attack ownership and return to a ready state.
	 * Safe to call repeatedly; active attacks broadcast one interrupted end event.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void AbortAttack();

	/**
	 * Execute the selected attack (called when in range)
	 * Plays attack montage with CounterWindow notify
	 * @return True if attack execution started
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	bool ExecuteAttack();

	/**
	 * Called when player successfully counters this enemy's attack
	 * Transitions to appropriate state based on counter severity
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void OnCountered();

	/**
	 * Called when player successfully parries this enemy's attack
	 * Transitions to staggered state
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void OnParried();

	/**
	 * Called when this enemy takes damage
	 * May interrupt current attack and release token
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void OnDamaged();

	/**
	 * Called when this enemy dies
	 * Releases token and transitions to Dying state
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void OnDeath();

	/**
	 * Set the combat target (usually the player)
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void SetCombatTarget(AActor* Target);

	// ============================================================================
	// MOVEMENT API (for StateTree tasks)
	// ============================================================================

	/**
	 * Get the desired circling position relative to target
	 * @return World position to move toward for circling behavior
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Movement")
	FVector GetCirclingDestination() const;

	/**
	 * Check if we're within attack range of target
	 * @return True if close enough to execute attack
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Movement")
	bool IsInAttackRange() const;

	/**
	 * Get distance to combat target
	 * @return Distance in units, or MAX_FLT if no target
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Movement")
	float GetDistanceToTarget() const;

	/**
	 * Update circling direction (call periodically for variety)
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Movement")
	void RandomizeCirclingDirection();

	// ============================================================================
	// QUERIES
	// ============================================================================

	/** Check if currently holding an attack token */
	UFUNCTION(BlueprintPure, Category = "AI|State")
	bool HasAttackToken() const;

	/** Check if waiting in token queue */
	UFUNCTION(BlueprintPure, Category = "AI|State")
	bool IsWaitingForToken() const;

	/** Check if in a state that allows attacking */
	UFUNCTION(BlueprintPure, Category = "AI|State")
	bool CanAttemptAttack() const;

	/** Retain attack suppression for one exact defense interaction. */
	bool AcquireDefenseChainSuppression(const FDefenseInteractionId& InteractionId);

	/** Release only the suppression owned by the exact defense interaction. */
	bool ReleaseDefenseChainSuppression(const FDefenseInteractionId& InteractionId);

	bool IsDefenseChainSuppressed() const
	{
		return !DefenseChainSuppressions.IsEmpty();
	}

	/** Check if currently attacking */
	UFUNCTION(BlueprintPure, Category = "AI|State")
	bool IsAttacking() const { return CurrentState == EEnemyAIState::Attacking; }

	/** Check if staggered/vulnerable */
	UFUNCTION(BlueprintPure, Category = "AI|State")
	bool IsStaggered() const { return CurrentState == EEnemyAIState::Staggered; }

	/** Exact generation currently owned by the active StateTree attack task. */
	int32 GetActiveAttackGeneration() const
	{
		return ActiveAttackInstance.AttackGeneration;
	}

	/** True when this component observed source-side consumption for this generation. */
	bool WasAttackGenerationConsumed(int32 AttackGeneration) const
	{
		return AttackGeneration > 0
			&& LastConsumedAttackInstance.Attacker.Get() == GetOwner()
			&& LastConsumedAttackInstance.AttackGeneration == AttackGeneration;
	}

	/** Inject deterministic token ownership for automation worlds that do not own a GameInstance. */
	void SetTokenSubsystemForTesting(UCombatTokenSubsystem* InTokenSubsystem);

#if WITH_AUTOMATION_TESTS
	int32 GetTokenReleaseCountForTesting() const { return TokenReleaseCountForTesting; }
	int32 GetAttackEndBroadcastCountForTesting() const { return AttackEndBroadcastCountForTesting; }
#endif

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when AI state changes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAIStateChanged, EEnemyAIState, OldState, EEnemyAIState, NewState);
	UPROPERTY(BlueprintAssignable, Category = "AI|Events")
	FOnAIStateChanged OnAIStateChanged;

	/** Broadcast when attack token is granted */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTokenGranted);
	UPROPERTY(BlueprintAssignable, Category = "AI|Events")
	FOnTokenGranted OnTokenGranted;

	/** Broadcast when attack begins */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttackStarted, UAttackData*, AttackData);
	UPROPERTY(BlueprintAssignable, Category = "AI|Events")
	FOnAttackStarted OnAttackStarted;

	/** Broadcast when attack completes or is interrupted */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttackEnded, bool, bWasInterrupted);
	UPROPERTY(BlueprintAssignable, Category = "AI|Events")
	FOnAttackEnded OnAttackEnded;

protected:
	// ============================================================================
	// INTERNAL
	// ============================================================================

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Cached reference to token subsystem */
	UPROPERTY()
	TObjectPtr<UCombatTokenSubsystem> TokenSubsystem;

	/** Timer handle for recovery states */
	FTimerHandle RecoveryTimerHandle;

	/** Timer handle for circling direction changes */
	FTimerHandle CirclingDirectionTimerHandle;

	/** Index for sequential attack selection */
	int32 SequentialAttackIndex = 0;

	/** Time when approach started (for timeout) */
	float ApproachStartTime = 0.0f;

	/** Change AI state with broadcast */
	void SetState(EEnemyAIState NewState);

	/** Select an attack based on current selection mode */
	UAttackData* SelectAttack();

	/** Release attack token and clean up */
	void ReleaseTokenAndCleanup();

	void HandleAttackConsumedInternal(const FAttackConsumedEvent& Event);
	bool TerminateActiveAttack(
		bool bInterrupted,
		EEnemyAIState TerminalState,
		float RecoveryDuration,
		bool bStopActiveMontage);
	void UnbindAttackConsumption();

	/** Return to the next non-attacking state after an attack could not start. */
	void ReturnToReadyState();

	/** Called when recovery timer expires */
	UFUNCTION()
	void OnRecoveryComplete();

	/** Called when token is granted (bound to subsystem delegate) */
	UFUNCTION()
	void HandleTokenGranted(AActor* Attacker);

	/** Called when attack montage ends */
	UFUNCTION()
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Called when the owning combat character receives lethal damage. */
	UFUNCTION()
	void HandleOwnerDying(AActor* Killer);

	/** Schedule circling direction change */
	void ScheduleCirclingDirectionChange();

	/** Replace cached token subsystem and keep delegate bindings consistent. */
	void SetTokenSubsystem(UCombatTokenSubsystem* InTokenSubsystem);

	/** Ensure owner death delegates are bound before this component can hold combat tokens. */
	void BindOwnerDeathEvents();

	/** True only while this component is queued and waiting for an async token grant. */
	bool bWaitingForTokenGrant = false;

	TSet<FDefenseInteractionId> DefenseChainSuppressions;

	FAttackInstanceId ActiveAttackInstance;
	FAttackInstanceId LastConsumedAttackInstance;
	FDelegateHandle AttackConsumedDelegateHandle;
	bool bAttackTerminationCommitted = false;

#if WITH_AUTOMATION_TESTS
	int32 TokenReleaseCountForTesting = 0;
	int32 AttackEndBroadcastCountForTesting = 0;
#endif
};
