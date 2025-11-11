// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_ParryWindow.h"
#include "Core/CombatComponent.h"

UAnimNotifyState_ParryWindow::UAnimNotifyState_ParryWindow()
{
}

void UAnimNotifyState_ParryWindow::OnOpenWindow_V1(UCombatComponent* CombatComp, float Duration)
{
	if (CombatComp)
	{
		CombatComp->OpenParryWindow(Duration);
	}
}

void UAnimNotifyState_ParryWindow::OnCloseWindow_V1(UCombatComponent* CombatComp)
{
	if (CombatComp)
	{
		CombatComp->CloseParryWindow();
	}
}

FString UAnimNotifyState_ParryWindow::GetNotifyName_Implementation() const
{
	return TEXT("Parry Window");
}