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
 * instance is advancing (FAnimMontageInstance::IsPlaying). A montage blending out after its last section,
 * or paused, still counts as playing root motion but extracts none, so the push goes through movement.
 */
KATANACOMBAT_API EDisplacementChannel SelectChannel(bool bPlayingRootMotion, bool bHasMotionWarping, bool bRootMotionMontageAdvancing);

/** Finite, positive, horizontal unit direction, ActorTime clock (WorldTime arrives with paired entry). */
KATANACOMBAT_API bool IsValid(const FProceduralDisplacement& Displacement);
}
