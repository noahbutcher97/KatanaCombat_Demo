// Copyright Epic Games, Inc. All Rights Reserved.

#include "KatanaCombat.h"
#include "Core/CombatComponent.h"
#include "Debug/DefenseTelemetry.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"

namespace
{
TAutoConsoleVariable<int32> CVarDefenseDebug(
	TEXT("Combat.Defense.Debug"),
	0,
	TEXT("Capture bounded per-combat defense telemetry. 0: disabled, 1: enabled"),
	ECVF_Default);

bool IsRuntimeDefenseWorld(const UWorld* World)
{
	return World && (World->WorldType == EWorldType::Game
		|| World->WorldType == EWorldType::PIE
		|| World->WorldType == EWorldType::GamePreview);
}

void ForEachRuntimeCombatComponent(TFunctionRef<void(UCombatComponent&)> Callback)
{
	for (TObjectIterator<UCombatComponent> It; It; ++It)
	{
		UCombatComponent* Combat = *It;
		if (!IsValid(Combat)
			|| Combat->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
			|| !IsRuntimeDefenseWorld(Combat->GetWorld()))
		{
			continue;
		}
		Callback(*Combat);
	}
}

void DumpDefenseTelemetry(const TArray<FString>& Args)
{
	if (Args.IsEmpty())
	{
		UE_LOG(LogKatanaCombat, Error,
			TEXT("Combat.Defense.DumpTelemetry requires an absolute or project-relative CSV path"));
		return;
	}

	TArray<FDefenseTelemetryRecord> Records;
	ForEachRuntimeCombatComponent([&Records](UCombatComponent& Combat)
	{
		Records.Append(Combat.GetDefenseTelemetry());
	});

	FString ResolvedPath;
	FString Error;
	if (!DefenseTelemetry::WriteCsv(FString::Join(Args, TEXT(" ")), Records, ResolvedPath, Error))
	{
		UE_LOG(LogKatanaCombat, Error, TEXT("%s"), *Error);
		return;
	}
	UE_LOG(LogKatanaCombat, Display,
		TEXT("Wrote %d defense telemetry records to %s"), Records.Num(), *ResolvedPath);
}

void ClearDefenseTelemetry()
{
	int32 ClearedComponents = 0;
	ForEachRuntimeCombatComponent([&ClearedComponents](UCombatComponent& Combat)
	{
		Combat.ClearDefenseTelemetry();
		++ClearedComponents;
	});
	UE_LOG(LogKatanaCombat, Display,
		TEXT("Cleared defense telemetry on %d runtime combat components"), ClearedComponents);
}

FAutoConsoleCommand DumpDefenseTelemetryCommand(
	TEXT("Combat.Defense.DumpTelemetry"),
	TEXT("Write all runtime combat-component defense telemetry to the supplied CSV path"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DumpDefenseTelemetry));

FAutoConsoleCommand ClearDefenseTelemetryCommand(
	TEXT("Combat.Defense.ClearTelemetry"),
	TEXT("Clear all runtime combat-component defense telemetry rings"),
	FConsoleCommandDelegate::CreateStatic(&ClearDefenseTelemetry));
}

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, KatanaCombat, "KatanaCombat" );

DEFINE_LOG_CATEGORY(LogKatanaCombat)
