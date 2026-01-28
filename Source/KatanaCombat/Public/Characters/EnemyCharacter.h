// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCombatCharacter.h"
#include "EnemyCharacter.generated.h"

/**
 * Base class for AI-controlled enemy characters
 * Inherits combat functionality from BaseCombatCharacter
 *
 * This class provides:
 * - Default enemy team assignment
 * - Foundation for AI behavior integration
 * - Common enemy configuration options
 *
 * For specialized enemies, create derived classes (e.g., AMinibossCharacter, ABossCharacter)
 */
UCLASS()
class KATANACOMBAT_API AEnemyCharacter : public ABaseCombatCharacter
{
    GENERATED_BODY()

public:
    AEnemyCharacter();

    // ========================================================================
    // ENEMY CONFIGURATION
    // ========================================================================

    /** Display name for UI purposes (health bar, lock-on indicator) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Info")
    FText DisplayName;

    /** Whether this enemy drops items on death */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Loot")
    bool bDropsLoot = true;

    /** Experience value when killed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Loot")
    int32 ExperienceValue = 100;

protected:
    virtual void BeginPlay() override;

    /**
     * Override death handling for enemy-specific behavior
     * (spawning loot, notifying AI manager, etc.)
     */
    virtual void HandleDeath_Implementation(AActor* Killer) override;
};
