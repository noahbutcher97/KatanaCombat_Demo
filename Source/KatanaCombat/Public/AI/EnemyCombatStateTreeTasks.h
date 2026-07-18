// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyAITypes.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"
#include "EnemyCombatStateTreeTasks.generated.h"

class AAIController;
class AActor;

USTRUCT()
struct FStateTreeSetEnemyCombatTargetInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> EnemyActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bUsePlayerPawnIfTargetUnset = true;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 0))
	int32 PlayerIndex = 0;
};

USTRUCT(meta = (DisplayName = "Set Enemy Combat Target", Category = "Katana Combat"))
struct KATANACOMBAT_API FStateTreeSetEnemyCombatTargetTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeSetEnemyCombatTargetInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FStateTreeRequestEnemyAttackTokenInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> EnemyActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 0.0))
	float MaxQueueWaitTime = 3.0f;

	UPROPERTY(VisibleAnywhere, Category = Runtime)
	float ElapsedTime = 0.0f;
};

USTRUCT(meta = (DisplayName = "Request Enemy Attack Token", Category = "Katana Combat"))
struct KATANACOMBAT_API FStateTreeRequestEnemyAttackTokenTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeRequestEnemyAttackTokenInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FStateTreeEnemyCombatMoveInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> EnemyActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> Controller = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = 0.05))
	float UpdateInterval = 0.1f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float CirclingAcceptanceRadius = 80.0f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bStopMovementOnExit = true;

	UPROPERTY(VisibleAnywhere, Category = Runtime)
	float ElapsedTime = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = Runtime)
	float TimeSinceUpdate = 0.0f;
};

USTRUCT(meta = (DisplayName = "Circle Enemy Combat Target", Category = "Katana Combat"))
struct KATANACOMBAT_API FStateTreeCircleEnemyCombatTargetTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEnemyCombatMoveInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT(meta = (DisplayName = "Approach Enemy Combat Target", Category = "Katana Combat"))
struct KATANACOMBAT_API FStateTreeApproachEnemyCombatTargetTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEnemyCombatMoveInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FStateTreeExecuteEnemyAttackInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> EnemyActor = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Runtime)
	bool bAttackStarted = false;

	UPROPERTY(VisibleAnywhere, Category = Runtime)
	int32 AttackGeneration = 0;
};

USTRUCT(meta = (DisplayName = "Execute Enemy Attack", Category = "Katana Combat"))
struct KATANACOMBAT_API FStateTreeExecuteEnemyAttackTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeExecuteEnemyAttackInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FStateTreeEnemyCombatStateConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> EnemyActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Condition)
	EEnemyAIState RequiredState = EEnemyAIState::Idle;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};

USTRUCT(DisplayName = "Enemy Combat State")
struct KATANACOMBAT_API FStateTreeEnemyCombatStateCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEnemyCombatStateConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FStateTreeEnemyAttackTokenConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> EnemyActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};

USTRUCT(DisplayName = "Enemy Has Attack Token")
struct KATANACOMBAT_API FStateTreeEnemyAttackTokenCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEnemyAttackTokenConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
