// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/MontageUtilityLibrary.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ActionQueueTypes.h"
#include "Animation/AnimNotifyState_ActionWindow_Base.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Data/AttackData.h"
#include "Core/CombatComponent.h"  // For LogCombat declaration
#include "Animation/AnimInstance.h"

// ============================================================================
// MONTAGE TIME QUERIES
// ============================================================================

float UMontageUtilityLibrary::GetCurrentMontageTime(ACharacter* Character)
{
	if (!Character)
	{
		return -1.0f;
	}

	UAnimInstance* AnimInstance = GetAnimInstance(Character);
	if (!AnimInstance)
	{
		return -1.0f;
	}

	UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();
	if (!CurrentMontage)
	{
		return -1.0f;
	}

	return AnimInstance->Montage_GetPosition(CurrentMontage);
}

UAnimMontage* UMontageUtilityLibrary::GetCurrentMontage(ACharacter* Character)
{
	if (!Character)
	{
		return nullptr;
	}

	UAnimInstance* AnimInstance = GetAnimInstance(Character);
	if (!AnimInstance)
	{
		return nullptr;
	}

	return AnimInstance->GetCurrentActiveMontage();
}

UAnimInstance* UMontageUtilityLibrary::GetAnimInstance(ACharacter* Character)
{
	if (!Character)
	{
		return nullptr;
	}

	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (!Mesh)
	{
		return nullptr;
	}

	return Mesh->GetAnimInstance();
}

// ============================================================================
// MONTAGE PLAYBACK CONTROL
// ============================================================================

bool UMontageUtilityLibrary::SetMontagePlayRate(ACharacter* Character, float PlayRate)
{
	if (!Character)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetAnimInstance(Character);
	if (!AnimInstance)
	{
		return false;
	}

	UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();
	if (!CurrentMontage)
	{
		return false;
	}

	AnimInstance->Montage_SetPlayRate(CurrentMontage, PlayRate);
	return true;
}

float UMontageUtilityLibrary::GetMontagePlayRate(ACharacter* Character)
{
	if (!Character)
	{
		return 1.0f;
	}

	UAnimInstance* AnimInstance = GetAnimInstance(Character);
	if (!AnimInstance)
	{
		return 1.0f;
	}

	UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();
	if (!CurrentMontage)
	{
		return 1.0f;
	}

	return AnimInstance->Montage_GetPlayRate(CurrentMontage);
}

// ============================================================================
// CHECKPOINT DISCOVERY
// ============================================================================

int32 UMontageUtilityLibrary::DiscoverCheckpoints(UAnimMontage* Montage, TArray<FTimerCheckpoint>& OutCheckpoints)
{
	OutCheckpoints.Empty();

	if (!Montage)
	{
		return 0;
	}

	const float MontageDuration = Montage->GetPlayLength();
	if (MontageDuration <= 0.0f)
	{
		return 0;
	}

	// ========================================================================
	// PHASE 1: Find phase transition notifies (instant events, NOT states)
	// ========================================================================
	// Phase transitions define contiguous phase boundaries:
	// - Windup: 0.0s → Active transition
	// - Active: Active transition → Recovery transition
	// - Recovery: Recovery transition → Montage end

	float ActiveTransitionTime = -1.0f;   // When Active phase starts
	float RecoveryTransitionTime = -1.0f; // When Recovery phase starts (Active ends)

	UE_LOG(LogCombat, Verbose, TEXT("[CHECKPOINT] Scanning montage '%s' (duration: %.3fs)..."),
		*Montage->GetName(), MontageDuration);

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		// Check for instant notifies (AnimNotify, not AnimNotifyState)
		if (NotifyEvent.Notify)
		{
			// Check if this is an AttackPhaseTransition notify
			if (const UAnimNotify_AttackPhaseTransition* PhaseNotify =
				Cast<UAnimNotify_AttackPhaseTransition>(NotifyEvent.Notify))
			{
				const float TriggerTime = NotifyEvent.GetTriggerTime();

				if (PhaseNotify->TransitionToPhase == EAttackPhase::Active)
				{
					ActiveTransitionTime = TriggerTime;
					UE_LOG(LogCombat, Verbose, TEXT("[CHECKPOINT] Found Active transition at %.3fs"), TriggerTime);
				}
				else if (PhaseNotify->TransitionToPhase == EAttackPhase::Recovery)
				{
					RecoveryTransitionTime = TriggerTime;
					UE_LOG(LogCombat, Verbose, TEXT("[CHECKPOINT] Found Recovery transition at %.3fs"), TriggerTime);
				}
			}
		}
	}

	// ========================================================================
	// PHASE 2: Infer windows from phase transitions
	// ========================================================================
	// Combo System (Implicit Windows):
	// - Input during Windup/Active → Snap execution at Active END (Recovery start)
	// - Input during Recovery → Immediate interrupt (responsive)
	// - No input by Recovery END → Combo resets
	//
	// This means:
	// - Combo window spans entire attack (montage start to end)
	// - Snap checkpoint at Recovery transition (Active→Recovery boundary)
	// - Parry window during Active phase (for defenders to check attacker's state)

	// Only create implicit windows if we found at least one phase transition
	if (ActiveTransitionTime >= 0.0f || RecoveryTransitionTime >= 0.0f)
	{
		// Default fallbacks if only partial transitions found
		if (ActiveTransitionTime < 0.0f)
		{
			ActiveTransitionTime = 0.0f; // Assume immediate Active
			UE_LOG(LogCombat, Verbose, TEXT("[CHECKPOINT] No Active transition found, assuming 0.0s"));
		}
		if (RecoveryTransitionTime < 0.0f)
		{
			RecoveryTransitionTime = MontageDuration; // Assume no Recovery phase
			UE_LOG(LogCombat, Verbose, TEXT("[CHECKPOINT] No Recovery transition found, assuming end of montage"));
		}

		// 1. COMBO WINDOW: Entire attack duration (input is always buffered)
		// The execution timing differs based on WHEN input is received:
		// - During Windup/Active: Queued for snap execution at Active end
		// - During Recovery: Immediate interrupt
		{
			FTimerCheckpoint ComboCheckpoint;
			ComboCheckpoint.WindowType = EActionWindowType::Combo;
			ComboCheckpoint.MontageTime = 0.0f; // Starts at montage begin
			ComboCheckpoint.Duration = MontageDuration; // Spans entire montage
			ComboCheckpoint.bActive = true;
			OutCheckpoints.Add(ComboCheckpoint);
			UE_LOG(LogCombat, Log, TEXT("[CHECKPOINT] + Combo window: 0.0s - %.3fs (full montage)"), MontageDuration);
		}

		// 2. SNAP CHECKPOINT (Recovery start): When queued actions execute
		// This is the key timing point - actions queued during Windup/Active
		// execute at this moment (end of Active phase)
		{
			FTimerCheckpoint SnapCheckpoint;
			SnapCheckpoint.WindowType = EActionWindowType::Recovery;
			SnapCheckpoint.MontageTime = RecoveryTransitionTime;
			SnapCheckpoint.Duration = MontageDuration - RecoveryTransitionTime;
			SnapCheckpoint.bActive = true;
			OutCheckpoints.Add(SnapCheckpoint);
			UE_LOG(LogCombat, Log, TEXT("[CHECKPOINT] + Recovery (snap point): %.3fs - %.3fs"), RecoveryTransitionTime, MontageDuration);
		}

		// 3. PARRY WINDOW: During Active phase
		// Defenders check if attacker is in parry window to trigger parry
		// Parry window = Active phase duration
		if (ActiveTransitionTime < RecoveryTransitionTime)
		{
			FTimerCheckpoint ParryCheckpoint;
			ParryCheckpoint.WindowType = EActionWindowType::Parry;
			ParryCheckpoint.MontageTime = ActiveTransitionTime;
			ParryCheckpoint.Duration = RecoveryTransitionTime - ActiveTransitionTime;
			ParryCheckpoint.bActive = true;
			OutCheckpoints.Add(ParryCheckpoint);
			UE_LOG(LogCombat, Log, TEXT("[CHECKPOINT] + Parry window: %.3fs - %.3fs (Active phase)"),
				ActiveTransitionTime, RecoveryTransitionTime);
		}
	}
	else
	{
		UE_LOG(LogCombat, Warning, TEXT("[CHECKPOINT] No phase transitions found in montage '%s' - windows cannot be inferred"),
			*Montage->GetName());
	}

	// ========================================================================
	// PHASE 3: Scan for explicit AnimNotifyState windows (ActionWindow_Base)
	// ========================================================================
	// Generic scanning for any ActionWindow_Base subclass. This allows:
	// - HoldWindow (explicit duration tracking for hold mechanics)
	// - ParryWindow (explicit override for specialized parry timing)
	// - ComboWindow (explicit override for specialized combo timing)
	// - CancelWindow (future: animation cancellation timing)
	//
	// NOTE: Explicit AnimNotifyStates found here are ADDITIVE to inferred windows.
	// They can provide specialized timing for specific attacks while defaults
	// are inferred from phase transitions.

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (NotifyEvent.NotifyStateClass)
		{
			UClass* NotifyClass = static_cast<UClass*>(NotifyEvent.NotifyStateClass);

			// Check if this is an ActionWindow subclass
			if (NotifyClass && NotifyClass->IsChildOf(UAnimNotifyState_ActionWindow_Base::StaticClass()))
			{
				// Get the CDO to call GetWindowType()
				if (const UAnimNotifyState_ActionWindow_Base* WindowCDO =
					NotifyClass->GetDefaultObject<UAnimNotifyState_ActionWindow_Base>())
				{
					FTimerCheckpoint Checkpoint;
					Checkpoint.WindowType = WindowCDO->GetWindowType();
					Checkpoint.MontageTime = NotifyEvent.GetTriggerTime();
					Checkpoint.Duration = NotifyEvent.GetDuration();
					Checkpoint.bActive = true;
					OutCheckpoints.Add(Checkpoint);

					// Log window type name
					const TCHAR* WindowTypeName = TEXT("Unknown");
					switch (Checkpoint.WindowType)
					{
						case EActionWindowType::Combo:    WindowTypeName = TEXT("Combo"); break;
						case EActionWindowType::Parry:    WindowTypeName = TEXT("Parry"); break;
						case EActionWindowType::Hold:     WindowTypeName = TEXT("Hold"); break;
						case EActionWindowType::Cancel:   WindowTypeName = TEXT("Cancel"); break;
						case EActionWindowType::Recovery: WindowTypeName = TEXT("Recovery"); break;
					}
					UE_LOG(LogCombat, Log, TEXT("[CHECKPOINT] + %s window (explicit): %.3fs - %.3fs"),
						WindowTypeName, Checkpoint.MontageTime, Checkpoint.MontageTime + Checkpoint.Duration);
				}
			}
		}
	}

	// Sort checkpoints by montage time
	OutCheckpoints.Sort([](const FTimerCheckpoint& A, const FTimerCheckpoint& B)
	{
		return A.MontageTime < B.MontageTime;
	});

	UE_LOG(LogCombat, Log, TEXT("[CHECKPOINT] Discovered %d checkpoints in '%s'"),
		OutCheckpoints.Num(), *Montage->GetName());

	return OutCheckpoints.Num();
}

float UMontageUtilityLibrary::GetMontageDuration(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return 0.0f;
	}

	return Montage->GetPlayLength();
}

bool UMontageUtilityLibrary::IsTimeInWindow(float CurrentTime, float StartTime, float Duration)
{
	return CurrentTime >= StartTime && CurrentTime <= (StartTime + Duration);
}

// ============================================================================
// PROCEDURAL EASING
// ============================================================================

float UMontageUtilityLibrary::EvaluateEasing(float Alpha, EEasingType EasingType)
{
	// Clamp alpha to [0, 1]
	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

	switch (EasingType)
	{
		case EEasingType::Linear:
			return Alpha;

		case EEasingType::EaseInQuad:
			return Alpha * Alpha;

		case EEasingType::EaseOutQuad:
			return 1.0f - (1.0f - Alpha) * (1.0f - Alpha);

		case EEasingType::EaseInOutQuad:
			return Alpha < 0.5f
				? 2.0f * Alpha * Alpha
				: 1.0f - FMath::Pow(-2.0f * Alpha + 2.0f, 2.0f) / 2.0f;

		case EEasingType::EaseInCubic:
			return Alpha * Alpha * Alpha;

		case EEasingType::EaseOutCubic:
			return 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);

		case EEasingType::EaseInOutCubic:
			return Alpha < 0.5f
				? 4.0f * Alpha * Alpha * Alpha
				: 1.0f - FMath::Pow(-2.0f * Alpha + 2.0f, 3.0f) / 2.0f;

		case EEasingType::EaseInExpo:
			return Alpha == 0.0f ? 0.0f : FMath::Pow(2.0f, 10.0f * Alpha - 10.0f);

		case EEasingType::EaseOutExpo:
			return Alpha == 1.0f ? 1.0f : 1.0f - FMath::Pow(2.0f, -10.0f * Alpha);

		case EEasingType::EaseInOutSine:
			return -(FMath::Cos(PI * Alpha) - 1.0f) / 2.0f;

		default:
			return Alpha;
	}
}

float UMontageUtilityLibrary::EaseLerp(float Start, float End, float Alpha, EEasingType EasingType)
{
	float EasedAlpha = EvaluateEasing(Alpha, EasingType);
	return FMath::Lerp(Start, End, EasedAlpha);
}

float UMontageUtilityLibrary::CalculateTransitionPlayRate(
	float StartRate,
	float TargetRate,
	float ElapsedTime,
	float Duration,
	EEasingType EasingType,
	UCurveFloat* Curve)
{
	if (Duration <= 0.0f)
	{
		return TargetRate;
	}

	float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);

	// Use custom curve if provided
	if (Curve)
	{
		Alpha = Curve->GetFloatValue(Alpha);
	}
	else
	{
		// Use procedural easing
		Alpha = EvaluateEasing(Alpha, EasingType);
	}

	return FMath::Lerp(StartRate, TargetRate, Alpha);
}

// ============================================================================
// ADVANCED HOLD MECHANICS
// ============================================================================

float UMontageUtilityLibrary::CalculateChargeLevel(
	float HoldDuration,
	float MaxChargeTime,
	EEasingType EasingType,
	UCurveFloat* ChargeCurve)
{
	if (MaxChargeTime <= 0.0f)
	{
		return 1.0f; // Instant full charge if no max time
	}

	float Alpha = FMath::Clamp(HoldDuration / MaxChargeTime, 0.0f, 1.0f);

	// Use custom curve if provided
	if (ChargeCurve)
	{
		return ChargeCurve->GetFloatValue(Alpha);
	}

	// Use procedural easing
	return EvaluateEasing(Alpha, EasingType);
}

float UMontageUtilityLibrary::GetMultiStageHoldPlayRate(
	float HoldDuration,
	const TArray<float>& StageThresholds,
	const TArray<float>& StagePlayRates)
{
	// Validate arrays
	if (StageThresholds.Num() != StagePlayRates.Num() || StageThresholds.Num() == 0)
	{
		return 1.0f; // Default playrate if invalid data
	}

	// Find current stage
	int32 CurrentStage = -1;
	for (int32 i = StageThresholds.Num() - 1; i >= 0; --i)
	{
		if (HoldDuration >= StageThresholds[i])
		{
			CurrentStage = i;
			break;
		}
	}

	// Return playrate for current stage
	if (CurrentStage >= 0)
	{
		return StagePlayRates[CurrentStage];
	}

	// Before first stage - return normal playrate
	return 1.0f;
}

int32 UMontageUtilityLibrary::GetHoldStageIndex(float HoldDuration, const TArray<float>& StageThresholds)
{
	if (StageThresholds.Num() == 0)
	{
		return -1;
	}

	// Find current stage (highest threshold that we've passed)
	for (int32 i = StageThresholds.Num() - 1; i >= 0; --i)
	{
		if (HoldDuration >= StageThresholds[i])
		{
			return i;
		}
	}

	return -1; // Before first stage
}

// ============================================================================
// MONTAGE SECTION UTILITIES
// ============================================================================

TArray<FName> UMontageUtilityLibrary::GetMontageSections(UAnimMontage* Montage)
{
	TArray<FName> SectionNames;

	if (!Montage)
	{
		return SectionNames;
	}

	// Get composite sections from montage
	const TArray<FCompositeSection>& CompositeSections = Montage->CompositeSections;
	for (const FCompositeSection& Section : CompositeSections)
	{
		SectionNames.Add(Section.SectionName);
	}

	return SectionNames;
}

float UMontageUtilityLibrary::GetSectionStartTime(UAnimMontage* Montage, FName SectionName)
{
	if (!Montage)
	{
		return -1.0f;
	}

	int32 SectionIndex = Montage->GetSectionIndex(SectionName);
	if (SectionIndex == INDEX_NONE)
	{
		return -1.0f;
	}

	return Montage->GetAnimCompositeSection(SectionIndex).GetTime();
}

float UMontageUtilityLibrary::GetSectionDuration(UAnimMontage* Montage, FName SectionName)
{
	if (!Montage)
	{
		return -1.0f;
	}

	int32 SectionIndex = Montage->GetSectionIndex(SectionName);
	if (SectionIndex == INDEX_NONE)
	{
		return -1.0f;
	}

	// Get section start time
	float SectionStartTime = Montage->GetAnimCompositeSection(SectionIndex).GetTime();

	// Find next section or use montage end
	float SectionEndTime = Montage->GetPlayLength();
	if (SectionIndex + 1 < Montage->CompositeSections.Num())
	{
		SectionEndTime = Montage->GetAnimCompositeSection(SectionIndex + 1).GetTime();
	}

	return SectionEndTime - SectionStartTime;
}

FName UMontageUtilityLibrary::GetCurrentSectionName(ACharacter* Character)
{
	if (!Character)
	{
		return NAME_None;
	}

	UAnimInstance* AnimInstance = GetAnimInstance(Character);
	if (!AnimInstance)
	{
		return NAME_None;
	}

	UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();
	if (!CurrentMontage)
	{
		return NAME_None;
	}

	// Get current section from montage instance
	return AnimInstance->Montage_GetCurrentSection(CurrentMontage);
}

bool UMontageUtilityLibrary::JumpToSectionWithBlend(ACharacter* Character, FName SectionName, float BlendTime)
{
	if (!Character)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetAnimInstance(Character);
	if (!AnimInstance)
	{
		return false;
	}

	UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();
	if (!CurrentMontage)
	{
		return false;
	}

	// BLEND IMPLEMENTATION:
	// UE's Montage_JumpToSection doesn't support blend time (instant jump)
	// We implement blending by re-playing the montage at the target section with blend settings

	// CRITICAL FIX: Always use instant jump to prevent montage interruption
	// The original Stop+Re-play pattern causes race conditions where OnMontageEnded
	// fires before the new play starts, clearing combat state prematurely.
	// This is especially critical for looping sections (heavy charge loops).
	AnimInstance->Montage_JumpToSection(SectionName, CurrentMontage);

	// NOTE: BlendTime parameter is currently ignored
	// Future enhancement: Could implement visual blending via playrate ramping
	// or by creating transition sections in the montage itself

	return true;
}

// ============================================================================
// WINDOW STATE QUERIES
// ============================================================================

TArray<EActionWindowType> UMontageUtilityLibrary::GetActiveWindows(
	ACharacter* Character,
	const TArray<FTimerCheckpoint>& Checkpoints)
{
	TArray<EActionWindowType> ActiveWindows;

	float CurrentTime = GetCurrentMontageTime(Character);
	if (CurrentTime < 0.0f)
	{
		return ActiveWindows; // No active montage
	}

	// Check each checkpoint
	for (const FTimerCheckpoint& Checkpoint : Checkpoints)
	{
		if (Checkpoint.bActive && IsTimeInWindow(CurrentTime, Checkpoint.MontageTime, Checkpoint.Duration))
		{
			ActiveWindows.AddUnique(Checkpoint.WindowType);
		}
	}

	return ActiveWindows;
}

bool UMontageUtilityLibrary::IsWindowActive(
	ACharacter* Character,
	const TArray<FTimerCheckpoint>& Checkpoints,
	EActionWindowType WindowType)
{
	float CurrentTime = GetCurrentMontageTime(Character);
	if (CurrentTime < 0.0f)
	{
		return false; // No active montage
	}

	// Check if any checkpoint of this type is active
	for (const FTimerCheckpoint& Checkpoint : Checkpoints)
	{
		if (Checkpoint.WindowType == WindowType &&
			Checkpoint.bActive &&
			IsTimeInWindow(CurrentTime, Checkpoint.MontageTime, Checkpoint.Duration))
		{
			return true;
		}
	}

	return false;
}

float UMontageUtilityLibrary::GetWindowTimeRemaining(ACharacter* Character, const FTimerCheckpoint& Checkpoint)
{
	float CurrentTime = GetCurrentMontageTime(Character);
	if (CurrentTime < 0.0f)
	{
		return 0.0f; // No active montage
	}

	// Check if we're in the window
	if (!IsTimeInWindow(CurrentTime, Checkpoint.MontageTime, Checkpoint.Duration))
	{
		return 0.0f; // Not in window
	}

	// Calculate remaining time
	float WindowEnd = Checkpoint.MontageTime + Checkpoint.Duration;
	return FMath::Max(0.0f, WindowEnd - CurrentTime);
}

bool UMontageUtilityLibrary::GetNextCheckpoint(
	ACharacter* Character,
	const TArray<FTimerCheckpoint>& Checkpoints,
	EActionWindowType WindowType,
	FTimerCheckpoint& OutCheckpoint)
{
	float CurrentTime = GetCurrentMontageTime(Character);
	if (CurrentTime < 0.0f)
	{
		return false; // No active montage
	}

	// Find next checkpoint of specified type
	float ClosestTime = MAX_FLT;
	bool bFound = false;

	for (const FTimerCheckpoint& Checkpoint : Checkpoints)
	{
		if (Checkpoint.WindowType == WindowType &&
			Checkpoint.MontageTime > CurrentTime &&
			Checkpoint.MontageTime < ClosestTime)
		{
			OutCheckpoint = Checkpoint;
			ClosestTime = Checkpoint.MontageTime;
			bFound = true;
		}
	}

	return bFound;
}

// ============================================================================
// MONTAGE BLENDING
// ============================================================================

bool UMontageUtilityLibrary::CrossfadeMontage(
	ACharacter* Character,
	UAnimMontage* TargetMontage,
	float BlendTime,
	float StartPosition,
	float PlayRate)
{
	if (!Character || !TargetMontage)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetAnimInstance(Character);
	if (!AnimInstance)
	{
		return false;
	}

	// Stop current montage with blend out
	UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();
	if (CurrentMontage)
	{
		AnimInstance->Montage_Stop(BlendTime, CurrentMontage);
	}

	// Play new montage with blend in
	AnimInstance->Montage_Play(TargetMontage, PlayRate, EMontagePlayReturnType::MontageLength, StartPosition, true);

	return true;
}

bool UMontageUtilityLibrary::BlendOutMontage(ACharacter* Character, float BlendOutTime)
{
	if (!Character)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetAnimInstance(Character);
	if (!AnimInstance)
	{
		return false;
	}

	UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();
	if (!CurrentMontage)
	{
		return false;
	}

	AnimInstance->Montage_Stop(BlendOutTime, CurrentMontage);
	return true;
}

// ============================================================================
// DEBUG & VISUALIZATION
// ============================================================================

void UMontageUtilityLibrary::DrawCheckpointTimeline(
	UObject* WorldContextObject,
	ACharacter* Character,
	const TArray<FTimerCheckpoint>& Checkpoints,
	float DrawDuration,
	float YOffset)
{
	if (!WorldContextObject || !Character)
	{
		return;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return;
	}

	float CurrentTime = GetCurrentMontageTime(Character);
	UAnimMontage* CurrentMontage = GetCurrentMontage(Character);

	if (CurrentTime < 0.0f || !CurrentMontage)
	{
		return; // No active montage
	}

	float MontageDuration = CurrentMontage->GetPlayLength();
	if (MontageDuration <= 0.0f)
	{
		return;
	}

	// Get actor location for debug drawing
	FVector ActorLocation = Character->GetActorLocation();

	// Timeline rendering constants
	const float TimelineWidth = 600.0f;
	const float TimelineHeight = 20.0f;
	const float WindowHeight = 15.0f;

	// Draw timeline background
	FVector TimelineStart = ActorLocation + FVector(0, 0, YOffset);
	FVector TimelineEnd = TimelineStart + FVector(TimelineWidth, 0, 0);

	DrawDebugLine(World, TimelineStart, TimelineEnd, FColor::White, false, DrawDuration, 0, 2.0f);

	// Draw current time marker
	float CurrentX = (CurrentTime / MontageDuration) * TimelineWidth;
	FVector MarkerPos = TimelineStart + FVector(CurrentX, 0, 0);
	DrawDebugLine(World, MarkerPos, MarkerPos + FVector(0, 0, TimelineHeight), FColor::Green, false, DrawDuration, 0, 3.0f);

	// Draw checkpoints
	for (const FTimerCheckpoint& Checkpoint : Checkpoints)
	{
		float StartX = (Checkpoint.MontageTime / MontageDuration) * TimelineWidth;
		float EndX = ((Checkpoint.MontageTime + Checkpoint.Duration) / MontageDuration) * TimelineWidth;

		FVector WindowStart = TimelineStart + FVector(StartX, 0, -WindowHeight);
		FVector WindowEnd = TimelineStart + FVector(EndX, 0, -WindowHeight);

		// Color based on window type
		FColor WindowColor = FColor::Blue;
		switch (Checkpoint.WindowType)
		{
			case EActionWindowType::Combo:
				WindowColor = FColor::Yellow;
				break;
			case EActionWindowType::Parry:
				WindowColor = FColor::Red;
				break;
			case EActionWindowType::Hold:
				WindowColor = FColor::Purple;
				break;
			case EActionWindowType::Cancel:
				WindowColor = FColor::Orange;
				break;
			case EActionWindowType::Recovery:
				WindowColor = FColor::Cyan;
				break;
		}

		// Highlight if currently active
		bool bIsActive = IsTimeInWindow(CurrentTime, Checkpoint.MontageTime, Checkpoint.Duration);
		if (bIsActive)
		{
			WindowColor = FColor::Green;
		}

		DrawDebugLine(World, WindowStart, WindowEnd, WindowColor, false, DrawDuration, 0, 5.0f);
	}
}

void UMontageUtilityLibrary::LogCheckpoints(const TArray<FTimerCheckpoint>& Checkpoints, const FString& Prefix)
{
	FString LogPrefix = Prefix.IsEmpty() ? TEXT("[Checkpoints]") : FString::Printf(TEXT("[%s]"), *Prefix);

	UE_LOG(LogTemp, Log, TEXT("%s Total Checkpoints: %d"), *LogPrefix, Checkpoints.Num());

	for (int32 i = 0; i < Checkpoints.Num(); ++i)
	{
		const FTimerCheckpoint& Checkpoint = Checkpoints[i];

		FString WindowTypeName;
		switch (Checkpoint.WindowType)
		{
			case EActionWindowType::Combo:
				WindowTypeName = TEXT("Combo");
				break;
			case EActionWindowType::Parry:
				WindowTypeName = TEXT("Parry");
				break;
			case EActionWindowType::Hold:
				WindowTypeName = TEXT("Hold");
				break;
			case EActionWindowType::Cancel:
				WindowTypeName = TEXT("Cancel");
				break;
			case EActionWindowType::Recovery:
				WindowTypeName = TEXT("Recovery");
				break;
			default:
				WindowTypeName = TEXT("Unknown");
				break;
		}

		UE_LOG(LogTemp, Log, TEXT("%s [%d] %s: Time=%.3f Duration=%.3f Active=%s"),
			*LogPrefix,
			i,
			*WindowTypeName,
			Checkpoint.MontageTime,
			Checkpoint.Duration,
			Checkpoint.bActive ? TEXT("Yes") : TEXT("No")
		);
	}
}

// ============================================================================
// ATTACK RESOLUTION (Combo Progression)
// ============================================================================

UAttackData* UMontageUtilityLibrary::GetComboAttack(
	UAttackData* CurrentAttack,
	EInputType InputType,
	EAttackDirection Direction)
{
	if (!CurrentAttack)
	{
		UE_LOG(LogTemp, Warning, TEXT("[COMBO RESOLVE] GetComboAttack called with nullptr CurrentAttack"));
		return nullptr;
	}

	// Light input → traverse light combo chain (NextComboAttack)
	if (InputType == EInputType::LightAttack)
	{
		// ========================================================================
		// PRIORITY 1: Directional Follow-Ups (if direction specified)
		// ========================================================================
		// ARCHITECTURAL NOTE: Direction parameter is ONLY != None when:
		// 1. Player held attack button (hold window opened)
		// 2. Hold completed (animation froze/charged)
		// 3. Player released button WITH directional input
		// 4. DirectionalInputBuffer captured direction at release
		//
		// This ensures directional attacks require INTENTIONAL input, not accidental
		// movement stick deflection during normal combos.
		//
		// If Direction == None → Skip this check → Fall through to normal combo chain (Priority 2)
		if (Direction != EAttackDirection::None && CurrentAttack->DirectionalFollowUps.Num() > 0)
		{
			if (TObjectPtr<UAttackData>* DirectionalAttack = CurrentAttack->DirectionalFollowUps.Find(Direction))
			{
				UE_LOG(LogTemp, Log, TEXT("[COMBO RESOLVE] Found directional light follow-up from '%s': '%s'"),
					*CurrentAttack->GetName(), *(*DirectionalAttack)->GetName());
				return *DirectionalAttack;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[COMBO RESOLVE] Direction '%s' specified but no directional follow-up found for '%s'"),
					*UEnum::GetValueAsString(Direction), *CurrentAttack->GetName());
			}
		}

		// Normal light combo chain (fallback from directional or no direction specified)
		UAttackData* NextAttack = CurrentAttack->NextComboAttack;
		if (NextAttack)
		{
			UE_LOG(LogTemp, Log, TEXT("[COMBO RESOLVE] Light combo chain: '%s' → '%s'"),
				*CurrentAttack->GetName(), *NextAttack->GetName());
		}
		else
		{
			// Terminal node - check if this is a dead-end (no NextComboAttack AND no DirectionalFollowUps)
			// If so, return nullptr to signal combo reset instead of allowing infinite loops
			if (CurrentAttack->DirectionalFollowUps.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[COMBO RESOLVE] Terminal node '%s' (no NextComboAttack, no DirectionalFollowUps) → combo chain ends, resetting to default"),
					*CurrentAttack->GetName());
				return nullptr; // Combo reset - ResolveNextAttack will use default attack
			}

			UE_LOG(LogTemp, Warning, TEXT("[COMBO RESOLVE] Light combo chain ends at '%s' (NextComboAttack is nullptr, but has DirectionalFollowUps)"),
				*CurrentAttack->GetName());
		}
		return NextAttack;
	}

	// Heavy input → traverse heavy combo branch (HeavyComboAttack)
	if (InputType == EInputType::HeavyAttack)
	{
		// ========================================================================
		// PRIORITY 1: Heavy Directional Follow-Ups (if direction specified)
		// ========================================================================
		// Same architectural principle as Light directionals (see above)
		// Direction is only populated when DirectionalInputBuffer has valid input
		// (hold completed + released with direction)
		if (Direction != EAttackDirection::None && CurrentAttack->HeavyDirectionalFollowUps.Num() > 0)
		{
			if (TObjectPtr<UAttackData>* DirectionalAttack = CurrentAttack->HeavyDirectionalFollowUps.Find(Direction))
			{
				UE_LOG(LogTemp, Log, TEXT("[COMBO RESOLVE] Found directional heavy follow-up from '%s': '%s'"),
					*CurrentAttack->GetName(), *(*DirectionalAttack)->GetName());
				return *DirectionalAttack;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[COMBO RESOLVE] Direction '%s' specified but no heavy directional follow-up found for '%s'"),
					*UEnum::GetValueAsString(Direction), *CurrentAttack->GetName());
			}
		}

		// Normal heavy branch
		UAttackData* HeavyBranch = CurrentAttack->HeavyComboAttack;
		if (HeavyBranch)
		{
			UE_LOG(LogTemp, Log, TEXT("[COMBO RESOLVE] Heavy combo branch: '%s' → '%s'"),
				*CurrentAttack->GetName(), *HeavyBranch->GetName());
		}
		else
		{
			// Terminal node - check if this is a dead-end (no HeavyComboAttack AND no HeavyDirectionalFollowUps)
			// If so, return nullptr to signal combo reset instead of allowing infinite loops
			if (CurrentAttack->HeavyDirectionalFollowUps.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[COMBO RESOLVE] Terminal node '%s' (no HeavyComboAttack, no HeavyDirectionalFollowUps) → combo chain ends, resetting to default"),
					*CurrentAttack->GetName());
				return nullptr; // Combo reset - ResolveNextAttack will use default attack
			}

			UE_LOG(LogTemp, Warning, TEXT("[COMBO RESOLVE] Heavy combo branch ends at '%s' (HeavyComboAttack is nullptr, but has HeavyDirectionalFollowUps)"),
				*CurrentAttack->GetName());
		}
		return HeavyBranch;
	}

	// Other input types (Dodge, Block) don't have combo chains
	UE_LOG(LogTemp, Warning, TEXT("[COMBO RESOLVE] GetComboAttack called with non-attack input type"));
	return nullptr;
}

UAttackData* UMontageUtilityLibrary::ResolveNextAttack(
	UAttackData* CurrentAttack,
	EInputType InputType,
	bool bComboWindowActive,
	bool bIsHolding,
	UAttackData* DefaultLightAttack,
	UAttackData* DefaultHeavyAttack,
	EAttackDirection Direction)
{
	const TCHAR* InputTypeName = InputType == EInputType::LightAttack ? TEXT("Light") :
	                             InputType == EInputType::HeavyAttack ? TEXT("Heavy") : TEXT("Other");

	UE_LOG(LogTemp, Log, TEXT("[COMBO RESOLVE] ResolveNextAttack: Input=%s, ComboWindow=%s, CurrentAttack=%s, Holding=%s"),
		InputTypeName,
		bComboWindowActive ? TEXT("ACTIVE") : TEXT("Inactive"),
		CurrentAttack ? *CurrentAttack->GetName() : TEXT("nullptr"),
		bIsHolding ? TEXT("Yes") : TEXT("No"));

	// If combo window is active and we have a current attack, try to combo
	if (bComboWindowActive && CurrentAttack)
	{
		UE_LOG(LogTemp, Log, TEXT("[COMBO RESOLVE] Attempting combo progression from '%s'..."), *CurrentAttack->GetName());
		UAttackData* ComboAttack = GetComboAttack(CurrentAttack, InputType, Direction);

		// If combo chain continues, use it
		if (ComboAttack)
		{
			UE_LOG(LogTemp, Log, TEXT("[COMBO RESOLVE] ✓ Resolved to combo: '%s'"), *ComboAttack->GetName());
			return ComboAttack;
		}

		// If combo chain ends (nullptr), fall through to default attacks
		UE_LOG(LogTemp, Warning, TEXT("[COMBO RESOLVE] Combo chain ended, falling back to default attack"));
	}
	else
	{
		if (!bComboWindowActive)
		{
			UE_LOG(LogTemp, Log, TEXT("[COMBO RESOLVE] No combo window → using default attack"));
		}
		if (!CurrentAttack)
		{
			UE_LOG(LogTemp, Log, TEXT("[COMBO RESOLVE] No current attack → using default attack"));
		}
	}

	// No combo window, or combo chain ended, or no current attack → return default
	UAttackData* ResolvedAttack = nullptr;
	switch (InputType)
	{
		case EInputType::LightAttack:
			ResolvedAttack = DefaultLightAttack;
			break;

		case EInputType::HeavyAttack:
			ResolvedAttack = DefaultHeavyAttack;
			break;

		default:
			break; // Other input types don't have attacks
	}

	if (ResolvedAttack)
	{
		UE_LOG(LogTemp, Log, TEXT("[COMBO RESOLVE] ✓ Resolved to default: '%s'"), *ResolvedAttack->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[COMBO RESOLVE] ✗ Failed to resolve attack (nullptr result)"));
	}

	return ResolvedAttack;
}

// ============================================================================
// CONTEXT-AWARE ATTACK RESOLUTION
// ============================================================================

FAttackResolutionResult UMontageUtilityLibrary::ResolveNextAttackContextual(
	UAttackData* CurrentAttack,
	EInputType InputType,
	EAttackDirection Direction,
	const FHoldState& HoldState,
	bool bComboWindowActive,
	UAttackData* DefaultLightAttack,
	UAttackData* DefaultHeavyAttack,
	const FGameplayTagContainer& ActiveContext,
	TSet<UAttackData*>& VisitedAttacks)
{
	FAttackResolutionResult Result;

	// Safety: Check for cycle (visited this attack already)
	if (CurrentAttack && VisitedAttacks.Contains(CurrentAttack))
	{
		UE_LOG(LogCombat, Error, TEXT("[RESOLVE] Cycle detected! Attack '%s' already visited. Falling back to default."),
			*CurrentAttack->GetName());
		Result.bCycleDetected = true;

		// GRACEFUL FALLBACK: Set CurrentAttack to nullptr to skip Priority 2/3 and jump directly to Priority 4 (default attacks)
		// This ensures player gets a functional attack instead of "dead input"
		CurrentAttack = nullptr;

		// Continue to default attack resolution below (don't return early)
	}

	// Add current attack to visited set
	if (CurrentAttack)
	{
		VisitedAttacks.Add(CurrentAttack);
	}

	const TCHAR* InputTypeName = InputType == EInputType::LightAttack ? TEXT("Light") :
	                             InputType == EInputType::HeavyAttack ? TEXT("Heavy") : TEXT("Other");

	UE_LOG(LogCombat, Log, TEXT("[RESOLVE] Input=%s, Direction=%d, HoldCompleted=%s, ComboWindow=%s, CurrentAttack=%s"),
		InputTypeName,
		static_cast<int32>(Direction),
		HoldState.IsHoldCompleted() ? TEXT("Yes") : TEXT("No"),
		bComboWindowActive ? TEXT("ACTIVE") : TEXT("Inactive"),
		CurrentAttack ? *CurrentAttack->GetName() : TEXT("nullptr"));

	// ========================================================================
	// PRIORITY 1: Context-Sensitive Attacks (Future: Parry Counters, Finishers)
	// ========================================================================
	// TODO: Check RequiredContextTags against ActiveContext
	// For now, skip this priority (no context-sensitive attacks yet)

	// ========================================================================
	// PRIORITY 2: Directional Follow-Ups (if hold COMPLETED + direction + current attack has directionals)
	// ========================================================================
	// Check IsHoldCompleted() instead of just IsHolding()
	// This ensures hold was completed (button held through freeze/charge then released)
	// Prevents directional attacks from triggering during normal combos when player presses movement direction
	if (HoldState.IsHoldCompleted() && Direction != EAttackDirection::None && CurrentAttack)
	{
		UE_LOG(LogCombat, Log, TEXT("[RESOLVE] Checking directional follow-ups (Hold COMPLETED detected)..."));

		// Check input-type-specific directional maps
		UAttackData* DirectionalAttack = nullptr;
		if (InputType == EInputType::HeavyAttack && CurrentAttack->HeavyDirectionalFollowUps.Contains(Direction))
		{
			DirectionalAttack = CurrentAttack->HeavyDirectionalFollowUps[Direction];
			UE_LOG(LogCombat, Log, TEXT("[RESOLVE] Found HeavyDirectionalFollowUp for direction %d"), static_cast<int32>(Direction));
		}
		else if (InputType == EInputType::LightAttack && CurrentAttack->DirectionalFollowUps.Contains(Direction))
		{
			DirectionalAttack = CurrentAttack->DirectionalFollowUps[Direction];
			UE_LOG(LogCombat, Log, TEXT("[RESOLVE] Found DirectionalFollowUp for direction %d"), static_cast<int32>(Direction));
		}

		if (DirectionalAttack)
		{
			Result.Attack = DirectionalAttack;
			Result.Path = EResolutionPath::DirectionalFollowUp;
			Result.bShouldClearDirectionalInput = true; // Signal to clear LastDirectionalInput
			UE_LOG(LogCombat, Log, TEXT("[RESOLVE] Resolved to DirectionalFollowUp: '%s' (CLEAR SIGNAL)"), *DirectionalAttack->GetName());
			return Result;
		}
		else
		{
			UE_LOG(LogCombat, Log, TEXT("[RESOLVE] No directional follow-up found for direction %d"), static_cast<int32>(Direction));
		}
	}

	// ========================================================================
	// PRIORITY 3: Normal Combo Chain (if combo window active)
	// ========================================================================
	if (bComboWindowActive && CurrentAttack)
	{
		UE_LOG(LogCombat, Log, TEXT("[RESOLVE] Checking combo chain (ComboWindow active)..."));

		UAttackData* ComboAttack = GetComboAttack(CurrentAttack, InputType, Direction);
		if (ComboAttack)
		{
			Result.Attack = ComboAttack;
			Result.Path = EResolutionPath::NormalCombo;
			UE_LOG(LogCombat, Log, TEXT("[RESOLVE] Resolved to NormalCombo: '%s'"), *ComboAttack->GetName());
			return Result;
		}
		else
		{
			UE_LOG(LogCombat, Log, TEXT("[RESOLVE] Combo chain ended (nullptr), falling back to default"));
		}
	}

	// ========================================================================
	// PRIORITY 4: Default Attacks (fallback)
	// ========================================================================
	UAttackData* DefaultAttack = nullptr;
	switch (InputType)
	{
		case EInputType::LightAttack:
			DefaultAttack = DefaultLightAttack;
			break;

		case EInputType::HeavyAttack:
			DefaultAttack = DefaultHeavyAttack;
			break;

		default:
			break; // Other input types don't have attacks
	}

	if (DefaultAttack)
	{
		Result.Attack = DefaultAttack;
		Result.Path = EResolutionPath::Default;
		UE_LOG(LogCombat, Log, TEXT("[RESOLVE] Resolved to Default: '%s'"), *DefaultAttack->GetName());
	}
	else
	{
		// EMERGENCY FALLBACK: Default attack is nullptr - this should never happen if validation works
		// But we provide graceful degradation instead of crashing or giving "dead input"
		UE_LOG(LogCombat, Error, TEXT("[RESOLVE] CRITICAL: Default %s attack is nullptr! Check CombatSettings setup."),
			InputType == EInputType::LightAttack ? TEXT("Light") : TEXT("Heavy"));

		// Try to use the original current attack as fallback (repeat same attack)
		// We saved CurrentAttack before potentially setting it to nullptr for cycle detection
		if (VisitedAttacks.Num() > 0)
		{
			// Get the first attack we visited (the one that started this resolution)
			TArray<UAttackData*> VisitedArray = VisitedAttacks.Array();
			UAttackData* OriginalAttack = VisitedArray[0];

			if (OriginalAttack)
			{
				Result.Attack = OriginalAttack;
				Result.Path = EResolutionPath::Default; // Mark as default even though it's emergency
				UE_LOG(LogCombat, Warning, TEXT("[RESOLVE] Emergency fallback: Repeating original attack '%s'"),
					*OriginalAttack->GetName());

				// Show on-screen error in editor
				#if WITH_EDITOR
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
						FString::Printf(TEXT("⚠️ CRITICAL: Default %s attack is nullptr! Fix in CombatComponent!"),
							InputType == EInputType::LightAttack ? TEXT("Light") : TEXT("Heavy")));
				}
				#endif
			}
			else
			{
				// LAST RESORT: Even emergency fallback failed - return nullptr, DON'T crash
				UE_LOG(LogCombat, Error, TEXT("[RESOLVE] Cannot resolve attack: No default AND no current attack. "
				                              "Check CombatSettings assignment on character."));
				#if WITH_EDITOR
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
						TEXT("Combat: No attacks configured. Assign CombatSettings!"));
				}
				#endif
				// Return empty result - caller handles nullptr gracefully
			}
		}
		else
		{
			// No visited attacks either (first attack in chain was nullptr) - DON'T crash
			UE_LOG(LogCombat, Error, TEXT("[RESOLVE] Cannot resolve attack: No default AND no current attack. "
			                              "Check CombatSettings assignment on character."));
			#if WITH_EDITOR
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
					TEXT("Combat: No attacks configured. Assign CombatSettings!"));
			}
			#endif
			// Return empty result - caller handles nullptr gracefully
		}
	}

	return Result;
}

// ============================================================================
// HOLD SYSTEM HELPERS
// ============================================================================

bool UMontageUtilityLibrary::LoopMontageSection(ACharacter* Character, FName LoopSectionName)
{
	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Hold] LoopMontageSection failed: Character is nullptr"));
		return false;
	}

	UAnimInstance* AnimInstance = GetAnimInstance(Character);
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Hold] LoopMontageSection failed: AnimInstance is nullptr"));
		return false;
	}

	UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();
	if (!CurrentMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Hold] LoopMontageSection failed: No active montage"));
		return false;
	}

	// Verify section exists
	int32 SectionIndex = CurrentMontage->GetSectionIndex(LoopSectionName);
	if (SectionIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Hold] LoopMontageSection failed: Section '%s' not found in montage '%s'"),
			*LoopSectionName.ToString(), *CurrentMontage->GetName());
		return false;
	}

	// Set section to loop back to itself
	AnimInstance->Montage_SetNextSection(LoopSectionName, LoopSectionName, CurrentMontage);

	UE_LOG(LogTemp, Log, TEXT("[Hold] Section '%s' set to loop in montage '%s'"),
		*LoopSectionName.ToString(), *CurrentMontage->GetName());

	return true;
}

EAttackDirection UMontageUtilityLibrary::GetDirectionFromInput(FVector2D DirectionInput, FRotator ActorRotation, float DeadzoneThreshold)
{
	// Check if input magnitude is below deadzone
	float InputMagnitude = DirectionInput.Length();
	if (InputMagnitude < DeadzoneThreshold)
	{
		return EAttackDirection::None;
	}

	// ACTOR-RELATIVE TRANSFORMATION:
	// The input is camera-relative, but we need it relative to the actor's facing direction.
	// We do this by rotating the input vector by the inverse of the actor's yaw.

	// Convert 2D input to 3D vector (XY plane, Z=0)
	FVector InputVector3D(DirectionInput.X, DirectionInput.Y, 0.0f);

	// Create inverse rotation (negative yaw) to transform to actor-local space
	// This makes "forward" relative to where the actor is facing, not world-north
	FRotator InverseActorYaw(0.0f, -ActorRotation.Yaw, 0.0f);
	FVector ActorRelativeVector = InverseActorYaw.RotateVector(InputVector3D);

	// Convert back to 2D
	FVector2D ActorRelativeInput(ActorRelativeVector.X, ActorRelativeVector.Y);
	ActorRelativeInput.Normalize();

	// Calculate angle in degrees (0° = forward relative to actor, increases clockwise)
	// Atan2 returns radians, convert to degrees
	// Note: Y is forward in UE, X is right
	float Angle = FMath::Atan2(ActorRelativeInput.X, ActorRelativeInput.Y) * (180.0f / PI);

	// Normalize angle to [0, 360)
	if (Angle < 0.0f)
	{
		Angle += 360.0f;
	}

	// Map angle to 4 cardinal directions (90° quadrants)
	// Forward: 315° to 45° (90° cone) - relative to actor facing
	// Right: 45° to 135° (90° cone) - actor's right
	// Backward: 135° to 225° (90° cone) - behind actor
	// Left: 225° to 315° (90° cone) - actor's left

	if (Angle >= 315.0f || Angle < 45.0f)
	{
		return EAttackDirection::Forward;
	}
	else if (Angle >= 45.0f && Angle < 135.0f)
	{
		return EAttackDirection::Right;
	}
	else if (Angle >= 135.0f && Angle < 225.0f)
	{
		return EAttackDirection::Backward;
	}
	else // 225.0° to 315.0°
	{
		return EAttackDirection::Left;
	}
}