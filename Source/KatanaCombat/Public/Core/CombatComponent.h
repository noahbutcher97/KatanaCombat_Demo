// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Containers/Ticker.h"
#include "ActionQueueTypes.h"
#include "CombatTypes.h"
#include "Data/PairedAnimationTypes.h"
#include "Data/ProceduralAnimationTypes.h"
#include "Debug/DefenseTelemetry.h"
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
class UPairedAnimationData;
class UPairedAnimationComponent;
class UDefenseConfiguration;

USTRUCT()
struct FDefenseInteractionCacheRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FDefenseInteractionId Id;

	UPROPERTY()
	FDefenseContactReceipt Receipt;

	UPROPERTY()
	bool bFinalized = false;

	UPROPERTY()
	bool bSourceTerminal = false;

	UPROPERTY()
	double TerminalUnscaledTime = 0.0;

	UPROPERTY()
	uint64 TerminalSequence = 0;
};

// ============================================================================
// DEBUG VISUALIZATION TESTING SUPPORT
// ============================================================================

UCLASS(Blueprintable, ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class KATANACOMBAT_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

	// Test access for INPUT-1 race condition tests
	friend class FComboRace_GuardPreventsStateClear;
	friend class FComboRace_ComboChainDataIntegrity;
	friend class FComboRace_RevertOnFailure;
	friend class FComboRace_AttackDataSetBeforePlay;
	friend class FComboRace_AttackDataNotNullAfterExecute;
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

	/** Current attack-state generation used for native contact identity validation. */
	int32 GetCurrentAttackGeneration() const { return AttackStateMachine.AttackGeneration; }

	/** Immutable process-local identity used as the deterministic threat tie-break. */
	FCombatantStableId GetCombatantStableId() const { return CombatantStableId; }

	/** Append one diagnostic observation to this combatant's bounded telemetry ring. */
	void AppendDefenseTelemetry(FDefenseTelemetryRecord Record);
	const TArray<FDefenseTelemetryRecord>& GetDefenseTelemetry() const { return DefenseTelemetryRecords; }
	void ClearDefenseTelemetry();
	static constexpr int32 GetDefenseTelemetryCapacity() { return DefenseTelemetryCapacity; }

	/** Build a value snapshot of the currently published attack state. */
	FAttackExecutionSnapshot BuildAttackExecutionSnapshot() const;

	/** Open a canonical attacker-owned window for this exact attack and notify runtime instance. */
	FAttackWindowInstanceId OpenAttackWindow(
		EAttackWindowKind Kind,
		const FAnimNotifyRuntimeSourceId& NotifySource,
		int32 MontageInstanceId,
		float Duration);

	/** Refresh one open window's runtime deadline without changing its canonical generation. */
	FAttackWindowInstanceId RefreshAttackWindow(
		EAttackWindowKind Kind,
		const FAnimNotifyRuntimeSourceId& NotifySource,
		int32 MontageInstanceId,
		float RemainingDuration);

	/** Retire the oldest matching Begin; closes the published window only when that Begin is current. */
	bool CloseAttackWindow(
		EAttackWindowKind Kind,
		const FAnimNotifyRuntimeSourceId& NotifySource,
		int32 MontageInstanceId);

	/** Return the currently published canonical window of this kind. */
	FAttackWindowInstanceId GetActiveAttackWindow(EAttackWindowKind Kind) const;

	/** Atomically consume the current matching attack generation. */
	bool ConsumeActiveAttack(
		const FAttackInstanceId& AttackId,
		EAttackConsumeReason Reason);

	/** Immediately retire only the exact current attack generation. */
	bool AbortActiveAttack(const FAttackInstanceId& AttackId);

	bool IsAttackConsumed(const FAttackInstanceId& AttackId) const
	{
		return AttackId.IsValid() && ConsumedAttackInstance == AttackId;
	}

	/** Publish prediction evidence for the current attack generation. */
	void PublishAttackThreatPrediction(const FAttackThreatPrediction& Prediction);

	/** Publish high-confidence evidence from an authored runtime window and explicit attack target. */
	bool PublishReviewedAttackWindowPrediction(const FAttackWindowInstanceId& Window);

	/** Invalidate prediction evidence without mutating the active attack. */
	void InvalidateAttackThreatPrediction(EThreatInvalidationReason Reason);

	/** Capture the explicit defender intended by the active or pending attack. */
	void SetAttackIntentTarget(AActor* IntendedTarget);

	/** Enumerate and deterministically select one immutable defense threat snapshot. */
	FDefenseThreatSelectionResult SelectDefenseThreat(double SimulationNow);

	/** Refresh the held-guard threat lock, coalescing event requests within one frame. */
	void RefreshGuardThreat(EThreatRefreshReason Reason);

	/** Release held-guard threat ownership and its simulation-time refresh timer. */
	void ClearGuardThreat(EThreatClearReason Reason);

	/** Route normalized player yaw intent through the capped held-guard alignment request. */
	void SetDefenseManualYawInput(float NormalizedYawInput);

	/** Resolve stance, component, character-settings, then C++ default defense configuration. */
	const UDefenseConfiguration* GetEffectiveDefenseConfiguration() const;

	/** Install a scoped stance override. The newest active override has highest precedence. */
	FDefenseConfigurationOverrideHandle AcquireDefenseStanceOverride(UDefenseConfiguration* Configuration);

	/** Release only the scoped stance override represented by Handle. */
	bool ReleaseDefenseStanceOverride(FDefenseConfigurationOverrideHandle Handle);

	/** Last immutable threat selected for this defender, if still locked. */
	const FAttackExecutionSnapshot& GetLockedDefenseThreat() const { return LockedDefenseThreat; }

#if WITH_AUTOMATION_TESTS
	void SetCombatantStableIdForTesting(FCombatantStableId StableId) { CombatantStableId = StableId; }
	void SetAttackMontagePlayRateForTesting(float PlayRate)
	{
		AttackMontagePlayRateForTesting = FMath::Max(UE_SMALL_NUMBER, PlayRate);
	}
	float GetAttackMontagePlayRateForTesting() const
	{
		return AttackMontagePlayRateForTesting;
	}
	void SetDefenseManualYawInputForTesting(float NormalizedYawInput, double UnscaledNow);
	void SeedAttackWindowStateForTesting(UAttackData* Attack, EAttackPhase Phase, int32 Generation)
	{
		CurrentAttackData = Attack;
		CurrentPhase = Phase;
		AttackStateMachine.AttackGeneration = Generation;
	}
	const FDefenseResolution& GetLastInputDefenseResolutionForTesting() const
	{
		return LastInputDefenseResolution;
	}
	int32 GetPendingAttackConsumedEventCountForTesting() const
	{
		return PendingAttackConsumedEvents.Num();
	}
	int32 GetClearQueueCallCountForTesting() const
	{
		return ClearQueueCallCountForTesting;
	}
#endif

	// ============================================================================
	// CACHED REFERENCES
	// ============================================================================

	/** Cached owner character (for performance - avoids repeated casts) */
	UPROPERTY()
	TObjectPtr<ABaseCombatCharacter> OwnerCharacter = nullptr;

	/** Combat settings (cached from character for performance) */
	UPROPERTY()
	TObjectPtr<UCombatSettings> CombatSettings = nullptr;

	/** Defender-local configuration seam; Task 3 adds stance/settings precedence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Defense")
	TObjectPtr<UDefenseConfiguration> DefenseConfigurationOverride = nullptr;

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

	/** Begin sustained normal blocking when Block is held and no counter/parry consumed the input. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Block")
	bool BeginBlock(AActor* ThreatActor = nullptr);

	/** End sustained normal blocking when Block is released or combat state is cleared. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Block")
	void EndBlock();

	/** Is the owning character currently holding a normal block? */
	UFUNCTION(BlueprintPure, Category = "Combat|Block")
	bool IsBlocking() const { return bIsBlocking; }

	/** True when the held block should mitigate an incoming attack from this attacker. */
	UFUNCTION(BlueprintPure, Category = "Combat|Block")
	bool CanBlockAttackFrom(AActor* Attacker) const;

	/** True when the held block should mitigate this concrete incoming hit. */
	UFUNCTION(BlueprintPure, Category = "Combat|Block")
	bool CanBlockHit(const FHitReactionInfo& HitInfo) const;

	/** Native guard snapshot used by the rich defense resolver. */
	bool IsGuardHeldForDefense() const { return bIsBlocking; }

	// ============================================================================
	// DEFENSE INTERACTION COMMIT CACHE
	// ============================================================================

	EDefenseCommitStatus BeginDefenseInteraction(
		const FDefenseInteractionKey& Key,
		FDefenseInteractionId& OutId,
		FDefenseContactReceipt& OutExistingReceipt,
		bool bAllowNewRegistration = true);

	void FinalizeDefenseInteraction(
		const FDefenseInteractionId& Id,
		const FDefenseContactReceipt& Receipt);

	/** True only while this exact target-owned epoch remains finalized in the cache. */
	bool IsDefenseInteractionFinalized(const FDefenseInteractionId& Id) const;

	void MarkDefenseContactSourceTerminal(
		const FContactInstanceId& ContactId,
		double UnscaledNow);

	void SweepDefenseInteractionCache(double UnscaledNow);

	/** Broadcast only after gameplay commit and source accounting are coherent. */
	FOnDefenseResolvedNative OnDefenseResolvedNative;

	/** Immediate source-side termination signal for AI and other native ownership systems. */
	FOnAttackConsumedNative OnAttackConsumedInternal;

	/** Add an active runtime context tag for C++ attack-resolution code. */
	void AddActiveContextTag(FGameplayTag ContextTag);

	/** Remove an active runtime context tag for C++ attack-resolution code. */
	void RemoveActiveContextTag(FGameplayTag ContextTag);

	/** Clear all active runtime context tags. */
	void ClearActiveContextTags();

	/** Acquire one independently releasable contribution to a runtime context tag. */
	FCombatContextLeaseHandle AcquireContextTagLease(FGameplayTag ContextTag, FName Owner);

	/** Release only the contribution represented by Handle. Duplicate release is a no-op. */
	void ReleaseContextTagLease(FCombatContextLeaseHandle Handle);

	/** True when this component currently has the supplied runtime context tag. */
	UFUNCTION(BlueprintPure, Category = "Combat|Context")
	bool HasActiveContextTag(FGameplayTag ContextTag) const;

	/** Report the component-owned combat state for character interfaces and animation. */
	UFUNCTION(BlueprintPure, Category = "Combat|State")
	ECombatState GetCombatState() const;

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
	 * Execute an attack data asset through the normal CombatComponent path.
	 * Used by AI and other non-input callers that already selected an attack.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Queue")
	bool ExecuteAttackData(UAttackData* AttackData, AActor* ExplicitWarpTarget = nullptr, EInputType InputType = EInputType::LightAttack);

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

	/** Canonical phase path; invalid or stale runtime notify context cannot mutate phase or tracing. */
	bool OnPhaseTransitionWithContext(
		EAttackPhase NewPhase,
		const FAnimNotifyRuntimeSourceId& NotifySource,
		int32 MontageInstanceId,
		float RemainingWindowDuration);

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

	// ============================================================================
	// PAIRED ANIMATION COMPONENT FORWARDING WRAPPERS
	// ============================================================================
	// These forward to UPairedAnimationComponent for backward compatibility.
	// New code should access PairedAnimationComponent directly.
	// ============================================================================

	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	bool IsInCounterWindow() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	float GetCounterWindowProgress() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	const FCounterContext& GetCounterWindowData() const;

	void SetCounterWindowData(EAttackType InAttackType, ESwingDirection InSwingDirection,
							  UPairedAnimationData* InCounterData, float InWindowDuration);

	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	bool IsInParryWindow() const;

	void SetParryWindowActive(bool bActive);

	void ClearCounterWindowData();

	UFUNCTION(BlueprintCallable, Category = "Combat|Counter")
	bool TryCounter();

	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	bool CanCounter() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	AActor* FindCounterableEnemy() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	FCounterContext GetEnemyCounterContext(AActor* Enemy) const;

	UFUNCTION(BlueprintPure, Category = "Combat|Counter")
	AActor* FindParryableEnemy() const;

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

	/** Fires when this character's weapon hits a target (for audio, VFX, UI responses) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnAttackHit OnAttackHit;

	/** Deferred public notification after source-side attack consumption is coherent. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnAttackConsumed OnAttackConsumed;

	// Paired Animation forwarding wrappers (delegates to UPairedAnimationComponent)

	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void AddPairedPartner(AActor* Partner);

	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void RemovePairedPartner(AActor* Partner);

	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void ClearPairedPartners();

	UFUNCTION(BlueprintPure, Category = "Combat|Paired Animation")
	bool IsPairedPartner(AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category = "Combat|Paired Animation")
	int32 GetPairedPartnerCount() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Paired Animation")
	bool IsInputBlocked() const;

	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void BeginPairedAnimation(UPairedAnimationData* PairedAnimData, EPairedReactionType ReactionType, bool bIsCriticalMoment = true);

	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void EndPairedAnimation();

	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void TriggerSyncPointEffects(FName SyncPointName);

	UFUNCTION(BlueprintPure, Category = "Combat|Paired Animation")
	bool IsPairedAnimationActive() const;

	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void OnPairedPartnerDeath(AActor* DeadPartner);

	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void CancelPairedAnimation(float BlendOutTime = 0.1f);

	UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
	void CompletePairedAnimation();

	// ============================================================================
	// DEBUG / VISUALIZATION
	// ============================================================================

	/** Draw queue state and checkpoints */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void DrawDebugInfo() const;

	/** Get queue statistics */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	FQueueStats GetQueueStats() const { return QueueStats; }

	/** Bounded chronological ledger of captured combat-input edges. */
	const TArray<FCombatInputRecord>& GetCombatInputHistory() const { return CombatInputHistory; }

	/** Reset statistics */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void ResetStats() { QueueStats.Reset(); }

	// ============================================================================
	// PUBLIC STATE (for debug visualization)
	// ============================================================================

	/** Action queue (FIFO execution) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	TArray<FActionQueueEntry> ActionQueue;

#if WITH_AUTOMATION_TESTS
	int32 ClearQueueCallCountForTesting = 0;
	float AttackMontagePlayRateForTesting = 1.0f;
#endif

	/** Timer checkpoints for current montage */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	TArray<FTimerCheckpoint> Checkpoints;

	/** Hold state (persists across combos) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	FHoldState HoldState;

	/** Whether movement is currently disabled due to hold freeze */
	bool bMovementCurrentlyDisabled = false;

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
	 * Used by ResolveNextAttack for context-sensitive attack resolution.
	 * Mutated through C++ helpers; gameplay-event producers require explicit lifecycle ownership.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Context")
	FGameplayTagContainer ActiveContextTags;

	struct FCombatContextLeaseRecord
	{
		FGameplayTag Tag;
		FName Owner = NAME_None;
	};

	TMap<FCombatContextLeaseHandle, FCombatContextLeaseRecord> ActiveContextTagLeases;
	TMap<FGameplayTag, int32> ActiveContextTagLeaseCounts;
	TMap<FGameplayTag, TArray<FCombatContextLeaseHandle>> LegacyContextTagLeases;
	uint64 NextContextTagLeaseId = 0;

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
	int32 GetDefenseInteractionCacheSizeForTesting() const
	{
		return DefenseInteractionCache.Num();
	}

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
	friend class FComboRace_PendingTransitionsStartZero;
	friend class FComboRace_ComboChainDataIntegrity;
	friend class FComboRace_RevertOnFailure;
	friend class FComboRace_SetPhaseNoneClearsState;
	friend class FDefenseThreat_AttackSnapshotPublication;
	friend class FDefenseThreat_HighConfidenceRequiresCompleteEvidence;
	friend class FDefenseThreat_ComponentSelectionOwnership;
	friend class FDefenseAlignment_GuardUsesOwnedSmoothRequest;
	friend class FDefenseAlignment_GuardManualOverridePreservesBudget;
	friend class FDefenseAlignment_GuardManualThresholdAndPriority;
	friend class FDefenseAlignment_PlayerLookRoutesManualYaw;
#endif // WITH_AUTOMATION_TESTS

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void EnsureCombatantStableId();

	/** Active scoped stance overrides, retained for GC until their owner releases the handle. */
	UPROPERTY(Transient)
	TMap<uint64, TObjectPtr<UDefenseConfiguration>> DefenseStanceOverrides;

	uint64 NextDefenseStanceOverrideId = 1;

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
	void OnCharacterDeath(AActor* Killer);

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

	/** Last 64 captured input edges, including terminal rejections. */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	TArray<FCombatInputRecord> CombatInputHistory;

	/** Process-monotonic identity for the next captured input edge. */
	uint64 NextCombatInputSerial = 1;

	/** Current attack phase (tracked independently) */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	EAttackPhase CurrentPhase = EAttackPhase::None;

	/** Currently executing attack (for combo progression tracking) */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	TObjectPtr<UAttackData> CurrentAttackData = nullptr;

	/** Process-local deterministic identity assigned when the component registers. */
	FCombatantStableId CombatantStableId;

	/** Explicit defender captured by attack selection, never inferred during defense query. */
	UPROPERTY()
	TWeakObjectPtr<AActor> AttackIntentTarget;

	/** Latest prediction evidence, bound to PublishedPredictionAttackInstance. */
	UPROPERTY()
	FAttackThreatPrediction PublishedAttackThreatPrediction;

	UPROPERTY()
	FAttackInstanceId PublishedPredictionAttackInstance;

	/** All unmatched canonical Begin records, retained so delayed End callbacks retire FIFO. */
	UPROPERTY(Transient)
	TArray<FAttackWindowInstanceId> OpenAttackWindowRecords;

	UPROPERTY(Transient)
	FAttackWindowInstanceId ActiveHitWindow;

	UPROPERTY(Transient)
	FAttackWindowInstanceId ActiveParryWindow;

	UPROPERTY(Transient)
	FAttackWindowInstanceId ActiveCounterWindow;

	int32 NextAttackWindowGeneration = 0;

	UPROPERTY(Transient)
	FAttackInstanceId ConsumedAttackInstance;

	UPROPERTY(Transient)
	TArray<FAttackConsumedEvent> PendingAttackConsumedEvents;

	FTSTicker::FDelegateHandle DeferredAttackConsumedTickerHandle;
	bool bConsumedPendingPresentation = false;

	UPROPERTY(Transient)
	FDefenseResolution LastInputDefenseResolution;

	EThreatInvalidationReason LastThreatInvalidationReason = EThreatInvalidationReason::None;

	/** Defender-owned immutable lock selected from one targeting enumeration. */
	UPROPERTY()
	FAttackExecutionSnapshot LockedDefenseThreat;

	UPROPERTY()
	TWeakObjectPtr<AActor> LockedDefenseThreatActor;

	FCombatantStableId LockedDefenseThreatId;
	double DefenseThreatLockAcquiredSimulationTime = -1.0;
	float RemainingDefenseAutomaticTurn = 0.0f;
	FAlignmentRequestHandle GuardAlignmentRequestHandle;
	int32 GuardAlignmentGeneration = 0;
	bool bGuardThreatCandidatesExist = false;
	bool bGuardThreatRefreshInProgress = false;
	uint64 LastGuardThreatRefreshFrame = MAX_uint64;
	FTimerHandle GuardThreatRefreshTimerHandle;
	FTimerHandle CoalescedGuardThreatRefreshTimerHandle;
	FTSTicker::FDelegateHandle GuardManualResumeTickerHandle;
	float DefenseManualYawInput = 0.0f;
	double GuardManualInputBelowThresholdRealTime = -1.0;
	bool bGuardManualOverrideActive = false;

	/** Input type that triggered current attack (Light/Heavy) */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	EInputType CurrentAttackInputType = EInputType::None;

	/** True while the Block input is held and not consumed by a parry/counter. */
	UPROPERTY(VisibleAnywhere, Category = "Combat|State")
	bool bIsBlocking = false;

	/** Half-angle of the normal block defensive cone. */
	UPROPERTY(EditAnywhere, Category = "Combat|Block", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float BlockFacingConeHalfAngle = 70.0f;

	UPROPERTY(Transient)
	TMap<FDefenseInteractionKey, FDefenseInteractionCacheRecord> DefenseInteractionCache;
	uint64 NextDefenseInteractionEpoch = 0;
	TArray<FDefenseTelemetryRecord> DefenseTelemetryRecords;
	uint64 NextDefenseTelemetrySequence = 0;
	static constexpr int32 DefenseTelemetryCapacity = 512;
	uint64 NextDefenseTerminalSequence = 0;
	static constexpr double DefenseInteractionTombstoneSeconds = 1.0;
	static constexpr int32 DefenseTerminalInteractionCacheCap = 128;

	/** Optional target supplied by external attack execution, used before soft-aim fallback. */
	TWeakObjectPtr<AActor> ExplicitAttackWarpTarget;

	/** Timer handle for light attack ease transition (timer-based, NOT tick-based) */
	FTimerHandle EaseTimerHandle;

	/** Cached reference to PairedAnimationComponent (for forwarding wrappers) */
	UPROPERTY()
	TObjectPtr<UPairedAnimationComponent> CachedPairedAnimComp = nullptr;

	// ========================================================================
	// ATTACK STATE MACHINE (replaces scattered flags)
	// ========================================================================

	/**
	 * Centralized state machine for attack lifecycle management
	 * Tracks owner montage, combo blends, and filters stale callbacks
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Debug")
	FAttackStateMachine AttackStateMachine;

	// DEPRECATED: These flags are kept for backwards compatibility during transition
	// The state machine is now the source of truth - use AttackStateMachine.IsComboBlending() instead
	/** @deprecated Use AttackStateMachine.IsComboBlending() instead */
	bool bInComboBlend = false;

	/** @deprecated Use AttackStateMachine.ComboBlendEndTime instead */
	float BlendTransitionEndTime = 0.0f;

	/** Was current attack triggered by directional follow-up? (prevents infinite directional loops) */
	bool bCurrentAttackIsDirectionalFollowUp = false;

	/**
	 * Procedural blend configuration for combo transitions (BUG-2 FIX).
	 * Replaces per-attack ComboBlendInTime/ComboBlendOutTime with dynamic calculation.
	 * Blend time is calculated based on animation progress: near end = fast blend, mid-animation = slow blend.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
	FProceduralBlendConfig ProceduralBlendConfig;

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

	/** Debug: Last checkpoint count for DrawDebugInfo (per-instance, not static) */
	mutable int32 DebugLastCheckpointCount = 0;

	/**
	 * Setup motion warp for attack based on context
	 * Uses soft aim assist to find best target in direction - if found, uses translation+rotation warp
	 * If no target found, uses rotation-only warp toward input direction
	 * @param AttackData - Attack being executed (contains WarpConfig)
	 */
	void SetupAttackWarp(UAttackData* AttackData);

	/** Simulation-time timer callback used only while held guard has candidates. */
	void HandleGuardThreatRefreshTimer();
	void HandleCoalescedGuardThreatRefresh();

	void RefreshGuardThreatInternal(EThreatRefreshReason Reason, bool bForceRevalidation);
	void UpdateGuardAlignmentRequest();
	void SetDefenseManualYawInputAtTime(float NormalizedYawInput, double UnscaledNow);
	void ScheduleGuardManualResume(double DelaySeconds);
	void CancelGuardManualResume();
	bool HandleGuardManualResumeTicker(float DeltaTime);
	void ResetDefenseManualYawOverride();
	bool TryCommitPerfectParry(double BlockPressSimulationTime, double BlockPressUnscaledTime);
	FDefenseQuery BuildDefenseInputQuery(
		double BlockPressSimulationTime,
		double BlockPressUnscaledTime) const;
	bool ConsumeActiveAttackInternal(
		const FAttackInstanceId& AttackId,
		EAttackConsumeReason Reason,
		const FDefenseInteractionId& InteractionId);
	bool FlushDeferredAttackConsumedEvents(float DeltaTime);
	bool HasRegisteredDefenseContactForAttack(const FAttackInstanceId& AttackId) const;
	bool CloseHitWindowFromPhaseTransition(
		const FAnimNotifyRuntimeSourceId& CloseSource,
		int32 MontageInstanceId);
	void ClearPublishedAttackWindowsForAttack(const FAttackInstanceId& AttackInstance);

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

	/** Capture an input edge before any routing or eligibility gate. */
	uint64 CaptureCombatInput(
		EInputType InputType,
		EInputEventType EventType,
		EInputDirection InputDirection);

	/** Finalize a retained record by identity; safe if reentrant input evicted it. */
	void FinalizeCombatInput(
		uint64 Serial,
		ECombatInputRoute Route,
		ECombatInputDisposition Disposition);

	/** Queue implementation that reports whether the normal route accepted the edge. */
	bool TryQueueAction(const FQueuedInputAction& InputAction, UAttackData* AttackData = nullptr);

};
