// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AttackConfiguration.generated.h"

// Forward declarations
class UAttackData;

/**
 * Attack Configuration Data Asset
 *
 * Defines the default attack moveset for a combat style.
 * Combos and directional follow-ups are defined within each UAttackData.
 *
 * Current System:
 * - Light/Heavy attacks use hold windows (AnimNotifyState_HoldWindow)
 * - During hold: Input direction → Execute DirectionalFollowUps from AttackData
 * - Charging: Built into heavy attacks via MaxChargeTime in AttackData
 * - Combos: Defined via NextComboAttack/HeavyComboAttack chains in AttackData
 *
 * Usage:
 * - Create different AttackConfiguration assets for different weapon types
 * - Reference from CombatSettings to create complete combat presets
 * - Mix and match with different CombatSettings for variety
 *
 * Example:
 * - AttackConfig_Katana: Fast, light attacks with quick directional follow-ups
 * - AttackConfig_Greatsword: Slow, heavy attacks with powerful directional slashes
 * - AttackConfig_DualBlades: Rapid light combo chains
 */
UCLASS(BlueprintType)
class KATANACOMBAT_API UAttackConfiguration : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UAttackConfiguration();

	// ============================================================================
	// BASE ATTACKS (REQUIRED)
	// ============================================================================

	/** Default light attack (tap light button from idle/recovery) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Attacks")
	TObjectPtr<UAttackData> DefaultLightAttack;

	/** Default heavy attack (tap heavy button from idle/recovery) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Attacks")
	TObjectPtr<UAttackData> DefaultHeavyAttack;

	// ============================================================================
	// MOVEMENT ATTACKS (OPTIONAL - Future expansion)
	// ============================================================================

	/** Sprint attack (attack while sprinting - not yet implemented) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement Attacks")
	TObjectPtr<UAttackData> SprintAttack;

	/** Jump attack (attack while jumping - not yet implemented) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement Attacks")
	TObjectPtr<UAttackData> JumpAttack;

	/** Plunging attack (attack while falling - not yet implemented) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement Attacks")
	TObjectPtr<UAttackData> PlungingAttack;
};