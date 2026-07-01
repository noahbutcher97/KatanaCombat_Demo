// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/Operations/ContentReadinessAuditOperation.h"

#include "AttackDataNotifyGenerationService.h"
#include "Animation/AnimMontage.h"
#include "Commandlets/KatanaAssetMigrationRunner.h"
#include "Data/AttackConfiguration.h"
#include "Data/AttackData.h"
#include "Data/PairedAnimationData.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

const FString FContentReadinessAuditOperation::OperationName = TEXT("ContentReadinessAudit");

namespace
{
	bool PackageFileExists(const FString& PackageName)
	{
		FString AssetFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, AssetFilename, FPackageName::GetAssetPackageExtension()) &&
			FPaths::FileExists(AssetFilename))
		{
			return true;
		}

		FString MapFilename;
		return FPackageName::TryConvertLongPackageNameToFilename(PackageName, MapFilename, FPackageName::GetMapPackageExtension()) &&
			FPaths::FileExists(MapFilename);
	}

	UObject* LoadContentTarget(const FString& PackageName, const FString& ObjectPath)
	{
		if (UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath))
		{
			return LoadedObject;
		}

		UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_None);
		if (!Package)
		{
			return nullptr;
		}

		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		if (UObject* World = FindObject<UWorld>(Package, *AssetName))
		{
			return World;
		}

		return FindObject<UObject>(Package, *AssetName);
	}

	bool IsSectionValid(const UAnimMontage* Montage, const FName SectionName)
	{
		return Montage && (SectionName.IsNone() || Montage->GetSectionIndex(SectionName) != INDEX_NONE);
	}

	void CopyAttackDataAnalysis(const FAttackDataNotifyAnalysis& Analysis, FKatanaAssetMigrationRow& OutRow)
	{
		OutRow.Montage = Analysis.Montage ? Analysis.Montage->GetPathName() : FString();
		OutRow.Section = Analysis.SectionName.ToString();
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
	}

	void AuditPairedAnimationReference(const TCHAR* Label, const UPairedAnimationData* PairedAnimationData, FKatanaAssetMigrationRow& OutRow)
	{
		if (!PairedAnimationData)
		{
			OutRow.Errors.Add(FString::Printf(TEXT("%s is required but unset"), Label));
			return;
		}

		const FString PairedDataPath = PairedAnimationData->GetPathName();
		if (!PairedAnimationData->AttackerMontage)
		{
			OutRow.Errors.Add(FString::Printf(TEXT("%s AttackerMontage is required but unset: %s"), Label, *PairedDataPath));
		}
		else if (!IsSectionValid(PairedAnimationData->AttackerMontage, PairedAnimationData->AttackerMontageSection))
		{
			OutRow.Errors.Add(FString::Printf(TEXT("%s AttackerMontageSection is invalid: %s on %s"),
				Label,
				*PairedAnimationData->AttackerMontageSection.ToString(),
				*PairedDataPath));
		}

		if (!PairedAnimationData->VictimMontage)
		{
			OutRow.Errors.Add(FString::Printf(TEXT("%s VictimMontage is required but unset: %s"), Label, *PairedDataPath));
		}
		else if (!IsSectionValid(PairedAnimationData->VictimMontage, PairedAnimationData->VictimMontageSection))
		{
			OutRow.Errors.Add(FString::Printf(TEXT("%s VictimMontageSection is invalid: %s on %s"),
				Label,
				*PairedAnimationData->VictimMontageSection.ToString(),
				*PairedDataPath));
		}

		if (!PairedAnimationData->IsValid())
		{
			OutRow.Errors.Add(FString::Printf(TEXT("%s runtime validation failed: %s"), Label, *PairedDataPath));
		}
	}

	void AuditAttackDataPairedReferences(const UAttackData* AttackData, FKatanaAssetMigrationRow& OutRow)
	{
		if (AttackData->bHasCounterVariant)
		{
			AuditPairedAnimationReference(TEXT("CounterData"), AttackData->CounterData, OutRow);
		}

		if (AttackData->bCanTriggerFinisher)
		{
			AuditPairedAnimationReference(TEXT("FinisherData"), AttackData->FinisherData, OutRow);
		}
	}

	void AuditAttackData(UAttackData* AttackData, FKatanaAssetMigrationRow& OutRow)
	{
		OutRow.AttackData = AttackData->GetPathName();
		const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);
		CopyAttackDataAnalysis(Analysis, OutRow);
		OutRow.bAttackDataSectionValid = Analysis.bValid && IsSectionValid(AttackData->AttackMontage, AttackData->MontageSection);

		if (!Analysis.bValid)
		{
			OutRow.Errors.Append(Analysis.Errors);
			return;
		}

		if (Analysis.LegacyNotifiesFound.Num() > 0 ||
			Analysis.StaleCanonicalNotifiesFound.Num() > 0 ||
			Analysis.CanonicalNotifiesMissing.Num() > 0)
		{
			OutRow.Warnings.Add(TEXT("AttackData notify plan is not clean; run AttackDataNotifyMigration before accepting this asset as branch-ready"));
		}

		AuditAttackDataPairedReferences(AttackData, OutRow);
	}

	void AuditAttackConfiguration(UAttackConfiguration* AttackConfiguration, FKatanaAssetMigrationRow& OutRow)
	{
		if (!AttackConfiguration->DefaultLightAttack)
		{
			OutRow.Errors.Add(TEXT("DefaultLightAttack is required but unset"));
		}
		if (!AttackConfiguration->DefaultHeavyAttack)
		{
			OutRow.Errors.Add(TEXT("DefaultHeavyAttack is required but unset"));
		}
	}

	void AuditPairedAnimationData(UPairedAnimationData* PairedAnimationData, FKatanaAssetMigrationRow& OutRow)
	{
		OutRow.bPairedAnimationValid = PairedAnimationData->IsValid();
		OutRow.Montage = PairedAnimationData->AttackerMontage ? PairedAnimationData->AttackerMontage->GetPathName() : FString();
		OutRow.Section = PairedAnimationData->AttackerMontageSection.ToString();
		OutRow.bPairedAttackerSectionValid = IsSectionValid(PairedAnimationData->AttackerMontage, PairedAnimationData->AttackerMontageSection);
		OutRow.bPairedVictimSectionValid = IsSectionValid(PairedAnimationData->VictimMontage, PairedAnimationData->VictimMontageSection);

		if (!PairedAnimationData->AttackerMontage)
		{
			OutRow.Errors.Add(TEXT("AttackerMontage is required but unset"));
		}
		else if (!OutRow.bPairedAttackerSectionValid)
		{
			OutRow.Errors.Add(FString::Printf(TEXT("AttackerMontageSection is invalid: %s"), *PairedAnimationData->AttackerMontageSection.ToString()));
		}

		if (!PairedAnimationData->VictimMontage)
		{
			OutRow.Errors.Add(TEXT("VictimMontage is required but unset"));
		}
		else if (!OutRow.bPairedVictimSectionValid)
		{
			OutRow.Errors.Add(FString::Printf(TEXT("VictimMontageSection is invalid: %s"), *PairedAnimationData->VictimMontageSection.ToString()));
		}

		if (!PairedAnimationData->IsValid())
		{
			OutRow.Errors.Add(TEXT("Paired animation runtime validation failed"));
		}
	}
}

bool FContentReadinessAuditOperation::Run(const FString& TargetString, EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const
{
	OutRow = FKatanaAssetMigrationRow();
	OutRow.InputTarget = TargetString;

	if (Mode != EKatanaAssetMigrationMode::Audit && Mode != EKatanaAssetMigrationMode::Plan)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors.Add(TEXT("ContentReadinessAudit is read-only and supports only Audit or Plan mode"));
		return false;
	}

	FString PackageName;
	FString ObjectPath;
	FString Error;
	if (!FKatanaAssetMigrationRunner::NormalizeContentTargetObjectPath(TargetString, PackageName, ObjectPath, Error))
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors.Add(Error);
		return false;
	}

	UObject* LoadedObject = LoadContentTarget(PackageName, ObjectPath);
	return RunLoadedObject(TargetString, LoadedObject, PackageFileExists(PackageName), OutRow);
}

bool FContentReadinessAuditOperation::RunLoadedObject(const FString& TargetString, UObject* LoadedObject, bool bPackageFileExists, FKatanaAssetMigrationRow& OutRow) const
{
	OutRow = FKatanaAssetMigrationRow();
	OutRow.InputTarget = TargetString;
	OutRow.bPackageFileExists = bPackageFileExists;

	FString Error;
	if (!FKatanaAssetMigrationRunner::NormalizeContentTargetObjectPath(TargetString, OutRow.PackageName, OutRow.ObjectPath, Error))
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors.Add(Error);
		return false;
	}

	if (!LoadedObject)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors.Add(FString::Printf(TEXT("Target did not load: %s"), *OutRow.ObjectPath));
		return false;
	}

	OutRow.bLoaded = true;
	OutRow.AssetClass = LoadedObject->GetClass()->GetPathName();
	OutRow.bMapLoaded = LoadedObject->IsA<UWorld>();

	if (UAttackData* AttackData = Cast<UAttackData>(LoadedObject))
	{
		AuditAttackData(AttackData, OutRow);
	}
	else if (UAttackConfiguration* AttackConfiguration = Cast<UAttackConfiguration>(LoadedObject))
	{
		AuditAttackConfiguration(AttackConfiguration, OutRow);
	}
	else if (UPairedAnimationData* PairedAnimationData = Cast<UPairedAnimationData>(LoadedObject))
	{
		AuditPairedAnimationData(PairedAnimationData, OutRow);
	}

	if (OutRow.Errors.Num() > 0)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		return false;
	}

	OutRow.Status = (OutRow.Warnings.Num() > 0 || OutRow.BranchReadinessWarnings.Num() > 0)
		? EKatanaAssetMigrationStatus::WouldChange
		: EKatanaAssetMigrationStatus::Unchanged;
	return true;
}
