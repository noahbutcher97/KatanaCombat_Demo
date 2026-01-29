// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ActionQueueTypes.h"
#include "CombatTypes.h"
#include "Characters/BaseCombatCharacter.h"
#include "CombatComponent.generated.h"

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
 * Combat Component - Timer-Based Action Queue
 *
 * This component implements the combat system with:
 * - Timestamped input queue (all input captured, timing determined later)
 * - Timer checkpoint execution (snap vs responsive based on windows)
 * - Hold state persistence across combos
 * - Priority-based action cancellation
 *
 * Architecture:
 * 1. Input -> FQueuedInputAction created with timestamp
 * 2. Input added to queue, matched with press/release pairs
 * 3. Timer checkpoints discovered from montage AnimNotifyStates
 * 4. Actions scheduled at checkpoints (snap or responsive)
 * 5. Montage playback reaches checkpoint -> action executes
 *
 * This component is the core of the combat system, managing attack execution,
 * input buffering, combo chains, and hold mechanics.
 */

// Forward declarations
class UAttackData;
class UCombatSettings;
class UAnimInstance;

// ============================================================================
// DEBUG VISUALIZATION TESTING SUPPORT
// ============================================================================

UCLASS(Blueprintable, ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class KATANACOMBAT_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	UFUNCTION(BlueprintPure, Category = "Combat|Input")
	ABaseCombatCharacter* GetOwnerCharacter() const;

	UFUNCTION(BlueprintPure, Category= "Combat|Debug")
	bool GetDebugDraw() const;

	/** Get default light attack from AttackConfiguration */
	UFUNCTION(BlueprintPure, Category = "Combat|Attack")
	UAttackData* GetDefaultLightAttack() const;

	/** Get default heavy attack from AttackConfiguration */
	UFUNCTION(BlueprintPure, Category = "Combat|Attack")
	UAttackData* GetDefaultHeavyAttack() const;

	/** Get currently executing attack data */
	UFUNCTION(BlueprintPure, Category = "Combat|Attack")
	UAttackData* GetCurrentAttack() const { return CurrentAttackData; }

	// ============================================================================
	// CACHED REFERENCES
	// ============================================================================

	/** Cached owner character (for performance - avoids repeated casts) */
	UPROPERTY()
	TObjectPtr<ABaseCombatCharacter> OwnerCharacter = nullptr;

	/** Combat settings (cached from character for performance) */
	UPROPERTY()
	TObjectPtr<UCombatSettings> CombatSettings = nullptr;

	// ============================================================================
	// INPUT PROCESSING
	// ============================================================================

	/**
	 * Core input event handler - processes already-transformed input
	 * All input processing logic lives here (queuing, hold detection, state management)
	 *
	 * NOTE: Input should be pre-transformed to character-relative space via OnInputEventWithTransform()
	 * or OnInputEventAuto(). Direct calls with camera-relative directions will produce incorrect results.
	 *
	 * @param InputType - Type of input (LightAttack, HeavyAttack, Evade, Block)
	 * @param EventType - Press or Release
	 * @param InputDirection - Character-relative 8-way directional input
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Input", meta = (DisplayName = "On Input Event (Core Handler)"))
	void OnInputEvent(EInputType InputType, EInputEventType EventType, EInputDirection InputDirection = EInputDirection::None);

	/**
	 * Character-relative input transformation handler
	 * Transforms camera-relative input to character space (accounting for mesh offset)
	 * then delegates to OnInputEvent() for processing
	 *
	 * @param InputType - Type of input (LightAttack, HeavyAttack, Evade, Block)
	 * @param EventType - Press or Release
	 * @param CameraRelativeInput - Raw 2D input from gamepad/keyboard (X=right, Y=forward relative to camera)
	 * @param CameraRotation - Current camera rotation (only Yaw used)
	 * @param CharacterRotation - Current character ACTOR rotation (mesh offset applied automatically)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Input", meta = (DisplayName = "On Input Event (Character-Relative)"))
	void OnInputEventWithTransform(
		EInputType InputType,
		EInputEventType EventType,
		FVector2D CameraRelativeInput,
		FRotator CameraRotation,
		FRotator CharacterRotation);

	/**
	 * SIMPLIFIED helper that automatically gets camera and character rotations
	 * Call this from Blueprint or C++ with just input type, event type, and raw movement input
	 * Automatically transforms to character-relative or camera-relative space based on flag
	 *
	 * @param InputType - Type of input (LightAttack, HeavyAttack, Evade, Block)
	 * @param EventType - Press or Release
	 * @param MovementInput - Raw 2D input from gamepad/keyboard (X=right, Y=forward relative to camera)
	 * @param bCharacterRelative - If true (default), transforms to character-relative. If false, uses camera-relative (legacy behavior)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Input", meta = (DisplayName = "On Input Event (Auto Transform)"))
	void OnInputEventAuto(
		EInputType InputType,
		EInputEventType EventType,
		FVector2D MovementInput,
		bool bCharacterRelative = true);

	/**
	 * Check if input can be processed
	 * Accepts input in most states (including during attacks)
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
	 * Process queued actions at current montage time (DEPRECATED - tick-based)
	 * Replaced by event-driven ProcessQueuedActions(TargetPhase)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Queue")
	void ProcessQueue(float CurrentMontageTime);

	/**
	 * Process queued actions targeting specific phase (EVENT-DRIVEN)
	 * Called from OnPhaseTransition instead of tick
	 * @param TargetPhase - Execute actions queued for this phase
	 */
	void ProcessQueuedActions(EAttackPhase TargetPhase);

	/**
	 * Execute action from queue
	 * Returns true if execution succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Queue")
	bool ExecuteAction(FActionQueueEntry& Action);

	/**
	 * Play attack montage (independent implementation)
	 * Manages its own phases
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
	 * Called by window notifies (Combo, Parry, Cancel, Hold)
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
	// HOLD SYSTEM
	// ============================================================================

	/**
	 * Called when hold window starts (from AnimNotify_HoldWindowStart)
	 * Event-driven hold detection - checks button state at window start
	 *
	 * Implementation:
	 * - Checks if corresponding button is STILL pressed (via HeldInputs map)
	 * - Light attacks: Calls ActivateHold() to begin ease slowdown
	 * - Heavy attacks: Calls ActivateHold() and loops charge section
	 *
	 * @param InputType - Which input to check (LightAttack or HeavyAttack)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Hold")
	void OnHoldWindowStart(EInputType InputType);


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


	// ============================================================================
	// PHASE TRANSITION SYSTEM
	// ============================================================================

	/**
	 * Handle phase transition from AnimNotify
	 * Updates CurrentPhase and registers checkpoint for snap/immediate execution
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Phase")
	void OnPhaseTransition(EAttackPhase NewPhase);

	/**
	 * Set phase (internal phase management)
	 * Called when independently changing phase state
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

	/** Get total number of actions in queue */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	int32 GetQueueSize() const { return ActionQueue.Num(); }

	/** Get number of pending (not yet executing) actions */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	int32 GetPendingActionCount() const;

	/** Get const reference to action queue for inspection/testing */
	const TArray<FActionQueueEntry>& GetActionQueue() const { return ActionQueue; }

	/** Is currently in hold state? */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	bool IsHolding() const { return HoldState.IsHolding(); }

	/** Get hold duration */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	float GetHoldDuration() const;

	/** Get input type that triggered current hold */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	EInputType GetHoldInputType() const { return HoldState.CurrentHold.InputType; }

	/** Get const reference to directional input buffer for inspection/testing */
	const FDirectionalInputBuffer& GetDirectionalInputBuffer() const { return DirectionalInputBuffer; }

	/** Get current attack phase */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	EAttackPhase GetCurrentPhase() const { return CurrentPhase; }

	/** Is combo window active? */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	bool IsInComboWindow() const { return bComboWindowActive; }

	/** Is character currently attacking? (has active attack data) */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	bool IsAttacking() const { return CurrentAttackData != nullptr; }

	/** Get active windows at specified montage time */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	TArray<FTimerCheckpoint> GetActiveWindows(float CurrentTime) const;

	// ============================================================================
	// EVENT DELEGATES (Blueprint-exposed)
	// ============================================================================

	/** Fires when attack starts (IMMEDIATE or queued execution) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnAttackStarted OnAttackStarted;

	/** Fires when attack phase changes (Windup->Active->Recovery->None) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnPhaseChanged OnPhaseChanged;

	/** Fires when combo window state changes (opened/closed) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnComboWindowChanged OnComboWindowChanged;

	/** Fires when hold state activates */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnHoldActivated OnHoldActivated;

	/** Fires on montage events (started, blending out, ended) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnMontageEvent OnMontageEvent;

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

	/**
	 * Last captured 8-way directional input (used for directional attacks, evades, holds)
	 *
	 * DEPRECATED: Use DirectionalInputBuffer instead.
	 * This variable sampled direction continuously (semantic input conflation bug).
	 * Maintained for backward compatibility only.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State", meta = (DeprecatedProperty, DeprecationMessage = "Use DirectionalInputBuffer instead."))
	EInputDirection LastDirectionalInput = EInputDirection::None;

	/**
	 * Tracks whether LastDirectionalInput has been consumed by a directional follow-up.
	 * Prevents infinite loop bug where holding direction + spamming attack repeats same directional.
	 * Reset to false on each new directional input, set to true after first directional follow-up.
	 *
	 * DEPRECATED: Use DirectionalInputBuffer instead.
	 * This was a symptom fix for semantic input conflation. Architectural fix now implemented.
	 * Maintained for backward compatibility only.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State", meta = (DeprecatedProperty, DeprecationMessage = "Use DirectionalInputBuffer.HasValidInput() instead."))
	bool bDirectionalInputConsumed = false;

	/**
	 * Directional input buffer - Captures direction at KEY MOMENTS only (hold release)
	 *
	 * ARCHITECTURAL FIX: Separates movement input (continuous) from attack input (discrete).
	 * Direction is sampled ONLY when player releases attack button after hold completion.
	 * This prevents movement stick deflection during normal combos from triggering directionals.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Directional Input")
	FDirectionalInputBuffer DirectionalInputBuffer;

	/**
	 * Current input interpretation context
	 *
	 * Determines how movement stick input is interpreted:
	 * - Movement: Stick = character movement ONLY (ignore for attacks)
	 * - DirectionalInput: Stick = directional attack input (during hold release window)
	 * - Disabled: No input processing
	 *
	 * Context switches automatically based on combat state (hold windows, phases, etc.)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Directional Input")
	EInputContext CurrentInputContext = EInputContext::Movement;

	// ============================================================================
	// CONTEXT TRACKING (Context-Aware Resolution)
	// ============================================================================

	/**
	 * Active runtime context tags (e.g., ParryCounter, LowHealthFinisher)
	 * Used by ResolveNextAttack for context-sensitive attack resolution
	 * Updated dynamically based on combat events (parry success, health thresholds, etc.)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Context")
	FGameplayTagContainer ActiveContextTags;

	/**
	 * Visited attacks during current resolution (cycle detection)
	 * Cleared at start of each resolution, prevents infinite loops
	 * Passed to ResolveNextAttack by reference
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Context")
	TSet<UAttackData*> VisitedAttacks;

	/**
	 * Maximum chain depth for combo resolution (safety limit)
	 * Prevents stack overflow from malformed attack data
	 * Default: 10 attacks per chain
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Context")
	int32 MaxChainDepth = 10;

	// ============================================================================
	// DEBUG VISUALIZATION TESTING (WITH_AUTOMATION_TESTS only)
	// ============================================================================

#if WITH_AUTOMATION_TESTS
	/**
	 * Calculate all debug visualization data without drawing
	 * Allows unit tests to verify positioning, coloring, and visibility logic
	 *
	 * @param CameraRotation - Camera/controller rotation
	 * @param CharacterRotation - Character actor rotation
	 * @param CameraRelativeInput - Raw input vector
	 * @param ResolvedDirection - Direction after transformation
	 * @return Complete visualization data for testing
	 */
	FDebugVisualizationData CalculateDebugVisualizationData(
		const FRotator& CameraRotation,
		const FRotator& CharacterRotation,
		const FVector2D& CameraRelativeInput,
		EInputDirection ResolvedDirection) const;

	/**
	 * Get current phase debug color
	 * @return Color based on current phase (Windup=Orange, Active=Red, Recovery=Yellow, None=White)
	 */
	FColor GetPhaseDebugColor() const;

	/**
	 * Should input arrow be drawn as dashed?
	 * @return True if hold-release input (dashed), False if continuous input (solid)
	 */
	bool ShouldUseDashedArrowForInput() const;

	// Friend declarations for test classes
	friend class FDebugLabelPositionTest;
	friend class FDebugArrowPositionTest;
	friend class FDebugHoldStateVisualizationTest;
	friend class FDebugPhaseColorTest;
	friend class FDebugQueueVisualizationTest;
	friend class FDebugArrowLengthTest;
	friend class FDebugChestHeightTest;
#endif // WITH_AUTOMATION_TESTS

protected:
	virtual void BeginPlay() override;

	/**
	 * Validates that default attacks are assigned (called in BeginPlay in editor builds)
	 * Shows on-screen warnings if defaults are missing
	 * Critical for graceful fallback system to work properly
	 */
	void ValidateDefaultAttacks();

	/**
	 * Called when character dies to reset all combat state
	 * Prevents state leaks across respawns (hold state, queued actions, input context)
	 * Bind this to character's OnDeath event delegate
	 */
	UFUNCTION()
	void OnCharacterDeath();

	/**
	 * Set current input interpretation context
	 *
	 * Controls how movement stick input is interpreted:
	 * - Movement: Stick ignored for attack resolution (normal behavior)
	 * - DirectionalInput: Stick sampled for directional attacks (during hold release)
	 * - Disabled: No input processing
	 *
	 * Called automatically by hold window callbacks and phase transitions.
	 * Logs context changes for debugging.
	 */
	void SetInputContext(EInputContext NewContext);

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

	/** Current attack phase (tracked independently) */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	EAttackPhase CurrentPhase = EAttackPhase::None;

	/** Currently executing attack (for combo progression tracking) */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	TObjectPtr<UAttackData> CurrentAttackData = nullptr;

	/** Input type that triggered current attack (Light/Heavy) */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	EInputType CurrentAttackInputType = EInputType::None;

	/** Timer handle for light attack ease transition (timer-based, NOT tick-based) */
	FTimerHandle EaseTimerHandle;

	/** Is character movement currently disabled? (for procedural sync) */
	bool bMovementCurrentlyDisabled = false;

	/** Is currently in combo blend transition? (prevents premature phase reset) */
	bool bInComboBlend = false;

	/** World time when current blend transition will complete (for rapid input detection) */
	float BlendTransitionEndTime = 0.0f;

	/** Was current attack triggered by directional follow-up? (prevents infinite directional loops) */
	bool bCurrentAttackIsDirectionalFollowUp = false;

	// ============================================================================
	// INTERNAL HELPERS
	// ============================================================================

	/** Timer callback for procedural ease transitions (timer-based, NOT tick-based) */
	void OnEaseTimerTick();

	/**
	 * Procedurally update movement state based on montage/hold state
	 * Called from: TickComponent, PlayAttackMontage, OnEaseTimerTick
	 * Ensures movement is always synced with animation state
	 */
	void UpdateMovementFromMontageState();

	/**
	 * Clear hold state completely (ease timer, flags, movement)
	 * Called when starting new attack or on montage end
	 */
	void ClearHoldState();

	/**
	 * Setup motion warp for attack based on context
	 * Uses soft aim assist to find best target in direction - if found, uses translation+rotation warp
	 * If no target found, uses rotation-only warp toward input direction
	 * @param AttackData - Attack being executed (contains WarpConfig)
	 */
	void SetupAttackWarp(UAttackData* AttackData);

	/** Match press/release pairs */
	void ProcessInputPair(const FQueuedInputAction& PressEvent, const FQueuedInputAction& ReleaseEvent);

	/** Determine execution mode for input */
	EActionExecutionMode DetermineExecutionMode(const FQueuedInputAction& InputAction) const;

	/**
	 * Get attack data for input type
	 *
	 * NOTE: Not const because it may mutate DirectionalInputBuffer (clearing consumed direction)
	 * This is intentional behavior - buffer consumption is part of attack resolution
	 */
	UAttackData* GetAttackForInput(EInputType InputType);

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
