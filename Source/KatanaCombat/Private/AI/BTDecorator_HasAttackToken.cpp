// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTDecorator_HasAttackToken.h"
#include "AI/EnemyCombatAIComponent.h"
#include "AIController.h"

UBTDecorator_HasAttackToken::UBTDecorator_HasAttackToken()
{
	NodeName = TEXT("Has Attack Token");
}

bool UBTDecorator_HasAttackToken::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return false;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return false;
	}

	UEnemyCombatAIComponent* CombatAI = Pawn->FindComponentByClass<UEnemyCombatAIComponent>();
	if (!CombatAI)
	{
		return false;
	}

	return CombatAI->HasAttackToken();
}

FString UBTDecorator_HasAttackToken::GetStaticDescription() const
{
	return TEXT("Check if enemy has an attack token.\nTrue = can attack, False = must wait.");
}
