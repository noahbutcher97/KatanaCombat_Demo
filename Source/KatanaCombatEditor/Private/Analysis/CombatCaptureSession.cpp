#include "Analysis/CombatCaptureSession.h"
#include "AnimationCapture/AnimationCaptureSession.h"
#include "AnimationCapture/AnimationCaptureJson.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Characters/BaseCombatCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/HitReactionComponent.h"
#include "Data/AttackData.h"
#include "Debug/ActionReactionTelemetry.h"
#include "Debug/DefenseTelemetry.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"
#include "HAL/IConsoleManager.h"

namespace
{
using namespace AnimationCaptureJson;
class FCombatCaptureExtension;
FCombatCaptureExtension *ActiveTelemetry = nullptr;

class FCombatCaptureExtension final : public IAnimationCaptureExtension
{
  public:
	explicit FCombatCaptureExtension(int32 InMaxRecords) : MaxRecords(InMaxRecords)
	{
	}
	bool Begin(TConstArrayView<FAnimationCaptureSubject> Subjects, FString &Error) override
	{
		if (ActiveTelemetry)
		{
			Error = TEXT("A combat telemetry recording is already active");
			return false;
		}
		ActiveTelemetry = this;
		Participants = TArray<FAnimationCaptureSubject>(Subjects);
		Telemetry.SetNum(Participants.Num());
		for (int32 I = 0; I < Participants.Num(); ++I)
		{
			const auto &P = Participants[I];
			if (const UCombatComponent *Combat = P.Actor->FindComponentByClass<UCombatComponent>())
			{
				const auto &Actions = Combat->GetActionReactionTelemetry();
				const auto &Defense = Combat->GetDefenseTelemetry();
				if (!Actions.IsEmpty())
				{
					Telemetry[I].ActionSequence = Actions.Last().Sequence;
					Telemetry[I].ActionTime = Actions.Last().UnscaledTimestamp;
				}
				if (!Defense.IsEmpty())
				{
					Telemetry[I].DefenseSequence = Defense.Last().Sequence;
					Telemetry[I].DefenseTime = Defense.Last().UnscaledTimestamp;
				}
			}
		}
		auto *ActionDebug = IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.ActionReaction.Debug"));
		auto *DefenseDebug = IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.Defense.Debug"));
		PreviousActionDebug = ActionDebug ? ActionDebug->GetInt() : 0;
		PreviousDefenseDebug = DefenseDebug ? DefenseDebug->GetInt() : 0;
		if (ActionDebug)
		{
			ActionDebug->SetWithCurrentPriority(1);
		}
		if (DefenseDebug)
		{
			DefenseDebug->SetWithCurrentPriority(1);
		}

		return true;
	}
	void Collect() override
	{
		CollectTelemetry();
	}
	TSharedPtr<FJsonObject> ObserveSubject(int32 Index) const override
	{
		AActor *Actor = Participants[Index].Actor.Get();
		auto Entry = MakeShared<FJsonObject>();
		if (!Actor)
		{
			return Entry;
		}
		if (const UCombatComponent *Combat = Actor->FindComponentByClass<UCombatComponent>())
		{
			Entry->SetStringField(TEXT("combat_state"), StaticEnum<ECombatState>()->GetNameStringByValue(
															static_cast<int64>(Combat->GetCombatState())));
			Entry->SetStringField(TEXT("attack_phase"), StaticEnum<EAttackPhase>()->GetNameStringByValue(
															static_cast<int64>(Combat->GetCurrentPhase())));
			Entry->SetStringField(TEXT("attack"), GetPathNameSafe(Combat->GetCurrentAttack()));
			Entry->SetNumberField(TEXT("attack_generation"), Combat->GetCurrentAttackGeneration());
			Entry->SetBoolField(TEXT("movement_input_suppressed"), Combat->IsMovementInputSuppressed());
			Entry->SetNumberField(TEXT("queue_size"), Combat->GetQueueSize());
			Entry->SetArrayField(TEXT("movement_input"),
								 VectorJson(FVector(Combat->GetMovementInputSample().CameraRelativeInput, 0)));
		}
		if (const ACharacter *Character = Cast<ACharacter>(Actor))
		{
			Entry->SetBoolField(TEXT("anim_root_motion_active"), Character->IsPlayingRootMotion());
			Entry->SetStringField(TEXT("movement_mode"), StaticEnum<EMovementMode>()->GetNameStringByValue(
															 Character->GetCharacterMovement()->MovementMode));
			Entry->SetArrayField(TEXT("movement_acceleration_cm_s2"),
								 VectorJson(Character->GetCharacterMovement()->GetCurrentAcceleration()));
		}
		if (const ABaseCombatCharacter *Character = Cast<ABaseCombatCharacter>(Actor))
		{
			Entry->SetNumberField(TEXT("health"), Character->CurrentHealth);
			Entry->SetBoolField(TEXT("dead_or_dying"), Character->IsDeadOrDying());
		}
		if (const UPairedAnimationComponent *Paired = Actor->FindComponentByClass<UPairedAnimationComponent>())
		{
			Entry->SetBoolField(TEXT("paired_active"), Paired->IsPairedAnimationActive());
			Entry->SetNumberField(TEXT("paired_state_lease_count"), Paired->GetActivePairedStateLeaseCount());
			Entry->SetBoolField(TEXT("paired_input_blocked"), Paired->IsInputBlocked());
			Entry->SetStringField(TEXT("chain_state"), StaticEnum<EChainCounterState>()->GetNameStringByValue(
														   static_cast<int64>(Paired->GetChainState())));
			Entry->SetNumberField(TEXT("paired_stage_generation"),
								  Paired->GetActiveDefenseSequenceContext().StageGeneration);
		}
		if (const UHitReactionComponent *Reaction = Actor->FindComponentByClass<UHitReactionComponent>())
		{
			Entry->SetBoolField(TEXT("paired_victim_active"), Reaction->IsInPairedAnimationState());
		}
		if (const auto *Warping = Actor->FindComponentByClass<UMotionWarpingComponent>())
		{
			TArray<TSharedPtr<FJsonValue>> Modifiers;
			for (const auto *Modifier : Warping->GetModifiers())
			{
				if (!Modifier)
				{
					continue;
				}
				auto M = MakeShared<FJsonObject>();
				M->SetStringField(TEXT("class"), Modifier->GetClass()->GetName());
				M->SetStringField(TEXT("state"), StaticEnum<ERootMotionModifierState>()->GetNameStringByValue(
													 static_cast<int64>(Modifier->GetState())));
				M->SetNumberField(TEXT("start_s"), Modifier->StartTime);
				M->SetNumberField(TEXT("end_s"), Modifier->EndTime);
				if (const auto *Warp = Cast<URootMotionModifier_Warp>(Modifier))
				{
					M->SetStringField(TEXT("target"), Warp->WarpTargetName.ToString());
					M->SetBoolField(TEXT("translation"), Warp->bWarpTranslation);
					M->SetBoolField(TEXT("rotation"), Warp->bWarpRotation);
					// End-of-world-tick observations, not the applied root-motion delta.
					M->SetArrayField(TEXT("cached_target_position_cm"), VectorJson(Warp->GetTargetLocation()));
					M->SetArrayField(TEXT("cached_target_rotation_xyzw"), QuaternionJson(Warp->GetTargetRotation()));
					const auto *Target = Warping->FindWarpTarget(Warp->WarpTargetName);
					M->SetBoolField(TEXT("component_target_present"), Target != nullptr);
					if (Target)
					{
						M->SetArrayField(TEXT("component_target_position_cm"), VectorJson(Target->GetLocation()));
						M->SetArrayField(TEXT("component_target_rotation_xyzw"), QuaternionJson(Target->GetRotation()));
					}
				}
				Modifiers.Add(MakeShared<FJsonValueObject>(M));
			}
			Entry->SetArrayField(TEXT("motion_warp_modifiers"), Modifiers);
		}
		if (const UEnemyCombatAIComponent *AI = Actor->FindComponentByClass<UEnemyCombatAIComponent>())
		{
			Entry->SetBoolField(TEXT("ai_has_attack_token"), AI->HasAttackToken());
			Entry->SetStringField(TEXT("ai_state"), StaticEnum<EEnemyAIState>()->GetNameStringByValue(
														static_cast<int64>(AI->CurrentState)));
		}
		return Entry;
	}
	TSharedPtr<FJsonObject> DescribeSubject(int32 Index) const override
	{
		auto Fields = MakeShared<FJsonObject>();
		Fields->SetNumberField(TEXT("telemetry_lost_records"), Telemetry[Index].Lost);
		Fields->SetNumberField(TEXT("telemetry_resets"), Telemetry[Index].Resets);
		Fields->SetNumberField(TEXT("action_records"), Telemetry[Index].Actions.Num());
		Fields->SetNumberField(TEXT("defense_records"), Telemetry[Index].Defense.Num());
		return Fields;
	}
	TSharedPtr<FJsonObject> DescribeSession() const override
	{
		auto Fields = MakeShared<FJsonObject>();
		Fields->SetNumberField(TEXT("max_telemetry_records_per_actor"), MaxRecords);
		return Fields;
	}
	void End(TArray<FAnimationCaptureTextArtifact> &Artifacts) override
	{
		for (int32 I = 0; I < Participants.Num(); ++I)
		{
			Artifacts.Add(
				{Participants[I].Id + TEXT(".actions.csv"), ActionReactionTelemetry::BuildCsv(Telemetry[I].Actions)});
			Artifacts.Add(
				{Participants[I].Id + TEXT(".defense.csv"), DefenseTelemetry::BuildCsv(Telemetry[I].Defense)});
		}
		if (auto *CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.ActionReaction.Debug")))
		{
			CVar->SetWithCurrentPriority(PreviousActionDebug);
		}
		if (auto *CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.Defense.Debug")))
		{
			CVar->SetWithCurrentPriority(PreviousDefenseDebug);
		}
		if (ActiveTelemetry == this)
		{
			ActiveTelemetry = nullptr;
		}
	}

  private:
	int32 MaxRecords;
	int32 PreviousActionDebug = 0, PreviousDefenseDebug = 0;
	TArray<FAnimationCaptureSubject> Participants;
	struct FTelemetry
	{
		TArray<FActionReactionTelemetryRecord> Actions;
		TArray<FDefenseTelemetryRecord> Defense;
		uint64 ActionSequence = 0, DefenseSequence = 0;
		double ActionTime = -1, DefenseTime = -1;
		int32 Lost = 0, Resets = 0;
	};
	TArray<FTelemetry> Telemetry;
	template <typename RecordType>
	void Collect(const TArray<RecordType> &Source, TArray<RecordType> &Dest, uint64 &Sequence, double &LastTime,
				 FTelemetry &State)
	{
		if (Source.IsEmpty())
		{
			return;
		}
		// Sequence numbers restart when a component clears telemetry. Include the wall
		// timestamp so a reset that already grew past the old sequence is still detected.
		bool bFoundPrevious = Sequence == 0;
		for (const auto &Row : Source)
		{
			bFoundPrevious |= Row.Sequence == Sequence && Row.UnscaledTimestamp == LastTime;
		}
		if (!bFoundPrevious && Source[0].Sequence <= Sequence && Source.Last().UnscaledTimestamp > LastTime)
		{
			++State.Resets;
			Sequence = 0;
		}
		for (const auto &Row : Source)
		{
			if (Row.UnscaledTimestamp <= LastTime && Row.Sequence <= Sequence)
			{
				continue;
			}
			if (Row.Sequence <= Sequence)
			{
				continue;
			}
			State.Lost += static_cast<int32>(Row.Sequence - Sequence - 1);
			if (Dest.Num() < MaxRecords)
			{
				Dest.Add(Row);
			}
			else
			{
				++State.Lost;
			}
			Sequence = Row.Sequence;
			LastTime = Row.UnscaledTimestamp;
		}
	}
	void CollectTelemetry()
	{
		for (int32 Index = 0; Index < Participants.Num(); ++Index)
		{
			AActor *Actor = Participants[Index].Actor.Get();
			UCombatComponent *Combat = Actor ? Actor->FindComponentByClass<UCombatComponent>() : nullptr;
			if (!Combat)
			{
				continue;
			}
			auto &State = Telemetry[Index];
			Collect(Combat->GetActionReactionTelemetry(), State.Actions, State.ActionSequence, State.ActionTime, State);
			Collect(Combat->GetDefenseTelemetry(), State.Defense, State.DefenseSequence, State.DefenseTime, State);
		}
	}
};
} // namespace

struct FCombatCaptureSession::FImpl
{
	FAnimationCaptureSession Session;
};
FCombatCaptureSession::FCombatCaptureSession() : Impl(MakeUnique<FImpl>())
{
}
FCombatCaptureSession::~FCombatCaptureSession() = default;

bool FCombatCaptureSession::Start(UWorld *World, const FCombatCaptureSettings &Settings,
								  TConstArrayView<FCombatCaptureParticipant> Participants, FString &Error)
{
	if (!IsValid(World) || World->WorldType != EWorldType::PIE || ActiveTelemetry)
	{
		Error = TEXT("Capture requires one explicit PIE world and no active recording");
		return false;
	}
	if (Settings.MaxTelemetryRecordsPerActor < 1 || Settings.MaxTelemetryRecordsPerActor > 100000)
	{
		Error = TEXT("Invalid telemetry record limit");
		return false;
	}
	FAnimationCaptureSettings Native;
	Native.Scenario = Settings.Scenario;
	Native.OutputRoot = FPaths::ProjectSavedDir() / TEXT("CombatCaptures");
	Native.SampleHz = Settings.SampleHz;
	Native.FrameHz = Settings.FrameHz;
	Native.bUseAsyncReadback = Settings.bUseAsyncReadback;
	Native.bUseAsyncDiagnosticResolution = Settings.bUseAsyncDiagnosticResolution;
	Native.MaxWallSeconds = Settings.MaxWallSeconds;
	Native.MaxSamples = Settings.MaxSamples;
	Native.MaxFrames = Settings.MaxFrames;
	Native.MaxDataBytes = Settings.MaxDataBytes;
	Native.Metadata = Settings.Metadata;
	TArray<FAnimationCaptureSubject> Subjects;
	for (const auto &Participant : Participants)
	{
		auto &Subject = Subjects.AddDefaulted_GetRef();
		Subject.Id = Participant.Role;
		Subject.Actor = Participant.Actor;
		Subject.Mesh = Participant.Mesh;
		if (!Subject.Mesh.IsValid() && Subject.Actor.IsValid())
		{
			Subject.Mesh = Subject.Actor->FindComponentByClass<USkeletalMeshComponent>();
		}
		Subject.Points = Participant.Points;
		Subject.PointSources = Participant.PointSources;
	}
	return Impl->Session.Start(World, Native, Subjects, Error,
							   MakeShared<FCombatCaptureExtension>(Settings.MaxTelemetryRecordsPerActor));
}
bool FCombatCaptureSession::Stop(const FString &Reason, FString &Error)
{
	return Impl->Session.Stop(Reason, Error);
}
void FCombatCaptureSession::Mark(const FString &Label)
{
	Impl->Session.Mark(Label);
}
bool FCombatCaptureSession::IsRecording() const
{
	return Impl->Session.IsRecording();
}
FString FCombatCaptureSession::GetOutputDirectory() const
{
	return Impl->Session.GetOutputDirectory();
}
int32 FCombatCaptureSession::GetSampleCount() const
{
	return Impl->Session.GetSampleCount();
}
int32 FCombatCaptureSession::GetFrameCount() const
{
	return Impl->Session.GetFrameCount();
}
FString FCombatCaptureSession::GetStopReason() const
{
	return Impl->Session.GetStopReason();
}
bool FCombatCaptureSession::IsExpectedPIEViewportClient(const UWorld *World, const FViewportClient *Drawn,
														const FViewportClient *Expected)
{
	return IsValid(World) && World->WorldType == EWorldType::PIE &&
		   FAnimationCaptureSession::IsExpectedViewportClient(World, Drawn, Expected);
}

TArray<FCombatCaptureParticipant> FCombatCaptureSession::DiscoverParticipants(UWorld *World)
{
	TArray<ACharacter *> Characters;
	if (World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			Characters.Add(*It);
		}
	}
	Characters.Sort([](const ACharacter &A, const ACharacter &B) { return A.GetPathName() < B.GetPathName(); });
	TArray<FCombatCaptureParticipant> Result;
	int32 PlayerIndex = 0, CharacterIndex = 0;
	for (ACharacter *Actor : Characters)
	{
		FCombatCaptureParticipant &P = Result.AddDefaulted_GetRef();
		P.Role = Actor->IsPlayerControlled() ? FString::Printf(TEXT("Player%d"), ++PlayerIndex)
											 : FString::Printf(TEXT("Character%d"), ++CharacterIndex);
		P.Actor = Actor;
		P.Mesh = Actor->GetMesh();
	}
	return Result;
}
