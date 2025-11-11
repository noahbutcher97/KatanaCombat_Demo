// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_ActionWindow_Base.h"
#include "Core/CombatComponent.h"
#include "Core/CombatComponentV2.h"
#include "Characters/SamuraiCharacter.h"
#include "Data/CombatSettings.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"

UAnimNotifyState_ActionWindow_Base::UAnimNotifyState_ActionWindow_Base()
{
}

void UAnimNotifyState_ActionWindow_Base::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	// Check if using V2 system via CombatSettings (owned by character)
	ASamuraiCharacter* Character = Cast<ASamuraiCharacter>(MeshComp->GetOwner());
	if (Character && Character->CombatSettings && Character->CombatSettings->bUseV2System)
	{
		// V2: Register checkpoint for timer-based execution
		if (UCombatComponentV2* CombatV2 = Character->FindComponentByClass<UCombatComponentV2>())
		{
			// Get current montage time for checkpoint registration
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				if (UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage())
				{
					float CurrentMontageTime = AnimInstance->Montage_GetPosition(CurrentMontage);
					CombatV2->RegisterCheckpoint(GetWindowType(), CurrentMontageTime, TotalDuration);
				}
			}
			return; // Early exit for V2
		}
	}

	// V1: Call legacy window open method
	if (Character)
	{
		if (UCombatComponent* CombatComp = Character->FindComponentByClass<UCombatComponent>())
		{
			OnOpenWindow_V1(CombatComp, TotalDuration);
		}
	}
}

void UAnimNotifyState_ActionWindow_Base::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	// V2: Checkpoints expire automatically via ClearExpiredCheckpoints()
	// Only need to close for V1 system
	
	if (ASamuraiCharacter* Character = Cast<ASamuraiCharacter>(MeshComp->GetOwner()))
	{
		// Check if V2 is enabled via CombatSettings (owned by character)
		bool bUseV2 = Character->CombatSettings && Character->CombatSettings->bUseV2System;

		// V1: Call legacy window close method
		if (!bUseV2)
		{
			if (UCombatComponent* CombatComp = Character->FindComponentByClass<UCombatComponent>())
			{
				OnCloseWindow_V1(CombatComp);
			}
		}
	}
}