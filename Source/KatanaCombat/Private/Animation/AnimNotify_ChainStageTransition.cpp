// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotify_ChainStageTransition.h"

#include "Animation/AnimMontage.h"
#include "Animation/CombatAnimNotifyIdentity.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "GameFramework/Actor.h"

bool UAnimNotify_ChainStageTransition::HasExactlyOnePlayableMarker(
	const UAnimMontage* Montage,
	const FName RequiredMarker,
	const EChainStageTransitionType RequiredTransition,
	const FName PlayedSection)
{
	float MarkerOffset = 0.0f;
	return TryGetSinglePlayableMarkerOffset(
		Montage,
		RequiredMarker,
		RequiredTransition,
		PlayedSection,
		MarkerOffset);
}

bool UAnimNotify_ChainStageTransition::TryGetSinglePlayableMarkerOffset(
	const UAnimMontage* Montage,
	const FName RequiredMarker,
	const EChainStageTransitionType RequiredTransition,
	const FName PlayedSection,
	float& OutSectionRelativeSeconds)
{
	OutSectionRelativeSeconds = 0.0f;
	if (!Montage || RequiredMarker.IsNone())
	{
		return false;
	}

	const FAnimNotifyEvent* MatchingEvent = nullptr;
	int32 MatchingCount = 0;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const UAnimNotify_ChainStageTransition* Notify =
			Cast<UAnimNotify_ChainStageTransition>(Event.Notify);
		if (Notify
			&& Notify->Transition == RequiredTransition
			&& Notify->MarkerName == RequiredMarker)
		{
			MatchingEvent = &Event;
			++MatchingCount;
		}
	}
	if (MatchingCount != 1 || !MatchingEvent || PlayedSection.IsNone())
	{
		return false;
	}

	const int32 SectionIndex = Montage->GetSectionIndex(PlayedSection);
	if (SectionIndex == INDEX_NONE)
	{
		return false;
	}
	float SectionStart = 0.0f;
	float SectionEnd = 0.0f;
	Montage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
	const float TriggerTime = MatchingEvent->GetTriggerTime();
	const bool bIsPlayable = FMath::IsFinite(SectionStart)
		&& FMath::IsFinite(SectionEnd)
		&& FMath::IsFinite(TriggerTime)
		&& SectionEnd > SectionStart
		&& TriggerTime > SectionStart + UE_KINDA_SMALL_NUMBER
		&& TriggerTime < SectionEnd - UE_KINDA_SMALL_NUMBER;
	if (bIsPlayable)
	{
		OutSectionRelativeSeconds = TriggerTime - SectionStart;
	}
	return bIsPlayable;
}

void UAnimNotify_ChainStageTransition::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	UPairedAnimationComponent* Paired = Owner
		? Owner->FindComponentByClass<UPairedAnimationComponent>()
		: nullptr;
	if (!Paired)
	{
		return;
	}

	Paired->HandleChainStageTransition(
		Transition,
		ResolveRuntimeMontageInstanceId(EventReference),
		ResolveRuntimeNotifySourceId(EventReference));
}

FString UAnimNotify_ChainStageTransition::GetNotifyName_Implementation() const
{
	const TCHAR* TransitionName = Transition == EChainStageTransitionType::OpenCounterWindow
		? TEXT("Open Counter")
		: TEXT("Auto Continue");
	return MarkerName.IsNone()
		? FString::Printf(TEXT("Chain: %s"), TransitionName)
		: FString::Printf(TEXT("Chain: %s [%s]"), TransitionName, *MarkerName.ToString());
}
