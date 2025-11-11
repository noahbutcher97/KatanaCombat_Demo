// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CombatComponentV2.h"
#include "Core/CombatComponent.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "Characters/SamuraiCharacter.h"
#include "Utilities/MontageUtilityLibrary.h"

UCombatComponentV2::UCombatComponentV2()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UCombatComponentV2::BeginPlay()
{
	Super::BeginPlay();

	// Cache owner character (ASamuraiCharacter instead of AActor for performance)
	OwnerCharacter = Cast<ASamuraiCharacter>(GetOwner());
	if (OwnerCharacter)
	{
		// Cache combat settings from character
		CombatSettings = OwnerCharacter->CombatSettings;

		// Cache CombatComponent reference for shared state access
		CombatComponent = OwnerCharacter->FindComponentByClass<UCombatComponent>();
		if (!CombatComponent)
		{
			UE_LOG(LogTemp, Error, TEXT("[CombatComponentV2] No CombatComponent found on %s"), *OwnerCharacter->GetName());
		}

		// Bind to montage event delegates for event-driven phase transitions
		if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
		{
			AnimInstance->OnMontageBlendingOut.AddDynamic(this, &UCombatComponentV2::OnMontageBlendingOut);
			AnimInstance->OnMontageEnded.AddDynamic(this, &UCombatComponentV2::OnMontageEnded);

			if (GetDebugDraw())
			{
				UE_LOG(LogTemp, Log, TEXT("[V2 INIT] Montage event delegates bound (BlendingOut, Ended)"));
			}
		}
	}
}

void UCombatComponentV2::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CombatComponent)
	{
		return;
	}

	// Get current montage time using utility library (encapsulates repetitive null checks)
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	float CurrentTime = UMontageUtilityLibrary::GetCurrentMontageTime(Character);

	if (CurrentTime >= 0.0f) // Valid montage time
	{
		// Process queue at current montage time
		ProcessQueue(CurrentTime);

		// Clear expired checkpoints
		ClearExpiredCheckpoints(CurrentTime);

		// Check commit window expiration
		if (bInCommitWindow && !IsInCommitWindow()) //TODO:: Check is redundant (bInCommitWindow && !IsInCommitWindow()))
		{
			bInCommitWindow = false;

			if (GetDebugDraw())
			{
				UE_LOG(LogTemp, Log, TEXT("[V2 COMMIT] Commit window ended (%.2fs elapsed)"),
					CurrentTime - CommitWindowStartTime);
			}
		}
	}

	// Update hold playrate if active
	if (HoldState.bIsHolding)
	{
		UpdateHoldPlayRate(DeltaTime);
	}

	// Debug visualization
	if (GetDebugDraw())
	{
		DrawDebugInfo();
	}
}

ASamuraiCharacter* UCombatComponentV2::GetOwnerCharacter() const
{
	// Return cached owner character (no cast needed - already cached in BeginPlay)
	return OwnerCharacter;
}

bool UCombatComponentV2::GetDebugDraw() const
{
	// Read debug draw setting from cached combat settings
	return CombatSettings && CombatSettings->bDebugDraw;
}

// ============================================================================
// INPUT PROCESSING (V2)
// ============================================================================

void UCombatComponentV2::OnInputEvent(EInputType InputType, EInputEventType EventType)
{
	// Early exit if V2 system is not enabled or dependencies missing
	if (!CombatSettings || !CombatSettings->bUseV2System || !CombatComponent)
	{
		return;
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
			UE_LOG(LogTemp, Log, TEXT("[V2 INPUT] %s PRESSED at %.2f (Combo: %s)"),
				*UEnum::GetValueAsString(InputType),
				CurrentTime,
				bComboWindowActive ? TEXT("YES") : TEXT("NO"));
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
			UE_LOG(LogTemp, Log, TEXT("[V2 INPUT] %s RELEASED at %.2f"),
				*UEnum::GetValueAsString(InputType),
				CurrentTime);
		}

		// Handle hold deactivation
		if (HoldState.bIsHolding && HoldState.HeldInputType == InputType)
		{
			DeactivateHold();
		}
	}

	// Queue action for processing
	QueueAction(InputAction);

	// Update stats
	QueueStats.TotalInputs++;
}

bool UCombatComponentV2::CanProcessInput(EInputType InputType) const
{
	if (!CombatComponent)
	{
		return false;
	}

	// V2 accepts input in more states than V1
	ECombatState CurrentState = CombatComponent->GetCombatState();

	switch (CurrentState)
	{
		case ECombatState::Idle:
		case ECombatState::Attacking:
		case ECombatState::Blocking:
			return true;

		case ECombatState::Dead:
		case ECombatState::HitStunned:
		case ECombatState::GuardBroken:
			return false;

		default:
			return true;
	}
}

// ============================================================================
// ACTION QUEUE MANAGEMENT
// ============================================================================

void UCombatComponentV2::QueueAction(const FQueuedInputAction& InputAction, UAttackData* AttackData)
{
	if (!CombatComponent)
	{
		return;
	}

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

	// Create queue entry
	FActionQueueEntry Entry(InputAction, AttackData, ExecMode);
	Entry.Priority = CalculatePriority(Entry);

	// Immediate mode: Execute synchronously (right now)
	if (ExecMode == EActionExecutionMode::Immediate)
	{
		// Commit window check: Block immediate execution to prevent animation restart spam
		if (IsInCommitWindow())
		{
			if (GetDebugDraw())
			{
				ASamuraiCharacter* Character = GetOwnerCharacter();
				float ElapsedTime = Character ? UMontageUtilityLibrary::GetCurrentMontageTime(Character) - CommitWindowStartTime : 0.0f;
				UE_LOG(LogTemp, Warning, TEXT("[V2 QUEUE] Immediate execution BLOCKED by commit window (%.2fs / %.2fs elapsed)"),
					ElapsedTime, CommitWindowDuration);
			}
			return; // Reject immediate execution during commit window
		}

		// CRITICAL: Clear all pending queued actions before starting new attack
		// This prevents old buffered inputs from executing after the new action
		if (ActionQueue.Num() > 0)
		{
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
				UE_LOG(LogTemp, Warning, TEXT("[V2 QUEUE] Cleared %d pending actions before IMMEDIATE execution"), ClearedCount);
			}
		}

		if (GetDebugDraw())
		{
			UE_LOG(LogTemp, Log, TEXT("[V2 QUEUE] Executing IMMEDIATE action: Type=%s"),
				*UEnum::GetValueAsString(InputAction.InputType));
		}

		// Execute immediately
		if (ExecuteAction(Entry))
		{
			// Start commit window to prevent input spam from restarting animation
			StartCommitWindow();

			QueueStats.ActionsExecuted++;
			QueueStats.ImmediateExecutions++;

			if (GetDebugDraw())
			{
				UE_LOG(LogTemp, Log, TEXT("[V2 QUEUE] Immediate execution SUCCESS"));
			}
		}
		else
		{
			if (GetDebugDraw())
			{
				UE_LOG(LogTemp, Warning, TEXT("[V2 QUEUE] Immediate execution FAILED"));
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
		UE_LOG(LogTemp, Log, TEXT("[V2 QUEUE] Added queued action: Type=%s, Mode=%s, Scheduled=%.2f, Priority=%d"),
			*UEnum::GetValueAsString(InputAction.InputType),
			*UEnum::GetValueAsString(ExecMode),
			Entry.ScheduledTime,
			Entry.Priority);
	}
}

void UCombatComponentV2::ProcessQueue(float CurrentMontageTime)
{
	if (ActionQueue.Num() == 0)
	{
		return;
	}

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
					UE_LOG(LogTemp, Log, TEXT("[V2 QUEUE] Executed action at %.2f (scheduled: %.2f)"),
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
					UE_LOG(LogTemp, Warning, TEXT("[V2 QUEUE] Action execution failed at %.2f, keeping in queue"),
						CurrentMontageTime);
				}
			}
		}
	}
}

bool UCombatComponentV2::ExecuteAction(FActionQueueEntry& Action)
{
	if (!Action.AttackData)
	{
		return false;
	}

	Action.State = EActionState::Executing;

	// V2 executes independently - does NOT call V1's ExecuteAttack
	bool bSuccess = false;

	switch (Action.InputAction.InputType)
	{
		case EInputType::LightAttack:
		case EInputType::HeavyAttack:
		{
			// Play montage independently (V2 does NOT touch V1 state machine)
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

				// Broadcast attack started event
				bool bIsCombo = (CurrentPhase == EAttackPhase::Recovery || CurrentPhase == EAttackPhase::Active);
				OnAttackStarted.Broadcast(Action.AttackData, Action.InputAction.InputType, bIsCombo);

				if (GetDebugDraw())
				{
					FString SectionName = Action.AttackData->MontageSection.IsNone() ?
						TEXT("Default") : Action.AttackData->MontageSection.ToString();

					UE_LOG(LogTemp, Log, TEXT("[V2 EXECUTE] ═══════════════════════════════════════"));
					UE_LOG(LogTemp, Log, TEXT("[V2 EXECUTE] Attack Data: %s"), *Action.AttackData->GetName());
					UE_LOG(LogTemp, Log, TEXT("[V2 EXECUTE] Montage: %s"), *Action.AttackData->AttackMontage->GetName());
					UE_LOG(LogTemp, Log, TEXT("[V2 EXECUTE] Section: %s"), *SectionName);
					UE_LOG(LogTemp, Log, TEXT("[V2 EXECUTE] Input Type: %s"), *UEnum::GetValueAsString(CurrentAttackInputType));
					UE_LOG(LogTemp, Log, TEXT("[V2 EXECUTE] Is Combo: %s"), bIsCombo ? TEXT("YES") : TEXT("NO"));
					UE_LOG(LogTemp, Log, TEXT("[V2 EXECUTE] Checkpoints Discovered: %d"), Checkpoints.Num());
					UE_LOG(LogTemp, Log, TEXT("[V2 EXECUTE] ═══════════════════════════════════════"));
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

bool UCombatComponentV2::PlayAttackMontage(UAttackData* AttackData)
{
	if (!AttackData || !AttackData->AttackMontage)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogTemp, Warning, TEXT("[V2 MONTAGE] Failed - Invalid AttackData or Montage"));
		}
		return false;
	}

	ASamuraiCharacter* Character = GetOwnerCharacter();
	if (!Character || !Character->GetMesh())
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogTemp, Warning, TEXT("[V2 MONTAGE] Failed - No character or mesh"));
		}
		return false;
	}

	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogTemp, Warning, TEXT("[V2 MONTAGE] Failed - No AnimInstance"));
		}
		return false;
	}

	// Play montage (V2 does NOT touch V1 state machine)
	// Note: OnMontageEnded delegate already bound in BeginPlay() for event-driven phase management
	const float PlayRate = 1.0f;
	const float StartPosition = 0.0f;
	AnimInstance->Montage_Play(AttackData->AttackMontage, PlayRate, EMontagePlayReturnType::MontageLength, StartPosition);

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
				UE_LOG(LogTemp, Log, TEXT("[V2 MONTAGE] Section-only mode: %s (no auto-advance)"),
					*AttackData->MontageSection.ToString());
			}
		}
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogTemp, Log, TEXT("[V2 MONTAGE] Playing: %s | Section: %s | Delegate bound"),
			*AttackData->AttackMontage->GetName(),
			*AttackData->MontageSection.ToString());
	}

	return true;
}

void UCombatComponentV2::ClearQueue(bool bCancelCurrent)
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

	if ( GetDebugDraw())
	{
		UE_LOG(LogTemp, Log, TEXT("[V2 QUEUE] Cleared (CancelCurrent=%s) - Combo state reset"), bCancelCurrent ? TEXT("YES") : TEXT("NO"));
	}
}

void UCombatComponentV2::CancelActionsWithPriority(int32 MinPriority)
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
				UE_LOG(LogTemp, Log, TEXT("[V2 QUEUE] Cancelled action (Priority %d < %d)"),
					Entry.Priority, MinPriority);
			}
		}
	}
}

// ============================================================================
// TIMER CHECKPOINT SYSTEM
// ============================================================================

void UCombatComponentV2::DiscoverCheckpoints(UAnimMontage* Montage)
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
		UE_LOG(LogTemp, Log, TEXT("[V2 CHECKPOINTS] Discovered %d checkpoints from montage: %s"),
			NumDiscovered,
			*Montage->GetName());

		// Log discovered checkpoints
		UMontageUtilityLibrary::LogCheckpoints(Checkpoints, TEXT("V2 DISCOVERY"));
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

void UCombatComponentV2::RegisterCheckpoint(EActionWindowType WindowType, float StartTime, float Duration)
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
		UE_LOG(LogTemp, Log, TEXT("[V2 CHECKPOINTS] Registered: Type=%s, Start=%.2f, Duration=%.2f"),
			*UEnum::GetValueAsString(WindowType),
			StartTime,
			Duration);
	}
}

bool UCombatComponentV2::HasReachedCheckpoint(const FTimerCheckpoint& Checkpoint, float CurrentTime) const
{
	return Checkpoint.bActive && CurrentTime >= Checkpoint.MontageTime;
}

float UCombatComponentV2::GetExecutionCheckpoint(const FActionQueueEntry& Action) const
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
				UE_LOG(LogTemp, Log, TEXT("[V2 CHECKPOINT] Found Active-end checkpoint at %.2f for queued execution"),
					Checkpoint.MontageTime);
			}

			return Checkpoint.MontageTime;
		}
	}

	// Checkpoint not found yet - use sentinel value to indicate "execute at Active-end"
	// The checkpoint will be created when Active→Recovery transition happens via OnPhaseTransition
	if (GetDebugDraw())
	{
		UE_LOG(LogTemp, Log, TEXT("[V2 CHECKPOINT] Active-end checkpoint not found yet, will execute when created"));
	}

	//TODO: Consider warning if no Active-end checkpoint found after montage ends
	return -1.0f; // Sentinel: Execute at Active-end (checkpoint will be created later)
}

// ============================================================================
// HOLD SYSTEM (V2)
// ============================================================================

void UCombatComponentV2::CheckHoldActivation()
{
	if (!CombatComponent || HoldState.bActivatedThisAttack)
	{
		return;
	}

	// Find hold checkpoint
	FTimerCheckpoint* HoldCheckpoint = FindCheckpoint(EActionWindowType::Hold);
	if (!HoldCheckpoint)
	{
		return;
	}

	// Check if any attack input is currently held
	for (const TPair<EInputType, float>& Pair : HeldInputs)
	{
		if (Pair.Key == EInputType::LightAttack || Pair.Key == EInputType::HeavyAttack)
		{
			// Input is held - activate hold
			ActivateHold(Pair.Key, 0.2f); // Slow playrate
			break;
		}
	}
}

void UCombatComponentV2::ActivateHold(EInputType InputType, float PlayRate)
{
	if (!CombatComponent)
	{
		return;
	}

	HoldState.Activate(InputType, GetWorld()->GetTimeSeconds(), PlayRate);

	// Apply playrate to montage using utility library
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UMontageUtilityLibrary::SetMontagePlayRate(Character, PlayRate);

	if ( GetDebugDraw())
	{
		UE_LOG(LogTemp, Log, TEXT("[V2 HOLD] Activated: Input=%s, PlayRate=%.2f"),
			*UEnum::GetValueAsString(InputType),
			PlayRate);
	}
}

void UCombatComponentV2::DeactivateHold()
{
	if (!HoldState.bIsHolding)
	{
		return;
	}

	HoldState.Deactivate();

	// Restore normal playrate using utility library
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UMontageUtilityLibrary::SetMontagePlayRate(Character, 1.0f);

	if ( GetDebugDraw())
	{
		UE_LOG(LogTemp, Log, TEXT("[V2 HOLD] Deactivated"));
	}
}

void UCombatComponentV2::UpdateHoldPlayRate(float DeltaTime)
{
	// Hold playrate updates could be implemented here
	// For now, hold maintains constant playrate set at activation
}

// ============================================================================
// PHASE TRANSITION SYSTEM (V2)
// ============================================================================

void UCombatComponentV2::OnPhaseTransition(EAttackPhase NewPhase)
{
	// CRITICAL: Update CurrentPhase FIRST before any other logic
	// This ensures DetermineExecutionMode sees the correct phase for incoming input
	SetPhase(NewPhase);

	// Register phase transition as a checkpoint for execution timing (V1 AnimNotify integration)
	// Queued inputs (buffered during Windup/Active) execute when Active ends (transitioning to Recovery)
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
		{
			if (UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage())
			{
				float CurrentTime = AnimInstance->Montage_GetPosition(CurrentMontage);

				// Only register checkpoint when transitioning TO Recovery (end of Active phase)
				// This is when queued inputs should execute
				if (NewPhase == EAttackPhase::Recovery)
				{
					// Active phase ended - register execution checkpoint for queued inputs
					// Use EActionWindowType::Combo to identify this as the "Active end" checkpoint
					RegisterCheckpoint(EActionWindowType::Combo, CurrentTime, 0.0f);

					if (GetDebugDraw())
					{
						UE_LOG(LogTemp, Log, TEXT("[V2 PHASE] Registered Active-end checkpoint at %.2f for queued input execution"),
							CurrentTime);
					}
				}
			}
		}
	}
}

void UCombatComponentV2::SetPhase(EAttackPhase NewPhase)
{
	if (CurrentPhase == NewPhase)
	{
		return; // No change needed
	}

	EAttackPhase OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;

	if (GetDebugDraw())
	{
		UE_LOG(LogTemp, Log, TEXT("[V2 PHASE] Phase transition: %d → %d"),
			static_cast<int32>(OldPhase),
			static_cast<int32>(NewPhase));
	}

	// Broadcast phase changed event
	OnPhaseChanged.Broadcast(OldPhase, NewPhase);

	// Handle phase-specific logic
	switch (NewPhase)
	{
		case EAttackPhase::None:
			// Attack finished - reset combo state for next attack
			CurrentAttackData = nullptr;
			CurrentAttackInputType = EInputType::None;

			// Clear commit window (attack completely finished)
			bInCommitWindow = false;

			// Clear hold state for next attack
			if (HoldState.bIsHolding)
			{
				HoldState.bActivatedThisAttack = false;
			}

			if (GetDebugDraw())
			{
				UE_LOG(LogTemp, Log, TEXT("[V2 PHASE] Attack finished - Combo state reset"));
			}
			break;

		default:
			break;
	}
}
// TODO:: Consider adding OnPhaseEnter/Exit events for more granular control --> Also we know that active phase is when queued input actions can be executed so perhaps having phase specific logic for simple things like this would be prudent

void UCombatComponentV2::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (GetDebugDraw())
	{
		UE_LOG(LogTemp, Log, TEXT("[V2 MONTAGE] Montage blending out: %s | Interrupted: %s"),
			Montage ? *Montage->GetName() : TEXT("None"),
			bInterrupted ? TEXT("YES") : TEXT("NO"));
	}

	// Broadcast montage event
	OnMontageEvent.Broadcast(Montage, bInterrupted, FName("BlendingOut"));

	// Prepare for next attack during blend out (smoother transitions)
	// Phase transition to None happens in OnMontageEnded
	//TODO:: Consider allowing new attack to start here during blend out? Place some helpful checks and transition smoothing here of some sort.
}

void UCombatComponentV2::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (GetDebugDraw())
	{
		UE_LOG(LogTemp, Log, TEXT("[V2 MONTAGE] Montage ended: %s | Interrupted: %s"),
			Montage ? *Montage->GetName() : TEXT("None"),
			bInterrupted ? TEXT("YES") : TEXT("NO"));
	}

	// Broadcast montage event
	OnMontageEvent.Broadcast(Montage, bInterrupted, FName("Ended"));

	// CRITICAL: Execute any pending queued actions BEFORE clearing state
	// BUT only if their checkpoint was actually reached (ScheduledTime >= 0)
	// Actions with ScheduledTime=-1.0 mean checkpoint was never registered → discard them
	if (ActionQueue.Num() > 0)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogTemp, Warning, TEXT("[V2 MONTAGE] Montage ended with %d queued actions - checking which are ready"), ActionQueue.Num());
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
						UE_LOG(LogTemp, Warning, TEXT("[V2 QUEUE] Discarding action (checkpoint never reached): Type=%s, ScheduledTime=%.2f"),
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
						UE_LOG(LogTemp, Log, TEXT("[V2 QUEUE] Executing action from ended montage: Type=%s, ScheduledTime=%.2f, MontageEndTime=%.2f"),
							*UEnum::GetValueAsString(Entry.InputAction.InputType), Entry.ScheduledTime, MontageEndTime);
					}

					// Execute the pending action
					if (ExecuteAction(Entry))
					{
						ActionQueue.RemoveAt(i);

						// Start commit window
						StartCommitWindow();

						QueueStats.ActionsExecuted++;

						// Only execute the first valid action (FIFO)
						break;
					}
				}
				else
				{
					if (GetDebugDraw())
					{
						UE_LOG(LogTemp, Warning, TEXT("[V2 QUEUE] Discarding action (montage ended before checkpoint): Type=%s, ScheduledTime=%.2f, MontageEndTime=%.2f"),
							*UEnum::GetValueAsString(Entry.InputAction.InputType), Entry.ScheduledTime, MontageEndTime);
					}

					// Discard action - montage ended before checkpoint
					ActionQueue.RemoveAt(i);
					QueueStats.ActionsCancelled++;
				}
			}
		}
	}

	// Transition to None phase when montage completes (if no new attack started)
	if (CurrentPhase != EAttackPhase::Windup && CurrentPhase != EAttackPhase::Active)
	{
		SetPhase(EAttackPhase::None);
	}

	// Clear checkpoints for finished montage (new montage will have its own)
	Checkpoints.Empty();
}

//NOTE:: OnMontageEnded is where queued actions get a last chance to execute if their checkpoint was reached.
// ============================================================================
// STATE QUERIES
// ============================================================================

int32 UCombatComponentV2::GetPendingActionCount() const
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

float UCombatComponentV2::GetHoldDuration() const
{
	return HoldState.GetHoldDuration(GetWorld()->GetTimeSeconds());
}

// ============================================================================
// DEBUG / VISUALIZATION
// ============================================================================

void UCombatComponentV2::DrawDebugInfo() const
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
	FString QueueInfo = FString::Printf(TEXT("V2 Queue: %d pending | %d total"),
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
	if (HoldState.bIsHolding)
	{
		FString HoldInfo = FString::Printf(TEXT("HOLDING: %s (%.2fs)"),
			*UEnum::GetValueAsString(HoldState.HeldInputType),
			GetHoldDuration());

		DrawDebugString(GetWorld(), OwnerLocation + Offset * 2.5f, HoldInfo, nullptr, FColor::Yellow, 0.0f, true);
	}

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
			UMontageUtilityLibrary::LogCheckpoints(Checkpoints, TEXT("V2 DEBUG"));
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

void UCombatComponentV2::ProcessInputPair(const FQueuedInputAction& PressEvent, const FQueuedInputAction& ReleaseEvent)
{
	// Calculate hold duration
	float HoldDuration = ReleaseEvent.Timestamp - PressEvent.Timestamp;

	if ( GetDebugDraw())
	{
		UE_LOG(LogTemp, Log, TEXT("[V2 INPUT] Pair processed: %s held for %.2fs"),
			*UEnum::GetValueAsString(PressEvent.InputType),
			HoldDuration);
	}

	// Press/release pair processing logic
	// This could trigger special actions based on hold duration in future
}

EActionExecutionMode UCombatComponentV2::DetermineExecutionMode(const FQueuedInputAction& InputAction) const
{
	// ============================================================================
	// PHASE-BASED EXECUTION (V2 Smart Queue Management)
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

UAttackData* UCombatComponentV2::GetAttackForInput(EInputType InputType) const
{
	if (!CombatComponent)
	{
		return nullptr;
	}

	// Get default attacks as fallbacks
	UAttackData* DefaultLightAttack = CombatComponent->GetDefaultLightAttack();
	UAttackData* DefaultHeavyAttack = CombatComponent->GetDefaultHeavyAttack();

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
			UE_LOG(LogTemp, Log, TEXT("[V2 COMBO] Allowing combo from phase %s (CurrentAttack=%s)"),
				*UEnum::GetValueAsString(CurrentPhase),
				*CurrentAttackData->GetName());
		}
	}

	// Debug: Log combo resolution context
	if (GetDebugDraw())
	{
		UE_LOG(LogTemp, Warning, TEXT("[V2 COMBO DEBUG] GetAttackForInput: Phase=%s, CurrentAttack=%s, ComboWindow=%s, bShouldCombo=%s"),
			*UEnum::GetValueAsString(CurrentPhase),
			CurrentAttackData ? *CurrentAttackData->GetName() : TEXT("nullptr"),
			bComboWindowActive ? TEXT("ACTIVE") : TEXT("Inactive"),
			bShouldCombo ? TEXT("TRUE") : TEXT("FALSE"));
	}

	// Use utility to resolve combo or default attack
	UAttackData* ResolvedAttack = UMontageUtilityLibrary::ResolveNextAttack(
		CurrentAttackData,
		InputType,
		bShouldCombo,
		HoldState.bIsHolding,
		DefaultLightAttack,
		DefaultHeavyAttack,
		EAttackDirection::None // TODO: Add directional input detection
	);

	// Debug: Log resolved attack
	if (GetDebugDraw() && ResolvedAttack)
	{
		UE_LOG(LogTemp, Log, TEXT("[V2 COMBO RESOLVE] Resolved to: '%s' (bShouldCombo=%s)"),
			*ResolvedAttack->GetName(),
			bShouldCombo ? TEXT("TRUE") : TEXT("FALSE"));
	}

	return ResolvedAttack;
}

int32 UCombatComponentV2::CalculatePriority(const FActionQueueEntry& Action) const
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

void UCombatComponentV2::SortQueueByTime()
{
	ActionQueue.Sort([](const FActionQueueEntry& A, const FActionQueueEntry& B)
	{
		return A.ScheduledTime < B.ScheduledTime;
	});
}

FTimerCheckpoint* UCombatComponentV2::FindCheckpoint(EActionWindowType WindowType)
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

void UCombatComponentV2::ClearExpiredCheckpoints(float CurrentTime)
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
				UE_LOG(LogTemp, Log, TEXT("[V2 CHECKPOINTS] Expired: Type=%s at %.2f"),
					*UEnum::GetValueAsString(Checkpoint.WindowType),
					CurrentTime);
			}

			Checkpoints.RemoveAt(i);
		}
	}
}

void UCombatComponentV2::StartCommitWindow()
{
	ASamuraiCharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		return;
	}

	float CurrentMontageTime = UMontageUtilityLibrary::GetCurrentMontageTime(Character);

	bInCommitWindow = true;
	CommitWindowStartTime = CurrentMontageTime;

	if (GetDebugDraw())
	{
		UE_LOG(LogTemp, Log, TEXT("[V2 COMMIT] Commit window started at %.2f (duration: %.2fs)"),
			CurrentMontageTime, CommitWindowDuration);
	}
}

bool UCombatComponentV2::IsInCommitWindow() const
{
	if (!bInCommitWindow)
	{
		return false;
	}

	ASamuraiCharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		return false;
	}

	float CurrentMontageTime = UMontageUtilityLibrary::GetCurrentMontageTime(Character);
	float ElapsedTime = CurrentMontageTime - CommitWindowStartTime;

	// Still within commit window duration
	return ElapsedTime < CommitWindowDuration;
}

bool UCombatComponentV2::CanAcceptNewInput(EInputType InputType) const
{
	// V2 Design: Input is ALWAYS buffered during Windup/Active (queued execution)
	// Commit window only blocks IMMEDIATE execution during None/Recovery, not queuing
	// Therefore, commit window check is NOT in CanAcceptNewInput - it's in QueueAction

	// Block if queue already has pending actions of same type (prevents double-queueing)
	for (const FActionQueueEntry& Entry : ActionQueue)
	{
		if (Entry.IsPending() && Entry.InputAction.InputType == InputType)
		{
			if (GetDebugDraw())
			{
				UE_LOG(LogTemp, Warning, TEXT("[V2 INPUT] Input REJECTED - Already queued action of same type"));
			}
			return false;
		}
	}

	return true;
}