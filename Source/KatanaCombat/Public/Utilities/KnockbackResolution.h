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

/**
 * The largest charge knockback multiplier, at runtime as in the editor. UHT metadata cannot name a constant, so
 * UAttackData::MaxChargeKnockbackMultiplier's ClampMax repeats it, and
 * KatanaCombat.Knockback.Resolution.ChargeMultiplierCappedAtRuntime fails if the two differ.
 */
inline constexpr float MaxChargeKnockbackMultiplierCap = 5.0f;

/**
 * Distance x Lerp(1, clamp(MaxChargeKnockbackMultiplier, 1, MaxChargeKnockbackMultiplierCap), clamp(ChargeLevel, 0, 1))
 * x max(0, VictimScale). A non-finite charge level counts as uncharged (0) and a non-finite multiplier as 1.
 */
KATANACOMBAT_API float PushDistance(float Distance, float ChargeLevel, float MaxChargeKnockbackMultiplier, float VictimScale);

/** A push distance the executor can deliver: finite and above KINDA_SMALL_NUMBER. */
KATANACOMBAT_API bool IsUsablePushDistance(float PushDistance);

/**
 * Why a hit resolves no usable push distance, for the Rejected telemetry row and the debug log: "no attack data",
 * "no combat settings", "missing type default (<type>)", "non-finite scale or distance", "zero victim scale" or
 * "zero authored distance". Settings are the ones Resolve used; empty when PushDistance is usable.
 */
KATANACOMBAT_API FString NoPushDistanceCause(const UAttackData* AttackData, const UCombatSettings* Settings,
	float VictimScale, float PushDistance);

/**
 * Horizontal unit direction, or zero when the actors are stacked vertically. AlongSwing is continuous in the
 * swing: the part of the flat blade velocity that points toward the attacker is replaced by the same length of
 * AwayFromAttacker. A mostly vertical (overhead chop) or non-finite blade velocity gives AwayFromAttacker.
 */
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
