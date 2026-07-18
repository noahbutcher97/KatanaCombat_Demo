// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/KatanaAssetMigrationTypes.h"

class UAttackData;

class KATANACOMBATEDITOR_API FAttackDataNotifyMigrationOperation
{
public:
	static const FString OperationName;

	static bool BuildPlanReport(
		const FKatanaAssetMigrationOptions& Options,
		const TArray<UAttackData*>& Targets,
		FKatanaAssetMigrationReport& OutReport,
		TArray<FString>& OutErrors);
	static bool FinalizePlanReportFingerprint(
		FKatanaAssetMigrationReport& InOutReport,
		TArray<FString>& OutErrors);
	static bool ValidateApprovedPlanJson(
		const FString& Json,
		const FString& ApprovedPlanFingerprint,
		const FKatanaAssetMigrationReport& CurrentPlanReport,
		TArray<FString>& OutErrors);
	static bool ValidateApprovedPlanBinding(
		const FKatanaAssetMigrationOptions& Options,
		const FKatanaAssetMigrationReport& CurrentPlanReport,
		TArray<FString>& OutErrors);

	bool Run(UAttackData* AttackData, EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const;
};
