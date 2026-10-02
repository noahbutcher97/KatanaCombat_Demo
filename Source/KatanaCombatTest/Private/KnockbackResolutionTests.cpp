#include "CombatTestHelpers.h"
#include "Utilities/KnockbackResolution.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackResolveDefaultsTest, "KatanaCombat.Knockback.Resolution.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackResolveDefaultsTest::RunTest(const FString&)
{
	UCombatSettings* Settings = NewObject<UCombatSettings>();
	UAttackData* Light = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* Heavy = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	const FKnockbackConfig L = KnockbackResolution::Resolve(Light, Settings);
	TestEqual(TEXT("Light default distance"), L.Distance, 25.0f);
	TestEqual(TEXT("Light default duration"), L.Duration, 0.2f);
	TestEqual(TEXT("Default direction"), L.DirectionMode, EKnockbackDirection::AwayFromAttacker);
	TestEqual(TEXT("Default profile"), L.SpeedProfile, EDisplacementSpeedProfile::EaseOut);
	TestEqual(TEXT("Default blend adds to the reaction's own root motion"), L.AnimationBlend, EDisplacementAnimationBlend::AddToAnimation);
	const FKnockbackConfig H = KnockbackResolution::Resolve(Heavy, Settings);
	// The Heavy reactions are authored knockback animations that travel about 89 cm on their own, so the push adds 20.
	TestEqual(TEXT("Heavy default distance"), H.Distance, 20.0f);
	TestEqual(TEXT("Heavy default duration"), H.Duration, 0.25f);
	TestEqual(TEXT("Heavy default direction"), H.DirectionMode, EKnockbackDirection::AwayFromAttacker);
	TestEqual(TEXT("Heavy default profile"), H.SpeedProfile, EDisplacementSpeedProfile::EaseOut);
	TestEqual(TEXT("Heavy default blend"), H.AnimationBlend, EDisplacementAnimationBlend::AddToAnimation);
	UAttackData* Special = FCombatTestHelpers::CreateTestAttack(EAttackType::Special);
	TestEqual(TEXT("Missing type resolves to no push"), KnockbackResolution::Resolve(Special, Settings).Distance, 0.0f);
	TestEqual(TEXT("Null attack resolves to no push"), KnockbackResolution::Resolve(nullptr, Settings).Distance, 0.0f);
	TestEqual(TEXT("Null settings without overrides resolve to no push"), KnockbackResolution::Resolve(Light, nullptr).Distance, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackResolveOverridesTest, "KatanaCombat.Knockback.Resolution.IndependentOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackResolveOverridesTest::RunTest(const FString&)
{
	UCombatSettings* Settings = NewObject<UCombatSettings>();
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	Attack->Knockback.bOverrideDuration = true;
	Attack->Knockback.Duration = 0.5f;
	FKnockbackConfig Resolved = KnockbackResolution::Resolve(Attack, Settings);
	TestEqual(TEXT("Overridden duration"), Resolved.Duration, 0.5f);
	TestEqual(TEXT("Inherited distance"), Resolved.Distance, 25.0f);
	Attack->Knockback.bOverrideDistance = true;
	Attack->Knockback.Distance = 90.0f;
	Attack->Knockback.bOverrideDirectionMode = true;
	Attack->Knockback.DirectionMode = EKnockbackDirection::AlongSwing;
	Attack->Knockback.bOverrideSpeedProfile = true;
	Attack->Knockback.SpeedProfile = EDisplacementSpeedProfile::Linear;
	Attack->Knockback.bOverrideAnimationBlend = true;
	Attack->Knockback.AnimationBlend = EDisplacementAnimationBlend::ReplaceAnimation;
	Resolved = KnockbackResolution::Resolve(Attack, Settings);
	TestEqual(TEXT("Overridden distance"), Resolved.Distance, 90.0f);
	TestEqual(TEXT("Overridden direction"), Resolved.DirectionMode, EKnockbackDirection::AlongSwing);
	TestEqual(TEXT("Overridden profile"), Resolved.SpeedProfile, EDisplacementSpeedProfile::Linear);
	TestEqual(TEXT("Overridden blend"), Resolved.AnimationBlend, EDisplacementAnimationBlend::ReplaceAnimation);
	TestEqual(TEXT("No combat settings means no push, even with overrides"), KnockbackResolution::Resolve(Attack, nullptr).Distance, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackPushDistanceTest, "KatanaCombat.Knockback.Resolution.PushDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackPushDistanceTest::RunTest(const FString&)
{
	TestEqual(TEXT("Uncharged"), KnockbackResolution::PushDistance(60.f, 0.f, 2.f, 1.f), 60.0f);
	TestEqual(TEXT("Half charged"), KnockbackResolution::PushDistance(60.f, 0.5f, 2.f, 1.f), 90.0f);
	TestEqual(TEXT("Fully charged"), KnockbackResolution::PushDistance(60.f, 1.f, 2.f, 1.f), 120.0f);
	TestEqual(TEXT("Charge level is clamped"), KnockbackResolution::PushDistance(60.f, 3.f, 2.f, 1.f), 120.0f);
	TestEqual(TEXT("Victim scale applies last"), KnockbackResolution::PushDistance(60.f, 1.f, 2.f, 0.5f), 60.0f);
	TestEqual(TEXT("Immune victim"), KnockbackResolution::PushDistance(60.f, 1.f, 2.f, 0.f), 0.0f);
	TestEqual(TEXT("A multiplier below one never reduces the push"), KnockbackResolution::PushDistance(60.f, 1.f, 0.5f, 1.f), 60.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackNonFiniteChargeTest, "KatanaCombat.Knockback.Resolution.NonFiniteChargeIsUncharged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackNonFiniteChargeTest::RunTest(const FString&)
{
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float Inf = std::numeric_limits<float>::infinity();
	// Finite first: a NaN can slip through a plain float comparison under fast floating point.
	const auto Pushes = [this](const TCHAR* What, const float Actual, const float Expected)
	{
		TestTrue(FString::Printf(TEXT("%s (%f, expected %f)"), What, Actual, Expected),
			FMath::IsFinite(Actual) && FMath::Abs(Actual - Expected) <= 1e-3f);
	};
	// FMath::Clamp(NaN, 0, 1) is 1, so an unguarded NaN charge would count as full charge.
	Pushes(TEXT("NaN charge counts as uncharged"), KnockbackResolution::PushDistance(60.f, NaN, 2.f, 1.f), 60.0f);
	Pushes(TEXT("+Inf charge counts as uncharged"), KnockbackResolution::PushDistance(60.f, Inf, 2.f, 1.f), 60.0f);
	Pushes(TEXT("-Inf charge counts as uncharged"), KnockbackResolution::PushDistance(60.f, -Inf, 2.f, 1.f), 60.0f);
	Pushes(TEXT("A finite charge still scales"), KnockbackResolution::PushDistance(60.f, 1.f, 2.f, 1.f), 120.0f);
	Pushes(TEXT("NaN multiplier counts as 1 at full charge"), KnockbackResolution::PushDistance(60.f, 1.f, NaN, 1.f), 60.0f);
	Pushes(TEXT("+Inf multiplier counts as 1 when uncharged"), KnockbackResolution::PushDistance(60.f, 0.f, Inf, 1.f), 60.0f);
	Pushes(TEXT("+Inf multiplier counts as 1 at full charge"), KnockbackResolution::PushDistance(60.f, 1.f, Inf, 1.f), 60.0f);
#if WITH_METADATA
	// The editor caps the multiplier, as it caps the distance (500) and the victim scale (5).
	const FProperty* Multiplier = UAttackData::StaticClass()->FindPropertyByName(
		GET_MEMBER_NAME_CHECKED(UAttackData, MaxChargeKnockbackMultiplier));
	if (TestNotNull(TEXT("Multiplier property"), Multiplier))
	{
		TestEqual(TEXT("The multiplier has a finite editor cap"), Multiplier->GetMetaData(TEXT("ClampMax")), FString(TEXT("5.0")));
	}
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackDirectionTest, "KatanaCombat.Knockback.Resolution.Direction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackDirectionTest::RunTest(const FString&)
{
	const FVector Attacker(0, 0, 0);
	const FVector Victim(100, 0, 20);
	const FVector SidewaysSwing = FVector(0, -1, 0); // DirectionToAttacker = negated blade velocity (+Y swing)
	TestTrue(TEXT("Away ignores the swing"), KnockbackResolution::ResolveDirection(EKnockbackDirection::AwayFromAttacker, Attacker, Victim, SidewaysSwing).Equals(FVector(1, 0, 0), 1e-4));
	const FVector DiagonalSwing = -FVector(1, 1, 0).GetSafeNormal();
	TestTrue(TEXT("Along swing follows the flattened blade velocity"), KnockbackResolution::ResolveDirection(EKnockbackDirection::AlongSwing, Attacker, Victim, DiagonalSwing).Equals(FVector(1, 1, 0).GetSafeNormal(), 1e-4));
	// Off the attacker-victim plane, so following its flattened swing (diagonal) differs from the fallback.
	const FVector OverheadChop = -FVector(0.2, 0.2, -0.95).GetSafeNormal();
	TestTrue(TEXT("Mostly vertical swing falls back to away"), KnockbackResolution::ResolveDirection(EKnockbackDirection::AlongSwing, Attacker, Victim, OverheadChop).Equals(FVector(1, 0, 0), 1e-4));
	const FVector BackSwing = FVector(1, 0, 0); // blade moving toward the attacker
	TestTrue(TEXT("Swing toward the attacker falls back to away"), KnockbackResolution::ResolveDirection(EKnockbackDirection::AlongSwing, Attacker, Victim, BackSwing).Equals(FVector(1, 0, 0), 1e-4));
	TestTrue(TEXT("Stacked actors have no direction"), KnockbackResolution::ResolveDirection(EKnockbackDirection::AwayFromAttacker, FVector(0, 0, 0), FVector(0, 0, 150), SidewaysSwing).IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackAlongSwingContinuityTest, "KatanaCombat.Knockback.Resolution.AlongSwingContinuousNearTangent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackAlongSwingContinuityTest::RunTest(const FString&)
{
	const FVector Attacker(0, 0, 0);
	const FVector Victim(100, 0, 0);
	const auto Along = [&](const double SwingHeadingDegrees)
	{
		// DirectionToAttacker is the negated blade velocity; the swing is horizontal at this heading from +X (away).
		const double Radians = FMath::DegreesToRadians(SwingHeadingDegrees);
		const FVector Swing(FMath::Cos(Radians), FMath::Sin(Radians), 0.0);
		return KnockbackResolution::ResolveDirection(EKnockbackDirection::AlongSwing, Attacker, Victim, -Swing);
	};
	const auto AngleBetween = [](const FVector& A, const FVector& B)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.0, 1.0)));
	};
	// A tangential slash sits where the swing turns from away to toward the attacker; a degree either side must not flip the push.
	const FVector JustAway = Along(89.0);
	const FVector JustToward = Along(91.0);
	const double Gap = AngleBetween(JustAway, JustToward);
	TestTrue(FString::Printf(TEXT("Swings 1 degree either side of tangential push within 5 degrees of each other (%.2f: %s vs %s)"),
		Gap, *JustAway.ToString(), *JustToward.ToString()), Gap < 5.0);
	// Only the toward-attacker part is replaced, by the same length of away: 135 degrees keeps its sideways part.
	const double Heading135 = FMath::RadiansToDegrees(FMath::Atan2(Along(135.0).Y, Along(135.0).X));
	TestTrue(FString::Printf(TEXT("A partly backward swing pushes between away and sideways (%.2f degrees)"), Heading135),
		FMath::IsNearlyEqual(Heading135, 67.5, 0.1));
	TestTrue(TEXT("A swing straight away stays straight away"), Along(0.0).Equals(FVector(1, 0, 0), 1e-4));
	TestTrue(TEXT("A tangential swing stays tangential"), Along(90.0).Equals(FVector(0, 1, 0), 1e-4));
	TestTrue(TEXT("A swing straight back becomes away"), Along(180.0).Equals(FVector(1, 0, 0), 1e-4));
	for (double Heading = -180.0; Heading <= 180.0; Heading += 5.0)
	{
		const FVector Result = Along(Heading);
		TestTrue(FString::Printf(TEXT("Heading %.0f: a horizontal unit push"), Heading),
			!Result.ContainsNaN() && FMath::IsNearlyEqual(Result.Size(), 1.0, 1e-4) && FMath::Abs(Result.Z) < 1e-4);
		TestTrue(FString::Printf(TEXT("Heading %.0f: never toward the attacker"), Heading), Result.X >= -1e-4);
	}
	// A non-finite blade velocity says nothing about the swing: push straight away. ContainsNaN first, because a
	// NaN can slip through a plain float comparison under fast floating point.
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Inf = std::numeric_limits<double>::infinity();
	for (const FVector& Blade : {FVector(NaN, 0, 0), FVector(Inf, 1, 0), FVector(0, 0, Inf), FVector::ZeroVector})
	{
		const FVector Result = KnockbackResolution::ResolveDirection(EKnockbackDirection::AlongSwing, Attacker, Victim, Blade);
		TestTrue(FString::Printf(TEXT("Blade %s falls back to away (%s)"), *Blade.ToString(), *Result.ToString()),
			!Result.ContainsNaN() && Result.Equals(FVector(1, 0, 0), 1e-4));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackNoPushCauseTest, "KatanaCombat.Knockback.Resolution.NoPushDistanceCauses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackNoPushCauseTest::RunTest(const FString&)
{
	UCombatSettings* Settings = NewObject<UCombatSettings>();
	UAttackData* Light = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* Special = FCombatTestHelpers::CreateTestAttack(EAttackType::Special);
	UAttackData* ZeroDistance = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	ZeroDistance->Knockback.bOverrideDistance = true;
	ZeroDistance->Knockback.Distance = 0.0f;
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float Inf = std::numeric_limits<float>::infinity();
	// The cause StartKnockback records for one hit, uncharged.
	const auto Cause = [](const UAttackData* Attack, const UCombatSettings* With, const float VictimScale)
	{
		const float Distance = KnockbackResolution::PushDistance(
			KnockbackResolution::Resolve(Attack, With).Distance, 0.0f, 1.0f, VictimScale);
		return KnockbackResolution::NoPushDistanceCause(Attack, With, VictimScale, Distance);
	};

	TestTrue(TEXT("A positive finite distance is usable"), KnockbackResolution::IsUsablePushDistance(25.0f));
	TestFalse(TEXT("Zero is not usable"), KnockbackResolution::IsUsablePushDistance(0.0f));
	TestFalse(TEXT("NaN is not usable"), KnockbackResolution::IsUsablePushDistance(NaN));
	TestFalse(TEXT("+Inf is not usable"), KnockbackResolution::IsUsablePushDistance(Inf));
	TestEqual(TEXT("A usable push has no cause"), Cause(Light, Settings, 1.0f), FString());

	TestEqual(TEXT("No attack data"), Cause(nullptr, Settings, 1.0f), FString(TEXT("no attack data")));
	TestEqual(TEXT("No combat settings"), Cause(Light, nullptr, 1.0f), FString(TEXT("no combat settings")));
	TestEqual(TEXT("Missing type default"), Cause(Special, Settings, 1.0f), FString(TEXT("missing type default (Special)")));
	TestEqual(TEXT("Zero victim scale"), Cause(Light, Settings, 0.0f), FString(TEXT("zero victim scale")));
	TestEqual(TEXT("Negative victim scale"), Cause(Light, Settings, -1.0f), FString(TEXT("zero victim scale")));
	TestEqual(TEXT("NaN victim scale"), Cause(Light, Settings, NaN), FString(TEXT("non-finite scale or distance")));
	TestEqual(TEXT("+Inf victim scale"), Cause(Light, Settings, Inf), FString(TEXT("non-finite scale or distance")));
	TestEqual(TEXT("Zero authored distance"), Cause(ZeroDistance, Settings, 1.0f), FString(TEXT("zero authored distance")));

	// A distance override on a type with no default is authored, not missing.
	Special->Knockback.bOverrideDistance = true;
	Special->Knockback.Distance = 0.0f;
	TestEqual(TEXT("Overridden zero on a type without a default"), Cause(Special, Settings, 1.0f), FString(TEXT("zero authored distance")));
	Special->Knockback.Distance = 30.0f;
	TestEqual(TEXT("Overridden distance on a type without a default pushes"), Cause(Special, Settings, 1.0f), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackEligibilityTest, "KatanaCombat.Knockback.Resolution.Eligibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackEligibilityTest::RunTest(const FString&)
{
	KnockbackResolution::FEligibility Eligible;
	Eligible.bReactionStarted = true;
	Eligible.bSettingsPath = true;
	TestTrue(TEXT("Started interrupting reaction pushes"), KnockbackResolution::ShouldApply(Eligible));
	auto Without = [&](auto Mutate) { KnockbackResolution::FEligibility Copy = Eligible; Mutate(Copy); return KnockbackResolution::ShouldApply(Copy); };
	// Blocked and parried hits never start a directional reaction (pinned by the Knockback.Architecture source tests).
	TestFalse(TEXT("Blocked or parried hit: no reaction started"), Without([](auto& E) { E.bReactionStarted = false; }));
	TestFalse(TEXT("Super armor"), Without([](auto& E) { E.bSuperArmor = true; }));
	TestFalse(TEXT("Suppressed paired state"), Without([](auto& E) { E.bReactionsSuppressed = true; }));
	TestFalse(TEXT("Dying victim"), Without([](auto& E) { E.bAlive = false; }));
	TestFalse(TEXT("Legacy fallback path"), Without([](auto& E) { E.bSettingsPath = false; }));
	TestFalse(TEXT("Non-interrupting reaction (step 4 additive flinch)"), Without([](auto& E) { E.bInterruptingReaction = false; }));
	return true;
}
