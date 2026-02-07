// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTTask_CirclePlayer.h"
#include "AI/EnemyCombatAIComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_CirclePlayer::UBTTask_CirclePlayer()
{
	NodeName = TEXT("Circle Player");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_CirclePlayer::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_CirclePlayer] No EnemyCombatAIComponent on %s"),
			*Pawn->GetName());
		return EBTNodeResult::Failed;
	}

	// Reset timer
	TimeSinceLastUpdate = DestinationUpdateInterval; // Force immediate update

	// Continue in progress - this task runs until aborted
	return EBTNodeResult::InProgress;
}

void UBTTask_CirclePlayer::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
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

	// Update destination periodically
	TimeSinceLastUpdate += DeltaSeconds;
	if (TimeSinceLastUpdate >= DestinationUpdateInterval)
	{
		TimeSinceLastUpdate = 0.0f;

		FVector Destination = CombatAI->GetCirclingDestination();

		// Move toward circling destination
		AIController->MoveToLocation(
			Destination,
			AcceptanceRadius,
			true,  // bStopOnOverlap
			true,  // bUsePathfinding
			false, // bProjectDestinationToNavigation
			true,  // bCanStrafe
			nullptr, // FilterClass
			true   // bAllowPartialPath
		);
	}

	// This task runs indefinitely until aborted
	// Don't call FinishLatentTask - let it run
}

EBTNodeResult::Type UBTTask_CirclePlayer::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		// Stop movement
		AIController->StopMovement();
	}

	return EBTNodeResult::Aborted;
}

FString UBTTask_CirclePlayer::GetStaticDescription() const
{
	return FString::Printf(TEXT("Circle around the player.\nUpdate interval: %.1fs\nAcceptance radius: %.0f"),
		DestinationUpdateInterval, AcceptanceRadius);
}
