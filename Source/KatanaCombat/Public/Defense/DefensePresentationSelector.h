// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"

class UDefenseConfiguration;

class KATANACOMBAT_API IDefensePresentationSelector
{
public:
	virtual ~IDefensePresentationSelector() = default;

	virtual FDefensePresentationSelectionResult SelectDefender(
		const FDefensePresentationSelectionContext& Context,
		const UDefenseConfiguration* Configuration) const = 0;

	virtual FDefensePresentationSelectionResult SelectAttacker(
		const FDefensePresentationSelectionContext& Context,
		const UDefenseConfiguration* AttackerConfiguration) const = 0;
};

class KATANACOMBAT_API FTableDefensePresentationSelector final
	: public IDefensePresentationSelector
{
public:
	FDefensePresentationSelectionResult SelectDefender(
		const FDefensePresentationSelectionContext& Context,
		const UDefenseConfiguration* Configuration) const override;

	FDefensePresentationSelectionResult SelectAttacker(
		const FDefensePresentationSelectionContext& Context,
		const UDefenseConfiguration* AttackerConfiguration) const override;

	/** Select only the explicit wildcard defender fallback for payload composition. */
	FDefensePresentationSelectionResult SelectGenericDefender(
		const FDefensePresentationSelectionContext& Context,
		const UDefenseConfiguration* Configuration) const;

	/** Select only the explicit wildcard attacker fallback for an unusable exact montage. */
	FDefensePresentationSelectionResult SelectGenericAttacker(
		const FDefensePresentationSelectionContext& Context,
		const UDefenseConfiguration* AttackerConfiguration) const;
};
