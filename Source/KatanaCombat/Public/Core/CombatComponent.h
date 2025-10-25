// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "CombatComponent.generated.h"

// Forward declarations
class UAttackData;
class UCombatSettings;
class UAnimInstance;
class UAnimMontage;
class UTargetingComponent;
class UWeaponComponent;
class UMotionWarpingComponent;
class ACharacter;

/**
 * Main combat component handling state machine, attacks, posture, combos, and parry/counter mechanics
 * This is the core of the combat system - consolidates tightly coupled combat flow logic
 * 
 * Responsibilities:
 * - State machine (10 combat states)
 * - Attack execution and timing
 * - Posture system (regen, depletion, guard break)
 * - Combo tracking and branching (hybrid responsive + snappy system)
 * - Parry/counter windows
 * - Input handling and buffering
 * 
 * This component intentionally consolidates related functionality rather than fragmenting
 * into multiple smaller components, as the combat flow logic is tightly coupled.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class KATANACOMBAT_API UCombatComponent : public UActorComponent
{
    GENERATED_BODY()

#if WITH_AUTOMATION_TESTS
    // Friend declarations for test classes
    friend class FStateTransitionTest;
    friend class FInputBufferingTest;
    friend class FHoldWindowTest;
    friend class FParryDetectionTest;
    friend class FAttackExecutionTest;
    friend class FMemorySafetyTest;
    friend class FPhasesVsWindowsTest;
#endif

public:
    UCombatComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ============================================================================
    // CONFIGURATION
    // ============================================================================

    /** Global combat settings (posture rates, timing windows, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Settings")
    TObjectPtr<UCombatSettings> CombatSettings;

    /** Default light attack to use when no combo is active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Settings")
    TObjectPtr<UAttackData> DefaultLightAttack;

    /** Default heavy attack to use when no combo is active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Settings")
    TObjectPtr<UAttackData> DefaultHeavyAttack;

    /** Enable debug visualization (state changes, timing windows, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
    bool bDebugDraw = false;

    // ============================================================================
    // STATE MACHINE
    // ============================================================================

    /** Get current combat state */
    UFUNCTION(BlueprintPure, Category = "Combat|State")
    ECombatState GetCombatState() const { return CurrentState; }

    /** Get current attack phase */
    UFUNCTION(BlueprintPure, Category = "Combat|State")
    EAttackPhase GetCurrentPhase() const { return CurrentPhase; }

    /**
     * Check if can transition to a new state
     * @param NewState - State to check
     * @return True if transition is valid
     */
    UFUNCTION(BlueprintPure, Category = "Combat|State")
    bool CanTransitionTo(ECombatState NewState) const;

    /**
     * Force state change (use carefully - prefer input functions)
     * @param NewState - State to transition to
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|State")
    void SetCombatState(ECombatState NewState);

    /** Is currently in any attack state? */
    UFUNCTION(BlueprintPure, Category = "Combat|State")
    bool IsAttacking() const;

    /**
     * Is currently holding an attack? (frozen at 0.0 playrate)
     * Used by AnimInstance to prevent locomotion updates during hold state
     * @return True if in hold state
     */
    UFUNCTION(BlueprintPure, Category = "Combat|State")
    bool IsHolding() const { return bIsHolding; }

    // ============================================================================
    // ATTACK EXECUTION
    // ============================================================================

    /**
     * Execute an attack
     * @param AttackData - Attack to perform
     * @return True if attack was successfully started
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
    bool ExecuteAttack(UAttackData* AttackData);

    /**
     * Can perform an attack right now?
     * @return True if able to attack
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Attack")
    bool CanAttack() const;

    /**
     * Get currently executing attack
     * @return Current attack data, or nullptr if not attacking
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Attack")
    UAttackData* GetCurrentAttack() const { return CurrentAttackData; }

    /** Stop current attack immediately */
    UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
    void StopCurrentAttack();

    // ============================================================================
    // COMBO SYSTEM
    // ============================================================================

    /**
     * Get current combo count
     * @return Number of attacks in current combo chain
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Combo")
    int32 GetComboCount() const { return ComboCount; }

    /**
     * Can input next combo?
     * @return True if in combo input window
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Combo")
    bool CanCombo() const { return bCanCombo; }

    /** Reset combo chain to start */
    UFUNCTION(BlueprintCallable, Category = "Combat|Combo")
    void ResetCombo();

    /**
     * Open combo input window
     * Called by AnimNotifyState_ComboWindow
     * @param Duration - How long window stays open
     */
    void OpenComboWindow(float Duration);

    /** Close combo input window */
    void CloseComboWindow();

    // ============================================================================
    // POSTURE SYSTEM
    // ============================================================================

    /**
     * Get current posture (0-100)
     * @return Current posture value
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Posture")
    float GetCurrentPosture() const { return CurrentPosture; }

    /**
     * Get max posture
     * @return Maximum posture value
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Posture")
    float GetMaxPosture() const;

    /**
     * Get posture as percentage (0-1)
     * @return Posture percent (0 = broken, 1 = full)
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Posture")
    float GetPosturePercent() const;

    /**
     * Apply posture damage (when blocking)
     * @param Amount - Posture to remove
     * @return True if guard was broken
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Posture")
    bool ApplyPostureDamage(float Amount);

    /**
     * Is guard currently broken?
     * @return True if posture depleted
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Posture")
    bool IsGuardBroken() const { return CurrentState == ECombatState::GuardBroken; }

    /** Trigger guard break state manually */
    UFUNCTION(BlueprintCallable, Category = "Combat|Posture")
    void TriggerGuardBreak();

    // ============================================================================
    // BLOCKING & PARRY
    // ============================================================================

    /**
     * Is currently blocking?
     * @return True if in blocking state
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Defense")
    bool IsBlocking() const { return CurrentState == ECombatState::Blocking; }

    /**
     * Can block right now?
     * @return True if able to block
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Defense")
    bool CanBlock() const;

    /** Start blocking */
    UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
    void StartBlocking();

    /** Stop blocking */
    UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
    void StopBlocking();

    /**
     * Attempt perfect parry (call during parry window)
     * Defender-side detection: Checks nearby attackers for IsInParryWindow()
     * @return True if parry was successful
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
    bool TryParry();

    // ============================================================================
    // PARRY WINDOWS
    // ============================================================================

    /**
     * Is currently in parry window? (ATTACKER's state)
     * Defender checks this on nearby attackers to determine if parry is possible
     * @return True if this character is vulnerable to being parried
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Defense")
    bool IsInParryWindow() const { return bIsInParryWindow; }

    /**
     * Open parry window (mark attacker as vulnerable to parry)
     * Called by AnimNotifyState_ParryWindow on ATTACKER's montage
     * @param Duration - How long window stays open
     */
    void OpenParryWindow(float Duration);

    /** Close parry window */
    void CloseParryWindow();

    // ============================================================================
    // HOLD WINDOWS
    // ============================================================================

    /**
     * Is currently in hold window?
     * @return True if in window where held button can trigger hold state
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Hold")
    bool IsInHoldWindow() const { return bIsInHoldWindow; }

    /**
     * Open hold window (check if attack button is still held)
     * Called by AnimNotifyState_HoldWindow in light attack montages
     * @param Duration - How long window stays open
     */
    void OpenHoldWindow(float Duration);

    /** Close hold window */
    void CloseHoldWindow();

    // ============================================================================
    // COUNTER WINDOWS
    // ============================================================================

    /**
     * Is currently in counter window?
     * @return True if vulnerable to counter attacks
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Counter")
    bool IsInCounterWindow() const { return bIsInCounterWindow; }

    /**
     * Open counter window (enemy is vulnerable)
     * @param Duration - How long window stays open
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Counter")
    void OpenCounterWindow(float Duration);

    /** Close counter window */
    UFUNCTION(BlueprintCallable, Category = "Combat|Counter")
    void CloseCounterWindow();

    // ============================================================================
    // INPUT HANDLING
    // ============================================================================

    /** Store movement input for directional attacks */
    UFUNCTION(BlueprintCallable, Category = "Combat|Input")
    void SetMovementInput(FVector2D Input);

    /** Light attack button pressed */
    void OnLightAttackPressed();

    /** Light attack button released */
    void OnLightAttackReleased();

    /** Heavy attack button pressed */
    void OnHeavyAttackPressed();

    /** Heavy attack button released */
    void OnHeavyAttackReleased();

    /** Block button pressed */
    void OnBlockPressed();

    /** Block button released */
    void OnBlockReleased();

    /** Evade button pressed */
    void OnEvadePressed();

    // ============================================================================
    // ANIMATION CALLBACKS
    // ============================================================================

    /**
     * Called when attack phase begins (from AnimNotifyState)
     * @param Phase - Phase that is beginning
     */
    void OnAttackPhaseBegin(EAttackPhase Phase);

    /**
     * Called when attack phase ends (from AnimNotifyState)
     * @param Phase - Phase that is ending
     */
    void OnAttackPhaseEnd(EAttackPhase Phase);

    // ============================================================================
    // EVENTS
    // ============================================================================
    // NOTE: Delegate types are declared in CombatTypes.h (declared ONCE system-wide)
    // Components only use UPROPERTY to expose them

    /** Broadcast when combat state changes */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnCombatStateChanged OnCombatStateChanged;

    /** Broadcast when posture changes */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnPostureChanged OnPostureChanged;

    /** Broadcast when guard is broken */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnGuardBroken OnGuardBroken;

    /** Broadcast on successful perfect parry */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnPerfectParry OnPerfectParry;

    /** Broadcast on successful perfect evade */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnPerfectEvade OnPerfectEvade;

    /** Broadcast when attack hits a target */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnAttackHit OnAttackHit;

protected:
    virtual void BeginPlay() override;

private:
    // ============================================================================
    // STATE
    // ============================================================================

    /** Current combat state */
    UPROPERTY(VisibleAnywhere, Category = "Combat|State")
    ECombatState CurrentState = ECombatState::Idle;

    /** Current attack phase */
    UPROPERTY(VisibleAnywhere, Category = "Combat|State")
    EAttackPhase CurrentPhase = EAttackPhase::None;

    /** Currently executing attack */
    UPROPERTY()
    TObjectPtr<UAttackData> CurrentAttackData = nullptr;

    // ============================================================================
    // POSTURE
    // ============================================================================

    /** Current posture value (0-100) */
    UPROPERTY(VisibleAnywhere, Category = "Combat|Posture")
    float CurrentPosture = 100.0f;

    /** Timer for guard break recovery */
    FTimerHandle GuardBreakRecoveryTimer;

    // ============================================================================
    // COMBO SYSTEM - HYBRID (Responsive + Snappy)
    // ============================================================================

    /** Number of attacks in current combo */
    UPROPERTY(VisibleAnywhere, Category = "Combat|Combo")
    int32 ComboCount = 0;

    /** Can currently input next combo? (combo window is open) */
    UPROPERTY(VisibleAnywhere, Category = "Combat|Combo")
    bool bCanCombo = false;

    /** Timer for combo window */
    FTimerHandle ComboWindowTimer;

    /** Timer to reset combo after timeout */
    FTimerHandle ComboResetTimer;

    /** Queued combo inputs during combo window (snappy path) */
    TArray<EInputType> ComboInputBuffer;

    /** Flag: Did we queue a combo during combo window? */
    bool bHasQueuedCombo = false;

    /** Input type that initiated the current attack animation (persists throughout animation lifecycle for hold detection) */
    EInputType CurrentAttackInputType = EInputType::None;

    // ============================================================================
    // PARRY WINDOW
    // ============================================================================

    /** Is parry window currently active? (Attacker is vulnerable to being parried) */
    UPROPERTY(VisibleAnywhere, Category = "Combat|Parry")
    bool bIsInParryWindow = false;

    /** Timer to close parry window */
    FTimerHandle ParryWindowTimer;

    // ============================================================================
    // HOLD WINDOW
    // ============================================================================

    /** Is hold window currently active? */
    UPROPERTY(VisibleAnywhere, Category = "Combat|Hold")
    bool bIsInHoldWindow = false;

    /** Timer to close hold window */
    FTimerHandle HoldWindowTimer;

    // ============================================================================
    // COUNTER WINDOW
    // ============================================================================

    /** Is counter window currently active? */
    UPROPERTY(VisibleAnywhere, Category = "Combat|Counter")
    bool bIsInCounterWindow = false;

    /** Timer to close counter window */
    FTimerHandle CounterWindowTimer;

    // ============================================================================
    // INPUT BUFFERING - RESPONSIVE PATH
    // ============================================================================

    /** Buffered light attack input */
    bool bLightAttackBuffered = false;

    /** Buffered heavy attack input */
    bool bHeavyAttackBuffered = false;

    /** Buffered evade input */
    bool bEvadeBuffered = false;

    /** Was light attack buffered during combo window? */
    bool bLightAttackInComboWindow = false;

    /** Was heavy attack buffered during combo window? */
    bool bHeavyAttackInComboWindow = false;

    /** Is light attack button held? */
    bool bLightAttackHeld = false;

    /** Is heavy attack button held? */
    bool bHeavyAttackHeld = false;

    // ============================================================================
    // CHARGING (Heavy Attacks)
    // ============================================================================

    /** Currently charging heavy attack? */
    bool bIsCharging = false;

    /** Current charge time */
    float CurrentChargeTime = 0.0f;

    // ============================================================================
    // HOLD STATE (Light Attacks)
    // ============================================================================

    /** Currently holding light attack? */
    bool bIsHolding = false;

    /** Current hold time */
    float CurrentHoldTime = 0.0f;

    /** Did the hold window expire while button was still held? */
    bool bHoldWindowExpired = false;

    /** Directional input queued during hold window (sampled during hold, not on release) */
    EAttackDirection QueuedDirectionalInput = EAttackDirection::None;

    /** Playback rate blending for holds */
    bool bIsBlendingToHold = false;
    bool bIsBlendingFromHold = false;
    float HoldBlendAlpha = 0.0f;

public:

    UPROPERTY(EditAnywhere, Category = "Combat")
    float HoldBlendSpeed = 5.0f;  // Speed of blend (0 to 1 in ~0.2 seconds at default 5.0)

private:
    // ============================================================================
    // MOVEMENT INPUT
    // ============================================================================

    /** Stored movement input (X = Right, Y = Forward) for directional follow-ups */
    FVector2D StoredMovementInput = FVector2D::ZeroVector;

    // ============================================================================
    // CACHED REFERENCES
    // ============================================================================

    /** Owner character (cached for performance) */
    UPROPERTY()
    TObjectPtr<ACharacter> OwnerCharacter;

    /** Owner's anim instance (for montage playback) */
    UPROPERTY()
    TObjectPtr<UAnimInstance> AnimInstance;

    /** Targeting component (for finding enemies) */
    UPROPERTY()
    TObjectPtr<UTargetingComponent> TargetingComponent;

    /** Weapon component (for hit detection) */
    UPROPERTY()
    TObjectPtr<UWeaponComponent> WeaponComponent;

    /** Motion warping component (for chase attacks) */
    UPROPERTY()
    TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

    // ============================================================================
    // INTERNAL HELPERS - STATE & POSTURE
    // ============================================================================

    /** Update posture regeneration per frame */
    void UpdatePosture(float DeltaTime);

    /** Regenerate posture based on current state */
    void RegeneratePosture(float DeltaTime);

    /** Restore posture by amount */
    void RestorePosture(float Amount);

    /** Get current posture regen rate based on state */
    float GetCurrentPostureRegenRate() const;

    /** Trigger guard break state */
    void HandleGuardBreak();

    /** Handle guard break recovery after stun duration */
    void RecoverFromGuardBreak();

    /** Reset combo chain after timeout */
    void ResetComboChain();

    // ============================================================================
    // INTERNAL HELPERS - COMBO SYSTEM (Hybrid)
    // ============================================================================

    /** Queue combo input during combo window (SNAPPY PATH) */
    void QueueComboInput(EInputType InputType);

    /** Cancel recovery and execute combo immediately (SNAPPY PATH) */
    void CancelRecoveryAndExecuteCombo();

    /** Process queued combo inputs */
    void ProcessQueuedCombo();

    /** Process combo window inputs at end of Active phase */
    void ProcessComboWindowInput();

    /** Process recovery completion naturally (RESPONSIVE PATH) */
    void ProcessRecoveryComplete();

    /** Get combo attack data from input type */
    UAttackData* GetComboFromInput(EInputType InputType);

    /** Execute combo attack with hold tracking */
    void ExecuteComboAttackWithHoldTracking(UAttackData* NextAttack, EInputType InputType);

    /** Execute combo attack (wrapper) */
    void ExecuteComboAttack(UAttackData* NextAttack);

    /** Execute directional follow-up attack */
    void ExecuteDirectionalFollowUp(EAttackDirection Direction);

    // ============================================================================
    // INTERNAL HELPERS - INPUT & BUFFERING
    // ============================================================================

    /** Process all buffered inputs (non-combo inputs) */
    void ProcessBufferedInputs();

    /** Clear all input buffers */
    void ClearInputBuffers();

    /** Convert stored 2D input to a normalized world space direction */
    FVector GetWorldSpaceMovementInput() const;

    /** Convert a world-space direction to an attack direction relative to owner */
    EAttackDirection GetAttackDirectionFromWorldDirection(const FVector& WorldDir) const;

    // ============================================================================
    // INTERNAL HELPERS - ANIMATION & MOTION WARPING
    // ============================================================================

    /** Setup motion warping for attack */
    void SetupMotionWarping(UAttackData* AttackData);

    /** Play attack montage */
    bool PlayAttackMontage(UAttackData* AttackData);

    // ============================================================================
    // INTERNAL HELPERS - CHARGING & HOLDING
    // ============================================================================

    /** Start charging heavy attack */
    void StartChargingHeavy();

    /** Update heavy attack charging */
    void UpdateHeavyCharging(float DeltaTime);

    /** Release charged heavy attack */
    void ReleaseChargedHeavy();

    /** Update hold time (works for both light and heavy) */
    void UpdateHoldTime(float DeltaTime);

    /**
     * Release held light attack with directional input
     * @param bWasWindowExpired - Captured state of bHoldWindowExpired at moment of button release (prevents race condition)
     */
    void ReleaseHeldLight(bool bWasWindowExpired);

    /**
     * Release held heavy attack (play follow-through)
     * @param bWasWindowExpired - Captured state of bHoldWindowExpired at moment of button release (prevents race condition)
     */
    void ReleaseHeldHeavy(bool bWasWindowExpired);

    /**
     * Force restore normal playrate (1.0) for all active montages
     * SAFETY FUNCTION: Ensures we never have movement enabled while animation is frozen
     * Called when exiting hold state to prevent stuck animations
     */
    void ForceRestoreNormalPlayRate();
};