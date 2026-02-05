// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatTypes.h"
#include "CinematicEffectsUtilityLibrary.generated.h"

/**
 * Cinematic Effects Utility Library
 *
 * Static utility functions for cinematic effects during combat:
 * - Time dilation (slow motion, hitstop)
 * - Camera shake triggering
 * - Per-hit hitstop (Sakurai-style freeze)
 * - Future: VFX spawning, post-process effects, screen effects
 *
 * Design: Separated from PairedAnimationUtilityLibrary to allow reuse
 * across various combat scenarios (not just paired animations).
 */
UCLASS()
class KATANACOMBAT_API UCinematicEffectsUtilityLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
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
     * Restore world time dilation to normal (1.0).
     * Safe to call multiple times - idempotent operation.
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
     * Restore actor time dilation to normal (1.0).
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
     * Restore multiple actors simultaneously (hardcodes to 1.0f).
     * Prefer RestoreActorsFromSaved() for overlapping slow-mo scenarios.
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
