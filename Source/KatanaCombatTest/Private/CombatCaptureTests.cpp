// Copyright Epic Games, Inc. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Analysis/CombatCaptureSession.h"
#include "CombatTestHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/WeaponData.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCaptureWorldIdentityTest,
	"KatanaCombat.Capture.WorldAndViewportIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCaptureWorldIdentityTest::RunTest(const FString&)
{
	class FWorldClient : public FViewportClient
	{
	public:
		explicit FWorldClient(UWorld* InWorld) : World(InWorld) {}
		virtual UWorld* GetWorld() const override { return World; }
		UWorld* World;
	};
	UWorld* PIE = NewObject<UWorld>(); PIE->WorldType = EWorldType::PIE;
	UWorld* Editor = NewObject<UWorld>(); Editor->WorldType = EWorldType::Editor;
	FWorldClient Client(PIE), Other(PIE), EditorClient(Editor);
	TestTrue(TEXT("Exact PIE client accepted"), FCombatCaptureSession::IsExpectedPIEViewportClient(PIE, &Client, &Client));
	TestFalse(TEXT("Another viewport rejected"), FCombatCaptureSession::IsExpectedPIEViewportClient(PIE, &Other, &Client));
	TestFalse(TEXT("Editor viewport rejected"), FCombatCaptureSession::IsExpectedPIEViewportClient(Editor, &EditorClient, &EditorClient));
	TestFalse(TEXT("Wrong world rejected"), FCombatCaptureSession::IsExpectedPIEViewportClient(PIE, &EditorClient, &EditorClient));
	TestFalse(TEXT("Null rejected"), FCombatCaptureSession::IsExpectedPIEViewportClient(PIE, nullptr, &Client));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCaptureLifecycleTest,
	"KatanaCombat.Capture.LifecycleAndLimits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCaptureLifecycleTest::RunTest(const FString&)
{
	FCombatCaptureSession Session, Concurrent;
	FCombatCaptureSettings Settings;
	Settings.Scenario = TEXT("CaptureLifecycle"); Settings.FrameHz = 0; Settings.MaxSamples = 2;
	FString Error;
	TestFalse(TEXT("No world rejected"), Session.Start(nullptr, Settings, {}, Error));
	// A recorder lifecycle fixture needs world ticks, but no gameplay BeginPlay.
	// Avoid starting unrelated renderer subsystems in a synthetic world after PIE.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AActor* Actor = World->SpawnActor<AActor>();
	TArray<FCombatCaptureParticipant> Participants;
	Participants.AddDefaulted_GetRef().Role = TEXT("Subject"); Participants[0].Actor = Actor;
	TestFalse(TEXT("Non-PIE world rejected"), Session.Start(World, Settings, Participants, Error));
	World->WorldType = EWorldType::PIE;
	Participants[0].Role = TEXT("../escape");
	TestFalse(TEXT("Unsafe role rejected"), Session.Start(World, Settings, Participants, Error));
	Participants[0].Role = TEXT("Subject");
	const FCombatCaptureParticipant DuplicateParticipant = Participants[0];
	Participants.Add(DuplicateParticipant);
	TestFalse(TEXT("Duplicate role rejected"), Session.Start(World, Settings, Participants, Error));
	Participants.Pop();
	IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.ActionReaction.Debug"));
	const int32 PreviousDebug = Debug->GetInt();
	TestTrue(TEXT("PIE session starts"), Session.Start(World, Settings, Participants, Error));
	TestEqual(TEXT("Telemetry enabled"), Debug->GetInt(), 1);
	TestFalse(TEXT("Concurrent recording rejected"), Concurrent.Start(World, Settings, Participants, Error));
	Session.Mark(TEXT("movement"));
	World->Tick(LEVELTICK_All, 0.02f);
	World->Tick(LEVELTICK_All, 0.02f);
	World->Tick(LEVELTICK_All, 0.02f);
	TestEqual(TEXT("Sample bound enforced"), Session.GetSampleCount(), 2);
	TestFalse(TEXT("Session auto-stopped"), Session.IsRecording());
	TestEqual(TEXT("Telemetry restored"), Debug->GetInt(), PreviousDebug);
	TestTrue(TEXT("Stop is idempotent after success"), Session.Stop(TEXT("repeat"), Error));
	const FString FirstDirectory = Session.GetOutputDirectory();
	FString Json;
	TestTrue(TEXT("Manifest persisted"), FFileHelper::LoadFileToString(Json, *(FirstDirectory / TEXT("session.json"))));
	TSharedPtr<FJsonObject> Manifest;
	TestTrue(TEXT("Manifest parse"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Manifest));
	if (Manifest)
	{
		TestEqual(TEXT("Finalized status"), Manifest->GetStringField(TEXT("status")), FString(TEXT("complete")));
		TestEqual(TEXT("Explicit limit reason"), Manifest->GetStringField(TEXT("stop_reason")), FString(TEXT("sample_limit_reached")));
	}
	TestTrue(TEXT("Session can restart"), Session.Start(World, Settings, Participants, Error));
	TestNotEqual(TEXT("Recordings never overwrite one another"), Session.GetOutputDirectory(), FirstDirectory);
	FCombatTestHelpers::DestroyTestWorld(World);
	TestFalse(TEXT("World teardown finalizes session"), Session.IsRecording());
	TestTrue(TEXT("World teardown export succeeded"), Session.Stop(TEXT("repeat"), Error));
	TestEqual(TEXT("Teardown restored telemetry"), Debug->GetInt(), PreviousDebug);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCaptureFailurePathsTest,
	"KatanaCombat.Capture.FailurePaths", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCaptureFailurePathsTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->WorldType = EWorldType::PIE;
	AActor* Actor = World->SpawnActor<AActor>();
	TArray<FCombatCaptureParticipant> Participants;
	Participants.AddDefaulted_GetRef().Role = TEXT("Subject"); Participants[0].Actor = Actor;
	FCombatCaptureSettings Settings; Settings.FrameHz = 0; Settings.MaxWallSeconds = 0.01;
	FCombatCaptureSession Capture; FString Error;
	TestTrue(TEXT("Watchdog session starts"), Capture.Start(World, Settings, Participants, Error));
	// Deliberately no world ticks: the independent core ticker must still stop it.
	FPlatformProcess::Sleep(0.02f);
	FTSTicker::GetCoreTicker().Tick(0.02f);
	TestFalse(TEXT("No-world-tick timeout stops recording"), Capture.IsRecording());
	TestEqual(TEXT("Wall timeout is explicit"), Capture.GetStopReason(), FString(TEXT("wall_time_limit_reached")));
	Settings.MaxWallSeconds = 30; Settings.MaxDataBytes = 1;
	TestTrue(TEXT("Budget test starts with diagnostic manifest"), Capture.Start(World, Settings, Participants, Error));
	FTSTicker::GetCoreTicker().Tick(0.02f);
	TestFalse(TEXT("Budget failure stops recording"), Capture.IsRecording());
	TestFalse(TEXT("Budget-exhausted evidence cannot succeed"), Capture.Stop(TEXT("repeat"), Error));
	TestFalse(TEXT("Budget failure explains itself"), Error.IsEmpty());
	Settings.MaxDataBytes = 512ll * 1024 * 1024;
	TestTrue(TEXT("Participant-loss test starts"), Capture.Start(World, Settings, Participants, Error));
	Actor->Destroy();
	World->Tick(LEVELTICK_All, 0.02f);
	TestTrue(TEXT("Missing actor remains an exportable observation"), Capture.Stop(TEXT("participant_destroyed"), Error));
	FString Samples;
	FFileHelper::LoadFileToString(Samples, *(Capture.GetOutputDirectory() / TEXT("samples.jsonl")));
	TestTrue(TEXT("Missing actor is recorded as invalid"), Samples.Contains(TEXT("\"valid\":false")));
	Participants[0].Actor = World->SpawnActor<AActor>();
	TestTrue(TEXT("Write-failure test starts"), Capture.Start(World, Settings, Participants, Error));
	// Only the newly created recording's manifest is replaced with an empty directory.
	const FString Manifest = Capture.GetOutputDirectory() / TEXT("session.json");
	TestTrue(TEXT("Remove this fixture's initial manifest"), IFileManager::Get().Delete(*Manifest));
	TestTrue(TEXT("Create a deterministic manifest write obstruction"), IFileManager::Get().MakeDirectory(*Manifest));
	TestFalse(TEXT("Final manifest failure cannot produce success"), Capture.Stop(TEXT("write_failure"), Error));
	TestTrue(TEXT("Write failure is diagnosed"), Error.Contains(TEXT("manifest")));
	IFileManager::Get().DeleteDirectory(*Manifest, false, false);
	// Mesh enrollment is fixed: destruction must not silently switch to a replacement.
	AActor* ReplacementOwner = Participants[0].Actor.Get();
	USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(ReplacementOwner);
	Mesh->RegisterComponent(); Participants[0].Mesh = Mesh;
	TestTrue(TEXT("Mesh replacement observation starts"), Capture.Start(World, Settings, Participants, Error));
	Mesh->DestroyComponent();
	USkeletalMeshComponent* ReplacementMesh = NewObject<USkeletalMeshComponent>(ReplacementOwner);
	ReplacementMesh->RegisterComponent();
	World->Tick(LEVELTICK_All, 0.02f);
	TestTrue(TEXT("Destroyed mesh remains diagnosable"), Capture.Stop(TEXT("mesh_replaced"), Error));
	FFileHelper::LoadFileToString(Samples, *(Capture.GetOutputDirectory() / TEXT("samples.jsonl")));
	TestTrue(TEXT("Missing mesh point is null"), Samples.Contains(TEXT("\"pelvis\":null")));
	Participants[0].Mesh.Reset();
	UCombatComponent* Combat = NewObject<UCombatComponent>(ReplacementOwner);
	Combat->RegisterComponent();
	Settings.MaxTelemetryRecordsPerActor = 2;
	TestTrue(TEXT("Telemetry discontinuity observation starts"), Capture.Start(World, Settings, Participants, Error));
	FActionReactionTelemetryRecord Record; Record.Event = EActionReactionTelemetryEvent::InputCaptured;
	Combat->AppendActionReactionTelemetry(Record);
	World->Tick(LEVELTICK_All, 0.02f);
	Combat->ClearActionReactionTelemetry();
	Combat->AppendActionReactionTelemetry(Record);
	World->Tick(LEVELTICK_All, 0.02f);
	for (int32 I = 0; I < 3; ++I) { Combat->AppendActionReactionTelemetry(Record); }
	World->Tick(LEVELTICK_All, 0.02f);
	TestTrue(TEXT("Loss counts can be exported"), Capture.Stop(TEXT("telemetry_discontinuity"), Error));
	FString TelemetryManifest; TSharedPtr<FJsonObject> TelemetryJson;
	FFileHelper::LoadFileToString(TelemetryManifest, *(Capture.GetOutputDirectory() / TEXT("session.json")));
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TelemetryManifest), TelemetryJson);
	if (TestTrue(TEXT("Telemetry manifest parses"), TelemetryJson.IsValid()))
	{
		const auto Role = TelemetryJson->GetArrayField(TEXT("participants"))[0]->AsObject();
		TestEqual(TEXT("Reset is explicit"), Role->GetIntegerField(TEXT("telemetry_resets")), 1);
		TestEqual(TEXT("Overflow is explicit"), Role->GetIntegerField(TEXT("telemetry_lost_records")), 3);
		TestEqual(TEXT("Export bound holds"), Role->GetIntegerField(TEXT("action_records")), 2);
	}
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCapturePointSourceTest,
	"KatanaCombat.Capture.NominatedWeaponPointSource", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatCapturePointSourceTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->WorldType = EWorldType::PIE;
	AActor* Owner = World->SpawnActor<AActor>(); AActor* Other = World->SpawnActor<AActor>();
	USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Owner); Owner->SetRootComponent(Mesh); Mesh->RegisterComponent();
	UWeaponData* Weapon = LoadObject<UWeaponData>(nullptr, TEXT("/Game/ProjectFiles/Data/PDA/Weapons/DA_Weapon_Katana.DA_Weapon_Katana"));
	if (!TestNotNull(TEXT("Existing katana data available"), Weapon)) { FCombatTestHelpers::DestroyTestWorld(World); return false; }
	UStaticMeshComponent* Source = NewObject<UStaticMeshComponent>(Owner); Source->SetStaticMesh(Weapon->WeaponMesh.LoadSynchronous()); Source->RegisterComponent();
	Source->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform);
	FCombatCaptureParticipant Participant; Participant.Role = TEXT("Subject"); Participant.Actor = Owner; Participant.Mesh = Mesh;
	Participant.Points = {Weapon->TraceStartSocket}; Participant.PointSources.Add(Weapon->TraceStartSocket, Source);
	FCombatCaptureSettings Settings; Settings.FrameHz = 0;
	FCombatCaptureSession Capture; FString Error;
	UStaticMeshComponent* Foreign = NewObject<UStaticMeshComponent>(Other); Foreign->RegisterComponent();
	Participant.PointSources[Weapon->TraceStartSocket] = Foreign;
	TestFalse(TEXT("Another actor cannot supply this participant's point"), Capture.Start(World, Settings, {Participant}, Error));
	Participant.PointSources[Weapon->TraceStartSocket] = Source;
	if (!TestTrue(TEXT("Exact owned weapon source starts"), Capture.Start(World, Settings, {Participant}, Error))) { FCombatTestHelpers::DestroyTestWorld(World); return false; }
	World->Tick(LEVELTICK_All, .02f);
	const FString SourceName = Source->GetPathName(); Source->DestroyComponent();
	auto* Replacement = NewObject<UStaticMeshComponent>(Owner); Replacement->SetStaticMesh(Weapon->WeaponMesh.Get()); Replacement->RegisterComponent();
	World->Tick(LEVELTICK_All, .02f);
	TestTrue(TEXT("Missing source stays exportable"), Capture.Stop(TEXT("point_source_destroyed"), Error));
	TArray<FString> Lines; FFileHelper::LoadFileToStringArray(Lines, *(Capture.GetOutputDirectory() / TEXT("samples.jsonl")));
	if (TestEqual(TEXT("Two sampled observations"), Lines.Num(), 2))
	{
		TSharedPtr<FJsonObject> First, Last;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Lines[0]), First); FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Lines[1]), Last);
		const auto Point = First->GetArrayField(TEXT("actors"))[0]->AsObject()->GetObjectField(TEXT("points"))->GetObjectField(Weapon->TraceStartSocket.ToString());
		TestEqual(TEXT("Source identity is explicit"), Point->GetStringField(TEXT("source_component")), SourceName);
		TestEqual(TEXT("Weapon asset identity is explicit"), Point->GetStringField(TEXT("source_asset")), Weapon->WeaponMesh->GetPathName());
		TestTrue(TEXT("Destroyed nominated source remains null despite replacement"), Last->GetArrayField(TEXT("actors"))[0]->AsObject()->GetObjectField(TEXT("points"))->Values[Weapon->TraceStartSocket.ToString()]->IsNull());
	}
	FCombatTestHelpers::DestroyTestWorld(World); return true;
}

namespace
{
class FCombatObservationCommand : public IAutomationLatentCommand
{
public:
	FCombatObservationCommand(FAutomationTestBase* InTest, bool bInConsole)
		: Test(InTest), bConsole(bInConsole) {}
	virtual bool Update() override
	{
		if (StartWall == 0) { StartWall = FPlatformTime::Seconds(); }
		if (FPlatformTime::Seconds() - StartWall > 75)
		{
			Test->AddError(TEXT("Combat capture observation exceeded watchdog"));
			FString Unused; if (Capture) { Capture->Stop(TEXT("test_watchdog"), Unused); }
			return true;
		}
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!World || World->WorldType != EWorldType::PIE) { return false; }
		if (bVerifyingImageFailure)
		{
			if (OwnedCapture.IsRecording()) { return false; }
			FString Error;
			Test->TestFalse(TEXT("PNG write failure cannot finalize a successful capture"), OwnedCapture.Stop(TEXT("repeat"), Error));
			Test->TestEqual(TEXT("PNG failure stops the recorder explicitly"), OwnedCapture.GetStopReason(), FString(TEXT("image_write_failed")));
			Test->TestEqual(TEXT("Failed PNG is not counted as a captured frame"), OwnedCapture.GetFrameCount(), 0);
			FString Text; TSharedPtr<FJsonObject> Manifest;
			FFileHelper::LoadFileToString(Text, *(OwnedCapture.GetOutputDirectory() / TEXT("session.json")));
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Manifest);
			if (Test->TestTrue(TEXT("Failed image capture manifest is readable"), Manifest.IsValid()))
			{
				Test->TestEqual(TEXT("Incomplete evidence is marked error"), Manifest->GetStringField(TEXT("status")), FString(TEXT("error")));
				Test->TestTrue(TEXT("Failed image count is explicit"), Manifest->GetIntegerField(TEXT("image_failed_frames")) > 0);
				Test->TestEqual(TEXT("Failure still drains every submitted task"), Manifest->GetIntegerField(TEXT("image_pending_frames")), 0);
			}
			return true;
		}
		if (bVerifyingFrameLimit)
		{
			if (OwnedCapture.IsRecording()) { return false; }
			Test->TestEqual(TEXT("Frame budget stops at requested count"), OwnedCapture.GetFrameCount(), 1);
			Test->TestEqual(TEXT("Frame limit has its own reason"), OwnedCapture.GetStopReason(), FString(TEXT("frame_limit_reached")));
			Test->TestTrue(TEXT("Final bounded PNG exists before stop returns"), FPaths::FileExists(OwnedCapture.GetOutputDirectory() / TEXT("frames/frame_000001.png")));
			FString Error; FCombatCaptureSettings Settings; Settings.FrameHz = 5;
			bVerifyingImageFailure = OwnedCapture.Start(World, Settings, FCombatCaptureSession::DiscoverParticipants(World), Error);
			Test->TestTrue(TEXT("Image failure session starts"), bVerifyingImageFailure);
			if (bVerifyingImageFailure)
			{
				Test->TestTrue(TEXT("Obstruct only this fixture's first PNG"), IFileManager::Get().MakeDirectory(*(OwnedCapture.GetOutputDirectory() / TEXT("frames/frame_000001.png"))));
			}
			return !bVerifyingImageFailure;
		}
		if (!Capture)
		{
			auto Participants = FCombatCaptureSession::DiscoverParticipants(World);
			if (Participants.IsEmpty()) { return false; }
			FString Error;
			Scenario = bConsole ? TEXT("ThirdPersonObservation") : TEXT("DefenseMatrixObservation");
			if (bConsole)
			{
				Test->TestTrue(TEXT("Ordinary PIE console starts capture"), IConsoleManager::Get().ProcessUserConsoleInput(
					TEXT("Combat.Capture.Start ThirdPersonObservation 60 5 60"), *GLog, World));
				Capture = CombatCaptureCommands::GetSession();
			}
			else
			{
				FCombatCaptureSettings Settings; Settings.Scenario = Scenario;
				Test->TestTrue(TEXT("C++ scenario starts same recorder"), OwnedCapture.Start(World, Settings, Participants, Error));
				Capture = &OwnedCapture;
			}
			if (!Capture || !Capture->IsRecording()) { Test->AddError(TEXT("Capture session failed to start: ") + Error); return true; }
			Capture->Mark(TEXT("observation_started"));
			StartSimulation = World->GetTimeSeconds();
			return false;
		}
		const double Elapsed = World->GetTimeSeconds() - StartSimulation;
		APlayerController* PC = World->GetFirstPlayerController();
		UCombatComponent* Combat = PC && PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UCombatComponent>() : nullptr;
		if (!bInputSubmitted && Elapsed >= 1.0)
		{
			bInputSubmitted = true;
			Test->TestNotNull(TEXT("Playable combat participant"), Combat);
			Capture->Mark(TEXT("scripted_light_attack_press"));
			if (Combat) { Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press); }
		}
		if (bInputSubmitted && !bInputReleased && Elapsed >= 1.1)
		{
			bInputReleased = true;
			Capture->Mark(TEXT("scripted_light_attack_release"));
			if (Combat) { Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Release); }
		}
		if (Elapsed < 3.0 || (FApp::CanEverRender() && Capture->GetFrameCount() < 3)) { return false; }
		FString Error;
		if (bConsole)
		{
			IConsoleManager::Get().ProcessUserConsoleInput(TEXT("Combat.Capture.Mark observation_finished"), *GLog, World);
			IConsoleManager::Get().ProcessUserConsoleInput(TEXT("Combat.Capture.Stop"), *GLog, World);
		}
		else { Capture->Mark(TEXT("observation_finished")); }
		Test->TestTrue(TEXT("Capture exports successfully"), Capture->Stop(TEXT("scenario_finished"), Error));
		Test->TestTrue(TEXT("Motion samples recorded"), Capture->GetSampleCount() >= 10);
		if (FApp::CanEverRender()) { Test->TestTrue(TEXT("Actual PIE frames recorded"), Capture->GetFrameCount() >= 3); }
		else { Test->TestEqual(TEXT("Headless mode makes no image claim"), Capture->GetFrameCount(), 0); }
		const FString Directory = Capture->GetOutputDirectory();
		Test->TestTrue(TEXT("Session exists"), FPaths::FileExists(Directory / TEXT("session.json")));
		if (FApp::CanEverRender())
		{
			TArray<FString> Lines; FFileHelper::LoadFileToStringArray(Lines, *(Directory / TEXT("frames.jsonl")));
			Test->TestEqual(TEXT("Every successful image has a finalized frame row"), Lines.Num(), Capture->GetFrameCount());
			for (int32 Index = 0; Index < Lines.Num(); ++Index)
			{
				TSharedPtr<FJsonObject> Frame; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Lines[Index]), Frame);
				if (!Test->TestTrue(TEXT("Frame row parses"), Frame.IsValid())) { continue; }
				Test->TestEqual(TEXT("Frame order survives asynchronous completion"), Frame->GetIntegerField(TEXT("index")), Index + 1);
				Test->TestTrue(TEXT("Original observation predates image collection"), Frame->GetNumberField(TEXT("wall_elapsed_s")) <= Frame->GetNumberField(TEXT("image_collected_wall_elapsed_s")));
				Test->TestTrue(TEXT("Frame references a persisted PNG"), FPaths::FileExists(Directory / Frame->GetStringField(TEXT("file"))));
			}
		}
		FFileHelper::SaveStringToFile(Directory, *(FPaths::ProjectSavedDir() / TEXT("CombatCaptures") / (Scenario + TEXT("-latest.txt"))));
		Test->AddInfo(FString::Printf(TEXT("Reusable combat capture: %s"), *Directory));
		if (FApp::CanEverRender())
		{
			FCombatCaptureSettings LimitSettings; LimitSettings.Scenario = TEXT("RenderedFrameLimit");
			LimitSettings.FrameHz = 60; LimitSettings.MaxFrames = 1; LimitSettings.MaxWallSeconds = 10;
			bVerifyingFrameLimit = OwnedCapture.Start(World, LimitSettings, FCombatCaptureSession::DiscoverParticipants(World), Error);
			Test->TestTrue(TEXT("Frame budget session starts"), bVerifyingFrameLimit);
			return !bVerifyingFrameLimit;
		}
		return true;
	}
private:
	FAutomationTestBase* Test;
	bool bConsole;
	bool bInputSubmitted = false, bInputReleased = false;
	bool bVerifyingFrameLimit = false;
	bool bVerifyingImageFailure = false;
	FCombatCaptureSession OwnedCapture;
	FCombatCaptureSession* Capture = nullptr;
	double StartWall = 0, StartSimulation = 0;
	FString Scenario;
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCombatCapturePIEObservationTest,
	"KatanaCombat.Capture.PIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FCombatCapturePIEObservationTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	Names.Add(TEXT("ThirdPersonConsole")); Commands.Add(TEXT("ThirdPerson"));
	Names.Add(TEXT("DefenseMatrixAPI")); Commands.Add(TEXT("DefenseMatrix"));
}

bool FCombatCapturePIEObservationTest::RunTest(const FString& Parameters)
{
	const bool bConsole = Parameters == TEXT("ThirdPerson");
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(bConsole ? TEXT("/Game/ProjectFiles/Levels/Lvl_ThirdPerson1") : TEXT("/Game/ProjectFiles/Levels/Test/Lvl_DefenseMatrix")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FCombatObservationCommand(this, bConsole));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	return true;
}
