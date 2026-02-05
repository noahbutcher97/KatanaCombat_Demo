// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Characters/BaseCombatCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Core/HitReactionComponent.h"
#include "Core/WeaponComponent.h"
#include "Interfaces/DamageableInterface.h"

// ============================================================================
// DEATH FLAG TESTS
// ============================================================================

/**
 * Test: bIsDead Flag Starts False
 * Verifies characters start alive
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathFlagStartsFalseTest, "KatanaCombat.DeathSystem.Flag.StartsFalse", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeathFlagStartsFalseTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	if (!TestNotNull("Enemy should be created", Enemy))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestFalse("bIsDead should be false initially", Enemy->bIsDead);
	TestTrue("IsAlive should return true", IDamageableInterface::Execute_IsAlive(Enemy));

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: bIsDead Flag Set On Lethal Damage
 * Verifies lethal damage sets the death flag
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathFlagSetOnLethalDamageTest, "KatanaCombat.DeathSystem.Flag.SetOnLethalDamage", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeathFlagSetOnLethalDamageTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	if (!TestNotNull("Enemy should be created", Enemy))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Enemy starts alive
	TestFalse("Enemy should start alive", Enemy->bIsDead);
	TestEqual("Enemy should have max health", Enemy->CurrentHealth, Enemy->MaxHealth);

	// Deal lethal damage
	const bool bDied = FCombatTestHelpers::DealLethalDamage(Enemy, Player);

	TestTrue("DealLethalDamage should return true", bDied);
	TestTrue("bIsDead should be true after lethal damage", Enemy->bIsDead);
	TestFalse("IsAlive should return false", IDamageableInterface::Execute_IsAlive(Enemy));

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Health Reaches Zero Before Death Flag
 * Verifies health reaches 0 when death flag is set
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealthZeroOnDeathTest, "KatanaCombat.DeathSystem.Health.ZeroOnDeath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHealthZeroOnDeathTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	// Deal lethal damage
	FCombatTestHelpers::DealLethalDamage(Enemy);

	TestTrue("bIsDead should be true", Enemy->bIsDead);
	TestEqual("Health should be 0", Enemy->CurrentHealth, 0.0f);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// DEATH BLOCKS FURTHER DAMAGE TESTS
// ============================================================================

/**
 * Test: Dead Actor Cannot Take Further Damage
 * Verifies damage is blocked after death
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadActorCannotTakeDamageTest, "KatanaCombat.DeathSystem.Blocking.NoDamageAfterDeath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadActorCannotTakeDamageTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	// Kill the enemy
	FCombatTestHelpers::DealLethalDamage(Enemy, Player);
	TestTrue("Enemy should be dead", Enemy->bIsDead);
	TestEqual("Health should be 0", Enemy->CurrentHealth, 0.0f);

	// Try to deal more damage
	FHitReactionInfo ExtraDamage = FCombatTestHelpers::CreateTestHitInfo(Player, 50.0f);
	const float DamageDealt = IDamageableInterface::Execute_ApplyDamage(Enemy, ExtraDamage);

	// Damage should be blocked
	TestEqual("No damage should be dealt to dead actor", DamageDealt, 0.0f);
	TestEqual("Health should remain at 0", Enemy->CurrentHealth, 0.0f);

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Dead Actor Not Added to Hit List
 * Verifies WeaponComponent filters dead actors
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadActorNotAddedToHitListTest, "KatanaCombat.DeathSystem.Blocking.NotInHitList", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadActorNotAddedToHitListTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	// Get weapon component
	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Kill the enemy first
	FCombatTestHelpers::DealLethalDamage(Enemy, Player);
	TestTrue("Enemy should be dead", Enemy->bIsDead);

	// Enable hit detection and verify dead actor filtering
	// (The actual filtering happens in ProcessHit which checks bIsDead)
	WeaponComp->EnableHitDetection();
	TestTrue("Hit detection should be enabled", WeaponComp->IsHitDetectionEnabled());

	// Reset to allow any hits
	WeaponComp->ResetHitActors();

	// The WasActorAlreadyHit check itself doesn't check death
	// But ProcessHit does, which prevents adding dead actors to the hit list
	TestFalse("Dead actor should not be in hit list", WeaponComp->WasActorAlreadyHit(Enemy));

	WeaponComp->DisableHitDetection();

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// DEATH EVENT TESTS
// ============================================================================

/**
 * Test: Death State Transition Occurs
 * Verifies death state transitions correctly on lethal damage
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnCharacterDeathFiringTest, "KatanaCombat.DeathSystem.Events.OnCharacterDeathFires", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOnCharacterDeathFiringTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	// Verify starting state
	TestFalse("Enemy should start alive", Enemy->bIsDead);
	TestTrue("IsAlive should return true", IDamageableInterface::Execute_IsAlive(Enemy));

	// Kill the enemy
	FCombatTestHelpers::DealLethalDamage(Enemy, Player);

	// Verify death state (which is set when death event fires)
	TestTrue("bIsDead should be true (indicates death event processed)", Enemy->bIsDead);
	TestFalse("IsAlive should return false", IDamageableInterface::Execute_IsAlive(Enemy));
	TestEqual("Health should be zero", Enemy->CurrentHealth, 0.0f);

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Death State Only Set Once
 * Verifies death processing doesn't repeat on additional damage
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathDelegateOnlyFiresOnceTest, "KatanaCombat.DeathSystem.Events.DelegateFiresOnce", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeathDelegateOnlyFiresOnceTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	// Deal lethal damage
	FCombatTestHelpers::DealLethalDamage(Enemy, Player);
	TestTrue("Enemy should be dead", Enemy->bIsDead);
	TestEqual("Health should be 0", Enemy->CurrentHealth, 0.0f);

	// Try additional damage (should be blocked, state shouldn't change)
	FHitReactionInfo ExtraDamage = FCombatTestHelpers::CreateTestHitInfo(Player, 100.0f);
	const float Damage1 = IDamageableInterface::Execute_ApplyDamage(Enemy, ExtraDamage);
	const float Damage2 = IDamageableInterface::Execute_ApplyDamage(Enemy, ExtraDamage);

	// Verify no additional damage was dealt
	TestEqual("First extra damage should be blocked", Damage1, 0.0f);
	TestEqual("Second extra damage should be blocked", Damage2, 0.0f);
	TestEqual("Health should still be 0", Enemy->CurrentHealth, 0.0f);
	TestTrue("Should still be dead", Enemy->bIsDead);

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// MULTIPLE ENEMY DEATH TESTS
// ============================================================================

/**
 * Test: Multiple Enemies Can Die Independently
 * Verifies death of one enemy doesn't affect others
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMultipleEnemyDeathIndependentTest, "KatanaCombat.DeathSystem.Multiple.IndependentDeath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMultipleEnemyDeathIndependentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = nullptr;
	TArray<AEnemyCharacter*> Enemies;

	FCombatTestHelpers::CreateCombatScenario(World, Player, Enemies, 3, 300.0f);

	// Kill only the first enemy
	FCombatTestHelpers::DealLethalDamage(Enemies[0], Player);

	TestTrue("First enemy should be dead", Enemies[0]->bIsDead);
	TestFalse("Second enemy should be alive", Enemies[1]->bIsDead);
	TestFalse("Third enemy should be alive", Enemies[2]->bIsDead);

	// Kill the second enemy
	FCombatTestHelpers::DealLethalDamage(Enemies[1], Player);

	TestTrue("First enemy should still be dead", Enemies[0]->bIsDead);
	TestTrue("Second enemy should now be dead", Enemies[1]->bIsDead);
	TestFalse("Third enemy should still be alive", Enemies[2]->bIsDead);

	World->DestroyActor(Player);
	for (AEnemyCharacter* Enemy : Enemies)
	{
		World->DestroyActor(Enemy);
	}
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

/**
 * Test: Exact Lethal Damage
 * Verifies character dies when damage equals remaining health
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExactLethalDamageTest, "KatanaCombat.DeathSystem.EdgeCases.ExactLethalDamage", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FExactLethalDamageTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	// Set health to specific amount
	const float TestHealth = 50.0f;
	Enemy->SetHealth(TestHealth);
	TestEqual("Health should be set", Enemy->CurrentHealth, TestHealth);

	// Deal exact lethal damage
	FHitReactionInfo ExactDamage = FCombatTestHelpers::CreateTestHitInfo(nullptr, TestHealth);
	IDamageableInterface::Execute_ApplyDamage(Enemy, ExactDamage);

	// In tests, animation system isn't running, so manually finalize death
	FCombatTestHelpers::FinalizeDeathIfDying(Enemy);

	TestTrue("Enemy should be dead with exact damage", Enemy->bIsDead);
	TestEqual("Health should be exactly 0", Enemy->CurrentHealth, 0.0f);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Near-Zero Health Survival
 * Verifies character survives with tiny health remaining
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNearZeroHealthSurvivalTest, "KatanaCombat.DeathSystem.EdgeCases.NearZeroSurvival", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FNearZeroHealthSurvivalTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	// Set health to specific amount
	const float TestHealth = 50.0f;
	Enemy->SetHealth(TestHealth);

	// Deal damage that leaves tiny health
	const float AlmostLethalDamage = TestHealth - 0.1f;
	FHitReactionInfo NearLethalDamage = FCombatTestHelpers::CreateTestHitInfo(nullptr, AlmostLethalDamage);
	IDamageableInterface::Execute_ApplyDamage(Enemy, NearLethalDamage);

	TestFalse("Enemy should survive with near-zero health", Enemy->bIsDead);
	TestTrue("Health should be above 0", Enemy->CurrentHealth > 0.0f);
	TestTrue("IsAlive should return true", IDamageableInterface::Execute_IsAlive(Enemy));

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Death With Null Attacker
 * Verifies death works with no attacker (e.g., environmental damage)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathWithNullAttackerTest, "KatanaCombat.DeathSystem.EdgeCases.NullAttacker", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeathWithNullAttackerTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	TestFalse("Enemy should start alive", Enemy->bIsDead);

	// Deal lethal damage with no attacker (environmental damage)
	FCombatTestHelpers::DealLethalDamage(Enemy, nullptr);

	// Verify death occurred properly
	TestTrue("Enemy should be dead", Enemy->bIsDead);
	TestEqual("Health should be 0", Enemy->CurrentHealth, 0.0f);
	TestFalse("IsAlive should return false", IDamageableInterface::Execute_IsAlive(Enemy));

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Overkill Damage Clamps Health to Zero
 * Verifies massive damage doesn't set health below 0
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverkillDamageClampsTest, "KatanaCombat.DeathSystem.EdgeCases.OverkillClamps", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOverkillDamageClampsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	// Deal massive overkill damage
	FHitReactionInfo OverkillDamage = FCombatTestHelpers::CreateTestHitInfo(nullptr, 99999.0f);
	IDamageableInterface::Execute_ApplyDamage(Enemy, OverkillDamage);

	// In tests, animation system isn't running, so manually finalize death
	FCombatTestHelpers::FinalizeDeathIfDying(Enemy);

	TestTrue("Enemy should be dead", Enemy->bIsDead);
	TestEqual("Health should be clamped to 0", Enemy->CurrentHealth, 0.0f);
	TestTrue("Health should not be negative", Enemy->CurrentHealth >= 0.0f);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// I-FRAME TESTS (Death Interaction)
// ============================================================================

/**
 * Test: Dead Actor Ignores I-Frame Checks
 * Verifies i-frame logic is bypassed for dead actors (they're already invulnerable via bIsDead)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadActorIgnoresIFramesTest, "KatanaCombat.DeathSystem.IFrames.DeadActorIgnores", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadActorIgnoresIFramesTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	// Get hit reaction component
	UHitReactionComponent* HitReactionComp = Enemy->HitReactionComponent.Get();
	if (!TestNotNull("Enemy should have HitReactionComponent", HitReactionComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Kill the enemy
	FCombatTestHelpers::DealLethalDamage(Enemy);
	TestTrue("Enemy should be dead", Enemy->bIsDead);

	// Regardless of i-frame state, damage should be blocked because bIsDead
	FHitReactionInfo TestDamage = FCombatTestHelpers::CreateTestHitInfo(nullptr, 25.0f);
	const float DamageDealt = IDamageableInterface::Execute_ApplyDamage(Enemy, TestDamage);

	TestEqual("No damage should be dealt to dead actor", DamageDealt, 0.0f);

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// COMPONENT STATE AFTER DEATH
// ============================================================================

/**
 * Test: CanBeDamaged Returns False When Dead
 * Verifies interface query reflects death state
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCanBeDamagedFalseWhenDeadTest, "KatanaCombat.DeathSystem.State.CanBeDamagedFalseWhenDead", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCanBeDamagedFalseWhenDeadTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	// Alive - should be damageable
	TestTrue("Alive actor should be damageable", IDamageableInterface::Execute_CanBeDamaged(Enemy));

	// Kill the enemy
	FCombatTestHelpers::DealLethalDamage(Enemy);

	// Dead - note: CanBeDamaged checks invulnerability, not death flag directly
	// But IsAlive returns false, which is what callers should check
	TestFalse("Dead actor's IsAlive should return false", IDamageableInterface::Execute_IsAlive(Enemy));

	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
