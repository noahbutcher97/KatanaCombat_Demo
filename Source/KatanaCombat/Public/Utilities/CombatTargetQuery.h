#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

/**
 * Shared world query for combat targets around an actor. Pure direction math lives in
 * CombatMath; this layer owns the overlap, deduplication and gameplay eligibility rules
 * so every system agrees on who can be targeted.
 */
struct FCombatTargetQuery
{
	/** Sphere radius around the querier, in centimeters. Nonfinite or negative finds nothing. */
	float Radius = 0.0f;

	ECollisionChannel Channel = ECC_Pawn;

	/** Candidates must implement IDamageableInterface. */
	bool bRequireDamageable = true;

	/** Candidates must report IDamageableInterface::IsAlive (implies a damageable check). */
	bool bRequireAlive = true;

	/**
	 * Candidates must be hostile per ITeamMemberInterface::IsHostileTo on the querier.
	 * Queriers without the team interface accept every candidate.
	 */
	bool bRequireHostile = true;
};

namespace CombatTargetQuery
{
/**
 * Appends each eligible actor once, nearest first. The querier itself is never included.
 * An actor with several colliding components still appears a single time.
 */
KATANACOMBAT_API void GatherTargets(const AActor* Querier, const FCombatTargetQuery& Query, TArray<AActor*>& OutTargets);

/** True when Candidate passes the query's eligibility rules (radius and channel excluded). */
KATANACOMBAT_API bool IsEligible(const AActor* Querier, AActor* Candidate, const FCombatTargetQuery& Query);
}
