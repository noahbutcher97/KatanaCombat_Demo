// Copyright Epic Games, Inc. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Analysis/CombatCaptureSession.h"
#include "Core/CombatComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
TSharedPtr<FJsonObject> ReadJsonFile(const FString& Path)
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (FFileHelper::LoadFileToString(Text, *Path)) { FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root); }
	return Root;
}

/**
 * The human path: ordinary PIE in the level viewport, the ordinary console, and
 * `Combat.Capture.Start ... Video=1`. The same command must yield linked video and data, and
 * every video frame must join a motion sample by engine frame.
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

	bool Start(UWorld* World)
	{
		if (FCombatCaptureSession::DiscoverParticipants(World).IsEmpty()) { return false; }
		Console(World, TEXT("Combat.Capture.Start ConsoleVideo 20 0 60 Video=1 VideoFPS=60 VideoResolution=720"));
		Capture = CombatCaptureCommands::GetSession();
		if (!FApp::CanEverRender())
		{
			// The refusal happens before a bundle is created, so no silent data-only capture remains.
			Test->TestFalse(TEXT("Video is refused without a rendering editor"), Capture && Capture->IsRecording());
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
		FFileHelper::SaveStringToFile(Bundle, *(FPaths::ProjectSavedDir() / TEXT("CombatCaptures") / TEXT("ConsoleVideo-latest.txt")));
		Test->AddInfo(FString::Printf(TEXT("Console video capture %s: %d video frames, %d joined by engine frame, viewport %s"),
			*Bundle, VideoFrames, Joined, *FString::Printf(TEXT("%dx%d"), static_cast<int32>(Seat->GetIntegerField(TEXT("viewportWidth"))), static_cast<int32>(Seat->GetIntegerField(TEXT("viewportHeight"))))));
	}

	FAutomationTestBase* Test;
	FCombatCaptureSession* Capture = nullptr;
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
