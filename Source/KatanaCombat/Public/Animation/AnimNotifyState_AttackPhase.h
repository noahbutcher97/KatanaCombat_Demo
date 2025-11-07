
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CombatTypes.h"
#include "AnimNotifyState_AttackPhase.generated.h"

/**
 * DEPRECATED: Use AnimNotify_AttackPhaseTransition instead
 *
 * This NotifyState-based phase system has been replaced with a simpler event-based approach.
 * Attack phases are contiguous and implicit:
 * - Windup: Montage start → Active transition (implicit)
 * - Active: AnimNotify_AttackPhaseTransition(Active) → Recovery transition
 * - Recovery: AnimNotify_AttackPhaseTransition(Recovery) → Montage end (implicit)
 *
 * Migration:
 * - Replace 3 NotifyStates (Windup/Active/Recovery) with 2 transition events
 * - Add AnimNotify_AttackPhaseTransition at the END of Windup phase (transition to Active)
 * - Add AnimNotify_AttackPhaseTransition at the END of Active phase (transition to Recovery)
 * - Remove all AnimNotifyState_AttackPhase instances
 *
 * Benefits of new system:
 * - Simpler: 2 events instead of 6 (3 Begin + 3 End)
 * - No gaps: Phases are always defined by implicit boundaries
 * - Automatic hit detection: Enabled/disabled automatically with Active phase
 *
 * See: docs/PHASE_SYSTEM_MIGRATION.md for complete migration guide
 *
 * ---
 *
 * OLD DOCUMENTATION (for reference):
 * AnimNotifyState that marks attack phases (Windup, Active, Recovery, Hold)
 * Used by AttackData to read timing, and triggers callbacks in AnimInstance
 *
 * Usage:
 * 1. Add to attack montage in Animation Editor
 * 2. Set Phase property (Windup/Active/Recovery/Hold)
 * 3. Position at appropriate times in animation
 * 4. NotifyBegin/End automatically route to CombatComponent
 *
 * Attack Phase Timeline:
 * [──Windup──][──Active──][──Recovery──]
 *     30%         20%          50%
 *
 * - Windup: Telegraphing, can be parried, motion warping active
 * - Active: Hit detection enabled, damage dealt
 * - Recovery: Vulnerable, combo input window opens
 * - Hold: Optional pause for directional input (light attacks)
 *
 * This notify drives the combat system's timing. Multiple phases should not overlap.
 */
UCLASS(meta = (DisplayName = "Attack Phase (DEPRECATED - Use AnimNotify_AttackPhaseTransition)"))
class KATANACOMBAT_API UAnimNotifyState_AttackPhase : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_AttackPhase();

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Which phase this notify represents */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Phase")
	EAttackPhase Phase = EAttackPhase::Windup;

	// ============================================================================
	// ANIMNOTIFYSTATE INTERFACE
	// ============================================================================

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override { return true; }
#endif
};