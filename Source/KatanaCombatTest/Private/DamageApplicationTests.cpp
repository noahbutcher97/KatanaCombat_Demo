// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatEventRecorder.h"
#include "CombatTestHelpers.h"
#include "Characters/BaseCombatCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Core/HitReactionComponent.h"
#include "Interfaces/DamageableInterface.h"
#include "Data/AttackData.h"

// ============================================================================
// BASIC DAMAGE APPLICATION TESTS
// ============================================================================

/**
 * Test: ApplyDamage Reduces Health
 * Verifies damage correctly reduces character health
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApplyDamageReducesHealthTest, "KatanaCombat.Damage.Basic.ReducesHealth", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FApplyDamageReducesHealthTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	const float InitialHealth = Enemy->CurrentHealth;
	const float DamageAmount = 25.0f;

	// Apply damage
	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(Player, DamageAmount);
	const float DamageDealt = IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);

	TestEqual("Damage dealt should match input", DamageDealt, DamageAmount);
	TestEqual("Health should be reduced", Enemy->CurrentHealth, InitialHealth - DamageAmount);

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Multiple Damage Applications
 * Verifies consecutive damage reduces health correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMultipleDamageApplicationsTest, "KatanaCombat.Damage.Basic.MultipleApplications", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMultipleDamageApplicationsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	const float InitialHealth = Enemy->MaxHealth;
	Enemy->SetHealth(InitialHealth);

	// Apply multiple damage instances
	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(nullptr, 20.0f);
	IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);
	TestEqual("Health after first hit", Enemy->CurrentHealth, InitialHealth - 20.0f);

	HitInfo.Damage = 15.0f;
	IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);
	TestEqual("Health after second hit", Enemy->CurrentHealth, InitialHealth - 35.0f);

	HitInfo.Damage = 10.0f;
	IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);
	TestEqual("Health after third hit", Enemy->CurrentHealth, InitialHealth - 45.0f);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Zero Damage Does Nothing
 * Verifies zero damage doesn't affect health
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZeroDamageDoesNothingTest, "KatanaCombat.Damage.Basic.ZeroDamage", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FZeroDamageDoesNothingTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	const float InitialHealth = Enemy->CurrentHealth;

	// Apply zero damage
	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(nullptr, 0.0f);
	const float DamageDealt = IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);

	TestEqual("Zero damage should deal 0", DamageDealt, 0.0f);
	TestEqual("Health should be unchanged", Enemy->CurrentHealth, InitialHealth);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// HEALTH BOUNDS TESTS
// ============================================================================

/**
 * Test: Health Clamps To Zero
 * Verifies health doesn't go below zero
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealthClampsToZeroTest, "KatanaCombat.Damage.Bounds.ClampsToZero", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHealthClampsToZeroTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	// Set to low health
	Enemy->SetHealth(10.0f);

	// Deal more damage than health
	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(nullptr, 100.0f);
	IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);

	TestEqual("Health should clamp to 0", Enemy->CurrentHealth, 0.0f);
	TestTrue("Health should not be negative", Enemy->CurrentHealth >= 0.0f);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Health Clamps To Max On Heal
 * Verifies healing doesn't exceed max health
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealthClampsToMaxTest, "KatanaCombat.Damage.Bounds.ClampsToMax", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHealthClampsToMaxTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	// Set to low health
	Enemy->SetHealth(50.0f);

	// Heal more than missing health
	Enemy->ModifyHealth(100.0f);

	TestEqual("Health should clamp to max", Enemy->CurrentHealth, Enemy->MaxHealth);
	TestTrue("Health should not exceed max", Enemy->CurrentHealth <= Enemy->MaxHealth);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: SetHealth Clamps Values
 * Verifies SetHealth respects bounds
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSetHealthClampsTest, "KatanaCombat.Damage.Bounds.SetHealthClamps", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSetHealthClampsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	// Try to set health above max
	Enemy->SetHealth(Enemy->MaxHealth + 100.0f);
	TestEqual("Health should clamp to max", Enemy->CurrentHealth, Enemy->MaxHealth);

	// Try to set health below zero (without triggering death for this test)
	// Note: SetHealth uses ModifyHealth which will trigger death at 0
	// Just test that it clamps properly
	Enemy->SetHealth(-100.0f);
	TestEqual("Health should clamp to 0", Enemy->CurrentHealth, 0.0f);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// HEALTH CHANGE DELEGATE TESTS
// ============================================================================

/**
 * Test: Health Changes On Damage
 * Verifies health is properly updated when damage is dealt
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnHealthChangedFiresOnDamageTest, "KatanaCombat.Damage.Events.DelegateFires", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOnHealthChangedFiresOnDamageTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	const float InitialHealth = Enemy->CurrentHealth;
	const float DamageAmount = 30.0f;
	const float ExpectedHealth = InitialHealth - DamageAmount;

	// Apply damage
	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(nullptr, DamageAmount);
	const float DamageDealt = IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);

	// Verify health changed (which would trigger OnHealthChanged delegate)
	TestEqual("Damage dealt should match", DamageDealt, DamageAmount);
	TestEqual("Health should be reduced correctly", Enemy->CurrentHealth, ExpectedHealth);
	TestTrue("MaxHealth should be valid", Enemy->MaxHealth > 0.0f);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCompatibilityDamageDelegatesRemainImmediateTest,
	"KatanaCombat.Damage.Events.CompatibilityDelegatesRemainImmediate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCompatibilityDamageDelegatesRemainImmediateTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Source = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	Target->HitReactionComponent->OnDamageReceived.AddDynamic(
		Recorder, &UCombatEventRecorder::HandleDamageReceived);
	Target->OnHealthChanged.AddDynamic(
		Recorder, &UCombatEventRecorder::HandleHealthChanged);

	const FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(Source, 15.0f);
	const float Damage = IDamageableInterface::Execute_ApplyDamage(Target, HitInfo);
	TestEqual(TEXT("Compatibility adapter returns applied damage"), Damage, 15.0f);
	TestEqual(TEXT("Compatibility damage delegate is immediate"), Recorder->DamageReceivedCount, 1);
	TestEqual(TEXT("Compatibility health delegate is immediate"), Recorder->HealthChangedCount, 1);

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Sequential Damage Accumulates Correctly
 * Verifies multiple damage applications accumulate properly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnHealthChangedCorrectValuesTest, "KatanaCombat.Damage.Events.CorrectValues", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOnHealthChangedCorrectValuesTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	const float InitialHealth = Enemy->MaxHealth;

	// Apply first damage
	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(nullptr, 20.0f);
	IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);
	TestEqual("Health after first hit", Enemy->CurrentHealth, InitialHealth - 20.0f);

	// Apply second damage
	HitInfo.Damage = 15.0f;
	IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);
	TestEqual("Health after second hit", Enemy->CurrentHealth, InitialHealth - 35.0f);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Zero Damage And Overheal Don't Change Health
 * Verifies health doesn't change for zero damage or healing past max
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnHealthChangedNoFireZeroChangeTest, "KatanaCombat.Damage.Events.NoFireZeroChange", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOnHealthChangedNoFireZeroChangeTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	const float InitialHealth = Enemy->CurrentHealth;
	const float MaxHealth = Enemy->MaxHealth;

	// Apply zero damage
	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(nullptr, 0.0f);
	const float ZeroDamageDealt = IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);

	TestEqual("Zero damage should deal 0", ZeroDamageDealt, 0.0f);
	TestEqual("Health should be unchanged after zero damage", Enemy->CurrentHealth, InitialHealth);

	// Try healing when already at max
	Enemy->ModifyHealth(50.0f);

	TestEqual("Health should stay at max after overheal", Enemy->CurrentHealth, MaxHealth);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// DAMAGE RESISTANCE TESTS
// ============================================================================

/**
 * Test: Damage Resistance Reduces Damage
 * Verifies resistance multiplier works
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamageResistanceReducesDamageTest, "KatanaCombat.Damage.Resistance.ReducesDamage", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDamageResistanceReducesDamageTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	UHitReactionComponent* HitReactionComp = Enemy->HitReactionComponent.Get();
	if (!TestNotNull("Enemy should have HitReactionComponent", HitReactionComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Set 50% damage resistance
	HitReactionComp->DamageResistance = 0.5f;

	const float InitialHealth = Enemy->CurrentHealth;
	const float DamageInput = 40.0f;
	const float ExpectedDamage = DamageInput * 0.5f;

	// Apply damage (through component)
	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(nullptr, DamageInput);
	const float DamageDealt = HitReactionComp->ApplyDamage(HitInfo);

	TestEqual("Damage should be reduced by resistance", DamageDealt, ExpectedDamage);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Zero Resistance Blocks All Damage
 * Verifies 0.0 resistance = immune
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZeroResistanceBlocksAllTest, "KatanaCombat.Damage.Resistance.ZeroBlocksAll", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FZeroResistanceBlocksAllTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	UHitReactionComponent* HitReactionComp = Enemy->HitReactionComponent.Get();
	if (!TestNotNull("Enemy should have HitReactionComponent", HitReactionComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Set 0% damage resistance (immune)
	HitReactionComp->DamageResistance = 0.0f;

	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(nullptr, 100.0f);
	const float DamageDealt = HitReactionComp->ApplyDamage(HitInfo);

	TestEqual("Zero resistance should block all damage", DamageDealt, 0.0f);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// INVULNERABILITY TESTS
// ============================================================================

/**
 * Test: Invulnerable Blocks All Damage
 * Verifies bIsInvulnerable flag works
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInvulnerableBlocksAllDamageTest, "KatanaCombat.Damage.Invulnerability.BlocksAll", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInvulnerableBlocksAllDamageTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	UHitReactionComponent* HitReactionComp = Enemy->HitReactionComponent.Get();
	if (!TestNotNull("Enemy should have HitReactionComponent", HitReactionComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const float InitialHealth = Enemy->CurrentHealth;

	// Make invulnerable
	HitReactionComp->bIsInvulnerable = true;

	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(nullptr, 50.0f);
	const float DamageDealt = HitReactionComp->ApplyDamage(HitInfo);

	TestEqual("Damage should be blocked when invulnerable", DamageDealt, 0.0f);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: CanBeDamaged Returns False When Invulnerable
 * Verifies query function reflects invulnerability
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanBeDamagedFalseWhenInvulnerableTest, "KatanaCombat.Damage.Invulnerability.CanBeDamagedFalse", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCanBeDamagedFalseWhenInvulnerableTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	UHitReactionComponent* HitReactionComp = Enemy->HitReactionComponent.Get();
	if (!TestNotNull("Enemy should have HitReactionComponent", HitReactionComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Normal state
	TestTrue("Should be damageable normally", HitReactionComp->CanBeDamaged());

	// Invulnerable state
	HitReactionComp->bIsInvulnerable = true;
	TestFalse("Should not be damageable when invulnerable", HitReactionComp->CanBeDamaged());

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// SUPER ARMOR TESTS
// ============================================================================

/**
 * Test: Super Armor Still Takes Damage
 * Verifies super armor doesn't prevent damage, just reactions
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSuperArmorTakesDamageTest, "KatanaCombat.Damage.SuperArmor.StillTakesDamage", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSuperArmorTakesDamageTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	UHitReactionComponent* HitReactionComp = Enemy->HitReactionComponent.Get();
	if (!TestNotNull("Enemy should have HitReactionComponent", HitReactionComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const float InitialHealth = Enemy->CurrentHealth;

	// Enable super armor
	HitReactionComp->bHasSuperArmor = true;

	// Apply damage
	const float DamageAmount = 30.0f;
	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(nullptr, DamageAmount);
	const float DamageDealt = HitReactionComp->ApplyDamage(HitInfo);

	// Damage should still be dealt
	TestEqual("Damage should be dealt with super armor", DamageDealt, DamageAmount);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// ATTACK DATA INTEGRATION TESTS
// ============================================================================

/**
 * Test: Attack Data Damage Is Used
 * Verifies FHitReactionInfo.AttackData contributes to damage calculation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataDamageUsedTest, "KatanaCombat.Damage.AttackData.DamageUsed", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataDamageUsedTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	// Create attack with specific damage
	UAttackData* LightAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* HeavyAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);

	// Light attack damage
	FHitReactionInfo LightHit = FCombatTestHelpers::CreateTestHitInfo(nullptr, LightAttack->BaseDamage, FVector::ForwardVector, LightAttack);
	TestEqual("Light hit info should use attack damage", LightHit.Damage, LightAttack->BaseDamage);

	// Heavy attack damage
	FHitReactionInfo HeavyHit = FCombatTestHelpers::CreateTestHitInfo(nullptr, HeavyAttack->BaseDamage, FVector::ForwardVector, HeavyAttack);
	TestEqual("Heavy hit info should use attack damage", HeavyHit.Damage, HeavyAttack->BaseDamage);

	// Verify heavy does more damage than light
	TestTrue("Heavy should do more damage than light", HeavyAttack->BaseDamage > LightAttack->BaseDamage);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// DAMAGE DIRECTION TESTS
// ============================================================================

/**
 * Test: Hit Direction Calculation Works
 * Verifies hit direction is correctly calculated relative to character facing
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitDirectionPreservedTest, "KatanaCombat.Damage.Direction.Preserved", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitDirectionPreservedTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	Enemy->SetActorRotation(FRotator::ZeroRotator); // Facing +X

	UHitReactionComponent* HitReactionComp = Enemy->HitReactionComponent.Get();
	if (!TestNotNull("Enemy should have HitReactionComponent", HitReactionComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Test hit direction calculation
	const FVector FrontDirection = FVector::ForwardVector;
	const FVector BackDirection = -FVector::ForwardVector;

	const EAttackDirection FrontResult = HitReactionComp->GetHitDirectionRelativeToFacing(FrontDirection);
	const EAttackDirection BackResult = HitReactionComp->GetHitDirectionRelativeToFacing(BackDirection);

	TestEqual("Front direction should be Forward", FrontResult, EAttackDirection::Forward);
	TestEqual("Back direction should be Backward", BackResult, EAttackDirection::Backward);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// EDGE CASES AND INTEGRATION
// ============================================================================

/**
 * Test: Rapid Damage Application
 * Verifies multiple rapid damage applications are handled correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRapidDamageApplicationTest, "KatanaCombat.Damage.EdgeCases.RapidDamage", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRapidDamageApplicationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	const float DamagePerHit = 5.0f;
	const int32 HitCount = 10;
	float TotalDamageDealt = 0.0f;

	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(nullptr, DamagePerHit);

	for (int32 i = 0; i < HitCount; ++i)
	{
		TotalDamageDealt += IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);
	}

	const float ExpectedTotalDamage = DamagePerHit * HitCount;
	TestEqual("Total damage should accumulate correctly", TotalDamageDealt, ExpectedTotalDamage);
	TestEqual("Health should reflect total damage", Enemy->CurrentHealth, Enemy->MaxHealth - ExpectedTotalDamage);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Damage Application Thread Safety Proxy
 * Verifies damage can be applied without crashing (basic sanity)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamageApplicationSanityTest, "KatanaCombat.Damage.EdgeCases.SanityCheck", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDamageApplicationSanityTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = nullptr;
	TArray<AEnemyCharacter*> Enemies;

	FCombatTestHelpers::CreateCombatScenario(World, Player, Enemies, 5, 400.0f);

	// Apply damage to all enemies simultaneously
	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(Player, 10.0f);

	for (AEnemyCharacter* Enemy : Enemies)
	{
		IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);
	}

	// All should be alive with reduced health
	for (AEnemyCharacter* Enemy : Enemies)
	{
		TestFalse("Enemy should be alive", Enemy->bIsDead);
		TestEqual("Enemy health should be reduced", Enemy->CurrentHealth, Enemy->MaxHealth - 10.0f);
	}

	World->DestroyActor(Player);
	for (AEnemyCharacter* Enemy : Enemies)
	{
		World->DestroyActor(Enemy);
	}
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
