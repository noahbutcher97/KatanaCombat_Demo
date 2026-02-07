// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyAITypes.generated.h"

/**
 * Enemy AI State Machine States
 * Used by EnemyCombatAIComponent to track high-level enemy behavior
 */
UENUM(BlueprintType)
enum class EEnemyAIState : uint8
{
	/** Standing, not engaged with player */
	Idle            UMETA(DisplayName = "Idle"),

	/** Moving around player, waiting for attack token */
	Circling        UMETA(DisplayName = "Circling"),

	/** Has attack token, moving into attack range */
	Approaching     UMETA(DisplayName = "Approaching"),

	/** Executing attack animation (has counter window) */
	Attacking       UMETA(DisplayName = "Attacking"),

	/** Post-attack recovery, releasing token */
	Recovering      UMETA(DisplayName = "Recovering"),

	/** Hit by player, staggered and vulnerable */
	Staggered       UMETA(DisplayName = "Staggered"),

	/** Playing death animation or ragdolling */
	Dying           UMETA(DisplayName = "Dying")
};

/**
 * Enemy attack selection mode
 * How the AI chooses which attack to perform
 */
UENUM(BlueprintType)
enum class EEnemyAttackSelection : uint8
{
	/** Always use the same attack */
	Single          UMETA(DisplayName = "Single Attack"),

	/** Random selection from available attacks */
	Random          UMETA(DisplayName = "Random"),

	/** Cycle through attacks in sequence */
	Sequential      UMETA(DisplayName = "Sequential"),

	/** Choose based on distance/situation */
	Contextual      UMETA(DisplayName = "Contextual")
};

/**
 * Configuration for enemy circling behavior
 */
USTRUCT(BlueprintType)
struct FEnemyCirclingConfig
{
	GENERATED_BODY()

	/** Distance to maintain from player while circling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circling")
	float CircleRadius = 400.0f;

	/** Movement speed while circling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circling")
	float CircleSpeed = 200.0f;

	/** How often to change circling direction (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circling")
	float DirectionChangeInterval = 3.0f;

	/** Randomization range for direction change interval */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circling")
	float DirectionChangeVariance = 1.0f;
};

/**
 * Configuration for enemy approach behavior
 */
USTRUCT(BlueprintType)
struct FEnemyApproachConfig
{
	GENERATED_BODY()

	/** Distance at which to start the attack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Approach")
	float AttackRange = 150.0f;

	/** Movement speed while approaching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Approach")
	float ApproachSpeed = 400.0f;

	/** Maximum time to spend approaching before giving up token */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Approach")
	float ApproachTimeout = 5.0f;
};

/**
 * Configuration for enemy attack behavior
 */
USTRUCT(BlueprintType)
struct FEnemyAttackConfig
{
	GENERATED_BODY()

	/** Attack data asset to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	TObjectPtr<class UAttackData> AttackData;

	/** Weight for random selection (higher = more likely) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = 0.1))
	float SelectionWeight = 1.0f;

	/** Minimum distance to player for this attack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float MinRange = 0.0f;

	/** Maximum distance to player for this attack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float MaxRange = 200.0f;
};
