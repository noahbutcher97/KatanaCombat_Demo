// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/AssetAuthoringApprovalService.h"

using FDefenseMatrixAuthoringApprovalContract =
	FKatanaAssetAuthoringApprovalContract;

struct FDefenseProofManifest;

/** Authors the fixed Gate B defense-matrix montage, configuration, and playable fixture. */
class KATANACOMBATEDITOR_API FDefenseMatrixAuthoringOperation
{
public:
	static const FString OperationName;

	static TArray<FString> GetDestinationPackageNames();
	static FString ComputeApprovalFingerprint(
		const FDefenseMatrixAuthoringApprovalContract& Contract);
	static bool BuildCurrentApprovalContract(
		FDefenseMatrixAuthoringApprovalContract& OutContract,
		TArray<FString>& OutErrors);
	static bool ValidateApprovedPlanJson(
		const FString& Json,
		const FString& ApprovedPlanFingerprint,
		const FDefenseMatrixAuthoringApprovalContract& CurrentContract,
		TArray<FString>& OutErrors);
	static bool ValidateManifestCatalog(
		const FDefenseProofManifest& Manifest,
		TArray<FString>& OutErrors);

	bool Run(
		const FKatanaAssetMigrationOptions& Options,
		FKatanaAssetMigrationReport& OutReport) const;
};
