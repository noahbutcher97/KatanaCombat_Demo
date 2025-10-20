// Copyright Epic Games, Inc. All Rights Reserved.

#include "KatanaCombatEditor.h"
#include "PropertyEditorModule.h"
#include "Customizations/AttackDataCustomization.h"
#include "Data/AttackData.h"

#define LOCTEXT_NAMESPACE "FKatanaCombatEditorModule"

void FKatanaCombatEditorModule::StartupModule()
{
	RegisterCustomizations();
}

void FKatanaCombatEditorModule::ShutdownModule()
{
	UnregisterCustomizations();
}

void FKatanaCombatEditorModule::RegisterCustomizations()
{
	// Register custom details panel for AttackData
	FPropertyEditorModule& PropertyModule = 
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    
	PropertyModule.RegisterCustomClassLayout(
		UAttackData::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FAttackDataCustomization::MakeInstance)
	);
}

void FKatanaCombatEditorModule::UnregisterCustomizations()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = 
			FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
        
		PropertyModule.UnregisterCustomClassLayout(UAttackData::StaticClass()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FKatanaCombatEditorModule, KatanaCombatEditor)