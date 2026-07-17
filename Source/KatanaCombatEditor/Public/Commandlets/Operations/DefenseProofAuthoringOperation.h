// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/KatanaAssetMigrationTypes.h"

/** Authors the fixed, visually reviewed Gate A defense presentation assets. */
class KATANACOMBATEDITOR_API FDefenseProofAuthoringOperation
{
public:
	static const FString OperationName;

	static TArray<FString> GetDestinationPackageNames();

	bool Run(
		const FKatanaAssetMigrationOptions& Options,
		FKatanaAssetMigrationReport& OutReport) const;
};
