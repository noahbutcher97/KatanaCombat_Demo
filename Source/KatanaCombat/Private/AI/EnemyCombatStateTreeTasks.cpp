// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/EnemyCombatStateTreeTasks.h"
#include "AI/EnemyCombatAIComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeExecutionTypes.h"

namespace
{
UEnemyCombatAIComponent* FindEnemyCombatAI(AActor* EnemyActor)
{
	return EnemyActor ? EnemyActor->FindComponentByClass<UEnemyCombatAIComponent>() : nullptr;
}

AAIController* ResolveAIController(AActor* EnemyActor, AAIController* BoundController)
{
	if (BoundController)
	{
		return BoundController;
	}

	APawn* Pawn = Cast<APawn>(EnemyActor);
	return Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
}

AActor* ResolveCombatTarget(const FStateTreeSetEnemyCombatTargetInstanceData& InstanceData)
{
	if (InstanceData.TargetActor)
	{
		return InstanceData.TargetActor;
	}

	return InstanceData.bUsePlayerPawnIfTargetUnset
		? UGameplayStatics::GetPlayerPawn(InstanceData.EnemyActor.Get(), InstanceData.PlayerIndex)
		: nullptr;
}

EStateTreeRunStatus MoveToCombatTarget(FStateTreeEnemyCombatMoveInstanceData& InstanceData)
{
	UEnemyCombatAIComponent* CombatAI = FindEnemyCombatAI(InstanceData.EnemyActor);
	AAIController* Controller = ResolveAIController(InstanceData.EnemyActor, InstanceData.Controller);
	if (!CombatAI || !Controller)
	{
		return EStateTreeRunStatus::Failed;
	}

	AActor* Target = CombatAI->CombatTarget.Get();
	if (!Target)
	{
		Controller->StopMovement();
		return EStateTreeRunStatus::Failed;
	}

	if (CombatAI->IsInAttackRange())
	{
		Controller->StopMovement();
		return EStateTreeRunStatus::Succeeded;
	}

	const float AcceptanceRadius = FMath::Max(1.0f, CombatAI->ApproachConfig.AttackRange * 0.8f);
	const EPathFollowingRequestResult::Type MoveResult = Controller->MoveToActor(
		Target,
		AcceptanceRadius,
		true,
		true,
		true,
		nullptr,
		true);

	return MoveResult == EPathFollowingRequestResult::Failed
		? EStateTreeRunStatus::Failed
		: EStateTreeRunStatus::Running;
}
}

EStateTreeRunStatus FStateTreeSetEnemyCombatTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UEnemyCombatAIComponent* CombatAI = FindEnemyCombatAI(InstanceData.EnemyActor);
	if (!CombatAI)
	{
		return EStateTreeRunStatus::Failed;
	}

	AActor* Target = ResolveCombatTarget(InstanceData);
	if (!Target)
	{
		CombatAI->SetCombatTarget(nullptr);
		return EStateTreeRunStatus::Failed;
	}

	CombatAI->SetCombatTarget(Target);
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FStateTreeRequestEnemyAttackTokenTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.0f;

	UEnemyCombatAIComponent* CombatAI = FindEnemyCombatAI(InstanceData.EnemyActor);
	if (!CombatAI)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (CombatAI->HasAttackToken())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	if (CombatAI->TryInitiateAttack())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return CombatAI->IsWaitingForToken() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FStateTreeRequestEnemyAttackTokenTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime += DeltaTime;

	UEnemyCombatAIComponent* CombatAI = FindEnemyCombatAI(InstanceData.EnemyActor);
	if (!CombatAI)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (CombatAI->HasAttackToken())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	if (!CombatAI->IsWaitingForToken())
	{
		return EStateTreeRunStatus::Failed;
	}

	return InstanceData.ElapsedTime >= InstanceData.MaxQueueWaitTime
		? EStateTreeRunStatus::Failed
		: EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeCircleEnemyCombatTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.0f;
	InstanceData.TimeSinceUpdate = InstanceData.UpdateInterval;
	return Tick(Context, 0.0f);
}

EStateTreeRunStatus FStateTreeCircleEnemyCombatTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime += DeltaTime;
	InstanceData.TimeSinceUpdate += DeltaTime;

	UEnemyCombatAIComponent* CombatAI = FindEnemyCombatAI(InstanceData.EnemyActor);
	AAIController* Controller = ResolveAIController(InstanceData.EnemyActor, InstanceData.Controller);
	if (!CombatAI || !Controller || !CombatAI->CombatTarget.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.TimeSinceUpdate >= InstanceData.UpdateInterval)
	{
		InstanceData.TimeSinceUpdate = 0.0f;
		Controller->MoveToLocation(
			CombatAI->GetCirclingDestination(),
			InstanceData.CirclingAcceptanceRadius,
			true,
			true,
			false,
			true,
			nullptr,
			true);
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeCircleEnemyCombatTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.bStopMovementOnExit)
	{
		if (AAIController* Controller = ResolveAIController(InstanceData.EnemyActor, InstanceData.Controller))
		{
			Controller->StopMovement();
		}
	}
}

EStateTreeRunStatus FStateTreeApproachEnemyCombatTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.0f;
	InstanceData.TimeSinceUpdate = InstanceData.UpdateInterval;
	return MoveToCombatTarget(InstanceData);
}

EStateTreeRunStatus FStateTreeApproachEnemyCombatTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime += DeltaTime;
	InstanceData.TimeSinceUpdate += DeltaTime;

	UEnemyCombatAIComponent* CombatAI = FindEnemyCombatAI(InstanceData.EnemyActor);
	AAIController* Controller = ResolveAIController(InstanceData.EnemyActor, InstanceData.Controller);
	if (!CombatAI || !Controller)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (CombatAI->IsInAttackRange())
	{
		Controller->StopMovement();
		return EStateTreeRunStatus::Succeeded;
	}

	if (InstanceData.ElapsedTime >= CombatAI->ApproachConfig.ApproachTimeout)
	{
		Controller->StopMovement();
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.TimeSinceUpdate >= InstanceData.UpdateInterval || Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		InstanceData.TimeSinceUpdate = 0.0f;
		return MoveToCombatTarget(InstanceData);
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeApproachEnemyCombatTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.bStopMovementOnExit)
	{
		if (AAIController* Controller = ResolveAIController(InstanceData.EnemyActor, InstanceData.Controller))
		{
			Controller->StopMovement();
		}
	}
}

EStateTreeRunStatus FStateTreeExecuteEnemyAttackTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bAttackStarted = false;

	UEnemyCombatAIComponent* CombatAI = FindEnemyCombatAI(InstanceData.EnemyActor);
	if (!CombatAI)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (CombatAI->IsAttacking())
	{
		InstanceData.bAttackStarted = true;
		return EStateTreeRunStatus::Running;
	}

	if (!CombatAI->ExecuteAttack())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.bAttackStarted = true;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeExecuteEnemyAttackTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UEnemyCombatAIComponent* CombatAI = FindEnemyCombatAI(InstanceData.EnemyActor);
	if (!CombatAI || !InstanceData.bAttackStarted)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (CombatAI->IsAttacking())
	{
		return EStateTreeRunStatus::Running;
	}

	return CombatAI->CurrentState == EEnemyAIState::Recovering
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

bool FStateTreeEnemyCombatStateCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const UEnemyCombatAIComponent* CombatAI = FindEnemyCombatAI(InstanceData.EnemyActor);
	if (!CombatAI)
	{
		return false;
	}

	const bool bMatches = CombatAI->CurrentState == InstanceData.RequiredState;
	return InstanceData.bInvert ? !bMatches : bMatches;
}

bool FStateTreeEnemyAttackTokenCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const UEnemyCombatAIComponent* CombatAI = FindEnemyCombatAI(InstanceData.EnemyActor);
	if (!CombatAI)
	{
		return false;
	}

	const bool bHasToken = CombatAI->HasAttackToken();
	return InstanceData.bInvert ? !bHasToken : bHasToken;
}

#if WITH_EDITOR
FText FStateTreeSetEnemyCombatTargetTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Set EnemyCombatAIComponent combat target."));
}

FText FStateTreeRequestEnemyAttackTokenTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Request or wait for a combat attack token."));
}

FText FStateTreeCircleEnemyCombatTargetTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Move around the current combat target."));
}

FText FStateTreeApproachEnemyCombatTargetTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Move toward the combat target until attack range."));
}

FText FStateTreeExecuteEnemyAttackTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Execute the selected enemy attack."));
}

FText FStateTreeEnemyCombatStateCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Check EnemyCombatAIComponent state."));
}

FText FStateTreeEnemyAttackTokenCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Check whether enemy has an attack token."));
}
#endif
