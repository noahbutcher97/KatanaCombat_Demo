// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_HoldWindow.generated.h"

/**
 * AnimNotifyState that marks when a light attack can be held for directional follow-ups
 *
 * Design Philosophy:
 * - Hold Window is NOT an attack phase - it's an independent window
 * - Placed during light attack animations (typically near end of Active phase)
 * - Checks if light attack button is STILL HELD from initial press
 * - If held → Pause animation, wait for directional input + release
 * - If not held → Animation continues normally
 *
 * Usage:
 * 1. Add to light attack montage near end of Active phase
 * 2. Typically 0.2-0.4s duration (window to detect hold)
 * 3. Attack must have bCanHold = true in AttackData
 *
 * Hold Mechanic Timeline:
 * [──Windup──][──Active──][──Recovery──]
 *                   ▲▲▲▲
 *              Hold Window (check if still pressed)
 *
 * If held during this window:
 * - Animation pauses (freeze frame)
 * - Player can input direction
 * - On release → Execute DirectionalFollowUps[Direction]
 *
 * Key Difference from Hold Phase:
 * - This is a WINDOW (independent timing marker)
 * - Not a phase in the attack state machine
 * - Multiple windows can overlap (HoldWindow + ComboWindow)
 * - Phases are mutually exclusive (only one active at a time)
 */
UCLASS(meta = (DisplayName = "Hold Window"))
class KATANACOMBAT_API UAnimNotifyState_HoldWindow : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    UAnimNotifyState_HoldWindow();

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