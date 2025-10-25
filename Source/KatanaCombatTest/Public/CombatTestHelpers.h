// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/CombatSettings.h"
#include "Data/AttackData.h"

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
	static ACharacter* CreateTestCharacterWithCombat(UWorld* World, UCombatComponent*& OutCombat)
	{
		ACharacter* Character = World->SpawnActor<ACharacter>();
		OutCombat = NewObject<UCombatComponent>(Character);
		OutCombat->RegisterComponent();

		// Setup minimal combat settings
		UCombatSettings* Settings = NewObject<UCombatSettings>();
		OutCombat->CombatSettings = Settings;

		return Character;
	}

	/**
	 * Create a test character with combat and targeting components
	 * @param World - World to spawn character in
	 * @param OutCombat - Output parameter for created combat component
	 * @param OutTargeting - Output parameter for created targeting component
	 * @return Created character
	 */
	static ACharacter* CreateTestCharacterWithCombatAndTargeting(
		UWorld* World,
		UCombatComponent*& OutCombat,
		UTargetingComponent*& OutTargeting)
	{
		ACharacter* Character = CreateTestCharacterWithCombat(World, OutCombat);

		OutTargeting = NewObject<UTargetingComponent>(Character);
		OutTargeting->RegisterComponent();

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