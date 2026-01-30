// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityObject.h"
#include "MontageAnalysisTypes.h"
#include "MontageAnalyzerTestUtility.generated.h"

class UAnimMontage;

/**
 * Editor Utility for testing MontageAnalyzerTools
 *
 * Usage:
 * 1. Right-click this asset in Content Browser
 * 2. Select "Run Editor Utility"
 * 3. Or call functions directly from Blueprint
 *
 * You can also create a child Blueprint of this class
 * to customize the testing workflow.
 */
UCLASS(Blueprintable, BlueprintType)
class KATANACOMBATEDITOR_API UMontageAnalyzerTestUtility : public UEditorUtilityObject
{
	GENERATED_BODY()

public:
	// ========================================================================
	// TEST FUNCTIONS
	// ========================================================================

	/**
	 * Analyze a montage and print results to the Output Log
	 * @param Montage - The montage to analyze
	 * @param SectionName - Specific section to analyze (NAME_None for full montage)
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Test")
	void AnalyzeMontage(UAnimMontage* Montage, FName SectionName = NAME_None);

	/**
	 * Analyze timing and print to log
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Test")
	void PrintTimingAnalysis(UAnimMontage* Montage, FName SectionName = NAME_None);

	/**
	 * Find and print sync point information
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Test")
	void PrintSyncPointInfo(UAnimMontage* Montage);

	/**
	 * Print all notifies in the montage
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Test")
	void PrintNotifyList(UAnimMontage* Montage);

	/**
	 * Print root motion analysis
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Test")
	void PrintRootMotionAnalysis(UAnimMontage* Montage);

	/**
	 * Sample and print bone trajectory
	 * @param BoneName - Name of bone to track (e.g., "hand_r", "weapon_r")
	 * @param SampleCount - Number of samples to take
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Test")
	void PrintBoneTrajectory(UAnimMontage* Montage, FName BoneName, int32 SampleCount = 10);

	/**
	 * Comprehensive analysis - runs all tests
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Test", meta = (CallInEditor = "true"))
	void RunFullAnalysis(UAnimMontage* Montage);

	// ========================================================================
	// PAIRED ANIMATION SPECIFIC
	// ========================================================================

	/**
	 * Validate a paired animation data asset
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Paired Animation")
	void ValidatePairedAnimationData(class UPairedAnimationData* PairedData);

	/**
	 * Analyze sync point alignment between attacker and victim montages
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Paired Animation")
	void AnalyzePairedMontageSync(UAnimMontage* AttackerMontage, UAnimMontage* VictimMontage, float ExpectedSyncTime);

protected:
	/** Helper to print section header */
	void PrintHeader(const FString& Title);

	/** Helper to print key-value pair */
	void PrintValue(const FString& Key, const FString& Value);
};
