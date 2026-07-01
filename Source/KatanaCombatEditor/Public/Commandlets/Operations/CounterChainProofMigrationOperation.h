// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/KatanaAssetMigrationTypes.h"

class UAttackData;
class UPairedAnimationData;

struct FCounterChainProofTargetSpec
{
	FString InputTarget;
	FString AttackDataObjectPath;
	FString CounterDataPackageName;
	FString CounterDataObjectPath;
	FString TemplatePackageName;
	FString TemplateObjectPath;
};

class KATANACOMBATEDITOR_API FCounterChainProofMigrationOperation
{
public:
	static const FString OperationName;

	static bool ParseTargetSpec(const FString& TargetString, FCounterChainProofTargetSpec& OutSpec, FString& OutError);
	static void SnapshotInitiallyDirtyPackages(const TArray<FString>& TargetStrings, TSet<FString>& OutDirtyPackages);

	bool Run(const FString& TargetString, EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const;
	bool RunLoadedObjects(
		const FCounterChainProofTargetSpec& Spec,
		UAttackData* AttackData,
		UPairedAnimationData* ExistingCounterData,
		UPairedAnimationData* TemplateData,
		EKatanaAssetMigrationMode Mode,
		FKatanaAssetMigrationRow& OutRow) const;
};
