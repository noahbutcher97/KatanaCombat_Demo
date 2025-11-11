// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/AttackConfiguration.h"

UAttackConfiguration::UAttackConfiguration()
{
	// Initialize to nullptr - must be configured in data asset
	DefaultLightAttack = nullptr;
	DefaultHeavyAttack = nullptr;
	SprintAttack = nullptr;
	JumpAttack = nullptr;
	PlungingAttack = nullptr;
}