// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotify_ActivateRagdoll.h"
#include "Core/HitReactionComponent.h"
#include "GameFramework/Character.h"

UAnimNotify_ActivateRagdoll::UAnimNotify_ActivateRagdoll()
{
	BlendTime = 0.1f;

#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(255, 50, 50, 255);  // Red for ragdoll activation
#endif
}

void UAnimNotify_ActivateRagdoll::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	// Get the HitReactionComponent and trigger ragdoll
	ACharacter* Character = Cast<ACharacter>(Owner);
	if (!Character)
	{
		return;
	}

	UHitReactionComponent* HitReactionComp = Character->FindComponentByClass<UHitReactionComponent>();
	if (HitReactionComp)
	{
		HitReactionComp->TriggerRagdollFromNotify(BlendTime);
	}
}

FString UAnimNotify_ActivateRagdoll::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Activate Ragdoll (%.2fs blend)"), BlendTime);
}
