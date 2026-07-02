// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/Operations/AttackDataTimingMigrationOperation.h"

#include "AttackDataNotifyGenerationService.h"
#include "Animation/AnimMontage.h"
#include "Data/AttackData.h"

const FString FAttackDataTimingMigrationOperation::OperationName = TEXT("AttackDataTimingMigration");

namespace
{
	void PopulateTimingRow(UAttackData* AttackData, const FAttackDataNotifyAnalysis& Analysis, FKatanaAssetMigrationRow& OutRow)
	{
		OutRow = FKatanaAssetMigrationRow();
		OutRow.InputTarget = AttackData ? AttackData->GetPathName() : FString();
		OutRow.AttackData = AttackData ? AttackData->GetPathName() : FString();
		OutRow.Montage = (AttackData && AttackData->AttackMontage) ? AttackData->AttackMontage->GetPathName() : FString();
		OutRow.Section = AttackData ? AttackData->MontageSection.ToString() : FString();
		OutRow.SectionStart = Analysis.SectionStart;
		OutRow.SectionEnd = Analysis.SectionEnd;
		OutRow.SectionLength = Analysis.SectionLength;
		OutRow.WindupDuration = Analysis.WindupDuration;
		OutRow.ActiveDuration = Analysis.ActiveDuration;
		OutRow.RecoveryDuration = Analysis.RecoveryDuration;
		OutRow.TimingTotal = Analysis.TimingTotal;
		OutRow.HoldWindowStart = Analysis.HoldWindowStart;
		OutRow.ProposedWindupDuration = Analysis.WindupDuration;
		OutRow.ProposedActiveDuration = Analysis.ActiveDuration;
		OutRow.ProposedRecoveryDuration = Analysis.RecoveryDuration;
		OutRow.ProposedTimingTotal = Analysis.TimingTotal;
		OutRow.AttackTags = Analysis.AttackTags;
		OutRow.RequiredContextTags = Analysis.RequiredContextTags;
		OutRow.bHasRequiredContextTags = Analysis.bHasRequiredContextTags;
		OutRow.bHasUnblockableTag = Analysis.bHasUnblockableTag;
	}
}

bool FAttackDataTimingMigrationOperation::Run(UAttackData* AttackData, EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const
{
	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);
	PopulateTimingRow(AttackData, Analysis, OutRow);

	if (!AttackData)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors = Analysis.Errors;
		return false;
	}

	if (!AttackData->AttackMontage || Analysis.SectionLength <= 0.0f)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors = Analysis.Errors;
		return false;
	}

	if (Analysis.WindupDuration <= 0.0f || Analysis.ActiveDuration <= 0.0f || Analysis.RecoveryDuration < 0.0f)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors = Analysis.Errors;
		return false;
	}

	if (Analysis.TimingTotal <= Analysis.SectionLength + KINDA_SMALL_NUMBER)
	{
		if (!Analysis.bValid)
		{
			OutRow.Status = EKatanaAssetMigrationStatus::Failed;
			OutRow.Errors = Analysis.Errors;
			return false;
		}

		OutRow.Status = EKatanaAssetMigrationStatus::Unchanged;
		return true;
	}

	const float WindupAndActiveDuration = Analysis.WindupDuration + Analysis.ActiveDuration;
	if (WindupAndActiveDuration > Analysis.SectionLength + KINDA_SMALL_NUMBER)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors.Add(FString::Printf(
			TEXT("Windup plus Active %.3fs exceeds section length %.3fs; recovery clamp cannot preserve attack phases"),
			WindupAndActiveDuration,
			Analysis.SectionLength));
		return false;
	}

	const float ProposedRecoveryDuration = FMath::Max(0.0f, Analysis.SectionLength - WindupAndActiveDuration);
	OutRow.ProposedRecoveryDuration = ProposedRecoveryDuration;
	OutRow.ProposedTimingTotal = Analysis.WindupDuration + Analysis.ActiveDuration + ProposedRecoveryDuration;
	OutRow.PlannedAdditions.Add(FString::Printf(
		TEXT("ManualTiming.RecoveryDuration %.3fs -> %.3fs"),
		Analysis.RecoveryDuration,
		ProposedRecoveryDuration));

	if (AttackData->GetOutermost())
	{
		OutRow.ChangedPackages.AddUnique(AttackData->GetOutermost()->GetName());
	}

	if (Mode == EKatanaAssetMigrationMode::Audit || Mode == EKatanaAssetMigrationMode::Plan)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::WouldChange;
		return true;
	}

	AttackData->Modify();
	AttackData->ManualTiming.RecoveryDuration = ProposedRecoveryDuration;
	AttackData->MarkPackageDirty();

	OutRow.Status = EKatanaAssetMigrationStatus::Changed;
	return true;
}
