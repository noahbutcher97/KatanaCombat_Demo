// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/Operations/CounterChainProofMigrationOperation.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AttackDataNotifyGenerationService.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyState_CounterWindow.h"
#include "Commandlets/KatanaAssetMigrationRunner.h"
#include "Data/AttackData.h"
#include "Data/PairedAnimationData.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

const FString FCounterChainProofMigrationOperation::OperationName = TEXT("CounterChainProofMigration");

namespace
{
	constexpr float FloatTolerance = 0.001f;

	bool IsTimeInSection(float Time, float SectionStart, float SectionEnd)
	{
		return Time >= SectionStart && Time < SectionEnd;
	}

	void CopyAnalysisToRow(const FAttackDataNotifyAnalysis& Analysis, FKatanaAssetMigrationRow& OutRow)
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
		OutRow.AttackTags = Analysis.AttackTags;
		OutRow.RequiredContextTags = Analysis.RequiredContextTags;
		OutRow.bHasParryWindow = Analysis.bHasParryWindow;
		OutRow.bHasCounterWindow = Analysis.bHasCounterWindow;
		OutRow.bCounterVariantHasData = Analysis.bCounterVariantHasData;
		OutRow.bFinisherHasData = Analysis.bFinisherHasData;
		OutRow.bHasRequiredContextTags = Analysis.bHasRequiredContextTags;
		OutRow.bHasUnblockableTag = Analysis.bHasUnblockableTag;
	}

	void AddChangedPackage(UObject* Object, FKatanaAssetMigrationRow& OutRow)
	{
		if (Object && Object->GetOutermost())
		{
			OutRow.ChangedPackages.AddUnique(Object->GetOutermost()->GetName());
		}
	}

	void AddDirtyPackage(UObject* Object, TSet<FString>& OutDirtyPackages)
	{
		if (!Object || !Object->GetOutermost())
		{
			return;
		}

		if (Object->GetOutermost()->IsDirty())
		{
			OutDirtyPackages.Add(Object->GetOutermost()->GetName());
		}
	}

	FString BuildGeneratedDescription(const UPairedAnimationData* TemplateData)
	{
		return FString::Printf(
			TEXT("Nonlethal counter-chain proof data generated from %s."),
			TemplateData ? *TemplateData->GetPathName() : TEXT("<missing template>"));
	}

	void CopyPairedDefaults(const UPairedAnimationData* TemplateData, UPairedAnimationData* CounterData, const FString& AssetName)
	{
		check(TemplateData);
		check(CounterData);

		CounterData->AnimationName = FName(*AssetName);
		CounterData->ReactionType = EPairedReactionType::Counter;
		CounterData->Description = BuildGeneratedDescription(TemplateData);
		CounterData->AttackerMontage = TemplateData->AttackerMontage;
		CounterData->AttackerMontageSection = TemplateData->AttackerMontageSection;
		CounterData->VictimMontage = TemplateData->VictimMontage;
		CounterData->VictimMontageSection = TemplateData->VictimMontageSection;
		CounterData->SyncPointTime = TemplateData->SyncPointTime;
		CounterData->SyncPointName = TemplateData->SyncPointName;
		CounterData->VictimStartOffset = TemplateData->VictimStartOffset;
		CounterData->AttackerBlendIn = TemplateData->AttackerBlendIn;
		CounterData->AttackerBlendOut = TemplateData->AttackerBlendOut;
		CounterData->VictimBlendIn = TemplateData->VictimBlendIn;
		CounterData->VictimBlendOut = TemplateData->VictimBlendOut;
		CounterData->VictimRelativePosition = TemplateData->VictimRelativePosition;
		CounterData->VictimFacingMode = TemplateData->VictimFacingMode;
		CounterData->VictimRelativeRotation = TemplateData->VictimRelativeRotation;
		CounterData->MaxWarpDistance = TemplateData->MaxWarpDistance;
		CounterData->MinTriggerDistance = TemplateData->MinTriggerDistance;
		CounterData->MaxTriggerDistance = TemplateData->MaxTriggerDistance;
		CounterData->AttackerWarpConfig = TemplateData->AttackerWarpConfig;
		CounterData->VictimWarpConfig = TemplateData->VictimWarpConfig;
		CounterData->bApplySlowMotion = TemplateData->bApplySlowMotion;
		CounterData->SlowMotionScale = TemplateData->SlowMotionScale;
		CounterData->SlowMotionDuration = TemplateData->SlowMotionDuration;
		CounterData->ImpactCameraShake = TemplateData->ImpactCameraShake;
		CounterData->ImpactSound = TemplateData->ImpactSound;
		CounterData->VictimReactionSound = TemplateData->VictimReactionSound;
		CounterData->AttackerVoiceLine = TemplateData->AttackerVoiceLine;
		CounterData->MusicDuckingDB = TemplateData->MusicDuckingDB;
		CounterData->ImpactVFX = TemplateData->ImpactVFX;
		CounterData->SlowMoPostProcessMaterial = TemplateData->SlowMoPostProcessMaterial;
		CounterData->SlowMoPostProcessWeight = TemplateData->SlowMoPostProcessWeight;
		CounterData->ScreenBloodMaterial = TemplateData->ScreenBloodMaterial;
		CounterData->bSpawnBloodDecals = TemplateData->bSpawnBloodDecals;
		CounterData->BaseDamage = TemplateData->BaseDamage;
		CounterData->DamageMultiplier = TemplateData->DamageMultiplier;
		CounterData->bIsLethal = false;
		CounterData->VictimDeathOutcome = TemplateData->VictimDeathOutcome;
		CounterData->RagdollBlendTime = TemplateData->RagdollBlendTime;
	}

	bool PairedDefaultsMatch(const UPairedAnimationData* CounterData, const UPairedAnimationData* TemplateData, const FString& AssetName)
	{
		if (!CounterData || !TemplateData)
		{
			return false;
		}

		return CounterData->AnimationName == FName(*AssetName) &&
			CounterData->ReactionType == EPairedReactionType::Counter &&
			CounterData->Description == BuildGeneratedDescription(TemplateData) &&
			CounterData->AttackerMontage == TemplateData->AttackerMontage &&
			CounterData->AttackerMontageSection == TemplateData->AttackerMontageSection &&
			CounterData->VictimMontage == TemplateData->VictimMontage &&
			CounterData->VictimMontageSection == TemplateData->VictimMontageSection &&
			FMath::IsNearlyEqual(CounterData->SyncPointTime, TemplateData->SyncPointTime, FloatTolerance) &&
			CounterData->SyncPointName == TemplateData->SyncPointName &&
			FMath::IsNearlyEqual(CounterData->VictimStartOffset, TemplateData->VictimStartOffset, FloatTolerance) &&
			FMath::IsNearlyEqual(CounterData->AttackerBlendIn, TemplateData->AttackerBlendIn, FloatTolerance) &&
			FMath::IsNearlyEqual(CounterData->AttackerBlendOut, TemplateData->AttackerBlendOut, FloatTolerance) &&
			FMath::IsNearlyEqual(CounterData->VictimBlendIn, TemplateData->VictimBlendIn, FloatTolerance) &&
			FMath::IsNearlyEqual(CounterData->VictimBlendOut, TemplateData->VictimBlendOut, FloatTolerance) &&
			CounterData->VictimRelativePosition.Equals(TemplateData->VictimRelativePosition, FloatTolerance) &&
			CounterData->VictimFacingMode == TemplateData->VictimFacingMode &&
			CounterData->VictimRelativeRotation.Equals(TemplateData->VictimRelativeRotation, FloatTolerance) &&
			FMath::IsNearlyEqual(CounterData->MaxWarpDistance, TemplateData->MaxWarpDistance, FloatTolerance) &&
			FMath::IsNearlyEqual(CounterData->MinTriggerDistance, TemplateData->MinTriggerDistance, FloatTolerance) &&
			FMath::IsNearlyEqual(CounterData->MaxTriggerDistance, TemplateData->MaxTriggerDistance, FloatTolerance) &&
			CounterData->AttackerWarpConfig.WarpTargetName == TemplateData->AttackerWarpConfig.WarpTargetName &&
			CounterData->AttackerWarpConfig.RelativeOffset.Equals(TemplateData->AttackerWarpConfig.RelativeOffset, FloatTolerance) &&
			FMath::IsNearlyEqual(CounterData->AttackerWarpConfig.MaxWarpDistance, TemplateData->AttackerWarpConfig.MaxWarpDistance, FloatTolerance) &&
			CounterData->AttackerWarpConfig.bWarpTranslation == TemplateData->AttackerWarpConfig.bWarpTranslation &&
			CounterData->AttackerWarpConfig.bWarpRotation == TemplateData->AttackerWarpConfig.bWarpRotation &&
			CounterData->AttackerWarpConfig.bAdjustToTerrain == TemplateData->AttackerWarpConfig.bAdjustToTerrain &&
			CounterData->VictimWarpConfig.WarpTargetName == TemplateData->VictimWarpConfig.WarpTargetName &&
			CounterData->VictimWarpConfig.RelativeOffset.Equals(TemplateData->VictimWarpConfig.RelativeOffset, FloatTolerance) &&
			FMath::IsNearlyEqual(CounterData->VictimWarpConfig.MaxWarpDistance, TemplateData->VictimWarpConfig.MaxWarpDistance, FloatTolerance) &&
			CounterData->VictimWarpConfig.bWarpTranslation == TemplateData->VictimWarpConfig.bWarpTranslation &&
			CounterData->VictimWarpConfig.bWarpRotation == TemplateData->VictimWarpConfig.bWarpRotation &&
			CounterData->VictimWarpConfig.bAdjustToTerrain == TemplateData->VictimWarpConfig.bAdjustToTerrain &&
			CounterData->bApplySlowMotion == TemplateData->bApplySlowMotion &&
			FMath::IsNearlyEqual(CounterData->SlowMotionScale, TemplateData->SlowMotionScale, FloatTolerance) &&
			FMath::IsNearlyEqual(CounterData->SlowMotionDuration, TemplateData->SlowMotionDuration, FloatTolerance) &&
			CounterData->ImpactCameraShake == TemplateData->ImpactCameraShake &&
			CounterData->ImpactSound == TemplateData->ImpactSound &&
			CounterData->VictimReactionSound == TemplateData->VictimReactionSound &&
			CounterData->AttackerVoiceLine == TemplateData->AttackerVoiceLine &&
			FMath::IsNearlyEqual(CounterData->MusicDuckingDB, TemplateData->MusicDuckingDB, FloatTolerance) &&
			CounterData->ImpactVFX == TemplateData->ImpactVFX &&
			CounterData->SlowMoPostProcessMaterial == TemplateData->SlowMoPostProcessMaterial &&
			FMath::IsNearlyEqual(CounterData->SlowMoPostProcessWeight, TemplateData->SlowMoPostProcessWeight, FloatTolerance) &&
			CounterData->ScreenBloodMaterial == TemplateData->ScreenBloodMaterial &&
			CounterData->bSpawnBloodDecals == TemplateData->bSpawnBloodDecals &&
			FMath::IsNearlyEqual(CounterData->BaseDamage, TemplateData->BaseDamage, FloatTolerance) &&
			FMath::IsNearlyEqual(CounterData->DamageMultiplier, TemplateData->DamageMultiplier, FloatTolerance) &&
			!CounterData->bIsLethal &&
			CounterData->VictimDeathOutcome == TemplateData->VictimDeathOutcome &&
			FMath::IsNearlyEqual(CounterData->RagdollBlendTime, TemplateData->RagdollBlendTime, FloatTolerance);
	}

	UPairedAnimationData* CreateCounterDataAsset(const FString& PackageName, UPairedAnimationData* TemplateData, FString& OutError)
	{
		if (!FPackageName::IsValidLongPackageName(PackageName))
		{
			OutError = FString::Printf(TEXT("CounterData package path is invalid: %s"), *PackageName);
			return nullptr;
		}

		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		if (AssetName.IsEmpty())
		{
			OutError = FString::Printf(TEXT("CounterData package path has no asset name: %s"), *PackageName);
			return nullptr;
		}

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			OutError = FString::Printf(TEXT("Failed to create CounterData package: %s"), *PackageName);
			return nullptr;
		}

		UPairedAnimationData* CounterData = NewObject<UPairedAnimationData>(
			Package,
			*AssetName,
			RF_Public | RF_Standalone | RF_Transactional);
		if (!CounterData)
		{
			OutError = FString::Printf(TEXT("Failed to create CounterData asset: %s"), *PackageName);
			return nullptr;
		}

		CopyPairedDefaults(TemplateData, CounterData, AssetName);
		FAssetRegistryModule::AssetCreated(CounterData);
		CounterData->MarkPackageDirty();
		return CounterData;
	}

	UAnimNotifyState_CounterWindow* FindCounterWindowInSection(const FAttackDataNotifyAnalysis& Analysis)
	{
		if (!Analysis.Montage)
		{
			return nullptr;
		}

		for (const FAnimNotifyEvent& Event : Analysis.Montage->Notifies)
		{
			const float NotifyTime = Event.GetTriggerTime();
			if (!IsTimeInSection(NotifyTime, Analysis.SectionStart, Analysis.SectionEnd))
			{
				continue;
			}

			if (UAnimNotifyState_CounterWindow* CounterWindow = Cast<UAnimNotifyState_CounterWindow>(Event.NotifyStateClass))
			{
				return CounterWindow;
			}
		}

		return nullptr;
	}

	bool CounterWindowMatches(
		const FAttackDataNotifyAnalysis& Analysis,
		const UAttackData* AttackData,
		const UPairedAnimationData* CounterData)
	{
		const UAnimNotifyState_CounterWindow* CounterWindow = FindCounterWindowInSection(Analysis);
		return CounterWindow &&
			AttackData &&
			CounterWindow->AttackType == AttackData->AttackType &&
			CounterWindow->CounterData == CounterData;
	}

	bool AddOrUpdateCounterWindow(
		const FAttackDataNotifyAnalysis& Analysis,
		UAttackData* AttackData,
		UPairedAnimationData* CounterData,
		FKatanaAssetMigrationRow& OutRow)
	{
		if (!Analysis.Montage || !AttackData || !CounterData)
		{
			OutRow.Errors.Add(TEXT("Cannot seed CounterWindow without montage, AttackData, and CounterData"));
			return false;
		}

		UAnimMontage* Montage = Analysis.Montage;
		Montage->Modify();

		if (UAnimNotifyState_CounterWindow* ExistingCounterWindow = FindCounterWindowInSection(Analysis))
		{
			ExistingCounterWindow->Modify();
			ExistingCounterWindow->AttackType = AttackData->AttackType;
			ExistingCounterWindow->CounterData = CounterData;
			Montage->RefreshCacheData();
			Montage->MarkPackageDirty();
			return true;
		}

		const float Duration = FMath::Max(KINDA_SMALL_NUMBER, FMath::Min(Analysis.WindupDuration, Analysis.SectionLength));
		UAnimNotifyState_CounterWindow* CounterWindow = NewObject<UAnimNotifyState_CounterWindow>(Montage);
		CounterWindow->AttackType = AttackData->AttackType;
		CounterWindow->SwingDirection = ESwingDirection::Horizontal;
		CounterWindow->CounterData = CounterData;

		FAnimNotifyEvent Event;
		Event.NotifyStateClass = CounterWindow;
		Event.SetTime(Analysis.SectionStart);
		Event.SetDuration(Duration);
		Event.TriggerTimeOffset = EAnimEventTriggerOffsets::OffsetBefore;
		Event.TrackIndex = 0;
		Montage->Notifies.Add(Event);
		Montage->SortNotifies();
		Montage->RefreshCacheData();
		Montage->MarkPackageDirty();
		return true;
	}
}

bool FCounterChainProofMigrationOperation::ParseTargetSpec(
	const FString& TargetString,
	FCounterChainProofTargetSpec& OutSpec,
	FString& OutError)
{
	OutSpec = FCounterChainProofTargetSpec();
	OutSpec.InputTarget = TargetString;

	TArray<FString> Fields;
	TargetString.ParseIntoArray(Fields, TEXT("|"), false);
	if (Fields.Num() != 3)
	{
		OutError = TEXT("CounterChainProofMigration target must be AttackData|CounterDataPackage|TemplatePairedData");
		return false;
	}

	for (FString& Field : Fields)
	{
		Field.TrimStartAndEndInline();
	}

	if (!FKatanaAssetMigrationRunner::NormalizeAttackDataTargetObjectPath(Fields[0], OutSpec.AttackDataObjectPath, OutError))
	{
		return false;
	}

	if (!FKatanaAssetMigrationRunner::NormalizeContentTargetObjectPath(
			Fields[1],
			OutSpec.CounterDataPackageName,
			OutSpec.CounterDataObjectPath,
			OutError))
	{
		return false;
	}

	if (!FKatanaAssetMigrationRunner::NormalizeContentTargetObjectPath(
			Fields[2],
			OutSpec.TemplatePackageName,
			OutSpec.TemplateObjectPath,
			OutError))
	{
		return false;
	}

	return true;
}

void FCounterChainProofMigrationOperation::SnapshotInitiallyDirtyPackages(
	const TArray<FString>& TargetStrings,
	TSet<FString>& OutDirtyPackages)
{
	for (FString TargetString : TargetStrings)
	{
		TargetString.TrimStartAndEndInline();
		if (TargetString.IsEmpty() || TargetString.StartsWith(TEXT("#")))
		{
			continue;
		}

		FCounterChainProofTargetSpec Spec;
		FString Error;
		if (!ParseTargetSpec(TargetString, Spec, Error))
		{
			continue;
		}

		UAttackData* AttackData = Cast<UAttackData>(
			StaticLoadObject(UAttackData::StaticClass(), nullptr, *Spec.AttackDataObjectPath));
		AddDirtyPackage(AttackData, OutDirtyPackages);
		if (AttackData)
		{
			AddDirtyPackage(AttackData->AttackMontage, OutDirtyPackages);
		}

		UPairedAnimationData* CounterData = Cast<UPairedAnimationData>(
			StaticLoadObject(UPairedAnimationData::StaticClass(), nullptr, *Spec.CounterDataObjectPath));
		AddDirtyPackage(CounterData, OutDirtyPackages);
	}
}

bool FCounterChainProofMigrationOperation::Run(
	const FString& TargetString,
	EKatanaAssetMigrationMode Mode,
	FKatanaAssetMigrationRow& OutRow) const
{
	FCounterChainProofTargetSpec Spec;
	FString Error;
	if (!ParseTargetSpec(TargetString, Spec, Error))
	{
		OutRow = FKatanaAssetMigrationRow();
		OutRow.InputTarget = TargetString;
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors.Add(Error);
		return false;
	}

	UAttackData* AttackData = Cast<UAttackData>(
		StaticLoadObject(UAttackData::StaticClass(), nullptr, *Spec.AttackDataObjectPath));
	UPairedAnimationData* ExistingCounterData = Cast<UPairedAnimationData>(
		StaticLoadObject(UPairedAnimationData::StaticClass(), nullptr, *Spec.CounterDataObjectPath));
	UPairedAnimationData* TemplateData = Cast<UPairedAnimationData>(
		StaticLoadObject(UPairedAnimationData::StaticClass(), nullptr, *Spec.TemplateObjectPath));

	return RunLoadedObjects(Spec, AttackData, ExistingCounterData, TemplateData, Mode, OutRow);
}

bool FCounterChainProofMigrationOperation::RunLoadedObjects(
	const FCounterChainProofTargetSpec& Spec,
	UAttackData* AttackData,
	UPairedAnimationData* ExistingCounterData,
	UPairedAnimationData* TemplateData,
	EKatanaAssetMigrationMode Mode,
	FKatanaAssetMigrationRow& OutRow) const
{
	OutRow = FKatanaAssetMigrationRow();
	OutRow.InputTarget = Spec.InputTarget;
	OutRow.ObjectPath = Spec.AttackDataObjectPath;
	OutRow.CounterData = Spec.CounterDataObjectPath;
	OutRow.CounterDataPackage = Spec.CounterDataPackageName;
	OutRow.TemplatePairedData = Spec.TemplateObjectPath;
	OutRow.AttackData = AttackData ? AttackData->GetPathName() : Spec.AttackDataObjectPath;
	OutRow.AssetClass = AttackData ? AttackData->GetClass()->GetPathName() : FString();
	OutRow.PackageName = AttackData && AttackData->GetOutermost() ? AttackData->GetOutermost()->GetName() : FString();

	if (!AttackData)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors.Add(FString::Printf(TEXT("Target did not load as UAttackData: %s"), *Spec.AttackDataObjectPath));
		return false;
	}

	if (!TemplateData)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors.Add(FString::Printf(TEXT("Template did not load as UPairedAnimationData: %s"), *Spec.TemplateObjectPath));
		return false;
	}

	if (!TemplateData->IsValid())
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors.Add(FString::Printf(TEXT("Template paired data is invalid: %s"), *Spec.TemplateObjectPath));
		return false;
	}

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);
	CopyAnalysisToRow(Analysis, OutRow);
	OutRow.bAttackDataSectionValid = Analysis.bValid;
	if (!Analysis.bValid)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors = Analysis.Errors;
		return false;
	}

	const FString CounterAssetName = FPackageName::GetLongPackageAssetName(Spec.CounterDataPackageName);
	const bool bNeedsCounterDataAsset = !PairedDefaultsMatch(ExistingCounterData, TemplateData, CounterAssetName);
	const bool bNeedsAttackDataLink =
		!AttackData->bHasCounterVariant ||
		AttackData->CounterData != ExistingCounterData ||
		AttackData->CounterData == nullptr;
	const bool bNeedsCounterWindow = !ExistingCounterData || !CounterWindowMatches(Analysis, AttackData, ExistingCounterData);

	if (bNeedsCounterDataAsset)
	{
		OutRow.PlannedAdditions.Add(ExistingCounterData
			? FString::Printf(TEXT("Update CounterData %s from template %s with ReactionType=Counter and bIsLethal=false"),
				*Spec.CounterDataObjectPath,
				*Spec.TemplateObjectPath)
			: FString::Printf(TEXT("Create CounterData %s from template %s with ReactionType=Counter and bIsLethal=false"),
				*Spec.CounterDataObjectPath,
				*Spec.TemplateObjectPath));
		OutRow.ChangedPackages.AddUnique(Spec.CounterDataPackageName);
	}
	if (bNeedsAttackDataLink)
	{
		OutRow.PlannedAdditions.Add(FString::Printf(
			TEXT("Set %s bHasCounterVariant=true and CounterData=%s"),
			*Spec.AttackDataObjectPath,
			*Spec.CounterDataObjectPath));
		AddChangedPackage(AttackData, OutRow);
	}
	if (bNeedsCounterWindow)
	{
		OutRow.PlannedAdditions.Add(FString::Printf(
			TEXT("Seed AnimNotifyState_CounterWindow on %s section %s"),
			*OutRow.Montage,
			*OutRow.Section));
		AddChangedPackage(AttackData->AttackMontage, OutRow);
	}

	const bool bHasChanges = bNeedsCounterDataAsset || bNeedsAttackDataLink || bNeedsCounterWindow;
	if (!bHasChanges)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Unchanged;
		return true;
	}

	if (Mode == EKatanaAssetMigrationMode::Audit || Mode == EKatanaAssetMigrationMode::Plan)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::WouldChange;
		return true;
	}

	UPairedAnimationData* CounterData = ExistingCounterData;
	if (!CounterData)
	{
		FString CreateError;
		CounterData = CreateCounterDataAsset(Spec.CounterDataPackageName, TemplateData, CreateError);
		if (!CounterData)
		{
			OutRow.Status = EKatanaAssetMigrationStatus::Failed;
			OutRow.Errors.Add(CreateError);
			return false;
		}
	}

	if (bNeedsCounterDataAsset)
	{
		CounterData->Modify();
		CopyPairedDefaults(TemplateData, CounterData, CounterAssetName);
		CounterData->MarkPackageDirty();
	}

	if (bNeedsAttackDataLink)
	{
		AttackData->Modify();
		AttackData->bHasCounterVariant = true;
		AttackData->CounterData = CounterData;
		AttackData->MarkPackageDirty();
	}

	const FAttackDataNotifyAnalysis LinkAnalysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);
	if (bNeedsCounterWindow && !AddOrUpdateCounterWindow(LinkAnalysis, AttackData, CounterData, OutRow))
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		return false;
	}

	const FAttackDataNotifyAnalysis FinalAnalysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);
	CopyAnalysisToRow(FinalAnalysis, OutRow);
	OutRow.bAttackDataSectionValid = FinalAnalysis.bValid;
	if (!FinalAnalysis.bValid)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors = FinalAnalysis.Errors;
		return false;
	}

	OutRow.Status = EKatanaAssetMigrationStatus::Changed;
	return true;
}
