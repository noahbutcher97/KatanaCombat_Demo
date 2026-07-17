// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "CombatTypes.h"
#include "AnimNotify_ChainStageTransition.generated.h"

/** Identity-bearing gameplay marker for one retained defense-chain transition. */
UCLASS(meta = (DisplayName = "Chain Stage Transition"))
class KATANACOMBAT_API UAnimNotify_ChainStageTransition : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain")
	EChainStageTransitionType Transition = EChainStageTransitionType::OpenCounterWindow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain")
	FName MarkerName = NAME_None;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
