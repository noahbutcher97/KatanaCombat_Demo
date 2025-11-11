// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActionQueueTypes.h"
#include "CombatTypes.h"
#include "Characters/SamuraiCharacter.h"
#include "CombatComponentV2.generated.h"

// ============================================================================
// LOG CATEGORY
// ============================================================================

/**
 * Log category for Combat System
 * Usage: UE_LOG(LogCombat, Log, TEXT("Message"));
 * Console: Log LogCombat Verbose (enable detailed logging)
 * Console: Log LogCombat Warning (only warnings/errors)
 * Console: Log LogCombat Off (disable all combat logging)
 */
DECLARE_LOG_CATEGORY_EXTERN(LogCombat, Log, All);

/**
 * V2 Combat System - Timer-Based Action Queue
 *
 * This component implements the V2 combat system with:
 * - Timestamped input queue (all input captured, timing determined later)
 * - Timer checkpoint execution (snap vs responsive based on windows)
 * - Hold state persistence across combos
 * - Priority-based action cancellation
 *
 * Architecture:
 * 1. Input → FQueuedInputAction created with timestamp
 * 2. Input added to queue, matched with press/release pairs
 * 3. Timer checkpoints discovered from montage AnimNotifyStates
 * 4. Actions scheduled at checkpoints (snap or responsive)
 * 5. Montage playback reaches checkpoint → action executes
 *
 * Compatibility:
 * - Can run alongside V1 system (toggled via bUseV2System)
 * - Shares CombatComponent state, attack data, targeting
 * - Independent input processing and execution logic
 */

// Forward declarations
class UCombatComponent;
class UAttackData;
class UCombatSettings;
class UAnimInstance;

UCLASS(Blueprintable, ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class KATANACOMBAT_API UCombatComponentV2 : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponentV2();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	UFUNCTION(BlueprintPure, Category = "Combat|Input")
	ASamuraiCharacter* GetOwnerCharacter() const;

	UFUNCTION(BlueprintPure, Category= "Combat|Debug")
	bool GetDebugDraw() const;

	// ============================================================================
	// CACHED REFERENCES
	// ============================================================================

	/** Reference to main combat component (for shared state access) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	UCombatComponent* CombatComponent = nullptr;

	/** Cached owner character (for performance - avoids repeated casts) */
	UPROPERTY()
	TObjectPtr<ASamuraiCharacter> OwnerCharacter = nullptr;

	/** Combat settings (cached from character for performance) */
	UPROPERTY()
	TObjectPtr<UCombatSettings> CombatSettings = nullptr;

	// ============================================================================
	// INPUT PROCESSING (V2)
	// ============================================================================

	/**
	 * Unified input event handler
	 * All input goes through here (light, heavy, dodge, block)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Input")
	void OnInputEvent(EInputType InputType, EInputEventType EventType);

	/**
	 * Check if input can be processed
	 * V2 accepts input in more states than V1 (including during attacks)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Input")
	bool CanProcessInput(EInputType InputType) const;

	// ============================================================================
	// ACTION QUEUE MANAGEMENT
	// ============================================================================

	/**
	 * Add action to execution queue
	 * Determines execution mode (snap/responsive/immediate) based on context
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Queue")
	void QueueAction(const FQueuedInputAction& InputAction, UAttackData* AttackData = nullptr);

	/**
	 * Process queued actions at current montage time
	 * Called each tick to check for checkpoint arrivals
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Queue")
	void ProcessQueue(float CurrentMontageTime);

	/**
	 * Execute action from queue
	 * Returns true if execution succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Queue")
	bool ExecuteAction(FActionQueueEntry& Action);

	/**
	 * Play attack montage (V2 independent implementation)
	 * Does NOT touch V1 state machine - V2 manages its own phases
	 * @param AttackData - Attack to play
	 * @return True if montage started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Queue")
	bool PlayAttackMontage(UAttackData* AttackData);

	/**
	 * Cancel all pending actions
	 * Called on hit, guard break, or manual clear
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Queue")
	void ClearQueue(bool bCancelCurrent = false);

	/**
	 * Cancel actions based on priority
	 * Used for hit interrupts (severity determines priority)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Queue")
	void CancelActionsWithPriority(int32 MinPriority);

	// ============================================================================
	// TIMER CHECKPOINT SYSTEM
	// ============================================================================

	/**
	 * Discover checkpoints from current montage
	 * Scans AnimNotifyStates for window timings
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timing")
	void DiscoverCheckpoints(class UAnimMontage* Montage);

	/**
	 * Register checkpoint from AnimNotifyState
	 * Called by V2 window notifies (Combo, Parry, Cancel, Hold)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timing")
	void RegisterCheckpoint(EActionWindowType WindowType, float StartTime, float Duration);

	/**
	 * Check if montage time has reached checkpoint
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Timing")
	bool HasReachedCheckpoint(const FTimerCheckpoint& Checkpoint, float CurrentTime) const;

	/**
	 * Get execution checkpoint for action
	 * Returns snap (Active end) or responsive (Recovery end) based on combo window
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Timing")
	float GetExecutionCheckpoint(const FActionQueueEntry& Action) const;

	// ============================================================================
	// HOLD SYSTEM (V2)
	// ============================================================================

	/**
	 * Check if hold should activate
	 * V2: Checks button state at hold window, not duration
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Hold")
	void CheckHoldActivation();

	/**
	 * Activate hold state
	 * Persists across combo chain
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Hold")
	void ActivateHold(EInputType InputType, float PlayRate);

	/**
	 * Deactivate hold state
	 * Called on release event
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Hold")
	void DeactivateHold();

	/**
	 * Update hold playrate
	 * Called each frame during hold
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Hold")
	void UpdateHoldPlayRate(float DeltaTime);

	// ============================================================================
	// PHASE TRANSITION SYSTEM (V2)
	// ============================================================================

	/**
	 * Handle phase transition from AnimNotify
	 * Updates CurrentPhase and registers checkpoint for snap/immediate execution
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Phase")
	void OnPhaseTransition(EAttackPhase NewPhase);

	/**
	 * Set phase (internal V2 phase management)
	 * Called when V2 independently changes phase state
	 */
	void SetPhase(EAttackPhase NewPhase);

	/**
	 * Callback when montage starts blending out (early transition signal)
	 * Fires before OnMontageEnded for smoother transitions
	 */
	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * Callback when montage ends (event-driven phase transition)
	 * Automatically transitions to None phase
	 */
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// ============================================================================
	// STATE QUERIES
	// ============================================================================

	/** Is queue empty? */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	bool IsQueueEmpty() const { return ActionQueue.Num() == 0; }

	/** Get number of pending actions */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	int32 GetPendingActionCount() const;

	/** Is currently in hold state? */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	bool IsHolding() const { return HoldState.bIsHolding; }

	/** Get hold duration */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	float GetHoldDuration() const;

	/** Get current attack phase */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	EAttackPhase GetCurrentPhase() const { return CurrentPhase; }

	/** Is combo window active? */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	bool IsInComboWindow() const { return bComboWindowActive; }

	// ============================================================================
	// EVENT DELEGATES (Blueprint-exposed)
	// ============================================================================

	/** Fires when attack starts (IMMEDIATE or queued execution) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnV2AttackStarted OnAttackStarted;

	/** Fires when attack phase changes (Windup→Active→Recovery→None) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnV2PhaseChanged OnPhaseChanged;

	/** Fires when combo window state changes (opened/closed) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnV2ComboWindowChanged OnComboWindowChanged;

	/** Fires when hold state activates */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnV2HoldActivated OnHoldActivated;

	/** Fires on montage events (started, blending out, ended) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnV2MontageEvent OnMontageEvent;

	// ============================================================================
	// DEBUG / VISUALIZATION
	// ============================================================================

	/** Draw queue state and checkpoints */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void DrawDebugInfo() const;

	/** Get queue statistics */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	FQueueStats GetQueueStats() const { return QueueStats; }

	/** Reset statistics */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void ResetStats() { QueueStats.Reset(); }

	// ============================================================================
	// PUBLIC STATE (for debug visualization)
	// ============================================================================

	/** Action queue (FIFO execution) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	TArray<FActionQueueEntry> ActionQueue;

	/** Timer checkpoints for current montage */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	TArray<FTimerCheckpoint> Checkpoints;

	/** Hold state (persists across combos) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	FHoldState HoldState;

	/** Currently held inputs (for press/release matching) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	TMap<EInputType, float> HeldInputs;

protected:
	virtual void BeginPlay() override;

	/** Is combo window currently active? */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	bool bComboWindowActive = false;

	/** Combo window start time */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	float ComboWindowStart = 0.0f;

	/** Combo window duration */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	float ComboWindowDuration = 0.0f;

	/** Queue statistics */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	FQueueStats QueueStats;

	/** Current attack phase (tracked independently from V1) */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	EAttackPhase CurrentPhase = EAttackPhase::None;

	/** Currently executing attack (for combo progression tracking) */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	TObjectPtr<UAttackData> CurrentAttackData = nullptr;

	/** Input type that triggered current attack (Light/Heavy) */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	EInputType CurrentAttackInputType = EInputType::None;

	// ============================================================================
	// INTERNAL HELPERS
	// ============================================================================

	/** Match press/release pairs */
	void ProcessInputPair(const FQueuedInputAction& PressEvent, const FQueuedInputAction& ReleaseEvent);

	/** Determine execution mode for input */
	EActionExecutionMode DetermineExecutionMode(const FQueuedInputAction& InputAction) const;

	/** Get attack data for input type */
	UAttackData* GetAttackForInput(EInputType InputType) const;

	/** Calculate action priority */
	int32 CalculatePriority(const FActionQueueEntry& Action) const;

	/** Sort queue by scheduled time */
	void SortQueueByTime();

	/** Find next checkpoint of type */
	FTimerCheckpoint* FindCheckpoint(EActionWindowType WindowType);

	/** Clear expired checkpoints */
	void ClearExpiredCheckpoints(float CurrentTime);

	/** Check if can accept new input (prevents double-queueing same input) */
	bool CanAcceptNewInput(EInputType InputType) const;
};