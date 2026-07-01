// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/Operations/AttackDataNotifyMigrationOperation.h"

#include "AttackDataNotifyGenerationService.h"
#include "Animation/AnimMontage.h"
#include "Data/AttackData.h"

const FString FAttackDataNotifyMigrationOperation::OperationName = TEXT("AttackDataNotifyMigration");

bool FAttackDataNotifyMigrationOperation::Run(UAttackData* AttackData, EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const
{
	OutRow = FKatanaAssetMigrationRow();
	OutRow.InputTarget = AttackData ? AttackData->GetPathName() : FString();
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
	OutRow.bHasParryWindow = Analysis.bHasParryWindow;
	OutRow.bHasCounterWindow = Analysis.bHasCounterWindow;
	OutRow.bCounterVariantHasData = Analysis.bCounterVariantHasData;
	OutRow.bFinisherHasData = Analysis.bFinisherHasData;

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
