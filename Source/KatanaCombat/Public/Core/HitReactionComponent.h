// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "HitReactionComponent.generated.h"

class UAnimMontage;
class ACharacter;
class UAnimInstance;

/**
 * Handles receiving damage, playing hit reactions, and managing stun states
 * Shared by player and enemies for consistent hit response
 * 
 * Features:
 * - Directional hit reactions (front/back/left/right)
 * - Intensity-based reactions (light/heavy)
 * - Hitstun management
 * - Damage modifiers (super armor, invulnerability, resistance)
 * - Guard broken reactions
 * - Finisher victim animations
 * 
 * This component is designed to be reusable across different character types
 * while maintaining consistent hit response behavior throughout the game.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class KATANACOMBAT_API UHitReactionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHitReactionComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ============================================================================
    // CONFIGURATION - HIT REACTION ANIMATIONS
    // ============================================================================

    /** Light hit reactions (minimal stagger) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions|Light")
    FHitReactionAnimSet LightHitReactions;

    /** Heavy hit reactions (full directional stagger) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions|Heavy")
    FHitReactionAnimSet HeavyHitReactions;

    /** Guard broken animation (posture depleted) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions")
    TObjectPtr<UAnimMontage> GuardBrokenMontage = nullptr;

    /** Finisher victim animations (paired with attacker finisher) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions|Finishers")
    TMap<FName, TObjectPtr<UAnimMontage>> FinisherVictimAnimations;

    // ============================================================================
    // CONFIGURATION - DAMAGE MODIFIERS
    // ============================================================================

    /** If true, attacks don't cause hitstun (still take damage) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Modifiers")
    bool bHasSuperArmor = false;

    /** If true, completely immune to damage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Modifiers")
    bool bIsInvulnerable = false;

    /** Damage reduction multiplier (1.0 = normal, 0.5 = half damage, 0.0 = immune) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Modifiers", 
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DamageResistance = 1.0f;

    // ============================================================================
    // DAMAGE APPLICATION
    // ============================================================================

    /**
     * Apply damage to this actor
     * @param HitInfo - Complete hit information (attacker, direction, damage, etc.)
     * @return Actual damage dealt (after resistances and modifiers)
     */
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    float ApplyDamage(const FHitReactionInfo& HitInfo);

    /**
     * Play appropriate hit reaction based on hit direction and intensity
     * Automatically selects correct animation from configured sets
     * @param HitInfo - Hit information including direction and intensity
     */
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    void PlayHitReaction(const FHitReactionInfo& HitInfo);

    /**
     * Apply hitstun to this actor
     * @param Duration - How long to be stunned (seconds)
     */
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    void ApplyHitStun(float Duration);

    /**
     * Play guard broken reaction (when posture depletes to 0)
     */
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    void PlayGuardBrokenReaction();

    /**
     * Play finisher victim animation (paired with attacker)
     * @param FinisherName - Name of finisher animation to play
     * @return True if animation was found and played
     */
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    bool PlayFinisherVictimAnimation(FName FinisherName);

    // ============================================================================
    // STATE QUERIES
    // ============================================================================

    /**
     * Is currently stunned?
     * @return True if in hitstun state
     */
    UFUNCTION(BlueprintPure, Category = "Hit Reaction")
    bool IsStunned() const { return bIsStunned; }

    /**
     * Get remaining stun time
     * @return Seconds of stun remaining
     */
    UFUNCTION(BlueprintPure, Category = "Hit Reaction")
    float GetRemainingStunTime() const { return StunTimeRemaining; }

    /**
     * Can be damaged right now?
     * Checks invulnerability, super armor, and other conditions
     * @return True if vulnerable to damage
     */
    UFUNCTION(BlueprintPure, Category = "Hit Reaction")
    bool CanBeDamaged() const;

    // ============================================================================
    // EVENTS
    // ============================================================================

    /** Event called when damage is received */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDamageReceived, const FHitReactionInfo&, HitInfo);

    UPROPERTY(BlueprintAssignable, Category = "Hit Reaction")
    FOnDamageReceived OnDamageReceived;

    /** Event called when hit reaction starts playing */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHitReactionStarted, EAttackDirection, Direction, bool, bIsHeavyHit);

    UPROPERTY(BlueprintAssignable, Category = "Hit Reaction")
    FOnHitReactionStarted OnHitReactionStarted;

    /** Event called when stun begins */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStunBegin, float, Duration);

    UPROPERTY(BlueprintAssignable, Category = "Hit Reaction")
    FOnStunBegin OnStunBegin;

    /** Event called when stun ends */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStunEnd);

    UPROPERTY(BlueprintAssignable, Category = "Hit Reaction")
    FOnStunEnd OnStunEnd;

protected:
    virtual void BeginPlay() override;

private:
    // ============================================================================
    // STATE
    // ============================================================================

    /** Currently stunned? */
    UPROPERTY(VisibleAnywhere, Category = "Hit Reaction")
    bool bIsStunned = false;

    /** Remaining stun time (seconds) */
    UPROPERTY(VisibleAnywhere, Category = "Hit Reaction")
    float StunTimeRemaining = 0.0f;

    // ============================================================================
    // CACHED REFERENCES
    // ============================================================================

    /** Owner character (cached for performance) */
    UPROPERTY()
    TObjectPtr<ACharacter> OwnerCharacter;

    /** Owner's anim instance (for playing reactions) */
    UPROPERTY()
    TObjectPtr<UAnimInstance> AnimInstance;

    // ============================================================================
    // INTERNAL HELPERS
    // ============================================================================

    /**
     * Select appropriate hit reaction montage based on direction and intensity
     * @param HitInfo - Hit information
     * @return Selected montage, or nullptr if none found
     */
    UAnimMontage* SelectHitReactionMontage(const FHitReactionInfo& HitInfo) const;

    /**
     * Calculate hit direction relative to character facing
     * @param HitDirection - World space hit direction
     * @return Directional enum (Front, Back, Left, Right)
     */
    EAttackDirection GetHitDirectionRelativeToFacing(const FVector& HitDirection) const;

    /** Update stun timer (called in Tick) */
    void UpdateStun(float DeltaTime);

    /** End current stun */
    void EndStun();
};