// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_ActionWindow_Base.h"
#include "Core/CombatComponent.h"
#include "Characters/BaseCombatCharacter.h"
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

	// Cast to BaseCombatCharacter to support both player and enemy characters
	ABaseCombatCharacter* Character = Cast<ABaseCombatCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		// Register checkpoint for timer-based execution
		if (UCombatComponent* Combat = Character->GetCombatComponent())
		{
			// Get current montage time for checkpoint registration
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				if (UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage())
				{
					float CurrentMontageTime = AnimInstance->Montage_GetPosition(CurrentMontage);
					Combat->RegisterCheckpoint(GetWindowType(), CurrentMontageTime, TotalDuration);
				}
			}
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

	// Checkpoints expire automatically via ClearExpiredCheckpoints()
}
