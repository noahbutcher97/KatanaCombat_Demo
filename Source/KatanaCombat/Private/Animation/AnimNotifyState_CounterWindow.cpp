// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_CounterWindow.h"
#include "Core/CombatComponent.h"
#include "Characters/BaseCombatCharacter.h"
#include "Data/PairedAnimationData.h"
#include "Animation/AnimInstance.h"

UAnimNotifyState_CounterWindow::UAnimNotifyState_CounterWindow()
{
}

FString UAnimNotifyState_CounterWindow::GetNotifyName_Implementation() const
{
	return TEXT("Counter Window");
}

void UAnimNotifyState_CounterWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	// Call base to register checkpoint
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	// Get the attacker's combat component
	ABaseCombatCharacter* Attacker = Cast<ABaseCombatCharacter>(MeshComp->GetOwner());
	if (Attacker)
	{
		if (UCombatComponent* Combat = Attacker->GetCombatComponent())
		{
			// Store counter context on the attacker for defenders to query
			Combat->SetCounterWindowData(AttackType, SwingDirection, CounterData, TotalDuration);
		}
	}
}

void UAnimNotifyState_CounterWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	// Clear counter context when window ends
	ABaseCombatCharacter* Attacker = Cast<ABaseCombatCharacter>(MeshComp->GetOwner());
	if (Attacker)
	{
		if (UCombatComponent* Combat = Attacker->GetCombatComponent())
		{
			Combat->ClearCounterWindowData();
		}
	}
}
