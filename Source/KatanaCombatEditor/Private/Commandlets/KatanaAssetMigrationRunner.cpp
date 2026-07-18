// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/KatanaAssetMigrationRunner.h"

#include "Animation/AnimMontage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Commandlets/Operations/AttackDataNotifyMigrationOperation.h"
#include "Commandlets/Operations/AttackDataTimingMigrationOperation.h"
#include "Commandlets/Operations/ContentReadinessAuditOperation.h"
#include "Commandlets/Operations/CounterChainProofMigrationOperation.h"
#include "Commandlets/Operations/DefenseMatrixAuthoringOperation.h"
#include "Commandlets/Operations/DefenseProofAuthoringOperation.h"
#include "Commandlets/Operations/DefenseProofMigrationOperation.h"
#include "Commandlets/Operations/EnemyAIProofAssetsOperation.h"
#include "Data/AttackData.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectHash.h"

namespace
{
#if WITH_AUTOMATION_TESTS
	FString GForcedSaveFailurePackage;
#endif

	// PackageTools sends this notification after loading replacements; FiB must release
	// the old Blueprint path before replacement loading begins.
	void PrepareBlueprintsForPackageReload(const TArray<UPackage*>& PackagesToReload)
	{
		for (UPackage* Package : PackagesToReload)
		{
			TArray<UObject*> PackageObjects;
			GetObjectsWithPackage(Package, PackageObjects, true);
			for (UObject* Object : PackageObjects)
			{
				if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
				{
					Blueprint->ClearEditorReferences();
				}
			}
		}
	}
}

#if WITH_AUTOMATION_TESTS
void FKatanaAssetMigrationRunner::SetForcedSaveFailurePackageForTesting(
	const FString& PackageName)
{
	GForcedSaveFailurePackage = PackageName;
}
#endif

bool FKatanaAssetMigrationRunner::ParseOptions(const FString& Params, FKatanaAssetMigrationOptions& OutOptions, TArray<FString>& OutErrors)
{
	FString ModeString;
	FParse::Value(*Params, TEXT("Operation="), OutOptions.Operation);
	FParse::Value(*Params, TEXT("Mode="), ModeString);
	FParse::Value(*Params, TEXT("TargetsFile="), OutOptions.TargetsFile);
	FParse::Value(*Params, TEXT("ReportPath="), OutOptions.ReportPath);
	FParse::Value(*Params, TEXT("ApprovedPlanReport="), OutOptions.ApprovedPlanReport);
	FParse::Value(*Params, TEXT("ApprovedPlanFingerprint="), OutOptions.ApprovedPlanFingerprint);
	OutOptions.bAllowGlobalScan = FParse::Param(*Params, TEXT("AllowGlobalScan"));
	OutOptions.bAllowPackageSave = FParse::Param(*Params, TEXT("AllowPackageSave"));
	OutOptions.bAllowDirtyPackages = FParse::Param(*Params, TEXT("AllowDirtyPackages"));
	OutOptions.bAllowTimingMutation = FParse::Param(*Params, TEXT("AllowTimingMutation"));

	if (!ModeString.IsEmpty() && !TryParseKatanaAssetMigrationMode(ModeString, OutOptions.Mode))
	{
		OutErrors.Add(FString::Printf(TEXT("Unknown mode '%s'"), *ModeString));
		return false;
	}

	return ValidateOptions(OutOptions, OutErrors);
}

bool FKatanaAssetMigrationRunner::ValidateOptions(const FKatanaAssetMigrationOptions& Options, TArray<FString>& OutErrors)
{
	if (Options.Operation.IsEmpty())
	{
		OutErrors.Add(TEXT("Missing -Operation"));
	}
	else if (!Options.Operation.Equals(FAttackDataNotifyMigrationOperation::OperationName, ESearchCase::IgnoreCase) &&
		!Options.Operation.Equals(FAttackDataTimingMigrationOperation::OperationName, ESearchCase::IgnoreCase) &&
		!Options.Operation.Equals(FCounterChainProofMigrationOperation::OperationName, ESearchCase::IgnoreCase) &&
		!Options.Operation.Equals(FDefenseMatrixAuthoringOperation::OperationName, ESearchCase::IgnoreCase) &&
		!Options.Operation.Equals(FDefenseProofAuthoringOperation::OperationName, ESearchCase::IgnoreCase) &&
		!Options.Operation.Equals(FDefenseProofMigrationOperation::OperationName, ESearchCase::IgnoreCase) &&
		!Options.Operation.Equals(FContentReadinessAuditOperation::OperationName, ESearchCase::IgnoreCase) &&
		!Options.Operation.Equals(FEnemyAIProofAssetsOperation::OperationName, ESearchCase::IgnoreCase))
	{
		OutErrors.Add(FString::Printf(TEXT("Unknown operation '%s'"), *Options.Operation));
	}
	else if (Options.Operation.Equals(FAttackDataNotifyMigrationOperation::OperationName, ESearchCase::IgnoreCase) && Options.bAllowTimingMutation)
	{
		OutErrors.Add(TEXT("-AllowTimingMutation is not supported by AttackDataNotifyMigration; fix AttackData timing explicitly before notify migration"));
	}
	else if (Options.Operation.Equals(
		FAttackDataNotifyMigrationOperation::OperationName,
		ESearchCase::IgnoreCase) && Options.bAllowGlobalScan)
	{
		OutErrors.Add(TEXT(
			"AttackDataNotifyMigration requires an explicit TargetsFile; global scan is intentionally unsupported"));
	}
	else if (Options.Operation.Equals(FContentReadinessAuditOperation::OperationName, ESearchCase::IgnoreCase) &&
		(Options.Mode == EKatanaAssetMigrationMode::Apply || Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave))
	{
		OutErrors.Add(TEXT("ContentReadinessAudit is read-only and supports only Audit or Plan mode"));
	}
	else if (Options.Operation.Equals(FContentReadinessAuditOperation::OperationName, ESearchCase::IgnoreCase) && Options.bAllowGlobalScan)
	{
		OutErrors.Add(TEXT("ContentReadinessAudit requires an explicit TargetsFile; global scan is intentionally unsupported"));
	}
	else if (Options.Operation.Equals(FCounterChainProofMigrationOperation::OperationName, ESearchCase::IgnoreCase) && Options.bAllowGlobalScan)
	{
		OutErrors.Add(TEXT("CounterChainProofMigration requires an explicit TargetsFile; global scan is intentionally unsupported"));
	}
	else if (Options.Operation.Equals(FDefenseProofMigrationOperation::OperationName, ESearchCase::IgnoreCase)
		&& Options.bAllowGlobalScan)
	{
		OutErrors.Add(TEXT("DefenseProofMigration requires explicit manifest targets; global scan is intentionally unsupported"));
	}
	else if (Options.Operation.Equals(FDefenseProofAuthoringOperation::OperationName, ESearchCase::IgnoreCase)
		&& (Options.bAllowGlobalScan || !Options.TargetsFile.IsEmpty()))
	{
		OutErrors.Add(TEXT("DefenseProofAuthoring uses the fixed reviewed Gate A recipe and rejects scans or target files"));
	}
	else if (Options.Operation.Equals(FDefenseMatrixAuthoringOperation::OperationName, ESearchCase::IgnoreCase)
		&& (Options.bAllowGlobalScan || !Options.TargetsFile.IsEmpty()))
	{
		OutErrors.Add(TEXT("DefenseMatrixAuthoring uses the fixed reviewed Gate B recipe and rejects scans or target files"));
	}

	if (Options.Operation.Equals(FDefenseProofMigrationOperation::OperationName, ESearchCase::IgnoreCase)
		&& (Options.Mode == EKatanaAssetMigrationMode::Apply
			|| Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
		&& (Options.ApprovedPlanReport.IsEmpty() || Options.ApprovedPlanFingerprint.IsEmpty()))
	{
		OutErrors.Add(TEXT("DefenseProofMigration Apply modes require -ApprovedPlanReport and -ApprovedPlanFingerprint"));
	}
	if (Options.Operation.Equals(
		FAttackDataNotifyMigrationOperation::OperationName,
		ESearchCase::IgnoreCase)
		&& (Options.Mode == EKatanaAssetMigrationMode::Apply
			|| Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
		&& (Options.ApprovedPlanReport.IsEmpty()
			|| Options.ApprovedPlanFingerprint.IsEmpty()))
	{
		OutErrors.Add(TEXT(
			"AttackDataNotifyMigration Apply modes require -ApprovedPlanReport and -ApprovedPlanFingerprint"));
	}
	if (Options.Operation.Equals(FDefenseProofAuthoringOperation::OperationName, ESearchCase::IgnoreCase)
		&& (Options.Mode == EKatanaAssetMigrationMode::Apply
			|| Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
		&& (Options.ApprovedPlanReport.IsEmpty() || Options.ApprovedPlanFingerprint.IsEmpty()))
	{
		OutErrors.Add(TEXT("DefenseProofAuthoring Apply modes require -ApprovedPlanReport and -ApprovedPlanFingerprint"));
	}
	if (Options.Operation.Equals(FDefenseMatrixAuthoringOperation::OperationName, ESearchCase::IgnoreCase)
		&& (Options.Mode == EKatanaAssetMigrationMode::Apply
			|| Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
		&& (Options.ApprovedPlanReport.IsEmpty() || Options.ApprovedPlanFingerprint.IsEmpty()))
	{
		OutErrors.Add(TEXT("DefenseMatrixAuthoring Apply modes require -ApprovedPlanReport and -ApprovedPlanFingerprint"));
	}

	if (!Options.Operation.Equals(FEnemyAIProofAssetsOperation::OperationName, ESearchCase::IgnoreCase) &&
		!Options.Operation.Equals(FDefenseMatrixAuthoringOperation::OperationName, ESearchCase::IgnoreCase) &&
		!Options.Operation.Equals(FDefenseProofAuthoringOperation::OperationName, ESearchCase::IgnoreCase) &&
		!Options.bAllowGlobalScan &&
		Options.TargetsFile.IsEmpty())
	{
		OutErrors.Add(TEXT("Explicit targets are required unless -AllowGlobalScan is present"));
	}

	if (Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave && !Options.bAllowPackageSave)
	{
		OutErrors.Add(TEXT("ApplyAndSave requires -AllowPackageSave"));
	}

	return OutErrors.Num() == 0;
}

bool FKatanaAssetMigrationRunner::NormalizeAttackDataTargetObjectPath(const FString& TargetString, FString& OutObjectPath, FString& OutError)
{
	FString TrimmedTarget = TargetString;
	TrimmedTarget.TrimStartAndEndInline();
	if (TrimmedTarget.IsEmpty())
	{
		OutError = TEXT("Target path is empty");
		return false;
	}

	if (TrimmedTarget.Contains(TEXT(".")))
	{
		OutObjectPath = TrimmedTarget;
		return true;
	}

	if (!FPackageName::IsValidLongPackageName(TrimmedTarget))
	{
		OutError = FString::Printf(TEXT("Target package path is invalid: %s"), *TrimmedTarget);
		return false;
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(TrimmedTarget);
	if (AssetName.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Target package path has no asset name: %s"), *TrimmedTarget);
		return false;
	}

	OutObjectPath = FString::Printf(TEXT("%s.%s"), *TrimmedTarget, *AssetName);
	return true;
}

bool FKatanaAssetMigrationRunner::NormalizeContentTargetObjectPath(const FString& TargetString, FString& OutPackageName, FString& OutObjectPath, FString& OutError)
{
	OutPackageName.Empty();
	OutObjectPath.Empty();

	FString TrimmedTarget = TargetString;
	TrimmedTarget.TrimStartAndEndInline();
	if (TrimmedTarget.IsEmpty())
	{
		OutError = TEXT("Target path is empty");
		return false;
	}

	if (TrimmedTarget.Contains(TEXT(".")))
	{
		OutObjectPath = TrimmedTarget;
		int32 DotIndex = INDEX_NONE;
		if (!TrimmedTarget.FindChar(TCHAR('.'), DotIndex) || DotIndex <= 0)
		{
			OutError = FString::Printf(TEXT("Target object path is invalid: %s"), *TrimmedTarget);
			return false;
		}
		OutPackageName = TrimmedTarget.Left(DotIndex);
	}
	else
	{
		OutPackageName = TrimmedTarget;
	}

	if (!FPackageName::IsValidLongPackageName(OutPackageName))
	{
		OutError = FString::Printf(TEXT("Target package path is invalid: %s"), *OutPackageName);
		return false;
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(OutPackageName);
	if (AssetName.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Target package path has no asset name: %s"), *OutPackageName);
		return false;
	}

	if (OutObjectPath.IsEmpty())
	{
		OutObjectPath = FString::Printf(TEXT("%s.%s"), *OutPackageName, *AssetName);
	}

	return true;
}

FString FKatanaAssetMigrationRunner::ResolveProjectRelativeFilePath(const FString& FilePath)
{
	FString TrimmedPath = FilePath;
	TrimmedPath.TrimStartAndEndInline();
	if (TrimmedPath.IsEmpty())
	{
		return FString();
	}

	return FPaths::IsRelative(TrimmedPath)
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TrimmedPath)
		: FPaths::ConvertRelativePathToFull(TrimmedPath);
}

void FKatanaAssetMigrationRunner::Summarize(FKatanaAssetMigrationReport& Report)
{
	Report.Summary = FKatanaAssetMigrationSummary();
	Report.Summary.Targets = Report.Rows.Num();
	for (const FKatanaAssetMigrationRow& Row : Report.Rows)
	{
		switch (Row.Status)
		{
		case EKatanaAssetMigrationStatus::WouldChange:
			++Report.Summary.WouldChange;
			break;
		case EKatanaAssetMigrationStatus::Changed:
			++Report.Summary.Changed;
			break;
		case EKatanaAssetMigrationStatus::Saved:
			++Report.Summary.Saved;
			break;
		case EKatanaAssetMigrationStatus::Failed:
			++Report.Summary.Failed;
			break;
		case EKatanaAssetMigrationStatus::Unchanged:
			++Report.Summary.Unchanged;
			break;
		}
	}
}

static TArray<TSharedPtr<FJsonValue>> ToJsonArray(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const FString& Value : Values)
	{
		Result.Add(MakeShared<FJsonValueString>(Value));
	}
	return Result;
}

bool FKatanaAssetMigrationRunner::SerializeReport(
	const FKatanaAssetMigrationReport& Report,
	FString& OutJson,
	TArray<FString>& OutErrors)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema_version"), Report.SchemaVersion);
	Root->SetStringField(TEXT("operation"), Report.Operation);
	Root->SetStringField(TEXT("mode"), LexToString(Report.Mode));
	Root->SetStringField(TEXT("manifest_path"), Report.ManifestPath);
	Root->SetStringField(TEXT("gate"), Report.Gate);
	Root->SetStringField(TEXT("plan_fingerprint"), Report.PlanFingerprint);

	TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("targets"), Report.Summary.Targets);
	Summary->SetNumberField(TEXT("would_change"), Report.Summary.WouldChange);
	Summary->SetNumberField(TEXT("changed"), Report.Summary.Changed);
	Summary->SetNumberField(TEXT("unchanged"), Report.Summary.Unchanged);
	Summary->SetNumberField(TEXT("failed"), Report.Summary.Failed);
	Summary->SetNumberField(TEXT("saved"), Report.Summary.Saved);
	Root->SetObjectField(TEXT("summary"), Summary);

	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const FKatanaAssetMigrationRow& Row : Report.Rows)
	{
		TSharedRef<FJsonObject> RowObject = MakeShared<FJsonObject>();
		RowObject->SetStringField(TEXT("input_target"), Row.InputTarget);
		RowObject->SetStringField(TEXT("package_name"), Row.PackageName);
		RowObject->SetStringField(TEXT("object_path"), Row.ObjectPath);
		RowObject->SetStringField(TEXT("asset_class"), Row.AssetClass);
		RowObject->SetStringField(TEXT("attack_data"), Row.AttackData);
		RowObject->SetStringField(TEXT("montage"), Row.Montage);
		RowObject->SetStringField(TEXT("section"), Row.Section);
		RowObject->SetStringField(TEXT("counter_data"), Row.CounterData);
		RowObject->SetStringField(TEXT("counter_data_package"), Row.CounterDataPackage);
		RowObject->SetStringField(TEXT("template_paired_data"), Row.TemplatePairedData);
		RowObject->SetStringField(TEXT("status"), LexToString(Row.Status));
		RowObject->SetNumberField(TEXT("section_start"), Row.SectionStart);
		RowObject->SetNumberField(TEXT("section_end"), Row.SectionEnd);
		RowObject->SetNumberField(TEXT("section_length"), Row.SectionLength);
		RowObject->SetNumberField(TEXT("windup_duration"), Row.WindupDuration);
		RowObject->SetNumberField(TEXT("active_duration"), Row.ActiveDuration);
		RowObject->SetNumberField(TEXT("recovery_duration"), Row.RecoveryDuration);
		RowObject->SetNumberField(TEXT("timing_total"), Row.TimingTotal);
		RowObject->SetNumberField(TEXT("hold_window_start"), Row.HoldWindowStart);
		RowObject->SetNumberField(TEXT("proposed_windup_duration"), Row.ProposedWindupDuration);
		RowObject->SetNumberField(TEXT("proposed_active_duration"), Row.ProposedActiveDuration);
		RowObject->SetNumberField(TEXT("proposed_recovery_duration"), Row.ProposedRecoveryDuration);
		RowObject->SetNumberField(TEXT("proposed_timing_total"), Row.ProposedTimingTotal);
		RowObject->SetArrayField(TEXT("legacy_notifies_found"), ToJsonArray(Row.LegacyNotifiesFound));
		RowObject->SetArrayField(TEXT("stale_canonical_notifies_found"), ToJsonArray(Row.StaleCanonicalNotifiesFound));
		RowObject->SetArrayField(TEXT("canonical_notifies_missing"), ToJsonArray(Row.CanonicalNotifiesMissing));
		RowObject->SetArrayField(TEXT("branch_readiness_warnings"), ToJsonArray(Row.BranchReadinessWarnings));
		RowObject->SetArrayField(TEXT("attack_tags"), ToJsonArray(Row.AttackTags));
		RowObject->SetArrayField(TEXT("required_context_tags"), ToJsonArray(Row.RequiredContextTags));
		RowObject->SetBoolField(TEXT("package_file_exists"), Row.bPackageFileExists);
		RowObject->SetBoolField(TEXT("loaded"), Row.bLoaded);
		RowObject->SetBoolField(TEXT("map_loaded"), Row.bMapLoaded);
		RowObject->SetBoolField(TEXT("attack_data_section_valid"), Row.bAttackDataSectionValid);
		RowObject->SetBoolField(TEXT("paired_animation_valid"), Row.bPairedAnimationValid);
		RowObject->SetBoolField(TEXT("paired_attacker_section_valid"), Row.bPairedAttackerSectionValid);
		RowObject->SetBoolField(TEXT("paired_victim_section_valid"), Row.bPairedVictimSectionValid);
		RowObject->SetBoolField(TEXT("has_parry_window"), Row.bHasParryWindow);
		RowObject->SetBoolField(TEXT("has_counter_window"), Row.bHasCounterWindow);
		RowObject->SetBoolField(TEXT("counter_variant_has_data"), Row.bCounterVariantHasData);
		RowObject->SetBoolField(TEXT("finisher_has_data"), Row.bFinisherHasData);
		RowObject->SetBoolField(TEXT("has_required_context_tags"), Row.bHasRequiredContextTags);
		RowObject->SetBoolField(TEXT("has_unblockable_tag"), Row.bHasUnblockableTag);
		RowObject->SetArrayField(TEXT("planned_removals"), ToJsonArray(Row.PlannedRemovals));
		RowObject->SetArrayField(TEXT("planned_additions"), ToJsonArray(Row.PlannedAdditions));
		RowObject->SetArrayField(TEXT("changed_packages"), ToJsonArray(Row.ChangedPackages));
		RowObject->SetArrayField(TEXT("saved_packages"), ToJsonArray(Row.SavedPackages));
		RowObject->SetArrayField(TEXT("warnings"), ToJsonArray(Row.Warnings));
		RowObject->SetArrayField(TEXT("errors"), ToJsonArray(Row.Errors));
		TSharedRef<FJsonObject> Details = MakeShared<FJsonObject>();
		TArray<FString> DetailKeys;
		Row.Details.GetKeys(DetailKeys);
		DetailKeys.Sort();
		for (const FString& Key : DetailKeys)
		{
			Details->SetStringField(Key, Row.Details.FindChecked(Key));
		}
		RowObject->SetObjectField(TEXT("details"), Details);
		Rows.Add(MakeShared<FJsonValueObject>(RowObject));
	}
	Root->SetArrayField(TEXT("rows"), Rows);

	TArray<TSharedPtr<FJsonValue>> PackageLedger;
	for (const FKatanaAssetMigrationPackageLedgerEntry& Entry : Report.PackageLedger)
	{
		TSharedRef<FJsonObject> LedgerObject = MakeShared<FJsonObject>();
		LedgerObject->SetStringField(TEXT("package_name"), Entry.PackageName);
		LedgerObject->SetStringField(TEXT("package_role"), Entry.PackageRole);
		LedgerObject->SetBoolField(TEXT("initially_dirty"), Entry.bInitiallyDirty);
		LedgerObject->SetStringField(TEXT("planned_action"), Entry.PlannedAction);
		LedgerObject->SetStringField(TEXT("actual_action"), Entry.ActualAction);
		LedgerObject->SetStringField(TEXT("save_result"), Entry.SaveResult);
		LedgerObject->SetStringField(TEXT("post_save_reload_result"), Entry.PostSaveReloadResult);
		PackageLedger.Add(MakeShared<FJsonValueObject>(LedgerObject));
	}
	Root->SetArrayField(TEXT("package_ledger"), PackageLedger);

	OutJson.Reset();
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		OutErrors.Add(TEXT("Failed to serialize JSON report"));
		return false;
	}
	return true;
}

bool FKatanaAssetMigrationRunner::WriteReport(
	const FKatanaAssetMigrationReport& Report,
	const FString& ReportPath,
	TArray<FString>& OutErrors)
{
	if (ReportPath.IsEmpty())
	{
		OutErrors.Add(TEXT("Report path is empty"));
		return false;
	}

	FString JsonText;
	if (!SerializeReport(Report, JsonText, OutErrors))
	{
		return false;
	}

	const FString ResolvedReportPath = ResolveProjectRelativeFilePath(ReportPath);
	const FString ReportDirectory = FPaths::GetPath(ResolvedReportPath);
	if (!ReportDirectory.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*ReportDirectory, true);
	}

	if (!FFileHelper::SaveStringToFile(JsonText, *ResolvedReportPath))
	{
		OutErrors.Add(FString::Printf(TEXT("Failed to write report '%s'"), *ResolvedReportPath));
		return false;
	}

	return true;
}

bool FKatanaAssetMigrationRunner::RequiresFreshProcessAudit(
	const FKatanaAssetMigrationReport& Report)
{
	return Report.PackageLedger.ContainsByPredicate(
		[](const FKatanaAssetMigrationPackageLedgerEntry& Entry)
		{
			return Entry.PostSaveReloadResult
				== TEXT("FreshProcessAuditRequired");
		});
}

void FKatanaAssetMigrationRunner::AnnotateFreshProcessAuditRequirement(
	FKatanaAssetMigrationReport& Report)
{
	if (!RequiresFreshProcessAudit(Report) || Report.Rows.IsEmpty())
	{
		return;
	}
	Report.Rows[0].Warnings.AddUnique(
		TEXT("Saved map persistence is not verified in-process; run Audit in a new commandlet process"));
	Report.Rows[0].Details.Add(
		TEXT("post_save_persistence_verification"),
		TEXT("FreshProcessAuditRequired"));
}

bool FKatanaAssetMigrationRunner::LoadTargetStrings(const FKatanaAssetMigrationOptions& Options, TArray<FString>& OutTargetStrings, TArray<FKatanaAssetMigrationRow>& OutFailedRows) const
{
	if (Options.TargetsFile.IsEmpty())
	{
		FKatanaAssetMigrationRow Row;
		Row.Status = EKatanaAssetMigrationStatus::Failed;
		Row.Errors.Add(TEXT("TargetsFile is required"));
		OutFailedRows.Add(Row);
		return false;
	}

	const FString ResolvedTargetsFile = ResolveProjectRelativeFilePath(Options.TargetsFile);
	if (!FFileHelper::LoadFileToStringArray(OutTargetStrings, *ResolvedTargetsFile))
	{
		FKatanaAssetMigrationRow Row;
		Row.Status = EKatanaAssetMigrationStatus::Failed;
		Row.Errors.Add(FString::Printf(TEXT("Failed to read TargetsFile '%s'"), *ResolvedTargetsFile));
		OutFailedRows.Add(Row);
		return false;
	}

	return true;
}

bool FKatanaAssetMigrationRunner::LoadTargets(const FKatanaAssetMigrationOptions& Options, TArray<UAttackData*>& OutTargets, TArray<FKatanaAssetMigrationRow>& OutFailedRows) const
{
	TArray<FString> TargetStrings;
	if (!Options.TargetsFile.IsEmpty())
	{
		const FString ResolvedTargetsFile = ResolveProjectRelativeFilePath(Options.TargetsFile);
		if (!FFileHelper::LoadFileToStringArray(TargetStrings, *ResolvedTargetsFile))
		{
			FKatanaAssetMigrationRow Row;
			Row.Status = EKatanaAssetMigrationStatus::Failed;
			Row.Errors.Add(FString::Printf(TEXT("Failed to read TargetsFile '%s'"), *ResolvedTargetsFile));
			OutFailedRows.Add(Row);
			return false;
		}
	}

	if (Options.bAllowGlobalScan)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().SearchAllAssets(true);

		TArray<FAssetData> AssetDataList;
		AssetRegistryModule.Get().GetAssetsByClass(UAttackData::StaticClass()->GetClassPathName(), AssetDataList, true);
		for (const FAssetData& AssetData : AssetDataList)
		{
			if (UAttackData* AttackData = Cast<UAttackData>(AssetData.GetAsset()))
			{
				OutTargets.AddUnique(AttackData);
			}
		}
	}

	for (FString TargetString : TargetStrings)
	{
		TargetString.TrimStartAndEndInline();
		if (TargetString.IsEmpty() || TargetString.StartsWith(TEXT("#")))
		{
			continue;
		}

		FString ObjectPath;
		FString Error;
		if (!NormalizeAttackDataTargetObjectPath(TargetString, ObjectPath, Error))
		{
			FKatanaAssetMigrationRow Row;
			Row.InputTarget = TargetString;
			Row.Status = EKatanaAssetMigrationStatus::Failed;
			Row.Errors.Add(Error);
			OutFailedRows.Add(Row);
			continue;
		}

		UObject* Object = StaticLoadObject(UAttackData::StaticClass(), nullptr, *ObjectPath);
		UAttackData* AttackData = Cast<UAttackData>(Object);
		if (!AttackData)
		{
			FKatanaAssetMigrationRow Row;
			Row.InputTarget = TargetString;
			Row.Status = EKatanaAssetMigrationStatus::Failed;
			Row.Errors.Add(FString::Printf(TEXT("Target did not load as UAttackData: %s"), *ObjectPath));
			OutFailedRows.Add(Row);
			continue;
		}

		OutTargets.AddUnique(AttackData);
	}

	return OutFailedRows.Num() == 0;
}

bool FKatanaAssetMigrationRunner::RunAttackDataNotifyMigration(const FKatanaAssetMigrationOptions& Options, const TArray<UAttackData*>& Targets, FKatanaAssetMigrationReport& OutReport) const
{
	FAttackDataNotifyMigrationOperation Operation;
	if (Options.Mode == EKatanaAssetMigrationMode::Audit)
	{
		OutReport = FKatanaAssetMigrationReport();
		OutReport.Operation = FAttackDataNotifyMigrationOperation::OperationName;
		OutReport.Mode = Options.Mode;
		for (UAttackData* Target : Targets)
		{
			FKatanaAssetMigrationRow Row;
			Operation.Run(Target, Options.Mode, Row);
			OutReport.Rows.Add(MoveTemp(Row));
		}
		Summarize(OutReport);
		return OutReport.Summary.Failed == 0;
	}

	FKatanaAssetMigrationReport CurrentPlanReport;
	TArray<FString> PlanErrors;
	if (!FAttackDataNotifyMigrationOperation::BuildPlanReport(
		Options, Targets, CurrentPlanReport, PlanErrors))
	{
		OutReport = MoveTemp(CurrentPlanReport);
		OutReport.Mode = Options.Mode;
		Summarize(OutReport);
		return false;
	}
	if (Options.Mode == EKatanaAssetMigrationMode::Plan)
	{
		OutReport = MoveTemp(CurrentPlanReport);
		return true;
	}

	TArray<FString> ApprovalErrors;
	if (!FAttackDataNotifyMigrationOperation::ValidateApprovedPlanBinding(
		Options, CurrentPlanReport, ApprovalErrors))
	{
		OutReport = MoveTemp(CurrentPlanReport);
		OutReport.Mode = Options.Mode;
		FKatanaAssetMigrationRow FailureRow;
		FailureRow.InputTarget = OutReport.ManifestPath;
		FailureRow.AssetClass = TEXT("ApprovalContract");
		FailureRow.Status = EKatanaAssetMigrationStatus::Failed;
		FailureRow.Errors = MoveTemp(ApprovalErrors);
		OutReport.Rows.Add(MoveTemp(FailureRow));
		Summarize(OutReport);
		return false;
	}

	struct FNotifyBatchSnapshot
	{
		TArray<FAnimNotifyEvent> Notifies;
		bool bPackageWasDirty = false;
	};
	TMap<UAnimMontage*, FNotifyBatchSnapshot> Snapshots;
	for (UAttackData* Target : Targets)
	{
		if (!Target || !Target->AttackMontage
			|| Snapshots.Contains(Target->AttackMontage))
		{
			continue;
		}
		FNotifyBatchSnapshot Snapshot;
		Snapshot.Notifies = Target->AttackMontage->Notifies;
		Snapshot.bPackageWasDirty =
			Target->AttackMontage->GetOutermost()->IsDirty();
		Snapshots.Add(Target->AttackMontage, MoveTemp(Snapshot));
	}

	OutReport = CurrentPlanReport;
	OutReport.Mode = Options.Mode;
	OutReport.Rows.Reset();
	TArray<UAttackData*> SortedTargets = Targets;
	SortedTargets.Sort([](const UAttackData& Left, const UAttackData& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});
	bool bApplyFailed = false;
	for (UAttackData* Target : SortedTargets)
	{
		FKatanaAssetMigrationRow Row;
		const bool bApplied = Operation.Run(Target, Options.Mode, Row);
		if (const FKatanaAssetMigrationRow* PlanRow =
			CurrentPlanReport.Rows.FindByPredicate(
				[&Row](const FKatanaAssetMigrationRow& Candidate)
				{
					return Candidate.AttackData == Row.AttackData;
				}))
		{
			for (const TPair<FString, FString>& Detail : PlanRow->Details)
			{
				if (Detail.Key.StartsWith(TEXT("approval_")))
				{
					Row.Details.Add(Detail.Key, Detail.Value);
				}
			}
		}
		OutReport.Rows.Add(MoveTemp(Row));
		bApplyFailed |= !bApplied;
		if (bApplyFailed)
		{
			break;
		}
	}

	if (bApplyFailed)
	{
		for (const TPair<UAnimMontage*, FNotifyBatchSnapshot>& Pair : Snapshots)
		{
			Pair.Key->Notifies = Pair.Value.Notifies;
			Pair.Key->RefreshCacheData();
			Pair.Key->GetOutermost()->SetDirtyFlag(Pair.Value.bPackageWasDirty);
		}
		for (FKatanaAssetMigrationRow& Row : OutReport.Rows)
		{
			if (Row.Status == EKatanaAssetMigrationStatus::Changed)
			{
				Row.Status = EKatanaAssetMigrationStatus::Failed;
				Row.Errors.Add(TEXT(
					"notify-migration batch was rolled back after a later target failed"));
			}
		}
		Summarize(OutReport);
		return false;
	}

	for (FKatanaAssetMigrationPackageLedgerEntry& Entry : OutReport.PackageLedger)
	{
		if (Entry.PlannedAction == TEXT("Update"))
		{
			Entry.ActualAction = TEXT("UpdatedInMemory");
		}
	}
	Summarize(OutReport);
	return OutReport.Summary.Failed == 0;
}

bool FKatanaAssetMigrationRunner::RunAttackDataTimingMigration(const FKatanaAssetMigrationOptions& Options, const TArray<UAttackData*>& Targets, FKatanaAssetMigrationReport& OutReport) const
{
	FAttackDataTimingMigrationOperation Operation;
	OutReport.Operation = FAttackDataTimingMigrationOperation::OperationName;
	OutReport.Mode = Options.Mode;

	for (UAttackData* Target : Targets)
	{
		FKatanaAssetMigrationRow Row;
		Operation.Run(Target, Options.Mode, Row);
		OutReport.Rows.Add(Row);
	}

	Summarize(OutReport);
	return OutReport.Summary.Failed == 0;
}

bool FKatanaAssetMigrationRunner::RunContentReadinessAudit(const FKatanaAssetMigrationOptions& Options, FKatanaAssetMigrationReport& OutReport) const
{
	FContentReadinessAuditOperation Operation;
	OutReport.Operation = FContentReadinessAuditOperation::OperationName;
	OutReport.Mode = Options.Mode;

	TArray<FString> TargetStrings;
	TArray<FKatanaAssetMigrationRow> FailedRows;
	LoadTargetStrings(Options, TargetStrings, FailedRows);
	OutReport.Rows.Append(FailedRows);

	for (FString TargetString : TargetStrings)
	{
		TargetString.TrimStartAndEndInline();
		if (TargetString.IsEmpty() || TargetString.StartsWith(TEXT("#")))
		{
			continue;
		}

		FKatanaAssetMigrationRow Row;
		Operation.Run(TargetString, Options.Mode, Row);
		OutReport.Rows.Add(Row);
	}

	Summarize(OutReport);
	return OutReport.Summary.Failed == 0;
}

bool FKatanaAssetMigrationRunner::RunCounterChainProofMigration(const FKatanaAssetMigrationOptions& Options, FKatanaAssetMigrationReport& OutReport) const
{
	FCounterChainProofMigrationOperation Operation;
	OutReport.Operation = FCounterChainProofMigrationOperation::OperationName;
	OutReport.Mode = Options.Mode;

	TArray<FString> TargetStrings;
	TArray<FKatanaAssetMigrationRow> FailedRows;
	LoadTargetStrings(Options, TargetStrings, FailedRows);
	OutReport.Rows.Append(FailedRows);

	for (FString TargetString : TargetStrings)
	{
		TargetString.TrimStartAndEndInline();
		if (TargetString.IsEmpty() || TargetString.StartsWith(TEXT("#")))
		{
			continue;
		}

		FKatanaAssetMigrationRow Row;
		Operation.Run(TargetString, Options.Mode, Row);
		OutReport.Rows.Add(Row);
	}

	Summarize(OutReport);
	return OutReport.Summary.Failed == 0;
}

bool FKatanaAssetMigrationRunner::RunDefenseProofMigration(
	const FKatanaAssetMigrationOptions& Options,
	FKatanaAssetMigrationReport& OutReport) const
{
	TArray<FString> TargetStrings;
	TArray<FKatanaAssetMigrationRow> FailedRows;
	LoadTargetStrings(Options, TargetStrings, FailedRows);
	TArray<FString> ManifestTargets;
	for (FString Target : TargetStrings)
	{
		Target.TrimStartAndEndInline();
		if (!Target.IsEmpty() && !Target.StartsWith(TEXT("#")))
		{
			ManifestTargets.Add(Target);
		}
	}
	if (!FailedRows.IsEmpty() || ManifestTargets.Num() != 1)
	{
		OutReport = FKatanaAssetMigrationReport();
		OutReport.SchemaVersion = 2;
		OutReport.Operation = FDefenseProofMigrationOperation::OperationName;
		OutReport.Mode = Options.Mode;
		OutReport.Rows = MoveTemp(FailedRows);
		if (ManifestTargets.Num() != 1)
		{
			FKatanaAssetMigrationRow Row;
			Row.Status = EKatanaAssetMigrationStatus::Failed;
			Row.Errors.Add(TEXT("DefenseProofMigration TargetsFile must contain exactly one manifest path"));
			OutReport.Rows.Add(MoveTemp(Row));
		}
		Summarize(OutReport);
		return false;
	}

	return FDefenseProofMigrationOperation().Run(
		ManifestTargets[0], Options, OutReport);
}

bool FKatanaAssetMigrationRunner::RunEnemyAIProofAssets(const FKatanaAssetMigrationOptions& Options, FKatanaAssetMigrationReport& OutReport) const
{
	FEnemyAIProofAssetsOperation Operation;
	OutReport.Operation = FEnemyAIProofAssetsOperation::OperationName;
	OutReport.Mode = Options.Mode;

	FKatanaAssetMigrationRow Row;
	Operation.Run(Options.Mode, Row);
	OutReport.Rows.Add(Row);

	Summarize(OutReport);
	return OutReport.Summary.Failed == 0;
}

bool FKatanaAssetMigrationRunner::RunDefenseProofAuthoring(
	const FKatanaAssetMigrationOptions& Options,
	FKatanaAssetMigrationReport& OutReport) const
{
	return FDefenseProofAuthoringOperation().Run(Options, OutReport);
}

bool FKatanaAssetMigrationRunner::RunDefenseMatrixAuthoring(
	const FKatanaAssetMigrationOptions& Options,
	FKatanaAssetMigrationReport& OutReport) const
{
	return FDefenseMatrixAuthoringOperation().Run(Options, OutReport);
}

static void SnapshotInitiallyDirtyPackages(const TArray<UAttackData*>& Targets, TSet<FString>& OutDirtyPackages)
{
	for (const UAttackData* Target : Targets)
	{
		if (!Target)
		{
			continue;
		}

		if (const UPackage* AttackDataPackage = Target->GetOutermost())
		{
			if (AttackDataPackage->IsDirty())
			{
				OutDirtyPackages.Add(AttackDataPackage->GetName());
			}
		}

		if (Target->AttackMontage)
		{
			if (const UPackage* MontagePackage = Target->AttackMontage->GetOutermost())
			{
				if (MontagePackage->IsDirty())
				{
					OutDirtyPackages.Add(MontagePackage->GetName());
				}
			}
		}
	}
}

static bool IsLoadedMapPackage(UPackage* Package)
{
	if (!Package)
	{
		return false;
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(Package->GetName());
	return FindObject<UWorld>(Package, *AssetName) != nullptr;
}

bool FKatanaAssetMigrationRunner::SaveChangedPackages(const FKatanaAssetMigrationOptions& Options, const TSet<FString>& InitiallyDirtyPackages, FKatanaAssetMigrationReport& Report) const
{
	if (Options.Mode != EKatanaAssetMigrationMode::ApplyAndSave)
	{
		return true;
	}

	if (!Options.bAllowPackageSave)
	{
		return false;
	}

	TSet<FString> PreflightedPackages;
	TMap<FString, UPackage*> PackagesByName;
	TMap<FString, FString> FilenamesByPackage;
	TSet<FString> MapPackages;
	for (FKatanaAssetMigrationRow& Row : Report.Rows)
	{
		if (Row.Status != EKatanaAssetMigrationStatus::Changed)
		{
			continue;
		}
		for (const FString& PackageName : Row.ChangedPackages)
		{
			if (PreflightedPackages.Contains(PackageName))
			{
				continue;
			}
			PreflightedPackages.Add(PackageName);
			FKatanaAssetMigrationPackageLedgerEntry* LedgerEntry =
				Report.PackageLedger.FindByPredicate(
					[&PackageName](const FKatanaAssetMigrationPackageLedgerEntry& Entry)
					{
						return Entry.PackageName == PackageName;
					});
			UPackage* Package = FindPackage(nullptr, *PackageName);
			if (!Package)
			{
				Row.Errors.Add(FString::Printf(
					TEXT("Changed package was not loaded: %s"), *PackageName));
				Row.Status = EKatanaAssetMigrationStatus::Failed;
				if (LedgerEntry)
				{
					LedgerEntry->SaveResult = TEXT("PackageNotLoaded");
				}
				continue;
			}
			if (InitiallyDirtyPackages.Contains(PackageName) && !Options.bAllowDirtyPackages)
			{
				Row.Errors.Add(FString::Printf(
					TEXT("Package was dirty before migration and was refused without -AllowDirtyPackages: %s"),
					*PackageName));
				Row.Status = EKatanaAssetMigrationStatus::Failed;
				if (LedgerEntry)
				{
					LedgerEntry->SaveResult = TEXT("RefusedInitiallyDirty");
				}
				continue;
			}
			UObject* PackageAsset = Package->FindAssetInPackage();
			if (!PackageAsset || !PackageAsset->HasAnyFlags(RF_Standalone))
			{
				Row.Errors.Add(FString::Printf(
					TEXT("Package has no saveable standalone top-level asset: %s"),
					*PackageName));
				Row.Status = EKatanaAssetMigrationStatus::Failed;
				if (LedgerEntry)
				{
					LedgerEntry->SaveResult = TEXT("UnsaveableTopLevel");
				}
				continue;
			}

			FString PackageFilename;
			const bool bIsMapPackage = IsLoadedMapPackage(Package);
			const FString PackageExtension = bIsMapPackage
				? FPackageName::GetMapPackageExtension()
				: FPackageName::GetAssetPackageExtension();
			if (!FPackageName::TryConvertLongPackageNameToFilename(
				PackageName, PackageFilename, PackageExtension))
			{
				Row.Errors.Add(FString::Printf(
					TEXT("Failed to resolve package filename: %s"), *PackageName));
				Row.Status = EKatanaAssetMigrationStatus::Failed;
				if (LedgerEntry)
				{
					LedgerEntry->SaveResult = TEXT("FilenameResolutionFailed");
				}
				continue;
			}
			PackagesByName.Add(PackageName, Package);
			FilenamesByPackage.Add(PackageName, MoveTemp(PackageFilename));
			if (bIsMapPackage)
			{
				MapPackages.Add(PackageName);
			}
		}
	}
	Summarize(Report);
	if (Report.Summary.Failed > 0)
	{
		return false;
	}

	struct FPackageFileBackup
	{
		FString PackageFilename;
		FString BackupFilename;
		bool bFileExisted = false;
	};
	TMap<FString, FPackageFileBackup> FileBackups;
	for (const TPair<FString, FString>& Pair : FilenamesByPackage)
	{
		FPackageFileBackup Backup;
		Backup.PackageFilename = Pair.Value;
		Backup.bFileExisted = IFileManager::Get().FileExists(*Pair.Value);
		if (Backup.bFileExisted)
		{
			Backup.BackupFilename = FPaths::CreateTempFilename(
				*FPaths::ProjectSavedDir(), TEXT("KatanaMigrationBackup"), TEXT(".tmp"));
			if (IFileManager::Get().Copy(
				*Backup.BackupFilename, *Pair.Value, true, true) != COPY_OK)
			{
				if (!Report.Rows.IsEmpty())
				{
					Report.Rows[0].Errors.Add(FString::Printf(
						TEXT("Failed to create rollback backup for package: %s"), *Pair.Key));
					Report.Rows[0].Status = EKatanaAssetMigrationStatus::Failed;
				}
				for (const TPair<FString, FPackageFileBackup>& Existing : FileBackups)
				{
					if (!Existing.Value.BackupFilename.IsEmpty())
					{
						IFileManager::Get().Delete(
							*Existing.Value.BackupFilename, false, true, true);
					}
				}
				Summarize(Report);
				return false;
			}
		}
		FileBackups.Add(Pair.Key, MoveTemp(Backup));
	}
	const auto CleanupBackups = [&FileBackups]()
	{
		for (const TPair<FString, FPackageFileBackup>& Pair : FileBackups)
		{
			if (!Pair.Value.BackupFilename.IsEmpty())
			{
				IFileManager::Get().Delete(
					*Pair.Value.BackupFilename, false, true, true);
			}
		}
	};

	TSet<FString> SavedThisRun;
	TSet<FString> ReloadedPackageNames;
	TArray<UPackage*> PackagesToReload;
	bool bSaveFailed = false;
	for (FKatanaAssetMigrationRow& Row : Report.Rows)
	{
		if (bSaveFailed)
		{
			break;
		}
		if (Row.Status != EKatanaAssetMigrationStatus::Changed)
		{
			continue;
		}

		for (const FString& PackageName : Row.ChangedPackages)
		{
			if (SavedThisRun.Contains(PackageName))
			{
				Row.SavedPackages.AddUnique(PackageName);
				continue;
			}
			FKatanaAssetMigrationPackageLedgerEntry* LedgerEntry =
				Report.PackageLedger.FindByPredicate(
					[&PackageName](const FKatanaAssetMigrationPackageLedgerEntry& Entry)
					{
						return Entry.PackageName == PackageName;
					});
			UPackage* Package = PackagesByName.FindChecked(PackageName);
			const FString& PackageFileName = FilenamesByPackage.FindChecked(PackageName);

			const FString PackageDirectory = FPaths::GetPath(PackageFileName);
			if (!PackageDirectory.IsEmpty())
			{
				IFileManager::Get().MakeDirectory(*PackageDirectory, true);
			}

			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			UObject* PackageAsset = Package->FindAssetInPackage();
			const bool bForcedSaveFailure =
#if WITH_AUTOMATION_TESTS
				GForcedSaveFailurePackage == PackageName;
#else
				false;
#endif
			const bool bSaved = !bForcedSaveFailure && (MapPackages.Contains(PackageName)
				? GEditor && GEditor->SavePackage(
					Package, PackageAsset, *PackageFileName, SaveArgs)
				: UPackage::SavePackage(
					Package, PackageAsset, *PackageFileName, SaveArgs));
			if (!bSaved)
			{
				Row.Errors.Add(FString::Printf(TEXT("Failed to save package: %s"), *PackageName));
				Row.Status = EKatanaAssetMigrationStatus::Failed;
				if (LedgerEntry)
				{
					LedgerEntry->SaveResult = TEXT("SaveFailed");
				}
				bSaveFailed = true;
				break;
			}

			Row.SavedPackages.AddUnique(PackageName);
			SavedThisRun.Add(PackageName);
			if (LedgerEntry)
			{
				LedgerEntry->SaveResult = TEXT("Saved");
			}
			if (MapPackages.Contains(PackageName))
			{
				if (LedgerEntry)
				{
					LedgerEntry->PostSaveReloadResult = TEXT("FreshProcessAuditRequired");
				}
			}
			else
			{
				PackagesToReload.AddUnique(Package);
				ReloadedPackageNames.Add(PackageName);
			}
		}

		if (Row.Errors.Num() == 0 && Row.SavedPackages.Num() > 0)
		{
			Row.Status = EKatanaAssetMigrationStatus::Saved;
		}
	}

	Summarize(Report);
	if (bSaveFailed)
	{
		bool bRollbackSucceeded = true;
		for (const FString& PackageName : SavedThisRun)
		{
			const FPackageFileBackup* Backup = FileBackups.Find(PackageName);
			if (!Backup)
			{
				bRollbackSucceeded = false;
				continue;
			}
			const bool bRestored = Backup->bFileExisted
				? IFileManager::Get().Copy(
					*Backup->PackageFilename, *Backup->BackupFilename, true, true) == COPY_OK
				: IFileManager::Get().Delete(
					*Backup->PackageFilename, false, true, true);
			bRollbackSucceeded &= bRestored;
			if (UPackage** Package = PackagesByName.Find(PackageName))
			{
				(*Package)->SetDirtyFlag(true);
			}
			if (FKatanaAssetMigrationPackageLedgerEntry* Entry =
				Report.PackageLedger.FindByPredicate(
					[&PackageName](const FKatanaAssetMigrationPackageLedgerEntry& Candidate)
					{
						return Candidate.PackageName == PackageName;
					}))
			{
				Entry->SaveResult = bRestored ? TEXT("RolledBack") : TEXT("RollbackFailed");
				Entry->PostSaveReloadResult = TEXT("RolledBackAfterSaveFailure");
			}
		}
		for (FKatanaAssetMigrationRow& Row : Report.Rows)
		{
			if (Row.SavedPackages.ContainsByPredicate(
				[&SavedThisRun](const FString& PackageName)
				{
					return SavedThisRun.Contains(PackageName);
				}))
			{
				Row.SavedPackages.RemoveAll(
					[&SavedThisRun](const FString& PackageName)
					{
						return SavedThisRun.Contains(PackageName);
					});
				Row.Errors.Add(bRollbackSucceeded
					? TEXT("Package-save batch was rolled back after a later save failure")
					: TEXT("Package-save batch rollback failed; inspect package files before retrying"));
				Row.Status = EKatanaAssetMigrationStatus::Failed;
			}
		}
		for (FKatanaAssetMigrationPackageLedgerEntry& Entry : Report.PackageLedger)
		{
			if (ReloadedPackageNames.Contains(Entry.PackageName))
			{
				Entry.PostSaveReloadResult = TEXT("SkippedAfterSaveFailure");
			}
		}
		CleanupBackups();
		Summarize(Report);
		return false;
	}
	if (Report.Summary.Failed == 0 && !PackagesToReload.IsEmpty())
	{
		PrepareBlueprintsForPackageReload(PackagesToReload);
		FText ReloadError;
		const bool bReloaded = UPackageTools::ReloadPackages(
			PackagesToReload, ReloadError, EReloadPackagesInteractionMode::AssumePositive);
		for (FKatanaAssetMigrationPackageLedgerEntry& Entry : Report.PackageLedger)
		{
			if (ReloadedPackageNames.Contains(Entry.PackageName))
			{
				Entry.PostSaveReloadResult = bReloaded ? TEXT("Reloaded") : TEXT("ReloadFailed");
			}
		}
		if (!bReloaded)
		{
			FKatanaAssetMigrationRow* FailureRow = Report.Rows.FindByPredicate(
				[](const FKatanaAssetMigrationRow& Row)
				{
					return !Row.SavedPackages.IsEmpty();
				});
			if (FailureRow)
			{
				FailureRow->Errors.Add(FString::Printf(
					TEXT("Post-save package reload failed: %s"), *ReloadError.ToString()));
				FailureRow->Status = EKatanaAssetMigrationStatus::Failed;
			}
			Summarize(Report);
			bool bRollbackSucceeded = true;
			for (const FString& PackageName : SavedThisRun)
			{
				const FPackageFileBackup* Backup = FileBackups.Find(PackageName);
				if (!Backup)
				{
					bRollbackSucceeded = false;
					continue;
				}
				const bool bRestored = Backup->bFileExisted
					? IFileManager::Get().Copy(
						*Backup->PackageFilename, *Backup->BackupFilename, true, true) == COPY_OK
					: IFileManager::Get().Delete(
						*Backup->PackageFilename, false, true, true);
				bRollbackSucceeded &= bRestored;
				if (UPackage** Package = PackagesByName.Find(PackageName))
				{
					(*Package)->SetDirtyFlag(true);
				}
				if (FKatanaAssetMigrationPackageLedgerEntry* Entry =
					Report.PackageLedger.FindByPredicate(
						[&PackageName](const FKatanaAssetMigrationPackageLedgerEntry& Candidate)
						{
							return Candidate.PackageName == PackageName;
						}))
				{
					Entry->SaveResult = bRestored ? TEXT("RolledBack") : TEXT("RollbackFailed");
					Entry->PostSaveReloadResult = TEXT("RolledBackAfterReloadFailure");
				}
			}
			for (FKatanaAssetMigrationRow& Row : Report.Rows)
			{
				if (Row.SavedPackages.ContainsByPredicate(
					[&SavedThisRun](const FString& PackageName)
					{
						return SavedThisRun.Contains(PackageName);
					}))
				{
					Row.SavedPackages.RemoveAll(
						[&SavedThisRun](const FString& PackageName)
						{
							return SavedThisRun.Contains(PackageName);
						});
					Row.Errors.Add(bRollbackSucceeded
						? TEXT("Package-save batch was rolled back after reload failure")
						: TEXT("Package-save batch rollback failed after reload failure"));
					Row.Status = EKatanaAssetMigrationStatus::Failed;
				}
			}
			if (!bRollbackSucceeded && FailureRow)
			{
				FailureRow->Errors.Add(
					TEXT("Disk rollback failed after package reload failure"));
			}
			CleanupBackups();
			Summarize(Report);
			return false;
		}
	}
	CleanupBackups();
	return Report.Summary.Failed == 0;
}

EKatanaAssetMigrationExitCode FKatanaAssetMigrationRunner::Run(const FKatanaAssetMigrationOptions& Options)
{
	TArray<FString> Errors;
	if (!ValidateOptions(Options, Errors))
	{
		return EKatanaAssetMigrationExitCode::InvalidArguments;
	}

	TArray<UAttackData*> Targets;
	TArray<FKatanaAssetMigrationRow> FailedRows;
	if (Options.Operation.Equals(FContentReadinessAuditOperation::OperationName, ESearchCase::IgnoreCase))
	{
		FKatanaAssetMigrationReport Report;
		RunContentReadinessAudit(Options, Report);

		bool bReportFailed = false;
		if (!Options.ReportPath.IsEmpty())
		{
			TArray<FString> ReportErrors;
			if (!WriteReport(Report, Options.ReportPath, ReportErrors))
			{
				bReportFailed = true;
				for (const FString& ReportError : ReportErrors)
				{
					UE_LOG(LogTemp, Error, TEXT("%s"), *ReportError);
				}
			}
		}

		return (Report.Summary.Failed > 0 || bReportFailed)
			? EKatanaAssetMigrationExitCode::RowFailure
			: EKatanaAssetMigrationExitCode::Success;
	}
	if (Options.Operation.Equals(FCounterChainProofMigrationOperation::OperationName, ESearchCase::IgnoreCase))
	{
		TArray<FString> TargetStrings;
		LoadTargetStrings(Options, TargetStrings, FailedRows);

		TSet<FString> InitiallyDirtyPackages;
		FCounterChainProofMigrationOperation::SnapshotInitiallyDirtyPackages(TargetStrings, InitiallyDirtyPackages);

		FKatanaAssetMigrationReport Report;
		RunCounterChainProofMigration(Options, Report);

		bool bSaveFailed = false;
		if (Report.Summary.Failed == 0 && Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
		{
			bSaveFailed = !SaveChangedPackages(Options, InitiallyDirtyPackages, Report);
		}

		bool bReportFailed = false;
		if (!Options.ReportPath.IsEmpty())
		{
			TArray<FString> ReportErrors;
			if (!WriteReport(Report, Options.ReportPath, ReportErrors))
			{
				bReportFailed = true;
				for (const FString& ReportError : ReportErrors)
				{
					UE_LOG(LogTemp, Error, TEXT("%s"), *ReportError);
				}
			}
		}

		if (bSaveFailed)
		{
			return EKatanaAssetMigrationExitCode::SaveFailure;
		}

		return (Report.Summary.Failed > 0 || bReportFailed)
			? EKatanaAssetMigrationExitCode::RowFailure
			: EKatanaAssetMigrationExitCode::Success;
	}
	if (Options.Operation.Equals(FDefenseProofMigrationOperation::OperationName, ESearchCase::IgnoreCase))
	{
		FKatanaAssetMigrationReport Report;
		const bool bOperationSucceeded = RunDefenseProofMigration(Options, Report);
		TSet<FString> InitiallyDirtyPackages;
		for (const FKatanaAssetMigrationPackageLedgerEntry& Entry : Report.PackageLedger)
		{
			if (Entry.bInitiallyDirty)
			{
				InitiallyDirtyPackages.Add(Entry.PackageName);
			}
		}

		bool bSaveFailed = false;
		if (bOperationSucceeded && Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
		{
			bSaveFailed = !SaveChangedPackages(Options, InitiallyDirtyPackages, Report);
		}
		if (!bSaveFailed && bOperationSucceeded
			&& Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
		{
			if (RequiresFreshProcessAudit(Report))
			{
				AnnotateFreshProcessAuditRequirement(Report);
			}
			else
			{
				FKatanaAssetMigrationOptions AuditOptions = Options;
				AuditOptions.Mode = EKatanaAssetMigrationMode::Audit;
				AuditOptions.ApprovedPlanReport.Reset();
				AuditOptions.ApprovedPlanFingerprint.Reset();
				FKatanaAssetMigrationReport PostSaveAudit;
				const bool bAuditSucceeded =
					RunDefenseProofMigration(AuditOptions, PostSaveAudit);
				if (!bAuditSucceeded || PostSaveAudit.Summary.Failed > 0
					|| PostSaveAudit.Summary.WouldChange > 0)
				{
					bSaveFailed = true;
					if (!Report.Rows.IsEmpty())
					{
						Report.Rows[0].Errors.Add(
							TEXT("Post-save Audit did not return Unchanged"));
						Report.Rows[0].Status = EKatanaAssetMigrationStatus::Failed;
						Summarize(Report);
					}
				}
			}
		}

		bool bReportFailed = false;
		if (!Options.ReportPath.IsEmpty())
		{
			TArray<FString> ReportErrors;
			if (!WriteReport(Report, Options.ReportPath, ReportErrors))
			{
				bReportFailed = true;
				for (const FString& ReportError : ReportErrors)
				{
					UE_LOG(LogTemp, Error, TEXT("%s"), *ReportError);
				}
			}
		}
		if (bSaveFailed)
		{
			return EKatanaAssetMigrationExitCode::SaveFailure;
		}
		return (!bOperationSucceeded || Report.Summary.Failed > 0 || bReportFailed)
			? EKatanaAssetMigrationExitCode::RowFailure
			: EKatanaAssetMigrationExitCode::Success;
	}
	if (Options.Operation.Equals(FDefenseMatrixAuthoringOperation::OperationName, ESearchCase::IgnoreCase))
	{
		FKatanaAssetMigrationReport Report;
		const bool bOperationSucceeded = RunDefenseMatrixAuthoring(Options, Report);
		TSet<FString> InitiallyDirtyPackages;
		for (const FKatanaAssetMigrationPackageLedgerEntry& Entry : Report.PackageLedger)
		{
			if (Entry.bInitiallyDirty)
			{
				InitiallyDirtyPackages.Add(Entry.PackageName);
			}
		}

		bool bSaveFailed = false;
		if (bOperationSucceeded && Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
		{
			bSaveFailed = !SaveChangedPackages(Options, InitiallyDirtyPackages, Report);
		}
		if (!bSaveFailed && bOperationSucceeded
			&& Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
		{
			if (RequiresFreshProcessAudit(Report))
			{
				AnnotateFreshProcessAuditRequirement(Report);
			}
			else
			{
				FKatanaAssetMigrationOptions AuditOptions = Options;
				AuditOptions.Mode = EKatanaAssetMigrationMode::Audit;
				AuditOptions.ApprovedPlanReport.Reset();
				AuditOptions.ApprovedPlanFingerprint.Reset();
				FKatanaAssetMigrationReport PostSaveAudit;
				if (!RunDefenseMatrixAuthoring(AuditOptions, PostSaveAudit)
					|| PostSaveAudit.Summary.Failed > 0
					|| PostSaveAudit.Summary.WouldChange > 0)
				{
					bSaveFailed = true;
					if (!Report.Rows.IsEmpty())
					{
						Report.Rows[0].Errors.Add(
							TEXT("post-save DefenseMatrixAuthoring Audit did not return Unchanged"));
						Report.Rows[0].Status = EKatanaAssetMigrationStatus::Failed;
						Summarize(Report);
					}
				}
			}
		}

		bool bReportFailed = false;
		if (!Options.ReportPath.IsEmpty())
		{
			TArray<FString> ReportErrors;
			if (!WriteReport(Report, Options.ReportPath, ReportErrors))
			{
				bReportFailed = true;
				for (const FString& ReportError : ReportErrors)
				{
					UE_LOG(LogTemp, Error, TEXT("%s"), *ReportError);
				}
			}
		}
		if (bSaveFailed)
		{
			return EKatanaAssetMigrationExitCode::SaveFailure;
		}
		return (!bOperationSucceeded || Report.Summary.Failed > 0 || bReportFailed)
			? EKatanaAssetMigrationExitCode::RowFailure
			: EKatanaAssetMigrationExitCode::Success;
	}
	if (Options.Operation.Equals(FDefenseProofAuthoringOperation::OperationName, ESearchCase::IgnoreCase))
	{
		FKatanaAssetMigrationReport Report;
		const bool bOperationSucceeded = RunDefenseProofAuthoring(Options, Report);
		TSet<FString> InitiallyDirtyPackages;
		for (const FKatanaAssetMigrationPackageLedgerEntry& Entry : Report.PackageLedger)
		{
			if (Entry.bInitiallyDirty)
			{
				InitiallyDirtyPackages.Add(Entry.PackageName);
			}
		}

		bool bSaveFailed = false;
		if (bOperationSucceeded && Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
		{
			bSaveFailed = !SaveChangedPackages(Options, InitiallyDirtyPackages, Report);
		}
		if (!bSaveFailed && bOperationSucceeded
			&& Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
		{
			if (RequiresFreshProcessAudit(Report))
			{
				AnnotateFreshProcessAuditRequirement(Report);
			}
			else
			{
				FKatanaAssetMigrationOptions AuditOptions = Options;
				AuditOptions.Mode = EKatanaAssetMigrationMode::Audit;
				AuditOptions.ApprovedPlanReport.Reset();
				AuditOptions.ApprovedPlanFingerprint.Reset();
				FKatanaAssetMigrationReport PostSaveAudit;
				if (!RunDefenseProofAuthoring(AuditOptions, PostSaveAudit)
					|| PostSaveAudit.Summary.Failed > 0
					|| PostSaveAudit.Summary.WouldChange > 0)
				{
					bSaveFailed = true;
					if (!Report.Rows.IsEmpty())
					{
						Report.Rows[0].Errors.Add(
							TEXT("post-save DefenseProofAuthoring Audit did not return Unchanged"));
						Report.Rows[0].Status = EKatanaAssetMigrationStatus::Failed;
						Summarize(Report);
					}
				}
			}
		}

		bool bReportFailed = false;
		if (!Options.ReportPath.IsEmpty())
		{
			TArray<FString> ReportErrors;
			if (!WriteReport(Report, Options.ReportPath, ReportErrors))
			{
				bReportFailed = true;
				for (const FString& ReportError : ReportErrors)
				{
					UE_LOG(LogTemp, Error, TEXT("%s"), *ReportError);
				}
			}
		}
		if (bSaveFailed)
		{
			return EKatanaAssetMigrationExitCode::SaveFailure;
		}
		return (!bOperationSucceeded || Report.Summary.Failed > 0 || bReportFailed)
			? EKatanaAssetMigrationExitCode::RowFailure
			: EKatanaAssetMigrationExitCode::Success;
	}
	if (Options.Operation.Equals(FEnemyAIProofAssetsOperation::OperationName, ESearchCase::IgnoreCase))
	{
		TSet<FString> InitiallyDirtyPackages;
		FEnemyAIProofAssetsOperation::SnapshotInitiallyDirtyPackages(InitiallyDirtyPackages);

		FKatanaAssetMigrationReport Report;
		RunEnemyAIProofAssets(Options, Report);

		bool bSaveFailed = false;
		if (Report.Summary.Failed == 0 && Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
		{
			bSaveFailed = !SaveChangedPackages(Options, InitiallyDirtyPackages, Report);
		}

		bool bReportFailed = false;
		if (!Options.ReportPath.IsEmpty())
		{
			TArray<FString> ReportErrors;
			if (!WriteReport(Report, Options.ReportPath, ReportErrors))
			{
				bReportFailed = true;
				for (const FString& ReportError : ReportErrors)
				{
					UE_LOG(LogTemp, Error, TEXT("%s"), *ReportError);
				}
			}
		}

		if (bSaveFailed)
		{
			return EKatanaAssetMigrationExitCode::SaveFailure;
		}

		return (Report.Summary.Failed > 0 || bReportFailed)
			? EKatanaAssetMigrationExitCode::RowFailure
			: EKatanaAssetMigrationExitCode::Success;
	}

	LoadTargets(Options, Targets, FailedRows);

	TSet<FString> InitiallyDirtyPackages;
	SnapshotInitiallyDirtyPackages(Targets, InitiallyDirtyPackages);

	FKatanaAssetMigrationReport Report;

	if (Options.Operation.Equals(FAttackDataNotifyMigrationOperation::OperationName, ESearchCase::IgnoreCase))
	{
		if (!FailedRows.IsEmpty())
		{
			Report.SchemaVersion = 2;
			Report.Operation = Options.Operation;
			Report.Mode = Options.Mode;
			Report.Rows = MoveTemp(FailedRows);
		}
		else
		{
			RunAttackDataNotifyMigration(Options, Targets, Report);
		}
	}
	else if (Options.Operation.Equals(FAttackDataTimingMigrationOperation::OperationName, ESearchCase::IgnoreCase))
	{
		Report.Operation = Options.Operation;
		Report.Mode = Options.Mode;
		Report.Rows.Append(FailedRows);
		FKatanaAssetMigrationReport OperationReport;
		RunAttackDataTimingMigration(Options, Targets, OperationReport);
		Report.Rows.Append(OperationReport.Rows);
	}

	Summarize(Report);

	bool bSaveFailed = false;
	if (Report.Summary.Failed == 0 && Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
	{
		bSaveFailed = !SaveChangedPackages(Options, InitiallyDirtyPackages, Report);
	}

	bool bReportFailed = false;
	if (!Options.ReportPath.IsEmpty())
	{
		TArray<FString> ReportErrors;
		if (!WriteReport(Report, Options.ReportPath, ReportErrors))
		{
			bReportFailed = true;
			for (const FString& ReportError : ReportErrors)
			{
				UE_LOG(LogTemp, Error, TEXT("%s"), *ReportError);
			}
		}
	}

	if (bSaveFailed)
	{
		return EKatanaAssetMigrationExitCode::SaveFailure;
	}

	return (Report.Summary.Failed > 0 || bReportFailed)
		? EKatanaAssetMigrationExitCode::RowFailure
		: EKatanaAssetMigrationExitCode::Success;
}
