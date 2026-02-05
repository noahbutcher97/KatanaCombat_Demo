// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifyState_ActionWindow_Base.h"
#include "CombatTypes.h"
#include "AnimNotifyState_CounterWindow.generated.h"

class UPairedAnimationData;

/**
 * AnimNotifyState that marks when the attacker can be countered by the defender
 * CRITICAL: This is placed on the ATTACKER's montage, NOT the defender's
 *
 * Design Philosophy (AC3/Arkham model):
 * - Counter window is on attacker's animation (typically Windup phase)
 * - Defender checks nearby attackers via IsInCounterWindow() when pressing counter
 * - If enemy is in counter window → Counter action (counter-kill or parry chain)
 * - If enemy NOT in counter window → Block/dodge action
 *
 * Differs from ParryWindow:
 * - ParryWindow: Defender deflects attack, attacker staggers (Sekiro model)
 * - CounterWindow: Defender performs counter-kill or initiates parry chain (AC3 model)
 *
 * Includes pose-matching data for procedural animation selection:
 * - SwingDirection: Horizontal/Vertical/Thrust/Sweep/Grab
 * - CounterData: Specific counter animation for this attack
 *
 * Counter Window Timeline:
 * [──Windup──][──Active──][──Recovery──]
 *      ▲▲▲▲▲▲▲▲
 *  Counter Window (attacker vulnerable to counter)
 *
 * During this window:
 * - Attacker is marked as "in counter window" (bIsInCounterWindow = true)
 * - Defender pressing counter checks IsInCounterWindow() on nearby attackers
 * - Successful counter → Counter animation plays based on mode (AC3 vs Chain)
 */
UCLASS(meta = (DisplayName = "Counter Window"))
class KATANACOMBAT_API UAnimNotifyState_CounterWindow : public UAnimNotifyState_ActionWindow_Base
{
	GENERATED_BODY()

public:
	UAnimNotifyState_CounterWindow();

	virtual FString GetNotifyName_Implementation() const override;

	// ============================================================================
	// POSE-MATCHING CONFIGURATION
	// ============================================================================

	/**
	 * Attack type for counter animation pool selection
	 * Used to categorize attacks for appropriate counter response
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counter|Pose Matching")
	EAttackType AttackType = EAttackType::Light;

	/**
	 * Swing direction for procedural pose-matching
	 * Determines how the defender's counter animation is adjusted to match this attack
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counter|Pose Matching")
	ESwingDirection SwingDirection = ESwingDirection::Horizontal;

	/**
	 * Specific counter animation for this attack (optional)
	 * If set, overrides pool-based selection with this exact counter
	 * If null, counter is selected from weapon's counter pool based on AttackType/SwingDirection
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counter|Animation")
	TObjectPtr<UPairedAnimationData> CounterData = nullptr;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override { return true; }
#endif

protected:
	// ============================================================================
	// ACTIONWINDOW_BASE INTERFACE
	// ============================================================================

	virtual EActionWindowType GetWindowType() const override { return EActionWindowType::Counter; }

	// ============================================================================
	// NOTIFY OVERRIDES
	// ============================================================================

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
