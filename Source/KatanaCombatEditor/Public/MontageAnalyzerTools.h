// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MontageAnalysisTypes.h"
#include "MontageAnalyzerTools.generated.h"

class UAnimMontage;
class USkeletalMesh;
class UAnimSequence;
struct FAnimNotifyEvent;

/**
 * Montage Analyzer Tools
 *
 * Static utility functions for editor-time montage analysis:
 * - Timing extraction and validation
 * - Bone trajectory sampling and visualization
 * - Notify event analysis
 * - Root motion analysis
 *
 * Design: Base class for montage analysis. Subclassable for
 * specialized analysis (paired animations, combat sequences).
 *
 * All functions are editor-only with no runtime overhead.
 */
UCLASS()
class KATANACOMBATEDITOR_API UMontageAnalyzerTools : public UObject
{
	GENERATED_BODY()

public:
	// ========================================================================
	// TIMING ANALYSIS
	// ========================================================================

	/**
	 * Get full timing information for a montage
	 *
	 * @param Montage - Montage to analyze
	 * @param SectionName - Section to analyze (NAME_None for whole montage)
	 * @return Timing information struct
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Timing")
	static FMontageTimingInfo GetMontageTiming(UAnimMontage* Montage, FName SectionName = NAME_None);

	/**
	 * Get montage duration
	 *
	 * @param Montage - Montage to query
	 * @param SectionName - Section to query (NAME_None for whole montage)
	 * @return Duration in seconds
	 */
	UFUNCTION(BlueprintPure, Category = "Montage Analyzer|Timing")
	static float GetMontageDuration(UAnimMontage* Montage, FName SectionName = NAME_None);

	/**
	 * Get section start time
	 *
	 * @param Montage - Montage containing section
	 * @param SectionName - Section to query
	 * @return Section start time in seconds
	 */
	UFUNCTION(BlueprintPure, Category = "Montage Analyzer|Timing")
	static float GetSectionStartTime(UAnimMontage* Montage, FName SectionName);

	/**
	 * Get all section names in montage
	 *
	 * @param Montage - Montage to query
	 * @return Array of section names
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Timing")
	static TArray<FName> GetAllSectionNames(UAnimMontage* Montage);

	/**
	 * Find sync point time based on notify names
	 * Searches for notifies containing "Sync" in their class name
	 *
	 * @param Montage - Montage to search
	 * @param SectionName - Section to search within (NAME_None for all)
	 * @return Sync point time, or -1 if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Timing")
	static float FindSyncPointTime(UAnimMontage* Montage, FName SectionName = NAME_None);

	// ========================================================================
	// NOTIFY ANALYSIS
	// ========================================================================

	/**
	 * Get all notifies within a time range
	 *
	 * @param Montage - Montage to search
	 * @param StartTime - Range start (montage time)
	 * @param EndTime - Range end (montage time)
	 * @return Array of notify events
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Notifies")
	static TArray<FAnimNotifyEvent> GetNotifiesInRange(UAnimMontage* Montage, float StartTime, float EndTime);

	/**
	 * Get notifies of a specific class
	 *
	 * @param Montage - Montage to search
	 * @param NotifyClass - Class to filter by
	 * @param SectionName - Section to search (NAME_None for all)
	 * @return Array of notify events of this class
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Notifies")
	static TArray<FAnimNotifyEvent> GetNotifiesOfClass(UAnimMontage* Montage, UClass* NotifyClass, FName SectionName = NAME_None);

	/**
	 * Check if montage has a specific notify type
	 *
	 * @param Montage - Montage to check
	 * @param NotifyClass - Class to search for
	 * @return True if at least one notify of this class exists
	 */
	UFUNCTION(BlueprintPure, Category = "Montage Analyzer|Notifies")
	static bool HasNotifyOfClass(UAnimMontage* Montage, UClass* NotifyClass);

	/**
	 * Get all notify class names in montage
	 * Useful for debugging and analysis display
	 *
	 * @param Montage - Montage to analyze
	 * @return Array of unique notify class names
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Notifies")
	static TArray<FString> GetNotifyClassNames(UAnimMontage* Montage);

	// ========================================================================
	// BONE TRAJECTORY ANALYSIS
	// ========================================================================

	/**
	 * Sample bone trajectory through montage
	 * Captures position, velocity, and speed at each sample
	 *
	 * @param Montage - Montage to sample
	 * @param SkeletalMesh - Skeleton to evaluate poses on
	 * @param BoneName - Bone to track
	 * @param SampleCount - Number of samples (higher = more accurate)
	 * @param SectionName - Section to sample (NAME_None for whole montage)
	 * @return Trajectory data with all samples
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Trajectory")
	static FBoneTrajectoryData SampleBoneTrajectory(
		UAnimMontage* Montage,
		USkeletalMesh* SkeletalMesh,
		FName BoneName,
		int32 SampleCount = 30,
		FName SectionName = NAME_None);

	/**
	 * Get bone velocity at specific time
	 *
	 * @param Montage - Montage to evaluate
	 * @param SkeletalMesh - Skeleton to use
	 * @param BoneName - Bone to query
	 * @param Time - Time in montage
	 * @param DeltaTime - Sample delta for velocity calculation
	 * @return Velocity vector
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Trajectory")
	static FVector GetBoneVelocityAtTime(
		UAnimMontage* Montage,
		USkeletalMesh* SkeletalMesh,
		FName BoneName,
		float Time,
		float DeltaTime = 0.016f);

	/**
	 * Get maximum bone speed during montage
	 *
	 * @param Montage - Montage to analyze
	 * @param SkeletalMesh - Skeleton to use
	 * @param BoneName - Bone to track
	 * @param SectionName - Section to analyze (NAME_None for whole montage)
	 * @return Maximum speed reached
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Trajectory")
	static float GetMaxBoneSpeed(
		UAnimMontage* Montage,
		USkeletalMesh* SkeletalMesh,
		FName BoneName,
		FName SectionName = NAME_None);

	/**
	 * Get bone transform at specific time
	 *
	 * @param Montage - Montage to evaluate
	 * @param SkeletalMesh - Skeleton to use
	 * @param BoneName - Bone to query
	 * @param Time - Time in montage
	 * @return Bone transform in component space
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Trajectory")
	static FTransform GetBoneTransformAtTime(
		UAnimMontage* Montage,
		USkeletalMesh* SkeletalMesh,
		FName BoneName,
		float Time);

	/**
	 * Sample multiple bones simultaneously (more efficient)
	 *
	 * @param Montage - Montage to sample
	 * @param SkeletalMesh - Skeleton to use
	 * @param BoneNames - Bones to track
	 * @param SampleCount - Number of samples
	 * @param SectionName - Section to sample
	 * @return Map of bone name to trajectory data
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Trajectory")
	static TMap<FName, FBoneTrajectoryData> SampleMultipleBoneTrajectories(
		UAnimMontage* Montage,
		USkeletalMesh* SkeletalMesh,
		const TArray<FName>& BoneNames,
		int32 SampleCount = 30,
		FName SectionName = NAME_None);

	// ========================================================================
	// ROOT MOTION ANALYSIS
	// ========================================================================

	/**
	 * Get total root motion distance in montage
	 *
	 * @param Montage - Montage to analyze
	 * @param SectionName - Section to analyze (NAME_None for whole montage)
	 * @return Total distance traveled by root
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Root Motion")
	static float GetRootMotionDistance(UAnimMontage* Montage, FName SectionName = NAME_None);

	/**
	 * Get root motion transform at time
	 *
	 * @param Montage - Montage to evaluate
	 * @param Time - Time in montage
	 * @return Root motion transform accumulated to this time
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Root Motion")
	static FTransform GetRootMotionAtTime(UAnimMontage* Montage, float Time);

	/**
	 * Check if montage has root motion
	 *
	 * @param Montage - Montage to check
	 * @return True if montage contains root motion
	 */
	UFUNCTION(BlueprintPure, Category = "Montage Analyzer|Root Motion")
	static bool HasRootMotion(UAnimMontage* Montage);

	/**
	 * Get root motion direction at time
	 *
	 * @param Montage - Montage to evaluate
	 * @param Time - Time in montage
	 * @return Normalized root motion direction
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Root Motion")
	static FVector GetRootMotionDirectionAtTime(UAnimMontage* Montage, float Time);

	// ========================================================================
	// VALIDATION
	// ========================================================================

	/**
	 * Validate montage configuration
	 * Checks for common issues
	 *
	 * @param Montage - Montage to validate
	 * @param OutMessages - Validation messages
	 * @return True if valid (no errors, warnings acceptable)
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer|Validation")
	static bool ValidateMontage(UAnimMontage* Montage, TArray<FAnalysisMessage>& OutMessages);

	/**
	 * Check if montage section exists
	 *
	 * @param Montage - Montage to check
	 * @param SectionName - Section to look for
	 * @return True if section exists
	 */
	UFUNCTION(BlueprintPure, Category = "Montage Analyzer|Validation")
	static bool DoesSectionExist(UAnimMontage* Montage, FName SectionName);

	/**
	 * Check if bone exists in skeleton
	 *
	 * @param SkeletalMesh - Skeleton to check
	 * @param BoneName - Bone to look for
	 * @return True if bone exists
	 */
	UFUNCTION(BlueprintPure, Category = "Montage Analyzer|Validation")
	static bool DoesBoneExist(USkeletalMesh* SkeletalMesh, FName BoneName);

	// ========================================================================
	// COMPLETE ANALYSIS
	// ========================================================================

	/**
	 * Run complete montage analysis
	 *
	 * @param Montage - Montage to analyze
	 * @param SkeletalMesh - Skeleton for bone analysis (optional)
	 * @param BonesToAnalyze - Specific bones to track (empty = key bones only)
	 * @param SectionName - Section to analyze (NAME_None for whole montage)
	 * @return Complete analysis result
	 */
	UFUNCTION(BlueprintCallable, Category = "Montage Analyzer")
	static FMontageAnalysisResult AnalyzeMontage(
		UAnimMontage* Montage,
		USkeletalMesh* SkeletalMesh,
		const TArray<FName>& BonesToAnalyze,
		FName SectionName = NAME_None);

protected:
	// ========================================================================
	// INTERNAL HELPERS
	// ========================================================================

	/** Get the anim sequence from montage slot at time */
	static UAnimSequence* GetAnimSequenceAtTime(UAnimMontage* Montage, float Time);

	/** Convert montage time to sequence time */
	static float MontageTimeToSequenceTime(UAnimMontage* Montage, float MontageTime);

	/** Get default bones for analysis (hands, feet, weapon sockets) */
	static TArray<FName> GetDefaultAnalysisBones();

	/** Evaluate pose at time on skeleton */
	static bool EvaluatePoseAtTime(
		UAnimMontage* Montage,
		USkeletalMesh* SkeletalMesh,
		float Time,
		TArray<FTransform>& OutBoneTransforms);
};
