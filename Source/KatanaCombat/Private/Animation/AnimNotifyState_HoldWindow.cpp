// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_HoldWindow.h"
#include "Core/CombatComponent.h"
#include "GameFramework/Character.h"

UAnimNotifyState_HoldWindow::UAnimNotifyState_HoldWindow()
{
}

void UAnimNotifyState_HoldWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

    if (!MeshComp || !MeshComp->GetOwner())
    {
        return;
    }

    ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner());
    if (!Character)
    {
        return;
    }

    // Find combat component and open hold window
    if (UCombatComponent* CombatComp = Character->FindComponentByClass<UCombatComponent>())
    {
        CombatComp->OpenHoldWindow(TotalDuration);
    }
}

void UAnimNotifyState_HoldWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyEnd(MeshComp, Animation, EventReference);

    if (!MeshComp || !MeshComp->GetOwner())
    {
        return;
    }

    ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner());
    if (!Character)
    {
        return;
    }

    // Find combat component and close hold window
    if (UCombatComponent* CombatComp = Character->FindComponentByClass<UCombatComponent>())
    {
        CombatComp->CloseHoldWindow();
    }
}

FString UAnimNotifyState_HoldWindow::GetNotifyName_Implementation() const
{
    return TEXT("Hold Window");
}