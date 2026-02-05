// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "CombatTypes.h"
#include "Data/CombatFXData.h"
#include "Data/WeaponData.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"

// ============================================================================
// IMPACT SOUND ENTRY TESTS
// ============================================================================

/**
 * Test: FImpactSoundEntry defaults are correct.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactSoundEntryDefaultsTest, "KatanaCombat.ImpactFXPool.Config.SoundEntry.DefaultValues", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FImpactSoundEntryDefaultsTest::RunTest(const FString& Parameters)
{
	FImpactSoundEntry Entry;

	TestTrue("Sound defaults to nullptr", Entry.Sound == nullptr);
	TestEqual("VolumeMultiplier defaults to 1.0", Entry.VolumeMultiplier, 1.0f);
	TestEqual("PitchMultiplier defaults to 1.0", Entry.PitchMultiplier, 1.0f);

	return true;
}

/**
 * Test: FImpactSoundEntry::IsValid returns correct results.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactSoundEntryIsValidTest, "KatanaCombat.ImpactFXPool.Config.SoundEntry.IsValid", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FImpactSoundEntryIsValidTest::RunTest(const FString& Parameters)
{
	FImpactSoundEntry Empty;
	TestFalse("Empty entry is invalid", Empty.IsValid());

	// Note: Can't easily test valid case without creating actual USoundBase asset
	// The logic is trivial (Sound != nullptr), so this is sufficient

	return true;
}

// ============================================================================
// IMPACT VFX ENTRY TESTS (Scaffold)
// ============================================================================

/**
 * Test: FImpactVFXEntry defaults are correct.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactVFXEntryDefaultsTest, "KatanaCombat.ImpactFXPool.Config.VFXEntry.DefaultValues", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FImpactVFXEntryDefaultsTest::RunTest(const FString& Parameters)
{
	FImpactVFXEntry Entry;

	TestTrue("VFX defaults to nullptr", Entry.VFX == nullptr);
	TestEqual("ScaleMultiplier defaults to 1.0", Entry.ScaleMultiplier, 1.0f);

	return true;
}

/**
 * Test: FImpactVFXEntry::IsValid returns correct results.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactVFXEntryIsValidTest, "KatanaCombat.ImpactFXPool.Config.VFXEntry.IsValid", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FImpactVFXEntryIsValidTest::RunTest(const FString& Parameters)
{
	FImpactVFXEntry Empty;
	TestFalse("Empty entry is invalid", Empty.IsValid());

	return true;
}

// ============================================================================
// IMPACT FX POOL TESTS
// ============================================================================

/**
 * Test: FImpactFXPool defaults are correct.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactFXPoolDefaultsTest, "KatanaCombat.ImpactFXPool.Pool.DefaultValues", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FImpactFXPoolDefaultsTest::RunTest(const FString& Parameters)
{
	FImpactFXPool Pool;

	TestEqual("ImpactSounds array is empty", Pool.ImpactSounds.Num(), 0);
	TestEqual("ImpactVFX array is empty", Pool.ImpactVFX.Num(), 0);
	TestEqual("PitchVariation defaults to 0.05", Pool.PitchVariation, 0.05f);
	TestTrue("bAlignVFXToSurface defaults to true", Pool.bAlignVFXToSurface);

	return true;
}

/**
 * Test: FImpactFXPool::GetRandomSound returns nullptr for empty pool.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactFXPoolEmptyRandomTest, "KatanaCombat.ImpactFXPool.Pool.GetRandomSound.EmptyPool", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FImpactFXPoolEmptyRandomTest::RunTest(const FString& Parameters)
{
	FImpactFXPool Pool;
	TestTrue("Empty pool returns nullptr", Pool.GetRandomSound() == nullptr);

	// Pool with entries but all invalid (null Sound)
	FImpactFXPool PoolWithNulls;
	PoolWithNulls.ImpactSounds.Add(FImpactSoundEntry());
	PoolWithNulls.ImpactSounds.Add(FImpactSoundEntry());
	TestTrue("Pool with only null entries returns nullptr", PoolWithNulls.GetRandomSound() == nullptr);

	return true;
}

/**
 * Test: FImpactFXPool::HasSounds correctly reports pool status.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactFXPoolHasSoundsTest, "KatanaCombat.ImpactFXPool.Pool.HasSounds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FImpactFXPoolHasSoundsTest::RunTest(const FString& Parameters)
{
	FImpactFXPool EmptyPool;
	TestFalse("Empty pool has no sounds", EmptyPool.HasSounds());

	FImpactFXPool PoolWithNulls;
	PoolWithNulls.ImpactSounds.Add(FImpactSoundEntry());
	TestFalse("Pool with only null sounds has no sounds", PoolWithNulls.HasSounds());

	return true;
}

/**
 * Test: FImpactFXPool::HasVFX correctly reports pool status (scaffold).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactFXPoolHasVFXTest, "KatanaCombat.ImpactFXPool.Pool.HasVFX", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FImpactFXPoolHasVFXTest::RunTest(const FString& Parameters)
{
	FImpactFXPool EmptyPool;
	TestFalse("Empty pool has no VFX", EmptyPool.HasVFX());

	FImpactFXPool PoolWithNulls;
	PoolWithNulls.ImpactVFX.Add(FImpactVFXEntry());
	TestFalse("Pool with only null VFX has no VFX", PoolWithNulls.HasVFX());

	return true;
}

// ============================================================================
// COMBAT FX DATA ASSET TESTS
// ============================================================================

/**
 * Test: UCombatFXData can be created and has empty pools by default.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFXDataCreationTest, "KatanaCombat.ImpactFXPool.CombatFXData.Creation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatFXDataCreationTest::RunTest(const FString& Parameters)
{
	UCombatFXData* FXData = NewObject<UCombatFXData>();
	TestNotNull("CombatFXData created", FXData);
	if (!FXData) return false;

	TestEqual("AttackTypePools is empty by default", FXData->AttackTypePools.Num(), 0);
	TestEqual("SurfacePools is empty by default", FXData->SurfacePools.Num(), 0);
	TestFalse("bUseBlockedPool is false by default", FXData->bUseBlockedPool);

	return true;
}

/**
 * Test: UCombatFXData::GetPoolForAttackType returns correct pool.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFXDataGetPoolTest, "KatanaCombat.ImpactFXPool.CombatFXData.GetPoolForAttackType", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatFXDataGetPoolTest::RunTest(const FString& Parameters)
{
	UCombatFXData* FXData = NewObject<UCombatFXData>();
	if (!FXData) return false;

	// Empty - should return nullptr
	TestTrue("No Light pool by default", FXData->GetPoolForAttackType(EAttackType::Light) == nullptr);

	// Add a pool
	FImpactFXPool LightPool;
	LightPool.PitchVariation = 0.1f; // Unique value to identify
	FXData->AttackTypePools.Add(EAttackType::Light, LightPool);

	// Should now find it
	const FImpactFXPool* Found = FXData->GetPoolForAttackType(EAttackType::Light);
	TestNotNull("Light pool found after adding", Found);
	if (Found)
	{
		TestEqual("Found pool has correct PitchVariation", Found->PitchVariation, 0.1f);
	}

	// Heavy should still be null
	TestTrue("Heavy pool still null", FXData->GetPoolForAttackType(EAttackType::Heavy) == nullptr);

	return true;
}

/**
 * Test: UCombatFXData::GetPoolForAttackType returns nullptr for unconfigured type.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFXDataGetPoolNotConfiguredTest, "KatanaCombat.ImpactFXPool.CombatFXData.GetPoolForAttackType.NotConfigured", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatFXDataGetPoolNotConfiguredTest::RunTest(const FString& Parameters)
{
	UCombatFXData* FXData = NewObject<UCombatFXData>();
	if (!FXData) return false;

	// None configured
	TestTrue("None type returns nullptr", FXData->GetPoolForAttackType(EAttackType::None) == nullptr);
	TestTrue("Light type returns nullptr", FXData->GetPoolForAttackType(EAttackType::Light) == nullptr);
	TestTrue("Heavy type returns nullptr", FXData->GetPoolForAttackType(EAttackType::Heavy) == nullptr);
	TestTrue("Special type returns nullptr", FXData->GetPoolForAttackType(EAttackType::Special) == nullptr);

	return true;
}

/**
 * Test: UCombatFXData::ResolvePool resolves to attack type pool.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFXDataResolvePoolAttackTypeTest, "KatanaCombat.ImpactFXPool.CombatFXData.ResolvePool.AttackTypeOnly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatFXDataResolvePoolAttackTypeTest::RunTest(const FString& Parameters)
{
	UCombatFXData* FXData = NewObject<UCombatFXData>();
	if (!FXData) return false;

	FImpactFXPool LightPool;
	LightPool.PitchVariation = 0.15f;
	FXData->AttackTypePools.Add(EAttackType::Light, LightPool);

	// Resolve should find the Light pool
	const FImpactFXPool* Resolved = FXData->ResolvePool(EAttackType::Light, false);
	TestNotNull("Resolved Light pool", Resolved);
	if (Resolved)
	{
		TestEqual("Resolved pool has correct PitchVariation", Resolved->PitchVariation, 0.15f);
	}

	// Non-existent type should return nullptr
	TestTrue("Heavy not configured returns nullptr", FXData->ResolvePool(EAttackType::Heavy, false) == nullptr);

	return true;
}

/**
 * Test: UCombatFXData::ResolvePool uses blocked pool when configured.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFXDataResolvePoolBlockedTest, "KatanaCombat.ImpactFXPool.CombatFXData.ResolvePool.BlockedOverride", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatFXDataResolvePoolBlockedTest::RunTest(const FString& Parameters)
{
	UCombatFXData* FXData = NewObject<UCombatFXData>();
	if (!FXData) return false;

	// Setup attack type pool
	FImpactFXPool LightPool;
	LightPool.PitchVariation = 0.15f;
	FXData->AttackTypePools.Add(EAttackType::Light, LightPool);

	// Not blocked, no blocked pool - should get attack type
	const FImpactFXPool* NotBlocked = FXData->ResolvePool(EAttackType::Light, false);
	TestNotNull("Not blocked gets attack type pool", NotBlocked);

	// Blocked but bUseBlockedPool = false - should still get attack type
	const FImpactFXPool* BlockedNoFlag = FXData->ResolvePool(EAttackType::Light, true);
	TestNotNull("Blocked but flag off gets attack type pool", BlockedNoFlag);

	// Enable blocked pool but leave it empty (no sounds)
	FXData->bUseBlockedPool = true;
	const FImpactFXPool* BlockedEmptyPool = FXData->ResolvePool(EAttackType::Light, true);
	TestNotNull("Blocked with empty pool falls back to attack type", BlockedEmptyPool);
	if (BlockedEmptyPool)
	{
		TestEqual("Falls back to attack type pool", BlockedEmptyPool->PitchVariation, 0.15f);
	}

	return true;
}

/**
 * Test: UCombatFXData::ResolvePool surface pool priority (scaffold).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFXDataResolvePoolSurfaceTest, "KatanaCombat.ImpactFXPool.CombatFXData.ResolvePool.SurfaceOverride", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatFXDataResolvePoolSurfaceTest::RunTest(const FString& Parameters)
{
	UCombatFXData* FXData = NewObject<UCombatFXData>();
	if (!FXData) return false;

	// Default surface type skips surface lookup
	FImpactFXPool LightPool;
	LightPool.PitchVariation = 0.2f;
	FXData->AttackTypePools.Add(EAttackType::Light, LightPool);

	const FImpactFXPool* DefaultSurface = FXData->ResolvePool(EAttackType::Light, false, ECombatSurfaceType::Default);
	TestNotNull("Default surface uses attack type pool", DefaultSurface);
	if (DefaultSurface)
	{
		TestEqual("Got attack type pool", DefaultSurface->PitchVariation, 0.2f);
	}

	// Non-default surface but no pool configured - falls back to attack type
	const FImpactFXPool* NoSurfacePool = FXData->ResolvePool(EAttackType::Light, false, ECombatSurfaceType::Metal);
	TestNotNull("Missing surface pool falls back to attack type", NoSurfacePool);

	return true;
}

// ============================================================================
// RESOLUTION CHAIN TESTS
// ============================================================================

/**
 * Test: ResolveAndPlayImpactSound handles null world safely.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResolutionNullSafetyTest, "KatanaCombat.ImpactFXPool.Resolution.NullSafety", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FResolutionNullSafetyTest::RunTest(const FString& Parameters)
{
	FImpactAudioConfig Config;
	Config.bUseWeaponFallback = false;

	// Null world should return false
	bool Result = UCinematicEffectsUtilityLibrary::ResolveAndPlayImpactSound(
		nullptr, Config, nullptr, EAttackType::Light, nullptr, FVector::ZeroVector, false, nullptr);
	TestFalse("Null world returns false", Result);

	return true;
}

/**
 * Test: ResolveAndPlayImpactSound with null CombatFXData falls through correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResolutionNullFXDataTest, "KatanaCombat.ImpactFXPool.Resolution.NullCombatFXData", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FResolutionNullFXDataTest::RunTest(const FString& Parameters)
{
	FImpactAudioConfig Config;
	Config.bUseWeaponFallback = false; // No fallback, no per-attack sound

	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	if (World)
	{
		// No per-attack sound, no CombatFXData, no weapon fallback = silent (Tier 4)
		bool Result = UCinematicEffectsUtilityLibrary::ResolveAndPlayImpactSound(
			World, Config, nullptr, EAttackType::Light, nullptr, FVector::ZeroVector, false, nullptr);
		TestFalse("No sound sources returns false (silent)", Result);

		FCombatTestHelpers::DestroyTestWorld(World);
	}

	return true;
}

// ============================================================================
// WEAPON DATA INTEGRATION TESTS
// ============================================================================

/**
 * Test: UWeaponData has CombatFXData field.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponDataHasCombatFXDataTest, "KatanaCombat.ImpactFXPool.WeaponData.HasCombatFXDataField", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWeaponDataHasCombatFXDataTest::RunTest(const FString& Parameters)
{
	// Verify via reflection that CombatFXData property exists
	UClass* WeaponClass = UWeaponData::StaticClass();
	FProperty* FXDataProp = WeaponClass->FindPropertyByName(FName("CombatFXData"));
	TestNotNull("CombatFXData property exists via reflection", FXDataProp);

	return true;
}

/**
 * Test: UWeaponData CombatFXData defaults to nullptr.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponDataCombatFXDataDefaultsTest, "KatanaCombat.ImpactFXPool.WeaponData.DefaultsToNull", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWeaponDataCombatFXDataDefaultsTest::RunTest(const FString& Parameters)
{
	UWeaponData* Weapon = NewObject<UWeaponData>();
	TestNotNull("WeaponData created", Weapon);
	if (!Weapon) return false;

	TestTrue("CombatFXData defaults to nullptr", Weapon->CombatFXData == nullptr);

	return true;
}

// ============================================================================
// SURFACE TYPE ENUM TEST
// ============================================================================

/**
 * Test: ECombatSurfaceType enum values exist.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSurfaceTypeEnumTest, "KatanaCombat.ImpactFXPool.SurfaceType.EnumValues", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatSurfaceTypeEnumTest::RunTest(const FString& Parameters)
{
	// Just verify the enum values compile and are distinct
	TestEqual("Default is 0", static_cast<uint8>(ECombatSurfaceType::Default), 0);
	TestEqual("Flesh is 1", static_cast<uint8>(ECombatSurfaceType::Flesh), 1);
	TestEqual("Armor is 2", static_cast<uint8>(ECombatSurfaceType::Armor), 2);
	TestEqual("Wood is 3", static_cast<uint8>(ECombatSurfaceType::Wood), 3);
	TestEqual("Stone is 4", static_cast<uint8>(ECombatSurfaceType::Stone), 4);
	TestEqual("Metal is 5", static_cast<uint8>(ECombatSurfaceType::Metal), 5);

	return true;
}

// ============================================================================
// IMPACT VFX CONFIG TESTS (U-16)
// ============================================================================

/**
 * Test: FImpactVFXConfig defaults are correct.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactVFXConfigDefaultsTest, "KatanaCombat.ImpactFXPool.VFXConfig.DefaultValues", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FImpactVFXConfigDefaultsTest::RunTest(const FString& Parameters)
{
	FImpactVFXConfig Config;

	TestTrue("ImpactVFX defaults to nullptr", Config.ImpactVFX == nullptr);
	TestEqual("ScaleMultiplier defaults to 1.0", Config.ScaleMultiplier, 1.0f);
	TestTrue("bAlignToSurface defaults to true", Config.bAlignToSurface);
	TestTrue("bUseWeaponFallback defaults to true", Config.bUseWeaponFallback);

	return true;
}

/**
 * Test: FImpactVFXConfig::IsActive returns correct results.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactVFXConfigIsActiveTest, "KatanaCombat.ImpactFXPool.VFXConfig.IsActive", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FImpactVFXConfigIsActiveTest::RunTest(const FString& Parameters)
{
	FImpactVFXConfig DefaultConfig;
	TestTrue("Default config is active (bUseWeaponFallback = true)", DefaultConfig.IsActive());

	FImpactVFXConfig NoFallbackConfig;
	NoFallbackConfig.bUseWeaponFallback = false;
	TestFalse("No VFX and no fallback is inactive", NoFallbackConfig.IsActive());

	// Note: Can't test with actual VFX asset without loading one

	return true;
}

/**
 * Test: FImpactFXPool::GetRandomVFX returns nullptr for empty pool.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactFXPoolEmptyVFXRandomTest, "KatanaCombat.ImpactFXPool.Pool.GetRandomVFX.EmptyPool", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FImpactFXPoolEmptyVFXRandomTest::RunTest(const FString& Parameters)
{
	FImpactFXPool Pool;
	TestTrue("Empty pool returns nullptr", Pool.GetRandomVFX() == nullptr);

	// Pool with entries but all invalid (null VFX)
	FImpactFXPool PoolWithNulls;
	PoolWithNulls.ImpactVFX.Add(FImpactVFXEntry());
	PoolWithNulls.ImpactVFX.Add(FImpactVFXEntry());
	TestTrue("Pool with only null entries returns nullptr", PoolWithNulls.GetRandomVFX() == nullptr);

	return true;
}

// ============================================================================
// VFX RESOLUTION CHAIN TESTS (U-16)
// ============================================================================

/**
 * Test: ResolveAndSpawnImpactVFX handles null world safely.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFXResolutionNullSafetyTest, "KatanaCombat.ImpactFXPool.VFX.Resolution.NullSafety", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVFXResolutionNullSafetyTest::RunTest(const FString& Parameters)
{
	FImpactVFXConfig Config;
	Config.bUseWeaponFallback = false;

	// Null world should return false
	bool Result = UCinematicEffectsUtilityLibrary::ResolveAndSpawnImpactVFX(
		nullptr, Config, nullptr, EAttackType::Light, nullptr,
		FVector::ZeroVector, FVector::UpVector, false, NAME_None);
	TestFalse("Null world returns false", Result);

	return true;
}

/**
 * Test: ResolveAndSpawnImpactVFX with null CombatFXData falls through correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVFXResolutionNullFXDataTest, "KatanaCombat.ImpactFXPool.VFX.Resolution.NullCombatFXData", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVFXResolutionNullFXDataTest::RunTest(const FString& Parameters)
{
	FImpactVFXConfig Config;
	Config.bUseWeaponFallback = false; // No fallback, no per-attack VFX

	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	if (World)
	{
		// No per-attack VFX, no CombatFXData, no weapon fallback = nothing (Tier 4)
		bool Result = UCinematicEffectsUtilityLibrary::ResolveAndSpawnImpactVFX(
			World, Config, nullptr, EAttackType::Light, nullptr,
			FVector::ZeroVector, FVector::UpVector, false, NAME_None);
		TestFalse("No VFX sources returns false", Result);

		FCombatTestHelpers::DestroyTestWorld(World);
	}

	return true;
}

/**
 * Test: SpawnImpactVFX handles null world safely.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpawnImpactVFXNullWorldTest, "KatanaCombat.ImpactFXPool.VFX.SpawnImpactVFX.NullWorld", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSpawnImpactVFXNullWorldTest::RunTest(const FString& Parameters)
{
	FImpactVFXConfig Config;

	bool Result = UCinematicEffectsUtilityLibrary::SpawnImpactVFX(
		nullptr, Config, nullptr, FVector::ZeroVector, FVector::UpVector, NAME_None);
	TestFalse("Null world returns false", Result);

	return true;
}

/**
 * Test: SpawnImpactVFX with no VFX configured returns false gracefully.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpawnImpactVFXNoVFXTest, "KatanaCombat.ImpactFXPool.VFX.SpawnImpactVFX.NoVFX", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSpawnImpactVFXNoVFXTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	if (World)
	{
		FImpactVFXConfig Config;
		Config.bUseWeaponFallback = false; // No VFX, no fallback

		bool Result = UCinematicEffectsUtilityLibrary::SpawnImpactVFX(
			World, Config, nullptr, FVector::ZeroVector, FVector::UpVector, NAME_None);
		TestFalse("No VFX configured returns false", Result);

		FCombatTestHelpers::DestroyTestWorld(World);
	}

	return true;
}

// ============================================================================
// WEAPON DATA VFX INTEGRATION TESTS (U-16)
// ============================================================================

/**
 * Test: UWeaponData has HitVFX field.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponDataHasHitVFXTest, "KatanaCombat.ImpactFXPool.WeaponData.HasHitVFXField", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWeaponDataHasHitVFXTest::RunTest(const FString& Parameters)
{
	// Verify via reflection that HitVFX property exists
	UClass* WeaponClass = UWeaponData::StaticClass();
	FProperty* HitVFXProp = WeaponClass->FindPropertyByName(FName("HitVFX"));
	TestNotNull("HitVFX property exists via reflection", HitVFXProp);

	return true;
}

/**
 * Test: UWeaponData HitVFX defaults to nullptr.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponDataHitVFXDefaultsTest, "KatanaCombat.ImpactFXPool.WeaponData.HitVFXDefaultsToNull", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWeaponDataHitVFXDefaultsTest::RunTest(const FString& Parameters)
{
	UWeaponData* Weapon = NewObject<UWeaponData>();
	TestNotNull("WeaponData created", Weapon);
	if (!Weapon) return false;

	TestTrue("HitVFX defaults to nullptr", Weapon->HitVFX == nullptr);

	return true;
}
