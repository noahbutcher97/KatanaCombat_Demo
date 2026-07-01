// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/KatanaAssetMigrationTypes.h"

class UObject;

class KATANACOMBATEDITOR_API FContentReadinessAuditOperation
{
public:
	static const FString OperationName;

	bool Run(const FString& TargetString, EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const;
	bool RunLoadedObject(const FString& TargetString, UObject* LoadedObject, bool bPackageFileExists, FKatanaAssetMigrationRow& OutRow) const;
};
