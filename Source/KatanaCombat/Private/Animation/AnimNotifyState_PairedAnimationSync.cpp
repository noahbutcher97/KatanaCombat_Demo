// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/HitReactionComponent.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "Engine/World.h"

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

    // Find paired animation component to trigger sync point effects
    UPairedAnimationComponent* PairedComp = Owner->FindComponentByClass<UPairedAnimationComponent>();
    if (PairedComp)
    {
        // Trigger sync point effects (camera shake, etc.) AND broadcast delegate
        PairedComp->TriggerSyncPointEffects(SyncPointName);
    }

    // Also check for HitReactionComponent (in case this is on victim's montage)
    UHitReactionComponent* HitReactionComp = Owner->FindComponentByClass<UHitReactionComponent>();
    if (HitReactionComp)
    {
        HitReactionComp->OnPairedAnimationSyncPoint.Broadcast(ReactionType, SyncPointName);
    }

    // ========================================================================
    // ALIGNMENT VALIDATION
    // ========================================================================
    // At sync point, verify attacker and victim are properly aligned.
    // Misalignment indicates animation drift from root motion conflicts or
    // failed warp tracking. Log for debugging and optionally auto-correct.

    if (bValidateAlignment && bIsPrimarySyncPoint && PairedComp)
    {
        // Find the primary paired partner (victim in finisher scenario)
        AActor* PairedPartner = nullptr;
        if (PairedComp->PairedAnimationPartners.Num() > 0)
        {
            PairedPartner = PairedComp->PairedAnimationPartners[0].Get();
        }

        if (PairedPartner)
        {
            const FVector AttackerLocation = Owner->GetActorLocation();
            const FVector VictimLocation = PairedPartner->GetActorLocation();
            const float ActualDistance = FVector::Dist(AttackerLocation, VictimLocation);

            // Check for misalignment
            if (ActualDistance > MaxContactDistance)
            {
                // SEVERE MISALIGNMENT - animation will look wrong
                if (bLogMisalignment)
                {
                    UE_LOG(LogCombat, Warning,
                        TEXT("[SYNC VALIDATION] MISALIGNED at '%s': Distance %.1f > Max %.1f (Attacker: %s, Victim: %s)"),
                        *SyncPointName.ToString(),
                        ActualDistance,
                        MaxContactDistance,
                        *Owner->GetName(),
                        *PairedPartner->GetName());
                }

                // Still proceed with damage/effects (graceful degradation)
                // The animation will look off, but gameplay continues
            }
            else if (ActualDistance > NudgeThreshold && bLogMisalignment)
            {
                // MINOR MISALIGNMENT - within acceptable range but not perfect
                UE_LOG(LogCombat, Log,
                    TEXT("[SYNC VALIDATION] Minor drift at '%s': Distance %.1f (Threshold: %.1f, Max: %.1f)"),
                    *SyncPointName.ToString(),
                    ActualDistance,
                    NudgeThreshold,
                    MaxContactDistance);
            }

            // Auto-correct minor misalignment if enabled
            if (bNudgeOnMinorMisalignment && ActualDistance > NudgeThreshold && ActualDistance <= MaxContactDistance)
            {
                // Calculate correct victim position (maintain current offset direction, just adjust distance)
                const FVector DirectionToVictim = (VictimLocation - AttackerLocation).GetSafeNormal();
                const float DesiredDistance = NudgeThreshold * 0.8f;  // Target 80% of nudge threshold for buffer
                const FVector CorrectedLocation = AttackerLocation + DirectionToVictim * DesiredDistance;

                // Keep Z the same to avoid terrain issues
                FVector FinalLocation = CorrectedLocation;
                FinalLocation.Z = VictimLocation.Z;

                // Teleport victim to corrected position
                PairedPartner->SetActorLocation(FinalLocation, false, nullptr, ETeleportType::TeleportPhysics);

                if (bLogMisalignment)
                {
                    UE_LOG(LogCombat, Log,
                        TEXT("[SYNC VALIDATION] Nudged victim %s: %.1f -> %.1f units from attacker"),
                        *PairedPartner->GetName(),
                        ActualDistance,
                        FVector::Dist(AttackerLocation, FinalLocation));
                }
            }
        }
        else if (bLogMisalignment)
        {
            // Gap 19.2 fix: Log warning when paired partner is null at primary sync point
            // This indicates the partner was destroyed/teleported mid-animation
            UE_LOG(LogCombat, Warning,
                TEXT("[SYNC VALIDATION] %s at sync point '%s' but PairedPartner is null (destroyed during animation?)"),
                *Owner->GetName(),
                *SyncPointName.ToString());
        }
    }

    // ========================================================================
    // SAKURAI-STYLE HITSTOP (Hit Pause)
    // ========================================================================
    // Principle: Both attacker AND victim freeze for identical duration
    // Background, particles, and other actors continue (selective freeze)
    // Emphasizes impact power and gives player's eyes time to process

    if (bApplyHitstop && HitstopDuration > 0.0f && bIsPrimarySyncPoint)
    {
        // Collect actors to freeze (attacker + all paired partners)
        TArray<AActor*> ActorsToFreeze;
        ActorsToFreeze.Add(Owner);

        // Get paired partners from PairedAnimationComponent
        if (PairedComp)
        {
            for (const TWeakObjectPtr<AActor>& PartnerRef : PairedComp->PairedAnimationPartners)
            {
                if (AActor* Partner = PartnerRef.Get())
                {
                    ActorsToFreeze.Add(Partner);
                }
            }
        }

        if (UCinematicEffectsUtilityLibrary::ApplyHitstopToActors(ActorsToFreeze, HitstopDuration))
        {
            UE_LOG(LogCombat, Log, TEXT("[HITSTOP] %s: Freezing %d actors for %.3fs at sync point '%s'"),
                *Owner->GetName(), ActorsToFreeze.Num(), HitstopDuration, *SyncPointName.ToString());
        }
    }

    // Log sync point for debugging
    UE_LOG(LogCombat, Verbose, TEXT("PairedAnimationSync: %s reached sync point '%s' (Type: %d, Primary: %d)"),
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
