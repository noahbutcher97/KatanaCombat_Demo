// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_HoldWindow.h"
// V1 REMOVED: #include "Core/CombatComponent.h"

UAnimNotifyState_HoldWindow::UAnimNotifyState_HoldWindow()
{
}

// V1 REMOVED: V1 callback functions no longer needed (base class pure virtuals removed)
// void UAnimNotifyState_HoldWindow::OnOpenWindow_V1(UCombatComponent* CombatComp, float Duration)
// {
//     if (CombatComp)
//     {
//         CombatComp->OpenHoldWindow(Duration);
//     }
// }
//
// void UAnimNotifyState_HoldWindow::OnCloseWindow_V1(UCombatComponent* CombatComp)
// {
//     if (CombatComp)
//     {
//         CombatComp->CloseHoldWindow();
//     }
// }

FString UAnimNotifyState_HoldWindow::GetNotifyName_Implementation() const
{
    return TEXT("Hold Window");
}