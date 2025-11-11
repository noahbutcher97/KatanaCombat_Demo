// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifyState_ActionWindow_Base.h"
#include "AnimNotifyState_ParryWindow.generated.h"

/**
 * AnimNotifyState that marks when the attacker is vulnerable to being parried
 * CRITICAL: This is placed on the ATTACKER's montage, NOT the defender's
 *
 * Design Philosophy:
 * - Parry window is on attacker's animation (typically early Windup phase)
 * - Defender checks nearby attackers via IsInParryWindow() when pressing block
 * - If enemy is in parry window → Parry action (no damage, counter window)
 * - If enemy NOT in parry window → Block action (posture damage)
 *
 * Usage:
 * 1. Add to attack montage during early Windup ph
 * ase
 * 2. Typically 0.3s duration (first 30% of windup)
 * 3. Marks the telegraphing window where defender can parry
 *
 * Parry Window Timeline:
 * [──Windup──][──Active──][──Recovery──]
 *      ▲▲▲▲
 *  Parry Window (attacker vulnerable)
 *
 * During this window:
 * - Attacker is marked as "in parry window" (bIsInParryWindow = true)
 * - Defender pressing block checks IsInParryWindow() on nearby attackers
 * - Successful parry → Defender enters Parrying state, Attacker enters CounterWindow
 * - Failed timing → Normal block with posture damage
 *
 * Defender-Side Detection Flow:
 * 1. Defender presses block button
 * 2. Find nearby enemies in range
 * 3. Check each enemy: IsInParryWindow()?
 * 4. If YES → TryParry() → Parry action
 * 5. If NO → StartBlocking() → Block action
 *
 * This design matches Sekiro's deflect system: timed block during enemy attack startup.
 */
UCLASS(meta = (DisplayName = "Parry Window"))
class KATANACOMBAT_API UAnimNotifyState_ParryWindow : public UAnimNotifyState_ActionWindow_Base
{
	GENERATED_BODY()

public:
	UAnimNotifyState_ParryWindow();

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override { return true; }
#endif

protected:
	// ============================================================================
	// ACTIONWINDOW_BASE INTERFACE
	// ============================================================================

	virtual EActionWindowType GetWindowType() const override { return EActionWindowType::Parry; }
	virtual void OnOpenWindow_V1(class UCombatComponent* CombatComp, float Duration) override;
	virtual void OnCloseWindow_V1(class UCombatComponent* CombatComp) override;
};