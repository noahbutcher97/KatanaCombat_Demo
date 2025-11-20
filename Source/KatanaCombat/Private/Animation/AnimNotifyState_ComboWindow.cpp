// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_ComboWindow.h"
// V1 REMOVED: #include "Core/CombatComponent.h"

UAnimNotifyState_ComboWindow::UAnimNotifyState_ComboWindow()
{
}

// V1 REMOVED: V1 callback functions no longer needed (base class pure virtuals removed)
// void UAnimNotifyState_ComboWindow::OnOpenWindow_V1(UCombatComponent* CombatComp, float Duration)
// {
//     if (CombatComp)
//     {
//         CombatComp->OpenComboWindow(Duration);
//     }
// }
//
// void UAnimNotifyState_ComboWindow::OnCloseWindow_V1(UCombatComponent* CombatComp)
// {
//     if (CombatComp)
//     {
//         CombatComp->CloseComboWindow();
//     }
// }

FString UAnimNotifyState_ComboWindow::GetNotifyName_Implementation() const
{
	return TEXT("Combo Window");
}