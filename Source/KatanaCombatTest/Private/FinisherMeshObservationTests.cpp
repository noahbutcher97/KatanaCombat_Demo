#include "Misc/AutomationTest.h"
#include "AnimationCapture/AnimationCaptureMeshReference.h"
#include "AnimationCapture/AnimationCaptureMeshGPU.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Characters/PlayerCharacter.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/TargetingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRelevance.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Serialization/JsonSerializer.h"
#include "SkeletalRenderPublic.h"
#include "StaticMeshResources.h"

namespace FinisherMeshObservation
{
const FAnimationMeshLimits Limits{4, 250000, 1500000, 64, 512, 256ll * 1024 * 1024};
const TCHAR* AttackerMeshPath = TEXT("/Game/Assets/Characters/CyberpunkRunner/Meshes/SKM_CyberpunkRunnerr_B");
const TCHAR* VictimMeshPath = TEXT("/Game/Assets/Characters/FuturisticMercenary/Meshes/SKM_FuturisticMercenary_FullBodyC");
const TCHAR* WeaponPath = TEXT("/Game/Assets/Characters/CyberpunkRunner/Meshes/SKM_Katana");
const TCHAR* SourceRoot = TEXT("/Game/Assets/Animations/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Execution/");

FString OutputRoot()
{
	return FPlatformMisc::GetEnvironmentVariable(TEXT("KATANA_MESH_OUTPUT"));
}

bool WriteJson(const FString& Path, const TSharedRef<FJsonObject>& Object)
{
	FString Text; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text));
	return !IFileManager::Get().FileExists(*Path)
		&& FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FAnimationMeshEnrollment Enrollment(UMeshComponent* Mesh, const FString& Role)
{
	FAnimationMeshEnrollment E;
	E.ComponentId = Role; E.SubjectId = Role; E.StreamId = TEXT("finisher-reference");
	E.ComponentGeneration = 1; E.ConfigurationGeneration = 1;
	E.ConfigurationId = TEXT("katana-finisher-observation-v1"); E.AnalysisLOD = 0;
	if (const auto* S = Cast<USkeletalMeshComponent>(Mesh)) { E.AssetId = GetPathNameSafe(S->GetSkeletalMeshAsset()); }
	else { E.AssetId = GetPathNameSafe(CastChecked<UStaticMeshComponent>(Mesh)->GetStaticMesh()); }
	for (int32 I = 0; I < Mesh->GetNumMaterials(); ++I)
	{
		E.MaterialIds.Add(Mesh->GetMaterial(I) ? Mesh->GetMaterial(I)->GetPathName() : FString());
	}
	return E;
}

TSharedRef<FJsonObject> Inventory(UMeshComponent* Mesh, const FString& Role)
{
	auto J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("role"), Role); J->SetStringField(TEXT("component"), Mesh->GetPathName());
	J->SetStringField(TEXT("class"), Mesh->GetClass()->GetPathName());
	J->SetStringField(TEXT("asset"), Enrollment(Mesh, Role).AssetId);
	J->SetStringField(TEXT("parent"), GetPathNameSafe(Mesh->GetAttachParent()));
	J->SetStringField(TEXT("socket"), Mesh->GetAttachSocketName().ToString());
	J->SetStringField(TEXT("world_transform"), Mesh->GetComponentTransform().ToHumanReadableString());
	J->SetStringField(TEXT("relative_transform"), Mesh->GetRelativeTransform().ToHumanReadableString());
	J->SetNumberField(TEXT("engine_frame"), GFrameCounter);
	J->SetNumberField(TEXT("world_time_s"), Mesh->GetWorld()->GetTimeSeconds());
	J->SetNumberField(TEXT("observed_monotonic_s"), FPlatformTime::Seconds());
	J->SetBoolField(TEXT("registered"), Mesh->IsRegistered());
	J->SetBoolField(TEXT("simulating_physics"), Mesh->IsSimulatingPhysics());
	TArray<TSharedPtr<FJsonValue>> Materials;
	for (int32 I = 0; I < Mesh->GetNumMaterials(); ++I)
	{
		auto M = MakeShared<FJsonObject>(); auto* Material = Mesh->GetMaterial(I);
		M->SetNumberField(TEXT("slot"), I); M->SetStringField(TEXT("effective_material"), GetPathNameSafe(Material));
		if (Material)
		{
			const auto R = Material->GetRelevance_Concurrent(Mesh->GetWorld()->GetFeatureLevel());
			M->SetBoolField(TEXT("masked"), R.bMasked); M->SetBoolField(TEXT("translucent"), R.bNormalTranslucency);
			M->SetBoolField(TEXT("uses_wpo"), R.bUsesWorldPositionOffset);
			M->SetBoolField(TEXT("uses_pdo"), R.bUsesPixelDepthOffset);
		}
		Materials.Add(MakeShared<FJsonValueObject>(M));
	}
	J->SetArrayField(TEXT("materials"), Materials);
	if (auto* S = Cast<USkeletalMeshComponent>(Mesh))
	{
		J->SetNumberField(TEXT("animation_mode"), int32(S->GetAnimationMode()));
		J->SetStringField(TEXT("anim_instance"), GetPathNameSafe(S->GetAnimInstance() ? S->GetAnimInstance()->GetClass() : nullptr));
		J->SetStringField(TEXT("post_process_instance"), GetPathNameSafe(S->GetPostProcessInstance()));
		J->SetStringField(TEXT("post_process_class"), GetPathNameSafe(S->GetPostProcessAnimBPClassToBeUsed()));
		J->SetStringField(TEXT("leader"), GetPathNameSafe(S->LeaderPoseComponent.Get()));
		J->SetBoolField(TEXT("blend_physics"), S->bBlendPhysics);
		J->SetBoolField(TEXT("parallel_evaluation_running"), S->IsRunningParallelEvaluation());
		J->SetBoolField(TEXT("ref_pose_override"), S->GetRefPoseOverride().IsValid());
		J->SetNumberField(TEXT("predicted_lod"), S->GetPredictedLODLevel());
		J->SetNumberField(TEXT("render_object_lod"), S->MeshObject ? S->MeshObject->GetLOD() : -1);
		J->SetNumberField(TEXT("required_bones"), S->RequiredBones.Num());
		J->SetNumberField(TEXT("visibility_tick_option"), int32(S->VisibilityBasedAnimTickOption));
		int32 ActiveMorphs = 0; for (float W : S->MorphTargetWeights) { if (W != 0) { ++ActiveMorphs; } }
		J->SetNumberField(TEXT("morph_weight_entries"), S->MorphTargetWeights.Num());
		J->SetNumberField(TEXT("nonzero_morph_weights"), ActiveMorphs);
		J->SetBoolField(TEXT("external_morph_sets"), S->GetExternalMorphSets(0).Num() > 0);
		J->SetBoolField(TEXT("analysis_lod_deformer"), S->GetMeshDeformerInstanceForLOD(0) != nullptr);
		TArray<TSharedPtr<FJsonValue>> Lods;
		if (const auto* Render = S->GetSkeletalMeshRenderData())
		{
			for (int32 I = 0; I < Render->LODRenderData.Num(); ++I)
			{
				const auto& L = Render->LODRenderData[I]; auto O = MakeShared<FJsonObject>();
				O->SetNumberField(TEXT("lod"), I); O->SetNumberField(TEXT("vertices"), L.GetNumVertices());
				const auto* Indices = L.MultiSizeIndexContainer.GetIndexBuffer();
				O->SetNumberField(TEXT("indices"), Indices ? Indices->Num() : 0);
				FAnimationMeshData Topology; Topology.Enrollment = Enrollment(Mesh, Role); Topology.Enrollment.AnalysisLOD = I;
				Topology.Positions.SetNum(L.GetNumVertices());
				if (Indices) { for (int32 N = 0; N < Indices->Num(); ++N) { Topology.Indices.Add(Indices->Get(N)); } }
				int32 Cloth = 0; TArray<TSharedPtr<FJsonValue>> Sections;
				for (const auto& Section : L.RenderSections)
				{
					Cloth += Section.HasClothingData() ? 1 : 0; auto T = MakeShared<FJsonObject>();
					T->SetNumberField(TEXT("first_index"), Section.BaseIndex); T->SetNumberField(TEXT("triangles"), Section.NumTriangles);
					T->SetNumberField(TEXT("material_slot"), Section.MaterialIndex); Sections.Add(MakeShared<FJsonValueObject>(T));
					FAnimationMeshSection Mapping; Mapping.Id = FString::FromInt(Topology.Sections.Num());
					Mapping.FirstIndex = Section.BaseIndex; Mapping.IndexCount = Section.NumTriangles * 3;
					if (Topology.Enrollment.MaterialIds.IsValidIndex(Section.MaterialIndex)) { Mapping.MaterialId = Topology.Enrollment.MaterialIds[Section.MaterialIndex]; }
					Topology.Sections.Add(Mapping);
				}
				O->SetStringField(TEXT("topology_sha256"), AnimationCaptureMeshReplay::TopologyIdentity(Topology));
				O->SetNumberField(TEXT("cloth_sections"), Cloth); O->SetArrayField(TEXT("sections"), Sections);
				Lods.Add(MakeShared<FJsonValueObject>(O));
			}
		}
		J->SetArrayField(TEXT("lods"), Lods);
	}
	else if (const auto* Rigid = Cast<UStaticMeshComponent>(Mesh); Rigid && Rigid->GetStaticMesh())
	{
		J->SetBoolField(TEXT("nanite"), Rigid->GetStaticMesh()->HasValidNaniteData());
		J->SetStringField(TEXT("render_lod_coverage"), TEXT("not_observed; analysis LOD 0 requested"));
		if (const auto* R = Rigid->GetStaticMesh()->GetRenderData(); R && R->LODResources.Num())
		{
			J->SetNumberField(TEXT("analysis_lod_vertices"), R->LODResources[0].GetNumVertices());
			J->SetNumberField(TEXT("analysis_lod_indices"), R->LODResources[0].IndexBuffer.GetNumIndices());
		}
	}
	return J;
}

// Opt-in observer runs inside the existing gameplay scenario. It never changes the
// live pose, LOD, material or component; unavailable enrollments own no geometry.
struct FLiveInspection
{
	FDelegateHandle Handle;
	FLiveInspection()
	{
		if (OutputRoot().IsEmpty()) { return; }
		Handle = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FLiveInspection::Observe);
	}
	~FLiveInspection() { FWorldDelegates::OnWorldPostActorTick.Remove(Handle); }
	void Observe(UWorld* World, ELevelTick, float)
	{
		if (World->WorldType != EWorldType::PIE || IFileManager::Get().FileExists(*(OutputRoot() / TEXT("live-inventory.json")))) { return; }
		for (TActorIterator<APlayerCharacter> It(World); It; ++It)
		{
			auto* Player = *It; auto* Anim = Player->GetMesh()->GetAnimInstance();
			if (!Player->PairedAnimationComponent->IsPairedAnimationActive() || !Anim) { continue; }
			auto* Montage = Anim->GetCurrentActiveMontage(); const float Position = Anim->Montage_GetPosition(Montage);
			if (!Montage || Position < .4f || Position > .55f) { continue; }
			auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("attacker_montage_time_s"), Position);
			J->SetStringField(TEXT("montage"), GetPathNameSafe(Montage));
			TArray<UMeshComponent*> Components{Player->GetMesh()};
			if (auto* Target = Player->TargetingComponent->GetCurrentTarget())
			{ if (auto* Mesh = Target->FindComponentByClass<USkeletalMeshComponent>()) { Components.Add(Mesh); } }
			TArray<UStaticMeshComponent*> Static; Player->GetComponents(Static);
			for (auto* Mesh : Static) { if (Mesh->GetAttachParent() == Player->GetMesh()) { Components.Add(Mesh); } }
			TArray<TSharedPtr<FJsonValue>> Rows;
			auto Budget = MakeShared<FAnimationMeshBudget, ESPMode::ThreadSafe>(Limits);
			auto Shared = MakeShared<FAnimationCaptureBudget, ESPMode::ThreadSafe>(FAnimationCaptureBudgetLimits{8, Limits.MaxBytes});
			for (int32 I = 0; I < Components.Num(); ++I)
			{
				auto* Mesh = Components[I]; const FString Role = FString::Printf(TEXT("live-component-%d"), I);
				auto Row = Inventory(Mesh, Role); FString Error;
				Row->SetStringField(TEXT("inventory_scope"), TEXT("effective component settings at the recorded callback; no finalized live geometry acquired"));
				auto CPU = FAnimationCaptureMeshReference::Create(Mesh, Enrollment(Mesh, Role), Budget, Error);
				Row->SetBoolField(TEXT("cpu_enrolled"), CPU.IsValid()); Row->SetStringField(TEXT("cpu_error"), Error);
				if (auto* Skeletal = Cast<USkeletalMeshComponent>(Mesh))
				{
					auto* Viewport = World->GetGameViewport() ? World->GetGameViewport()->Viewport : nullptr;
					auto GPU = FAnimationCaptureMeshGPU::Create(World, Viewport, Skeletal,
						Enrollment(Mesh, Role), Limits, FAnimationMeshGPULimits{}, Shared, Error);
					Row->SetBoolField(TEXT("gpu_enrolled"), GPU.IsValid()); Row->SetStringField(TEXT("gpu_error"), Error);
				}
				Rows.Add(MakeShared<FJsonValueObject>(Row));
			}
			J->SetArrayField(TEXT("components"), Rows);
			if (!WriteJson(OutputRoot() / TEXT("live-inventory.json"), J)) { UE_LOG(LogTemp, Error, TEXT("Finisher mesh inventory publication failed")); }
			return;
		}
	}
};
FLiveInspection LiveInspection;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFinisherAuthoredMeshObservationTest,
	"KatanaCombat.Capture.Mesh.AuthoredFinisherReference", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFinisherAuthoredMeshObservationTest::RunTest(const FString&)
{
	using namespace FinisherMeshObservation;
	const FString Base = OutputRoot().IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("MeshReferences") / FGuid::NewGuid().ToString(EGuidFormats::Digits) : OutputRoot();
	const FString Root = Base / TEXT("authored-reference");
	AddInfo(FString::Printf(TEXT("Authored mesh reference output: %s"), *Root));
	if (IFileManager::Get().DirectoryExists(*Root)) { AddError(TEXT("Reference output already exists")); return false; }
	IFileManager::Get().MakeDirectory(*Root, true);
	auto* AMesh = LoadObject<USkeletalMesh>(nullptr, AttackerMeshPath);
	auto* VMesh = LoadObject<USkeletalMesh>(nullptr, VictimMeshPath);
	auto* WeaponAsset = LoadObject<UStaticMesh>(nullptr, WeaponPath);
	auto* ASeq = LoadObject<UAnimSequence>(nullptr, *(FString(SourceRoot) + TEXT("GhostSamurai_Ambush01")));
	auto* VSeq = LoadObject<UAnimSequence>(nullptr, *(FString(SourceRoot) + TEXT("GhostSamurai_Ambushed01")));
	if (!AMesh || !VMesh || !WeaponAsset || !ASeq || !VSeq) { AddError(TEXT("Authored reference assets missing")); return false; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto CreateMesh = [World](USkeletalMesh* Asset, UAnimSequence* Seq)
	{
		auto* Actor = World->SpawnActor<AActor>(); auto* Mesh = NewObject<USkeletalMeshComponent>(Actor);
		Actor->SetRootComponent(Mesh); Mesh->SetSkeletalMesh(Asset); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Mesh->RegisterComponent(); Mesh->PlayAnimation(Seq, false); Mesh->GetSingleNodeInstance()->SetPlaying(false); Mesh->SetForcedLOD(1);
		return Mesh;
	};
	auto* Attacker = CreateMesh(AMesh, ASeq); auto* Victim = CreateMesh(VMesh, VSeq);
	auto* Weapon = NewObject<UStaticMeshComponent>(Attacker->GetOwner()); Weapon->SetStaticMesh(WeaponAsset);
	Weapon->SetupAttachment(Attacker, TEXT("weapon_r")); Weapon->RegisterComponent();
	auto Shared = MakeShared<FAnimationCaptureBudget, ESPMode::ThreadSafe>(FAnimationCaptureBudgetLimits{8, Limits.MaxBytes});
	auto Budget = MakeShared<FAnimationMeshBudget, ESPMode::ThreadSafe>(Limits, Shared);
	FString Error;
	auto BodySampler = FAnimationCaptureMeshReference::Create(Victim, Enrollment(Victim, TEXT("victim")), Budget, Error);
	TestTrue(*Error, BodySampler.IsValid());
	auto WeaponSampler = FAnimationCaptureMeshReference::Create(Weapon, Enrollment(Weapon, TEXT("katana")), Budget, Error);
	TestTrue(*Error, WeaponSampler.IsValid());
	auto Report = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Rows;
	Report->SetStringField(TEXT("workflow"), TEXT("authored paused single-node CPU reference; not live gameplay"));
	Report->SetStringField(TEXT("attacker_sequence"), ASeq->GetPathName()); Report->SetStringField(TEXT("victim_sequence"), VSeq->GetPathName());
	for (int32 I = 0; I < 5 && BodySampler && WeaponSampler; ++I)
	{
		const float Time = .4f + I / 30.f;
		auto Pose = [Time](USkeletalMeshComponent* Mesh, UAnimSequence* Seq, FVector Origin)
		{
			const FTransform Basis(FRotator(0, -90, 0), Origin);
			const FTransform RootMotion = Seq->ExtractRootMotionFromRange(0.0, double(Time), FAnimExtractContext(double(Time), true));
			Mesh->SetWorldTransform(RootMotion * Basis); Mesh->SetPosition(Time, false);
			Mesh->TickAnimation(0, false); Mesh->RefreshBoneTransforms(); Mesh->UpdateComponentToWorld();
		};
		Pose(Attacker, ASeq, FVector(0, 0, 0)); Pose(Victim, VSeq, FVector(100, 0, 0));
		Weapon->UpdateComponentToWorld();
		auto Row = MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("source_time_s"), Time);
		Row->SetStringField(TEXT("head_world_cm"), Victim->GetBoneLocation(TEXT("head")).ToString());
		const FVector Head = Victim->GetBoneLocation(TEXT("head"));
		Row->SetArrayField(TEXT("head_world_xyz_cm"), {MakeShared<FJsonValueNumber>(Head.X), MakeShared<FJsonValueNumber>(Head.Y), MakeShared<FJsonValueNumber>(Head.Z)});
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const FString Name = FString::Printf(TEXT("%s-%d"), Side ? TEXT("katana") : TEXT("victim"), I);
			const double Before = FPlatformTime::Seconds();
			auto Snapshot = (Side ? WeaponSampler : BodySampler)->Capture(Name, Error);
			auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("bundle"), Name);
			J->SetNumberField(TEXT("capture_wall_ms"), (FPlatformTime::Seconds() - Before) * 1000);
			J->SetStringField(TEXT("capture_error"), Error); J->SetBoolField(TEXT("acquired"), Snapshot.IsValid());
			TestTrue(*FString::Printf(TEXT("%s acquisition: %s"), *Name, *Error), Snapshot.IsValid());
			if (Snapshot)
			{
				const double Export = FPlatformTime::Seconds();
				TestTrue(*Error, AnimationCaptureMeshReplay::Write(Root, Name, *Snapshot, Error));
				J->SetStringField(TEXT("export_error"), Error); J->SetNumberField(TEXT("export_ms"), (FPlatformTime::Seconds() - Export) * 1000);
				J->SetNumberField(TEXT("acquired_s"), Snapshot->Data().AcquiredSeconds);
				J->SetNumberField(TEXT("completed_s"), Snapshot->Data().CompletedSeconds);
			}
			Row->SetObjectField(Side ? TEXT("katana") : TEXT("victim"), J);
		}
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	Report->SetArrayField(TEXT("samples"), Rows); Report->SetNumberField(TEXT("peak_reserved_bytes"), Budget->PeakBytes());
	Report->SetObjectField(TEXT("victim_inventory"), Inventory(Victim, TEXT("victim")));
	Report->SetObjectField(TEXT("attacker_inventory"), Inventory(Attacker, TEXT("attacker")));
	Report->SetObjectField(TEXT("weapon_inventory"), Inventory(Weapon, TEXT("katana")));
	// A deliberately exhausted shared admission must return no geometry.
	auto Tiny = MakeShared<FAnimationCaptureBudget, ESPMode::ThreadSafe>(FAnimationCaptureBudgetLimits{1, 1});
	auto TinyBudget = MakeShared<FAnimationMeshBudget, ESPMode::ThreadSafe>(Limits, Tiny);
	auto Rejected = FAnimationCaptureMeshReference::Create(Weapon, Enrollment(Weapon, TEXT("budget-control")), TinyBudget, Error);
	Attacker->TickAnimation(0, false); Attacker->RefreshBoneTransforms(); Weapon->UpdateComponentToWorld();
	TestTrue(TEXT("Budget control enrolls"), Rejected.IsValid());
	if (Rejected) { TestFalse(TEXT("Exhausted combined budget produces no geometry"), Rejected->Capture(TEXT("exhausted"), Error).IsValid()); }
	Report->SetStringField(TEXT("exhausted_budget_error"), Error);
	TestTrue(TEXT("Reference report persisted"), WriteJson(Root / TEXT("reference.json"), Report));
	Rejected.Reset(); BodySampler.Reset(); WeaponSampler.Reset();
	GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	return true;
}
