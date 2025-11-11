// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_ComboWindow.h"
#include "Core/CombatComponent.h"

UAnimNotifyState_ComboWindow::UAnimNotifyState_ComboWindow()
{
}

void UAnimNotifyState_ComboWindow::OnOpenWindow_V1(UCombatComponent* CombatComp, float Duration)
{
	if (CombatComp)
	{
		CombatComp->OpenComboWindow(Duration);
	}
}

void UAnimNotifyState_ComboWindow::OnCloseWindow_V1(UCombatComponent* CombatComp)
{
	if (CombatComp)
	{
		CombatComp->CloseComboWindow();
	}
}

FString UAnimNotifyState_ComboWindow::GetNotifyName_Implementation() const
{
	return TEXT("Combo Window");
}