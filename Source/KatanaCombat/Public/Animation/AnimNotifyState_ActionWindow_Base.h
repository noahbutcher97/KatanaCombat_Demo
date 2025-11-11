// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ActionQueueTypes.h"
#include "AnimNotifyState_ActionWindow_Base.generated.h"

/**
 * Base class for action window notify states (Combo, Hold, Parry, Cancel)
 *
 * Provides dual-system support:
 * - V1: Calls virtual methods for legacy window open/close
 * - V2: Registers timer checkpoints for execution scheduling
 *
 * Subclasses must:
 * 1. Override GetWindowType() to specify EActionWindowType
 * 2. Override OnOpenWindow_V1() / OnCloseWindow_V1() for V1 behavior
 *
 * Example subclass (ComboWindow):
 * ```cpp
 * EActionWindowType GetWindowType() const override { return EActionWindowType::Combo; }
 *
 * void OnOpenWindow_V1(UCombatComponent* CombatComp, float Duration) override
 * {
 *     CombatComp->OpenComboWindow(Duration);
 * }
 *
 * void OnCloseWindow_V1(UCombatComponent* CombatComp) override
 * {
 *     CombatComp->CloseComboWindow();
 * }
 * ```
 */
UCLASS(Abstract)
class KATANACOMBAT_API UAnimNotifyState_ActionWindow_Base : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_ActionWindow_Base();

	// ============================================================================
	// ANIMNOTIFYSTATE INTERFACE
	// ============================================================================

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	// ============================================================================
	// SUBCLASS INTERFACE
	// ============================================================================

	/**
	 * Get the window type for V2 checkpoint registration
	 * Must be overridden by subclasses
	 */
	virtual EActionWindowType GetWindowType() const PURE_VIRTUAL(UAnimNotifyState_ActionWindow_Base::GetWindowType, return EActionWindowType::Combo;);

	/**
	 * V1 system: Open window
	 * Called when window begins in V1 combat system
	 */
	virtual void OnOpenWindow_V1(class UCombatComponent* CombatComp, float Duration) PURE_VIRTUAL(UAnimNotifyState_ActionWindow_Base::OnOpenWindow_V1,);

	/**
	 * V1 system: Close window
	 * Called when window ends in V1 combat system
	 */
	virtual void OnCloseWindow_V1(class UCombatComponent* CombatComp) PURE_VIRTUAL(UAnimNotifyState_ActionWindow_Base::OnCloseWindow_V1,);
};