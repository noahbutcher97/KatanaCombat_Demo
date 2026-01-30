// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"

// ============================================================================
// LOG CATEGORY FOR DEBUG UTILITIES
// ============================================================================
DECLARE_LOG_CATEGORY_EXTERN(LogDebug, Log, All);

/**
 * Combat Debug Configuration
 *
 * Centralized CVar-based debug control for all combat systems.
 * Use console commands to enable/disable debug visualization at runtime.
 *
 * Console Commands:
 *   Combat.Debug.All 1         - Enable all debug visualization
 *   Combat.Debug.Direction 1   - Direction transformation arrows
 *   Combat.Debug.Targeting 1   - Targeting cones and targets
 *   Combat.Debug.Weapon 1      - Weapon trace visualization
 *   Combat.Debug.Phase 1       - Attack phase indicators
 *   Combat.Debug.Queue 1       - Action queue state
 *   Combat.Debug.Hold 1        - Hold state visualization
 *   Combat.Debug.LogVerbose 1  - Enable verbose logging
 *
 * Benefits:
 *   - Runtime control via console (no recompile needed)
 *   - Single source of truth (no scattered bDebugDraw flags)
 *   - Granular control (enable only what you need)
 *   - Saved in console history for quick toggle
 */
namespace CombatDebug
{
    // ========================================================================
    // CVAR DECLARATIONS (defined in DebugConfig.cpp)
    // ========================================================================

    /** Master toggle - enables ALL debug visualization */
    extern TAutoConsoleVariable<int32> CVarDebugAll;

    /** Direction transformation arrows (camera -> input -> character-relative -> attack) */
    extern TAutoConsoleVariable<int32> CVarDebugDirection;

    /** Targeting system (cones, potential targets, selected target) */
    extern TAutoConsoleVariable<int32> CVarDebugTargeting;

    /** Weapon trace visualization (swept sphere traces, hits) */
    extern TAutoConsoleVariable<int32> CVarDebugWeapon;

    /** Attack phase indicators (Windup/Active/Recovery state) */
    extern TAutoConsoleVariable<int32> CVarDebugPhase;

    /** Action queue state (queued actions, current action) */
    extern TAutoConsoleVariable<int32> CVarDebugQueue;

    /** Hold state visualization (hold active indicator, direction capture) */
    extern TAutoConsoleVariable<int32> CVarDebugHold;

    /** Debug draw duration in seconds (0 = single frame) */
    extern TAutoConsoleVariable<float> CVarDebugDrawDuration;

    /** Enable verbose logging to output log */
    extern TAutoConsoleVariable<int32> CVarDebugLogVerbose;

    /** Environment/terrain visualization (slopes, ground detection, alignment) */
    extern TAutoConsoleVariable<int32> CVarDebugEnvironment;

    /** Paired animation debug (finishers, counters, warp tracking) */
    extern TAutoConsoleVariable<int32> CVarDebugPairedAnim;

    /** Paired animation sub-toggle: Warp target visualization */
    extern TAutoConsoleVariable<int32> CVarDebugPairedAnimWarp;

    /** Paired animation sub-toggle: Partner connection lines */
    extern TAutoConsoleVariable<int32> CVarDebugPairedAnimPartners;

    /** Paired animation sub-toggle: Sync point visualization */
    extern TAutoConsoleVariable<int32> CVarDebugPairedAnimSync;

    /** Paired animation sub-toggle: Vulnerability indicators */
    extern TAutoConsoleVariable<int32> CVarDebugPairedAnimVulnerability;

    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================

    /** Check if any debug visualization is enabled */
    FORCEINLINE bool IsDebugEnabled()
    {
        return CVarDebugAll.GetValueOnGameThread() != 0;
    }

    /** Check if direction debug is enabled (standalone or via master toggle) */
    FORCEINLINE bool IsDirectionDebugEnabled()
    {
        return IsDebugEnabled() || CVarDebugDirection.GetValueOnGameThread() != 0;
    }

    /** Check if targeting debug is enabled (standalone or via master toggle) */
    FORCEINLINE bool IsTargetingDebugEnabled()
    {
        return IsDebugEnabled() || CVarDebugTargeting.GetValueOnGameThread() != 0;
    }

    /** Check if weapon debug is enabled (standalone or via master toggle) */
    FORCEINLINE bool IsWeaponDebugEnabled()
    {
        return IsDebugEnabled() || CVarDebugWeapon.GetValueOnGameThread() != 0;
    }

    /** Check if phase debug is enabled (standalone or via master toggle) */
    FORCEINLINE bool IsPhaseDebugEnabled()
    {
        return IsDebugEnabled() || CVarDebugPhase.GetValueOnGameThread() != 0;
    }

    /** Check if queue debug is enabled (standalone or via master toggle) */
    FORCEINLINE bool IsQueueDebugEnabled()
    {
        return IsDebugEnabled() || CVarDebugQueue.GetValueOnGameThread() != 0;
    }

    /** Check if hold debug is enabled (standalone or via master toggle) */
    FORCEINLINE bool IsHoldDebugEnabled()
    {
        return IsDebugEnabled() || CVarDebugHold.GetValueOnGameThread() != 0;
    }

    /** Check if verbose logging is enabled */
    FORCEINLINE bool IsVerboseLogEnabled()
    {
        return CVarDebugLogVerbose.GetValueOnGameThread() != 0;
    }

    /** Get debug draw duration (0 = single frame) */
    FORCEINLINE float GetDebugDrawDuration()
    {
        return CVarDebugDrawDuration.GetValueOnGameThread();
    }

    /** Check if environment/terrain debug is enabled (standalone or via master toggle) */
    FORCEINLINE bool IsEnvironmentDebugEnabled()
    {
        return IsDebugEnabled() || CVarDebugEnvironment.GetValueOnGameThread() != 0;
    }

    /** Check if paired animation debug is enabled (standalone or via master toggle) */
    FORCEINLINE bool IsPairedAnimDebugEnabled()
    {
        return IsDebugEnabled() || CVarDebugPairedAnim.GetValueOnGameThread() != 0;
    }

    /** Check if paired animation warp debug is enabled */
    FORCEINLINE bool IsPairedAnimWarpDebugEnabled()
    {
        return IsPairedAnimDebugEnabled() || CVarDebugPairedAnimWarp.GetValueOnGameThread() != 0;
    }

    /** Check if paired animation partner connection debug is enabled */
    FORCEINLINE bool IsPairedAnimPartnerDebugEnabled()
    {
        return IsPairedAnimDebugEnabled() || CVarDebugPairedAnimPartners.GetValueOnGameThread() != 0;
    }

    /** Check if paired animation sync point debug is enabled */
    FORCEINLINE bool IsPairedAnimSyncDebugEnabled()
    {
        return IsPairedAnimDebugEnabled() || CVarDebugPairedAnimSync.GetValueOnGameThread() != 0;
    }

    /** Check if paired animation vulnerability debug is enabled */
    FORCEINLINE bool IsPairedAnimVulnerabilityDebugEnabled()
    {
        return IsPairedAnimDebugEnabled() || CVarDebugPairedAnimVulnerability.GetValueOnGameThread() != 0;
    }
}
