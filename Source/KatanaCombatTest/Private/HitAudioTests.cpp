// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"
#include "Data/AttackData.h"
#include "Sound/SoundWave.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"

// ============================================================================
// IMPACT AUDIO CONFIG TESTS
// ============================================================================

/**
 * Test: FImpactAudioConfig defaults are correct.
 * Verifies null sound, 1.0 volume/pitch, 0.05 pitch variation, weapon fallback on.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitAudioConfigDefaultsTest, "KatanaCombat.HitAudio.Config.DefaultValues", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitAudioConfigDefaultsTest::RunTest(const FString& Parameters)
{
	FImpactAudioConfig Config;

	TestTrue("ImpactSound defaults to nullptr", Config.ImpactSound == nullptr);
	TestEqual("VolumeMultiplier defaults to 1.0", Config.VolumeMultiplier, 1.0f);
	TestEqual("PitchMultiplier defaults to 1.0", Config.PitchMultiplier, 1.0f);
	TestEqual("PitchVariation defaults to 0.05", Config.PitchVariation, 0.05f);
	TestTrue("bUseWeaponFallback defaults to true", Config.bUseWeaponFallback);

	return true;
}

/**
 * Test: FImpactAudioConfig::IsActive returns correct results.
 * Null sound + no fallback = inactive. Null sound + fallback = active.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitAudioConfigIsActiveTest, "KatanaCombat.HitAudio.Config.IsActiveValidation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitAudioConfigIsActiveTest::RunTest(const FString& Parameters)
{
	// Default: null sound + weapon fallback enabled = active
	FImpactAudioConfig DefaultConfig;
	TestTrue("Default config is active (weapon fallback)", DefaultConfig.IsActive());

	// Null sound + no fallback = inactive
	FImpactAudioConfig InactiveConfig;
	InactiveConfig.bUseWeaponFallback = false;
	TestFalse("Null sound + no fallback is inactive", InactiveConfig.IsActive());

	return true;
}

/**
 * Test: FImpactAudioConfig::GetRandomizedPitch returns values within expected range.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitAudioPitchRandomizationTest, "KatanaCombat.HitAudio.Config.PitchRandomization", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitAudioPitchRandomizationTest::RunTest(const FString& Parameters)
{
	FImpactAudioConfig Config;
	Config.PitchMultiplier = 1.0f;
	Config.PitchVariation = 0.1f;

	// Sample multiple times to verify range
	const int32 NumSamples = 100;
	float MinPitch = 10.0f;
	float MaxPitch = -10.0f;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float Pitch = Config.GetRandomizedPitch();
		MinPitch = FMath::Min(MinPitch, Pitch);
		MaxPitch = FMath::Max(MaxPitch, Pitch);

		// Each sample must be within [0.1, 4.0] (the clamped output range)
		TestTrue("Pitch within clamped range", Pitch >= 0.1f && Pitch <= 4.0f);
	}

	// With 100 samples and ±0.1 variation, we should see spread
	// Min should be <= 1.0 and Max should be >= 1.0 (statistically near-certain)
	TestTrue("Min pitch is at or below base", MinPitch <= 1.0f);
	TestTrue("Max pitch is at or above base", MaxPitch >= 1.0f);

	// With zero variation, pitch should equal multiplier exactly
	FImpactAudioConfig NoVariation;
	NoVariation.PitchMultiplier = 1.5f;
	NoVariation.PitchVariation = 0.0f;
	TestEqual("Zero variation returns exact multiplier", NoVariation.GetRandomizedPitch(), 1.5f);

	return true;
}

// ============================================================================
// PLAY IMPACT SOUND NULL SAFETY
// ============================================================================

/**
 * Test: PlayImpactSound handles null world/sound without crashing.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitAudioPlayNullSafetyTest, "KatanaCombat.HitAudio.PlayImpactSound.NullSafety", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitAudioPlayNullSafetyTest::RunTest(const FString& Parameters)
{
	FImpactAudioConfig Config;
	Config.bUseWeaponFallback = false; // No fallback, no sound

	// Null world should return false
	TestFalse("Null world returns false",
		UCinematicEffectsUtilityLibrary::PlayImpactSound(nullptr, Config, nullptr, FVector::ZeroVector));

	// Valid world but no sound (null config sound + no fallback + null weapon sound)
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	if (World)
	{
		TestFalse("No sound available returns false",
			UCinematicEffectsUtilityLibrary::PlayImpactSound(World, Config, nullptr, FVector::ZeroVector));

		FCombatTestHelpers::DestroyTestWorld(World);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitAudioPlaybackInvocationTest,
	"KatanaCombat.HitAudio.PlayImpactSound.PlaybackInvocation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitAudioPlaybackInvocationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	USoundWave* Sound = NewObject<USoundWave>(World);
	if (!World || !Sound)
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	bool bInvoked = false;
	const FVector ExpectedLocation(10.0, 20.0, 30.0);
	const FDelegateHandle Handle =
		UCinematicEffectsUtilityLibrary::OnImpactSoundPlaybackInvokedForTesting.AddLambda(
			[&bInvoked, World, Sound, ExpectedLocation](
				UWorld* PlayedWorld,
				USoundBase* PlayedSound,
				const FVector& Location,
				AActor* Attacker)
			{
				bInvoked = PlayedWorld == World
					&& PlayedSound == Sound
					&& Location.Equals(ExpectedLocation)
					&& Attacker == nullptr;
			});

	FImpactAudioConfig Config;
	Config.ImpactSound = Sound;
	Config.bUseWeaponFallback = false;
	const bool bPlayed = UCinematicEffectsUtilityLibrary::PlayImpactSound(
		World, Config, nullptr, ExpectedLocation);
	UCinematicEffectsUtilityLibrary::OnImpactSoundPlaybackInvokedForTesting.Remove(Handle);

	TestTrue(TEXT("A concrete impact sound should reach the engine playback call"), bPlayed);
	TestTrue(TEXT("The playback-site observer should receive the resolved sound"), bInvoked);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// ATTACK DATA INTEGRATION TESTS
// ============================================================================

/**
 * Test: UAttackData has ImpactAudioConfig field.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitAudioAttackDataFieldTest, "KatanaCombat.HitAudio.AttackData.HasAudioField", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitAudioAttackDataFieldTest::RunTest(const FString& Parameters)
{
	UAttackData* Attack = NewObject<UAttackData>();
	TestNotNull("AttackData created", Attack);
	if (!Attack) return false;

	// ImpactAudioConfig should exist with defaults
	TestTrue("ImpactAudioConfig.ImpactSound defaults to nullptr", Attack->ImpactAudioConfig.ImpactSound == nullptr);
	TestTrue("ImpactAudioConfig.bUseWeaponFallback defaults to true", Attack->ImpactAudioConfig.bUseWeaponFallback);
	TestEqual("ImpactAudioConfig.VolumeMultiplier defaults to 1.0", Attack->ImpactAudioConfig.VolumeMultiplier, 1.0f);

	// ImpactVFXConfig scaffold should exist
	TestTrue("ImpactVFXConfig.ImpactVFX defaults to nullptr", Attack->ImpactVFXConfig.ImpactVFX == nullptr);
	TestTrue("ImpactVFXConfig.bUseWeaponFallback defaults to true", Attack->ImpactVFXConfig.bUseWeaponFallback);

	return true;
}

// ============================================================================
// HIT REACTION INFO NEW FIELDS
// ============================================================================

/**
 * Test: FHitReactionInfo has ImpactNormal and BoneName fields with defaults.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitAudioHitInfoFieldsTest, "KatanaCombat.HitAudio.HitReactionInfo.HasNewFields", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitAudioHitInfoFieldsTest::RunTest(const FString& Parameters)
{
	FHitReactionInfo HitInfo;

	// New fields should exist with defaults
	TestEqual("ImpactNormal defaults to UpVector", HitInfo.ImpactNormal, FVector::UpVector);
	TestEqual("BoneName defaults to NAME_None", HitInfo.BoneName, FName(NAME_None));

	// Fields should be writable
	HitInfo.ImpactNormal = FVector::ForwardVector;
	HitInfo.BoneName = FName("spine_03");
	TestEqual("ImpactNormal can be set", HitInfo.ImpactNormal, FVector::ForwardVector);
	TestEqual("BoneName can be set", HitInfo.BoneName, FName("spine_03"));

	return true;
}

// ============================================================================
// ON ATTACK HIT DELEGATE
// ============================================================================

/**
 * Test: CombatComponent has OnAttackHit delegate UPROPERTY.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitAudioDelegateExistsTest, "KatanaCombat.HitAudio.Delegate.OnAttackHitExists", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitAudioDelegateExistsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	if (!Player)
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UCombatComponent* CombatComp = Player->CombatComponent.Get();
	TestNotNull("Player should have CombatComponent", CombatComp);
	if (!CombatComp)
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// OnAttackHit delegate should be valid (not bound but accessible)
	TestTrue("OnAttackHit is not bound initially", !CombatComp->OnAttackHit.IsBound());

	// Verify the delegate is a proper UPROPERTY by checking it through reflection
	UClass* CombatClass = UCombatComponent::StaticClass();
	FProperty* DelegateProp = CombatClass->FindPropertyByName(FName("OnAttackHit"));
	TestNotNull("OnAttackHit property exists via reflection", DelegateProp);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
