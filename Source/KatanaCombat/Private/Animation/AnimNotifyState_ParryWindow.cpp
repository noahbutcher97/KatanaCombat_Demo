// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_ParryWindow.h"
#include "Animation/CombatAnimNotifyIdentity.h"
#include "Core/CombatComponent.h"
#include "Characters/BaseCombatCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

namespace
{
FAnimMontageInstance* ResolveExactMontageInstance(
	UAnimInstance* AnimInstance,
	const UAnimMontage* Montage,
	const FAnimNotifyEventReference& EventReference)
{
	if (!AnimInstance || !Montage)
	{
		return nullptr;
	}

	FAnimMontageInstance* Instance = AnimInstance->GetMontageInstanceForID(
		ResolveRuntimeMontageInstanceId(EventReference));
	return Instance && Instance->Montage == Montage ? Instance : nullptr;
}

float ResolveSimulationRelativePlayRate(
	const USkeletalMeshComponent* MeshComp,
	const FAnimMontageInstance* MontageInstance,
	const UAnimMontage* Montage)
{
	const AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	const float MeshRateScale = MeshComp ? MeshComp->GlobalAnimRateScale : 0.0f;
	const float ActorTimeDilation = Owner ? Owner->CustomTimeDilation : 0.0f;
	if (!MontageInstance
		|| !Montage
		|| !FMath::IsFinite(MeshRateScale)
		|| !FMath::IsFinite(ActorTimeDilation)
		|| MeshRateScale <= UE_SMALL_NUMBER
		|| ActorTimeDilation <= UE_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return MontageInstance->GetPlayRate()
		* Montage->RateScale
		* MeshRateScale
		* ActorTimeDilation;
}

bool ResolveRemainingRuntimeWindowDuration(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference,
	float& OutRemainingDuration)
{
	OutRemainingDuration = 0.0f;
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	const FAnimNotifyEvent* NotifyEvent = EventReference.GetNotify();
	FAnimMontageInstance* MontageInstance = ResolveExactMontageInstance(
		AnimInstance, Montage, EventReference);
	if (!MontageInstance || !NotifyEvent)
	{
		return false;
	}

	const float EffectivePlayRate = ResolveSimulationRelativePlayRate(
		MeshComp, MontageInstance, Montage);
	const float MontagePosition = MontageInstance->GetPosition();
	if (!FMath::IsFinite(EffectivePlayRate)
		|| !FMath::IsFinite(MontagePosition)
		|| FMath::Abs(EffectivePlayRate) <= UE_SMALL_NUMBER)
	{
		return false;
	}

	const float AuthoredRemaining = EffectivePlayRate > 0.0f
		? NotifyEvent->GetEndTriggerTime() - MontagePosition
		: MontagePosition - NotifyEvent->GetTriggerTime();
	OutRemainingDuration = FMath::Max(0.0f, AuthoredRemaining) / FMath::Abs(EffectivePlayRate);
	return FMath::IsFinite(OutRemainingDuration);
}
}

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

	ABaseCombatCharacter* Attacker = Cast<ABaseCombatCharacter>(MeshComp->GetOwner());
	if (Attacker)
	{
		if (UCombatComponent* Combat = Attacker->GetCombatComponent())
		{
			float RemainingDuration = 0.0f;
			if (!ResolveRemainingRuntimeWindowDuration(
				MeshComp, Animation, EventReference, RemainingDuration))
			{
				Combat->InvalidateAttackThreatPrediction(
					EThreatInvalidationReason::MontageRateChanged);
				return;
			}
			const FAttackWindowInstanceId Window = Combat->OpenAttackWindow(
				EAttackWindowKind::Parry,
				ResolveRuntimeNotifySourceId(EventReference),
				ResolveRuntimeMontageInstanceId(EventReference),
				RemainingDuration);
			if (Window.IsValid())
			{
				Combat->PublishReviewedAttackWindowPrediction(Window);
			}
		}
	}
}

void UAnimNotifyState_ParryWindow::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	ABaseCombatCharacter* Attacker = MeshComp
		? Cast<ABaseCombatCharacter>(MeshComp->GetOwner())
		: nullptr;
	UCombatComponent* Combat = Attacker ? Attacker->GetCombatComponent() : nullptr;
	float RemainingDuration = 0.0f;
	if (!Combat
		|| !ResolveRemainingRuntimeWindowDuration(
			MeshComp, Animation, EventReference, RemainingDuration))
	{
		if (Combat)
		{
			Combat->InvalidateAttackThreatPrediction(EThreatInvalidationReason::MontageRateChanged);
		}
		return;
	}

	const FAttackWindowInstanceId RefreshedWindow = Combat->RefreshAttackWindow(
		EAttackWindowKind::Parry,
		ResolveRuntimeNotifySourceId(EventReference),
		ResolveRuntimeMontageInstanceId(EventReference),
		RemainingDuration);
	if (RefreshedWindow.IsValid())
	{
		Combat->PublishReviewedAttackWindowPrediction(RefreshedWindow);
	}
}

void UAnimNotifyState_ParryWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	ABaseCombatCharacter* Attacker = Cast<ABaseCombatCharacter>(MeshComp->GetOwner());
	if (Attacker)
	{
		if (UCombatComponent* Combat = Attacker->GetCombatComponent())
		{
			Combat->CloseAttackWindow(
				EAttackWindowKind::Parry,
				ResolveRuntimeNotifySourceId(EventReference),
				ResolveRuntimeMontageInstanceId(EventReference));
		}
	}
}
