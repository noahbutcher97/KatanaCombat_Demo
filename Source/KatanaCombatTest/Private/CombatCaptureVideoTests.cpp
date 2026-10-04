// Copyright Epic Games, Inc. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Analysis/CombatCaptureSession.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Animation/AnimInstance.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StateTreeComponent.h"
#include "Core/CombatComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PresentationCapture/PresentationRecording.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
TSet<FString> CaptureBundles()
{
	TArray<FString> Names;
	IFileManager::Get().FindFiles(Names, *(FPaths::ProjectSavedDir() / TEXT("CombatCaptures") / TEXT("*")), false, true);
	return TSet<FString>(Names);
}

TSharedPtr<FJsonObject> ReadJsonFile(const FString& Path)
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (FFileHelper::LoadFileToString(Text, *Path)) { FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root); }
	return Root;
}

/**
 * The human path: ordinary PIE in the level viewport, the ordinary console, and
 * `Combat.Capture.Start ... Video=1`. The same command must yield linked video and data, every
 * video frame must join a motion sample by engine frame, and the landed character hits must be
 * marked. As in the test captures recorded on 2026-10-03, one enemy becomes a training dummy in front of the player.
 */
class FConsoleVideoCaptureCommand : public IAutomationLatentCommand
{
public:
	explicit FConsoleVideoCaptureCommand(FAutomationTestBase* InTest) : Test(InTest) {}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartWall == 0) { StartWall = Now; }
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (Now - StartWall > 120)
		{
			Test->AddError(TEXT("Console video capture exceeded its watchdog"));
			if (World && Capture && Capture->IsRecording()) { Console(World, TEXT("Combat.Capture.Stop")); }
			return true;
		}
		if (!World || World->WorldType != EWorldType::PIE) { return false; }
		switch (Stage)
		{
		case EStage::Start: return Start(World);
		case EStage::Act: return Act(World);
		case EStage::Finalize: return Finalize(Now);
		}
		return true;
	}

private:
	enum class EStage { Start, Act, Finalize };

	static void Console(UWorld* World, const TCHAR* Command)
	{
		IConsoleManager::Get().ProcessUserConsoleInput(Command, *GLog, World);
	}

	/** Fixture setup before recording: stop every enemy's StateTree, put the first enemy 150 cm in
	 * front of the player facing it, and move the others away. Returns the dummy's console role. */
	bool PrepareTrainingDummy(UWorld* World)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		APlayerCharacter* Player = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
		TArray<AEnemyCharacter*> Enemies;
		for (TActorIterator<AEnemyCharacter> It(World); It; ++It) { Enemies.Add(*It); }
		Enemies.Sort([](const AEnemyCharacter& A, const AEnemyCharacter& B) { return A.GetPathName() < B.GetPathName(); });
		if (!Player || Enemies.IsEmpty()) { return false; }
		const FVector Base = Player->GetActorLocation();
		Player->SetActorRotation(FRotator::ZeroRotator);
		PC->SetControlRotation(FRotator(-10.0f, 0.0f, 0.0f));
		for (int32 Index = 0; Index < Enemies.Num(); ++Index)
		{
			AEnemyCharacter* Enemy = Enemies[Index];
			if (AController* Controller = Enemy->GetController())
			{
				if (UStateTreeComponent* Tree = Controller->FindComponentByClass<UStateTreeComponent>()) { Tree->StopLogic(TEXT("ConsoleVideoTrainingDummy")); }
			}
			Enemy->CombatAIComponent->AbortAttack();
			Enemy->CombatComponent->ClearQueue(true);
			if (UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance()) { Anim->StopAllMontages(0.0f); }
			const FVector Offset = Index == 0 ? FVector(150.0f, 0.0f, 0.0f) : FVector(-600.0f - 200.0f * Index, 1500.0f + 300.0f * Index, 0.0f);
			Enemy->SetActorLocationAndRotation(Base + Offset, FRotator(0.0f, 180.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		Dummy = Enemies[0];
		for (const FCombatCaptureParticipant& Participant : FCombatCaptureSession::DiscoverParticipants(World))
		{
			if (Participant.Actor.Get() == Player) { PlayerRole = Participant.Role; }
			if (Participant.Actor.Get() == Dummy.Get()) { DummyRole = Participant.Role; }
		}
		DummyHealth = Dummy->CurrentHealth;
		return !PlayerRole.IsEmpty() && !DummyRole.IsEmpty();
	}

	bool Start(UWorld* World)
	{
		if (FCombatCaptureSession::DiscoverParticipants(World).IsEmpty()) { return false; }
		if (FApp::CanEverRender() && !Test->TestTrue(TEXT("A training dummy stands in front of the player"), PrepareTrainingDummy(World)))
		{
			return true;
		}
		const TSet<FString> BundlesBefore = CaptureBundles();
		Console(World, TEXT("Combat.Capture.Start ConsoleVideo 20 0 60 Video=1 VideoFPS=60 VideoResolution=720"));
		Capture = CombatCaptureCommands::GetSession();
		if (!FApp::CanEverRender())
		{
			// The refusal happens before a bundle is created, so no silent data-only capture remains.
			Test->TestFalse(TEXT("Video is refused without a rendering editor"), Capture && Capture->IsRecording());
			Test->TestTrue(TEXT("No capture bundle is created for a refused video request"),
				CaptureBundles().Difference(BundlesBefore).IsEmpty());
			return true;
		}
		if (!Test->TestTrue(TEXT("Console capture with video records"), Capture && Capture->IsRecording() && Capture->HasVideo()))
		{
			return true;
		}
		StartSimulation = World->GetTimeSeconds();
		Stage = EStage::Act;
		return false;
	}

	bool Act(UWorld* World)
	{
		const double Elapsed = World->GetTimeSeconds() - StartSimulation;
		APlayerController* PC = World->GetFirstPlayerController();
		UCombatComponent* Combat = PC && PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UCombatComponent>() : nullptr;
		static const double Presses[] = {1.0, 2.0};
		if (Combat && NextPress < static_cast<int32>(UE_ARRAY_COUNT(Presses)))
		{
			if (!bPressed && Elapsed >= Presses[NextPress])
			{
				Console(World, TEXT("Combat.Capture.Mark scripted_light_attack_press"));
				Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
				bPressed = true;
			}
			else if (bPressed && Elapsed >= Presses[NextPress] + 0.1)
			{
				Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Release);
				bPressed = false;
				++NextPress;
			}
		}
		if (Elapsed < 3.0) { return false; }
		Console(World, TEXT("Combat.Capture.Mark console_video_finished"));
		Console(World, TEXT("Combat.Capture.Stop"));
		Test->TestFalse(TEXT("Console stop finalizes the data session"), Capture->IsRecording());
		StopWall = FPlatformTime::Seconds();
		Stage = EStage::Finalize;
		return false;
	}

	bool Finalize(double Now)
	{
		// Video stops asynchronously; the encoder finalizes the MP4 on later ticks.
		if (Capture->IsVideoFinalizing() && Now - StopWall < 45) { return false; }
		Test->TestFalse(TEXT("The clip finalized after stop"), Capture->IsVideoFinalizing());
		Verify();
		return true;
	}

	void Verify()
	{
		const FString Bundle = Capture->GetOutputDirectory();
		const FString VideoDirectory = Capture->GetVideoDirectory();
		const TSharedPtr<FJsonObject> Link = ReadJsonFile(Capture->GetLinkPath());
		if (!Test->TestTrue(TEXT("capture-link.json is written"), Link.IsValid())) { return; }
		const TSharedPtr<FJsonObject> Video = Link->GetObjectField(TEXT("video"));
		const TSharedPtr<FJsonObject> Analysis = Link->GetObjectField(TEXT("analysis"));
		Test->TestEqual(TEXT("Link is final"), Link->GetStringField(TEXT("status")), FString(TEXT("stopped")));
		Test->TestEqual(TEXT("Link names this clip"), Video->GetStringField(TEXT("capture_id")), FPaths::GetCleanFilename(VideoDirectory));
		Test->TestTrue(TEXT("Clip lives inside the bundle"), FPaths::FileExists(Bundle / Video->GetStringField(TEXT("directory")) / TEXT("video-manifest.json")));
		const double AnalysisStartFrame = Analysis->GetObjectField(TEXT("start"))->GetNumberField(TEXT("engine_frame"));
		Test->TestTrue(TEXT("Analysis start has an engine frame"), AnalysisStartFrame > 0);
		Test->TestTrue(TEXT("Video starts after the data session"), Video->GetObjectField(TEXT("start"))->GetNumberField(TEXT("engine_frame")) >= AnalysisStartFrame);
		Test->TestTrue(TEXT("Analysis start has a platform time"), Analysis->GetObjectField(TEXT("start"))->GetNumberField(TEXT("platform_seconds")) > 0);

		const TSharedPtr<FJsonObject> Manifest = ReadJsonFile(VideoDirectory / TEXT("video-manifest.json"));
		if (!Test->TestTrue(TEXT("video-manifest.json is written"), Manifest.IsValid())) { return; }
		Test->TestTrue(TEXT("Recorder reports a complete clip"), Manifest->GetBoolField(TEXT("complete")));
		const TArray<TSharedPtr<FJsonValue>>& Windows = Manifest->GetArrayField(TEXT("windows"));
		if (!Test->TestEqual(TEXT("One PIE seat was recorded"), Windows.Num(), 1)) { return; }
		const TSharedPtr<FJsonObject> Seat = Windows[0]->AsObject();
		Test->TestTrue(TEXT("Frames were encoded"), Seat->GetIntegerField(TEXT("encodedFrames")) > 0);
		Test->TestTrue(TEXT("MP4 exists"), IFileManager::Get().FileSize(*(VideoDirectory / Seat->GetStringField(TEXT("output")))) > 0);

		TArray<FString> Rows;
		FFileHelper::LoadFileToStringArray(Rows, *(VideoDirectory / Seat->GetStringField(TEXT("timestamps"))));
		TArray<FString> Header;
		if (Rows.IsEmpty()) { Test->AddError(TEXT("Video frames CSV is empty")); return; }
		Rows[0].ParseIntoArray(Header, TEXT(","));
		const int32 DrawColumn = Header.IndexOfByKey(TEXT("drawGameFrame"));
		if (!Test->TestTrue(TEXT("Frames CSV has drawGameFrame"), DrawColumn != INDEX_NONE)) { return; }
		TSet<int64> SampleFrames;
		TArray<FString> Samples;
		FFileHelper::LoadFileToStringArray(Samples, *(Bundle / TEXT("samples.jsonl")));
		int64 FirstSample = MAX_int64, LastSample = 0;
		for (const FString& Line : Samples)
		{
			TSharedPtr<FJsonObject> Sample;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Sample) || !Sample.IsValid()) { continue; }
			const int64 Frame = static_cast<int64>(Sample->GetNumberField(TEXT("engine_frame")));
			SampleFrames.Add(Frame);
			FirstSample = FMath::Min(FirstSample, Frame);
			LastSample = FMath::Max(LastSample, Frame);
		}
		int32 VideoFrames = 0, Inside = 0, Joined = 0;
		for (int32 Index = 1; Index < Rows.Num(); ++Index)
		{
			TArray<FString> Cells;
			Rows[Index].ParseIntoArray(Cells, TEXT(","), false);
			if (!Cells.IsValidIndex(DrawColumn)) { continue; }
			const int64 Frame = FCString::Atoi64(*Cells[DrawColumn]);
			++VideoFrames;
			Inside += Frame >= FirstSample && Frame <= LastSample;
			Joined += SampleFrames.Contains(Frame);
		}
		Test->TestEqual(TEXT("Every encoded frame has a CSV row"), VideoFrames, static_cast<int32>(Seat->GetIntegerField(TEXT("encodedFrames"))));
		Test->TestEqual(TEXT("Every video frame lies inside the sampled interval"), Inside, VideoFrames);
		Test->TestEqual(TEXT("Every video frame joins a motion sample by engine frame"), Joined, VideoFrames);

		FString Markers;
		FFileHelper::LoadFileToString(Markers, *(Bundle / TEXT("markers.jsonl")));
		Test->TestTrue(TEXT("Video start is marked in the data session"), Markers.Contains(TEXT("\"marker\":\"video_started\"")));
		Test->TestTrue(TEXT("Video stop is marked in the data session"), Markers.Contains(TEXT("\"marker\":\"video_stop_requested\"")));
		TArray<FString> MarkerLines;
		Markers.ParseIntoArrayLines(MarkerLines);
		int32 HitMarkers = 0;
		for (const FString& Line : MarkerLines)
		{
			TSharedPtr<FJsonObject> Row;
			const TSharedPtr<FJsonObject>* Payload = nullptr;
			if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Row) && Row.IsValid()
				&& Row->GetStringField(TEXT("marker")) == TEXT("contact") && Row->TryGetObjectField(TEXT("payload"), Payload)
				&& (*Payload)->GetStringField(TEXT("outcome")) == TEXT("Hit")
				&& (*Payload)->GetStringField(TEXT("attacker")) == PlayerRole && (*Payload)->GetStringField(TEXT("victim")) == DummyRole)
			{
				++HitMarkers;
			}
		}
		Test->TestTrue(TEXT("The scripted attacks landed on the dummy"), Dummy.IsValid() && Dummy->CurrentHealth < DummyHealth);
		Test->TestTrue(TEXT("Every landed character hit is a contact marker with its outcome"), HitMarkers > 0);
		FFileHelper::SaveStringToFile(Bundle, *(FPaths::ProjectSavedDir() / TEXT("CombatCaptures") / TEXT("ConsoleVideo-latest.txt")));
		Test->AddInfo(FString::Printf(TEXT("Console video capture %s: %d video frames, %d joined by engine frame, %d hit marker(s) %s->%s, viewport %s"),
			*Bundle, VideoFrames, Joined, HitMarkers, *PlayerRole, *DummyRole, *FString::Printf(TEXT("%dx%d"), static_cast<int32>(Seat->GetIntegerField(TEXT("viewportWidth"))), static_cast<int32>(Seat->GetIntegerField(TEXT("viewportHeight"))))));
	}

	FAutomationTestBase* Test;
	FCombatCaptureSession* Capture = nullptr;
	TWeakObjectPtr<AEnemyCharacter> Dummy;
	FString PlayerRole, DummyRole;
	float DummyHealth = 0;
	EStage Stage = EStage::Start;
	double StartWall = 0, StartSimulation = 0, StopWall = 0;
	int32 NextPress = 0;
	bool bPressed = false;
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCaptureConsoleVideoTest,
	"KatanaCombat.Capture.Video.ConsolePIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCaptureConsoleVideoTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender())
	{
		// Headless coverage: the request is refused with an actionable reason and records nothing.
		AddExpectedError(TEXT("Video capture needs a rendering editor"), EAutomationExpectedErrorFlags::Contains, 1);
	}
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/ProjectFiles/Levels/Lvl_ThirdPerson1")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FConsoleVideoCaptureCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	return true;
}

namespace
{
/**
 * Video that ends without Combat.Capture.Stop: PIE ending (the ordinary human path) and the data
 * session stopping itself at a limit. Each must stop the clip, finalize capture-link.json with the
 * reason and release the recorder channel, so a later recording never claims this bundle.
 */
class FVideoTeardownCommand : public IAutomationLatentCommand
{
public:
	FVideoTeardownCommand(FAutomationTestBase* InTest, bool bInPIEEnd) : Test(InTest), bPIEEnd(bInPIEEnd) {}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartWall == 0) { StartWall = Now; }
		if (Now - StartWall > 90) { Test->AddError(TEXT("Video teardown exceeded its watchdog")); return true; }
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!Capture)
		{
			if (!World || World->WorldType != EWorldType::PIE || FCombatCaptureSession::DiscoverParticipants(World).IsEmpty()) { return false; }
			// The session limit is 2 wall seconds; the clip's own bound is 10, so the data session ends first.
			IConsoleManager::Get().ProcessUserConsoleInput(bPIEEnd ? TEXT("Combat.Capture.Start VideoPIEEnd 20 0 60 Video=1")
				: TEXT("Combat.Capture.Start VideoSessionLimit 2 0 60 Video=1 VideoSeconds=10"), *GLog, World);
			Capture = CombatCaptureCommands::GetSession();
			if (!Test->TestTrue(TEXT("Console capture with video records"), Capture && Capture->IsRecording() && Capture->HasVideo())) { return true; }
			StartSimulation = World->GetTimeSeconds();
			return false;
		}
		if (bPIEEnd)
		{
			// End PIE without Combat.Capture.Stop; the next latent command does it.
			return World && World->GetTimeSeconds() - StartSimulation >= 1.5;
		}
		if (Capture->IsRecording() || Capture->IsVideoFinalizing()) { return false; }
		VerifyVideoTeardown(Test, Capture, TEXT("stopped_by_analysis_session"), TEXT("analysis_session_stopped:wall_time_limit_reached"));
		return true;
	}

	static void VerifyVideoTeardown(FAutomationTestBase* Test, FCombatCaptureSession* Capture, const TCHAR* Status, const TCHAR* Reason)
	{
		Test->TestFalse(TEXT("The data session stopped"), Capture->IsRecording());
		Test->TestFalse(TEXT("The clip finalized"), Capture->IsVideoFinalizing());
		const TSharedPtr<FJsonObject> Link = ReadJsonFile(Capture->GetLinkPath());
		if (!Test->TestTrue(TEXT("capture-link.json is written"), Link.IsValid())) { return; }
		Test->TestEqual(TEXT("The link records how the capture ended"), Link->GetStringField(TEXT("status")), FString(Status));
		const TSharedPtr<FJsonObject> Video = Link->GetObjectField(TEXT("video"));
		Test->TestEqual(TEXT("The link records why the clip stopped"), Video->GetStringField(TEXT("stop_reason")), FString(Reason));
		Test->TestTrue(TEXT("The link records the stop instant"), Video->HasField(TEXT("stop_requested")));
		Test->TestEqual(TEXT("The session requested this stop"), Video->GetStringField(TEXT("stopped_by")), FString(TEXT("session")));
		const TSharedPtr<FJsonObject> Manifest = ReadJsonFile(Capture->GetVideoDirectory() / TEXT("video-manifest.json"));
		Test->TestTrue(TEXT("The recorder finalized a complete clip"), Manifest.IsValid() && Manifest->GetBoolField(TEXT("complete")));
		Test->TestFalse(TEXT("The recorder channel no longer names this bundle"),
			PresentationRecording::GetChannelStatus().Contains(TEXT("KatanaCombatCapture")));
	}

private:
	FAutomationTestBase* Test;
	bool bPIEEnd;
	FCombatCaptureSession* Capture = nullptr;
	double StartWall = 0, StartSimulation = 0;
};

/** After PIE has ended without Combat.Capture.Stop: wait for the clip, then check the link and channel. */
class FVerifyPIEEndTeardownCommand : public IAutomationLatentCommand
{
public:
	explicit FVerifyPIEEndTeardownCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartWall == 0) { StartWall = Now; }
		FCombatCaptureSession* Capture = CombatCaptureCommands::GetSession();
		if (!Capture) { Test->AddError(TEXT("No console capture session")); return true; }
		if ((Capture->IsVideoFinalizing() || (GEditor && GEditor->PlayWorld)) && Now - StartWall < 45) { return false; }
		FVideoTeardownCommand::VerifyVideoTeardown(Test, Capture, TEXT("stopped_by_pie_end"), TEXT("pie_ended"));
		FString Markers;
		FFileHelper::LoadFileToString(Markers, *(Capture->GetOutputDirectory() / TEXT("markers.jsonl")));
		Test->TestTrue(TEXT("The data session marks the PIE-end stop"), Markers.Contains(TEXT("\"reason\":\"pie_ended\"")));
		return true;
	}
private:
	FAutomationTestBase* Test;
	double StartWall = 0;
};

/**
 * The clip reaching its own VideoSeconds bound while the data session keeps recording. The link must be
 * final as soon as the recorder has finalized the clip: the limit as the reason and the instant the recorder
 * stopped as the anchor. A later Stop must neither rewrite the link nor record a later stop instant.
 */
class FRecorderLimitCommand : public IAutomationLatentCommand
{
public:
	explicit FRecorderLimitCommand(FAutomationTestBase* InTest) : Test(InTest) {}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (StartWall == 0) { StartWall = Now; }
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (Now - StartWall > 90)
		{
			Test->AddError(TEXT("Recorder-limit capture exceeded its watchdog"));
			if (World && Capture && Capture->IsRecording()) { IConsoleManager::Get().ProcessUserConsoleInput(TEXT("Combat.Capture.Stop"), *GLog, World); }
			return true;
		}
		if (!Capture)
		{
			if (!World || World->WorldType != EWorldType::PIE || FCombatCaptureSession::DiscoverParticipants(World).IsEmpty()) { return false; }
			// The clip's bound is far shorter than the data session's, so the recorder ends the clip on its own.
			IConsoleManager::Get().ProcessUserConsoleInput(*FString::Printf(TEXT("Combat.Capture.Start VideoRecorderLimit %d 0 60 Video=1 VideoSeconds=%d"),
				SessionSeconds, ClipSeconds), *GLog, World);
			Capture = CombatCaptureCommands::GetSession();
			if (!Test->TestTrue(TEXT("Console capture with video records"), Capture && Capture->IsRecording() && Capture->HasVideo())) { return true; }
			return false;
		}
		if (Capture->IsVideoFinalizing()) { return false; }
		VerifyBeforeStop(World);
		return true;
	}

private:
	static TSharedPtr<FJsonObject> VideoOf(const TSharedPtr<FJsonObject>& Link)
	{
		const TSharedPtr<FJsonObject>* Video = nullptr;
		return Link.IsValid() && Link->TryGetObjectField(TEXT("video"), Video) ? *Video : nullptr;
	}

	void VerifyBeforeStop(UWorld* World)
	{
		Test->TestTrue(TEXT("The data session outlives the clip"), Capture->IsRecording());
		const TSharedPtr<FJsonObject> Link = ReadJsonFile(Capture->GetLinkPath());
		const TSharedPtr<FJsonObject> Video = VideoOf(Link);
		if (!Test->TestTrue(TEXT("capture-link.json is written"), Video.IsValid())) { return; }
		Test->TestEqual(TEXT("The link is final once the recorder has finalized its clip"),
			Link->GetStringField(TEXT("status")), FString(TEXT("stopped_by_recorder_limit")));
		Test->TestTrue(TEXT("The reason names the recorder's own bound"), Video->GetStringField(TEXT("stop_reason")).StartsWith(TEXT("recorder_limit")));
		Test->TestEqual(TEXT("The link names who stopped the clip"), Video->GetStringField(TEXT("stopped_by")), FString(TEXT("recorder")));
		Test->TestFalse(TEXT("Nobody requested this stop"), Video->HasField(TEXT("stop_requested")));
		const TSharedPtr<FJsonObject>* Observed = nullptr;
		if (!Test->TestTrue(TEXT("The link records when the recorder stopped"), Video->TryGetObjectField(TEXT("recorder_stop_observed"), Observed))) { return; }
		const double ObservedFrame = (*Observed)->GetNumberField(TEXT("engine_frame"));
		const double ObservedSeconds = (*Observed)->GetNumberField(TEXT("platform_seconds"));

		const TSharedPtr<FJsonObject> Manifest = ReadJsonFile(Capture->GetVideoDirectory() / TEXT("video-manifest.json"));
		if (!Test->TestTrue(TEXT("The recorder finalized a complete clip"), Manifest.IsValid() && Manifest->GetBoolField(TEXT("complete")))) { return; }
		const double RequestedSeconds = Video->GetObjectField(TEXT("requested"))->GetNumberField(TEXT("seconds"));
		Test->TestEqual(TEXT("The clip used the requested bound"), RequestedSeconds, static_cast<double>(ClipSeconds));
		Test->TestTrue(TEXT("The stop was observed at or after the clip's bound"),
			ObservedSeconds >= Manifest->GetNumberField(TEXT("captureEpochPlatformSeconds")) + RequestedSeconds);
		Test->TestFalse(TEXT("The recorder channel no longer names this bundle"),
			PresentationRecording::GetChannelStatus().Contains(TEXT("KatanaCombatCapture")));

		const double StopFrame = static_cast<double>(GFrameCounter);
		const double StopSeconds = FPlatformTime::Seconds();
		IConsoleManager::Get().ProcessUserConsoleInput(TEXT("Combat.Capture.Stop"), *GLog, World);
		Test->TestFalse(TEXT("Stop finalizes the data session"), Capture->IsRecording());
		Test->TestTrue(TEXT("The recorder stop precedes the session stop"), ObservedFrame < StopFrame && ObservedSeconds < StopSeconds);
		const TSharedPtr<FJsonObject> After = ReadJsonFile(Capture->GetLinkPath());
		const TSharedPtr<FJsonObject> AfterVideo = VideoOf(After);
		if (!Test->TestTrue(TEXT("capture-link.json survives Stop"), AfterVideo.IsValid())) { return; }
		Test->TestEqual(TEXT("A later Stop keeps the recorder's outcome"), After->GetStringField(TEXT("status")), FString(TEXT("stopped_by_recorder_limit")));
		Test->TestFalse(TEXT("A later Stop records no stop request"), AfterVideo->HasField(TEXT("stop_requested")));
		const TSharedPtr<FJsonObject>* AfterObserved = nullptr;
		Test->TestTrue(TEXT("A later Stop keeps the recorder's stop instant"),
			AfterVideo->TryGetObjectField(TEXT("recorder_stop_observed"), AfterObserved)
			&& (*AfterObserved)->GetNumberField(TEXT("engine_frame")) == ObservedFrame
			&& (*AfterObserved)->GetNumberField(TEXT("platform_seconds")) == ObservedSeconds);
		FString Markers;
		FFileHelper::LoadFileToString(Markers, *(Capture->GetOutputDirectory() / TEXT("markers.jsonl")));
		Test->TestTrue(TEXT("The data session marks the recorder's stop"), Markers.Contains(TEXT("\"marker\":\"video_stopped_by_recorder\"")));
		Test->TestFalse(TEXT("The data session marks no later stop request"), Markers.Contains(TEXT("\"marker\":\"video_stop_requested\"")));
	}

	static constexpr int32 ClipSeconds = 2;
	static constexpr int32 SessionSeconds = 30;
	FAutomationTestBase* Test;
	FCombatCaptureSession* Capture = nullptr;
	double StartWall = 0;
};
} // namespace

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCombatCaptureVideoTeardownTest,
	"KatanaCombat.Capture.Video.Teardown", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FCombatCaptureVideoTeardownTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	Names.Add(TEXT("PIEEnd")); Commands.Add(TEXT("PIEEnd"));
	Names.Add(TEXT("SessionLimit")); Commands.Add(TEXT("SessionLimit"));
	Names.Add(TEXT("RecorderLimit")); Commands.Add(TEXT("RecorderLimit"));
}

bool FCombatCaptureVideoTeardownTest::RunTest(const FString& Parameters)
{
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("Video teardown needs a rendering editor; the -NullRHI refusal is covered by KatanaCombat.Capture.Video.ConsolePIE"));
		return true;
	}
	const bool bPIEEnd = Parameters == TEXT("PIEEnd");
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/ProjectFiles/Levels/Lvl_ThirdPerson1")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	if (Parameters == TEXT("RecorderLimit")) { ADD_LATENT_AUTOMATION_COMMAND(FRecorderLimitCommand(this)); }
	else { ADD_LATENT_AUTOMATION_COMMAND(FVideoTeardownCommand(this, bPIEEnd)); }
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	if (bPIEEnd) { ADD_LATENT_AUTOMATION_COMMAND(FVerifyPIEEndTeardownCommand(this)); }
	return true;
}

/**
 * How the link reports a clip the recorder ended without a stop request, from its finalized manifest. The
 * rendered RecorderLimit variant covers the bound end to end; an encoder failing mid-clip and another
 * recorder client's Stop cannot be provoked reliably in a test editor, so their classification is checked here.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCaptureRecorderEndTest,
	"KatanaCombat.Capture.Video.RecorderEndOutcome", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatCaptureRecorderEndTest::RunTest(const FString&)
{
	// Fixture clock: the clip's epoch and bound are this test's own values.
	const double Epoch = 1000.0, Bound = 2.0, Tick = 1.0 / 60.0;
	const auto Classify = [&](bool bRead, bool bComplete, const TCHAR* Failure, double Observed)
	{
		return FCombatCaptureSession::ClassifyRecorderEnd(bRead, bComplete, Failure, Epoch, Bound, Observed);
	};

	const FCombatCaptureRecorderEnd Limit = Classify(true, true, TEXT(""), Epoch + Bound + Tick);
	TestEqual(TEXT("A complete clip seen stopping after its bound reached its limit"), Limit.Status, FString(TEXT("stopped_by_recorder_limit")));
	TestTrue(TEXT("The limit reason is named"), Limit.Reason.StartsWith(TEXT("recorder_limit")));
	TestEqual(TEXT("Exactly at the bound is the limit"), Classify(true, true, TEXT(""), Epoch + Bound).Status, FString(TEXT("stopped_by_recorder_limit")));

	const FCombatCaptureRecorderEnd Failed = Classify(true, false, TEXT("encoder failed (exit 3). Inspect the encoder log."), Epoch + 0.5 * Bound);
	TestEqual(TEXT("An encoder failing mid-clip is a recorder error"), Failed.Status, FString(TEXT("stopped_by_recorder_error")));
	TestTrue(TEXT("The error carries the recorder's failure reason"),
		Failed.Reason.StartsWith(TEXT("recorder_error")) && Failed.Reason.Contains(TEXT("encoder failed (exit 3)")));
	TestEqual(TEXT("An incomplete clip at its bound is still an error, not the limit"),
		Classify(true, false, TEXT(""), Epoch + Bound + Tick).Status, FString(TEXT("stopped_by_recorder_error")));
	TestEqual(TEXT("A missing or unreadable manifest is an error"),
		Classify(false, false, TEXT(""), Epoch + Bound + Tick).Status, FString(TEXT("stopped_by_recorder_error")));

	const FCombatCaptureRecorderEnd Early = Classify(true, true, TEXT(""), Epoch + 0.5 * Bound);
	TestEqual(TEXT("A complete clip stopped before its bound was stopped by another recorder client"), Early.Status, FString(TEXT("stopped_by_recorder")));
	TestTrue(TEXT("The early stop is named"), Early.Reason.StartsWith(TEXT("recorder_stopped")));
	TestEqual(TEXT("Without a capture epoch the limit cannot be established"),
		FCombatCaptureSession::ClassifyRecorderEnd(true, true, TEXT(""), 0.0, Bound, Epoch + Bound + Tick).Status, FString(TEXT("stopped_by_recorder")));
	return true;
}
