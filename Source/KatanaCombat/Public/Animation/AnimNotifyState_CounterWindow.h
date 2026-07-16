// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifyState_ActionWindow_Base.h"
#include "CombatTypes.h"
#include "AnimNotifyState_CounterWindow.generated.h"

class UPairedAnimationData;

/**
 * Marks attacker-owned timing for the legacy direct-counter path. Place this
 * notify on the ATTACKER's montage.
 *
 * This notify window is not `EChainCounterState::CounterWindow`. Revised Chain
 * Counter enters that state only after a committed perfect parry and completed
 * parry bridge. Chain attack selection uses the defender's selected
 * `UAttackData::CounterData`; `SpecificCounterData` is an explicitly allowed
 * fallback, not the primary source.
 *
 * The revised target requires Notify Begin/End to affect only the matching
 * window generation so a stale callback cannot close a newer direct-counter
 * window.
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
