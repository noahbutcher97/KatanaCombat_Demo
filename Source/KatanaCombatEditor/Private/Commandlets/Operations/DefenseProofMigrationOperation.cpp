// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/Operations/DefenseProofMigrationOperation.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyState_ParryWindow.h"
#include "Characters/BaseCombatCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Commandlets/KatanaAssetMigrationRunner.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Data/DefenseConfiguration.h"
#include "Data/PairedAnimationData.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"

const FString FDefenseProofMigrationOperation::OperationName = TEXT("DefenseProofMigration");

namespace
{
using FCanonicalJsonWriter = TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

void WriteCanonicalValue(
	const TSharedPtr<FJsonValue>& Value,
	const TSharedRef<FCanonicalJsonWriter>& Writer)
{
	if (!Value.IsValid())
	{
		Writer->WriteNull();
		return;
	}

	switch (Value->Type)
	{
	case EJson::Object:
	{
		Writer->WriteObjectStart();
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		TArray<FString> Keys;
		Object->Values.GetKeys(Keys);
		Keys.Sort();
		for (const FString& Key : Keys)
		{
			Writer->WriteIdentifierPrefix(Key);
			WriteCanonicalValue(Object->Values.FindChecked(Key), Writer);
		}
		Writer->WriteObjectEnd();
		break;
	}
	case EJson::Array:
		Writer->WriteArrayStart();
		for (const TSharedPtr<FJsonValue>& Element : Value->AsArray())
		{
			WriteCanonicalValue(Element, Writer);
		}
		Writer->WriteArrayEnd();
		break;
	case EJson::String:
		Writer->WriteValue(Value->AsString());
		break;
	case EJson::Number:
		Writer->WriteValue(Value->AsNumber());
		break;
	case EJson::Boolean:
		Writer->WriteValue(Value->AsBool());
		break;
	case EJson::Null:
	default:
		Writer->WriteNull();
		break;
	}
}

void AppendFingerprintField(FString& Buffer, const FString& Name, const FString& Value)
{
	Buffer += Name;
	Buffer += TEXT(":");
	Buffer += LexToString(Value.Len());
	Buffer += TEXT(":");
	Buffer += Value;
	Buffer += TEXT("\n");
}

FString ObjectPathOrNone(const UObject* Object)
{
	return Object ? Object->GetPathName() : TEXT("None");
}

FString PackageNameForObjectOrPath(const UObject* Object, const FString& ObjectPath)
{
	if (Object && Object->GetOutermost())
	{
		return Object->GetOutermost()->GetName();
	}
	int32 DotIndex = INDEX_NONE;
	return ObjectPath.FindChar(TCHAR('.'), DotIndex) ? ObjectPath.Left(DotIndex) : ObjectPath;
}

UClass* ResolveObjectClass(UObject* Object)
{
	if (UClass* Class = Cast<UClass>(Object))
	{
		return Class;
	}
	if (const UBlueprint* Blueprint = Cast<UBlueprint>(Object))
	{
		return Blueprint->GeneratedClass;
	}
	return nullptr;
}

void AddLedgerRole(FKatanaAssetMigrationPackageLedgerEntry& Entry, const FString& Role)
{
	TArray<FString> Roles;
	Entry.PackageRole.ParseIntoArray(Roles, TEXT(","), true);
	Roles.AddUnique(Role);
	Roles.Sort();
	Entry.PackageRole = FString::Join(Roles, TEXT(","));
}

void AddPlannedChange(
	FDefenseProofMigrationPlan& Plan,
	const FString& Description,
	const FString& Role,
	const UObject* Object,
	const FString& ObjectPath,
	const bool bTimingMutation = false)
{
	Plan.ProposedChanges.AddUnique(Description);
	Plan.bRequiresTimingMutation |= bTimingMutation;
	const FString PackageName = PackageNameForObjectOrPath(Object, ObjectPath);
	if (PackageName.IsEmpty())
	{
		return;
	}
	FKatanaAssetMigrationPackageLedgerEntry* Ledger = Plan.PackageLedger.FindByPredicate(
		[&PackageName](const FKatanaAssetMigrationPackageLedgerEntry& Entry)
		{
			return Entry.PackageName == PackageName;
		});
	if (!Ledger)
	{
		FKatanaAssetMigrationPackageLedgerEntry Entry;
		Entry.PackageName = PackageName;
		Entry.PackageRole = Role;
		Entry.bInitiallyDirty = Object && Object->GetOutermost()
			? Object->GetOutermost()->IsDirty()
			: false;
		Entry.PlannedAction = TEXT("Modify");
		Plan.PackageLedger.Add(MoveTemp(Entry));
	}
	else
	{
		AddLedgerRole(*Ledger, Role);
		Ledger->bInitiallyDirty |= Object && Object->GetOutermost()
			? Object->GetOutermost()->IsDirty()
			: false;
	}
}

TArray<FString> SortedTagNames(const FGameplayTagContainer& Container)
{
	TArray<FGameplayTag> Tags;
	Container.GetGameplayTagArray(Tags);
	TArray<FString> Names;
	for (const FGameplayTag& Tag : Tags)
	{
		Names.Add(Tag.ToString());
	}
	Names.Sort();
	return Names;
}

bool AttackTagsMatch(const UAttackData* AttackData, const FDefenseProofAttackEntry& Entry)
{
	if (!AttackData)
	{
		return false;
	}
	TArray<FString> Expected = Entry.ExpectedTags;
	Expected.Sort();
	return SortedTagNames(AttackData->AttackTags) == Expected;
}

bool GetSectionBounds(
	const UAnimMontage* Montage,
	const FName Section,
	float& OutStart,
	float& OutEnd)
{
	if (!Montage)
	{
		return false;
	}
	const int32 SectionIndex = Montage->GetSectionIndex(Section);
	if (SectionIndex == INDEX_NONE)
	{
		return false;
	}
	Montage->GetSectionStartAndEndTime(SectionIndex, OutStart, OutEnd);
	return OutEnd > OutStart;
}

bool ParryWindowMatches(
	const UAnimMontage* Montage,
	const FDefenseProofAttackEntry& Entry)
{
	float SectionStart = 0.0f;
	float SectionEnd = 0.0f;
	if (!GetSectionBounds(Montage, FName(*Entry.Section), SectionStart, SectionEnd))
	{
		return false;
	}
	int32 MatchingWindowCount = 0;
	int32 CrossingWindowCount = 0;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (!Event.NotifyStateClass
			|| !Event.NotifyStateClass->IsA(UAnimNotifyState_ParryWindow::StaticClass()))
		{
			continue;
		}
		const double Start = Event.GetTriggerTime();
		const double End = Start + Event.GetDuration();
		const bool bInside = Start >= SectionStart - KINDA_SMALL_NUMBER
			&& End <= SectionEnd + KINDA_SMALL_NUMBER;
		const bool bOverlaps = End > SectionStart + KINDA_SMALL_NUMBER
			&& Start < SectionEnd - KINDA_SMALL_NUMBER;
		if (bInside)
		{
			++MatchingWindowCount;
			if (!Entry.ParryWindow.bPresent
				|| !FMath::IsNearlyEqual(Start - SectionStart,
					Entry.ParryWindow.StartSeconds, 0.001)
				|| !FMath::IsNearlyEqual(End - SectionStart,
					Entry.ParryWindow.EndSeconds, 0.001))
			{
				return false;
			}
		}
		else if (bOverlaps)
		{
			++CrossingWindowCount;
		}
	}
	return CrossingWindowCount == 0
		&& MatchingWindowCount == (Entry.ParryWindow.bPresent ? 1 : 0);
}

EPairedReactionType ReactionForRole(const FString& Role)
{
	return Role == TEXT("Bridge")
		? EPairedReactionType::Parry
		: Role == TEXT("Counter")
			? EPairedReactionType::Counter
			: EPairedReactionType::Finisher;
}

bool PairedDefinitionMatches(
	const UPairedAnimationData* Data,
	const FDefenseProofPairedDependencyEntry& Entry,
	const FDefenseProofAssetSet& Assets)
{
	if (!Data)
	{
		return false;
	}
	const UAnimMontage* AttackerMontage = Cast<UAnimMontage>(Assets.Find(Entry.AttackerMontage));
	const UAnimMontage* VictimMontage = Cast<UAnimMontage>(Assets.Find(Entry.VictimMontage));
	const EPairedAnimationRole DriverRole = Entry.DriverRole == TEXT("Victim")
		? EPairedAnimationRole::Victim
		: EPairedAnimationRole::Attacker;
	const FPairedChainTransitionPolicy& Policy = Data->ChainTransitionPolicy;
	return Data->ReactionType == ReactionForRole(Entry.Role)
		&& Data->AttackerMontage == AttackerMontage
		&& Data->VictimMontage == VictimMontage
		&& Data->AttackerMontageSection == FName(*Entry.AttackerSection)
		&& Data->VictimMontageSection == FName(*Entry.VictimSection)
		&& Data->AttackerWarpConfig.WarpTargetName == FName(*Entry.AttackerWarpTarget)
		&& Data->VictimWarpConfig.WarpTargetName == FName(*Entry.VictimWarpTarget)
		&& Data->AttackerWarpConfig.bWarpRotation
		&& Data->VictimWarpConfig.bWarpRotation
		&& (Entry.Role == TEXT("Finisher")
			? Policy.RequiredMarker.IsNone() && !Policy.bAutoContinue
			: Policy.DriverRole == DriverRole
				&& Policy.RequiredMarker == FName(*Entry.DriverMarker)
				&& Policy.bAutoContinue == (Entry.Role == TEXT("Counter"))
				&& Policy.AttackerReadySection == (Entry.bHasAttackerReadySection
					? FName(*Entry.AttackerReadySection) : NAME_None)
				&& Policy.VictimReadySection == (Entry.bHasVictimReadySection
					? FName(*Entry.VictimReadySection) : NAME_None)
				&& Policy.bAttackerTerminalPoseCompatible == Entry.bAttackerTerminalPoseCompatible
				&& Policy.bVictimTerminalPoseCompatible == Entry.bVictimTerminalPoseCompatible);
}

void AppendPackageByteFact(
	const FString& ObjectPath,
	const UObject* Object,
	TArray<FString>& OutFacts,
	TArray<FString>& OutErrors)
{
	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	const UPackage* ObjectPackage = Object ? Object->GetOutermost() : nullptr;
	const UPackage* LoadedPackage = FindPackage(nullptr, *PackageName);
	const bool bObjectUsesDeclaredPackage = ObjectPackage
		&& ObjectPackage->GetName() == PackageName;
	const bool bDirty = (LoadedPackage && LoadedPackage->IsDirty())
		|| (bObjectUsesDeclaredPackage && ObjectPackage->IsDirty());
	if (bDirty)
	{
		OutErrors.Add(FString::Printf(
			TEXT("approval package has unsaved in-memory changes: %s"), *PackageName));
	}

	FString Filename;
	if (!FPackageName::DoesPackageExist(PackageName, &Filename))
	{
		OutFacts.Add(FString::Printf(TEXT("package|%s|missing|dirty=%s"),
			*ObjectPath, *LexToString(bDirty)));
		if (bObjectUsesDeclaredPackage)
		{
			OutErrors.Add(FString::Printf(
				TEXT("approval package has no readable on-disk file: %s"), *PackageName));
		}
		return;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
	{
		OutFacts.Add(FString::Printf(TEXT("package|%s|unreadable|dirty=%s"),
			*ObjectPath, *LexToString(bDirty)));
		OutErrors.Add(FString::Printf(
			TEXT("approval package could not be hashed: %s"), *PackageName));
		return;
	}

	const FString FileHash = FSHA1::HashBuffer(
		Bytes.GetData(), static_cast<uint64>(Bytes.Num())).ToString();
	OutFacts.Add(FString::Printf(TEXT("package|%s|sha1=%s|size=%d|dirty=%s"),
		*ObjectPath, *FileHash, Bytes.Num(), *LexToString(bDirty)));
}

void AddValidationFacts(
	const FDefenseProofManifest& Manifest,
	const FDefenseProofAssetSet& Assets,
	const FDefenseAssetValidationResult& Validation,
	FString& OutFacts,
	TArray<FString>& OutErrors)
{
	TArray<FString> Facts;
	for (const FString& Path : FDefenseAssetValidationService::CollectExplicitObjectPaths(Manifest))
	{
		const UObject* Object = Assets.Find(Path);
		AppendPackageByteFact(Path, Object, Facts, OutErrors);
		Facts.Add(FString::Printf(TEXT("object|%s|%s|dirty=%s"), *Path,
			Object ? *Object->GetClass()->GetPathName() : TEXT("Missing"),
			Object && Object->GetOutermost()
				? *LexToString(Object->GetOutermost()->IsDirty()) : TEXT("false")));
	}
	for (const FString& Path : Manifest.CombatSettings)
	{
		const UCombatSettings* Settings = Cast<UCombatSettings>(Assets.Find(Path));
		Facts.Add(FString::Printf(TEXT("settings|%s|defense=%s"),
			*Path, Settings ? *ObjectPathOrNone(Settings->DefenseConfiguration) : TEXT("Missing")));
	}
	if (const UInputAction* Action = Cast<UInputAction>(Assets.Find(Manifest.Fixture.InputAction)))
	{
		Facts.Add(FString::Printf(TEXT("input_action|%s|type=%d"),
			*Manifest.Fixture.InputAction, static_cast<int32>(Action->ValueType)));
	}
	if (const UInputMappingContext* Context =
		Cast<UInputMappingContext>(Assets.Find(Manifest.Fixture.InputMappingContext)))
	{
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			Facts.Add(FString::Printf(TEXT("input_mapping|%s|%s|%s"),
				*Manifest.Fixture.InputMappingContext,
				*ObjectPathOrNone(Mapping.Action.Get()),
				*Mapping.Key.ToString()));
		}
	}
	if (UClass* PlayerClass = ResolveObjectClass(Assets.Find(Manifest.Fixture.PlayerBlueprint)))
	{
		if (const APlayerCharacter* Player = Cast<APlayerCharacter>(PlayerClass->GetDefaultObject()))
		{
			Facts.Add(FString::Printf(
				TEXT("player|%s|settings=%s|action=%s|context=%s|anim=%s"),
				*Manifest.Fixture.PlayerBlueprint,
				*ObjectPathOrNone(Player->CombatSettings),
				*ObjectPathOrNone(Player->BlockAction),
				*ObjectPathOrNone(Player->DefaultMappingContext),
				Player->GetMesh() && Player->GetMesh()->GetAnimClass()
					? *Player->GetMesh()->GetAnimClass()->GetPathName() : TEXT("None")));
		}
	}
	for (const FString& EnemyPath : Manifest.Fixture.EnemyBlueprints)
	{
		if (UClass* EnemyClass = ResolveObjectClass(Assets.Find(EnemyPath)))
		{
			if (const ABaseCombatCharacter* Enemy =
				Cast<ABaseCombatCharacter>(EnemyClass->GetDefaultObject()))
			{
				Facts.Add(FString::Printf(TEXT("enemy|%s|settings=%s"),
					*EnemyPath, *ObjectPathOrNone(Enemy->CombatSettings)));
			}
		}
	}
	for (const FDefenseProofAttackEntry& Entry : Manifest.Attacks)
	{
		const UAttackData* Attack = Cast<UAttackData>(Assets.Find(Entry.AttackData));
		if (!Attack)
		{
			continue;
		}
		Facts.Add(FString::Printf(
			TEXT("attack|%s|montage=%s|section=%s|height=%d|lane=%d|swing=%d|socket=%s|bone=%s|counter=%s|hascounter=%s|finisher=%s|hasfinisher=%s|tags=%s"),
			*Entry.AttackData, *ObjectPathOrNone(Attack->AttackMontage),
			*Attack->MontageSection.ToString(), static_cast<int32>(Attack->DefenseProfile.Height),
			static_cast<int32>(Attack->DefenseProfile.NominalLane),
			static_cast<int32>(Attack->DefenseProfile.SwingShape),
			*Attack->DefenseProfile.SourceContactSocketOverride.ToString(),
			*Attack->GetDefenseTargetBoneFallback().ToString(),
			*ObjectPathOrNone(Attack->CounterData), *LexToString(Attack->bHasCounterVariant),
			*ObjectPathOrNone(Attack->FinisherData), *LexToString(Attack->bCanTriggerFinisher),
			*FString::Join(SortedTagNames(Attack->AttackTags), TEXT(","))));
		const UAnimMontage* Montage = Cast<UAnimMontage>(Assets.Find(Entry.Montage));
		if (Montage)
		{
			for (const FAnimNotifyEvent& Event : Montage->Notifies)
			{
				if (Event.NotifyStateClass
					&& Event.NotifyStateClass->IsA(UAnimNotifyState_ParryWindow::StaticClass()))
				{
					Facts.Add(FString::Printf(TEXT("parry_window|%s|%.6f|%.6f"),
						*Entry.Montage, Event.GetTriggerTime(), Event.GetDuration()));
				}
			}
		}
	}
	for (const FDefenseProofPairedDependencyEntry& Entry : Manifest.PairedDependencies)
	{
		const UPairedAnimationData* Data = Cast<UPairedAnimationData>(Assets.Find(Entry.PairedData));
		if (!Data)
		{
			continue;
		}
		Facts.Add(FString::Printf(
			TEXT("paired|%s|role=%d|attacker=%s|victim=%s|asection=%s|vsection=%s|awarp=%s|arotation=%s|vwarp=%s|vrotation=%s|driver=%d|marker=%s|auto=%s|aready=%s|vready=%s|aterminal=%s|vterminal=%s"),
			*Entry.PairedData, static_cast<int32>(Data->ReactionType),
			*ObjectPathOrNone(Data->AttackerMontage), *ObjectPathOrNone(Data->VictimMontage),
			*Data->AttackerMontageSection.ToString(), *Data->VictimMontageSection.ToString(),
			*Data->AttackerWarpConfig.WarpTargetName.ToString(),
			*LexToString(Data->AttackerWarpConfig.bWarpRotation),
			*Data->VictimWarpConfig.WarpTargetName.ToString(),
			*LexToString(Data->VictimWarpConfig.bWarpRotation),
			static_cast<int32>(Data->ChainTransitionPolicy.DriverRole),
			*Data->ChainTransitionPolicy.RequiredMarker.ToString(),
			*LexToString(Data->ChainTransitionPolicy.bAutoContinue),
			*Data->ChainTransitionPolicy.AttackerReadySection.ToString(),
			*Data->ChainTransitionPolicy.VictimReadySection.ToString(),
			*LexToString(Data->ChainTransitionPolicy.bAttackerTerminalPoseCompatible),
			*LexToString(Data->ChainTransitionPolicy.bVictimTerminalPoseCompatible)));
	}
	for (const FDefenseAssetValidationRow& Row : Validation.Rows)
	{
		Facts.Add(FString::Printf(TEXT("validation_row|%s|%s|%s"),
			*Row.Kind, *Row.Name, *Row.AssetPath));
		TArray<FString> Keys;
		Row.Facts.GetKeys(Keys);
		Keys.Sort();
		for (const FString& Key : Keys)
		{
			Facts.Add(FString::Printf(TEXT("validation_fact|%s|%s|%s|%s"),
				*Row.Kind, *Row.Name, *Key, *Row.Facts.FindChecked(Key)));
		}
	}
	for (const FDefenseAssetValidationFinding& Finding : Validation.Findings)
	{
		Facts.Add(FString::Printf(TEXT("finding|%d|%s|%s|%s"),
			static_cast<int32>(Finding.Severity), *Finding.Code, *Finding.Context, *Finding.Message));
	}
	Facts.Sort();
	OutFacts = FString::Join(Facts, TEXT("\n"));
}

void MarkObjectChanged(UObject* Object, TSet<FString>& OutChangedPackages)
{
	if (!Object)
	{
		return;
	}
	Object->MarkPackageDirty();
	if (Object->GetOutermost())
	{
		OutChangedPackages.Add(Object->GetOutermost()->GetName());
	}
}

void MarkBlueprintDefaultsChanged(UObject* BlueprintObject, TSet<FString>& OutChangedPackages)
{
	if (UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintObject))
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
	MarkObjectChanged(BlueprintObject, OutChangedPackages);
}

bool IsCorrectableFinding(const FString& Code)
{
	static const TSet<FString> CorrectableCodes = {
		TEXT("DefenseConfigurationAssignmentMismatch"),
		TEXT("BlockInputActionTypeMismatch"),
		TEXT("BlockInputMappingMissing"),
		TEXT("DeprecatedBlockInputMapping"),
		TEXT("PlayerBlockInputAssignmentMismatch"),
		TEXT("PlayerCombatSettingsAssignmentMismatch"),
		TEXT("PlayerGuardAnimAssignmentMismatch"),
		TEXT("EnemyCombatSettingsAssignmentMismatch"),
		TEXT("AttackMontageMismatch"),
		TEXT("AttackSectionMismatch"),
		TEXT("AttackProfileMismatch"),
		TEXT("CounterReferenceMismatch"),
		TEXT("FinisherReferenceMismatch"),
		TEXT("AttackTagsMismatch"),
		TEXT("ParryWindowCardinality"),
		TEXT("ParryWindowOutsideSection"),
		TEXT("ParryWindowTimingMismatch"),
		TEXT("TagWindowMismatch"),
		TEXT("PairedRoleMismatch"),
		TEXT("PairedMontageSectionMismatch"),
		TEXT("PairedDriverPolicyMismatch"),
		TEXT("CounterAutoContinueMissing"),
		TEXT("BridgeAutoContinueUnexpected"),
		TEXT("PairedReadyPoseMissing"),
		TEXT("PairedReadyPoseMismatch"),
		TEXT("PairedWarpTargetMismatch"),
		TEXT("PairedRotationWarpConfigMismatch"),
		TEXT("FinisherMarkerUnexpected"),
		TEXT("AttackCounterReferenceMismatch"),
		TEXT("AttackFinisherReferenceMismatch")};
	return CorrectableCodes.Contains(Code);
}

bool HasUncorrectableErrors(const FDefenseProofMigrationPlan& Plan)
{
	return Plan.Validation.Findings.ContainsByPredicate(
		[](const FDefenseAssetValidationFinding& Finding)
		{
			return Finding.Severity == EDefenseAssetValidationSeverity::Error
				&& !IsCorrectableFinding(Finding.Code);
		});
}

bool ChangeMentionsRow(
	const FString& Change,
	const FDefenseAssetValidationRow& ValidationRow)
{
	return (!ValidationRow.Name.IsEmpty() && Change.Contains(ValidationRow.Name))
		|| (!ValidationRow.AssetPath.IsEmpty() && Change.Contains(ValidationRow.AssetPath));
}

void PopulateReportFromPlan(
	const FString& ManifestPath,
	const FDefenseProofMigrationPlan& Plan,
	const EKatanaAssetMigrationMode Mode,
	const TSet<FString>* ChangedPackages,
	FKatanaAssetMigrationReport& OutReport)
{
	OutReport = FKatanaAssetMigrationReport();
	OutReport.SchemaVersion = 2;
	OutReport.Operation = FDefenseProofMigrationOperation::OperationName;
	OutReport.Mode = Mode;
	OutReport.ManifestPath = ManifestPath;
	OutReport.Gate = Plan.Manifest.Gate;
	OutReport.PlanFingerprint = Plan.Fingerprint;
	OutReport.PackageLedger = Plan.PackageLedger;

	const bool bUncorrectable = HasUncorrectableErrors(Plan);
	FKatanaAssetMigrationRow ManifestRow;
	ManifestRow.InputTarget = ManifestPath;
	ManifestRow.ObjectPath = ManifestPath;
	ManifestRow.AssetClass = TEXT("DefenseProofManifest");
	ManifestRow.Details.Add(TEXT("kind"), TEXT("Manifest"));
	ManifestRow.Details.Add(TEXT("gate"), Plan.Manifest.Gate);
	ManifestRow.Details.Add(TEXT("requires_timing_mutation"),
		LexToString(Plan.bRequiresTimingMutation));
	ManifestRow.PlannedAdditions = Plan.ProposedChanges;
	for (const FDefenseAssetValidationFinding& Finding : Plan.Validation.Findings)
	{
		const FString Message = FString::Printf(TEXT("%s|%s|%s"),
			*Finding.Code, *Finding.Context, *Finding.Message);
		if (Finding.Severity == EDefenseAssetValidationSeverity::Error
			&& !IsCorrectableFinding(Finding.Code))
		{
			ManifestRow.Errors.Add(Message);
		}
		else
		{
			ManifestRow.Warnings.Add(Message);
		}
	}
	if (bUncorrectable)
	{
		ManifestRow.Status = EKatanaAssetMigrationStatus::Failed;
	}
	else if (Mode == EKatanaAssetMigrationMode::Apply
		|| Mode == EKatanaAssetMigrationMode::ApplyAndSave)
	{
		ManifestRow.Status = ChangedPackages && !ChangedPackages->IsEmpty()
			? EKatanaAssetMigrationStatus::Changed
			: EKatanaAssetMigrationStatus::Unchanged;
	}
	else
	{
		ManifestRow.Status = Plan.ProposedChanges.IsEmpty()
			? EKatanaAssetMigrationStatus::Unchanged
			: EKatanaAssetMigrationStatus::WouldChange;
	}
	if (ChangedPackages)
	{
		ManifestRow.ChangedPackages = ChangedPackages->Array();
		ManifestRow.ChangedPackages.Sort();
	}
	OutReport.Rows.Add(MoveTemp(ManifestRow));

	for (const FDefenseAssetValidationRow& ValidationRow : Plan.Validation.Rows)
	{
		FKatanaAssetMigrationRow Row;
		Row.InputTarget = ManifestPath;
		Row.ObjectPath = ValidationRow.AssetPath;
		Row.PackageName = PackageNameForObjectOrPath(nullptr, ValidationRow.AssetPath);
		Row.AssetClass = ValidationRow.Kind;
		Row.Details = ValidationRow.Facts;
		Row.Details.Add(TEXT("kind"), ValidationRow.Kind);
		Row.Details.Add(TEXT("name"), ValidationRow.Name);
		for (const FString& Change : Plan.ProposedChanges)
		{
			if (ChangeMentionsRow(Change, ValidationRow))
			{
				Row.PlannedAdditions.Add(Change);
			}
		}
		const bool bRowChanged = !Row.PlannedAdditions.IsEmpty();
		Row.Status = bRowChanged
			? (Mode == EKatanaAssetMigrationMode::Apply
				|| Mode == EKatanaAssetMigrationMode::ApplyAndSave
					? EKatanaAssetMigrationStatus::Changed
					: EKatanaAssetMigrationStatus::WouldChange)
			: EKatanaAssetMigrationStatus::Unchanged;
		for (const FDefenseAssetValidationFinding& Finding : Plan.Validation.Findings)
		{
			if ((!ValidationRow.Name.IsEmpty() && Finding.Context.Contains(ValidationRow.Name))
				|| (!ValidationRow.AssetPath.IsEmpty()
					&& Finding.Context.Contains(ValidationRow.AssetPath)))
			{
				const FString Message = FString::Printf(TEXT("%s|%s"),
					*Finding.Code, *Finding.Message);
				if (Finding.Severity == EDefenseAssetValidationSeverity::Error
					&& !IsCorrectableFinding(Finding.Code))
				{
					Row.Errors.Add(Message);
					Row.Status = EKatanaAssetMigrationStatus::Failed;
				}
				else
				{
					Row.Warnings.Add(Message);
				}
			}
		}
		OutReport.Rows.Add(MoveTemp(Row));
	}
	FKatanaAssetMigrationRunner::Summarize(OutReport);
}
}

bool FDefenseProofMigrationOperation::CanonicalizeJson(
	const FString& Json,
	FString& OutCanonicalJson,
	FString& OutError)
{
	OutCanonicalJson.Reset();
	OutError.Reset();
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("JSON root must be a valid object");
		return false;
	}

	const TSharedRef<FCanonicalJsonWriter> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutCanonicalJson);
	WriteCanonicalValue(MakeShared<FJsonValueObject>(RootObject), Writer);
	if (!Writer->Close())
	{
		OutError = TEXT("failed to close canonical JSON writer");
		OutCanonicalJson.Reset();
		return false;
	}
	return true;
}

FString FDefenseProofMigrationOperation::ComputePlanFingerprint(
	const FString& CanonicalManifest,
	const FString& CanonicalAssetFacts,
	const TArray<FString>& ProposedChanges,
	const TArray<FKatanaAssetMigrationPackageLedgerEntry>& PackageLedger)
{
	FString Input;
	AppendFingerprintField(Input, TEXT("operation"), OperationName);
	AppendFingerprintField(Input, TEXT("schema"), TEXT("2"));
	AppendFingerprintField(Input, TEXT("manifest"), CanonicalManifest);
	AppendFingerprintField(Input, TEXT("asset_facts"), CanonicalAssetFacts);

	TArray<FString> SortedChanges = ProposedChanges;
	SortedChanges.Sort();
	for (const FString& Change : SortedChanges)
	{
		AppendFingerprintField(Input, TEXT("change"), Change);
	}

	TArray<FKatanaAssetMigrationPackageLedgerEntry> SortedLedger = PackageLedger;
	SortedLedger.Sort([](const FKatanaAssetMigrationPackageLedgerEntry& Left,
		const FKatanaAssetMigrationPackageLedgerEntry& Right)
	{
		return Left.PackageName == Right.PackageName
			? Left.PackageRole < Right.PackageRole
			: Left.PackageName < Right.PackageName;
	});
	for (const FKatanaAssetMigrationPackageLedgerEntry& Entry : SortedLedger)
	{
		AppendFingerprintField(Input, TEXT("package"), Entry.PackageName);
		AppendFingerprintField(Input, TEXT("role"), Entry.PackageRole);
		AppendFingerprintField(Input, TEXT("initially_dirty"), LexToString(Entry.bInitiallyDirty));
		AppendFingerprintField(Input, TEXT("planned_action"), Entry.PlannedAction);
	}

	FTCHARToUTF8 Utf8(*Input);
	return FSHA1::HashBuffer(Utf8.Get(), static_cast<uint64>(Utf8.Length())).ToString();
}

bool FDefenseProofMigrationOperation::BuildLoadedPlan(
	const FDefenseProofManifest& Manifest,
	const FString& CanonicalManifest,
	const FDefenseProofAssetSet& Assets,
	FDefenseProofMigrationPlan& OutPlan,
	TArray<FString>& OutErrors)
{
	OutPlan = FDefenseProofMigrationPlan();
	OutPlan.Manifest = Manifest;
	OutPlan.CanonicalManifest = CanonicalManifest;
	FDefenseAssetValidationService::ValidateManifestObjects(
		Manifest, Assets, OutPlan.Validation);

	const UDefenseConfiguration* Configuration =
		Cast<UDefenseConfiguration>(Assets.Find(Manifest.DefenseConfiguration));
	for (const FString& Path : Manifest.CombatSettings)
	{
		const UCombatSettings* Settings = Cast<UCombatSettings>(Assets.Find(Path));
		if (Settings && Settings->DefenseConfiguration != Configuration)
		{
			AddPlannedChange(OutPlan,
				FString::Printf(TEXT("SetCombatSettingsDefenseConfiguration|%s|%s"),
					*Path, *Manifest.DefenseConfiguration),
				TEXT("CombatSettings"), Settings, Path);
		}
	}

	const UInputAction* BlockAction =
		Cast<UInputAction>(Assets.Find(Manifest.Fixture.InputAction));
	if (BlockAction && BlockAction->ValueType != EInputActionValueType::Boolean)
	{
		AddPlannedChange(OutPlan,
			FString::Printf(TEXT("SetBlockInputBoolean|%s"), *Manifest.Fixture.InputAction),
			TEXT("InputAction"), BlockAction, Manifest.Fixture.InputAction);
	}
	const UInputMappingContext* MappingContext =
		Cast<UInputMappingContext>(Assets.Find(Manifest.Fixture.InputMappingContext));
	const FKey BlockKey(FName(*Manifest.Fixture.BlockKey));
	if (BlockAction && MappingContext && BlockKey.IsValid()
		&& !MappingContext->GetMappings().ContainsByPredicate(
			[BlockAction, BlockKey](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == BlockAction && Mapping.Key == BlockKey;
			}))
	{
		AddPlannedChange(OutPlan,
			FString::Printf(TEXT("MapBlockInput|%s|%s|%s"),
				*Manifest.Fixture.InputMappingContext,
				*Manifest.Fixture.InputAction, *Manifest.Fixture.BlockKey),
			TEXT("InputMappingContext"), MappingContext,
			Manifest.Fixture.InputMappingContext);
	}
	if (BlockAction && MappingContext
		&& MappingContext->GetMappings().ContainsByPredicate(
			[BlockAction](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == BlockAction && Mapping.Key == EKeys::RightMouseButton;
			}))
	{
		AddPlannedChange(OutPlan,
			FString::Printf(TEXT("RemoveDeprecatedBlockMapping|%s|%s"),
				*Manifest.Fixture.InputMappingContext, *Manifest.Fixture.InputAction),
			TEXT("InputMappingContext"), MappingContext,
			Manifest.Fixture.InputMappingContext);
	}

	UObject* PlayerObject = Assets.Find(Manifest.Fixture.PlayerBlueprint);
	UClass* PlayerClass = ResolveObjectClass(PlayerObject);
	const APlayerCharacter* PlayerDefault = PlayerClass
		&& PlayerClass->IsChildOf(APlayerCharacter::StaticClass())
		? Cast<APlayerCharacter>(PlayerClass->GetDefaultObject()) : nullptr;
	const UCombatSettings* ExpectedPlayerSettings =
		Cast<UCombatSettings>(Assets.Find(Manifest.Fixture.PlayerCombatSettings));
	UClass* GuardClass = ResolveObjectClass(Assets.Find(Manifest.Fixture.GuardAnimBlueprint));
	if (PlayerDefault && (PlayerDefault->BlockAction != BlockAction
		|| PlayerDefault->DefaultMappingContext != MappingContext
		|| PlayerDefault->CombatSettings != ExpectedPlayerSettings
		|| !PlayerDefault->GetMesh()
		|| PlayerDefault->GetMesh()->GetAnimClass() != GuardClass))
	{
		AddPlannedChange(OutPlan,
			FString::Printf(TEXT("SetPlayerDefenseFixture|%s"), *Manifest.Fixture.PlayerBlueprint),
			TEXT("PlayerBlueprint"), PlayerObject, Manifest.Fixture.PlayerBlueprint);
	}
	for (int32 EnemyIndex = 0; EnemyIndex < Manifest.Fixture.EnemyBlueprints.Num(); ++EnemyIndex)
	{
		UObject* EnemyObject = Assets.Find(Manifest.Fixture.EnemyBlueprints[EnemyIndex]);
		UClass* EnemyClass = ResolveObjectClass(EnemyObject);
		const ABaseCombatCharacter* EnemyDefault = EnemyClass
			&& EnemyClass->IsChildOf(ABaseCombatCharacter::StaticClass())
			? Cast<ABaseCombatCharacter>(EnemyClass->GetDefaultObject()) : nullptr;
		const UCombatSettings* ExpectedSettings =
			Manifest.Fixture.EnemyCombatSettings.IsValidIndex(EnemyIndex)
				? Cast<UCombatSettings>(Assets.Find(Manifest.Fixture.EnemyCombatSettings[EnemyIndex]))
				: nullptr;
		if (EnemyDefault && EnemyDefault->CombatSettings != ExpectedSettings)
		{
			AddPlannedChange(OutPlan,
				FString::Printf(TEXT("SetEnemyCombatSettings|%s|%s"),
					*Manifest.Fixture.EnemyBlueprints[EnemyIndex],
					Manifest.Fixture.EnemyCombatSettings.IsValidIndex(EnemyIndex)
						? *Manifest.Fixture.EnemyCombatSettings[EnemyIndex] : TEXT("None")),
				TEXT("EnemyBlueprint"), EnemyObject,
				Manifest.Fixture.EnemyBlueprints[EnemyIndex]);
		}
	}

	for (const FDefenseProofAttackEntry& Entry : Manifest.Attacks)
	{
		const UAttackData* Attack = Cast<UAttackData>(Assets.Find(Entry.AttackData));
		const UAnimMontage* Montage = Cast<UAnimMontage>(Assets.Find(Entry.Montage));
		if (!Attack || !Montage)
		{
			continue;
		}
		const bool bDefinitionMismatch = Attack->AttackMontage != Montage
			|| Attack->MontageSection != FName(*Entry.Section)
			|| Attack->DefenseProfile.Height != static_cast<EAttackHeight>(
				StaticEnum<EAttackHeight>()->GetValueByNameString(Entry.ExpectedHeight))
			|| Attack->DefenseProfile.NominalLane != static_cast<EIncomingAttackLane>(
				StaticEnum<EIncomingAttackLane>()->GetValueByNameString(Entry.ExpectedLane))
			|| Attack->DefenseProfile.SwingShape != static_cast<ESwingDirection>(
				StaticEnum<ESwingDirection>()->GetValueByNameString(Entry.ExpectedSwing))
			|| Attack->DefenseProfile.SourceContactSocketOverride != FName(*Entry.ExpectedSourceSocket)
			|| Attack->GetDefenseTargetBoneFallback() != FName(*Entry.ExpectedTargetBone)
			|| !AttackTagsMatch(Attack, Entry);
		if (bDefinitionMismatch)
		{
			AddPlannedChange(OutPlan,
				FString::Printf(TEXT("SetAttackDefenseDefinition|%s|%s|%s|%s"),
					*Entry.AttackData, *Entry.ExpectedHeight, *Entry.ExpectedLane, *Entry.ExpectedSwing),
				TEXT("AttackData"), Attack, Entry.AttackData);
		}
		if (Attack->bHasCounterVariant != (Attack->CounterData != nullptr)
			|| Attack->bCanTriggerFinisher != (Attack->FinisherData != nullptr))
		{
			AddPlannedChange(OutPlan,
				FString::Printf(TEXT("SetAttackVariantFlags|%s|counter=%s|finisher=%s"),
					*Entry.AttackData, *LexToString(Attack->CounterData != nullptr),
					*LexToString(Attack->FinisherData != nullptr)),
				TEXT("AttackData"), Attack, Entry.AttackData);
		}
		if (!ParryWindowMatches(Montage, Entry))
		{
			AddPlannedChange(OutPlan,
				FString::Printf(TEXT("SetParryWindow|%s|%s|%.6f|%.6f"),
					*Entry.Montage, *Entry.Section,
					Entry.ParryWindow.StartSeconds, Entry.ParryWindow.EndSeconds),
				TEXT("AttackMontage"), Montage, Entry.Montage, true);
		}
	}

	for (const FDefenseProofPairedDependencyEntry& Entry : Manifest.PairedDependencies)
	{
		const UPairedAnimationData* Data =
			Cast<UPairedAnimationData>(Assets.Find(Entry.PairedData));
		if (Data && !PairedDefinitionMatches(Data, Entry, Assets))
		{
			AddPlannedChange(OutPlan,
				FString::Printf(TEXT("SetPairedDefinition|%s|%s"), *Entry.PairedData, *Entry.Role),
				TEXT("PairedData"), Data, Entry.PairedData);
		}
	}

	TMap<FString, const FDefenseProofPairedDependencyEntry*> PairedEntries;
	for (const FDefenseProofPairedDependencyEntry& Entry : Manifest.PairedDependencies)
	{
		PairedEntries.Add(Entry.Name, &Entry);
	}
	for (const FDefenseProofExpectedCaseEntry& Case : Manifest.ExpectedCases)
	{
		const FDefenseProofAttackEntry* AttackEntry = Manifest.Attacks.FindByPredicate(
			[&Case](const FDefenseProofAttackEntry& Entry) { return Entry.Name == Case.Attack; });
		const UAttackData* Attack = AttackEntry
			? Cast<UAttackData>(Assets.Find(AttackEntry->AttackData)) : nullptr;
		if (!Attack || !AttackEntry)
		{
			continue;
		}
		for (const FString& DependencyName : Case.PairedDependencies)
		{
			const FDefenseProofPairedDependencyEntry* const* PairedEntry =
				PairedEntries.Find(DependencyName);
			if (!PairedEntry)
			{
				continue;
			}
			const UPairedAnimationData* Data =
				Cast<UPairedAnimationData>(Assets.Find((*PairedEntry)->PairedData));
			if ((*PairedEntry)->Role == TEXT("Counter")
				&& (Attack->CounterData != Data || !Attack->bHasCounterVariant))
			{
				AddPlannedChange(OutPlan,
					FString::Printf(TEXT("SetAttackCounter|%s|%s"),
						*AttackEntry->AttackData, *(*PairedEntry)->PairedData),
					TEXT("AttackData"), Attack, AttackEntry->AttackData);
			}
			if ((*PairedEntry)->Role == TEXT("Finisher")
				&& (Attack->FinisherData != Data || !Attack->bCanTriggerFinisher))
			{
				AddPlannedChange(OutPlan,
					FString::Printf(TEXT("SetAttackFinisher|%s|%s"),
						*AttackEntry->AttackData, *(*PairedEntry)->PairedData),
					TEXT("AttackData"), Attack, AttackEntry->AttackData);
			}
		}
	}

	OutPlan.ProposedChanges.Sort();
	OutPlan.PackageLedger.Sort([](const FKatanaAssetMigrationPackageLedgerEntry& Left,
		const FKatanaAssetMigrationPackageLedgerEntry& Right)
	{
		return Left.PackageName < Right.PackageName;
	});
	AddValidationFacts(
		Manifest, Assets, OutPlan.Validation, OutPlan.CanonicalAssetFacts, OutErrors);
	OutPlan.Fingerprint = ComputePlanFingerprint(
		OutPlan.CanonicalManifest, OutPlan.CanonicalAssetFacts,
		OutPlan.ProposedChanges, OutPlan.PackageLedger);
	return OutErrors.IsEmpty();
}

bool FDefenseProofMigrationOperation::ApplyLoadedPlan(
	const FDefenseProofMigrationPlan& Plan,
	const FDefenseProofAssetSet& Assets,
	const bool bAllowTimingMutation,
	TSet<FString>& OutChangedPackages,
	TArray<FString>& OutErrors)
{
	OutChangedPackages.Reset();
	if (Plan.bRequiresTimingMutation && !bAllowTimingMutation)
	{
		OutErrors.Add(TEXT("reviewed parry-window edits require -AllowTimingMutation"));
		return false;
	}
	for (const FDefenseProofAttackEntry& Entry : Plan.Manifest.Attacks)
	{
		for (const FString& TagName : Entry.ExpectedTags)
		{
			if (!FGameplayTag::RequestGameplayTag(FName(*TagName), false).IsValid())
			{
				OutErrors.Add(FString::Printf(TEXT("manifest attack tag is not registered: %s"), *TagName));
			}
		}
	}
	if (!OutErrors.IsEmpty())
	{
		return false;
	}
	for (const FDefenseProofAttackEntry& Entry : Plan.Manifest.Attacks)
	{
		const UAnimMontage* Montage = Cast<UAnimMontage>(Assets.Find(Entry.Montage));
		if (Montage && !ParryWindowMatches(Montage, Entry))
		{
			float SectionStart = 0.0f;
			float SectionEnd = 0.0f;
			if (!GetSectionBounds(Montage, FName(*Entry.Section), SectionStart, SectionEnd))
			{
				OutErrors.Add(FString::Printf(
					TEXT("cannot apply parry timing; section missing: %s"), *Entry.Section));
			}
		}
	}
	if (!OutErrors.IsEmpty())
	{
		return false;
	}
	for (const FDefenseProofPairedDependencyEntry& Entry : Plan.Manifest.PairedDependencies)
	{
		const UAnimMontage* AttackerMontage =
			Cast<UAnimMontage>(Assets.Find(Entry.AttackerMontage));
		const UAnimMontage* VictimMontage =
			Cast<UAnimMontage>(Assets.Find(Entry.VictimMontage));
		if (!AttackerMontage
			|| !AttackerMontage->IsValidSectionName(FName(*Entry.AttackerSection)))
		{
			OutErrors.Add(FString::Printf(
				TEXT("cannot apply paired definition; attacker section missing: %s"),
				*Entry.AttackerSection));
		}
		if (!VictimMontage
			|| !VictimMontage->IsValidSectionName(FName(*Entry.VictimSection)))
		{
			OutErrors.Add(FString::Printf(
				TEXT("cannot apply paired definition; victim section missing: %s"),
				*Entry.VictimSection));
		}
		if (Entry.bHasAttackerReadySection
			&& (!AttackerMontage
				|| !AttackerMontage->IsValidSectionName(FName(*Entry.AttackerReadySection))))
		{
			OutErrors.Add(FString::Printf(
				TEXT("cannot apply paired definition; attacker ready section missing: %s"),
				*Entry.AttackerReadySection));
		}
		if (Entry.bHasVictimReadySection
			&& (!VictimMontage
				|| !VictimMontage->IsValidSectionName(FName(*Entry.VictimReadySection))))
		{
			OutErrors.Add(FString::Printf(
				TEXT("cannot apply paired definition; victim ready section missing: %s"),
				*Entry.VictimReadySection));
		}
	}
	if (!OutErrors.IsEmpty())
	{
		return false;
	}

	UDefenseConfiguration* Configuration =
		Cast<UDefenseConfiguration>(Assets.Find(Plan.Manifest.DefenseConfiguration));
	for (const FString& Path : Plan.Manifest.CombatSettings)
	{
		UCombatSettings* Settings = Cast<UCombatSettings>(Assets.Find(Path));
		if (Settings && Settings->DefenseConfiguration != Configuration)
		{
			Settings->Modify();
			Settings->DefenseConfiguration = Configuration;
			MarkObjectChanged(Settings, OutChangedPackages);
		}
	}

	UInputAction* BlockAction = Cast<UInputAction>(Assets.Find(Plan.Manifest.Fixture.InputAction));
	if (BlockAction && BlockAction->ValueType != EInputActionValueType::Boolean)
	{
		BlockAction->Modify();
		BlockAction->ValueType = EInputActionValueType::Boolean;
		MarkObjectChanged(BlockAction, OutChangedPackages);
	}
	UInputMappingContext* MappingContext =
		Cast<UInputMappingContext>(Assets.Find(Plan.Manifest.Fixture.InputMappingContext));
	const FKey BlockKey(FName(*Plan.Manifest.Fixture.BlockKey));
	const bool bNeedsExpectedMapping = BlockAction && MappingContext && BlockKey.IsValid()
		&& !MappingContext->GetMappings().ContainsByPredicate(
			[BlockAction, BlockKey](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == BlockAction && Mapping.Key == BlockKey;
			});
	const bool bNeedsDeprecatedMappingRemoval = BlockAction && MappingContext
		&& MappingContext->GetMappings().ContainsByPredicate(
			[BlockAction](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == BlockAction && Mapping.Key == EKeys::RightMouseButton;
			});
	if (bNeedsExpectedMapping || bNeedsDeprecatedMappingRemoval)
	{
		MappingContext->Modify();
		if (bNeedsDeprecatedMappingRemoval)
		{
			MappingContext->UnmapKey(BlockAction, EKeys::RightMouseButton);
		}
		if (bNeedsExpectedMapping)
		{
			MappingContext->MapKey(BlockAction, BlockKey);
		}
		MarkObjectChanged(MappingContext, OutChangedPackages);
	}

	UObject* PlayerObject = Assets.Find(Plan.Manifest.Fixture.PlayerBlueprint);
	UClass* PlayerClass = ResolveObjectClass(PlayerObject);
	APlayerCharacter* PlayerDefault = PlayerClass
		&& PlayerClass->IsChildOf(APlayerCharacter::StaticClass())
		? Cast<APlayerCharacter>(PlayerClass->GetDefaultObject()) : nullptr;
	UCombatSettings* ExpectedPlayerSettings =
		Cast<UCombatSettings>(Assets.Find(Plan.Manifest.Fixture.PlayerCombatSettings));
	UClass* GuardClass = ResolveObjectClass(Assets.Find(Plan.Manifest.Fixture.GuardAnimBlueprint));
	if (PlayerDefault && (PlayerDefault->BlockAction != BlockAction
		|| PlayerDefault->DefaultMappingContext != MappingContext
		|| PlayerDefault->CombatSettings != ExpectedPlayerSettings
		|| !PlayerDefault->GetMesh()
		|| PlayerDefault->GetMesh()->GetAnimClass() != GuardClass))
	{
		PlayerObject->Modify();
		PlayerDefault->Modify();
		PlayerDefault->BlockAction = BlockAction;
		PlayerDefault->DefaultMappingContext = MappingContext;
		PlayerDefault->CombatSettings = ExpectedPlayerSettings;
		if (PlayerDefault->GetMesh())
		{
			PlayerDefault->GetMesh()->Modify();
			PlayerDefault->GetMesh()->SetAnimInstanceClass(GuardClass);
		}
		MarkBlueprintDefaultsChanged(PlayerObject, OutChangedPackages);
	}
	for (int32 EnemyIndex = 0;
		EnemyIndex < Plan.Manifest.Fixture.EnemyBlueprints.Num(); ++EnemyIndex)
	{
		UObject* EnemyObject = Assets.Find(Plan.Manifest.Fixture.EnemyBlueprints[EnemyIndex]);
		UClass* EnemyClass = ResolveObjectClass(EnemyObject);
		ABaseCombatCharacter* EnemyDefault = EnemyClass
			&& EnemyClass->IsChildOf(ABaseCombatCharacter::StaticClass())
			? Cast<ABaseCombatCharacter>(EnemyClass->GetDefaultObject()) : nullptr;
		UCombatSettings* ExpectedSettings =
			Plan.Manifest.Fixture.EnemyCombatSettings.IsValidIndex(EnemyIndex)
				? Cast<UCombatSettings>(Assets.Find(
					Plan.Manifest.Fixture.EnemyCombatSettings[EnemyIndex])) : nullptr;
		if (EnemyDefault && EnemyDefault->CombatSettings != ExpectedSettings)
		{
			EnemyObject->Modify();
			EnemyDefault->Modify();
			EnemyDefault->CombatSettings = ExpectedSettings;
			MarkBlueprintDefaultsChanged(EnemyObject, OutChangedPackages);
		}
	}

	for (const FDefenseProofAttackEntry& Entry : Plan.Manifest.Attacks)
	{
		UAttackData* Attack = Cast<UAttackData>(Assets.Find(Entry.AttackData));
		UAnimMontage* Montage = Cast<UAnimMontage>(Assets.Find(Entry.Montage));
		if (!Attack || !Montage)
		{
			continue;
		}
		if (Attack->AttackMontage != Montage
			|| Attack->MontageSection != FName(*Entry.Section)
			|| Attack->DefenseProfile.Height != static_cast<EAttackHeight>(
				StaticEnum<EAttackHeight>()->GetValueByNameString(Entry.ExpectedHeight))
			|| Attack->DefenseProfile.NominalLane != static_cast<EIncomingAttackLane>(
				StaticEnum<EIncomingAttackLane>()->GetValueByNameString(Entry.ExpectedLane))
			|| Attack->DefenseProfile.SwingShape != static_cast<ESwingDirection>(
				StaticEnum<ESwingDirection>()->GetValueByNameString(Entry.ExpectedSwing))
			|| Attack->DefenseProfile.SourceContactSocketOverride != FName(*Entry.ExpectedSourceSocket)
			|| Attack->GetDefenseTargetBoneFallback() != FName(*Entry.ExpectedTargetBone)
			|| !AttackTagsMatch(Attack, Entry))
		{
			Attack->Modify();
			Attack->AttackMontage = Montage;
			Attack->MontageSection = FName(*Entry.Section);
			Attack->DefenseProfile.Height = static_cast<EAttackHeight>(
				StaticEnum<EAttackHeight>()->GetValueByNameString(Entry.ExpectedHeight));
			Attack->DefenseProfile.NominalLane = static_cast<EIncomingAttackLane>(
				StaticEnum<EIncomingAttackLane>()->GetValueByNameString(Entry.ExpectedLane));
			Attack->DefenseProfile.SwingShape = static_cast<ESwingDirection>(
				StaticEnum<ESwingDirection>()->GetValueByNameString(Entry.ExpectedSwing));
			Attack->DefenseProfile.SourceContactSocketOverride = FName(*Entry.ExpectedSourceSocket);
			Attack->DefenseProfile.DefenderTargetBoneFallback = FName(*Entry.ExpectedTargetBone);
			Attack->AttackTags.Reset();
			for (const FString& TagName : Entry.ExpectedTags)
			{
				Attack->AttackTags.AddTag(FGameplayTag::RequestGameplayTag(FName(*TagName), false));
			}
			MarkObjectChanged(Attack, OutChangedPackages);
		}
		if (Attack->bHasCounterVariant != (Attack->CounterData != nullptr)
			|| Attack->bCanTriggerFinisher != (Attack->FinisherData != nullptr))
		{
			Attack->Modify();
			Attack->bHasCounterVariant = Attack->CounterData != nullptr;
			Attack->bCanTriggerFinisher = Attack->FinisherData != nullptr;
			MarkObjectChanged(Attack, OutChangedPackages);
		}

		if (!ParryWindowMatches(Montage, Entry))
		{
			float SectionStart = 0.0f;
			float SectionEnd = 0.0f;
			if (!GetSectionBounds(Montage, FName(*Entry.Section), SectionStart, SectionEnd))
			{
				OutErrors.Add(FString::Printf(TEXT("cannot apply parry timing; section missing: %s"),
					*Entry.Section));
				continue;
			}
			Montage->Modify();
			for (int32 Index = Montage->Notifies.Num() - 1; Index >= 0; --Index)
			{
				const FAnimNotifyEvent& Event = Montage->Notifies[Index];
				if (!Event.NotifyStateClass
					|| !Event.NotifyStateClass->IsA(UAnimNotifyState_ParryWindow::StaticClass()))
				{
					continue;
				}
				const double Start = Event.GetTriggerTime();
				const double End = Start + Event.GetDuration();
				if (End > SectionStart + KINDA_SMALL_NUMBER
					&& Start < SectionEnd - KINDA_SMALL_NUMBER)
				{
					Montage->Notifies.RemoveAt(Index);
				}
			}
			if (Entry.ParryWindow.bPresent)
			{
				UAnimNotifyState_ParryWindow* Notify =
					NewObject<UAnimNotifyState_ParryWindow>(Montage);
				FAnimNotifyEvent Event;
				Event.NotifyStateClass = Notify;
				Event.SetTime(SectionStart + Entry.ParryWindow.StartSeconds);
				Event.SetDuration(Entry.ParryWindow.EndSeconds - Entry.ParryWindow.StartSeconds);
				Montage->Notifies.Add(Event);
			}
			Montage->SortNotifies();
			Montage->RefreshCacheData();
			MarkObjectChanged(Montage, OutChangedPackages);
		}
	}

	for (const FDefenseProofPairedDependencyEntry& Entry : Plan.Manifest.PairedDependencies)
	{
		UPairedAnimationData* Data = Cast<UPairedAnimationData>(Assets.Find(Entry.PairedData));
		if (!Data || PairedDefinitionMatches(Data, Entry, Assets))
		{
			continue;
		}
		Data->Modify();
		Data->ReactionType = ReactionForRole(Entry.Role);
		Data->AttackerMontage = Cast<UAnimMontage>(Assets.Find(Entry.AttackerMontage));
		Data->VictimMontage = Cast<UAnimMontage>(Assets.Find(Entry.VictimMontage));
		Data->AttackerMontageSection = FName(*Entry.AttackerSection);
		Data->VictimMontageSection = FName(*Entry.VictimSection);
		Data->AttackerWarpConfig.WarpTargetName = FName(*Entry.AttackerWarpTarget);
		Data->VictimWarpConfig.WarpTargetName = FName(*Entry.VictimWarpTarget);
		Data->AttackerWarpConfig.bWarpRotation = true;
		Data->VictimWarpConfig.bWarpRotation = true;
		if (Entry.Role == TEXT("Finisher"))
		{
			Data->ChainTransitionPolicy.RequiredMarker = NAME_None;
			Data->ChainTransitionPolicy.bAutoContinue = false;
		}
		else
		{
			Data->ChainTransitionPolicy.DriverRole = Entry.DriverRole == TEXT("Victim")
				? EPairedAnimationRole::Victim : EPairedAnimationRole::Attacker;
			Data->ChainTransitionPolicy.RequiredMarker = FName(*Entry.DriverMarker);
			Data->ChainTransitionPolicy.bAutoContinue = Entry.Role == TEXT("Counter");
			Data->ChainTransitionPolicy.AttackerReadySection = Entry.bHasAttackerReadySection
				? FName(*Entry.AttackerReadySection) : NAME_None;
			Data->ChainTransitionPolicy.VictimReadySection = Entry.bHasVictimReadySection
				? FName(*Entry.VictimReadySection) : NAME_None;
			Data->ChainTransitionPolicy.bAttackerTerminalPoseCompatible =
				Entry.bAttackerTerminalPoseCompatible;
			Data->ChainTransitionPolicy.bVictimTerminalPoseCompatible =
				Entry.bVictimTerminalPoseCompatible;
		}
		MarkObjectChanged(Data, OutChangedPackages);
	}

	TMap<FString, const FDefenseProofPairedDependencyEntry*> PairedEntries;
	for (const FDefenseProofPairedDependencyEntry& Entry : Plan.Manifest.PairedDependencies)
	{
		PairedEntries.Add(Entry.Name, &Entry);
	}
	for (const FDefenseProofExpectedCaseEntry& Case : Plan.Manifest.ExpectedCases)
	{
		const FDefenseProofAttackEntry* AttackEntry = Plan.Manifest.Attacks.FindByPredicate(
			[&Case](const FDefenseProofAttackEntry& Entry) { return Entry.Name == Case.Attack; });
		UAttackData* Attack = AttackEntry
			? Cast<UAttackData>(Assets.Find(AttackEntry->AttackData)) : nullptr;
		if (!Attack)
		{
			continue;
		}
		for (const FString& DependencyName : Case.PairedDependencies)
		{
			const FDefenseProofPairedDependencyEntry* const* PairedEntry =
				PairedEntries.Find(DependencyName);
			if (!PairedEntry)
			{
				continue;
			}
			UPairedAnimationData* Data =
				Cast<UPairedAnimationData>(Assets.Find((*PairedEntry)->PairedData));
			if ((*PairedEntry)->Role == TEXT("Counter")
				&& (Attack->CounterData != Data || !Attack->bHasCounterVariant))
			{
				Attack->Modify();
				Attack->CounterData = Data;
				Attack->bHasCounterVariant = Data != nullptr;
				MarkObjectChanged(Attack, OutChangedPackages);
			}
			if ((*PairedEntry)->Role == TEXT("Finisher")
				&& (Attack->FinisherData != Data || !Attack->bCanTriggerFinisher))
			{
				Attack->Modify();
				Attack->FinisherData = Data;
				Attack->bCanTriggerFinisher = Data != nullptr;
				MarkObjectChanged(Attack, OutChangedPackages);
			}
		}
	}

	return OutErrors.IsEmpty();
}

bool FDefenseProofMigrationOperation::ValidateApprovedPlanBinding(
	const FKatanaAssetMigrationOptions& Options,
	const FDefenseProofMigrationPlan& CurrentPlan,
	TArray<FString>& OutErrors)
{
	if (Options.ApprovedPlanReport.IsEmpty() || Options.ApprovedPlanFingerprint.IsEmpty())
	{
		OutErrors.Add(TEXT("approved plan report and fingerprint are both required"));
		return false;
	}
	if (Options.ApprovedPlanFingerprint != CurrentPlan.Fingerprint)
	{
		OutErrors.Add(TEXT("approved fingerprint does not match the current deterministic plan"));
	}

	FString Json;
	const FString ReportPath =
		FKatanaAssetMigrationRunner::ResolveProjectRelativeFilePath(Options.ApprovedPlanReport);
	if (!FFileHelper::LoadFileToString(Json, *ReportPath))
	{
		OutErrors.Add(FString::Printf(TEXT("could not read approved plan report: %s"), *ReportPath));
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutErrors.Add(TEXT("approved plan report is not valid JSON"));
		return false;
	}
	FString Operation;
	FString ReportFingerprint;
	FString Mode;
	double SchemaVersion = 0.0;
	if (!Root->TryGetNumberField(TEXT("schema_version"), SchemaVersion)
		|| SchemaVersion != 2.0
		|| !Root->TryGetStringField(TEXT("operation"), Operation)
		|| Operation != OperationName
		|| !Root->TryGetStringField(TEXT("mode"), Mode)
		|| Mode != TEXT("Plan")
		|| !Root->TryGetStringField(TEXT("plan_fingerprint"), ReportFingerprint))
	{
		OutErrors.Add(TEXT("approved report must be a schema-v2 DefenseProofMigration Plan report"));
		return false;
	}
	if (ReportFingerprint != Options.ApprovedPlanFingerprint
		|| ReportFingerprint != CurrentPlan.Fingerprint)
	{
		OutErrors.Add(TEXT("approved report fingerprint, argument, and current plan do not agree"));
	}

	const TArray<TSharedPtr<FJsonValue>>* LedgerValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("package_ledger"), LedgerValues) || !LedgerValues)
	{
		OutErrors.Add(TEXT("approved report is missing package_ledger"));
		return false;
	}
	TArray<FKatanaAssetMigrationPackageLedgerEntry> ApprovedLedger;
	for (const TSharedPtr<FJsonValue>& Value : *LedgerValues)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FKatanaAssetMigrationPackageLedgerEntry Entry;
		if (!Object.IsValid()
			|| !Object->TryGetStringField(TEXT("package_name"), Entry.PackageName)
			|| !Object->TryGetStringField(TEXT("package_role"), Entry.PackageRole)
			|| !Object->TryGetBoolField(TEXT("initially_dirty"), Entry.bInitiallyDirty)
			|| !Object->TryGetStringField(TEXT("planned_action"), Entry.PlannedAction))
		{
			OutErrors.Add(TEXT("approved package ledger contains a malformed entry"));
			return false;
		}
		ApprovedLedger.Add(MoveTemp(Entry));
	}
	ApprovedLedger.Sort([](const FKatanaAssetMigrationPackageLedgerEntry& Left,
		const FKatanaAssetMigrationPackageLedgerEntry& Right)
	{
		return Left.PackageName < Right.PackageName;
	});
	TArray<FKatanaAssetMigrationPackageLedgerEntry> CurrentLedger = CurrentPlan.PackageLedger;
	CurrentLedger.Sort([](const FKatanaAssetMigrationPackageLedgerEntry& Left,
		const FKatanaAssetMigrationPackageLedgerEntry& Right)
	{
		return Left.PackageName < Right.PackageName;
	});
	if (ApprovedLedger.Num() != CurrentLedger.Num())
	{
		OutErrors.Add(TEXT("approved package ledger cardinality differs from the current plan"));
	}
	else
	{
		for (int32 Index = 0; Index < CurrentLedger.Num(); ++Index)
		{
			const FKatanaAssetMigrationPackageLedgerEntry& Approved = ApprovedLedger[Index];
			const FKatanaAssetMigrationPackageLedgerEntry& Current = CurrentLedger[Index];
			if (Approved.PackageName != Current.PackageName
				|| Approved.PackageRole != Current.PackageRole
				|| Approved.bInitiallyDirty != Current.bInitiallyDirty
				|| Approved.PlannedAction != Current.PlannedAction)
			{
				OutErrors.Add(TEXT("approved package ledger differs from the current plan"));
				break;
			}
		}
	}
	return OutErrors.IsEmpty();
}

bool FDefenseProofMigrationOperation::ValidateChangedPackageSet(
	const FDefenseProofMigrationPlan& Plan,
	const TSet<FString>& ChangedPackages,
	TArray<FString>& OutErrors)
{
	TSet<FString> PlannedPackages;
	for (const FKatanaAssetMigrationPackageLedgerEntry& Entry : Plan.PackageLedger)
	{
		PlannedPackages.Add(Entry.PackageName);
	}
	if (!ChangedPackages.Difference(PlannedPackages).IsEmpty()
		|| !PlannedPackages.Difference(ChangedPackages).IsEmpty())
	{
		OutErrors.Add(TEXT("actual changed packages differ from the approved package ledger"));
	}
	return OutErrors.IsEmpty();
}

bool FDefenseProofMigrationOperation::ValidateInitialDirtyPackageGate(
	const FDefenseProofMigrationPlan& Plan,
	const bool bAllowDirtyPackages,
	TArray<FString>& OutErrors)
{
	if (!bAllowDirtyPackages
		&& Plan.PackageLedger.ContainsByPredicate(
			[](const FKatanaAssetMigrationPackageLedgerEntry& Entry)
			{
				return Entry.bInitiallyDirty;
			}))
	{
		OutErrors.Add(TEXT("approved plan contains an initially dirty package"));
	}
	return OutErrors.IsEmpty();
}

bool FDefenseProofMigrationOperation::Run(
	const FString& ManifestPath,
	const FKatanaAssetMigrationOptions& Options,
	FKatanaAssetMigrationReport& OutReport) const
{
	const FString ResolvedPath =
		FKatanaAssetMigrationRunner::ResolveProjectRelativeFilePath(ManifestPath);
	FString Json;
	FDefenseProofManifest Manifest;
	TArray<FString> Errors;
	if (!FFileHelper::LoadFileToString(Json, *ResolvedPath)
		|| !FDefenseAssetValidationService::ParseManifestJson(Json, Manifest, Errors))
	{
		FDefenseProofMigrationPlan FailedPlan;
		FailedPlan.Manifest.SourcePath = ResolvedPath;
		for (const FString& Error : Errors)
		{
			FailedPlan.Validation.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("ManifestParseFailed"), ResolvedPath, Error);
		}
		if (Errors.IsEmpty())
		{
			FailedPlan.Validation.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("ManifestReadFailed"), ResolvedPath, TEXT("could not read manifest"));
		}
		PopulateReportFromPlan(ManifestPath, FailedPlan, Options.Mode, nullptr, OutReport);
		return false;
	}
	Manifest.SourcePath = ResolvedPath;
	FString CanonicalManifest;
	FString CanonicalError;
	if (!CanonicalizeJson(Json, CanonicalManifest, CanonicalError))
	{
		FDefenseProofMigrationPlan FailedPlan;
		FailedPlan.Manifest = Manifest;
		FailedPlan.Validation.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("ManifestCanonicalizationFailed"), ResolvedPath, CanonicalError);
		PopulateReportFromPlan(ManifestPath, FailedPlan, Options.Mode, nullptr, OutReport);
		return false;
	}

	FDefenseProofAssetSet Assets;
	FDefenseAssetValidationService::LoadExplicitObjects(Manifest, Assets);
	FDefenseProofMigrationPlan Plan;
	if (!BuildLoadedPlan(Manifest, CanonicalManifest, Assets, Plan, Errors))
	{
		PopulateReportFromPlan(ManifestPath, Plan, Options.Mode, nullptr, OutReport);
		return false;
	}

	if (Options.Mode == EKatanaAssetMigrationMode::Audit
		|| Options.Mode == EKatanaAssetMigrationMode::Plan)
	{
		PopulateReportFromPlan(ManifestPath, Plan, Options.Mode, nullptr, OutReport);
		return !HasUncorrectableErrors(Plan);
	}

	if (HasUncorrectableErrors(Plan))
	{
		PopulateReportFromPlan(ManifestPath, Plan, Options.Mode, nullptr, OutReport);
		return false;
	}
	if (!ValidateApprovedPlanBinding(Options, Plan, Errors))
	{
		Plan.Validation.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("ApprovedPlanMismatch"), ManifestPath, FString::Join(Errors, TEXT("; ")));
		PopulateReportFromPlan(ManifestPath, Plan, Options.Mode, nullptr, OutReport);
		return false;
	}
	Errors.Reset();
	if (!ValidateInitialDirtyPackageGate(Plan, Options.bAllowDirtyPackages, Errors))
	{
		Plan.Validation.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("InitiallyDirtyPackageRefused"), ManifestPath,
			FString::Join(Errors, TEXT("; ")));
		PopulateReportFromPlan(ManifestPath, Plan, Options.Mode, nullptr, OutReport);
		return false;
	}

	TSet<FString> ChangedPackages;
	Errors.Reset();
	if (!ApplyLoadedPlan(Plan, Assets, Options.bAllowTimingMutation, ChangedPackages, Errors))
	{
		Plan.Validation.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("ApplyFailed"), ManifestPath, FString::Join(Errors, TEXT("; ")));
		PopulateReportFromPlan(ManifestPath, Plan, Options.Mode, &ChangedPackages, OutReport);
		return false;
	}
	Errors.Reset();
	if (!ValidateChangedPackageSet(Plan, ChangedPackages, Errors))
	{
		Plan.Validation.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("ChangedPackageSetMismatch"), ManifestPath,
			TEXT("actual changed packages differ from the approved package ledger"));
		PopulateReportFromPlan(ManifestPath, Plan, Options.Mode, &ChangedPackages, OutReport);
		return false;
	}

	FDefenseProofMigrationPlan PostApplyPlan;
	Errors.Reset();
	if (!BuildLoadedPlan(Manifest, CanonicalManifest, Assets, PostApplyPlan, Errors))
	{
		Plan.Validation.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("ApplyValidationFailed"), ManifestPath,
			FString::Printf(TEXT("could not build post-apply validation plan: %s"),
				*FString::Join(Errors, TEXT("; "))));
		PopulateReportFromPlan(ManifestPath, Plan, Options.Mode, &ChangedPackages, OutReport);
		return false;
	}
	if (!PostApplyPlan.ProposedChanges.IsEmpty())
	{
		Plan.Validation.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("ApplyNotIdempotent"), ManifestPath,
			TEXT("post-apply plan still proposes mutations"));
		PopulateReportFromPlan(ManifestPath, Plan, Options.Mode, &ChangedPackages, OutReport);
		return false;
	}
	const bool bPostApplyValidationFailed =
		PostApplyPlan.Validation.Findings.ContainsByPredicate(
			[](const FDefenseAssetValidationFinding& Finding)
			{
				return Finding.Severity == EDefenseAssetValidationSeverity::Error;
			});
	if (bPostApplyValidationFailed)
	{
		Plan.Validation.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("ApplyValidationFailed"), ManifestPath,
			TEXT("post-apply validation still contains errors"));
		PopulateReportFromPlan(ManifestPath, Plan, Options.Mode, &ChangedPackages, OutReport);
		return false;
	}

	PopulateReportFromPlan(ManifestPath, Plan, Options.Mode, &ChangedPackages, OutReport);
	for (FKatanaAssetMigrationPackageLedgerEntry& Entry : OutReport.PackageLedger)
	{
		Entry.ActualAction = ChangedPackages.Contains(Entry.PackageName)
			? TEXT("Modified") : TEXT("Missing");
	}
	FKatanaAssetMigrationRunner::Summarize(OutReport);
	return true;
}
