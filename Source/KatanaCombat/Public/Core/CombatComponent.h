
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
 * - Combo tracking and branching
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
     * @return True if able to enter blocking state
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Defense")
    bool CanBlock() const;

    /** Start blocking (enter block state) */
    UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
    void StartBlocking();

    /** Stop blocking (return to idle) */
    UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
    void StopBlocking();

    /**
     * Attempt to parry an incoming attack
     * Must be called during enemy windup phase
     * @return True if parry was successful
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
    bool TryParry();

    // ============================================================================
    // COUNTER WINDOWS
    // ============================================================================

    /**
     * Is in counter window? (vulnerable to bonus damage)
     * @return True if counter window is active
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Counter")
    bool IsInCounterWindow() const { return bIsInCounterWindow; }

    /**
     * Open counter window on this actor (after being parried/evaded)
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

    /** Light attack pressed */
    UFUNCTION(BlueprintCallable, Category = "Combat|Input")
    void OnLightAttackPressed();

    /** Light attack released */
    UFUNCTION(BlueprintCallable, Category = "Combat|Input")
    void OnLightAttackReleased();

    /** Heavy attack pressed */
    UFUNCTION(BlueprintCallable, Category = "Combat|Input")
    void OnHeavyAttackPressed();

    /** Heavy attack released */
    UFUNCTION(BlueprintCallable, Category = "Combat|Input")
    void OnHeavyAttackReleased();

    /** Block pressed */
    UFUNCTION(BlueprintCallable, Category = "Combat|Input")
    void OnBlockPressed();

    /** Block released */
    UFUNCTION(BlueprintCallable, Category = "Combat|Input")
    void OnBlockReleased();

    /** Evade pressed */
    UFUNCTION(BlueprintCallable, Category = "Combat|Input")
    void OnEvadePressed();

    /** Set last movement input for directional follow-ups */
    UFUNCTION(BlueprintCallable, Category = "Combat|Input")
    void SetMovementInput(FVector2D Input);

    // ============================================================================
    // ATTACK PHASE CALLBACKS (from AnimNotifyStates)
    // ============================================================================

    /**
     * Called when an attack phase begins
     * Routed from AnimInstance via ICombatInterface
     * @param Phase - Which phase is beginning
     */
    void OnAttackPhaseBegin(EAttackPhase Phase);

    /**
     * Called when an attack phase ends
     * Routed from AnimInstance via ICombatInterface
     * @param Phase - Which phase is ending
     */
    void OnAttackPhaseEnd(EAttackPhase Phase);

    // ============================================================================
    // EVENTS
    // ============================================================================

    /** Event broadcast when combat state changes */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnCombatStateChanged OnCombatStateChanged;

    /** Event broadcast when attack hits an actor */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnAttackHit OnAttackHit;

    /** Event broadcast when posture changes */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnPostureChanged OnPostureChanged;

    /** Event broadcast when guard breaks */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnGuardBroken OnGuardBroken;

    /** Event broadcast on successful perfect parry */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnPerfectParry OnPerfectParry;

    /** Event broadcast on successful perfect evade */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnPerfectEvade OnPerfectEvade;

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
    // COMBO
    // ============================================================================

    /** Number of attacks in current combo */
    UPROPERTY(VisibleAnywhere, Category = "Combat|Combo")
    int32 ComboCount = 0;

    /** Can currently input next combo? */
    UPROPERTY(VisibleAnywhere, Category = "Combat|Combo")
    bool bCanCombo = false;

    /** Timer to reset combo after timeout */
    FTimerHandle ComboResetTimer;

    // ============================================================================
    // COUNTER WINDOW
    // ============================================================================

    /** Is counter window currently active? */
    UPROPERTY(VisibleAnywhere, Category = "Combat|Counter")
    bool bIsInCounterWindow = false;

    /** Timer to close counter window */
    FTimerHandle CounterWindowTimer;

    // ============================================================================
    // INPUT BUFFERING
    // ============================================================================

    /** Buffered light attack input */
    bool bLightAttackBuffered = false;

    /** Buffered heavy attack input */
    bool bHeavyAttackBuffered = false;

    /** Buffered evade input */
    bool bEvadeBuffered = false;

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

    // ============================================================================
    // MOVEMENT INPUT
    // ============================================================================

    /** Last movement input (X = Right, Y = Forward) used for directional follow-ups */
    UPROPERTY()
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
    // INTERNAL HELPERS
    // ============================================================================

    /** Regenerate posture based on current state */
    void RegeneratePosture(float DeltaTime);

    /** Get current posture regen rate based on state */
    float GetCurrentPostureRegenRate() const;

    /** Handle combo input during recovery phase */
    void HandleComboInput();

    /** Execute combo attack */
    void ExecuteComboAttack(UAttackData* NextAttack);

    /** Execute directional follow-up attack */
    void ExecuteDirectionalFollowUp(EAttackDirection Direction);

    /** Process all buffered inputs */
    void ProcessBufferedInputs();

    /** Clear all input buffers */
    void ClearInputBuffers();

    /** Setup motion warping for attack */
    void SetupMotionWarping(UAttackData* AttackData);

    /** Play attack montage */
    bool PlayAttackMontage(UAttackData* AttackData);

    /** Handle guard break recovery after stun duration */
    void RecoverFromGuardBreak();

    /** Reset combo chain after timeout */
    void ResetComboChain();

    /** Update heavy attack charging */
    void UpdateHeavyCharging(float DeltaTime);

    /** Release charged heavy attack */
    void ReleaseChargedHeavy();

    /** Update light attack hold */
    void UpdateLightHold(float DeltaTime);

    /** Release held light attack with directional input */
    void ReleaseHeldLight();

    /** Convert stored 2D input to a normalized world space direction */
    FVector GetWorldSpaceMovementInput() const;
};