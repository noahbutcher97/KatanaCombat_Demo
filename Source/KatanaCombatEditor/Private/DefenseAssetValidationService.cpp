// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefenseAssetValidationService.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimNotify_ChainStageTransition.h"
#include "Animation/AnimNotifyState_ParryWindow.h"
#include "Animation/AnimNotifyState_PairedAnimationCollision.h"
#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "Animation/SamuraiAnimInstance.h"
#include "AnimNotifyState_MotionWarping.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimStateNode.h"
#include "Characters/BaseCombatCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "CombatTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Data/DefenseConfiguration.h"
#include "Data/PairedAnimationData.h"
#include "Defense/DefensePresentationSelector.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "K2Node_Variable.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "NiagaraSystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "RootMotionModifier.h"
#include "UObject/UObjectGlobals.h"
#include "Utilities/CombatGameplayTags.h"

namespace
{
void AddError(TArray<FString>& Errors, const FString& Context, const FString& Message)
{
	Errors.Add(FString::Printf(TEXT("%s: %s"), *Context, *Message));
}

void RejectUnknownFields(
	const TSharedPtr<FJsonObject>& Object,
	std::initializer_list<const TCHAR*> Allowed,
	const FString& Context,
	TArray<FString>& Errors)
{
	TSet<FString> AllowedFields;
	for (const TCHAR* Field : Allowed)
	{
		AllowedFields.Add(Field);
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		if (!AllowedFields.Contains(Pair.Key))
		{
			AddError(Errors, Context,
				FString::Printf(TEXT("unknown field '%s'"), *Pair.Key));
		}
	}
}

bool ReadRequiredString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	const FString& Context,
	FString& OutValue,
	TArray<FString>& Errors)
{
	if (!Object->TryGetStringField(Field, OutValue))
	{
		AddError(Errors, Context, FString::Printf(TEXT("missing string field '%s'"), Field));
		return false;
	}
	OutValue.TrimStartAndEndInline();
	if (OutValue.IsEmpty())
	{
		AddError(Errors, Context, FString::Printf(TEXT("field '%s' must not be empty"), Field));
		return false;
	}
	return true;
}

bool ReadRequiredBool(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	const FString& Context,
	bool& OutValue,
	TArray<FString>& Errors)
{
	if (!Object->TryGetBoolField(Field, OutValue))
	{
		AddError(Errors, Context, FString::Printf(TEXT("missing boolean field '%s'"), Field));
		return false;
	}
	return true;
}

bool ReadRequiredNumber(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	const FString& Context,
	double& OutValue,
	TArray<FString>& Errors)
{
	if (!Object->TryGetNumberField(Field, OutValue) || !FMath::IsFinite(OutValue))
	{
		AddError(Errors, Context, FString::Printf(TEXT("missing or non-finite number field '%s'"), Field));
		return false;
	}
	return true;
}

bool ReadRequiredNullableString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	const FString& Context,
	FString& OutValue,
	bool& bOutHasValue,
	TArray<FString>& Errors)
{
	OutValue.Reset();
	bOutHasValue = false;
	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(Field);
	if (!Value || !Value->IsValid())
	{
		AddError(Errors, Context, FString::Printf(TEXT("missing nullable string field '%s'"), Field));
		return false;
	}
	if ((*Value)->Type == EJson::Null)
	{
		return true;
	}
	if ((*Value)->Type != EJson::String || !(*Value)->TryGetString(OutValue))
	{
		AddError(Errors, Context,
			FString::Printf(TEXT("field '%s' must be a non-empty string or null"), Field));
		return false;
	}
	OutValue.TrimStartAndEndInline();
	if (OutValue.IsEmpty())
	{
		AddError(Errors, Context,
			FString::Printf(TEXT("field '%s' must not be an empty string"), Field));
		return false;
	}
	bOutHasValue = true;
	return true;
}

bool ReadStringArray(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	const FString& Context,
	TArray<FString>& OutValues,
	TArray<FString>& Errors,
	const bool bRequireNonEmpty = true)
{
	OutValues.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object->TryGetArrayField(Field, Values) || !Values)
	{
		AddError(Errors, Context, FString::Printf(TEXT("field '%s' must be an array"), Field));
		return false;
	}
	if (bRequireNonEmpty && Values->IsEmpty())
	{
		AddError(Errors, Context, FString::Printf(TEXT("field '%s' must be a non-empty array"), Field));
		return false;
	}
	TSet<FString> Unique;
	for (int32 Index = 0; Index < Values->Num(); ++Index)
	{
		FString Value;
		if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetString(Value))
		{
			AddError(Errors, Context,
				FString::Printf(TEXT("field '%s' entry %d must be a string"), Field, Index));
			continue;
		}
		Value.TrimStartAndEndInline();
		if (Value.IsEmpty())
		{
			AddError(Errors, Context,
				FString::Printf(TEXT("field '%s' entry %d must not be empty"), Field, Index));
			continue;
		}
		if (Unique.Contains(Value))
		{
			AddError(Errors, Context,
				FString::Printf(TEXT("field '%s' contains duplicate '%s'"), Field, *Value));
			continue;
		}
		Unique.Add(Value);
		OutValues.Add(MoveTemp(Value));
	}
	return OutValues.Num() == Values->Num();
}

void RequireGameObjectPath(
	const FString& Path,
	const FString& Context,
	const FString& Field,
	TArray<FString>& Errors)
{
	if (!Path.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidObjectPath(Path))
	{
		AddError(Errors, Context,
			FString::Printf(TEXT("field '%s' must be an explicit /Game object path"), *Field));
	}
}

bool IsNamedEnumValue(const UEnum* Enum, const FString& Value)
{
	return Enum && Enum->GetValueByNameString(Value, EGetByNameFlags::CaseSensitive) != INDEX_NONE;
}

template <typename TEntry>
void RejectDuplicateName(
	const TEntry& Entry,
	TSet<FString>& Names,
	const FString& Context,
	TArray<FString>& Errors)
{
	if (Entry.Name.IsEmpty())
	{
		return;
	}
	if (Names.Contains(Entry.Name))
	{
		AddError(Errors, Context,
			FString::Printf(TEXT("duplicate name '%s'"), *Entry.Name));
	}
	Names.Add(Entry.Name);
}

bool ReadObjectArray(
	const TSharedPtr<FJsonObject>& Root,
	const TCHAR* Field,
	const TArray<TSharedPtr<FJsonValue>>*& OutValues,
	TArray<FString>& Errors)
{
	if (!Root->TryGetArrayField(Field, OutValues) || !OutValues || OutValues->IsEmpty())
	{
		AddError(Errors, TEXT("manifest"),
			FString::Printf(TEXT("field '%s' must be a non-empty array"), Field));
		return false;
	}
	return true;
}

template <typename TEnum>
TEnum ParseCheckedEnum(const FString& Value)
{
	return static_cast<TEnum>(StaticEnum<TEnum>()->GetValueByNameString(
		Value, EGetByNameFlags::CaseSensitive));
}

TArray<FString> GetSortedTagNames(const FGameplayTagContainer& Tags)
{
	TArray<FGameplayTag> TagArray;
	Tags.GetGameplayTagArray(TagArray);
	TArray<FString> Names;
	Names.Reserve(TagArray.Num());
	for (const FGameplayTag& Tag : TagArray)
	{
		Names.Add(Tag.ToString());
	}
	Names.Sort();
	return Names;
}

bool IsConcreteAudio(const FImpactAudioConfig& Audio)
{
	return Audio.ImpactSound != nullptr;
}

bool IsConcreteVFX(const FImpactVFXConfig& VFX)
{
	return VFX.ImpactVFX != nullptr;
}

bool IsValidPayloadMontage(const FDefensePresentationPayload& Payload)
{
	return Payload.Montage
		&& (Payload.MontageSection.IsNone()
			|| Payload.Montage->IsValidSectionName(Payload.MontageSection));
}

template <typename NotifyType>
int32 CountStateNotifies(const UAnimMontage* Montage)
{
	if (!Montage)
	{
		return 0;
	}
	int32 Count = 0;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		Count += Event.NotifyStateClass && Event.NotifyStateClass->IsA(NotifyType::StaticClass()) ? 1 : 0;
	}
	return Count;
}

int32 CountChainMarkers(
	const UAnimMontage* Montage,
	const FName Marker,
	const EChainStageTransitionType Transition,
	bool& bOutHasUnexpectedMarker)
{
	bOutHasUnexpectedMarker = false;
	if (!Montage)
	{
		return 0;
	}
	int32 Matching = 0;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const UAnimNotify_ChainStageTransition* Notify =
			Cast<UAnimNotify_ChainStageTransition>(Event.Notify);
		if (!Notify)
		{
			continue;
		}
		if (Notify->MarkerName == Marker && Notify->Transition == Transition)
		{
			++Matching;
		}
		else
		{
			bOutHasUnexpectedMarker = true;
		}
	}
	return Matching;
}

bool HasNamedRotationWarp(const UAnimMontage* Montage, const FName TargetName)
{
	if (!Montage || TargetName.IsNone())
	{
		return false;
	}
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const UAnimNotifyState_MotionWarping* Notify =
			Cast<UAnimNotifyState_MotionWarping>(Event.NotifyStateClass);
		const URootMotionModifier_Warp* Warp = Notify
			? Cast<URootMotionModifier_Warp>(Notify->RootMotionModifier)
			: nullptr;
		if (Warp && Warp->WarpTargetName == TargetName && Warp->bWarpRotation)
		{
			return true;
		}
	}
	return false;
}

void AddObjectDependency(const UObject* Object, FDefenseAssetValidationResult& Result)
{
	if (!Object)
	{
		return;
	}
	const FString Path = Object->GetPathName();
	if (Path.StartsWith(TEXT("/Game/")))
	{
		Result.Dependencies.AddUnique(Path);
	}
}

void AddMontageDependencies(
	const UAnimMontage* Montage,
	FDefenseAssetValidationResult& Result)
{
	if (!Montage)
	{
		return;
	}
	for (const FSlotAnimationTrack& SlotTrack : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : SlotTrack.AnimTrack.AnimSegments)
		{
			AddObjectDependency(Segment.GetAnimReference(), Result);
		}
	}
}

void AddAnimBlueprintDependencies(
	const UAnimBlueprint* AnimBlueprint,
	FDefenseAssetValidationResult& Result)
{
	if (!AnimBlueprint)
	{
		return;
	}
	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);
	for (const UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (const UAnimGraphNode_AssetPlayerBase* Player =
				Cast<UAnimGraphNode_AssetPlayerBase>(Node))
			{
				AddObjectDependency(Player->GetAnimationAsset(), Result);
			}
		}
	}
}

template <typename TObjectType>
const TObjectType* FindTypedObject(
	const FDefenseProofAssetSet& Assets,
	const FString& Path,
	const FString& Context,
	FDefenseAssetValidationResult& Result)
{
	UObject* Object = Assets.Find(Path);
	if (!Object)
	{
		return nullptr;
	}
	const TObjectType* TypedObject = Cast<TObjectType>(Object);
	if (!TypedObject)
	{
		Result.AddFinding(
			EDefenseAssetValidationSeverity::Error,
			TEXT("ManifestAssetTypeMismatch"),
			Context,
			FString::Printf(TEXT("%s loaded as %s, expected %s"),
				*Path, *Object->GetClass()->GetName(), *TObjectType::StaticClass()->GetName()));
	}
	return TypedObject;
}

UClass* ResolveDeclaredClass(UObject* Object)
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

bool IsNamedGuardState(const FString& Name)
{
	return Name.Contains(TEXT("Block"), ESearchCase::IgnoreCase)
		|| Name.Contains(TEXT("Guard"), ESearchCase::IgnoreCase);
}

void ValidateGuardAnimBlueprint(
	const FDefenseProofFixture& Fixture,
	UObject* GuardObject,
	FDefenseAssetValidationResult& Result)
{
	const FString Context = TEXT("fixture.guardAnimBlueprint");
	UClass* GuardClass = ResolveDeclaredClass(GuardObject);
	if (!GuardClass || !GuardClass->IsChildOf(USamuraiAnimInstance::StaticClass()))
	{
		Result.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("GuardAnimBlueprintClassMismatch"), Context,
			TEXT("guard AnimBP must generate a USamuraiAnimInstance subclass"));
		return;
	}
	if (!GuardClass->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USamuraiAnimInstance, bIsBlocking)))
	{
		Result.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("GuardBlockingPropertyMissing"), Context,
			TEXT("guard AnimBP class cannot read bIsBlocking"));
	}

	const UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(GuardObject);
	if (!AnimBlueprint)
	{
		return;
	}
	bool bHasGuardState = false;
	bool bReadsBlocking = false;
	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);
	for (const UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (const UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
			{
				bHasGuardState |= IsNamedGuardState(StateNode->GetStateName());
			}
			if (const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node))
			{
				bReadsBlocking |= VariableNode->GetVarName()
					== GET_MEMBER_NAME_CHECKED(USamuraiAnimInstance, bIsBlocking);
			}
		}
	}
	if (!bHasGuardState)
	{
		Result.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("GuardAnimStateMissing"), Context,
			TEXT("guard AnimBP has no state named for block or guard"));
	}
	if (!bReadsBlocking)
	{
		Result.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("GuardAnimTransitionMissing"), Context,
			TEXT("guard AnimBP graphs do not read bIsBlocking"));
	}

	FDefenseAssetValidationRow Row;
	Row.Kind = TEXT("Fixture");
	Row.Name = TEXT("GuardAnimBlueprint");
	Row.AssetPath = Fixture.GuardAnimBlueprint;
	Row.Facts.Add(TEXT("guard_state"), LexToString(bHasGuardState));
	Row.Facts.Add(TEXT("blocking_read"), LexToString(bReadsBlocking));
	Result.Rows.Add(MoveTemp(Row));
}
}

bool FDefenseAssetValidationResult::HasErrors() const
{
	return Findings.ContainsByPredicate([](const FDefenseAssetValidationFinding& Finding)
	{
		return Finding.Severity == EDefenseAssetValidationSeverity::Error;
	});
}

bool FDefenseAssetValidationResult::HasFinding(const FString& Code) const
{
	return Findings.ContainsByPredicate([&Code](const FDefenseAssetValidationFinding& Finding)
	{
		return Finding.Code == Code;
	});
}

TArray<FDefenseAssetValidationRow> FDefenseAssetValidationResult::FindRows(const FString& Kind) const
{
	TArray<FDefenseAssetValidationRow> Matching;
	for (const FDefenseAssetValidationRow& Row : Rows)
	{
		if (Row.Kind == Kind)
		{
			Matching.Add(Row);
		}
	}
	return Matching;
}

void FDefenseAssetValidationResult::AddFinding(
	const EDefenseAssetValidationSeverity Severity,
	const FString& Code,
	const FString& Context,
	const FString& Message)
{
	Findings.Add({Severity, Code, Context, Message});
}

void FDefenseProofAssetSet::Add(const FString& ObjectPath, UObject* Object)
{
	Objects.Add(ObjectPath, Object);
}

UObject* FDefenseProofAssetSet::Find(const FString& ObjectPath) const
{
	UObject* const* Found = Objects.Find(ObjectPath);
	return Found ? *Found : nullptr;
}

bool FDefenseAssetValidationService::ParseManifestJson(
	const FString& Json,
	FDefenseProofManifest& OutManifest,
	TArray<FString>& OutErrors)
{
	OutManifest = {};
	OutErrors.Reset();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutErrors.Add(TEXT("manifest JSON is malformed or is not an object"));
		return false;
	}

	RejectUnknownFields(Root,
		{TEXT("schemaVersion"), TEXT("gate"), TEXT("map"), TEXT("defenseConfiguration"),
		 TEXT("fixture"), TEXT("combatSettings"), TEXT("supportingAssets"), TEXT("attacks"), TEXT("presentations"),
		 TEXT("pairedDependencies"), TEXT("expectedCases")},
		TEXT("manifest"), OutErrors);
	double SchemaVersion = 0.0;
	if (!ReadRequiredNumber(Root, TEXT("schemaVersion"), TEXT("manifest"), SchemaVersion, OutErrors)
		|| SchemaVersion != 1.0)
	{
		AddError(OutErrors, TEXT("manifest"), TEXT("schemaVersion must be exactly 1"));
	}
	else
	{
		OutManifest.SchemaVersion = 1;
	}
	ReadRequiredString(Root, TEXT("gate"), TEXT("manifest"), OutManifest.Gate, OutErrors);
	if (OutManifest.Gate != TEXT("A") && OutManifest.Gate != TEXT("B"))
	{
		AddError(OutErrors, TEXT("manifest"), TEXT("gate must be 'A' or 'B'"));
	}
	ReadRequiredString(Root, TEXT("map"), TEXT("manifest"), OutManifest.Map, OutErrors);
	RequireGameObjectPath(OutManifest.Map, TEXT("manifest"), TEXT("map"), OutErrors);
	ReadRequiredString(Root, TEXT("defenseConfiguration"), TEXT("manifest"),
		OutManifest.DefenseConfiguration, OutErrors);
	RequireGameObjectPath(OutManifest.DefenseConfiguration, TEXT("manifest"),
		TEXT("defenseConfiguration"), OutErrors);
	const TSharedPtr<FJsonValue>* FixtureValue = Root->Values.Find(TEXT("fixture"));
	if (!FixtureValue || !FixtureValue->IsValid() || (*FixtureValue)->Type != EJson::Object)
	{
		AddError(OutErrors, TEXT("manifest"), TEXT("fixture must be an object"));
	}
	else
	{
		const TSharedPtr<FJsonObject> Fixture = (*FixtureValue)->AsObject();
		RejectUnknownFields(Fixture,
			{TEXT("playerBlueprint"), TEXT("playerCombatSettings"),
			 TEXT("enemyBlueprints"), TEXT("enemyCombatSettings"), TEXT("inputAction"),
			 TEXT("inputMappingContext"), TEXT("blockKey"), TEXT("guardAnimBlueprint"),
			 TEXT("reviewed")},
			TEXT("manifest.fixture"), OutErrors);
		ReadRequiredString(Fixture, TEXT("playerBlueprint"), TEXT("manifest.fixture"),
			OutManifest.Fixture.PlayerBlueprint, OutErrors);
		ReadRequiredString(Fixture, TEXT("playerCombatSettings"), TEXT("manifest.fixture"),
			OutManifest.Fixture.PlayerCombatSettings, OutErrors);
		ReadStringArray(Fixture, TEXT("enemyBlueprints"), TEXT("manifest.fixture"),
			OutManifest.Fixture.EnemyBlueprints, OutErrors);
		ReadStringArray(Fixture, TEXT("enemyCombatSettings"), TEXT("manifest.fixture"),
			OutManifest.Fixture.EnemyCombatSettings, OutErrors);
		ReadRequiredString(Fixture, TEXT("inputAction"), TEXT("manifest.fixture"),
			OutManifest.Fixture.InputAction, OutErrors);
		ReadRequiredString(Fixture, TEXT("inputMappingContext"), TEXT("manifest.fixture"),
			OutManifest.Fixture.InputMappingContext, OutErrors);
		ReadRequiredString(Fixture, TEXT("blockKey"), TEXT("manifest.fixture"),
			OutManifest.Fixture.BlockKey, OutErrors);
		if (OutManifest.Fixture.BlockKey != TEXT("ThumbMouseButton")
			&& OutManifest.Fixture.BlockKey != TEXT("ThumbMouseButton2"))
		{
			AddError(OutErrors, TEXT("manifest.fixture.blockKey"),
				TEXT("blockKey must be a thumb mouse button (ThumbMouseButton or ThumbMouseButton2)"));
		}
		ReadRequiredString(Fixture, TEXT("guardAnimBlueprint"), TEXT("manifest.fixture"),
			OutManifest.Fixture.GuardAnimBlueprint, OutErrors);
		ReadRequiredBool(Fixture, TEXT("reviewed"), TEXT("manifest.fixture"),
			OutManifest.Fixture.bReviewed, OutErrors);
		if (!OutManifest.Fixture.bReviewed)
		{
			AddError(OutErrors, TEXT("manifest.fixture"), TEXT("reviewed must be true"));
		}
		RequireGameObjectPath(OutManifest.Fixture.PlayerBlueprint, TEXT("manifest.fixture"),
			TEXT("playerBlueprint"), OutErrors);
		RequireGameObjectPath(OutManifest.Fixture.PlayerCombatSettings, TEXT("manifest.fixture"),
			TEXT("playerCombatSettings"), OutErrors);
		for (const FString& Path : OutManifest.Fixture.EnemyBlueprints)
		{
			RequireGameObjectPath(Path, TEXT("manifest.fixture.enemyBlueprints"), TEXT("entry"), OutErrors);
		}
		for (const FString& Path : OutManifest.Fixture.EnemyCombatSettings)
		{
			RequireGameObjectPath(Path, TEXT("manifest.fixture.enemyCombatSettings"), TEXT("entry"), OutErrors);
		}
		if (OutManifest.Fixture.EnemyBlueprints.Num()
			!= OutManifest.Fixture.EnemyCombatSettings.Num())
		{
			AddError(OutErrors, TEXT("manifest.fixture"),
				TEXT("enemyBlueprints and enemyCombatSettings must have equal cardinality"));
		}
		RequireGameObjectPath(OutManifest.Fixture.InputAction, TEXT("manifest.fixture"),
			TEXT("inputAction"), OutErrors);
		RequireGameObjectPath(OutManifest.Fixture.InputMappingContext, TEXT("manifest.fixture"),
			TEXT("inputMappingContext"), OutErrors);
		RequireGameObjectPath(OutManifest.Fixture.GuardAnimBlueprint, TEXT("manifest.fixture"),
			TEXT("guardAnimBlueprint"), OutErrors);
	}
	ReadStringArray(Root, TEXT("combatSettings"), TEXT("manifest"),
		OutManifest.CombatSettings, OutErrors);
	for (const FString& Path : OutManifest.CombatSettings)
	{
		RequireGameObjectPath(Path, TEXT("manifest.combatSettings"), TEXT("entry"), OutErrors);
	}
	ReadStringArray(Root, TEXT("supportingAssets"), TEXT("manifest"),
		OutManifest.SupportingAssets, OutErrors, false);
	for (const FString& Path : OutManifest.SupportingAssets)
	{
		RequireGameObjectPath(Path, TEXT("manifest.supportingAssets"), TEXT("entry"), OutErrors);
	}
	if (!OutManifest.CombatSettings.Contains(OutManifest.Fixture.PlayerCombatSettings))
	{
		AddError(OutErrors, TEXT("manifest.fixture.playerCombatSettings"),
			TEXT("path must also appear in combatSettings"));
	}
	for (const FString& Path : OutManifest.Fixture.EnemyCombatSettings)
	{
		if (!OutManifest.CombatSettings.Contains(Path))
		{
			AddError(OutErrors, TEXT("manifest.fixture.enemyCombatSettings"),
				FString::Printf(TEXT("%s must also appear in combatSettings"), *Path));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* AttackValues = nullptr;
	if (ReadObjectArray(Root, TEXT("attacks"), AttackValues, OutErrors))
	{
		TSet<FString> Names;
		TSet<FString> AttackDataPaths;
		TSet<FString> MontageSections;
		for (int32 Index = 0; Index < AttackValues->Num(); ++Index)
		{
			const FString Context = FString::Printf(TEXT("attacks[%d]"), Index);
			const TSharedPtr<FJsonValue>& Value = (*AttackValues)[Index];
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				AddError(OutErrors, Context, TEXT("entry must be an object"));
				continue;
			}
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			RejectUnknownFields(Object,
				{TEXT("name"), TEXT("attackData"), TEXT("montage"), TEXT("section"),
				 TEXT("expectedHeight"), TEXT("expectedLane"), TEXT("expectedSwing"),
				 TEXT("expectedSourceSocket"), TEXT("expectedTargetBone"),
				 TEXT("requiresBlockedImpactAudio"), TEXT("requiresBlockedImpactVFX"),
				 TEXT("expectedTags"), TEXT("parryWindow")},
				Context, OutErrors);
			FDefenseProofAttackEntry Entry;
			ReadRequiredString(Object, TEXT("name"), Context, Entry.Name, OutErrors);
			ReadRequiredString(Object, TEXT("attackData"), Context, Entry.AttackData, OutErrors);
			ReadRequiredString(Object, TEXT("montage"), Context, Entry.Montage, OutErrors);
			ReadRequiredString(Object, TEXT("section"), Context, Entry.Section, OutErrors);
			ReadRequiredString(Object, TEXT("expectedHeight"), Context, Entry.ExpectedHeight, OutErrors);
			ReadRequiredString(Object, TEXT("expectedLane"), Context, Entry.ExpectedLane, OutErrors);
			ReadRequiredString(Object, TEXT("expectedSwing"), Context, Entry.ExpectedSwing, OutErrors);
			ReadRequiredString(Object, TEXT("expectedSourceSocket"), Context,
				Entry.ExpectedSourceSocket, OutErrors);
			ReadRequiredString(Object, TEXT("expectedTargetBone"), Context,
				Entry.ExpectedTargetBone, OutErrors);
			ReadRequiredBool(Object, TEXT("requiresBlockedImpactAudio"), Context,
				Entry.bRequiresBlockedImpactAudio, OutErrors);
			ReadRequiredBool(Object, TEXT("requiresBlockedImpactVFX"), Context,
				Entry.bRequiresBlockedImpactVFX, OutErrors);
			ReadStringArray(Object, TEXT("expectedTags"), Context, Entry.ExpectedTags, OutErrors);
			TSet<FString> UniqueExpectedTags;
			for (const FString& TagName : Entry.ExpectedTags)
			{
				if (UniqueExpectedTags.Contains(TagName))
				{
					AddError(OutErrors, Context,
						FString::Printf(TEXT("expectedTags contains duplicate '%s'"), *TagName));
				}
				UniqueExpectedTags.Add(TagName);
				if (!FGameplayTag::RequestGameplayTag(FName(*TagName), false).IsValid())
				{
					AddError(OutErrors, Context,
						FString::Printf(TEXT("expected tag is not registered: %s"), *TagName));
				}
			}
			RequireGameObjectPath(Entry.AttackData, Context, TEXT("attackData"), OutErrors);
			RequireGameObjectPath(Entry.Montage, Context, TEXT("montage"), OutErrors);
			if (!IsNamedEnumValue(StaticEnum<EAttackHeight>(), Entry.ExpectedHeight))
			{
				AddError(OutErrors, Context, TEXT("expectedHeight is unknown"));
			}
			if (!IsNamedEnumValue(StaticEnum<EIncomingAttackLane>(), Entry.ExpectedLane))
			{
				AddError(OutErrors, Context, TEXT("expectedLane is unknown"));
			}
			if (!IsNamedEnumValue(StaticEnum<ESwingDirection>(), Entry.ExpectedSwing))
			{
				AddError(OutErrors, Context, TEXT("expectedSwing is unknown"));
			}
			const TSharedPtr<FJsonValue>* WindowValue = Object->Values.Find(TEXT("parryWindow"));
			if (!WindowValue || !WindowValue->IsValid())
			{
				AddError(OutErrors, Context, TEXT("parryWindow must be an object or null"));
			}
			else if ((*WindowValue)->Type == EJson::Null)
			{
				Entry.ParryWindow.bPresent = false;
			}
			else if ((*WindowValue)->Type != EJson::Object)
			{
				AddError(OutErrors, Context, TEXT("parryWindow must be an object or null"));
			}
			else
			{
				Entry.ParryWindow.bPresent = true;
				const TSharedPtr<FJsonObject> Window = (*WindowValue)->AsObject();
				const FString WindowContext = Context + TEXT(".parryWindow");
				RejectUnknownFields(Window,
					{TEXT("basis"), TEXT("startSeconds"), TEXT("endSeconds"), TEXT("reviewed")},
					WindowContext, OutErrors);
				ReadRequiredString(Window, TEXT("basis"), WindowContext,
					Entry.ParryWindow.Basis, OutErrors);
				if (Entry.ParryWindow.Basis != TEXT("SectionRelative"))
				{
					AddError(OutErrors, WindowContext, TEXT("basis must be 'SectionRelative'"));
				}
				ReadRequiredNumber(Window, TEXT("startSeconds"), WindowContext,
					Entry.ParryWindow.StartSeconds, OutErrors);
				ReadRequiredNumber(Window, TEXT("endSeconds"), WindowContext,
					Entry.ParryWindow.EndSeconds, OutErrors);
				ReadRequiredBool(Window, TEXT("reviewed"), WindowContext,
					Entry.ParryWindow.bReviewed, OutErrors);
				if (!Entry.ParryWindow.bReviewed)
				{
					AddError(OutErrors, WindowContext, TEXT("reviewed must be true"));
				}
				if (Entry.ParryWindow.StartSeconds < 0.0
					|| Entry.ParryWindow.EndSeconds <= Entry.ParryWindow.StartSeconds)
				{
					AddError(OutErrors, WindowContext,
						TEXT("endSeconds must be greater than a nonnegative startSeconds"));
				}
			}
			const bool bExpectsParryable = Entry.ExpectedTags.Contains(TEXT("Attack.Defense.Parryable"));
			if (bExpectsParryable != Entry.ParryWindow.bPresent)
			{
				AddError(OutErrors, Context,
					TEXT("Attack.Defense.Parryable and parryWindow presence must agree"));
			}
			if (AttackDataPaths.Contains(Entry.AttackData))
			{
				AddError(OutErrors, Context,
					FString::Printf(TEXT("attackData path '%s' is targeted more than once"),
						*Entry.AttackData));
			}
			AttackDataPaths.Add(Entry.AttackData);
			const FString MontageSectionKey = Entry.Montage + TEXT("|") + Entry.Section;
			if (MontageSections.Contains(MontageSectionKey))
			{
				AddError(OutErrors, Context,
					FString::Printf(TEXT("montage/section target '%s|%s' is targeted more than once"),
						*Entry.Montage, *Entry.Section));
			}
			MontageSections.Add(MontageSectionKey);
			RejectDuplicateName(Entry, Names, Context, OutErrors);
			OutManifest.Attacks.Add(MoveTemp(Entry));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* PresentationValues = nullptr;
	if (ReadObjectArray(Root, TEXT("presentations"), PresentationValues, OutErrors))
	{
		TSet<FString> Names;
		for (int32 Index = 0; Index < PresentationValues->Num(); ++Index)
		{
			const FString Context = FString::Printf(TEXT("presentations[%d]"), Index);
			const TSharedPtr<FJsonValue>& Value = (*PresentationValues)[Index];
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				AddError(OutErrors, Context, TEXT("entry must be an object"));
				continue;
			}
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			RejectUnknownFields(Object,
				{TEXT("name"), TEXT("outcome"), TEXT("attackerResponse"),
				 TEXT("defenderRow"), TEXT("attackerRow"),
				 TEXT("requiresDefenderMontage"), TEXT("requiresAttackerMontage"),
				 TEXT("requiresImpactAudio"), TEXT("requiresImpactVFX"),
				 TEXT("expectedSourceSocket"), TEXT("expectedTargetBone"), TEXT("reviewed")},
				Context, OutErrors);
			FDefenseProofPresentationEntry Entry;
			ReadRequiredString(Object, TEXT("name"), Context, Entry.Name, OutErrors);
			ReadRequiredString(Object, TEXT("outcome"), Context, Entry.Outcome, OutErrors);
			ReadRequiredString(Object, TEXT("attackerResponse"), Context,
				Entry.AttackerResponse, OutErrors);
			ReadRequiredNullableString(Object, TEXT("defenderRow"), Context,
				Entry.DefenderRow, Entry.bHasDefenderRow, OutErrors);
			ReadRequiredNullableString(Object, TEXT("attackerRow"), Context,
				Entry.AttackerRow, Entry.bHasAttackerRow, OutErrors);
			ReadRequiredBool(Object, TEXT("requiresDefenderMontage"), Context,
				Entry.bRequiresDefenderMontage, OutErrors);
			ReadRequiredBool(Object, TEXT("requiresAttackerMontage"), Context,
				Entry.bRequiresAttackerMontage, OutErrors);
			ReadRequiredBool(Object, TEXT("requiresImpactAudio"), Context,
				Entry.bRequiresImpactAudio, OutErrors);
			ReadRequiredBool(Object, TEXT("requiresImpactVFX"), Context,
				Entry.bRequiresImpactVFX, OutErrors);
			ReadRequiredString(Object, TEXT("expectedSourceSocket"), Context,
				Entry.ExpectedSourceSocket, OutErrors);
			ReadRequiredString(Object, TEXT("expectedTargetBone"), Context,
				Entry.ExpectedTargetBone, OutErrors);
			ReadRequiredBool(Object, TEXT("reviewed"), Context, Entry.bReviewed, OutErrors);
			if (!Entry.bReviewed)
			{
				AddError(OutErrors, Context, TEXT("reviewed must be true"));
			}
			if (!IsNamedEnumValue(StaticEnum<EDefenseOutcome>(), Entry.Outcome))
			{
				AddError(OutErrors, Context, TEXT("outcome is unknown"));
			}
			if (!IsNamedEnumValue(StaticEnum<EAttackerResponse>(), Entry.AttackerResponse))
			{
				AddError(OutErrors, Context, TEXT("attackerResponse is unknown"));
			}
			if (Entry.bRequiresDefenderMontage && !Entry.bHasDefenderRow)
			{
				AddError(OutErrors, Context,
					TEXT("requiresDefenderMontage requires a defenderRow"));
			}
			if (Entry.bRequiresAttackerMontage && !Entry.bHasAttackerRow)
			{
				AddError(OutErrors, Context,
					TEXT("requiresAttackerMontage requires an attackerRow"));
			}
			RejectDuplicateName(Entry, Names, Context, OutErrors);
			OutManifest.Presentations.Add(MoveTemp(Entry));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* PairedValues = nullptr;
	if (ReadObjectArray(Root, TEXT("pairedDependencies"), PairedValues, OutErrors))
	{
		TSet<FString> Names;
		TSet<FString> PairedDataPaths;
		for (int32 Index = 0; Index < PairedValues->Num(); ++Index)
		{
			const FString Context = FString::Printf(TEXT("pairedDependencies[%d]"), Index);
			const TSharedPtr<FJsonValue>& Value = (*PairedValues)[Index];
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				AddError(OutErrors, Context, TEXT("entry must be an object"));
				continue;
			}
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			RejectUnknownFields(Object,
				{TEXT("name"), TEXT("role"), TEXT("pairedData"),
				 TEXT("attackerMontage"), TEXT("attackerSection"),
				 TEXT("victimMontage"), TEXT("victimSection"),
				 TEXT("driverRole"), TEXT("driverMarker"),
				 TEXT("attackerWarpTarget"), TEXT("victimWarpTarget"),
				 TEXT("attackerReadySection"), TEXT("victimReadySection"),
				 TEXT("attackerTerminalPoseCompatible"), TEXT("victimTerminalPoseCompatible"),
				 TEXT("reviewed")}, Context, OutErrors);
			FDefenseProofPairedDependencyEntry Entry;
			ReadRequiredString(Object, TEXT("name"), Context, Entry.Name, OutErrors);
			ReadRequiredString(Object, TEXT("role"), Context, Entry.Role, OutErrors);
			ReadRequiredString(Object, TEXT("pairedData"), Context, Entry.PairedData, OutErrors);
			ReadRequiredString(Object, TEXT("attackerMontage"), Context,
				Entry.AttackerMontage, OutErrors);
			ReadRequiredString(Object, TEXT("attackerSection"), Context,
				Entry.AttackerSection, OutErrors);
			ReadRequiredString(Object, TEXT("victimMontage"), Context,
				Entry.VictimMontage, OutErrors);
			ReadRequiredString(Object, TEXT("victimSection"), Context,
				Entry.VictimSection, OutErrors);
			ReadRequiredNullableString(Object, TEXT("driverRole"), Context,
				Entry.DriverRole, Entry.bHasDriverRole, OutErrors);
			ReadRequiredNullableString(Object, TEXT("driverMarker"), Context,
				Entry.DriverMarker, Entry.bHasDriverMarker, OutErrors);
			ReadRequiredString(Object, TEXT("attackerWarpTarget"), Context,
				Entry.AttackerWarpTarget, OutErrors);
			ReadRequiredString(Object, TEXT("victimWarpTarget"), Context,
				Entry.VictimWarpTarget, OutErrors);
			ReadRequiredNullableString(Object, TEXT("attackerReadySection"), Context,
				Entry.AttackerReadySection, Entry.bHasAttackerReadySection, OutErrors);
			ReadRequiredNullableString(Object, TEXT("victimReadySection"), Context,
				Entry.VictimReadySection, Entry.bHasVictimReadySection, OutErrors);
			ReadRequiredBool(Object, TEXT("attackerTerminalPoseCompatible"), Context,
				Entry.bAttackerTerminalPoseCompatible, OutErrors);
			ReadRequiredBool(Object, TEXT("victimTerminalPoseCompatible"), Context,
				Entry.bVictimTerminalPoseCompatible, OutErrors);
			ReadRequiredBool(Object, TEXT("reviewed"), Context, Entry.bReviewed, OutErrors);
			RequireGameObjectPath(Entry.PairedData, Context, TEXT("pairedData"), OutErrors);
			RequireGameObjectPath(Entry.AttackerMontage, Context, TEXT("attackerMontage"), OutErrors);
			RequireGameObjectPath(Entry.VictimMontage, Context, TEXT("victimMontage"), OutErrors);
			if (Entry.Role != TEXT("Bridge") && Entry.Role != TEXT("Counter") && Entry.Role != TEXT("Finisher"))
			{
				AddError(OutErrors, Context, TEXT("role must be Bridge, Counter, or Finisher"));
			}
			if (Entry.bHasDriverRole
				&& Entry.DriverRole != TEXT("Attacker") && Entry.DriverRole != TEXT("Victim"))
			{
				AddError(OutErrors, Context, TEXT("driverRole must be Attacker or Victim"));
			}
			if (Entry.bHasDriverRole != Entry.bHasDriverMarker)
			{
				AddError(OutErrors, Context,
					TEXT("driverRole and driverMarker must both be strings or both be null"));
			}
			if (Entry.Role != TEXT("Finisher") && !Entry.bHasDriverMarker)
			{
				AddError(OutErrors, Context,
					TEXT("Bridge and Counter dependencies require a driver marker"));
			}
			if (Entry.Role == TEXT("Finisher") && Entry.bHasDriverMarker)
			{
				AddError(OutErrors, Context,
					TEXT("Finisher dependencies must terminate without a driver marker"));
			}
			if (Entry.Role != TEXT("Finisher")
				&& (!(Entry.bHasAttackerReadySection || Entry.bAttackerTerminalPoseCompatible)
					|| !(Entry.bHasVictimReadySection || Entry.bVictimTerminalPoseCompatible)))
			{
				AddError(OutErrors, Context,
					TEXT("retained paired stages require reviewed ready-pose ownership"));
			}
			if (!Entry.bReviewed)
			{
				AddError(OutErrors, Context, TEXT("reviewed must be true"));
			}
			if (PairedDataPaths.Contains(Entry.PairedData))
			{
				AddError(OutErrors, Context,
					FString::Printf(TEXT("pairedData path '%s' is targeted more than once"),
						*Entry.PairedData));
			}
			PairedDataPaths.Add(Entry.PairedData);
			RejectDuplicateName(Entry, Names, Context, OutErrors);
			OutManifest.PairedDependencies.Add(MoveTemp(Entry));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* CaseValues = nullptr;
	if (ReadObjectArray(Root, TEXT("expectedCases"), CaseValues, OutErrors))
	{
		TSet<FString> Names;
		for (int32 Index = 0; Index < CaseValues->Num(); ++Index)
		{
			const FString Context = FString::Printf(TEXT("expectedCases[%d]"), Index);
			const TSharedPtr<FJsonValue>& Value = (*CaseValues)[Index];
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				AddError(OutErrors, Context, TEXT("entry must be an object"));
				continue;
			}
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			RejectUnknownFields(Object,
				{TEXT("name"), TEXT("attack"), TEXT("outcome"), TEXT("reason"), TEXT("attackerResponse"),
				 TEXT("presentation"), TEXT("pairedDependencies"), TEXT("reviewed")},
				Context, OutErrors);
			FDefenseProofExpectedCaseEntry Entry;
			ReadRequiredString(Object, TEXT("name"), Context, Entry.Name, OutErrors);
			ReadRequiredString(Object, TEXT("attack"), Context, Entry.Attack, OutErrors);
			ReadRequiredString(Object, TEXT("outcome"), Context, Entry.Outcome, OutErrors);
			ReadRequiredString(Object, TEXT("reason"), Context, Entry.Reason, OutErrors);
			ReadRequiredString(Object, TEXT("attackerResponse"), Context,
				Entry.AttackerResponse, OutErrors);
			ReadRequiredNullableString(Object, TEXT("presentation"), Context,
				Entry.Presentation, Entry.bHasPresentation, OutErrors);
			ReadStringArray(Object, TEXT("pairedDependencies"), Context,
				Entry.PairedDependencies, OutErrors, false);
			ReadRequiredBool(Object, TEXT("reviewed"), Context, Entry.bReviewed, OutErrors);
			if (!Entry.bReviewed)
			{
				AddError(OutErrors, Context, TEXT("reviewed must be true"));
			}
			if (!IsNamedEnumValue(StaticEnum<EDefenseOutcome>(), Entry.Outcome))
			{
				AddError(OutErrors, Context, TEXT("outcome is unknown"));
			}
			if (!IsNamedEnumValue(StaticEnum<EDefenseReason>(), Entry.Reason))
			{
				AddError(OutErrors, Context, TEXT("reason is unknown"));
			}
			if (!IsNamedEnumValue(StaticEnum<EAttackerResponse>(), Entry.AttackerResponse))
			{
				AddError(OutErrors, Context, TEXT("attackerResponse is unknown"));
			}
			if (Entry.Outcome == TEXT("PerfectParry") && Entry.PairedDependencies.IsEmpty())
			{
				AddError(OutErrors, Context,
					TEXT("PerfectParry expected cases require pairedDependencies"));
			}
			if (Entry.Outcome == TEXT("PerfectParry")
				&& Entry.AttackerResponse != TEXT("ParryStagger"))
			{
				AddError(OutErrors, Context,
					TEXT("PerfectParry expected cases require ParryStagger"));
			}
			if (Entry.Outcome == TEXT("NormalBlock")
				&& Entry.AttackerResponse != TEXT("Continue")
				&& Entry.AttackerResponse != TEXT("Recoil"))
			{
				AddError(OutErrors, Context,
					TEXT("NormalBlock expected cases require Continue or Recoil"));
			}
			RejectDuplicateName(Entry, Names, Context, OutErrors);
			OutManifest.ExpectedCases.Add(MoveTemp(Entry));
		}
	}

	TSet<FString> AttackNames;
	for (const FDefenseProofAttackEntry& Entry : OutManifest.Attacks) AttackNames.Add(Entry.Name);
	TMap<FString, const FDefenseProofPresentationEntry*> PresentationsByName;
	for (const FDefenseProofPresentationEntry& Entry : OutManifest.Presentations)
	{
		PresentationsByName.Add(Entry.Name, &Entry);
	}
	TMap<FString, const FDefenseProofPairedDependencyEntry*> PairedByName;
	for (const FDefenseProofPairedDependencyEntry& Entry : OutManifest.PairedDependencies)
	{
		PairedByName.Add(Entry.Name, &Entry);
	}
	TMap<FString, FString> PairedRoleAuthorityByAttack;
	for (const FDefenseProofExpectedCaseEntry& Entry : OutManifest.ExpectedCases)
	{
		const FString Context = FString::Printf(TEXT("expectedCases.%s"), *Entry.Name);
		if (!AttackNames.Contains(Entry.Attack))
		{
			AddError(OutErrors, Context, TEXT("attack reference does not exist"));
		}
		const FDefenseProofPresentationEntry* const* Presentation = Entry.bHasPresentation
			? PresentationsByName.Find(Entry.Presentation)
			: nullptr;
		if (Entry.bHasPresentation && !Presentation)
		{
			AddError(OutErrors, Context, TEXT("presentation reference does not exist"));
		}
		else if (Presentation
			&& ((*Presentation)->Outcome != Entry.Outcome
				|| (*Presentation)->AttackerResponse != Entry.AttackerResponse))
		{
			AddError(OutErrors, Context,
				TEXT("referenced presentation outcome/attackerResponse does not match the expected case"));
		}
		if (!Entry.bHasPresentation
			&& (Entry.Outcome == TEXT("NormalBlock") || Entry.Outcome == TEXT("PerfectParry")))
		{
			AddError(OutErrors, Context,
				TEXT("NormalBlock and PerfectParry expected cases require a presentation"));
		}
		for (const FString& Dependency : Entry.PairedDependencies)
		{
			const FDefenseProofPairedDependencyEntry* const* PairedEntry =
				PairedByName.Find(Dependency);
			if (!PairedEntry)
			{
				AddError(OutErrors, Context,
					FString::Printf(TEXT("paired dependency '%s' does not exist"), *Dependency));
				continue;
			}
			if ((*PairedEntry)->Role == TEXT("Counter")
				|| (*PairedEntry)->Role == TEXT("Finisher"))
			{
				const FString AuthorityKey = Entry.Attack + TEXT("|") + (*PairedEntry)->Role;
				const FString* ExistingAuthority = PairedRoleAuthorityByAttack.Find(AuthorityKey);
				if (ExistingAuthority && *ExistingAuthority != Dependency)
				{
					AddError(OutErrors, Context, FString::Printf(
						TEXT("ambiguous %s authority for attack '%s': '%s' and '%s'"),
						*(*PairedEntry)->Role, *Entry.Attack, **ExistingAuthority, *Dependency));
				}
				else
				{
					PairedRoleAuthorityByAttack.Add(AuthorityKey, Dependency);
				}
			}
		}
	}
	return OutErrors.IsEmpty();
}

bool FDefenseAssetValidationService::LoadManifestFile(
	const FString& ManifestPath,
	FDefenseProofManifest& OutManifest,
	TArray<FString>& OutErrors)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *ManifestPath))
	{
		OutErrors = {FString::Printf(TEXT("could not read manifest file '%s'"), *ManifestPath)};
		return false;
	}
	const bool bParsed = ParseManifestJson(Json, OutManifest, OutErrors);
	if (bParsed)
	{
		OutManifest.SourcePath = ManifestPath;
	}
	return bParsed;
}

TArray<FString> FDefenseAssetValidationService::CollectExplicitObjectPaths(
	const FDefenseProofManifest& Manifest)
{
	TSet<FString> UniquePaths;
	const auto AddPath = [&UniquePaths](const FString& Path)
	{
		if (!Path.IsEmpty())
		{
			UniquePaths.Add(Path);
		}
	};

	AddPath(Manifest.Map);
	AddPath(Manifest.DefenseConfiguration);
	AddPath(Manifest.Fixture.PlayerBlueprint);
	AddPath(Manifest.Fixture.PlayerCombatSettings);
	for (const FString& Path : Manifest.Fixture.EnemyBlueprints)
	{
		AddPath(Path);
	}
	for (const FString& Path : Manifest.Fixture.EnemyCombatSettings)
	{
		AddPath(Path);
	}
	AddPath(Manifest.Fixture.InputAction);
	AddPath(Manifest.Fixture.InputMappingContext);
	AddPath(Manifest.Fixture.GuardAnimBlueprint);
	for (const FString& Path : Manifest.CombatSettings)
	{
		AddPath(Path);
	}
	for (const FString& Path : Manifest.SupportingAssets)
	{
		AddPath(Path);
	}
	for (const FDefenseProofAttackEntry& Entry : Manifest.Attacks)
	{
		AddPath(Entry.AttackData);
		AddPath(Entry.Montage);
	}
	for (const FDefenseProofPairedDependencyEntry& Entry : Manifest.PairedDependencies)
	{
		AddPath(Entry.PairedData);
		AddPath(Entry.AttackerMontage);
		AddPath(Entry.VictimMontage);
	}

	TArray<FString> Paths = UniquePaths.Array();
	Paths.Sort();
	return Paths;
}

void FDefenseAssetValidationService::LoadExplicitObjects(
	const FDefenseProofManifest& Manifest,
	FDefenseProofAssetSet& OutAssets)
{
	OutAssets.Objects.Reset();
	for (const FString& Path : CollectExplicitObjectPaths(Manifest))
	{
		OutAssets.Add(Path, StaticLoadObject(UObject::StaticClass(), nullptr, *Path));
	}
}

void FDefenseAssetValidationService::ValidateManifestObjects(
	const FDefenseProofManifest& Manifest,
	const FDefenseProofAssetSet& Assets,
	FDefenseAssetValidationResult& OutResult)
{
	const TArray<FString> ExplicitPaths = CollectExplicitObjectPaths(Manifest);
	for (const FString& Path : ExplicitPaths)
	{
		OutResult.Dependencies.AddUnique(Path);
		if (!Assets.Find(Path))
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("MissingManifestAsset"), Path,
				TEXT("explicit manifest object did not load"));
		}
	}

	const UWorld* ProofWorld = FindTypedObject<UWorld>(
		Assets, Manifest.Map, TEXT("manifest.map"), OutResult);
	FDefenseAssetValidationRow MapRow;
	MapRow.Kind = TEXT("Fixture");
	MapRow.Name = TEXT("Map");
	MapRow.AssetPath = Manifest.Map;
	MapRow.Facts.Add(TEXT("loaded"), LexToString(ProofWorld != nullptr));
	OutResult.Rows.Add(MoveTemp(MapRow));

	const UDefenseConfiguration* Configuration = FindTypedObject<UDefenseConfiguration>(
		Assets, Manifest.DefenseConfiguration, TEXT("manifest.defenseConfiguration"), OutResult);
	FDefenseAssetValidationRow ConfigurationRow;
	ConfigurationRow.Kind = TEXT("Configuration");
	ConfigurationRow.Name = TEXT("DefenseConfiguration");
	ConfigurationRow.AssetPath = Manifest.DefenseConfiguration;
	ConfigurationRow.Facts.Add(TEXT("loaded"), LexToString(Configuration != nullptr));
	if (Configuration)
	{
		ConfigurationRow.Facts.Add(TEXT("hard_guard_cone_half_angle"),
			LexToString(Configuration->HardGuardConeHalfAngle));
		ConfigurationRow.Facts.Add(TEXT("maximum_automatic_turn"),
			LexToString(Configuration->MaximumAutomaticTurn));
		ConfigurationRow.Facts.Add(TEXT("defense_turn_rate"),
			LexToString(Configuration->DefenseTurnRate));
		ConfigurationRow.Facts.Add(TEXT("normal_block_final_tolerance"),
			LexToString(Configuration->NormalBlockFinalTolerance));
		ConfigurationRow.Facts.Add(TEXT("perfect_parry_final_tolerance"),
			LexToString(Configuration->PerfectParryFinalTolerance));
		ConfigurationRow.Facts.Add(TEXT("center_lane_half_angle"),
			LexToString(Configuration->CenterLaneHalfAngle));
		ConfigurationRow.Facts.Add(TEXT("threat_lock_min_seconds"),
			LexToString(Configuration->ThreatLockMinSeconds));
		ConfigurationRow.Facts.Add(TEXT("threat_switch_lead_seconds"),
			LexToString(Configuration->ThreatSwitchLeadSeconds));
		ConfigurationRow.Facts.Add(TEXT("guarded_threat_refresh_seconds"),
			LexToString(Configuration->GuardedThreatRefreshSeconds));
		ConfigurationRow.Facts.Add(TEXT("maximum_prediction_age"),
			LexToString(Configuration->MaximumHighConfidencePredictionAge));
		ConfigurationRow.Facts.Add(TEXT("defense_threat_range"),
			LexToString(Configuration->DefenseThreatRange));
		ConfigurationRow.Facts.Add(TEXT("guard_manual_override_threshold"),
			LexToString(Configuration->GuardManualOverrideThreshold));
		ConfigurationRow.Facts.Add(TEXT("guard_auto_facing_resume_seconds"),
			LexToString(Configuration->GuardAutoFacingResumeSeconds));
		ConfigurationRow.Facts.Add(TEXT("interaction_tombstone_seconds"),
			LexToString(Configuration->InteractionTombstoneSeconds));
		ConfigurationRow.Facts.Add(TEXT("terminal_interaction_cache_cap"),
			LexToString(Configuration->TerminalInteractionCacheCap));
		ConfigurationRow.Facts.Add(TEXT("no_montage_parry_bridge_seconds"),
			LexToString(Configuration->NoMontageParryBridgeSeconds));
		ConfigurationRow.Facts.Add(TEXT("parry_stagger_duration"),
			LexToString(Configuration->ParryStaggerDuration));
		ConfigurationRow.Facts.Add(TEXT("counter_window_seconds"),
			LexToString(Configuration->CounterWindowSeconds));
		ConfigurationRow.Facts.Add(TEXT("finisher_ready_seconds"),
			LexToString(Configuration->FinisherReadySeconds));
		ConfigurationRow.Facts.Add(TEXT("time_dilation_watchdog_seconds"),
			LexToString(Configuration->TimeDilationLeaseWatchdogSeconds));
		ConfigurationRow.Facts.Add(TEXT("normal_block_translation_allowance"),
			LexToString(Configuration->NormalBlockTranslationAllowance));
		ConfigurationRow.Facts.Add(TEXT("normal_block_translation_drift_tolerance"),
			LexToString(Configuration->NormalBlockTranslationDriftTolerance));
		ConfigurationRow.Facts.Add(TEXT("perfect_parry_translation_allowance_per_role"),
			LexToString(Configuration->PerfectParryTranslationAllowancePerRole));
		ConfigurationRow.Facts.Add(TEXT("guard_enter_montage"), Configuration->GuardEnterMontage
			? Configuration->GuardEnterMontage->GetPathName() : TEXT("None"));
		ConfigurationRow.Facts.Add(TEXT("guard_exit_montage"), Configuration->GuardExitMontage
			? Configuration->GuardExitMontage->GetPathName() : TEXT("None"));
		ConfigurationRow.Facts.Add(TEXT("default_block_audio"),
			Configuration->DefaultBlockImpactAudio.ImpactSound
				? Configuration->DefaultBlockImpactAudio.ImpactSound->GetPathName() : TEXT("None"));
		ConfigurationRow.Facts.Add(TEXT("default_block_vfx"),
			Configuration->DefaultBlockImpactVFX.ImpactVFX
				? Configuration->DefaultBlockImpactVFX.ImpactVFX->GetPathName() : TEXT("None"));
		ConfigurationRow.Facts.Add(TEXT("default_parry_audio"),
			Configuration->DefaultParryImpactAudio.ImpactSound
				? Configuration->DefaultParryImpactAudio.ImpactSound->GetPathName() : TEXT("None"));
		ConfigurationRow.Facts.Add(TEXT("default_parry_vfx"),
			Configuration->DefaultParryImpactVFX.ImpactVFX
				? Configuration->DefaultParryImpactVFX.ImpactVFX->GetPathName() : TEXT("None"));
		ConfigurationRow.Facts.Add(TEXT("defender_presentation_rows"),
			LexToString(Configuration->DefenderPresentationRows.Num()));
		ConfigurationRow.Facts.Add(TEXT("attacker_presentation_rows"),
			LexToString(Configuration->AttackerResponseRows.Num()));
		ConfigurationRow.Facts.Add(TEXT("bone_height_rows"),
			LexToString(Configuration->BoneHeightRows.Num()));
	}
	OutResult.Rows.Add(MoveTemp(ConfigurationRow));
	for (const FString& SettingsPath : Manifest.CombatSettings)
	{
		const UCombatSettings* Settings = FindTypedObject<UCombatSettings>(
			Assets, SettingsPath, FString::Printf(TEXT("combatSettings.%s"), *SettingsPath), OutResult);
		if (Settings)
		{
			AddObjectDependency(Settings->DefenseConfiguration, OutResult);
			if (Configuration && Settings->DefenseConfiguration != Configuration)
			{
				OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
					TEXT("DefenseConfigurationAssignmentMismatch"), SettingsPath,
					TEXT("combat settings does not reference the manifest defense configuration"));
			}
		}
		FDefenseAssetValidationRow Row;
		Row.Kind = TEXT("CombatSettings");
		Row.Name = FPackageName::ObjectPathToObjectName(SettingsPath);
		Row.AssetPath = SettingsPath;
		Row.Facts.Add(TEXT("defense_configuration"),
			Settings && Settings->DefenseConfiguration
				? Settings->DefenseConfiguration->GetPathName()
				: TEXT("None"));
		OutResult.Rows.Add(MoveTemp(Row));
	}

	const UInputAction* BlockAction = FindTypedObject<UInputAction>(
		Assets, Manifest.Fixture.InputAction, TEXT("fixture.inputAction"), OutResult);
	const UInputMappingContext* MappingContext = FindTypedObject<UInputMappingContext>(
		Assets, Manifest.Fixture.InputMappingContext, TEXT("fixture.inputMappingContext"), OutResult);
	const FKey ExpectedBlockKey(FName(*Manifest.Fixture.BlockKey));
	if (!ExpectedBlockKey.IsValid())
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("BlockInputKeyInvalid"), TEXT("fixture.blockKey"),
			TEXT("manifest block key is not a registered input key"));
	}
	if (BlockAction && BlockAction->ValueType != EInputActionValueType::Boolean)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("BlockInputActionTypeMismatch"), TEXT("fixture.inputAction"),
			TEXT("block input action must use a Boolean value type"));
	}
	bool bHasExpectedMapping = false;
	bool bHasDeprecatedRightMouseMapping = false;
	bool bHasExpectedKeyConflict = false;
	if (BlockAction && MappingContext && ExpectedBlockKey.IsValid())
	{
		for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
		{
			bHasExpectedMapping |= Mapping.Action == BlockAction && Mapping.Key == ExpectedBlockKey;
			bHasDeprecatedRightMouseMapping |=
				Mapping.Action == BlockAction && Mapping.Key == EKeys::RightMouseButton;
			bHasExpectedKeyConflict |= Mapping.Key == ExpectedBlockKey
				&& Mapping.Action.Get() && Mapping.Action != BlockAction;
		}
		if (!bHasExpectedMapping)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("BlockInputMappingMissing"), TEXT("fixture.inputMappingContext"),
				TEXT("mapping context does not bind IA_Block to the reviewed thumb-mouse key"));
		}
		if (bHasDeprecatedRightMouseMapping)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("DeprecatedBlockInputMapping"), TEXT("fixture.inputMappingContext"),
				TEXT("IA_Block must not consume RightMouseButton reserved for heavy attack"));
		}
		if (bHasExpectedKeyConflict)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("BlockInputKeyConflict"), TEXT("fixture.inputMappingContext"),
				TEXT("the reviewed thumb-mouse key is also bound to another action"));
		}
	}
	FDefenseAssetValidationRow InputRow;
	InputRow.Kind = TEXT("Fixture");
	InputRow.Name = TEXT("BlockInput");
	InputRow.AssetPath = Manifest.Fixture.InputMappingContext;
	InputRow.Facts.Add(TEXT("action"), Manifest.Fixture.InputAction);
	InputRow.Facts.Add(TEXT("key"), Manifest.Fixture.BlockKey);
	InputRow.Facts.Add(TEXT("mapping_present"), LexToString(bHasExpectedMapping));
	InputRow.Facts.Add(TEXT("deprecated_right_mouse_mapping"),
		LexToString(bHasDeprecatedRightMouseMapping));
	InputRow.Facts.Add(TEXT("expected_key_conflict"), LexToString(bHasExpectedKeyConflict));
	OutResult.Rows.Add(MoveTemp(InputRow));

	UObject* GuardObject = Assets.Find(Manifest.Fixture.GuardAnimBlueprint);
	if (GuardObject)
	{
		ValidateGuardAnimBlueprint(Manifest.Fixture, GuardObject, OutResult);
		AddAnimBlueprintDependencies(Cast<UAnimBlueprint>(GuardObject), OutResult);
	}
	UClass* GuardClass = ResolveDeclaredClass(GuardObject);
	UClass* PlayerClass = ResolveDeclaredClass(Assets.Find(Manifest.Fixture.PlayerBlueprint));
	const APlayerCharacter* PlayerDefault = nullptr;
	if (!PlayerClass || !PlayerClass->IsChildOf(APlayerCharacter::StaticClass()))
	{
		if (Assets.Find(Manifest.Fixture.PlayerBlueprint))
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("PlayerBlueprintClassMismatch"), TEXT("fixture.playerBlueprint"),
				TEXT("player Blueprint must generate an APlayerCharacter subclass"));
		}
	}
	else
	{
		PlayerDefault = Cast<APlayerCharacter>(PlayerClass->GetDefaultObject());
		if (!PlayerDefault
			|| PlayerDefault->BlockAction != BlockAction
			|| PlayerDefault->DefaultMappingContext != MappingContext)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("PlayerBlockInputAssignmentMismatch"), TEXT("fixture.playerBlueprint"),
				TEXT("player defaults do not assign the manifest block action and mapping context"));
		}
		const UCombatSettings* ExpectedPlayerSettings = Cast<UCombatSettings>(
			Assets.Find(Manifest.Fixture.PlayerCombatSettings));
		if (!PlayerDefault || PlayerDefault->CombatSettings != ExpectedPlayerSettings)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("PlayerCombatSettingsAssignmentMismatch"), TEXT("fixture.playerBlueprint"),
				TEXT("player defaults do not assign one of the manifest combat settings"));
		}
		if (!PlayerDefault || !PlayerDefault->GetMesh()
			|| PlayerDefault->GetMesh()->GetAnimClass() != GuardClass)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("PlayerGuardAnimAssignmentMismatch"), TEXT("fixture.playerBlueprint"),
				TEXT("player mesh does not assign the manifest guard AnimBP"));
		}
	}

	for (int32 EnemyIndex = 0; EnemyIndex < Manifest.Fixture.EnemyBlueprints.Num(); ++EnemyIndex)
	{
		const FString& EnemyPath = Manifest.Fixture.EnemyBlueprints[EnemyIndex];
		UClass* EnemyClass = ResolveDeclaredClass(Assets.Find(EnemyPath));
		if (!EnemyClass || !EnemyClass->IsChildOf(ABaseCombatCharacter::StaticClass()))
		{
			if (Assets.Find(EnemyPath))
			{
				OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
					TEXT("EnemyBlueprintClassMismatch"), EnemyPath,
					TEXT("enemy Blueprint must generate an ABaseCombatCharacter subclass"));
			}
			continue;
		}
		const ABaseCombatCharacter* EnemyDefault =
			Cast<ABaseCombatCharacter>(EnemyClass->GetDefaultObject());
		const UCombatSettings* ExpectedEnemySettings =
			Manifest.Fixture.EnemyCombatSettings.IsValidIndex(EnemyIndex)
				? Cast<UCombatSettings>(Assets.Find(
					Manifest.Fixture.EnemyCombatSettings[EnemyIndex]))
				: nullptr;
		if (!EnemyDefault || EnemyDefault->CombatSettings != ExpectedEnemySettings)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("EnemyCombatSettingsAssignmentMismatch"), EnemyPath,
				TEXT("enemy defaults do not assign one of the manifest combat settings"));
		}
	}

	if (Configuration)
	{
		AddObjectDependency(Configuration->GuardEnterMontage, OutResult);
		AddMontageDependencies(Configuration->GuardEnterMontage, OutResult);
		AddObjectDependency(Configuration->GuardExitMontage, OutResult);
		AddMontageDependencies(Configuration->GuardExitMontage, OutResult);
		AddObjectDependency(Configuration->DefaultBlockImpactAudio.ImpactSound, OutResult);
		AddObjectDependency(Configuration->DefaultBlockImpactVFX.ImpactVFX, OutResult);
		AddObjectDependency(Configuration->DefaultParryImpactAudio.ImpactSound, OutResult);
		AddObjectDependency(Configuration->DefaultParryImpactVFX.ImpactVFX, OutResult);
		for (const FDefensePresentationRow& Row : Configuration->DefenderPresentationRows)
		{
			AddObjectDependency(Row.Payload.Montage, OutResult);
			AddMontageDependencies(Row.Payload.Montage, OutResult);
			AddObjectDependency(Row.Payload.ImpactAudio.ImpactSound, OutResult);
			AddObjectDependency(Row.Payload.ImpactVFX.ImpactVFX, OutResult);
			AddObjectDependency(Row.Payload.PairedBridgeData, OutResult);
		}
		for (const FAttackerResponsePresentationRow& Row : Configuration->AttackerResponseRows)
		{
			AddObjectDependency(Row.Payload.Montage, OutResult);
			AddMontageDependencies(Row.Payload.Montage, OutResult);
			AddObjectDependency(Row.Payload.ImpactAudio.ImpactSound, OutResult);
			AddObjectDependency(Row.Payload.ImpactVFX.ImpactVFX, OutResult);
			AddObjectDependency(Row.Payload.PairedBridgeData, OutResult);
		}
	}

	TMap<FString, const FDefenseProofAttackEntry*> AttackEntries;
	TMap<FString, const UAttackData*> AttackAssets;
	for (const FDefenseProofAttackEntry& Entry : Manifest.Attacks)
	{
		AttackEntries.Add(Entry.Name, &Entry);
		const UAttackData* AttackData = FindTypedObject<UAttackData>(
			Assets, Entry.AttackData, FString::Printf(TEXT("attack.%s"), *Entry.Name), OutResult);
		const UAnimMontage* Montage = FindTypedObject<UAnimMontage>(
			Assets, Entry.Montage, FString::Printf(TEXT("attack.%s.montage"), *Entry.Name), OutResult);
		AttackAssets.Add(Entry.Name, AttackData);
		if (AttackData && Montage)
		{
			ValidateAttackEntry(Entry, AttackData, Montage, OutResult);
			AddMontageDependencies(Montage, OutResult);
			AddObjectDependency(AttackData->AttackMontage, OutResult);
			AddObjectDependency(AttackData->CounterData, OutResult);
			AddObjectDependency(AttackData->FinisherData, OutResult);
			AddObjectDependency(AttackData->DefenseProfile.BlockedImpactAudio.ImpactSound, OutResult);
			AddObjectDependency(AttackData->DefenseProfile.BlockedImpactVFX.ImpactVFX, OutResult);
		}
		else
		{
			FDefenseAssetValidationRow Row;
			Row.Kind = TEXT("Attack");
			Row.Name = Entry.Name;
			Row.AssetPath = Entry.AttackData;
			Row.Facts.Add(TEXT("attack_data"), Entry.AttackData);
			Row.Facts.Add(TEXT("montage"), Entry.Montage);
			Row.Facts.Add(TEXT("section"), Entry.Section);
			Row.Facts.Add(TEXT("height"), Entry.ExpectedHeight);
			Row.Facts.Add(TEXT("lane"), Entry.ExpectedLane);
			Row.Facts.Add(TEXT("swing"), Entry.ExpectedSwing);
			Row.Facts.Add(TEXT("loaded"), TEXT("false"));
			OutResult.Rows.Add(MoveTemp(Row));
		}
	}

	TMap<FString, const FDefenseProofPairedDependencyEntry*> PairedEntries;
	TMap<FString, const UPairedAnimationData*> PairedByName;
	TArray<const UPairedAnimationData*> PairedAssets;
	for (const FDefenseProofPairedDependencyEntry& Entry : Manifest.PairedDependencies)
	{
		PairedEntries.Add(Entry.Name, &Entry);
		const UPairedAnimationData* PairedData = FindTypedObject<UPairedAnimationData>(
			Assets, Entry.PairedData, FString::Printf(TEXT("paired.%s"), *Entry.Name), OutResult);
		const UAnimMontage* ManifestAttackerMontage = FindTypedObject<UAnimMontage>(
			Assets, Entry.AttackerMontage,
			FString::Printf(TEXT("paired.%s.attackerMontage"), *Entry.Name), OutResult);
		const UAnimMontage* ManifestVictimMontage = FindTypedObject<UAnimMontage>(
			Assets, Entry.VictimMontage,
			FString::Printf(TEXT("paired.%s.victimMontage"), *Entry.Name), OutResult);
		PairedByName.Add(Entry.Name, PairedData);
		PairedAssets.Add(PairedData);
		ValidatePairedDependency(Entry, PairedData, Configuration, OutResult,
			ManifestAttackerMontage, ManifestVictimMontage);
		if (PairedData)
		{
			AddObjectDependency(PairedData->AttackerMontage, OutResult);
			AddObjectDependency(PairedData->VictimMontage, OutResult);
			AddMontageDependencies(PairedData->AttackerMontage, OutResult);
			AddMontageDependencies(PairedData->VictimMontage, OutResult);
		}
	}
	ValidatePairedSequence(Manifest.PairedDependencies, PairedAssets, OutResult);

	TMap<FString, const FDefenseProofPresentationEntry*> PresentationEntries;
	for (const FDefenseProofPresentationEntry& Entry : Manifest.Presentations)
	{
		PresentationEntries.Add(Entry.Name, &Entry);
		const FDefenseProofExpectedCaseEntry* ReferencingCase = Manifest.ExpectedCases.FindByPredicate(
			[&Entry](const FDefenseProofExpectedCaseEntry& Case)
			{
				return Case.bHasPresentation && Case.Presentation == Entry.Name;
			});
		const FDefenseProofAttackEntry* const* AttackEntry = ReferencingCase
			? AttackEntries.Find(ReferencingCase->Attack)
			: nullptr;
		const UAttackData* const* AttackData = ReferencingCase
			? AttackAssets.Find(ReferencingCase->Attack)
			: nullptr;
		if (!ReferencingCase)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("UnreferencedPresentation"), FString::Printf(TEXT("presentation.%s"), *Entry.Name),
				TEXT("presentation is not referenced by an expected case"));
			FDefenseAssetValidationRow Row;
			Row.Kind = TEXT("Presentation");
			Row.Name = Entry.Name;
			OutResult.Rows.Add(MoveTemp(Row));
		}
		else
		{
			ValidatePresentationEntry(Entry,
				AttackEntry ? **AttackEntry : FDefenseProofAttackEntry(),
				AttackData ? *AttackData : nullptr,
				Configuration, Configuration, OutResult);
		}
	}

	for (const FDefenseProofExpectedCaseEntry& Case : Manifest.ExpectedCases)
	{
		FDefenseAssetValidationRow Row;
		Row.Kind = TEXT("ExpectedCase");
		Row.Name = Case.Name;
		Row.Facts.Add(TEXT("attack"), Case.Attack);
		Row.Facts.Add(TEXT("outcome"), Case.Outcome);
		Row.Facts.Add(TEXT("reason"), Case.Reason);
		Row.Facts.Add(TEXT("attacker_response"), Case.AttackerResponse);

		const FDefenseProofAttackEntry* const* AttackEntry = AttackEntries.Find(Case.Attack);
		const UAttackData* const* AttackData = AttackAssets.Find(Case.Attack);
		const FDefenseProofPresentationEntry* const* Presentation = Case.bHasPresentation
			? PresentationEntries.Find(Case.Presentation)
			: nullptr;
		TMap<FString, const UPairedAnimationData*> PairedByRole;
		for (const FString& DependencyName : Case.PairedDependencies)
		{
			const FDefenseProofPairedDependencyEntry* const* Dependency =
				PairedEntries.Find(DependencyName);
			const UPairedAnimationData* const* DependencyAsset = PairedByName.Find(DependencyName);
			if (!Dependency || !DependencyAsset)
			{
				OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
					TEXT("ExpectedCaseDependencyMissing"), Case.Name,
					FString::Printf(TEXT("paired dependency %s is unresolved"), *DependencyName));
				continue;
			}
			if (PairedByRole.Contains((*Dependency)->Role))
			{
				OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
					TEXT("ExpectedCaseDependencyRoleAmbiguous"), Case.Name,
					FString::Printf(TEXT("multiple %s dependencies are named"), *(*Dependency)->Role));
			}
			PairedByRole.Add((*Dependency)->Role, *DependencyAsset);
		}

		if (Case.Outcome == TEXT("PerfectParry"))
		{
			for (const FString RequiredRole : {FString(TEXT("Bridge")), FString(TEXT("Counter")), FString(TEXT("Finisher"))})
			{
				if (!PairedByRole.Contains(RequiredRole))
				{
					OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
						TEXT("ExpectedCaseDependencyRoleMissing"), Case.Name,
						FString::Printf(TEXT("PerfectParry proof requires a %s dependency"), *RequiredRole));
				}
			}

			if (AttackEntry && AttackData && *AttackData)
			{
				const UPairedAnimationData* const* Counter = PairedByRole.Find(TEXT("Counter"));
				const UPairedAnimationData* const* Finisher = PairedByRole.Find(TEXT("Finisher"));
				if (Counter && ((*AttackData)->CounterData != *Counter || !(*AttackData)->bHasCounterVariant))
				{
					OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
						TEXT("AttackCounterReferenceMismatch"), Case.Name,
						TEXT("attack CounterData/bHasCounterVariant does not match the named Counter dependency"));
				}
				if (Finisher && ((*AttackData)->FinisherData != *Finisher || !(*AttackData)->bCanTriggerFinisher))
				{
					OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
						TEXT("AttackFinisherReferenceMismatch"), Case.Name,
						TEXT("attack FinisherData/bCanTriggerFinisher does not match the named Finisher dependency"));
				}
			}

			if (AttackEntry && AttackData && *AttackData && Presentation && Configuration)
			{
				FDefensePresentationSelectionContext SelectionContext;
				SelectionContext.Outcome = EDefenseOutcome::PerfectParry;
				SelectionContext.AttackerResponse = ParseCheckedEnum<EAttackerResponse>(Case.AttackerResponse);
				SelectionContext.Height = ParseCheckedEnum<EAttackHeight>((*AttackEntry)->ExpectedHeight);
				SelectionContext.Lane = ParseCheckedEnum<EIncomingAttackLane>((*AttackEntry)->ExpectedLane);
				SelectionContext.SwingShape = ParseCheckedEnum<ESwingDirection>((*AttackEntry)->ExpectedSwing);
				SelectionContext.AttackTags = (*AttackData)->AttackTags;
				SelectionContext.bPairedBridgeUsable = true;
				const FDefensePresentationSelectionResult Selection =
					FTableDefensePresentationSelector().SelectDefender(SelectionContext, Configuration);
				const UPairedAnimationData* const* Bridge = PairedByRole.Find(TEXT("Bridge"));
				if (Bridge && Selection.Payload.PairedBridgeData != *Bridge)
				{
					OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
						TEXT("PresentationBridgeReferenceMismatch"), Case.Name,
						TEXT("selected defender presentation does not reference the named Bridge dependency"));
				}
			}
		}

		Row.Facts.Add(TEXT("paired_dependency_count"), LexToString(Case.PairedDependencies.Num()));
		OutResult.Rows.Add(MoveTemp(Row));
	}

	if (Configuration && PlayerDefault && PlayerDefault->GetMesh()
		&& PlayerDefault->GetMesh()->GetSkeletalMeshAsset())
	{
		const FReferenceSkeleton& Skeleton =
			PlayerDefault->GetMesh()->GetSkeletalMeshAsset()->GetRefSkeleton();
		for (const FDefenseProofAttackEntry& Entry : Manifest.Attacks)
		{
			const FName TargetBone(*Entry.ExpectedTargetBone);
			const int32 BoneIndex = Skeleton.FindBoneIndex(TargetBone);
			TArray<FName> ParentChain;
			for (int32 ParentIndex = BoneIndex != INDEX_NONE ? Skeleton.GetParentIndex(BoneIndex) : INDEX_NONE;
				ParentIndex != INDEX_NONE;
				ParentIndex = Skeleton.GetParentIndex(ParentIndex))
			{
				ParentChain.Add(Skeleton.GetBoneName(ParentIndex));
			}
			if (BoneIndex == INDEX_NONE)
			{
				OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
					TEXT("DefenseTargetBoneMissing"), Entry.Name,
					TEXT("manifest target bone is absent from the player proof skeleton"));
			}
			const FDefenseHeightResolution Resolution = Configuration->ResolveHeight(
				TargetBone, ParentChain, ParseCheckedEnum<EAttackHeight>(Entry.ExpectedHeight));
			if (Resolution.Height != ParseCheckedEnum<EAttackHeight>(Entry.ExpectedHeight))
			{
				OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
					TEXT("BoneHeightResolutionMismatch"), Entry.Name,
					TEXT("bone-height resolution differs from the manifest attack height"));
			}
			FDefenseAssetValidationRow Row;
			Row.Kind = TEXT("BoneHeight");
			Row.Name = Entry.Name;
			Row.Facts.Add(TEXT("target_bone"), Entry.ExpectedTargetBone);
			Row.Facts.Add(TEXT("matched_bone"), Resolution.MatchedBone.ToString());
			Row.Facts.Add(TEXT("provenance"),
				StaticEnum<EDefenseHeightProvenance>()->GetNameStringByValue(
					static_cast<int64>(Resolution.Provenance)));
			OutResult.Rows.Add(MoveTemp(Row));
		}
	}

	const TSet<FString> ExplicitPathSet(ExplicitPaths);
	for (const FString& Dependency : OutResult.Dependencies)
	{
		if (!ExplicitPathSet.Contains(Dependency))
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("UndeclaredManifestDependency"), Dependency,
				TEXT("loaded assets reference this /Game object, but supportingAssets does not declare it"));
		}
	}
	OutResult.Dependencies.Sort();
}

void FDefenseAssetValidationService::ValidateAttackEntry(
	const FDefenseProofAttackEntry& Entry,
	const UAttackData* AttackData,
	const UAnimMontage* Montage,
	FDefenseAssetValidationResult& OutResult)
{
	const FString Context = FString::Printf(TEXT("attack.%s"), *Entry.Name);
	FDefenseAssetValidationRow Row;
	Row.Kind = TEXT("Attack");
	Row.Name = Entry.Name;
	Row.AssetPath = AttackData ? AttackData->GetPathName() : Entry.AttackData;
	if (!AttackData)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error, TEXT("MissingAttackData"),
			Context, TEXT("attack data did not load"));
		OutResult.Rows.Add(MoveTemp(Row));
		return;
	}
	if (!Montage)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error, TEXT("MissingAttackMontage"),
			Context, TEXT("attack montage did not load"));
		OutResult.Rows.Add(MoveTemp(Row));
		return;
	}
	if (AttackData->AttackMontage != Montage)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error, TEXT("AttackMontageMismatch"),
			Context, TEXT("AttackData.AttackMontage does not match the manifest montage"));
	}
	if (AttackData->MontageSection != FName(*Entry.Section)
		|| !Montage->IsValidSectionName(FName(*Entry.Section)))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error, TEXT("AttackSectionMismatch"),
			Context, TEXT("AttackData section is missing or differs from the manifest"));
		OutResult.Rows.Add(MoveTemp(Row));
		return;
	}

	const EAttackHeight ExpectedHeight = ParseCheckedEnum<EAttackHeight>(Entry.ExpectedHeight);
	const EIncomingAttackLane ExpectedLane = ParseCheckedEnum<EIncomingAttackLane>(Entry.ExpectedLane);
	const ESwingDirection ExpectedSwing = ParseCheckedEnum<ESwingDirection>(Entry.ExpectedSwing);
	if (AttackData->DefenseProfile.Height != ExpectedHeight
		|| AttackData->DefenseProfile.NominalLane != ExpectedLane
		|| AttackData->DefenseProfile.SwingShape != ExpectedSwing
		|| AttackData->DefenseProfile.SourceContactSocketOverride != FName(*Entry.ExpectedSourceSocket)
		|| AttackData->GetDefenseTargetBoneFallback() != FName(*Entry.ExpectedTargetBone))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error, TEXT("AttackProfileMismatch"),
			Context, TEXT("loaded defense height, lane, swing, socket, or target bone differs from the manifest"));
	}
	if (AttackData->bHasCounterVariant != (AttackData->CounterData != nullptr))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("CounterReferenceMismatch"), Context,
			TEXT("bHasCounterVariant and CounterData must agree"));
	}
	if (AttackData->bCanTriggerFinisher != (AttackData->FinisherData != nullptr))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("FinisherReferenceMismatch"), Context,
			TEXT("bCanTriggerFinisher and FinisherData must agree"));
	}

	TArray<FString> ExpectedTags = Entry.ExpectedTags;
	ExpectedTags.Sort();
	const TArray<FString> ActualTags = GetSortedTagNames(AttackData->AttackTags);
	if (ActualTags != ExpectedTags)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error, TEXT("AttackTagsMismatch"),
			Context, TEXT("loaded AttackTags differ from the manifest's exact expected tag set"));
	}

	const int32 SectionIndex = Montage->GetSectionIndex(FName(*Entry.Section));
	float SectionStart = 0.0f;
	float SectionEnd = 0.0f;
	Montage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
	int32 WindowsInSection = 0;
	int32 WindowsCrossingSectionBoundary = 0;
	double ActualRelativeStart = 0.0;
	double ActualRelativeEnd = 0.0;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (!Event.NotifyStateClass
			|| !Event.NotifyStateClass->IsA(UAnimNotifyState_ParryWindow::StaticClass()))
		{
			continue;
		}
		const double Start = Event.GetTriggerTime();
		const double End = Start + Event.GetDuration();
		const bool bInside = Start >= static_cast<double>(SectionStart) - KINDA_SMALL_NUMBER
			&& End <= static_cast<double>(SectionEnd) + KINDA_SMALL_NUMBER;
		const bool bOverlapsSection = End > static_cast<double>(SectionStart) + KINDA_SMALL_NUMBER
			&& Start < static_cast<double>(SectionEnd) - KINDA_SMALL_NUMBER;
		if (bInside)
		{
			++WindowsInSection;
			ActualRelativeStart = Start - SectionStart;
			ActualRelativeEnd = End - SectionStart;
		}
		else if (bOverlapsSection)
		{
			++WindowsCrossingSectionBoundary;
		}
	}
	if (WindowsCrossingSectionBoundary > 0)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("ParryWindowOutsideSection"), Context,
			TEXT("a parry window overlaps but extends beyond the manifest attack section"));
	}
	const int32 ExpectedWindowCount = Entry.ParryWindow.bPresent ? 1 : 0;
	if (WindowsInSection != ExpectedWindowCount)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("ParryWindowCardinality"), Context,
			FString::Printf(TEXT("expected %d section parry window(s), found %d"),
				ExpectedWindowCount, WindowsInSection));
	}
	if (Entry.ParryWindow.bPresent && WindowsInSection == 1
		&& (!FMath::IsNearlyEqual(ActualRelativeStart, Entry.ParryWindow.StartSeconds, 0.001)
			|| !FMath::IsNearlyEqual(ActualRelativeEnd, Entry.ParryWindow.EndSeconds, 0.001)))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("ParryWindowTimingMismatch"), Context,
			TEXT("section-relative parry-window timing differs from the reviewed manifest"));
	}
	const bool bParryableTag = AttackData->AttackTags.HasTagExact(
		KatanaCombatGameplayTags::AttackDefenseParryable());
	if (bParryableTag != (WindowsInSection == 1))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("TagWindowMismatch"), Context,
			TEXT("Attack.Defense.Parryable and the canonical section window do not agree"));
	}

	Row.Facts.Add(TEXT("height"), Entry.ExpectedHeight);
	Row.Facts.Add(TEXT("lane"), Entry.ExpectedLane);
	Row.Facts.Add(TEXT("swing"), Entry.ExpectedSwing);
	Row.Facts.Add(TEXT("source_socket"), Entry.ExpectedSourceSocket);
	Row.Facts.Add(TEXT("target_bone"), Entry.ExpectedTargetBone);
	Row.Facts.Add(TEXT("parry_windows_in_section"), LexToString(WindowsInSection));
	Row.Facts.Add(TEXT("parry_windows_crossing_section"), LexToString(WindowsCrossingSectionBoundary));
	Row.Facts.Add(TEXT("blocked_audio_override"),
		LexToString(AttackData->DefenseProfile.bOverrideBlockedImpactAudio
			&& IsConcreteAudio(AttackData->DefenseProfile.BlockedImpactAudio)));
	Row.Facts.Add(TEXT("blocked_vfx_override"),
		LexToString(AttackData->DefenseProfile.bOverrideBlockedImpactVFX
			&& IsConcreteVFX(AttackData->DefenseProfile.BlockedImpactVFX)));
	OutResult.Rows.Add(MoveTemp(Row));
}

void FDefenseAssetValidationService::ValidatePresentationEntry(
	const FDefenseProofPresentationEntry& Entry,
	const FDefenseProofAttackEntry& AttackEntry,
	const UAttackData* AttackData,
	const UDefenseConfiguration* DefenderConfiguration,
	const UDefenseConfiguration* AttackerConfiguration,
	FDefenseAssetValidationResult& OutResult)
{
	const FString Context = FString::Printf(TEXT("presentation.%s"), *Entry.Name);
	FDefenseAssetValidationRow Row;
	Row.Kind = TEXT("Presentation");
	Row.Name = Entry.Name;
	if (!AttackData || !DefenderConfiguration || !AttackerConfiguration)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("MissingPresentationDependency"), Context,
			TEXT("attack, defender configuration, or attacker configuration did not load"));
		OutResult.Rows.Add(MoveTemp(Row));
		return;
	}

	FDefensePresentationSelectionContext SelectionContext;
	SelectionContext.Outcome = ParseCheckedEnum<EDefenseOutcome>(Entry.Outcome);
	SelectionContext.AttackerResponse = ParseCheckedEnum<EAttackerResponse>(Entry.AttackerResponse);
	SelectionContext.Height = ParseCheckedEnum<EAttackHeight>(AttackEntry.ExpectedHeight);
	SelectionContext.Lane = ParseCheckedEnum<EIncomingAttackLane>(AttackEntry.ExpectedLane);
	SelectionContext.SwingShape = ParseCheckedEnum<ESwingDirection>(AttackEntry.ExpectedSwing);
	SelectionContext.AttackTags = AttackData->AttackTags;
	SelectionContext.bPairedBridgeUsable = true;

	const FTableDefensePresentationSelector Selector;
	const FDefensePresentationSelectionResult Defender =
		Selector.SelectDefender(SelectionContext, DefenderConfiguration);
	const FDefensePresentationSelectionResult Attacker =
		Selector.SelectAttacker(SelectionContext, AttackerConfiguration);
	if (Entry.bHasDefenderRow
		&& (!Defender.bFound || Defender.RowName != FName(*Entry.DefenderRow)))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("PresentationRowMismatch"), Context,
			TEXT("selected defender row differs from the manifest"));
	}
	if (Entry.bHasAttackerRow
		&& (!Attacker.bFound || Attacker.RowName != FName(*Entry.AttackerRow)))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("PresentationRowMismatch"), Context,
			TEXT("selected attacker row differs from the manifest"));
	}
	if (Defender.bAmbiguous || Attacker.bAmbiguous)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("PresentationAmbiguous"), Context,
			TEXT("multiple matching presentation rows have equal rank"));
	}

	if (Entry.bHasDefenderRow)
	{
		const FDefensePresentationSelectionResult Generic =
			Selector.SelectGenericDefender(SelectionContext, DefenderConfiguration);
		if (!Generic.bFound || Generic.bAmbiguous)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("MissingGenericFallback"), Context,
				TEXT("defender presentation lacks one deterministic generic fallback"));
		}
	}
	if (Entry.bHasAttackerRow)
	{
		const FDefensePresentationSelectionResult Generic =
			Selector.SelectGenericAttacker(SelectionContext, AttackerConfiguration);
		if (!Generic.bFound || Generic.bAmbiguous)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("MissingGenericFallback"), Context,
				TEXT("attacker presentation lacks one deterministic generic fallback"));
		}
	}

	if (Entry.bRequiresDefenderMontage && !IsValidPayloadMontage(Defender.Payload))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("MissingVisibleMontage"), Context,
			TEXT("selected defender payload lacks a valid montage and section"));
	}
	if (Entry.bRequiresAttackerMontage && !IsValidPayloadMontage(Attacker.Payload))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("MissingVisibleMontage"), Context,
			TEXT("selected attacker payload lacks a valid montage and section"));
	}

	const bool bProfileAudio = AttackData->DefenseProfile.bOverrideBlockedImpactAudio
		&& IsConcreteAudio(AttackData->DefenseProfile.BlockedImpactAudio);
	const bool bProfileVFX = AttackData->DefenseProfile.bOverrideBlockedImpactVFX
		&& IsConcreteVFX(AttackData->DefenseProfile.BlockedImpactVFX);
	const bool bDefaultAudio = SelectionContext.Outcome == EDefenseOutcome::PerfectParry
		? IsConcreteAudio(DefenderConfiguration->DefaultParryImpactAudio)
		: IsConcreteAudio(DefenderConfiguration->DefaultBlockImpactAudio);
	const bool bDefaultVFX = SelectionContext.Outcome == EDefenseOutcome::PerfectParry
		? IsConcreteVFX(DefenderConfiguration->DefaultParryImpactVFX)
		: IsConcreteVFX(DefenderConfiguration->DefaultBlockImpactVFX);
	const bool bHasConcreteAudio = Defender.Payload.bOverrideImpactAudio
		&& IsConcreteAudio(Defender.Payload.ImpactAudio) || bProfileAudio || bDefaultAudio;
	const bool bHasConcreteVFX = Defender.Payload.bOverrideImpactVFX
		&& IsConcreteVFX(Defender.Payload.ImpactVFX) || bProfileVFX || bDefaultVFX;
	if (Entry.bRequiresImpactAudio && !bHasConcreteAudio)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("MissingVisibleImpactAudio"), Context,
			TEXT("no concrete blocked/parry impact sound is reachable"));
	}
	if (Entry.bRequiresImpactVFX && !bHasConcreteVFX)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("MissingVisibleImpactVFX"), Context,
			TEXT("no concrete blocked/parry Niagara effect is reachable"));
	}

	const FName ResolvedSocket = !Defender.Payload.SourceSocketOverride.IsNone()
		? Defender.Payload.SourceSocketOverride
		: AttackData->DefenseProfile.SourceContactSocketOverride;
	const FName ResolvedBone = !Defender.Payload.TargetBoneOverride.IsNone()
		? Defender.Payload.TargetBoneOverride
		: AttackData->GetDefenseTargetBoneFallback();
	if (ResolvedSocket != FName(*Entry.ExpectedSourceSocket)
		|| ResolvedBone != FName(*Entry.ExpectedTargetBone))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("PresentationContactMismatch"), Context,
			TEXT("resolved presentation socket or target bone differs from the manifest"));
	}

	if (SelectionContext.Outcome == EDefenseOutcome::NormalBlock && Defender.Payload.Montage)
	{
		FDefenseRootMotionMeasurement Measurement;
		FString MeasurementError;
		if (MeasureRootMotion(Defender.Payload.Montage, Defender.Payload.MontageSection,
			Measurement, MeasurementError))
		{
			ValidateRootMotionBudget(Context, Measurement,
				DefenderConfiguration->NormalBlockTranslationDriftTolerance,
				DefenderConfiguration->DefenseTurnRate, OutResult);
			Row.Facts.Add(TEXT("root_horizontal_cm"), LexToString(Measurement.HorizontalTranslation));
			Row.Facts.Add(TEXT("root_max_yaw_rate"), LexToString(Measurement.MaximumYawRate));
		}
		else
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("RootMotionMeasurementFailed"), Context, MeasurementError);
		}
	}

	Row.Facts.Add(TEXT("defender_row"), Defender.RowName.ToString());
	Row.Facts.Add(TEXT("attacker_row"), Attacker.RowName.ToString());
	Row.Facts.Add(TEXT("defender_montage"), Defender.Payload.Montage
		? Defender.Payload.Montage->GetPathName() : TEXT("None"));
	Row.Facts.Add(TEXT("attacker_montage"), Attacker.Payload.Montage
		? Attacker.Payload.Montage->GetPathName() : TEXT("None"));
	Row.Facts.Add(TEXT("paired_bridge"), Defender.Payload.PairedBridgeData
		? Defender.Payload.PairedBridgeData->GetPathName() : TEXT("None"));
	Row.Facts.Add(TEXT("impact_audio"), Defender.Payload.ImpactAudio.ImpactSound
		? Defender.Payload.ImpactAudio.ImpactSound->GetPathName() : TEXT("None"));
	Row.Facts.Add(TEXT("impact_vfx"), Defender.Payload.ImpactVFX.ImpactVFX
		? Defender.Payload.ImpactVFX.ImpactVFX->GetPathName() : TEXT("None"));
	Row.Facts.Add(TEXT("source_socket"), ResolvedSocket.ToString());
	Row.Facts.Add(TEXT("target_bone"), ResolvedBone.ToString());
	Row.Facts.Add(TEXT("concrete_audio"), LexToString(bHasConcreteAudio));
	Row.Facts.Add(TEXT("concrete_vfx"), LexToString(bHasConcreteVFX));
	OutResult.Rows.Add(MoveTemp(Row));
}

void FDefenseAssetValidationService::ValidatePairedDependency(
	const FDefenseProofPairedDependencyEntry& Entry,
	const UPairedAnimationData* PairedData,
	const UDefenseConfiguration* Configuration,
	FDefenseAssetValidationResult& OutResult,
	const UAnimMontage* ManifestAttackerMontage,
	const UAnimMontage* ManifestVictimMontage)
{
	const FString Context = FString::Printf(TEXT("paired.%s"), *Entry.Name);
	FDefenseAssetValidationRow Row;
	Row.Kind = TEXT("Paired");
	Row.Name = Entry.Name;
	Row.AssetPath = PairedData ? PairedData->GetPathName() : Entry.PairedData;
	if (!PairedData || !Configuration)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("MissingPairedDependency"), Context,
			TEXT("paired data or defense configuration did not load"));
		OutResult.Rows.Add(MoveTemp(Row));
		return;
	}
	const UAnimMontage* AttackerMontage = ManifestAttackerMontage
		? ManifestAttackerMontage : PairedData->AttackerMontage.Get();
	const UAnimMontage* VictimMontage = ManifestVictimMontage
		? ManifestVictimMontage : PairedData->VictimMontage.Get();

	const EPairedReactionType ExpectedReaction = Entry.Role == TEXT("Bridge")
		? EPairedReactionType::Parry
		: Entry.Role == TEXT("Counter")
			? EPairedReactionType::Counter
			: EPairedReactionType::Finisher;
	if (PairedData->ReactionType != ExpectedReaction)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("PairedRoleMismatch"), Context,
			TEXT("paired reaction type differs from the manifest stage role"));
	}
	if (!PairedData->AttackerMontage || !PairedData->VictimMontage
		|| PairedData->AttackerMontage->GetPathName() != Entry.AttackerMontage
		|| PairedData->VictimMontage->GetPathName() != Entry.VictimMontage
		|| PairedData->AttackerMontageSection != FName(*Entry.AttackerSection)
		|| PairedData->VictimMontageSection != FName(*Entry.VictimSection))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("PairedMontageSectionMismatch"), Context,
			TEXT("both PairedData montage references and section properties must match the manifest"));
	}
	if (!AttackerMontage || !VictimMontage
		|| !AttackerMontage->IsValidSectionName(FName(*Entry.AttackerSection))
		|| !VictimMontage->IsValidSectionName(FName(*Entry.VictimSection)))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("PairedManifestSectionMissing"), Context,
			TEXT("the manifest-target montages must contain both exact paired sections"));
	}

	const FPairedChainTransitionPolicy& Policy = PairedData->ChainTransitionPolicy;
	if (Entry.Role != TEXT("Finisher"))
	{
		const EPairedAnimationRole ExpectedDriver = Entry.DriverRole == TEXT("Attacker")
			? EPairedAnimationRole::Attacker
			: EPairedAnimationRole::Victim;
		if (!Entry.bHasDriverRole || !Entry.bHasDriverMarker
			|| Policy.DriverRole != ExpectedDriver
			|| Policy.RequiredMarker != FName(*Entry.DriverMarker))
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("PairedDriverPolicyMismatch"), Context,
				TEXT("driver role or required marker differs from the manifest"));
		}
		const EChainStageTransitionType Transition = Entry.Role == TEXT("Bridge")
			? EChainStageTransitionType::OpenCounterWindow
			: EChainStageTransitionType::AutoContinue;
		const UAnimMontage* DriverMontage = ExpectedDriver == EPairedAnimationRole::Attacker
			? AttackerMontage : VictimMontage;
		const UAnimMontage* PartnerMontage = ExpectedDriver == EPairedAnimationRole::Attacker
			? VictimMontage : AttackerMontage;
		bool bUnexpectedDriverMarker = false;
		bool bUnexpectedPartnerMarker = false;
		const int32 DriverMarkers = CountChainMarkers(
			DriverMontage, FName(*Entry.DriverMarker), Transition, bUnexpectedDriverMarker);
		const int32 PartnerMarkers = CountChainMarkers(
			PartnerMontage, FName(*Entry.DriverMarker), Transition, bUnexpectedPartnerMarker);
		if (DriverMarkers != 1 || PartnerMarkers != 0
			|| bUnexpectedDriverMarker || bUnexpectedPartnerMarker)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("DriverMarkerAmbiguous"), Context,
				TEXT("exactly one matching marker must exist on the driver and none on the partner"));
		}
		if (Entry.Role == TEXT("Counter") && !Policy.bAutoContinue)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("CounterAutoContinueMissing"), Context,
				TEXT("a reviewed counter-to-finisher proof requires bAutoContinue"));
		}
		if (Entry.Role == TEXT("Bridge") && Policy.bAutoContinue)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("BridgeAutoContinueUnexpected"), Context,
				TEXT("a bridge must wait for its explicit counter-window marker"));
		}
		if (!Policy.HasRetainableReadyPose())
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("PairedReadyPoseMissing"), Context,
				TEXT("both roles need a valid ready section or reviewed terminal-pose compatibility"));
		}
		if ((Entry.bHasAttackerReadySection
				? Policy.AttackerReadySection != FName(*Entry.AttackerReadySection)
				: !Policy.AttackerReadySection.IsNone())
			|| (Entry.bHasVictimReadySection
				? Policy.VictimReadySection != FName(*Entry.VictimReadySection)
				: !Policy.VictimReadySection.IsNone())
			|| Policy.bAttackerTerminalPoseCompatible != Entry.bAttackerTerminalPoseCompatible
			|| Policy.bVictimTerminalPoseCompatible != Entry.bVictimTerminalPoseCompatible)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("PairedReadyPoseMismatch"), Context,
				TEXT("loaded ready sections or terminal-pose review flags differ from the manifest"));
		}
		if ((Entry.bHasAttackerReadySection
				&& (!AttackerMontage
					|| !AttackerMontage->IsValidSectionName(FName(*Entry.AttackerReadySection))))
			|| (Entry.bHasVictimReadySection
				&& (!VictimMontage
					|| !VictimMontage->IsValidSectionName(FName(*Entry.VictimReadySection)))))
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("PairedReadySectionMissing"), Context,
				TEXT("manifest ready sections must exist on their semantic role montages"));
		}
	}
	else if (!Policy.RequiredMarker.IsNone() || Policy.bAutoContinue
		|| Entry.bHasDriverMarker || Entry.bHasDriverRole)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("FinisherMarkerUnexpected"), Context,
			TEXT("the terminal finisher stage must not advertise or auto-continue another Chain stage"));
	}

	if (PairedData->AttackerWarpConfig.WarpTargetName != FName(*Entry.AttackerWarpTarget)
		|| PairedData->VictimWarpConfig.WarpTargetName != FName(*Entry.VictimWarpTarget))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("PairedWarpTargetMismatch"), Context,
			TEXT("role warp-target names differ from the manifest"));
	}
	if (!PairedData->AttackerWarpConfig.bWarpRotation
		|| !PairedData->VictimWarpConfig.bWarpRotation)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("PairedRotationWarpConfigMismatch"), Context,
			TEXT("both role warp configs must enable rotation"));
	}
	if (!HasNamedRotationWarp(AttackerMontage, FName(*Entry.AttackerWarpTarget))
		|| !HasNamedRotationWarp(VictimMontage, FName(*Entry.VictimWarpTarget)))
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("PairedRotationWarpMissing"), Context,
			TEXT("both role configs and montages require a matching named rotation-warp window"));
	}
	if (CountStateNotifies<UAnimNotifyState_PairedAnimationSync>(AttackerMontage) < 1)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("PairedSyncNotifyMissing"), Context,
			TEXT("the semantic attacker montage lacks a paired sync notify"));
	}
	if (CountStateNotifies<UAnimNotifyState_PairedAnimationCollision>(AttackerMontage) < 1
		|| CountStateNotifies<UAnimNotifyState_PairedAnimationCollision>(VictimMontage) < 1)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("PairedCollisionNotifyMissing"), Context,
			TEXT("both role montages require paired collision ownership windows"));
	}

	FDefenseRootMotionMeasurement AttackerRoot;
	FDefenseRootMotionMeasurement VictimRoot;
	FString RootError;
	const bool bApplyPerfectParryBridgeBudget = Entry.Role == TEXT("Bridge");
	if (MeasureRootMotion(AttackerMontage, FName(*Entry.AttackerSection),
		AttackerRoot, RootError))
	{
		if (bApplyPerfectParryBridgeBudget)
		{
			ValidateRootMotionBudget(Context + TEXT(".attacker"), AttackerRoot,
				Configuration->PerfectParryTranslationAllowancePerRole,
				Configuration->DefenseTurnRate, OutResult);
		}
	}
	else
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("RootMotionMeasurementFailed"), Context + TEXT(".attacker"), RootError);
	}
	if (MeasureRootMotion(VictimMontage, FName(*Entry.VictimSection),
		VictimRoot, RootError))
	{
		if (bApplyPerfectParryBridgeBudget)
		{
			ValidateRootMotionBudget(Context + TEXT(".victim"), VictimRoot,
				Configuration->PerfectParryTranslationAllowancePerRole,
				Configuration->DefenseTurnRate, OutResult);
		}
	}
	else
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("RootMotionMeasurementFailed"), Context + TEXT(".victim"), RootError);
	}

	Row.Facts.Add(TEXT("role"), Entry.Role);
	Row.Facts.Add(TEXT("driver_role"), Entry.DriverRole);
	Row.Facts.Add(TEXT("driver_marker"), Entry.DriverMarker);
	Row.Facts.Add(TEXT("attacker_section"), Entry.AttackerSection);
	Row.Facts.Add(TEXT("victim_section"), Entry.VictimSection);
	Row.Facts.Add(TEXT("manifest_attacker_montage"), Entry.AttackerMontage);
	Row.Facts.Add(TEXT("manifest_victim_montage"), Entry.VictimMontage);
	Row.Facts.Add(TEXT("actual_attacker_montage"), PairedData->AttackerMontage
		? PairedData->AttackerMontage->GetPathName() : TEXT("None"));
	Row.Facts.Add(TEXT("actual_victim_montage"), PairedData->VictimMontage
		? PairedData->VictimMontage->GetPathName() : TEXT("None"));
	Row.Facts.Add(TEXT("attacker_ready_section"), Policy.AttackerReadySection.ToString());
	Row.Facts.Add(TEXT("victim_ready_section"), Policy.VictimReadySection.ToString());
	Row.Facts.Add(TEXT("attacker_terminal_pose_compatible"),
		LexToString(Policy.bAttackerTerminalPoseCompatible));
	Row.Facts.Add(TEXT("victim_terminal_pose_compatible"),
		LexToString(Policy.bVictimTerminalPoseCompatible));
	Row.Facts.Add(TEXT("auto_continue"), LexToString(Policy.bAutoContinue));
	Row.Facts.Add(TEXT("perfect_parry_bridge_budget_applied"),
		LexToString(bApplyPerfectParryBridgeBudget));
	Row.Facts.Add(TEXT("attacker_root_horizontal_cm"), LexToString(AttackerRoot.HorizontalTranslation));
	Row.Facts.Add(TEXT("victim_root_horizontal_cm"), LexToString(VictimRoot.HorizontalTranslation));
	OutResult.Rows.Add(MoveTemp(Row));
}

void FDefenseAssetValidationService::ValidatePairedSequence(
	const TArray<FDefenseProofPairedDependencyEntry>& Entries,
	const TArray<const UPairedAnimationData*>& PairedAssets,
	FDefenseAssetValidationResult& OutResult)
{
	if (Entries.Num() != PairedAssets.Num())
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("PairedSequenceCardinality"), TEXT("paired.sequence"),
			TEXT("manifest entries and loaded paired assets have different cardinality"));
		return;
	}
	for (int32 Index = 1; Index < PairedAssets.Num(); ++Index)
	{
		if (Entries[Index - 1].AttackerMontage == Entries[Index].AttackerMontage
			|| Entries[Index - 1].VictimMontage == Entries[Index].VictimMontage)
		{
			OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
				TEXT("AdjacentMontageReuse"),
				FString::Printf(TEXT("paired.sequence.%s.%s"),
					*Entries[Index - 1].Name, *Entries[Index].Name),
				TEXT("adjacent stages reuse a montage for the same semantic role"));
		}
	}
}

bool FDefenseAssetValidationService::MeasureRootMotion(
	const UAnimMontage* Montage,
	const FName Section,
	FDefenseRootMotionMeasurement& OutMeasurement,
	FString& OutError)
{
	OutMeasurement = {};
	OutError.Reset();
	if (!Montage)
	{
		OutError = TEXT("montage is null");
		return false;
	}
	float Start = 0.0f;
	float End = Montage->GetPlayLength();
	if (!Section.IsNone())
	{
		const int32 SectionIndex = Montage->GetSectionIndex(Section);
		if (SectionIndex == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("montage lacks section '%s'"), *Section.ToString());
			return false;
		}
		Montage->GetSectionStartAndEndTime(SectionIndex, Start, End);
	}
	if (!FMath::IsFinite(Start) || !FMath::IsFinite(End) || End <= Start)
	{
		OutError = TEXT("montage root-motion range is invalid");
		return false;
	}

	const FAnimExtractContext TotalContext(static_cast<double>(End), true, FDeltaTimeRecord(), false);
	const FTransform Total = Montage->ExtractRootMotionFromTrackRange(Start, End, TotalContext);
	const FVector Translation = Total.GetTranslation();
	OutMeasurement.HorizontalTranslation = FVector2D(Translation.X, Translation.Y).Size();
	OutMeasurement.TotalYaw = FMath::Abs(FMath::UnwindDegrees(Total.Rotator().Yaw));
	OutMeasurement.StartSeconds = Start;
	OutMeasurement.EndSeconds = End;

	constexpr double SampleRate = 60.0;
	const int32 SampleCount = FMath::Max(1, FMath::CeilToInt((End - Start) * SampleRate));
	OutMeasurement.SampleCount = SampleCount;
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const double Previous = FMath::Lerp(static_cast<double>(Start), static_cast<double>(End),
			static_cast<double>(Index) / SampleCount);
		const double Current = FMath::Lerp(static_cast<double>(Start), static_cast<double>(End),
			static_cast<double>(Index + 1) / SampleCount);
		const FAnimExtractContext SampleContext(Current, true, FDeltaTimeRecord(), false);
		const FTransform Delta = Montage->ExtractRootMotionFromTrackRange(
			static_cast<float>(Previous), static_cast<float>(Current), SampleContext);
		const double DeltaSeconds = Current - Previous;
		const double DeltaYaw = FMath::Abs(FMath::UnwindDegrees(Delta.Rotator().Yaw));
		OutMeasurement.MaximumYawRate = FMath::Max(
			OutMeasurement.MaximumYawRate, DeltaYaw / DeltaSeconds);
	}
	return true;
}

void FDefenseAssetValidationService::ValidateRootMotionBudget(
	const FString& Context,
	const FDefenseRootMotionMeasurement& Measurement,
	const double MaximumHorizontalTranslation,
	const double MaximumYawRate,
	FDefenseAssetValidationResult& OutResult)
{
	constexpr double NumericalTolerance = 0.001;
	if (!FMath::IsFinite(Measurement.HorizontalTranslation)
		|| Measurement.HorizontalTranslation > MaximumHorizontalTranslation + NumericalTolerance)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("RootTranslationBudget"), Context,
			FString::Printf(TEXT("horizontal root displacement %.4f cm exceeds %.4f cm"),
				Measurement.HorizontalTranslation, MaximumHorizontalTranslation));
	}
	if (!FMath::IsFinite(Measurement.MaximumYawRate)
		|| Measurement.MaximumYawRate > MaximumYawRate + NumericalTolerance)
	{
		OutResult.AddFinding(EDefenseAssetValidationSeverity::Error,
			TEXT("RootYawRateBudget"), Context,
			FString::Printf(TEXT("authored root yaw rate %.4f deg/s exceeds %.4f deg/s"),
				Measurement.MaximumYawRate, MaximumYawRate));
	}
}
