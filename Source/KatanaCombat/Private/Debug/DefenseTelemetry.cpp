// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/DefenseTelemetry.h"

#include "Data/AttackData.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
FString EventName(const EDefenseTelemetryEvent Event)
{
	switch (Event)
	{
	case EDefenseTelemetryEvent::ThreatSelection: return TEXT("ThreatSelection");
	case EDefenseTelemetryEvent::Resolution: return TEXT("Resolution");
	case EDefenseTelemetryEvent::PresentationStart: return TEXT("PresentationStart");
	case EDefenseTelemetryEvent::AlignmentRequest: return TEXT("AlignmentRequest");
	case EDefenseTelemetryEvent::AlignmentFrame: return TEXT("AlignmentFrame");
	case EDefenseTelemetryEvent::StageStart: return TEXT("StageStart");
	case EDefenseTelemetryEvent::StageTransition: return TEXT("StageTransition");
	case EDefenseTelemetryEvent::Cleanup: return TEXT("Cleanup");
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

FString ActorPath(const TWeakObjectPtr<AActor>& Actor)
{
	return Actor.IsValid() ? Actor->GetPathName() : FString();
}

void AddVectorFields(TArray<FString>& Fields, const FVector& Vector)
{
	Fields.Add(FString::Printf(TEXT("%.6f"), Vector.X));
	Fields.Add(FString::Printf(TEXT("%.6f"), Vector.Y));
	Fields.Add(FString::Printf(TEXT("%.6f"), Vector.Z));
}

void AddTransformFields(TArray<FString>& Fields, const FTransform& Transform)
{
	AddVectorFields(Fields, Transform.GetLocation());
	const FRotator Rotation = Transform.Rotator();
	Fields.Add(FString::Printf(TEXT("%.6f"), Rotation.Yaw));
	Fields.Add(FString::Printf(TEXT("%.6f"), Rotation.Pitch));
	Fields.Add(FString::Printf(TEXT("%.6f"), Rotation.Roll));
}

FString BuildRow(const FDefenseTelemetryRecord& Record)
{
	TArray<FString> Fields;
	Fields.Reserve(72);
	Fields.Add(TEXT("1"));
	Fields.Add(FString::Printf(TEXT("%llu"), Record.Sequence));
	Fields.Add(EventName(Record.Event));
	Fields.Add(FString::Printf(TEXT("%.9f"), Record.SimulationTimestamp));
	Fields.Add(FString::Printf(TEXT("%.9f"), Record.UnscaledTimestamp));
	Fields.Add(FString::Printf(TEXT("%llu"), Record.InteractionId.Epoch));
	Fields.Add(EnumName(Record.InteractionId.Key.Stage));
	Fields.Add(FString::Printf(TEXT("%d"), Record.AttackInstance.AttackGeneration));
	Fields.Add(Record.AttackDataPath.ToString());
	Fields.Add(EnumName(Record.AttackWindow.Kind));
	Fields.Add(FString::Printf(TEXT("%d"), Record.AttackWindow.WindowGeneration));
	Fields.Add(FString::Printf(TEXT("%d"), Record.AttackWindow.MontageInstanceId));
	Fields.Add(Record.AttackWindow.NotifySource.SourceAnimation.ToString());
	Fields.Add(FString::Printf(TEXT("%d"), Record.AttackWindow.NotifySource.NotifyEventIndex));
	Fields.Add(FString::Printf(TEXT("%d"), Record.StageGeneration));
	Fields.Add(Record.StageName.ToString());
	Fields.Add(ActorPath(Record.Defender));
	Fields.Add(ActorPath(Record.Attacker));
	Fields.Add(ActorPath(Record.Candidate));
	Fields.Add(FString::Printf(TEXT("%llu"), Record.DefenderStableId.Value));
	Fields.Add(FString::Printf(TEXT("%llu"), Record.AttackerStableId.Value));
	Fields.Add(FString::Printf(TEXT("%llu"), Record.CandidateStableId.Value));
	Fields.Add(FString::Printf(TEXT("%llu"), Record.LockedThreatStableId.Value));
	AddTransformFields(Fields, Record.OwnerTransform);
	AddTransformFields(Fields, Record.CounterpartTransform);
	Fields.Add(Record.CandidateDisposition.ToString());
	Fields.Add(Record.ThreatSwitchReason.ToString());
	Fields.Add(EnumName(Record.Outcome));
	Fields.Add(EnumName(Record.Reason));
	Fields.Add(EnumName(Record.PredictedHeight));
	Fields.Add(EnumName(Record.PredictedLane));
	Fields.Add(EnumName(Record.PredictedSwing));
	Fields.Add(EnumName(Record.ActualHeight));
	Fields.Add(EnumName(Record.ActualLane));
	Fields.Add(EnumName(Record.ActualSwing));
	AddVectorFields(Fields, Record.PredictedAxis);
	AddVectorFields(Fields, Record.ActualAxis);
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.InitialYawError));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.RemainingYawError));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.TimeToDeadline));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.MaximumTurnRate));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.RemainingTurnBudget));
	Fields.Add(Record.AlignmentOwner.ToString());
	Fields.Add(EnumName(Record.AlignmentExecutor));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.ConfiguredEngineWarpRate));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.FrameSimulationDelta));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.AppliedFrameYaw));
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.FinalFrameYawError));
	AddVectorFields(Fields, Record.FrameDisplacement);
	AddVectorFields(Fields, Record.ExpectedAuthoredDisplacement);
	AddVectorFields(Fields, Record.ExpectedWarpDisplacement);
	AddVectorFields(Fields, Record.UnexpectedDisplacement);
	Fields.Add(FString::Printf(TEXT("%.6f"), Record.PelvisDelta));
	Fields.Add(Record.CacheDisposition.ToString());
	Fields.Add(Record.WeaponDisposition.ToString());
	Fields.Add(Record.SelectedPresentationRow.ToString());
	Fields.Add(EnumName(Record.PresentationFallback));
	Fields.Add(Record.AttackerPresentationRow.ToString());
	Fields.Add(EnumName(Record.AttackerPresentationFallback));
	Fields.Add(Record.CleanupReason.ToString());
	for (FString& Field : Fields)
	{
		Field = CsvField(MoveTemp(Field));
	}
	return FString::Join(Fields, TEXT(","));
}

const TCHAR* CsvHeader =
	TEXT("schema_version,sequence,event,simulation_timestamp,unscaled_timestamp,")
	TEXT("interaction_epoch,interaction_stage,attack_generation,attack_data_path,window_kind,window_generation,")
	TEXT("montage_instance_id,notify_source,notify_event_index,stage_generation,stage_name,")
	TEXT("defender,attacker,candidate,defender_stable_id,attacker_stable_id,candidate_stable_id,")
	TEXT("locked_threat_stable_id,owner_location_x,owner_location_y,owner_location_z,owner_yaw,")
	TEXT("owner_pitch,owner_roll,counterpart_location_x,counterpart_location_y,counterpart_location_z,")
	TEXT("counterpart_yaw,counterpart_pitch,counterpart_roll,candidate_disposition,threat_switch_reason,outcome,reason,")
	TEXT("predicted_height,predicted_lane,predicted_swing,actual_height,actual_lane,actual_swing,")
	TEXT("predicted_axis_x,predicted_axis_y,predicted_axis_z,actual_axis_x,actual_axis_y,actual_axis_z,")
	TEXT("initial_yaw_error,remaining_yaw_error,time_to_deadline,maximum_turn_rate,remaining_turn_budget,")
	TEXT("alignment_owner,alignment_executor,configured_engine_warp_rate,frame_simulation_delta,")
	TEXT("applied_frame_yaw,final_frame_yaw_error,frame_displacement_x,frame_displacement_y,")
	TEXT("frame_displacement_z,expected_authored_displacement_x,expected_authored_displacement_y,")
	TEXT("expected_authored_displacement_z,expected_warp_displacement_x,expected_warp_displacement_y,")
	TEXT("expected_warp_displacement_z,unexpected_displacement_x,unexpected_displacement_y,")
	TEXT("unexpected_displacement_z,pelvis_delta,cache_disposition,weapon_disposition,")
	TEXT("selected_presentation_row,presentation_fallback,attacker_presentation_row,")
	TEXT("attacker_presentation_fallback,cleanup_reason");
}

bool DefenseTelemetry::IsEnabled()
{
	static const IConsoleVariable* Variable =
		IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.Defense.Debug"));
	return Variable && Variable->GetInt() != 0;
}

FDefenseTelemetryRecord DefenseTelemetry::FromResolution(
	const FDefenseResolution& Resolution,
	const EDefenseTelemetryEvent Event)
{
	FDefenseTelemetryRecord Record;
	Record.Event = Event;
	Record.InteractionId = Resolution.InteractionId;
	Record.AttackInstance = Resolution.Decision.AttackInstance;
	if (!Record.AttackInstance.IsValid())
	{
		if (Resolution.InteractionId.Key.Stage == EDefenseQueryStage::InputIntent)
		{
			Record.AttackInstance = Resolution.InteractionId.Key.AttackInstance;
		}
		else if (Resolution.InteractionId.Key.ContactInstance.bUsesAttackWindow)
		{
			Record.AttackInstance =
				Resolution.InteractionId.Key.ContactInstance.AttackWindow.AttackInstance;
		}
	}
	if (Resolution.InteractionId.Key.Stage == EDefenseQueryStage::Contact
		&& Resolution.InteractionId.Key.ContactInstance.bUsesAttackWindow)
	{
		Record.AttackWindow = Resolution.InteractionId.Key.ContactInstance.AttackWindow;
	}
	Record.Defender = Resolution.InteractionId.Key.Defender;
	Record.Attacker = Record.AttackInstance.Attacker;
	Record.Outcome = Resolution.Decision.Outcome;
	Record.Reason = Resolution.Decision.Reason;
	Record.PredictedHeight = Resolution.PredictedContact.Height;
	Record.PredictedLane = Resolution.PredictedContact.Lane;
	Record.PredictedSwing = Resolution.Decision.SwingShape;
	Record.PredictedAxis = Resolution.PredictedContact.PathDirection;
	Record.ActualHeight = Resolution.bHasActualContact && Resolution.ActualContact.bIsValid
		? Resolution.ActualContact.Height
		: Resolution.Decision.Height;
	Record.ActualLane = Resolution.bHasActualContact && Resolution.ActualContact.bIsValid
		? Resolution.ActualContact.Lane
		: Resolution.Decision.Lane;
	Record.ActualSwing = Resolution.Decision.SwingShape;
	Record.ActualAxis = Resolution.bHasActualContact && Resolution.ActualContact.bIsValid
		? Resolution.ActualContact.IncomingTrajectory
		: FVector::ZeroVector;
	Record.InitialYawError = Resolution.Decision.MeasuredYawDegrees;
	Record.RemainingYawError = Resolution.Decision.MeasuredYawDegrees;
	Record.RemainingTurnBudget = Resolution.Decision.AvailableTurnDegrees;
	Record.SelectedPresentationRow = Resolution.PresentationRow;
	Record.PresentationFallback = Resolution.PresentationFallback;
	Record.AttackerPresentationRow = Resolution.AttackerPresentationRow;
	Record.AttackerPresentationFallback = Resolution.AttackerPresentationFallback;
	if (Resolution.Decision.SelectedAttack)
	{
		Record.AttackDataPath = FSoftObjectPath(Resolution.Decision.SelectedAttack.Get());
	}
	return Record;
}

FString DefenseTelemetry::BuildCsv(TConstArrayView<FDefenseTelemetryRecord> Records)
{
	TArray<FDefenseTelemetryRecord> Sorted(Records);
	Sorted.StableSort([](const FDefenseTelemetryRecord& Left, const FDefenseTelemetryRecord& Right)
	{
		if (Left.UnscaledTimestamp != Right.UnscaledTimestamp)
		{
			return Left.UnscaledTimestamp < Right.UnscaledTimestamp;
		}
		if (Left.SimulationTimestamp != Right.SimulationTimestamp)
		{
			return Left.SimulationTimestamp < Right.SimulationTimestamp;
		}
		if (Left.DefenderStableId.Value != Right.DefenderStableId.Value)
		{
			return Left.DefenderStableId.Value < Right.DefenderStableId.Value;
		}
		return Left.Sequence < Right.Sequence;
	});

	FString Csv(CsvHeader);
	Csv.AppendChar(TEXT('\n'));
	for (const FDefenseTelemetryRecord& Record : Sorted)
	{
		Csv += BuildRow(Record);
		Csv.AppendChar(TEXT('\n'));
	}
	return Csv;
}

bool DefenseTelemetry::WriteCsv(
	const FString& RequestedPath,
	TConstArrayView<FDefenseTelemetryRecord> Records,
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
		OutError = FString::Printf(TEXT("Could not create telemetry directory for '%s'"), *OutResolvedPath);
		return false;
	}
	if (!FFileHelper::SaveStringToFile(
		BuildCsv(Records),
		*OutResolvedPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Could not write defense telemetry to '%s'"), *OutResolvedPath);
		return false;
	}
	return true;
}
