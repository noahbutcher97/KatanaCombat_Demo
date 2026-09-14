#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"

/** Actor-independent upright alignment math. Collision and ownership belong to the executor. */
namespace AlignmentMotion
{
struct FStep
{
	FVector Translation = FVector::ZeroVector;
	double Yaw = 0.0;
	double Seconds = 0.0;
	EAlignmentMotionOutcome Outcome = EAlignmentMotionOutcome::Running;
};

KATANACOMBAT_API bool IsValid(const FAlignmentMotionLimits& Limits);
KATANACOMBAT_API bool IsValidGoal(const FTransform& Goal);
/** Undo UE component-tick actor dilation; nonpositive/nonfinite scales are unsupported. */
KATANACOMBAT_API double SimulationDelta(double ComponentDelta, double ActorTimeScale);
KATANACOMBAT_API bool CanReach(const FTransform& Current, const FTransform& Goal, const FAlignmentMotionLimits& Limits);
KATANACOMBAT_API FStep CalculateStep(const FTransform& Current, const FTransform& Goal,
	const FAlignmentMotionLimits& Limits, const FAlignmentMotionState& State, double DeltaSeconds);
}
