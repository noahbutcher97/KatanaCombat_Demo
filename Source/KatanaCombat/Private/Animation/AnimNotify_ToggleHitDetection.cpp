// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotify_ToggleHitDetection.h"
#include "Interfaces/CombatInterface.h"

UAnimNotify_ToggleHitDetection::UAnimNotify_ToggleHitDetection()
{
	bEnable = true;
}

void UAnimNotify_ToggleHitDetection::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
    
	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}
    
	// Route to combat interface
	if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(MeshComp->GetOwner()))
	{
		if (bEnable)
		{
			CombatInterface->Execute_OnEnableHitDetection(MeshComp->GetOwner());
		}
		else
		{
			CombatInterface->Execute_OnDisableHitDetection(MeshComp->GetOwner());
		}
	}
}

FString UAnimNotify_ToggleHitDetection::GetNotifyName_Implementation() const
{
	return bEnable ? TEXT("Enable Hit Detection") : TEXT("Disable Hit Detection");
}