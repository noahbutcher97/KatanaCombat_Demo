#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"

class UAttackData;
class UCombatSettings;

/** Pure knockback rules. The owning component supplies actors and settings. */
namespace KnockbackResolution
{
/** Field-wise: the attack's override when set, else the settings default for its type. A null attack or null settings gives Distance 0 (spec: no settings, no push), as does a missing type default without a distance override. */
KATANACOMBAT_API FKnockbackConfig Resolve(const UAttackData* AttackData, const UCombatSettings* Settings);

/** Distance x Lerp(1, max(1, MaxChargeKnockbackMultiplier), clamp(ChargeLevel, 0, 1)) x max(0, VictimScale). */
KATANACOMBAT_API float PushDistance(float Distance, float ChargeLevel, float MaxChargeKnockbackMultiplier, float VictimScale);

/** Horizontal unit direction, or zero when the actors are stacked vertically. */
KATANACOMBAT_API FVector ResolveDirection(EKnockbackDirection Mode, const FVector& AttackerLocation,
	const FVector& VictimLocation, const FVector& DirectionToAttacker);

struct FEligibility
{
	/** A directional hit reaction started. Blocked and parried hits never start one. */
	bool bReactionStarted = false;
	bool bSuperArmor = false;
	bool bReactionsSuppressed = false;
	bool bAlive = true;
	/** The reaction came from the settings-driven directional path (not the legacy fallback). */
	bool bSettingsPath = false;
	/** Step 4 sets false for additive flinches; every reaction interrupts until then. */
	bool bInterruptingReaction = true;
};

KATANACOMBAT_API bool ShouldApply(const FEligibility& Eligibility);
}
