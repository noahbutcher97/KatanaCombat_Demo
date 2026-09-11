#include "Analysis/PairedWarpTuning.h"
// Copyright Epic Games, Inc. All Rights Reserved.
#include "Subsystems/PairedAnimationAnalysisSubsystem.h"
#include "PairedAnimationAnalysisLibrary.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "Data/PairedAnimationData.h"
#include "Data/WeaponData.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PreviewScene.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

TArray<FPairedSyncEvent> UPairedAnimationAnalysisSubsystem::CollectMontageSyncEvents(const UAnimMontage* Montage, double Start, double End)
{
	TArray<FPairedSyncEvent> Events;
	if (!Montage || !FMath::IsFinite(Start) || !FMath::IsFinite(End) || End <= Start
		|| !FMath::IsFinite(Montage->RateScale) || Montage->RateScale <= 0) { return Events; }
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const auto* Sync = Cast<UAnimNotifyState_PairedAnimationSync>(Event.NotifyStateClass);
		// UAnimSequenceBase::GetAnimNotifiesFromDeltaPositions uses state overlap,
		// not just the begin timestamp. In particular, a start-offset state can
		// trigger before zero and still receive NotifyBegin on the first tick.
		if (!Sync || Event.GetTriggerTime() > End || Event.GetEndTriggerTime() <= Start) { continue; }
		auto& Row = Events.AddDefaulted_GetRef();
		Row.Name = Sync->SyncPointName; Row.VictimBone = Sync->VictimContactBone;
		Row.NominalMontageTime = Event.GetTime(); Row.TriggerMontageTime = Event.GetTriggerTime();
		Row.EndTriggerMontageTime = Event.GetEndTriggerTime();
		Row.bActiveAtEntry = Row.TriggerMontageTime < Start;
		Row.PairTime = (FMath::Max(Start, Row.TriggerMontageTime) - Start) / Montage->RateScale;
		Row.bPrimary = Sync->bIsPrimarySyncPoint; Row.bDamageConfigured = Sync->bApplyDamage;
	}
	return Events;
}

namespace PairedContactProfile
{
using FJson = TSharedPtr<FJsonObject>;
FJson Read(const FString& Path)
{
	FString Text; FJson Json;
	if (FFileHelper::LoadFileToString(Text, *Path)) { FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json); }
	return Json;
}
bool Number(const FJson& Json, const TCHAR* Key, double& Value)
{
	return Json && Json->TryGetNumberField(Key, Value) && FMath::IsFinite(Value);
}
bool Vector(const FJson& Json, const TCHAR* Key, FVector& Value)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Json || !Json->TryGetArrayField(Key, Values) || Values->Num() != 3) { return false; }
	double XYZ[3];
	for (int32 I = 0; I < 3; ++I) { if (!(*Values)[I]->TryGetNumber(XYZ[I]) || !FMath::IsFinite(XYZ[I])) { return false; } }
	Value = FVector(XYZ[0], XYZ[1], XYZ[2]); return true;
}
FString Escape(FString Text)
{
	return Text.Replace(TEXT("&"), TEXT("&amp;")).Replace(TEXT("<"), TEXT("&lt;")).Replace(TEXT(">"), TEXT("&gt;")).Replace(TEXT("\""), TEXT("&quot;"));
}
bool Write(const FString& Path, const FJson& Json)
{
	FString Text; FJsonSerializer::Serialize(Json.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
	return FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
FString Status(EPairedContactResult Value)
{
	return Value == EPairedContactResult::Pass ? TEXT("pass") : Value == EPairedContactResult::Fail ? TEXT("fail") : TEXT("inconclusive");
}
struct FProfile
{
	FJson Definition;
	TStrongObjectPtr<UPairedAnimationData> Pair;
	FPairedAnimationPreviewModel Model;
	TArray<FPairedContactRule> Rules;
	TArray<FPairedAlignmentRule> AlignmentRules;
	double Hz = 60;
	FString Basis;
	TArray<TSharedPtr<FJsonValue>> SyncEvents;
};
bool Section(UAnimMontage* Montage, FName Name, float& Start, float& End)
{
	if (!Montage || !FMath::IsFinite(Montage->RateScale) || Montage->RateScale <= 0) { return false; }
	Start = 0; End = Montage->GetPlayLength();
	if (!Name.IsNone())
	{
		const int32 Index = Montage->GetSectionIndex(Name); if (Index == INDEX_NONE) { return false; }
		Montage->GetSectionStartAndEndTime(Index, Start, End);
	}
	// Each slot must have one continuous animation track. Branching/looping
	// section traversal is a separate runtime contract and is not inferred here.
	return End > Start && Montage->SlotAnimTracks.Num() == 1;
}
bool Load(const FString& Path, FProfile& Profile, FString& Error)
{
	Profile.Definition = Read(Path); const FJson& Json = Profile.Definition;
	double Version = 0; FString PairPath, AttackerPath, VictimPath;
	if (!Number(Json, TEXT("schema_version"), Version) || Version != 1
		|| !Json->TryGetStringField(TEXT("paired_asset"), PairPath)
		|| !Json->TryGetStringField(TEXT("attacker_mesh"), AttackerPath) || !Json->TryGetStringField(TEXT("victim_mesh"), VictimPath)
		|| !Json->TryGetStringField(TEXT("criteria_basis"), Profile.Basis) || Profile.Basis.IsEmpty()
		|| !Number(Json, TEXT("sample_hz"), Profile.Hz) || Profile.Hz < 10 || Profile.Hz > 240)
	{
		Error = TEXT("Profile requires schema 1, paired asset, meshes, criteria basis and sample rate 10..240"); return false;
	}
	Profile.Pair.Reset(LoadObject<UPairedAnimationData>(nullptr, *PairPath));
	UPairedAnimationData* Pair = Profile.Pair.Get();
	if (!Pair) { Error = TEXT("Paired asset could not be loaded"); return false; }
	auto& Model = Profile.Model;
	Model.AttackerSkeleton = LoadObject<USkeletalMesh>(nullptr, *AttackerPath);
	Model.VictimSkeleton = LoadObject<USkeletalMesh>(nullptr, *VictimPath);
	Model.AttackerMontage = Pair->AttackerMontage; Model.VictimMontage = Pair->VictimMontage;
	Model.AttackerMontageSection = Pair->AttackerMontageSection; Model.VictimMontageSection = Pair->VictimMontageSection;
	if (!Model.AttackerSkeleton.IsValid() || !Model.VictimSkeleton.IsValid()
		|| !Section(Pair->AttackerMontage, Pair->AttackerMontageSection, Model.AttackerSectionStart, Model.AttackerSectionEnd)
		|| !Section(Pair->VictimMontage, Pair->VictimMontageSection, Model.VictimSectionStart, Model.VictimSectionEnd))
	{
		Error = TEXT("Missing mesh, invalid montage/section/rate, or unsupported multiple slots"); return false;
	}
	if (!Pair->AttackerMontage->GetSkeleton() || !Pair->VictimMontage->GetSkeleton()
		|| !Pair->AttackerMontage->GetSkeleton()->IsCompatibleForEditor(Model.GetAttackerSkeleton()->GetSkeleton())
		|| !Pair->VictimMontage->GetSkeleton()->IsCompatibleForEditor(Model.GetVictimSkeleton()->GetSkeleton()))
	{
		Error = TEXT("Montage and selected mesh skeletons are incompatible"); return false;
	}
	if (!FMath::IsFinite(Pair->VictimStartOffset)) { Error = TEXT("Non-finite victim start offset"); return false; }
	// Current runtime starts the victim at max(0,-offset), then a named section
	// jump supersedes that position. It does not delay the victim's world start.
	if (Pair->VictimMontageSection.IsNone()) { Model.VictimSectionStart = FMath::Max(0.0f, -Pair->VictimStartOffset); }
	Model.VictimTimeOffset = 0;
	Model.MaxDuration = FMath::Min((Model.AttackerSectionEnd - Model.AttackerSectionStart) / Pair->AttackerMontage->RateScale,
		(Model.VictimSectionEnd - Model.VictimSectionStart) / Pair->VictimMontage->RateScale);
	FVector ARotation, VRotation;
	if (Model.MaxDuration <= 0 || Model.MaxDuration > 30
		|| !Vector(Json, TEXT("attacker_mesh_position_cm"), Model.AttackerConfig.PositionOffset)
		|| !Vector(Json, TEXT("victim_mesh_position_cm"), Model.VictimConfig.PositionOffset)
		|| !Vector(Json, TEXT("attacker_mesh_rotation_deg"), ARotation) || !Vector(Json, TEXT("victim_mesh_rotation_deg"), VRotation))
	{
		Error = TEXT("Invalid duration or mesh transforms; rotation arrays are pitch,yaw,roll"); return false;
	}
	Model.AttackerConfig.RotationOffset = FRotator(ARotation.X, ARotation.Y, ARotation.Z);
	Model.VictimConfig.RotationOffset = FRotator(VRotation.X, VRotation.Y, VRotation.Z);
	Model.bLoopPlayback = false; Model.bIsPlaying = false; Model.CurrentTime = 0;
	for (int32 Role = 0; Role < 2; ++Role)
	{
		FString WeaponPath;
		if (Json->TryGetStringField(Role == 0 ? TEXT("attacker_weapon_data") : TEXT("victim_weapon_data"), WeaponPath))
		{
			UWeaponData* Weapon = LoadObject<UWeaponData>(nullptr, *WeaponPath);
			if (!Weapon || !Weapon->WeaponMesh.LoadSynchronous()) { Error = TEXT("Configured weapon data or mesh is unavailable"); return false; }
			auto& Config = Role == 0 ? Model.AttackerWeaponConfig : Model.VictimWeaponConfig;
			Config.SetWeaponMesh(Weapon->WeaponMesh.Get()); Config.AttachmentSocket = Weapon->EquippedSocket;
			Config.AttachmentOffset = Weapon->MeshAttachOffset;
			Config.WeaponBaseSocket = Weapon->TraceStartSocket; Config.WeaponTipSocket = Weapon->TraceEndSocket;
			Config.bUseForContactDetection = !Weapon->bUseCharacterSocketsForTrace;
		}
	}
	for (int32 Role = 0; Role < 2; ++Role)
	{
		UAnimMontage* Montage = Role == 0 ? Pair->AttackerMontage.Get() : Pair->VictimMontage.Get();
		const float Start = Role == 0 ? Model.AttackerSectionStart : Model.VictimSectionStart;
		const float End = Role == 0 ? Model.AttackerSectionEnd : Model.VictimSectionEnd;
		for (const FPairedSyncEvent& Event : UPairedAnimationAnalysisSubsystem::CollectMontageSyncEvents(Montage, Start, End))
		{
			auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("role"), Role == 0 ? TEXT("Attacker") : TEXT("Victim"));
			Row->SetStringField(TEXT("name"), Event.Name.ToString()); Row->SetStringField(TEXT("victim_bone"), Event.VictimBone.ToString());
			Row->SetNumberField(TEXT("montage_time_s"), Event.TriggerMontageTime);
			Row->SetNumberField(TEXT("nominal_montage_time_s"), Event.NominalMontageTime);
			Row->SetNumberField(TEXT("trigger_offset_s"), Event.TriggerMontageTime - Event.NominalMontageTime);
			Row->SetNumberField(TEXT("end_trigger_montage_time_s"), Event.EndTriggerMontageTime);
			Row->SetNumberField(TEXT("pair_time_s"), Event.PairTime); Row->SetBoolField(TEXT("active_at_entry"), Event.bActiveAtEntry);
			Row->SetBoolField(TEXT("is_primary"), Event.bPrimary);
			// Retain the schema-1 field as a configured flag for existing readers.
			Row->SetBoolField(TEXT("applies_damage"), Event.bDamageConfigured);
			Row->SetBoolField(TEXT("damage_configured"), Event.bDamageConfigured);
			Row->SetBoolField(TEXT("requests_damage_on_owner"), Event.bPrimary && Event.bDamageConfigured);
			Row->SetStringField(TEXT("runtime_damage_commit"), TEXT("not_measured"));
			Profile.SyncEvents.Add(MakeShared<FJsonValueObject>(Row));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Contacts = nullptr;
	if (!Json->TryGetArrayField(TEXT("contacts"), Contacts) || Contacts->IsEmpty() || Contacts->Num() > 32) { Error = TEXT("Profile requires 1..32 explicit contacts"); return false; }
	TSet<FString> Names;
	for (const auto& Value : *Contacts)
	{
		const FJson RuleJson = Value->Type == EJson::Object ? Value->AsObject() : nullptr;
		FPairedContactRule Rule;
		if (!RuleJson || !RuleJson->TryGetStringField(TEXT("name"), Rule.Name) || Rule.Name.IsEmpty() || Names.Contains(Rule.Name)
			|| !RuleJson->TryGetStringField(TEXT("source"), Rule.SourcePoint) || !RuleJson->TryGetStringField(TEXT("target"), Rule.TargetPoint)
			|| !Number(RuleJson, TEXT("start_s"), Rule.StartSeconds) || !Number(RuleJson, TEXT("end_s"), Rule.EndSeconds)
			|| !Number(RuleJson, TEXT("target_radius_cm"), Rule.TargetRadiusCm)
			|| !Number(RuleJson, TEXT("minimum_gap_cm"), Rule.MinimumGapCm) || !Number(RuleJson, TEXT("maximum_gap_cm"), Rule.MaximumGapCm)
			|| !Number(RuleJson, TEXT("maximum_sample_gap_s"), Rule.MaximumSampleGapSeconds)
			|| !RuleJson->TryGetBoolField(TEXT("sustained"), Rule.bSustained))
		{
			Error = TEXT("Malformed/duplicate contact: explicit geometry, interval, sampling limit and sustained flag required"); return false;
		}
		Names.Add(Rule.Name); RuleJson->TryGetStringField(TEXT("source_end"), Rule.SourceEndPoint);
		RuleJson->TryGetBoolField(TEXT("measure_orientation"), Rule.bMeasureOrientation);
		if (Rule.bMeasureOrientation && (!Vector(RuleJson, TEXT("target_local_normal"), Rule.TargetLocalNormal)
			|| !Number(RuleJson, TEXT("expected_angle_deg"), Rule.ExpectedAngleDegrees) || !Number(RuleJson, TEXT("angle_tolerance_deg"), Rule.AngleToleranceDegrees)))
		{
			Error = TEXT("Orientation criterion lacks normal, expected angle or tolerance"); return false;
		}
		FString Anchor;
		if (RuleJson->TryGetStringField(TEXT("sync_name"), Anchor))
		{
			FString AnchorRole;
			if (!RuleJson->TryGetStringField(TEXT("sync_role"), AnchorRole) || (AnchorRole != TEXT("Attacker") && AnchorRole != TEXT("Victim"))) { Error = TEXT("A sync anchor requires explicit sync_role Attacker or Victim"); return false; }
			TArray<double> Matches;
			for (const auto& Sync : Profile.SyncEvents) { const auto Event = Sync->AsObject(); if (Event->GetStringField(TEXT("role")) == AnchorRole && Event->GetStringField(TEXT("name")) == Anchor) { Matches.Add(Event->GetNumberField(TEXT("pair_time_s"))); } }
			if (Matches.Num() != 1) { Error = TEXT("Contact sync anchor requires exactly one matching montage notify on its declared role"); return false; }
			Rule.StartSeconds += Matches[0]; Rule.EndSeconds += Matches[0];
		}
		if (Rule.StartSeconds < 0 || Rule.EndSeconds > Model.MaxDuration) { Error = TEXT("Contact interval lies outside shared non-looping montage playback"); return false; }
		Profile.Rules.Add(Rule);
	}
	const TArray<TSharedPtr<FJsonValue>>* Alignments = nullptr;
	if (Json->TryGetArrayField(TEXT("alignment_budgets"), Alignments))
	{
		if (Alignments->Num() > 32) { Error = TEXT("At most 32 alignment intervals are supported"); return false; }
		for (const auto& Value : *Alignments)
		{
			const FJson Item = Value->Type == EJson::Object ? Value->AsObject() : nullptr;
			FPairedAlignmentRule Rule;
			if (!Item || !Item->TryGetStringField(TEXT("name"), Rule.Name) || Rule.Name.IsEmpty() || Names.Contains(Rule.Name)
				|| !Number(Item, TEXT("start_s"), Rule.StartSeconds) || !Number(Item, TEXT("end_s"), Rule.EndSeconds)
				|| Rule.StartSeconds < 0 || Rule.EndSeconds < Rule.StartSeconds || Rule.EndSeconds > Model.MaxDuration
				|| !Number(Item, TEXT("maximum_sample_gap_s"), Rule.MaximumSampleGapSeconds) || Rule.MaximumSampleGapSeconds <= 0
				|| !Number(Item, TEXT("maximum_translation_cm"), Rule.MaximumTranslationCm) || Rule.MaximumTranslationCm < 0
				|| !Number(Item, TEXT("maximum_rotation_deg"), Rule.MaximumRotationDegrees) || Rule.MaximumRotationDegrees < 0
				|| !Number(Item, TEXT("maximum_victim_timing_error_s"), Rule.MaximumVictimTimingErrorSeconds) || Rule.MaximumVictimTimingErrorSeconds < 0)
			{
				Error = TEXT("Malformed alignment budget; explicit interval, translation, rotation, victim timing and sampling limits are required"); return false;
			}
			Names.Add(Rule.Name); Profile.AlignmentRules.Add(Rule);
		}
	}
	return true;
}

TSet<FString> PointKeys(const FProfile& Profile)
{
	TSet<FString> Keys;
	if (!Profile.AlignmentRules.IsEmpty()) { Keys.Add(TEXT("Attacker:root")); Keys.Add(TEXT("Victim:root")); }
	for (const auto& Rule : Profile.Rules) { Keys.Add(Rule.SourcePoint); Keys.Add(Rule.TargetPoint); if (!Rule.SourceEndPoint.IsEmpty()) { Keys.Add(Rule.SourceEndPoint); } }
	return Keys;
}

bool AcquireAuthored(const FProfile& Profile, TArray<FPairedContactPose>& Samples, FString& Error)
{
	FPreviewScene Scene{FPreviewScene::ConstructionValues()};
	UDebugSkelMeshComponent* Meshes[] = {NewObject<UDebugSkelMeshComponent>(), NewObject<UDebugSkelMeshComponent>()};
	for (auto* Mesh : Meshes) { Scene.AddComponent(Mesh, FTransform::Identity); }
	UStaticMeshComponent* Weapons[] = {nullptr, nullptr};
	const FWeaponMeshConfig WeaponConfigs[] = {Profile.Model.AttackerWeaponConfig, Profile.Model.VictimWeaponConfig};
	for (int32 Role = 0; Role < 2; ++Role)
	{
		if (!WeaponConfigs[Role].UseForContactDetection()) { continue; }
		Weapons[Role] = NewObject<UStaticMeshComponent>(); Weapons[Role]->SetStaticMesh(WeaponConfigs[Role].WeaponMesh.Get());
		Scene.AddComponent(Weapons[Role], FTransform::Identity);
	}
	const TSet<FString> Keys = PointKeys(Profile);
	const int32 Steps = FMath::CeilToInt(Profile.Model.MaxDuration * Profile.Hz);
	for (int32 I = 0; I <= Steps; ++I)
	{
		FPairedContactPose Pose; Pose.TimeSeconds = FMath::Min(I / Profile.Hz, double(Profile.Model.MaxDuration)); Pose.OriginalTimeSeconds = Pose.TimeSeconds;
		if (!UPairedAnimationAnalysisSubsystem::SampleContactPreviewPose(Profile.Model, Pose.TimeSeconds, Meshes[0], Meshes[1], Error)) { return false; }
		for (int32 Role = 0; Role < 2; ++Role)
		{
			if (!Weapons[Role]) { continue; }
			if (!Meshes[Role]->DoesSocketExist(WeaponConfigs[Role].AttachmentSocket)) { Error = TEXT("Required weapon attachment socket is missing"); return false; }
			Weapons[Role]->AttachToComponent(Meshes[Role], FAttachmentTransformRules::KeepRelativeTransform, WeaponConfigs[Role].AttachmentSocket);
			Weapons[Role]->SetRelativeTransform(WeaponConfigs[Role].AttachmentOffset);
		}
		for (const FString& Key : Keys)
		{
			FString Role, Point;
			if (!Key.Split(TEXT(":"), &Role, &Point) || (Role != TEXT("Attacker") && Role != TEXT("Victim"))) { Error = TEXT("Contact points use Attacker:bone/socket or Victim:bone/socket"); return false; }
			const int32 RoleIndex = Role == TEXT("Attacker") ? 0 : 1;
			USceneComponent* Source = Meshes[RoleIndex];
			const FName Socket(*Point);
			if (WeaponConfigs[RoleIndex].UseForContactDetection() && (Socket == WeaponConfigs[RoleIndex].WeaponBaseSocket || Socket == WeaponConfigs[RoleIndex].WeaponTipSocket)) { Source = Weapons[RoleIndex]; }
			if (!Source || !Source->DoesSocketExist(Socket)) { Error = TEXT("Required preview bone/socket is missing: ") + Key; return false; }
			Pose.Points.Add(Key, Source->GetSocketTransform(Socket));
		}
		Samples.Add(MoveTemp(Pose));
	}
	return true;
}
}

bool UPairedAnimationAnalysisSubsystem::SampleContactPreviewPose(const FPairedAnimationPreviewModel& Model, double Time,
	UDebugSkelMeshComponent* AttackerMesh, UDebugSkelMeshComponent* VictimMesh, FString& OutError)
{
	if (!FMath::IsFinite(Time) || Time < 0 || Time > Model.MaxDuration + UE_KINDA_SMALL_NUMBER) { OutError = TEXT("Preview time outside shared montage interval"); return false; }
	UDebugSkelMeshComponent* Meshes[] = {AttackerMesh, VictimMesh};
	UAnimMontage* Montages[] = {Model.GetAttackerMontage(), Model.GetVictimMontage()};
	USkeletalMesh* Assets[] = {Model.GetAttackerSkeleton(), Model.GetVictimSkeleton()};
	const FCharacterPreviewConfig Configs[] = {Model.AttackerConfig, Model.VictimConfig};
	const float Starts[] = {Model.AttackerSectionStart, Model.VictimSectionStart};
	const float Ends[] = {Model.AttackerSectionEnd, Model.VictimSectionEnd};
	for (int32 Role = 0; Role < 2; ++Role)
	{
		auto* Mesh = Meshes[Role]; auto* Montage = Montages[Role];
		if (!Mesh || !Montage || !Assets[Role]) { OutError = TEXT("Missing preview mesh or montage"); return false; }
		if (Mesh->GetSkeletalMeshAsset() != Assets[Role]) { Mesh->SetSkeletalMesh(Assets[Role]); Mesh->SetForcedLOD(1); }
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		if (!Mesh->GetSingleNodeInstance() || Mesh->GetSingleNodeInstance()->GetCurrentAsset() != Montage) { Mesh->SetAnimation(Montage); }
		// The widget advances its own clock. A later preview-world tick must not
		// independently advance the montage beyond this absolute sample.
		Mesh->GetSingleNodeInstance()->SetPlaying(false);
		Mesh->GetSingleNodeInstance()->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		const float Position = FMath::Min(Ends[Role], Starts[Role] + Time * Montage->RateScale);
		Mesh->SetPosition(Position, false); Mesh->TickAnimation(0, false); Mesh->RefreshBoneTransforms();
		const FAnimExtractContext Extraction(static_cast<double>(Position), true, FDeltaTimeRecord(), false);
		const FTransform RootDelta = Montage->ExtractRootMotionFromTrackRange(Starts[Role], Position, Extraction);
		const FTransform Initial(Configs[Role].RotationOffset, Configs[Role].PositionOffset, FVector(Configs[Role].Scale));
		Mesh->SetWorldTransform(RootDelta * Initial); Mesh->RefreshBoneTransforms();
	}
	return true;
}

bool UPairedAnimationAnalysisSubsystem::LoadContactProfileIntoPreview(const FString& ProfilePath, FPairedAnimationPreviewModel& Model, FString& OutError)
{
	PairedContactProfile::FProfile Profile;
	if (!PairedContactProfile::Load(ProfilePath, Profile, OutError)) { return false; }
	Model = Profile.Model; return true;
}

namespace PairedContactProfile
{
bool Transform(const FJson& Json, FTransform& Out)
{
	FVector Position; const TArray<TSharedPtr<FJsonValue>>* Rotation = nullptr;
	if (!Vector(Json, TEXT("world_cm"), Position) || !Json->TryGetArrayField(TEXT("world_rotation_xyzw"), Rotation) || Rotation->Num() != 4) { return false; }
	double Q[4]; for (int32 I = 0; I < 4; ++I) { if (!(*Rotation)[I]->TryGetNumber(Q[I]) || !FMath::IsFinite(Q[I])) { return false; } }
	const FQuat Quaternion(Q[0], Q[1], Q[2], Q[3]);
	if (!Quaternion.IsNormalized()) { return false; }
	Out = FTransform(Quaternion, Position); return Out.IsValid();
}

bool AcquireRuntime(const FProfile& Profile, const FString& Directory, TArray<FPairedContactPose>& Samples, FString& Error)
{
	const FJson Manifest = Read(Directory / TEXT("session.json")); double Schema = 0;
	FString State, StopReason;
	if (!Number(Manifest, TEXT("schema_version"), Schema) || Schema != 2
		|| !Manifest->TryGetStringField(TEXT("status"), State) || State != TEXT("complete")
		|| !Manifest->TryGetStringField(TEXT("stop_reason"), StopReason) || StopReason != TEXT("scenario_finished"))
	{
		Error = TEXT("Runtime comparison requires a complete schema-2 scenario capture"); return false;
	}
	const FJson Scenario = Read(Directory / TEXT("scenario.json"));
	const FJson Context = Read(Directory / TEXT("run-context.json"));
	FString Experiment = TEXT("none"), Requested = TEXT("none"), Captured = TEXT("none"), OverrideText = TEXT("[]");
	if (Scenario) { Scenario->TryGetStringField(TEXT("runtime_experiment"), Experiment); }
	if (Context) { Context->TryGetStringField(TEXT("runtime_experiment"), Requested); }
	const TSharedPtr<FJsonObject>* Metadata = nullptr;
	if (Manifest->TryGetObjectField(TEXT("metadata"), Metadata))
	{
		(*Metadata)->TryGetStringField(TEXT("runtime_experiment"), Captured);
		(*Metadata)->TryGetStringField(TEXT("runtime_asset_overrides"), OverrideText);
	}
	TArray<TSharedPtr<FJsonValue>> CapturedOverrides;
	const TArray<TSharedPtr<FJsonValue>>* Overrides = nullptr;
	if (Scenario) { Scenario->TryGetArrayField(TEXT("runtime_asset_overrides"), Overrides); }
	const TArray<TSharedPtr<FJsonValue>> EmptyOverrides;
	const bool bTuning = Experiment == TEXT("paired-warp-tuning");
	const FString WarpRole = Experiment == TEXT("attacker-source-translation") ? TEXT("Attacker")
		: (Experiment == TEXT("victim-source-translation") || Experiment == TEXT("victim-source-rotation")) ? TEXT("Victim") : TEXT("");
	const FString WarpProperty = Experiment == TEXT("victim-source-rotation") ? TEXT("RootMotionModifier.bWarpRotation") : TEXT("RootMotionModifier.bWarpTranslation");
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(OverrideText), CapturedOverrides)
		|| Experiment != Requested || Experiment != Captured
		|| (Experiment != TEXT("none") && Experiment != TEXT("permit-root-motion") && WarpRole.IsEmpty() && !bTuning)
		|| !FJsonValue::CompareEqual(FJsonValueArray(CapturedOverrides), FJsonValueArray(Overrides ? *Overrides : EmptyOverrides))
		|| ((Experiment == TEXT("none")) != CapturedOverrides.IsEmpty()))
	{
		Error = TEXT("Inconsistent runtime experiment provenance"); return false;
	}
	if (bTuning && !PairedWarpTuning::Validate(Context, CapturedOverrides, Profile.Model.GetAttackerMontage()->GetPathName(),
		Profile.Model.GetVictimMontage()->GetPathName(), Profile.Pair->GetPathName()))
	{
		Error = TEXT("Invalid paired warp tuning provenance"); return false;
	}
	TSet<FString> OverrideRoles;
	TSet<FString> NotifyIds;
	int32 WarpOverrides = 0;
	for (const auto& Value : CapturedOverrides)
	{
		if (bTuning) { break; }
		const auto Row = Value->Type == EJson::Object ? Value->AsObject() : nullptr;
		FString Role, Property, Class, Asset; bool Before = false, After = true; double Index = -1;
		if (!Row || !Row->TryGetStringField(TEXT("role"), Role) || !Row->TryGetStringField(TEXT("property"), Property)
			|| !Row->TryGetStringField(TEXT("notify_class"), Class) || !Row->TryGetStringField(TEXT("asset"), Asset)
			|| !Row->TryGetBoolField(TEXT("before"), Before) || !Row->TryGetBoolField(TEXT("after"), After) || !Number(Row, TEXT("notify_index"), Index)
			|| (Role != TEXT("Attacker") && Role != TEXT("Victim"))
			|| !Before || After || Index < 0 || Index != FMath::FloorToDouble(Index)
			|| Asset != (Role == TEXT("Attacker") ? Profile.Model.GetAttackerMontage()->GetPathName() : Profile.Model.GetVictimMontage()->GetPathName()))
		{
			Error = TEXT("Unsupported runtime notify override"); return false;
		}
		const FString NotifyId = Role + TEXT("|") + Asset + TEXT("|") + FString::Printf(TEXT("%.0f"), Index);
		if (NotifyIds.Contains(NotifyId)) { Error = TEXT("Duplicate runtime notify override"); return false; }
		NotifyIds.Add(NotifyId);
		if (Property == TEXT("bDisableMovement") && Class == TEXT("AnimNotifyState_PairedAnimationCollision")) { OverrideRoles.Add(Role); }
		else if (Property == WarpProperty && Class == TEXT("AnimNotifyState_MotionWarping")
			&& !WarpRole.IsEmpty() && Role == WarpRole) { ++WarpOverrides; }
		else { Error = TEXT("Unsupported runtime notify override"); return false; }
	}
	if (!bTuning && Experiment != TEXT("none") && OverrideRoles.Num() != 2) { Error = TEXT("Movement experiment must describe both participants"); return false; }
	if (WarpOverrides != (WarpRole.IsEmpty() ? 0 : 1)) { Error = TEXT("Missing or excess warp override"); return false; }
	TArray<FString> Lines; if (!FFileHelper::LoadFileToStringArray(Lines, *(Directory / TEXT("samples.jsonl"))) || Lines.Num() > 144000) { Error = TEXT("Missing or oversized pose ledger"); return false; }
	const TSet<FString> Keys = PointKeys(Profile);
	int32 InstanceIds[] = {INDEX_NONE, INDEX_NONE}; double PreviousTime = -DBL_MAX;
	FString ComponentIds[2]; TMap<FString, FString> PointSourceIds;
	const FString Paths[] = {Profile.Model.GetAttackerMontage()->GetPathName(), Profile.Model.GetVictimMontage()->GetPathName()};
	const FString MeshPaths[] = {Profile.Model.GetAttackerSkeleton()->GetPathName(), Profile.Model.GetVictimSkeleton()->GetPathName()};
	for (const FString& Line : Lines)
	{
		FJson Row; if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Row) || !Row) { Error = TEXT("Malformed runtime pose row"); return false; }
		FPairedContactPose Pose; double Frame = 0, WorldDilation = 0; bool bPaused = false;
		const TArray<TSharedPtr<FJsonValue>>* Actors = nullptr;
		if (!Number(Row, TEXT("simulation_time_s"), Pose.OriginalTimeSeconds) || !Number(Row, TEXT("engine_frame"), Frame)
			|| !Number(Row, TEXT("world_time_dilation"), WorldDilation) || !Row->TryGetBoolField(TEXT("world_paused"), bPaused)
			|| !Row->TryGetArrayField(TEXT("actors"), Actors)) { Error = TEXT("Missing runtime clocks or participants"); return false; }
		FJson Participants[2], Montages[2];
		for (const auto& Value : *Actors)
		{
			if (Value->Type != EJson::Object) { continue; } const FJson Actor = Value->AsObject(); FString Role;
			if (!Actor->TryGetStringField(TEXT("role"), Role)) { continue; }
			const int32 Index = Role == TEXT("Attacker") ? 0 : Role == TEXT("Victim") ? 1 : INDEX_NONE;
			if (Index == INDEX_NONE) { continue; }
			if (Participants[Index]) { Error = TEXT("Duplicate runtime role"); return false; }
			Participants[Index] = Actor; const TArray<TSharedPtr<FJsonValue>>* Instances = nullptr;
			if (!Actor->TryGetArrayField(TEXT("montages"), Instances)) { continue; }
			for (const auto& Instance : *Instances)
			{
				if (Instance->Type != EJson::Object) { continue; } const FJson Montage = Instance->AsObject(); FString Path; double Weight = 0;
				if (Montage->TryGetStringField(TEXT("asset"), Path) && Path == Paths[Index] && Number(Montage, TEXT("weight"), Weight) && Weight > 0)
				{
					if (Montages[Index]) { Error = TEXT("Ambiguous contributing paired montage"); return false; } Montages[Index] = Montage;
				}
			}
		}
		// Montage position supplies the original authored clock; no fitted temporal
		// alignment can hide victim timing drift. Rows outside this pair are excluded.
		if (!Montages[0]) { continue; }
		double Position = 0;
		if (!Number(Montages[0], TEXT("position_s"), Position)) { Error = TEXT("Missing attacker montage position"); return false; }
		Pose.TimeSeconds = (Position - Profile.Model.AttackerSectionStart) / Profile.Model.GetAttackerMontage()->RateScale;
		if (Pose.TimeSeconds < 0 || Pose.TimeSeconds > Profile.Model.MaxDuration + UE_KINDA_SMALL_NUMBER) { continue; }
		if (Pose.TimeSeconds <= PreviousTime) { Error = TEXT("Runtime pair clock paused, looped or restarted; use a separate interval"); return false; }
		PreviousTime = Pose.TimeSeconds;
		Pose.bEligible = !bPaused && FMath::IsNearlyEqual(WorldDilation, 1.0);
		for (int32 Role = 0; Role < 2; ++Role)
		{
			bool bValid = false, bFresh = false; double PoseFrame = -1, PoseTime = 0, Serial = 0, Dilation = 0, InstanceId = -1;
			FString MeshPath, ComponentId; double PlayRate = 0;
			const FJson Actor = Participants[Role];
			if (!Actor || !Montages[Role] || !Actor->TryGetBoolField(TEXT("valid"), bValid) || !bValid
				|| !Actor->TryGetBoolField(TEXT("pose_finalized_this_frame"), bFresh) || !bFresh
				|| !Number(Actor, TEXT("pose_engine_frame"), PoseFrame) || PoseFrame != Frame
				|| !Number(Actor, TEXT("pose_simulation_time_s"), PoseTime) || FMath::Abs(Pose.OriginalTimeSeconds - PoseTime) > .04
				|| !Number(Actor, TEXT("pose_evaluation_serial"), Serial) || Serial <= 0
				|| !Number(Actor, TEXT("custom_time_dilation"), Dilation) || !FMath::IsNearlyEqual(Dilation, 1.0)
				|| !Actor->TryGetStringField(TEXT("mesh_asset"), MeshPath) || MeshPath != MeshPaths[Role]
				|| !Number(Montages[Role], TEXT("instance_id"), InstanceId)
				|| !Actor->TryGetStringField(TEXT("mesh_component"), ComponentId) || ComponentId.IsEmpty()
				|| !Number(Montages[Role], TEXT("play_rate"), PlayRate) || !FMath::IsNearlyEqual(PlayRate, 1.0)) { Pose.bEligible = false; continue; }
			if (ComponentIds[Role].IsEmpty()) { ComponentIds[Role] = ComponentId; }
			if (ComponentIds[Role] != ComponentId) { Error = TEXT("Nominated runtime mesh component changed"); return false; }
			if (Role == 1)
			{
				double VictimPosition = 0;
				if (Number(Montages[Role], TEXT("position_s"), VictimPosition)) { Pose.VictimTimeSeconds = (VictimPosition - Profile.Model.VictimSectionStart) / Profile.Model.GetVictimMontage()->RateScale; }
			}
			if (InstanceIds[Role] == INDEX_NONE) { InstanceIds[Role] = int32(InstanceId); }
			if (InstanceIds[Role] != int32(InstanceId)) { Error = TEXT("Multiple paired instances require separate captures"); return false; }
			const TSharedPtr<FJsonObject>* Points = nullptr;
			if (!Actor->TryGetObjectField(TEXT("points"), Points)) { Pose.bEligible = false; continue; }
			for (const FString& Key : Keys)
			{
				FString KeyRole, Point; Key.Split(TEXT(":"), &KeyRole, &Point);
				if (KeyRole != (Role == 0 ? TEXT("Attacker") : TEXT("Victim"))) { continue; }
				const TSharedPtr<FJsonObject>* PointJson = nullptr; FTransform Value;
				if (!(*Points)->TryGetObjectField(Point, PointJson) || !Transform(*PointJson, Value)) { continue; }
				FString Source;
				if (!(*PointJson)->TryGetStringField(TEXT("source_component"), Source) || Source.IsEmpty()) { continue; }
				const auto& Weapon = Role == 0 ? Profile.Model.AttackerWeaponConfig : Profile.Model.VictimWeaponConfig;
				if (Weapon.UseForContactDetection() && (FName(*Point) == Weapon.WeaponBaseSocket || FName(*Point) == Weapon.WeaponTipSocket))
				{
					FString Asset, Parent, Socket;
					if (!(*PointJson)->TryGetStringField(TEXT("source_asset"), Asset) || Asset != Weapon.WeaponMesh->GetPathName()
						|| !(*PointJson)->TryGetStringField(TEXT("attach_parent"), Parent) || Parent != ComponentId
						|| !(*PointJson)->TryGetStringField(TEXT("attachment_socket"), Socket) || Socket != Weapon.AttachmentSocket.ToString()) { continue; }
				}
				else if (Source != ComponentId) { continue; }
				if (const auto* Existing = PointSourceIds.Find(Key); Existing && *Existing != Source) { Error = TEXT("Nominated point source changed: ") + Key; return false; }
				PointSourceIds.Add(Key, Source); Pose.Points.Add(Key, Value);
			}
		}
		Pose.IneligibilityReason = TEXT("Missing paired contribution, stale pose, different mesh or unsupported time dilation");
		Samples.Add(MoveTemp(Pose));
	}
	if (Samples.IsEmpty()) { Error = TEXT("Capture has no matching paired montage poses"); return false; }
	return true;
}

FString FileIdentity(const FString& Path)
{
	TArray<uint8> Bytes; FSHAHash Hash;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path)) { return TEXT("unavailable"); }
	FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash.Hash); return Hash.ToString();
}

// Snapshot the relevant project packages rather than trusting equal asset paths.
// Runtime comparison rejects missing/changed bytes; previews reject unsaved assets.
bool AssetIdentity(const FProfile& Profile, const FString& CaptureDirectory, FJson& Identity, FString& Error)
{
	Identity = MakeShared<FJsonObject>();
	TArray<FName> Pending;
	for (const UObject* Object : {static_cast<UObject*>(Profile.Pair.Get()), static_cast<UObject*>(Profile.Model.GetAttackerSkeleton()), static_cast<UObject*>(Profile.Model.GetVictimSkeleton())})
	{
		Pending.Add(Object->GetOutermost()->GetFName());
	}
	for (const TCHAR* Key : {TEXT("attacker_weapon_data"), TEXT("victim_weapon_data")})
	{
		FString Path; if (Profile.Definition->TryGetStringField(Key, Path)) { Pending.Add(FName(*FPackageName::ObjectPathToPackageName(Path))); }
	}
	// Preview-selected weapon meshes can differ from the data asset's mesh.
	// Include their actual bytes and dependencies in the effective-input identity.
	for (const UStaticMesh* Mesh : {Profile.Model.AttackerWeaponConfig.WeaponMesh.Get(), Profile.Model.VictimWeaponConfig.WeaponMesh.Get()})
	{
		if (Mesh) { Pending.Add(Mesh->GetOutermost()->GetFName()); }
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TSet<FName> Seen;
	FJson Captured = CaptureDirectory.IsEmpty() ? nullptr : Read(CaptureDirectory / TEXT("asset-identity.json"));
	const TSharedPtr<FJsonObject>* CapturedFiles = nullptr;
	if (!CaptureDirectory.IsEmpty() && (!Captured || !Captured->TryGetObjectField(TEXT("files_sha1"), CapturedFiles))) { Error = TEXT("Runtime comparison requires captured project SHA-1 asset hashes from the current scenario runner"); return false; }
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		const FName Name = Pending[Index];
		if (Seen.Contains(Name) || !Name.ToString().StartsWith(TEXT("/Game/"))) { continue; }
		Seen.Add(Name);
		if (UPackage* Package = FindPackage(nullptr, *Name.ToString()); Package && Package->IsDirty()) { Error = TEXT("Unsaved evaluated asset: ") + Name.ToString(); return false; }
		TArray<FName> Dependencies; Registry.GetDependencies(Name, Dependencies, UE::AssetRegistry::EDependencyCategory::Package); Pending.Append(Dependencies);
		FString Path;
		if (!FPackageName::DoesPackageExist(Name.ToString(), &Path))
		{
			// Registry soft dependencies can be absent; the loaded pair/meshes have
			// already been checked. Preserve absence and compare it explicitly.
			Identity->SetField(Name.ToString(), MakeShared<FJsonValueNull>());
			if (Captured)
			{
				const TArray<TSharedPtr<FJsonValue>>* Missing = nullptr;
				if (!Captured->TryGetArrayField(TEXT("absent_soft_dependencies"), Missing) || !Missing->ContainsByPredicate([&](const auto& V) { return V->AsString() == Name.ToString(); })) { Error = TEXT("Unrecorded missing project dependency: ") + Name.ToString(); return false; }
			}
			continue;
		}
		const FString Digest = FileIdentity(Path).ToLower();
		if (Digest == TEXT("unavailable")) { Error = TEXT("Cannot identify evaluated asset bytes: ") + Name.ToString(); return false; }
		Path = FPaths::ConvertRelativePathToFull(Path); FPaths::MakePathRelativeTo(Path, *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
		FPaths::NormalizeFilename(Path); Identity->SetStringField(Path, Digest);
		if (CapturedFiles)
		{
			FString Previous;
			if (!(*CapturedFiles)->TryGetStringField(Path, Previous) || Previous != Digest) { Error = TEXT("Captured asset hash is missing or differs: ") + Path; return false; }
		}
	}
	return true;
}

TArray<TSharedPtr<FJsonValue>> TransformJson(const FTransform& Value)
{
	TArray<TSharedPtr<FJsonValue>> Numbers;
	const FVector P = Value.GetLocation(), S = Value.GetScale3D(); const FQuat Q = Value.GetRotation();
	for (double Number : {P.X, P.Y, P.Z, Q.X, Q.Y, Q.Z, Q.W, S.X, S.Y, S.Z}) { Numbers.Add(MakeShared<FJsonValueNumber>(Number)); }
	return Numbers;
}

bool Reject(const FString& Directory, const FJson& Report, const FString& Error)
{
	Report->SetStringField(TEXT("status"), TEXT("inconclusive")); Report->SetStringField(TEXT("reason"), Error);
	FFileHelper::SaveStringToFile(TEXT("<!doctype html><meta charset='utf-8'><h1>Paired evaluation: inconclusive</h1><p>") + Escape(Error) + TEXT("</p><a href='evaluation.json'>Evidence</a>"), *(Directory / TEXT("report.html")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	Write(Directory / TEXT("evaluation.json"), Report); return false;
}

FString FrameLink(const FString& CaptureDirectory, const FString& ReportDirectory,
	const TArray<FJson>& Frames, double Time, const FJson& Observation)
{
	FJson Nearest; double Offset = DBL_MAX;
	for (const auto& Frame : Frames)
	{
		double FrameTime = 0;
		if (Number(Frame, TEXT("simulation_time_s"), FrameTime) && FMath::Abs(FrameTime - Time) < FMath::Abs(Offset)) { Nearest = Frame; Offset = FrameTime - Time; }
	}
	FString File;
	if (!Nearest || !Nearest->TryGetStringField(TEXT("file"), File)) { return TEXT("unavailable"); }
	FString Path = FPaths::ConvertRelativePathToFull(CaptureDirectory / File);
	if (!FPaths::IsUnderDirectory(Path, FPaths::ConvertRelativePathToFull(CaptureDirectory / TEXT("frames"))) || !FPaths::FileExists(Path)) { return TEXT("unavailable"); }
	FPaths::MakePathRelativeTo(Path, *(ReportDirectory + TEXT("/")));
	Observation->SetStringField(TEXT("nearest_frame"), Path); Observation->SetNumberField(TEXT("frame_time_offset_s"), Offset);
	return FString::Printf(TEXT("<a href=\"%s\">frame (%+.3f s)</a>"), *Escape(Path), Offset);
}

bool Evaluate(const FString& ProfilePath, const FString& CaptureDirectory, const FPairedAnimationPreviewModel* Override,
	FString& Directory, FString& Error)
{
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("PairedAnimationEvaluations") /
		(FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%S")) + TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	if (!IFileManager::Get().MakeDirectory(*Directory, true)) { Error = TEXT("Could not create evaluation output directory"); return false; }
	auto Report = MakeShared<FJsonObject>(); Report->SetNumberField(TEXT("schema_version"), 1); Report->SetStringField(TEXT("status"), TEXT("inconclusive"));
	Report->SetStringField(TEXT("reason"), TEXT("Evaluation in progress")); Report->SetStringField(TEXT("profile"), FPaths::ConvertRelativePathToFull(ProfilePath));
	if (!Write(Directory / TEXT("evaluation.json"), Report)) { Error = TEXT("Could not initialize evaluation report"); return false; }
	FProfile Profile;
	if (!Load(ProfilePath, Profile, Error)) { return Reject(Directory, Report, Error); }
	if (Override)
	{
		if (Override->GetAttackerMontage() != Profile.Model.GetAttackerMontage() || Override->GetVictimMontage() != Profile.Model.GetVictimMontage()
			|| Override->GetAttackerSkeleton() != Profile.Model.GetAttackerSkeleton() || Override->GetVictimSkeleton() != Profile.Model.GetVictimSkeleton()
			|| Override->AttackerSectionStart != Profile.Model.AttackerSectionStart || Override->VictimSectionStart != Profile.Model.VictimSectionStart
			|| Override->AttackerSectionEnd != Profile.Model.AttackerSectionEnd || Override->VictimSectionEnd != Profile.Model.VictimSectionEnd
			|| Override->VictimTimeOffset != 0 || Override->MaxDuration != Profile.Model.MaxDuration)
		{
			Error = TEXT("Preview assets or timing differ from the contact profile; reload a matching profile"); return Reject(Directory, Report, Error);
		}
		Profile.Model.AttackerConfig = Override->AttackerConfig; Profile.Model.VictimConfig = Override->VictimConfig;
		Profile.Model.AttackerWeaponConfig = Override->AttackerWeaponConfig; Profile.Model.VictimWeaponConfig = Override->VictimWeaponConfig;
		if (!Override->AttackerWeaponConfig.WeaponGripSocket.IsNone() || !Override->VictimWeaponConfig.WeaponGripSocket.IsNone())
		{
			Error = TEXT("Contact profile playback uses the weapon-data attachment transform; additional grip-socket correction is unsupported"); return Reject(Directory, Report, Error);
		}
	}
	Report->SetObjectField(TEXT("definition"), Profile.Definition); Report->SetStringField(TEXT("criteria_basis"), Profile.Basis);
	Report->SetArrayField(TEXT("authored_sync_events"), Profile.SyncEvents);
	Report->SetStringField(TEXT("sync_timing_basis"), TEXT("Montage-level states overlapping fresh forward section playback; active states anchor to entry. Trigger/nominal clocks are retained. Segment notifies and observed runtime damage commits are not measured."));
	Report->SetStringField(TEXT("profile_sha1"), FileIdentity(ProfilePath));
	Report->SetStringField(TEXT("editor_binary_sha1"), FileIdentity(FPaths::ProjectDir() / TEXT("Binaries/Win64/UnrealEditor-KatanaCombatEditor.dll")));
	Report->SetStringField(TEXT("capture_directory"), CaptureDirectory);
	Report->SetStringField(TEXT("runtime_status"), CaptureDirectory.IsEmpty() ? TEXT("not_run") : TEXT("requested"));
	Report->SetBoolField(TEXT("preview_overrides"), Override != nullptr);
	auto Effective = MakeShared<FJsonObject>();
	Effective->SetStringField(TEXT("transform_layout"), TEXT("position_cm xyz, quaternion xyzw, scale xyz"));
	for (int32 Role = 0; Role < 2; ++Role)
	{
		const auto& Config = Role == 0 ? Profile.Model.AttackerConfig : Profile.Model.VictimConfig;
		const auto& Weapon = Role == 0 ? Profile.Model.AttackerWeaponConfig : Profile.Model.VictimWeaponConfig;
		auto Item = MakeShared<FJsonObject>();
		Item->SetArrayField(TEXT("mesh_transform"), TransformJson(FTransform(Config.RotationOffset, Config.PositionOffset, FVector(Config.Scale))));
		Item->SetArrayField(TEXT("weapon_transform"), TransformJson(Weapon.AttachmentOffset));
		Item->SetStringField(TEXT("weapon_mesh"), GetPathNameSafe(Weapon.WeaponMesh.Get())); Item->SetStringField(TEXT("attachment_socket"), Weapon.AttachmentSocket.ToString());
		Item->SetStringField(TEXT("weapon_start"), Weapon.WeaponBaseSocket.ToString()); Item->SetStringField(TEXT("weapon_end"), Weapon.WeaponTipSocket.ToString());
		Effective->SetObjectField(Role == 0 ? TEXT("Attacker") : TEXT("Victim"), Item);
	}
	Report->SetObjectField(TEXT("effective_preview"), Effective);
	FJson Assets;
	if (!AssetIdentity(Profile, CaptureDirectory, Assets, Error)) { return Reject(Directory, Report, Error); }
	Report->SetObjectField(TEXT("project_asset_sha1"), Assets);
	Report->SetStringField(TEXT("timing_basis"), TEXT("Authored section start + elapsed * montage RateScale; runtime uses original attacker montage position with simulation timestamps retained. Victim start follows current runtime offset/section precedence."));
	Report->SetStringField(TEXT("unmeasured"), TEXT("Skin penetration, foot support, artistic/readability quality, full AnimGraph blend parity and causal warp correction remain unmeasured. Segment/sphere contact is a declared geometric proxy. Contact results apply to observed poses only: brief contacts or interruptions can occur between samples. A sampled failure does not prove continuous absence of contact."));
	TArray<FPairedContactPose> Authored, Runtime;
	if (!AcquireAuthored(Profile, Authored, Error)) { return Reject(Directory, Report, Error); }
	FString RuntimeError; const bool bHasRuntime = !CaptureDirectory.IsEmpty() && AcquireRuntime(Profile, CaptureDirectory, Runtime, RuntimeError);
	TArray<FJson> Frames;
	if (!CaptureDirectory.IsEmpty())
	{
		const FJson Scenario = Read(CaptureDirectory / TEXT("scenario.json"));
		FString Experiment = TEXT("none"); const TArray<TSharedPtr<FJsonValue>>* Overrides = nullptr;
		if (Scenario) { Scenario->TryGetStringField(TEXT("runtime_experiment"), Experiment); Scenario->TryGetArrayField(TEXT("runtime_asset_overrides"), Overrides); }
		Report->SetStringField(TEXT("runtime_experiment"), Experiment);
		if (Overrides) { Report->SetArrayField(TEXT("runtime_asset_overrides"), *Overrides); }
		TArray<FString> Lines;
		if (FFileHelper::LoadFileToStringArray(Lines, *(CaptureDirectory / TEXT("frames.jsonl"))) && Lines.Num() <= 10000)
		{
			for (const FString& Line : Lines) { FJson Frame; if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Frame) && Frame) { Frames.Add(Frame); } }
		}
		Report->SetStringField(TEXT("runtime_status"), bHasRuntime ? TEXT("measured") : TEXT("inconclusive"));
	}
	if (!CaptureDirectory.IsEmpty()) { Report->SetStringField(TEXT("runtime_samples_sha1"), FileIdentity(CaptureDirectory / TEXT("samples.jsonl"))); }
	TArray<TSharedPtr<FJsonValue>> Cases;
	FString Html = TEXT("<!doctype html><meta charset='utf-8'><title>Paired animation evaluation</title><style>body{font:16px system-ui;max-width:1100px;margin:32px auto;padding:16px;background:#151b24;color:#e8edf4}a{color:#80c9ff}table{border-collapse:collapse;width:100%}td,th{padding:8px;border-bottom:1px solid #445;text-align:left}code{overflow-wrap:anywhere}summary{cursor:pointer}</style><h1>Paired animation evaluation</h1>");
	Html += TEXT("<p>") + Escape(Profile.Pair->GetPathName()) + TEXT("</p><p>") + Escape(Profile.Basis) + TEXT("</p><p>") + Escape(Report->GetStringField(TEXT("unmeasured"))) + TEXT("</p><p><a href='evaluation.json'>Full measurements and criteria</a></p>");
	if (!CaptureDirectory.IsEmpty())
	{
		Html += TEXT("<p>Runtime experiment: <strong>") + Escape(Report->GetStringField(TEXT("runtime_experiment")))
			+ TEXT("</strong>. Explicit in-memory asset overrides are retained in the JSON. Authored playback uses the unchanged disk assets.</p>");
	}
	Html += TEXT("<h2>Authored sync states</h2><p>") + Escape(Report->GetStringField(TEXT("sync_timing_basis")))
		+ TEXT("</p><table><tr><th>Role / event</th><th>Nominal montage time</th><th>Trigger time</th><th>Effective pair time</th><th>Primary</th><th>Damage configured</th><th>Runtime commit</th></tr>");
	for (const auto& Value : Profile.SyncEvents)
	{
		const auto Event = Value->AsObject();
		Html += FString::Printf(TEXT("<tr><td>%s / %s</td><td>%.6f s</td><td>%.6f s</td><td>%.6f s%s</td><td>%s</td><td>%s</td><td>not measured</td></tr>"),
			*Escape(Event->GetStringField(TEXT("role"))), *Escape(Event->GetStringField(TEXT("name"))), Event->GetNumberField(TEXT("nominal_montage_time_s")),
			Event->GetNumberField(TEXT("montage_time_s")), Event->GetNumberField(TEXT("pair_time_s")), Event->GetBoolField(TEXT("active_at_entry")) ? TEXT(" (active at entry)") : TEXT(""),
			Event->GetBoolField(TEXT("is_primary")) ? TEXT("yes") : TEXT("no"), Event->GetBoolField(TEXT("damage_configured")) ? TEXT("yes") : TEXT("no"));
	}
	Html += TEXT("</table>");
	bool bFailed = false, bInconclusive = !CaptureDirectory.IsEmpty() && !bHasRuntime;
	for (int32 Lane = 0; Lane < (CaptureDirectory.IsEmpty() ? 1 : 2); ++Lane)
	{
		const FString LaneName = Lane == 0 ? TEXT("authored") : TEXT("runtime");
		for (const auto& Rule : Profile.Rules)
		{
			FPairedContactEvaluation Result;
			if (Lane == 1 && !bHasRuntime) { Result.Reason = RuntimeError; }
			else { Result = UPairedAnimationAnalysisLibrary::EvaluateIntendedContact(Rule, Lane == 0 ? Authored : Runtime); }
			bFailed |= Result.Status == EPairedContactResult::Fail; bInconclusive |= Result.Status == EPairedContactResult::Inconclusive;
			auto Case = MakeShared<FJsonObject>(); Case->SetStringField(TEXT("name"), Rule.Name); Case->SetStringField(TEXT("lane"), LaneName);
			Case->SetStringField(TEXT("status"), Status(Result.Status)); Case->SetStringField(TEXT("reason"), Result.Reason);
			Case->SetNumberField(TEXT("interval_start_s"), Rule.StartSeconds); Case->SetNumberField(TEXT("interval_end_s"), Rule.EndSeconds);
			Case->SetNumberField(TEXT("maximum_observed_sample_gap_s"), Result.MaximumSampleGapSeconds);
			Case->SetNumberField(TEXT("maximum_allowed_sample_gap_s"), Rule.MaximumSampleGapSeconds);
			Case->SetNumberField(TEXT("minimum_allowed_gap_cm"), Rule.MinimumGapCm); Case->SetNumberField(TEXT("maximum_allowed_gap_cm"), Rule.MaximumGapCm);
			Case->SetNumberField(TEXT("matching_samples"), Result.MatchingSamples);
			TArray<TSharedPtr<FJsonValue>> Rows;
			Html += FString::Printf(TEXT("<h2>%s / %s: %s</h2><p>%s</p><p>Interval %.3f–%.3f s; allowed gap %.2f–%.2f cm; maximum observed sample gap %.4f s.</p><details><summary>Measured poses</summary><table><tr><th>Pair time (s)</th><th>Original time (s)</th><th>Gap (cm)</th><th>Angle error (deg)</th><th>Within criteria</th></tr>"), *Escape(LaneName), *Escape(Rule.Name), *Status(Result.Status), *Escape(Result.Reason), Rule.StartSeconds, Rule.EndSeconds, Rule.MinimumGapCm, Rule.MaximumGapCm, Result.MaximumSampleGapSeconds);
			for (const auto& Observation : Result.Observations)
			{
				auto Row = MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("pair_time_s"), Observation.TimeSeconds); Row->SetNumberField(TEXT("original_time_s"), Observation.OriginalTimeSeconds);
				Row->SetNumberField(TEXT("signed_gap_cm"), Observation.SignedGapCm);
				if (Rule.bMeasureOrientation) { Row->SetNumberField(TEXT("angle_error_deg"), Observation.AngleErrorDegrees); }
				else { Row->SetField(TEXT("angle_error_deg"), MakeShared<FJsonValueNull>()); }
				Row->SetBoolField(TEXT("within_criteria"), Observation.bWithinCriteria); Rows.Add(MakeShared<FJsonValueObject>(Row));
				const FString Link = Lane == 1 ? FrameLink(CaptureDirectory, Directory, Frames, Observation.OriginalTimeSeconds, Row) : TEXT("authored sample");
				const FString Angle = Rule.bMeasureOrientation ? FString::Printf(TEXT("%.2f"), Observation.AngleErrorDegrees) : TEXT("not measured");
				Html += FString::Printf(TEXT("<tr><td>%.4f</td><td>%.4f<br>%s</td><td>%.3f</td><td>%s</td><td>%s</td></tr>"), Observation.TimeSeconds, Observation.OriginalTimeSeconds, *Link, Observation.SignedGapCm, *Angle, Observation.bWithinCriteria ? TEXT("yes") : TEXT("no"));
			}
			Html += TEXT("</table></details>"); Case->SetArrayField(TEXT("observations"), Rows); Cases.Add(MakeShared<FJsonValueObject>(Case));
		}
	}
	for (const auto& Rule : Profile.AlignmentRules)
	{
		FPairedAlignmentEvaluation Result;
		if (bHasRuntime) { Result = UPairedAnimationAnalysisLibrary::EvaluateRelativeAlignment(Rule, Authored, Runtime); }
		else { Result.Reason = CaptureDirectory.IsEmpty() ? TEXT("Runtime capture was not supplied") : RuntimeError; }
		const FString ResultStatus = CaptureDirectory.IsEmpty() ? TEXT("not_run") : Status(Result.Status);
		if (!CaptureDirectory.IsEmpty()) { bFailed |= Result.Status == EPairedContactResult::Fail; bInconclusive |= Result.Status == EPairedContactResult::Inconclusive; }
		auto Case = MakeShared<FJsonObject>(); Case->SetStringField(TEXT("name"), Rule.Name); Case->SetStringField(TEXT("lane"), TEXT("relative_alignment"));
		Case->SetStringField(TEXT("status"), ResultStatus); Case->SetStringField(TEXT("reason"), Result.Reason);
		Case->SetNumberField(TEXT("interval_start_s"), Rule.StartSeconds); Case->SetNumberField(TEXT("interval_end_s"), Rule.EndSeconds);
		Case->SetNumberField(TEXT("maximum_translation_cm"), Rule.MaximumTranslationCm); Case->SetNumberField(TEXT("maximum_rotation_deg"), Rule.MaximumRotationDegrees);
		Case->SetNumberField(TEXT("maximum_victim_timing_error_s"), Rule.MaximumVictimTimingErrorSeconds); Case->SetNumberField(TEXT("maximum_sample_gap_s"), Rule.MaximumSampleGapSeconds);
		Html += FString::Printf(TEXT("<h2>Relative alignment / %s: %s</h2><p>%s</p><p>%.3f–%.3f s; budget %.2f cm, %.2f deg, victim timing %.3f s. This measures the geometric correction needed to match authored relative root placement; actual motion-warp correction remains unmeasured.</p><details><summary>Measured poses</summary><table><tr><th>Pair time (s)</th><th>Frame</th><th>Translation (cm)</th><th>Rotation (deg)</th><th>Victim timing error (s)</th></tr>"), *Escape(Rule.Name), *ResultStatus, *Escape(Result.Reason), Rule.StartSeconds, Rule.EndSeconds, Rule.MaximumTranslationCm, Rule.MaximumRotationDegrees, Rule.MaximumVictimTimingErrorSeconds);
		TArray<TSharedPtr<FJsonValue>> Rows;
		for (const auto& Observation : Result.Observations)
		{
			auto Row = MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("pair_time_s"), Observation.TimeSeconds); Row->SetNumberField(TEXT("original_time_s"), Observation.OriginalTimeSeconds);
			Row->SetNumberField(TEXT("translation_cm"), Observation.TranslationCm); Row->SetNumberField(TEXT("rotation_deg"), Observation.RotationDegrees);
			Row->SetNumberField(TEXT("victim_timing_error_s"), Observation.VictimTimingErrorSeconds); Row->SetBoolField(TEXT("within_criteria"), Observation.bWithinCriteria);
			const FString Link = FrameLink(CaptureDirectory, Directory, Frames, Observation.OriginalTimeSeconds, Row);
			Html += FString::Printf(TEXT("<tr><td>%.4f</td><td>%s</td><td>%.3f</td><td>%.3f</td><td>%+.4f</td></tr>"), Observation.TimeSeconds, *Link, Observation.TranslationCm, Observation.RotationDegrees, Observation.VictimTimingErrorSeconds);
			Rows.Add(MakeShared<FJsonValueObject>(Row));
		}
		Case->SetArrayField(TEXT("observations"), Rows); Cases.Add(MakeShared<FJsonValueObject>(Case)); Html += TEXT("</table></details>");
	}
	if (!CaptureDirectory.IsEmpty())
	{
		FString Relative = FPaths::ConvertRelativePathToFull(CaptureDirectory / TEXT("report.html")); FPaths::MakePathRelativeTo(Relative, *(Directory + TEXT("/")));
		Html += TEXT("<p><a href=\"") + Escape(Relative) + TEXT("\">Captured frame timeline (original simulation timestamps)</a></p>");
	}
	Report->SetArrayField(TEXT("cases"), Cases); Report->SetStringField(TEXT("status"), bFailed ? TEXT("fail") : bInconclusive ? TEXT("inconclusive") : TEXT("pass"));
	FJson FinalAssets;
	if (!AssetIdentity(Profile, CaptureDirectory, FinalAssets, Error)) { return Reject(Directory, Report, Error); }
	for (const auto& Asset : Assets->Values)
	{
		if (Asset.Value->IsNull())
		{
			const auto* FinalValue = FinalAssets->Values.Find(Asset.Key);
			if (!FinalValue || !(*FinalValue)->IsNull()) { Error = TEXT("Missing evaluated dependency appeared during sampling"); return Reject(Directory, Report, Error); }
			continue;
		}
		FString FinalHash;
		if (!FinalAssets->TryGetStringField(Asset.Key, FinalHash) || FinalHash != Asset.Value->AsString()) { Error = TEXT("Evaluated assets changed during sampling"); return Reject(Directory, Report, Error); }
	}
	Report->SetStringField(TEXT("reason"), TEXT("Configured geometric criteria evaluated; see individual evidence lanes and unmeasured capabilities"));
	if (!FFileHelper::SaveStringToFile(Html, *(Directory / TEXT("report.html")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) || !Write(Directory / TEXT("evaluation.json"), Report))
	{
		Error = TEXT("Evaluation report write failed"); return false;
	}
	return true;
}
}

bool UPairedAnimationAnalysisSubsystem::EvaluateContactProfile(const FString& ProfilePath, const FString& CaptureDirectory, FString& OutReportDirectory, FString& OutError)
{
	return PairedContactProfile::Evaluate(ProfilePath, CaptureDirectory, nullptr, OutReportDirectory, OutError);
}

bool UPairedAnimationAnalysisSubsystem::EvaluateContactProfileForPreview(const FString& ProfilePath, const FPairedAnimationPreviewModel& Model, FString& OutReportDirectory, FString& OutError)
{
	return PairedContactProfile::Evaluate(ProfilePath, FString(), &Model, OutReportDirectory, OutError);
}
