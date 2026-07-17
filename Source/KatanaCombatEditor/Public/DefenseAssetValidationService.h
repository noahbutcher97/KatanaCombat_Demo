// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAnimMontage;
class UAttackData;
class UDefenseConfiguration;
class UPairedAnimationData;
class UObject;

enum class EDefenseAssetValidationSeverity : uint8
{
	Info,
	Warning,
	Error
};

struct FDefenseAssetValidationFinding
{
	EDefenseAssetValidationSeverity Severity = EDefenseAssetValidationSeverity::Error;
	FString Code;
	FString Context;
	FString Message;
};

struct FDefenseAssetValidationRow
{
	FString Kind;
	FString Name;
	FString AssetPath;
	TMap<FString, FString> Facts;
};

struct FDefenseRootMotionMeasurement
{
	double HorizontalTranslation = 0.0;
	double TotalYaw = 0.0;
	double MaximumYawRate = 0.0;
	double StartSeconds = 0.0;
	double EndSeconds = 0.0;
	int32 SampleCount = 0;
};

struct KATANACOMBATEDITOR_API FDefenseAssetValidationResult
{
	TArray<FDefenseAssetValidationFinding> Findings;
	TArray<FDefenseAssetValidationRow> Rows;
	TArray<FString> Dependencies;

	bool HasErrors() const;
	bool HasFinding(const FString& Code) const;
	TArray<FDefenseAssetValidationRow> FindRows(const FString& Kind) const;
	void AddFinding(
		EDefenseAssetValidationSeverity Severity,
		const FString& Code,
		const FString& Context,
		const FString& Message);
};

struct FDefenseProofParryWindow
{
	bool bPresent = false;
	FString Basis;
	double StartSeconds = 0.0;
	double EndSeconds = 0.0;
	bool bReviewed = false;
};

struct FDefenseProofFixture
{
	FString PlayerBlueprint;
	FString PlayerCombatSettings;
	TArray<FString> EnemyBlueprints;
	TArray<FString> EnemyCombatSettings;
	FString InputAction;
	FString InputMappingContext;
	FString BlockKey;
	FString GuardAnimBlueprint;
	bool bReviewed = false;
};

struct FDefenseProofAttackEntry
{
	FString Name;
	FString AttackData;
	FString Montage;
	FString Section;
	FString ExpectedHeight;
	FString ExpectedLane;
	FString ExpectedSwing;
	FString ExpectedSourceSocket;
	FString ExpectedTargetBone;
	bool bRequiresBlockedImpactAudio = false;
	bool bRequiresBlockedImpactVFX = false;
	TArray<FString> ExpectedTags;
	FDefenseProofParryWindow ParryWindow;
};

struct FDefenseProofPresentationEntry
{
	FString Name;
	FString Outcome;
	FString AttackerResponse;
	FString DefenderRow;
	bool bHasDefenderRow = false;
	FString AttackerRow;
	bool bHasAttackerRow = false;
	bool bRequiresDefenderMontage = false;
	bool bRequiresAttackerMontage = false;
	bool bRequiresImpactAudio = false;
	bool bRequiresImpactVFX = false;
	FString ExpectedSourceSocket;
	FString ExpectedTargetBone;
	bool bReviewed = false;
};

struct FDefenseProofPairedDependencyEntry
{
	FString Name;
	FString Role;
	FString PairedData;
	FString AttackerMontage;
	FString AttackerSection;
	FString VictimMontage;
	FString VictimSection;
	FString DriverRole;
	bool bHasDriverRole = false;
	FString DriverMarker;
	bool bHasDriverMarker = false;
	FString AttackerWarpTarget;
	FString VictimWarpTarget;
	FString AttackerReadySection;
	bool bHasAttackerReadySection = false;
	FString VictimReadySection;
	bool bHasVictimReadySection = false;
	bool bAttackerTerminalPoseCompatible = false;
	bool bVictimTerminalPoseCompatible = false;
	bool bReviewed = false;
};

struct FDefenseProofExpectedCaseEntry
{
	FString Name;
	FString Attack;
	FString Outcome;
	FString Reason;
	FString AttackerResponse;
	FString Presentation;
	bool bHasPresentation = false;
	TArray<FString> PairedDependencies;
	bool bReviewed = false;
};

struct FDefenseProofManifest
{
	int32 SchemaVersion = 0;
	FString Gate;
	FString Map;
	FString DefenseConfiguration;
	FDefenseProofFixture Fixture;
	TArray<FString> CombatSettings;
	TArray<FString> SupportingAssets;
	TArray<FDefenseProofAttackEntry> Attacks;
	TArray<FDefenseProofPresentationEntry> Presentations;
	TArray<FDefenseProofPairedDependencyEntry> PairedDependencies;
	TArray<FDefenseProofExpectedCaseEntry> ExpectedCases;
	FString SourcePath;
};

/** Injectable object-path table used by tests and by the explicit production loader. */
struct KATANACOMBATEDITOR_API FDefenseProofAssetSet
{
	TMap<FString, UObject*> Objects;

	void Add(const FString& ObjectPath, UObject* Object);
	UObject* Find(const FString& ObjectPath) const;
};

/** Strict parser and read-only inventory service for reviewed defense proof manifests. */
class KATANACOMBATEDITOR_API FDefenseAssetValidationService
{
public:
	static bool ParseManifestJson(
		const FString& Json,
		FDefenseProofManifest& OutManifest,
		TArray<FString>& OutErrors);

	static bool LoadManifestFile(
		const FString& ManifestPath,
		FDefenseProofManifest& OutManifest,
		TArray<FString>& OutErrors);

	/** Returns the unique, sorted object paths explicitly declared by the manifest. */
	static TArray<FString> CollectExplicitObjectPaths(const FDefenseProofManifest& Manifest);

	/** Loads only explicitly declared manifest paths. Validation reports missing paths separately. */
	static void LoadExplicitObjects(
		const FDefenseProofManifest& Manifest,
		FDefenseProofAssetSet& OutAssets);

	/** Validates the complete manifest graph against an injected or explicitly loaded object set. */
	static void ValidateManifestObjects(
		const FDefenseProofManifest& Manifest,
		const FDefenseProofAssetSet& Assets,
		FDefenseAssetValidationResult& OutResult);

	static void ValidateAttackEntry(
		const FDefenseProofAttackEntry& Entry,
		const UAttackData* AttackData,
		const UAnimMontage* Montage,
		FDefenseAssetValidationResult& OutResult);

	static void ValidatePresentationEntry(
		const FDefenseProofPresentationEntry& Entry,
		const FDefenseProofAttackEntry& AttackEntry,
		const UAttackData* AttackData,
		const UDefenseConfiguration* DefenderConfiguration,
		const UDefenseConfiguration* AttackerConfiguration,
		FDefenseAssetValidationResult& OutResult);

	static void ValidatePairedDependency(
		const FDefenseProofPairedDependencyEntry& Entry,
		const UPairedAnimationData* PairedData,
		const UDefenseConfiguration* Configuration,
		FDefenseAssetValidationResult& OutResult,
		const UAnimMontage* ManifestAttackerMontage = nullptr,
		const UAnimMontage* ManifestVictimMontage = nullptr);

	static void ValidatePairedSequence(
		const TArray<FDefenseProofPairedDependencyEntry>& Entries,
		const TArray<const UPairedAnimationData*>& PairedAssets,
		FDefenseAssetValidationResult& OutResult);

	static bool MeasureRootMotion(
		const UAnimMontage* Montage,
		FName Section,
		FDefenseRootMotionMeasurement& OutMeasurement,
		FString& OutError);

	static void ValidateRootMotionBudget(
		const FString& Context,
		const FDefenseRootMotionMeasurement& Measurement,
		double MaximumHorizontalTranslation,
		double MaximumYawRate,
		FDefenseAssetValidationResult& OutResult);
};
