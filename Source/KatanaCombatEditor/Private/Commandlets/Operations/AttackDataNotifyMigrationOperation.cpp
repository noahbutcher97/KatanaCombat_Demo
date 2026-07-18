// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/Operations/AttackDataNotifyMigrationOperation.h"

#include "Algo/Unique.h"
#include "AttackDataNotifyGenerationService.h"
#include "Animation/AnimMontage.h"
#include "Commandlets/AssetAuthoringApprovalService.h"
#include "Commandlets/KatanaAssetMigrationRunner.h"
#include "Commandlets/Operations/DefenseProofMigrationOperation.h"
#include "Data/AttackData.h"
#include "Data/PairedAnimationData.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/Package.h"

namespace
{
	template <typename EnumType>
	FString EnumName(EnumType Value)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		return Enum
			? Enum->GetNameStringByValue(static_cast<int64>(Value))
			: LexToString(static_cast<int64>(Value));
	}

	FString ObjectPath(const UObject* Object)
	{
		return IsValid(Object) ? Object->GetPathName() : FString();
	}

	FString HashBytes(const TArray<uint8>& Bytes)
	{
		return FSHA1::HashBuffer(
			Bytes.GetData(), static_cast<uint64>(Bytes.Num())).ToString();
	}

	void AddApprovalDetails(
		FKatanaAssetMigrationRow& Row,
		const FString& TargetsFileIdentity,
		const FString& TargetsFileHash,
		const FString& InputStateHash,
		const int32 InputStateCount)
	{
		Row.Details.Add(TEXT("approval_contract_version"), TEXT("1"));
		Row.Details.Add(TEXT("approval_targets_file"), TargetsFileIdentity);
		Row.Details.Add(TEXT("approval_targets_file_hash"), TargetsFileHash);
		Row.Details.Add(TEXT("approval_input_state_hash"), InputStateHash);
		Row.Details.Add(TEXT("approval_input_state_count"), LexToString(InputStateCount));
	}

	void PopulateDefenseInventoryDetails(
		const UAttackData* AttackData,
		const FAttackDataNotifyAnalysis& Analysis,
		FKatanaAssetMigrationRow& OutRow)
	{
		if (!AttackData)
		{
			return;
		}

		const FDefenseAttackProfile& Profile = AttackData->DefenseProfile;
		OutRow.Details.Add(TEXT("attack_type"), EnumName(AttackData->AttackType));
		OutRow.Details.Add(TEXT("attack_direction"), EnumName(AttackData->Direction));
		OutRow.Details.Add(TEXT("defense_height"), EnumName(Profile.Height));
		OutRow.Details.Add(TEXT("defense_nominal_lane"), EnumName(Profile.NominalLane));
		OutRow.Details.Add(TEXT("defense_swing_shape"), EnumName(Profile.SwingShape));
		OutRow.Details.Add(TEXT("defense_source_socket"), Profile.SourceContactSocketOverride.ToString());
		OutRow.Details.Add(TEXT("defense_target_bone"), AttackData->GetDefenseTargetBoneFallback().ToString());
		OutRow.Details.Add(TEXT("attack_hand"), AttackData->AttackHand.ToString());
		OutRow.Details.Add(TEXT("default_contact_bone"), AttackData->DefaultContactBone.ToString());
		OutRow.Details.Add(TEXT("blocked_audio_override"),
			LexToString(Profile.bOverrideBlockedImpactAudio));
		OutRow.Details.Add(TEXT("blocked_audio_asset"),
			ObjectPath(Profile.BlockedImpactAudio.ImpactSound));
		OutRow.Details.Add(TEXT("blocked_vfx_override"),
			LexToString(Profile.bOverrideBlockedImpactVFX));
		OutRow.Details.Add(TEXT("blocked_vfx_asset"),
			ObjectPath(Profile.BlockedImpactVFX.ImpactVFX));
		OutRow.Details.Add(TEXT("counter_data_asset"), ObjectPath(AttackData->CounterData));
		OutRow.Details.Add(TEXT("finisher_data_asset"), ObjectPath(AttackData->FinisherData));

		int32 SegmentIndex = 0;
		if (Analysis.Montage)
		{
			for (const FSlotAnimationTrack& Slot : Analysis.Montage->SlotAnimTracks)
			{
				for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
				{
					if (Segment.GetEndPos() <= Analysis.SectionStart + KINDA_SMALL_NUMBER
						|| Segment.StartPos >= Analysis.SectionEnd - KINDA_SMALL_NUMBER)
					{
						continue;
					}

					const FString Key = FString::Printf(TEXT("section_segment_%d"), SegmentIndex++);
					OutRow.Details.Add(Key, FString::Printf(
						TEXT("slot=%s|animation=%s|montage_start=%.6f|montage_end=%.6f|")
						TEXT("source_start=%.6f|source_end=%.6f|play_rate=%.6f|loop_count=%d"),
						*Slot.SlotName.ToString(),
						*ObjectPath(Segment.GetAnimReference()),
						Segment.StartPos,
						Segment.GetEndPos(),
						Segment.AnimStartTime,
						Segment.AnimEndTime,
						Segment.AnimPlayRate,
						Segment.LoopingCount));
				}
			}
		}
		OutRow.Details.Add(TEXT("section_segment_count"), LexToString(SegmentIndex));
	}
}

const FString FAttackDataNotifyMigrationOperation::OperationName = TEXT("AttackDataNotifyMigration");

bool FAttackDataNotifyMigrationOperation::BuildPlanReport(
	const FKatanaAssetMigrationOptions& Options,
	const TArray<UAttackData*>& Targets,
	FKatanaAssetMigrationReport& OutReport,
	TArray<FString>& OutErrors)
{
	OutReport = FKatanaAssetMigrationReport();
	OutReport.SchemaVersion = 2;
	OutReport.Operation = OperationName;
	OutReport.Mode = EKatanaAssetMigrationMode::Plan;
	OutReport.Gate = TEXT("AttackDataNotifyApproval");

	FString TargetsFileIdentity =
		FKatanaAssetMigrationRunner::ResolveProjectRelativeFilePath(Options.TargetsFile);
	FPaths::NormalizeFilename(TargetsFileIdentity);
	OutReport.ManifestPath = TargetsFileIdentity;

	TArray<uint8> TargetsFileBytes;
	FString TargetsFileHash;
	if (TargetsFileIdentity.IsEmpty()
		|| !FFileHelper::LoadFileToArray(TargetsFileBytes, *TargetsFileIdentity))
	{
		OutErrors.Add(FString::Printf(
			TEXT("could not hash notify-migration TargetsFile: %s"),
			*TargetsFileIdentity));
	}
	else
	{
		TargetsFileHash = HashBytes(TargetsFileBytes);
	}

	TArray<FString> ExpectedTargetPaths;
	TArray<FString> TargetLines;
	if (!TargetsFileIdentity.IsEmpty()
		&& FFileHelper::LoadFileToStringArray(TargetLines, *TargetsFileIdentity))
	{
		for (FString TargetLine : TargetLines)
		{
			TargetLine.TrimStartAndEndInline();
			if (TargetLine.IsEmpty() || TargetLine.StartsWith(TEXT("#")))
			{
				continue;
			}
			FString ObjectPathValue;
			FString Error;
			if (!FKatanaAssetMigrationRunner::NormalizeAttackDataTargetObjectPath(
				TargetLine, ObjectPathValue, Error))
			{
				OutErrors.Add(Error);
				continue;
			}
			ExpectedTargetPaths.AddUnique(ObjectPathValue);
		}
	}
	ExpectedTargetPaths.Sort();

	TArray<UAttackData*> SortedTargets = Targets;
	SortedTargets.RemoveAll([](const UAttackData* Target)
	{
		return !IsValid(Target);
	});
	SortedTargets.Sort([](const UAttackData& Left, const UAttackData& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});
	TArray<FString> LoadedTargetPaths;
	for (const UAttackData* Target : SortedTargets)
	{
		LoadedTargetPaths.AddUnique(Target->GetPathName());
	}
	LoadedTargetPaths.Sort();
	if (ExpectedTargetPaths.IsEmpty())
	{
		OutErrors.Add(TEXT("notify-migration TargetsFile contains no AttackData targets"));
	}
	else if (ExpectedTargetPaths != LoadedTargetPaths)
	{
		OutErrors.Add(TEXT(
			"loaded notify-migration target scope differs from the current TargetsFile"));
	}

	const FAttackDataNotifyMigrationOperation Operation;
	TSet<FString> ChangedPackages;
	TArray<FString> InputIdentities;
	TMap<FString, TSet<FString>> PackageRoles;
	for (UAttackData* Target : SortedTargets)
	{
		FKatanaAssetMigrationRow Row;
		Operation.Run(Target, EKatanaAssetMigrationMode::Plan, Row);
		OutReport.Rows.Add(MoveTemp(Row));
		InputIdentities.AddUnique(Target->GetPathName());
		if (const UPackage* AttackPackage = Target->GetOutermost())
		{
			PackageRoles.FindOrAdd(AttackPackage->GetName()).Add(TEXT("AttackDataInput"));
		}
		if (Target->AttackMontage)
		{
			InputIdentities.AddUnique(Target->AttackMontage->GetPathName());
			for (const FString& ChangedPackage : OutReport.Rows.Last().ChangedPackages)
			{
				ChangedPackages.Add(ChangedPackage);
			}
			const FString MontagePackage = Target->AttackMontage->GetOutermost()->GetName();
			PackageRoles.FindOrAdd(MontagePackage).Add(
				ChangedPackages.Contains(MontagePackage)
					? TEXT("MontageDestination")
					: TEXT("MontageInput"));
		}
	}

	FString InputStateHash;
	int32 InputStateCount = 0;
	TArray<FString> StateErrors;
	FKatanaAssetAuthoringApprovalService::BuildPackageStateHash(
		InputIdentities,
		TEXT("AttackDataNotifyInput"),
		true,
		true,
		InputStateHash,
		InputStateCount,
		StateErrors);
	OutErrors.Append(StateErrors);

	TArray<FString> PackageNames;
	PackageRoles.GetKeys(PackageNames);
	PackageNames.Sort();
	for (const FString& PackageName : PackageNames)
	{
		TArray<FString> Roles = PackageRoles.FindChecked(PackageName).Array();
		Roles.Sort();
		FKatanaAssetMigrationPackageLedgerEntry Entry;
		Entry.PackageName = PackageName;
		Entry.PackageRole = FString::Join(Roles, TEXT("+"));
		if (const UPackage* Package = FindPackage(nullptr, *PackageName))
		{
			Entry.bInitiallyDirty = Package->IsDirty();
		}
		Entry.PlannedAction = ChangedPackages.Contains(PackageName)
			? TEXT("Update")
			: TEXT("None");
		OutReport.PackageLedger.Add(MoveTemp(Entry));
	}

	if (OutReport.Rows.IsEmpty())
	{
		FKatanaAssetMigrationRow Row;
		Row.InputTarget = TargetsFileIdentity;
		Row.Status = EKatanaAssetMigrationStatus::Failed;
		Row.Errors.Add(TEXT("notify-migration Plan has no loaded targets"));
		OutReport.Rows.Add(MoveTemp(Row));
	}
	for (FKatanaAssetMigrationRow& Row : OutReport.Rows)
	{
		AddApprovalDetails(
			Row,
			TargetsFileIdentity,
			TargetsFileHash,
			InputStateHash,
			InputStateCount);
	}
	if (!OutErrors.IsEmpty())
	{
		OutReport.Rows[0].Errors.Append(OutErrors);
		OutReport.Rows[0].Status = EKatanaAssetMigrationStatus::Failed;
	}

	FKatanaAssetMigrationRunner::Summarize(OutReport);
	if (OutReport.Summary.Failed > 0)
	{
		return false;
	}
	return FinalizePlanReportFingerprint(OutReport, OutErrors);
}

bool FAttackDataNotifyMigrationOperation::FinalizePlanReportFingerprint(
	FKatanaAssetMigrationReport& InOutReport,
	TArray<FString>& OutErrors)
{
	if (InOutReport.SchemaVersion != 2
		|| InOutReport.Operation != OperationName
		|| InOutReport.Mode != EKatanaAssetMigrationMode::Plan)
	{
		OutErrors.Add(TEXT(
			"notify-migration approval requires a schema-v2 AttackDataNotifyMigration Plan"));
		return false;
	}

	FKatanaAssetMigrationReport FingerprintInput = InOutReport;
	FingerprintInput.PlanFingerprint.Reset();
	FString Json;
	if (!FKatanaAssetMigrationRunner::SerializeReport(
		FingerprintInput, Json, OutErrors))
	{
		return false;
	}
	FString CanonicalJson;
	FString CanonicalError;
	if (!FDefenseProofMigrationOperation::CanonicalizeJson(
		Json, CanonicalJson, CanonicalError))
	{
		OutErrors.Add(FString::Printf(
			TEXT("could not canonicalize notify-migration Plan: %s"),
			*CanonicalError));
		return false;
	}
	InOutReport.PlanFingerprint = FKatanaAssetAuthoringApprovalService::HashText(
		TEXT("AttackDataNotifyApproval/v1\n") + CanonicalJson);
	return true;
}

bool FAttackDataNotifyMigrationOperation::ValidateApprovedPlanJson(
	const FString& Json,
	const FString& ApprovedPlanFingerprint,
	const FKatanaAssetMigrationReport& CurrentPlanReport,
	TArray<FString>& OutErrors)
{
	if (ApprovedPlanFingerprint.IsEmpty())
	{
		OutErrors.Add(TEXT("approved notify-migration Plan fingerprint is required"));
		return false;
	}
	FKatanaAssetMigrationReport RecomputedPlan = CurrentPlanReport;
	const FString CurrentFingerprint = CurrentPlanReport.PlanFingerprint;
	if (!FinalizePlanReportFingerprint(RecomputedPlan, OutErrors)
		|| RecomputedPlan.PlanFingerprint != CurrentFingerprint)
	{
		OutErrors.Add(TEXT("current notify-migration Plan fingerprint is invalid"));
		return false;
	}
	if (ApprovedPlanFingerprint != CurrentFingerprint)
	{
		OutErrors.Add(TEXT(
			"approved notify-migration fingerprint differs from the current Plan"));
		return false;
	}

	FString CurrentJson;
	if (!FKatanaAssetMigrationRunner::SerializeReport(
		CurrentPlanReport, CurrentJson, OutErrors))
	{
		return false;
	}
	FString CanonicalApproved;
	FString CanonicalCurrent;
	FString CanonicalError;
	if (!FDefenseProofMigrationOperation::CanonicalizeJson(
		Json, CanonicalApproved, CanonicalError))
	{
		OutErrors.Add(FString::Printf(
			TEXT("approved notify-migration Plan is invalid JSON: %s"),
			*CanonicalError));
		return false;
	}
	if (!FDefenseProofMigrationOperation::CanonicalizeJson(
		CurrentJson, CanonicalCurrent, CanonicalError))
	{
		OutErrors.Add(FString::Printf(
			TEXT("current notify-migration Plan could not be canonicalized: %s"),
			*CanonicalError));
		return false;
	}
	if (CanonicalApproved != CanonicalCurrent)
	{
		OutErrors.Add(TEXT(
			"approved notify-migration report content differs from the current Plan"));
		return false;
	}
	return true;
}

bool FAttackDataNotifyMigrationOperation::ValidateApprovedPlanBinding(
	const FKatanaAssetMigrationOptions& Options,
	const FKatanaAssetMigrationReport& CurrentPlanReport,
	TArray<FString>& OutErrors)
{
	FString Json;
	const FString ReportPath =
		FKatanaAssetMigrationRunner::ResolveProjectRelativeFilePath(
			Options.ApprovedPlanReport);
	if (!FFileHelper::LoadFileToString(Json, *ReportPath))
	{
		OutErrors.Add(FString::Printf(
			TEXT("could not read approved notify-migration Plan: %s"),
			*ReportPath));
		return false;
	}
	return ValidateApprovedPlanJson(
		Json,
		Options.ApprovedPlanFingerprint,
		CurrentPlanReport,
		OutErrors);
}

bool FAttackDataNotifyMigrationOperation::Run(UAttackData* AttackData, EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const
{
	OutRow = FKatanaAssetMigrationRow();
	OutRow.InputTarget = AttackData ? AttackData->GetPathName() : FString();
	OutRow.PackageName = AttackData ? AttackData->GetOutermost()->GetName() : FString();
	OutRow.ObjectPath = AttackData ? AttackData->GetPathName() : FString();
	OutRow.AssetClass = AttackData
		? AttackData->GetClass()->GetClassPathName().ToString()
		: FString();
	OutRow.AttackData = AttackData ? AttackData->GetPathName() : FString();
	OutRow.Montage = (AttackData && AttackData->AttackMontage) ? AttackData->AttackMontage->GetPathName() : FString();
	OutRow.Section = AttackData ? AttackData->MontageSection.ToString() : FString();

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);
	OutRow.SectionStart = Analysis.SectionStart;
	OutRow.SectionEnd = Analysis.SectionEnd;
	OutRow.SectionLength = Analysis.SectionLength;
	OutRow.WindupDuration = Analysis.WindupDuration;
	OutRow.ActiveDuration = Analysis.ActiveDuration;
	OutRow.RecoveryDuration = Analysis.RecoveryDuration;
	OutRow.TimingTotal = Analysis.TimingTotal;
	OutRow.HoldWindowStart = Analysis.HoldWindowStart;
	OutRow.LegacyNotifiesFound = Analysis.LegacyNotifiesFound;
	OutRow.StaleCanonicalNotifiesFound = Analysis.StaleCanonicalNotifiesFound;
	OutRow.CanonicalNotifiesMissing = Analysis.CanonicalNotifiesMissing;
	OutRow.BranchReadinessWarnings = Analysis.BranchReadinessWarnings;
	OutRow.AttackTags = Analysis.AttackTags;
	OutRow.RequiredContextTags = Analysis.RequiredContextTags;
	OutRow.bHasParryWindow = Analysis.bHasParryWindow;
	OutRow.bHasCounterWindow = Analysis.bHasCounterWindow;
	OutRow.bCounterVariantHasData = Analysis.bCounterVariantHasData;
	OutRow.bFinisherHasData = Analysis.bFinisherHasData;
	OutRow.bHasRequiredContextTags = Analysis.bHasRequiredContextTags;
	OutRow.bHasUnblockableTag = Analysis.bHasUnblockableTag;
	PopulateDefenseInventoryDetails(AttackData, Analysis, OutRow);

	if (!Analysis.bValid)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors = Analysis.Errors;
		return false;
	}

	if (Mode == EKatanaAssetMigrationMode::Audit)
	{
		OutRow.Status = (Analysis.LegacyNotifiesFound.Num() > 0 ||
				Analysis.StaleCanonicalNotifiesFound.Num() > 0 ||
				Analysis.CanonicalNotifiesMissing.Num() > 0)
			? EKatanaAssetMigrationStatus::WouldChange
			: EKatanaAssetMigrationStatus::Unchanged;
		return true;
	}

	const FAttackDataNotifyPlan Plan = FAttackDataNotifyGenerationService::BuildAttackDataNotifyPlan(Analysis);
	OutRow.PlannedRemovals = Plan.PlannedRemovals;
	OutRow.PlannedAdditions = Plan.PlannedAdditions;

	if (!Plan.bValid)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors = Plan.Errors;
		return false;
	}

	if (!Plan.HasChanges())
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Unchanged;
		return true;
	}

	if (AttackData && AttackData->AttackMontage)
	{
		OutRow.ChangedPackages.AddUnique(AttackData->AttackMontage->GetOutermost()->GetName());
	}

	if (Mode == EKatanaAssetMigrationMode::Plan)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::WouldChange;
		return true;
	}

	if (!FAttackDataNotifyGenerationService::ApplyAttackDataNotifyPlan(AttackData, Plan))
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors.Add(TEXT("ApplyAttackDataNotifyPlan failed"));
		return false;
	}

	OutRow.Status = EKatanaAssetMigrationStatus::Changed;
	return true;
}
