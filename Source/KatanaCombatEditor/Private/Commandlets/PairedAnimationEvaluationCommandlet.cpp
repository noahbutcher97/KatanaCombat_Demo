// Copyright Epic Games, Inc. All Rights Reserved.
#include "Commandlets/PairedAnimationEvaluationCommandlet.h"
#include "Subsystems/PairedAnimationAnalysisSubsystem.h"
#include "Editor.h"
#include "Misc/Parse.h"

UPairedAnimationEvaluationCommandlet::UPairedAnimationEvaluationCommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UPairedAnimationEvaluationCommandlet::Main(const FString& Params)
{
	FString Profile, Capture, Directory, Error;
	FParse::Value(*Params, TEXT("Profile="), Profile); FParse::Value(*Params, TEXT("Capture="), Capture);
	UPairedAnimationAnalysisSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UPairedAnimationAnalysisSubsystem>() : nullptr;
	if (!Subsystem || Profile.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("Provide -Profile=<json>; -Capture=<directory> is optional")); return 2; }
	const bool bCompleted = Subsystem->EvaluateContactProfile(Profile, Capture, Directory, Error);
	UE_LOG(LogTemp, Display, TEXT("PAIRED_EVALUATION_OUTPUT=%s"), *Directory);
	if (!bCompleted) { UE_LOG(LogTemp, Error, TEXT("Paired evaluation incomplete: %s"), *Error); }
	return bCompleted ? 0 : 2;
}
