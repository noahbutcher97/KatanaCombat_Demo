// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/DebugConfig.h"

// ============================================================================
// LOG CATEGORY DEFINITION
// ============================================================================
DEFINE_LOG_CATEGORY(LogDebug);

namespace CombatDebug
{
    // ========================================================================
    // CVAR DEFINITIONS
    // ========================================================================

    TAutoConsoleVariable<int32> CVarDebugAll(
        TEXT("Combat.Debug.All"),
        0,
        TEXT("Enable all combat debug visualization\n")
        TEXT("  0: Disabled (default)\n")
        TEXT("  1: Enable all debug systems"),
        ECVF_Default);

    TAutoConsoleVariable<int32> CVarDebugDirection(
        TEXT("Combat.Debug.Direction"),
        0,
        TEXT("Enable direction transformation debug arrows\n")
        TEXT("Shows: Camera -> Input -> Character-Relative -> Attack direction pipeline\n")
        TEXT("  0: Disabled (default)\n")
        TEXT("  1: Enabled"),
        ECVF_Default);

    TAutoConsoleVariable<int32> CVarDebugTargeting(
        TEXT("Combat.Debug.Targeting"),
        0,
        TEXT("Enable targeting system debug visualization\n")
        TEXT("Shows: Targeting cones, potential targets, selected target\n")
        TEXT("  0: Disabled (default)\n")
        TEXT("  1: Enabled"),
        ECVF_Default);

    TAutoConsoleVariable<int32> CVarDebugWeapon(
        TEXT("Combat.Debug.Weapon"),
        0,
        TEXT("Enable weapon trace debug visualization\n")
        TEXT("Shows: Swept sphere traces, hit points, trace paths\n")
        TEXT("  0: Disabled (default)\n")
        TEXT("  1: Enabled"),
        ECVF_Default);

    TAutoConsoleVariable<int32> CVarDebugPhase(
        TEXT("Combat.Debug.Phase"),
        0,
        TEXT("Enable attack phase debug visualization\n")
        TEXT("Shows: Current phase (Windup/Active/Recovery), phase transitions\n")
        TEXT("  0: Disabled (default)\n")
        TEXT("  1: Enabled"),
        ECVF_Default);

    TAutoConsoleVariable<int32> CVarDebugQueue(
        TEXT("Combat.Debug.Queue"),
        0,
        TEXT("Enable action queue debug visualization\n")
        TEXT("Shows: Queued actions, current executing action, queue state\n")
        TEXT("  0: Disabled (default)\n")
        TEXT("  1: Enabled"),
        ECVF_Default);

    TAutoConsoleVariable<int32> CVarDebugHold(
        TEXT("Combat.Debug.Hold"),
        0,
        TEXT("Enable hold state debug visualization\n")
        TEXT("Shows: Hold active indicator, captured direction, hold timing\n")
        TEXT("  0: Disabled (default)\n")
        TEXT("  1: Enabled"),
        ECVF_Default);

    TAutoConsoleVariable<float> CVarDebugDrawDuration(
        TEXT("Combat.Debug.DrawDuration"),
        0.0f,
        TEXT("Debug shape persistence duration in seconds\n")
        TEXT("  0.0: Single frame (updated each tick, default)\n")
        TEXT("  >0: Shapes persist for this duration"),
        ECVF_Default);

    TAutoConsoleVariable<int32> CVarDebugLogVerbose(
        TEXT("Combat.Debug.LogVerbose"),
        0,
        TEXT("Enable verbose combat logging to Output Log\n")
        TEXT("  0: Disabled (default)\n")
        TEXT("  1: Enabled - logs state transitions, action execution, etc."),
        ECVF_Default);
}
