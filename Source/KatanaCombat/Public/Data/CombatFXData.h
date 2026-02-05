// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatTypes.h"
#include "CombatFXData.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCombatFX, Log, All);

/**
 * Pooled impact FX data asset - maps EAttackType to arrays of sounds/VFX.
 * Random selection from pools reduces design time and prevents repetitive audio.
 *
 * Composition:
 * - WeaponData references this asset
 * - Each weapon can have unique impact character via different CombatFXData assets
 *
 * Resolution Chain (4-tier):
 *   1. AttackData.ImpactAudioConfig.ImpactSound (per-attack override)
 *   2. CombatFXData pool[AttackType] (random from pool - THIS)
 *   3. WeaponData.HitSound (simple weapon fallback)
 *   4. silent
 *
 * Pattern follows HitReactionSettings: UPrimaryDataAsset + TMap<EEnum, FStruct>
 */
UCLASS(BlueprintType)
class KATANACOMBAT_API UCombatFXData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UCombatFXData();

	// ========================================================================
	// ATTACK TYPE POOLS
	// ========================================================================

	/**
	 * Impact FX pools organized by attack type.
	 * Each attack type gets its own pool of sounds and VFX.
	 *
	 * Example setup:
	 *   Light  -> { 3 light slash sounds, 2 small spark VFX }
	 *   Heavy  -> { 2 heavy clang sounds, 2 large spark VFX }
	 *   Special -> { 1 unique finisher sound, 1 dramatic VFX }
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact FX")
	TMap<EAttackType, FImpactFXPool> AttackTypePools;

	// ========================================================================
	// SURFACE FX (Scaffold - not wired until bReturnPhysicalMaterial enabled)
	// ========================================================================

	/**
	 * Surface-specific FX overrides.
	 * When physical material info is available, surface type overrides
	 * attack type pool for audio (keeps VFX from attack type).
	 *
	 * [SCAFFOLD] - Not wired. bReturnPhysicalMaterial = false in weapon traces.
	 * Implementation requires WeaponComponent trace change + PhysMaterial mapping.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface FX (Scaffold)")
	TMap<ECombatSurfaceType, FImpactFXPool> SurfacePools;

	// ========================================================================
	// BLOCKED ATTACK FX
	// ========================================================================

	/**
	 * FX pool for when attacks are blocked.
	 * If configured, replaces the normal attack type pool sound on block.
	 * If not configured, uses the normal attack type pool.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact FX|Block")
	FImpactFXPool BlockedPool;

	/** Whether to use BlockedPool when attack is blocked */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact FX|Block")
	bool bUseBlockedPool = false;

	// ========================================================================
	// SELECTION API
	// ========================================================================

	/**
	 * Get the FX pool for an attack type.
	 * @param AttackType - Type of attack (Light, Heavy, Special)
	 * @return Pointer to FX pool, or nullptr if type not configured
	 */
	const FImpactFXPool* GetPoolForAttackType(EAttackType AttackType) const;

	/**
	 * Get the FX pool for a surface type (scaffold).
	 * @param SurfaceType - Physical surface type
	 * @return Pointer to FX pool, or nullptr if surface not configured
	 */
	const FImpactFXPool* GetPoolForSurface(ECombatSurfaceType SurfaceType) const;

	/**
	 * Resolve which pool to use given attack type, block state, and surface.
	 * Priority: Surface (if available) -> Blocked (if blocked + configured) -> AttackType
	 * @param AttackType - Attack type from AttackData
	 * @param bWasBlocked - Whether the hit was blocked
	 * @param SurfaceType - Surface type (Default if no PhysMaterial)
	 * @return Pointer to the best matching pool, or nullptr
	 */
	const FImpactFXPool* ResolvePool(
		EAttackType AttackType,
		bool bWasBlocked = false,
		ECombatSurfaceType SurfaceType = ECombatSurfaceType::Default) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
