// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/CombatGameplayTags.h"

namespace
{
FGameplayTag RequestCombatSemanticTag(const TCHAR* TagName)
{
	return FGameplayTag::RequestGameplayTag(FName(TagName), false);
}
}

FGameplayTag KatanaCombatGameplayTags::AttackPropertyUnblockable()
{
	static const FGameplayTag Tag = RequestCombatSemanticTag(TEXT("Attack.Property.Unblockable"));
	return Tag;
}

FGameplayTag KatanaCombatGameplayTags::ContextParryCounter()
{
	static const FGameplayTag Tag = RequestCombatSemanticTag(TEXT("Context.ParryCounter"));
	return Tag;
}

FGameplayTag KatanaCombatGameplayTags::ContextLowHealthFinisher()
{
	static const FGameplayTag Tag = RequestCombatSemanticTag(TEXT("Context.LowHealthFinisher"));
	return Tag;
}
