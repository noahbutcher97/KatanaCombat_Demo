// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifyState_ActionWindow_Base.h"
#include "AnimNotifyState_ComboWindow.generated.h"

/**
 * AnimNotifyState that opens the combo input window
 * Allows player to input next combo attack during this time
 *
 * Usage:
 * 1. Add to attack montage during Recovery phase
 * 2. Typically placed at start of recovery (first 60%)
 * 3. Duration determines how long player has to input combo
 * 4. Default window: 0.6s (configurable in CombatSettings)
 *
 * Combo Window Timeline:
 * [──Windup──][──Active──][──Recovery──]
 *                              ▲▲▲▲▲▲
 *                          Combo Window
 *
 * During this window:
 * - Light attack input → Execute NextComboAttack
 * - Heavy attack input → Execute HeavyComboAttack
 * - No input → Chain breaks, return to idle
 *
 * This notify enables fluid combo chains by providing clear input timing.
 */
UCLASS(meta = (DisplayName = "Combo Window"))
class KATANACOMBAT_API UAnimNotifyState_ComboWindow : public UAnimNotifyState_ActionWindow_Base
{
	GENERATED_BODY()

public:
	UAnimNotifyState_ComboWindow();

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override { return true; }
#endif

protected:
	// ============================================================================
	// ACTIONWINDOW_BASE INTERFACE
	// ============================================================================

	virtual EActionWindowType GetWindowType() const override { return EActionWindowType::Combo; }
	virtual void OnOpenWindow_V1(class UCombatComponent* CombatComp, float Duration) override;
	virtual void OnCloseWindow_V1(class UCombatComponent* CombatComp) override;
};