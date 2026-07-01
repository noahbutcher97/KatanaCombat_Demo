// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EKatanaAssetMigrationMode : uint8
{
	Audit,
	Plan,
	Apply,
	ApplyAndSave
};

enum class EKatanaAssetMigrationStatus : uint8
{
	Unchanged,
	WouldChange,
	Changed,
	Saved,
	Failed
};

enum class EKatanaAssetMigrationExitCode : int32
{
	Success = 0,
	RowFailure = 1,
	InvalidArguments = 2,
	SaveFailure = 3
};

struct FKatanaAssetMigrationOptions
{
	FString Operation;
	EKatanaAssetMigrationMode Mode = EKatanaAssetMigrationMode::Audit;
	FString TargetsFile;
	FString ReportPath;
	bool bAllowGlobalScan = false;
	bool bAllowPackageSave = false;
	bool bAllowDirtyPackages = false;
	bool bAllowTimingMutation = false;
};

struct FKatanaAssetMigrationRow
{
	FString InputTarget;
	FString PackageName;
	FString ObjectPath;
	FString AssetClass;
	FString AttackData;
	FString Montage;
	FString Section;
	EKatanaAssetMigrationStatus Status = EKatanaAssetMigrationStatus::Unchanged;
	float SectionStart = 0.0f;
	float SectionEnd = 0.0f;
	float SectionLength = 0.0f;
	float WindupDuration = 0.0f;
	float ActiveDuration = 0.0f;
	float RecoveryDuration = 0.0f;
	float TimingTotal = 0.0f;
	float HoldWindowStart = 0.0f;
	float ProposedWindupDuration = 0.0f;
	float ProposedActiveDuration = 0.0f;
	float ProposedRecoveryDuration = 0.0f;
	float ProposedTimingTotal = 0.0f;
	TArray<FString> LegacyNotifiesFound;
	TArray<FString> StaleCanonicalNotifiesFound;
	TArray<FString> CanonicalNotifiesMissing;
	TArray<FString> BranchReadinessWarnings;
	TArray<FString> PlannedRemovals;
	TArray<FString> PlannedAdditions;
	TArray<FString> ChangedPackages;
	TArray<FString> SavedPackages;
	TArray<FString> Warnings;
	TArray<FString> Errors;
	bool bPackageFileExists = false;
	bool bLoaded = false;
	bool bMapLoaded = false;
	bool bAttackDataSectionValid = false;
	bool bPairedAnimationValid = false;
	bool bPairedAttackerSectionValid = false;
	bool bPairedVictimSectionValid = false;
	bool bHasParryWindow = false;
	bool bHasCounterWindow = false;
	bool bCounterVariantHasData = false;
	bool bFinisherHasData = false;
};

struct FKatanaAssetMigrationSummary
{
	int32 Targets = 0;
	int32 WouldChange = 0;
	int32 Changed = 0;
	int32 Unchanged = 0;
	int32 Failed = 0;
	int32 Saved = 0;
};

struct FKatanaAssetMigrationReport
{
	int32 SchemaVersion = 1;
	FString Operation;
	EKatanaAssetMigrationMode Mode = EKatanaAssetMigrationMode::Audit;
	FKatanaAssetMigrationSummary Summary;
	TArray<FKatanaAssetMigrationRow> Rows;
};

inline FString LexToString(EKatanaAssetMigrationMode Mode)
{
	switch (Mode)
	{
	case EKatanaAssetMigrationMode::Audit:
		return TEXT("Audit");
	case EKatanaAssetMigrationMode::Plan:
		return TEXT("Plan");
	case EKatanaAssetMigrationMode::Apply:
		return TEXT("Apply");
	case EKatanaAssetMigrationMode::ApplyAndSave:
		return TEXT("ApplyAndSave");
	}

	return TEXT("Audit");
}

inline FString LexToString(EKatanaAssetMigrationStatus Status)
{
	switch (Status)
	{
	case EKatanaAssetMigrationStatus::Unchanged:
		return TEXT("Unchanged");
	case EKatanaAssetMigrationStatus::WouldChange:
		return TEXT("WouldChange");
	case EKatanaAssetMigrationStatus::Changed:
		return TEXT("Changed");
	case EKatanaAssetMigrationStatus::Saved:
		return TEXT("Saved");
	case EKatanaAssetMigrationStatus::Failed:
		return TEXT("Failed");
	}

	return TEXT("Failed");
}

inline bool TryParseKatanaAssetMigrationMode(const FString& Value, EKatanaAssetMigrationMode& OutMode)
{
	if (Value.Equals(TEXT("Audit"), ESearchCase::IgnoreCase))
	{
		OutMode = EKatanaAssetMigrationMode::Audit;
		return true;
	}
	if (Value.Equals(TEXT("Plan"), ESearchCase::IgnoreCase))
	{
		OutMode = EKatanaAssetMigrationMode::Plan;
		return true;
	}
	if (Value.Equals(TEXT("Apply"), ESearchCase::IgnoreCase))
	{
		OutMode = EKatanaAssetMigrationMode::Apply;
		return true;
	}
	if (Value.Equals(TEXT("ApplyAndSave"), ESearchCase::IgnoreCase))
	{
		OutMode = EKatanaAssetMigrationMode::ApplyAndSave;
		return true;
	}

	return false;
}
