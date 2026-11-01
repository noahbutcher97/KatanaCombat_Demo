// Copyright Epic Games, Inc. All Rights Reserved.

#include "KatanaCombatEditor.h"
#include "Analysis/CombatCaptureSession.h"
#include "PropertyEditorModule.h"
#include "Customizations/AttackDataCustomization.h"
#include "Customizations/HitReactionEntryCustomization.h"
#include "Customizations/ReactionMontageVariantCustomization.h"
#include "PairedAnimationPreview.h"
#include "AttackDataTools.h"
#include "AttackTimingDerivationService.h"
#include "ContentBrowserMenuContexts.h"
#include "Data/AttackData.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FKatanaCombatEditorModule"

void FKatanaCombatEditorModule::StartupModule()
{
	CombatCaptureCommands::Register();
	RegisterCustomizations();
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FKatanaCombatEditorModule::RegisterAttackDataAssetMenu));

	// Register Paired Animation Preview (Window > Paired Animation Preview)
	SPairedAnimationPreview::RegisterTabSpawner();
}

void FKatanaCombatEditorModule::ShutdownModule()
{
	CombatCaptureCommands::Unregister();
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
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
		PropertyModule.UnregisterCustomPropertyTypeLayout("HitReactionEntry");
		PropertyModule.UnregisterCustomPropertyTypeLayout("ReactionMontageVariant");
	}
}

void FKatanaCombatEditorModule::RegisterAttackDataAssetMenu()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UAttackData::StaticClass());
	if (!Menu)
	{
		return;
	}

	FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
	FToolUIAction DeriveTimingAction;
	DeriveTimingAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& MenuContext)
	{
		const UContentBrowserAssetContextMenuContext* Context =
			UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
		if (!Context)
		{
			return;
		}

		TArray<UAttackData*> Attacks = Context->LoadSelectedObjects<UAttackData>();
		TArray<FAttackTimingApplyResult> Results;
		int32 Applied = 0;
		int32 Unchanged = 0;
		int32 Refused = 0;
		UAttackDataTools::BatchDeriveTimingFromMontage(Attacks, Results, Applied, Unchanged, Refused);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FAttackTimingDerivationService::DescribeResults(Results)),
			LOCTEXT("DeriveTimingFromMontageTitle", "Derive Timing From Montage"));
	});
	Section.AddMenuEntry(
		"AttackData_DeriveTimingFromMontage",
		LOCTEXT("DeriveTimingFromMontage", "Derive Timing From Montage"),
		LOCTEXT("DeriveTimingFromMontageTooltip",
			"Write each selected attack's Manual Timing from the notifies its montage section plays. One undo reverts "
			"every write; an attack whose notifies are missing or ambiguous is left unchanged and the summary says why."),
		FSlateIcon(),
		DeriveTimingAction);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FKatanaCombatEditorModule, KatanaCombatEditor)
