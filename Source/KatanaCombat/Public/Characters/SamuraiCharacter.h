// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/DamageableInterface.h"
#include "CombatTypes.h"
#include "SamuraiCharacter.generated.h"

// Forward declarations
class UCombatComponent;
class UCombatComponentV2;
class UCombatDebugWidget;
class UTargetingComponent;
class UMotionWarpingComponent;
class UWeaponComponent;
class UHitReactionComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * Main character class implementing combat and damageable interfaces
 * Integrates all combat components and handles input routing
 * 
 * This class is primarily a coordinator - actual combat logic lives in components:
 * - CombatComponent: State machine, attacks, posture, combos, parry/counters
 * - TargetingComponent: Enemy selection and motion warping setup
 * - WeaponComponent: Hit detection via socket tracing
 * - HitReactionComponent: Damage reception and hit reactions
 */
UCLASS()
class KATANACOMBAT_API ASamuraiCharacter : public ACharacter, public ICombatInterface, public IDamageableInterface
{
    GENERATED_BODY()

public:
    ASamuraiCharacter();

    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // ============================================================================
    // CONFIGURATION
    // ============================================================================

    /** Global combat settings (posture rates, timing windows, V1/V2 system selection, debug flags) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Settings")
    TObjectPtr<class UCombatSettings> CombatSettings;

    // ============================================================================
    // COMPONENTS
    // ============================================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UCombatComponent> CombatComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UTargetingComponent> TargetingComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UWeaponComponent> WeaponComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UHitReactionComponent> HitReactionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

    /** V2 combat component (alternative implementation) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UCombatComponentV2> CombatComponentV2;

    /** Debug visualization widget for V2 system */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UCombatDebugWidget> CombatDebugWidget;



    // ============================================================================
    // ENHANCED INPUT
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> LightAttackAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> HeavyAttackAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> BlockAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> EvadeAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> ToggleDebugAction;

    // ============================================================================
    // ICombatInterface IMPLEMENTATION
    // ============================================================================

    virtual bool CanPerformAttack_Implementation() const override;
    virtual ECombatState GetCombatState_Implementation() const override;
    virtual bool IsAttacking_Implementation() const override;
    virtual UAttackData* GetCurrentAttack_Implementation() const override;
    virtual EAttackPhase GetCurrentPhase_Implementation() const override;
    virtual void OnEnableHitDetection_Implementation() override;
    virtual void OnDisableHitDetection_Implementation() override;
    virtual void OnAttackPhaseBegin_Implementation(EAttackPhase Phase) override;
    virtual void OnAttackPhaseEnd_Implementation(EAttackPhase Phase) override;
    virtual void OnAttackPhaseTransition_Implementation(EAttackPhase NewPhase) override;
    virtual bool IsInParryWindow_Implementation() const override;

    // ============================================================================
    // IDamageableInterface IMPLEMENTATION
    // ============================================================================

    virtual float ApplyDamage_Implementation(const FHitReactionInfo& HitInfo) override;
    virtual bool ApplyPostureDamage_Implementation(float PostureDamage, AActor* Attacker) override;
    virtual bool CanBeDamaged_Implementation() const override;
    virtual bool IsBlocking_Implementation() const override;
    virtual bool IsGuardBroken_Implementation() const override;
    virtual bool ExecuteFinisher_Implementation(AActor* Attacker, UAttackData* FinisherData) override;
    virtual void OnAttackParried_Implementation(AActor* Parrier) override;
    virtual void OpenCounterWindow_Implementation(float Duration) override;
    virtual float GetCurrentPosture_Implementation() const override;
    virtual float GetMaxPosture_Implementation() const override;
    virtual bool IsInCounterWindow_Implementation() const override;

protected:
    virtual void BeginPlay() override;

    // ============================================================================
    // INPUT HANDLERS
    // ============================================================================

    /** Movement input (continuous) */
    void Move(const FInputActionValue& Value);
    
    /** Look input (continuous) */
    void Look(const FInputActionValue& Value);

    /** Light attack button pressed */
    void OnLightAttackStarted(const FInputActionValue& Value);
    
    /** Light attack button released */
    void OnLightAttackCompleted(const FInputActionValue& Value);

    /** Heavy attack button pressed */
    void OnHeavyAttackStarted(const FInputActionValue& Value);
    
    /** Heavy attack button released */
    void OnHeavyAttackCompleted(const FInputActionValue& Value);

    /** Block button pressed */
    void OnBlockStarted(const FInputActionValue& Value);
    
    /** Block button released */
    void OnBlockCompleted(const FInputActionValue& Value);

    /** Evade button pressed */
    void OnEvadeStarted(const FInputActionValue& Value);

    /** Debug toggle button pressed */
    void OnToggleDebug(const FInputActionValue& Value);

    // ============================================================================
    // WEAPON HIT PROCESSING
    // ============================================================================

    /**
     * Called when weapon hits a target (bound to WeaponComponent->OnWeaponHit)
     * Handles damage application through IDamageableInterface
     * @param HitActor - Actor that was hit
     * @param HitResult - Hit trace result
     * @param AttackData - Attack data for damage calculation
     */
    UFUNCTION()
    void OnWeaponHitTarget(AActor* HitActor, const FHitResult& HitResult, UAttackData* AttackData);
};