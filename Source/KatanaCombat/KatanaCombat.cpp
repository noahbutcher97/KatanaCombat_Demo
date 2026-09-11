// Copyright Epic Games, Inc. All Rights Reserved.

#include "KatanaCombat.h"
#include "Core/CombatComponent.h"
#include "Debug/ActionReactionTelemetry.h"
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

TAutoConsoleVariable<int32> CVarActionReactionDebug(
	TEXT("Combat.ActionReaction.Debug"),
	0,
	TEXT("Capture bounded per-combat action/reaction telemetry. 0: disabled, 1: enabled"),
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

void DumpActionReactionTelemetry(const TArray<FString>& Args)
{
	if (Args.IsEmpty())
	{
		UE_LOG(LogKatanaCombat, Error,
			TEXT("Combat.ActionReaction.DumpTelemetry requires an absolute or project-relative CSV path"));
		return;
	}

	TArray<FActionReactionTelemetryRecord> Records;
	ForEachRuntimeCombatComponent([&Records](UCombatComponent& Combat)
	{
		Records.Append(Combat.GetActionReactionTelemetry());
	});

	FString ResolvedPath;
	FString Error;
	if (!ActionReactionTelemetry::WriteCsv(
		FString::Join(Args, TEXT(" ")), Records, ResolvedPath, Error))
	{
		UE_LOG(LogKatanaCombat, Error, TEXT("%s"), *Error);
		return;
	}
	UE_LOG(LogKatanaCombat, Display,
		TEXT("Wrote %d action-reaction telemetry records to %s"), Records.Num(), *ResolvedPath);
}

void ClearActionReactionTelemetry()
{
	int32 ClearedComponents = 0;
	ForEachRuntimeCombatComponent([&ClearedComponents](UCombatComponent& Combat)
	{
		Combat.ClearActionReactionTelemetry();
		++ClearedComponents;
	});
	UE_LOG(LogKatanaCombat, Display,
		TEXT("Cleared action-reaction telemetry on %d runtime combat components"),
		ClearedComponents);
}

FAutoConsoleCommand DumpDefenseTelemetryCommand(
	TEXT("Combat.Defense.DumpTelemetry"),
	TEXT("Write all runtime combat-component defense telemetry to the supplied CSV path"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DumpDefenseTelemetry));

FAutoConsoleCommand ClearDefenseTelemetryCommand(
	TEXT("Combat.Defense.ClearTelemetry"),
	TEXT("Clear all runtime combat-component defense telemetry rings"),
	FConsoleCommandDelegate::CreateStatic(&ClearDefenseTelemetry));

FAutoConsoleCommand DumpActionReactionTelemetryCommand(
	TEXT("Combat.ActionReaction.DumpTelemetry"),
	TEXT("Write all runtime combat-component action/reaction telemetry to the supplied CSV path"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DumpActionReactionTelemetry));

FAutoConsoleCommand ClearActionReactionTelemetryCommand(
	TEXT("Combat.ActionReaction.ClearTelemetry"),
	TEXT("Clear all runtime combat-component action/reaction telemetry rings"),
	FConsoleCommandDelegate::CreateStatic(&ClearActionReactionTelemetry));
}

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, KatanaCombat, "KatanaCombat" );

DEFINE_LOG_CATEGORY(LogKatanaCombat)
