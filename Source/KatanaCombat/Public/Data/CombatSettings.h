// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatTypes.h"
#include "CombatSettings.generated.h"

class UWeaponData;
class UAttackConfiguration;
class UTargetingSettings;
class UMotionWarpingSettings;
class UHitReactionSettings;
class UDefenseConfiguration;

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
 *   ├── WeaponData (weapon + moveset via AttackConfiguration)
 *   ├── HitReactionSettings (hit reactions, damage response)
 *   └── [Future: PostureSettings, CounterSettings, etc.]
 *
 * Override Pattern:
 *   Component.SettingsOverride → CombatSettings.SubsystemSettings → Hardcoded fallback
 *   (DefenseConfiguration additionally prepends transient DefenseStanceOverrides and falls
 *   back to the class CDO — see CombatComponent::GetEffectiveDefenseConfiguration.)
 *
 * Weapon/Attack Configuration Pattern:
 *   WeaponComponent.WeaponData (per-instance) → CombatSettings.DefaultWeaponData → nullptr
 *   WeaponData contains AttackConfiguration, so setting WeaponData implicitly sets moveset.
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

    /** [NOT WIRED] No system dereferences this slot — warping is driven by per-attack
     * FAttackWarpConfig (pending delete decision, see docs/audits/DATA_ASSET_AUDIT_2026-07-21.md). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subsystems")
    TObjectPtr<UMotionWarpingSettings> MotionWarpingSettings;

    /**
     * Default weapon data for this combat configuration.
     * Contains weapon properties (mesh, sockets, damage multiplier) AND
     * AttackConfiguration (moveset/default attacks).
     *
     * WeaponComponent can override this per-instance via WeaponDataOverride.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subsystems")
    TObjectPtr<UWeaponData> DefaultWeaponData;

    /** Hit reaction configuration (directional reactions, special reactions) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subsystems")
    TObjectPtr<UHitReactionSettings> HitReactionSettings;

    /** Defense kinematics, threat policy, and presentation configuration */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subsystems")
    TObjectPtr<UDefenseConfiguration> DefenseConfiguration;

    /** Knockback per attack type; attacks override fields individually. Types missing from the map do not push. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockback")
    TMap<EAttackType, FKnockbackConfig> DefaultKnockback;

    // ============================================================================
    // FUTURE SUBSYSTEMS (Add as implemented)
    // ============================================================================
    // TObjectPtr<UPostureSettings> PostureSettings;
    // TObjectPtr<UCounterSettings> CounterSettings;

    // ============================================================================
    // DEPRECATED (Backward Compatibility)
    // ============================================================================

private:
    /**
     * @deprecated Use DefaultWeaponData->AttackConfiguration instead.
     * This field is kept for serialization compatibility with existing assets.
     * PostLoad migrates this to DefaultWeaponData->AttackConfiguration.
     *
     * IMPORTANT: Field name must remain "AttackConfiguration" for serialization compatibility
     * with existing CombatSettings assets that used the old direct field pattern.
     */
    UPROPERTY()
    TObjectPtr<UAttackConfiguration> AttackConfiguration;

public:

    // ============================================================================
    // CONVENIENCE ACCESSORS
    // ============================================================================

    /** Get AttackConfiguration from DefaultWeaponData (nullptr if no weapon) */
    UFUNCTION(BlueprintPure, Category = "Combat Settings")
    class UAttackConfiguration* GetAttackConfiguration() const;

    // ============================================================================
    // SERIALIZATION
    // ============================================================================

    /** Migrate deprecated AttackConfiguration to DefaultWeaponData on load */
    virtual void PostLoad() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
