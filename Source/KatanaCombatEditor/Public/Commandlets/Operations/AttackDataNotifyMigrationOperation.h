// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/KatanaAssetMigrationTypes.h"

class UAttackData;

class KATANACOMBATEDITOR_API FAttackDataNotifyMigrationOperation
{
public:
	static const FString OperationName;

	bool Run(UAttackData* AttackData, EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const;
};
