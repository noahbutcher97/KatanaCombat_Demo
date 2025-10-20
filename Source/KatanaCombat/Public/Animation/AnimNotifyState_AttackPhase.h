
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CombatTypes.h"
#include "AnimNotifyState_AttackPhase.generated.h"

/**
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
UCLASS(meta = (DisplayName = "Attack Phase"))
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