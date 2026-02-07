// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_HasAttackToken.generated.h"

/**
 * BT Decorator: Has Attack Token
 *
 * Checks if the enemy currently holds an attack token.
 * Used to gate attack execution branches.
 *
 * Common usage pattern in Behavior Tree:
 * - Selector
 *   - Sequence [HasAttackToken = true]
 *     - BTTask_ApproachPlayer (move into range)
 *     - BTTask_ExecuteAttack (play attack)
 *   - Sequence [HasAttackToken = false]
 *     - BTTask_RequestAttackToken (try to get token)
 *     - BTTask_CirclePlayer (wait while circling)
 */
UCLASS()
class KATANACOMBAT_API UBTDecorator_HasAttackToken : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_HasAttackToken();

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;
};
