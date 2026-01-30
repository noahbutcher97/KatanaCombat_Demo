// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "Data/PairedAnimationTypes.h"
#include "HitReactionComponent.generated.h"

class UAnimMontage;
class ACharacter;
class UAnimInstance;
class UHitReactionSettings;
class UHitReactionData;

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
    // SETTINGS (Data Asset Configuration)
    // ============================================================================

    /**
     * Override hit reaction settings (if null, uses CombatSettings)
     * Use this for per-character customization without changing CombatSettings
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    TObjectPtr<UHitReactionSettings> HitReactionSettingsOverride;

    /**
     * Get effective settings (override → CombatSettings → nullptr)
     * Components should call this to access configuration
     */
    UFUNCTION(BlueprintPure, Category = "Settings")
    UHitReactionSettings* GetEffectiveSettings() const;

    // ============================================================================
    // ACTIVE REACTION STATE (for i-frame tracking)
    // ============================================================================

    /** Time since current reaction started */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    float CurrentReactionTime = 0.0f;

    /** Is currently in i-frames? */
    UFUNCTION(BlueprintPure, Category = "State")
    bool IsInIFrames() const;

private:
    /** Current reaction has i-frames enabled? */
    bool bCurrentReactionHasIFrames = false;

    /** I-frame window start time */
    float CurrentIFrameStart = 0.0f;

    /** I-frame window end time */
    float CurrentIFrameEnd = 0.0f;

public:

    // ============================================================================
    // LEGACY CONFIGURATION (Deprecated - use HitReactionSettings instead)
    // Kept for backwards compatibility during migration
    // ============================================================================

    /** Light hit reactions (minimal stagger) - DEPRECATED: use HitReactionSettings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions|Legacy",
        meta = (DeprecatedProperty, DeprecationMessage = "Use HitReactionSettings instead"))
    FHitReactionAnimSet LightHitReactions;

    /** Heavy hit reactions (full directional stagger) - DEPRECATED: use HitReactionSettings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions|Legacy",
        meta = (DeprecatedProperty, DeprecationMessage = "Use HitReactionSettings instead"))
    FHitReactionAnimSet HeavyHitReactions;

    /** Guard broken animation (posture depleted) - DEPRECATED: use HitReactionSettings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions|Legacy",
        meta = (DeprecatedProperty, DeprecationMessage = "Use HitReactionSettings instead"))
    TObjectPtr<UAnimMontage> GuardBrokenMontage = nullptr;

    /** Finisher victim animations (paired with attacker finisher) - DEPRECATED: use HitReactionSettings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions|Legacy",
        meta = (DeprecatedProperty, DeprecationMessage = "Use HitReactionSettings instead"))
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
    // NEW DATA-DRIVEN API
    // ============================================================================

    /**
     * Play hit reaction from inline entry (directional reactions)
     * Handles section selection, play rate, and i-frame tracking
     * @param ReactionEntry - Reaction entry config to play
     * @param Direction - Direction for event broadcast
     * @param bIsHeavy - Is this a heavy reaction (for event broadcast)
     * @return True if reaction started successfully
     */
    bool PlayReactionFromEntry(const FHitReactionEntry& ReactionEntry, EAttackDirection Direction, bool bIsHeavy);

    /**
     * Play hit reaction from inline entry with explicit intensity (for variety selection)
     * @param ReactionEntry - Reaction entry config to play
     * @param Direction - Direction for event broadcast
     * @param bIsHeavy - Is this a heavy reaction (for event broadcast)
     * @param Intensity - Hit intensity for variety selection history
     * @return True if reaction started successfully
     */
    bool PlayReactionFromEntry(const FHitReactionEntry& ReactionEntry, EAttackDirection Direction, bool bIsHeavy, EHitIntensity Intensity);

    /**
     * Play hit reaction from HitReactionData asset (special/paired reactions)
     * Handles section selection, play rate, and i-frame tracking
     * @param ReactionData - Reaction data asset to play
     * @return True if reaction started successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    bool PlayReactionFromData(UHitReactionData* ReactionData);

    /**
     * Play paired reaction (counter/finisher victim)
     * Extension point: Called when AttackData has counter/finisher name
     * @param PairedType - Type of paired reaction
     * @param ReactionName - Name identifier for lookup
     * @return True if reaction started
     */
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    bool PlayPairedReaction(EPairedReactionType PairedType, FName ReactionName);

    /**
     * Play special reaction (guard broken, knockdown, etc.)
     * @param SpecialType - Type of special reaction
     * @return True if reaction started
     */
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    bool PlaySpecialReaction(ESpecialReactionType SpecialType);

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

    /** Event called when ragdoll physics is activated */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRagdollActivated);

    UPROPERTY(BlueprintAssignable, Category = "Hit Reaction")
    FOnRagdollActivated OnRagdollActivated;

    // ============================================================================
    // PAIRED ANIMATION EVENTS
    // ============================================================================

    /** Fires when a paired animation (finisher, counter) starts on this character */
    UPROPERTY(BlueprintAssignable, Category = "Paired Animation")
    FOnPairedAnimationStarted OnPairedAnimationStarted;

    /** Fires at sync points during paired animations (impact, damage application) */
    UPROPERTY(BlueprintAssignable, Category = "Paired Animation")
    FOnPairedAnimationSyncPoint OnPairedAnimationSyncPoint;

    /** Fires when paired animation ends */
    UPROPERTY(BlueprintAssignable, Category = "Paired Animation")
    FOnPairedAnimationEnded OnPairedAnimationEnded;

    // ============================================================================
    // DEATH REACTION
    // ============================================================================

    /**
     * Play directional death reaction
     * Selects animation based on direction, handles ragdoll transition if configured
     * @param Direction - Direction the killing blow came from (relative to victim)
     * @return True if death reaction started successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    bool PlayDeathReaction(EAttackDirection Direction);

    /**
     * Calculate hit direction relative to character facing
     * @param HitDirection - World space direction (from attacker to victim)
     * @return Directional enum (Forward, Backward, Left, Right)
     */
    UFUNCTION(BlueprintPure, Category = "Hit Reaction")
    EAttackDirection GetHitDirectionRelativeToFacing(const FVector& HitDirection) const;

    // ============================================================================
    // REACTION VARIETY
    // ============================================================================

    /**
     * Clear reaction history (call on death/respawn for fresh variety selection)
     */
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    void ClearReactionHistory();

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
     * Get owner character with lazy initialization
     * Returns cached OwnerCharacter if available, otherwise attempts to get from GetOwner()
     * This ensures the component works in test environments where BeginPlay may not be called
     * @return Owner character or nullptr
     */
    ACharacter* GetOwnerCharacterCached() const;

    /** Update stun timer (called in Tick) */
    void UpdateStun(float DeltaTime);

    /** End current stun */
    void EndStun();

    // ============================================================================
    // REACTION VARIETY (n-2 randomization)
    // ============================================================================

    /** History of played reactions: Intensity → Direction → History */
    TMap<EHitIntensity, TMap<EAttackDirection, FReactionHistory>> ReactionHistoryMap;

    /**
     * Select montage variant with variety (n-2 for 3+, alternation for 2, direct for 1)
     * @param Entry - Reaction entry with montage variant(s)
     * @param Intensity - Hit intensity for history lookup
     * @param Direction - Hit direction for history lookup
     * @param OutVariant - Selected variant with montage and section
     * @return True if a valid variant was selected
     */
    bool SelectMontageWithVariety(
        const FHitReactionEntry& Entry,
        EHitIntensity Intensity,
        EAttackDirection Direction,
        FReactionMontageVariant& OutVariant);

    /** Record that a montage index was played for history */
    void RecordMontagePlay(int32 MontageIndex, EHitIntensity Intensity, EAttackDirection Direction);

    // ============================================================================
    // DEATH/RAGDOLL STATE
    // ============================================================================

    /** Outcome to apply when death animation completes */
    EReactionOutcome PendingDeathOutcome = EReactionOutcome::StandardRecovery;

    /** Is death outcome pending? */
    bool bDeathOutcomePending = false;

    /** Blend time for pending ragdoll transition */
    float PendingRagdollBlendTime = 0.2f;

    /** The death montage we're waiting for (to filter global delegate) */
    UPROPERTY()
    TObjectPtr<UAnimMontage> PendingDeathMontage = nullptr;

    /**
     * Handle montage blending out - apply death outcome BEFORE blend completes
     * This prevents returning to idle pose before we can freeze or ragdoll
     * Uses GLOBAL delegate since per-montage delegates seem unreliable
     */
    UFUNCTION()
    void OnAnyMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

    /** Activate ragdoll physics on mesh */
    void ActivateRagdoll(float BlendTime);

    /** Has ragdoll already been activated (prevents double activation from notify + blend-out) */
    bool bRagdollActivated = false;

public:
    /**
     * Trigger ragdoll from AnimNotify (mid-animation activation)
     * Called by AnimNotify_ActivateRagdoll to enable early ragdoll transition
     * @param BlendTime - Blend time from notify (overrides pending blend time)
     */
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    void TriggerRagdollFromNotify(float BlendTime);

private:
    /**
     * Freeze animation at current pose using pose snapshot
     * Captures current skeletal pose and applies it as a static pose
     * This decouples from montage system and enables future recovery
     */
    void FreezeAtCurrentPose();

    // ============================================================================
    // POSE SNAPSHOT (for death/knockdown recovery)
    // ============================================================================

    /** Name used for death pose snapshot */
    static const FName DeathPoseSnapshotName;

    /** Has a death pose been captured? */
    bool bHasDeathPoseSnapshot = false;

public:
    /**
     * Check if a death pose snapshot has been captured
     * Useful for recovery systems that need to blend from death pose
     */
    UFUNCTION(BlueprintPure, Category = "Hit Reaction")
    bool HasDeathPoseSnapshot() const { return bHasDeathPoseSnapshot; }

    /**
     * Get the snapshot name for use in Animation Blueprints
     * Use with "Pose Snapshot" node to blend from captured death pose
     */
    UFUNCTION(BlueprintPure, Category = "Hit Reaction")
    static FName GetDeathPoseSnapshotName() { return DeathPoseSnapshotName; }

    /**
     * Clear the death pose snapshot (call when character recovers/revives)
     */
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    void ClearDeathPoseSnapshot();
};