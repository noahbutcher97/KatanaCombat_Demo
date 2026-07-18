// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_CounterWindow.h"
#include "Animation/CombatAnimNotifyIdentity.h"
#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
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

	// Get the attacker's paired animation component (counter window state lives here)
	AActor* Owner = MeshComp->GetOwner();
	bool bOpenedCanonicalWindow = false;
	if (UCombatComponent* Combat = Owner->FindComponentByClass<UCombatComponent>())
	{
		bOpenedCanonicalWindow = Combat->OpenAttackWindow(
			EAttackWindowKind::Counter,
			ResolveRuntimeNotifySourceId(EventReference),
			ResolveRuntimeMontageInstanceId(EventReference),
			TotalDuration).IsValid();
	}
	if (bOpenedCanonicalWindow)
	{
		if (UPairedAnimationComponent* PairedComp = Owner->FindComponentByClass<UPairedAnimationComponent>())
		{
			// Legacy query data mirrors only an accepted canonical runtime window.
			PairedComp->SetCounterWindowData(AttackType, SwingDirection, CounterData, TotalDuration);
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

	AActor* Owner = MeshComp->GetOwner();
	bool bClosedCanonicalWindow = false;
	if (UCombatComponent* Combat = Owner->FindComponentByClass<UCombatComponent>())
	{
		bClosedCanonicalWindow = Combat->CloseAttackWindow(
			EAttackWindowKind::Counter,
			ResolveRuntimeNotifySourceId(EventReference),
			ResolveRuntimeMontageInstanceId(EventReference));
	}
	if (bClosedCanonicalWindow)
	{
		if (UPairedAnimationComponent* PairedComp = Owner->FindComponentByClass<UPairedAnimationComponent>())
		{
			PairedComp->ClearCounterWindowData();
		}
	}
}
