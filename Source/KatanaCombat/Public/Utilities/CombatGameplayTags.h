// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

namespace KatanaCombatGameplayTags
{
	KATANACOMBAT_API FGameplayTag AttackPropertyUnblockable();
	KATANACOMBAT_API FGameplayTag AttackDefenseParryable();
	KATANACOMBAT_API FGameplayTag AttackDefenseBlockInterruptible();
	KATANACOMBAT_API FGameplayTag ContextParryCounter();
	KATANACOMBAT_API FGameplayTag ContextLowHealthFinisher();
}
