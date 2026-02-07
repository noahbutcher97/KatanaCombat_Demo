// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTTask_ExecuteAttack.h"
#include "AI/EnemyCombatAIComponent.h"
#include "AI/EnemyAITypes.h"
#include "AIController.h"

UBTTask_ExecuteAttack::UBTTask_ExecuteAttack()
{
	NodeName = TEXT("Execute Attack");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_ExecuteAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_ExecuteAttack] No EnemyCombatAIComponent on %s"),
			*Pawn->GetName());
		return EBTNodeResult::Failed;
	}

	// Execute the attack
	bool bStarted = CombatAI->ExecuteAttack();

	if (!bStarted)
	{
		return EBTNodeResult::Failed;
	}

	// Task is now in progress - will complete when attack ends
	return EBTNodeResult::InProgress;
}

void UBTTask_ExecuteAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UEnemyCombatAIComponent* CombatAI = Pawn->FindComponentByClass<UEnemyCombatAIComponent>();
	if (!CombatAI)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Check if attack is still in progress
	if (CombatAI->IsAttacking())
	{
		// Still attacking - continue
		return;
	}

	// Attack ended - check if we transitioned to recovering (normal) or staggered (interrupted)
	if (CombatAI->CurrentState == EEnemyAIState::Recovering)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
	else
	{
		// Staggered, died, or other interruption
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
	}
}

EBTNodeResult::Type UBTTask_ExecuteAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// If we're aborting, the CombatAI component will handle cleanup via OnDamaged/OnCountered/etc
	return EBTNodeResult::Aborted;
}

FString UBTTask_ExecuteAttack::GetStaticDescription() const
{
	return TEXT("Execute the selected attack.\nWaits for montage completion.");
}
