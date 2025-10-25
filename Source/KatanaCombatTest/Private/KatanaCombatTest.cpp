#include "KatanaCombatTest.h"

DEFINE_LOG_CATEGORY(KatanaCombatTest);

#define LOCTEXT_NAMESPACE "FKatanaCombatTest"

void FKatanaCombatTest::StartupModule()
{
	UE_LOG(KatanaCombatTest, Warning, TEXT("KatanaCombatTest module has been loaded"));
}

void FKatanaCombatTest::ShutdownModule()
{
	UE_LOG(KatanaCombatTest, Warning, TEXT("KatanaCombatTest module has been unloaded"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FKatanaCombatTest, KatanaCombatTest)