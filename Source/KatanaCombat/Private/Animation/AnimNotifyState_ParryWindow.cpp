// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_ParryWindow.h"
// V1 REMOVED: #include "Core/CombatComponent.h"

UAnimNotifyState_ParryWindow::UAnimNotifyState_ParryWindow()
{
}

// V1 REMOVED: V1 callback functions no longer needed (base class pure virtuals removed)
// void UAnimNotifyState_ParryWindow::OnOpenWindow_V1(UCombatComponent* CombatComp, float Duration)
// {
//     if (CombatComp)
//     {
//         CombatComp->OpenParryWindow(Duration);
//     }
// }
//
// void UAnimNotifyState_ParryWindow::OnCloseWindow_V1(UCombatComponent* CombatComp)
// {
//     if (CombatComp)
//     {
//         CombatComp->CloseParryWindow();
//     }
// }

FString UAnimNotifyState_ParryWindow::GetNotifyName_Implementation() const
{
	return TEXT("Parry Window");
}