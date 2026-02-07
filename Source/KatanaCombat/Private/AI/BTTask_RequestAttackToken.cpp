// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTTask_RequestAttackToken.h"
#include "AI/EnemyCombatAIComponent.h"
#include "AIController.h"

UBTTask_RequestAttackToken::UBTTask_RequestAttackToken()
{
	NodeName = TEXT("Request Attack Token");
}

EBTNodeResult::Type UBTTask_RequestAttackToken::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	UEnemyCombatAIComponent* CombatAI = Pawn->FindComponentByClass<UEnemyCombatAIComponent>();
	if (!CombatAI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_RequestAttackToken] No EnemyCombatAIComponent on %s"),
			*Pawn->GetName());
		return EBTNodeResult::Failed;
	}

	// TryInitiateAttack handles attack selection, token request, and state transition
	bool bTokenGranted = CombatAI->TryInitiateAttack();

	return bTokenGranted ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

FString UBTTask_RequestAttackToken::GetStaticDescription() const
{
	return TEXT("Request attack token from combat system.\nSucceeds if token granted, fails if queued/denied.");
}
