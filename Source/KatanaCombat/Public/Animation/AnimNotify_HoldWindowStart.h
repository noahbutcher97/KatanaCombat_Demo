// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "CombatTypes.h"
#include "AnimNotify_HoldWindowStart.generated.h"

/**
 * AnimNotify for hold window start events
 *
 * V2 HOLD SYSTEM (Event-Driven):
 * - Hold detection no longer uses AnimNotifyState duration tracking
 * - Single AnimNotify event fires at hold window start time
 * - System checks if button is STILL pressed (not tracking duration)
 * - Replaces tick-based duration checks with event-driven button state queries
 *
 * Hold Behavior by Attack Type:
 *
 * LIGHT ATTACKS:
 * 1. AnimNotify fires at hold window start
 * 2. System checks if light attack button is still pressed
 * 3. If pressed: Begin ease slowdown (procedural playrate transition)
 * 4. Wait for release + directional input
 * 5. Execute directional follow-up attack
 *
 * HEAVY ATTACKS:
 * 1. AnimNotify fires at hold window start (usually early in animation)
 * 2. System checks if heavy attack button is still pressed
 * 3. If pressed: Loop charge section of montage
 * 4. Wait for release (max charge time enforced)
 * 5. Execute charged attack (damage scaled by hold duration)
 *
 * Timeline Example:
 * [==Windup==][==Active==][====Recovery====]
 * 0.0s  0.2s  0.3s       0.5s              1.0s
 *       ↑
 *       Hold Window Start (this notify fires here)
 *       System checks button state synchronously
 *
 * Contrast with Old System:
 * - OLD: AnimNotifyState_HoldWindow with Begin/End + tick-based duration checks
 * - NEW: Single AnimNotify event + button state query (no tick dependencies)
 *
 * Usage:
 * 1. Add this notify to montage at desired hold detection time
 * 2. Set InputType property (LightAttack or HeavyAttack)
 * 3. System will check if that input is currently pressed
 * 4. Configure hold behavior in AttackData (ease duration, charge loop section, etc.)
 */
UCLASS(meta = (DisplayName = "Hold Window Start"))
class KATANACOMBAT_API UAnimNotify_HoldWindowStart : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_HoldWindowStart();

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Which input to check for hold state (LightAttack or HeavyAttack) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hold Detection")
	EInputType InputType = EInputType::LightAttack;

	// ============================================================================
	// ANIMNOTIFY INTERFACE
	// ============================================================================

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override { return true; }
#endif
};
