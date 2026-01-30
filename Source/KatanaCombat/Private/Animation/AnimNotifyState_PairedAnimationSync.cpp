// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"

UAnimNotifyState_PairedAnimationSync::UAnimNotifyState_PairedAnimationSync()
{
    // Defaults configured in header
}

void UAnimNotifyState_PairedAnimationSync::NotifyBegin(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    float TotalDuration,
    const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

    if (!MeshComp)
    {
        return;
    }

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
    {
        return;
    }

    // Find combat component to broadcast sync point
    UCombatComponent* CombatComp = Owner->FindComponentByClass<UCombatComponent>();
    if (CombatComp)
    {
        // Broadcast sync point event
        // CombatComponent can relay this to damage system, effects, etc.
        CombatComp->OnPairedAnimationSyncPoint.Broadcast(ReactionType, SyncPointName);
    }

    // Also check for HitReactionComponent (in case this is on victim's montage)
    UHitReactionComponent* HitReactionComp = Owner->FindComponentByClass<UHitReactionComponent>();
    if (HitReactionComp)
    {
        HitReactionComp->OnPairedAnimationSyncPoint.Broadcast(ReactionType, SyncPointName);
    }

    // Log sync point for debugging
    UE_LOG(LogTemp, Verbose, TEXT("PairedAnimationSync: %s reached sync point '%s' (Type: %d, Primary: %d)"),
        *Owner->GetName(),
        *SyncPointName.ToString(),
        static_cast<int32>(ReactionType),
        bIsPrimarySyncPoint);
}

void UAnimNotifyState_PairedAnimationSync::NotifyEnd(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyEnd(MeshComp, Animation, EventReference);

    // Sync point ended - useful for duration-based effects
    // Most effects are triggered at NotifyBegin (the impact moment)
}

FString UAnimNotifyState_PairedAnimationSync::GetNotifyName_Implementation() const
{
    if (SyncPointName.IsNone())
    {
        return TEXT("Paired Sync");
    }

    return FString::Printf(TEXT("Sync: %s"), *SyncPointName.ToString());
}
