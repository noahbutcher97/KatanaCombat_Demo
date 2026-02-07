// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_ParryWindow.h"
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

	// Set parry window active on the attacker's combat component
	ABaseCombatCharacter* Attacker = Cast<ABaseCombatCharacter>(MeshComp->GetOwner());
	if (Attacker)
	{
		if (UCombatComponent* Combat = Attacker->GetCombatComponent())
		{
			Combat->SetParryWindowActive(true);
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

	// Clear parry window on the attacker's combat component
	ABaseCombatCharacter* Attacker = Cast<ABaseCombatCharacter>(MeshComp->GetOwner());
	if (Attacker)
	{
		if (UCombatComponent* Combat = Attacker->GetCombatComponent())
		{
			Combat->SetParryWindowActive(false);
		}
	}
}
