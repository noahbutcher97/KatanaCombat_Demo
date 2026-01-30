// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MontageAnalyzerTools.h"
#include "MontageAnalysisTypes.h"
#include "PairedMontageAnalyzer.generated.h"

class UPairedAnimationData;
class UAnimMontage;
class USkeletalMesh;

/**
 * Paired Montage Analyzer
 *
 * Specialized analysis tools for paired animation montages:
 * - Contact point prediction between attacker and victim
 * - Sync point alignment validation
 * - Reach requirement analysis
 * - Timing synchronization checks
 * - Warp distance recommendations
 *
 * Design: Extends base MontageAnalyzerTools with paired animation specific analysis.
 * Works with UPairedAnimationData assets.
 *
 * All functions are editor-only with no runtime overhead.
 */
UCLASS()
class KATANACOMBATEDITOR_API UPairedMontageAnalyzer : public UMontageAnalyzerTools
{
	GENERATED_BODY()

public:
	// ========================================================================
	// PAIRED ANIMATION ANALYSIS
	// ========================================================================

	/**
	 * Run complete analysis on PairedAnimationData
	 *
	 * @param PairedData - PairedAnimationData to analyze
	 * @param AttackerMesh - Attacker skeleton (optional, uses default if null)
	 * @param VictimMesh - Victim skeleton (optional, uses same as attacker if null)
	 * @return Complete paired analysis result
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer")
	static FPairedMontageAnalysisResult AnalyzePairedAnimation(
		UPairedAnimationData* PairedData,
		USkeletalMesh* AttackerMesh = nullptr,
		USkeletalMesh* VictimMesh = nullptr);

	/**
	 * Validate PairedAnimationData configuration
	 * Checks for common issues and misconfigurations
	 *
	 * @param PairedData - Data to validate
	 * @param OutMessages - Validation messages
	 * @return True if valid (no errors)
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer|Validation")
	static bool ValidatePairedAnimationData(
		UPairedAnimationData* PairedData,
		TArray<FAnalysisMessage>& OutMessages);

	// ========================================================================
	// CONTACT POINT ANALYSIS
	// ========================================================================

	/**
	 * Predict contact points between attacker and victim montages
	 * Finds where bones are closest during the animations
	 *
	 * @param AttackerMontage - Attacker's montage
	 * @param VictimMontage - Victim's montage
	 * @param AttackerMesh - Attacker skeleton
	 * @param VictimMesh - Victim skeleton
	 * @param AttackerBones - Attacker bones to check (empty = weapon bones)
	 * @param VictimBones - Victim bones to check (empty = body regions)
	 * @param CharacterDistance - Distance between character origins
	 * @return Array of predicted contact points
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer|Contact")
	static TArray<FContactPointAnalysis> PredictContactPoints(
		UAnimMontage* AttackerMontage,
		UAnimMontage* VictimMontage,
		USkeletalMesh* AttackerMesh,
		USkeletalMesh* VictimMesh,
		const TArray<FName>& AttackerBones,
		const TArray<FName>& VictimBones,
		float CharacterDistance = 100.0f);

	/**
	 * Find the primary contact point (closest approach)
	 *
	 * @param AttackerMontage - Attacker's montage
	 * @param VictimMontage - Victim's montage
	 * @param AttackerMesh - Attacker skeleton
	 * @param VictimMesh - Victim skeleton
	 * @param CharacterDistance - Distance between character origins
	 * @return Primary contact point analysis
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer|Contact")
	static FContactPointAnalysis FindPrimaryContactPoint(
		UAnimMontage* AttackerMontage,
		UAnimMontage* VictimMontage,
		USkeletalMesh* AttackerMesh,
		USkeletalMesh* VictimMesh,
		float CharacterDistance = 100.0f);

	/**
	 * Analyze contact at specific sync point time
	 *
	 * @param AttackerMontage - Attacker's montage
	 * @param VictimMontage - Victim's montage
	 * @param AttackerMesh - Attacker skeleton
	 * @param VictimMesh - Victim skeleton
	 * @param SyncPointTime - Time of sync point
	 * @param AttackerBone - Attacker bone making contact
	 * @param VictimBone - Victim bone receiving contact
	 * @param CharacterDistance - Distance between character origins
	 * @return Contact point analysis at sync point
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer|Contact")
	static FContactPointAnalysis AnalyzeContactAtSyncPoint(
		UAnimMontage* AttackerMontage,
		UAnimMontage* VictimMontage,
		USkeletalMesh* AttackerMesh,
		USkeletalMesh* VictimMesh,
		float SyncPointTime,
		FName AttackerBone,
		FName VictimBone,
		float CharacterDistance = 100.0f);

	// ========================================================================
	// SYNC POINT ANALYSIS
	// ========================================================================

	/**
	 * Analyze sync point alignment between montages
	 *
	 * @param AttackerMontage - Attacker's montage
	 * @param VictimMontage - Victim's montage
	 * @param SyncPointTime - Expected sync point time
	 * @param VictimStartOffset - Victim montage start offset
	 * @return Sync point analysis
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer|Sync")
	static FSyncPointAnalysis AnalyzeSyncPoint(
		UAnimMontage* AttackerMontage,
		UAnimMontage* VictimMontage,
		float SyncPointTime,
		float VictimStartOffset = 0.0f);

	/**
	 * Find optimal sync point time based on contact analysis
	 * Searches for the time when contact bones are closest
	 *
	 * @param AttackerMontage - Attacker's montage
	 * @param VictimMontage - Victim's montage
	 * @param AttackerMesh - Attacker skeleton
	 * @param VictimMesh - Victim skeleton
	 * @param AttackerContactBone - Bone making contact
	 * @param VictimContactBone - Bone receiving contact
	 * @param CharacterDistance - Distance between character origins
	 * @return Recommended sync point time
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer|Sync")
	static float FindOptimalSyncPointTime(
		UAnimMontage* AttackerMontage,
		UAnimMontage* VictimMontage,
		USkeletalMesh* AttackerMesh,
		USkeletalMesh* VictimMesh,
		FName AttackerContactBone,
		FName VictimContactBone,
		float CharacterDistance = 100.0f);

	/**
	 * Validate sync point is within valid range for both montages
	 *
	 * @param AttackerMontage - Attacker's montage
	 * @param VictimMontage - Victim's montage
	 * @param SyncPointTime - Proposed sync point time
	 * @param VictimStartOffset - Victim start offset
	 * @param OutMessages - Validation messages
	 * @return True if sync point is valid
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer|Sync")
	static bool ValidateSyncPointTime(
		UAnimMontage* AttackerMontage,
		UAnimMontage* VictimMontage,
		float SyncPointTime,
		float VictimStartOffset,
		TArray<FAnalysisMessage>& OutMessages);

	// ========================================================================
	// REACH ANALYSIS
	// ========================================================================

	/**
	 * Analyze reach requirements for contact
	 *
	 * @param AttackerMesh - Attacker skeleton
	 * @param VictimMesh - Victim skeleton
	 * @param AttackerBone - Bone making contact
	 * @param VictimBone - Bone receiving contact
	 * @param CharacterDistance - Distance between character origins
	 * @return Reach analysis result
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer|Reach")
	static FReachAnalysis AnalyzeReachRequirement(
		USkeletalMesh* AttackerMesh,
		USkeletalMesh* VictimMesh,
		FName AttackerBone,
		FName VictimBone,
		float CharacterDistance);

	/**
	 * Calculate recommended warp distance based on reach
	 *
	 * @param AttackerMesh - Attacker skeleton
	 * @param VictimMesh - Victim skeleton
	 * @param AttackerContactBone - Bone making contact
	 * @param VictimContactBone - Bone receiving contact
	 * @param DesiredExtensionRatio - Desired extension (0.7 = comfortable)
	 * @return Recommended distance between characters
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer|Reach")
	static float CalculateRecommendedDistance(
		USkeletalMesh* AttackerMesh,
		USkeletalMesh* VictimMesh,
		FName AttackerContactBone,
		FName VictimContactBone,
		float DesiredExtensionRatio = 0.7f);

	// ========================================================================
	// TIMING SYNCHRONIZATION
	// ========================================================================

	/**
	 * Calculate optimal victim start offset
	 * Aligns victim animation to sync with attacker
	 *
	 * @param AttackerMontage - Attacker's montage
	 * @param VictimMontage - Victim's montage
	 * @param AttackerSyncTime - Time of sync point in attacker montage
	 * @param VictimSyncTime - Time of sync point in victim montage
	 * @return Recommended victim start offset
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer|Timing")
	static float CalculateVictimStartOffset(
		UAnimMontage* AttackerMontage,
		UAnimMontage* VictimMontage,
		float AttackerSyncTime,
		float VictimSyncTime);

	/**
	 * Check if montage lengths are compatible
	 *
	 * @param AttackerMontage - Attacker's montage
	 * @param VictimMontage - Victim's montage
	 * @param SyncPointTime - Sync point time
	 * @param OutMessages - Validation messages
	 * @return True if lengths are compatible
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer|Timing")
	static bool CheckMontageCompatibility(
		UAnimMontage* AttackerMontage,
		UAnimMontage* VictimMontage,
		float SyncPointTime,
		TArray<FAnalysisMessage>& OutMessages);

	// ========================================================================
	// AUTO-FILL RECOMMENDATIONS
	// ========================================================================

	/**
	 * Generate recommended values for PairedAnimationData
	 * Analyzes montages and suggests optimal settings
	 *
	 * @param PairedData - Data to generate recommendations for
	 * @param AttackerMesh - Attacker skeleton
	 * @param VictimMesh - Victim skeleton
	 * @param OutRecommendedSyncTime - Recommended sync point time
	 * @param OutRecommendedDistance - Recommended warp distance
	 * @param OutRecommendedVictimOffset - Recommended victim start offset
	 * @return True if recommendations could be generated
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer")
	static bool GenerateRecommendations(
		UPairedAnimationData* PairedData,
		USkeletalMesh* AttackerMesh,
		USkeletalMesh* VictimMesh,
		float& OutRecommendedSyncTime,
		float& OutRecommendedDistance,
		float& OutRecommendedVictimOffset);

	/**
	 * Auto-fill PairedAnimationData with analyzed values
	 * Modifies the asset with recommended settings
	 *
	 * @param PairedData - Data to fill
	 * @param AttackerMesh - Attacker skeleton
	 * @param VictimMesh - Victim skeleton
	 * @return True if auto-fill succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Montage Analyzer")
	static bool AutoFillPairedAnimationData(
		UPairedAnimationData* PairedData,
		USkeletalMesh* AttackerMesh,
		USkeletalMesh* VictimMesh);

protected:
	// ========================================================================
	// INTERNAL HELPERS
	// ========================================================================

	/** Get default attacker bones for contact analysis (weapon, hands) */
	static TArray<FName> GetDefaultAttackerContactBones();

	/** Get default victim bones for contact analysis (body regions) */
	static TArray<FName> GetDefaultVictimContactBones();

	/** Calculate distance between two bones at specific time */
	static float GetBoneDistanceAtTime(
		UAnimMontage* AttackerMontage,
		UAnimMontage* VictimMontage,
		USkeletalMesh* AttackerMesh,
		USkeletalMesh* VictimMesh,
		FName AttackerBone,
		FName VictimBone,
		float Time,
		float CharacterDistance);

	/** Find time of closest approach between two bones */
	static float FindClosestApproachTime(
		UAnimMontage* AttackerMontage,
		UAnimMontage* VictimMontage,
		USkeletalMesh* AttackerMesh,
		USkeletalMesh* VictimMesh,
		FName AttackerBone,
		FName VictimBone,
		float CharacterDistance,
		float& OutMinDistance);
};
