// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/KatanaAssetMigrationTypes.h"

class UAttackData;

class KATANACOMBATEDITOR_API FKatanaAssetMigrationRunner
{
public:
	static bool ParseOptions(const FString& Params, FKatanaAssetMigrationOptions& OutOptions, TArray<FString>& OutErrors);
	static bool ValidateOptions(const FKatanaAssetMigrationOptions& Options, TArray<FString>& OutErrors);
	static bool NormalizeAttackDataTargetObjectPath(const FString& TargetString, FString& OutObjectPath, FString& OutError);
	static bool NormalizeContentTargetObjectPath(const FString& TargetString, FString& OutPackageName, FString& OutObjectPath, FString& OutError);
	static FString ResolveProjectRelativeFilePath(const FString& FilePath);
	static void Summarize(FKatanaAssetMigrationReport& Report);
	static bool WriteReport(const FKatanaAssetMigrationReport& Report, const FString& ReportPath, TArray<FString>& OutErrors);

	EKatanaAssetMigrationExitCode Run(const FKatanaAssetMigrationOptions& Options);

private:
	bool LoadTargetStrings(const FKatanaAssetMigrationOptions& Options, TArray<FString>& OutTargetStrings, TArray<FKatanaAssetMigrationRow>& OutFailedRows) const;
	bool LoadTargets(const FKatanaAssetMigrationOptions& Options, TArray<UAttackData*>& OutTargets, TArray<FKatanaAssetMigrationRow>& OutFailedRows) const;
	bool RunAttackDataNotifyMigration(const FKatanaAssetMigrationOptions& Options, const TArray<UAttackData*>& Targets, FKatanaAssetMigrationReport& OutReport) const;
	bool RunAttackDataTimingMigration(const FKatanaAssetMigrationOptions& Options, const TArray<UAttackData*>& Targets, FKatanaAssetMigrationReport& OutReport) const;
	bool RunCounterChainProofMigration(const FKatanaAssetMigrationOptions& Options, FKatanaAssetMigrationReport& OutReport) const;
	bool RunContentReadinessAudit(const FKatanaAssetMigrationOptions& Options, FKatanaAssetMigrationReport& OutReport) const;
	bool SaveChangedPackages(const FKatanaAssetMigrationOptions& Options, const TSet<FString>& InitiallyDirtyPackages, FKatanaAssetMigrationReport& Report) const;
};
