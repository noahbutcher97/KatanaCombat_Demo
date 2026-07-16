// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifyState_ActionWindow_Base.h"
#include "AnimNotifyState_ParryWindow.generated.h"

/**
 * Marks an attacker-owned timing window during which a Block Press may qualify
 * for perfect parry. Place this notify on the ATTACKER's montage.
 *
 * Current runtime still uses legacy direct detection. Under the revised defense
 * target, this window supplies timing only. Target intent,
 * `Attack.Defense.Parryable`, attack/window identity, team policy, defender
 * state, and reachable alignment are separate requirements. Failed
 * qualification enters or retains guard; normal block is decided later from
 * physical contact and causes no posture damage. A committed parry consumes the
 * matching attack generation and enters `CounterWindow` only after its bridge.
 *
 * The revised target requires Notify Begin/End to affect only the matching
 * window generation so a stale callback cannot close a newer attack's window.
 */
UCLASS(meta = (DisplayName = "Parry Window"))
class KATANACOMBAT_API UAnimNotifyState_ParryWindow : public UAnimNotifyState_ActionWindow_Base
{
	GENERATED_BODY()

public:
	UAnimNotifyState_ParryWindow();

	virtual FString GetNotifyName_Implementation() const override;

	// Wire parry window state to CombatComponent
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override { return true; }
#endif

protected:
	// ============================================================================
	// ACTIONWINDOW_BASE INTERFACE
	// ============================================================================

	virtual EActionWindowType GetWindowType() const override { return EActionWindowType::Parry; }
};
