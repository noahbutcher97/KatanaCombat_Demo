// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UAnimMontage;
class UPairedAnimationData;

/**
 * Slate window for Montage Analyzer Tools
 *
 * Accessible via Window → Montage Analyzer in the editor
 */
class KATANACOMBATEDITOR_API SMontageAnalyzerWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMontageAnalyzerWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Register the window with the editor's Window menu */
	static void RegisterTabSpawner();

	/** Unregister when module shuts down */
	static void UnregisterTabSpawner();

	/** Get the tab name for spawning */
	static FName GetTabName() { return TEXT("MontageAnalyzerTab"); }

private:
	// Selected assets
	TWeakObjectPtr<UAnimMontage> SelectedMontage;
	TWeakObjectPtr<UAnimMontage> SelectedAttackerMontage;
	TWeakObjectPtr<UAnimMontage> SelectedVictimMontage;
	TWeakObjectPtr<UPairedAnimationData> SelectedPairedData;

	// UI Callbacks
	FReply OnAnalyzeMontageClicked();
	FReply OnAnalyzePairedClicked();
	FReply OnValidatePairedDataClicked();
	FReply OnRunFullAnalysisClicked();

	// Asset picker helpers
	FString GetMontagePath() const;
	FString GetAttackerMontagePath() const;
	FString GetVictimMontagePath() const;
	FString GetPairedDataPath() const;

	void OnMontageSelected(const FAssetData& AssetData);
	void OnAttackerMontageSelected(const FAssetData& AssetData);
	void OnVictimMontageSelected(const FAssetData& AssetData);
	void OnPairedDataSelected(const FAssetData& AssetData);

	// Tab spawner callback
	static TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);
};
