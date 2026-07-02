// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Components/StateTreeAIComponent.h"
#include "EnemyCombatAIController.generated.h"

/**
 * StateTree component variant for project enemies.
 *
 * Bare C++ enemies and automation fixtures may not have an authored StateTree
 * asset yet. Missing assets are allowed; invalid assigned assets still use the
 * engine's normal validation path.
 */
UCLASS(ClassGroup = AI, Blueprintable, meta = (BlueprintSpawnableComponent))
class KATANACOMBAT_API UEnemyStateTreeAIComponent : public UStateTreeAIComponent
{
	GENERATED_BODY()

public:
	const UStateTree* GetAssignedStateTree() const;

protected:
	virtual void ValidateStateTreeReference() override;
};

/**
 * Project enemy AI controller.
 *
 * Owns the StateTree runtime for AEnemyCharacter. Combat token ownership,
 * attack selection, and attack execution remain in UEnemyCombatAIComponent.
 */
UCLASS()
class KATANACOMBAT_API AEnemyCombatAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyCombatAIController();

	UFUNCTION(BlueprintPure, Category = "Enemy|AI")
	UStateTreeAIComponent* GetStateTreeAIComponent() const { return StateTreeAIComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyStateTreeAIComponent> StateTreeAIComponent;
};
