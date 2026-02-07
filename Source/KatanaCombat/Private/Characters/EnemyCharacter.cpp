// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/EnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AEnemyCharacter::AEnemyCharacter()
{
    // Set default team for enemy
    TeamId = ETeamId::Enemy;

    // Default display name
    DisplayName = FText::FromString(TEXT("Enemy"));

    // Configure character movement for AI
    GetCharacterMovement()->bUseControllerDesiredRotation = true;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
    GetCharacterMovement()->MaxWalkSpeed = 400.0f;

    // CAM-1 FIX: Use "Enemy" collision profiles that ignore camera
    // This prevents the spring arm from colliding with enemies
    GetCapsuleComponent()->SetCollisionProfileName(TEXT("Enemy"));
    GetMesh()->SetCollisionProfileName(TEXT("EnemyMesh"));
}

void AEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Additional enemy-specific initialization can go here
    // (e.g., register with AI manager, spawn alert indicator)
}

void AEnemyCharacter::HandleDeath_Implementation(AActor* Killer)
{
    // Call base implementation (disables movement, fires event)
    Super::HandleDeath_Implementation(Killer);

    // Enemy-specific death handling
    // TODO: Spawn loot if bDropsLoot
    // TODO: Award experience to killer
    // TODO: Notify AI manager of death
    // TODO: Play death animation/ragdoll

    // For now, just log the death
    UE_LOG(LogTemp, Log, TEXT("%s was killed by %s"),
        *GetName(),
        Killer ? *Killer->GetName() : TEXT("Unknown"));
}
