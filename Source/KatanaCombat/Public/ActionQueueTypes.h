// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"
#include "ActionQueueTypes.generated.h"

/**
 * V2 Combat System - Action Queue Data Structures
 *
 * This file defines all data types for the V2 timer-based action queue system.
 * Key concepts:
 * - Input events are timestamped and queued (not processed immediately)
 * - Timer checkpoints define when actions execute (snap vs responsive)
 * - Actions can be cancelled/replaced based on priority
 * - Hold states persist across combo chains
 */

// Forward declarations
class UAttackData;

/**
 * Input event types for the V2 system
 */
UENUM(BlueprintType)
enum class EInputEventType : uint8
{
	Press,
	Release
};

/**
 * Action execution timing modes (V2)
 */
UENUM(BlueprintType)
enum class EActionExecutionMode : uint8
{
	/** Queue for execution at Active phase end (input during Windup/Active) */
	Queued,

	/** Execute immediately on next tick (input during Idle/Recovery) */
	Immediate
};

/**
 * Action state in the queue
 */
UENUM(BlueprintType)
enum class EActionState : uint8
{
	/** Waiting for execution */
	Pending,

	/** Currently executing */
	Executing,

	/** Completed successfully */
	Completed,

	/** Cancelled before execution */
	Cancelled
};

/**
 * Window types for timer checkpoints
 */
UENUM(BlueprintType)
enum class EActionWindowType : uint8
{
	/** Combo input window (enables snap execution) */
	Combo,

	/** Parry detection window */
	Parry,

	/** Cancel/interrupt window */
	Cancel,

	/** Hold activation window */
	Hold,

	/** Recovery completion (base execution point) */
	Recovery
};

/**
 * Represents a single input event with timestamp (V2)
 */
USTRUCT(BlueprintType)
struct FQueuedInputAction
{
	GENERATED_BODY()

	/** Type of input (light/heavy attack, dodge, block) */
	UPROPERTY(BlueprintReadOnly, Category = "Input")
	EInputType InputType = EInputType::None;

	/** Press or release event */
	UPROPERTY(BlueprintReadOnly, Category = "Input")
	EInputEventType EventType = EInputEventType::Press;

	/** Game time when input occurred */
	UPROPERTY(BlueprintReadOnly, Category = "Input")
	float Timestamp = 0.0f;

	/** Was this input during a combo window? */
	UPROPERTY(BlueprintReadOnly, Category = "Input")
	bool bInComboWindow = false;

	FQueuedInputAction() = default;

	FQueuedInputAction(EInputType InType, EInputEventType InEvent, float InTime, bool bComboWindow = false)
		: InputType(InType)
		, EventType(InEvent)
		, Timestamp(InTime)
		, bInComboWindow(bComboWindow)
	{
	}

	/** Check if this is a press event */
	bool IsPress() const { return EventType == EInputEventType::Press; }

	/** Check if this is a release event */
	bool IsRelease() const { return EventType == EInputEventType::Release; }
};

/**
 * Timer checkpoint defining when an action can execute
 */
USTRUCT(BlueprintType)
struct FTimerCheckpoint
{
	GENERATED_BODY()

	/** Type of window this checkpoint represents */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	EActionWindowType WindowType = EActionWindowType::Recovery;

	/** Montage time when checkpoint is reached */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float MontageTime = 0.0f;

	/** Duration of the window (for validation) */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float Duration = 0.0f;

	/** Is this checkpoint active? */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	bool bActive = false;

	FTimerCheckpoint() = default;

	FTimerCheckpoint(EActionWindowType InType, float InTime, float InDuration)
		: WindowType(InType)
		, MontageTime(InTime)
		, Duration(InDuration)
		, bActive(false)
	{
	}
};

/**
 * Action queued for execution
 */
USTRUCT(BlueprintType)
struct FActionQueueEntry
{
	GENERATED_BODY()

	/** Input that triggered this action */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	FQueuedInputAction InputAction;

	/** Attack data to execute (if applicable) */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	UAttackData* AttackData = nullptr;

	/** When should this execute? */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	EActionExecutionMode ExecutionMode = EActionExecutionMode::Queued;

	/** Current state of this action */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	EActionState State = EActionState::Pending;

	/** Priority for cancellation (higher = harder to cancel) */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	int32 Priority = 0;

	/** Phase transition that triggers execution (event-driven - new system) */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	EAttackPhase TargetPhase = EAttackPhase::None;

	/** DEPRECATED: Checkpoint this action is scheduled at (old time-based system, kept for Phase 6 migration) */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	float ScheduledTime = 0.0f;

	FActionQueueEntry() = default;

	FActionQueueEntry(const FQueuedInputAction& InInput, UAttackData* InAttack, EActionExecutionMode InMode, int32 InPriority = 0)
		: InputAction(InInput)
		, AttackData(InAttack)
		, ExecutionMode(InMode)
		, State(EActionState::Pending)
		, Priority(InPriority)
		, TargetPhase(EAttackPhase::None)
		, ScheduledTime(0.0f)
	{
	}

	/** Check if this action can be cancelled by another action */
	bool CanBeCancelledBy(const FActionQueueEntry& Other) const
	{
		return Other.Priority >= Priority;
	}

	/** Check if action is pending execution */
	bool IsPending() const { return State == EActionState::Pending; }

	/** Check if action is currently executing */
	bool IsExecuting() const { return State == EActionState::Executing; }
};

/**
 * Hold state tracking (persists across attacks)
 */
USTRUCT(BlueprintType)
struct FHoldState
{
	GENERATED_BODY()

	/** Is player currently holding input? */
	UPROPERTY(BlueprintReadOnly, Category = "Hold")
	bool bIsHolding = false;

	/** Which input is being held? */
	UPROPERTY(BlueprintReadOnly, Category = "Hold")
	EInputType HeldInputType = EInputType::None;

	/** When did the hold start? */
	UPROPERTY(BlueprintReadOnly, Category = "Hold")
	float HoldStartTime = 0.0f;

	/** Current playrate during hold */
	UPROPERTY(BlueprintReadOnly, Category = "Hold")
	float CurrentPlayRate = 1.0f;

	/** Was hold activated during this attack? */
	UPROPERTY(BlueprintReadOnly, Category = "Hold")
	bool bActivatedThisAttack = false;

	/** Directional input during hold (for follow-ups) */
	UPROPERTY(BlueprintReadOnly, Category = "Hold")
	EAttackDirection HoldDirection = EAttackDirection::None;

	/** Ease transition state for light attack holds */
	UPROPERTY(BlueprintReadOnly, Category = "Hold")
	bool bIsEasing = false;

	/** Ease transition start time */
	UPROPERTY(BlueprintReadOnly, Category = "Hold")
	float EaseStartTime = 0.0f;

	/** Ease transition start playrate */
	UPROPERTY(BlueprintReadOnly, Category = "Hold")
	float EaseStartPlayRate = 1.0f;

	/** Is this an ease-out transition? (false = ease-in, true = ease-out) */
	UPROPERTY(BlueprintReadOnly, Category = "Hold")
	bool bIsEasingOut = false;

	FHoldState() = default;

	/** Activate hold state */
	void Activate(EInputType InputType, float CurrentTime, float PlayRate)
	{
		bIsHolding = true;
		HeldInputType = InputType;
		HoldStartTime = CurrentTime;
		CurrentPlayRate = PlayRate;
		bActivatedThisAttack = true;
	}

	/** Deactivate hold state */
	void Deactivate()
	{
		bIsHolding = false;
		HeldInputType = EInputType::None;
		HoldStartTime = 0.0f;
		CurrentPlayRate = 1.0f;
		HoldDirection = EAttackDirection::None;
		bIsEasing = false;
		EaseStartTime = 0.0f;
		EaseStartPlayRate = 1.0f;
		// Keep bActivatedThisAttack until reset
	}

	/** Reset for new attack chain */
	void Reset()
	{
		Deactivate();
		bActivatedThisAttack = false;
	}

	/** Get hold duration */
	float GetHoldDuration(float CurrentTime) const
	{
		return bIsHolding ? (CurrentTime - HoldStartTime) : 0.0f;
	}
};

/**
 * Queue statistics for debugging
 */
USTRUCT(BlueprintType)
struct FQueueStats
{
	GENERATED_BODY()

	/** Total inputs received */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 TotalInputs = 0;

	/** Actions executed */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 ActionsExecuted = 0;

	/** Actions cancelled */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 ActionsCancelled = 0;

	/** Queued executions (input buffered to Active-end) */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 QueuedExecutions = 0;

	/** Immediate executions (input executed synchronously) */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 ImmediateExecutions = 0;

	void Reset()
	{
		TotalInputs = 0;
		ActionsExecuted = 0;
		ActionsCancelled = 0;
		QueuedExecutions = 0;
		ImmediateExecutions = 0;
	}
};