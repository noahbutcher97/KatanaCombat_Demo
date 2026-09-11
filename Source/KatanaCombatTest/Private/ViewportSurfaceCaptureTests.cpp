// Copyright Epic Games, Inc. All Rights Reserved.
#include "Analysis/ViewportSurfaceCapture.h"
#include "Misc/AutomationTest.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceLabelOwnershipTest, "KatanaCombat.Capture.Surfaces.LabelOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSurfaceLabelOwnershipTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* Actor = World->SpawnActor<AActor>();
	UStaticMeshComponent* A = NewObject<UStaticMeshComponent>(Actor); A->RegisterComponent();
	UStaticMeshComponent* B = NewObject<UStaticMeshComponent>(Actor); B->RegisterComponent();
	A->SetCustomDepthStencilValue(27); A->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_1);
	FScopedSurfaceCaptureLabels Scope; FString Error;
	TArray<FSurfaceCaptureLabel> Labels = {{5, TEXT("First"), A}, {5, TEXT("Second"), B}};
	TestFalse(TEXT("Duplicate labels rejected"), Scope.Apply(World, Labels, Error));
	TestEqual(TEXT("Failed preflight does not mutate earlier components"), A->CustomDepthStencilValue, 27);
	Labels[1].Id = 9;
	TestTrue(TEXT("Explicit arbitrary labels accepted"), Scope.Apply(World, Labels, Error));
	TestTrue(TEXT("Custom depth enabled"), bool(A->bRenderCustomDepth));
	TestEqual(TEXT("Explicit ID applied"), A->CustomDepthStencilValue, 5);
	FScopedSurfaceCaptureLabels Other;
	TestFalse(TEXT("Existing scope cannot be reapplied"), Scope.Apply(World, Labels, Error));
	TestFalse(TEXT("Another scope cannot acquire the same components"), Other.Apply(World, Labels, Error));
	TArray<FSurfaceCaptureLabel> Collision = {{5, TEXT("Another"), B}};
	TestFalse(TEXT("An outside primitive cannot alias an owned label"), Other.Apply(World, Collision, Error));
	B->DestroyComponent(); Scope.Restore(); Scope.Restore();
	TestFalse(TEXT("Original enabled flag restored"), bool(A->bRenderCustomDepth));
	TestEqual(TEXT("Original stencil restored"), A->CustomDepthStencilValue, 27);
	TestEqual(TEXT("Original write mask restored"), A->CustomDepthStencilWriteMask, ERendererStencilMask::ERSM_1);
	World->DestroyWorld(false); return true;
}

namespace
{
class FSurfaceControlCommand : public IAutomationLatentCommand
{
public:
	explicit FSurfaceControlCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	~FSurfaceControlCommand() { Cleanup(); }
	bool Update() override
	{
		if (StartWall == 0) { StartWall = FPlatformTime::Seconds(); }
		if (FPlatformTime::Seconds() - StartWall > 80) { Test->AddError(TEXT("Surface controls timed out")); Cleanup(); return true; }
		if (Done) { Cleanup(); return true; }
		if (!Reader)
		{
			UWorld* World = AutomationCommon::GetAnyGameWorld();
			if (!World || World->WorldType != EWorldType::PIE || !World->GetFirstPlayerController() || !World->GetGameViewport()) { return false; }
			Client = World->GetGameViewport(); Viewport = Client->Viewport; if (!Viewport) { return false; }
			PreviousFlags = Client->EngineShowFlags;
			PreviousViewMode = Client->ViewModeIndex;
			Client->SetViewMode(VMI_Unlit);
			Client->EngineShowFlags.SetScreenPercentage(false); Client->EngineShowFlags.SetAntiAliasing(false);
			// Uniformly visible material colour is necessary for the independent RGB
			// silhouette control; an unlit side can otherwise equal the black background.
			Client->EngineShowFlags.SetLighting(false);
			PreviousSize = Viewport->GetSizeXY(); PreviousFixedSize = Client->GetGameViewport()->HasFixedSize();
			Client->GetGameViewport()->SetFixedViewportSize(640, 480);
			CustomDepth = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth")); PreviousCustomDepth = CustomDepth->GetInt(); CustomDepth->SetWithCurrentPriority(3);
			PC = World->GetFirstPlayerController(); PreviousTarget = PC->GetViewTarget();
			Camera = World->SpawnActor<ACameraActor>(FVector(0, 0, 10000), FRotator::ZeroRotator);
			Camera->GetCameraComponent()->FieldOfView = 60; Camera->GetCameraComponent()->bConstrainAspectRatio = false; PC->SetViewTarget(Camera.Get());
			World->SpawnActor<ADirectionalLight>(FVector(0, 0, 10500), FRotator(-35, -35, 0));
			UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			for (int32 I = 0; I < 3; ++I)
			{
				AActor* Actor = World->SpawnActor<AActor>();
				UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor); Actor->SetRootComponent(Mesh);
				Mesh->SetStaticMesh(Cube); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent(); Meshes.Add(Mesh);
			}
			TArray<FSurfaceCaptureLabel> Subjects = {{7, TEXT("FirstSolid"), Meshes[0]}, {23, TEXT("SecondSolid"), Meshes[1]}};
			FString Error; if (!Labels.Apply(World, Subjects, Error)) { Test->AddError(Error); Done = true; return false; }
			Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SurfaceObservations") / FGuid::NewGuid().ToString(EGuidFormats::Digits));
			IFileManager::Get().MakeDirectory(*Directory, true);
			Reader = MakeUnique<FViewportSurfaceCapture>(World, Viewport, true);
			DrawHandle = UGameViewportClient::OnViewportRendered().AddRaw(this, &FSurfaceControlCommand::Draw);
			SetControl(); return false;
		}
		if (!Waiting && ++Warmup >= 4)
		{
			FString Error;
			if (!Reader->Request(Error)) { Test->AddError(Error); Done = true; }
			else
			{
				Waiting = true;
				Test->TestFalse(TEXT("Only one surface observation may be pending"), Reader->Request(Error));
			}
		}
		return false;
	}
private:
	void SetControl()
	{
		Meshes[0]->SetWorldLocation(FVector(400, -60, 10000));
		Meshes[1]->SetWorldLocation(FVector(400, 80, 10000));
		Meshes[2]->SetWorldLocation(FVector(200, 500, 10000));
		if (Control == 1) { Meshes[1]->SetWorldLocation(FVector(400, 40, 10000)); }
		if (Control == 2) { Meshes[1]->SetWorldLocation(FVector(425, 20, 10000)); }
		if (Control == 3) { Meshes[1]->SetWorldLocation(FVector(700, -105, 10000)); }
		if (Control == 4) { Meshes[2]->SetWorldLocation(FVector(250, 80, 10000)); Meshes[2]->SetWorldScale3D(FVector(1, 2, 2)); }
		Warmup = 0; Waiting = false;
	}
	void Draw(FViewport* Drawn)
	{
		if (!Waiting || Done || Drawn != Viewport) { return; }
		FViewportSurfaceFrame Frame; TArray<FColor> RGB; FString Error;
		if (!StaleFrameChecked)
		{
			Test->TestFalse(TEXT("Different viewport cannot collect the observation"), Reader->Collect(nullptr, GFrameCounter, Frame, RGB, Error));
			const bool Collected = Reader->Collect(Drawn, GFrameCounter + 1, Frame, RGB, Error);
			if (!Collected && Error.IsEmpty()) { return; }
			Test->TestFalse(TEXT("A mismatched frame cannot be accepted"), Collected);
			if (!Test->TestTrue(TEXT("Stale frame has an explicit diagnosis"), Error.Contains(TEXT("frame identity")))) { Done = true; return; }
			StaleFrameChecked = true; SetControl(); return;
		}
		if (!Reader->Collect(Drawn, GFrameCounter, Frame, RGB, Error))
		{
			if (!Error.IsEmpty()) { Test->AddError(Error); Done = true; }
			return;
		}
		Test->TestEqual(TEXT("RGB and surface render frame agree"), Frame.EngineFrame, GFrameCounter);
		Test->TestEqual(TEXT("Requested raster dimensions"), Frame.Size, FIntPoint(640, 480));
		const int32 Pixels = Frame.Size.X * Frame.Size.Y;
		int32 FirstPixels = 0, SecondPixels = 0, FirstDepthValid = 0;
		for (int32 I = 0; I < Pixels; ++I)
		{
			if (Frame.Labels[I] == 7) { ++FirstPixels; FirstDepthValid += FMath::IsNearlyEqual(Frame.LabelDepthCm[I], 350.0f, 1.0f); }
			SecondPixels += Frame.Labels[I] == 23;
		}
		Test->TestTrue(TEXT("First labelled solid is rasterized"), FirstPixels > 100);
		Test->TestTrue(TEXT("Known front plane has 350 cm camera-axis depth"), FirstDepthValid > 100);
		if (Control != 3) { Test->TestTrue(TEXT("Second label present in custom depth"), SecondPixels > 100); }
		else { Test->TestEqual(TEXT("A fully hidden labelled surface is absent from the frontmost label plane"), SecondPixels, 0); }
		// One binary binds RGB, labels and both depth planes to the same renderer frame.
		TArray<uint8> Bytes;
		const ANSICHAR Magic[8] = {'S','U','R','F','A','C','E','1'}; Bytes.Append(reinterpret_cast<const uint8*>(Magic), 8);
		Bytes.Append(reinterpret_cast<const uint8*>(&Frame.EngineFrame), 8);
		uint32 Width = Frame.Size.X, Height = Frame.Size.Y;
		Bytes.Append(reinterpret_cast<const uint8*>(&Width), 4); Bytes.Append(reinterpret_cast<const uint8*>(&Height), 4);
		Bytes.Append(reinterpret_cast<const uint8*>(RGB.GetData()), RGB.Num() * sizeof(FColor));
		Bytes.Append(Frame.Labels);
		Bytes.Append(reinterpret_cast<const uint8*>(Frame.SceneDepthCm.GetData()), Pixels * sizeof(float));
		Bytes.Append(reinterpret_cast<const uint8*>(Frame.LabelDepthCm.GetData()), Pixels * sizeof(float));
		const FString File = FString::Printf(TEXT("observation_%02d.surface"), Control);
		Test->TestTrue(TEXT("Bounded surface bundle saved"), FFileHelper::SaveArrayToFile(Bytes, *(Directory / File)));
		auto Row = MakeShared<FJsonObject>();
		const TCHAR* Names[] = {TEXT("Separated"), TEXT("Touching"), TEXT("Intersecting"), TEXT("DepthOccluded"), TEXT("SceneOccluded")};
		Row->SetStringField(TEXT("control"), Names[Control]); Row->SetStringField(TEXT("file"), File);
		Row->SetNumberField(TEXT("engine_frame"), double(Frame.EngineFrame));
		Row->SetNumberField(TEXT("simulation_time_s"), PC->GetWorld()->GetTimeSeconds());
		Row->SetNumberField(TEXT("width"), Width); Row->SetNumberField(TEXT("height"), Height);
		Row->SetNumberField(TEXT("readback_wall_s"), Frame.ReadbackSeconds);
		TArray<TSharedPtr<FJsonValue>> Boxes;
		for (int32 I = 0; I < 2; ++I)
		{
			const FVector Center = Meshes[I]->GetComponentLocation();
			Boxes.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
				MakeShared<FJsonValueNumber>(Center.X), MakeShared<FJsonValueNumber>(Center.Y), MakeShared<FJsonValueNumber>(Center.Z), MakeShared<FJsonValueNumber>(50)}));
		}
		Row->SetArrayField(TEXT("control_cube_centers_and_half_extent_cm"), Boxes);
		TArray<TSharedPtr<FJsonValue>> Matrix;
		for (int32 R = 0; R < 4; ++R) { for (int32 C = 0; C < 4; ++C) { Matrix.Add(MakeShared<FJsonValueNumber>(Frame.WorldToClip.M[R][C])); } }
		Row->SetArrayField(TEXT("world_to_clip_row_major"), Matrix);
		Rows.Add(MakeShared<FJsonValueObject>(Row));
		if (++Control == 5)
		{
			auto Manifest = MakeShared<FJsonObject>(); Manifest->SetNumberField(TEXT("schema_version"), 1);
			Manifest->SetStringField(TEXT("backend"), TEXT("D3D11_D32F_S8"));
			Manifest->SetStringField(TEXT("depth_convention"), TEXT("camera_axis_cm_clear_infinity"));
			Manifest->SetStringField(TEXT("label_semantics"), TEXT("frontmost_custom_depth"));
			Manifest->SetStringField(TEXT("render_policy"), TEXT("perspective_no_aa_no_screen_percentage_unlit_control"));
			auto Subjects = MakeShared<FJsonObject>(); Subjects->SetNumberField(TEXT("FirstSolid"), 7); Subjects->SetNumberField(TEXT("SecondSolid"), 23);
			Manifest->SetObjectField(TEXT("subjects"), Subjects); Manifest->SetArrayField(TEXT("frames"), Rows);
			FString Text; FJsonSerializer::Serialize(Manifest, TJsonWriterFactory<>::Create(&Text));
			Test->TestTrue(TEXT("Surface manifest saved"), FFileHelper::SaveStringToFile(Text, *(Directory / TEXT("surfaces.json"))));
			Test->AddInfo(TEXT("SURFACE_OBSERVATION_OUTPUT=") + Directory);
			Done = true;
		}
		else { SetControl(); }
	}
	void Cleanup()
	{
		UGameViewportClient::OnViewportRendered().Remove(DrawHandle); Reader.Reset(); Labels.Restore();
		if (PC.IsValid() && PreviousTarget.IsValid()) { PC->SetViewTarget(PreviousTarget.Get()); }
		if (Client.IsValid()) { Client->SetViewMode(EViewModeIndex(PreviousViewMode)); Client->EngineShowFlags = PreviousFlags; if (Client->GetGameViewport()) { Client->GetGameViewport()->SetFixedViewportSize(PreviousFixedSize ? PreviousSize.X : 0, PreviousFixedSize ? PreviousSize.Y : 0); } }
		if (CustomDepth) { CustomDepth->SetWithCurrentPriority(PreviousCustomDepth); CustomDepth = nullptr; }
		Client.Reset(); Viewport = nullptr;
	}
	FAutomationTestBase* Test;
	TWeakObjectPtr<UGameViewportClient> Client;
	FViewport* Viewport = nullptr;
	FEngineShowFlags PreviousFlags{ESFIM_Game};
	int32 PreviousViewMode = VMI_Lit;
	FIntPoint PreviousSize;
	bool PreviousFixedSize = false;
	TWeakObjectPtr<APlayerController> PC;
	TWeakObjectPtr<AActor> PreviousTarget;
	TWeakObjectPtr<ACameraActor> Camera;
	TArray<UStaticMeshComponent*> Meshes;
	TUniquePtr<FViewportSurfaceCapture> Reader;
	FScopedSurfaceCaptureLabels Labels;
	FDelegateHandle DrawHandle;
	IConsoleVariable* CustomDepth = nullptr;
	int32 PreviousCustomDepth = 0, Control = 0, Warmup = 0;
	bool Waiting = false, Done = false, StaleFrameChecked = false;
	double StartWall = 0;
	FString Directory;
	TArray<TSharedPtr<FJsonValue>> Rows;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceGeometryControlsTest, "KatanaCombat.Capture.Surfaces.RenderedGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSurfaceGeometryControlsTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddWarning(TEXT("Rendered surface controls require a rendered D3D11 run; no surface claim in NullRHI")); return true; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Engine/Maps/Entry")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FSurfaceControlCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
