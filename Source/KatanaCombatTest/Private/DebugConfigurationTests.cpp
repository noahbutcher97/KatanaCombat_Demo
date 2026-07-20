// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Debug/CombatDebugHUD.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/ConfigCacheIni.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDebugConfiguration_DefaultGameModeProvidesCombatHUD,
	"KatanaCombat.Debug.Configuration.DefaultGameModeProvidesCombatHUD",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDebugConfiguration_DefaultGameModeProvidesCombatHUD::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString DefaultGameModePath;
	TestTrue(TEXT("Project config declares a global default GameMode"),
		GConfig->GetString(
			TEXT("/Script/EngineSettings.GameMapsSettings"),
			TEXT("GlobalDefaultGameMode"),
			DefaultGameModePath,
			GEngineIni));

	UClass* DefaultGameModeClass = StaticLoadClass(
		AGameModeBase::StaticClass(),
		nullptr,
		*DefaultGameModePath);
	TestNotNull(TEXT("Global default GameMode resolves to an existing class"), DefaultGameModeClass);
	const AGameModeBase* DefaultGameMode = DefaultGameModeClass
		? DefaultGameModeClass->GetDefaultObject<AGameModeBase>()
		: nullptr;
	TestNotNull(TEXT("Global default GameMode has a class default object"), DefaultGameMode);
	TestTrue(TEXT("Maps using project defaults receive the persistent combat debug HUD"),
		DefaultGameMode
		&& DefaultGameMode->HUDClass
		&& DefaultGameMode->HUDClass->IsChildOf(ACombatDebugHUD::StaticClass()));
	return true;
}
