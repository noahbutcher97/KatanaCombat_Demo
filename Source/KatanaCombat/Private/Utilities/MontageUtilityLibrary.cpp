// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/MontageUtilityLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameFramework/Character.h"
#include "ActionQueueTypes.h"
#include "Animation/AnimNotifyState_ComboWindow.h"
#include "Animation/AnimNotifyState_ParryWindow.h"
#include "Animation/AnimNotifyState_HoldWindow.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Data/AttackData.h"

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

	// Scan all AnimNotifyStates in the montage
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (NotifyEvent.NotifyStateClass)
		{
			EActionWindowType WindowType = EActionWindowType::Recovery;
			bool bIsWindowNotify = false;

			// Check for window-type AnimNotifyStates
			if (Cast<UAnimNotifyState_ComboWindow>(NotifyEvent.NotifyStateClass))
			{
				WindowType = EActionWindowType::Combo;
				bIsWindowNotify = true;
			}
			else if (Cast<UAnimNotifyState_ParryWindow>(NotifyEvent.NotifyStateClass))
			{
				WindowType = EActionWindowType::Parry;
				bIsWindowNotify = true;
			}
			else if (Cast<UAnimNotifyState_HoldWindow>(NotifyEvent.NotifyStateClass))
			{
				WindowType = EActionWindowType::Hold;
				bIsWindowNotify = true;
			}
			// Note: Cancel windows can be added here when AnimNotifyState_CancelWindow is implemented

			if (bIsWindowNotify)
			{
				FTimerCheckpoint Checkpoint;
				Checkpoint.WindowType = WindowType;
				Checkpoint.MontageTime = NotifyEvent.GetTriggerTime();
				Checkpoint.Duration = NotifyEvent.GetDuration();
				Checkpoint.bActive = true;

				OutCheckpoints.Add(Checkpoint);
			}
		}
	}

	// Sort checkpoints by montage time
	OutCheckpoints.Sort([](const FTimerCheckpoint& A, const FTimerCheckpoint& B)
	{
		return A.MontageTime < B.MontageTime;
	});

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

	// Note: BlendTime parameter is for future implementation
	// UE's Montage_JumpToSection doesn't support blend time (instant jump)
	// Could be implemented with custom blend logic if needed
	AnimInstance->Montage_JumpToSection(SectionName, CurrentMontage);

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
		// Check for directional follow-up first (if direction specified)
		if (Direction != EAttackDirection::None)
		{
			if (TObjectPtr<UAttackData>* DirectionalAttack = CurrentAttack->DirectionalFollowUps.Find(Direction))
			{
				UE_LOG(LogTemp, Log, TEXT("[COMBO RESOLVE] Found directional light follow-up from '%s': '%s'"),
					*CurrentAttack->GetName(), *(*DirectionalAttack)->GetName());
				return *DirectionalAttack;
			}
		}

		// Normal light combo chain
		UAttackData* NextAttack = CurrentAttack->NextComboAttack;
		if (NextAttack)
		{
			UE_LOG(LogTemp, Log, TEXT("[COMBO RESOLVE] Light combo chain: '%s' → '%s'"),
				*CurrentAttack->GetName(), *NextAttack->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[COMBO RESOLVE] Light combo chain ends at '%s' (NextComboAttack is nullptr)"),
				*CurrentAttack->GetName());
		}
		return NextAttack;
	}

	// Heavy input → traverse heavy combo branch (HeavyComboAttack)
	if (InputType == EInputType::HeavyAttack)
	{
		// Check for heavy directional follow-up first (if direction specified)
		if (Direction != EAttackDirection::None)
		{
			if (TObjectPtr<UAttackData>* DirectionalAttack = CurrentAttack->HeavyDirectionalFollowUps.Find(Direction))
			{
				UE_LOG(LogTemp, Log, TEXT("[COMBO RESOLVE] Found directional heavy follow-up from '%s': '%s'"),
					*CurrentAttack->GetName(), *(*DirectionalAttack)->GetName());
				return *DirectionalAttack;
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
			UE_LOG(LogTemp, Warning, TEXT("[COMBO RESOLVE] Heavy combo branch ends at '%s' (HeavyComboAttack is nullptr)"),
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