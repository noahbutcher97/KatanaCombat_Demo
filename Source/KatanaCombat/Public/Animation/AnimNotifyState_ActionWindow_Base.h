// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ActionQueueTypes.h"
#include "AnimNotifyState_ActionWindow_Base.generated.h"

/**
 * Base class for action window notify states
 *
 * Provides a unified interface for checkpoint discovery in MontageUtilityLibrary.
 * Any subclass is automatically detected and registered during DiscoverCheckpoints().
 *
 * Window Timing Strategy:
 * ========================
 * 1. DEFAULT (Inferred): Combo/Parry windows are inferred from phase transitions
 *    - Combo: Full montage duration (input always buffered)
 *    - Parry: Active phase duration (when attacker is vulnerable)
 *    - See AnimNotify_AttackPhaseTransition for phase transition events
 *
 * 2. EXPLICIT (AnimNotifyState): Add to montage for specialized timing
 *    - Overrides/supplements inferred windows
 *    - Use for narrower parry windows on heavy attacks
 *    - Use for extended combo windows on finishers
 *    - Use for hold windows (always explicit - requires duration tracking)
 *
 * Available Subclasses:
 * - UAnimNotifyState_HoldWindow   (EActionWindowType::Hold)
 * - UAnimNotifyState_ParryWindow  (EActionWindowType::Parry)
 * - UAnimNotifyState_ComboWindow  (EActionWindowType::Combo)
 * - [Future] UAnimNotifyState_CancelWindow (EActionWindowType::Cancel)
 *
 * Checkpoint Discovery Integration:
 * =================================
 * MontageUtilityLibrary::DiscoverCheckpoints() automatically:
 * 1. Scans for AnimNotifyState_ActionWindow_Base subclasses
 * 2. Calls GetWindowType() to identify the window type
 * 3. Creates FTimerCheckpoint with timing from the AnimNotifyState
 *
 * Usage:
 * 1. Create subclass: class UAnimNotifyState_MyWindow : public UAnimNotifyState_ActionWindow_Base
 * 2. Override GetWindowType() to return appropriate EActionWindowType
 * 3. Add to montage in editor - automatically discovered by combat system
 */
UCLASS(Abstract)
class KATANACOMBAT_API UAnimNotifyState_ActionWindow_Base : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_ActionWindow_Base();

	// ============================================================================
	// ANIMNOTIFYSTATE INTERFACE
	// ============================================================================

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	// ============================================================================
	// WINDOW TYPE IDENTIFICATION
	// ============================================================================

	/**
	 * Get the window type for checkpoint registration
	 * Called by MontageUtilityLibrary::DiscoverCheckpoints() to identify window type
	 * Must be overridden by subclasses
	 *
	 * @return The EActionWindowType this notify represents
	 */
	virtual EActionWindowType GetWindowType() const PURE_VIRTUAL(UAnimNotifyState_ActionWindow_Base::GetWindowType, return EActionWindowType::Combo;);
};