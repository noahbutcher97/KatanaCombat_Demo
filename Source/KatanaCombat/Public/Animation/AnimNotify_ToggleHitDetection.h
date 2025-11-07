// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ToggleHitDetection.generated.h"

/**
 * DEPRECATED: Hit detection is now automatic with the Active phase
 *
 * The new event-based phase system automatically enables/disables hit detection:
 * - Hit detection ENABLED when Active phase begins (AnimNotify_AttackPhaseTransition(Active))
 * - Hit detection DISABLED when Active phase ends (AnimNotify_AttackPhaseTransition(Recovery))
 *
 * Migration:
 * - Remove all AnimNotify_ToggleHitDetection instances from montages
 * - Hit detection will be automatic based on Active phase timing
 * - No replacement needed - handled by CombatComponent::HandlePhaseBegin/End
 *
 * Benefits of automatic system:
 * - No redundancy: One system (phases) instead of two (phases + manual toggles)
 * - No desync: Hit detection always matches Active phase timing
 * - Simpler montages: 2 fewer notifies per attack
 *
 * See: docs/PHASE_SYSTEM_MIGRATION.md for complete migration guide
 *
 * ---
 *
 * OLD DOCUMENTATION (for reference):
 * AnimNotify that toggles weapon hit detection on/off
 * Typically used in pairs: Enable at start of active phase, Disable at end
 *
 * Usage:
 * 1. Place two instances in attack montage:
 *    - First with bEnable = true at start of Active phase
 *    - Second with bEnable = false at end of Active phase
 * 2. Routes to WeaponComponent via ICombatInterface
 * 3. WeaponComponent performs swept sphere traces while enabled
 *
 * Attack Timeline with Hit Detection:
 * [──Windup──][──Active──][──Recovery──]
 *               ▼      ▲
 *            Enable  Disable
 *               ████████  <- Hit Detection Active
 *
 * Why toggle vs continuous:
 * - Performance: Only trace during damage-dealing frames
 * - Design: Precise control over when hits can occur
 * - Clarity: Clear visual feedback in animation timeline
 *
 * Note: Using AnimNotifyState_AttackPhase (Active) is preferred as it
 * automatically handles enable/disable. Use this only for fine-tuned control.
 */
UCLASS(meta = (DisplayName = "Toggle Hit Detection (DEPRECATED - Now Automatic)"))
class KATANACOMBAT_API UAnimNotify_ToggleHitDetection : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_ToggleHitDetection();

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Enable or disable hit detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Detection")
	bool bEnable = true;

	// ============================================================================
	// ANIMNOTIFY INTERFACE
	// ============================================================================

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override { return true; }
#endif
};