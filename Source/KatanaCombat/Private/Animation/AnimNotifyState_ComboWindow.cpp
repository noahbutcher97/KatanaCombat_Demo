// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_ComboWindow.h"
#include "Core/CombatComponent.h"
#include "GameFramework/Character.h"

UAnimNotifyState_ComboWindow::UAnimNotifyState_ComboWindow()
{
}

void UAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
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
    
	// Find combat component and open combo window
	if (UCombatComponent* CombatComp = Character->FindComponentByClass<UCombatComponent>())
	{
		CombatComp->OpenComboWindow(TotalDuration);
	}
}

void UAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
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
    
	// Find combat component and close combo window
	if (UCombatComponent* CombatComp = Character->FindComponentByClass<UCombatComponent>())
	{
		CombatComp->CloseComboWindow();
	}
}

FString UAnimNotifyState_ComboWindow::GetNotifyName_Implementation() const
{
	return TEXT("Combo Window");
}