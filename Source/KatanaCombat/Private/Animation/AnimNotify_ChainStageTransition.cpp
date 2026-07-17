// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotify_ChainStageTransition.h"

#include "Animation/CombatAnimNotifyIdentity.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "GameFramework/Actor.h"

void UAnimNotify_ChainStageTransition::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	UPairedAnimationComponent* Paired = Owner
		? Owner->FindComponentByClass<UPairedAnimationComponent>()
		: nullptr;
	if (!Paired)
	{
		return;
	}

	Paired->HandleChainStageTransition(
		Transition,
		ResolveRuntimeMontageInstanceId(EventReference),
		ResolveRuntimeNotifySourceId(EventReference));
}

FString UAnimNotify_ChainStageTransition::GetNotifyName_Implementation() const
{
	const TCHAR* TransitionName = Transition == EChainStageTransitionType::OpenCounterWindow
		? TEXT("Open Counter")
		: TEXT("Auto Continue");
	return MarkerName.IsNone()
		? FString::Printf(TEXT("Chain: %s"), TransitionName)
		: FString::Printf(TEXT("Chain: %s [%s]"), TransitionName, *MarkerName.ToString());
}
