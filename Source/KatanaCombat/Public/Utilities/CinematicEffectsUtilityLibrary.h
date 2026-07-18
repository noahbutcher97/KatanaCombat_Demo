// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatTypes.h"
#include "CinematicEffectsUtilityLibrary.generated.h"

class UCombatFXData;

/**
 * Cinematic Effects Utility Library
 *
 * Static utility functions for cinematic effects during combat:
 * - Time dilation (slow motion, hitstop)
 * - Camera shake triggering
 * - Per-hit hitstop (Sakurai-style freeze)
 * - Impact audio (per-attack hit sounds with pitch variation)
 * - Impact VFX (Niagara spawning with surface alignment) [scaffold]
 *
 * Design: Separated from PairedAnimationUtilityLibrary to allow reuse
 * across various combat scenarios (not just paired animations).
 */
UCLASS()
class KATANACOMBAT_API UCinematicEffectsUtilityLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
#if WITH_AUTOMATION_TESTS
    DECLARE_MULTICAST_DELEGATE_FourParams(
        FOnImpactSoundPlaybackInvokedForTesting,
        UWorld*,
        USoundBase*,
        const FVector&,
        AActor*);

    static FOnImpactSoundPlaybackInvokedForTesting OnImpactSoundPlaybackInvokedForTesting;

    DECLARE_MULTICAST_DELEGATE_FourParams(
        FOnImpactVFXSpawnInvokedForTesting,
        UWorld*,
        UNiagaraSystem*,
        const FVector&,
        FName);

    static FOnImpactVFXSpawnInvokedForTesting OnImpactVFXSpawnInvokedForTesting;
#endif

    // ========================================================================
    // TIME DILATION
    // ========================================================================

    /**
     * Apply slow motion to world time dilation.
     * Does NOT handle restoration - caller must manage duration/restore.
     *
     * @param World - World context
     * @param Scale - Time dilation scale (0.01-1.0, clamped)
     * @return True if successfully applied
     */
    UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|Time")
    static bool ApplySlowMotion(UWorld* World, float Scale);

    /**
     * Release this compatibility caller's world-dilation lease.
     * The last active owner restores the captured pre-lease baseline.
     *
     * @param World - World context
     */
    UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|Time")
    static void RestoreTimeDilation(UWorld* World);

    /**
     * Get current world time dilation.
     *
     * @param World - World context
     * @return Current time dilation (1.0 = normal)
     */
    UFUNCTION(BlueprintPure, Category = "Cinematic Effects|Time")
    static float GetTimeDilation(UWorld* World);

    /**
     * Check if slow motion is currently active.
     *
     * @param World - World context
     * @return True if time dilation is less than 1.0
     */
    UFUNCTION(BlueprintPure, Category = "Cinematic Effects|Time")
    static bool IsSlowMotionActive(UWorld* World);

    // ========================================================================
    // CAMERA SHAKE
    // ========================================================================

    /**
     * Play camera shake on actor's controlling player.
     * Only affects local player controllers.
     *
     * @param Actor - Actor whose controller should receive camera shake
     * @param CameraShakeClass - Camera shake class to play
     * @param Scale - Shake intensity scale (default 1.0)
     * @return True if camera shake was played
     */
    UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|Camera")
    static bool PlayCameraShakeOnActor(
        AActor* Actor,
        TSubclassOf<UCameraShakeBase> CameraShakeClass,
        float Scale = 1.0f);

    /**
     * Play camera shake at world location for nearby players.
     * Shake intensity falls off with distance from location.
     *
     * @param World - World context
     * @param Location - World location to center shake on
     * @param CameraShakeClass - Camera shake class to play
     * @param InnerRadius - Radius at full shake intensity
     * @param OuterRadius - Radius at zero shake intensity
     * @param Falloff - Falloff curve exponent (1.0 = linear)
     * @return Number of players affected
     */
    UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|Camera")
    static int32 PlayCameraShakeAtLocation(
        UWorld* World,
        const FVector& Location,
        TSubclassOf<UCameraShakeBase> CameraShakeClass,
        float InnerRadius = 0.0f,
        float OuterRadius = 1000.0f,
        float Falloff = 1.0f);

    // ========================================================================
    // HITSTOP (Per-Hit Impact Freeze)
    // ========================================================================

    /**
     * Apply Sakurai-style hitstop to attacker and victim.
     * Both actors freeze for the configured duration via per-actor CustomTimeDilation.
     * Camera shake fires immediately (camera is unaffected by actor freeze).
     * Uses FPlatformTime::Seconds() + FTSTicker for wall-clock accurate restoration.
     *
     * Industry standard technique (DMC5, Sekiro, Ghost of Tsushima):
     * - Per-actor freeze (background continues)
     * - Camera and particles continue during hitstop
     * - Duration scales with attack weight, not damage
     *
     * @param Attacker - Actor performing the attack (frozen during hitstop)
     * @param Victim - Actor receiving the hit (frozen during hitstop)
     * @param Config - Hitstop configuration (duration, camera shake, etc.)
     * @param bWasBlocked - Whether the hit was blocked (applies BlockedDurationMultiplier)
     * @return True if hitstop was applied
     */
    UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|Hitstop")
    static bool ApplyHitstop(
        AActor* Attacker,
        AActor* Victim,
        const FHitstopConfig& Config,
        bool bWasBlocked = false);

    /**
     * Apply per-actor hitstop to an arbitrary participant list.
     * Overlapping calls preserve each actor's original pre-hitstop dilation and
     * extend the active freeze window instead of restoring to a nested frozen value.
     *
     * @param Actors - Actors to freeze during hitstop
     * @param Duration - Wall-clock duration in seconds
     * @return True if at least one actor was frozen
     */
    static bool ApplyHitstopToActors(const TArray<AActor*>& Actors, float Duration);

    // ========================================================================
    // IMPACT AUDIO
    // ========================================================================

    /**
     * Play impact sound at hit location with pitch/volume variation.
     * Supports per-attack configuration with weapon fallback.
     *
     * Resolution order: Config.ImpactSound → WeaponFallbackSound → nothing.
     * Pitch variation (±5% default) prevents repetitive audio.
     *
     * @param World - World context for sound spawning
     * @param Config - Audio configuration from AttackData
     * @param WeaponFallbackSound - Weapon's default hit sound (used if Config has no sound and bUseWeaponFallback)
     * @param ImpactLocation - World location for spatial audio
     * @param Attacker - Attacking actor (for sound attenuation override)
     * @return True if sound was played
     */
    UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|Audio")
    static bool PlayImpactSound(
        UWorld* World,
        const FImpactAudioConfig& Config,
        USoundBase* WeaponFallbackSound,
        const FVector& ImpactLocation,
        AActor* Attacker = nullptr);

    /**
     * Resolve and play impact sound through 4-tier resolution chain.
     * New pooled FX system with per-weapon random selection.
     *
     * Resolution order (first valid wins):
     *   1. AttackData.ImpactAudioConfig.ImpactSound (per-attack override)
     *   2. CombatFXData pool[AttackType] (random from pool)
     *   3. WeaponFallbackSound (simple weapon fallback)
     *   4. silent
     *
     * @param World - World context for sound spawning
     * @param AudioConfig - Per-attack audio configuration from AttackData
     * @param CombatFXData - Pooled FX data from WeaponData (can be nullptr)
     * @param AttackType - Attack type for pool lookup
     * @param WeaponFallbackSound - Weapon's default hit sound
     * @param ImpactLocation - World location for spatial audio
     * @param bWasBlocked - Whether the hit was blocked (uses BlockedPool if configured)
     * @param Attacker - Attacking actor (for sound attenuation override)
     * @return True if sound was played
     */
    UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|Audio")
    static bool ResolveAndPlayImpactSound(
        UWorld* World,
        const FImpactAudioConfig& AudioConfig,
        const UCombatFXData* CombatFXData,
        EAttackType AttackType,
        USoundBase* WeaponFallbackSound,
        const FVector& ImpactLocation,
        bool bWasBlocked = false,
        AActor* Attacker = nullptr);

    // ========================================================================
    // IMPACT VFX (U-16)
    // ========================================================================

    /**
     * Spawn impact VFX at hit location with surface alignment.
     * Supports per-attack configuration with weapon fallback.
     *
     * @param World - World context for VFX spawning
     * @param Config - VFX configuration from AttackData
     * @param WeaponFallbackVFX - Weapon's default hit VFX (used if Config has no VFX and bUseWeaponFallback)
     * @param ImpactLocation - World location for VFX spawn
     * @param ImpactNormal - Surface normal for alignment
     * @param BoneName - Bone that was hit (for attached VFX)
     * @return True if VFX was spawned
     */
    UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|VFX")
    static bool SpawnImpactVFX(
        UWorld* World,
        const FImpactVFXConfig& Config,
        UNiagaraSystem* WeaponFallbackVFX,
        const FVector& ImpactLocation,
        const FVector& ImpactNormal,
        FName BoneName = NAME_None);

    /**
     * Resolve and spawn impact VFX through 4-tier resolution chain.
     * Pooled FX system with per-weapon random selection.
     *
     * Resolution order (first valid wins):
     *   1. AttackData.ImpactVFXConfig.ImpactVFX (per-attack override)
     *   2. CombatFXData pool[AttackType] (random from pool)
     *   3. WeaponFallbackVFX (simple weapon fallback)
     *   4. nothing
     *
     * @param World - World context for VFX spawning
     * @param VFXConfig - Per-attack VFX configuration from AttackData
     * @param CombatFXData - Pooled FX data from WeaponData (can be nullptr)
     * @param AttackType - Attack type for pool lookup
     * @param WeaponFallbackVFX - Weapon's default hit VFX
     * @param ImpactLocation - World location for VFX spawn
     * @param ImpactNormal - Surface normal for alignment
     * @param bWasBlocked - Whether the hit was blocked
     * @param BoneName - Bone that was hit (for attached VFX)
     * @return True if VFX was spawned
     */
    UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|VFX")
    static bool ResolveAndSpawnImpactVFX(
        UWorld* World,
        const FImpactVFXConfig& VFXConfig,
        const UCombatFXData* CombatFXData,
        EAttackType AttackType,
        UNiagaraSystem* WeaponFallbackVFX,
        const FVector& ImpactLocation,
        const FVector& ImpactNormal,
        bool bWasBlocked = false,
        FName BoneName = NAME_None);

    // ========================================================================
    // ACTOR TIME DILATION (Per-Actor Effects)
    // ========================================================================

    /**
     * Set time dilation on specific actor (for Sakurai-style selective hitstop).
     * Allows freezing specific actors while world continues.
     *
     * @param Actor - Actor to affect
     * @param TimeDilation - Time scale (0.0 = frozen, 1.0 = normal)
     */
    UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|Time")
    static void SetActorTimeDilation(AActor* Actor, float TimeDilation);

    /**
     * Release this compatibility caller's actor-dilation lease.
     *
     * @param Actor - Actor to restore
     */
    UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|Time")
    static void RestoreActorTimeDilation(AActor* Actor);

    /**
     * Freeze multiple actors simultaneously (for paired animation hitstop).
     * Sets CustomTimeDilation to 0.0001f (near-zero, avoids division-by-zero).
     *
     * @param Actors - Array of actors to freeze
     */
    UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|Time")
    static void FreezeActors(const TArray<AActor*>& Actors);

    /**
     * Release one compatibility freeze lease for each actor.
     *
     * @param Actors - Array of actors to restore
     */
    UFUNCTION(BlueprintCallable, Category = "Cinematic Effects|Time")
    static void RestoreActors(const TArray<AActor*>& Actors);

    /**
     * Freeze multiple actors and save their pre-freeze time dilations.
     * Use with RestoreActorsFromSaved() to correctly restore overlapping slow-mo.
     *
     * @param Actors - Array of actors to freeze
     * @return Map of actor -> saved CustomTimeDilation before freeze
     */
    static TMap<TWeakObjectPtr<AActor>, float> FreezeActorsWithSave(const TArray<AActor*>& Actors);

    /**
     * Restore multiple actors to their saved pre-freeze time dilations.
     * Counterpart to FreezeActorsWithSave().
     *
     * @param SavedDilations - Map returned by FreezeActorsWithSave()
     */
    static void RestoreActorsFromSaved(const TMap<TWeakObjectPtr<AActor>, float>& SavedDilations);
};
