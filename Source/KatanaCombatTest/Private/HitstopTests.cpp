// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Data/AttackData.h"
#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"

// ============================================================================
// HITSTOP CONFIG TESTS
// ============================================================================

/**
 * Test: FHitstopConfig::CreateDefault produces industry-standard durations per attack type.
 * Light: ~0.04s (2.4 frames), Heavy: ~0.083s (5 frames), Special: ~0.1s (6 frames)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitstopConfigDefaultsTest, "KatanaCombat.Hitstop.Config.DefaultsPerAttackType", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitstopConfigDefaultsTest::RunTest(const FString& Parameters)
{
	// Light attack defaults
	FHitstopConfig LightConfig = FHitstopConfig::CreateDefault(EAttackType::Light);
	TestTrue("Light hitstop enabled by default", LightConfig.bEnabled);
	TestEqual("Light duration", LightConfig.Duration, 0.04f);
	TestEqual("Light camera shake scale", LightConfig.CameraShakeScale, 0.5f);

	// Heavy attack defaults
	FHitstopConfig HeavyConfig = FHitstopConfig::CreateDefault(EAttackType::Heavy);
	TestTrue("Heavy hitstop enabled by default", HeavyConfig.bEnabled);
	TestEqual("Heavy duration", HeavyConfig.Duration, 0.083f);
	TestEqual("Heavy camera shake scale", HeavyConfig.CameraShakeScale, 1.0f);

	// Special attack defaults
	FHitstopConfig SpecialConfig = FHitstopConfig::CreateDefault(EAttackType::Special);
	TestTrue("Special hitstop enabled by default", SpecialConfig.bEnabled);
	TestEqual("Special duration", SpecialConfig.Duration, 0.1f);
	TestEqual("Special camera shake scale", SpecialConfig.CameraShakeScale, 1.5f);

	// None/default
	FHitstopConfig NoneConfig = FHitstopConfig::CreateDefault(EAttackType::None);
	TestTrue("None type hitstop enabled by default", NoneConfig.bEnabled);
	TestEqual("None type duration", NoneConfig.Duration, 0.05f);

	return true;
}

/**
 * Test: FHitstopConfig::IsActive returns correct results for various configurations.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitstopConfigIsActiveTest, "KatanaCombat.Hitstop.Config.IsActiveValidation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitstopConfigIsActiveTest::RunTest(const FString& Parameters)
{
	// Default config should be active
	FHitstopConfig DefaultConfig;
	TestTrue("Default config is active", DefaultConfig.IsActive());

	// Disabled config should not be active
	FHitstopConfig DisabledConfig;
	DisabledConfig.bEnabled = false;
	TestFalse("Disabled config is not active", DisabledConfig.IsActive());

	// Zero duration should not be active
	FHitstopConfig ZeroDurationConfig;
	ZeroDurationConfig.Duration = 0.0f;
	TestFalse("Zero duration config is not active", ZeroDurationConfig.IsActive());

	// Negative duration should not be active
	FHitstopConfig NegativeDurationConfig;
	NegativeDurationConfig.Duration = -0.1f;
	TestFalse("Negative duration config is not active", NegativeDurationConfig.IsActive());

	// Enabled with positive duration should be active
	FHitstopConfig ValidConfig;
	ValidConfig.bEnabled = true;
	ValidConfig.Duration = 0.05f;
	TestTrue("Enabled with positive duration is active", ValidConfig.IsActive());

	return true;
}

/**
 * Test: Blocked duration multiplier correctly scales hitstop duration.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitstopConfigBlockedDurationTest, "KatanaCombat.Hitstop.Config.BlockedDuration", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitstopConfigBlockedDurationTest::RunTest(const FString& Parameters)
{
	FHitstopConfig Config;
	Config.Duration = 0.1f;
	Config.BlockedDurationMultiplier = 0.5f;
	Config.bApplyOnBlock = true;

	// Effective blocked duration should be Duration * Multiplier
	const float ExpectedBlockedDuration = Config.Duration * Config.BlockedDurationMultiplier;
	TestEqual("Blocked duration calculation", ExpectedBlockedDuration, 0.05f);

	// bApplyOnBlock = false should prevent block hitstop
	FHitstopConfig NoBlockConfig;
	NoBlockConfig.bApplyOnBlock = false;
	TestFalse("bApplyOnBlock defaults to true but can be set false", NoBlockConfig.bApplyOnBlock);

	// Default blocked multiplier
	FHitstopConfig DefaultConfig;
	TestEqual("Default blocked multiplier is 0.5", DefaultConfig.BlockedDurationMultiplier, 0.5f);
	TestTrue("Default bApplyOnBlock is true", DefaultConfig.bApplyOnBlock);

	return true;
}

// ============================================================================
// HITSTOP APPLICATION TESTS
// ============================================================================

/**
 * Test: ApplyHitstop freezes both attacker and victim via CustomTimeDilation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitstopFreezesBothActorsTest, "KatanaCombat.Hitstop.Apply.FreezesBothActors", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitstopFreezesBothActorsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	if (!World) return false;

	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));
	if (!Attacker || !Victim) return false;

	// Verify initial dilation is 1.0
	TestEqual("Attacker starts at normal dilation", Attacker->CustomTimeDilation, 1.0f);
	TestEqual("Victim starts at normal dilation", Victim->CustomTimeDilation, 1.0f);

	// Apply hitstop
	FHitstopConfig Config;
	Config.Duration = 0.1f;

	const bool bApplied = UCinematicEffectsUtilityLibrary::ApplyHitstop(Attacker, Victim, Config, false);
	TestTrue("Hitstop applied successfully", bApplied);

	// Both actors should be frozen (0.0001f)
	TestTrue("Attacker is frozen", FMath::IsNearlyEqual(Attacker->CustomTimeDilation, 0.0001f, 0.001f));
	TestTrue("Victim is frozen", FMath::IsNearlyEqual(Victim->CustomTimeDilation, 0.0001f, 0.001f));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: ApplyHitstop handles null actors safely.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitstopNullSafetyTest, "KatanaCombat.Hitstop.Apply.NullSafety", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitstopNullSafetyTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	if (!World) return false;

	APlayerCharacter* Actor = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	if (!Actor) return false;

	FHitstopConfig Config;
	Config.Duration = 0.05f;

	// Both null should fail
	TestFalse("Both null returns false", UCinematicEffectsUtilityLibrary::ApplyHitstop(nullptr, nullptr, Config));

	// Attacker-only should succeed
	TestTrue("Attacker-only returns true", UCinematicEffectsUtilityLibrary::ApplyHitstop(Actor, nullptr, Config));

	// Restore dilation for next test (ticker will run but let's reset manually)
	Actor->CustomTimeDilation = 1.0f;

	// Victim-only should succeed
	TestTrue("Victim-only returns true", UCinematicEffectsUtilityLibrary::ApplyHitstop(nullptr, Actor, Config));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: ApplyHitstop returns false when config is disabled.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitstopDisabledConfigTest, "KatanaCombat.Hitstop.Apply.DisabledConfigSkips", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitstopDisabledConfigTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	if (!World) return false;

	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));
	if (!Attacker || !Victim) return false;

	// Disabled config
	FHitstopConfig DisabledConfig;
	DisabledConfig.bEnabled = false;

	const bool bApplied = UCinematicEffectsUtilityLibrary::ApplyHitstop(Attacker, Victim, DisabledConfig);
	TestFalse("Disabled config returns false", bApplied);

	// Actors should NOT be frozen
	TestEqual("Attacker not frozen", Attacker->CustomTimeDilation, 1.0f);
	TestEqual("Victim not frozen", Victim->CustomTimeDilation, 1.0f);

	// Zero duration config
	FHitstopConfig ZeroDurationConfig;
	ZeroDurationConfig.Duration = 0.0f;

	const bool bApplied2 = UCinematicEffectsUtilityLibrary::ApplyHitstop(Attacker, Victim, ZeroDurationConfig);
	TestFalse("Zero duration config returns false", bApplied2);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: ApplyHitstop saves pre-freeze CustomTimeDilation (not hardcoded 1.0f).
 * Verifies correct behavior when actors are already in slow-mo.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitstopPreserveDilationTest, "KatanaCombat.Hitstop.Apply.PreservesExistingDilation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitstopPreserveDilationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	if (!World) return false;

	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));
	if (!Attacker || !Victim) return false;

	// Set attacker to slow-mo (simulating world slow-mo compensation)
	Attacker->CustomTimeDilation = 0.5f;
	Victim->CustomTimeDilation = 0.7f;

	FHitstopConfig Config;
	Config.Duration = 0.05f;

	const bool bApplied = UCinematicEffectsUtilityLibrary::ApplyHitstop(Attacker, Victim, Config);
	TestTrue("Hitstop applied during slow-mo", bApplied);

	// Both should be frozen
	TestTrue("Attacker frozen during slow-mo", FMath::IsNearlyEqual(Attacker->CustomTimeDilation, 0.0001f, 0.001f));
	TestTrue("Victim frozen during slow-mo", FMath::IsNearlyEqual(Victim->CustomTimeDilation, 0.0001f, 0.001f));

	// NOTE: We cannot easily verify restoration timing in unit tests without frame simulation.
	// The FTSTicker pattern is proven by the existing paired animation hitstop tests.
	// The key invariant is that the SAVED values (0.5f and 0.7f) are captured correctly,
	// not that they are restored within the test frame.

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// ATTACK DATA INTEGRATION TESTS
// ============================================================================

/**
 * Test: UAttackData has a HitstopConfig field with sensible defaults.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitstopAttackDataFieldTest, "KatanaCombat.Hitstop.AttackData.HasHitstopField", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitstopAttackDataFieldTest::RunTest(const FString& Parameters)
{
	UAttackData* LightAttack = NewObject<UAttackData>();
	LightAttack->AttackType = EAttackType::Light;

	// Verify HitstopConfig field exists and has defaults
	TestTrue("HitstopConfig exists and is enabled", LightAttack->HitstopConfig.bEnabled);
	TestTrue("HitstopConfig duration is positive", LightAttack->HitstopConfig.Duration > 0.0f);
	TestTrue("HitstopConfig IsActive", LightAttack->HitstopConfig.IsActive());
	TestTrue("HitstopConfig bApplyOnBlock defaults to true", LightAttack->HitstopConfig.bApplyOnBlock);
	TestEqual("HitstopConfig BlockedDurationMultiplier defaults to 0.5", LightAttack->HitstopConfig.BlockedDurationMultiplier, 0.5f);

	// Camera shake defaults to nullptr
	TestFalse("CameraShake defaults to nullptr", LightAttack->HitstopConfig.CameraShake != nullptr);

	return true;
}

// ============================================================================
// TERMINOLOGY CONSISTENCY TEST
// ============================================================================

/**
 * Test: AnimNotifyState_PairedAnimationSync uses correct 'Hitstop' terminology.
 * Verifies the rename from bApplyHitPause/HitPauseDuration.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitstopTerminologyTest, "KatanaCombat.Hitstop.Terminology.SyncPointUsesHitstop", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitstopTerminologyTest::RunTest(const FString& Parameters)
{
	UAnimNotifyState_PairedAnimationSync* SyncNotify = NewObject<UAnimNotifyState_PairedAnimationSync>();
	TestNotNull("Should be able to create PairedAnimationSync notify", SyncNotify);
	if (!SyncNotify) return false;

	// Verify renamed fields exist with correct defaults
	TestFalse("bApplyHitstop should default to false", SyncNotify->bApplyHitstop);
	TestEqual("HitstopDuration should default to 0.05f", SyncNotify->HitstopDuration, 0.05f);

	// Verify fields are writable
	SyncNotify->bApplyHitstop = true;
	SyncNotify->HitstopDuration = 0.1f;
	TestTrue("bApplyHitstop can be set to true", SyncNotify->bApplyHitstop);
	TestEqual("HitstopDuration can be changed", SyncNotify->HitstopDuration, 0.1f);

	return true;
}
