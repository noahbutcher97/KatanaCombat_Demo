// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTTask_ApproachPlayer.h"
#include "AI/EnemyCombatAIComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_ApproachPlayer::UBTTask_ApproachPlayer()
{
	NodeName = TEXT("Approach Player");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_ApproachPlayer::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_ApproachPlayer] No EnemyCombatAIComponent on %s"),
			*Pawn->GetName());
		return EBTNodeResult::Failed;
	}

	AActor* Target = CombatAI->CombatTarget.Get();
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_ApproachPlayer] No combat target for %s"),
			*Pawn->GetName());
		return EBTNodeResult::Failed;
	}

	// Already in range?
	if (CombatAI->IsInAttackRange())
	{
		return EBTNodeResult::Succeeded;
	}

	// Reset timers
	ApproachTime = 0.0f;
	TimeSinceRangeCheck = RangeCheckInterval; // Force immediate check

	// Start movement toward target
	float AcceptanceRadius = CombatAI->ApproachConfig.AttackRange * 0.8f; // Slightly inside attack range
	EPathFollowingRequestResult::Type Result = AIController->MoveToActor(
		Target,
		AcceptanceRadius,
		true,  // bStopOnOverlap
		true,  // bUsePathfinding
		true,  // bCanStrafe
		nullptr, // FilterClass
		true   // bAllowPartialPath
	);

	if (Result == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_ApproachPlayer] MoveToActor failed for %s"),
			*Pawn->GetName());
		return EBTNodeResult::Failed;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_ApproachPlayer::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
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

	// Track time
	ApproachTime += DeltaSeconds;
	TimeSinceRangeCheck += DeltaSeconds;

	// Check approach timeout
	if (ApproachTime >= CombatAI->ApproachConfig.ApproachTimeout)
	{
		UE_LOG(LogTemp, Log, TEXT("[BTTask_ApproachPlayer] %s approach timed out"),
			*Pawn->GetName());
		AIController->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Periodic range check
	if (TimeSinceRangeCheck >= RangeCheckInterval)
	{
		TimeSinceRangeCheck = 0.0f;

		if (CombatAI->IsInAttackRange())
		{
			AIController->StopMovement();
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
			return;
		}

		// Re-issue move command to track moving target
		AActor* Target = CombatAI->CombatTarget.Get();
		if (Target)
		{
			float AcceptanceRadius = CombatAI->ApproachConfig.AttackRange * 0.8f;
			AIController->MoveToActor(
				Target,
				AcceptanceRadius,
				true, true, true, nullptr, true
			);
		}
		else
		{
			// Lost target
			AIController->StopMovement();
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			return;
		}
	}

	// Check if movement failed
	EPathFollowingStatus::Type MoveStatus = AIController->GetMoveStatus();
	if (MoveStatus == EPathFollowingStatus::Idle)
	{
		// Movement completed or was stopped - check if we're in range
		if (CombatAI->IsInAttackRange())
		{
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
		else
		{
			// Movement stopped but not in range - try again
			AActor* Target = CombatAI->CombatTarget.Get();
			if (Target)
			{
				float AcceptanceRadius = CombatAI->ApproachConfig.AttackRange * 0.8f;
				AIController->MoveToActor(Target, AcceptanceRadius, true, true, true, nullptr, true);
			}
		}
	}
}

EBTNodeResult::Type UBTTask_ApproachPlayer::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		AIController->StopMovement();
	}

	return EBTNodeResult::Aborted;
}

FString UBTTask_ApproachPlayer::GetStaticDescription() const
{
	return TEXT("Approach the combat target until in attack range.\nSucceeds when in range, fails on timeout.");
}
