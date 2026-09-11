// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActionQueueTypes.h"
#include "CombatTypes.h"

enum class EActionReactionTelemetryEvent : uint8
{
	InputCaptured,
	InputFinalized,
	QueueAccepted,
	QueueRejected,
	QueueCancelled,
	ActionExecutionStarted,
	ActionExecutionFinished,
	PhaseChanged,
	InputContextChanged,
	MontageCallbackAccepted,
	MontageCallbackRejected,
	TerminalReset,
	HoldStateChanged,
	HoldReleaseRejected,
	MovementStateChanged,
	PairedStageMarkerAccepted,
	PairedStageMarkerRejected,
	PairedStageStartSucceeded,
	PairedStageStartFailed,
	ReactionDecision,
	AITokenChanged,
	AlignmentChanged,
	OrbitMoveChanged,
	AnimationLaneChanged,
	MovementInputDecisionChanged
};

enum class EActionReactionTelemetryReason : uint8
{
	None,
	Captured,
	MissingCombatSettings,
	CombatStateRejected,
	InputConsumed,
	InputCanceled,
	StatefulControlConsumed,
	StatefulControlRejected,
	ChainAdvanced,
	ChainExpired,
	DuplicatePendingInput,
	PendingInputReplaced,
	InvalidComboBranch,
	FreshChainReset,
	PriorityCancelled,
	ExplicitQueueClear,
	AttackConsumed,
	HoldReleasedToIdle,
	AttackResolutionFailed,
	Queued,
	ImmediateExecutionSucceeded,
	ImmediateExecutionFailed,
	CheckpointNotReached,
	MontageEndedBeforeCheckpoint,
	Executed,
	ExecutionFailed,
	MontageCompleted,
	MontageInterrupted,
	StaleMontageCallback,
	TerminalCleanup,
	HoldActivated,
	HoldReleased,
	HoldSourceRejected,
	StaleHoldGeneration,
	MissingAttackContext,
	MovementDisabled,
	MovementRestored,
	MarkerAccepted,
	MarkerContextInvalid,
	MarkerParticipantInvalid,
	MarkerReporterMismatch,
	MarkerDriverRoleMismatch,
	MarkerMontageInstanceMismatch,
	MarkerNotifySourceMismatch,
	MarkerNotifyIndexInvalid,
	MarkerPolicyMismatch,
	MarkerStateMismatch,
	StagePreflightFailed,
	StageDependenciesMissing,
	StageOwnershipFailed,
	StagePlaybackFailed,
	StageStarted,
	TokenGranted,
	TokenReleased,
	AlignmentRequested,
	AlignmentReleased,
	MovementInputCleared,
	MovementInputAllowed,
	MovementInputSuppressedByHold,
	MovementInputSuppressedByPaired,
	MovementInputSuppressedByTerminalState
};

/** One observational event spanning input, action, reaction, AI, alignment, and animation ownership. */
struct KATANACOMBAT_API FActionReactionTelemetryRecord
{
	uint64 Sequence = 0;
	EActionReactionTelemetryEvent Event = EActionReactionTelemetryEvent::InputCaptured;
	EActionReactionTelemetryReason Reason = EActionReactionTelemetryReason::None;
	double SimulationTimestamp = 0.0;
	double UnscaledTimestamp = 0.0;
	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<AActor> Counterpart;
	FString ActorPathSnapshot;
	FString CounterpartPathSnapshot;
	FCombatantStableId ActorStableId;
	FCombatantStableId CounterpartStableId;
	uint64 InputSerial = 0;
	uint64 QueueEntryId = 0;
	int32 AttackGeneration = INDEX_NONE;
	int32 PrimaryActionGeneration = INDEX_NONE;
	int32 HoldGeneration = 0;
	int32 MontageInstanceId = INDEX_NONE;
	FSoftObjectPath AttackDataPath;
	FSoftObjectPath MontagePath;
	FSoftObjectPath NotifySourcePath;
	EInputType InputType = EInputType::None;
	EInputEventType InputEvent = EInputEventType::Press;
	EInputDirection InputDirection = EInputDirection::None;
	ECombatInputRoute InputRoute = ECombatInputRoute::NormalQueue;
	ECombatInputDisposition InputDisposition = ECombatInputDisposition::Captured;
	EActionExecutionMode ExecutionMode = EActionExecutionMode::Queued;
	EActionState ActionState = EActionState::Pending;
	EAttackPhase AttackPhase = EAttackPhase::None;
	EInputContext InputContext = EInputContext::Movement;
	FName ActionName = NAME_None;
	int32 QueueDepth = 0;
	FName MovementDisposition = NAME_None;
	FName CharacterMovementMode = NAME_None;
	uint8 CharacterCustomMovementMode = 0;
	float MovementMagnitude = 0.0f;
	bool bRootMotionActive = false;
	FName TokenDisposition = NAME_None;
	int32 ActiveTokenCount = 0;
	FName AlignmentOwner = NAME_None;
	FName AlignmentDisposition = NAME_None;
	float RequestedYaw = 0.0f;
	float AppliedYaw = 0.0f;
	float RemainingYawError = 0.0f;
	FVector OrbitDestination = FVector::ZeroVector;
	float OrbitRadialError = 0.0f;
	FName ReactionClass = NAME_None;
	FName ReactionDisposition = NAME_None;
	FName AnimationLane = NAME_None;
	FName AnimationFallback = NAME_None;
	float WeaponSocketDrift = 0.0f;
	FName TerminalDisposition = NAME_None;
	FString Detail;
};

/** Bounded component-owned storage with monotonic sequence assignment. */
class KATANACOMBAT_API FActionReactionTelemetryBuffer
{
public:
	explicit FActionReactionTelemetryBuffer(int32 InCapacity = 1024);

	void Append(FActionReactionTelemetryRecord Record);
	void Reset();
	const TArray<FActionReactionTelemetryRecord>& GetRecords() const { return Records; }
	int32 GetCapacity() const { return Capacity; }

private:
	TArray<FActionReactionTelemetryRecord> Records;
	uint64 NextSequence = 0;
	int32 Capacity = 1024;
};

namespace ActionReactionTelemetry
{
	KATANACOMBAT_API bool IsEnabled();
	KATANACOMBAT_API FString BuildCsv(TConstArrayView<FActionReactionTelemetryRecord> Records);
	KATANACOMBAT_API bool WriteCsv(
		const FString& RequestedPath,
		TConstArrayView<FActionReactionTelemetryRecord> Records,
		FString& OutResolvedPath,
		FString& OutError);
}
