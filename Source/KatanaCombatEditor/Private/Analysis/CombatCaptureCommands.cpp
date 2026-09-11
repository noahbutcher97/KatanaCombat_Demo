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
	if (Args.Num() > 0) { Settings.Scenario = Args[0]; }
	if (Args.Num() > 4 || (Args.Num() > 1 && !LexTryParseString(Settings.MaxWallSeconds, *Args[1]))
		|| (Args.Num() > 2 && !LexTryParseString(Settings.FrameHz, *Args[2]))
		|| (Args.Num() > 3 && !LexTryParseString(Settings.SampleHz, *Args[3])))
	{
		UE_LOG(LogTemp, Error, TEXT("Usage: Combat.Capture.Start [Scenario] [MaxWallSeconds=60] [FrameHz=5] [SampleHz=60]")); return;
	}
	if (!ConsoleSession) { ConsoleSession = MakeUnique<FCombatCaptureSession>(); }
	FString Error;
	if (!ConsoleSession->Start(World, Settings, FCombatCaptureSession::DiscoverParticipants(World), Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Capture start failed: %s"), *Error); return;
	}
	UE_LOG(LogTemp, Display, TEXT("Combat capture recording: %s"), *ConsoleSession->GetOutputDirectory());
}

void StopCapture()
{
	if (!ConsoleSession) { return; }
	FString Error;
	if (!ConsoleSession->Stop(TEXT("console_stop"), Error)) { UE_LOG(LogTemp, Error, TEXT("Capture stop failed: %s"), *Error); }
	else { UE_LOG(LogTemp, Display, TEXT("Combat capture saved: %s"), *ConsoleSession->GetOutputDirectory()); }
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
		TEXT("Record the current PIE world: [Scenario] [MaxWallSeconds=60] [FrameHz=5] [SampleHz=60]. FrameHz=0 disables images."),
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
