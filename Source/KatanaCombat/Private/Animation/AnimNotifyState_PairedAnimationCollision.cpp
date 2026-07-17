// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_PairedAnimationCollision.h"

#include "Animation/CombatAnimNotifyIdentity.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "GameFramework/Actor.h"

UAnimNotifyState_PairedAnimationCollision::UAnimNotifyState_PairedAnimationCollision() = default;

void UAnimNotifyState_PairedAnimationCollision::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	UPairedAnimationComponent* Paired = Owner
		? Owner->FindComponentByClass<UPairedAnimationComponent>()
		: nullptr;
	if (!Paired)
	{
		return;
	}

	Paired->BeginPairedCollisionNotify(
		ResolveRuntimeNotifySourceId(EventReference),
		ResolveRuntimeMontageInstanceId(EventReference),
		bUseTrackedPartnersOnly,
		bDisablePawnCollision,
		bDisableCapsulePhysics,
		bDisableMovement,
		bScanForDynamicObstructions,
		DynamicObstructionRadius);
}

void UAnimNotifyState_PairedAnimationCollision::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UPairedAnimationComponent* Paired = Owner
		? Owner->FindComponentByClass<UPairedAnimationComponent>()
		: nullptr)
	{
		Paired->TickPairedCollisionNotify(
			ResolveRuntimeNotifySourceId(EventReference),
			ResolveRuntimeMontageInstanceId(EventReference));
	}
}

void UAnimNotifyState_PairedAnimationCollision::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UPairedAnimationComponent* Paired = Owner
		? Owner->FindComponentByClass<UPairedAnimationComponent>()
		: nullptr)
	{
		Paired->EndPairedCollisionNotify(
			ResolveRuntimeNotifySourceId(EventReference),
			ResolveRuntimeMontageInstanceId(EventReference));
	}
}

FString UAnimNotifyState_PairedAnimationCollision::GetNotifyName_Implementation() const
{
	FString Modifiers;
	if (bDisablePawnCollision)
	{
		Modifiers += TEXT("Pawn");
	}
	if (bDisableCapsulePhysics)
	{
		if (!Modifiers.IsEmpty())
		{
			Modifiers += TEXT("+");
		}
		Modifiers += TEXT("Physics");
	}
	if (bDisableMovement)
	{
		if (!Modifiers.IsEmpty())
		{
			Modifiers += TEXT("+");
		}
		Modifiers += TEXT("Move");
	}
	return Modifiers.IsEmpty()
		? TEXT("Paired Collision")
		: FString::Printf(TEXT("Paired Collision [%s]"), *Modifiers);
}
