// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_PairedAnimationCollision.generated.h"

/**
 * Animation notify state to disable character collision during paired animations
 *
 * Problem: During close paired animations (finishers, counters), character capsules
 * overlap and push each other apart, causing visual clipping and sync point misalignment.
 *
 * Solution: This notify temporarily disables collision with TRACKED PARTNERS only,
 * allowing characters to overlap without affecting collision with other pawns (allies,
 * environmental characters, etc.).
 *
 * Architecture:
 * - Uses PairedAnimationComponent's retained partner records for targeted collision ignore
 * - Uses IgnoreActorWhenMoving() API instead of global ECR_Ignore on ECC_Pawn
 * - Supports multi-partner scenarios (double takedowns, group finishers)
 * - Easier to debug: clear partner list vs opaque global pawn ignore
 *
 * Usage:
 * 1. Before montage: register both participants through PairedAnimationComponent
 * 2. Add notify to BOTH attacker's and victim's montages
 * 3. Position to cover the close-contact portion of the animation
 * 4. Collision automatically restores when notify ends or animation is interrupted
 *
 * Timeline Example:
 * [──Wind-up──][──PairedCollision─Start─][─SYNC─][──PairedCollision─End──][─Recovery─]
 *               ▲ Disable collision here   ▲ Impact (no push)   ▲ Restore collision
 *
 * Reference Pattern: HitReactionComponent.cpp:718-729 (ragdoll collision management)
 */
UCLASS(meta = (DisplayName = "Paired Animation Collision"))
class KATANACOMBAT_API UAnimNotifyState_PairedAnimationCollision : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_PairedAnimationCollision();

	// ========================================================================
	// COLLISION SETTINGS
	// ========================================================================

	/**
	 * Use tracked partners retained by PairedAnimationComponent.
	 * This is the preferred approach: only ignores collision with specific tracked partners.
	 * Partners must be registered by the paired-animation sequence before montage playback.
	 *
	 * If false, falls back to global pawn collision disable (ECR_Ignore on ECC_Pawn).
	 * RECOMMENDED: Keep true unless debugging without paired-animation setup.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", AdvancedDisplay)
	bool bUseTrackedPartnersOnly = true;

	/** Disable collision with tracked partners (or all pawns if bUseTrackedPartnersOnly is false) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	bool bDisablePawnCollision = true;

	/** Disable all capsule physics (for root-motion-driven paired animations) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	bool bDisableCapsulePhysics = false;

	/**
	 * Dynamically scan for actors entering the danger zone during animation.
	 * If an NPC walks into the paired animation area, it will be added to IgnoredPartners.
	 * Uses NotifyTick to periodically check for new actors in the danger radius.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	bool bScanForDynamicObstructions = true;

	/**
	 * Radius around the character to scan for dynamic obstructions.
	 * Actors within this radius will have collision ignored if they enter during the animation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision",
		meta = (EditCondition = "bScanForDynamicObstructions", ClampMin = "50.0", ClampMax = "500.0"))
	float DynamicObstructionRadius = 150.0f;

	/** Also disable movement component (prevents CharacterMovement from fighting root motion) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bDisableMovement = true;

	// ========================================================================
	// ANIMNOTIFYSTATE INTERFACE
	// ========================================================================

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual FLinearColor GetEditorColor() override { return FLinearColor(0.2f, 0.8f, 1.0f); }  // Cyan for collision
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override { return true; }
#endif

};
