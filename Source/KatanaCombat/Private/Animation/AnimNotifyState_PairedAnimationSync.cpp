// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"

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

    // ========================================================================
    // SAKURAI-STYLE HITSTOP (Hit Pause)
    // ========================================================================
    // Principle: Both attacker AND victim freeze for identical duration
    // Background, particles, and other actors continue (selective freeze)
    // Emphasizes impact power and gives player's eyes time to process

    if (bApplyHitPause && HitPauseDuration > 0.0f && bIsPrimarySyncPoint)
    {
        // Collect actors to freeze (attacker + all paired partners)
        TArray<AActor*> ActorsToFreeze;
        ActorsToFreeze.Add(Owner);

        // Get paired partners from CombatComponent
        if (CombatComp)
        {
            for (const TWeakObjectPtr<AActor>& PartnerRef : CombatComp->PairedAnimationPartners)
            {
                if (AActor* Partner = PartnerRef.Get())
                {
                    ActorsToFreeze.Add(Partner);
                }
            }
        }

        // Freeze all participants using CustomTimeDilation
        // This is per-actor, so background/particles/other actors continue
        for (AActor* ActorToFreeze : ActorsToFreeze)
        {
            ActorToFreeze->CustomTimeDilation = 0.0f;
        }

        UE_LOG(LogTemp, Log, TEXT("[HITSTOP] %s: Freezing %d actors for %.3fs at sync point '%s'"),
            *Owner->GetName(), ActorsToFreeze.Num(), HitPauseDuration, *SyncPointName.ToString());

        // ====================================================================
        // PLATFORM TIME-BASED RESTORATION (Sakurai Hitstop)
        // ====================================================================
        // Use FPlatformTime::Seconds() for accurate real wall-clock timing.
        // This ensures hitstop duration is exact regardless of any world or
        // actor time dilation effects. The ticker runs every frame and checks
        // if enough real time has elapsed.

        const double HitstopEndTime = FPlatformTime::Seconds() + static_cast<double>(HitPauseDuration);

        // Convert raw pointers to weak references for safe capture
        TArray<TWeakObjectPtr<AActor>> WeakActorsToRestore;
        WeakActorsToRestore.Reserve(ActorsToFreeze.Num());
        for (AActor* Actor : ActorsToFreeze)
        {
            WeakActorsToRestore.Add(Actor);
        }

        // Use FTSTicker (thread-safe ticker) to check platform time each frame
        // Returns true to continue ticking, false to remove the ticker
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([WeakActorsToRestore, HitstopEndTime](float DeltaTime) -> bool
            {
                // Check if enough REAL time has elapsed (unaffected by any time dilation)
                if (FPlatformTime::Seconds() >= HitstopEndTime)
                {
                    // Restore time dilation for all frozen actors
                    for (const TWeakObjectPtr<AActor>& WeakActor : WeakActorsToRestore)
                    {
                        if (AActor* Actor = WeakActor.Get())
                        {
                            Actor->CustomTimeDilation = 1.0f;

                            UE_LOG(LogTemp, Verbose, TEXT("[HITSTOP] Restored time dilation for %s"),
                                *Actor->GetName());
                        }
                    }

                    UE_LOG(LogTemp, Verbose, TEXT("[HITSTOP] Hitstop complete (platform time)"));

                    // Remove ticker - hitstop is complete
                    return false;
                }

                // Continue ticking until enough real time has passed
                return true;
            })
        );
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
