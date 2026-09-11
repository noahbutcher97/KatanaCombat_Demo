// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/ActionReactionTelemetry.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ActionReactionTelemetryPrivate
{
FString EventName(const EActionReactionTelemetryEvent Event)
{
	switch (Event)
	{
	case EActionReactionTelemetryEvent::InputCaptured: return TEXT("InputCaptured");
	case EActionReactionTelemetryEvent::InputFinalized: return TEXT("InputFinalized");
	case EActionReactionTelemetryEvent::QueueAccepted: return TEXT("QueueAccepted");
	case EActionReactionTelemetryEvent::QueueRejected: return TEXT("QueueRejected");
	case EActionReactionTelemetryEvent::QueueCancelled: return TEXT("QueueCancelled");
	case EActionReactionTelemetryEvent::ActionExecutionStarted: return TEXT("ActionExecutionStarted");
	case EActionReactionTelemetryEvent::ActionExecutionFinished: return TEXT("ActionExecutionFinished");
	case EActionReactionTelemetryEvent::PhaseChanged: return TEXT("PhaseChanged");
	case EActionReactionTelemetryEvent::InputContextChanged: return TEXT("InputContextChanged");
	case EActionReactionTelemetryEvent::MontageCallbackAccepted: return TEXT("MontageCallbackAccepted");
	case EActionReactionTelemetryEvent::MontageCallbackRejected: return TEXT("MontageCallbackRejected");
	case EActionReactionTelemetryEvent::TerminalReset: return TEXT("TerminalReset");
	case EActionReactionTelemetryEvent::HoldStateChanged: return TEXT("HoldStateChanged");
	case EActionReactionTelemetryEvent::HoldReleaseRejected: return TEXT("HoldReleaseRejected");
	case EActionReactionTelemetryEvent::MovementStateChanged: return TEXT("MovementStateChanged");
	case EActionReactionTelemetryEvent::PairedStageMarkerAccepted: return TEXT("PairedStageMarkerAccepted");
	case EActionReactionTelemetryEvent::PairedStageMarkerRejected: return TEXT("PairedStageMarkerRejected");
	case EActionReactionTelemetryEvent::PairedStageStartSucceeded: return TEXT("PairedStageStartSucceeded");
	case EActionReactionTelemetryEvent::PairedStageStartFailed: return TEXT("PairedStageStartFailed");
	case EActionReactionTelemetryEvent::ReactionDecision: return TEXT("ReactionDecision");
	case EActionReactionTelemetryEvent::AITokenChanged: return TEXT("AITokenChanged");
	case EActionReactionTelemetryEvent::AlignmentChanged: return TEXT("AlignmentChanged");
	case EActionReactionTelemetryEvent::OrbitMoveChanged: return TEXT("OrbitMoveChanged");
	case EActionReactionTelemetryEvent::AnimationLaneChanged: return TEXT("AnimationLaneChanged");
	case EActionReactionTelemetryEvent::MovementInputDecisionChanged: return TEXT("MovementInputDecisionChanged");
	default: return TEXT("Unknown");
	}
}

FString ReasonName(const EActionReactionTelemetryReason Reason)
{
	switch (Reason)
	{
	case EActionReactionTelemetryReason::None: return TEXT("None");
	case EActionReactionTelemetryReason::Captured: return TEXT("Captured");
	case EActionReactionTelemetryReason::MissingCombatSettings: return TEXT("MissingCombatSettings");
	case EActionReactionTelemetryReason::CombatStateRejected: return TEXT("CombatStateRejected");
	case EActionReactionTelemetryReason::InputConsumed: return TEXT("InputConsumed");
	case EActionReactionTelemetryReason::InputCanceled: return TEXT("InputCanceled");
	case EActionReactionTelemetryReason::StatefulControlConsumed: return TEXT("StatefulControlConsumed");
	case EActionReactionTelemetryReason::StatefulControlRejected: return TEXT("StatefulControlRejected");
	case EActionReactionTelemetryReason::ChainAdvanced: return TEXT("ChainAdvanced");
	case EActionReactionTelemetryReason::ChainExpired: return TEXT("ChainExpired");
	case EActionReactionTelemetryReason::DuplicatePendingInput: return TEXT("DuplicatePendingInput");
	case EActionReactionTelemetryReason::PendingInputReplaced: return TEXT("PendingInputReplaced");
	case EActionReactionTelemetryReason::InvalidComboBranch: return TEXT("InvalidComboBranch");
	case EActionReactionTelemetryReason::FreshChainReset: return TEXT("FreshChainReset");
	case EActionReactionTelemetryReason::PriorityCancelled: return TEXT("PriorityCancelled");
	case EActionReactionTelemetryReason::ExplicitQueueClear: return TEXT("ExplicitQueueClear");
	case EActionReactionTelemetryReason::AttackConsumed: return TEXT("AttackConsumed");
	case EActionReactionTelemetryReason::HoldReleasedToIdle: return TEXT("HoldReleasedToIdle");
	case EActionReactionTelemetryReason::AttackResolutionFailed: return TEXT("AttackResolutionFailed");
	case EActionReactionTelemetryReason::Queued: return TEXT("Queued");
	case EActionReactionTelemetryReason::ImmediateExecutionSucceeded: return TEXT("ImmediateExecutionSucceeded");
	case EActionReactionTelemetryReason::ImmediateExecutionFailed: return TEXT("ImmediateExecutionFailed");
	case EActionReactionTelemetryReason::CheckpointNotReached: return TEXT("CheckpointNotReached");
	case EActionReactionTelemetryReason::MontageEndedBeforeCheckpoint: return TEXT("MontageEndedBeforeCheckpoint");
	case EActionReactionTelemetryReason::Executed: return TEXT("Executed");
	case EActionReactionTelemetryReason::ExecutionFailed: return TEXT("ExecutionFailed");
	case EActionReactionTelemetryReason::MontageCompleted: return TEXT("MontageCompleted");
	case EActionReactionTelemetryReason::MontageInterrupted: return TEXT("MontageInterrupted");
	case EActionReactionTelemetryReason::StaleMontageCallback: return TEXT("StaleMontageCallback");
	case EActionReactionTelemetryReason::TerminalCleanup: return TEXT("TerminalCleanup");
	case EActionReactionTelemetryReason::HoldActivated: return TEXT("HoldActivated");
	case EActionReactionTelemetryReason::HoldReleased: return TEXT("HoldReleased");
	case EActionReactionTelemetryReason::HoldSourceRejected: return TEXT("HoldSourceRejected");
	case EActionReactionTelemetryReason::StaleHoldGeneration: return TEXT("StaleHoldGeneration");
	case EActionReactionTelemetryReason::MissingAttackContext: return TEXT("MissingAttackContext");
	case EActionReactionTelemetryReason::MovementDisabled: return TEXT("MovementDisabled");
	case EActionReactionTelemetryReason::MovementRestored: return TEXT("MovementRestored");
	case EActionReactionTelemetryReason::MarkerAccepted: return TEXT("MarkerAccepted");
	case EActionReactionTelemetryReason::MarkerContextInvalid: return TEXT("MarkerContextInvalid");
	case EActionReactionTelemetryReason::MarkerParticipantInvalid: return TEXT("MarkerParticipantInvalid");
	case EActionReactionTelemetryReason::MarkerReporterMismatch: return TEXT("MarkerReporterMismatch");
	case EActionReactionTelemetryReason::MarkerDriverRoleMismatch: return TEXT("MarkerDriverRoleMismatch");
	case EActionReactionTelemetryReason::MarkerMontageInstanceMismatch: return TEXT("MarkerMontageInstanceMismatch");
	case EActionReactionTelemetryReason::MarkerNotifySourceMismatch: return TEXT("MarkerNotifySourceMismatch");
	case EActionReactionTelemetryReason::MarkerNotifyIndexInvalid: return TEXT("MarkerNotifyIndexInvalid");
	case EActionReactionTelemetryReason::MarkerPolicyMismatch: return TEXT("MarkerPolicyMismatch");
	case EActionReactionTelemetryReason::MarkerStateMismatch: return TEXT("MarkerStateMismatch");
	case EActionReactionTelemetryReason::StagePreflightFailed: return TEXT("StagePreflightFailed");
	case EActionReactionTelemetryReason::StageDependenciesMissing: return TEXT("StageDependenciesMissing");
	case EActionReactionTelemetryReason::StageOwnershipFailed: return TEXT("StageOwnershipFailed");
	case EActionReactionTelemetryReason::StagePlaybackFailed: return TEXT("StagePlaybackFailed");
	case EActionReactionTelemetryReason::StageStarted: return TEXT("StageStarted");
	case EActionReactionTelemetryReason::TokenGranted: return TEXT("TokenGranted");
	case EActionReactionTelemetryReason::TokenReleased: return TEXT("TokenReleased");
	case EActionReactionTelemetryReason::AlignmentRequested: return TEXT("AlignmentRequested");
	case EActionReactionTelemetryReason::AlignmentReleased: return TEXT("AlignmentReleased");
	case EActionReactionTelemetryReason::MovementInputCleared: return TEXT("MovementInputCleared");
	case EActionReactionTelemetryReason::MovementInputAllowed: return TEXT("MovementInputAllowed");
	case EActionReactionTelemetryReason::MovementInputSuppressedByHold: return TEXT("MovementInputSuppressedByHold");
	case EActionReactionTelemetryReason::MovementInputSuppressedByPaired: return TEXT("MovementInputSuppressedByPaired");
	case EActionReactionTelemetryReason::MovementInputSuppressedByTerminalState: return TEXT("MovementInputSuppressedByTerminalState");
	default: return TEXT("Unknown");
	}
}

template <typename TEnum>
FString EnumName(const TEnum Value)
{
	const UEnum* Enum = StaticEnum<TEnum>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Value)) : TEXT("Unknown");
}

FString CsvField(FString Value)
{
	const bool bNeedsQuotes = Value.Contains(TEXT(","))
		|| Value.Contains(TEXT("\""))
		|| Value.Contains(TEXT("\r"))
		|| Value.Contains(TEXT("\n"));
	if (!bNeedsQuotes)
	{
		return Value;
	}
	Value.ReplaceInline(TEXT("\""), TEXT("\"\""));
	return FString::Printf(TEXT("\"%s\""), *Value);
}

FString ActorPath(const TWeakObjectPtr<AActor>& Actor, const FString& Snapshot)
{
	if (!Snapshot.IsEmpty())
	{
		return Snapshot;
	}
	return Actor.IsValid() ? Actor->GetPathName() : FString();
}

FString BuildRow(const FActionReactionTelemetryRecord& Record)
{
	TArray<FString> Fields;
	Fields.Reserve(53);
	Fields.Add(TEXT("2"));
	Fields.Add(FString::Printf(TEXT("%llu"), Record.Sequence));
	Fields.Add(EventName(Record.Event));
	Fields.Add(ReasonName(Record.Reason));
	Fields.Add(FString::Printf(TEXT("%.9f"), Record.SimulationTimestamp));
	Fields.Add(FString::Printf(TEXT("%.9f"), Record.UnscaledTimestamp));
	Fields.Add(ActorPath(Record.Actor, Record.ActorPathSnapshot));
	Fields.Add(ActorPath(Record.Counterpart, Record.CounterpartPathSnapshot));
	Fields.Add(FString::Printf(TEXT("%llu"), Record.ActorStableId.Value));
	Fields.Add(FString::Printf(TEXT("%llu"), Record.CounterpartStableId.Value));
	Fields.Add(FString::Printf(TEXT("%llu"), Record.InputSerial));
	Fields.Add(FString::Printf(TEXT("%llu"), Record.QueueEntryId));
	Fields.Add(FString::FromInt(Record.AttackGeneration));
	Fields.Add(FString::FromInt(Record.PrimaryActionGeneration));
	Fields.Add(FString::FromInt(Record.HoldGeneration));
	Fields.Add(FString::FromInt(Record.MontageInstanceId));
	Fields.Add(Record.AttackDataPath.ToString());
	Fields.Add(Record.MontagePath.ToString());
	Fields.Add(Record.NotifySourcePath.ToString());
	Fields.Add(EnumName(Record.InputType));
	Fields.Add(EnumName(Record.InputEvent));
	Fields.Add(EnumName(Record.InputDirection));
	Fields.Add(EnumName(Record.InputRoute));
	Fields.Add(EnumName(Record.InputDisposition));
	Fields.Add(EnumName(Record.ExecutionMode));
	Fields.Add(EnumName(Record.ActionState));
	Fields.Add(EnumName(Record.AttackPhase));
	Fields.Add(EnumName(Record.InputContext));
	Fields.Add(Record.ActionName.ToString());
	Fields.Add(FString::FromInt(Record.QueueDepth));
	Fields.Add(Record.MovementDisposition.ToString());
	Fields.Add(Record.CharacterMovementMode.ToString());
	Fields.Add(FString::FromInt(Record.CharacterCustomMovementMode));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.MovementMagnitude));
	Fields.Add(Record.bRootMotionActive ? TEXT("1") : TEXT("0"));
	Fields.Add(Record.TokenDisposition.ToString());
	Fields.Add(FString::FromInt(Record.ActiveTokenCount));
	Fields.Add(Record.AlignmentOwner.ToString());
	Fields.Add(Record.AlignmentDisposition.ToString());
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.RequestedYaw));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.AppliedYaw));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.RemainingYawError));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.OrbitDestination.X));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.OrbitDestination.Y));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.OrbitDestination.Z));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.OrbitRadialError));
	Fields.Add(Record.ReactionClass.ToString());
	Fields.Add(Record.ReactionDisposition.ToString());
	Fields.Add(Record.AnimationLane.ToString());
	Fields.Add(Record.AnimationFallback.ToString());
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.WeaponSocketDrift));
	Fields.Add(Record.TerminalDisposition.ToString());
	Fields.Add(Record.Detail);
	for (FString& Field : Fields)
	{
		Field = CsvField(MoveTemp(Field));
	}
	return FString::Join(Fields, TEXT(","));
}

const TCHAR* CsvHeader =
	TEXT("schema_version,sequence,event,reason,simulation_timestamp,unscaled_timestamp,")
	TEXT("actor,counterpart,actor_stable_id,counterpart_stable_id,input_serial,queue_entry_id,")
	TEXT("attack_generation,primary_action_generation,hold_generation,montage_instance_id,")
	TEXT("attack_data_path,montage_path,notify_source_path,input_type,input_event,input_direction,")
	TEXT("input_route,input_disposition,execution_mode,action_state,attack_phase,input_context,")
	TEXT("action_name,queue_depth,movement_disposition,character_movement_mode,")
	TEXT("character_custom_movement_mode,movement_magnitude,root_motion_active,")
	TEXT("token_disposition,active_token_count,")
	TEXT("alignment_owner,alignment_disposition,requested_yaw,applied_yaw,remaining_yaw_error,")
	TEXT("orbit_destination_x,orbit_destination_y,orbit_destination_z,orbit_radial_error,")
	TEXT("reaction_class,reaction_disposition,animation_lane,animation_fallback,weapon_socket_drift,")
	TEXT("terminal_disposition,detail");
}

FActionReactionTelemetryBuffer::FActionReactionTelemetryBuffer(const int32 InCapacity)
	: Capacity(FMath::Max(1, InCapacity))
{
	Records.Reserve(Capacity);
}

void FActionReactionTelemetryBuffer::Append(FActionReactionTelemetryRecord Record)
{
	Record.Sequence = ++NextSequence;
	Records.Add(MoveTemp(Record));
	const int32 Overflow = Records.Num() - Capacity;
	if (Overflow > 0)
	{
		Records.RemoveAt(0, Overflow, EAllowShrinking::No);
	}
}

void FActionReactionTelemetryBuffer::Reset()
{
	Records.Reset();
	NextSequence = 0;
}

bool ActionReactionTelemetry::IsEnabled()
{
	static const IConsoleVariable* ActionVariable =
		IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.ActionReaction.Debug"));
	static const IConsoleVariable* MasterVariable =
		IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.Debug.All"));
	return (ActionVariable && ActionVariable->GetInt() != 0)
		|| (MasterVariable && MasterVariable->GetInt() != 0);
}

FString ActionReactionTelemetry::BuildCsv(
	TConstArrayView<FActionReactionTelemetryRecord> Records)
{
	TArray<FActionReactionTelemetryRecord> Sorted(Records);
	Sorted.StableSort([](
		const FActionReactionTelemetryRecord& Left,
		const FActionReactionTelemetryRecord& Right)
	{
		if (Left.UnscaledTimestamp != Right.UnscaledTimestamp)
		{
			return Left.UnscaledTimestamp < Right.UnscaledTimestamp;
		}
		if (Left.SimulationTimestamp != Right.SimulationTimestamp)
		{
			return Left.SimulationTimestamp < Right.SimulationTimestamp;
		}
		if (Left.ActorStableId.Value != Right.ActorStableId.Value)
		{
			return Left.ActorStableId.Value < Right.ActorStableId.Value;
		}
		return Left.Sequence < Right.Sequence;
	});

	FString Csv(ActionReactionTelemetryPrivate::CsvHeader);
	Csv.AppendChar(TEXT('\n'));
	for (const FActionReactionTelemetryRecord& Record : Sorted)
	{
		Csv += ActionReactionTelemetryPrivate::BuildRow(Record);
		Csv.AppendChar(TEXT('\n'));
	}
	return Csv;
}

bool ActionReactionTelemetry::WriteCsv(
	const FString& RequestedPath,
	TConstArrayView<FActionReactionTelemetryRecord> Records,
	FString& OutResolvedPath,
	FString& OutError)
{
	OutError.Reset();
	const FString TrimmedPath = RequestedPath.TrimStartAndEnd();
	if (TrimmedPath.IsEmpty())
	{
		OutError = TEXT("A CSV output path is required");
		return false;
	}

	OutResolvedPath = FPaths::IsRelative(TrimmedPath)
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TrimmedPath)
		: FPaths::ConvertRelativePathToFull(TrimmedPath);
	FPaths::NormalizeFilename(OutResolvedPath);
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutResolvedPath), true))
	{
		OutError = FString::Printf(
			TEXT("Could not create telemetry directory for '%s'"), *OutResolvedPath);
		return false;
	}
	if (!FFileHelper::SaveStringToFile(
		BuildCsv(Records),
		*OutResolvedPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(
			TEXT("Could not write action-reaction telemetry to '%s'"), *OutResolvedPath);
		return false;
	}
	return true;
}
