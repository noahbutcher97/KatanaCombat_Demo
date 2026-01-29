// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Characters/BaseCombatCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/WeaponComponent.h"
#include "Core/HitReactionComponent.h"
#include "Core/TargetingComponent.h"
#include "Interfaces/DamageableInterface.h"
#include "Interfaces/TeamMemberInterface.h"
#include "Data/AttackData.h"

// ============================================================================
// FULL COMBAT FLOW TESTS
// ============================================================================

/**
 * Test: Complete Damage Flow
 * Verifies: Attack -> Hit -> Damage -> Health Reduction
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompleteDamageFlowTest, "KatanaCombat.Integration.Flow.CompleteDamage", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCompleteDamageFlowTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	const float InitialHealth = Enemy->CurrentHealth;

	// Simulate hit via damage interface (weapon hit detection is tick-based)
	const float DamageAmount = 25.0f;
	FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(Player, DamageAmount);
	const float DamageDealt = IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);

	// Verify flow by checking state changes (state changes imply delegates fired)
	TestEqual("Damage should be dealt", DamageDealt, DamageAmount);
	TestEqual("Health should be reduced", Enemy->CurrentHealth, InitialHealth - DamageAmount);
	TestFalse("Enemy should still be alive", Enemy->bIsDead);

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Complete Kill Flow
 * Verifies: Damage -> Death Flag -> Death Event -> Blocking Further Damage
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompleteKillFlowTest, "KatanaCombat.Integration.Flow.CompleteKill", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCompleteKillFlowTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	// Step 1: Verify alive state
	TestFalse("Step 1: Enemy should start alive", Enemy->bIsDead);
	TestTrue("Step 1: IsAlive should return true", IDamageableInterface::Execute_IsAlive(Enemy));

	// Step 2: Deal lethal damage
	const bool bKilled = FCombatTestHelpers::DealLethalDamage(Enemy, Player);

	// Step 3: Verify death state (state changes imply death event fired)
	TestTrue("Step 3: Kill should succeed", bKilled);
	TestTrue("Step 3: bIsDead should be true", Enemy->bIsDead);
	TestFalse("Step 3: IsAlive should return false", IDamageableInterface::Execute_IsAlive(Enemy));
	TestEqual("Step 3: Health should be 0", Enemy->CurrentHealth, 0.0f);

	// Step 4: Verify damage blocking
	FHitReactionInfo FollowupDamage = FCombatTestHelpers::CreateTestHitInfo(Player, 50.0f);
	const float DamageDealt = IDamageableInterface::Execute_ApplyDamage(Enemy, FollowupDamage);

	TestEqual("Step 4: Follow-up damage should be blocked", DamageDealt, 0.0f);
	TestEqual("Step 4: Health should remain at 0", Enemy->CurrentHealth, 0.0f);
	TestTrue("Step 4: Should still be dead", Enemy->bIsDead);

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// MULTI-ENEMY COMBAT TESTS
// ============================================================================

/**
 * Test: Sequential Enemy Kills
 * Verifies: Player can kill multiple enemies one after another
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSequentialEnemyKillsTest, "KatanaCombat.Integration.MultiEnemy.SequentialKills", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSequentialEnemyKillsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = nullptr;
	TArray<AEnemyCharacter*> Enemies;

	FCombatTestHelpers::CreateCombatScenario(World, Player, Enemies, 3, 300.0f);

	// Kill enemies one by one and verify state (state change indicates death event fired)
	FCombatTestHelpers::DealLethalDamage(Enemies[0], Player);
	TestTrue("First enemy should be dead", Enemies[0]->bIsDead);
	TestFalse("Second enemy still alive", Enemies[1]->bIsDead);
	TestFalse("Third enemy still alive", Enemies[2]->bIsDead);

	FCombatTestHelpers::DealLethalDamage(Enemies[1], Player);
	TestTrue("Second enemy should be dead", Enemies[1]->bIsDead);
	TestFalse("Third enemy still alive", Enemies[2]->bIsDead);

	FCombatTestHelpers::DealLethalDamage(Enemies[2], Player);
	TestTrue("Third enemy should be dead", Enemies[2]->bIsDead);

	// Verify all dead
	for (int32 i = 0; i < Enemies.Num(); ++i)
	{
		TestTrue(FString::Printf(TEXT("Enemy %d should be dead"), i), Enemies[i]->bIsDead);
		TestEqual(FString::Printf(TEXT("Enemy %d health should be 0"), i), Enemies[i]->CurrentHealth, 0.0f);
	}

	World->DestroyActor(Player);
	for (AEnemyCharacter* Enemy : Enemies)
	{
		World->DestroyActor(Enemy);
	}
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Partial Enemy Damage
 * Verifies: Some enemies damaged but not killed
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartialEnemyDamageTest, "KatanaCombat.Integration.MultiEnemy.PartialDamage", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPartialEnemyDamageTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = nullptr;
	TArray<AEnemyCharacter*> Enemies;

	FCombatTestHelpers::CreateCombatScenario(World, Player, Enemies, 3, 300.0f);

	// Deal partial damage to all
	for (AEnemyCharacter* Enemy : Enemies)
	{
		FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(Player, 30.0f);
		IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);
	}

	// All should be alive but damaged
	for (int32 i = 0; i < Enemies.Num(); ++i)
	{
		TestFalse(FString::Printf(TEXT("Enemy %d should be alive"), i), Enemies[i]->bIsDead);
		TestEqual(FString::Printf(TEXT("Enemy %d health reduced"), i), Enemies[i]->CurrentHealth, Enemies[i]->MaxHealth - 30.0f);
	}

	// Kill one
	FCombatTestHelpers::DealLethalDamage(Enemies[0], Player);

	// Verify states
	TestTrue("First enemy should be dead", Enemies[0]->bIsDead);
	TestFalse("Second enemy should still be alive", Enemies[1]->bIsDead);
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
// COMPONENT COORDINATION TESTS
// ============================================================================

/**
 * Test: All Combat Components Present
 * Verifies: Characters have all required components
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAllCombatComponentsPresentTest, "KatanaCombat.Integration.Components.AllPresent", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAllCombatComponentsPresentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();

	// Test player
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	TestNotNull("Player CombatComponent", Player->CombatComponent.Get());
	TestNotNull("Player WeaponComponent", Player->WeaponComponent.Get());
	TestNotNull("Player HitReactionComponent", Player->HitReactionComponent.Get());
	TestNotNull("Player TargetingComponent", Player->TargetingComponent.Get());

	// Test enemy
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	TestNotNull("Enemy CombatComponent", Enemy->CombatComponent.Get());
	TestNotNull("Enemy WeaponComponent", Enemy->WeaponComponent.Get());
	TestNotNull("Enemy HitReactionComponent", Enemy->HitReactionComponent.Get());
	TestNotNull("Enemy TargetingComponent", Enemy->TargetingComponent.Get());

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Interface Implementations
 * Verifies: Characters implement required interfaces
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInterfaceImplementationsTest, "KatanaCombat.Integration.Interfaces.Implemented", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInterfaceImplementationsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	// Test interface implementations
	TestTrue("Player implements IDamageableInterface", Player->Implements<UDamageableInterface>());
	TestTrue("Player implements ITeamMemberInterface", Player->Implements<UTeamMemberInterface>());

	TestTrue("Enemy implements IDamageableInterface", Enemy->Implements<UDamageableInterface>());
	TestTrue("Enemy implements ITeamMemberInterface", Enemy->Implements<UTeamMemberInterface>());

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEAM SYSTEM INTEGRATION TESTS
// ============================================================================

/**
 * Test: Team System Hostility
 * Verifies: Team hostility calculations work correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeamSystemHostilityTest, "KatanaCombat.Integration.Team.Hostility", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTeamSystemHostilityTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	// Set teams
	Player->TeamId = ETeamId::Player;
	Enemy->TeamId = ETeamId::Enemy;

	// Test hostility
	const bool bPlayerHostileToEnemy = ITeamMemberInterface::Execute_IsHostileTo(Player, Enemy);
	const bool bEnemyHostileToPlayer = ITeamMemberInterface::Execute_IsHostileTo(Enemy, Player);

	TestTrue("Player should be hostile to enemy", bPlayerHostileToEnemy);
	TestTrue("Enemy should be hostile to player", bEnemyHostileToPlayer);

	// Test friendliness
	const bool bPlayerFriendlyToEnemy = ITeamMemberInterface::Execute_IsFriendlyTo(Player, Enemy);
	const bool bEnemyFriendlyToPlayer = ITeamMemberInterface::Execute_IsFriendlyTo(Enemy, Player);

	TestFalse("Player should not be friendly to enemy", bPlayerFriendlyToEnemy);
	TestFalse("Enemy should not be friendly to player", bEnemyFriendlyToPlayer);

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Same Team Friendliness
 * Verifies: Characters on same team are friendly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSameTeamFriendlyTest, "KatanaCombat.Integration.Team.SameTeamFriendly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSameTeamFriendlyTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();

	AEnemyCharacter* Enemy1 = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector::ZeroVector);
	AEnemyCharacter* Enemy2 = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	// Both on enemy team
	Enemy1->TeamId = ETeamId::Enemy;
	Enemy2->TeamId = ETeamId::Enemy;

	// Test friendliness
	const bool bEnemy1FriendlyToEnemy2 = ITeamMemberInterface::Execute_IsFriendlyTo(Enemy1, Enemy2);
	const bool bEnemy2FriendlyToEnemy1 = ITeamMemberInterface::Execute_IsFriendlyTo(Enemy2, Enemy1);

	TestTrue("Enemy1 should be friendly to Enemy2", bEnemy1FriendlyToEnemy2);
	TestTrue("Enemy2 should be friendly to Enemy1", bEnemy2FriendlyToEnemy1);

	// Test hostility
	const bool bEnemy1HostileToEnemy2 = ITeamMemberInterface::Execute_IsHostileTo(Enemy1, Enemy2);

	TestFalse("Enemy1 should not be hostile to Enemy2", bEnemy1HostileToEnemy2);

	World->DestroyActor(Enemy1);
	World->DestroyActor(Enemy2);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// EDGE CASE INTEGRATION TESTS
// ============================================================================

/**
 * Test: Null Actor Handling
 * Verifies: System handles null actors gracefully
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNullActorHandlingTest, "KatanaCombat.Integration.EdgeCases.NullActors", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FNullActorHandlingTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	// Test hostility with null
	const bool bPlayerHostileToNull = ITeamMemberInterface::Execute_IsHostileTo(Player, nullptr);
	TestFalse("Should not be hostile to null", bPlayerHostileToNull);

	// Test friendliness with null
	const bool bPlayerFriendlyToNull = ITeamMemberInterface::Execute_IsFriendlyTo(Player, nullptr);
	TestFalse("Should not be friendly to null", bPlayerFriendlyToNull);

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Rapid State Changes
 * Verifies: System handles rapid state changes
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRapidStateChangesTest, "KatanaCombat.Integration.EdgeCases.RapidStateChanges", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FRapidStateChangesTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();

	// Rapid enable/disable cycles
	for (int32 i = 0; i < 100; ++i)
	{
		WeaponComp->EnableHitDetection();
		WeaponComp->DisableHitDetection();
	}

	TestFalse("Should end disabled after rapid cycling", WeaponComp->IsHitDetectionEnabled());

	// Rapid damage applications
	for (int32 i = 0; i < 10; ++i)
	{
		FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(Player, 5.0f);
		IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);
	}

	TestEqual("Health should reflect all 10 damage instances", Enemy->CurrentHealth, Enemy->MaxHealth - 50.0f);

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Combat After Death Reset
 * Verifies: Dead enemy properly blocks all combat
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAfterDeathTest, "KatanaCombat.Integration.EdgeCases.CombatAfterDeath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatAfterDeathTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	// Kill enemy
	FCombatTestHelpers::DealLethalDamage(Enemy, Player);
	TestTrue("Enemy should be dead", Enemy->bIsDead);
	TestEqual("Health should be 0", Enemy->CurrentHealth, 0.0f);

	// Track total damage blocked
	float TotalDamageBlocked = 0.0f;

	// Attempt many damage applications
	for (int32 i = 0; i < 50; ++i)
	{
		FHitReactionInfo HitInfo = FCombatTestHelpers::CreateTestHitInfo(Player, 25.0f);
		const float DamageDealt = IDamageableInterface::Execute_ApplyDamage(Enemy, HitInfo);
		if (DamageDealt == 0.0f)
		{
			TotalDamageBlocked += 25.0f;
		}
	}

	// Verify all damage was blocked
	TestEqual("All 50 hits should be blocked", TotalDamageBlocked, 1250.0f);
	TestEqual("Health still at 0", Enemy->CurrentHealth, 0.0f);
	TestTrue("Still dead", Enemy->bIsDead);

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
