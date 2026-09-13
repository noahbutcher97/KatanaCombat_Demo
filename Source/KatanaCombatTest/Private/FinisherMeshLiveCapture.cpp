#include "FinisherMeshObservation.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/PlayerCharacter.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/TargetingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"

namespace FinisherMeshObservation
{
namespace
{
using FSnapshot = TSharedPtr<const FAnimationMeshSnapshot, ESPMode::ThreadSafe>;
const FAnimationMeshLimits LiveLimits{16, 250000, 1500000, 64, 512, 256ll * 1024 * 1024};

/** A bounded observer of one existing gameplay pair. It never drives animation. */
class FLivePairCapture
{
public:
	FLivePairCapture()
	{
		if (OutputRoot().IsEmpty() || FPlatformMisc::GetEnvironmentVariable(TEXT("KATANA_MESH_BATCH")) != TEXT("1")) { return; }
		Root = OutputRoot() / TEXT("live-batch");
		IFileManager::Get().MakeDirectory(*Root, true);
		TickHandle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FLivePairCapture::Observe);
		CleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw(this, &FLivePairCapture::Cleanup);
	}
	~FLivePairCapture()
	{
		FWorldDelegates::OnWorldTickEnd.Remove(TickHandle);
		FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
		if (bEnrolled && !bWritten)
		{
			if (!bFinished) { Finish(TEXT("module_shutdown_before_interval")); }
			Flush(TEXT("module_shutdown"));
		}
	}
private:
	struct FAttempt
	{
		TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		TArray<FSnapshot> Snapshots;
	};
	FDelegateHandle TickHandle, CleanupHandle;
	FString Root;
	TWeakObjectPtr<UWorld> BoundWorld;
	TWeakObjectPtr<APlayerCharacter> Player;
	TWeakObjectPtr<USkeletalMeshComponent> Body, Attacker;
	TWeakObjectPtr<UStaticMeshComponent> Weapon;
	TWeakObjectPtr<UAnimMontage> Montage;
	TSharedPtr<FAnimationMeshBudget, ESPMode::ThreadSafe> Budget, TinyBudget;
	TUniquePtr<FAnimationCaptureMeshReference> BodySampler, WeaponSampler, TinyBody, TinyWeapon;
	TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	TArray<FAttempt> Attempts;
	bool bFinished = false;
	bool bWritten = false;
	bool bEnrolled = false;
	int32 NextSample = 0;

	FAnimationMeshEnrollment LiveEnrollment(UMeshComponent* Component, const FString& Role) const
	{
		auto E = Enrollment(Component, Role);
		E.PosePolicy = EAnimationMeshPosePolicy::FinalizedAnimation;
		E.StreamId = TEXT("live-finisher-batch"); E.ConfigurationId = TEXT("katana-live-finisher-finalized-v1");
		return E;
	}

	TSharedRef<FJsonObject> CurrentInventory() const
	{
		auto J = MakeShared<FJsonObject>();
		if (Body.IsValid()) { J->SetObjectField(TEXT("victim"), Inventory(Body.Get(), TEXT("victim"))); }
		if (Attacker.IsValid()) { J->SetObjectField(TEXT("attacker"), Inventory(Attacker.Get(), TEXT("attacker"))); }
		if (Weapon.IsValid()) { J->SetObjectField(TEXT("katana"), Inventory(Weapon.Get(), TEXT("katana"))); }
		return J;
	}

	bool Enroll(APlayerCharacter* Owner, UAnimMontage* ActiveMontage, double Position)
	{
		bEnrolled = true;
		Player = Owner; BoundWorld = Owner->GetWorld(); Montage = ActiveMontage; Attacker = Owner->GetMesh();
		auto* Target = Owner->TargetingComponent->GetCurrentTarget();
		Body = Target ? Target->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
		TArray<UStaticMeshComponent*> Components; Owner->GetComponents(Components);
		int32 Matches = 0;
		for (auto* Component : Components)
		{
			if (Component->GetAttachParent() == Attacker.Get() && Component->GetAttachSocketName() == TEXT("weapon_r")
				&& Component->GetStaticMesh() && Component->GetStaticMesh()->GetOutermost()->GetName()
				== TEXT("/Game/Assets/Characters/CyberpunkRunner/Meshes/SKM_Katana")) { Weapon = Component; ++Matches; }
		}
		Report->SetStringField(TEXT("workflow"), TEXT("live finalized AnimBlueprint bone/rigid reference; pre-material surface"));
		Report->SetStringField(TEXT("montage"), GetPathNameSafe(ActiveMontage));
		Report->SetNumberField(TEXT("enrollment_montage_s"), Position);
		Report->SetNumberField(TEXT("enrollment_engine_frame"), GFrameCounter);
		Report->SetNumberField(TEXT("required_interval_start_montage_s"), .4);
		Report->SetNumberField(TEXT("required_interval_end_montage_s"), .55);
		Report->SetNumberField(TEXT("requested_sample_hz"), 30);
		Report->SetNumberField(TEXT("required_max_gap_s"), .05);
		Report->SetNumberField(TEXT("expected_attempts"), 7);
		Report->SetObjectField(TEXT("enrollment_inventory"), CurrentInventory());
		if (!Body.IsValid() || Matches != 1) { Finish(TEXT("participant_selection_unavailable")); return false; }
		auto Shared = MakeShared<FAnimationCaptureBudget, ESPMode::ThreadSafe>(FAnimationCaptureBudgetLimits{16, LiveLimits.MaxBytes});
		Budget = MakeShared<FAnimationMeshBudget, ESPMode::ThreadSafe>(LiveLimits, Shared);
		FString Error;
		BodySampler = FAnimationCaptureMeshReference::Create(Body.Get(), LiveEnrollment(Body.Get(), TEXT("victim")), Budget.ToSharedRef(), Error);
		Report->SetStringField(TEXT("victim_enrollment_error"), Error);
		WeaponSampler = FAnimationCaptureMeshReference::Create(Weapon.Get(), LiveEnrollment(Weapon.Get(), TEXT("katana")), Budget.ToSharedRef(), Error);
		Report->SetStringField(TEXT("katana_enrollment_error"), Error);
		if (!BodySampler || !WeaponSampler) { Finish(TEXT("enrollment_rejected")); return false; }
		// Separate negative admission control, enrolled early enough for real witnesses.
		auto TinyLimits = LiveLimits; TinyLimits.MaxSnapshots = 1;
		TinyBudget = MakeShared<FAnimationMeshBudget, ESPMode::ThreadSafe>(TinyLimits);
		TinyBody = FAnimationCaptureMeshReference::Create(Body.Get(), LiveEnrollment(Body.Get(), TEXT("victim")), TinyBudget.ToSharedRef(), Error);
		TinyWeapon = FAnimationCaptureMeshReference::Create(Weapon.Get(), LiveEnrollment(Weapon.Get(), TEXT("katana")), TinyBudget.ToSharedRef(), Error);
		return true;
	}

	void Controls()
	{
		auto Results = MakeShared<FJsonObject>();
		const auto Reject = [&](const TCHAR* Name, TArray<FAnimationCaptureMeshReference*> Samplers, int32 Max)
		{
			TArray<FSnapshot> Outputs; FString Error;
			const bool Accepted = FAnimationCaptureMeshReference::CaptureBatch(Samplers, Name, Max, Outputs, Error);
			auto Row = MakeShared<FJsonObject>(); Row->SetBoolField(TEXT("rejected_without_partial_pair"), !Accepted && Outputs.IsEmpty());
			Row->SetStringField(TEXT("error"), Error); Results->SetObjectField(Name, Row);
			if (Accepted || !Outputs.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("Live mesh negative control unexpectedly admitted: %s"), Name); }
		};
		Reject(TEXT("exhausted_snapshot_budget"), {TinyBody.Get(), TinyWeapon.Get()}, 2);
		Results->SetNumberField(TEXT("budget_control_retained_snapshots"), TinyBudget->LiveSnapshots());
		Results->SetNumberField(TEXT("budget_control_retained_bytes"), TinyBudget->LiveBytes());
		Reject(TEXT("duplicate_component"), {BodySampler.Get(), BodySampler.Get()}, 2);
		FString Error;
		auto Unwitnessed = FAnimationCaptureMeshReference::Create(Body.Get(), LiveEnrollment(Body.Get(), TEXT("victim")), Budget.ToSharedRef(), Error);
		Results->SetBoolField(TEXT("unwitnessed_control_enrolled"), Unwitnessed.IsValid());
		Results->SetStringField(TEXT("unwitnessed_enrollment_error"), Error);
		Reject(TEXT("missing_subsequent_finalization"), {Unwitnessed.Get(), WeaponSampler.Get()}, 2);
		Report->SetObjectField(TEXT("native_controls"), Results);
	}

	void Observe(UWorld* World, ELevelTick, float)
	{
		if (bFinished || World->WorldType != EWorldType::PIE) { return; }
		if (!bEnrolled)
		{
			for (TActorIterator<APlayerCharacter> It(World); It; ++It)
			{
				auto* Anim = It->GetMesh()->GetAnimInstance(); auto* Current = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
				if (Current && It->PairedAnimationComponent->IsPairedAnimationActive() && !It->PairedAnimationComponent->IsPreparingPairedEntry())
				{
					if (!Enroll(*It, Current, Anim->Montage_GetPosition(Current))) { return; }
					return; // Require a subsequent natural finalization, never drive a pose.
				}
			}
			return;
		}
		if (BoundWorld.Get() != World) { return; }
		if (!Player.IsValid() || !Body.IsValid() || !Attacker.IsValid() || !Weapon.IsValid())
		{
			Finish(TEXT("participant_retired")); return;
		}
		auto* Anim = Player->GetMesh()->GetAnimInstance();
		if (!Anim || !Montage.IsValid() || !Anim->Montage_IsPlaying(Montage.Get())) { Finish(TEXT("playback_ended_before_interval")); return; }
		const double Position = Anim->Montage_GetPosition(Montage.Get());
		const double Requested = .375 + NextSample / 30.;
		if (Position + 1.e-6 < Requested) { return; }
		FAttempt& Attempt = Attempts.AddDefaulted_GetRef();
		const FString Request = FString::Printf(TEXT("live-pair-%d"), NextSample);
		Attempt.Row->SetStringField(TEXT("batch_request_id"), Request);
		Attempt.Row->SetNumberField(TEXT("requested_montage_s"), Requested);
		Attempt.Row->SetNumberField(TEXT("actual_montage_s"), Position);
		Attempt.Row->SetNumberField(TEXT("world_time_s"), World->GetTimeSeconds());
		Attempt.Row->SetNumberField(TEXT("engine_frame"), GFrameCounter);
		Attempt.Row->SetBoolField(TEXT("world_in_tick"), World->bInTick);
		FString Error; const double Start = FPlatformTime::Seconds();
		const TArray<FAnimationCaptureMeshReference*> Samplers{BodySampler.Get(), WeaponSampler.Get()};
		const bool Acquired = FAnimationCaptureMeshReference::CaptureBatch(Samplers, Request, 2, Attempt.Snapshots, Error);
		Attempt.Row->SetNumberField(TEXT("capture_wall_ms"), (FPlatformTime::Seconds() - Start) * 1000);
		Attempt.Row->SetBoolField(TEXT("acquired"), Acquired); Attempt.Row->SetStringField(TEXT("capture_error"), Error);
		Attempt.Row->SetNumberField(TEXT("output_count"), Attempt.Snapshots.Num());
		const double InventoryStart = FPlatformTime::Seconds();
		Attempt.Row->SetObjectField(TEXT("inventory"), CurrentInventory());
		Attempt.Row->SetNumberField(TEXT("inventory_wall_ms"), (FPlatformTime::Seconds() - InventoryStart) * 1000);
		if (NextSample == 0) { Controls(); }
		++NextSample;
		if (NextSample == 7) { Finish(TEXT("sampling_finished")); }
	}

	void Finish(const TCHAR* Reason)
	{
		if (bFinished) { return; } bFinished = true;
		Report->SetStringField(TEXT("stop_reason"), Reason);
		// Keep the bounded immutable snapshots until PIE teardown. File I/O inside
		// the active run otherwise introduces a frame gap into its RGB/pose evidence.
	}
	void Flush(const TCHAR* Phase)
	{
		if (bWritten) { return; } bWritten = true;
		Report->SetStringField(TEXT("export_phase"), Phase);
		TArray<TSharedPtr<FJsonValue>> Rows;
		for (int32 Index = 0; Index < Attempts.Num(); ++Index)
		{
			auto& Attempt = Attempts[Index];
			for (int32 Side = 0; Side < Attempt.Snapshots.Num(); ++Side)
			{
				const FString Role = Side == 0 ? TEXT("victim") : TEXT("katana");
				const FString Bundle = FString::Printf(TEXT("%s-%d"), *Role, Index);
				const auto& Data = Attempt.Snapshots[Side]->Data();
				auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("bundle"), Bundle);
				Row->SetStringField(TEXT("request_id"), Data.RequestId);
				Row->SetNumberField(TEXT("acquired_s"), Data.AcquiredSeconds); Row->SetNumberField(TEXT("completed_s"), Data.CompletedSeconds);
				Row->SetNumberField(TEXT("engine_frame"), Data.FrameId); Row->SetNumberField(TEXT("pose_revision"), Data.PoseRevision);
				Row->SetNumberField(TEXT("component_generation"), Data.Enrollment.ComponentGeneration);
				Row->SetStringField(TEXT("configuration_id"), Data.ConfigurationId); Row->SetStringField(TEXT("topology_id"), Data.TopologyId);
				FString Error; const double Start = FPlatformTime::Seconds();
				Row->SetBoolField(TEXT("exported"), AnimationCaptureMeshReplay::Write(Root, Bundle, *Attempt.Snapshots[Side], Error));
				Row->SetNumberField(TEXT("export_wall_ms"), (FPlatformTime::Seconds() - Start) * 1000); Row->SetStringField(TEXT("export_error"), Error);
				Attempt.Row->SetObjectField(Role, Row);
			}
			Rows.Add(MakeShared<FJsonValueObject>(Attempt.Row));
		}
		Report->SetArrayField(TEXT("attempts"), Rows);
		if (Budget) { Report->SetNumberField(TEXT("peak_reserved_bytes"), Budget->PeakBytes()); }
		Attempts.Reset(); BodySampler.Reset(); WeaponSampler.Reset(); TinyBody.Reset(); TinyWeapon.Reset();
		if (Budget) { Report->SetNumberField(TEXT("retained_snapshots_after_release"), Budget->LiveSnapshots()); }
		if (!WriteJson(Root / TEXT("live-pairs.json"), Report)) { UE_LOG(LogTemp, Error, TEXT("Live mesh batch report could not be written")); }
	}
	void Cleanup(UWorld* World, bool, bool)
	{
		if (BoundWorld.Get() == World)
		{
			if (!bFinished) { Finish(TEXT("world_cleanup_before_interval")); }
			Flush(TEXT("world_cleanup"));
		}
	}
};
FLivePairCapture LivePairCapture;
}
}
