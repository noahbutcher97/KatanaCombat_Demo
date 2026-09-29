#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"

/**
 * Authoritative, actor-independent combat direction math.
 *
 * Frame convention (Unreal default): +X forward, +Y right, +Z up. Yaw angles are in
 * degrees; a positive signed yaw means the direction lies to the RIGHT of forward.
 * "Flat" functions ignore Z so height differences never change a horizontal answer.
 *
 * Degenerate input policy: a nearly-zero vector has no direction. Angle functions
 * treat it as aligned (0 degrees) and classifiers return EAttackDirection::Forward,
 * matching the long-standing fallbacks of the call sites these functions replace.
 * No function here returns NaN for finite input.
 */
namespace CombatMath
{
/** Unsigned angle between two 3D directions in [0, 180]. Inputs need not be normalized. */
KATANACOMBAT_API double AngleBetweenDegrees(const FVector& A, const FVector& B);

/** True when Direction lies within HalfAngleDegrees of Forward (3D, inclusive). */
KATANACOMBAT_API bool IsWithinCone(const FVector& Forward, const FVector& Direction, double HalfAngleDegrees);

/**
 * FHitReactionInfo::DirectionToAttacker for a position-based hit: unit vector from the
 * victim toward the attacker, or zero when they coincide. This is the single definition
 * of the hit-direction convention; velocity-based writers negate the weapon velocity.
 */
KATANACOMBAT_API FVector DirectionToAttacker(const FVector& VictimLocation, const FVector& AttackerLocation);

/** Horizontal unit direction From -> To, or zero when the points coincide horizontally. */
KATANACOMBAT_API FVector FlatDirection(const FVector& From, const FVector& To);

/**
 * Signed horizontal angle from Forward to Direction in (-180, 180].
 * Positive = right of Forward. Both inputs are flattened before measuring.
 */
KATANACOMBAT_API double SignedYawDegrees(const FVector& Forward, const FVector& Direction);

/**
 * World-space yaw bearing of a horizontal direction, measured from world +X in (-180, 180].
 * Positive = toward world +Y.
 */
KATANACOMBAT_API double BearingDegrees(const FVector& Direction);

/**
 * Four-way classification of a direction already expressed in a character's local space
 * (+X forward, +Y right). The dominant horizontal axis wins; see the implementation for
 * the diagonal tie-break policy.
 */
KATANACOMBAT_API EAttackDirection ClassifyLocalDirection(const FVector& LocalDirection);

/** Four-way classification of a world-space direction relative to a character's facing. */
KATANACOMBAT_API EAttackDirection ClassifyRelativeToFacing(const FTransform& Facing, const FVector& WorldDirection);
}
