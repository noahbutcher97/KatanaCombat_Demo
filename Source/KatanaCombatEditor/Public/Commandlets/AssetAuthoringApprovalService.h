// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/KatanaAssetMigrationTypes.h"

/** Stable identity fields bound into a deterministic authoring approval. */
struct FKatanaAssetAuthoringIdentity
{
	FString Operation;
	FString ManifestPath;
	FString Gate;
	FString RowInputTarget;
	FString RowAssetClass;
};

/** Complete state that a reviewed authoring Plan authorizes. */
struct FKatanaAssetAuthoringApprovalContract
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

/** Shared hash, approval, and report rules for fixed asset-authoring recipes. */
class KATANACOMBATEDITOR_API FKatanaAssetAuthoringApprovalService
{
public:
	static FString HashText(const FString& Input);

	static bool BuildPackageStateHash(
		const TArray<FString>& Identities,
		const FString& Role,
		bool bRequired,
		bool bRejectDirty,
		FString& OutHash,
		int32& OutCount,
		TArray<FString>& OutErrors);

	static bool BuildManifestHash(
		const FString& ManifestPath,
		FString& OutHash,
		TArray<FString>& OutErrors);

	static FString ComputeFingerprint(
		const FKatanaAssetAuthoringIdentity& Identity,
		const FKatanaAssetAuthoringApprovalContract& Contract);

	static bool ValidateApprovedPlanJson(
		const FKatanaAssetAuthoringIdentity& Identity,
		const FString& Json,
		const FString& ApprovedPlanFingerprint,
		const FKatanaAssetAuthoringApprovalContract& CurrentContract,
		TArray<FString>& OutErrors);

	static bool ValidateApprovedPlanBinding(
		const FKatanaAssetAuthoringIdentity& Identity,
		const FKatanaAssetMigrationOptions& Options,
		const FKatanaAssetAuthoringApprovalContract& CurrentContract,
		TArray<FString>& OutErrors);

	static void PopulateReport(
		const FKatanaAssetAuthoringIdentity& Identity,
		const FKatanaAssetAuthoringApprovalContract& Contract,
		EKatanaAssetMigrationMode Mode,
		const TArray<FString>& Errors,
		int32 DestinationPackageCount,
		const TSet<FString>* ChangedPackages,
		FKatanaAssetMigrationReport& OutReport);
};
