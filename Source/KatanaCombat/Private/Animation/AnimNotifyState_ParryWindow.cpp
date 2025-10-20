// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_ParryWindow.h"
#include "Core/CombatComponent.h"
#include "GameFramework/Character.h"

UAnimNotifyState_ParryWindow::UAnimNotifyState_ParryWindow()
{
}

void UAnimNotifyState_ParryWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner());
	if (!Character)
	{
		return;
	}

	// Find combat component and open parry window
	// This marks the ATTACKER as vulnerable to being parried
	if (UCombatComponent* CombatComp = Character->FindComponentByClass<UCombatComponent>())
	{
		CombatComp->OpenParryWindow(TotalDuration);
	}
}

void UAnimNotifyState_ParryWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner());
	if (!Character)
	{
		return;
	}

	// Find combat component and close parry window
	if (UCombatComponent* CombatComp = Character->FindComponentByClass<UCombatComponent>())
	{
		CombatComp->CloseParryWindow();
	}
}

FString UAnimNotifyState_ParryWindow::GetNotifyName_Implementation() const
{
	return TEXT("Parry Window");
}