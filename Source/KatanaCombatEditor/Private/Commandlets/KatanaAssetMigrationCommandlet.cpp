// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/KatanaAssetMigrationCommandlet.h"

#include "Commandlets/KatanaAssetMigrationRunner.h"

DEFINE_LOG_CATEGORY_STATIC(LogKatanaAssetMigrationCommandlet, Log, All);

UKatanaAssetMigrationCommandlet::UKatanaAssetMigrationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	FastExit = true;
	LogToConsole = true;
	UseCommandletResultAsExitCode = true;
}

int32 UKatanaAssetMigrationCommandlet::Main(const FString& Params)
{
	FKatanaAssetMigrationOptions Options;
	TArray<FString> Errors;
	if (!FKatanaAssetMigrationRunner::ParseOptions(Params, Options, Errors))
	{
		for (const FString& Error : Errors)
		{
			UE_LOG(LogKatanaAssetMigrationCommandlet, Warning, TEXT("%s"), *Error);
		}
		return static_cast<int32>(EKatanaAssetMigrationExitCode::InvalidArguments);
	}

	FKatanaAssetMigrationRunner Runner;
	const EKatanaAssetMigrationExitCode Result = Runner.Run(Options);
	return static_cast<int32>(Result);
}
