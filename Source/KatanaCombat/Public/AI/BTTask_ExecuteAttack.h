// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ExecuteAttack.generated.h"

/**
 * BT Task: Execute Attack
 *
 * Executes the attack that was selected when the token was granted.
 * Should only be called when:
 * - Enemy has an attack token (from TryInitiateAttack)
 * - Enemy is in Approaching state and within attack range
 *
 * The task:
 * - Plays the attack montage with CounterWindow notifies
 * - Waits for montage completion
 * - Returns Succeeded on normal completion, Failed if interrupted
 *
 * Note: Token release is handled automatically by EnemyCombatAIComponent
 * when the attack ends (normal or interrupted).
 */
UCLASS()
class KATANACOMBAT_API UBTTask_ExecuteAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ExecuteAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
