// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ActivateRagdoll.generated.h"

/**
 * AnimNotify that activates ragdoll physics at a specific frame in the death animation.
 *
 * Purpose:
 * - Prevents "popping" on slopes by transitioning to ragdoll mid-animation
 * - Gives animators precise control over when physics takes over
 * - Works with any death animation regardless of length
 *
 * Usage:
 * 1. Add this notify to a death montage at the frame where ragdoll should begin
 *    (typically when the character starts falling or loses balance)
 * 2. The HitReactionComponent will activate ragdoll physics at this exact frame
 * 3. Current animation pose is used as starting pose for physics simulation
 *
 * Example Timeline:
 * [==Death Animation==========================]
 * 0.0s                    0.8s              1.2s
 *                         ↑
 *                         AnimNotify_ActivateRagdoll
 *                         (character begins falling, physics takes over)
 *
 * Fallback Behavior:
 * - If no AnimNotify is present, ragdoll activates at montage blend-out (original behavior)
 * - This provides backwards compatibility with existing death montages
 */
UCLASS(meta = (DisplayName = "Activate Ragdoll"))
class KATANACOMBAT_API UAnimNotify_ActivateRagdoll : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_ActivateRagdoll();

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/**
	 * Blend time from current animation pose to ragdoll physics.
	 * Lower values = more sudden transition (useful for impacts)
	 * Higher values = smoother blend (useful for falling)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlendTime = 0.1f;

	// ============================================================================
	// ANIMNOTIFY INTERFACE
	// ============================================================================

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override { return true; }
#endif
};
