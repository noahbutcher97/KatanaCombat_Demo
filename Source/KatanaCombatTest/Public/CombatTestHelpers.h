// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Characters/PlayerCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Animation/AnimMontage.h"
#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/CombatSettings.h"
#include "Data/AttackData.h"
#include "Data/AttackConfiguration.h"
#include "Data/WeaponData.h"
#include "Core/HitReactionComponent.h"
#include "Core/WeaponComponent.h"
#include "Characters/BaseCombatCharacter.h"
#include "Interfaces/DamageableInterface.h"
#include "UObject/StrongObjectPtr.h"

class UHitReactionComponent;
class UWeaponComponent;

/**
 * Shared test utilities for KatanaCombat test suite
 * Provides helper functions for creating test worlds, characters, and combat data
 */
class KATANACOMBATTEST_API FCombatTestHelpers
{
public:
    /** Configure a reusable synthetic weapon mesh with valid trace sockets. */
    static void ConfigureTestWeaponTraceSockets(ABaseCombatCharacter* Character)
    {
        if (!Character || !Character->CombatSettings || !Character->WeaponComponent)
        {
            return;
        }

        static TStrongObjectPtr<UStaticMesh> TestWeaponMesh;
        if (!TestWeaponMesh.IsValid())
        {
            UStaticMesh* TemplateMesh = LoadObject<UStaticMesh>(
                nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
            UStaticMesh* Mesh = TemplateMesh
                ? DuplicateObject<UStaticMesh>(TemplateMesh, GetTransientPackage())
                : nullptr;
            if (!Mesh)
            {
                return;
            }

            UStaticMeshSocket* StartSocket = NewObject<UStaticMeshSocket>(Mesh);
            StartSocket->SocketName = TEXT("weapon_start");
            StartSocket->RelativeLocation = FVector::ZeroVector;
            Mesh->AddSocket(StartSocket);

            UStaticMeshSocket* EndSocket = NewObject<UStaticMeshSocket>(Mesh);
            EndSocket->SocketName = TEXT("weapon_end");
            EndSocket->RelativeLocation = FVector(100.0f, 0.0f, 0.0f);
            Mesh->AddSocket(EndSocket);
            TestWeaponMesh.Reset(Mesh);
        }

        UWeaponData* WeaponData = Character->CombatSettings->DefaultWeaponData;
        if (!WeaponData || !TestWeaponMesh.IsValid())
        {
            return;
        }
        WeaponData->WeaponMesh = TestWeaponMesh.Get();
        WeaponData->bUseCharacterSocketsForTrace = false;
        WeaponData->EquippedSocket = NAME_None;
        WeaponData->HolsteredSocket = NAME_None;
        Character->WeaponComponent->InitializeFromWeaponData(WeaponData, true);
    }

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
     * Create minimal combat settings for testing
     * @return Combat settings configured with test defaults
     */
    static UCombatSettings* CreateTestCombatSettings()
    {
        UCombatSettings* Settings = NewObject<UCombatSettings>();
        //Settings->MaxPosture = 100.0f;
        //Settings->PostureRegenRate_Idle = 20.0f;
        //Settings->PostureRegenRate_Attacking = 50.0f;
        //Settings->PostureRegenRate_NotBlocking = 30.0f;

        // Create DefaultWeaponData with AttackConfiguration (new pattern)
        UWeaponData* TestWeaponData = NewObject<UWeaponData>();
        TestWeaponData->AttackConfiguration = NewObject<UAttackConfiguration>();
        Settings->DefaultWeaponData = TestWeaponData;

        //Settings->CounterWindowDuration = 1.5f;
        //Settings->CounterDamageMultiplier = 1.5f;
        return Settings;
    }

    /**
     * Create a test character with combat component and settings
     * @param World - World to spawn character in
     * @param OutCombat - Output parameter for created combat component
     * @return Created character
     */
    static APlayerCharacter* CreateTestCharacterWithCombat(
        UWorld* World,
        UCombatComponent*& OutCombat,
        bool bConfigureWeaponTraceSockets = true)
    {
        APlayerCharacter* Character = World->SpawnActor<APlayerCharacter>();

        // Setup minimal combat settings
        Character->CombatSettings = CreateTestCombatSettings();

        // Get the combat component (created by character constructor)
        OutCombat = Character->CombatComponent;

        if (bConfigureWeaponTraceSockets)
        {
            ConfigureTestWeaponTraceSockets(Character);
        }

        return Character;
    }

    /**
     * Create a test character with combat and targeting components
     * @param World - World to spawn character in
     * @param OutCombat - Output parameter for created combat component
     * @param OutTargeting - Output parameter for created targeting component
     * @return Created character
     */
    static APlayerCharacter* CreateTestCharacterWithCombatAndTargeting(
        UWorld* World,
        UCombatComponent*& OutCombat,
        UTargetingComponent*& OutTargeting)
    {
        APlayerCharacter* Character = CreateTestCharacterWithCombat(World, OutCombat);

        // Get the existing targeting component (created by character constructor)
        OutTargeting = Character->TargetingComponent;

        return Character;
    }

    /**
     * Create a player character for testing
     * @param World - World to spawn character in
     * @param Location - Spawn location (default: origin)
     * @return Created player character
     */
    static APlayerCharacter* CreateTestPlayerCharacter(
        UWorld* World,
        FVector Location = FVector::ZeroVector,
        bool bConfigureWeaponTraceSockets = true)
    {
        FActorSpawnParameters SpawnParams;
        APlayerCharacter* Character = World->SpawnActor<APlayerCharacter>(APlayerCharacter::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);

        // Setup minimal combat settings
        Character->CombatSettings = CreateTestCombatSettings();

        if (bConfigureWeaponTraceSockets)
        {
            ConfigureTestWeaponTraceSockets(Character);
        }

        return Character;
    }

    /**
     * Create an enemy character for testing
     * @param World - World to spawn character in
     * @param Location - Spawn location (default: origin)
     * @return Created enemy character
     */
    static AEnemyCharacter* CreateTestEnemyCharacter(
        UWorld* World,
        FVector Location = FVector::ZeroVector,
        bool bConfigureWeaponTraceSockets = true)
    {
        FActorSpawnParameters SpawnParams;
        AEnemyCharacter* Character = World->SpawnActor<AEnemyCharacter>(AEnemyCharacter::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);

        // Setup minimal combat settings
        Character->CombatSettings = CreateTestCombatSettings();

        if (bConfigureWeaponTraceSockets)
        {
            ConfigureTestWeaponTraceSockets(Character);
        }

        return Character;
    }

    /**
     * Create a combat scenario with player and enemies
     * @param World - World to spawn actors in
     * @param OutPlayer - Output parameter for created player character
     * @param OutEnemies - Output array of created enemy characters
     * @param EnemyCount - Number of enemies to create
     * @param EnemyDistance - Distance from player to spawn enemies
     */
    static void CreateCombatScenario(
        UWorld* World,
        APlayerCharacter*& OutPlayer,
        TArray<AEnemyCharacter*>& OutEnemies,
        int32 EnemyCount = 1,
        float EnemyDistance = 300.0f)
    {
        // Create player at origin
        OutPlayer = CreateTestPlayerCharacter(World, FVector::ZeroVector);

        // Create enemies in a semicircle in front of the player
        OutEnemies.Empty();
        OutEnemies.Reserve(EnemyCount);

        const float AngleStep = (EnemyCount > 1) ? 90.0f / (EnemyCount - 1) : 0.0f;
        const float StartAngle = (EnemyCount > 1) ? -45.0f : 0.0f;

        for (int32 i = 0; i < EnemyCount; ++i)
        {
            const float Angle = FMath::DegreesToRadians(StartAngle + (AngleStep * i));
            FVector EnemyLocation(
                FMath::Cos(Angle) * EnemyDistance,
                FMath::Sin(Angle) * EnemyDistance,
                0.0f
            );

            AEnemyCharacter* Enemy = CreateTestEnemyCharacter(World, EnemyLocation);
            OutEnemies.Add(Enemy);
        }
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

    /**
     * Create a hit reaction info for testing damage application
     * @param Attacker - Actor dealing the damage
     * @param Damage - Base damage amount
     * @param HitDirection - Direction of the hit (world space)
     * @param AttackData - Optional attack data for damage type
     * @return Configured FHitReactionInfo
     */
    static FHitReactionInfo CreateTestHitInfo(
        AActor* Attacker = nullptr,
        float Damage = 25.0f,
        FVector HitDirection = FVector::ForwardVector,
        UAttackData* AttackData = nullptr)
    {
        FHitReactionInfo HitInfo;
        HitInfo.Attacker = Attacker;
        HitInfo.Damage = Damage;
        HitInfo.HitDirection = HitDirection;
        HitInfo.AttackData = AttackData;
        HitInfo.StunDuration = 0.2f;
        HitInfo.bWasCounter = false;
        HitInfo.ImpactPoint = FVector::ZeroVector;
        return HitInfo;
    }

    /**
     * Get HitReactionComponent from a character
     * @param Character - Character to query
     * @return HitReactionComponent or nullptr
     */
    static UHitReactionComponent* GetHitReactionComponent(ACharacter* Character)
    {
        if (!Character)
        {
            return nullptr;
        }
        return Character->FindComponentByClass<UHitReactionComponent>();
    }

    /**
     * Get WeaponComponent from a character
     * @param Character - Character to query
     * @return WeaponComponent or nullptr
     */
    static UWeaponComponent* GetWeaponComponent(ACharacter* Character)
    {
        if (!Character)
        {
            return nullptr;
        }
        return Character->FindComponentByClass<UWeaponComponent>();
    }

    /**
     * Simulate dealing lethal damage to a character
     * @param Target - Character to damage
     * @param Attacker - Actor dealing the damage
     * @return True if character died
     *
     * NOTE: In tests, the animation system isn't running, so death montages never
     * complete. We manually call FinalizeDeath() to transition from DYING to DEAD
     * state after applying lethal damage. This is appropriate for unit tests that
     * don't need to verify animation playback.
     */
    static bool DealLethalDamage(ABaseCombatCharacter* Target, AActor* Attacker = nullptr)
    {
        if (!Target)
        {
            return false;
        }

        // Create damage info that will kill the target
        FHitReactionInfo LethalHit = CreateTestHitInfo(Attacker, Target->MaxHealth + 100.0f);

        // Apply damage through the interface
        if (Target->Implements<UDamageableInterface>())
        {
            IDamageableInterface::Execute_ApplyDamage(Target, LethalHit);
        }

        // In tests, animation system isn't running, so death montages never complete.
        // Manually finalize death if character entered DYING state (health <= 0).
        // This transitions bIsDying -> bIsDead, which is what tests expect.
        if (Target->bIsDying && !Target->bIsDead)
        {
            Target->FinalizeDeath();
        }

        return Target->bIsDead;
    }

    /**
     * Check if a character is dead (via bIsDead flag)
     * @param Character - Character to check
     * @return True if dead
     */
    static bool IsCharacterDead(ABaseCombatCharacter* Character)
    {
        return Character ? Character->bIsDead : false;
    }

    /**
     * Finalize death for a dying character (test utility)
     *
     * In tests, animation systems don't run, so the death montage completion
     * that normally calls FinalizeDeath() never happens. Call this after
     * ApplyDamage when you expect the character to die.
     *
     * @param Character - Character that may be dying
     * @return True if character is now dead (bIsDead = true)
     */
    static bool FinalizeDeathIfDying(ABaseCombatCharacter* Character)
    {
        if (Character && Character->bIsDying && !Character->bIsDead)
        {
            Character->FinalizeDeath();
        }
        return Character ? Character->bIsDead : false;
    }

    /**
     * Set character health directly for testing
     * @param Character - Character to modify
     * @param NewHealth - New health value
     */
    static void SetCharacterHealth(ABaseCombatCharacter* Character, float NewHealth)
    {
        if (Character)
        {
            Character->SetHealth(NewHealth);
        }
    }

    /**
     * Get character health for testing
     * @param Character - Character to query
     * @return Current health value
     */
    static float GetCharacterHealth(ABaseCombatCharacter* Character)
    {
        return Character ? Character->CurrentHealth : 0.0f;
    }
};
