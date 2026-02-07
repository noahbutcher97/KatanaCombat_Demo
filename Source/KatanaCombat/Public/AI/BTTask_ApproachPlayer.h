// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ApproachPlayer.generated.h"

/**
 * BT Task: Approach Player
 *
 * Moves the enemy toward the player until within attack range.
 * Used after obtaining an attack token, before executing attack.
 *
 * Behavior:
 * - Uses AIController::MoveToActor for dynamic tracking
 * - Succeeds when IsInAttackRange() returns true
 * - Fails if approach timeout expires or movement fails
 * - Respects attack range from EnemyCombatAIComponent::ApproachConfig
 */
UCLASS()
class KATANACOMBAT_API UBTTask_ApproachPlayer : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ApproachPlayer();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** How often to check if we're in range */
	UPROPERTY(EditAnywhere, Category = "Approach", meta = (ClampMin = 0.05))
	float RangeCheckInterval = 0.1f;

private:
	/** Time elapsed since approach started */
	float ApproachTime = 0.0f;

	/** Time since last range check */
	float TimeSinceRangeCheck = 0.0f;
};
