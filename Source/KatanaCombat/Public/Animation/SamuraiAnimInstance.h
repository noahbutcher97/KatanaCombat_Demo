// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CombatTypes.h"
#include "SamuraiAnimInstance.generated.h"

class UCombatComponent;
class UHitReactionComponent;
class ACharacter;
class UCharacterMovementComponent;

/**
 * Animation instance for samurai character
 * Synchronizes combat state with animation blueprint and routes AnimNotify callbacks
 * 
 * Responsibilities:
 * 1. Sync combat state variables to Animation Blueprint every frame
 * 2. Route AnimNotify callbacks from animations to appropriate components
 * 3. Provide state information for Animation Blueprint logic
 * 
 * Animation Blueprint reads these variables to drive:
 * - State machines (Idle/Attacking/Blocking/etc.)
 * - Blend spaces (directional movement)
 * - Transitions (combo chaining, hit reactions)
 * - Additive layers (breathing, posture weakness)
 * 
 * AnimNotify Routing:
 * Animation → AnimNotifyState → This AnimInstance → Component
 * Example: AttackPhase → OnAttackPhaseBegin() → CombatComponent::OnAttackPhaseBegin()
 * 
 * This class is the bridge between the animation system and combat logic.
 */
UCLASS()
class KATANACOMBAT_API USamuraiAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaTime) override;

    // ============================================================================
    // COMBAT STATE (Read by Animation Blueprint)
    // ============================================================================

    /** Current combat state (drives state machine) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    ECombatState CombatState = ECombatState::Idle;

    /** Current attack phase (for phase-specific animations) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    EAttackPhase CurrentPhase = EAttackPhase::None;

    /** Is currently attacking? (quick state check) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    bool bIsAttacking = false;

    /** Is currently blocking? (drives block pose) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    bool bIsBlocking = false;

    /** Is guard broken? (drives staggered pose) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    bool bIsGuardBroken = false;

    /** Is currently stunned? (disables upper body animations) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    bool bIsStunned = false;

    /**
     * Is currently holding an attack? (frozen at 0.0 playrate)
     * CRITICAL: Used to prevent locomotion updates during hold state
     * When true, UpdateMovement() skips velocity calculations to prevent
     * animation state machine conflicts with frozen montage
     */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    bool bIsHoldingAttack = false;

    // ============================================================================
    // MOVEMENT (Read by Animation Blueprint)
    // ============================================================================

    /** Current speed (units per second) */
    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    float Speed = 0.0f;

    /** Current direction relative to velocity (-180 to 180) */
    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    float Direction = 0.0f;

    /** Is in air? (drives jump/fall animations) */
    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bIsInAir = false;

    /** Is in combat stance? (affects idle pose) */
    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bIsInCombat = false;

    // ============================================================================
    // COMBO (Read by Animation Blueprint)
    // ============================================================================

    /** Current combo count (for combo-specific animations) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Combo")
    int32 ComboCount = 0;

    /** Can input next combo? (for UI feedback) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Combo")
    bool bCanCombo = false;

    // ============================================================================
    // POSTURE (Read by Animation Blueprint)
    // ============================================================================

    /** Posture as percentage (0-1, drives additive breathing intensity) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Posture")
    float PosturePercent = 1.0f;

    /** Is posture low? (< 40%, affects stance) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Posture")
    bool bIsPostureLow = false;

    // ============================================================================
    // CHARGE (Read by Animation Blueprint)
    // ============================================================================

    /** Heavy attack charge percentage (0-1, for VFX timing) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Charge")
    float ChargePercent = 0.0f;

    /** Is currently charging? (drives charge pose) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Charge")
    bool bIsCharging = false;

    // ============================================================================
    // HIT REACTIONS (Read by Animation Blueprint)
    // ============================================================================

    /** Last hit direction (for directional reactions) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Hit Reaction")
    EAttackDirection HitDirection = EAttackDirection::None;

    /** Hit intensity (0-1 for blend spaces) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Hit Reaction")
    float HitIntensity = 0.0f;

    // ============================================================================
    // ANIMNOTIFY ROUTING
    // ============================================================================

    /**
     * Called by AnimNotifyState_AttackPhase when a phase begins
     * Routes to CombatComponent
     * @param Phase - Which phase is beginning
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void OnAttackPhaseBegin(EAttackPhase Phase);

    /**
     * Called by AnimNotifyState_AttackPhase when a phase ends
     * Routes to CombatComponent
     * @param Phase - Which phase is ending
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void OnAttackPhaseEnd(EAttackPhase Phase);

    /**
     * Called by AnimNotify_AttackPhaseTransition when transitioning to a new phase
     * NEW PHASE SYSTEM: Single-event transitions instead of NotifyState ranges
     * Routes to CombatComponent
     * @param NewPhase - Phase we're transitioning TO (Active or Recovery)
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void OnAttackPhaseTransition(EAttackPhase NewPhase);

    /**
     * Called by AnimNotifyState_ComboWindow when combo window opens
     * Routes to CombatComponent
     * @param Duration - How long window stays open
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void OnComboWindowOpened(float Duration);

    /**
     * Called by AnimNotifyState_ComboWindow when combo window closes
     * Routes to CombatComponent
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void OnComboWindowClosed();

    /**
     * Called by AnimNotify_ToggleHitDetection to enable hit detection
     * Routes to WeaponComponent via ICombatInterface
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void OnEnableHitDetection();

    /**
     * Called by AnimNotify_ToggleHitDetection to disable hit detection
     * Routes to WeaponComponent via ICombatInterface
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void OnDisableHitDetection();

protected:
    /** Update all animation variables from components (called in NativeUpdateAnimation) */
    void UpdateAnimationVariables();

    /** Update combat state variables */
    void UpdateCombatState();

    /** Update movement variables */
    void UpdateMovement();

    /** Update combo variables */
    void UpdateCombo();

    /** Update posture variables */
    void UpdatePosture();

    /** Update charge variables */
    void UpdateCharge();

    /** Update hit reaction variables */
    void UpdateHitReaction();

private:
    // ============================================================================
    // CACHED REFERENCES
    // ============================================================================

    /** Owner character (cached for performance) */
    UPROPERTY()
    TObjectPtr<ACharacter> OwnerCharacter;

    /** Movement component (for speed/direction) */
    UPROPERTY()
    TObjectPtr<UCharacterMovementComponent> MovementComponent;

    /** Combat component (for combat state) */
    UPROPERTY()
    TObjectPtr<UCombatComponent> CombatComponent;

    /** Hit reaction component (for stun state) */
    UPROPERTY()
    TObjectPtr<UHitReactionComponent> HitReactionComponent;
};