// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponData.generated.h"

class UStaticMesh;
class UAnimMontage;
class UAttackConfiguration;
class USoundBase;
class UCombatFXData;

/**
 * Defines a weapon's visual, combat, and configuration properties
 *
 * Integration:
 * - WeaponComponent reads this to spawn mesh, configure sockets, apply damage multipliers
 * - AttackConfiguration defines the moveset (light/heavy attacks, combos)
 * - Different WeaponData assets = different weapon types with unique movesets
 *
 * Example Setup:
 * - WD_Katana: Fast attacks, standard trace, katana mesh
 * - WD_Greatsword: Slow attacks, larger trace radius, greatsword mesh
 * - WD_DualBlades: Rapid combos, two trace points, dual blade mesh
 *
 * Socket Requirements:
 * - Character mesh needs: EquippedSocket (default: "weapon_r"), HolsteredSocket (default: "weapon_back")
 * - Weapon mesh needs: TraceStartSocket (default: "weapon_start"), TraceEndSocket (default: "weapon_end")
 *   OR use character mesh sockets if weapon has no skeletal mesh
 */
UCLASS(BlueprintType)
class KATANACOMBAT_API UWeaponData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UWeaponData();

    // ============================================================================
    // IDENTIFICATION
    // ============================================================================

    /** Display name for UI and debugging */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Identity")
    FText DisplayName;

    /** Optional description */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Identity", meta = (MultiLine = true))
    FText Description;

    // ============================================================================
    // VISUAL - MESH
    // ============================================================================

    /** Static mesh to spawn for this weapon (nullptr = no visible weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
    TSoftObjectPtr<UStaticMesh> WeaponMesh;

    /** Scale applied to weapon mesh */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
    FVector MeshScale = FVector(1.0f, 1.0f, 1.0f);

    /** Transform offset applied when attached (fine-tune position/rotation in hand) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visual")
    FTransform MeshAttachOffset;

    // ============================================================================
    // ATTACHMENT SOCKETS (on character mesh)
    // ============================================================================

    /** Socket on character mesh for equipped (in-hand) position */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Sockets")
    FName EquippedSocket = "weapon_r";

    /** Socket on character mesh for holstered position */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Sockets")
    FName HolsteredSocket = "weapon_back";

    // ============================================================================
    // EQUIP/HOLSTER ANIMATIONS
    // ============================================================================

    /**
     * Montage to play when drawing this weapon
     * Should contain AnimNotify_WeaponEquip at the frame where weapon moves to hand
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animations")
    TObjectPtr<UAnimMontage> EquipMontage;

    /**
     * Montage to play when sheathing this weapon
     * Should contain AnimNotify_WeaponHolster at the frame where weapon moves to holster
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animations")
    TObjectPtr<UAnimMontage> HolsterMontage;

    /** Playrate for equip animation (1.0 = normal speed) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animations",
        meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float EquipPlayRate = 1.0f;

    /** Playrate for holster animation (1.0 = normal speed) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animations",
        meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float HolsterPlayRate = 1.0f;

    // ============================================================================
    // HIT DETECTION (trace configuration)
    // ============================================================================

    /**
     * Socket for trace start point
     * If weapon has skeletal mesh: use socket on weapon mesh
     * If weapon is static mesh: use socket on character mesh
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Hit Detection")
    FName TraceStartSocket = "weapon_start";

    /**
     * Socket for trace end point
     * If weapon has skeletal mesh: use socket on weapon mesh
     * If weapon is static mesh: use socket on character mesh
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Hit Detection")
    FName TraceEndSocket = "weapon_end";

    /** Radius of swept sphere trace */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Hit Detection",
        meta = (ClampMin = "1.0", ClampMax = "50.0"))
    float TraceRadius = 5.0f;

    /**
     * If true, trace sockets are on the character mesh (for static mesh weapons)
     * If false, trace sockets are on the weapon's skeletal mesh component
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Hit Detection")
    bool bUseCharacterSocketsForTrace = true;

    // ============================================================================
    // COMBAT CONFIGURATION
    // ============================================================================

    /**
     * Attack moveset for this weapon
     * Defines: DefaultLightAttack, DefaultHeavyAttack, movement attacks
     * Each attack has its own combo chains, directional follow-ups, etc.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
    TObjectPtr<UAttackConfiguration> AttackConfiguration;

    /**
     * Damage multiplier applied to all attacks with this weapon
     * Stacks with AttackData::BaseDamage
     * Example: Greatsword = 1.5x, Dagger = 0.8x
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat",
        meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float DamageMultiplier = 1.0f;

    /**
     * Weapon reach for targeting system adjustments
     * Longer weapons can hit from further away
     * Used to adjust soft aim assist range
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat",
        meta = (ClampMin = "50.0", ClampMax = "500.0"))
    float WeaponReach = 150.0f;

    // ============================================================================
    // AUDIO/VFX (Weapon Defaults - Fallback for AttackData)
    // ============================================================================

    /** Default hit sound for this weapon (fallback when AttackData has no sound) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Effects")
    TObjectPtr<USoundBase> HitSound;

    /**
     * Pooled impact FX data for this weapon.
     * Maps attack types to pools of sounds/VFX with random selection.
     * When set, acts as middle-tier fallback between per-attack overrides and weapon defaults.
     *
     * Resolution: AttackData.ImpactSound -> CombatFXData pool -> HitSound -> nothing
     *
     * nullptr = skip pool layer, use existing AttackData -> HitSound fallback
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Effects")
    TObjectPtr<UCombatFXData> CombatFXData;

    // TODO: Uncomment when VFX system ships (U-16)
    // UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Effects")
    // TObjectPtr<UNiagaraSystem> HitVFX;

    // TODO: Uncomment when weapon swing/trail system ships
    // UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Effects")
    // TObjectPtr<USoundBase> SwingSound;
    // UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Effects")
    // TObjectPtr<UNiagaraSystem> TrailEffect;

    // ============================================================================
    // UTILITY
    // ============================================================================

    /** Get display name as string */
    UFUNCTION(BlueprintPure, Category = "Weapon")
    FString GetDisplayNameString() const { return DisplayName.ToString(); }

    /** Check if this weapon data has a valid mesh assigned */
    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool HasValidMesh() const { return !WeaponMesh.IsNull(); }

    /** Check if this weapon data has a valid attack configuration */
    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool HasValidAttackConfiguration() const { return AttackConfiguration != nullptr; }

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
