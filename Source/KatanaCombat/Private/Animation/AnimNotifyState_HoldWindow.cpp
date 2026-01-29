// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_HoldWindow.h"

UAnimNotifyState_HoldWindow::UAnimNotifyState_HoldWindow()
{
}

FString UAnimNotifyState_HoldWindow::GetNotifyName_Implementation() const
{
    return TEXT("Hold Window");
}
