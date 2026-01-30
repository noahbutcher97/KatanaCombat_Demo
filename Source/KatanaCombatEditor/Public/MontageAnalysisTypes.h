// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/CombatMathTypes.h"
#include "MontageAnalysisTypes.generated.h"

class UAnimMontage;
class UPairedAnimationData;

/**
 * Severity level for analysis messages
 */
UENUM(BlueprintType)
enum class EAnalysisMessageSeverity : uint8
{
	Info		UMETA(DisplayName = "Info"),
	Warning		UMETA(DisplayName = "Warning"),
	Error		UMETA(DisplayName = "Error")
};

/**
 * A single analysis message with severity
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FAnalysisMessage
{
	GENERATED_BODY()

	/** Message severity */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	EAnalysisMessageSeverity Severity = EAnalysisMessageSeverity::Info;

	/** Human-readable message */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	FText Message;

	/** Optional: Time in montage where issue occurs */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	float MontageTime = -1.0f;

	/** Optional: Bone name related to issue */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	FName RelatedBone = NAME_None;

	FAnalysisMessage() = default;

	FAnalysisMessage(EAnalysisMessageSeverity InSeverity, const FText& InMessage)
		: Severity(InSeverity), Message(InMessage) {}

	FAnalysisMessage(EAnalysisMessageSeverity InSeverity, const FText& InMessage, float InTime)
		: Severity(InSeverity), Message(InMessage), MontageTime(InTime) {}
};

/**
 * Timing information extracted from a montage
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FMontageTimingInfo
{
	GENERATED_BODY()

	/** Total montage duration */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float TotalDuration = 0.0f;

	/** Section start time (0 if whole montage) */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float SectionStartTime = 0.0f;

	/** Section duration */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float SectionDuration = 0.0f;

	/** Section name (NAME_None if whole montage) */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	FName SectionName = NAME_None;

	/** Notify times within section (relative to section start) */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	TArray<float> NotifyTimes;

	/** Notify names within section */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	TArray<FName> NotifyNames;

	/** Playback rate of the montage */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float PlayRate = 1.0f;
};

/**
 * Bone trajectory sample at a specific frame
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FBoneTrajectorySample
{
	GENERATED_BODY()

	/** Time in montage */
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	float Time = 0.0f;

	/** World-space transform of bone */
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	FTransform Transform;

	/** Velocity at this sample (calculated from neighbors) */
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	FVector Velocity = FVector::ZeroVector;

	/** Speed at this sample */
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	float Speed = 0.0f;
};

/**
 * Complete trajectory data for a bone through a montage
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FBoneTrajectoryData
{
	GENERATED_BODY()

	/** Bone being tracked */
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	FName BoneName = NAME_None;

	/** All samples along trajectory */
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	TArray<FBoneTrajectorySample> Samples;

	/** Maximum speed reached */
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	float MaxSpeed = 0.0f;

	/** Time of maximum speed */
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	float TimeOfMaxSpeed = 0.0f;

	/** Total distance traveled */
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	float TotalDistance = 0.0f;

	/** Bounding box of trajectory */
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	FBox TrajectoryBounds;

	FBoneTrajectoryData()
		: TrajectoryBounds(ForceInit) {}
};

/**
 * Contact point analysis between two montages
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FContactPointAnalysis
{
	GENERATED_BODY()

	/** Attacker bone that makes contact */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	FName AttackerBone = NAME_None;

	/** Victim bone that receives contact */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	FName VictimBone = NAME_None;

	/** Predicted contact location (world space, relative to sync setup) */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	FVector ContactLocation = FVector::ZeroVector;

	/** Contact surface normal */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	FVector ContactNormal = FVector::UpVector;

	/** Time of contact in attacker montage */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	float AttackerContactTime = 0.0f;

	/** Time of contact in victim montage */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	float VictimContactTime = 0.0f;

	/** Confidence in prediction (0-1) */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	float Confidence = 0.0f;

	/** Is this the primary contact point? */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	bool bIsPrimaryContact = false;

	/** Distance between bones at predicted contact */
	UPROPERTY(BlueprintReadOnly, Category = "Contact")
	float DistanceAtContact = 0.0f;
};

/**
 * Reach analysis for a paired animation
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FReachAnalysis
{
	GENERATED_BODY()

	/** Is the reach achievable with current positioning? */
	UPROPERTY(BlueprintReadOnly, Category = "Reach")
	bool bIsReachable = false;

	/** Required reach distance */
	UPROPERTY(BlueprintReadOnly, Category = "Reach")
	float RequiredReach = 0.0f;

	/** Available reach (from skeleton chain) */
	UPROPERTY(BlueprintReadOnly, Category = "Reach")
	float AvailableReach = 0.0f;

	/** Extension ratio (1.0 = full extension) */
	UPROPERTY(BlueprintReadOnly, Category = "Reach")
	float ExtensionRatio = 0.0f;

	/** Effector bone being extended */
	UPROPERTY(BlueprintReadOnly, Category = "Reach")
	FName EffectorBone = NAME_None;

	/** Root bone of the chain */
	UPROPERTY(BlueprintReadOnly, Category = "Reach")
	FName ChainRoot = NAME_None;

	/** Suggested position adjustment to achieve reach */
	UPROPERTY(BlueprintReadOnly, Category = "Reach")
	FVector SuggestedAdjustment = FVector::ZeroVector;
};

/**
 * Sync point analysis between paired montages
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FSyncPointAnalysis
{
	GENERATED_BODY()

	/** Sync point name */
	UPROPERTY(BlueprintReadOnly, Category = "Sync")
	FName SyncPointName = NAME_None;

	/** Time in attacker montage */
	UPROPERTY(BlueprintReadOnly, Category = "Sync")
	float AttackerTime = 0.0f;

	/** Time in victim montage */
	UPROPERTY(BlueprintReadOnly, Category = "Sync")
	float VictimTime = 0.0f;

	/** Expected distance between characters at sync */
	UPROPERTY(BlueprintReadOnly, Category = "Sync")
	float ExpectedDistance = 0.0f;

	/** Is alignment valid? */
	UPROPERTY(BlueprintReadOnly, Category = "Sync")
	bool bIsAligned = true;

	/** Alignment error distance (if any) */
	UPROPERTY(BlueprintReadOnly, Category = "Sync")
	float AlignmentError = 0.0f;

	/** Timing difference (attacker vs victim at their sync points) */
	UPROPERTY(BlueprintReadOnly, Category = "Sync")
	float TimingDifference = 0.0f;
};

/**
 * Complete analysis result for a single montage
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FMontageAnalysisResult
{
	GENERATED_BODY()

	/** Montage that was analyzed */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	TWeakObjectPtr<UAnimMontage> AnalyzedMontage;

	/** Timing information */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	FMontageTimingInfo TimingInfo;

	/** Bone trajectories that were analyzed */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	TMap<FName, FBoneTrajectoryData> BoneTrajectories;

	/** Analysis messages (warnings, errors) */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	TArray<FAnalysisMessage> Messages;

	/** Was analysis successful? */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	bool bAnalysisSuccessful = false;

	/** Has any errors? */
	bool HasErrors() const
	{
		for (const FAnalysisMessage& Msg : Messages)
		{
			if (Msg.Severity == EAnalysisMessageSeverity::Error)
			{
				return true;
			}
		}
		return false;
	}

	/** Has any warnings? */
	bool HasWarnings() const
	{
		for (const FAnalysisMessage& Msg : Messages)
		{
			if (Msg.Severity == EAnalysisMessageSeverity::Warning)
			{
				return true;
			}
		}
		return false;
	}
};

/**
 * Complete analysis result for a paired animation (two montages)
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FPairedMontageAnalysisResult
{
	GENERATED_BODY()

	/** PairedAnimationData that was analyzed */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	TWeakObjectPtr<UPairedAnimationData> AnalyzedData;

	/** Attacker montage analysis */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	FMontageAnalysisResult AttackerAnalysis;

	/** Victim montage analysis */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	FMontageAnalysisResult VictimAnalysis;

	/** Contact point predictions */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	TArray<FContactPointAnalysis> ContactPoints;

	/** Reach analysis results */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	TArray<FReachAnalysis> ReachAnalyses;

	/** Sync point analysis */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	TArray<FSyncPointAnalysis> SyncPointAnalyses;

	/** Combined messages from both montages */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	TArray<FAnalysisMessage> CombinedMessages;

	/** Overall analysis success */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	bool bAnalysisSuccessful = false;

	/** Overall validity (no blocking errors) */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	bool bIsValid = false;

	/** Recommended warp distance based on analysis */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	float RecommendedWarpDistance = 0.0f;

	/** Recommended sync point time based on analysis */
	UPROPERTY(BlueprintReadOnly, Category = "Analysis")
	float RecommendedSyncPointTime = 0.0f;
};
