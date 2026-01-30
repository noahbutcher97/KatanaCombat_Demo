// Copyright Epic Games, Inc. All Rights Reserved.

#include "MontageAnalyzerWindow.h"
#include "MontageAnalyzerTools.h"
#include "MontageAnalysisTypes.h"
#include "MontageAnalyzerTestUtility.h"
#include "Data/PairedAnimationData.h"
#include "Animation/AnimMontage.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "PropertyCustomizationHelpers.h"
#include "Framework/Docking/TabManager.h"
#include "LevelEditor.h"

#define LOCTEXT_NAMESPACE "MontageAnalyzerWindow"

void SMontageAnalyzerWindow::RegisterTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		GetTabName(),
		FOnSpawnTab::CreateStatic(&SMontageAnalyzerWindow::SpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Montage Analyzer"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Analyze animation montages for timing, sync points, and paired animation validation"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);  // We'll add our own menu entry

	// Add menu entry under Window menu
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender);
	MenuExtender->AddMenuExtension(
		"LevelEditor",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("MontageAnalyzerMenuEntry", "Montage Analyzer"),
				LOCTEXT("MontageAnalyzerMenuTooltip", "Open the Montage Analyzer tool"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([]()
				{
					FGlobalTabmanager::Get()->TryInvokeTab(GetTabName());
				}))
			);
		})
	);
	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}

void SMontageAnalyzerWindow::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GetTabName());
}

TSharedRef<SDockTab> SMontageAnalyzerWindow::SpawnTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SMontageAnalyzerWindow)
		];
}

void SMontageAnalyzerWindow::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(10.0f)
		[
			SNew(SVerticalBox)

			// ========== HEADER ==========
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 10)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WindowTitle", "Montage Analyzer Tools"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
			]

			// ========== SINGLE MONTAGE ANALYSIS ==========
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SingleMontageHeader", "Single Montage Analysis"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MontageLabel", "Montage:"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UAnimMontage::StaticClass())
					.ObjectPath(this, &SMontageAnalyzerWindow::GetMontagePath)
					.OnObjectChanged(this, &SMontageAnalyzerWindow::OnMontageSelected)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 5, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("AnalyzeBtn", "Analyze"))
					.OnClicked(this, &SMontageAnalyzerWindow::OnAnalyzeMontageClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("FullAnalysisBtn", "Full Analysis"))
					.OnClicked(this, &SMontageAnalyzerWindow::OnRunFullAnalysisClicked)
				]
			]

			// ========== SEPARATOR ==========
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 15)
			[
				SNew(SSeparator)
			]

			// ========== PAIRED MONTAGE ANALYSIS ==========
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PairedMontageHeader", "Paired Montage Sync Analysis"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AttackerLabel", "Attacker Montage:"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UAnimMontage::StaticClass())
					.ObjectPath(this, &SMontageAnalyzerWindow::GetAttackerMontagePath)
					.OnObjectChanged(this, &SMontageAnalyzerWindow::OnAttackerMontageSelected)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("VictimLabel", "Victim Montage:"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UAnimMontage::StaticClass())
					.ObjectPath(this, &SMontageAnalyzerWindow::GetVictimMontagePath)
					.OnObjectChanged(this, &SMontageAnalyzerWindow::OnVictimMontageSelected)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SButton)
				.Text(LOCTEXT("AnalyzePairedBtn", "Analyze Paired Sync"))
				.OnClicked(this, &SMontageAnalyzerWindow::OnAnalyzePairedClicked)
			]

			// ========== SEPARATOR ==========
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 15)
			[
				SNew(SSeparator)
			]

			// ========== PAIRED ANIMATION DATA VALIDATION ==========
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PairedDataHeader", "PairedAnimationData Validation"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PairedDataLabel", "Paired Data:"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UPairedAnimationData::StaticClass())
					.ObjectPath(this, &SMontageAnalyzerWindow::GetPairedDataPath)
					.OnObjectChanged(this, &SMontageAnalyzerWindow::OnPairedDataSelected)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SButton)
				.Text(LOCTEXT("ValidateBtn", "Validate"))
				.OnClicked(this, &SMontageAnalyzerWindow::OnValidatePairedDataClicked)
			]

			// ========== INSTRUCTIONS ==========
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 20, 0, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Instructions", "Results are printed to Output Log (Window > Developer Tools > Output Log)"))
				.ColorAndOpacity(FSlateColor(FLinearColor::Gray))
			]
		]
	];
}

FReply SMontageAnalyzerWindow::OnAnalyzeMontageClicked()
{
	if (SelectedMontage.IsValid())
	{
		UMontageAnalyzerTestUtility* Analyzer = NewObject<UMontageAnalyzerTestUtility>();
		Analyzer->AnalyzeMontage(SelectedMontage.Get());
		Analyzer->PrintTimingAnalysis(SelectedMontage.Get());
		Analyzer->PrintNotifyList(SelectedMontage.Get());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MontageAnalyzer: No montage selected"));
	}
	return FReply::Handled();
}

FReply SMontageAnalyzerWindow::OnRunFullAnalysisClicked()
{
	if (SelectedMontage.IsValid())
	{
		UMontageAnalyzerTestUtility* Analyzer = NewObject<UMontageAnalyzerTestUtility>();
		Analyzer->RunFullAnalysis(SelectedMontage.Get());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MontageAnalyzer: No montage selected"));
	}
	return FReply::Handled();
}

FReply SMontageAnalyzerWindow::OnAnalyzePairedClicked()
{
	if (SelectedAttackerMontage.IsValid() && SelectedVictimMontage.IsValid())
	{
		UMontageAnalyzerTestUtility* Analyzer = NewObject<UMontageAnalyzerTestUtility>();
		Analyzer->AnalyzePairedMontageSync(
			SelectedAttackerMontage.Get(),
			SelectedVictimMontage.Get(),
			0.0f);  // No expected time - will just compare what it finds
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MontageAnalyzer: Select both attacker and victim montages"));
	}
	return FReply::Handled();
}

FReply SMontageAnalyzerWindow::OnValidatePairedDataClicked()
{
	if (SelectedPairedData.IsValid())
	{
		UMontageAnalyzerTestUtility* Analyzer = NewObject<UMontageAnalyzerTestUtility>();
		Analyzer->ValidatePairedAnimationData(SelectedPairedData.Get());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MontageAnalyzer: No PairedAnimationData selected"));
	}
	return FReply::Handled();
}

FString SMontageAnalyzerWindow::GetMontagePath() const
{
	return SelectedMontage.IsValid() ? SelectedMontage->GetPathName() : FString();
}

FString SMontageAnalyzerWindow::GetAttackerMontagePath() const
{
	return SelectedAttackerMontage.IsValid() ? SelectedAttackerMontage->GetPathName() : FString();
}

FString SMontageAnalyzerWindow::GetVictimMontagePath() const
{
	return SelectedVictimMontage.IsValid() ? SelectedVictimMontage->GetPathName() : FString();
}

FString SMontageAnalyzerWindow::GetPairedDataPath() const
{
	return SelectedPairedData.IsValid() ? SelectedPairedData->GetPathName() : FString();
}

void SMontageAnalyzerWindow::OnMontageSelected(const FAssetData& AssetData)
{
	SelectedMontage = Cast<UAnimMontage>(AssetData.GetAsset());
}

void SMontageAnalyzerWindow::OnAttackerMontageSelected(const FAssetData& AssetData)
{
	SelectedAttackerMontage = Cast<UAnimMontage>(AssetData.GetAsset());
}

void SMontageAnalyzerWindow::OnVictimMontageSelected(const FAssetData& AssetData)
{
	SelectedVictimMontage = Cast<UAnimMontage>(AssetData.GetAsset());
}

void SMontageAnalyzerWindow::OnPairedDataSelected(const FAssetData& AssetData)
{
	SelectedPairedData = Cast<UPairedAnimationData>(AssetData.GetAsset());
}

#undef LOCTEXT_NAMESPACE
