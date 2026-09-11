// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"
#include "Debug/CombatDebugHUD.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
class FScopedDebugConsoleInt
{
public:
	FScopedDebugConsoleInt(const TCHAR* Name, const int32 Value)
		: Variable(IConsoleManager::Get().FindConsoleVariable(Name))
	{
		if (Variable)
		{
			PreviousValue = Variable->GetInt();
			Set(Value);
		}
	}

	~FScopedDebugConsoleInt()
	{
		Set(PreviousValue);
		IConsoleManager::Get().CallAllConsoleVariableSinks();
	}

	void Set(const int32 Value) const
	{
		if (Variable)
		{
			Variable->SetWithCurrentPriority(Value);
		}
	}

	bool IsValid() const { return Variable != nullptr; }

private:
	IConsoleVariable* Variable = nullptr;
	int32 PreviousValue = 0;
};
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDebugConfiguration_CombatStateCategoryRouting,
	"KatanaCombat.Debug.Configuration.CombatStateCategoryRouting",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugConfiguration_CombatStateCategoryRouting::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedDebugConsoleInt Master(TEXT("Combat.Debug.All"), 0);
	FScopedDebugConsoleInt Phase(TEXT("Combat.Debug.Phase"), 0);
	FScopedDebugConsoleInt Queue(TEXT("Combat.Debug.Queue"), 0);
	FScopedDebugConsoleInt Hold(TEXT("Combat.Debug.Hold"), 0);
	TestTrue(TEXT("Combat state debug CVars should be registered"),
		Master.IsValid() && Phase.IsValid() && Queue.IsValid() && Hold.IsValid());
	IConsoleManager::Get().CallAllConsoleVariableSinks();

	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player =
		FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	if (!TestNotNull(TEXT("Player should be created"), Player)
		|| !TestNotNull(TEXT("Combat component should be created"), Combat))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestFalse(TEXT("Combat component debug tick should default off"),
		Combat->IsComponentTickEnabled());

	Phase.Set(1);
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	TestTrue(TEXT("Phase CVar should activate the persistent component overlay tick"),
		Combat->IsComponentTickEnabled());

	Phase.Set(0);
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	TestFalse(TEXT("Disabling Phase should stop the component overlay tick"),
		Combat->IsComponentTickEnabled());

	Queue.Set(1);
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	TestTrue(TEXT("Queue CVar should activate the persistent component overlay tick"),
		Combat->IsComponentTickEnabled());

	Queue.Set(0);
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	TestFalse(TEXT("Disabling Queue should stop the component overlay tick"),
		Combat->IsComponentTickEnabled());

	Hold.Set(1);
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	TestTrue(TEXT("Hold CVar should activate the persistent component overlay tick"),
		Combat->IsComponentTickEnabled());

	Hold.Set(0);
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	TestFalse(TEXT("Disabling Hold should stop the component overlay tick"),
		Combat->IsComponentTickEnabled());

	Master.Set(1);
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	TestTrue(TEXT("Master CVar should activate the component overlay tick"),
		Combat->IsComponentTickEnabled());

	Master.Set(0);
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	TestFalse(TEXT("Disabling all state CVars should stop the component overlay tick"),
		Combat->IsComponentTickEnabled());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
