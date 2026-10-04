// Copyright Epic Games, Inc. All Rights Reserved.
#include "Analysis/CombatCaptureSession.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "String/LexFromString.h"

namespace
{
TUniquePtr<FCombatCaptureSession> ConsoleSession;
TArray<IConsoleObject*> Commands;

const TCHAR* StartUsage = TEXT("Usage: Combat.Capture.Start [Scenario] [MaxWallSeconds=60] [FrameHz=5] [SampleHz=60] [Video=0] [VideoFPS=60] [VideoResolution=720] [VideoSeconds=0]. ")
	TEXT("The first four are positional or Name=Value; the video options are Name=Value. Video=1 records an MP4 of the PIE viewport (FrameHz defaults to 0 with video).");

/** Positional arguments keep their historical order; Name=Value arguments may follow in any order. */
bool ParseStartArguments(const TArray<FString>& Args, FCombatCaptureSettings& Settings)
{
	int32 Positional = 0;
	bool bFrameHzGiven = false;
	for (const FString& Arg : Args)
	{
		FString Name, Value;
		if (!Arg.Split(TEXT("="), &Name, &Value))
		{
			Value = Arg;
			switch (Positional++)
			{
			case 0: Name = TEXT("Scenario"); break;
			case 1: Name = TEXT("MaxWallSeconds"); break;
			case 2: Name = TEXT("FrameHz"); break;
			case 3: Name = TEXT("SampleHz"); break;
			default: return false;
			}
		}
		bool bParsed = false;
		if (Name.Equals(TEXT("Scenario"), ESearchCase::IgnoreCase)) { Settings.Scenario = Value; bParsed = !Value.IsEmpty(); }
		else if (Name.Equals(TEXT("MaxWallSeconds"), ESearchCase::IgnoreCase)) { bParsed = LexTryParseString(Settings.MaxWallSeconds, *Value); }
		else if (Name.Equals(TEXT("FrameHz"), ESearchCase::IgnoreCase)) { bParsed = LexTryParseString(Settings.FrameHz, *Value); bFrameHzGiven = true; }
		else if (Name.Equals(TEXT("SampleHz"), ESearchCase::IgnoreCase)) { bParsed = LexTryParseString(Settings.SampleHz, *Value); }
		else if (Name.Equals(TEXT("Video"), ESearchCase::IgnoreCase))
		{
			int32 Enabled = 0;
			bParsed = LexTryParseString(Enabled, *Value) && (Enabled == 0 || Enabled == 1);
			Settings.bRecordVideo = Enabled == 1;
		}
		else if (Name.Equals(TEXT("VideoFPS"), ESearchCase::IgnoreCase)) { bParsed = LexTryParseString(Settings.VideoFramesPerSecond, *Value); }
		else if (Name.Equals(TEXT("VideoResolution"), ESearchCase::IgnoreCase)) { bParsed = LexTryParseString(Settings.VideoResolution, *Value); }
		else if (Name.Equals(TEXT("VideoSeconds"), ESearchCase::IgnoreCase)) { bParsed = LexTryParseString(Settings.VideoSeconds, *Value); }
		if (!bParsed) { return false; }
	}
	// Synchronous PNG readback stalls the game thread, and the stalls would show in the clip.
	if (Settings.bRecordVideo && !bFrameHzGiven) { Settings.FrameHz = 0; }
	return true;
}

void StartCapture(const TArray<FString>& Args, UWorld* Context)
{
	UWorld* World = Context && Context->WorldType == EWorldType::PIE ? Context : nullptr;
	if (!World && GEngine)
	{
		for (const FWorldContext& Entry : GEngine->GetWorldContexts())
		{
			if (Entry.WorldType != EWorldType::PIE) { continue; }
			if (World) { UE_LOG(LogTemp, Error, TEXT("Capture: choose a PIE world via its console; multiple PIE worlds are running")); return; }
			World = Entry.World();
		}
	}
	FCombatCaptureSettings Settings;
	if (!ParseStartArguments(Args, Settings))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), StartUsage); return;
	}
	if (!ConsoleSession) { ConsoleSession = MakeUnique<FCombatCaptureSession>(); }
	FString Error;
	if (!ConsoleSession->Start(World, Settings, FCombatCaptureSession::DiscoverParticipants(World), Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Capture start failed: %s"), *Error); return;
	}
	UE_LOG(LogTemp, Display, TEXT("Combat capture recording: %s"), *ConsoleSession->GetOutputDirectory());
	// Hits, blocks and parries between any two recorded characters become contact markers.
	FString ContactError;
	if (!ConsoleSession->ObserveParticipantContacts(ContactError))
	{
		UE_LOG(LogTemp, Display, TEXT("Combat capture contact markers unavailable: %s"), *ContactError);
	}
	if (ConsoleSession->HasVideo())
	{
		UE_LOG(LogTemp, Display, TEXT("Combat capture video: %s (joined by %s)"), *ConsoleSession->GetVideoDirectory(), *ConsoleSession->GetLinkPath());
	}
}

void StopCapture()
{
	if (!ConsoleSession) { return; }
	FString Error;
	if (!ConsoleSession->Stop(TEXT("console_stop"), Error)) { UE_LOG(LogTemp, Error, TEXT("Capture stop failed: %s"), *Error); }
	else { UE_LOG(LogTemp, Display, TEXT("Combat capture saved: %s"), *ConsoleSession->GetOutputDirectory()); }
	if (ConsoleSession->HasVideo())
	{
		UE_LOG(LogTemp, Display, TEXT("Combat capture video finalizes asynchronously in %s; wait for its video-manifest.json"), *ConsoleSession->GetVideoDirectory());
	}
}

void MarkCapture(const TArray<FString>& Args)
{
	if (ConsoleSession) { ConsoleSession->Mark(FString::Join(Args, TEXT(" "))); }
}
}

void CombatCaptureCommands::Register()
{
	auto& Manager = IConsoleManager::Get();
	Commands.Add(Manager.RegisterConsoleCommand(TEXT("Combat.Capture.Start"),
		TEXT("Record the current PIE world: [Scenario] [MaxWallSeconds=60] [FrameHz=5] [SampleHz=60] [Video=0] [VideoFPS=60] [VideoResolution=720] [VideoSeconds=0]. ")
		TEXT("FrameHz=0 disables images; Video=1 also records an MP4 joined by capture-link.json."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartCapture)));
	Commands.Add(Manager.RegisterConsoleCommand(TEXT("Combat.Capture.Stop"), TEXT("Finalize the current combat recording."),
		FConsoleCommandDelegate::CreateStatic(&StopCapture)));
	Commands.Add(Manager.RegisterConsoleCommand(TEXT("Combat.Capture.Mark"), TEXT("Add an event label to the current recording."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&MarkCapture)));
}

void CombatCaptureCommands::Unregister()
{
	ConsoleSession.Reset();
	for (IConsoleObject* Command : Commands) { IConsoleManager::Get().UnregisterConsoleObject(Command); }
	Commands.Reset();
}

FCombatCaptureSession* CombatCaptureCommands::GetSession() { return ConsoleSession.Get(); }
