// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_RequestAttackToken.generated.h"

/**
 * BT Task: Request Attack Token
 *
 * Attempts to request an attack token from the combat token subsystem.
 * - If token granted immediately: Succeeds
 * - If added to queue: Fails (enemy should continue circling)
 * - If denied: Fails
 *
 * Uses EnemyCombatAIComponent::TryInitiateAttack() which handles
 * attack selection and state transitions internally.
 */
UCLASS()
class KATANACOMBAT_API UBTTask_RequestAttackToken : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_RequestAttackToken();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
