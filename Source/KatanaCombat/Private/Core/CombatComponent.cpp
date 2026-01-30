// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CombatComponent.h"
#include "Core/WeaponComponent.h"
#include "Interfaces/CombatInterface.h"
#include "Data/AttackData.h"
#include "Data/AttackConfiguration.h"
#include "Data/CombatSettings.h"
#include "Data/PairedAnimationData.h"
#include "Data/TargetingSettings.h"
#include "Debug/DebugConfig.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Characters/BaseCombatCharacter.h"
#include "Core/TargetingComponent.h"
#include "Core/HitReactionComponent.h"
#include "Utilities/MontageUtilityLibrary.h"
#include "Utilities/CombatUtils.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "Debug/DebugUtils.h"

// ============================================================================
// LOG CATEGORY DEFINITION
// ============================================================================

DEFINE_LOG_CATEGORY(LogCombat);

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	// CRITICAL: Initialize input context to Movement (default state)
	// Ensures clean state on component spawn/respawn
	CurrentInputContext = EInputContext::Movement;

	// Cache owner character (ABaseCombatCharacter for proper CombatSettings access)
	OwnerCharacter = Cast<ABaseCombatCharacter>(GetOwner());
	if (OwnerCharacter)
	{
		// Cache combat settings from character
		CombatSettings = OwnerCharacter->CombatSettings;

		// Bind to montage event delegates for event-driven phase transitions
		if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
		{
			AnimInstance->OnMontageBlendingOut.AddDynamic(this, &UCombatComponent::OnMontageBlendingOut);
			AnimInstance->OnMontageEnded.AddDynamic(this, &UCombatComponent::OnMontageEnded);

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[INIT] Montage event delegates bound (BlendingOut, Ended)"));
			}
		}

		// Validate default attacks are assigned (critical for graceful fallback system)
		#if WITH_EDITOR
		ValidateDefaultAttacks();
		#endif
	}
}

void UCombatComponent::ValidateDefaultAttacks()
{
	if (!CombatSettings || !CombatSettings->AttackConfiguration)
	{
		UE_LOG(LogCombat, Error, TEXT("[VALIDATION] CombatSettings or AttackConfiguration is nullptr on %s! "
		                              "Combat system cannot function. Assign CombatSettings with AttackConfiguration in Character Blueprint."),
			*GetOwner()->GetName());

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
				FString::Printf(TEXT("⚠️ %s: Missing CombatSettings or AttackConfiguration!"), *GetOwner()->GetName()));
		}
		return;
	}

	bool bHasErrors = false;

	// Check if default light attack is assigned
	UAttackData* DefaultLight = GetDefaultLightAttack();
	if (!DefaultLight)
	{
		UE_LOG(LogCombat, Error, TEXT("[VALIDATION] DefaultLightAttack is nullptr on %s! "
		                              "Combat system will not work properly. Assign in AttackConfiguration asset."),
			*GetOwner()->GetName());
		bHasErrors = true;
	}

	// Check if default heavy attack is assigned
	UAttackData* DefaultHeavy = GetDefaultHeavyAttack();
	if (!DefaultHeavy)
	{
		UE_LOG(LogCombat, Error, TEXT("[VALIDATION] DefaultHeavyAttack is nullptr on %s! "
		                              "Combat system will not work properly. Assign in AttackConfiguration asset."),
			*GetOwner()->GetName());
		bHasErrors = true;
	}

	// Show on-screen warning in editor PIE sessions
	if (bHasErrors && GEngine)
	{
		const FString ErrorMsg = FString::Printf(
			TEXT("⚠️ CombatComponent on %s: Default attacks not assigned! Fix in AttackConfiguration asset."),
			*GetOwner()->GetName()
		);

		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, ErrorMsg);

		// Log comprehensive fix instructions
		UE_LOG(LogCombat, Warning, TEXT("[VALIDATION] To fix this:"));
		UE_LOG(LogCombat, Warning, TEXT("  1. Open your CombatSettings asset"));
		UE_LOG(LogCombat, Warning, TEXT("  2. Open the AttackConfiguration asset"));
		UE_LOG(LogCombat, Warning, TEXT("  3. Assign DefaultLightAttack and DefaultHeavyAttack in the Details panel"));
	}
	else if (!bHasErrors && GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[VALIDATION] ✓ Default attacks validated successfully"));
	}
}

void UCombatComponent::OnCharacterDeath()
{
	// CRITICAL: Full combat state reset on death
	// Prevents state leaks across respawns (hold state, queued actions, input context)

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Warning, TEXT("[DEATH] Character died - resetting all combat state"));
	}

	// PAIRED ANIMATION INTERRUPT: Notify all partners that this character died
	// This allows victims to cancel their paired animations if attacker dies
	if (PairedAnimationPartners.Num() > 0)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[DEATH] Notifying %d paired animation partners"), PairedAnimationPartners.Num());
		}

		// Copy array before iterating (partners will remove themselves when notified)
		TArray<TWeakObjectPtr<AActor>> PartnersCopy = PairedAnimationPartners;
		for (const TWeakObjectPtr<AActor>& PartnerPtr : PartnersCopy)
		{
			if (AActor* Partner = PartnerPtr.Get())
			{
				// Find partner's CombatComponent and notify them
				if (UCombatComponent* PartnerCombat = Partner->FindComponentByClass<UCombatComponent>())
				{
					PartnerCombat->OnPairedPartnerDeath(GetOwner());
				}
			}
		}
	}

	// Clear our own partners list
	ClearPairedPartners();

	// If we were in a paired animation, end it
	if (IsPairedAnimationActive())
	{
		EndPairedAnimation();
	}

	// Safety: Always restore combat input on death (prevent stuck state)
	bBlockCombatInput = false;

	// Clear action queue and statistics
	ClearQueue(true);

	// Clear hold state (ease timer, flags, playrate)
	ClearHoldState();

	// Reset directional input buffer
	DirectionalInputBuffer.Reset();

	// Clear held inputs
	HeldInputs.Empty();

	// Reset to idle phase
	SetPhase(EAttackPhase::None);

	// Clear checkpoints
	Checkpoints.Empty();

	// Reset input context to Movement (default)
	SetInputContext(EInputContext::Movement);

	// Clear context tracking
	ActiveContextTags.Reset();
	VisitedAttacks.Empty();

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[DEATH] ✓ Combat state reset complete - ready for respawn"));
	}
}

void UCombatComponent::SetInputContext(EInputContext NewContext)
{
	if (CurrentInputContext != NewContext)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[INPUT CONTEXT] %s → %s"),
				*UEnum::GetValueAsString(CurrentInputContext),
				*UEnum::GetValueAsString(NewContext));
		}

		CurrentInputContext = NewContext;
	}
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Combat system is fully event-driven!
	// - Input processing: OnInputEvent (immediate)
	// - Queue execution: ProcessQueuedActions (called on phase transitions)
	// - Hold easing: OnEaseTimerTick (timer-based, 60Hz)
	// - Phase tracking: OnPhaseTransition (AnimNotify events)
	//
	// Only debug visualization remains in tick (harmless, can be disabled)

	// NOTE: Movement sync is EVENT-DRIVEN (called from phase transitions, playrate changes)
	// NO per-frame logic here except debug visualization

	if (GetDebugDraw())
	{
		DrawDebugInfo();
	}
}

ABaseCombatCharacter* UCombatComponent::GetOwnerCharacter() const
{
	// Return cached owner character (no cast needed - already cached in BeginPlay)
	return OwnerCharacter;
}

bool UCombatComponent::GetDebugDraw() const
{
	// Debug visualization is now CVar-controlled (Combat.Debug.All or specific CVars)
	return CombatDebug::IsDebugEnabled();
}

UAttackData* UCombatComponent::GetDefaultLightAttack() const
{
	// Priority 1: WeaponComponent's effective attack configuration (weapon-specific moveset)
	if (AActor* Owner = GetOwner())
	{
		if (UWeaponComponent* WeaponComp = Owner->FindComponentByClass<UWeaponComponent>())
		{
			if (UAttackConfiguration* WeaponConfig = WeaponComp->GetEffectiveAttackConfiguration())
			{
				if (WeaponConfig->DefaultLightAttack)
				{
					return WeaponConfig->DefaultLightAttack;
				}
			}
		}
	}

	// Priority 2: CombatSettings → AttackConfiguration → DefaultLightAttack
	if (CombatSettings && CombatSettings->AttackConfiguration)
	{
		return CombatSettings->AttackConfiguration->DefaultLightAttack;
	}

	return nullptr;
}

UAttackData* UCombatComponent::GetDefaultHeavyAttack() const
{
	// Priority 1: WeaponComponent's effective attack configuration (weapon-specific moveset)
	if (AActor* Owner = GetOwner())
	{
		if (UWeaponComponent* WeaponComp = Owner->FindComponentByClass<UWeaponComponent>())
		{
			if (UAttackConfiguration* WeaponConfig = WeaponComp->GetEffectiveAttackConfiguration())
			{
				if (WeaponConfig->DefaultHeavyAttack)
				{
					return WeaponConfig->DefaultHeavyAttack;
				}
			}
		}
	}

	// Priority 2: CombatSettings → AttackConfiguration → DefaultHeavyAttack
	if (CombatSettings && CombatSettings->AttackConfiguration)
	{
		return CombatSettings->AttackConfiguration->DefaultHeavyAttack;
	}

	return nullptr;
}

// ============================================================================
// INPUT PROCESSING
// ============================================================================

void UCombatComponent::OnInputEventWithTransform(
	EInputType InputType,
	EInputEventType EventType,
	FVector2D CameraRelativeInput,
	FRotator CameraRotation,
	FRotator CharacterRotation)
{
	// Get owner character for mesh offset detection
	ACharacter* Character = Cast<ACharacter>(GetOwner());

	// Transform camera-relative input to character-relative direction
	// CRITICAL FIX (2025-11-20): Now passes Character for automatic mesh offset correction
	EInputDirection CharacterRelativeDirection = UCombatUtils::VectorToCharacterRelativeDirection(
		CameraRelativeInput,
		CameraRotation,
		Character,  // Mesh offset detection
		CharacterRotation,
		0.2f  // Dead zone
	);

	// Delegate to core input handler with transformed direction
	OnInputEvent(InputType, EventType, CharacterRelativeDirection);
}

void UCombatComponent::OnInputEventAuto(
	EInputType InputType,
	EInputEventType EventType,
	FVector2D MovementInput,
	bool bCharacterRelative)
{
	// Get the owning character
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		UE_LOG(LogCombat, Error, TEXT("[INPUT AUTO] Owner is not a Character! Cannot auto-transform input."));
		return;
	}

	// Get character rotation (easy - just get owner's rotation)
	FRotator CharacterRotation = Character->GetActorRotation();

	// Get camera rotation from the controller
	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC)
	{
		// Fallback: If no player controller, assume camera faces same direction as character
		// This handles AI-controlled characters gracefully
		UE_LOG(LogCombat, Warning, TEXT("[INPUT AUTO] No PlayerController found, assuming camera = character rotation"));
		OnInputEventWithTransform(InputType, EventType, MovementInput, CharacterRotation, CharacterRotation);
		return;
	}

	// Get camera rotation (control rotation for player-controlled characters)
	FRotator CameraRotation = PC->GetControlRotation();

	// TOGGLE: Character-relative vs Camera-relative
	if (bCharacterRelative)
	{
		// Character-relative: Transform input based on character facing
		OnInputEventWithTransform(InputType, EventType, MovementInput, CameraRotation, CharacterRotation);
	}
	else
	{
		// Camera-relative (legacy): Transform uses camera as reference frame
		// Pass camera rotation as both camera AND character to skip character transformation
		OnInputEventWithTransform(InputType, EventType, MovementInput, CameraRotation, CameraRotation);
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[INPUT AUTO] Mode=%s, Camera=%.1f°, Character=%.1f°"),
			bCharacterRelative ? TEXT("Character-Relative") : TEXT("Camera-Relative"),
			CameraRotation.Yaw, CharacterRotation.Yaw);
	}
}

void UCombatComponent::OnInputEvent(EInputType InputType, EInputEventType EventType, EInputDirection InputDirection)
{
	// Early exit if no CombatSettings
	if (!CombatSettings)
	{
		return;
	}

	// Check if input can be processed (gate stunned/dead/guard broken states)
	if (!CanProcessInput(InputType))
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Warning, TEXT("[INPUT] Input REJECTED - Cannot process in current combat state"));
		}
		return;
	}

	// ============================================================================
	// CONTEXT-AWARE DIRECTIONAL INPUT SAMPLING (Architectural Fix)
	// ============================================================================
	// Direction is ONLY captured during DirectionalInput context (hold release window)
	// This prevents movement stick deflection during normal combos from triggering directionals
	//
	// Design: Movement input (continuous) != Attack input (discrete)
	// - Movement context: Stick for character movement ONLY, ignored for attacks
	// - DirectionalInput context: Stick sampled at release for directional attacks

	if (InputDirection != EInputDirection::None)
	{
		if (CurrentInputContext == EInputContext::DirectionalInput)
		{
			// Sample direction ONLY at RELEASE event (after hold completion)
			if (EventType == EInputEventType::Release)
			{
				// Capture direction in buffer (discrete sampling, not continuous)
				DirectionalInputBuffer.CaptureDirection(InputDirection, GetWorld()->GetTimeSeconds());

				// CRITICAL: Update HoldEvent direction for directional follow-up resolution
				// Convert EInputDirection → EAttackDirection and store in current hold event
				if (HoldState.IsHolding())
				{
					HoldState.CurrentHold.Direction = UCombatUtils::InputToAttackDirection(InputDirection);

					if (GetDebugDraw())
					{
						UE_LOG(LogCombat, Log, TEXT("[DIRECTIONAL] Direction captured at RELEASE: %s (time=%.2f) → HoldEvent.Direction=%s"),
							*UDebugUtils::FormatInputDirectionDebug(InputDirection),
							DirectionalInputBuffer.CaptureTime,
							*UDebugUtils::FormatAttackDirectionDebug(HoldState.CurrentHold.Direction));
					}
				}
				else if (GetDebugDraw())
				{
					UE_LOG(LogCombat, Log, TEXT("[DIRECTIONAL] Direction captured at RELEASE: %s (time=%.2f) [NO ACTIVE HOLD]"),
						*UDebugUtils::FormatInputDirectionDebug(InputDirection),
						DirectionalInputBuffer.CaptureTime);
				}
			}
			else if (GetDebugDraw())
			{
				// Direction provided during hold, but not capturing until release
				UE_LOG(LogCombat, Verbose, TEXT("[DIRECTIONAL] Direction during hold: %s (awaiting release to capture)"),
					*UDebugUtils::FormatInputDirectionDebug(InputDirection));
			}
		}
		else if (GetDebugDraw())
		{
			// Direction provided but context is Movement (ignored for attack purposes)
			UE_LOG(LogCombat, Verbose, TEXT("[DIRECTIONAL] Direction IGNORED (context=%s, not DirectionalInput)"),
				*UEnum::GetValueAsString(CurrentInputContext));
		}

		// DEPRECATED: Keep LastDirectionalInput for backward compatibility (will be removed in future)
		LastDirectionalInput = InputDirection;
		bDirectionalInputConsumed = false;
	}
	else if (GetDebugDraw())
	{
		// No direction provided (Blueprint might not be passing movement input)
		UE_LOG(LogCombat, Verbose, TEXT("[INPUT] No directional input provided"));
	}

	// Get current game time
	const float CurrentTime = GetWorld()->GetTimeSeconds();

	// Create input action
	FQueuedInputAction InputAction(InputType, EventType, CurrentTime, bComboWindowActive);

	// Track press/release pairs
	if (EventType == EInputEventType::Press)
	{
		// Check if we can accept new input (not in commit window, not duplicate)
		if (!CanAcceptNewInput(InputType))
		{
			return; // Input rejected
		}

		HeldInputs.Add(InputType, CurrentTime);

		if ( GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[INPUT] %s PRESSED at %.2f (Combo: %s, Direction: %s)"),
				*UEnum::GetValueAsString(InputType),
				CurrentTime,
				bComboWindowActive ? TEXT("YES") : TEXT("NO"),
				InputDirection != EInputDirection::None ? *UEnum::GetValueAsString(InputDirection) : TEXT("None"));
		}
	}
	else // Release
	{
		if (float* PressTime = HeldInputs.Find(InputType))
		{
			// Found matching press - process as pair
			FQueuedInputAction PressEvent(InputType, EInputEventType::Press, *PressTime, bComboWindowActive);
			ProcessInputPair(PressEvent, InputAction);
			HeldInputs.Remove(InputType);
		}

		if ( GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[INPUT] %s RELEASED at %.2f"),
				*UEnum::GetValueAsString(InputType),
				CurrentTime);
		}

		// Handle hold deactivation
		if (HoldState.IsHolding() && HoldState.GetHeldInputType() == InputType)
		{
			DeactivateHold();
		}
	}

	// Queue action for processing
	QueueAction(InputAction);

	// Update stats
	QueueStats.TotalInputs++;
}

bool UCombatComponent::CanProcessInput(EInputType InputType) const
{
	// Block combat input during paired animations (finishers, counters)
	// This prevents accidental input buffering during cinematics
	if (bBlockCombatInput)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Verbose, TEXT("[INPUT] Input blocked - paired animation in progress"));
		}
		return false;
	}

	// Accepts input unless explicitly disabled
	// Higher-level systems (death, hit reactions) should disable input at the source
	return true;
}

// ============================================================================
// ACTION QUEUE MANAGEMENT
// ============================================================================

void UCombatComponent::QueueAction(const FQueuedInputAction& InputAction, UAttackData* AttackData)
{
	// Only queue press events (releases handled separately)
	if (InputAction.EventType != EInputEventType::Press)
	{
		return;
	}

	// Determine execution mode
	EActionExecutionMode ExecMode = DetermineExecutionMode(InputAction);

	// Get attack data if not provided
	if (!AttackData)
	{
		AttackData = GetAttackForInput(InputAction.InputType);
	}

	// CRITICAL: If we couldn't resolve any attack, bail out gracefully
	// This can happen if CombatSettings is not assigned to the character
	if (!AttackData)
	{
		UE_LOG(LogCombat, Warning, TEXT("[QUEUE] Cannot queue action: No attack resolved. "
		                                "Check that CombatSettings is assigned to the character."));
		return;
	}

	// Create queue entry
	FActionQueueEntry Entry(InputAction, AttackData, ExecMode);
	Entry.Priority = CalculatePriority(Entry);

	// PHASE 9: Set TargetPhase for event-driven execution
	// Immediate: Execute synchronously (no phase wait)
	// Queued: Execute on Recovery phase transition (when Active ends)
	Entry.TargetPhase = (ExecMode == EActionExecutionMode::Immediate)
		? EAttackPhase::None      // Synchronous execution
		: EAttackPhase::Recovery; // Execute when Active phase ends

	// Immediate mode: Execute synchronously (right now)
	if (ExecMode == EActionExecutionMode::Immediate)
	{
		// COMBO-AWARE QUEUE MANAGEMENT:
		// Check if the executing action has combo branches (NextComboAttack, HeavyComboAttack, etc.)
		// If YES: Preserve ONLY valid combo inputs (enables alternating light/heavy chains)
		// If NO: Clear all pending (combo chain ended, starting fresh)

		UAttackData* ExecutingAttack = Entry.AttackData;
		bool bHasComboBranches = ExecutingAttack &&
			(ExecutingAttack->NextComboAttack != nullptr ||
			 ExecutingAttack->HeavyComboAttack != nullptr ||
			 ExecutingAttack->DirectionalFollowUps.Num() > 0 ||
			 ExecutingAttack->HeavyDirectionalFollowUps.Num() > 0);

		if (bHasComboBranches && ActionQueue.Num() > 0)
		{
			// This attack has combo branches - preserve ONLY the FIRST valid combo of each type
			// This prevents button mashing from executing entire combo chains (max 1 Light + 1 Heavy queued)
			TArray<FActionQueueEntry> ValidCombos;
			bool bHasQueuedLight = false;
			bool bHasQueuedHeavy = false;
			int32 CancelledCount = 0;

			for (FActionQueueEntry& QueuedEntry : ActionQueue)
			{
				if (!QueuedEntry.IsPending())
				{
					ValidCombos.Add(QueuedEntry); // Keep non-pending
					continue;
				}

				// Check if this queued action is a valid combo from executing attack
				bool bIsValidCombo = false;
				bool bAlreadyQueued = false;

				if (QueuedEntry.InputAction.InputType == EInputType::LightAttack)
				{
					bIsValidCombo = (ExecutingAttack->NextComboAttack != nullptr);
					bAlreadyQueued = bHasQueuedLight;
					if (bIsValidCombo && !bAlreadyQueued)
					{
						bHasQueuedLight = true; // Mark that we've queued a Light
					}
				}
				else if (QueuedEntry.InputAction.InputType == EInputType::HeavyAttack)
				{
					bIsValidCombo = (ExecutingAttack->HeavyComboAttack != nullptr);
					bAlreadyQueued = bHasQueuedHeavy;
					if (bIsValidCombo && !bAlreadyQueued)
					{
						bHasQueuedHeavy = true; // Mark that we've queued a Heavy
					}
				}

				if (bIsValidCombo && !bAlreadyQueued)
				{
					ValidCombos.Add(QueuedEntry); // Keep FIRST valid combo of this type
				}
				else
				{
					// Cancel: either invalid combo OR duplicate input (spam prevention)
					QueuedEntry.State = EActionState::Cancelled;
					QueueStats.ActionsCancelled++;
					CancelledCount++;
				}
			}

			// Replace queue with only valid combos (max 1 Light + 1 Heavy)
			ActionQueue = ValidCombos;

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[QUEUE] Combo-aware clear: Preserved %d valid combos (anti-spam), cancelled %d"),
					ValidCombos.Num(), CancelledCount);
			}
		}
		else if (ActionQueue.Num() > 0)
		{
			// No combo branches - clear all pending (starting fresh attack chain)
			int32 ClearedCount = 0;
			for (FActionQueueEntry& QueuedEntry : ActionQueue)
			{
				if (QueuedEntry.IsPending())
				{
					QueuedEntry.State = EActionState::Cancelled;
					QueueStats.ActionsCancelled++;
					ClearedCount++;
				}
			}

			ActionQueue.Empty();

			if (GetDebugDraw() && ClearedCount > 0)
			{
				UE_LOG(LogCombat, Warning, TEXT("[QUEUE] Cleared %d pending actions (no combo branches - chain ended)"), ClearedCount);
			}
		}

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[QUEUE] Executing IMMEDIATE action: Type=%s"),
				*UEnum::GetValueAsString(InputAction.InputType));
		}

		// Execute immediately
		if (ExecuteAction(Entry))
		{
			QueueStats.ActionsExecuted++;
			QueueStats.ImmediateExecutions++;

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[QUEUE] Immediate execution SUCCESS"));
			}
		}
		else
		{
			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Warning, TEXT("[QUEUE] Immediate execution FAILED"));
			}
		}

		return; // Don't add to queue
	}

	// Queued mode: Schedule for later execution at Active-end
	Entry.ScheduledTime = GetExecutionCheckpoint(Entry);

	// Add to queue
	ActionQueue.Add(Entry);

	// Sort by scheduled time
	SortQueueByTime();

	if ( GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[QUEUE] Added queued action: Type=%s, Mode=%s, Scheduled=%.2f, Priority=%d"),
			*UEnum::GetValueAsString(InputAction.InputType),
			*UEnum::GetValueAsString(ExecMode),
			Entry.ScheduledTime,
			Entry.Priority);
	}
}

void UCombatComponent::ProcessQueuedActions(EAttackPhase TargetPhase)
{
	// PHASE 9: EVENT-DRIVEN QUEUE PROCESSING (NOT tick-based!)
	// Execute actions that are waiting for this phase transition
	// This completely replaces montage-time-based polling

	if (ActionQueue.Num() == 0)
	{
		return;
	}

	int32 ExecutedCount = 0;

	// Process actions targeting this phase (FIFO order maintained)
	for (int32 i = ActionQueue.Num() - 1; i >= 0; --i)
	{
		FActionQueueEntry& Entry = ActionQueue[i];

		if (!Entry.IsPending())
		{
			continue; // Skip non-pending actions
		}

		// Check if this action targets the current phase transition
		bool bShouldExecute = (Entry.TargetPhase == TargetPhase);

		if (bShouldExecute)
		{
			// Execute action
			if (ExecuteAction(Entry))
			{
				Entry.State = EActionState::Completed;
				QueueStats.ActionsExecuted++;

				if (Entry.ExecutionMode == EActionExecutionMode::Queued)
				{
					QueueStats.QueuedExecutions++;
				}

				ExecutedCount++;

				if (GetDebugDraw())
				{
					UE_LOG(LogCombat, Log, TEXT("[EVENT-DRIVEN] Executed action on phase %s (TargetPhase: %s)"),
						*UEnum::GetValueAsString(TargetPhase),
						*UEnum::GetValueAsString(Entry.TargetPhase));
				}

				// Remove from queue after successful execution
				ActionQueue.RemoveAt(i);
			}
			else
			{
				// Execution failed - mark as cancelled
				Entry.State = EActionState::Cancelled;
				QueueStats.ActionsCancelled++;

				if (GetDebugDraw())
				{
					UE_LOG(LogCombat, Warning, TEXT("[EVENT-DRIVEN] Action execution failed on phase %s, cancelled"),
						*UEnum::GetValueAsString(TargetPhase));
				}

				ActionQueue.RemoveAt(i);
			}
		}
	}

	if (GetDebugDraw() && ExecutedCount > 0)
	{
		UE_LOG(LogCombat, Log, TEXT("[EVENT-DRIVEN] Processed %d queued actions on phase %s"),
			ExecutedCount, *UEnum::GetValueAsString(TargetPhase));
	}
}

void UCombatComponent::ProcessQueue(float CurrentMontageTime)
{
	// DEPRECATED: Tick-based queue processing
	// Replaced by event-driven ProcessQueuedActions(TargetPhase) in Phase 9
	// Keeping this stub for potential backward compatibility or debugging

	if (ActionQueue.Num() == 0)
	{
		return;
	}

	// OLD TICK-BASED LOGIC (no longer used):
	// Process actions that have reached their scheduled time
	for (int32 i = ActionQueue.Num() - 1; i >= 0; --i)
	{
		FActionQueueEntry& Entry = ActionQueue[i];

		if (!Entry.IsPending())
		{
			continue;
		}

		// Check if action is ready to execute
		bool bReadyToExecute = false;

		if (Entry.ScheduledTime < 0.0f)
		{
			// Sentinel value: Find Active-end checkpoint dynamically
			for (const FTimerCheckpoint& Checkpoint : Checkpoints)
			{
				if (Checkpoint.WindowType == EActionWindowType::Combo &&
					Checkpoint.bActive &&
					Checkpoint.Duration == 0.0f &&
					CurrentMontageTime >= Checkpoint.MontageTime)
				{
					bReadyToExecute = true;
					Entry.ScheduledTime = Checkpoint.MontageTime; // Update for logging
					break;
				}
			}
		}
		else if (CurrentMontageTime >= Entry.ScheduledTime)
		{
			// Normal scheduled time reached
			bReadyToExecute = true;
		}

		if (bReadyToExecute)
		{
			// Execute action
			if (ExecuteAction(Entry))
			{
				Entry.State = EActionState::Completed;
				QueueStats.ActionsExecuted++;

				if (Entry.ExecutionMode == EActionExecutionMode::Queued)
				{
					QueueStats.QueuedExecutions++;
				}

				if (GetDebugDraw())
				{
					UE_LOG(LogCombat, Log, TEXT("[QUEUE] Executed action at %.2f (scheduled: %.2f)"),
						CurrentMontageTime, Entry.ScheduledTime);
				}

				// Remove from queue after successful execution
				ActionQueue.RemoveAt(i);
			}
			else
			{
				// Execution failed - keep in queue for potential retry
				// Mark as cancelled if it keeps failing
				if (GetDebugDraw())
				{
					UE_LOG(LogCombat, Warning, TEXT("[QUEUE] Action execution failed at %.2f, keeping in queue"),
						CurrentMontageTime);
				}
			}
		}
	}
}

bool UCombatComponent::ExecuteAction(FActionQueueEntry& Action)
{
	if (!Action.AttackData)
	{
		return false;
	}

	Action.State = EActionState::Executing;

	// Event-driven execution - plays montage directly
	bool bSuccess = false;

	switch (Action.InputAction.InputType)
	{
		case EInputType::LightAttack:
		case EInputType::HeavyAttack:
		{
			// FINISHER CHECK: Before normal attack, try to execute finisher on vulnerable target
			if (TryExecuteFinisher(Action.AttackData))
			{
				// Finisher was executed - don't play normal attack
				bSuccess = true;
				break;
			}

			// Play normal attack montage
			bSuccess = PlayAttackMontage(Action.AttackData);

			// If successful, discover checkpoints for the new montage
			if (bSuccess && Action.AttackData->AttackMontage)
			{
				// Transition to Windup phase (event-driven phase management)
				SetPhase(EAttackPhase::Windup);

				DiscoverCheckpoints(Action.AttackData->AttackMontage);

				// Track current attack for combo progression
				CurrentAttackData = Action.AttackData;
				CurrentAttackInputType = Action.InputAction.InputType;

				// CRITICAL FIX: Reset hold state for new attack (clears bActivatedThisAttack)
				HoldState.Reset();

				// MOTION WARP: Setup warp based on context (target or direction)
				SetupAttackWarp(Action.AttackData);

				// Broadcast attack started event
				bool bIsCombo = (CurrentPhase == EAttackPhase::Recovery || CurrentPhase == EAttackPhase::Active);
				OnAttackStarted.Broadcast(Action.AttackData, Action.InputAction.InputType, bIsCombo);

				if (GetDebugDraw())
				{
					FString SectionName = Action.AttackData->MontageSection.IsNone() ?
						TEXT("Default") : Action.AttackData->MontageSection.ToString();

					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] ═══════════════════════════════════════"));
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] Attack Data: %s"), *Action.AttackData->GetName());
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] Montage: %s"), *Action.AttackData->AttackMontage->GetName());
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] Section: %s"), *SectionName);
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] Input Type: %s"), *UEnum::GetValueAsString(CurrentAttackInputType));
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] Is Combo: %s"), bIsCombo ? TEXT("YES") : TEXT("NO"));
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] Checkpoints Discovered: %d"), Checkpoints.Num());
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] ═══════════════════════════════════════"));
				}
			}
			break;
		}

		case EInputType::Evade:
			// Handle evade
			break;

		case EInputType::Block:
			// Handle block
			break;

		default:
			break;
	}

	return bSuccess;
}

bool UCombatComponent::TryExecuteFinisher(UAttackData* AttackData)
{
	// Validate attack has finisher data
	if (!AttackData || !AttackData->FinisherData)
	{
		return false;
	}

	// Get owner character (use the cached member, not a local variable)
	ABaseCombatCharacter* AttackerCharacter = GetOwnerCharacter();
	if (!AttackerCharacter)
	{
		return false;
	}

	// Get targeting component to find current target
	UTargetingComponent* TargetingComp = AttackerCharacter->GetTargetingComponent();
	if (!TargetingComp)
	{
		return false;
	}

	// Get current target
	AActor* TargetActor = TargetingComp->GetCurrentTarget();
	if (!TargetActor)
	{
		return false;
	}

	// ========================================================================
	// FINISHER DISTANCE VALIDATION (Gap 16.2)
	// ========================================================================
	// Verify target is within range before executing finisher.
	// Prevents finishers on distant targets that would look wrong.

	const float DistanceToTarget = FVector::Dist(
		AttackerCharacter->GetActorLocation(),
		TargetActor->GetActorLocation()
	);

	// Get max finisher range from targeting settings (or use fallback)
	float MaxFinisherRange = 500.0f;  // Fallback value
	if (const UTargetingSettings* TargetingSettings = TargetingComp->GetEffectiveSettings())
	{
		// Use soft aim range as finisher range (close-range interaction)
		MaxFinisherRange = TargetingSettings->SoftAimRange;
	}

	if (DistanceToTarget > MaxFinisherRange)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[FINISHER] Target %s too far: %.1f > %.1f (max range)"),
				*TargetActor->GetName(), DistanceToTarget, MaxFinisherRange);
		}
		return false;
	}

	// Get target's hit reaction component
	UHitReactionComponent* TargetHitReaction = TargetActor->FindComponentByClass<UHitReactionComponent>();
	if (!TargetHitReaction)
	{
		return false;
	}

	// Check if target is vulnerable to finisher
	if (!TargetHitReaction->IsVulnerableToFinisher())
	{
		return false;
	}

	// Get finisher trigger reason for logging/context
	EFinisherTriggerReason TriggerReason = TargetHitReaction->GetFinisherTriggerReason();

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[FINISHER] ═══════════════════════════════════════"));
		UE_LOG(LogCombat, Log, TEXT("[FINISHER] Executing finisher: %s"), *AttackData->FinisherData->GetDisplayName());
		UE_LOG(LogCombat, Log, TEXT("[FINISHER] Target: %s"), *TargetActor->GetName());
		UE_LOG(LogCombat, Log, TEXT("[FINISHER] Trigger Reason: %s"), *UEnum::GetValueAsString(TriggerReason));
	}

	// Mark target as finisher target (prevents stacking)
	TargetHitReaction->SetFinisherTarget(true);

	// Get target's combat component for partner tracking
	UCombatComponent* TargetCombatComp = TargetActor->FindComponentByClass<UCombatComponent>();

	// Add each other as paired animation partners
	AddPairedPartner(TargetActor);
	if (TargetCombatComp)
	{
		TargetCombatComp->AddPairedPartner(GetOwner());
	}

	// Begin paired animation effects (slow-mo, etc.)
	BeginPairedAnimation(AttackData->FinisherData, EPairedReactionType::Finisher, true);

	// Play attacker montage
	ACharacter* AttackerChar = Cast<ACharacter>(AttackerCharacter);
	if (AttackerChar && AttackData->FinisherData->AttackerMontage)
	{
		UAnimInstance* AttackerAnimInstance = AttackerChar->GetMesh()->GetAnimInstance();
		if (AttackerAnimInstance)
		{
			AttackerAnimInstance->Montage_Play(
				AttackData->FinisherData->AttackerMontage,
				1.0f,
				EMontagePlayReturnType::MontageLength,
				0.0f,
				true
			);

			// Set up attacker paired warp (continuous tracking toward victim)
			// Uses TargetingComponent's paired warp system for:
			// - Continuous position updates each frame (tracks moving victim)
			// - Automatic terrain adjustment
			// - Partner registration for collision ignore
			// - Symmetric with victim's SetupVictimWarp
			TargetingComp->SetupAttackerPairedWarp(TargetActor, AttackData->FinisherData->AttackerWarpConfig);

			// Set combat state to Finishing
			SetPhase(EAttackPhase::Active);

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[FINISHER] Attacker montage playing: %s"),
					*AttackData->FinisherData->AttackerMontage->GetName());
			}
		}
	}

	// Play victim montage
	ACharacter* VictimChar = Cast<ACharacter>(TargetActor);
	if (VictimChar && AttackData->FinisherData->VictimMontage)
	{
		UAnimInstance* VictimAnimInstance = VictimChar->GetMesh()->GetAnimInstance();
		if (VictimAnimInstance)
		{
			// Calculate victim start delay
			float StartPosition = FMath::Max(0.0f, -AttackData->FinisherData->VictimStartOffset);

			VictimAnimInstance->Montage_Play(
				AttackData->FinisherData->VictimMontage,
				1.0f,
				EMontagePlayReturnType::MontageLength,
				StartPosition,
				true
			);

			// Set up victim warp to attacker
			if (UTargetingComponent* VictimTargeting = TargetActor->FindComponentByClass<UTargetingComponent>())
			{
				VictimTargeting->SetupVictimWarp(GetOwner(), AttackData->FinisherData->VictimWarpConfig);
			}

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[FINISHER] Victim montage playing: %s (StartPos: %.2f)"),
					*AttackData->FinisherData->VictimMontage->GetName(), StartPosition);
			}
		}
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[FINISHER] ═══════════════════════════════════════"));
	}

	return true;
}

bool UCombatComponent::PlayAttackMontage(UAttackData* AttackData)
{
	if (!AttackData || !AttackData->AttackMontage)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Warning, TEXT("[MONTAGE] Failed - Invalid AttackData or Montage"));
		}
		return false;
	}

	ABaseCombatCharacter* Character = GetOwnerCharacter();
	if (!Character || !Character->GetMesh())
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Warning, TEXT("[MONTAGE] Failed - No character or mesh"));
		}
		return false;
	}

	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Warning, TEXT("[MONTAGE] Failed - No AnimInstance"));
		}
		return false;
	}

	// PHASE 1 FIX: Clear any active hold state when starting new attack
	// This prevents hold state leaks (easing, movement locks) from previous attack
	ClearHoldState();

	// CRITICAL: Clear directional input buffer for fresh attack
	// Each attack starts with clean directional state (no stale directions from previous attacks)
	DirectionalInputBuffer.Reset();

	// COMBO BLENDING: Determine blend times for smooth transitions
	// - Blend-out: Current attack's ComboBlendOutTime (0 = instant for first attack)
	// - Blend-in: New attack's ComboBlendInTime (configurable per attack)

	float BlendOutTime = 0.0f;  // Default: instant (first attack or no previous attack)
	float BlendInTime = AttackData->ComboBlendInTime;  // New attack's blend-in time

	// CRITICAL FIX: Detect if we're still in a blend transition (rapid input during blend)
	// Check both the flag AND the time-based tracker for robustness
	const float CurrentWorldTime = GetWorld()->GetTimeSeconds();
	const bool bStillInBlendTransition = bInComboBlend || (CurrentWorldTime < BlendTransitionEndTime);

	if (bStillInBlendTransition)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Warning, TEXT("[BLEND] Rapid input during blend (EndTime=%.2f, Now=%.2f)! Forcing instant cleanup."),
				BlendTransitionEndTime, CurrentWorldTime);
		}

		// Force instant stop of ALL montages to clean up the blend mess
		// This fixes the "half-blended loop" issue when mashing attack during blend-out
		AnimInstance->StopAllMontages(0.0f);
		bInComboBlend = false;
		BlendTransitionEndTime = 0.0f;

		// Use instant blend for this attack since we just force-stopped
		BlendOutTime = 0.0f;
		BlendInTime = 0.0f;
	}
	else if (CurrentAttackData)
	{
		// Normal case: We have a previous attack - use its blend-out time for smooth transition
		BlendOutTime = CurrentAttackData->ComboBlendOutTime;

		if (GetDebugDraw() && (BlendOutTime > 0.0f || BlendInTime > 0.0f))
		{
			UE_LOG(LogCombat, Log, TEXT("[BLEND] Combo transition: %s (out=%.2fs) → %s (in=%.2fs)"),
				*CurrentAttackData->GetName(), BlendOutTime,
				*AttackData->GetName(), BlendInTime);
		}
	}

	// BLEND-OUT: Stop current montage if blending is requested
	UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();
	if (CurrentMontage && BlendOutTime > 0.0f)
	{
		// Mark that we're in combo blend transition - prevents premature phase reset
		bInComboBlend = true;

		// Track when the blend will complete (max of blend-out and blend-in)
		// This allows time-based detection of rapid inputs during blend
		const float BlendDuration = FMath::Max(BlendOutTime, BlendInTime);
		BlendTransitionEndTime = GetWorld()->GetTimeSeconds() + BlendDuration;

		AnimInstance->Montage_Stop(BlendOutTime, CurrentMontage);

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[BLEND] Combo blend started - EndTime=%.2f (Duration=%.2fs)"),
				BlendTransitionEndTime, BlendDuration);
		}
	}

	// NOTE: bCurrentAttackIsDirectionalFollowUp flag is managed in GetAttackForInput()
	// It's set during resolution based on whether the attack was found in DirectionalFollowUps map

	// BLEND-IN: Play new montage with blend settings
	// Note: OnMontageEnded delegate already bound in BeginPlay() for event-driven phase management
	const float PlayRate = 1.0f;
	const float StartPosition = 0.0f;

	if (BlendInTime > 0.0f)
	{
		// Play with custom blend-in
		FAlphaBlendArgs BlendIn(BlendInTime);
		AnimInstance->Montage_PlayWithBlendSettings(
			AttackData->AttackMontage,
			BlendIn,
			PlayRate,
			EMontagePlayReturnType::MontageLength,
			StartPosition,
			false  // Don't stop all montages
		);
	}
	else
	{
		// Play with default blend (instant)
		AnimInstance->Montage_Play(AttackData->AttackMontage, PlayRate, EMontagePlayReturnType::MontageLength, StartPosition);
	}

	// Clear blend flag - new montage has started playing
	// Note: BlendTransitionEndTime is NOT cleared here - it provides time-based protection
	// against rapid inputs that may come in before the blend visually completes
	if (bInComboBlend)
	{
		bInComboBlend = false;

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[BLEND] New montage started - bInComboBlend=false (time-based protection until %.2f)"),
				BlendTransitionEndTime);
		}
	}

	// Handle montage sections if specified
	if (!AttackData->MontageSection.IsNone())
	{
		AnimInstance->Montage_JumpToSection(AttackData->MontageSection, AttackData->AttackMontage);

		// Prevent auto-advance to next section if bUseSectionOnly is true
		if (AttackData->bUseSectionOnly)
		{
			AnimInstance->Montage_SetNextSection(AttackData->MontageSection, NAME_None, AttackData->AttackMontage);

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[MONTAGE] Section-only mode: %s (no auto-advance)"),
					*AttackData->MontageSection.ToString());
			}
		}
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[MONTAGE] Playing: %s | Section: %s | Delegate bound"),
			*AttackData->AttackMontage->GetName(),
			*AttackData->MontageSection.ToString());
	}

	return true;
}

void UCombatComponent::ClearQueue(bool bCancelCurrent)
{
	if (bCancelCurrent)
	{
		// Cancel all including executing
		for (FActionQueueEntry& Entry : ActionQueue)
		{
			if (Entry.State != EActionState::Completed)
			{
				Entry.State = EActionState::Cancelled;
				QueueStats.ActionsCancelled++;
			}
		}
	}
	else
	{
		// Cancel only pending
		for (FActionQueueEntry& Entry : ActionQueue)
		{
			if (Entry.IsPending())
			{
				Entry.State = EActionState::Cancelled;
				QueueStats.ActionsCancelled++;
			}
		}
	}

	ActionQueue.Empty();

	// Reset combo state when queue is cleared
	CurrentAttackData = nullptr;
	CurrentAttackInputType = EInputType::None;

	// CRITICAL: Clear directional input buffer on queue clear
	// Prevents stale directional input from persisting across combat resets
	DirectionalInputBuffer.Reset();

	if ( GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[QUEUE] Cleared (CancelCurrent=%s) - Combo state and directional buffer reset"), bCancelCurrent ? TEXT("YES") : TEXT("NO"));
	}
}

void UCombatComponent::CancelActionsWithPriority(int32 MinPriority)
{
	for (int32 i = ActionQueue.Num() - 1; i >= 0; --i)
	{
		FActionQueueEntry& Entry = ActionQueue[i];

		if (Entry.IsPending() && Entry.Priority < MinPriority)
		{
			Entry.State = EActionState::Cancelled;
			QueueStats.ActionsCancelled++;
			ActionQueue.RemoveAt(i);

			if ( GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[QUEUE] Cancelled action (Priority %d < %d)"),
					Entry.Priority, MinPriority);
			}
		}
	}
}

// ============================================================================
// TIMER CHECKPOINT SYSTEM
// ============================================================================

void UCombatComponent::DiscoverCheckpoints(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return;
	}

	// Clear existing checkpoints
	Checkpoints.Empty();

	// Use MontageUtilityLibrary to discover checkpoints from AnimNotifyStates
	int32 NumDiscovered = UMontageUtilityLibrary::DiscoverCheckpoints(Montage, Checkpoints);

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[CHECKPOINTS] Discovered %d checkpoints from montage: %s"),
			NumDiscovered,
			*Montage->GetName());

		// Log discovered checkpoints
		UMontageUtilityLibrary::LogCheckpoints(Checkpoints, TEXT("DISCOVERY"));
	}

	// Update combo window state if any combo checkpoints were found
	for (const FTimerCheckpoint& Checkpoint : Checkpoints)
	{
		if (Checkpoint.WindowType == EActionWindowType::Combo)
		{
			bComboWindowActive = true;
			ComboWindowStart = Checkpoint.MontageTime;
			ComboWindowDuration = Checkpoint.Duration;
			break;
		}
	}
}

void UCombatComponent::RegisterCheckpoint(EActionWindowType WindowType, float StartTime, float Duration)
{
	FTimerCheckpoint Checkpoint(WindowType, StartTime, Duration);
	Checkpoint.bActive = true;

	Checkpoints.Add(Checkpoint);

	// Update combo window state if this is a combo checkpoint
	if (WindowType == EActionWindowType::Combo)
	{
		bComboWindowActive = true;
		ComboWindowStart = StartTime;
		ComboWindowDuration = Duration;
	}

	if ( GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[CHECKPOINTS] Registered: Type=%s, Start=%.2f, Duration=%.2f"),
			*UEnum::GetValueAsString(WindowType),
			StartTime,
			Duration);
	}
}

bool UCombatComponent::HasReachedCheckpoint(const FTimerCheckpoint& Checkpoint, float CurrentTime) const
{
	return Checkpoint.bActive && CurrentTime >= Checkpoint.MontageTime;
}

TArray<FTimerCheckpoint> UCombatComponent::GetActiveWindows(float CurrentTime) const
{
	TArray<FTimerCheckpoint> ActiveWindows;
	for (const FTimerCheckpoint& Checkpoint : Checkpoints)
	{
		if (Checkpoint.bActive &&
		    CurrentTime >= Checkpoint.MontageTime &&
		    CurrentTime <= (Checkpoint.MontageTime + Checkpoint.Duration))
		{
			ActiveWindows.Add(Checkpoint);
		}
	}
	return ActiveWindows;
}

float UCombatComponent::GetExecutionCheckpoint(const FActionQueueEntry& Action) const
{
	if (Action.ExecutionMode == EActionExecutionMode::Immediate)
	{
		return 0.0f; // Execute now
	}

	// Queued mode: Execute at Active phase end (Active → Recovery transition)
	// Find the zero-duration Combo checkpoint (registered when transitioning Active → Recovery)
	// This is the phase transition point, NOT the deprecated combo window
	for (const FTimerCheckpoint& Checkpoint : Checkpoints)
	{
		if (Checkpoint.WindowType == EActionWindowType::Combo &&
		    Checkpoint.bActive &&
		    Checkpoint.Duration == 0.0f) // Zero-duration = Active-end marker (phase transition)
		{
			// Return the checkpoint time (Active phase end)
			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[CHECKPOINT] Found Active-end checkpoint at %.2f for queued execution"),
					Checkpoint.MontageTime);
			}

			return Checkpoint.MontageTime;
		}
	}

	// Checkpoint not found yet - use sentinel value to indicate "execute at Active-end"
	// The checkpoint will be created when Active→Recovery transition happens via OnPhaseTransition
	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[CHECKPOINT] Active-end checkpoint not found yet, will execute when created"));
	}

	/*TODO: Consider warning if no Active-end checkpoint found after montage ends*/
	return -1.0f; // Sentinel: Execute at Active-end (checkpoint will be created later)
}

// ============================================================================
// HOLD SYSTEM
// ============================================================================

void UCombatComponent::OnHoldWindowStart(EInputType InputType)
{
	// EVENT-DRIVEN HOLD DETECTION:
	// AnimNotify fires at hold window start, we check if button is STILL pressed
	// This replaces tick-based CheckHoldActivation with event-driven pattern

	if (!CurrentAttackData || HoldState.bActivatedThisAttack)
	{
		return;
	}

	// Check if the specified input is currently pressed (via HeldInputs map)
	const float* PressTime = HeldInputs.Find(InputType);
	if (!PressTime)
	{
		// Button not held - normal combo flow
		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[HOLD] Window start, but button not held: %s"),
				*UEnum::GetValueAsString(InputType));
		}
		return;
	}

	// Button is held - activate hold behavior
	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[HOLD] Button held at window start: %s, activating hold"),
			*UEnum::GetValueAsString(InputType));
	}

	// ARCHITECTURAL FIX: Enable directional input context
	// From this point until hold clears, movement stick = directional attack input
	// This allows direction to be sampled at button release for directional follow-ups
	SetInputContext(EInputContext::DirectionalInput);

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[HOLD] Directional input window OPENED (awaiting release)"));
	}

	// Determine hold behavior based on attack type
	if (CurrentAttackData->AttackType == EAttackType::Heavy)
	{
		// HEAVY ATTACK HOLD: Jump to and loop charge section
		ACharacter* Character = Cast<ACharacter>(GetOwner());
		if (Character && CurrentAttackData->ChargeLoopSection != NAME_None)
		{
			// STEP 1: Jump to the charge section WITH BLEND (smooths transition from attack → charge loop)
			bool bJumped = UMontageUtilityLibrary::JumpToSectionWithBlend(
				Character,
				CurrentAttackData->ChargeLoopSection,
				CurrentAttackData->ChargeLoopBlendTime  // Blends initial attack anim → charge loop
			);

			if (!bJumped)
			{
				if (GetDebugDraw())
				{
					UE_LOG(LogCombat, Warning, TEXT("[HOLD] Failed to jump to charge section: %s"),
						*CurrentAttackData->ChargeLoopSection.ToString());
				}
				return;
			}

			// STEP 2: Set up the loop (section jumps back to itself)
			bool bLooped = UMontageUtilityLibrary::LoopMontageSection(Character, CurrentAttackData->ChargeLoopSection);

			if (bLooped)
			{
				// STEP 3: Activate hold state (no playrate change for heavy attacks - loops at normal speed)
				ActivateHold(InputType, 1.0f);

				// CRITICAL FIX: Mark Heavy hold as completed immediately after activation
				// This allows IsHoldCompleted() to return true for Heavy attacks
				// Without this, directional follow-ups never work for Heavy attacks
				HoldState.MarkHoldCompleted();

				if (GetDebugDraw())
				{
					UE_LOG(LogCombat, Log, TEXT("[HOLD] Heavy attack charge loop started and marked completed: jumped to '%s' and looping"),
						*CurrentAttackData->ChargeLoopSection.ToString());
				}
			}
			else if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Warning, TEXT("[HOLD] Failed to loop charge section: %s"),
					*CurrentAttackData->ChargeLoopSection.ToString());
			}
		}
		else if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Warning, TEXT("[HOLD] Heavy attack has no ChargeLoopSection defined"));
		}
	}
	else if (CurrentAttackData->AttackType == EAttackType::Light)
	{
		// LIGHT ATTACK HOLD: Begin EASE-IN slowdown (bidirectional easing system)
		// Smoothly transition from normal speed to hold slowdown using UE Timer System (NOT tick!)

		// Activate hold state (marks hold as active)
		HoldState.Activate(InputType, GetWorld()->GetTimeSeconds(), 1.0f);

		// Initialize EASE-IN transition state (1.0 → HoldTargetPlayRate)
		HoldState.bIsEasing = true;
		HoldState.bIsEasingOut = false; // EASE-IN direction
		HoldState.EaseStartTime = GetWorld()->GetTimeSeconds();
		HoldState.EaseStartPlayRate = 1.0f; // Current playrate (normal speed)

		// Start timer for ease updates (60 Hz for smooth transitions)
		// Timer calls OnEaseTimerTick() repeatedly until ease completes or is cancelled
		float TimerInterval = 1.0f / 60.0f; // 60 Hz update rate
		GetWorld()->GetTimerManager().SetTimer(
			EaseTimerHandle,
			this,
			&UCombatComponent::OnEaseTimerTick,
			TimerInterval,
			true // Loop
		);

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[HOLD TIMER] Light attack EASE-IN started (1.0 → %.2f over %.2fs using %s @ 60Hz)"),
				CurrentAttackData->HoldTargetPlayRate,
				CurrentAttackData->HoldEaseInDuration,
				*UEnum::GetValueAsString(CurrentAttackData->HoldEaseInType));
		}
	}
}

void UCombatComponent::ActivateHold(EInputType InputType, float PlayRate)
{
	HoldState.Activate(InputType, GetWorld()->GetTimeSeconds(), PlayRate);

	// Apply playrate to montage using utility library
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UMontageUtilityLibrary::SetMontagePlayRate(Character, PlayRate);

	if ( GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[HOLD] Activated: Input=%s, PlayRate=%.2f"),
			*UEnum::GetValueAsString(InputType),
			PlayRate);
	}
}

void UCombatComponent::DeactivateHold()
{
	if (!HoldState.IsHolding() || !CurrentAttackData)
	{
		return;
	}

	// Clear any existing ease timer (ease-in may still be running)
	if (GetWorld() && EaseTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(EaseTimerHandle);
	}

	// HEAVY ATTACK: Jump to release section (no easing)
	if (CurrentAttackData->AttackType == EAttackType::Heavy)
	{
		ACharacter* Character = Cast<ACharacter>(GetOwner());

		// Jump to the configured release section WITH BLEND (smooths transition from charge loop → release attack)
		if (Character && CurrentAttackData->ChargeReleaseSection != NAME_None)
		{
			bool bJumped = UMontageUtilityLibrary::JumpToSectionWithBlend(
				Character,
				CurrentAttackData->ChargeReleaseSection,
				CurrentAttackData->ChargeReleaseBlendTime  // Blends charge loop → release attack
			);

			if (GetDebugDraw())
			{
				if (bJumped)
				{
					UE_LOG(LogCombat, Log, TEXT("[HOLD] Heavy attack released: jumping to release section '%s'"),
						*CurrentAttackData->ChargeReleaseSection.ToString());
				}
				else
				{
					UE_LOG(LogCombat, Warning, TEXT("[HOLD] Failed to jump to release section '%s'"),
						*CurrentAttackData->ChargeReleaseSection.ToString());
				}
			}
		}
		else
		{
			// FALLBACK: No release section configured - try directional follow-up, otherwise blend to idle
			UAttackData* FollowUpAttack = nullptr;

			// Check for directional follow-up based on held direction
			if (HoldState.CurrentHold.Direction != EAttackDirection::None)
			{
				TObjectPtr<UAttackData>* DirectionalAttack = CurrentAttackData->DirectionalFollowUps.Find(HoldState.CurrentHold.Direction);
				if (DirectionalAttack && DirectionalAttack->Get())
				{
					FollowUpAttack = DirectionalAttack->Get();

					if (GetDebugDraw())
					{
						UE_LOG(LogCombat, Log, TEXT("[HOLD] Heavy attack has no ChargeReleaseSection - queueing directional follow-up: %s (direction=%s)"),
							*FollowUpAttack->GetName(),
							*UDebugUtils::FormatAttackDirectionDebug(HoldState.CurrentHold.Direction));
					}

					// Queue directional follow-up for immediate execution
					FQueuedInputAction FollowUpInput(
						CurrentAttackInputType,           // Same input type as current attack
						EInputEventType::Press,           // Treat as press event
						GetWorld()->GetTimeSeconds(),     // Current time
						false                             // Not in combo window
					);
					QueueAction(FollowUpInput, FollowUpAttack);
				}
			}

			// If no directional follow-up found, blend to idle
			if (!FollowUpAttack && Character)
			{
				UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
				if (AnimInstance)
				{
					UAnimMontage* CurrentMontage = UMontageUtilityLibrary::GetCurrentMontage(Character);
					if (CurrentMontage)
					{
						// Blend out using ChargeReleaseBlendTime (reuse same blend duration)
						AnimInstance->Montage_Stop(CurrentAttackData->ChargeReleaseBlendTime, CurrentMontage);

						if (GetDebugDraw())
						{
							UE_LOG(LogCombat, Log, TEXT("[HOLD] Heavy attack has no ChargeReleaseSection or directional follow-up - blending to idle (%.2fs)"),
								CurrentAttackData->ChargeReleaseBlendTime);
						}

						// CRITICAL: Clear attack state immediately (no follow-up attack)
						// OnMontageEnded will fire after blend completes, but we need to reset NOW
						CurrentAttackData = nullptr;
						CurrentAttackInputType = EInputType::None;
						SetPhase(EAttackPhase::None);
						Checkpoints.Empty();
						ActionQueue.Empty(); // Discard any queued actions - returning to idle

						if (GetDebugDraw())
						{
							UE_LOG(LogCombat, Log, TEXT("[HOLD] Heavy attack state cleared - ready for new input"));
						}
					}
				}
			}
		}

		// Deactivate hold state immediately (no easing for heavy attacks)
		HoldState.Deactivate();
		return;
	}

	// LIGHT ATTACK: Begin EASE-OUT transition (HoldTargetPlayRate → 1.0)
	// Reuse the same timer system but reverse the transition

	// CRITICAL FIX: Query ACTUAL montage playrate instead of HoldState.CurrentPlayRate
	// If button released during ease-in, HoldState may not match AnimInstance's actual playrate
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	float CurrentPlayRate = UMontageUtilityLibrary::GetMontagePlayRate(Character);

	// Fallback to HoldState if query fails (shouldn't happen, but safety first)
	if (CurrentPlayRate <= 0.0f)
	{
		CurrentPlayRate = HoldState.CurrentPlayRate;
		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Warning, TEXT("[HOLD] Failed to query montage playrate, using HoldState: %.2f"), CurrentPlayRate);
		}
	}

	// Initialize EASE-OUT transition state
	HoldState.bIsEasing = true;
	HoldState.bIsEasingOut = true; // EASE-OUT direction
	HoldState.EaseStartTime = GetWorld()->GetTimeSeconds();
	HoldState.EaseStartPlayRate = CurrentPlayRate; // Start from ACTUAL current playrate

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[HOLD] Light attack EASE-OUT starting from ACTUAL playrate: %.2f → 1.0"), CurrentPlayRate);
	}

	// NOTE: We keep HoldState.bIsHolding = true during ease-out
	// This prevents re-activation during the transition
	// Deactivate() will be called when ease-out complete
	// Start timer for EASE-OUT (60 Hz for smooth transitions)
	float TimerInterval = 1.0f / 60.0f; // 60 Hz update rate
	/*TODO: Update to support variable and dynamic framerate*/

	GetWorld()->GetTimerManager().SetTimer(
		EaseTimerHandle,
		this,
		&UCombatComponent::OnEaseTimerTick,
		TimerInterval,
		true // Loop
	);

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[HOLD TIMER] EASE-OUT started (%.2f → 1.0 over %.2fs using %s @ 60Hz)"),
			CurrentPlayRate,
			CurrentAttackData->HoldEaseOutDuration,
			*UEnum::GetValueAsString(CurrentAttackData->HoldEaseOutType));
	}
}

void UCombatComponent::OnEaseTimerTick()
{
	// TIMER-BASED EASE TRANSITION (NOT tick-based!)
	// This function is called by FTimerHandle at regular intervals (60 Hz)
	// Handles BOTH ease-in (1.0 → HoldTargetPlayRate) AND ease-out (HoldTargetPlayRate → 1.0)

	if (!HoldState.bIsEasing || !CurrentAttackData)
	{
		// Ease cancelled or completed - clear timer
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(EaseTimerHandle);
		}
		return;
	}

	// Calculate elapsed time since ease started
	float CurrentTime = GetWorld()->GetTimeSeconds();
	float ElapsedTime = CurrentTime - HoldState.EaseStartTime;

	// Determine if we're easing IN or OUT using dedicated flag (NOT playrate comparison)
	// CRITICAL FIX: Using playrate comparison fails when button released during ease-in
	// EASE-IN: Start = 1.0, Target = HoldTargetPlayRate (bIsEasingOut = false)
	// EASE-OUT: Start = current playrate, Target = 1.0 (bIsEasingOut = true)
	bool bIsEasingIn = !HoldState.bIsEasingOut; // Use flag instead of playrate comparison
	float TargetPlayRate = bIsEasingIn ? CurrentAttackData->HoldTargetPlayRate : 1.0f;
	float EaseDuration = bIsEasingIn ? CurrentAttackData->HoldEaseInDuration : CurrentAttackData->HoldEaseOutDuration;
	EEasingType EasingType = bIsEasingIn ? CurrentAttackData->HoldEaseInType : CurrentAttackData->HoldEaseOutType;

	// Check if ease transition is complete
	if (ElapsedTime >= EaseDuration)
	{
		// Ease complete - ensure we're at final target playrate
		HoldState.bIsEasing = false;
		HoldState.CurrentPlayRate = TargetPlayRate;

		ACharacter* CharacterRef = Cast<ACharacter>(GetOwner());
		UMontageUtilityLibrary::SetMontagePlayRate(CharacterRef, HoldState.CurrentPlayRate);

		// Clear timer - ease complete
		GetWorld()->GetTimerManager().ClearTimer(EaseTimerHandle);

		// If EASE-IN just completed, mark hold as completed (freeze state reached)
		if (bIsEasingIn)
		{
			HoldState.MarkHoldCompleted();

			if (GetDebugDraw())
			{
				float TotalHoldDuration = CurrentTime - HoldState.CurrentHold.StartTime;
				UE_LOG(LogCombat, Log, TEXT("[HOLD] Light attack freeze reached - hold marked completed (duration: %.2fs)"), TotalHoldDuration);
			}
		}
		// If EASE-OUT just completed, execute follow-up attack ONLY if hold was completed
		else
		{
			// Calculate total hold duration (from activation to release)
			float TotalHoldDuration = CurrentTime - HoldState.CurrentHold.StartTime;

			// CRITICAL: Only auto-queue follow-up if hold was COMPLETED
			// Light: Playrate reached 0 (freeze state)
			// Heavy: Charge loop became active (marked in ActivateHold)
			/*TODO: Perhaps add logic for threshold allowance to allow the system to be less rigid (within a threshold from playrate == 0.0f)*/
			if (HoldState.IsHoldCompleted())
			{
				// EXECUTE FOLLOW-UP ATTACK (directional only - NO NextComboAttack fallback)
				UAttackData* FollowUpAttack = nullptr;

				// Check for directional follow-up based on held direction
				if (CurrentAttackData && HoldState.CurrentHold.Direction != EAttackDirection::None)
				{
					// Try to find directional follow-up (TMap returns TObjectPtr<UAttackData>*)
					TObjectPtr<UAttackData>* DirectionalAttack = CurrentAttackData->DirectionalFollowUps.Find(HoldState.CurrentHold.Direction);
					if (DirectionalAttack && DirectionalAttack->Get())
					{
						FollowUpAttack = DirectionalAttack->Get();

						if (GetDebugDraw())
						{
							UE_LOG(LogCombat, Log, TEXT("[HOLD] Directional follow-up found: Direction=%s, Attack=%s (hold duration: %.2fs)"),
								*UEnum::GetValueAsString(HoldState.CurrentHold.Direction),
								*FollowUpAttack->GetName(),
								TotalHoldDuration);
						}
					}
				}

				// Queue the follow-up attack if found (NO fallback to NextComboAttack)
				if (FollowUpAttack)
				{
					FQueuedInputAction FollowUpInput(
						HoldState.GetHeldInputType(),          // Same input type as hold
						EInputEventType::Press,                // Treat as press event
						GetWorld()->GetTimeSeconds(),          // Current time
						false                                   // Not in combo window
					);

					QueueAction(FollowUpInput, FollowUpAttack);

					if (GetDebugDraw())
					{
						UE_LOG(LogCombat, Log, TEXT("[HOLD] Directional follow-up queued: %s"), *FollowUpAttack->GetName());
					}
				}
				else if (GetDebugDraw())
				{
					UE_LOG(LogCombat, Log, TEXT("[HOLD] No directional follow-up configured for direction=%s"),
						*UEnum::GetValueAsString(HoldState.CurrentHold.Direction));
				}
			}
			else if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[HOLD] Hold not completed - skipping follow-up attack (duration: %.2fs)"),
					TotalHoldDuration);
			}

			// Deactivate hold state
			HoldState.Deactivate();

			// PHASE 1 FIX: Procedurally update movement state (replaces manual SetMovementMode)
			UpdateMovementFromMontageState();
		}

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[HOLD TIMER] %s complete, final playrate: %.2f"),
				bIsEasingIn ? TEXT("EASE-IN") : TEXT("EASE-OUT"),
				HoldState.CurrentPlayRate);
		}
		return;
	}

	// PROCEDURAL EASING: Calculate playrate using CalculateTransitionPlayRate
	// This eliminates need for authored curves!
	float NewPlayRate = UMontageUtilityLibrary::CalculateTransitionPlayRate(
		HoldState.EaseStartPlayRate,         // Start rate (1.0 or HoldTargetPlayRate)
		TargetPlayRate,                       // Target rate (HoldTargetPlayRate or 1.0)
		ElapsedTime,                          // Current time in transition
		EaseDuration,                         // Transition duration (from AttackData)
		EasingType                            // Easing curve (EaseInType or EaseOutType)
	);

	// Update hold state
	HoldState.CurrentPlayRate = NewPlayRate;

	// Apply playrate to montage
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UMontageUtilityLibrary::SetMontagePlayRate(Character, NewPlayRate);

	// PHASE 1 FIX: Update movement state after changing playrate
	// This ensures movement locks/unlocks based on current playrate
	UpdateMovementFromMontageState();

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Verbose, TEXT("[HOLD TIMER] %s playrate: %.2f → %.2f (%.1f%% complete)"),
			bIsEasingIn ? TEXT("EASE-IN") : TEXT("EASE-OUT"),
			HoldState.EaseStartPlayRate, TargetPlayRate,
			(ElapsedTime / EaseDuration) * 100.0f);
	}
}

// ============================================================================
// PHASE TRANSITION SYSTEM
// ============================================================================

void UCombatComponent::OnPhaseTransition(EAttackPhase NewPhase)
{
	// CRITICAL: Update CurrentPhase FIRST before any other logic
	// This ensures DetermineExecutionMode sees the correct phase for incoming input
	SetPhase(NewPhase);

	// PHASE 9: EVENT-DRIVEN QUEUE PROCESSING
	// Execute queued actions that target this phase transition
	// This replaces tick-based ProcessQueue() polling!
	ProcessQueuedActions(NewPhase);

	// EVENT-DRIVEN MOVEMENT SYNC: Update movement state on phase changes
	// Ensures movement is correct when phases change (no tick needed)
	UpdateMovementFromMontageState();

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[PHASE] Phase transition complete: %s (queue processed event-driven)"),
			*UEnum::GetValueAsString(NewPhase));
	}
}

void UCombatComponent::SetPhase(EAttackPhase NewPhase)
{
	if (CurrentPhase == NewPhase)
	{
		return; // No change needed
	}

	EAttackPhase OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[PHASE] Phase transition: %d → %d"),
			static_cast<int32>(OldPhase),
			static_cast<int32>(NewPhase));
	}

	// Broadcast phase changed event
	OnPhaseChanged.Broadcast(OldPhase, NewPhase);

	// Handle phase-specific logic
	switch (NewPhase)
	{
		case EAttackPhase::Active:
			// ENABLE HIT DETECTION when entering Active phase
			// This is where the weapon can deal damage
			if (ABaseCombatCharacter* Character = GetOwnerCharacter())
			{
				ICombatInterface::Execute_OnEnableHitDetection(Character);

				if (GetDebugDraw())
				{
					UE_LOG(LogCombat, Log, TEXT("[PHASE] Active entered - Hit detection ENABLED"));
				}
			}
			break;

		case EAttackPhase::Recovery:
			// DISABLE HIT DETECTION when entering Recovery phase
			// Active window is over, no more damage
			if (ABaseCombatCharacter* Character = GetOwnerCharacter())
			{
				ICombatInterface::Execute_OnDisableHitDetection(Character);
			}

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[PHASE] Recovery entered - Hit detection DISABLED"));
			}
			break;

		case EAttackPhase::None:
			// DISABLE HIT DETECTION if we were in Active phase (handles interrupts)
			if (OldPhase == EAttackPhase::Active)
			{
				if (ABaseCombatCharacter* Character = GetOwnerCharacter())
				{
					ICombatInterface::Execute_OnDisableHitDetection(Character);
				}
			}

			// Attack finished - reset combo state for next attack
			CurrentAttackData = nullptr;
			CurrentAttackInputType = EInputType::None;

			// CRITICAL: Ensure input context returns to Movement on attack completion
			// This prevents directional input context leaking into idle/movement state
			SetInputContext(EInputContext::Movement);

			// CRITICAL: Clear hold state completely (ease timer, flags, movement)
			// This prevents hold state leaking into next attack
			ClearHoldState();

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[PHASE] Attack finished - Combo state, hold state, and input context cleared"));
			}
			break;

		default:
			break;
	}
}
/*TODO: Consider adding OnPhaseEnter/Exit events for more granular control --> Also we know that active phase is when queued input actions can be executed so perhaps having phase specific logic for simple things like this would be prudent*/

void UCombatComponent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[MONTAGE] Montage blending out: %s | Interrupted: %s"),
			Montage ? *Montage->GetName() : TEXT("None"),
			bInterrupted ? TEXT("YES") : TEXT("NO"));
	}

	// Broadcast montage event
	OnMontageEvent.Broadcast(Montage, bInterrupted, FName("BlendingOut"));

	// Prepare for next attack during blend out (smoother transitions)
	// Phase transition to None happens in OnMontageEnded
	/*TODO: Consider allowing new attack to start here during blend out? Place some helpful checks and transition smoothing here of some sort*/
}

void UCombatComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[MONTAGE] Montage ended: %s | Interrupted: %s"),
			Montage ? *Montage->GetName() : TEXT("None"),
			bInterrupted ? TEXT("YES") : TEXT("NO"));
	}

	// CRITICAL: Reset input state on interruption (stun, knockback, etc.)
	// Prevents directional input context leaking when montage is forcibly stopped
	if (bInterrupted)
	{
		SetInputContext(EInputContext::Movement);
		DirectionalInputBuffer.Reset();
		ClearHoldState();

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Warning, TEXT("[MONTAGE] Interrupted - input state reset"));
		}
	}

	// Broadcast montage event
	OnMontageEvent.Broadcast(Montage, bInterrupted, FName("Ended"));

	// CRITICAL: Execute any pending queued actions BEFORE clearing state
	/*TODO: In recent debug logs, checkpoint discovery has been failing, please ensure that the checkpoint system still works as designed*/
	
	// BUT only if their checkpoint was actually reached (ScheduledTime >= 0)
	// Actions with ScheduledTime=-1.0 mean checkpoint was never registered → discard them
	if (ActionQueue.Num() > 0)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Warning, TEXT("[MONTAGE] Montage ended with %d queued actions - checking which are ready"), ActionQueue.Num());
		}

		// Get montage end time to check if actions reached their checkpoint
		float MontageEndTime = 0.0f;
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
			{
				MontageEndTime = AnimInstance->Montage_GetPosition(Montage);
			}
		}

		// Execute pending actions that reached their checkpoint
		for (int32 i = ActionQueue.Num() - 1; i >= 0; --i)
		{
			FActionQueueEntry& Entry = ActionQueue[i];

			if (Entry.IsPending())
			{
				// CRITICAL FIX: Only execute if checkpoint was reached
				// ScheduledTime=-1.0 means Active→Recovery checkpoint never registered → discard
				if (Entry.ScheduledTime < 0.0f)
				{
					if (GetDebugDraw())
					{
						UE_LOG(LogCombat, Warning, TEXT("[QUEUE] Discarding action (checkpoint never reached): Type=%s, ScheduledTime=%.2f"),
							*UEnum::GetValueAsString(Entry.InputAction.InputType), Entry.ScheduledTime);
					}

					// Discard action - checkpoint never happened
					ActionQueue.RemoveAt(i);
					QueueStats.ActionsCancelled++;
					continue;
				}

				// Only execute if montage reached the scheduled time
				if (MontageEndTime >= Entry.ScheduledTime)
				{
					if (GetDebugDraw())
					{
						UE_LOG(LogCombat, Log, TEXT("[QUEUE] Executing action from ended montage: Type=%s, ScheduledTime=%.2f, MontageEndTime=%.2f"),
							*UEnum::GetValueAsString(Entry.InputAction.InputType), Entry.ScheduledTime, MontageEndTime);
					}

					// Execute the pending action
					if (ExecuteAction(Entry))
					{
						ActionQueue.RemoveAt(i);

						QueueStats.ActionsExecuted++;

						// Only execute the first valid action (FIFO)
						break;
					}
				}
				else
				{
					if (GetDebugDraw())
					{
						UE_LOG(LogCombat, Warning, TEXT("[QUEUE] Discarding action (montage ended before checkpoint): Type=%s, ScheduledTime=%.2f, MontageEndTime=%.2f"),
							*UEnum::GetValueAsString(Entry.InputAction.InputType), Entry.ScheduledTime, MontageEndTime);
					}

					// Discard action - montage ended before checkpoint
					ActionQueue.RemoveAt(i);
					QueueStats.ActionsCancelled++;
				}
			}
		}
	}

	// STRUCTURAL FIX: Only transition to None if NOT in combo blend
	// When bInComboBlend=true, we're mid-transition and new montage will start soon
	// This prevents phase desync: Windup → None → Active (old montage ending during blend-out)
	if (!bInComboBlend && CurrentPhase != EAttackPhase::Windup && CurrentPhase != EAttackPhase::Active)
	{
		SetPhase(EAttackPhase::None);
	}

	// SLOPE FIX: Safety net - snap character to ground if floating after attack
	// This catches cases where terrain-aware warp target wasn't enough (e.g., pure root motion without warp)
	if (!bInComboBlend)
	{
		ABaseCombatCharacter* Character = GetOwnerCharacter();
		if (Character && UDebugUtils::IsCharacterFloating(Character, 5.0f))
		{
			const bool bSnapped = UDebugUtils::SnapCharacterToGround(Character, 5.0f, GetDebugDraw());
			if (bSnapped && GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[SLOPE FIX] Character snapped to ground after attack montage"));
			}
		}
	}

	// Clear checkpoints for finished montage (new montage will have its own)
	Checkpoints.Empty();
}

//NOTE:: OnMontageEnded is where queued actions get a last chance to execute if their checkpoint was reached.

// ============================================================================
// PROCEDURAL MOVEMENT CONTROL (Phase 1 Fix)
// ============================================================================

void UCombatComponent::UpdateMovementFromMontageState()
{
	// PROCEDURAL MOVEMENT SYNC: Automatically enable/disable movement based on current animation state
	// This replaces manual DisableMovement/SetMovementMode calls scattered throughout the code
	// Called from: TickComponent (every frame), PlayAttackMontage (new attack), OnEaseTimerTick (during transitions)

	ABaseCombatCharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
	if (!MovementComp)
	{
		return;
	}

	// Determine if movement should be locked based on current state
	bool bShouldLockMovement = false;

	// RULE 1: Lock during hold freeze (playrate < threshold)
	if (HoldState.IsHolding())
	{
		// Query ACTUAL montage playrate (don't trust HoldState.CurrentPlayRate)
		float CurrentPlayRate = UMontageUtilityLibrary::GetMontagePlayRate(Character);
		if (CurrentPlayRate < 0.5f)
		{
			bShouldLockMovement = true;

			if (GetDebugDraw() && !bMovementCurrentlyDisabled)
			{
				UE_LOG(LogCombat, Log, TEXT("[MOVEMENT] Locking movement - hold freeze (playrate=%.2f)"), CurrentPlayRate);
			}
		}
	}

	// RULE 2: Lock during ease-in (transitioning to freeze)
	if (HoldState.bIsEasing && !HoldState.bIsEasingOut)
	{
		bShouldLockMovement = true;

		if (GetDebugDraw() && !bMovementCurrentlyDisabled)
		{
			UE_LOG(LogCombat, Log, TEXT("[MOVEMENT] Locking movement - ease-in to freeze"));
		}
	}

	// Apply movement state change if needed
	if (bShouldLockMovement && !bMovementCurrentlyDisabled)
	{
		// Need to disable movement
		MovementComp->DisableMovement();
		bMovementCurrentlyDisabled = true;

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[MOVEMENT] Movement DISABLED"));
		}
	}
	else if (!bShouldLockMovement && bMovementCurrentlyDisabled)
	{
		// Need to enable movement
		MovementComp->SetMovementMode(MOVE_Walking);
		bMovementCurrentlyDisabled = false;

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[MOVEMENT] Movement ENABLED"));
		}
	}
}

void UCombatComponent::ClearHoldState()
{
	// CRITICAL: Complete hold state cleanup when starting new attack or on montage end
	// Prevents state leaks between attacks

	// Cancel any active ease timer
	if (GetWorld() && EaseTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(EaseTimerHandle);

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[HOLD] Ease timer cleared"));
		}
	}

	// Deactivate hold state
	if (HoldState.IsHolding() || HoldState.bIsEasing)
	{
		HoldState.Deactivate();

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[HOLD] Hold state cleared"));
		}
	}

	// CRITICAL: Forcibly restore montage playrate to 1.0
	// This prevents blending artifacts when combo interrupts hold ease mid-transition
	// Without this, new montage starts with wrong playrate (e.g., 0.75) causing "partial blend" visual issues
	ABaseCombatCharacter* Character = GetOwnerCharacter();
	if (Character)
	{
		float CurrentPlayRate = UMontageUtilityLibrary::GetMontagePlayRate(Character);
		if (!FMath::IsNearlyEqual(CurrentPlayRate, 1.0f, 0.01f))
		{
			UMontageUtilityLibrary::SetMontagePlayRate(Character, 1.0f);

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[HOLD] Playrate restored: %.2f → 1.0"), CurrentPlayRate);
			}
		}
	}

	// ARCHITECTURAL FIX: Reset input context to Movement
	// Hold window closed, movement stick no longer interpreted as directional input
	SetInputContext(EInputContext::Movement);

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[HOLD] Directional input window CLOSED"));
	}

	// Ensure movement is synced to new state
	UpdateMovementFromMontageState();
}

void UCombatComponent::SetupAttackWarp(UAttackData* AttackData)
{
	// Early exit if no valid attack data or warp disabled
	if (!AttackData || !AttackData->WarpConfig.bEnableWarp)
	{
		return;
	}

	ABaseCombatCharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		return;
	}

	// Get targeting component for soft aim assist
	UTargetingComponent* Targeting = Character->GetTargetingComponent();
	if (!Targeting)
	{
		return;
	}

	// Determine direction source and convert to world vector
	EInputDirection FinalDirection = EInputDirection::None;
	FVector WorldDirection = FVector::ZeroVector;
	FString DirectionSource = TEXT("None");

	// PRIORITY 1: Use buffered direction from hold release (if available)
	if (DirectionalInputBuffer.HasValidInput())
	{
		FinalDirection = DirectionalInputBuffer.DirectionAtRelease;
		WorldDirection = UCombatUtils::InputDirectionToWorldVector(FinalDirection, Character);
		DirectionSource = TEXT("HoldRelease");
	}
	// PRIORITY 2: Fall back to current movement input (for non-hold attacks)
	else
	{
		const FVector2D MovementInput = Character->GetLastMovementInput();

		// Only use if movement input is significant (above dead zone)
		if (MovementInput.Size() > 0.2f)
		{
			// Get camera rotation for direction conversion
			FRotator CameraRotation = FRotator::ZeroRotator;
			if (AController* Controller = Character->GetController())
			{
				CameraRotation = Controller->GetControlRotation();
			}

			// Convert camera-relative input to character-relative direction
			FinalDirection = UCombatUtils::VectorToCharacterRelativeDirection(
				MovementInput,
				CameraRotation,
				Character,
				Character->GetActorRotation(),
				0.2f  // Dead zone
			);

			if (FinalDirection != EInputDirection::None)
			{
				WorldDirection = UCombatUtils::InputDirectionToWorldVector(FinalDirection, Character);
				DirectionSource = TEXT("LiveInput");
			}
		}
	}

	// Variables for target and rotation
	AActor* BestTarget = nullptr;
	FRotator TargetRotation = FRotator::ZeroRotator;
	const FAttackWarpConfig& WarpConfig = AttackData->WarpConfig;
	const FRotator CurrentFacing = Character->GetActorRotation();

	// CASE 1 & 2: We have a direction (from hold release or live input)
	if (FinalDirection != EInputDirection::None && !WorldDirection.IsNearlyZero())
	{
		// Use soft aim assist to find best target in direction
		// If found, will use translation+rotation warp; otherwise, rotation-only
		TargetRotation = Targeting->FindBestTargetForDirection(
			WorldDirection,
			BestTarget,  // OutBestTarget is second parameter
			-1.0f,       // Use TargetingSettings defaults for remaining params
			-1.0f,
			-1.0f,
			-1.0f,
			-1.0f
		);

		// Check if we should skip because already facing this direction (and no target to snap to)
		if (!BestTarget && WarpConfig.AlreadyFacingThreshold > 0.0f)
		{
			const float AngleDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentFacing.Yaw, TargetRotation.Yaw));
			if (AngleDelta <= WarpConfig.AlreadyFacingThreshold)
			{
				if (GetDebugDraw())
				{
					UE_LOG(LogCombat, Verbose, TEXT("[ATTACK WARP] Skipped: Already facing direction (delta=%.1f° <= threshold=%.1f°)"),
						AngleDelta, WarpConfig.AlreadyFacingThreshold);
				}
				// Clear buffer if we used it
				if (DirectionSource == TEXT("HoldRelease"))
				{
					DirectionalInputBuffer.Reset();
				}
				return;
			}
		}
	}
	// CASE 3: No direction input - try to find nearest target within facing cone
	else
	{
		// Only search within the configured facing cone (prevents 180° snaps)
		BestTarget = Targeting->FindNearestTarget(-1.0f, WarpConfig.NoInputFacingCone);

		if (BestTarget)
		{
			// Warp toward the nearest target
			const FVector ToTarget = BestTarget->GetActorLocation() - Character->GetActorLocation();
			TargetRotation = ToTarget.Rotation();
			DirectionSource = TEXT("NearestTarget");

			// Check if we should skip because already facing the target
			if (WarpConfig.AlreadyFacingThreshold > 0.0f)
			{
				const float AngleDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentFacing.Yaw, TargetRotation.Yaw));
				if (AngleDelta <= WarpConfig.AlreadyFacingThreshold)
				{
					if (GetDebugDraw())
					{
						UE_LOG(LogCombat, Verbose, TEXT("[ATTACK WARP] Skipped: Already facing target %s (delta=%.1f°)"),
							*BestTarget->GetName(), AngleDelta);
					}
					return;
				}
			}

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[ATTACK WARP] No input direction - using nearest target in %.0f° cone: %s"),
					WarpConfig.NoInputFacingCone, *BestTarget->GetName());
			}
		}
		// CASE 4: No direction AND no nearby target in cone - skip warp entirely
		else
		{
			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Verbose, TEXT("[ATTACK WARP] Skipped: No input direction and no targets in %.0f° facing cone"),
					WarpConfig.NoInputFacingCone);
			}
			return;
		}
	}

	// Setup the attack warp using unified function
	// If BestTarget exists: translation+rotation warp (uses TargetWarpName)
	// If BestTarget is null: rotation-only warp (uses RotationWarpName)
	const bool bSuccess = Targeting->SetupAttackWarp(
		BestTarget,
		TargetRotation,
		WarpConfig
	);

	if (GetDebugDraw())
	{
		if (bSuccess)
		{
			const bool bHasTarget = BestTarget != nullptr;
			UE_LOG(LogCombat, Log, TEXT("[ATTACK WARP] Set up %s warp: Direction=%s, Source=%s, Target=%s, Rotation=%.1f°"),
				bHasTarget ? TEXT("TARGET") : TEXT("ROTATION"),
				FinalDirection != EInputDirection::None ? *UEnum::GetValueAsString(FinalDirection) : TEXT("N/A"),
				*DirectionSource,
				BestTarget ? *BestTarget->GetName() : TEXT("None"),
				TargetRotation.Yaw);
		}
		else
		{
			UE_LOG(LogCombat, Warning, TEXT("[ATTACK WARP] Setup failed: Direction=%s, Source=%s"),
				FinalDirection != EInputDirection::None ? *UEnum::GetValueAsString(FinalDirection) : TEXT("N/A"),
				*DirectionSource);
		}
	}

	// Clear directional buffer after use (one-shot) - only if we used it
	if (DirectionSource == TEXT("HoldRelease"))
	{
		DirectionalInputBuffer.Reset();
	}
}

// ============================================================================
// STATE QUERIES
// ============================================================================

int32 UCombatComponent::GetPendingActionCount() const
{
	int32 Count = 0;
	for (const FActionQueueEntry& Entry : ActionQueue)
	{
		if (Entry.IsPending())
		{
			Count++;
		}
	}
	return Count;
}

float UCombatComponent::GetHoldDuration() const
{
	return HoldState.GetHoldDuration(GetWorld()->GetTimeSeconds());
}

// ============================================================================
// DEBUG / VISUALIZATION
// ============================================================================

void UCombatComponent::DrawDebugInfo() const
{
	if (!GetOwner())
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	const FVector OwnerLocation = Character->GetActorLocation();
	const FVector Offset = FVector(0, 0, 100);

	// ============================================================================
	// PHASE INDICATOR
	// ============================================================================
	FString PhaseInfo = FString::Printf(TEXT("Phase: %s"), *UEnum::GetValueAsString(CurrentPhase));
	FColor PhaseColor = FColor::White;

	switch (CurrentPhase)
	{
		case EAttackPhase::Windup:
			PhaseColor = FColor::Orange;
			break;
		case EAttackPhase::Active:
			PhaseColor = FColor::Red;
			break;
		case EAttackPhase::Recovery:
			PhaseColor = FColor::Yellow;
			break;
		case EAttackPhase::None:
			PhaseColor = FColor::White;
			break;
	}

	DrawDebugString(GetWorld(), OwnerLocation + Offset * 0.5f, PhaseInfo, nullptr, PhaseColor, 0.0f, true);

	// ============================================================================
	// QUEUE INFO
	// ============================================================================
	FString QueueInfo = FString::Printf(TEXT("Queue: %d pending | %d total"),
		GetPendingActionCount(),
		ActionQueue.Num());

	DrawDebugString(GetWorld(), OwnerLocation + Offset, QueueInfo, nullptr, FColor::Cyan, 0.0f, true);

	// Draw individual queued actions
	int32 ActionIndex = 0;
	for (const FActionQueueEntry& Entry : ActionQueue)
	{
		if (Entry.IsPending())
		{
			FString ActionInfo = FString::Printf(TEXT("  [%d] %s @ %.2f (%s)"),
				ActionIndex++,
				*UEnum::GetValueAsString(Entry.InputAction.InputType),
				Entry.ScheduledTime,
				*UEnum::GetValueAsString(Entry.ExecutionMode));

			DrawDebugString(GetWorld(), OwnerLocation + Offset * (1.2f + ActionIndex * 0.3f), ActionInfo,
				nullptr, FColor::Cyan, 0.0f, true);
		}
	}

	// ============================================================================
	// HOLD STATE
	// ============================================================================
	if (HoldState.IsHolding())
	{
		FString HoldInfo = FString::Printf(TEXT("HOLDING: %s (%.2fs) [%s]"),
			*UEnum::GetValueAsString(HoldState.GetHeldInputType()),
			GetHoldDuration(),
			HoldState.IsHoldCompleted() ? TEXT("COMPLETED") : TEXT("Incomplete"));

		DrawDebugString(GetWorld(), OwnerLocation + Offset * 2.5f, HoldInfo, nullptr, FColor::Yellow, 0.0f, true);
	}

	// ============================================================================
	// MOVEMENT STATE (Phase 1 Debug)
	// ============================================================================
	FString MovementState = bMovementCurrentlyDisabled ? TEXT("DISABLED") : TEXT("ENABLED");
	FColor MovementColor = bMovementCurrentlyDisabled ? FColor::Red : FColor::Green;

	DrawDebugString(GetWorld(), OwnerLocation + Offset * 3.0f,
		FString::Printf(TEXT("Movement: %s"), *MovementState),
		nullptr, MovementColor, 0.0f, true);

	// ============================================================================
	// STATS
	// ============================================================================
	FString StatsInfo = FString::Printf(TEXT("Stats: %d executed (%d queued + %d immediate) | %d cancelled"),
		QueueStats.ActionsExecuted,
		QueueStats.QueuedExecutions,
		QueueStats.ImmediateExecutions,
		QueueStats.ActionsCancelled);

	DrawDebugString(GetWorld(), OwnerLocation + Offset * 3.0f, StatsInfo, nullptr, FColor::White, 0.0f, true);

	// ============================================================================
	// CHECKPOINT TIMELINE (Visual)
	// ============================================================================
	if (Checkpoints.Num() > 0 && UMontageUtilityLibrary::GetCurrentMontageTime(Character) >= 0.0f)
	{
		// Draw visual timeline using MontageUtilityLibrary
		UMontageUtilityLibrary::DrawCheckpointTimeline(
			GetWorld(),
			Character,
			Checkpoints,
			0.0f,  // Draw duration (0 = single frame, updated each tick)
			150.0f  // Y offset above character
		);

		// Log checkpoints if they changed
		static int32 LastCheckpointCount = 0;
		if (Checkpoints.Num() != LastCheckpointCount)
		{
			UMontageUtilityLibrary::LogCheckpoints(Checkpoints, TEXT("DEBUG"));
			LastCheckpointCount = Checkpoints.Num();
		}
	}

	// ============================================================================
	// COMBO WINDOW INDICATOR
	// ============================================================================
	if (bComboWindowActive)
	{
		float CurrentTime = UMontageUtilityLibrary::GetCurrentMontageTime(Character);
		float TimeRemaining = (ComboWindowStart + ComboWindowDuration) - CurrentTime;

		FString ComboInfo = FString::Printf(TEXT("COMBO WINDOW: %.2fs remaining"),
			FMath::Max(0.0f, TimeRemaining));

		DrawDebugString(GetWorld(), OwnerLocation + Offset * 3.5f, ComboInfo, nullptr, FColor::Green, 0.0f, true);
	}
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

void UCombatComponent::ProcessInputPair(const FQueuedInputAction& PressEvent, const FQueuedInputAction& ReleaseEvent)
{
	// Calculate hold duration
	float HoldDuration = ReleaseEvent.Timestamp - PressEvent.Timestamp;

	if ( GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[INPUT] Pair processed: %s held for %.2fs"),
			*UEnum::GetValueAsString(PressEvent.InputType),
			HoldDuration);
	}

	// Press/release pair processing logic
	// This could trigger special actions based on hold duration in future
}

EActionExecutionMode UCombatComponent::DetermineExecutionMode(const FQueuedInputAction& InputAction) const
{
	// ============================================================================
	// PHASE-BASED EXECUTION (Smart Queue Management)
	// ============================================================================
	//
	// Input during Windup/Active → Queued (buffered, execute at Active end = "snap" timing)
	// Input during Recovery → IMMEDIATE (interrupt recovery for responsive combos)
	// Input during None (idle) → IMMEDIATE (start new action)
	//
	// This creates the "snappy" feel for queued inputs and "responsive" feel for
	// Recovery inputs, matching the Ghost of Tsushima combat design.
	// ============================================================================

	// During Windup or Active → Queue for Active end (cannot attack now)
	// This gives "snappy" execution - input buffered during windup executes at Active end
	if (CurrentPhase == EAttackPhase::Windup || CurrentPhase == EAttackPhase::Active)
	{
		return EActionExecutionMode::Queued;
	}

	// During Recovery or None → Execute immediately (responsive combos)
	// Recovery input interrupts the recovery animation for fluid combo flow
	return EActionExecutionMode::Immediate;
}

UAttackData* UCombatComponent::GetAttackForInput(EInputType InputType)
{
	// Get default attacks as fallbacks through CombatSettings → AttackConfiguration
	UAttackData* DefaultLightAttack = GetDefaultLightAttack();
	UAttackData* DefaultHeavyAttack = GetDefaultHeavyAttack();

	// Determine if we should combo: Check combo window OR valid attack continuation
	// This fixes combo progression for both queued AND immediate execution
	bool bShouldCombo = bComboWindowActive;

	// CRITICAL FIX: If we have CurrentAttackData and we're mid-attack (any phase except None),
	// allow combo continuation even if combo window flag hasn't been set yet
	// This handles rapid double-taps that queue input during Windup phase (before combo window opens)
	if (!bShouldCombo && CurrentAttackData && CurrentPhase != EAttackPhase::None)
	{
		bShouldCombo = true;

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[COMBO] Allowing combo from phase %s (CurrentAttack=%s)"),
				*UEnum::GetValueAsString(CurrentPhase),
				*CurrentAttackData->GetName());
		}
	}

	// Debug: Log combo resolution context
	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Warning, TEXT("[COMBO DEBUG] GetAttackForInput: Phase=%s, CurrentAttack=%s, ComboWindow=%s, bShouldCombo=%s"),
			*UEnum::GetValueAsString(CurrentPhase),
			CurrentAttackData ? *CurrentAttackData->GetName() : TEXT("nullptr"),
			bComboWindowActive ? TEXT("ACTIVE") : TEXT("Inactive"),
			bShouldCombo ? TEXT("TRUE") : TEXT("FALSE"));
	}

	// ============================================================================
	// DIRECTIONAL INPUT RESOLUTION (Architectural Fix)
	// ============================================================================
	// Use DirectionalInputBuffer (discrete sampling at hold release) instead of
	// LastDirectionalInput (continuous sampling causing semantic conflation)
	//
	// Direction is ONLY available if:
	// 1. Player held attack button (hold window opened)
	// 2. Input context switched to DirectionalInput
	// 3. Player released button WITH direction (captured at release event)
	//
	// If no directional input buffered → AttackDirection = None → resolution
	// uses normal combo chains (Priority 3) instead of directional follow-ups (Priority 2)

	EAttackDirection AttackDirection = EAttackDirection::None;

	if (DirectionalInputBuffer.HasValidInput())
	{
		// Use buffered direction (sampled at release during hold window)
		AttackDirection = UCombatUtils::InputToAttackDirection(DirectionalInputBuffer.DirectionAtRelease);

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[DIRECTIONAL] Using buffered direction: %s (captured at %.2f) → AttackDirection=%s"),
				*UEnum::GetValueAsString(DirectionalInputBuffer.DirectionAtRelease),
				DirectionalInputBuffer.CaptureTime,
				*UEnum::GetValueAsString(AttackDirection));
		}
	}
	else if (GetDebugDraw())
	{
		// No directional input buffered (movement context OR no hold release)
		UE_LOG(LogCombat, Verbose, TEXT("[DIRECTIONAL] No buffered direction (AttackDirection=None)"));
	}

	// ========================================================================
	// CONTEXT-AWARE RESOLUTION
	// ========================================================================

	// Clear visited set at start of resolution (cycle detection)
	VisitedAttacks.Empty();

	// Call context-aware resolution
	// Pass entire HoldState (not just IsHolding()) so resolution can check IsHoldCompleted()
	FAttackResolutionResult Result = UMontageUtilityLibrary::ResolveNextAttackContextual(
		CurrentAttackData,
		InputType,
		AttackDirection,
		HoldState,  // Pass entire HoldState, not just boolean
		bShouldCombo,
		DefaultLightAttack,
		DefaultHeavyAttack,
		ActiveContextTags,  // NEW: Pass runtime context tags
		VisitedAttacks  // NEW: Pass visited set for cycle detection
	);

	// Check for cycle detection error
	if (Result.bCycleDetected)
	{
		UE_LOG(LogCombat, Error, TEXT("[RESOLVE] Cycle detected! Aborting resolution for safety."));
		return nullptr;
	}

	// ARCHITECTURAL FIX: Clear directional input buffer after consumption
	// Buffer is cleared when:
	// 1. Directional follow-up executed (Path=DirectionalFollowUp)
	// 2. Resolution explicitly signals to clear (bShouldClearDirectionalInput)
	// This prevents reusing the same buffered direction for multiple attacks
	if (Result.bShouldClearDirectionalInput || Result.Path == EResolutionPath::DirectionalFollowUp)
	{
		DirectionalInputBuffer.Reset();

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[DIRECTIONAL] Buffer cleared (directional consumed)"));
		}

		// DEPRECATED: Keep LastDirectionalInput clearing for backward compatibility
		LastDirectionalInput = EInputDirection::None;
	}

	// Debug: Log resolution result with path metadata
	if (GetDebugDraw() && Result.Attack)
	{
		const TCHAR* PathName = TEXT("Unknown");
		switch (Result.Path)
		{
			case EResolutionPath::Default: PathName = TEXT("Default"); break;
			case EResolutionPath::NormalCombo: PathName = TEXT("NormalCombo"); break;
			case EResolutionPath::DirectionalFollowUp: PathName = TEXT("DirectionalFollowUp"); break;
			case EResolutionPath::ParryCounter: PathName = TEXT("ParryCounter"); break;
			case EResolutionPath::LowHealthFinisher: PathName = TEXT("LowHealthFinisher"); break;
			case EResolutionPath::ContextSensitive: PathName = TEXT("ContextSensitive"); break;
		}

		UE_LOG(LogCombat, Log, TEXT("[RESOLVE] ✓ Resolved to: '%s' (Path=%s, ClearInput=%s)"),
			*Result.Attack->GetName(),
			PathName,
			Result.bShouldClearDirectionalInput ? TEXT("YES") : TEXT("NO"));
	}

	// DEPRECATED: Old consumption flag system (replaced by DirectionalInputBuffer.Reset())
	// Kept for backward compatibility, will be removed when LastDirectionalInput fully deprecated
	if (Result.Path == EResolutionPath::DirectionalFollowUp)
	{
		const_cast<UCombatComponent*>(this)->bDirectionalInputConsumed = true;
	}

	// DEPRECATED: bCurrentAttackIsDirectionalFollowUp flag no longer needed
	// Keeping for backward compatibility only
	const_cast<UCombatComponent*>(this)->bCurrentAttackIsDirectionalFollowUp =
		(Result.Path == EResolutionPath::DirectionalFollowUp);

	return Result.Attack;
}

int32 UCombatComponent::CalculatePriority(const FActionQueueEntry& Action) const
{
	// Base priority on action type
	// Light attacks: 1
	// Heavy attacks: 2
	// Dodges: 3
	// Blocks: 4

	switch (Action.InputAction.InputType)
	{
		case EInputType::LightAttack:
			return 1;

		case EInputType::HeavyAttack:
			return 2;

		case EInputType::Evade:
			return 3;

		case EInputType::Block:
			return 4;

		default:
			return 0;
	}
}

void UCombatComponent::SortQueueByTime()
{
	ActionQueue.Sort([](const FActionQueueEntry& A, const FActionQueueEntry& B)
	{
		return A.ScheduledTime < B.ScheduledTime;
	});
}

FTimerCheckpoint* UCombatComponent::FindCheckpoint(EActionWindowType WindowType)
{
	for (FTimerCheckpoint& Checkpoint : Checkpoints)
	{
		if (Checkpoint.WindowType == WindowType && Checkpoint.bActive)
		{
			return &Checkpoint;
		}
	}
	return nullptr;
}

void UCombatComponent::ClearExpiredCheckpoints(float CurrentTime)
{
	for (int32 i = Checkpoints.Num() - 1; i >= 0; --i)
	{
		FTimerCheckpoint& Checkpoint = Checkpoints[i];

		if (Checkpoint.bActive && CurrentTime > (Checkpoint.MontageTime + Checkpoint.Duration))
		{
			Checkpoint.bActive = false;

			// Update combo window state if this was combo checkpoint
			if (Checkpoint.WindowType == EActionWindowType::Combo)
			{
				bComboWindowActive = false;
			}

			if ( GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[CHECKPOINTS] Expired: Type=%s at %.2f"),
					*UEnum::GetValueAsString(Checkpoint.WindowType),
					CurrentTime);
			}

			Checkpoints.RemoveAt(i);
		}
	}
}

bool UCombatComponent::CanAcceptNewInput(EInputType InputType) const
{
	// Input is ALWAYS buffered during Windup/Active (queued execution)
	// Commit window only blocks IMMEDIATE execution during None/Recovery, not queuing
	// Therefore, commit window check is NOT in CanAcceptNewInput - it's in QueueAction

	// Block if queue already has pending actions of same type (prevents double-queueing)
	for (const FActionQueueEntry& Entry : ActionQueue)
	{
		if (Entry.IsPending() && Entry.InputAction.InputType == InputType)
		{
			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Warning, TEXT("[INPUT] Input REJECTED - Already queued action of same type"));
			}
			return false;
		}
	}

	return true;
}

// ============================================================================
// DEBUG VISUALIZATION TEST HELPERS
// ============================================================================

#if WITH_AUTOMATION_TESTS

FDebugVisualizationData UCombatComponent::CalculateDebugVisualizationData(
	const FRotator& CameraRotation,
	const FRotator& CharacterRotation,
	const FVector2D& CameraRelativeInput,
	EInputDirection ResolvedDirection) const
{
	FDebugVisualizationData Data;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return Data;
	}

	const FVector CharacterLocation = Character->GetActorLocation();
	Data.ChestOffset = FVector(0.0f, 0.0f, 90.0f);
	Data.YawDelta = UDebugUtils::CalculateYawDelta(CameraRotation.Yaw, CharacterRotation.Yaw);
	Data.bShowHoldIndicator = HoldState.IsHolding();
	Data.HoldStateLabel = Data.bShowHoldIndicator ? TEXT("⬛ HOLD ACTIVE") : TEXT("");

	// Calculate transformation vectors
	const FVector CameraForward = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::X);
	const FVector CameraRight = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);
	FVector WorldInput = (CameraRight * CameraRelativeInput.X) + (CameraForward * CameraRelativeInput.Y);
	WorldInput.Z = 0.0f;
	WorldInput.Normalize();

	const FRotator InverseCharacterYaw(0.0f, -CharacterRotation.Yaw, 0.0f);
	const FVector CharacterRelativeVec = InverseCharacterYaw.RotateVector(WorldInput);
	const FVector CharacterForward = FRotationMatrix(CharacterRotation).GetScaledAxis(EAxis::X);
	const EAttackDirection AttackDir = UCombatUtils::InputToAttackDirection(ResolvedDirection);

	// Arrow 1: Camera
	{
		FDebugArrowInfo Arrow;
		Arrow.StartPosition = CharacterLocation + Data.ChestOffset;
		Arrow.Length = 180.0f;
		Arrow.EndPosition = Arrow.StartPosition + (CameraForward * Arrow.Length);
		Arrow.LabelPosition = Arrow.EndPosition + FVector(0, 0, 25);
		Arrow.Label = FString::Printf(TEXT("1.CAMERA\n%s"),
			*UDebugUtils::FormatRotationDebug(CameraRotation));
		Arrow.Color = FColor(0, 100, 255);
		Arrow.Thickness = 2.5f;
		Arrow.bIsDashed = false;
		Data.Arrows.Add(Arrow);
	}

	// Arrow 2: Input
	{
		FDebugArrowInfo Arrow;
		Arrow.StartPosition = CharacterLocation + Data.ChestOffset;
		Arrow.Length = 140.0f;
		Arrow.EndPosition = Arrow.StartPosition + (WorldInput * Arrow.Length);
		Arrow.LabelPosition = Arrow.EndPosition + FVector(0, 0, 25);
		Arrow.Label = Data.bShowHoldIndicator ? TEXT("2.INPUT (hold-release)") : TEXT("2.INPUT (continuous)");
		Arrow.Color = Data.bShowHoldIndicator ? FColor(255, 215, 0) : FColor(255, 255, 0);
		Arrow.Thickness = 2.5f;
		Arrow.bIsDashed = Data.bShowHoldIndicator;
		Data.Arrows.Add(Arrow);
	}

	// Arrow 3: Character Forward
	{
		FDebugArrowInfo Arrow;
		Arrow.StartPosition = CharacterLocation + Data.ChestOffset;
		Arrow.Length = 160.0f;
		Arrow.EndPosition = Arrow.StartPosition + (CharacterForward * Arrow.Length);
		Arrow.LabelPosition = Arrow.EndPosition + FVector(0, 0, 25);
		Arrow.Label = FString::Printf(TEXT("CHAR FORWARD\n%s"),
			*UDebugUtils::FormatRotationDebug(CharacterRotation));
		Arrow.Color = FColor(0, 255, 0);
		Arrow.Thickness = 3.0f;
		Arrow.bIsDashed = false;
		Data.Arrows.Add(Arrow);
	}

	// Arrow 4: Character-Relative
	{
		FDebugArrowInfo Arrow;
		Arrow.StartPosition = CharacterLocation + Data.ChestOffset;
		Arrow.Length = 120.0f;
		Arrow.EndPosition = Arrow.StartPosition + (CharacterRelativeVec * Arrow.Length);
		Arrow.LabelPosition = Arrow.EndPosition + FVector(0, 0, 25);
		Arrow.Label = TEXT("4.CHAR-REL");
		Arrow.Color = FColor(255, 165, 0);
		Arrow.Thickness = 2.5f;
		Arrow.bIsDashed = false;
		Data.Arrows.Add(Arrow);
	}

	// Arrow 5: Attack Direction
	{
		FDebugArrowInfo Arrow;
		Arrow.StartPosition = CharacterLocation + Data.ChestOffset;
		FVector FinalDirectionVec = CharacterRelativeVec;
		FinalDirectionVec.Normalize();
		Arrow.Length = 160.0f;
		Arrow.EndPosition = Arrow.StartPosition + (FinalDirectionVec * Arrow.Length);
		Arrow.LabelPosition = Arrow.EndPosition + FVector(0, 0, 30);
		Arrow.Label = FString::Printf(TEXT("5.ATTACK: %s"),
			*UDebugUtils::FormatAttackDirectionDebug(AttackDir));
		Arrow.Color = FColor(255, 0, 255);
		Arrow.Thickness = 4.0f;
		Arrow.bIsDashed = false;
		Data.Arrows.Add(Arrow);
	}

	// Calculate arc points
	const float ArcRadius = 100.0f;
	const float AbsYawDelta = FMath::Abs(Data.YawDelta);
	if (AbsYawDelta > 5.0f)
	{
		const int32 NumArcSegments = FMath::Max(3, FMath::CeilToInt(AbsYawDelta / 10.0f));
		const float StartAngle = CharacterRotation.Yaw * (PI / 180.0f);
		const float AngleStep = (Data.YawDelta * (PI / 180.0f)) / NumArcSegments;

		for (int32 i = 0; i <= NumArcSegments; ++i)
		{
			const float Angle = StartAngle + (AngleStep * i);
			const FVector Point = CharacterLocation + Data.ChestOffset +
				FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * ArcRadius;
			Data.ArcPoints.Add(Point);
		}
	}

	return Data;
}

FColor UCombatComponent::GetPhaseDebugColor() const
{
	switch (CurrentPhase)
	{
		case EAttackPhase::Windup:   return FColor::Orange;
		case EAttackPhase::Active:   return FColor::Red;
		case EAttackPhase::Recovery: return FColor::Yellow;
		default:                      return FColor::White;
	}
}

bool UCombatComponent::ShouldUseDashedArrowForInput() const
{
	return HoldState.IsHolding();
}

#endif // WITH_AUTOMATION_TESTS

// ============================================================================
// PAIRED ANIMATION PARTNER TRACKING
// ============================================================================

void UCombatComponent::AddPairedPartner(AActor* Partner)
{
	if (!Partner)
	{
		return;
	}

	// Check if already tracked (avoid duplicates)
	for (const TWeakObjectPtr<AActor>& Existing : PairedAnimationPartners)
	{
		if (Existing.Get() == Partner)
		{
			return;  // Already tracked
		}
	}

	PairedAnimationPartners.Add(Partner);

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[PAIRED] Added partner: %s (Total: %d)"),
			*Partner->GetName(), PairedAnimationPartners.Num());
	}
}

void UCombatComponent::RemovePairedPartner(AActor* Partner)
{
	if (!Partner)
	{
		return;
	}

	for (int32 i = PairedAnimationPartners.Num() - 1; i >= 0; --i)
	{
		if (PairedAnimationPartners[i].Get() == Partner)
		{
			PairedAnimationPartners.RemoveAt(i);

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[PAIRED] Removed partner: %s (Remaining: %d)"),
					*Partner->GetName(), PairedAnimationPartners.Num());
			}
			return;
		}
	}
}

void UCombatComponent::ClearPairedPartners()
{
	const int32 Count = PairedAnimationPartners.Num();
	PairedAnimationPartners.Empty();

	if (GetDebugDraw() && Count > 0)
	{
		UE_LOG(LogCombat, Log, TEXT("[PAIRED] Cleared all partners (was %d)"), Count);
	}
}

bool UCombatComponent::IsPairedPartner(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& Partner : PairedAnimationPartners)
	{
		if (Partner.Get() == Actor)
		{
			return true;
		}
	}

	return false;
}

// ============================================================================
// PAIRED ANIMATION EFFECT HANDLING
// ============================================================================

void UCombatComponent::BeginPairedAnimation(UPairedAnimationData* PairedAnimData, EPairedReactionType ReactionType, bool bIsCriticalMoment)
{
	if (!PairedAnimData)
	{
		UE_LOG(LogCombat, Warning, TEXT("[PAIRED EFFECTS] BeginPairedAnimation called with null PairedAnimData"));
		return;
	}

	// Store active paired animation data for effect handlers
	ActivePairedAnimData = PairedAnimData;
	ActivePairedReactionType = ReactionType;

	// Block combat input during paired animation (prevents accidental buffering)
	bBlockCombatInput = true;

	// Apply slow motion if configured
	if (bIsCriticalMoment && PairedAnimData->bApplySlowMotion)
	{
		// Apply slow motion via utility library
		UCinematicEffectsUtilityLibrary::ApplySlowMotion(GetWorld(), PairedAnimData->SlowMotionScale);

		// Set up restoration timer as safeguard against permanent slow-mo
		if (UWorld* World = GetWorld())
		{
			// Clear any existing timer first
			if (SlowMotionRestoreHandle.IsValid())
			{
				World->GetTimerManager().ClearTimer(SlowMotionRestoreHandle);
			}

			// Set timer for restoration
			World->GetTimerManager().SetTimer(
				SlowMotionRestoreHandle,
				this,
				&UCombatComponent::OnSlowMotionTimerExpired,
				PairedAnimData->SlowMotionDuration,
				false
			);

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[PAIRED EFFECTS] Slow motion applied: Scale=%.2f, Duration=%.2fs"),
					PairedAnimData->SlowMotionScale, PairedAnimData->SlowMotionDuration);
			}
		}
	}

	// Broadcast delegate for external systems
	OnPairedAnimationStarted.Broadcast(ReactionType, bIsCriticalMoment);

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[PAIRED EFFECTS] Started paired animation: %s (Type: %d, Critical: %d, SlowMo: %d)"),
			*PairedAnimData->GetDisplayName(),
			static_cast<int32>(ReactionType),
			bIsCriticalMoment,
			PairedAnimData->bApplySlowMotion);
	}
}

void UCombatComponent::EndPairedAnimation()
{
	// Capture reaction type before clearing (needed for delegate)
	const EPairedReactionType ReactionType = ActivePairedReactionType;

	// Clear the restoration timer if it's running
	if (SlowMotionRestoreHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SlowMotionRestoreHandle);
		}
	}

	// Ensure time dilation is restored (safeguard)
	UCinematicEffectsUtilityLibrary::RestoreTimeDilation(GetWorld());

	// Clear active paired animation data
	ActivePairedAnimData = nullptr;
	ActivePairedReactionType = EPairedReactionType::None;

	// Restore combat input
	bBlockCombatInput = false;

	// Broadcast delegate for external systems
	OnPairedAnimationEnded.Broadcast(ReactionType);

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[PAIRED EFFECTS] Ended paired animation (Type: %d)"),
			static_cast<int32>(ReactionType));
	}
}

void UCombatComponent::TriggerSyncPointEffects(FName SyncPointName)
{
	// Play camera shake if configured
	if (ActivePairedAnimData && ActivePairedAnimData->ImpactCameraShake)
	{
		UCinematicEffectsUtilityLibrary::PlayCameraShakeOnActor(GetOwner(), ActivePairedAnimData->ImpactCameraShake);

		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[PAIRED EFFECTS] Camera shake played: %s"),
				*ActivePairedAnimData->ImpactCameraShake->GetName());
		}
	}

	// Broadcast sync point delegate (for damage application, audio, etc.)
	OnPairedAnimationSyncPoint.Broadcast(ActivePairedReactionType, SyncPointName);

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[PAIRED EFFECTS] Sync point triggered: %s (Type: %d, CameraShake: %s)"),
			*SyncPointName.ToString(),
			static_cast<int32>(ActivePairedReactionType),
			ActivePairedAnimData && ActivePairedAnimData->ImpactCameraShake ? TEXT("Yes") : TEXT("No"));
	}
}

void UCombatComponent::OnSlowMotionTimerExpired()
{
	// Restore time dilation via utility library
	UCinematicEffectsUtilityLibrary::RestoreTimeDilation(GetWorld());

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[PAIRED EFFECTS] Slow motion timer expired - time dilation restored"));
	}
}

// ============================================================================
// PAIRED ANIMATION INTERRUPT HANDLING
// ============================================================================

void UCombatComponent::OnPairedPartnerDeath(AActor* DeadPartner)
{
	if (!DeadPartner)
	{
		return;
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Warning, TEXT("[PAIRED INTERRUPT] Partner %s died during paired animation"),
			*DeadPartner->GetName());
	}

	// Only interrupt if we're actually in a paired animation with this partner
	if (!IsPairedPartner(DeadPartner))
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogCombat, Log, TEXT("[PAIRED INTERRUPT] %s was not a tracked partner, ignoring"),
				*DeadPartner->GetName());
		}
		return;
	}

	// Remove the dead partner from our list
	RemovePairedPartner(DeadPartner);

	// If we were in an active paired animation, cancel it
	if (IsPairedAnimationActive())
	{
		CancelPairedAnimation();
	}
}

void UCombatComponent::CancelPairedAnimation(float BlendOutTime)
{
	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Warning, TEXT("[PAIRED INTERRUPT] Cancelling paired animation (BlendOutTime: %.2fs)"),
			BlendOutTime);
	}

	// Stop any playing montage on the owner
	if (AActor* Owner = GetOwner())
	{
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
			{
				// Stop montage with blend out
				AnimInstance->Montage_Stop(BlendOutTime);

				if (GetDebugDraw())
				{
					UE_LOG(LogCombat, Log, TEXT("[PAIRED INTERRUPT] Montage stopped on %s"),
						*Owner->GetName());
				}
			}
		}
	}

	// Clear all partners
	ClearPairedPartners();

	// End the paired animation effects (restores time dilation, broadcasts delegate)
	EndPairedAnimation();

	// Reset to idle phase
	SetPhase(EAttackPhase::None);

	// Clear any queued actions that might have been for the paired sequence
	ClearQueue(false);

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[PAIRED INTERRUPT] ✓ Paired animation cancelled - state reset"));
	}
}