// Copyright Epic Games, Inc. All Rights Reserved.

#include "KatanaCombatEditor.h"
#include "PropertyEditorModule.h"
#include "Customizations/AttackDataCustomization.h"
#include "Customizations/HitReactionDataCustomization.h"
#include "Customizations/HitReactionEntryCustomization.h"
#include "Customizations/ReactionMontageVariantCustomization.h"
#include "PairedAnimationPreview.h"
#include "Data/AttackData.h"
#include "Data/HitReactionData.h"

#define LOCTEXT_NAMESPACE "FKatanaCombatEditorModule"

void FKatanaCombatEditorModule::StartupModule()
{
	RegisterCustomizations();

	// Register Paired Animation Preview (Window > Paired Animation Preview)
	SPairedAnimationPreview::RegisterTabSpawner();
}

void FKatanaCombatEditorModule::ShutdownModule()
{
	// Unregister Paired Animation Preview
	SPairedAnimationPreview::UnregisterTabSpawner();

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

	// Register custom details panel for HitReactionData
	PropertyModule.RegisterCustomClassLayout(
		UHitReactionData::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHitReactionDataCustomization::MakeInstance)
	);

	// Register custom property type layout for FHitReactionEntry struct
	PropertyModule.RegisterCustomPropertyTypeLayout(
		"HitReactionEntry",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FHitReactionEntryCustomization::MakeInstance)
	);

	// Register custom property type layout for FReactionMontageVariant struct (array element)
	PropertyModule.RegisterCustomPropertyTypeLayout(
		"ReactionMontageVariant",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FReactionMontageVariantCustomization::MakeInstance)
	);
}

void FKatanaCombatEditorModule::UnregisterCustomizations()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = 
			FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
        
		PropertyModule.UnregisterCustomClassLayout(UAttackData::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UHitReactionData::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout("HitReactionEntry");
		PropertyModule.UnregisterCustomPropertyTypeLayout("ReactionMontageVariant");
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FKatanaCombatEditorModule, KatanaCombatEditor)