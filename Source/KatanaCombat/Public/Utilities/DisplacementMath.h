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

/** Average speed (cm/s) over [T, T + Step]: the constant force that lands exactly on the curve over one step. */
KATANACOMBAT_API double StepSpeed(EDisplacementSpeedProfile Profile, double Distance, double Duration, double T, double Step);

/** Animation channel only when a root-motion montage plays and motion warping can modify it. */
KATANACOMBAT_API EDisplacementChannel SelectChannel(bool bPlayingRootMotion, bool bHasMotionWarping);

/** Finite, positive, horizontal unit direction, ActorTime clock (WorldTime arrives with paired entry). */
KATANACOMBAT_API bool IsValid(const FProceduralDisplacement& Displacement);
}
