#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"

/** Actor-independent math for EAlignmentExecutor::ProceduralDisplacement. Times are in seconds. */
namespace DisplacementMath
{
/** Normalized distance covered at normalized time U (clamped to [0, 1]). */
KATANACOMBAT_API double Progress(EDisplacementSpeedProfile Profile, double U);

/** Distance covered between request times T0 and T1 (clamped to [0, Duration]). Duration <= 0 jumps at T0 == 0. */
KATANACOMBAT_API double DistanceBetween(EDisplacementSpeedProfile Profile, double Distance, double Duration, double T0, double T1);

/**
 * Animation channel only when a root-motion montage plays, motion warping can modify it, and the montage
 * instance is advancing (FAnimMontageInstance::IsPlaying). A montage holding its last pose (auto blend-out
 * disabled) or paused still counts as playing root motion but extracts none, so the push goes through movement.
 * (A stock auto blend-out calls Stop(), which clears the root-motion montage, so it never reaches this case.)
 */
KATANACOMBAT_API EDisplacementChannel SelectChannel(bool bPlayingRootMotion, bool bHasMotionWarping, bool bRootMotionMontageAdvancing);

/**
 * The push's own share of one measured step: Progress (movement along the push since the last measurement) less
 * KeptAnimationTravel (the animation root motion along the push that the animation channel kept), clamped to
 * [0, CommandedStep]. CommandedStep is the curve distance the push commanded over the same interval; a negative one
 * counts as zero. The cap matters when collision clips the step: animation travel of -3 cm, a +1 cm push and a wall
 * that holds the character still give the push's 1 cm, not the 3 cm the animation lost. Movement with no push time
 * behind it (a zero CommandedStep) is not the push's.
 * The movement channel keeps no animation root motion, so there the share is the movement itself, up to the commanded
 * step; the executor does not call this for a movement step that animation root motion overrode, since that movement
 * is not the push's.
 */
KATANACOMBAT_API double PushStep(double Progress, double KeptAnimationTravel, double CommandedStep);

/** Finite, positive, horizontal unit direction, ActorTime clock (WorldTime arrives with paired entry). */
KATANACOMBAT_API bool IsValid(const FProceduralDisplacement& Displacement);
}
