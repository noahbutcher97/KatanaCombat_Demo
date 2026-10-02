#include "Utilities/KnockbackResolution.h"
#include "Utilities/CombatMath.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"

namespace KnockbackResolution
{
FKnockbackConfig Resolve(const UAttackData* AttackData, const UCombatSettings* Settings)
{
	FKnockbackConfig Result;
	Result.Distance = 0.0f;
	if (!AttackData || !Settings)
	{
		return Result;
	}

	const FKnockbackConfig* Default = Settings->DefaultKnockback.Find(AttackData->AttackType);
	const FKnockbackOverride& Authored = AttackData->Knockback;
	Result.Distance = Authored.bOverrideDistance ? Authored.Distance : (Default ? Default->Distance : 0.0f);
	Result.Duration = Authored.bOverrideDuration ? Authored.Duration : (Default ? Default->Duration : 0.2f);
	Result.DirectionMode = Authored.bOverrideDirectionMode ? Authored.DirectionMode
		: (Default ? Default->DirectionMode : EKnockbackDirection::AwayFromAttacker);
	Result.SpeedProfile = Authored.bOverrideSpeedProfile ? Authored.SpeedProfile
		: (Default ? Default->SpeedProfile : EDisplacementSpeedProfile::EaseOut);
	Result.AnimationBlend = Authored.bOverrideAnimationBlend ? Authored.AnimationBlend
		: (Default ? Default->AnimationBlend : EDisplacementAnimationBlend::AddToAnimation);
	return Result;
}

float PushDistance(const float Distance, const float ChargeLevel, const float MaxChargeKnockbackMultiplier, const float VictimScale)
{
	// FMath::Clamp(NaN, 0, 1) is 1, so an unguarded non-finite charge would read as full charge.
	const float Charge = FMath::IsFinite(ChargeLevel) ? FMath::Clamp(ChargeLevel, 0.0f, 1.0f) : 0.0f;
	// Finite first, then the cap: a non-finite multiplier counts as 1, never as the cap.
	const float Multiplier = FMath::IsFinite(MaxChargeKnockbackMultiplier)
		? FMath::Clamp(MaxChargeKnockbackMultiplier, 1.0f, MaxChargeKnockbackMultiplierCap) : 1.0f;
	const float ChargeMultiplier = FMath::Lerp(1.0f, Multiplier, Charge);
	return FMath::Max(0.0f, Distance) * ChargeMultiplier * FMath::Max(0.0f, VictimScale);
}

bool IsUsablePushDistance(const float PushDistance)
{
	return FMath::IsFinite(PushDistance) && PushDistance > KINDA_SMALL_NUMBER;
}

FString NoPushDistanceCause(const UAttackData* AttackData, const UCombatSettings* Settings, const float VictimScale,
	const float PushDistance)
{
	if (IsUsablePushDistance(PushDistance))
	{
		return FString();
	}
	if (!AttackData)
	{
		return TEXT("no attack data");
	}
	if (!Settings)
	{
		return TEXT("no combat settings");
	}
	if (!AttackData->Knockback.bOverrideDistance && !Settings->DefaultKnockback.Contains(AttackData->AttackType))
	{
		return FString::Printf(TEXT("missing type default (%s)"),
			*StaticEnum<EAttackType>()->GetNameStringByValue(static_cast<int64>(AttackData->AttackType)));
	}
	const float AuthoredDistance = Resolve(AttackData, Settings).Distance;
	if (!FMath::IsFinite(AuthoredDistance) || !FMath::IsFinite(VictimScale) || !FMath::IsFinite(PushDistance))
	{
		return TEXT("non-finite scale or distance");
	}
	// Charge only adds (its multiplier is at least 1), so an authored distance that survives means the scale removed it.
	return AuthoredDistance > KINDA_SMALL_NUMBER ? TEXT("zero victim scale") : TEXT("zero authored distance");
}

FVector ResolveDirection(const EKnockbackDirection Mode, const FVector& AttackerLocation,
	const FVector& VictimLocation, const FVector& DirectionToAttacker)
{
	const FVector Away = CombatMath::FlatDirection(AttackerLocation, VictimLocation);
	if (Mode != EKnockbackDirection::AlongSwing || Away.IsZero())
	{
		return Away;
	}

	const FVector Swing = -DirectionToAttacker;
	const FVector FlatSwing(Swing.X, Swing.Y, 0.0);
	const double SwingLength = Swing.Size();
	// No usable blade velocity, or an overhead chop (mostly vertical): its horizontal part says little about where
	// the blade is going, so push straight away.
	if (!FMath::IsFinite(SwingLength) || SwingLength <= UE_SMALL_NUMBER || FlatSwing.Size() < 0.5 * SwingLength)
	{
		return Away;
	}
	// Continuous in the swing: remove only the part of the flat swing that points toward the attacker, and replace
	// it with the same length of Away. Straight away stays away, a tangential swing stays tangential and a pure
	// back-swing becomes Away, with no jump between them.
	const FVector Along = FlatSwing.GetSafeNormal();
	const double Radial = FVector::DotProduct(Along, Away);
	const FVector Clipped = Along - FMath::Min(0.0, Radial) * Away;
	const double Lost = 1.0 - Clipped.Size();
	return (Clipped + Lost * Away).GetSafeNormal();
}

bool ShouldApply(const FEligibility& Eligibility)
{
	return Eligibility.bReactionStarted
		&& !Eligibility.bSuperArmor
		&& !Eligibility.bReactionsSuppressed
		&& Eligibility.bAlive
		&& Eligibility.bSettingsPath
		&& Eligibility.bInterruptingReaction;
}
}
