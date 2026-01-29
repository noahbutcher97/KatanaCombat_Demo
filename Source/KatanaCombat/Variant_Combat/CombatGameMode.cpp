// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Combat/CombatGameMode.h"
#include "Debug/CombatDebugHUD.h"

ACombatGameMode::ACombatGameMode()
{
	// Set the debug HUD class for combat visualization
	HUDClass = ACombatDebugHUD::StaticClass();
}