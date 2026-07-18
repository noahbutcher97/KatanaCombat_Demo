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
	static bool SerializeReport(
		const FKatanaAssetMigrationReport& Report,
		FString& OutJson,
		TArray<FString>& OutErrors);
	static bool WriteReport(const FKatanaAssetMigrationReport& Report, const FString& ReportPath, TArray<FString>& OutErrors);
	static bool RequiresFreshProcessAudit(
		const FKatanaAssetMigrationReport& Report);
	static void AnnotateFreshProcessAuditRequirement(
		FKatanaAssetMigrationReport& Report);
	bool SaveChangedPackages(
		const FKatanaAssetMigrationOptions& Options,
		const TSet<FString>& InitiallyDirtyPackages,
		FKatanaAssetMigrationReport& Report) const;

#if WITH_AUTOMATION_TESTS
	static void SetForcedSaveFailurePackageForTesting(const FString& PackageName);
#endif

	EKatanaAssetMigrationExitCode Run(const FKatanaAssetMigrationOptions& Options);

private:
	bool LoadTargetStrings(const FKatanaAssetMigrationOptions& Options, TArray<FString>& OutTargetStrings, TArray<FKatanaAssetMigrationRow>& OutFailedRows) const;
	bool LoadTargets(const FKatanaAssetMigrationOptions& Options, TArray<UAttackData*>& OutTargets, TArray<FKatanaAssetMigrationRow>& OutFailedRows) const;
	bool RunAttackDataNotifyMigration(const FKatanaAssetMigrationOptions& Options, const TArray<UAttackData*>& Targets, FKatanaAssetMigrationReport& OutReport) const;
	bool RunAttackDataTimingMigration(const FKatanaAssetMigrationOptions& Options, const TArray<UAttackData*>& Targets, FKatanaAssetMigrationReport& OutReport) const;
	bool RunCounterChainProofMigration(const FKatanaAssetMigrationOptions& Options, FKatanaAssetMigrationReport& OutReport) const;
	bool RunDefenseProofMigration(const FKatanaAssetMigrationOptions& Options, FKatanaAssetMigrationReport& OutReport) const;
	bool RunDefenseProofAuthoring(const FKatanaAssetMigrationOptions& Options, FKatanaAssetMigrationReport& OutReport) const;
	bool RunDefenseMatrixAuthoring(const FKatanaAssetMigrationOptions& Options, FKatanaAssetMigrationReport& OutReport) const;
	bool RunContentReadinessAudit(const FKatanaAssetMigrationOptions& Options, FKatanaAssetMigrationReport& OutReport) const;
	bool RunEnemyAIProofAssets(const FKatanaAssetMigrationOptions& Options, FKatanaAssetMigrationReport& OutReport) const;
};
