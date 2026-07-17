// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "Data/PairedAnimationTypes.h"
#include "PairedAnimationComponent.generated.h"

// Forward declarations
class ABaseCombatCharacter;
class UCombatComponent;
class UAttackData;
class UPairedAnimationData;
class UTargetingComponent;
class UHitReactionComponent;
DECLARE_LOG_CATEGORY_EXTERN(LogPairedAnim, Log, All);

/**
 * Paired Animation Component - Manages finishers, counters, and all paired animation logic.
 *
 * Extracted from CombatComponent (Phase 3 decomposition) to reduce god-object complexity.
 * CombatComponent delegates to this component for all paired animation operations.
 *
 * Responsibilities:
 * - Finisher execution flow (TryExecuteFinisher -> CompletePairedAnimation)
 * - Counter system (AC3 instant counter-kill + Chain parry->counter->finisher)
 * - Counter/Parry window state management
 * - Partner tracking and collision management
 * - Paired animation effects (slow-mo, camera shake, audio, VFX)
 * - Paired animation lifecycle (begin, sync points, end, cancel)
 *
 * Dependencies (reads from CombatComponent):
 * - GetCurrentPhase() / SetPhase()
 * - GetCurrentAttack()
 * - ClearQueue()
 * - GetDebugDraw() (via own implementation)
 *
 * This component lives on ABaseCombatCharacter alongside the 4 existing combat components.
 */
UCLASS(Blueprintable, ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class KATANACOMBAT_API UPairedAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

	// ============================================================================
	// TEST FRIEND DECLARATIONS
	// ============================================================================

	// Counter system tests
	friend class FCounter_AC3LethalDamage;
	friend class FCounter_AC3StaggersEnemy;
	friend class FCounter_AC3HitInfoMarkedAsCounter;
	friend class FCounter_AC3SpecificCounterDataFallbackDamage;
	friend class FCounter_AC3NullAttackerFails;
	friend class FCounter_ChainParryTransition;
	friend class FCounter_ChainCancelResetsState;
	friend class FCounter_CancelNoopWhenNone;
	friend class FCounter_CounterAttackRequiresWindow;
	friend class FCounter_ChainParryStaggersEnemy;
	friend class FCounter_ChainNullAttackerFails;
	friend class FCounter_ChainAttackInputAdvancesCounter;
	friend class FCounter_ChainAdvanceRejectsNullAttackData;
	friend class FDefenseInput_ChainPreflightFailureExpires;

	// Paired animation tests
	friend class FPairedAnim_PartnerTracking;
	friend class FPairedAnim_InputBlocking;
	friend class FPairedAnim_EffectLifecycle;
	friend class FPairedAnimationRejectsFriendlyTargetTest;

public:
	UPairedAnimationComponent();

	// ============================================================================
	// CONFIGURATION / CACHED REFERENCES
	// ============================================================================

	/** Get the owner as a BaseCombatCharacter (cached for performance) */
	UFUNCTION(BlueprintPure, Category = "Combat|Paired Animation")
	ABaseCombatCharacter* GetOwnerCharacter() const;

	/** Debug draw enabled? (reads from CVar system via CombatDebug::IsDebugEnabled()) */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	bool GetDebugDraw() const;

	// ============================================================================
	// FINISHER EXECUTION
	// ============================================================================

	/**
	 * Try to execute a finisher on the current target.
	 * Checks if target is vulnerable and attack has FinisherData.
	 * If successful, plays paired finisher animation instead of normal attack.
	 *
	 * @param AttackData - Attack being executed (must have FinisherData)
	 * @return True if finisher was executed (caller should skip normal attack)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Finisher")
	bool TryExecuteFinisher(UAttackData* AttackData);

	// ============================================================================
	// COUNTER WINDOW API
	// ============================================================================

	/**
	 * Is this character currently in a counter window? (can be countered)
	 * Called by defenders to check if this attacker is vulnerable to counter
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	bool IsInCounterWindow() const { return bCounterWindowActive; }

	/**
	 * Get progress through counter window (0.0 = start, 1.0 = end)
	 * Used for perfect counter timing detection
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	float GetCounterWindowProgress() const
	{
		if (!bCounterWindowActive || CounterWindowData.WindowDuration <= 0.0f)
		{
			return 0.0f;
		}
		return FMath::Clamp(CounterWindowData.TimeInWindow / CounterWindowData.WindowDuration, 0.0f, 1.0f);
	}

	/**
	 * Get the counter window data for pose-matching
	 * Only valid when IsInCounterWindow() returns true
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	const FCounterContext& GetCounterWindowData() const { return CounterWindowData; }

	/**
	 * Set counter window data (called by AnimNotifyState_CounterWindow::NotifyBegin)
	 * Marks this character as counterable and provides pose-matching info
	 */
	void SetCounterWindowData(EAttackType InAttackType, ESwingDirection InSwingDirection,
							  UPairedAnimationData* InCounterData, float InWindowDuration);

	/**
	 * Is this character currently in a parry window? (can be parried)
	 * Called by defenders to check if this attacker's attack can be parried.
	 * Parry window is typically during early Windup phase.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	bool IsInParryWindow() const { return bParryWindowActive; }

	/** Set parry window active state (called by AnimNotifyState_ParryWindow) */
	void SetParryWindowActive(bool bActive);

	/**
	 * Clear counter window data (called by AnimNotifyState_CounterWindow::NotifyEnd)
	 * Marks this character as no longer counterable
	 */
	void ClearCounterWindowData();

	// ============================================================================
	// COUNTER SYSTEM API
	// ============================================================================

	/**
	 * Attempt to perform a counter action
	 * Routes to AC3 mode (instant counter-kill) or Chain mode (parry initiation)
	 * based on CounterMode setting
	 * @return True if counter was initiated successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Counter")
	bool TryCounter();

	/**
	 * Check if this character can currently perform a counter
	 * Validates combat state, nearby counterable enemies, and mode-specific requirements
	 * @return True if CanCounter conditions are met
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	bool CanCounter() const;

	/**
	 * Find the nearest enemy currently in their counter window
	 * Searches within soft-lock range for enemies with active counter windows
	 * @return Enemy actor if found, nullptr otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	AActor* FindCounterableEnemy() const;

	/**
	 * Get counter context for a specific enemy
	 * Used to retrieve pose-matching data when executing counter
	 * @param Enemy The enemy to get counter context from
	 * @return Counter context with attack type, swing direction, and counter data
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	FCounterContext GetEnemyCounterContext(AActor* Enemy) const;

	/**
	 * Get counter context for an enemy in their parry window.
	 * Used by Chain mode, where the attacker is parryable before they expose a counter window.
	 * @param Enemy The enemy to get parry context from
	 * @return Counter context with attacker and best-effort source attack metadata
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	FCounterContext GetEnemyParryContext(AActor* Enemy) const;

	/**
	 * Find the nearest enemy currently in their parry window.
	 * Used by Chain counter mode to find parryable enemies.
	 * @return Enemy actor if found, nullptr otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	AActor* FindParryableEnemy() const;

	/**
	 * Advance an active Chain counter from the waiting window using selected attack data.
	 * The selected attack data is resolved by CombatComponent from the player's input.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Counter")
	bool TryAdvanceChainCounter(UAttackData* SelectedAttackData);

	/** Current Chain counter state. */
	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	EChainCounterState GetChainState() const { return ChainState; }

	/** True while Chain mode is waiting for attack input after a successful parry. */
	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	bool IsChainCounterWaitingForAttack() const { return ChainState == EChainCounterState::CounterWindow; }

	/** True while Chain mode has a retained parried target for follow-up counter/finisher steps. */
	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	bool HasActiveChainTarget() const { return ActiveChainTarget.IsValid(); }

	/** Resolve whether paired animation data should be treated as lethal for this reaction type. */
	UFUNCTION(BlueprintPure, Category = "Combat|Paired Animation")
	bool ShouldTreatPairedAnimationAsLethal(EPairedReactionType ReactionType, const UPairedAnimationData* PairedAnimData) const;

	// ============================================================================
	// PAIRED ANIMATION EVENTS (Delegates)
	// ============================================================================

	/** Fires when a paired animation (finisher, counter) starts */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Paired Animation")
	FOnPairedAnimationStarted OnPairedAnimationStarted;

	/** Fires at sync points during paired animations (impact, damage application) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Paired Animation")
	FOnPairedAnimationSyncPoint OnPairedAnimationSyncPoint;

	/** Fires when paired animation ends */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Paired Animation")
	FOnPairedAnimationEnded OnPairedAnimationEnded;

	// ============================================================================
	// PAIRED ANIMATION PARTNER TRACKING
	// ============================================================================

	/**
	 * Actors currently participating in paired animation with this character.
	 * Used for targeted collision disabling (only ignore tracked partners, not all pawns).
	 * Supports multi-partner scenarios like double takedowns or group finishers.
	 *
	 * Managed via AddPairedPartner/RemovePairedPartner/ClearPairedPartners API.
	 * AnimNotifyState_PairedAnimationCollision reads this to know which actors to ignore.
	 */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> PairedAnimationPartners;

	/**
	 * Add actor as paired animation partner.
	 * Partner's collision will be ignored during paired animations via IgnoreActorWhenMoving().
	 * Safe to call multiple times with same actor (idempotent).
	 *
	 * @param Partner - Actor to track as paired animation partner
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void AddPairedPartner(AActor* Partner);

	/**
	 * Remove actor from paired animation partners.
	 * Collision with this actor will be restored.
	 * Safe to call if actor is not currently tracked (no-op).
	 *
	 * @param Partner - Actor to remove from partner tracking
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void RemovePairedPartner(AActor* Partner);

	/**
	 * Clear all paired animation partners.
	 * Called when paired animation ends or is interrupted.
	 * Does NOT restore collision - that's handled by AnimNotifyState_PairedAnimationCollision.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void ClearPairedPartners();

	/**
	 * Check if actor is currently a paired animation partner.
	 *
	 * @param Actor - Actor to check
	 * @return True if actor is tracked as partner
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Paired Animation")
	bool IsPairedPartner(AActor* Actor) const;

	/**
	 * Get count of current paired animation partners.
	 * Useful for debugging and multi-partner finisher logic.
	 *
	 * @return Number of tracked partners
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Paired Animation")
	int32 GetPairedPartnerCount() const { return PairedAnimationPartners.Num(); }

	// ========================================================================
	// PAIRED ANIMATION EFFECT HANDLING
	// ========================================================================

	/**
	 * Currently active paired animation data.
	 * Set when a paired animation starts, cleared when it ends.
	 * Used by effect handlers to access slow-motion and camera shake settings.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Paired Animation")
	TObjectPtr<UPairedAnimationData> ActivePairedAnimData;

	/**
	 * Whether combat input (attacks, evades) should be blocked.
	 * Set true during paired animations/finishers to prevent unintended input.
	 * Camera input is still allowed.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Paired Animation")
	bool bBlockCombatInput = false;

	/**
	 * Check if combat input is currently blocked (during paired animations/finishers).
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Paired Animation")
	bool IsInputBlocked() const { return bBlockCombatInput; }

	/**
	 * Begin a paired animation sequence with effect handling.
	 * Stores paired animation data, applies slow motion if configured, broadcasts delegates.
	 *
	 * @param PairedAnimData - The paired animation configuration (slow-mo, camera shake, etc.)
	 * @param ReactionType - Type of paired reaction (Finisher, Counter, etc.)
	 * @param bIsCriticalMoment - Whether this is a dramatic moment (triggers additional effects)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void BeginPairedAnimation(UPairedAnimationData* PairedAnimData, EPairedReactionType ReactionType, bool bIsCriticalMoment = true);

	/**
	 * End the current paired animation sequence.
	 * Restores time dilation, clears active paired animation data, broadcasts delegates.
	 * Safe to call even if no paired animation is active.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void EndPairedAnimation();

	/**
	 * Trigger sync point effects (camera shake, damage).
	 * Called at impact moment during paired animation.
	 * Uses ActivePairedAnimData for camera shake configuration.
	 *
	 * @param SyncPointName - Name of the sync point (for logging/identification)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void TriggerSyncPointEffects(FName SyncPointName);

	/**
	 * Check if a paired animation is currently active.
	 *
	 * @return True if ActivePairedAnimData is set
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Paired Animation")
	bool IsPairedAnimationActive() const { return ActivePairedAnimData != nullptr; }

	// ========================================================================
	// PAIRED ANIMATION INTERRUPT HANDLING
	// ========================================================================

	/**
	 * Called when a paired animation partner dies during an active paired animation.
	 * Cancels any ongoing paired animation montage and cleans up state.
	 *
	 * @param DeadPartner - The partner actor that just died
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void OnPairedPartnerDeath(AActor* DeadPartner);

	/**
	 * Cancel the current paired animation immediately.
	 * Used when a partner dies or other interrupt conditions occur.
	 * Stops montage, clears partners, restores state.
	 * Does NOT apply damage (use CompletePairedAnimation for successful completion).
	 *
	 * @param BlendOutTime - How quickly to blend out the current montage (default 0.1s)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void CancelPairedAnimation(float BlendOutTime = 0.1f);

	/**
	 * Complete the current paired animation successfully.
	 * Called when finisher montage ends normally (not interrupted).
	 * Applies damage to victim, handles death if lethal, cleans up all state.
	 * This is distinct from CancelPairedAnimation which is for interruptions.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void CompletePairedAnimation();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ============================================================================
	// COUNTER SYSTEM INTERNAL METHODS
	// ============================================================================

	/** AC3 mode: Instant counter-kill. Slow-mo -> paired animation -> lethal damage. */
	bool TryCounter_AC3Mode(const FCounterContext& Context);

	/** Start a paired animation directly against a known target using explicit paired data. */
	bool TryStartPairedAnimationWithTarget(AActor* TargetActor, UPairedAnimationData* PairedAnimData, EPairedReactionType ReactionType);

	/** True when a paired animation may legally start against TargetActor. */
	bool IsValidPairedTarget(AActor* TargetActor) const;

	/** Chain mode step 1: Parry the enemy's attack, opening a counter window on the player. */
	bool TryCounter_ChainMode(const FCounterContext& Context);

	/** Chain mode step 2: Execute counter attack during the player's counter window. */
	bool ExecuteChainCounterAttack(UAttackData* ChainAttackData);

	/** Chain mode step 3: Execute finisher to complete the chain. */
	bool ExecuteChainFinisher();

	/** Cancel the chain counter mid-sequence (timeout, damage taken, etc.) */
	void CancelChainCounter();

	/** Clear retained Chain context without running animation or combat side effects. */
	void ClearChainContext();

	/** Handle chain timeout expiration */
	void OnChainTimeout();

	// ============================================================================
	// COUNTER/PARRY WINDOW STATE
	// ============================================================================

	/** Is this character currently in a counter window? (can be countered by defenders) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Counter")
	bool bCounterWindowActive = false;

	/** Is this character currently in a parry window? (can be parried by defenders) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Counter")
	bool bParryWindowActive = false;

	/** Counter context data (pose-matching info for defenders) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Counter")
	FCounterContext CounterWindowData;

	/** Counter system mode - AC3 (one-step) vs Chain (three-step) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Counter")
	ECounterSystemMode CounterMode = ECounterSystemMode::Chain;

	/** Chain mode state machine (only used when CounterMode == Chain) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Counter")
	EChainCounterState ChainState = EChainCounterState::None;

	/** Retained context from the parried attack while Chain mode waits for follow-up input. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Counter")
	FCounterContext ActiveChainContext;

	/** Retained target from the parried attack while Chain mode waits for follow-up input. */
	UPROPERTY()
	TWeakObjectPtr<AActor> ActiveChainTarget;

	/** Attack data selected by the input that advances an active Chain counter. */
	UPROPERTY()
	TObjectPtr<UAttackData> ActiveChainAttackData = nullptr;

	/** True when a Chain counter paired animation should continue into finisher handling on completion. */
	bool bContinueChainAfterCounterPairedAnimation = false;

	/** Allows legacy notify-provided counter data to fill in when selected AttackData has no CounterData. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Counter")
	bool bAllowNotifyCounterDataFallback = false;

	/** Allows Counter paired animation data marked lethal to apply lethal damage. Disabled by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Counter")
	bool bAllowLethalCounterPairedData = false;

	/** Timer handle for chain mode timeout */
	FTimerHandle ChainTimeoutHandle;

	// ============================================================================
	// PAIRED ANIMATION INTERNAL STATE
	// ============================================================================

	/** Timer handle for slow-motion restoration (safeguard against permanent slow-mo on interrupt) */
	FTimerHandle SlowMotionRestoreHandle;

	/** Cached reaction type for active paired animation (used by EndPairedAnimation for delegate broadcast) */
	EPairedReactionType ActivePairedReactionType = EPairedReactionType::None;

	/** Tracked victim during finisher execution (for damage application at completion) */
	TWeakObjectPtr<AActor> CurrentFinisherVictim;

	/** Guard flag to prevent CompletePairedAnimation from being called multiple times (Gap 20.4) */
	bool bCompletingPairedAnimation = false;

	/** Is character movement currently disabled? (for procedural sync) */
	bool bMovementCurrentlyDisabled = false;

	/**
	 * Callback for slow-motion restoration timer.
	 * Called after SlowMotionDuration expires (safeguard against permanent slow-mo).
	 */
	void OnSlowMotionTimerExpired();

private:
	// ============================================================================
	// CACHED REFERENCES (initialized in BeginPlay)
	// ============================================================================

	/** Cached owner character (set in BeginPlay, avoids repeated casts) */
	UPROPERTY()
	TObjectPtr<ABaseCombatCharacter> CachedOwnerCharacter = nullptr;

	/** Cached combat component (for querying phase, clearing queue, etc.) */
	UPROPERTY()
	TObjectPtr<UCombatComponent> CachedCombatComponent = nullptr;

};
