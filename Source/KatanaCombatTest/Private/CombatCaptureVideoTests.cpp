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
#include "Engine/World.h"
#include "EngineUtils.h"
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
 * `Combat.Capture.Start ... Video=1`. The same command must yield linked video and data, every
 * video frame must join a motion sample by engine frame, and the landed character hits must be
 * marked. As in the 2026-10-03 spike, one enemy becomes a training dummy in front of the player.
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
