// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_CirclePlayer.generated.h"

/**
 * BT Task: Circle Player
 *
 * Moves the enemy in a circling pattern around the player.
 * Used while waiting for an attack token or between attack attempts.
 *
 * Behavior:
 * - Calculates circling destination from EnemyCombatAIComponent
 * - Uses AIController::MoveToLocation for pathfinding
 * - Periodically changes circling direction for variety
 * - Continues until aborted (by gaining token or other reason)
 *
 * Note: This task runs indefinitely and expects to be aborted
 * by a higher-priority branch (e.g., when token is granted).
 */
UCLASS()
class KATANACOMBAT_API UBTTask_CirclePlayer : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_CirclePlayer();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** How often to recalculate circling destination */
	UPROPERTY(EditAnywhere, Category = "Circle", meta = (ClampMin = 0.1))
	float DestinationUpdateInterval = 0.5f;

	/** Acceptance radius for reaching circling waypoint */
	UPROPERTY(EditAnywhere, Category = "Circle", meta = (ClampMin = 10.0))
	float AcceptanceRadius = 50.0f;

private:
	/** Time since last destination update */
	float TimeSinceLastUpdate = 0.0f;
};
