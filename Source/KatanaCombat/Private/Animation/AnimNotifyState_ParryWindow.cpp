// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_ParryWindow.h"
#include "Animation/CombatAnimNotifyIdentity.h"
#include "Core/CombatComponent.h"
#include "Characters/BaseCombatCharacter.h"

UAnimNotifyState_ParryWindow::UAnimNotifyState_ParryWindow()
{
}

FString UAnimNotifyState_ParryWindow::GetNotifyName_Implementation() const
{
	return TEXT("Parry Window");
}

void UAnimNotifyState_ParryWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	// Call base to register checkpoint
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	ABaseCombatCharacter* Attacker = Cast<ABaseCombatCharacter>(MeshComp->GetOwner());
	if (Attacker)
	{
		if (UCombatComponent* Combat = Attacker->GetCombatComponent())
		{
			Combat->OpenAttackWindow(
				EAttackWindowKind::Parry,
				ResolveRuntimeNotifySourceId(EventReference),
				ResolveRuntimeMontageInstanceId(EventReference),
				TotalDuration);
		}
	}
}

void UAnimNotifyState_ParryWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	ABaseCombatCharacter* Attacker = Cast<ABaseCombatCharacter>(MeshComp->GetOwner());
	if (Attacker)
	{
		if (UCombatComponent* Combat = Attacker->GetCombatComponent())
		{
			Combat->CloseAttackWindow(
				EAttackWindowKind::Parry,
				ResolveRuntimeNotifySourceId(EventReference),
				ResolveRuntimeMontageInstanceId(EventReference));
		}
	}
}
