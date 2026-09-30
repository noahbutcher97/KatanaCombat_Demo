#include "CombatTestHelpers.h"
#include "Utilities/KnockbackResolution.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"

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
	TestEqual(TEXT("Heavy default distance"), H.Distance, 60.0f);
	TestEqual(TEXT("Heavy default duration"), H.Duration, 0.25f);
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
	const FVector OverheadChop = -FVector(0.3, 0, -0.95).GetSafeNormal();
	TestTrue(TEXT("Mostly vertical swing falls back to away"), KnockbackResolution::ResolveDirection(EKnockbackDirection::AlongSwing, Attacker, Victim, OverheadChop).Equals(FVector(1, 0, 0), 1e-4));
	const FVector BackSwing = FVector(1, 0, 0); // blade moving toward the attacker
	TestTrue(TEXT("Swing toward the attacker falls back to away"), KnockbackResolution::ResolveDirection(EKnockbackDirection::AlongSwing, Attacker, Victim, BackSwing).Equals(FVector(1, 0, 0), 1e-4));
	TestTrue(TEXT("Stacked actors have no direction"), KnockbackResolution::ResolveDirection(EKnockbackDirection::AwayFromAttacker, FVector(0, 0, 0), FVector(0, 0, 150), SidewaysSwing).IsZero());
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
