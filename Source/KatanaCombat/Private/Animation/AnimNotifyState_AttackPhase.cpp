// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_AttackPhase.h"
#include "Interfaces/CombatInterface.h"

UAnimNotifyState_AttackPhase::UAnimNotifyState_AttackPhase()
{
    Phase = EAttackPhase::Windup;
}

void UAnimNotifyState_AttackPhase::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
    
    if (!MeshComp || !MeshComp->GetOwner())
    {
        return;
    }
    
    // Route to combat interface
    if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(MeshComp->GetOwner()))
    {
        CombatInterface->Execute_OnAttackPhaseBegin(MeshComp->GetOwner(), Phase);
    }
}

void UAnimNotifyState_AttackPhase::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyEnd(MeshComp, Animation, EventReference);
    
    if (!MeshComp || !MeshComp->GetOwner())
    {
        return;
    }
    
    // Route to combat interface
    if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(MeshComp->GetOwner()))
    {
        CombatInterface->Execute_OnAttackPhaseEnd(MeshComp->GetOwner(), Phase);
    }
}

FString UAnimNotifyState_AttackPhase::GetNotifyName_Implementation() const
{
    FString PhaseName;
    switch (Phase)
    {
        case EAttackPhase::Windup:
            PhaseName = TEXT("Windup");
            break;
        case EAttackPhase::Active:
            PhaseName = TEXT("Active");
            break;
        case EAttackPhase::Recovery:
            PhaseName = TEXT("Recovery");
            break;
        default:
            PhaseName = TEXT("Unknown");
            break;
    }
    
    return FString::Printf(TEXT("Attack Phase: %s"), *PhaseName);
}