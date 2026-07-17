// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/KatanaAssetMigrationTypes.h"

/** Deterministic state that a reviewed DefenseProofAuthoring Plan authorizes. */
struct FDefenseProofAuthoringApprovalContract
{
	int32 RecipeVersion = 0;
	FString RecipeFactsHash;
	FString SourceStateHash;
	FString DestinationStateHash;
	FString ManifestHash;
	int32 SourceStateCount = 0;
	int32 DestinationStateCount = 0;
	TArray<FString> ProposedChanges;
	TArray<FKatanaAssetMigrationPackageLedgerEntry> PackageLedger;
	FString Fingerprint;
};

/** Authors the fixed, visually reviewed Gate A defense presentation assets. */
class KATANACOMBATEDITOR_API FDefenseProofAuthoringOperation
{
public:
	static const FString OperationName;

	static TArray<FString> GetDestinationPackageNames();
	static FString ComputeApprovalFingerprint(
		const FDefenseProofAuthoringApprovalContract& Contract);
	static bool BuildCurrentApprovalContract(
		FDefenseProofAuthoringApprovalContract& OutContract,
		TArray<FString>& OutErrors);
	static bool ValidateApprovedPlanJson(
		const FString& Json,
		const FString& ApprovedPlanFingerprint,
		const FDefenseProofAuthoringApprovalContract& CurrentContract,
		TArray<FString>& OutErrors);
	static bool ValidateApprovedPlanBinding(
		const FKatanaAssetMigrationOptions& Options,
		const FDefenseProofAuthoringApprovalContract& CurrentContract,
		TArray<FString>& OutErrors);

	bool Run(
		const FKatanaAssetMigrationOptions& Options,
		FKatanaAssetMigrationReport& OutReport) const;
};
