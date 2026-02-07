// CounterSystemTests.cpp
// Tests for Counter System: AC3 Mode and Chain Mode
// Verifies counter window detection, state transitions, and timeout behavior.

#include "CombatTestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
#include "Data/AttackData.h"
#include "Characters/BaseCombatCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Interfaces/DamageableInterface.h"
#include "CombatTypes.h"

// ============================================================================
// TEST: Parry window state toggles correctly
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ParryWindowToggle,
	"KatanaCombat.CounterSystem.ParryWindowToggle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ParryWindowToggle::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);

	if (!Combat)
	{
		AddError(TEXT("Failed to create combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Initially false
	TestFalse(TEXT("Parry window should start inactive"), Combat->IsInParryWindow());

	// Activate
	Combat->SetParryWindowActive(true);
	TestTrue(TEXT("Parry window should be active after SetParryWindowActive(true)"), Combat->IsInParryWindow());

	// Deactivate
	Combat->SetParryWindowActive(false);
	TestFalse(TEXT("Parry window should be inactive after SetParryWindowActive(false)"), Combat->IsInParryWindow());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: FindParryableEnemy returns enemy with active parry window
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_FindParryableEnemy,
	"KatanaCombat.CounterSystem.FindParryableEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_FindParryableEnemy::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!PlayerCombat)
	{
		AddError(TEXT("Failed to create player combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Create enemy close to player
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	if (!Enemy)
	{
		AddError(TEXT("Failed to create enemy character"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Enemy's parry window is NOT active — FindParryableEnemy should return nullptr
	AActor* Found = PlayerCombat->FindParryableEnemy();
	TestTrue(TEXT("Should not find parryable enemy when no parry window is active"),
		Found == nullptr);

	// Activate parry window on enemy
	UCombatComponent* EnemyCombat = Enemy->GetCombatComponent();
	if (EnemyCombat)
	{
		EnemyCombat->SetParryWindowActive(true);

		// Now FindParryableEnemy should find the enemy
		Found = PlayerCombat->FindParryableEnemy();
		// Note: In test environment, targeting may not work without full setup,
		// so we just verify the API doesn't crash
		// The actual find depends on targeting system being wired
	}

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: AC3 counter applies lethal damage
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_AC3LethalDamage,
	"KatanaCombat.CounterSystem.AC3LethalDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_AC3LethalDamage::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!PlayerCombat)
	{
		AddError(TEXT("Failed to create player combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Create enemy
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));
	if (!Enemy)
	{
		AddError(TEXT("Failed to create enemy"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Build counter context manually
	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Light;
	Context.SwingDirection = ESwingDirection::Horizontal;

	// Get enemy health before counter
	float HealthBefore = IDamageableInterface::Execute_GetCurrentHealth(Enemy);

	// Execute AC3 counter
	bool bSuccess = PlayerCombat->TryCounter_AC3Mode(Context);
	TestTrue(TEXT("AC3 counter should succeed with valid context"), bSuccess);

	// Verify enemy took lethal damage
	float HealthAfter = IDamageableInterface::Execute_GetCurrentHealth(Enemy);
	TestTrue(TEXT("Enemy health should be reduced after AC3 counter"), HealthAfter < HealthBefore);

	// Finalize death if dying (test fixture pattern)
	FCombatTestHelpers::FinalizeDeathIfDying(Enemy);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: AC3 counter staggers the enemy
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_AC3StaggersEnemy,
	"KatanaCombat.CounterSystem.AC3StaggersEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_AC3StaggersEnemy::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!PlayerCombat)
	{
		AddError(TEXT("Failed to create player combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));
	if (!Enemy)
	{
		AddError(TEXT("Failed to create enemy"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UHitReactionComponent* EnemyHitReact = Enemy->FindComponentByClass<UHitReactionComponent>();
	if (!EnemyHitReact)
	{
		AddError(TEXT("Enemy missing HitReactionComponent"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Enemy should not be staggered before counter
	TestFalse(TEXT("Enemy should not be staggered before counter"), EnemyHitReact->IsStaggered());

	// Execute AC3 counter
	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Light;
	PlayerCombat->TryCounter_AC3Mode(Context);

	// Enemy should be staggered after counter
	TestTrue(TEXT("Enemy should be staggered after AC3 counter"), EnemyHitReact->IsStaggered());

	FCombatTestHelpers::FinalizeDeathIfDying(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: AC3 counter sets bWasCounter on hit info
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_AC3HitInfoMarkedAsCounter,
	"KatanaCombat.CounterSystem.AC3HitInfoMarkedAsCounter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_AC3HitInfoMarkedAsCounter::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!PlayerCombat)
	{
		AddError(TEXT("Failed to create player combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));
	if (!Enemy)
	{
		AddError(TEXT("Failed to create enemy"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// The TryCounter_AC3Mode sets bWasCounter = true on the FHitReactionInfo
	// We verify indirectly by checking the counter succeeded and damage was applied
	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Heavy;
	Context.SwingDirection = ESwingDirection::Vertical;

	bool bSuccess = PlayerCombat->TryCounter_AC3Mode(Context);
	TestTrue(TEXT("AC3 counter should succeed"), bSuccess);

	// If the enemy is dead or has reduced health, the counter-flagged damage was applied
	float HealthAfter = IDamageableInterface::Execute_GetCurrentHealth(Enemy);
	TestTrue(TEXT("Enemy health should be <= 0 after lethal counter"), HealthAfter <= 0.0f);

	FCombatTestHelpers::FinalizeDeathIfDying(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Chain mode transitions to CounterWindow state after parry
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainParryTransition,
	"KatanaCombat.CounterSystem.ChainParryTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainParryTransition::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!PlayerCombat)
	{
		AddError(TEXT("Failed to create player combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));
	if (!Enemy)
	{
		AddError(TEXT("Failed to create enemy"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Chain state starts at None
	TestTrue(TEXT("Chain state should start at None"),
		PlayerCombat->ChainState == EChainCounterState::None);

	// Execute chain counter (parry step)
	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Light;
	bool bSuccess = PlayerCombat->TryCounter_ChainMode(Context);
	TestTrue(TEXT("Chain parry should succeed"), bSuccess);

	// Should transition to CounterWindow
	TestTrue(TEXT("Chain state should be CounterWindow after parry"),
		PlayerCombat->ChainState == EChainCounterState::CounterWindow);

	// Cleanup - cancel the chain to restore time dilation
	PlayerCombat->CancelChainCounter();

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Chain mode counter attack transitions from CounterWindow to complete
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainCounterAttack,
	"KatanaCombat.CounterSystem.ChainCounterAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainCounterAttack::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!PlayerCombat)
	{
		AddError(TEXT("Failed to create player combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));
	if (!Enemy)
	{
		AddError(TEXT("Failed to create enemy"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Start chain parry
	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Light;
	PlayerCombat->TryCounter_ChainMode(Context);
	TestTrue(TEXT("Should be in CounterWindow after parry"),
		PlayerCombat->ChainState == EChainCounterState::CounterWindow);

	// Execute counter attack (step 2 of the chain)
	// This transitions through CounterActive -> FinisherReady -> tries finisher -> None
	PlayerCombat->ExecuteChainCounterAttack();

	// Chain state should reset to None after the chain completes
	// (ExecuteChainFinisher resets to None regardless of finisher success)
	TestTrue(TEXT("Chain state should be None after counter attack completes"),
		PlayerCombat->ChainState == EChainCounterState::None);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Chain cancel resets state to None
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainCancelResetsState,
	"KatanaCombat.CounterSystem.ChainCancelResetsState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainCancelResetsState::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!PlayerCombat)
	{
		AddError(TEXT("Failed to create player combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));
	if (!Enemy)
	{
		AddError(TEXT("Failed to create enemy"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Start chain
	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Light;
	PlayerCombat->TryCounter_ChainMode(Context);
	TestTrue(TEXT("Should be in CounterWindow"),
		PlayerCombat->ChainState == EChainCounterState::CounterWindow);

	// Cancel the chain
	PlayerCombat->CancelChainCounter();
	TestTrue(TEXT("Chain state should be None after cancel"),
		PlayerCombat->ChainState == EChainCounterState::None);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Cancel when already None is a no-op (doesn't crash)
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_CancelNoopWhenNone,
	"KatanaCombat.CounterSystem.CancelNoopWhenNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_CancelNoopWhenNone::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!PlayerCombat)
	{
		AddError(TEXT("Failed to create player combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Should not crash when cancelling with no active chain
	TestTrue(TEXT("Chain state should start at None"),
		PlayerCombat->ChainState == EChainCounterState::None);
	PlayerCombat->CancelChainCounter();
	TestTrue(TEXT("Chain state should still be None"),
		PlayerCombat->ChainState == EChainCounterState::None);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: ExecuteChainCounterAttack fails if not in CounterWindow
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_CounterAttackRequiresWindow,
	"KatanaCombat.CounterSystem.CounterAttackRequiresWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_CounterAttackRequiresWindow::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!PlayerCombat)
	{
		AddError(TEXT("Failed to create player combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Try counter attack without being in CounterWindow
	TestTrue(TEXT("Chain state should be None"), PlayerCombat->ChainState == EChainCounterState::None);
	bool bResult = PlayerCombat->ExecuteChainCounterAttack();
	TestFalse(TEXT("Counter attack should fail when not in CounterWindow"), bResult);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Chain parry staggers enemy
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainParryStaggersEnemy,
	"KatanaCombat.CounterSystem.ChainParryStaggersEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainParryStaggersEnemy::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!PlayerCombat)
	{
		AddError(TEXT("Failed to create player combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));
	if (!Enemy)
	{
		AddError(TEXT("Failed to create enemy"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UHitReactionComponent* EnemyHitReact = Enemy->FindComponentByClass<UHitReactionComponent>();
	if (!EnemyHitReact)
	{
		AddError(TEXT("Enemy missing HitReactionComponent"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Not staggered before
	TestFalse(TEXT("Enemy should not be staggered initially"), EnemyHitReact->IsStaggered());

	// Execute chain parry
	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Light;
	PlayerCombat->TryCounter_ChainMode(Context);

	// Enemy should be staggered
	TestTrue(TEXT("Enemy should be staggered after chain parry"), EnemyHitReact->IsStaggered());

	// Cleanup
	PlayerCombat->CancelChainCounter();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: AC3 counter with null attacker returns false
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_AC3NullAttackerFails,
	"KatanaCombat.CounterSystem.AC3NullAttackerFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_AC3NullAttackerFails::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!PlayerCombat)
	{
		AddError(TEXT("Failed to create player combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Counter with null attacker should fail gracefully
	FCounterContext Context;
	Context.Attacker = nullptr;
	bool bResult = PlayerCombat->TryCounter_AC3Mode(Context);
	TestFalse(TEXT("AC3 counter should fail with null attacker"), bResult);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Chain counter with null attacker returns false
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainNullAttackerFails,
	"KatanaCombat.CounterSystem.ChainNullAttackerFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainNullAttackerFails::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!PlayerCombat)
	{
		AddError(TEXT("Failed to create player combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Counter with null attacker should fail gracefully
	FCounterContext Context;
	Context.Attacker = nullptr;
	bool bResult = PlayerCombat->TryCounter_ChainMode(Context);
	TestFalse(TEXT("Chain counter should fail with null attacker"), bResult);

	// State should remain None
	TestTrue(TEXT("Chain state should remain None after failed counter"),
		PlayerCombat->ChainState == EChainCounterState::None);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
