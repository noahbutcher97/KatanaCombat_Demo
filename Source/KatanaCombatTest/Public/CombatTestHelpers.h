// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Characters/SamuraiCharacter.h"
#include "Animation/AnimMontage.h"
#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/CombatSettings.h"
#include "Data/AttackData.h"
#include "Data/AttackConfiguration.h"

/**
 * Shared test utilities for KatanaCombat test suite
 * Provides helper functions for creating test worlds, characters, and combat data
 */
class KATANACOMBATTEST_API FCombatTestHelpers
{
public:
	/**
	 * Create a minimal test world for combat tests
	 * @return New test world (must be destroyed with DestroyTestWorld)
	 */
	static UWorld* CreateTestWorld()
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		return World;
	}

	/**
	 * Destroy test world and clean up
	 * @param World - Test world to destroy
	 */
	static void DestroyTestWorld(UWorld* World)
	{
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	}

	/**
	 * Create a test character with combat component and settings
	 * @param World - World to spawn character in
	 * @param OutCombat - Output parameter for created combat component
	 * @return Created character
	 */
	static ASamuraiCharacter* CreateTestCharacterWithCombat(UWorld* World, UCombatComponent*& OutCombat)
	{
		ASamuraiCharacter* Character = World->SpawnActor<ASamuraiCharacter>();

		// Setup minimal attack configuration
		UAttackConfiguration* AttackConfig = NewObject<UAttackConfiguration>();
		// Default attacks are set by tests as needed

		// Setup minimal combat settings on the character (owner of CombatSettings)
		UCombatSettings* Settings = NewObject<UCombatSettings>();
		Settings->MaxPosture = 100.0f;
		Settings->PostureRegenRate_Idle = 20.0f;
		Settings->PostureRegenRate_Attacking = 50.0f;
		Settings->PostureRegenRate_NotBlocking = 30.0f;
		Settings->AttackConfiguration = AttackConfig;
		Settings->CounterWindowDuration = 1.5f;
		Settings->CounterDamageMultiplier = 1.5f;
		Character->CombatSettings = Settings;

		// Get the existing combat component (created by character constructor)
		OutCombat = Character->CombatComponent;

		return Character;
	}

	/**
	 * Create a test character with combat and targeting components
	 * @param World - World to spawn character in
	 * @param OutCombat - Output parameter for created combat component
	 * @param OutTargeting - Output parameter for created targeting component
	 * @return Created character
	 */
	static ASamuraiCharacter* CreateTestCharacterWithCombatAndTargeting(
		UWorld* World,
		UCombatComponent*& OutCombat,
		UTargetingComponent*& OutTargeting)
	{
		ASamuraiCharacter* Character = CreateTestCharacterWithCombat(World, OutCombat);

		// Get the existing targeting component (created by character constructor)
		OutTargeting = Character->TargetingComponent;

		return Character;
	}

	/**
	 * Create a basic attack data asset for testing
	 * @param Type - Type of attack (Light/Heavy)
	 * @return Created attack data
	 */
	static UAttackData* CreateTestAttack(EAttackType Type = EAttackType::Light)
	{
		UAttackData* Attack = NewObject<UAttackData>();
		Attack->AttackType = Type;
		Attack->BaseDamage = (Type == EAttackType::Light) ? 25.0f : 50.0f;
		Attack->PostureDamage = (Type == EAttackType::Light) ? 10.0f : 25.0f;
		Attack->bCanHold = (Type == EAttackType::Light);

		// Create a mock montage for testing
		// This allows ExecuteAttack to proceed without a real animation asset
		Attack->AttackMontage = NewObject<UAnimMontage>();

		return Attack;
	}

	/**
	 * Create a combo chain of attacks
	 * @param Length - Number of attacks in chain
	 * @param Type - Type of attacks
	 * @return First attack in chain (others linked via NextComboAttack)
	 */
	static UAttackData* CreateTestComboChain(int32 Length, EAttackType Type = EAttackType::Light)
	{
		if (Length <= 0)
		{
			return nullptr;
		}

		UAttackData* FirstAttack = CreateTestAttack(Type);
		UAttackData* CurrentAttack = FirstAttack;

		for (int32 i = 1; i < Length; ++i)
		{
			UAttackData* NextAttack = CreateTestAttack(Type);
			CurrentAttack->NextComboAttack = NextAttack;
			CurrentAttack = NextAttack;
		}

		return FirstAttack;
	}
};