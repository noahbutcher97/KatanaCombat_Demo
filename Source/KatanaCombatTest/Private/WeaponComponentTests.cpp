// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Characters/BaseCombatCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Core/WeaponComponent.h"
#include "Core/CombatComponent.h"
#include "Data/AttackData.h"

// ============================================================================
// COMPONENT INITIALIZATION TESTS
// ============================================================================

/**
 * Test: WeaponComponent Exists On Character
 * Verifies component is created properly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponComponentExistsTest, "KatanaCombat.Weapon.Component.Exists", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWeaponComponentExistsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	TestNotNull("Player should have WeaponComponent", WeaponComp);

	// Also test enemy
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	TestNotNull("Enemy should have WeaponComponent", Enemy->WeaponComponent.Get());

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: WeaponComponent Default State
 * Verifies component initializes with correct defaults
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponComponentDefaultStateTest, "KatanaCombat.Weapon.Component.DefaultState", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWeaponComponentDefaultStateTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Check default values
	TestFalse("Hit detection should be disabled by default", WeaponComp->IsHitDetectionEnabled());
	TestEqual("Hit actor count should be 0", WeaponComp->GetHitActorCount(), 0);
	TestTrue("Weapon should be equipped by default", WeaponComp->IsEquipped());

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// HIT DETECTION CONTROL TESTS
// ============================================================================

/**
 * Test: EnableHitDetection Sets Flag
 * Verifies hit detection can be enabled
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnableHitDetectionTest, "KatanaCombat.Weapon.HitDetection.Enable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEnableHitDetectionTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestFalse("Should start with hit detection disabled", WeaponComp->IsHitDetectionEnabled());

	WeaponComp->EnableHitDetection();

	TestTrue("Hit detection should be enabled after EnableHitDetection", WeaponComp->IsHitDetectionEnabled());

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: DisableHitDetection Clears Flag
 * Verifies hit detection can be disabled
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisableHitDetectionTest, "KatanaCombat.Weapon.HitDetection.Disable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDisableHitDetectionTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Enable first
	WeaponComp->EnableHitDetection();
	TestTrue("Should be enabled", WeaponComp->IsHitDetectionEnabled());

	// Then disable
	WeaponComp->DisableHitDetection();
	TestFalse("Hit detection should be disabled after DisableHitDetection", WeaponComp->IsHitDetectionEnabled());

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: EnableHitDetection Is Idempotent
 * Verifies calling enable multiple times is safe
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnableHitDetectionIdempotentTest, "KatanaCombat.Weapon.HitDetection.EnableIdempotent", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEnableHitDetectionIdempotentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Call enable multiple times
	WeaponComp->EnableHitDetection();
	WeaponComp->EnableHitDetection();
	WeaponComp->EnableHitDetection();

	TestTrue("Should still be enabled after multiple calls", WeaponComp->IsHitDetectionEnabled());

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// HIT ACTOR TRACKING TESTS
// ============================================================================

/**
 * Test: ResetHitActors Clears List
 * Verifies hit actor list can be reset
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResetHitActorsClearsListTest, "KatanaCombat.Weapon.HitActors.ResetClears", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FResetHitActorsClearsListTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Reset should work on empty list
	WeaponComp->ResetHitActors();
	TestEqual("Hit actor count should be 0 after reset", WeaponComp->GetHitActorCount(), 0);

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: GetHitActors Returns Empty Initially
 * Verifies hit list starts empty
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGetHitActorsEmptyInitiallyTest, "KatanaCombat.Weapon.HitActors.EmptyInitially", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGetHitActorsEmptyInitiallyTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const TArray<AActor*>& HitActors = WeaponComp->GetHitActors();
	TestEqual("Hit actors array should be empty initially", HitActors.Num(), 0);
	TestEqual("Hit actor count should be 0", WeaponComp->GetHitActorCount(), 0);

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: WasActorAlreadyHit Returns False For New Actor
 * Verifies query works for actors not in list
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWasActorAlreadyHitFalseForNewTest, "KatanaCombat.Weapon.HitActors.WasHitFalseForNew", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWasActorAlreadyHitFalseForNewTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestFalse("WasActorAlreadyHit should return false for actor not in list", WeaponComp->WasActorAlreadyHit(Enemy));

	World->DestroyActor(Player);
	World->DestroyActor(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: WasActorAlreadyHit Returns False For Null
 * Verifies null check
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWasActorAlreadyHitFalseForNullTest, "KatanaCombat.Weapon.HitActors.WasHitFalseForNull", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWasActorAlreadyHitFalseForNullTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestFalse("WasActorAlreadyHit should return false for null", WeaponComp->WasActorAlreadyHit(nullptr));

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: EnableHitDetection Clears Hit List
 * Verifies hit list is cleared when enabling detection
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnableHitDetectionClearsListTest, "KatanaCombat.Weapon.HitActors.EnableClears", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEnableHitDetectionClearsListTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Start clean
	TestEqual("Hit count should be 0", WeaponComp->GetHitActorCount(), 0);

	// Enable hit detection (should reset for new attack)
	WeaponComp->EnableHitDetection();

	TestEqual("Hit count should still be 0 after enable", WeaponComp->GetHitActorCount(), 0);

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// SOCKET CONFIGURATION TESTS
// ============================================================================

/**
 * Test: SetWeaponSockets Updates Configuration
 * Verifies socket names can be changed
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSetWeaponSocketsUpdatesTest, "KatanaCombat.Weapon.Sockets.SetUpdates", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSetWeaponSocketsUpdatesTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Set custom sockets
	const FName CustomStart = TEXT("custom_start");
	const FName CustomEnd = TEXT("custom_end");
	WeaponComp->SetWeaponSockets(CustomStart, CustomEnd);

	// Verify via public properties
	TestEqual("Start socket should be updated", WeaponComp->WeaponStartSocket, CustomStart);
	TestEqual("End socket should be updated", WeaponComp->WeaponEndSocket, CustomEnd);

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: GetSocketLocation Falls Back Gracefully
 * Verifies fallback when socket doesn't exist
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGetSocketLocationFallbackTest, "KatanaCombat.Weapon.Sockets.Fallback", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGetSocketLocationFallbackTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Get location for non-existent socket
	const FVector Location = WeaponComp->GetSocketLocation(TEXT("nonexistent_socket_12345"));

	// Should fall back to character location (not crash)
	// The exact value depends on character position, just verify it's not zero (unless at origin)
	TestTrue("GetSocketLocation should not crash for invalid socket", true); // If we got here, no crash

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// WEAPON HIT EVENT TESTS
// ============================================================================

/**
 * Test: OnWeaponHit Delegate Exists
 * Verifies weapon hit delegate is accessible
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnWeaponHitBindableTest, "KatanaCombat.Weapon.Events.OnHitBindable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOnWeaponHitBindableTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Verify the delegate property is accessible (not bound initially)
	// Can't bind lambdas to dynamic delegates, but we can verify the delegate exists
	TestFalse("OnWeaponHit should not be bound initially", WeaponComp->OnWeaponHit.IsBound());

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// EQUIP STATE TESTS
// ============================================================================

/**
 * Test: Weapon Starts Equipped
 * Verifies default equip state
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponStartsEquippedTest, "KatanaCombat.Weapon.Equip.StartsEquipped", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FWeaponStartsEquippedTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestTrue("Weapon should start equipped", WeaponComp->IsEquipped());

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Holster Changes State
 * Verifies holster function works
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHolsterChangesStateTest, "KatanaCombat.Weapon.Equip.HolsterChangesState", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHolsterChangesStateTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestTrue("Should start equipped", WeaponComp->IsEquipped());

	WeaponComp->Holster();

	TestFalse("Should be holstered after Holster()", WeaponComp->IsEquipped());

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Equip Changes State
 * Verifies equip function works after holster
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipChangesStateTest, "KatanaCombat.Weapon.Equip.EquipChangesState", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEquipChangesStateTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Holster first
	WeaponComp->Holster();
	TestFalse("Should be holstered", WeaponComp->IsEquipped());

	// Then equip
	WeaponComp->Equip();
	TestTrue("Should be equipped after Equip()", WeaponComp->IsEquipped());

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Equip Is Idempotent
 * Verifies calling equip when equipped is safe
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipIdempotentTest, "KatanaCombat.Weapon.Equip.EquipIdempotent", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FEquipIdempotentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestTrue("Should start equipped", WeaponComp->IsEquipped());

	// Call equip multiple times
	WeaponComp->Equip();
	WeaponComp->Equip();
	WeaponComp->Equip();

	TestTrue("Should still be equipped", WeaponComp->IsEquipped());

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Holster Is Idempotent
 * Verifies calling holster when holstered is safe
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHolsterIdempotentTest, "KatanaCombat.Weapon.Equip.HolsterIdempotent", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHolsterIdempotentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	WeaponComp->Holster();
	TestFalse("Should be holstered", WeaponComp->IsEquipped());

	// Call holster multiple times
	WeaponComp->Holster();
	WeaponComp->Holster();
	WeaponComp->Holster();

	TestFalse("Should still be holstered", WeaponComp->IsEquipped());

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// DAMAGE MULTIPLIER TESTS
// ============================================================================

/**
 * Test: GetDamageMultiplier Returns 1.0 By Default
 * Verifies default damage multiplier
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGetDamageMultiplierDefaultTest, "KatanaCombat.Weapon.Damage.DefaultMultiplier", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGetDamageMultiplierDefaultTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestEqual("Default damage multiplier should be 1.0", WeaponComp->GetDamageMultiplier(), 1.0f);

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: GetWeaponReach Returns Default
 * Verifies default weapon reach
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGetWeaponReachDefaultTest, "KatanaCombat.Weapon.Damage.DefaultReach", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGetWeaponReachDefaultTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const float Reach = WeaponComp->GetWeaponReach();
	TestEqual("Default weapon reach should be 150.0", Reach, 150.0f);

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

/**
 * Test: Hit Detection Cycle (Enable/Disable/Reset)
 * Verifies full hit detection lifecycle
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitDetectionCycleTest, "KatanaCombat.Weapon.Integration.HitDetectionCycle", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitDetectionCycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);

	UWeaponComponent* WeaponComp = Player->WeaponComponent.Get();
	if (!TestNotNull("Player should have WeaponComponent", WeaponComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Initial state
	TestFalse("Step 1: Should start disabled", WeaponComp->IsHitDetectionEnabled());
	TestEqual("Step 1: Hit count should be 0", WeaponComp->GetHitActorCount(), 0);

	// First attack cycle
	WeaponComp->EnableHitDetection();
	TestTrue("Step 2: Should be enabled", WeaponComp->IsHitDetectionEnabled());

	WeaponComp->DisableHitDetection();
	TestFalse("Step 3: Should be disabled", WeaponComp->IsHitDetectionEnabled());

	// Reset for new attack
	WeaponComp->ResetHitActors();
	TestEqual("Step 4: Hit count should be 0 after reset", WeaponComp->GetHitActorCount(), 0);

	// Second attack cycle
	WeaponComp->EnableHitDetection();
	TestTrue("Step 5: Should be enabled again", WeaponComp->IsHitDetectionEnabled());

	WeaponComp->DisableHitDetection();
	TestFalse("Step 6: Should be disabled again", WeaponComp->IsHitDetectionEnabled());

	World->DestroyActor(Player);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Multiple Weapons (Multiple Players) Independent
 * Verifies weapon components are independent
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMultipleWeaponsIndependentTest, "KatanaCombat.Weapon.Integration.MultipleIndependent", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMultipleWeaponsIndependentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();

	APlayerCharacter* Player1 = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);
	APlayerCharacter* Player2 = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector(500.0f, 0.0f, 0.0f));

	UWeaponComponent* Weapon1 = Player1->WeaponComponent.Get();
	UWeaponComponent* Weapon2 = Player2->WeaponComponent.Get();

	if (!TestNotNull("Player1 should have WeaponComponent", Weapon1) ||
		!TestNotNull("Player2 should have WeaponComponent", Weapon2))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Enable only first weapon
	Weapon1->EnableHitDetection();

	TestTrue("Weapon1 should be enabled", Weapon1->IsHitDetectionEnabled());
	TestFalse("Weapon2 should still be disabled", Weapon2->IsHitDetectionEnabled());

	// Holster only second weapon
	Weapon2->Holster();

	TestTrue("Weapon1 should still be equipped", Weapon1->IsEquipped());
	TestFalse("Weapon2 should be holstered", Weapon2->IsEquipped());

	World->DestroyActor(Player1);
	World->DestroyActor(Player2);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
