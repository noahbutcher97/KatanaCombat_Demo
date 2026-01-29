// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatSettings.generated.h"

class UAttackConfiguration;
class UTargetingSettings;
class UMotionWarpingSettings;
class UHitReactionSettings;

/**
 * Root configuration for combat system - composes subsystem settings
 *
 * Design Pattern:
 * - CombatSettings is the root configuration assigned to characters
 * - References modular settings data assets for each subsystem
 * - Components read from their respective settings (with optional per-instance overrides)
 * - Different character classes can use different CombatSettings assets
 *
 * Hierarchy:
 *   CombatSettings (assigned to character)
 *   ├── TargetingSettings (targeting/soft aim)
 *   ├── MotionWarpingSettings (warp distances/speeds)
 *   ├── AttackConfiguration (moveset/default attacks)
 *   ├── HitReactionSettings (hit reactions, damage response)
 *   └── [Future: PostureSettings, CounterSettings, etc.]
 *
 * Override Pattern:
 *   Component.SettingsOverride → CombatSettings.SubsystemSettings → Hardcoded fallback
 *
 * Debug visualization controlled via CVars (see DebugConfig.h):
 * - Combat.Debug.All 1         - Enable all debug visualization
 * - Combat.Debug.Direction 1   - Direction transformation arrows
 * - Combat.Debug.Targeting 1   - Targeting cones and targets
 * - Combat.Debug.Weapon 1      - Weapon trace visualization
 */
UCLASS(BlueprintType)
class KATANACOMBAT_API UCombatSettings : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UCombatSettings();

    // ============================================================================
    // SUBSYSTEM SETTINGS (Modular Data Assets)
    // ============================================================================

    /** Targeting and soft aim assist configuration */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subsystems")
    TObjectPtr<UTargetingSettings> TargetingSettings;

    /** Motion warping distances and speeds */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subsystems")
    TObjectPtr<UMotionWarpingSettings> MotionWarpingSettings;

    /** Attack moveset configuration (default attacks, movement attacks) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subsystems")
    TObjectPtr<UAttackConfiguration> AttackConfiguration;

    /** Hit reaction configuration (directional reactions, special reactions) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subsystems")
    TObjectPtr<UHitReactionSettings> HitReactionSettings;

    // ============================================================================
    // FUTURE SUBSYSTEMS (Add as implemented)
    // ============================================================================
    // TObjectPtr<UPostureSettings> PostureSettings;
    // TObjectPtr<UCounterSettings> CounterSettings;
};
