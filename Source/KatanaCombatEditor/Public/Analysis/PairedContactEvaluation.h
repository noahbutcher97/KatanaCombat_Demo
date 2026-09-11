// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include <limits>

/** A montage sync state intersecting one fresh, forward section playback.
 * Effective time clamps an already-active state to entry; authored clocks remain intact.
 * Damage flags describe configuration, never an observed runtime commit. */
struct FPairedSyncEvent
{
	FName Name;
	FName VictimBone;
	double NominalMontageTime = 0;
	double TriggerMontageTime = 0;
	double EndTriggerMontageTime = 0;
	double PairTime = 0;
	bool bActiveAtEntry = false;
	bool bPrimary = false;
	bool bDamageConfigured = false;
};

/** Editor-only measurement intent; names identify explicit role:bone/socket samples. */
struct FPairedContactRule
{
	FString Name;
	FString SourcePoint;
	FString SourceEndPoint; // Empty means point contact; otherwise the source is a line segment.
	FString TargetPoint;
	double StartSeconds = 0;
	double EndSeconds = 0;
	double TargetRadiusCm = 0;
	double MinimumGapCm = 0;
	double MaximumGapCm = 5;
	double MaximumSampleGapSeconds = 0.075;
	bool bSustained = false;
	bool bMeasureOrientation = false;
	FVector TargetLocalNormal = FVector::ForwardVector;
	double ExpectedAngleDegrees = 0;
	double AngleToleranceDegrees = 15;
};

struct FPairedContactPose
{
	double TimeSeconds = 0;
	double OriginalTimeSeconds = 0;
	double VictimTimeSeconds = std::numeric_limits<double>::quiet_NaN();
	bool bEligible = true;
	FString IneligibilityReason;
	TMap<FString, FTransform> Points;
};

struct FPairedContactObservation
{
	double TimeSeconds = 0;
	double OriginalTimeSeconds = 0;
	double SignedGapCm = 0;
	double AngleErrorDegrees = 0;
	bool bWithinCriteria = false;
};

enum class EPairedContactResult : uint8 { Pass, Fail, Inconclusive };

struct FPairedContactEvaluation
{
	EPairedContactResult Status = EPairedContactResult::Inconclusive;
	FString Reason;
	TArray<FPairedContactObservation> Observations;
	double MaximumSampleGapSeconds = 0;
	int32 MatchingSamples = 0;
};

/** Difference in victim-root relative to attacker-root, at the original attacker clock.
 * This is the geometric correction needed to match authored placement, not measured
 * motion-warp work: blending, locomotion and initial placement can also contribute. */
struct FPairedAlignmentRule
{
	FString Name;
	double StartSeconds = 0;
	double EndSeconds = 0;
	double MaximumSampleGapSeconds = 0.075;
	double MaximumTranslationCm = 0;
	double MaximumRotationDegrees = 0;
	double MaximumVictimTimingErrorSeconds = 0;
};

struct FPairedAlignmentObservation
{
	double TimeSeconds = 0;
	double OriginalTimeSeconds = 0;
	double TranslationCm = 0;
	double RotationDegrees = 0;
	double VictimTimingErrorSeconds = 0;
	bool bWithinCriteria = false;
};

struct FPairedAlignmentEvaluation
{
	EPairedContactResult Status = EPairedContactResult::Inconclusive;
	FString Reason;
	TArray<FPairedAlignmentObservation> Observations;
};
