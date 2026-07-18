// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/KatanaAssetMigrationTypes.h"
#include "DefenseAssetValidationService.h"

struct FDefenseProofMigrationPlan
{
	FDefenseProofManifest Manifest;
	FDefenseAssetValidationResult Validation;
	FString CanonicalManifest;
	FString CanonicalAssetFacts;
	TArray<FString> ProposedChanges;
	TArray<FKatanaAssetMigrationPackageLedgerEntry> PackageLedger;
	FString Fingerprint;
	bool bRequiresTimingMutation = false;
};

class KATANACOMBATEDITOR_API FDefenseProofMigrationOperation
{
public:
	static const FString OperationName;

	static bool CanonicalizeJson(
		const FString& Json,
		FString& OutCanonicalJson,
		FString& OutError);

	static FString ComputePlanFingerprint(
		const FString& CanonicalManifest,
		const FString& CanonicalAssetFacts,
		const TArray<FString>& ProposedChanges,
		const TArray<FKatanaAssetMigrationPackageLedgerEntry>& PackageLedger);

	static bool BuildLoadedPlan(
		const FDefenseProofManifest& Manifest,
		const FString& CanonicalManifest,
		const FDefenseProofAssetSet& Assets,
		FDefenseProofMigrationPlan& OutPlan,
		TArray<FString>& OutErrors,
		bool bRejectDirtyApprovalPackages = true);

	static bool ApplyLoadedPlan(
		const FDefenseProofMigrationPlan& Plan,
		const FDefenseProofAssetSet& Assets,
		bool bAllowTimingMutation,
		TSet<FString>& OutChangedPackages,
		TArray<FString>& OutErrors);

	static bool ValidateApprovedPlanBinding(
		const FKatanaAssetMigrationOptions& Options,
		const FDefenseProofMigrationPlan& CurrentPlan,
		TArray<FString>& OutErrors);

	static void BuildPlanReport(
		const FString& ManifestPath,
		const FDefenseProofMigrationPlan& Plan,
		FKatanaAssetMigrationReport& OutReport);

	static bool ValidateChangedPackageSet(
		const FDefenseProofMigrationPlan& Plan,
		const TSet<FString>& ChangedPackages,
		TArray<FString>& OutErrors);

	static bool ValidateInitialDirtyPackageGate(
		const FDefenseProofMigrationPlan& Plan,
		bool bAllowDirtyPackages,
		TArray<FString>& OutErrors);

	static bool ChangeTargetsValidationRow(
		const FString& Change,
		const FDefenseAssetValidationRow& ValidationRow);

	static bool FindingTargetsValidationRow(
		const FString& FindingContext,
		const FDefenseAssetValidationRow& ValidationRow);

	bool Run(
		const FString& ManifestPath,
		const FKatanaAssetMigrationOptions& Options,
		FKatanaAssetMigrationReport& OutReport) const;
};
