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
	const float ChargeMultiplier = FMath::Lerp(1.0f, FMath::Max(1.0f, MaxChargeKnockbackMultiplier), FMath::Clamp(ChargeLevel, 0.0f, 1.0f));
	return FMath::Max(0.0f, Distance) * ChargeMultiplier * FMath::Max(0.0f, VictimScale);
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
	if (SwingLength <= UE_SMALL_NUMBER || FlatSwing.Size() < 0.5 * SwingLength)
	{
		return Away;
	}
	const FVector Along = FlatSwing.GetSafeNormal();
	return FVector::DotProduct(Along, Away) > 0.0 ? Along : Away;
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
