// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "CombatTypes.h"
#include "AnimNotify_AttackPhaseTransition.generated.h"

/**
 * AnimNotify for attack phase transitions
 *
 * NEW PHASE SYSTEM (Simplified):
 * - Phases are CONTIGUOUS and defined by transition points
 * - Only 2 notifies needed per attack montage:
 *   1. Transition to Active (marks end of Windup, start of Active)
 *   2. Transition to Recovery (marks end of Active, start of Recovery)
 *
 * Implicit Phases:
 * - Windup: Montage start (0.0s) → Active transition
 * - Active: Active transition → Recovery transition
 * - Recovery: Recovery transition → Montage end
 *
 * Timeline Example:
 * [==Windup==][==Active==][====Recovery====]
 * 0.0s       0.3s       0.5s              1.0s
 *            ↑          ↑
 *            Active     Recovery
 *            Transition Transition
 *
 * Contrast with Windows (which use AnimNotifyStates):
 * - Windows can be non-contiguous, overlap, and have duration-based logic
 * - Examples: ComboWindow, HoldWindow, ParryWindow, CancelWindow
 *
 * Usage:
 * 1. Add this notify to montage at desired transition time
 * 2. Set TransitionToPhase property (Active or Recovery)
 * 3. Hit detection automatically enabled/disabled with Active phase
 */
UCLASS(meta = (DisplayName = "Attack Phase Transition"))
class KATANACOMBAT_API UAnimNotify_AttackPhaseTransition : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_AttackPhaseTransition();

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Which phase to transition TO */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase Transition")
	EAttackPhase TransitionToPhase = EAttackPhase::Active;

	// ============================================================================
	// ANIMNOTIFY INTERFACE
	// ============================================================================

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override { return true; }
#endif
};
