// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"

enum class EDefenseTelemetryEvent : uint8
{
	ThreatSelection,
	Resolution,
	PresentationStart,
	AlignmentRequest,
	AlignmentFrame,
	StageStart,
	StageTransition,
	StageDamage,
	Cleanup
};

/** One machine-readable observation from an authoritative defense owner. */
struct KATANACOMBAT_API FDefenseTelemetryRecord
{
	uint64 Sequence = 0;
	EDefenseTelemetryEvent Event = EDefenseTelemetryEvent::Resolution;
	FDefenseInteractionId InteractionId;
	FAttackInstanceId AttackInstance;
	FSoftObjectPath AttackDataPath;
	FAttackWindowInstanceId AttackWindow;
	int32 StageGeneration = 0;
	FName StageName = NAME_None;
	double SimulationTimestamp = 0.0;
	double UnscaledTimestamp = 0.0;
	TWeakObjectPtr<AActor> Defender;
	TWeakObjectPtr<AActor> Attacker;
	TWeakObjectPtr<AActor> Candidate;
	FCombatantStableId DefenderStableId;
	FCombatantStableId AttackerStableId;
	FCombatantStableId CandidateStableId;
	FCombatantStableId LockedThreatStableId;
	FTransform OwnerTransform = FTransform::Identity;
	FTransform CounterpartTransform = FTransform::Identity;
	FName CandidateDisposition = NAME_None;
	FName ThreatSwitchReason = NAME_None;
	EDefenseOutcome Outcome = EDefenseOutcome::Rejected;
	EDefenseReason Reason = EDefenseReason::None;
	EAttackHeight PredictedHeight = EAttackHeight::Middle;
	EIncomingAttackLane PredictedLane = EIncomingAttackLane::Center;
	ESwingDirection PredictedSwing = ESwingDirection::Horizontal;
	EAttackHeight ActualHeight = EAttackHeight::Middle;
	EIncomingAttackLane ActualLane = EIncomingAttackLane::Center;
	ESwingDirection ActualSwing = ESwingDirection::Horizontal;
	FVector PredictedAxis = FVector::ZeroVector;
	FVector ActualAxis = FVector::ZeroVector;
	float InitialYawError = 0.0f;
	float RemainingYawError = 0.0f;
	float TimeToDeadline = -1.0f;
	float MaximumTurnRate = 0.0f;
	float RemainingTurnBudget = 0.0f;
	FName AlignmentOwner = NAME_None;
	EAlignmentExecutor AlignmentExecutor = EAlignmentExecutor::None;
	float ConfiguredEngineWarpRate = 0.0f;
	float FrameSimulationDelta = 0.0f;
	float AppliedFrameYaw = 0.0f;
	float FinalFrameYawError = 0.0f;
	FVector FrameDisplacement = FVector::ZeroVector;
	FVector ExpectedAuthoredDisplacement = FVector::ZeroVector;
	FVector ExpectedWarpDisplacement = FVector::ZeroVector;
	FVector UnexpectedDisplacement = FVector::ZeroVector;
	float PelvisDelta = 0.0f;
	FName CacheDisposition = NAME_None;
	FName WeaponDisposition = NAME_None;
	FName SelectedPresentationRow = NAME_None;
	EDefensePresentationFallbackLevel PresentationFallback =
		EDefensePresentationFallbackLevel::NoPresentation;
	FName AttackerPresentationRow = NAME_None;
	EDefensePresentationFallbackLevel AttackerPresentationFallback =
		EDefensePresentationFallbackLevel::NoPresentation;
	FName CleanupReason = NAME_None;
};

namespace DefenseTelemetry
{
	KATANACOMBAT_API bool IsEnabled();
	KATANACOMBAT_API FDefenseTelemetryRecord FromResolution(
		const FDefenseResolution& Resolution,
		EDefenseTelemetryEvent Event);
	KATANACOMBAT_API FString BuildCsv(TConstArrayView<FDefenseTelemetryRecord> Records);
	KATANACOMBAT_API bool WriteCsv(
		const FString& RequestedPath,
		TConstArrayView<FDefenseTelemetryRecord> Records,
		FString& OutResolvedPath,
		FString& OutError);
}
