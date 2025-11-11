// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_HoldWindow.h"
#include "Core/CombatComponent.h"

UAnimNotifyState_HoldWindow::UAnimNotifyState_HoldWindow()
{
}

void UAnimNotifyState_HoldWindow::OnOpenWindow_V1(UCombatComponent* CombatComp, float Duration)
{
    if (CombatComp)
    {
        CombatComp->OpenHoldWindow(Duration);
    }
}

void UAnimNotifyState_HoldWindow::OnCloseWindow_V1(UCombatComponent* CombatComp)
{
    if (CombatComp)
    {
        CombatComp->CloseHoldWindow();
    }
}

FString UAnimNotifyState_HoldWindow::GetNotifyName_Implementation() const
{
    return TEXT("Hold Window");
}