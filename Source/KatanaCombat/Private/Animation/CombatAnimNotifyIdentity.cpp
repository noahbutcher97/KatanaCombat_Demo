// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/CombatAnimNotifyIdentity.h"

#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimSequenceBase.h"

FAnimNotifyRuntimeSourceId ResolveRuntimeNotifySourceId(
	const FAnimNotifyEventReference& EventReference)
{
	FAnimNotifyRuntimeSourceId Result;
	const UAnimSequenceBase* SourceAnimation = Cast<UAnimSequenceBase>(EventReference.GetSourceObject());
	const FAnimNotifyEvent* NotifyEvent = EventReference.GetNotify();
	if (!SourceAnimation || !NotifyEvent)
	{
		return Result;
	}

	for (int32 NotifyIndex = 0; NotifyIndex < SourceAnimation->Notifies.Num(); ++NotifyIndex)
	{
		if (&SourceAnimation->Notifies[NotifyIndex] == NotifyEvent)
		{
			Result.SourceAnimation = FSoftObjectPath(SourceAnimation);
			Result.NotifyEventIndex = NotifyIndex;
			break;
		}
	}

	return Result;
}

int32 ResolveRuntimeMontageInstanceId(const FAnimNotifyEventReference& EventReference)
{
	const UE::Anim::FAnimNotifyMontageInstanceContext* MontageContext =
		EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
	return MontageContext ? MontageContext->MontageInstanceID : INDEX_NONE;
}
