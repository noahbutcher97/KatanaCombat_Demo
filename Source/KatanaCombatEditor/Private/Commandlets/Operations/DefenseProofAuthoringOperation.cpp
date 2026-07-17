// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/Operations/DefenseProofAuthoringOperation.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_ChainStageTransition.h"
#include "Animation/AnimNotifyState_PairedAnimationCollision.h"
#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/SamuraiAnimInstance.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimationTransitionGraph.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimNotifyState_MotionWarping.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Commandlets/KatanaAssetMigrationRunner.h"
#include "Data/DefenseConfiguration.h"
#include "Data/PairedAnimationData.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2_Actions.h"
#include "EdGraphUtilities.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "NiagaraSystem.h"
#include "RootMotionModifier_SkewWarp.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "UObject/Package.h"

const FString FDefenseProofAuthoringOperation::OperationName = TEXT("DefenseProofAuthoring");

namespace
{
constexpr int32 RecipeVersion = 2;
constexpr float FloatTolerance = 0.001f;

const FString GuardAnimBlueprintPath =
	TEXT("/Game/ProjectFiles/Animation/ABP_SamuraiCharacter.ABP_SamuraiCharacter");
const FString GuardSequencePath =
	TEXT("/Game/Assets/Animations/DynamicKatana/AS_Block_Loop_Seq.AS_Block_Loop_Seq");
const FString ImpactAudioPath =
	TEXT("/Game/Assets/SFX/Combat_SFX_Bundle/Cues/Combat_Sounds_Weapons_And_Spells/Impact_Sword_Parry_01_Cue.Impact_Sword_Parry_01_Cue");
const FString ImpactVFXPath =
	TEXT("/Game/Assets/VFX/GoodFXImpactUE5/FX/NiagaraSystem/Yellow/NS_GFXI_Yellow_Parry.NS_GFXI_Yellow_Parry");

const FString MontageRoot =
	TEXT("/Game/ProjectFiles/Animation/Montages/Defense/GateA/");
const FString DataRoot =
	TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateA/");

const FString BlockMontagePackage = MontageRoot + TEXT("AM_Block_Middle_Center");
const FString ParryFallbackMontagePackage = MontageRoot + TEXT("AM_Parry_Fallback_Middle_Center");
const FString StaggerFallbackMontagePackage = MontageRoot + TEXT("AM_ParryStagger_Fallback_Middle_Center");
const FString BridgeDefenderMontagePackage = MontageRoot + TEXT("AM_ParryBridge_Defender");
const FString BridgeAttackerMontagePackage = MontageRoot + TEXT("AM_ParryBridge_Attacker");
const FString CounterDefenderMontagePackage = MontageRoot + TEXT("AM_Counter_Defender");
const FString CounterAttackerMontagePackage = MontageRoot + TEXT("AM_Counter_Attacker");
const FString FinisherDefenderMontagePackage = MontageRoot + TEXT("AM_Finisher_Defender");
const FString FinisherAttackerMontagePackage = MontageRoot + TEXT("AM_Finisher_Attacker");

const FString DefenseConfigurationPackage = DataRoot + TEXT("DA_DefenseConfiguration_GateA");
const FString BridgeDataPackage = DataRoot + TEXT("DA_ParryBridge_GateA");
const FString CounterDataPackage = DataRoot + TEXT("DA_Counter_GateA");
const FString FinisherDataPackage = DataRoot + TEXT("DA_Finisher_GateA");

const FString BlockSequencePath =
	TEXT("/Game/Assets/Animations/DynamicKatana/AS_Block_Hit_Seq.AS_Block_Hit_Seq");
const FString ParrySequencePath =
	TEXT("/Game/Assets/Animations/DynamicKatana/AS_Parry_R_Seq.AS_Parry_R_Seq");
const FString StaggerSequencePath =
	TEXT("/Game/Assets/Animations/DynamicKatana/AS_Block_Hit_Break_Seq.AS_Block_Hit_Break_Seq");
const FString CounterSequencePath =
	TEXT("/Game/Assets/Animations/DynamicKatana/AS_Parry_Counter_Attack_L_Seq.AS_Parry_Counter_Attack_L_Seq");
const FString CounterReactionSequencePath =
	TEXT("/Game/Assets/Animations/DynamicKatana/AS_Hit_Large_F_Seq.AS_Hit_Large_F_Seq");
const FString FinisherSequencePath =
	TEXT("/Game/Assets/Animations/DynamicKatana/AS_Execution_01_Seq.AS_Execution_01_Seq");
const FString FinisherReactionSequencePath =
	TEXT("/Game/Assets/Animations/DynamicKatana/AS_Execution_Target_01_Seq.AS_Execution_Target_01_Seq");

struct FDefenseAuthoringSegmentSpec
{
	FString SourcePath;
	float SourceStart = 0.0f;
	float SourceEnd = 0.0f;
	float PlayRate = 1.0f;
};

struct FDefenseAuthoringSectionSpec
{
	FName Name = NAME_None;
	float Time = 0.0f;
};

struct FDefenseAuthoringMontageSpec
{
	FString PackageName;
	TArray<FDefenseAuthoringSegmentSpec> Segments;
	TArray<FDefenseAuthoringSectionSpec> Sections;
	FName WarpTarget = NAME_None;
	float WarpStart = 0.0f;
	float WarpEnd = 0.0f;
	bool bWarpTranslation = false;
	bool bAddCollision = false;
	bool bAddSync = false;
	EPairedReactionType SyncReaction = EPairedReactionType::Parry;
	float SyncTime = 0.0f;
	bool bSyncAppliesDamage = false;
	bool bAddChainMarker = false;
	EChainStageTransitionType ChainTransition = EChainStageTransitionType::OpenCounterWindow;
	FName ChainMarker = NAME_None;
	float ChainTime = 0.0f;
};

struct FDefenseAuthoringPairedSpec
{
	FString PackageName;
	EPairedReactionType Reaction = EPairedReactionType::Parry;
	FString AttackerMontagePackage;
	FName AttackerSection = NAME_None;
	FString VictimMontagePackage;
	FName VictimSection = NAME_None;
	FName RequiredMarker = NAME_None;
	FName AttackerReadySection = NAME_None;
	FName VictimReadySection = NAME_None;
	bool bAttackerTerminalCompatible = false;
	bool bVictimTerminalCompatible = false;
	bool bAutoContinue = false;
	float SyncPointTime = 0.0f;
	float BaseDamage = 0.0f;
	bool bLethal = false;
};

struct FDefenseAuthoringPlan
{
	TArray<FString> ProposedChanges;
	TArray<FString> Errors;
	TArray<FKatanaAssetMigrationPackageLedgerEntry> PackageLedger;
	FString Fingerprint;
};

FString BuildObjectPath(const FString& PackageName)
{
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
	return FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
}

TArray<FDefenseAuthoringMontageSpec> BuildMontageSpecs()
{
	return {
		{BlockMontagePackage, {{BlockSequencePath, 0.0f, 0.833333f, 1.0f}},
			{{TEXT("Block"), 0.0f}}, TEXT("DefenseContactTarget"), 0.0f, 0.40f, false},
		{ParryFallbackMontagePackage, {{ParrySequencePath, 0.0f, 0.833333f, 1.0f}},
			{{TEXT("Parry"), 0.0f}}, TEXT("DefenseContactTarget"), 0.0f, 0.55f, false},
		{StaggerFallbackMontagePackage, {{StaggerSequencePath, 0.0f, 1.0f, 1.0f}},
			{{TEXT("Stagger"), 0.0f}}, TEXT("AttackerResponseTarget"), 0.0f, 0.65f, false},
		{BridgeDefenderMontagePackage,
			{{ParrySequencePath, 0.0f, 0.70f, 1.0f}, {ParrySequencePath, 0.70f, 0.72f, 0.20f}},
			{{TEXT("Bridge"), 0.0f}, {TEXT("CounterReady"), 0.70f}},
			TEXT("PairedTarget"), 0.0f, 0.70f, true, true, true,
			EPairedReactionType::Parry, 0.30f, false, true,
			EChainStageTransitionType::OpenCounterWindow, TEXT("CounterReady"), 0.70f},
		{BridgeAttackerMontagePackage,
			{{StaggerSequencePath, 0.0f, 0.70f, 1.0f}, {StaggerSequencePath, 0.70f, 0.72f, 0.20f}},
			{{TEXT("Bridge"), 0.0f}, {TEXT("CounterReady"), 0.70f}},
			TEXT("PairedTarget"), 0.0f, 0.70f, true, true},
		{CounterDefenderMontagePackage,
			{{CounterSequencePath, 0.0f, 1.45f, 1.0f}}, {{TEXT("Counter"), 0.0f}},
			TEXT("PairedTarget"), 0.0f, 0.55f, true, true, true,
			EPairedReactionType::Counter, 0.45f, true, true,
			EChainStageTransitionType::AutoContinue, TEXT("FinisherReady"), 1.30f},
		{CounterAttackerMontagePackage,
			{{StaggerSequencePath, 0.70f, 0.76f, 0.133333f},
			 {CounterReactionSequencePath, 0.0f, 1.0f, 1.0f}},
			{{TEXT("Counter"), 0.0f}}, TEXT("PairedTarget"), 0.0f, 0.55f, true, true},
		{FinisherDefenderMontagePackage,
			{{FinisherSequencePath, 0.0f, 2.916667f, 1.0f}}, {{TEXT("Finisher"), 0.0f}},
			TEXT("PairedTarget"), 0.0f, 0.75f, true, true, true,
			EPairedReactionType::Finisher, 0.55f, true},
		{FinisherAttackerMontagePackage,
			{{FinisherReactionSequencePath, 0.0f, 2.916667f, 1.0f}}, {{TEXT("Finisher"), 0.0f}},
			TEXT("PairedTarget"), 0.0f, 0.75f, true, true}
	};
}

TArray<FDefenseAuthoringPairedSpec> BuildPairedSpecs()
{
	return {
		{BridgeDataPackage, EPairedReactionType::Parry,
			BridgeDefenderMontagePackage, TEXT("Bridge"), BridgeAttackerMontagePackage, TEXT("Bridge"),
			TEXT("CounterReady"), TEXT("CounterReady"), TEXT("CounterReady"),
			false, false, false, 0.30f, 0.0f, false},
		{CounterDataPackage, EPairedReactionType::Counter,
			CounterDefenderMontagePackage, TEXT("Counter"), CounterAttackerMontagePackage, TEXT("Counter"),
			TEXT("FinisherReady"), NAME_None, NAME_None,
			true, true, true, 0.45f, 25.0f, false},
		{FinisherDataPackage, EPairedReactionType::Finisher,
			FinisherDefenderMontagePackage, TEXT("Finisher"), FinisherAttackerMontagePackage, TEXT("Finisher"),
			NAME_None, NAME_None, NAME_None,
			false, false, false, 0.55f, 100.0f, true}
	};
}

bool IsApplyMode(const EKatanaAssetMigrationMode Mode)
{
	return Mode == EKatanaAssetMigrationMode::Apply
		|| Mode == EKatanaAssetMigrationMode::ApplyAndSave;
}

template <typename TObjectType>
TObjectType* LoadObjectAtPath(const FString& ObjectPath)
{
	return Cast<TObjectType>(StaticLoadObject(TObjectType::StaticClass(), nullptr, *ObjectPath));
}

template <typename TObjectType>
TObjectType* LoadObjectAtPackage(const FString& PackageName)
{
	return LoadObjectAtPath<TObjectType>(BuildObjectPath(PackageName));
}

float SegmentDuration(const FDefenseAuthoringSegmentSpec& Segment)
{
	return (Segment.SourceEnd - Segment.SourceStart) / FMath::Abs(Segment.PlayRate);
}

float MontageDuration(const FDefenseAuthoringMontageSpec& Spec)
{
	float Result = 0.0f;
	for (const FDefenseAuthoringSegmentSpec& Segment : Spec.Segments)
	{
		Result += SegmentDuration(Segment);
	}
	return Result;
}

UObject* FindExistingAsset(const FString& ObjectPath)
{
	if (UObject* Loaded = FindObject<UObject>(nullptr, *ObjectPath))
	{
		return Loaded;
	}
	const FAssetData AssetData =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"))
		.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
	return AssetData.IsValid() ? AssetData.GetAsset() : nullptr;
}

UPackage* CreateOrLoadAssetPackage(const FString& PackageName, FString& OutError)
{
	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		OutError = FString::Printf(TEXT("invalid destination package: %s"), *PackageName);
		return nullptr;
	}
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		OutError = FString::Printf(TEXT("could not create destination package: %s"), *PackageName);
		return nullptr;
	}
	Package->FullyLoad();
	return Package;
}

bool ValidateMontageSources(
	const FDefenseAuthoringMontageSpec& Spec,
	TArray<FString>& OutErrors)
{
	USkeleton* ExpectedSkeleton = nullptr;
	for (const FDefenseAuthoringSegmentSpec& Segment : Spec.Segments)
	{
		UAnimSequenceBase* Source = LoadObjectAtPath<UAnimSequenceBase>(Segment.SourcePath);
		if (!Source)
		{
			OutErrors.Add(FString::Printf(TEXT("source animation did not load: %s"), *Segment.SourcePath));
			continue;
		}
		if (!FMath::IsFinite(Segment.SourceStart)
			|| !FMath::IsFinite(Segment.SourceEnd)
			|| !FMath::IsFinite(Segment.PlayRate)
			|| Segment.SourceStart < 0.0f
			|| Segment.SourceEnd <= Segment.SourceStart
			|| FMath::IsNearlyZero(Segment.PlayRate)
			|| Segment.SourceEnd > Source->GetPlayLength() + FloatTolerance)
		{
			OutErrors.Add(FString::Printf(
				TEXT("invalid reviewed segment range for %s: %.6f..%.6f at %.6f (source length %.6f)"),
				*Segment.SourcePath, Segment.SourceStart, Segment.SourceEnd,
				Segment.PlayRate, Source->GetPlayLength()));
		}
		if (!Source->GetSkeleton())
		{
			OutErrors.Add(FString::Printf(TEXT("source animation has no skeleton: %s"), *Segment.SourcePath));
		}
		else if (!ExpectedSkeleton)
		{
			ExpectedSkeleton = Source->GetSkeleton();
		}
		else if (!ExpectedSkeleton->IsCompatibleForEditor(Source->GetSkeleton()))
		{
			OutErrors.Add(FString::Printf(TEXT("montage sources use incompatible skeletons: %s"),
				*Spec.PackageName));
		}
	}
	return OutErrors.IsEmpty();
}

bool NearlyEqual(const float Left, const float Right)
{
	return FMath::IsNearlyEqual(Left, Right, FloatTolerance);
}

FName ExpectedSyncPointName(const EPairedReactionType Reaction)
{
	return Reaction == EPairedReactionType::Parry
		? FName(TEXT("Deflect"))
		: Reaction == EPairedReactionType::Counter
			? FName(TEXT("CounterImpact")) : FName(TEXT("FinisherImpact"));
}

bool MotionWarpMatches(
	const UAnimMontage* Montage,
	const FDefenseAuthoringMontageSpec& Spec)
{
	int32 MatchCount = 0;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const UAnimNotifyState_MotionWarping* Notify =
			Cast<UAnimNotifyState_MotionWarping>(Event.NotifyStateClass);
		if (!Notify)
		{
			continue;
		}
		const URootMotionModifier_SkewWarp* Warp =
			Cast<URootMotionModifier_SkewWarp>(Notify->RootMotionModifier);
		if (Warp
			&& Warp->WarpTargetName == Spec.WarpTarget
			&& Warp->bWarpRotation
			&& Warp->bWarpTranslation == Spec.bWarpTranslation
			&& NearlyEqual(Event.GetTriggerTime(), Spec.WarpStart)
			&& NearlyEqual(Event.GetDuration(), Spec.WarpEnd - Spec.WarpStart))
		{
			++MatchCount;
		}
		else
		{
			return false;
		}
	}
	return MatchCount == 1;
}

bool CollisionNotifyMatches(
	const UAnimMontage* Montage,
	const FDefenseAuthoringMontageSpec& Spec)
{
	int32 Count = 0;
	const float ExpectedDuration = FMath::Max(0.01f, MontageDuration(Spec) - 0.01f);
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const UAnimNotifyState_PairedAnimationCollision* Notify =
			Cast<UAnimNotifyState_PairedAnimationCollision>(Event.NotifyStateClass);
		if (Notify)
		{
			++Count;
			if (!NearlyEqual(Event.GetTriggerTime(), 0.0f)
				|| !NearlyEqual(Event.GetDuration(), ExpectedDuration)
				|| !Notify->bUseTrackedPartnersOnly
				|| !Notify->bDisablePawnCollision
				|| Notify->bDisableCapsulePhysics
				|| !Notify->bScanForDynamicObstructions
				|| !NearlyEqual(Notify->DynamicObstructionRadius, 150.0f)
				|| !Notify->bDisableMovement)
			{
				return false;
			}
		}
	}
	return Count == (Spec.bAddCollision ? 1 : 0);
}

bool SyncNotifyMatches(
	const UAnimMontage* Montage,
	const FDefenseAuthoringMontageSpec& Spec)
{
	int32 Count = 0;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const UAnimNotifyState_PairedAnimationSync* Notify =
			Cast<UAnimNotifyState_PairedAnimationSync>(Event.NotifyStateClass);
		if (!Notify)
		{
			continue;
		}
		++Count;
		if (!NearlyEqual(Event.GetTriggerTime(), Spec.SyncTime)
			|| !NearlyEqual(Event.GetDuration(), 0.08f)
			|| Notify->SyncPointName != ExpectedSyncPointName(Spec.SyncReaction)
			|| Notify->ReactionType != Spec.SyncReaction
			|| Notify->bApplyDamage != Spec.bSyncAppliesDamage
			|| Notify->bTriggerCameraShake != Spec.bSyncAppliesDamage
			|| Notify->VictimContactBone != TEXT("spine_03"))
		{
			return false;
		}
	}
	return Count == (Spec.bAddSync ? 1 : 0);
}

bool ChainNotifyMatches(
	const UAnimMontage* Montage,
	const FDefenseAuthoringMontageSpec& Spec)
{
	int32 Count = 0;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const UAnimNotify_ChainStageTransition* Notify =
			Cast<UAnimNotify_ChainStageTransition>(Event.Notify);
		if (!Notify)
		{
			continue;
		}
		++Count;
		if (!NearlyEqual(Event.GetTriggerTime(), Spec.ChainTime)
			|| Notify->Transition != Spec.ChainTransition
			|| Notify->MarkerName != Spec.ChainMarker)
		{
			return false;
		}
	}
	return Count == (Spec.bAddChainMarker ? 1 : 0);
}

bool MontageMatches(
	const UAnimMontage* Montage,
	const FDefenseAuthoringMontageSpec& Spec)
{
	if (!Montage || Montage->SlotAnimTracks.Num() != 1
		|| Montage->SlotAnimTracks[0].SlotName != TEXT("DefaultSlot")
		|| Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != Spec.Segments.Num()
		|| Montage->CompositeSections.Num() != Spec.Sections.Num()
		|| Montage->Notifies.Num() != 1
			+ (Spec.bAddCollision ? 1 : 0)
			+ (Spec.bAddSync ? 1 : 0)
			+ (Spec.bAddChainMarker ? 1 : 0)
		|| !NearlyEqual(Montage->GetPlayLength(), MontageDuration(Spec)))
	{
		return false;
	}

	float TimelineStart = 0.0f;
	for (int32 Index = 0; Index < Spec.Segments.Num(); ++Index)
	{
		const FDefenseAuthoringSegmentSpec& Expected = Spec.Segments[Index];
		const FAnimSegment& Actual = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[Index];
		if (!Actual.GetAnimReference()
			|| Actual.GetAnimReference()->GetPathName() != Expected.SourcePath
			|| !NearlyEqual(Actual.StartPos, TimelineStart)
			|| !NearlyEqual(Actual.AnimStartTime, Expected.SourceStart)
			|| !NearlyEqual(Actual.AnimEndTime, Expected.SourceEnd)
			|| !NearlyEqual(Actual.AnimPlayRate, Expected.PlayRate)
			|| Actual.LoopingCount != 1)
		{
			return false;
		}
		TimelineStart += SegmentDuration(Expected);
	}
	for (int32 Index = 0; Index < Spec.Sections.Num(); ++Index)
	{
		const FCompositeSection& Actual = Montage->CompositeSections[Index];
		if (Actual.SectionName != Spec.Sections[Index].Name
			|| !NearlyEqual(Actual.GetTime(), Spec.Sections[Index].Time))
		{
			return false;
		}
	}
	return MotionWarpMatches(Montage, Spec)
		&& CollisionNotifyMatches(Montage, Spec)
		&& SyncNotifyMatches(Montage, Spec)
		&& ChainNotifyMatches(Montage, Spec);
}

void AddStateNotify(
	UAnimMontage* Montage,
	UAnimNotifyState* Notify,
	const float Start,
	const float Duration)
{
	FAnimNotifyEvent Event;
	Event.NotifyStateClass = Notify;
	Event.SetTime(Start);
	Event.SetDuration(Duration);
	Montage->Notifies.Add(MoveTemp(Event));
}

UAnimMontage* CreateMontage(
	const FDefenseAuthoringMontageSpec& Spec,
	TArray<FString>& OutErrors)
{
	FString PackageError;
	UPackage* Package = CreateOrLoadAssetPackage(Spec.PackageName, PackageError);
	if (!Package)
	{
		OutErrors.Add(PackageError);
		return nullptr;
	}
	const FString AssetName = FPackageName::GetLongPackageAssetName(Spec.PackageName);
	UAnimMontage* Montage = NewObject<UAnimMontage>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!Montage)
	{
		OutErrors.Add(FString::Printf(TEXT("could not create montage: %s"), *Spec.PackageName));
		return nullptr;
	}

	if (Montage->SlotAnimTracks.Num() != 1
		|| Montage->SlotAnimTracks[0].SlotName != TEXT("DefaultSlot"))
	{
		OutErrors.Add(FString::Printf(TEXT("new montage has an unexpected default slot layout: %s"),
			*Spec.PackageName));
		return nullptr;
	}
	FSlotAnimationTrack& Slot = Montage->SlotAnimTracks[0];
	float TimelineStart = 0.0f;
	for (const FDefenseAuthoringSegmentSpec& SegmentSpec : Spec.Segments)
	{
		UAnimSequenceBase* Source = LoadObjectAtPath<UAnimSequenceBase>(SegmentSpec.SourcePath);
		if (!Source)
		{
			OutErrors.Add(FString::Printf(TEXT("source animation disappeared before apply: %s"),
				*SegmentSpec.SourcePath));
			return nullptr;
		}
		if (!Montage->GetSkeleton())
		{
			Montage->SetSkeleton(Source->GetSkeleton());
		}
		FAnimSegment Segment;
		Segment.SetAnimReference(Source);
		Segment.StartPos = TimelineStart;
		Segment.AnimStartTime = SegmentSpec.SourceStart;
		Segment.AnimEndTime = SegmentSpec.SourceEnd;
		Segment.AnimPlayRate = SegmentSpec.PlayRate;
		Segment.LoopingCount = 1;
		Slot.AnimTrack.AnimSegments.Add(MoveTemp(Segment));
		TimelineStart += SegmentDuration(SegmentSpec);
	}
	Montage->SetCompositeLength(TimelineStart);
	for (const FDefenseAuthoringSectionSpec& SectionSpec : Spec.Sections)
	{
		FCompositeSection Section;
		Section.SectionName = SectionSpec.Name;
		Section.SetTime(SectionSpec.Time);
		Montage->CompositeSections.Add(MoveTemp(Section));
	}

	UAnimNotifyState_MotionWarping* MotionWarp =
		NewObject<UAnimNotifyState_MotionWarping>(Montage);
	URootMotionModifier_SkewWarp* Warp = NewObject<URootMotionModifier_SkewWarp>(MotionWarp);
	Warp->WarpTargetName = Spec.WarpTarget;
	Warp->bWarpRotation = true;
	Warp->bWarpTranslation = Spec.bWarpTranslation;
	MotionWarp->RootMotionModifier = Warp;
	AddStateNotify(Montage, MotionWarp, Spec.WarpStart, Spec.WarpEnd - Spec.WarpStart);

	if (Spec.bAddCollision)
	{
		UAnimNotifyState_PairedAnimationCollision* Collision =
			NewObject<UAnimNotifyState_PairedAnimationCollision>(Montage);
		AddStateNotify(Montage, Collision, 0.0f,
			FMath::Max(0.01f, TimelineStart - 0.01f));
	}
	if (Spec.bAddSync)
	{
		UAnimNotifyState_PairedAnimationSync* Sync =
			NewObject<UAnimNotifyState_PairedAnimationSync>(Montage);
		Sync->SyncPointName = ExpectedSyncPointName(Spec.SyncReaction);
		Sync->ReactionType = Spec.SyncReaction;
		Sync->VictimContactBone = TEXT("spine_03");
		Sync->bApplyDamage = Spec.bSyncAppliesDamage;
		Sync->bTriggerCameraShake = Spec.bSyncAppliesDamage;
		AddStateNotify(Montage, Sync, Spec.SyncTime, 0.08f);
	}
	if (Spec.bAddChainMarker)
	{
		UAnimNotify_ChainStageTransition* Marker =
			NewObject<UAnimNotify_ChainStageTransition>(Montage);
		Marker->Transition = Spec.ChainTransition;
		Marker->MarkerName = Spec.ChainMarker;
		FAnimNotifyEvent Event;
		Event.Notify = Marker;
		Event.SetTime(Spec.ChainTime);
		Montage->Notifies.Add(MoveTemp(Event));
	}

	Montage->SortNotifies();
	Montage->RefreshCacheData();
	FAssetRegistryModule::AssetCreated(Montage);
	Montage->MarkPackageDirty();
	return Montage;
}

bool PairedDataMatches(
	const UPairedAnimationData* Data,
	const FDefenseAuthoringPairedSpec& Spec)
{
	const float RoleWarpLimit = Spec.Reaction == EPairedReactionType::Parry ? 75.0f : 300.0f;
	const FString AssetName = FPackageName::GetLongPackageAssetName(Spec.PackageName);
	return Data
		&& Data->AnimationName == FName(*AssetName)
		&& Data->Description == TEXT("Gate A reviewed defense-chain proof asset.")
		&& Data->ReactionType == Spec.Reaction
		&& Data->AttackerMontage
		&& Data->AttackerMontage->GetPathName() == BuildObjectPath(Spec.AttackerMontagePackage)
		&& Data->AttackerMontageSection == Spec.AttackerSection
		&& Data->VictimMontage
		&& Data->VictimMontage->GetPathName() == BuildObjectPath(Spec.VictimMontagePackage)
		&& Data->VictimMontageSection == Spec.VictimSection
		&& Data->ChainTransitionPolicy.DriverRole == EPairedAnimationRole::Attacker
		&& Data->ChainTransitionPolicy.RequiredMarker == Spec.RequiredMarker
		&& Data->ChainTransitionPolicy.AttackerReadySection == Spec.AttackerReadySection
		&& Data->ChainTransitionPolicy.VictimReadySection == Spec.VictimReadySection
		&& Data->ChainTransitionPolicy.bAttackerTerminalPoseCompatible
			== Spec.bAttackerTerminalCompatible
		&& Data->ChainTransitionPolicy.bVictimTerminalPoseCompatible
			== Spec.bVictimTerminalCompatible
		&& NearlyEqual(Data->ChainTransitionPolicy.ResponseWindowOverride, 0.0f)
		&& Data->ChainTransitionPolicy.bAutoContinue == Spec.bAutoContinue
		&& Data->ChainTransitionPolicy.bFinisherRetryable
		&& NearlyEqual(Data->SyncPointTime, Spec.SyncPointTime)
		&& Data->SyncPointName == ExpectedSyncPointName(Spec.Reaction)
		&& NearlyEqual(Data->VictimStartOffset, 0.0f)
		&& NearlyEqual(Data->AttackerBlendIn, 0.10f)
		&& NearlyEqual(Data->AttackerBlendOut, 0.10f)
		&& NearlyEqual(Data->VictimBlendIn, 0.05f)
		&& NearlyEqual(Data->VictimBlendOut, 0.10f)
		&& NearlyEqual(Data->BaseDamage, Spec.BaseDamage)
		&& NearlyEqual(Data->DamageMultiplier, 1.0f)
		&& Data->bIsLethal == Spec.bLethal
		&& Data->VictimRelativePosition.Equals(FVector(100.0f, 0.0f, 0.0f), FloatTolerance)
		&& Data->VictimFacingMode == -1
		&& Data->VictimRelativeRotation.IsNearlyZero(FloatTolerance)
		&& NearlyEqual(Data->MinTriggerDistance, 0.0f)
		&& NearlyEqual(Data->MaxTriggerDistance, 300.0f)
		&& NearlyEqual(Data->MaxWarpDistance, RoleWarpLimit)
		&& Data->AttackerWarpConfig.WarpTargetName == TEXT("PairedTarget")
		&& Data->VictimWarpConfig.WarpTargetName == TEXT("PairedTarget")
		&& Data->AttackerWarpConfig.RelativeOffset.Equals(
			FVector(100.0f, 0.0f, 0.0f), FloatTolerance)
		&& Data->VictimWarpConfig.RelativeOffset.Equals(
			FVector(100.0f, 0.0f, 0.0f), FloatTolerance)
		&& NearlyEqual(Data->AttackerWarpConfig.MaxWarpDistance, RoleWarpLimit)
		&& NearlyEqual(Data->VictimWarpConfig.MaxWarpDistance, RoleWarpLimit)
		&& Data->AttackerWarpConfig.bWarpTranslation
		&& Data->VictimWarpConfig.bWarpTranslation
		&& Data->AttackerWarpConfig.bWarpRotation
		&& Data->VictimWarpConfig.bWarpRotation
		&& Data->AttackerWarpConfig.bAdjustToTerrain
		&& Data->VictimWarpConfig.bAdjustToTerrain
		&& !Data->bApplySlowMotion;
}

UPairedAnimationData* CreatePairedData(
	const FDefenseAuthoringPairedSpec& Spec,
	TArray<FString>& OutErrors)
{
	FString PackageError;
	UPackage* Package = CreateOrLoadAssetPackage(Spec.PackageName, PackageError);
	if (!Package)
	{
		OutErrors.Add(PackageError);
		return nullptr;
	}
	UAnimMontage* AttackerMontage = LoadObjectAtPackage<UAnimMontage>(Spec.AttackerMontagePackage);
	UAnimMontage* VictimMontage = LoadObjectAtPackage<UAnimMontage>(Spec.VictimMontagePackage);
	if (!AttackerMontage || !VictimMontage)
	{
		OutErrors.Add(FString::Printf(TEXT("paired montage dependency missing for %s"),
			*Spec.PackageName));
		return nullptr;
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(Spec.PackageName);
	UPairedAnimationData* Data = NewObject<UPairedAnimationData>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	Data->AnimationName = FName(*AssetName);
	Data->Description = TEXT("Gate A reviewed defense-chain proof asset.");
	Data->ReactionType = Spec.Reaction;
	Data->AttackerMontage = AttackerMontage;
	Data->AttackerMontageSection = Spec.AttackerSection;
	Data->VictimMontage = VictimMontage;
	Data->VictimMontageSection = Spec.VictimSection;
	Data->ChainTransitionPolicy.DriverRole = EPairedAnimationRole::Attacker;
	Data->ChainTransitionPolicy.RequiredMarker = Spec.RequiredMarker;
	Data->ChainTransitionPolicy.AttackerReadySection = Spec.AttackerReadySection;
	Data->ChainTransitionPolicy.VictimReadySection = Spec.VictimReadySection;
	Data->ChainTransitionPolicy.bAttackerTerminalPoseCompatible =
		Spec.bAttackerTerminalCompatible;
	Data->ChainTransitionPolicy.bVictimTerminalPoseCompatible =
		Spec.bVictimTerminalCompatible;
	Data->ChainTransitionPolicy.bAutoContinue = Spec.bAutoContinue;
	Data->ChainTransitionPolicy.bFinisherRetryable = true;
	Data->SyncPointTime = Spec.SyncPointTime;
	Data->SyncPointName = ExpectedSyncPointName(Spec.Reaction);
	Data->VictimStartOffset = 0.0f;
	Data->AttackerBlendIn = 0.10f;
	Data->AttackerBlendOut = 0.10f;
	Data->VictimBlendIn = 0.05f;
	Data->VictimBlendOut = 0.10f;
	Data->VictimRelativePosition = FVector(100.0f, 0.0f, 0.0f);
	Data->VictimFacingMode = -1;
	Data->MinTriggerDistance = 0.0f;
	Data->MaxTriggerDistance = 300.0f;
	const float RoleWarpLimit = Spec.Reaction == EPairedReactionType::Parry ? 75.0f : 300.0f;
	Data->MaxWarpDistance = RoleWarpLimit;
	for (FPairedWarpConfig* WarpConfig :
		{&Data->AttackerWarpConfig, &Data->VictimWarpConfig})
	{
		WarpConfig->WarpTargetName = TEXT("PairedTarget");
		WarpConfig->RelativeOffset = FVector(100.0f, 0.0f, 0.0f);
		WarpConfig->MaxWarpDistance = RoleWarpLimit;
		WarpConfig->bWarpTranslation = true;
		WarpConfig->bWarpRotation = true;
		WarpConfig->bAdjustToTerrain = true;
	}
	Data->bApplySlowMotion = false;
	Data->BaseDamage = Spec.BaseDamage;
	Data->DamageMultiplier = 1.0f;
	Data->bIsLethal = Spec.bLethal;
	FAssetRegistryModule::AssetCreated(Data);
	Data->MarkPackageDirty();
	return Data;
}

bool PayloadMatches(
	const FDefensePresentationPayload& Payload,
	const FString& MontagePackage,
	const FName Section,
	const FString& PairedPackage,
	const float MaximumTranslation,
	const FName Marker,
	const bool bRequiresPreflight)
{
	const bool bHasPresentation = !MontagePackage.IsEmpty() || !PairedPackage.IsEmpty();
	return ((!MontagePackage.IsEmpty() && Payload.Montage
			&& Payload.Montage->GetPathName() == BuildObjectPath(MontagePackage))
		|| (MontagePackage.IsEmpty() && !Payload.Montage))
		&& Payload.MontageSection == Section
		&& ((!PairedPackage.IsEmpty() && Payload.PairedBridgeData
			&& Payload.PairedBridgeData->GetPathName() == BuildObjectPath(PairedPackage))
			|| (PairedPackage.IsEmpty() && !Payload.PairedBridgeData))
		&& Payload.bEnableRotationWarp == !MontagePackage.IsEmpty()
		&& NearlyEqual(Payload.BlendInSeconds, 0.10f)
		&& NearlyEqual(Payload.BlendOutSeconds, 0.10f)
		&& NearlyEqual(Payload.MaximumTranslation, MaximumTranslation)
		&& !Payload.bOverrideImpactAudio
		&& !Payload.bOverrideImpactVFX
		&& !Payload.bOverrideHitstop
		&& Payload.ReviewedDeflectionMarker == Marker
		&& Payload.SourceSocketOverride
			== (bHasPresentation ? FName(TEXT("weapon_end")) : NAME_None)
		&& Payload.TargetBoneOverride
			== (bHasPresentation ? FName(TEXT("spine_03")) : NAME_None)
		&& Payload.bRequiresBridgePreflight == bRequiresPreflight;
}

bool ImpactAudioMatches(const FImpactAudioConfig& Config, const FString& SoundPath)
{
	return Config.ImpactSound
		&& Config.ImpactSound->GetPathName() == SoundPath
		&& NearlyEqual(Config.VolumeMultiplier, 1.0f)
		&& NearlyEqual(Config.PitchMultiplier, 1.0f)
		&& NearlyEqual(Config.PitchVariation, 0.05f)
		&& Config.bUseWeaponFallback;
}

bool ImpactVFXMatches(const FImpactVFXConfig& Config, const FString& VFXPath)
{
	return Config.ImpactVFX
		&& Config.ImpactVFX->GetPathName() == VFXPath
		&& NearlyEqual(Config.ScaleMultiplier, 1.0f)
		&& Config.bAlignToSurface
		&& Config.bUseWeaponFallback;
}

bool DefenseConfigurationMatches(const UDefenseConfiguration* Configuration)
{
	if (!Configuration
		|| !NearlyEqual(Configuration->HardGuardConeHalfAngle, 70.0f)
		|| !NearlyEqual(Configuration->MaximumAutomaticTurn, 70.0f)
		|| !NearlyEqual(Configuration->DefenseTurnRate, 180.0f)
		|| !NearlyEqual(Configuration->NormalBlockFinalTolerance, 35.0f)
		|| !NearlyEqual(Configuration->PerfectParryFinalTolerance, 10.0f)
		|| !ImpactAudioMatches(Configuration->DefaultBlockImpactAudio, ImpactAudioPath)
		|| !ImpactAudioMatches(Configuration->DefaultParryImpactAudio, ImpactAudioPath)
		|| !ImpactVFXMatches(Configuration->DefaultBlockImpactVFX, ImpactVFXPath)
		|| !ImpactVFXMatches(Configuration->DefaultParryImpactVFX, ImpactVFXPath)
		|| !NearlyEqual(Configuration->PerfectParryTranslationAllowancePerRole, 75.0f)
		|| !NearlyEqual(Configuration->NormalBlockTranslationAllowance, 0.0f)
		|| !NearlyEqual(Configuration->NormalBlockTranslationDriftTolerance, 1.0f)
		|| Configuration->GuardEnterMontage
		|| Configuration->GuardExitMontage
		|| Configuration->BoneHeightRows.Num() != 3
		|| Configuration->DefenderPresentationRows.Num() != 2
		|| Configuration->AttackerResponseRows.Num() != 2)
	{
		return false;
	}
	const TArray<FDefenseBoneHeightRow>& Bones = Configuration->BoneHeightRows;
	if (Bones[0].BoneName != TEXT("head") || Bones[0].Height != EAttackHeight::High
		|| Bones[1].BoneName != TEXT("spine_03") || Bones[1].Height != EAttackHeight::Middle
		|| Bones[2].BoneName != TEXT("pelvis") || Bones[2].Height != EAttackHeight::Low)
	{
		return false;
	}
	const FDefensePresentationRow& Block = Configuration->DefenderPresentationRows[0];
	const FDefensePresentationRow& Parry = Configuration->DefenderPresentationRows[1];
	const FAttackerResponsePresentationRow& Continue = Configuration->AttackerResponseRows[0];
	const FAttackerResponsePresentationRow& Stagger = Configuration->AttackerResponseRows[1];
	return Block.RowName == TEXT("NormalBlockGeneric")
		&& Block.Outcome == EDefenseOutcome::NormalBlock
		&& Block.IsGenericFallback()
		&& PayloadMatches(Block.Payload, BlockMontagePackage, TEXT("Block"), FString(),
			0.0f, NAME_None, false)
		&& Parry.RowName == TEXT("PerfectParryGeneric")
		&& Parry.Outcome == EDefenseOutcome::PerfectParry
		&& Parry.IsGenericFallback()
		&& PayloadMatches(Parry.Payload, ParryFallbackMontagePackage, TEXT("Parry"),
			BridgeDataPackage, 75.0f, TEXT("CounterReady"), true)
		&& Continue.RowName == TEXT("ContinueGeneric")
		&& Continue.Response == EAttackerResponse::Continue
		&& Continue.IsGenericFallback()
		&& PayloadMatches(Continue.Payload, FString(), NAME_None, FString(),
			0.0f, NAME_None, false)
		&& Continue.Payload.IsEmpty()
		&& Stagger.RowName == TEXT("ParryStaggerGeneric")
		&& Stagger.Response == EAttackerResponse::ParryStagger
		&& Stagger.IsGenericFallback()
		&& PayloadMatches(Stagger.Payload, StaggerFallbackMontagePackage, TEXT("Stagger"),
			FString(), 75.0f, NAME_None, false);
}

void ConfigurePresentationPayload(
	FDefensePresentationPayload& Payload,
	const FString& MontagePackage,
	const FName Section,
	const FString& PairedPackage,
	const float MaximumTranslation,
	const FName Marker,
	const bool bRequiresPreflight)
{
	Payload.Montage = MontagePackage.IsEmpty()
		? nullptr : LoadObjectAtPackage<UAnimMontage>(MontagePackage);
	Payload.MontageSection = Section;
	Payload.PairedBridgeData = PairedPackage.IsEmpty()
		? nullptr : LoadObjectAtPackage<UPairedAnimationData>(PairedPackage);
	Payload.BlendInSeconds = 0.10f;
	Payload.BlendOutSeconds = 0.10f;
	Payload.bEnableRotationWarp = !MontagePackage.IsEmpty();
	Payload.MaximumTranslation = MaximumTranslation;
	Payload.ReviewedDeflectionMarker = Marker;
	const bool bHasPresentation = !MontagePackage.IsEmpty() || !PairedPackage.IsEmpty();
	Payload.SourceSocketOverride = bHasPresentation ? FName(TEXT("weapon_end")) : NAME_None;
	Payload.TargetBoneOverride = bHasPresentation ? FName(TEXT("spine_03")) : NAME_None;
	Payload.bRequiresBridgePreflight = bRequiresPreflight;
}

UDefenseConfiguration* CreateDefenseConfiguration(TArray<FString>& OutErrors)
{
	FString PackageError;
	UPackage* Package = CreateOrLoadAssetPackage(DefenseConfigurationPackage, PackageError);
	if (!Package)
	{
		OutErrors.Add(PackageError);
		return nullptr;
	}
	USoundBase* ImpactAudio = LoadObjectAtPath<USoundBase>(ImpactAudioPath);
	UNiagaraSystem* ImpactVFX = LoadObjectAtPath<UNiagaraSystem>(ImpactVFXPath);
	if (!ImpactAudio || !ImpactVFX)
	{
		OutErrors.Add(TEXT("reviewed defense impact audio or VFX did not load"));
		return nullptr;
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(DefenseConfigurationPackage);
	UDefenseConfiguration* Configuration = NewObject<UDefenseConfiguration>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	Configuration->HardGuardConeHalfAngle = 70.0f;
	Configuration->MaximumAutomaticTurn = 70.0f;
	Configuration->DefenseTurnRate = 180.0f;
	Configuration->NormalBlockFinalTolerance = 35.0f;
	Configuration->PerfectParryFinalTolerance = 10.0f;
	Configuration->NormalBlockTranslationAllowance = 0.0f;
	Configuration->NormalBlockTranslationDriftTolerance = 1.0f;
	Configuration->PerfectParryTranslationAllowancePerRole = 75.0f;
	Configuration->BoneHeightRows = {
		{TEXT("head"), EAttackHeight::High},
		{TEXT("spine_03"), EAttackHeight::Middle},
		{TEXT("pelvis"), EAttackHeight::Low}};
	Configuration->DefaultBlockImpactAudio.ImpactSound = ImpactAudio;
	Configuration->DefaultParryImpactAudio.ImpactSound = ImpactAudio;
	Configuration->DefaultBlockImpactVFX.ImpactVFX = ImpactVFX;
	Configuration->DefaultParryImpactVFX.ImpactVFX = ImpactVFX;

	FDefensePresentationRow Block;
	Block.RowName = TEXT("NormalBlockGeneric");
	Block.Outcome = EDefenseOutcome::NormalBlock;
	ConfigurePresentationPayload(Block.Payload, BlockMontagePackage, TEXT("Block"),
		FString(), 0.0f, NAME_None, false);
	FDefensePresentationRow Parry;
	Parry.RowName = TEXT("PerfectParryGeneric");
	Parry.Outcome = EDefenseOutcome::PerfectParry;
	ConfigurePresentationPayload(Parry.Payload, ParryFallbackMontagePackage, TEXT("Parry"),
		BridgeDataPackage, 75.0f, TEXT("CounterReady"), true);
	Configuration->DefenderPresentationRows = {Block, Parry};

	FAttackerResponsePresentationRow Continue;
	Continue.RowName = TEXT("ContinueGeneric");
	Continue.Response = EAttackerResponse::Continue;
	ConfigurePresentationPayload(Continue.Payload, FString(), NAME_None, FString(),
		0.0f, NAME_None, false);
	FAttackerResponsePresentationRow Stagger;
	Stagger.RowName = TEXT("ParryStaggerGeneric");
	Stagger.Response = EAttackerResponse::ParryStagger;
	ConfigurePresentationPayload(Stagger.Payload, StaggerFallbackMontagePackage,
		TEXT("Stagger"), FString(), 75.0f, NAME_None, false);
	Configuration->AttackerResponseRows = {Continue, Stagger};

	FAssetRegistryModule::AssetCreated(Configuration);
	Configuration->MarkPackageDirty();
	return Configuration;
}

struct FGuardGraphNodes
{
	UAnimationStateMachineGraph* StateMachine = nullptr;
	UAnimStateNode* Idle = nullptr;
	UAnimStateNode* Locomotion = nullptr;
	UAnimStateNode* Guard = nullptr;
};

FGuardGraphNodes FindGuardGraphNodes(UAnimBlueprint* AnimBlueprint)
{
	FGuardGraphNodes Result;
	if (!AnimBlueprint)
	{
		return Result;
	}
	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		UAnimationStateMachineGraph* StateMachine = Cast<UAnimationStateMachineGraph>(Graph);
		if (!StateMachine)
		{
			continue;
		}
		FGuardGraphNodes Candidate;
		Candidate.StateMachine = StateMachine;
		for (UEdGraphNode* Node : StateMachine->Nodes)
		{
			UAnimStateNode* State = Cast<UAnimStateNode>(Node);
			if (!State)
			{
				continue;
			}
			const FString StateName = State->GetStateName();
			if (StateName.Equals(TEXT("Idle"), ESearchCase::IgnoreCase))
			{
				Candidate.Idle = State;
			}
			else if (StateName.Equals(TEXT("Locomotion"), ESearchCase::IgnoreCase))
			{
				Candidate.Locomotion = State;
			}
			else if (StateName.Equals(TEXT("Guard"), ESearchCase::IgnoreCase))
			{
				Candidate.Guard = State;
			}
		}
		if (Candidate.Idle && Candidate.Locomotion)
		{
			return Candidate;
		}
	}
	return Result;
}

bool TransitionRuleMatches(
	const UAnimStateTransitionNode* Transition,
	const bool bExpectedNegated)
{
	const UAnimationTransitionGraph* RuleGraph = Transition
		? Cast<UAnimationTransitionGraph>(Transition->BoundGraph) : nullptr;
	const UAnimGraphNode_TransitionResult* ResultNode = RuleGraph
		? RuleGraph->MyResultNode.Get() : nullptr;
	const UEdGraphPin* ResultPin = ResultNode
		? ResultNode->FindPin(TEXT("bCanEnterTransition")) : nullptr;
	if (!RuleGraph || !ResultPin)
	{
		return false;
	}

	for (const UEdGraphNode* Node : RuleGraph->Nodes)
	{
		const UK2Node_VariableGet* Variable = Cast<UK2Node_VariableGet>(Node);
		if (!Variable
			|| Variable->GetVarName()
				!= GET_MEMBER_NAME_CHECKED(USamuraiAnimInstance, bIsBlocking))
		{
			continue;
		}
		const UEdGraphPin* ValuePin = Variable->GetValuePin();
		if (!bExpectedNegated)
		{
			return ValuePin && ValuePin->LinkedTo.Contains(ResultPin);
		}
		for (const UEdGraphPin* Linked : ValuePin ? ValuePin->LinkedTo : TArray<UEdGraphPin*>())
		{
			const UK2Node_CallFunction* Call = Linked
				? Cast<UK2Node_CallFunction>(Linked->GetOwningNode()) : nullptr;
			if (Call
				&& Call->FunctionReference.GetMemberName()
					== GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool)
				&& Call->GetReturnValuePin()
				&& Call->GetReturnValuePin()->LinkedTo.Contains(ResultPin))
			{
				return true;
			}
		}
	}
	return false;
}

const UAnimStateTransitionNode* FindTransition(
	const UAnimationStateMachineGraph* StateMachine,
	const UAnimStateNode* Previous,
	const UAnimStateNode* Next)
{
	if (!StateMachine || !Previous || !Next)
	{
		return nullptr;
	}
	for (const UEdGraphNode* Node : StateMachine->Nodes)
	{
		const UAnimStateTransitionNode* Transition = Cast<UAnimStateTransitionNode>(Node);
		if (Transition
			&& Transition->GetPreviousState() == Previous
			&& Transition->GetNextState() == Next)
		{
			return Transition;
		}
	}
	return nullptr;
}

bool GuardStateMatches(const FGuardGraphNodes& Nodes)
{
	if (!Nodes.StateMachine || !Nodes.Idle || !Nodes.Locomotion || !Nodes.Guard
		|| !Nodes.Guard->BoundGraph)
	{
		return false;
	}
	int32 AssetPlayerCount = 0;
	int32 MatchingAssetPlayers = 0;
	const UEdGraphPin* PoseSink = Nodes.Guard->GetPoseSinkPinInsideState();
	for (const UEdGraphNode* Node : Nodes.Guard->BoundGraph->Nodes)
	{
		const UAnimGraphNode_AssetPlayerBase* Player =
			Cast<UAnimGraphNode_AssetPlayerBase>(Node);
		if (!Player)
		{
			continue;
		}
		++AssetPlayerCount;
		const UAnimGraphNode_SequencePlayer* SequencePlayer =
			Cast<UAnimGraphNode_SequencePlayer>(Player);
		bool bPoseConnected = false;
		for (const UEdGraphPin* Pin : Player->Pins)
		{
			bPoseConnected |= Pin
				&& Pin->Direction == EGPD_Output
				&& PoseSink
				&& Pin->LinkedTo.Contains(PoseSink);
		}
		if (SequencePlayer
			&& SequencePlayer->Node.GetSequence()
			&& SequencePlayer->Node.GetSequence()->GetPathName() == GuardSequencePath
			&& SequencePlayer->Node.IsLooping()
			&& bPoseConnected)
		{
			++MatchingAssetPlayers;
		}
	}
	const UAnimStateTransitionNode* IdleToGuard =
		FindTransition(Nodes.StateMachine, Nodes.Idle, Nodes.Guard);
	const UAnimStateTransitionNode* LocomotionToGuard =
		FindTransition(Nodes.StateMachine, Nodes.Locomotion, Nodes.Guard);
	const UAnimStateTransitionNode* GuardToLocomotion =
		FindTransition(Nodes.StateMachine, Nodes.Guard, Nodes.Locomotion);
	return AssetPlayerCount == 1
		&& MatchingAssetPlayers == 1
		&& TransitionRuleMatches(IdleToGuard, false)
		&& TransitionRuleMatches(LocomotionToGuard, false)
		&& TransitionRuleMatches(GuardToLocomotion, true)
		&& NearlyEqual(IdleToGuard->CrossfadeDuration, 0.10f)
		&& NearlyEqual(LocomotionToGuard->CrossfadeDuration, 0.10f)
		&& NearlyEqual(GuardToLocomotion->CrossfadeDuration, 0.10f);
}

bool ConnectTransitionRule(
	UAnimStateTransitionNode* Transition,
	const bool bNegated,
	TArray<FString>& OutErrors)
{
	UAnimationTransitionGraph* RuleGraph = Transition
		? Cast<UAnimationTransitionGraph>(Transition->BoundGraph) : nullptr;
	UAnimGraphNode_TransitionResult* ResultNode = RuleGraph
		? RuleGraph->GetResultNode() : nullptr;
	UEdGraphPin* ResultPin = ResultNode
		? ResultNode->FindPin(TEXT("bCanEnterTransition")) : nullptr;
	if (!RuleGraph || !ResultPin)
	{
		OutErrors.Add(TEXT("new Guard transition has no writable result graph"));
		return false;
	}

	FGraphNodeCreator<UK2Node_VariableGet> VariableCreator(*RuleGraph);
	UK2Node_VariableGet* Variable = VariableCreator.CreateNode(false);
	Variable->VariableReference.SetSelfMember(
		GET_MEMBER_NAME_CHECKED(USamuraiAnimInstance, bIsBlocking));
	Variable->NodePosX = -300;
	Variable->NodePosY = 0;
	VariableCreator.Finalize();
	UEdGraphPin* ValuePin = Variable->GetValuePin();
	if (!ValuePin)
	{
		OutErrors.Add(TEXT("bIsBlocking variable node has no value pin"));
		return false;
	}

	UEdGraphPin* OutputPin = ValuePin;
	if (bNegated)
	{
		FGraphNodeCreator<UK2Node_CallFunction> NotCreator(*RuleGraph);
		UK2Node_CallFunction* NotNode = NotCreator.CreateNode(false);
		NotNode->FunctionReference.SetExternalMember(
			GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool),
			UKismetMathLibrary::StaticClass());
		NotNode->NodePosX = -120;
		NotNode->NodePosY = 0;
		NotCreator.Finalize();
		UEdGraphPin* InputPin = NotNode->FindPin(TEXT("A"));
		OutputPin = NotNode->GetReturnValuePin();
		if (!InputPin || !OutputPin
			|| !RuleGraph->GetSchema()->TryCreateConnection(ValuePin, InputPin))
		{
			OutErrors.Add(TEXT("could not connect the Guard exit NOT rule"));
			return false;
		}
	}
	if (!RuleGraph->GetSchema()->TryCreateConnection(OutputPin, ResultPin))
	{
		OutErrors.Add(TEXT("could not connect the Guard transition result"));
		return false;
	}
	return true;
}

UAnimStateTransitionNode* CreateTransition(
	UAnimationStateMachineGraph* StateMachine,
	UAnimStateNode* Previous,
	UAnimStateNode* Next,
	const bool bNegated,
	TArray<FString>& OutErrors)
{
	const FVector2f Location =
		(FVector2f(Previous->NodePosX, Previous->NodePosY)
			+ FVector2f(Next->NodePosX, Next->NodePosY)) * 0.5f;
	UAnimStateTransitionNode* Transition =
		FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateTransitionNode>(
			StateMachine, NewObject<UAnimStateTransitionNode>(), Location, false);
	if (!Transition)
	{
		OutErrors.Add(TEXT("could not create Guard state transition"));
		return nullptr;
	}
	Transition->CreateConnections(Previous, Next);
	Transition->CrossfadeDuration = 0.10f;
	return ConnectTransitionRule(Transition, bNegated, OutErrors) ? Transition : nullptr;
}

bool CreateGuardState(UAnimBlueprint* AnimBlueprint, TArray<FString>& OutErrors)
{
	FGuardGraphNodes Nodes = FindGuardGraphNodes(AnimBlueprint);
	UAnimSequenceBase* GuardSequence = LoadObjectAtPath<UAnimSequenceBase>(GuardSequencePath);
	if (!Nodes.StateMachine || !Nodes.Idle || !Nodes.Locomotion || Nodes.Guard)
	{
		OutErrors.Add(TEXT("Guard authoring requires one Idle/Locomotion state machine and no existing Guard state"));
		return false;
	}
	if (!GuardSequence || !GuardSequence->GetSkeleton()
		|| !AnimBlueprint->TargetSkeleton
		|| !AnimBlueprint->TargetSkeleton->IsCompatibleForEditor(GuardSequence->GetSkeleton()))
	{
		OutErrors.Add(TEXT("Guard sequence is missing or incompatible with the AnimBP skeleton"));
		return false;
	}

	AnimBlueprint->Modify();
	Nodes.StateMachine->Modify();
	const FVector2f GuardLocation(
		Nodes.Locomotion->NodePosX + 350.0f,
		Nodes.Locomotion->NodePosY + 250.0f);
	Nodes.Guard = FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateNode>(
		Nodes.StateMachine, NewObject<UAnimStateNode>(), GuardLocation, false);
	if (!Nodes.Guard || !Nodes.Guard->BoundGraph)
	{
		OutErrors.Add(TEXT("could not create the Guard state graph"));
		return false;
	}
	FEdGraphUtilities::RenameGraphToNameOrCloseToName(Nodes.Guard->BoundGraph, TEXT("Guard"));
	FEdGraphSchemaAction_K2NewNode PlayerAction;
	UAnimGraphNode_SequencePlayer* PlayerTemplate =
		NewObject<UAnimGraphNode_SequencePlayer>(GetTransientPackage());
	PlayerTemplate->Node.SetSequence(GuardSequence);
	PlayerTemplate->Node.SetLoopAnimation(true);
	PlayerAction.NodeTemplate = PlayerTemplate;
	if (!PlayerAction.PerformAction(
		Nodes.Guard->BoundGraph, Nodes.Guard->GetPoseSinkPinInsideState(),
		FVector2f(-300.0f, 0.0f), false))
	{
		OutErrors.Add(TEXT("could not create the Guard sequence player"));
		return false;
	}
	if (!CreateTransition(Nodes.StateMachine, Nodes.Idle, Nodes.Guard, false, OutErrors)
		|| !CreateTransition(Nodes.StateMachine, Nodes.Locomotion, Nodes.Guard, false, OutErrors)
		|| !CreateTransition(Nodes.StateMachine, Nodes.Guard, Nodes.Locomotion, true, OutErrors))
	{
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
	if (AnimBlueprint->Status == BS_Error || !GuardStateMatches(FindGuardGraphNodes(AnimBlueprint)))
	{
		OutErrors.Add(TEXT("Guard AnimBP failed compilation or post-authoring validation"));
		return false;
	}
	AnimBlueprint->MarkPackageDirty();
	return true;
}

void AddPlannedPackage(
	FDefenseAuthoringPlan& Plan,
	const FString& Change,
	const FString& PackageName,
	const FString& PackageRole,
	const FString& PlannedAction,
	const UObject* ExistingObject)
{
	Plan.ProposedChanges.Add(Change);
	FKatanaAssetMigrationPackageLedgerEntry Entry;
	Entry.PackageName = PackageName;
	Entry.PackageRole = PackageRole;
	const UPackage* ExistingPackage = FindPackage(nullptr, *PackageName);
	Entry.bInitiallyDirty = (ExistingObject && ExistingObject->GetOutermost()
			&& ExistingObject->GetOutermost()->IsDirty())
		|| (ExistingPackage && ExistingPackage->IsDirty());
	Entry.PlannedAction = PlannedAction;
	Plan.PackageLedger.Add(MoveTemp(Entry));
}

FString BuildRecipeFingerprintFacts()
{
	FString Facts = FString::Printf(
		TEXT("guard=%s|%s\nconfig=%s|audio=%s|vfx=%s|cone=70|turn=70|rate=180|block_tolerance=35|parry_tolerance=10|block_translation=0|drift=1|parry_translation=75\n"),
		*GuardAnimBlueprintPath, *GuardSequencePath, *DefenseConfigurationPackage,
		*ImpactAudioPath, *ImpactVFXPath);
	Facts += TEXT("bones=head:High,spine_03:Middle,pelvis:Low\n");
	Facts += TEXT("defender_rows=NormalBlockGeneric:NormalBlock:Block,PerfectParryGeneric:PerfectParry:Parry:CounterReady\n");
	Facts += TEXT("attacker_rows=ContinueGeneric:Continue:Empty,ParryStaggerGeneric:ParryStagger:Stagger\n");
	for (const FDefenseAuthoringMontageSpec& Spec : BuildMontageSpecs())
	{
		Facts += FString::Printf(
			TEXT("montage=%s|warp=%s|%.6f|%.6f|%d|collision=%d|sync=%d|reaction=%d|sync_time=%.6f|damage=%d|chain=%d|transition=%d|marker=%s|chain_time=%.6f\n"),
			*Spec.PackageName, *Spec.WarpTarget.ToString(), Spec.WarpStart, Spec.WarpEnd,
			Spec.bWarpTranslation, Spec.bAddCollision, Spec.bAddSync,
			static_cast<int32>(Spec.SyncReaction), Spec.SyncTime, Spec.bSyncAppliesDamage,
			Spec.bAddChainMarker, static_cast<int32>(Spec.ChainTransition),
			*Spec.ChainMarker.ToString(), Spec.ChainTime);
		for (const FDefenseAuthoringSegmentSpec& Segment : Spec.Segments)
		{
			Facts += FString::Printf(TEXT("segment=%s|%.6f|%.6f|%.6f\n"),
				*Segment.SourcePath, Segment.SourceStart, Segment.SourceEnd, Segment.PlayRate);
		}
		for (const FDefenseAuthoringSectionSpec& Section : Spec.Sections)
		{
			Facts += FString::Printf(TEXT("section=%s|%.6f\n"),
				*Section.Name.ToString(), Section.Time);
		}
	}
	for (const FDefenseAuthoringPairedSpec& Spec : BuildPairedSpecs())
	{
		Facts += FString::Printf(
			TEXT("paired=%s|reaction=%d|attacker=%s|%s|victim=%s|%s|required=%s|ready=%s|%s|terminal=%d|%d|auto=%d|sync=%.6f|damage=%.6f|lethal=%d\n"),
			*Spec.PackageName, static_cast<int32>(Spec.Reaction),
			*Spec.AttackerMontagePackage, *Spec.AttackerSection.ToString(),
			*Spec.VictimMontagePackage, *Spec.VictimSection.ToString(),
			*Spec.RequiredMarker.ToString(), *Spec.AttackerReadySection.ToString(),
			*Spec.VictimReadySection.ToString(), Spec.bAttackerTerminalCompatible,
			Spec.bVictimTerminalCompatible, Spec.bAutoContinue, Spec.SyncPointTime,
			Spec.BaseDamage, Spec.bLethal);
	}
	return Facts;
}

FString ComputeFingerprint(const FDefenseAuthoringPlan& Plan)
{
	FString Input = FString::Printf(TEXT("operation=%s\nrecipe_version=%d\n"),
		*FDefenseProofAuthoringOperation::OperationName, RecipeVersion);
	Input += BuildRecipeFingerprintFacts();
	for (const FString& Change : Plan.ProposedChanges)
	{
		Input += FString::Printf(TEXT("change=%s\n"), *Change);
	}
	for (const FKatanaAssetMigrationPackageLedgerEntry& Entry : Plan.PackageLedger)
	{
		Input += FString::Printf(
			TEXT("package=%s\nrole=%s\ndirty=%s\naction=%s\n"),
			*Entry.PackageName, *Entry.PackageRole,
			*LexToString(Entry.bInitiallyDirty), *Entry.PlannedAction);
	}
	FTCHARToUTF8 Utf8(*Input);
	return FSHA1::HashBuffer(Utf8.Get(), static_cast<uint64>(Utf8.Length())).ToString();
}

FDefenseAuthoringPlan BuildPlan()
{
	FDefenseAuthoringPlan Plan;
	UAnimBlueprint* AnimBlueprint = LoadObjectAtPath<UAnimBlueprint>(GuardAnimBlueprintPath);
	UAnimSequenceBase* GuardSequence = LoadObjectAtPath<UAnimSequenceBase>(GuardSequencePath);
	if (!AnimBlueprint
		|| !AnimBlueprint->GeneratedClass
		|| !AnimBlueprint->GeneratedClass->IsChildOf(USamuraiAnimInstance::StaticClass()))
	{
		Plan.Errors.Add(FString::Printf(TEXT("Guard AnimBP did not load as USamuraiAnimInstance: %s"),
			*GuardAnimBlueprintPath));
	}
	if (!GuardSequence)
	{
		Plan.Errors.Add(FString::Printf(TEXT("Guard sequence did not load: %s"), *GuardSequencePath));
	}
	else if (AnimBlueprint
		&& (!GuardSequence->GetSkeleton()
			|| !AnimBlueprint->TargetSkeleton
			|| !AnimBlueprint->TargetSkeleton->IsCompatibleForEditor(GuardSequence->GetSkeleton())))
	{
		Plan.Errors.Add(TEXT("Guard sequence is incompatible with the reviewed AnimBP skeleton"));
	}
	if (!LoadObjectAtPath<USoundBase>(ImpactAudioPath))
	{
		Plan.Errors.Add(FString::Printf(TEXT("reviewed impact audio did not load: %s"), *ImpactAudioPath));
	}
	if (!LoadObjectAtPath<UNiagaraSystem>(ImpactVFXPath))
	{
		Plan.Errors.Add(FString::Printf(TEXT("reviewed impact VFX did not load: %s"), *ImpactVFXPath));
	}

	for (const FDefenseAuthoringMontageSpec& Spec : BuildMontageSpecs())
	{
		ValidateMontageSources(Spec, Plan.Errors);
		UObject* Existing = FindExistingAsset(BuildObjectPath(Spec.PackageName));
		if (!Existing)
		{
			AddPlannedPackage(Plan,
				FString::Printf(TEXT("CreateMontage|%s"), *Spec.PackageName),
				Spec.PackageName, TEXT("DefenseMontage"), TEXT("Create"), nullptr);
		}
		else if (const UAnimMontage* Montage = Cast<UAnimMontage>(Existing))
		{
			if (!MontageMatches(Montage, Spec))
			{
				Plan.Errors.Add(FString::Printf(
					TEXT("existing destination montage differs from the reviewed recipe: %s"),
					*Spec.PackageName));
			}
		}
		else
		{
			Plan.Errors.Add(FString::Printf(
				TEXT("existing destination is not an AnimMontage: %s"), *Spec.PackageName));
		}
	}

	for (const FDefenseAuthoringPairedSpec& Spec : BuildPairedSpecs())
	{
		UObject* Existing = FindExistingAsset(BuildObjectPath(Spec.PackageName));
		if (!Existing)
		{
			AddPlannedPackage(Plan,
				FString::Printf(TEXT("CreatePairedData|%s"), *Spec.PackageName),
				Spec.PackageName, TEXT("PairedData"), TEXT("Create"), nullptr);
		}
		else if (const UPairedAnimationData* Data = Cast<UPairedAnimationData>(Existing))
		{
			if (!PairedDataMatches(Data, Spec))
			{
				Plan.Errors.Add(FString::Printf(
					TEXT("existing paired data differs from the reviewed recipe: %s"),
					*Spec.PackageName));
			}
		}
		else
		{
			Plan.Errors.Add(FString::Printf(
				TEXT("existing destination is not PairedAnimationData: %s"), *Spec.PackageName));
		}
	}

	UObject* ExistingConfiguration = FindExistingAsset(BuildObjectPath(DefenseConfigurationPackage));
	if (!ExistingConfiguration)
	{
		AddPlannedPackage(Plan,
			FString::Printf(TEXT("CreateDefenseConfiguration|%s"), *DefenseConfigurationPackage),
			DefenseConfigurationPackage, TEXT("DefenseConfiguration"), TEXT("Create"), nullptr);
	}
	else if (const UDefenseConfiguration* Configuration =
		Cast<UDefenseConfiguration>(ExistingConfiguration))
	{
		if (!DefenseConfigurationMatches(Configuration))
		{
			Plan.Errors.Add(FString::Printf(
				TEXT("existing defense configuration differs from the reviewed recipe: %s"),
				*DefenseConfigurationPackage));
		}
	}
	else
	{
		Plan.Errors.Add(FString::Printf(
			TEXT("existing destination is not DefenseConfiguration: %s"),
			*DefenseConfigurationPackage));
	}

	if (AnimBlueprint)
	{
		const FGuardGraphNodes Nodes = FindGuardGraphNodes(AnimBlueprint);
		if (!Nodes.StateMachine || !Nodes.Idle || !Nodes.Locomotion)
		{
			Plan.Errors.Add(TEXT("Guard AnimBP has no state machine containing Idle and Locomotion"));
		}
		else if (!Nodes.Guard)
		{
			AddPlannedPackage(Plan, TEXT("AddGuardState|ABP_SamuraiCharacter|bIsBlocking"),
				AnimBlueprint->GetOutermost()->GetName(), TEXT("GuardAnimBlueprint"),
				TEXT("Modify"), AnimBlueprint);
		}
		else if (!GuardStateMatches(Nodes))
		{
			Plan.Errors.Add(TEXT("existing Guard/Block state does not match the reviewed Guard graph"));
		}
	}

	Plan.ProposedChanges.Sort();
	Plan.PackageLedger.Sort([](
		const FKatanaAssetMigrationPackageLedgerEntry& Left,
		const FKatanaAssetMigrationPackageLedgerEntry& Right)
	{
		return Left.PackageName < Right.PackageName;
	});
	Plan.Errors.Sort();
	Plan.Fingerprint = ComputeFingerprint(Plan);
	return Plan;
}

bool ValidateApprovedPlanBinding(
	const FKatanaAssetMigrationOptions& Options,
	const FDefenseAuthoringPlan& Plan,
	TArray<FString>& OutErrors)
{
	if (Options.ApprovedPlanReport.IsEmpty() || Options.ApprovedPlanFingerprint.IsEmpty())
	{
		OutErrors.Add(TEXT("approved authoring Plan report and fingerprint are required"));
		return false;
	}
	if (Options.ApprovedPlanFingerprint != Plan.Fingerprint)
	{
		OutErrors.Add(TEXT("approved authoring fingerprint differs from the current plan"));
	}
	FString Json;
	const FString ReportPath = FKatanaAssetMigrationRunner::ResolveProjectRelativeFilePath(
		Options.ApprovedPlanReport);
	if (!FFileHelper::LoadFileToString(Json, *ReportPath))
	{
		OutErrors.Add(FString::Printf(TEXT("could not read approved authoring Plan: %s"), *ReportPath));
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutErrors.Add(TEXT("approved authoring Plan is not valid JSON"));
		return false;
	}
	FString Operation;
	FString Mode;
	FString Fingerprint;
	double SchemaVersion = 0.0;
	if (!Root->TryGetNumberField(TEXT("schema_version"), SchemaVersion)
		|| SchemaVersion != 2.0
		|| !Root->TryGetStringField(TEXT("operation"), Operation)
		|| Operation != FDefenseProofAuthoringOperation::OperationName
		|| !Root->TryGetStringField(TEXT("mode"), Mode)
		|| Mode != TEXT("Plan")
		|| !Root->TryGetStringField(TEXT("plan_fingerprint"), Fingerprint)
		|| Fingerprint != Plan.Fingerprint
		|| Fingerprint != Options.ApprovedPlanFingerprint)
	{
		OutErrors.Add(TEXT("approved report must be the matching schema-v2 DefenseProofAuthoring Plan"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* LedgerValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("package_ledger"), LedgerValues) || !LedgerValues
		|| LedgerValues->Num() != Plan.PackageLedger.Num())
	{
		OutErrors.Add(TEXT("approved authoring package ledger differs in cardinality"));
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
			OutErrors.Add(TEXT("approved authoring package ledger contains a malformed entry"));
			return false;
		}
		ApprovedLedger.Add(MoveTemp(Entry));
	}
	ApprovedLedger.Sort([](const auto& Left, const auto& Right)
	{
		return Left.PackageName < Right.PackageName;
	});
	for (int32 Index = 0; Index < ApprovedLedger.Num(); ++Index)
	{
		const FKatanaAssetMigrationPackageLedgerEntry& Approved = ApprovedLedger[Index];
		const FKatanaAssetMigrationPackageLedgerEntry& Current = Plan.PackageLedger[Index];
		if (Approved.PackageName != Current.PackageName
			|| Approved.PackageRole != Current.PackageRole
			|| Approved.bInitiallyDirty != Current.bInitiallyDirty
			|| Approved.PlannedAction != Current.PlannedAction)
		{
			OutErrors.Add(TEXT("approved authoring package ledger differs from the current plan"));
			break;
		}
	}
	return OutErrors.IsEmpty();
}

void PopulateReport(
	const FDefenseAuthoringPlan& Plan,
	const EKatanaAssetMigrationMode Mode,
	const TSet<FString>* ChangedPackages,
	FKatanaAssetMigrationReport& OutReport)
{
	OutReport = FKatanaAssetMigrationReport();
	OutReport.SchemaVersion = 2;
	OutReport.Operation = FDefenseProofAuthoringOperation::OperationName;
	OutReport.Mode = Mode;
	OutReport.Gate = TEXT("A");
	OutReport.PlanFingerprint = Plan.Fingerprint;
	OutReport.PackageLedger = Plan.PackageLedger;
	FKatanaAssetMigrationRow Row;
	Row.InputTarget = TEXT("GateAReviewedRecipeV2");
	Row.AssetClass = TEXT("DefenseProofAuthoringRecipe");
	Row.Details.Add(TEXT("recipe_version"), LexToString(RecipeVersion));
	Row.Details.Add(TEXT("destination_package_count"),
		LexToString(FDefenseProofAuthoringOperation::GetDestinationPackageNames().Num()));
	Row.PlannedAdditions = Plan.ProposedChanges;
	Row.Errors = Plan.Errors;
	if (!Plan.Errors.IsEmpty())
	{
		Row.Status = EKatanaAssetMigrationStatus::Failed;
	}
	else if (IsApplyMode(Mode))
	{
		Row.Status = ChangedPackages && !ChangedPackages->IsEmpty()
			? EKatanaAssetMigrationStatus::Changed
			: EKatanaAssetMigrationStatus::Unchanged;
	}
	else
	{
		Row.Status = Plan.ProposedChanges.IsEmpty()
			? EKatanaAssetMigrationStatus::Unchanged
			: EKatanaAssetMigrationStatus::WouldChange;
	}
	if (ChangedPackages)
	{
		Row.ChangedPackages = ChangedPackages->Array();
		Row.ChangedPackages.Sort();
	}
	OutReport.Rows.Add(MoveTemp(Row));
	FKatanaAssetMigrationRunner::Summarize(OutReport);
}

bool ApplyPlan(
	const FDefenseAuthoringPlan& Plan,
	TSet<FString>& OutChangedPackages,
	TArray<FString>& OutErrors)
{
	for (const FDefenseAuthoringMontageSpec& Spec : BuildMontageSpecs())
	{
		if (!FindExistingAsset(BuildObjectPath(Spec.PackageName)))
		{
			if (CreateMontage(Spec, OutErrors))
			{
				OutChangedPackages.Add(Spec.PackageName);
			}
		}
	}
	if (!OutErrors.IsEmpty())
	{
		return false;
	}
	for (const FDefenseAuthoringPairedSpec& Spec : BuildPairedSpecs())
	{
		if (!FindExistingAsset(BuildObjectPath(Spec.PackageName)))
		{
			if (CreatePairedData(Spec, OutErrors))
			{
				OutChangedPackages.Add(Spec.PackageName);
			}
		}
	}
	if (!OutErrors.IsEmpty())
	{
		return false;
	}
	if (!FindExistingAsset(BuildObjectPath(DefenseConfigurationPackage)))
	{
		if (CreateDefenseConfiguration(OutErrors))
		{
			OutChangedPackages.Add(DefenseConfigurationPackage);
		}
	}
	if (!OutErrors.IsEmpty())
	{
		return false;
	}
	UAnimBlueprint* AnimBlueprint = LoadObjectAtPath<UAnimBlueprint>(GuardAnimBlueprintPath);
	if (AnimBlueprint && !FindGuardGraphNodes(AnimBlueprint).Guard)
	{
		if (CreateGuardState(AnimBlueprint, OutErrors))
		{
			OutChangedPackages.Add(AnimBlueprint->GetOutermost()->GetName());
		}
	}

	TSet<FString> PlannedPackages;
	for (const FKatanaAssetMigrationPackageLedgerEntry& Entry : Plan.PackageLedger)
	{
		PlannedPackages.Add(Entry.PackageName);
	}
	if (!OutChangedPackages.Difference(PlannedPackages).IsEmpty()
		|| !PlannedPackages.Difference(OutChangedPackages).IsEmpty())
	{
		OutErrors.Add(TEXT("actual authored packages differ from the approved package ledger"));
	}
	return OutErrors.IsEmpty();
}
}

TArray<FString> FDefenseProofAuthoringOperation::GetDestinationPackageNames()
{
	TArray<FString> Packages;
	for (const FDefenseAuthoringMontageSpec& Spec : BuildMontageSpecs())
	{
		Packages.Add(Spec.PackageName);
	}
	for (const FDefenseAuthoringPairedSpec& Spec : BuildPairedSpecs())
	{
		Packages.Add(Spec.PackageName);
	}
	Packages.Add(DefenseConfigurationPackage);
	Packages.Add(FPackageName::ObjectPathToPackageName(GuardAnimBlueprintPath));
	Packages.Sort();
	return Packages;
}

bool FDefenseProofAuthoringOperation::Run(
	const FKatanaAssetMigrationOptions& Options,
	FKatanaAssetMigrationReport& OutReport) const
{
	FDefenseAuthoringPlan Plan = BuildPlan();
	if (!Plan.Errors.IsEmpty() || !IsApplyMode(Options.Mode))
	{
		PopulateReport(Plan, Options.Mode, nullptr, OutReport);
		return Plan.Errors.IsEmpty();
	}

	TArray<FString> Errors;
	if (!ValidateApprovedPlanBinding(Options, Plan, Errors))
	{
		Plan.Errors.Append(Errors);
		PopulateReport(Plan, Options.Mode, nullptr, OutReport);
		return false;
	}
	if (!Options.bAllowDirtyPackages
		&& Plan.PackageLedger.ContainsByPredicate([](const auto& Entry)
		{
			return Entry.bInitiallyDirty;
		}))
	{
		Plan.Errors.Add(TEXT("approved authoring plan contains an initially dirty package"));
		PopulateReport(Plan, Options.Mode, nullptr, OutReport);
		return false;
	}

	TSet<FString> ChangedPackages;
	if (!ApplyPlan(Plan, ChangedPackages, Errors))
	{
		Plan.Errors.Append(Errors);
		PopulateReport(Plan, Options.Mode, &ChangedPackages, OutReport);
		return false;
	}
	const FDefenseAuthoringPlan PostApplyPlan = BuildPlan();
	if (!PostApplyPlan.Errors.IsEmpty() || !PostApplyPlan.ProposedChanges.IsEmpty())
	{
		Plan.Errors.Add(TEXT("post-apply authoring validation was not idempotent"));
		Plan.Errors.Append(PostApplyPlan.Errors);
		PopulateReport(Plan, Options.Mode, &ChangedPackages, OutReport);
		return false;
	}

	PopulateReport(Plan, Options.Mode, &ChangedPackages, OutReport);
	for (FKatanaAssetMigrationPackageLedgerEntry& Entry : OutReport.PackageLedger)
	{
		Entry.ActualAction = ChangedPackages.Contains(Entry.PackageName)
			? (Entry.PlannedAction == TEXT("Create") ? TEXT("Created") : TEXT("Modified"))
			: TEXT("Missing");
	}
	return true;
}
