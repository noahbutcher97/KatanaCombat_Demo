// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/AssetAuthoringApprovalService.h"

#include "Algo/Unique.h"
#include "Commandlets/KatanaAssetMigrationRunner.h"
#include "Commandlets/Operations/DefenseProofMigrationOperation.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

namespace
{
bool IsApplyMode(const EKatanaAssetMigrationMode Mode)
{
	return Mode == EKatanaAssetMigrationMode::Apply
		|| Mode == EKatanaAssetMigrationMode::ApplyAndSave;
}

void AppendPackageStateFact(
	const FString& Identity,
	const FString& Role,
	const bool bRequired,
	const bool bRejectDirty,
	TArray<FString>& OutFacts,
	TArray<FString>& OutErrors)
{
	const FString PackageName = Identity.Contains(TEXT("."))
		? FPackageName::ObjectPathToPackageName(Identity)
		: Identity;
	FString Filename;
	const bool bExists = FPackageName::DoesPackageExist(PackageName, &Filename);
	const UPackage* LoadedPackage = FindPackage(nullptr, *PackageName);
	const bool bDirty = LoadedPackage && LoadedPackage->IsDirty();
	if (bRejectDirty && bDirty)
	{
		OutErrors.Add(FString::Printf(
			TEXT("approval package has unsaved in-memory changes: %s"), *PackageName));
	}
	if (!bExists)
	{
		OutFacts.Add(FString::Printf(TEXT("%s|%s|missing|dirty=%d"),
			*Role, *Identity, bDirty));
		if (bRequired)
		{
			OutErrors.Add(FString::Printf(
				TEXT("approval dependency package is missing: %s"), *PackageName));
		}
		return;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
	{
		OutFacts.Add(FString::Printf(TEXT("%s|%s|unreadable|dirty=%d"),
			*Role, *Identity, bDirty));
		OutErrors.Add(FString::Printf(
			TEXT("approval package could not be hashed: %s"), *PackageName));
		return;
	}

	const FString FileHash = FSHA1::HashBuffer(
		Bytes.GetData(), static_cast<uint64>(Bytes.Num())).ToString();
	OutFacts.Add(FString::Printf(TEXT("%s|%s|sha1=%s|size=%d|dirty=%d"),
		*Role, *Identity, *FileHash, Bytes.Num(), bDirty));
}
}

FString FKatanaAssetAuthoringApprovalService::HashText(const FString& Input)
{
	FTCHARToUTF8 Utf8(*Input);
	return FSHA1::HashBuffer(
		Utf8.Get(), static_cast<uint64>(Utf8.Length())).ToString();
}

bool FKatanaAssetAuthoringApprovalService::BuildPackageStateHash(
	const TArray<FString>& Identities,
	const FString& Role,
	const bool bRequired,
	const bool bRejectDirty,
	FString& OutHash,
	int32& OutCount,
	TArray<FString>& OutErrors)
{
	TArray<FString> SortedIdentities = Identities;
	SortedIdentities.Sort();
	SortedIdentities.SetNum(Algo::Unique(SortedIdentities));
	TArray<FString> Facts;
	for (const FString& Identity : SortedIdentities)
	{
		AppendPackageStateFact(
			Identity, Role, bRequired, bRejectDirty, Facts, OutErrors);
	}
	Facts.Sort();
	OutCount = Facts.Num();
	OutHash = HashText(FString::Join(Facts, TEXT("\n")));
	return OutErrors.IsEmpty();
}

bool FKatanaAssetAuthoringApprovalService::BuildManifestHash(
	const FString& ManifestPath,
	FString& OutHash,
	TArray<FString>& OutErrors)
{
	FString Json;
	const FString ResolvedPath =
		FKatanaAssetMigrationRunner::ResolveProjectRelativeFilePath(ManifestPath);
	if (!FFileHelper::LoadFileToString(Json, *ResolvedPath))
	{
		OutErrors.Add(FString::Printf(
			TEXT("could not read authoring manifest: %s"), *ResolvedPath));
		OutHash = HashText(TEXT("missing"));
		return false;
	}

	FString CanonicalManifest;
	FString CanonicalError;
	if (!FDefenseProofMigrationOperation::CanonicalizeJson(
		Json, CanonicalManifest, CanonicalError))
	{
		OutErrors.Add(FString::Printf(
			TEXT("could not canonicalize authoring manifest: %s"), *CanonicalError));
		OutHash = HashText(TEXT("invalid"));
		return false;
	}
	OutHash = HashText(CanonicalManifest);
	return true;
}

FString FKatanaAssetAuthoringApprovalService::ComputeFingerprint(
	const FKatanaAssetAuthoringIdentity& Identity,
	const FKatanaAssetAuthoringApprovalContract& Contract)
{
	FString Input = FString::Printf(
		TEXT("operation=%s\nrecipe_version=%d\nrecipe_facts_hash=%s\nsource_state_hash=%s\nsource_state_count=%d\ndestination_state_hash=%s\ndestination_state_count=%d\nmanifest_path=%s\nmanifest_hash=%s\ngate=%s\nrow=%s\nclass=%s\n"),
		*Identity.Operation, Contract.RecipeVersion, *Contract.RecipeFactsHash,
		*Contract.SourceStateHash, Contract.SourceStateCount,
		*Contract.DestinationStateHash, Contract.DestinationStateCount,
		*Identity.ManifestPath, *Contract.ManifestHash, *Identity.Gate,
		*Identity.RowInputTarget, *Identity.RowAssetClass);
	TArray<FString> Changes = Contract.ProposedChanges;
	Changes.Sort();
	for (const FString& Change : Changes)
	{
		Input += FString::Printf(TEXT("change=%s\n"), *Change);
	}
	TArray<FKatanaAssetMigrationPackageLedgerEntry> Ledger = Contract.PackageLedger;
	Ledger.Sort([](const auto& Left, const auto& Right)
	{
		return Left.PackageName < Right.PackageName;
	});
	for (const FKatanaAssetMigrationPackageLedgerEntry& Entry : Ledger)
	{
		Input += FString::Printf(
			TEXT("package=%s\nrole=%s\ndirty=%s\naction=%s\n"),
			*Entry.PackageName, *Entry.PackageRole,
			*LexToString(Entry.bInitiallyDirty), *Entry.PlannedAction);
	}
	return HashText(Input);
}

bool FKatanaAssetAuthoringApprovalService::ValidateApprovedPlanJson(
	const FKatanaAssetAuthoringIdentity& Identity,
	const FString& Json,
	const FString& ApprovedPlanFingerprint,
	const FKatanaAssetAuthoringApprovalContract& CurrentContract,
	TArray<FString>& OutErrors)
{
	if (ApprovedPlanFingerprint.IsEmpty())
	{
		OutErrors.Add(TEXT("approved authoring fingerprint is required"));
		return false;
	}
	if (CurrentContract.Fingerprint != ComputeFingerprint(Identity, CurrentContract))
	{
		OutErrors.Add(TEXT("current authoring contract contains an invalid fingerprint"));
	}
	if (ApprovedPlanFingerprint != CurrentContract.Fingerprint)
	{
		OutErrors.Add(TEXT("approved authoring fingerprint differs from the current plan"));
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutErrors.Add(TEXT("approved authoring Plan is not valid JSON"));
		return false;
	}

	double SchemaVersion = 0.0;
	FString Operation;
	FString Mode;
	FString ManifestPath;
	FString Gate;
	FString Fingerprint;
	if (!Root->TryGetNumberField(TEXT("schema_version"), SchemaVersion)
		|| SchemaVersion != 2.0
		|| !Root->TryGetStringField(TEXT("operation"), Operation)
		|| Operation != Identity.Operation
		|| !Root->TryGetStringField(TEXT("mode"), Mode)
		|| Mode != TEXT("Plan")
		|| !Root->TryGetStringField(TEXT("manifest_path"), ManifestPath)
		|| ManifestPath != Identity.ManifestPath
		|| !Root->TryGetStringField(TEXT("gate"), Gate)
		|| Gate != Identity.Gate
		|| !Root->TryGetStringField(TEXT("plan_fingerprint"), Fingerprint)
		|| Fingerprint != CurrentContract.Fingerprint
		|| Fingerprint != ApprovedPlanFingerprint)
	{
		OutErrors.Add(TEXT("approved report is not the matching schema-v2 authoring Plan"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!Root->TryGetArrayField(TEXT("rows"), Rows) || !Rows || Rows->Num() != 1)
	{
		OutErrors.Add(TEXT("approved authoring report must contain exactly one recipe row"));
		return false;
	}
	const TSharedPtr<FJsonObject> Row = (*Rows)[0].IsValid()
		? (*Rows)[0]->AsObject() : nullptr;
	FString InputTarget;
	FString AssetClass;
	FString Status;
	const FString ExpectedStatus = CurrentContract.ProposedChanges.IsEmpty()
		? TEXT("Unchanged") : TEXT("WouldChange");
	if (!Row.IsValid()
		|| !Row->TryGetStringField(TEXT("input_target"), InputTarget)
		|| InputTarget != Identity.RowInputTarget
		|| !Row->TryGetStringField(TEXT("asset_class"), AssetClass)
		|| AssetClass != Identity.RowAssetClass
		|| !Row->TryGetStringField(TEXT("status"), Status)
		|| Status != ExpectedStatus)
	{
		OutErrors.Add(TEXT("approved authoring recipe row identity or status differs"));
		return false;
	}
	for (const TCHAR* EmptyArrayField : {
		TEXT("errors"), TEXT("planned_removals"),
		TEXT("changed_packages"), TEXT("saved_packages")})
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Row->TryGetArrayField(EmptyArrayField, Values)
			|| !Values || !Values->IsEmpty())
		{
			OutErrors.Add(FString::Printf(
				TEXT("approved authoring Plan row must have an empty %s array"),
				EmptyArrayField));
			return false;
		}
	}

	const TSharedPtr<FJsonObject>* Details = nullptr;
	FString RecipeVersion;
	FString RecipeHash;
	FString SourceHash;
	FString SourceCount;
	FString DestinationHash;
	FString DestinationCount;
	FString ManifestHash;
	if (!Row->TryGetObjectField(TEXT("details"), Details)
		|| !Details || !Details->IsValid()
		|| !(*Details)->TryGetStringField(TEXT("recipe_version"), RecipeVersion)
		|| RecipeVersion != LexToString(CurrentContract.RecipeVersion)
		|| !(*Details)->TryGetStringField(TEXT("recipe_facts_hash"), RecipeHash)
		|| RecipeHash != CurrentContract.RecipeFactsHash
		|| !(*Details)->TryGetStringField(TEXT("source_state_hash"), SourceHash)
		|| SourceHash != CurrentContract.SourceStateHash
		|| !(*Details)->TryGetStringField(TEXT("source_state_count"), SourceCount)
		|| SourceCount != LexToString(CurrentContract.SourceStateCount)
		|| !(*Details)->TryGetStringField(TEXT("destination_state_hash"), DestinationHash)
		|| DestinationHash != CurrentContract.DestinationStateHash
		|| !(*Details)->TryGetStringField(TEXT("destination_state_count"), DestinationCount)
		|| DestinationCount != LexToString(CurrentContract.DestinationStateCount)
		|| !(*Details)->TryGetStringField(TEXT("manifest_hash"), ManifestHash)
		|| ManifestHash != CurrentContract.ManifestHash)
	{
		OutErrors.Add(TEXT("approved authoring recipe or state facts differ"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* AdditionValues = nullptr;
	if (!Row->TryGetArrayField(TEXT("planned_additions"), AdditionValues)
		|| !AdditionValues
		|| AdditionValues->Num() != CurrentContract.ProposedChanges.Num())
	{
		OutErrors.Add(TEXT("approved authoring planned additions differ in cardinality"));
		return false;
	}
	for (int32 Index = 0; Index < AdditionValues->Num(); ++Index)
	{
		FString Addition;
		if (!(*AdditionValues)[Index].IsValid()
			|| !(*AdditionValues)[Index]->TryGetString(Addition)
			|| Addition != CurrentContract.ProposedChanges[Index])
		{
			OutErrors.Add(TEXT("approved authoring planned additions differ"));
			return false;
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* LedgerValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("package_ledger"), LedgerValues)
		|| !LedgerValues
		|| LedgerValues->Num() != CurrentContract.PackageLedger.Num())
	{
		OutErrors.Add(TEXT("approved authoring package ledger differs in cardinality"));
		return false;
	}
	for (int32 Index = 0; Index < LedgerValues->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Object = (*LedgerValues)[Index].IsValid()
			? (*LedgerValues)[Index]->AsObject() : nullptr;
		FString PackageName;
		FString PackageRole;
		FString PlannedAction;
		FString ActualAction;
		FString SaveResult;
		FString ReloadResult;
		bool bInitiallyDirty = false;
		const FKatanaAssetMigrationPackageLedgerEntry& Current =
			CurrentContract.PackageLedger[Index];
		if (!Object.IsValid()
			|| !Object->TryGetStringField(TEXT("package_name"), PackageName)
			|| PackageName != Current.PackageName
			|| !Object->TryGetStringField(TEXT("package_role"), PackageRole)
			|| PackageRole != Current.PackageRole
			|| !Object->TryGetBoolField(TEXT("initially_dirty"), bInitiallyDirty)
			|| bInitiallyDirty != Current.bInitiallyDirty
			|| !Object->TryGetStringField(TEXT("planned_action"), PlannedAction)
			|| PlannedAction != Current.PlannedAction
			|| !Object->TryGetStringField(TEXT("actual_action"), ActualAction)
			|| ActualAction != TEXT("None")
			|| !Object->TryGetStringField(TEXT("save_result"), SaveResult)
			|| SaveResult != TEXT("NotRun")
			|| !Object->TryGetStringField(TEXT("post_save_reload_result"), ReloadResult)
			|| ReloadResult != TEXT("NotRun"))
		{
			OutErrors.Add(TEXT("approved authoring package ledger differs"));
			return false;
		}
	}
	return OutErrors.IsEmpty();
}

bool FKatanaAssetAuthoringApprovalService::ValidateApprovedPlanBinding(
	const FKatanaAssetAuthoringIdentity& Identity,
	const FKatanaAssetMigrationOptions& Options,
	const FKatanaAssetAuthoringApprovalContract& CurrentContract,
	TArray<FString>& OutErrors)
{
	if (Options.ApprovedPlanReport.IsEmpty()
		|| Options.ApprovedPlanFingerprint.IsEmpty())
	{
		OutErrors.Add(TEXT("approved authoring Plan report and fingerprint are required"));
		return false;
	}
	FString Json;
	const FString ReportPath =
		FKatanaAssetMigrationRunner::ResolveProjectRelativeFilePath(
			Options.ApprovedPlanReport);
	if (!FFileHelper::LoadFileToString(Json, *ReportPath))
	{
		OutErrors.Add(FString::Printf(
			TEXT("could not read approved authoring Plan: %s"), *ReportPath));
		return false;
	}
	return ValidateApprovedPlanJson(
		Identity, Json, Options.ApprovedPlanFingerprint, CurrentContract, OutErrors);
}

void FKatanaAssetAuthoringApprovalService::PopulateReport(
	const FKatanaAssetAuthoringIdentity& Identity,
	const FKatanaAssetAuthoringApprovalContract& Contract,
	const EKatanaAssetMigrationMode Mode,
	const TArray<FString>& Errors,
	const int32 DestinationPackageCount,
	const TSet<FString>* ChangedPackages,
	FKatanaAssetMigrationReport& OutReport)
{
	OutReport = FKatanaAssetMigrationReport();
	OutReport.SchemaVersion = 2;
	OutReport.Operation = Identity.Operation;
	OutReport.Mode = Mode;
	OutReport.ManifestPath = Identity.ManifestPath;
	OutReport.Gate = Identity.Gate;
	OutReport.PlanFingerprint = Contract.Fingerprint;
	OutReport.PackageLedger = Contract.PackageLedger;

	FKatanaAssetMigrationRow Row;
	Row.InputTarget = Identity.RowInputTarget;
	Row.AssetClass = Identity.RowAssetClass;
	Row.Details.Add(TEXT("recipe_version"), LexToString(Contract.RecipeVersion));
	Row.Details.Add(TEXT("recipe_facts_hash"), Contract.RecipeFactsHash);
	Row.Details.Add(TEXT("source_state_hash"), Contract.SourceStateHash);
	Row.Details.Add(TEXT("source_state_count"), LexToString(Contract.SourceStateCount));
	Row.Details.Add(TEXT("destination_state_hash"), Contract.DestinationStateHash);
	Row.Details.Add(TEXT("destination_state_count"),
		LexToString(Contract.DestinationStateCount));
	Row.Details.Add(TEXT("manifest_hash"), Contract.ManifestHash);
	Row.Details.Add(TEXT("destination_package_count"),
		LexToString(DestinationPackageCount));
	Row.PlannedAdditions = Contract.ProposedChanges;
	Row.Errors = Errors;
	if (!Errors.IsEmpty())
	{
		Row.Status = EKatanaAssetMigrationStatus::Failed;
	}
	else if (IsApplyMode(Mode))
	{
		Row.Status = ChangedPackages && !ChangedPackages->IsEmpty()
			? EKatanaAssetMigrationStatus::Changed
			: EKatanaAssetMigrationStatus::Unchanged;
	}
	else
	{
		Row.Status = Contract.ProposedChanges.IsEmpty()
			? EKatanaAssetMigrationStatus::Unchanged
			: EKatanaAssetMigrationStatus::WouldChange;
	}
	if (ChangedPackages)
	{
		Row.ChangedPackages = ChangedPackages->Array();
		Row.ChangedPackages.Sort();
	}
	OutReport.Rows.Add(MoveTemp(Row));
	FKatanaAssetMigrationRunner::Summarize(OutReport);
}
