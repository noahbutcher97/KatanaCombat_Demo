// Copyright Epic Games, Inc. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "CombatScenarioPlacement.h"
#include "CombatScenarioGrounding.h"
#include "Analysis/CombatCaptureSession.h"
#include "Analysis/PairedWarpTuning.h"
#include "Analysis/MontagePlaybackInspection.h"
#include "AI/CombatTokenSubsystem.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyState_PairedAnimationCollision.h"
#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "AnimNotifyState_MotionWarping.h"
#include "RootMotionModifier.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Components/StateTreeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/WeaponComponent.h"
#include "Data/WeaponData.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/AttackData.h"
#include "Data/PairedAnimationData.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Editor/UnrealEdEngine.h"
#include "PlayInEditorDataTypes.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "InputActionValue.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"
#include "UnrealEdGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace
{
TSharedPtr<FJsonObject> ReadScenarioJson(const FString& Path)
{
	FString Text; TSharedPtr<FJsonObject> Root;
	if (FFileHelper::LoadFileToString(Text, *Path)) { FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root); }
	return Root;
}

FVector ScenarioVector(const TArray<TSharedPtr<FJsonValue>>& Values)
{
	return Values.Num() == 3 ? FVector(Values[0]->AsNumber(), Values[1]->AsNumber(), Values[2]->AsNumber()) : FVector::ZeroVector;
}

/** Opt-in PresentationCapture clip requested by the runner (-CombatCaptureVideo=1). */
struct FScenarioVideoRequest
{
	bool bEnabled = false;
	int32 FramesPerSecond = 60;
	int32 Resolution = 720;

	/** The recorder's output box for the resolution; a viewport of exactly this size is recorded unscaled. */
	FIntPoint Size() const
	{
		return Resolution == 360 ? FIntPoint(640, 360) : Resolution == 1080 ? FIntPoint(1920, 1080) : FIntPoint(1280, 720);
	}
	static FScenarioVideoRequest FromCommandLine()
	{
		FScenarioVideoRequest Request;
		int32 Enabled = 0;
		FParse::Value(FCommandLine::Get(), TEXT("CombatCaptureVideo="), Enabled);
		Request.bEnabled = Enabled == 1;
		FParse::Value(FCommandLine::Get(), TEXT("CombatCaptureVideoFPS="), Request.FramesPerSecond);
		FParse::Value(FCommandLine::Get(), TEXT("CombatCaptureVideoResolution="), Request.Resolution);
		return Request;
	}
};

/**
 * When PIE ends, the engine copies a floating PIE window's size and position into the editor play
 * settings and saves them to the checkout's per-user config (UEditorEngine::EndPlayMap). A video run
 * must not change the user's "New Editor Window (PIE)" settings, so it restores what was there.
 */
struct FPlaySettingsSnapshot
{
	TArray<FIntPoint> MultipleInstancePositions;
	FIntPoint LastSize = FIntPoint::ZeroValue, NewWindowPosition = FIntPoint::ZeroValue;
	int32 NewWindowWidth = 0, NewWindowHeight = 0;
	bool bCenterNewWindow = false, bTaken = false;

	void Take()
	{
		const ULevelEditorPlaySettings* Settings = GetDefault<ULevelEditorPlaySettings>();
		MultipleInstancePositions = Settings->MultipleInstancePositions;
		LastSize = Settings->LastSize;
		NewWindowPosition = Settings->NewWindowPosition;
		NewWindowWidth = Settings->NewWindowWidth;
		NewWindowHeight = Settings->NewWindowHeight;
		bCenterNewWindow = Settings->CenterNewWindow;
		bTaken = true;
	}
	void Restore()
	{
		if (!bTaken) { return; }
		ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
		Settings->MultipleInstancePositions = MultipleInstancePositions;
		Settings->LastSize = LastSize;
		Settings->NewWindowPosition = NewWindowPosition;
		Settings->NewWindowWidth = NewWindowWidth;
		Settings->NewWindowHeight = NewWindowHeight;
		Settings->CenterNewWindow = bCenterNewWindow;
		Settings->SaveConfig();
		bTaken = false;
	}
};

/** Restores the play settings once PIE has really ended (EndPlayMap runs on a later editor tick). */
class FRestorePlaySettingsCommand : public IAutomationLatentCommand
{
public:
	explicit FRestorePlaySettingsCommand(TSharedRef<FPlaySettingsSnapshot> InSnapshot) : Snapshot(InSnapshot) {}
	bool Update() override
	{
		if (StartWall == 0) { StartWall = FPlatformTime::Seconds(); }
		if (GEditor && GEditor->PlayWorld && FPlatformTime::Seconds() - StartWall < 10) { return false; }
		Snapshot->Restore();
		return true;
	}
private:
	TSharedRef<FPlaySettingsSnapshot> Snapshot;
	double StartWall = 0;
};

/**
 * Starts PIE in its own window whose client area is exactly the requested size. The recorder
 * reads the viewport widget's area of the window backbuffer, so the widget must match the output
 * box to record without scaling. In the level editor viewport the widget size comes from the
 * editor layout instead: 759x378 under -RenderOffScreen in the 2026-10-03 spike, whatever -ResX/-ResY say.
 */
class FStartPIEInWindowCommand : public IAutomationLatentCommand
{
public:
	FStartPIEInWindowCommand(FIntPoint InSize, TSharedRef<FPlaySettingsSnapshot> InSnapshot) : Size(InSize), Snapshot(InSnapshot) {}
	bool Update() override
	{
		Snapshot->Take();
		ULevelEditorPlaySettings* PlaySettings = NewObject<ULevelEditorPlaySettings>(GetTransientPackage()); // Starts from the user's defaults.
		PlaySettings->NewWindowWidth = Size.X;
		PlaySettings->NewWindowHeight = Size.Y;
		PlaySettings->CenterNewWindow = true;
		PlaySettings->LastExecutedPlayModeType = PlayMode_InEditorFloating;
		FRequestPlaySessionParams Params;
		Params.EditorPlaySettings = PlaySettings;
		if (GUnrealEd->CheckForPlayerStart() == nullptr) { FAutomationEditorCommonUtils::SetPlaySessionStartToActiveViewport(Params); }
		GUnrealEd->RequestPlaySession(Params);
		return true;
	}
private:
	FIntPoint Size;
	TSharedRef<FPlaySettingsSnapshot> Snapshot;
};

/**
 * Resizes the PIE window until its game viewport widget is exactly Box. A window's client size
 * includes Slate's own title bar (1280x720 left a 1280x688 viewport in the first trial), so the
 * window is grown by the measured shortfall instead of assuming any chrome size. Gives up after a
 * few attempts; the capture link then records the mismatch and the clip is flagged as scaled.
 */
class FFitPIEViewportCommand : public IAutomationLatentCommand
{
public:
	explicit FFitPIEViewportCommand(FIntPoint InBox) : Box(InBox) {}
	bool Update() override
	{
		if (StartWall == 0) { StartWall = FPlatformTime::Seconds(); }
		const bool bTimedOut = FPlatformTime::Seconds() - StartWall > 20;
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		UGameViewportClient* Client = World && World->WorldType == EWorldType::PIE ? World->GetGameViewport() : nullptr;
		const TSharedPtr<SViewport> Widget = Client ? Client->GetGameViewportWidget() : nullptr;
		const TSharedPtr<SWindow> Window = Client ? Client->GetWindow() : nullptr;
		if (!Widget || !Window) { return bTimedOut; }
		if (SettleFrames > 0) { --SettleFrames; return false; } // Let the layout follow the last resize.
		const FVector2D Measured = Widget->GetCachedGeometry().GetAbsoluteSize();
		const FIntPoint Current(FMath::RoundToInt(Measured.X), FMath::RoundToInt(Measured.Y));
		if (Current == Box || Attempts >= 4 || bTimedOut)
		{
			UE_LOG(LogTemp, Display, TEXT("PIE viewport widget %dx%d for a %dx%d video box after %d resize(s)"), Current.X, Current.Y, Box.X, Box.Y, Attempts);
			return true;
		}
		if (Current.GetMin() <= 0) { return false; }
		++Attempts;
		SettleFrames = 2;
		Window->ReshapeWindow(Window->GetPositionInScreen(), Window->GetSizeInScreen() + FVector2D(Box - Current));
		return false;
	}
private:
	FIntPoint Box;
	double StartWall = 0;
	int32 Attempts = 0, SettleFrames = 0;
};

TSharedPtr<FPlaySettingsSnapshot> QueueScenarioPIE()
{
	const FScenarioVideoRequest Video = FScenarioVideoRequest::FromCommandLine();
	if (!Video.bEnabled)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
		return nullptr;
	}
	TSharedRef<FPlaySettingsSnapshot> Snapshot = MakeShared<FPlaySettingsSnapshot>();
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEInWindowCommand(Video.Size(), Snapshot));
	ADD_LATENT_AUTOMATION_COMMAND(FFitPIEViewportCommand(Video.Size()));
	return Snapshot;
}

void QueueScenarioEnd(const TSharedPtr<FPlaySettingsSnapshot>& Snapshot)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	if (Snapshot.IsValid()) { ADD_LATENT_AUTOMATION_COMMAND(FRestorePlaySettingsCommand(Snapshot.ToSharedRef())); }
}

class FCombatRecoveryScenario : public IAutomationLatentCommand
{
public:
	FCombatRecoveryScenario(FAutomationTestBase* InTest, TSharedPtr<FJsonObject> InDefinition, FString InMap, FString InVariant)
		: Test(InTest), Definition(MoveTemp(InDefinition)), MapKey(MoveTemp(InMap)), Variant(MoveTemp(InVariant)) {}
	~FCombatRecoveryScenario() { Cleanup(); }

	bool Update() override
	{
		if (StartWall == 0) { StartWall = FPlatformTime::Seconds(); }
		if (bDone) { return true; }
		if (bCaptureStopped) { return Finish(); } // Waiting for the clip to finalize.
		if (FPlatformTime::Seconds() - StartWall > 90)
		{
			Check(TEXT("scenario_completes"), false, TEXT("Wall watchdog expired")); return Finish();
		}
		if (!World.IsValid())
		{
			UWorld* Candidate = AutomationCommon::GetAnyGameWorld();
			if (!Candidate || Candidate->WorldType != EWorldType::PIE) { return false; }
			World = Candidate;
			if (!Initialize()) { return Finish(); }
			return false;
		}
		if (!Player.IsValid() || !Victim.IsValid())
		{
			Check(TEXT("participants_retained"), false, TEXT("Required participant disappeared")); return Finish();
		}
		const double Now = World->GetTimeSeconds();
		if (Now - BeginSimulation > Definition->GetNumberField(TEXT("timeout_s")))
		{
			Check(TEXT("scenario_completes"), false, TEXT("Simulation deadline expired")); return Finish();
		}
		UCombatComponent* Combat = Player->CombatComponent;
		if (Definition->GetStringField(TEXT("scenario")) == TEXT("HoldReleaseRecovery")) { return UpdateHoldRecovery(); }
		UPairedAnimationComponent* Paired = Player->PairedAnimationComponent;
		if (bEntryRequested && bRequested && !bReleased)
		{
			if (Paired->IsPreparingPairedEntry())
			{
				if (!bEntryObserved) { Mark(TEXT("entry_preparation_observed")); bEntryObserved = true; }
				bEntryNoEarlyPlayback &= !Player->GetMesh()->GetAnimInstance()->Montage_IsPlaying(TuningPair->AttackerMontage)
					&& !Victim->GetMesh()->GetAnimInstance()->Montage_IsPlaying(TuningPair->VictimMontage);
				bEntryNoEarlyDamage &= Victim->CurrentHealth == VictimHealthAtRequest && !Victim->IsDeadOrDying();
			}
			else if (bEntryObserved && !bEntryResultObserved)
			{
				EntryOutcome = Paired->GetLastPairedEntryOutcome(); bEntryResultObserved = true;
				Mark(EntryOutcome == EAlignmentMotionOutcome::Reached ? TEXT("entry_ready_observed") : TEXT("entry_failure_observed"));
			}
		}
		ObserveBystanders();
		// Observe damage independently of capture sampling. This is the first
		// automation observation, not an exact notify callback timestamp.
		if (bRequested && !bReleased && Paired->IsPairedAnimationActive() && TuningPair.IsValid()
			&& !FirstLethalMontageTime.IsSet() && Victim->IsDeadOrDying())
		{
			FirstLethalMontageTime = Player->GetMesh()->GetAnimInstance()->Montage_GetPosition(TuningPair->AttackerMontage);
		}
		if (!bRequested)
		{
			if (Now - BeginSimulation < Definition->GetNumberField(TEXT("warmup_s"))) { return false; }
			// Reset only the chosen pair at the declared scenario boundary. Bystander
			// controller/state-tree ticks remain active throughout the observation.
			Victim->CombatAIComponent->AbortAttack();
			Victim->GetCharacterMovement()->StopMovementImmediately();
			Victim->SetActorLocation(Base + ScenarioVector(Definition->GetArrayField(TEXT("victim_offset_cm"))), false, nullptr, ETeleportType::TeleportPhysics);
			Victim->SetActorRotation(FRotator(0, 180, 0));
			Victim->CurrentHealth = 1.0f;
			if (!ApplyNamedPlacement(true)) { return Finish(); }
			if (bEntryRequested && TuningPair.IsValid())
			{
				const auto& Entry = TuningPair->Entry;
				const FTransform OwnerPose(Player->GetActorRotation(), Player->GetActorLocation());
				const FTransform VictimPose(Victim->GetActorRotation(), Victim->GetActorLocation());
				const bool bMoveInitiator = Entry.MovingRole == EPairedEntryMovingRole::Initiator;
				bEntryReadyAtRequest = AlignmentMotion::CalculateStep(bMoveInitiator ? OwnerPose : VictimPose,
					bMoveInitiator ? Entry.VictimRelativeTransform.Inverse() * VictimPose : Entry.VictimRelativeTransform * OwnerPose,
					Entry.Limits, FAlignmentMotionState(), 0).Outcome == EAlignmentMotionOutcome::Reached;
			}
			VictimHealthAtRequest = Victim->CurrentHealth;
			Player->TargetingComponent->SetCurrentTarget(Victim.Get());
			Mark(TEXT("finisher_requested"));
			Inject(Player->LightAttackAction, FInputActionValue(true));
			bRequested = true;
			return false;
		}
		if (!bStarted)
		{
			if (Paired->IsPairedAnimationActive())
			{
				bStarted = true; PairedStart = Now;
				Mark(TEXT("finisher_started"));
				InitialGeneration = Combat->GetCurrentAttackGeneration();
				Check(TEXT("paired_takeover"), Paired->IsInputBlocked() && Victim->HitReactionComponent->IsInPairedAnimationState()
					&& !Victim->CombatComponent->CanProcessInput(EInputType::LightAttack) && Victim->CombatComponent->IsMovementInputSuppressed(),
					TEXT("Attacker paired ownership and victim hit-reaction ownership suppress normal input"));
				Inject(Player->LightAttackAction, FInputActionValue(false));
			}
			else if (Now - BeginSimulation > 4.0)
			{
				Check(TEXT("finisher_started"), false, FString::Printf(TEXT("Public input failed to start finisher; vulnerable=%d phase=%d attack=%s"), Victim->HitReactionComponent->IsVulnerableToFinisher(), int32(Combat->GetCurrentPhase()), *GetNameSafe(Combat->GetDefaultLightAttack())));
				return Finish();
			}
			return false;
		}
		if (!bReleased && Paired->IsPairedAnimationActive())
		{
			if (Now - PairedStart < 1.0 && Paired->GetActivePairedStateLeaseCount() > 0
				&& Victim->PairedAnimationComponent->GetActivePairedStateLeaseCount() > 0)
			{
				bPairedCollisionObserved = true;
				const bool bAttackerIgnores = Player->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Victim.Get());
				const bool bVictimIgnores = Victim->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Player.Get());
				if (bPairedCollisionHeld && (!bAttackerIgnores || !bVictimIgnores))
				{
					UE_LOG(LogTemp, Warning, TEXT("Paired collision ownership: attacker ignores victim=%d, victim ignores attacker=%d; tracked partners=%d/%d"),
						bAttackerIgnores, bVictimIgnores, Paired->GetPairedPartnerCount(), Victim->PairedAnimationComponent->GetPairedPartnerCount());
				}
				bPairedCollisionHeld &= bAttackerIgnores && bVictimIgnores;
			}
			if (Now - PairedStart >= Definition->GetNumberField(TEXT("paired_input_after_s")))
			{
				if (!bBlockedInputSent) { Mark(TEXT("input_during_finisher")); bBlockedInputSent = true; }
				Inject(Player->HeavyAttackAction, FInputActionValue(true));
				Inject(Player->MoveAction, FInputActionValue(FVector2D(0, 1)));
				bMovementSampleObserved |= !Combat->GetMovementInputSample().CameraRelativeInput.IsNearlyZero();
				bInputSuppressionHeld &= Combat->IsMovementInputSuppressed() && Combat->GetQueueSize() == 0 && Combat->GetCurrentAttackGeneration() == InitialGeneration;
			}
			if (!bControlApplied && ControlOffsetCm != 0 && Now - PairedStart >= 0.25)
			{
				Player->GetMesh()->AddLocalOffset(FVector(0, 0, ControlOffsetCm));
				bControlApplied = true; Mark(TEXT("controlled_pose_displacement"));
			}
			const double InterruptAfter = Definition->GetObjectField(TEXT("variants"))->GetNumberField(Variant);
			if (!bInterrupted && InterruptAfter >= 0 && Now - PairedStart >= InterruptAfter)
			{
				bVictimDeadAtInterruption = Victim->IsDeadOrDying();
				if (ExpectedPrimarySyncTime.IsSet() && TuningPair.IsValid())
				{
					InterruptionMontageTime = Player->GetMesh()->GetAnimInstance()->Montage_GetPosition(TuningPair->AttackerMontage);
					if (InterruptionMontageTime.GetValue() < ExpectedPrimarySyncTime.GetValue())
					{
						Check(TEXT("pre_sync_interruption_preserves_health"), Victim->CurrentHealth == VictimHealthAtRequest && !bVictimDeadAtInterruption,
							TEXT("Interruption before the requested primary sync retains request-time health"));
					}
				}
				Mark(TEXT("interruption_requested")); Paired->CancelPairedAnimation(); bInterrupted = true;
			}
			return false;
		}
		if (!bReleased)
		{
			bReleased = true; RecoveryStart = Now; Mark(TEXT("ownership_released"));
			Check(TEXT("paired_collision_ownership"), bPairedCollisionObserved && bPairedCollisionHeld,
				TEXT("Both active collision leases suppress movement collisions with the paired participant"));
			Check(TEXT("input_during_finisher"), bBlockedInputSent && bMovementSampleObserved && bInputSuppressionHeld,
				TEXT("Enhanced Input movement reaches combat sampling while attacks and movement application remain suppressed"));
			Check(TEXT("paired_cleanup"), !Paired->IsInputBlocked() && !Victim->PairedAnimationComponent->IsInputBlocked() && Paired->GetPairedPartnerCount() == 0,
				TEXT("Real montage completion/interruption releases partner and input ownership"));
			Check(TEXT("victim_outcome"), bInterrupted ? Victim->IsDeadOrDying() == bVictimDeadAtInterruption : Victim->IsDeadOrDying(),
				bInterrupted ? TEXT("Cancellation preserves the damage already committed by the authored sync notify") : TEXT("Completed finisher is lethal"));
			Check(TEXT("victim_token_cleanup"), !Victim->CombatAIComponent->HasAttackToken(), TEXT("Victim retains no attack token at paired cleanup"));
			Inject(Player->HeavyAttackAction, FInputActionValue(false));
			Inject(Player->MoveAction, FInputActionValue(FVector2D::ZeroVector));
			Mark(TEXT("held_input_released"));
			RecoveryGeneration = Combat->GetCurrentAttackGeneration();
			return false;
		}
		if (!bRepressed && Now - RecoveryStart >= 0.15)
		{
			// Survivor is no longer a finisher opportunity; the next edge must execute
			// a normal attack. This instance-only fixture change is recorded explicitly.
			if (!Victim->IsDeadOrDying()) { Victim->CurrentHealth = Victim->MaxHealth; }
			Player->TargetingComponent->SetCurrentTarget(nullptr);
			Mark(TEXT("recovery_repress")); Inject(Player->LightAttackAction, FInputActionValue(true));
			bRepressed = true;
		}
		if (bRepressed)
		{
			Inject(Player->MoveAction, FInputActionValue(FVector2D(0, -1)));
			bRecoveryAttackObserved |= Combat->GetCurrentAttackGeneration() > RecoveryGeneration;
			bRecoveryMovementObserved |= !Combat->IsMovementInputSuppressed() && Player->GetCharacterMovement()->GetCurrentAcceleration().SizeSquared() > 1;
			if (Now - RecoveryStart > 0.3) { Inject(Player->LightAttackAction, FInputActionValue(false)); }
		}
		if (Now - RecoveryStart < Definition->GetNumberField(TEXT("recovery_observation_s"))) { return false; }
		Mark(TEXT("recovery_observed"));
		Check(TEXT("repress_executes_attack"), bRecoveryAttackObserved, TEXT("A fresh post-release input advances the attack generation"));
		Check(TEXT("movement_recovers"), bRecoveryMovementObserved, TEXT("Post-paired movement reaches CharacterMovement acceleration"));
		Check(TEXT("bystanders_remain_active"), bBystanderLogicActive && bBystanderAttackObserved, TEXT("Bystander StateTrees remain running and a bystander executes an attack"));
		Check(TEXT("scenario_completes"), true, TEXT("All declared observation stages completed"));
		return Finish();
	}

private:
	bool UpdateHoldRecovery()
	{
		UCombatComponent* Combat = Player->CombatComponent;
		const double Now = World->GetTimeSeconds();
		if (Camera.IsValid())
		{
			// Keep the complete release/recovery interval reviewable while root motion
			// and restored locomotion move the player away from the initial position.
			Camera->SetActorLocation(Player->GetActorLocation() + CameraOffset);
			const FVector Focus = Player->GetActorLocation() + CameraFocus;
			Camera->SetActorRotation((Focus - Camera->GetActorLocation()).Rotation());
		}
		const FVector2D Direction = FVector2D(ScenarioVector(Definition->GetObjectField(TEXT("variants"))->GetArrayField(Variant)));
		if (HoldStage == 0)
		{
			if (Now - BeginSimulation < Definition->GetNumberField(TEXT("warmup_s"))) { return false; }
			HoldSource = Combat->GetDefaultLightAttack();
			const EAttackDirection ExpectedDirection = Variant == TEXT("Forward") ? EAttackDirection::Forward
				: Variant == TEXT("Backward") ? EAttackDirection::Backward : Variant == TEXT("Left") ? EAttackDirection::Left : EAttackDirection::Right;
			ExpectedFollowUp = HoldSource.IsValid() ? HoldSource->DirectionalFollowUps.FindRef(ExpectedDirection) : nullptr;
			if (!HoldSource.IsValid() || !HoldSource->bCanHold || !ExpectedFollowUp.IsValid())
			{
				Check(TEXT("authored_hold_available"), false, TEXT("Selected attack must author holding and the requested directional follow-up")); return Finish();
			}
			Check(TEXT("authored_hold_available"), true, HoldSource->GetPathName() + TEXT(" -> ") + ExpectedFollowUp->GetPathName());
			Player->TargetingComponent->SetCurrentTarget(nullptr);
			CastChecked<APlayerController>(Player->GetController())->SetControlRotation(FRotator::ZeroRotator);
			if (!ApplyNamedPlacement(true)) { return Finish(); }
			Mark(TEXT("hold_requested")); HoldStage = 1; HoldStageTime = Now;
		}
		if (HoldStage == 1)
		{
			Inject(Player->LightAttackAction, FInputActionValue(true));
			if (!Combat->IsHolding())
			{
				if (Now - HoldStageTime > 4) { Check(TEXT("real_hold_started"), false, TEXT("Real montage did not enter its authored hold window")); return Finish(); }
				return false;
			}
			if (HoldStart == 0)
			{
				HoldStart = Now; InitialGeneration = Combat->GetCurrentAttackGeneration(); Mark(TEXT("hold_started"));
				Check(TEXT("real_hold_started"), Combat->GetCurrentAttack() == HoldSource.Get() && Player->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying(), TEXT("Authored montage notify activated hold through injected input"));
			}
			Inject(Player->MoveAction, FInputActionValue(Direction));
			const double CommitDuration = HoldSource->HoldEaseInDuration + 0.15;
			if (Now - HoldStart < CommitDuration) { return false; }
			bMovementSampleObserved |= !Combat->GetMovementInputSample().CameraRelativeInput.IsNearlyZero();
			bInputSuppressionHeld &= Combat->IsMovementInputSuppressed();
			if (!bBlockedInputSent) { Mark(TEXT("competing_attack_pressed")); bBlockedInputSent = true; }
			Inject(Player->HeavyAttackAction, FInputActionValue(Now - HoldStart < CommitDuration + 0.1));
			bHoldOwnershipPreserved &= Combat->GetCurrentAttackGeneration() == InitialGeneration && Combat->GetCurrentAttack() == HoldSource.Get();
			if (Now - HoldStart < CommitDuration + 0.25) { return false; }
			Check(TEXT("hold_movement_suppressed"), bMovementSampleObserved && bInputSuppressionHeld, TEXT("Movement remains sampled while committed hold suppresses movement application"));
			Check(TEXT("competing_input_preserves_hold"), bBlockedInputSent && bHoldOwnershipPreserved, TEXT("Competing ordinary attack does not replace the committed hold generation"));
			Inject(Player->LightAttackAction, FInputActionValue(false)); Inject(Player->HeavyAttackAction, FInputActionValue(false));
			Mark(TEXT("hold_released")); HoldStage = 2; HoldStageTime = Now;
			return false;
		}
		if (HoldStage == 2)
		{
			Inject(Player->MoveAction, FInputActionValue(Direction));
			if (Combat->GetCurrentAttackGeneration() == InitialGeneration)
			{
				if (Now - HoldStageTime > 3) { Check(TEXT("authored_follow_up_starts"), false, TEXT("No attack generation followed hold release")); return Finish(); }
				return false;
			}
			const bool bExpected = Combat->GetCurrentAttack() == ExpectedFollowUp.Get();
			Check(TEXT("authored_follow_up_starts"), bExpected, FString::Printf(TEXT("First attack after release: expected=%s actual=%s"), *GetNameSafe(ExpectedFollowUp.Get()), *GetNameSafe(Combat->GetCurrentAttack())));
			Mark(TEXT("follow_up_observed"));
			if (!bExpected) { return Finish(); }
			HoldStage = 3; HoldStageTime = Now; return false;
		}
		if (HoldStage == 3)
		{
			Inject(Player->MoveAction, FInputActionValue(Direction));
			if (Combat->IsAttacking() || !Combat->IsQueueEmpty()) { return false; }
			Check(TEXT("hold_cleanup"), !Combat->IsHolding() && !Combat->IsMovementInputSuppressed(), TEXT("Natural montage completion clears hold and movement ownership"));
			Mark(TEXT("ownership_released")); HoldStage = 4; HoldStageTime = Now;
			RecoveryGeneration = Combat->GetCurrentAttackGeneration();
		}
		if (HoldStage == 4)
		{
			Inject(Player->MoveAction, FInputActionValue(Direction));
			bRecoveryMovementObserved |= !Combat->IsMovementInputSuppressed() && Player->GetCharacterMovement()->GetCurrentAcceleration().SizeSquared() > 1;
			if (Now - HoldStageTime < 0.25) { return false; }
			Check(TEXT("movement_recovers"), bRecoveryMovementObserved, TEXT("Released hold restores actual CharacterMovement acceleration"));
			Mark(TEXT("recovery_repress")); Inject(Player->LightAttackAction, FInputActionValue(true));
			HoldStage = 5; HoldStageTime = Now; return false;
		}
		if (HoldStage == 5)
		{
			Inject(Player->LightAttackAction, FInputActionValue(false));
			bRecoveryAttackObserved |= Combat->GetCurrentAttackGeneration() > RecoveryGeneration;
			if (Now - HoldStageTime < 0.4) { return false; }
			Check(TEXT("repress_executes_attack"), bRecoveryAttackObserved, TEXT("Fresh post-recovery light press advances the attack generation"));
			Check(TEXT("scenario_completes"), true, TEXT("Real hold, competing input, follow-up and recovery observed"));
			return Finish();
		}
		return false;
	}

	bool ApplyNamedPlacement(bool bObserve)
	{
		if (PlacementPoses.IsEmpty()) { return true; }
		double GroundingBudget;
		if (!CombatScenarioGrounding::ReadBudget(Definition, GroundingBudget)) { Check(TEXT("placement_support"), false, TEXT("Invalid floor preparation contract")); return false; }
		auto Roles = MakeShared<FJsonObject>();
		for (const TCHAR* Role : {TEXT("Attacker"), TEXT("Victim")})
		{
			ACharacter* Character = FString(Role) == TEXT("Attacker") ? static_cast<ACharacter*>(Player.Get()) : static_cast<ACharacter*>(Victim.Get());
			const FTransform& Pose = PlacementPoses.FindChecked(Role);
			Character->GetCharacterMovement()->StopMovementImmediately();
			Character->SetActorLocationAndRotation(Base + Pose.GetLocation(), Pose.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
			auto Observation = MakeShared<FJsonObject>();
			if (bObserve && GroundingBudget > 0)
			{
				auto Support = MakeShared<FJsonObject>();
				const bool bGrounded = CombatScenarioGrounding::Prepare(Character, GroundingBudget, Support);
				Observation->SetObjectField(TEXT("support"), Support);
				if (!bGrounded) { Check(TEXT("placement_support"), false, FString(Role) + TEXT(": ") + Support->GetStringField(TEXT("status"))); return false; }
			}
			Observation->SetArrayField(TEXT("location_cm"), CombatScenarioPlacement::VectorJson(Character->GetActorLocation()));
			Observation->SetNumberField(TEXT("yaw_deg"), Character->GetActorRotation().Yaw);
			Roles->SetObjectField(Role, Observation);
		}
		if (bObserve)
		{
			PlacementObservation = MakeShared<FJsonObject>();
			PlacementObservation->SetArrayField(TEXT("origin_cm"), CombatScenarioPlacement::VectorJson(Base));
			PlacementObservation->SetObjectField(TEXT("roles"), Roles);
		}
		return true;
	}

	bool Initialize()
	{
		APlayerController* PC = World->GetFirstPlayerController();
		Player = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
		for (TActorIterator<AEnemyCharacter> It(World.Get()); It; ++It) { Enemies.Add(*It); }
		Enemies.Sort([](const TWeakObjectPtr<AEnemyCharacter>& A, const TWeakObjectPtr<AEnemyCharacter>& B) { return A->GetPathName() < B->GetPathName(); });
		if (!Player.IsValid() || Enemies.Num() < 2 || !PC->GetLocalPlayer()) { Check(TEXT("fixture_ready"), false, TEXT("Missing player, local input or enemies")); return false; }
		Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
		Victim = Enemies[0];
		UAttackData* Attack = LoadObject<UAttackData>(nullptr, *Definition->GetStringField(TEXT("attack")));
		if (!Input.IsValid() || !Attack || (Definition->GetStringField(TEXT("scenario")) == TEXT("FinisherRecovery") && !Attack->FinisherData) || Player->CombatComponent->GetDefaultLightAttack() != Attack)
		{
			Check(TEXT("fixture_ready"), false, TEXT("Input mapping or authored finisher dependency unavailable")); return false;
		}
		AssetRoots = {FName(*Definition->GetObjectField(TEXT("maps"))->GetStringField(MapKey)), Attack->GetOutermost()->GetFName(), Player->GetClass()->GetOutermost()->GetFName()};
		if (const auto* Pair = Attack->FinisherData.Get(); Pair && Pair->AttackerMontage && Pair->VictimMontage)
		{
			PlaybackLayout = MakeShared<FJsonObject>();
			PlaybackLayout->SetObjectField(TEXT("Attacker"), MontagePlaybackInspection::Snapshot(*Pair->AttackerMontage, Pair->AttackerMontageSection));
			PlaybackLayout->SetObjectField(TEXT("Victim"), MontagePlaybackInspection::Snapshot(*Pair->VictimMontage, Pair->VictimMontageSection));
		}
		for (const auto& Enemy : Enemies) { AssetRoots.AddUnique(Enemy->GetClass()->GetOutermost()->GetFName()); }
		Mode = TEXT("rendered"); FParse::Value(FCommandLine::Get(), TEXT("CombatCaptureMode="), Mode);
		FParse::Value(FCommandLine::Get(), TEXT("CombatCaptureControlOffset="), ControlOffsetCm);
		FString ContextPath;
		if (FParse::Value(FCommandLine::Get(), TEXT("CombatCaptureRunContext="), ContextPath)) { RunContext = ReadScenarioJson(ContextPath); }
		RunId = RunContext ? RunContext->GetStringField(TEXT("run_id")) : FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (RunContext && RunContext->HasField(TEXT("placement")) && !RunContext->TryGetStringField(TEXT("placement"), PlacementName))
		{
			Check(TEXT("placement_ready"), false, TEXT("Placement selector must be a string")); return false;
		}
		if (!CombatScenarioPlacement::Read(Definition, PlacementName, PlacementPoses))
		{
			Check(TEXT("placement_ready"), false, TEXT("Named placement requires complete bounded poses for both registered roles")); return false;
		}
		CameraOffset = ScenarioVector(Definition->GetArrayField(TEXT("camera_offset_cm")));
		CameraFocus = ScenarioVector(Definition->GetArrayField(TEXT("camera_focus_cm")));
		double CameraFov = Definition->GetNumberField(TEXT("camera_fov_deg"));
		if (RunContext) { RunContext->TryGetStringField(TEXT("camera_view"), CameraView); }
		if (CameraView != TEXT("default"))
		{
			const TSharedPtr<FJsonObject>* Views = nullptr;
			const TSharedPtr<FJsonObject>* View = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Offset = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Focus = nullptr;
			if (!Definition->TryGetObjectField(TEXT("camera_views"), Views)
				|| !(*Views)->TryGetObjectField(CameraView, View)
				|| !(*View)->TryGetArrayField(TEXT("offset_cm"), Offset) || Offset->Num() != 3
				|| !(*View)->TryGetArrayField(TEXT("focus_cm"), Focus) || Focus->Num() != 3
				|| !(*View)->TryGetNumberField(TEXT("fov_deg"), CameraFov) || CameraFov <= 0 || CameraFov >= 180)
			{
				Check(TEXT("camera_view_ready"), false, TEXT("Requested camera view must exist in the registered scenario with offset, focus and valid FOV")); return false;
			}
			CameraOffset = ScenarioVector(*Offset); CameraFocus = ScenarioVector(*Focus);
		}
		FParse::Value(FCommandLine::Get(), TEXT("CombatFinisherExperiment="), Experiment);
		if (Experiment != TEXT("none") && Experiment != TEXT("permit-root-motion")
			&& Experiment != TEXT("attacker-source-translation") && Experiment != TEXT("victim-source-translation") && Experiment != TEXT("victim-source-rotation") && Experiment != TEXT("paired-warp-tuning"))
		{
			Check(TEXT("experiment_ready"), false, TEXT("Unknown finisher experiment")); return false;
		}
		if (Experiment != TEXT("none"))
		{
			if (Definition->GetStringField(TEXT("scenario")) != TEXT("FinisherRecovery") || !ApplyMovementExperiment(Attack->FinisherData))
			{
				Check(TEXT("experiment_ready"), false, TEXT("Movement experiment requires clean, movement-disabling finisher montage notifies")); return false;
			}
			Check(TEXT("experiment_ready"), true, TEXT("Declared transient notify overrides applied; input ownership remains unchanged"));
		}
		OriginalRandomSeed = FMath::GetRandSeed();
		FMath::RandInit(static_cast<int32>(Definition->GetNumberField(TEXT("random_seed")))); bSeedChanged = true;
		Base = Player->GetActorLocation(); Player->SetActorRotation(FRotator::ZeroRotator);
		Player->CurrentHealth = Player->MaxHealth * 100;
		Player->CombatComponent->ClearQueue(true);
		if (Player->GetMesh()->GetAnimInstance()) { Player->GetMesh()->GetAnimInstance()->StopAllMontages(0); }
		TArray<FCombatCaptureParticipant> Participants;
		FCombatCaptureParticipant& Attacker = Participants.AddDefaulted_GetRef(); Attacker.Role = TEXT("Attacker"); Attacker.Actor = Player.Get(); Attacker.Mesh = Player->GetMesh();
		const auto& Offsets = Definition->GetArrayField(TEXT("bystander_offsets_cm"));
		for (int32 I = 0; I < Enemies.Num(); ++I)
		{
			AEnemyCharacter* Enemy = Enemies[I].Get();
			Enemy->CombatAIComponent->AbortAttack();
			Enemy->CombatAIComponent->SetCombatTarget(Player.Get());
			FEnemyAttackConfig AttackConfig; AttackConfig.AttackData = Attack; AttackConfig.MinRange = 0; AttackConfig.MaxRange = 1000;
			Enemy->CombatAIComponent->AvailableAttacks = {AttackConfig};
			Enemy->CombatAIComponent->AttackSelectionMode = EEnemyAttackSelection::Single;
			Enemy->CombatAIComponent->CirclingConfig.DirectionChangeVariance = 0;
			Enemy->CombatComponent->ClearQueue(true);
			if (Enemy->GetMesh()->GetAnimInstance()) { Enemy->GetMesh()->GetAnimInstance()->StopAllMontages(0); }
			const FVector Offset = I == 0 ? ScenarioVector(Definition->GetArrayField(TEXT("victim_offset_cm"))) : ScenarioVector(Offsets[(I - 1) % Offsets.Num()]->AsArray());
			Enemy->SetActorLocation(Base + Offset, false, nullptr, ETeleportType::TeleportPhysics);
			Enemy->SetActorRotation(FRotator(0, 180, 0));
			FCombatCaptureParticipant& P = Participants.AddDefaulted_GetRef(); P.Role = I == 0 ? TEXT("Victim") : FString::Printf(TEXT("Bystander%d"), I); P.Actor = Enemy; P.Mesh = Enemy->GetMesh();
		}
		if (!ApplyNamedPlacement(false)) { return false; }
		OriginalPlayerTick = Player->GetMesh()->VisibilityBasedAnimTickOption;
		const TSharedPtr<FJsonObject>* ContactPoints = nullptr;
		if (Definition->TryGetObjectField(TEXT("capture_points"), ContactPoints))
		{
			for (auto& Participant : Participants)
			{
				const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
				if (!(*ContactPoints)->TryGetArrayField(Participant.Role, Points)) { continue; }
				for (const auto& Point : *Points) { Participant.Points.AddUnique(FName(*Point->AsString())); }
				const auto* Weapon = Participant.Actor->FindComponentByClass<UWeaponComponent>();
				if (!Weapon || !Weapon->WeaponData || Weapon->WeaponData->bUseCharacterSocketsForTrace) { continue; }
				if (!Participant.Points.Contains(Weapon->GetEffectiveStartSocketName()) && !Participant.Points.Contains(Weapon->GetEffectiveEndSocketName())) { continue; }
				TArray<UStaticMeshComponent*> Components; Participant.Actor->GetComponents(Components);
				TArray<UStaticMeshComponent*> Matches;
				for (auto* Component : Components) { if (Component->GetStaticMesh() == Weapon->WeaponData->WeaponMesh.Get() && Component->GetAttachParent() == Participant.Mesh.Get()) { Matches.Add(Component); } }
				if (Matches.Num() != 1) { Check(TEXT("weapon_point_source"), false, TEXT("Expected exactly one attached authored weapon mesh")); return false; }
				for (const FName Point : {Weapon->GetEffectiveStartSocketName(), Weapon->GetEffectiveEndSocketName()})
				{
					if (Participant.Points.Contains(Point)) { Participant.PointSources.Add(Point, Matches[0]); }
				}
			}
		}
		OriginalVictimTick = Victim->GetMesh()->VisibilityBasedAnimTickOption;
		OriginalMeshLocation = Player->GetMesh()->GetRelativeLocation();
		Player->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Victim->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		bPoseModeChanged = true;
		OriginalView = PC->GetViewTarget();
		Camera = World->SpawnActor<ACameraActor>();
		const FVector Focus = Base + CameraFocus;
		Camera->SetActorLocation(Base + CameraOffset);
		Camera->SetActorRotation((Focus - Camera->GetActorLocation()).Rotation());
		Camera->GetCameraComponent()->SetFieldOfView(CameraFov);
		PC->SetViewTarget(Camera.Get());
		// A clip is recorded unscaled only when the viewport equals the recorder's output box; the
		// definition's 960x540 is not a recorder size. Both are 16:9, so the framing is unchanged.
		ViewportSize = VideoRequest.bEnabled ? VideoRequest.Size()
			: FIntPoint(Definition->GetNumberField(TEXT("viewport_width")), Definition->GetNumberField(TEXT("viewport_height")));
		if (UGameViewportClient* Viewport = World->GetGameViewport(); FApp::CanEverRender() && Viewport && Viewport->GetGameViewport())
		{
			OriginalViewportSize = Viewport->Viewport->GetSizeXY();
			bOriginalFixedViewport = Viewport->GetGameViewport()->HasFixedSize();
			Viewport->GetGameViewport()->SetFixedViewportSize(ViewportSize.X, ViewportSize.Y);
		}
		BeginSimulation = World->GetTimeSeconds();
		if (Mode != TEXT("disabled"))
		{
			FCombatCaptureSettings Settings; Settings.Scenario = Definition->GetStringField(TEXT("scenario")) + TEXT(".") + Variant;
			Settings.FrameHz = Mode == TEXT("rendered") ? Definition->GetNumberField(TEXT("frame_hz")) : 0;
			Settings.SampleHz = Definition->GetNumberField(TEXT("sample_hz")); Settings.MaxWallSeconds = 80; Settings.MaxFrames = 1800;
			Settings.Metadata.Add(TEXT("run_id"), RunId); Settings.Metadata.Add(TEXT("scenario_version"), FString::FromInt(Definition->GetIntegerField(TEXT("version"))));
			Settings.Metadata.Add(TEXT("pose_mode"), Definition->GetStringField(TEXT("pose_mode")));
			Settings.Metadata.Add(TEXT("map_key"), MapKey); Settings.Metadata.Add(TEXT("variant"), Variant); Settings.Metadata.Add(TEXT("capture_mode"), Mode);
			Settings.Metadata.Add(TEXT("runtime_experiment"), Experiment);
			Settings.Metadata.Add(TEXT("camera_view"), CameraView);
			Settings.Metadata.Add(TEXT("placement"), PlacementName);
			if (VideoRequest.bEnabled)
			{
				if (Mode != TEXT("rendered")) { Check(TEXT("video_capture_mode"), false, TEXT("Video is recorded only in rendered mode")); return false; }
				// The clip replaces PNG frames: synchronous readback stalls would show in the video as hitches.
				Settings.FrameHz = 0;
				Settings.bRecordVideo = true;
				Settings.VideoFramesPerSecond = VideoRequest.FramesPerSecond;
				Settings.VideoResolution = VideoRequest.Resolution;
				Settings.VideoSeconds = 30;
				Settings.Metadata.Add(TEXT("video_fps"), FString::FromInt(VideoRequest.FramesPerSecond));
				Settings.Metadata.Add(TEXT("video_resolution"), FString::FromInt(VideoRequest.Resolution));
			}
			// Bind the bounded recorder metadata to a full sidecar, rather than
			// making detailed authoring experiments depend on a string-size limit.
			FJsonSerializer::Serialize(ExperimentOverrides, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&RuntimeOverridesJson));
			const FTCHARToUTF8 OverrideBytes(*RuntimeOverridesJson);
			FSHAHash OverrideHash; FSHA1::HashBuffer(OverrideBytes.Get(), OverrideBytes.Length(), OverrideHash.Hash);
			Settings.Metadata.Add(TEXT("runtime_asset_overrides_sha1"), OverrideHash.ToString().ToLower());
			if (RunContext)
			{
				for (const FString& Field : {TEXT("source_identity"), TEXT("scenario_hash"), TEXT("evaluator_identity")})
				{
					FString Value; if (RunContext->TryGetStringField(Field, Value)) { Settings.Metadata.Add(Field, Value); }
				}
			}
			FString Error; if (!Capture.Start(World.Get(), Settings, Participants, Error)) { Check(TEXT("capture_started"), false, Error); return false; }
			Directory = Capture.GetOutputDirectory();
			if (VideoRequest.bEnabled) { Check(TEXT("video_started"), Capture.HasVideo(), Capture.GetVideoDirectory()); }
			Check(TEXT("contact_observer_bound"), Capture.ObserveContacts(Player.Get(), TEXT("Attacker"), Victim.Get(), TEXT("Victim"), Error), Error);
		}
		else { Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CombatCaptures") / (RunId + TEXT("-disabled-")) + FGuid::NewGuid().ToString(EGuidFormats::Digits)); IFileManager::Get().MakeDirectory(*Directory, true); }
		if (Mode != TEXT("disabled") && !FFileHelper::SaveStringToFile(RuntimeOverridesJson, *(Directory / TEXT("runtime-overrides.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			Check(TEXT("override_provenance_written"), false, TEXT("Cannot retain complete runtime override provenance")); return false;
		}
		Mark(TEXT("scenario_ready"));
		return true;
	}

	bool ApplyMovementExperiment(UPairedAnimationData* Pair)
	{
		if (!Pair || Pair->GetOutermost()->IsDirty()) { return false; }
		const bool bTuning = Experiment == TEXT("paired-warp-tuning");
		TArray<double> Window, Offset;
		if (bTuning)
		{
			if (!PairedWarpTuning::Read(RunContext, Window, Offset) || !Pair->VictimMontage
				|| Window[1] > Pair->VictimMontage->GetPlayLength() || Pair->AttackerWarpConfig.bWarpTranslation
				|| !Pair->VictimWarpConfig.bWarpTranslation || !Pair->VictimWarpConfig.bWarpRotation)
			{
				UE_LOG(LogTemp, Error, TEXT("Warp tuning preflight failed: attacker translation=%d victim translation=%d rotation=%d"), Pair->AttackerWarpConfig.bWarpTranslation, Pair->VictimWarpConfig.bWarpTranslation, Pair->VictimWarpConfig.bWarpRotation);
				return false;
			}
			TuningPair.Reset(Pair); OriginalVictimWarp = Pair->VictimWarpConfig; OriginalEntry = Pair->Entry;
			if (RunContext->GetObjectField(TEXT("warp_tuning"))->HasField(TEXT("entry")))
			{
				FPairedEntryConfig Entry;
				if (!PairedEntryTuning::Read(RunContext->GetObjectField(TEXT("warp_tuning"))->GetObjectField(TEXT("entry")), Entry)) { return false; }
				auto EntryRow = MakeShared<FJsonObject>();
				EntryRow->SetStringField(TEXT("role"), TEXT("Victim")); EntryRow->SetStringField(TEXT("asset"), Pair->GetPathName());
				EntryRow->SetNumberField(TEXT("notify_index"), -1); EntryRow->SetStringField(TEXT("notify_class"), TEXT("PairedAnimationData"));
				EntryRow->SetStringField(TEXT("property"), TEXT("Entry"));
				EntryRow->SetObjectField(TEXT("before"), PairedEntryTuning::Snapshot(Pair->Entry));
				EntryRow->SetObjectField(TEXT("after"), PairedEntryTuning::Snapshot(Entry));
				ExperimentOverrides.Add(MakeShared<FJsonValueObject>(EntryRow)); Pair->Entry = Entry;
				if (Entry.MovementAnimation) { AssetRoots.AddUnique(Entry.MovementAnimation->GetOutermost()->GetFName()); }
				bEntryRequested = true;
			}
			auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("role"), TEXT("Victim"));
			Row->SetStringField(TEXT("asset"), Pair->GetPathName()); Row->SetNumberField(TEXT("notify_index"), -1);
			Row->SetStringField(TEXT("notify_class"), TEXT("PairedAnimationData"));
			Row->SetStringField(TEXT("property"), TEXT("VictimWarpConfig.RelativeOffset"));
			TArray<TSharedPtr<FJsonValue>> Before, After;
			for (int32 I = 0; I < 3; ++I) { Before.Add(MakeShared<FJsonValueNumber>(OriginalVictimWarp.RelativeOffset[I])); After.Add(MakeShared<FJsonValueNumber>(Offset[I])); }
			Row->SetArrayField(TEXT("before"), Before); Row->SetArrayField(TEXT("after"), After); ExperimentOverrides.Add(MakeShared<FJsonValueObject>(Row));
			Pair->VictimWarpConfig.RelativeOffset = FVector(Offset[0], Offset[1], Offset[2]);
			bool bFacingRequested = false;
			EPairedFacingPolicy Facing = OriginalVictimWarp.FacingPolicy;
			if (!PairedWarpTuning::ReadFacing(RunContext->GetObjectField(TEXT("warp_tuning")), Facing, bFacingRequested)) { return false; }
			if (bFacingRequested)
			{
				auto FacingRow = MakeShared<FJsonObject>();
				FacingRow->SetStringField(TEXT("role"), TEXT("Victim")); FacingRow->SetStringField(TEXT("asset"), Pair->GetPathName());
				FacingRow->SetNumberField(TEXT("notify_index"), -1); FacingRow->SetStringField(TEXT("notify_class"), TEXT("PairedAnimationData"));
				FacingRow->SetStringField(TEXT("property"), TEXT("VictimWarpConfig.FacingPolicy"));
				FacingRow->SetStringField(TEXT("before"), PairedWarpTuning::FacingName(OriginalVictimWarp.FacingPolicy));
				FacingRow->SetStringField(TEXT("after"), PairedWarpTuning::FacingName(Facing));
				ExperimentOverrides.Add(MakeShared<FJsonValueObject>(FacingRow));
				Pair->VictimWarpConfig.FacingPolicy = Facing;
			}
		}
		UAnimMontage* Montages[] = {Pair->AttackerMontage, Pair->VictimMontage};
		PairedSyncTuning::FSettings SyncSettings;
		const bool bSyncTuning = bTuning && RunContext->GetObjectField(TEXT("warp_tuning"))->HasField(TEXT("primary_sync"));
		if (bSyncTuning && !PairedSyncTuning::Read(RunContext->GetObjectField(TEXT("warp_tuning"))->GetObjectField(TEXT("primary_sync")), SyncSettings)) { return false; }
		ExpectedPrimarySyncTime = SyncSettings.Time;
		for (int32 Role = 0; Role < 2; ++Role)
		{
			auto* Montage = Montages[Role];
			if (!Montage || Montage->GetOutermost()->IsDirty()) { return false; }
			int32 Changed = 0;
			int32 WarpChanged = 0;
			int32 SyncChanged = 0;
			const bool bSourceTranslation = (bTuning && Role == 0) || Experiment == (Role == 0 ? TEXT("attacker-source-translation") : TEXT("victim-source-translation"));
			const bool bWindow = bTuning && Role == 1;
			const bool bSourceRotation = Role == 1 && Experiment == TEXT("victim-source-rotation");
			for (int32 Index = 0; Index < Montage->Notifies.Num(); ++Index)
			{
				auto& Event = Montage->Notifies[Index];
				auto* Original = Event.NotifyStateClass.Get();
				UAnimNotifyState* Replacement = nullptr;
				FString Property;
				if (auto* Collision = Cast<UAnimNotifyState_PairedAnimationCollision>(Original))
				{
					if (!Collision->bDisableMovement) { return false; }
					auto* Copy = DuplicateObject<UAnimNotifyState_PairedAnimationCollision>(Collision, GetTransientPackage());
					if (!Copy) { return false; }
					Copy->bDisableMovement = false; Replacement = Copy; Property = TEXT("bDisableMovement"); ++Changed;
				}
				else if (auto* Sync = Cast<UAnimNotifyState_PairedAnimationSync>(Original); bSyncTuning && Sync && Sync->bIsPrimarySyncPoint && Sync->bApplyDamage)
				{
					const double Start = SyncSettings.Time.Get(static_cast<double>(Event.GetTriggerTime()));
					const double End = Start + Event.GetEndTriggerTime() - Event.GetTriggerTime();
					if (End > Montage->GetPlayLength() || End <= Start) { return false; }
					auto* Copy = DuplicateObject<UAnimNotifyState_PairedAnimationSync>(Sync, GetTransientPackage());
					if (!Copy) { return false; }
					Copy->bNudgeOnMinorMisalignment = SyncSettings.Nudge.Get(Sync->bNudgeOnMinorMisalignment);
					Replacement = Copy; Property = TEXT("PrimarySyncSettings"); ++SyncChanged;
				}
				else if (auto* Notify = Cast<UAnimNotifyState_MotionWarping>(Original); Notify && (bSourceTranslation || bSourceRotation || bWindow))
				{
					auto* Warp = Cast<URootMotionModifier_Warp>(Notify->RootMotionModifier);
					if (!Warp || (bSourceTranslation ? !Warp->bWarpTranslation : !Warp->bWarpRotation)) { return false; }
					auto* Copy = DuplicateObject<UAnimNotifyState_MotionWarping>(Notify, GetTransientPackage());
					auto* WarpCopy = Copy ? Cast<URootMotionModifier_Warp>(Copy->RootMotionModifier) : nullptr;
					// Instanced modifiers must also be duplicated: never mutate the asset's template.
					if (!WarpCopy || WarpCopy == Warp || WarpCopy->GetOuter() != Copy) { return false; }
					if (bSourceTranslation) { WarpCopy->bWarpTranslation = false; }
					else if (bSourceRotation) { WarpCopy->bWarpRotation = false; }
					Replacement = Copy;
					Property = bWindow ? TEXT("TriggerWindowSeconds") : bSourceTranslation ? TEXT("RootMotionModifier.bWarpTranslation") : TEXT("RootMotionModifier.bWarpRotation"); ++WarpChanged;
				}
				if (!Replacement) { continue; }
				// Preserve original event linkage/timing as well as notify object identity.
				auto& Saved = NotifyOverrides.AddDefaulted_GetRef(); Saved.Montage.Reset(Montage);
				Saved.Original.Reset(Original); Saved.Replacement.Reset(Replacement); Saved.Index = Index; Saved.Event = Event;
				Event.NotifyStateClass = Replacement;
				auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("role"), Role == 0 ? TEXT("Attacker") : TEXT("Victim"));
				Row->SetStringField(TEXT("asset"), Montage->GetPathName()); Row->SetNumberField(TEXT("notify_index"), Index);
				Row->SetStringField(TEXT("notify_class"), Original->GetClass()->GetName());
				Row->SetStringField(TEXT("original_object"), Original->GetPathName());
				Row->SetStringField(TEXT("property"), Property); Row->SetBoolField(TEXT("before"), true); Row->SetBoolField(TEXT("after"), false);
				Row->SetNumberField(TEXT("trigger_start_s"), Event.GetTriggerTime()); Row->SetNumberField(TEXT("trigger_end_s"), Event.GetEndTriggerTime());
				if (Property == TEXT("TriggerWindowSeconds"))
				{
					Row->SetArrayField(TEXT("before"), {MakeShared<FJsonValueNumber>(Event.GetTriggerTime()), MakeShared<FJsonValueNumber>(Event.GetEndTriggerTime())});
					// Preserve authored trigger offsets while setting the effective window.
					if (!PairedWarpTuning::SetEffectiveWindow(Event, Montage, Window[0], Window[1])) { return false; }
					Row->SetArrayField(TEXT("after"), {MakeShared<FJsonValueNumber>(Event.GetTriggerTime()), MakeShared<FJsonValueNumber>(Event.GetEndTriggerTime())});
				}
				else if (Property == TEXT("PrimarySyncSettings"))
				{
					const auto* Sync = CastChecked<UAnimNotifyState_PairedAnimationSync>(Original);
					const auto* Copy = CastChecked<UAnimNotifyState_PairedAnimationSync>(Replacement);
					Row->SetObjectField(TEXT("before"), PairedSyncTuning::Snapshot(Event.GetTriggerTime(), Event.GetEndTriggerTime(), Sync->bNudgeOnMinorMisalignment));
					if (SyncSettings.Time.IsSet())
					{
						const double Duration = Event.GetEndTriggerTime() - Event.GetTriggerTime();
						if (!PairedWarpTuning::SetEffectiveWindow(Event, Montage, SyncSettings.Time.GetValue(), SyncSettings.Time.GetValue() + Duration)) { return false; }
					}
					Row->SetObjectField(TEXT("after"), PairedSyncTuning::Snapshot(Event.GetTriggerTime(), Event.GetEndTriggerTime(), Copy->bNudgeOnMinorMisalignment));
				}
				ExperimentOverrides.Add(MakeShared<FJsonValueObject>(Row));
			}
			if (Changed == 0 || ((bSourceTranslation || bSourceRotation || bWindow) && WarpChanged != 1) || (bSyncTuning && SyncChanged != 1)) { return false; }
		}
		const bool bValid = !bTuning || PairedWarpTuning::Validate(RunContext, ExperimentOverrides, Pair->AttackerMontage->GetPathName(), Pair->VictimMontage->GetPathName(), Pair->GetPathName());
		if (!bValid)
		{
			FString Json; FJsonSerializer::Serialize(ExperimentOverrides, TJsonWriterFactory<>::Create(&Json));
			UE_LOG(LogTemp, Error, TEXT("Warp tuning override validation failed: %s"), *Json);
		}
		return bValid;
	}

	bool RestoreMovementExperiment()
	{
		bool bRestored = true;
		for (const auto& Saved : NotifyOverrides)
		{
			auto* Montage = Saved.Montage.Get();
			if (!Montage || !Montage->Notifies.IsValidIndex(Saved.Index)
				|| Montage->Notifies[Saved.Index].NotifyStateClass != Saved.Replacement.Get()) { bRestored = false; continue; }
			Montage->Notifies[Saved.Index] = Saved.Event;
			bRestored &= Montage->Notifies[Saved.Index].NotifyStateClass == Saved.Original.Get()
				&& Montage->Notifies[Saved.Index].GetTriggerTime() == Saved.Event.GetTriggerTime()
				&& Montage->Notifies[Saved.Index].GetEndTriggerTime() == Saved.Event.GetEndTriggerTime();
			if (const auto* Collision = Cast<UAnimNotifyState_PairedAnimationCollision>(Saved.Original.Get())) { bRestored &= Collision->bDisableMovement; }
			if (const auto* Notify = Cast<UAnimNotifyState_MotionWarping>(Saved.Original.Get()))
			{
				const auto* Warp = Cast<URootMotionModifier_Warp>(Notify->RootMotionModifier);
				bRestored &= Warp && Warp->bWarpTranslation && Warp->bWarpRotation;
			}
			bRestored &= !Montage->GetOutermost()->IsDirty();
		}
		NotifyOverrides.Reset();
		if (TuningPair.IsValid())
		{
			TuningPair->VictimWarpConfig = OriginalVictimWarp;
			TuningPair->Entry = OriginalEntry;
			bRestored &= !TuningPair->GetOutermost()->IsDirty() && TuningPair->VictimWarpConfig.RelativeOffset == OriginalVictimWarp.RelativeOffset
				&& TuningPair->VictimWarpConfig.FacingPolicy == OriginalVictimWarp.FacingPolicy;
			TuningPair.Reset();
		}
		return bRestored;
	}

	void Inject(const UInputAction* Action, const FInputActionValue& Value)
	{
		if (Input.IsValid() && Action) { Input->InjectInputForAction(Action, Value, {}, {}); }
	}

	void ObserveBystanders()
	{
		for (int32 I = 1; I < Enemies.Num(); ++I)
		{
			AEnemyCharacter* Enemy = Enemies[I].Get();
			if (!Enemy || !Enemy->GetController()) { bBystanderLogicActive = false; continue; }
			UStateTreeComponent* Tree = Enemy->GetController()->FindComponentByClass<UStateTreeComponent>();
			bBystanderLogicActive &= Tree && Tree->IsRunning();
			bBystanderAttackObserved |= Enemy->CombatAIComponent->IsAttacking();
		}
	}

	static int32 CountContactMarkers(const FString& Path)
	{
		FString Text; TArray<FString> Lines; int32 Count = 0;
		if (!FFileHelper::LoadFileToString(Text, *Path)) { return 0; }
		Text.ParseIntoArrayLines(Lines, true);
		for (const FString& Line : Lines)
		{
			TSharedPtr<FJsonObject> Row; const TSharedPtr<FJsonObject>* Payload = nullptr; const TArray<TSharedPtr<FJsonValue>>* Direction = nullptr;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Row) || !Row.IsValid() || Row->GetStringField(TEXT("marker")) != TEXT("contact")) { continue; }
			if (!Row->TryGetObjectField(TEXT("payload"), Payload) || (*Payload)->GetStringField(TEXT("stage")) != TEXT("contact")) { continue; }
			if ((*Payload)->GetStringField(TEXT("hit")).IsEmpty() || (*Payload)->GetStringField(TEXT("attacker")) != TEXT("Attacker") || (*Payload)->GetStringField(TEXT("victim")) != TEXT("Victim")) { continue; }
			if (!(*Payload)->TryGetArrayField(TEXT("direction_cm"), Direction) || Direction->Num() != 3) { continue; }
			++Count;
		}
		return Count;
	}

	void Mark(const FString& Name)
	{
		Capture.Mark(Name);
		auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("marker"), Name);
		Row->SetNumberField(TEXT("simulation_time_s"), World.IsValid() ? World->GetTimeSeconds() : 0);
		Row->SetNumberField(TEXT("wall_elapsed_s"), FPlatformTime::Seconds() - StartWall);
		Events.Add(MakeShared<FJsonValueObject>(Row));
	}

	void Check(const FString& Name, bool bPassed, const FString& Detail)
	{
		auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), Name);
		Row->SetStringField(TEXT("status"), bPassed ? TEXT("pass") : TEXT("fail")); Row->SetStringField(TEXT("reason"), Detail);
		Row->SetNumberField(TEXT("simulation_time_s"), World.IsValid() ? World->GetTimeSeconds() : 0);
		Checks.Add(MakeShared<FJsonValueObject>(Row)); Test->TestTrue(Name + TEXT(": ") + Detail, bPassed);
	}

	bool Finish()
	{
		if (bDone) { return true; }
		if (bCaptureStopped) { return PublishAfterVideo(); }
		bCaptureStopped = true;
		Mark(TEXT("scenario_finished"));
		if (bEntryRequested)
		{
			EntryOutcome = Player.IsValid() ? Player->PairedAnimationComponent->GetLastPairedEntryOutcome() : EAlignmentMotionOutcome::Invalid;
			Check(TEXT("entry_start_eligibility"), bEntryObserved || (bEntryReadyAtRequest && EntryOutcome == EAlignmentMotionOutcome::Reached),
				TEXT("Preparation was observed or the request pose already satisfied entry readiness"));
			if (bEntryObserved)
			{
				Check(TEXT("entry_defers_playback_and_damage"), bEntryNoEarlyPlayback && bEntryNoEarlyDamage,
					TEXT("No paired montage playback or damage was observed during sampled preparation"));
			}
			Check(TEXT("entry_outcome"), EntryOutcome == EAlignmentMotionOutcome::Reached || (bInterrupted && EntryOutcome == EAlignmentMotionOutcome::Cancelled),
				TEXT("Entry reached its pose or was explicitly cancelled before playback"));
		}
		if (ExpectedPrimarySyncTime.IsSet())
		{
			Check(TEXT("primary_sync_damage_timing"),
				(!FirstLethalMontageTime.IsSet() || FirstLethalMontageTime.GetValue() + 0.0001 >= ExpectedPrimarySyncTime.GetValue())
				&& (bInterrupted || FirstLethalMontageTime.IsSet()),
				TEXT("No lethal state observed before requested sync; completion observes lethal state during the pair"));
		}
		FString Error;
		Capture.GetContactCounts(WeaponContactMarkers, PairedContactMarkers);
		if (Capture.IsRecording()) { Check(TEXT("capture_export"), Capture.Stop(TEXT("scenario_finished"), Error), Error); }
		else
		{
			if (Mode != TEXT("disabled") && !Directory.IsEmpty()) { Check(TEXT("capture_export"), false, TEXT("Recorder stopped before the scenario")); }
			// The data session stopped itself; Stop still stops the clip and finalizes the link.
			FString Unused; Capture.Stop(TEXT("scenario_finished"), Unused);
		}
		CommittedContactMarkers = Capture.GetCommittedContactCount();
		VideoStopWall = FPlatformTime::Seconds();
		return PublishAfterVideo();
	}

	/** The clip finalizes asynchronously after Stop; publish the result once its files are complete. */
	bool PublishAfterVideo()
	{
		if (Capture.IsVideoFinalizing() && FPlatformTime::Seconds() - VideoStopWall < 45.0) { return false; }
		VideoFinalizeWait = FPlatformTime::Seconds() - VideoStopWall;
		if (Capture.HasVideo())
		{
			Check(TEXT("video_finalized"), !Capture.IsVideoFinalizing(),
				FString::Printf(TEXT("Recorder finished %.2f s after the session stopped"), VideoFinalizeWait));
		}
		if (Mode != TEXT("disabled") && !Directory.IsEmpty())
		{
			// Contact markers carry the reaction-review payload: stage, hit, attacker, victim, region,
			// direction_cm and the gameplay source that observed the contact.
			const int32 ContactRows = CountContactMarkers(Directory / TEXT("markers.jsonl"));
			if (Definition->GetStringField(TEXT("scenario")) == TEXT("FinisherRecovery"))
			{
				Check(TEXT("contact_markers_present"), ContactRows > 0, FString::Printf(TEXT("%d contact marker row(s) with stage, hit, subjects and direction_cm"), ContactRows));
			}
			else
			{
				Check(TEXT("contact_markers_observed"), true, FString::Printf(TEXT("%d weapon contact marker row(s); none is required by this scenario"), ContactRows));
			}
		}
		if (Experiment != TEXT("none")) { Check(TEXT("experiment_restored"), RestoreMovementExperiment(), TEXT("Original notify objects and package dirty state restored without saving")); }
		if (!Directory.IsEmpty())
		{
			auto Result = MakeShared<FJsonObject>(); Result->SetNumberField(TEXT("schema_version"), 1);
			Result->SetStringField(TEXT("run_id"), RunId); Result->SetStringField(TEXT("scenario"), Definition->GetStringField(TEXT("scenario")));
			Result->SetStringField(TEXT("map_key"), MapKey); Result->SetStringField(TEXT("variant"), Variant); Result->SetStringField(TEXT("capture_mode"), Mode);
			Result->SetNumberField(TEXT("control_offset_cm"), ControlOffsetCm); Result->SetObjectField(TEXT("definition"), Definition.ToSharedRef());
			Result->SetStringField(TEXT("runtime_experiment"), Experiment); Result->SetArrayField(TEXT("runtime_asset_overrides"), ExperimentOverrides);
			Result->SetStringField(TEXT("camera_view"), CameraView);
			Result->SetStringField(TEXT("placement"), PlacementName);
			if (PlacementObservation) { Result->SetObjectField(TEXT("placement_observation"), PlacementObservation); }
			if (PlaybackLayout) { Result->SetObjectField(TEXT("authored_playback_layout"), PlaybackLayout); }
			if (bEntryRequested)
			{
				Result->SetStringField(TEXT("entry_outcome"), UEnum::GetValueAsString(EntryOutcome));
				Result->SetBoolField(TEXT("entry_ready_at_request"), bEntryReadyAtRequest);
				Result->SetBoolField(TEXT("entry_preparation_observed"), bEntryObserved);
			}
			Result->SetBoolField(TEXT("victim_dead_at_interruption"), bVictimDeadAtInterruption);
			Result->SetNumberField(TEXT("weapon_contact_markers"), WeaponContactMarkers); Result->SetNumberField(TEXT("paired_contact_markers"), PairedContactMarkers);
			Result->SetNumberField(TEXT("committed_contact_markers"), CommittedContactMarkers);
			Result->SetArrayField(TEXT("viewport_px"), {MakeShared<FJsonValueNumber>(ViewportSize.X), MakeShared<FJsonValueNumber>(ViewportSize.Y)});
			if (VideoRequest.bEnabled)
			{
				auto VideoResult = MakeShared<FJsonObject>();
				VideoResult->SetNumberField(TEXT("requested_fps"), VideoRequest.FramesPerSecond);
				VideoResult->SetNumberField(TEXT("requested_resolution"), VideoRequest.Resolution);
				VideoResult->SetBoolField(TEXT("started"), Capture.HasVideo());
				VideoResult->SetStringField(TEXT("directory"), Capture.GetVideoDirectory());
				VideoResult->SetStringField(TEXT("link"), Capture.GetLinkPath());
				VideoResult->SetBoolField(TEXT("finalized"), Capture.HasVideo() && !Capture.IsVideoFinalizing());
				VideoResult->SetNumberField(TEXT("finalize_wait_s"), VideoFinalizeWait);
				Result->SetObjectField(TEXT("video"), VideoResult);
			}
			if (bRequested && Definition->GetStringField(TEXT("scenario")) == TEXT("FinisherRecovery"))
			{
				Result->SetNumberField(TEXT("victim_health_at_request"), VictimHealthAtRequest);
			}
			if (ExpectedPrimarySyncTime.IsSet()) { Result->SetNumberField(TEXT("requested_primary_sync_time_s"), ExpectedPrimarySyncTime.GetValue()); }
			if (FirstLethalMontageTime.IsSet()) { Result->SetNumberField(TEXT("first_lethal_observed_montage_time_s"), FirstLethalMontageTime.GetValue()); }
			if (InterruptionMontageTime.IsSet()) { Result->SetNumberField(TEXT("interruption_observed_montage_time_s"), InterruptionMontageTime.GetValue()); }
			Result->SetArrayField(TEXT("checks"), Checks); Result->SetArrayField(TEXT("events"), Events);
			// Export project package dependencies for the external runner to hash. No
			// packages are loaded or saved by this identity walk; engine identity is separate.
			IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
			TSet<FName> Seen; TArray<FName> Pending = AssetRoots;
			for (int32 Index = 0; Index < Pending.Num(); ++Index)
			{
				const FName Package = Pending[Index];
				if (Seen.Contains(Package) || !Package.ToString().StartsWith(TEXT("/Game/"))) { continue; }
				Seen.Add(Package); TArray<FName> Dependencies;
				Registry.GetDependencies(Package, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
				Pending.Append(Dependencies);
			}
			TArray<FName> Packages = Seen.Array(); Packages.Sort(FNameLexicalLess());
			TArray<TSharedPtr<FJsonValue>> PackageValues;
			for (const FName Package : Packages) { PackageValues.Add(MakeShared<FJsonValueString>(Package.ToString())); }
			Result->SetArrayField(TEXT("project_package_dependencies"), PackageValues);
			Result->SetNumberField(TEXT("wall_duration_s"), FPlatformTime::Seconds() - StartWall);
			Result->SetNumberField(TEXT("simulation_duration_s"), World.IsValid() ? World->GetTimeSeconds() - BeginSimulation : 0);
			FString Text; FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Text));
			Test->TestTrue(TEXT("Scenario result written"), FFileHelper::SaveStringToFile(Text, *(Directory / TEXT("scenario.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
			UE_LOG(LogTemp, Display, TEXT("COMBAT_SCENARIO_OUTPUT=%s"), *Directory);
		}
		Cleanup(); bDone = true; return true;
	}

	void Cleanup()
	{
		RestoreMovementExperiment();
		{ FString Unused; Capture.Stop(TEXT("fixture_destroyed"), Unused); } // Unconditional: also finalizes video and its link.
		if (Player.IsValid())
		{
			Inject(Player->LightAttackAction, FInputActionValue(false)); Inject(Player->HeavyAttackAction, FInputActionValue(false)); Inject(Player->MoveAction, FInputActionValue(FVector2D::ZeroVector));
			if (bPoseModeChanged) { Player->GetMesh()->VisibilityBasedAnimTickOption = OriginalPlayerTick; Player->GetMesh()->SetRelativeLocation(OriginalMeshLocation); }
			if (APlayerController* PC = Cast<APlayerController>(Player->GetController()); PC && OriginalView.IsValid()) { PC->SetViewTarget(OriginalView.Get()); }
		}
		if (Victim.IsValid() && bPoseModeChanged) { Victim->GetMesh()->VisibilityBasedAnimTickOption = OriginalVictimTick; }
		bPoseModeChanged = false;
		if (Camera.IsValid()) { Camera->Destroy(); Camera.Reset(); }
		if (World.IsValid() && OriginalViewportSize.X > 0)
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport(); Viewport && Viewport->GetGameViewport())
			{
				Viewport->GetGameViewport()->SetFixedViewportSize(bOriginalFixedViewport ? OriginalViewportSize.X : 0, bOriginalFixedViewport ? OriginalViewportSize.Y : 0);
			}
			OriginalViewportSize = FIntPoint::ZeroValue;
		}
		if (bSeedChanged) { FMath::RandInit(OriginalRandomSeed); bSeedChanged = false; }
	}

	FAutomationTestBase* Test;
	struct FNotifyOverride
	{
		TStrongObjectPtr<UAnimMontage> Montage;
		TStrongObjectPtr<UAnimNotifyState> Original;
		TStrongObjectPtr<UAnimNotifyState> Replacement;
		int32 Index = INDEX_NONE;
		FAnimNotifyEvent Event;
	};
	TArray<FNotifyOverride> NotifyOverrides;
	TStrongObjectPtr<UPairedAnimationData> TuningPair;
	FPairedWarpConfig OriginalVictimWarp;
	FPairedEntryConfig OriginalEntry;
	TArray<TSharedPtr<FJsonValue>> ExperimentOverrides;
	FString Experiment = TEXT("none");
	TSharedPtr<FJsonObject> Definition, RunContext;
	FString MapKey, Variant, Mode, Directory, RunId;
	FString CameraView = TEXT("default");
	FString PlacementName = TEXT("default");
	FString RuntimeOverridesJson;
	TMap<FString, FTransform> PlacementPoses;
	TSharedPtr<FJsonObject> PlacementObservation;
	TSharedPtr<FJsonObject> PlaybackLayout;
	FVector CameraOffset = FVector::ZeroVector, CameraFocus = FVector::ZeroVector;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<APlayerCharacter> Player;
	TWeakObjectPtr<AEnemyCharacter> Victim;
	TArray<TWeakObjectPtr<AEnemyCharacter>> Enemies;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> Input;
	TWeakObjectPtr<ACameraActor> Camera;
	TWeakObjectPtr<AActor> OriginalView;
	FCombatCaptureSession Capture;
	TArray<TSharedPtr<FJsonValue>> Events, Checks;
	TArray<FName> AssetRoots;
	FVector Base, OriginalMeshLocation;
	FIntPoint OriginalViewportSize = FIntPoint::ZeroValue;
	FIntPoint ViewportSize = FIntPoint::ZeroValue;
	FScenarioVideoRequest VideoRequest = FScenarioVideoRequest::FromCommandLine();
	double VideoStopWall = 0, VideoFinalizeWait = 0;
	bool bCaptureStopped = false;
	int32 CommittedContactMarkers = 0;
	EVisibilityBasedAnimTickOption OriginalPlayerTick, OriginalVictimTick;
	double StartWall = 0, BeginSimulation = 0, PairedStart = 0, RecoveryStart = 0;
	TOptional<double> ExpectedPrimarySyncTime, FirstLethalMontageTime, InterruptionMontageTime;
	float VictimHealthAtRequest = 0;
	float ControlOffsetCm = 0;
	int32 InitialGeneration = 0, RecoveryGeneration = 0, OriginalRandomSeed = 0;
	int32 WeaponContactMarkers = 0, PairedContactMarkers = 0;
	bool bDone = false, bRequested = false, bStarted = false, bReleased = false, bInterrupted = false, bRepressed = false;
	bool bBlockedInputSent = false, bMovementSampleObserved = false, bInputSuppressionHeld = true;
	bool bRecoveryAttackObserved = false, bRecoveryMovementObserved = false, bBystanderLogicActive = true, bBystanderAttackObserved = false;
	bool bPoseModeChanged = false, bSeedChanged = false, bControlApplied = false;
	bool bVictimDeadAtInterruption = false;
	bool bPairedCollisionObserved = false, bPairedCollisionHeld = true;
	bool bEntryRequested = false, bEntryObserved = false, bEntryResultObserved = false;
	bool bEntryReadyAtRequest = false;
	bool bEntryNoEarlyPlayback = true, bEntryNoEarlyDamage = true;
	EAlignmentMotionOutcome EntryOutcome = EAlignmentMotionOutcome::Invalid;
	bool bOriginalFixedViewport = false;
	int32 HoldStage = 0;
	double HoldStageTime = 0, HoldStart = 0;
	bool bHoldOwnershipPreserved = true;
	TWeakObjectPtr<UAttackData> HoldSource, ExpectedFollowUp;
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCombatRecoveryScenarioTest,
	"KatanaCombat.Capture.Scenarios.FinisherRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FCombatRecoveryScenarioTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const FString& Map : {TEXT("ThirdPerson"), TEXT("DefenseMatrix")})
	{
		for (const FString& Variant : {TEXT("Completed"), TEXT("Interrupted")}) { Names.Add(Map + TEXT(".") + Variant); Commands.Add(Map + TEXT(".") + Variant); }
	}
}

bool FCombatRecoveryScenarioTest::RunTest(const FString& Parameters)
{
	const auto Definition = ReadScenarioJson(FPaths::ProjectDir() / TEXT("Tools/CombatCapture/scenarios/finisher-recovery.json"));
	FString Map, Variant;
	if (!Definition || !Parameters.Split(TEXT("."), &Map, &Variant) || !Definition->GetObjectField(TEXT("maps"))->HasField(Map) || !Definition->GetObjectField(TEXT("variants"))->HasField(Variant))
	{
		AddError(TEXT("Invalid tracked recovery scenario")); return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Definition->GetObjectField(TEXT("maps"))->GetStringField(Map)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
	const TSharedPtr<FPlaySettingsSnapshot> PlaySettings = QueueScenarioPIE();
	ADD_LATENT_AUTOMATION_COMMAND(FCombatRecoveryScenario(this, Definition, Map, Variant));
	QueueScenarioEnd(PlaySettings);
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FHoldReleaseRecoveryScenarioTest,
	"KatanaCombat.Capture.Scenarios.HoldReleaseRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FHoldReleaseRecoveryScenarioTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const FString& Map : {TEXT("ThirdPerson"), TEXT("DefenseMatrix")})
	{
		for (const FString& Variant : {TEXT("Forward"), TEXT("Backward"), TEXT("Left"), TEXT("Right")}) { Names.Add(Map + TEXT(".") + Variant); Commands.Add(Map + TEXT(".") + Variant); }
	}
}

bool FHoldReleaseRecoveryScenarioTest::RunTest(const FString& Parameters)
{
	const auto Definition = ReadScenarioJson(FPaths::ProjectDir() / TEXT("Tools/CombatCapture/scenarios/hold-release-recovery.json"));
	FString Map, Variant;
	if (!Definition || !Parameters.Split(TEXT("."), &Map, &Variant) || !Definition->GetObjectField(TEXT("maps"))->HasField(Map) || !Definition->GetObjectField(TEXT("variants"))->HasField(Variant))
	{
		AddError(TEXT("Invalid tracked hold/recovery scenario")); return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Definition->GetObjectField(TEXT("maps"))->GetStringField(Map)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
	const TSharedPtr<FPlaySettingsSnapshot> PlaySettings = QueueScenarioPIE();
	ADD_LATENT_AUTOMATION_COMMAND(FCombatRecoveryScenario(this, Definition, Map, Variant));
	QueueScenarioEnd(PlaySettings);
	return true;
}
