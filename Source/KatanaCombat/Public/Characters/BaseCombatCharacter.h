// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/DamageableInterface.h"
#include "Interfaces/TeamMemberInterface.h"
#include "CombatTypes.h"
#include "BaseCombatCharacter.generated.h"

// Forward declarations
class UCombatComponent;
class UTargetingComponent;
class UMotionWarpingComponent;
class UWeaponComponent;
class UHitReactionComponent;
class UCombatSettings;
class UAttackData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, NewHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterDying, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterDeath, AActor*, Killer);

/**
 * Abstract base class for all combat-capable characters
 * Provides common combat components, health system, and interface implementations
 *
 * This class is the foundation for:
 * - Player characters (APlayerCharacter)
 * - Enemy characters (AEnemyCharacter)
 * - Any other character that participates in combat
 *
 * Separation of concerns:
 * - BaseCombatCharacter: Components, interfaces, health/team systems
 * - Derived classes: Input handling, AI behavior, character-specific logic
 */
UCLASS(Abstract)
class KATANACOMBAT_API ABaseCombatCharacter : public ACharacter,
    public ICombatInterface, public IDamageableInterface, public ITeamMemberInterface
{
    GENERATED_BODY()

public:
    ABaseCombatCharacter();

    // ========================================================================
    // COMPONENTS (Common to all combat characters)
    // ========================================================================

    /** Combat component - handles action queue, phases, input processing */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
    TObjectPtr<UCombatComponent> CombatComponent;

    /** Targeting component - handles target selection, motion warp setup */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
    TObjectPtr<UTargetingComponent> TargetingComponent;

    /** Weapon component - handles hit detection via socket tracing */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
    TObjectPtr<UWeaponComponent> WeaponComponent;

    /** Hit reaction component - handles damage reception and reactions */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
    TObjectPtr<UHitReactionComponent> HitReactionComponent;

    /** Motion warping component - handles root motion warping for attacks */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
    TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /** Global combat settings (posture rates, timing windows, debug flags) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Settings")
    TObjectPtr<UCombatSettings> CombatSettings;

    // ========================================================================
    // TEAM SYSTEM
    // ========================================================================

    /** Which team this character belongs to */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Team")
    ETeamId TeamId = ETeamId::Neutral;

    // ========================================================================
    // HEALTH SYSTEM
    // ========================================================================

    /** Maximum health value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health", meta = (ClampMin = "1.0"))
    float MaxHealth = 100.0f;

    /** Current health value */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Health")
    float CurrentHealth = 100.0f;

    /** Called when health changes */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnHealthChanged OnHealthChanged;

    /** Called when character receives lethal damage (enters Dying state) */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnCharacterDying OnCharacterDying;

    /** Called when character's death animation completes (enters Dead state) */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnCharacterDeath OnCharacterDeath;

    // ========================================================================
    // TWO-STAGE DEATH SYSTEM
    // ========================================================================
    // Dying: Lethal damage received, death animation playing, combat blocked
    // Dead:  Death animation complete, ragdoll/freeze applied
    //
    // This separation allows death animations to play through naturally
    // before the final outcome (ragdoll/freeze) is applied.
    // ========================================================================

    /**
     * Is this character dying? (Lethal damage received, death animation playing)
     * When true: Combat blocked, can't be targeted, animation continues
     * Transitions to bIsDead when death animation completes
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Health")
    bool bIsDying = false;

    /**
     * Is this character dead? (Death animation complete, ragdoll/freeze applied)
     * Final state - character is truly finished
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Health")
    bool bIsDead = false;

    /** Check if character is in dying state (lethal damage, animation playing) */
    UFUNCTION(BlueprintPure, Category = "Combat|Health")
    bool IsDying() const { return bIsDying && !bIsDead; }

    /** Check if character is fully dead (animation complete, outcome applied) */
    UFUNCTION(BlueprintPure, Category = "Combat|Health")
    bool IsDead() const { return bIsDead; }

    /** Check if character should not be interacted with (dying OR dead) */
    UFUNCTION(BlueprintPure, Category = "Combat|Health")
    bool IsDeadOrDying() const { return bIsDying || bIsDead; }

    /**
     * Called by HitReactionComponent when death animation completes.
     * Transitions from Dying to Dead state.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Health")
    void FinalizeDeath();

    // ========================================================================
    // COMPONENT ACCESSORS
    // ========================================================================

    UFUNCTION(BlueprintPure, Category = "Combat")
    UCombatComponent* GetCombatComponent() const { return CombatComponent; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    UTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    UWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    UHitReactionComponent* GetHitReactionComponent() const { return HitReactionComponent; }

    /**
     * Get current movement input vector (camera-relative)
     * Used by debug visualization to show attack direction preview during hold
     * @return 2D movement input from last frame, or ZeroVector if no input
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Input")
    virtual FVector2D GetLastMovementInput() const { return FVector2D::ZeroVector; }

    // ========================================================================
    // HEALTH UTILITIES
    // ========================================================================

    /**
     * Modify health by delta amount
     * @param Delta - Amount to add (negative for damage)
     * @param DamageInstigator - Actor responsible for the change (optional)
     * @return Actual delta applied
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Health")
    float ModifyHealth(float Delta, AActor* DamageInstigator = nullptr);

    /**
     * Set health to specific value
     * @param NewHealth - New health value
     * @param DamageInstigator - Actor responsible for the change (optional)
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Health")
    void SetHealth(float NewHealth, AActor* DamageInstigator = nullptr);

    // ========================================================================
    // ITeamMemberInterface IMPLEMENTATION
    // ========================================================================

    virtual ETeamId GetTeamId_Implementation() const override;
    virtual bool IsHostileTo_Implementation(AActor* Other) const override;
    virtual bool IsFriendlyTo_Implementation(AActor* Other) const override;

    // ========================================================================
    // IDamageableInterface IMPLEMENTATION (Health Queries)
    // ========================================================================

    virtual float GetCurrentHealth_Implementation() const override;
    virtual float GetMaxHealth_Implementation() const override;
    virtual bool IsAlive_Implementation() const override;

    // ========================================================================
    // IDamageableInterface IMPLEMENTATION (Combat)
    // ========================================================================

    virtual float ApplyDamage_Implementation(const FHitReactionInfo& HitInfo) override;
    virtual bool ApplyPostureDamage_Implementation(float PostureDamage, AActor* Attacker) override;
    virtual bool CanBeDamaged_Implementation() const override;
    virtual bool IsBlocking_Implementation() const override;
    virtual bool IsGuardBroken_Implementation() const override;
    virtual bool IsStaggered_Implementation() const override;
    virtual bool ExecuteFinisher_Implementation(AActor* Attacker, UAttackData* FinisherData) override;
    virtual void OnAttackParried_Implementation(AActor* Parrier) override;
    virtual void OpenCounterWindow_Implementation(float Duration) override;
    virtual float GetCurrentPosture_Implementation() const override;
    virtual float GetMaxPosture_Implementation() const override;
    virtual bool IsInCounterWindow_Implementation() const override;

    // ========================================================================
    // ICombatInterface IMPLEMENTATION
    // ========================================================================

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
    virtual void OnHoldWindowStart_Implementation(EInputType InputType) override;

protected:
    virtual void BeginPlay() override;

    /**
     * Called when character dies (health reaches 0)
     * Override in derived classes for custom death behavior
     * @param Killer - Actor that dealt the killing blow
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Combat|Health")
    void HandleDeath(AActor* Killer);

    /**
     * Called when weapon hits a target
     * Handles damage application through IDamageableInterface
     * @param HitActor - Actor that was hit
     * @param HitResult - Hit trace result
     * @param AttackData - Attack data for damage calculation
     */
    UFUNCTION()
    virtual void OnWeaponHitTarget(AActor* HitActor, const FHitResult& HitResult, UAttackData* AttackData);
};
