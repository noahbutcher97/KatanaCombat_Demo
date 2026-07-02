// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/KatanaAssetMigrationTypes.h"

class UBlueprint;
class UStateTree;

class KATANACOMBATEDITOR_API FEnemyAIProofAssetsOperation
{
public:
	static const FString OperationName;

	static void SnapshotInitiallyDirtyPackages(TSet<FString>& OutDirtyPackages);

	bool Run(EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const;

private:
	bool RunInternal(EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const;
};
