#include "Analysis/CombatCaptureSession.h"
#include "Analysis/CombatCaptureContactObserver.h"
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
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "PresentationCapture/PresentationRecording.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"

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

/** One instant on the capture clocks: engine frame, process platform time and the world's simulation time. */
struct FCaptureAnchor
{
	bool bSet = false;
	uint64 EngineFrame = 0;
	double PlatformSeconds = 0;
	double PlatformUncertainty = 0;
	double WorldSeconds = -1;

	static FCaptureAnchor Now(const UWorld *World)
	{
		FCaptureAnchor Anchor;
		Anchor.bSet = true;
		Anchor.EngineFrame = GFrameCounter;
		Anchor.PlatformSeconds = FPlatformTime::Seconds();
		Anchor.WorldSeconds = World ? World->GetTimeSeconds() : -1;
		return Anchor;
	}
	TSharedRef<FJsonObject> ToJson() const
	{
		auto Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("engine_frame"), static_cast<double>(EngineFrame));
		Json->SetNumberField(TEXT("platform_seconds"), PlatformSeconds);
		Json->SetNumberField(TEXT("platform_seconds_uncertainty"), PlatformUncertainty);
		Json->SetNumberField(TEXT("world_time_s"), WorldSeconds);
		return Json;
	}
};

const FName VideoChannel(TEXT("KatanaCombatCapture"));

const TCHAR *RecorderStateName(EPresentationCaptureState State)
{
	switch (State)
	{
	case EPresentationCaptureState::Idle: return TEXT("Idle");
	case EPresentationCaptureState::Armed: return TEXT("Armed");
	case EPresentationCaptureState::AwaitingViewports: return TEXT("AwaitingViewports");
	case EPresentationCaptureState::Capturing: return TEXT("Capturing");
	case EPresentationCaptureState::Finalizing: return TEXT("Finalizing");
	case EPresentationCaptureState::Error: return TEXT("Error");
	}
	return TEXT("Unknown");
}

TArray<TSharedPtr<FJsonValue>> SizeJson(const FIntPoint &Size)
{
	return {MakeShared<FJsonValueNumber>(Size.X), MakeShared<FJsonValueNumber>(Size.Y)};
}
} // namespace

struct FCombatCaptureSession::FImpl
{
	FAnimationCaptureSession Session;
	TStrongObjectPtr<UCombatCaptureContactObserver> Observer;
	TArray<TPair<TWeakObjectPtr<ABaseCombatCharacter>, FString>> CombatParticipants;
	int32 RetainedWeaponContacts = 0, RetainedPairedContacts = 0, RetainedCommittedContacts = 0;
	TWeakObjectPtr<UWorld> World;
	FString Scenario, WorldPath, LinkPath;
	double SampleHz = 0, FrameHz = 0;
	FCaptureAnchor AnalysisStart;

	/** The optional clip recorded by PresentationCapture alongside this session. */
	struct FVideo
	{
		bool bStarted = false, bStopRequested = false;
		int32 FramesPerSecond = 0, Resolution = 0, Seconds = 0;
		FString Directory, CaptureId, ClockId, WorldId, StateAtStop, StopReason, Error;
		double TelemetryStartedSeconds = 0, StartCallBegin = 0, StartCallEnd = 0;
		FCaptureAnchor Start, StopRequest;
		FIntPoint SceneViewport = FIntPoint::ZeroValue, ViewportWidget = FIntPoint::ZeroValue;
	} Video;

	void ReleaseObserver()
	{
		if (Observer.IsValid())
		{
			RetainedWeaponContacts = Observer->GetWeaponContactCount();
			RetainedPairedContacts = Observer->GetPairedContactCount();
			RetainedCommittedContacts = Observer->GetCommittedContactCount();
			Observer->Unbind();
		}
		Observer.Reset();
	}

	/** The recorder reports CaptureWallSeconds as now minus its private start, so the
	 * start's platform time is recovered from the plugin's own status without changing it. */
	void RecordAnalysisStart(UWorld *InWorld)
	{
		const double Before = FPlatformTime::Seconds();
		const FAnimationCaptureSessionStatus Status = Session.GetStatus();
		const double After = FPlatformTime::Seconds();
		AnalysisStart.bSet = true;
		AnalysisStart.EngineFrame = GFrameCounter;
		AnalysisStart.PlatformSeconds = 0.5 * (Before + After) - Status.CaptureWallSeconds;
		AnalysisStart.PlatformUncertainty = 0.5 * (After - Before);
		AnalysisStart.WorldSeconds = Status.StartSimulationTime;
		World = InWorld;
		Scenario = Status.Scenario;
		WorldPath = Status.WorldPath;
		SampleHz = Status.SampleHz;
		FrameHz = Status.FrameHz;
	}

	static bool ValidateVideo(const FCombatCaptureSettings &Settings, FString &Error)
	{
		if (!FApp::CanEverRender())
		{
			Error = TEXT("Video capture needs a rendering editor: -RenderOffScreen works; -NullRHI cannot record viewport video");
			return false;
		}
		if (Settings.VideoFramesPerSecond < 1 || Settings.VideoFramesPerSecond > 120)
		{
			Error = TEXT("Video frame rate must be 1-120");
			return false;
		}
		if (Settings.VideoResolution != 360 && Settings.VideoResolution != 720 && Settings.VideoResolution != 1080)
		{
			Error = TEXT("Video resolution must be 360, 720 or 1080");
			return false;
		}
		if (Settings.VideoSeconds < 0 || Settings.VideoSeconds > 30)
		{
			Error = TEXT("Video seconds must be 0 (follow MaxWallSeconds) or 1-30");
			return false;
		}
		if (PresentationRecording::IsBusy())
		{
			Error = TEXT("PresentationCapture is already recording or finalizing; wait for it before starting a capture with video");
			return false;
		}
		return true;
	}

	bool StartVideo(UWorld *InWorld, const FCombatCaptureSettings &Settings, FString &Error)
	{
		FPresentationCaptureOptions Options;
		Options.Seconds = Settings.VideoSeconds > 0 ? Settings.VideoSeconds
													: FMath::Clamp(FMath::CeilToInt(Settings.MaxWallSeconds), 1, 30);
		Options.FramesPerSecond = Settings.VideoFramesPerSecond;
		Options.Resolution = Settings.VideoResolution == 360	 ? EPresentationResolution::P360
							 : Settings.VideoResolution == 1080 ? EPresentationResolution::P1080
																: EPresentationResolution::P720;
		Options.Aspect = EPresentationAspect::MatchViewport;
		Options.Seats = EPresentationSeats::All;
		Options.OutputDirectory = Session.GetOutputDirectory() / TEXT("video");
		Video.FramesPerSecond = Options.FramesPerSecond;
		Video.Resolution = Settings.VideoResolution;
		Video.Seconds = Options.Seconds;

		FPresentationSession Announced;
		bool bAnnounced = false;
		const FDelegateHandle Handle = PresentationRecording::OnStarted().AddLambda(
			[&Announced, &bAnnounced](const FPresentationSession &Value)
			{
				Announced = Value;
				bAnnounced = true;
			});
		Video.Start = FCaptureAnchor::Now(InWorld);
		Video.StartCallBegin = Video.Start.PlatformSeconds;
		FString RecorderError;
		const bool bStarted = PresentationRecording::Start(Options, RecorderError);
		Video.StartCallEnd = FPlatformTime::Seconds();
		PresentationRecording::OnStarted().Remove(Handle);
		if (!bStarted)
		{
			Video.Error = RecorderError;
			Error = TEXT("Video capture did not start: ") + RecorderError;
			return false;
		}
		Video.bStarted = true;
		Video.Directory = FPaths::ConvertRelativePathToFull(PresentationRecording::GetDirectory());
		Video.CaptureId = FPaths::GetCleanFilename(Video.Directory);
		if (bAnnounced)
		{
			// PresentationCapture announces its telemetry start inside Start, after its video epoch.
			Video.ClockId = Announced.ClockId;
			Video.TelemetryStartedSeconds = Announced.StartedSeconds;
			Video.Start.PlatformSeconds = Announced.StartedSeconds;
			for (const FPresentationWorld &Entry : Announced.Worlds)
			{
				if (Entry.World.Get() == InWorld) { Video.WorldId = Entry.Id; }
			}
		}
		else
		{
			Video.Start.PlatformSeconds = Video.StartCallEnd;
		}
		Video.Start.PlatformUncertainty = Video.StartCallEnd - Video.StartCallBegin;
		if (const UGameViewportClient *Client = InWorld->GetGameViewport())
		{
			if (Client->Viewport) { Video.SceneViewport = Client->Viewport->GetSizeXY(); }
			if (const TSharedPtr<SViewport> Widget = Client->GetGameViewportWidget())
			{
				// The recorder reads this widget's area of the window backbuffer and scales it to the output.
				const FVector2D Size = Widget->GetCachedGeometry().GetAbsoluteSize();
				Video.ViewportWidget = FIntPoint(FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y));
			}
		}
		auto Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("capture_id"), Video.CaptureId);
		Payload->SetStringField(TEXT("clock_id"), Video.ClockId);
		Payload->SetStringField(TEXT("directory"), RelativeVideoDirectory());
		Payload->SetNumberField(TEXT("requested_fps"), Video.FramesPerSecond);
		Payload->SetNumberField(TEXT("requested_resolution"), Video.Resolution);
		Payload->SetNumberField(TEXT("game_frame"), static_cast<double>(GFrameCounter));
		Payload->SetNumberField(TEXT("platform_seconds"), FPlatformTime::Seconds());
		Session.Mark(TEXT("video_started"), Payload);
		PresentationRecording::SetChannelStatus(VideoChannel,
			FString::Printf(TEXT("Katana combat capture %s; joined by capture-link.json"), *Session.GetOutputDirectory()));
		return true;
	}

	void StopVideo(const FString &Reason)
	{
		if (!Video.bStarted || Video.bStopRequested) { return; }
		Video.bStopRequested = true;
		Video.StopReason = Reason;
		Video.StopRequest = FCaptureAnchor::Now(World.Get());
		Video.StateAtStop = RecorderStateName(PresentationRecording::GetState());
		auto Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("capture_id"), Video.CaptureId);
		Payload->SetStringField(TEXT("recorder_state"), Video.StateAtStop);
		Payload->SetStringField(TEXT("reason"), Reason);
		Payload->SetNumberField(TEXT("game_frame"), static_cast<double>(GFrameCounter));
		Payload->SetNumberField(TEXT("platform_seconds"), FPlatformTime::Seconds());
		if (Session.IsRecording()) { Session.Mark(TEXT("video_stop_requested"), Payload); }
		// Stop only this session's clip; finalization continues asynchronously in the recorder.
		if (PresentationRecording::IsBusy() && IsOwnClip()) { PresentationRecording::Stop(); }
		PresentationRecording::SetChannelStatus(VideoChannel, FString());
	}

	bool IsOwnClip() const
	{
		return Video.bStarted && FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(PresentationRecording::GetDirectory()), Video.Directory);
	}

	FString RelativeVideoDirectory() const
	{
		FString Relative = Video.Directory;
		return FPaths::MakePathRelativeTo(Relative, *(Session.GetOutputDirectory() / TEXT(""))) ? Relative : Video.Directory;
	}

	/** capture-link.json: where the clip lives, both start instants and how to join them. */
	void WriteLink(const FString &Status) const
	{
		if (LinkPath.IsEmpty()) { return; }
		auto Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schema_version"), 1);
		Root->SetStringField(TEXT("kind"), TEXT("katana_combat_capture_link"));
		Root->SetStringField(TEXT("status"), Status);
		Root->SetStringField(TEXT("join"),
			TEXT("Primary: samples.jsonl engine_frame equals the video frames CSV drawGameFrame; both are GFrameCounter on the game thread. "
				 "Platform seconds are a secondary cross-check. A frame's PTS is its backbuffer acquisition, after its game frame: map events by engine frame, never by seconds."));
		Root->SetStringField(TEXT("clock"), TEXT("FPlatformTime::Seconds, process-local"));
		Root->SetNumberField(TEXT("process_id"), FPlatformProcess::GetCurrentProcessId());

		auto Analysis = MakeShared<FJsonObject>();
		Analysis->SetStringField(TEXT("directory"), TEXT("."));
		Analysis->SetStringField(TEXT("absolute_directory"), Session.GetOutputDirectory());
		Analysis->SetStringField(TEXT("scenario"), Scenario);
		Analysis->SetStringField(TEXT("world"), WorldPath);
		Analysis->SetNumberField(TEXT("sample_hz"), SampleHz);
		Analysis->SetNumberField(TEXT("frame_hz"), FrameHz);
		Analysis->SetObjectField(TEXT("start"), AnalysisStart.ToJson());
		Analysis->SetStringField(TEXT("start_platform_source"),
			TEXT("AnimationAnalysis status: platform time minus capture wall seconds; samples' wall_elapsed_s are relative to it"));
		Root->SetObjectField(TEXT("analysis"), Analysis);

		auto Requested = MakeShared<FJsonObject>();
		Requested->SetNumberField(TEXT("fps"), Video.FramesPerSecond);
		Requested->SetNumberField(TEXT("resolution"), Video.Resolution);
		Requested->SetNumberField(TEXT("seconds"), Video.Seconds);
		Requested->SetStringField(TEXT("aspect"), TEXT("match-viewport"));
		Requested->SetStringField(TEXT("seats"), TEXT("all"));
		auto VideoJson = MakeShared<FJsonObject>();
		VideoJson->SetObjectField(TEXT("requested"), Requested);
		VideoJson->SetBoolField(TEXT("started"), Video.bStarted);
		VideoJson->SetStringField(TEXT("error"), Video.Error);
		VideoJson->SetStringField(TEXT("capture_id"), Video.CaptureId);
		VideoJson->SetStringField(TEXT("clock_id"), Video.ClockId);
		VideoJson->SetStringField(TEXT("world_id"), Video.WorldId);
		VideoJson->SetStringField(TEXT("directory"), Video.bStarted ? RelativeVideoDirectory() : FString());
		VideoJson->SetStringField(TEXT("absolute_directory"), Video.Directory);
		VideoJson->SetObjectField(TEXT("start"), Video.Start.ToJson());
		VideoJson->SetArrayField(TEXT("start_call_platform_seconds"),
			{MakeShared<FJsonValueNumber>(Video.StartCallBegin), MakeShared<FJsonValueNumber>(Video.StartCallEnd)});
		VideoJson->SetNumberField(TEXT("telemetry_start_platform_seconds"), Video.TelemetryStartedSeconds);
		VideoJson->SetStringField(TEXT("start_platform_source"),
			TEXT("PresentationCapture OnStarted telemetry start, inside the start call; video-manifest.json captureEpochPlatformSeconds is the PTS origin"));
		VideoJson->SetArrayField(TEXT("scene_viewport_px"), SizeJson(Video.SceneViewport));
		VideoJson->SetArrayField(TEXT("viewport_widget_px"), SizeJson(Video.ViewportWidget));
		if (Video.bStopRequested)
		{
			VideoJson->SetObjectField(TEXT("stop_requested"), Video.StopRequest.ToJson());
			VideoJson->SetStringField(TEXT("state_at_stop"), Video.StateAtStop);
			VideoJson->SetStringField(TEXT("stop_reason"), Video.StopReason);
		}
		VideoJson->SetStringField(TEXT("manifest"), TEXT("video-manifest.json in the video directory; complete only after the recorder finalizes"));
		Root->SetObjectField(TEXT("video"), VideoJson);

		FString Text;
		const FString Temporary = LinkPath + TEXT(".tmp");
		if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text))
			|| !FFileHelper::SaveStringToFile(Text, *Temporary, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
			|| !IFileManager::Get().Move(*LinkPath, *Temporary, true, true))
		{
			UE_LOG(LogTemp, Error, TEXT("Combat capture could not write %s"), *LinkPath);
		}
	}
};
FCombatCaptureSession::FCombatCaptureSession() : Impl(MakeUnique<FImpl>())
{
}
FCombatCaptureSession::~FCombatCaptureSession()
{
	if (Impl->Video.bStarted && !Impl->Video.bStopRequested)
	{
		Impl->StopVideo(TEXT("session_destroyed"));
		Impl->WriteLink(TEXT("stopped_by_teardown"));
	}
	Impl->ReleaseObserver();
}

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
	if (Settings.bRecordVideo && !FImpl::ValidateVideo(Settings, Error))
	{
		return false;
	}
	TArray<FAnimationCaptureSubject> Subjects;
	Impl->CombatParticipants.Reset();
	Impl->RetainedWeaponContacts = Impl->RetainedPairedContacts = Impl->RetainedCommittedContacts = 0;
	Impl->Video = FImpl::FVideo();
	Impl->LinkPath.Empty();
	for (const auto &Participant : Participants)
	{
		if (ABaseCombatCharacter *Combatant = Cast<ABaseCombatCharacter>(Participant.Actor.Get()))
		{
			Impl->CombatParticipants.Emplace(Combatant, Participant.Role);
		}
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
	if (!Impl->Session.Start(World, Native, Subjects, Error,
							 MakeShared<FCombatCaptureExtension>(Settings.MaxTelemetryRecordsPerActor)))
	{
		return false;
	}
	Impl->RecordAnalysisStart(World);
	if (!Settings.bRecordVideo)
	{
		return true;
	}
	Impl->LinkPath = Impl->Session.GetOutputDirectory() / TEXT("capture-link.json");
	if (!Impl->StartVideo(World, Settings, Error))
	{
		// A requested clip is part of the evidence: never leave a silent data-only session behind.
		FString StopError;
		Impl->Session.Stop(TEXT("video_start_failed"), StopError);
		Impl->WriteLink(TEXT("video_start_failed"));
		return false;
	}
	Impl->WriteLink(TEXT("recording"));
	return true;
}
bool FCombatCaptureSession::Stop(const FString &Reason, FString &Error)
{
	// Stop the clip first so its last frames still have samples; it finalizes asynchronously.
	const bool bVideoWasActive = Impl->Video.bStarted && !Impl->Video.bStopRequested;
	Impl->StopVideo(Reason);
	const bool bSaved = Impl->Session.Stop(Reason, Error);
	Impl->ReleaseObserver();
	if (bVideoWasActive)
	{
		Impl->WriteLink(bSaved ? TEXT("stopped") : TEXT("stopped_with_errors"));
	}
	return bSaved;
}
void FCombatCaptureSession::Mark(const FString &Label)
{
	Impl->Session.Mark(Label);
}
void FCombatCaptureSession::Mark(const FString &Label, const TSharedPtr<FJsonObject> &Payload)
{
	Impl->Session.Mark(Label, Payload);
}
bool FCombatCaptureSession::ObserveContacts(ABaseCombatCharacter *Attacker, const FString &AttackerRole,
											ABaseCombatCharacter *Victim, const FString &VictimRole, FString &Error)
{
	if (!IsRecording())
	{
		Error = TEXT("Start the capture before observing contacts");
		return false;
	}
	if (!IsValid(Attacker) || !IsValid(Victim) || !Attacker->GetWeaponComponent() || !Attacker->PairedAnimationComponent)
	{
		Error = TEXT("Contact observation needs a live attacker with a weapon and paired animation component, and a live victim");
		return false;
	}
	Impl->ReleaseObserver();
	Impl->Observer.Reset(NewObject<UCombatCaptureContactObserver>(GetTransientPackage()));
	Impl->Observer->Bind(this, Attacker, AttackerRole, Victim, VictimRole);
	return true;
}
bool FCombatCaptureSession::ObserveParticipantContacts(FString &Error)
{
	if (!IsRecording())
	{
		Error = TEXT("Start the capture before observing contacts");
		return false;
	}
	TArray<TPair<ABaseCombatCharacter *, FString>> Combatants;
	for (const auto &Participant : Impl->CombatParticipants)
	{
		if (ABaseCombatCharacter *Combatant = Participant.Key.Get()) { Combatants.Emplace(Combatant, Participant.Value); }
	}
	if (Combatants.Num() < 2)
	{
		Error = TEXT("Participant contact observation needs at least two recorded combat characters");
		return false;
	}
	Impl->ReleaseObserver();
	Impl->Observer.Reset(NewObject<UCombatCaptureContactObserver>(GetTransientPackage()));
	Impl->Observer->BindParticipants(this, Combatants);
	return true;
}
void FCombatCaptureSession::GetContactCounts(int32 &OutWeapon, int32 &OutPaired) const
{
	OutWeapon = Impl->Observer.IsValid() ? Impl->Observer->GetWeaponContactCount() : Impl->RetainedWeaponContacts;
	OutPaired = Impl->Observer.IsValid() ? Impl->Observer->GetPairedContactCount() : Impl->RetainedPairedContacts;
}
int32 FCombatCaptureSession::GetCommittedContactCount() const
{
	return Impl->Observer.IsValid() ? Impl->Observer->GetCommittedContactCount() : Impl->RetainedCommittedContacts;
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
bool FCombatCaptureSession::HasVideo() const
{
	return Impl->Video.bStarted;
}
bool FCombatCaptureSession::IsVideoFinalizing() const
{
	return Impl->Video.bStarted && PresentationRecording::IsBusy() && Impl->IsOwnClip();
}
FString FCombatCaptureSession::GetVideoDirectory() const
{
	return Impl->Video.Directory;
}
FString FCombatCaptureSession::GetLinkPath() const
{
	return Impl->LinkPath;
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
