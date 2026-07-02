// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/EnemyCombatAIController.h"

const UStateTree* UEnemyStateTreeAIComponent::GetAssignedStateTree() const
{
	return StateTreeRef.GetStateTree();
}

void UEnemyStateTreeAIComponent::ValidateStateTreeReference()
{
	if (!StateTreeRef.IsValid())
	{
		return;
	}

	Super::ValidateStateTreeReference();
}

AEnemyCombatAIController::AEnemyCombatAIController()
{
	StateTreeAIComponent = CreateDefaultSubobject<UEnemyStateTreeAIComponent>(TEXT("StateTreeAIComponent"));
	check(StateTreeAIComponent);

	bStartAILogicOnPossess = true;
	bAttachToPawn = true;
}
