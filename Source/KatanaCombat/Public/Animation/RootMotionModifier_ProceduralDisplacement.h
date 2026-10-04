#pragma once

#include "CoreMinimal.h"
#include "RootMotionModifier.h"
#include "CombatTypes.h"
#include "RootMotionModifier_ProceduralDisplacement.generated.h"

/**
 * Runtime-only modifier added by UTargetingComponent's ProceduralDisplacement executor to the
 * playing root-motion montage. Character movement applies only animation root motion while
 * such a montage plays, so the push is delivered inside that root motion. Its clock advances
 * by the root-motion DeltaSeconds, so a hitstop-frozen owner does not move.
 */
UCLASS()
class KATANACOMBAT_API URootMotionModifier_ProceduralDisplacement : public URootMotionModifier
{
	GENERATED_BODY()

public:
	void Configure(const FProceduralDisplacement& InDisplacement, double StartElapsed);
	double GetElapsed() const { return Elapsed; }
	bool IsComplete() const { return Elapsed >= Displacement.Duration; }

	/** World travel of the animation's own root motion along the push direction since the last call (kept motion only). */
	double ConsumeAnimationTravel()
	{
		const double Travel = AnimationTravel;
		AnimationTravel = 0.0;
		return Travel;
	}

	virtual FTransform ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds) override;

	/** Combine a mesh-local push delta with the animation's root motion. */
	static FTransform ApplyDisplacement(const FTransform& InRootMotion, const FVector& LocalDelta, EDisplacementAnimationBlend Blend);

private:
	FProceduralDisplacement Displacement;
	double Elapsed = 0.0;
	double AnimationTravel = 0.0;
};
