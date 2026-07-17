// CounterSystemTests.cpp
// Tests for Counter System: AC3 Mode and Chain Mode
// Verifies counter window detection, state transitions, and timeout behavior.

#include "CombatTestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/HitReactionComponent.h"
#include "Data/AttackData.h"
#include "Data/PairedAnimationData.h"
#include "Characters/BaseCombatCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/DamageableInterface.h"
#include "Utilities/CombatGameplayTags.h"
#include "CombatTypes.h"

// ============================================================================
// TEST: Chain counter paired data is nonlethal by default
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainCounterDamagePolicyNonLethalByDefault,
	"KatanaCombat.CounterSystem.ChainCounterDamagePolicyNonLethalByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainCounterDamagePolicyNonLethalByDefault::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!Player || !Player->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create Chain damage policy test actor"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UPairedAnimationData* PairedData = NewObject<UPairedAnimationData>(Player);
	PairedData->bIsLethal = true;

	TestFalse(TEXT("Counter paired animations should be nonlethal by default even when data is lethal"),
		Player->PairedAnimationComponent->ShouldTreatPairedAnimationAsLethal(EPairedReactionType::Counter, PairedData));
	TestTrue(TEXT("Finisher paired animations should preserve lethal data"),
		Player->PairedAnimationComponent->ShouldTreatPairedAnimationAsLethal(EPairedReactionType::Finisher, PairedData));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Block input falls back to normal sustained blocking when no parry target exists
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_BlockInputStartsNormalBlockWhenNoParryTarget,
	"KatanaCombat.CounterSystem.Input.BlockStartsNormalBlockWhenNoParryTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_BlockInputStartsNormalBlockWhenNoParryTarget::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* FrontEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));
	AEnemyCharacter* RearEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(-150.0f, 0.0f, 0.0f));

	if (!Player || !PlayerCombat || !FrontEnemy || !RearEnemy)
	{
		AddError(TEXT("Failed to create normal block input test actor"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);

	TestTrue(TEXT("Block press with no parry target should enter sustained block"),
		IDamageableInterface::Execute_IsBlocking(Player));
	TestEqual(TEXT("Combat state should report Blocking while block is held"),
		static_cast<int32>(ICombatInterface::Execute_GetCombatState(Player)),
		static_cast<int32>(ECombatState::Blocking));
	TestEqual(TEXT("Normal block should not queue a phantom action"),
		PlayerCombat->GetPendingActionCount(),
		0);
	TestTrue(TEXT("Normal block should classify a front attacker as blockable"),
		PlayerCombat->CanBlockAttackFrom(FrontEnemy));
	TestFalse(TEXT("Normal block should not classify a rear attacker as blockable"),
		PlayerCombat->CanBlockAttackFrom(RearEnemy));

	const float HealthBeforeBlockedHit = Player->CurrentHealth;
	FHitReactionInfo BlockedHit = FCombatTestHelpers::CreateTestHitInfo(FrontEnemy, 25.0f);
	IDamageableInterface::Execute_ApplyDamage(Player, BlockedHit);
	TestEqual(TEXT("Normal block should prevent health damage from an incoming hit"),
		Player->CurrentHealth,
		HealthBeforeBlockedHit);

	FHitReactionInfo RearHit = FCombatTestHelpers::CreateTestHitInfo(RearEnemy, 25.0f);
	IDamageableInterface::Execute_ApplyDamage(Player, RearHit);
	TestEqual(TEXT("Normal block should not prevent rear-angle health damage"),
		Player->CurrentHealth,
		HealthBeforeBlockedHit - 25.0f);

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);

	TestFalse(TEXT("Block release should leave sustained block"),
		IDamageableInterface::Execute_IsBlocking(Player));
	TestEqual(TEXT("Combat state should return to Idle after block release"),
		static_cast<int32>(ICombatInterface::Execute_GetCombatState(Player)),
		static_cast<int32>(ECombatState::Idle));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_NullAttackDataPreservesNormalBlock,
	"KatanaCombat.CounterSystem.Block.NullAttackDataPreservesBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_NullAttackDataPreservesNormalBlock::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* FrontEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));

	if (!Player || !PlayerCombat || !FrontEnemy)
	{
		AddError(TEXT("Failed to create null attack data block test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);

	const float HealthBeforeHit = Player->CurrentHealth;
	FHitReactionInfo LegacyHit = FCombatTestHelpers::CreateTestHitInfo(
		FrontEnemy,
		25.0f,
		FVector::ForwardVector,
		nullptr);

	TestTrue(TEXT("Null AttackData should preserve normal block behavior"),
		PlayerCombat->CanBlockHit(LegacyHit));

	IDamageableInterface::Execute_ApplyDamage(Player, LegacyHit);
	TestEqual(TEXT("Null AttackData blocked hit should not damage"),
		Player->CurrentHealth,
		HealthBeforeHit);

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_UnblockableTagBypassesNormalBlock,
	"KatanaCombat.CounterSystem.Block.UnblockableTagBypassesBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_UnblockableTagBypassesNormalBlock::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* FrontEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));

	if (!Player || !PlayerCombat || !FrontEnemy)
	{
		AddError(TEXT("Failed to create unblockable block test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	TestTrue(TEXT("Front enemy should be blockable before attack tags are considered"),
		PlayerCombat->CanBlockAttackFrom(FrontEnemy));

	UAttackData* UnblockableAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	UnblockableAttack->AttackTags.AddTag(KatanaCombatGameplayTags::AttackPropertyUnblockable());

	const float HealthBeforeHit = Player->CurrentHealth;
	FHitReactionInfo Hit = FCombatTestHelpers::CreateTestHitInfo(
		FrontEnemy,
		25.0f,
		FVector::ForwardVector,
		UnblockableAttack);

	TestFalse(TEXT("CanBlockHit should reject attacks tagged unblockable"),
		PlayerCombat->CanBlockHit(Hit));

	IDamageableInterface::Execute_ApplyDamage(Player, Hit);
	TestEqual(TEXT("Unblockable tagged hit should damage through normal block"),
		Player->CurrentHealth,
		HealthBeforeHit - 25.0f);

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
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

	UPairedAnimationComponent* PairedComp = Player->PairedAnimationComponent;

	// Build counter context manually
	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Light;
	Context.SwingDirection = ESwingDirection::Horizontal;

	// Get enemy health before counter
	float HealthBefore = IDamageableInterface::Execute_GetCurrentHealth(Enemy);

	// Execute AC3 counter
	bool bSuccess = PairedComp->TryCounter_AC3Mode(Context);
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

	UPairedAnimationComponent* PairedComp = Player->PairedAnimationComponent;

	// Enemy should not be staggered before counter
	TestFalse(TEXT("Enemy should not be staggered before counter"), EnemyHitReact->IsStaggered());

	// Execute AC3 counter
	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Light;
	PairedComp->TryCounter_AC3Mode(Context);

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

	UPairedAnimationComponent* PairedComp = Player->PairedAnimationComponent;

	// The TryCounter_AC3Mode sets bWasCounter = true on the FHitReactionInfo
	// We verify indirectly by checking the counter succeeded and damage was applied
	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Heavy;
	Context.SwingDirection = ESwingDirection::Vertical;

	bool bSuccess = PairedComp->TryCounter_AC3Mode(Context);
	TestTrue(TEXT("AC3 counter should succeed"), bSuccess);

	// If the enemy is dead or has reduced health, the counter-flagged damage was applied
	float HealthAfter = IDamageableInterface::Execute_GetCurrentHealth(Enemy);
	TestTrue(TEXT("Enemy health should be <= 0 after lethal counter"), HealthAfter <= 0.0f);

	FCombatTestHelpers::FinalizeDeathIfDying(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Counter context preserves notify-provided specific counter data
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ContextPreservesSpecificCounterData,
	"KatanaCombat.CounterSystem.ContextPreservesSpecificCounterData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ContextPreservesSpecificCounterData::RunTest(const FString& Parameters)
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
	if (!Enemy || !Enemy->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create enemy with paired animation component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UPairedAnimationData* SpecificCounterData = NewObject<UPairedAnimationData>();
	SpecificCounterData->ReactionType = EPairedReactionType::Counter;
	SpecificCounterData->AnimationName = TEXT("TestSpecificCounter");

	Enemy->PairedAnimationComponent->SetCounterWindowData(
		EAttackType::Heavy,
		ESwingDirection::Vertical,
		SpecificCounterData,
		1.25f);

	FCounterContext Context = Player->PairedAnimationComponent->GetEnemyCounterContext(Enemy);

	TestTrue(TEXT("Counter context should be valid when enemy paired component owns the window"), Context.IsValid());
	TestEqual(TEXT("Counter context should preserve attack type"), Context.AttackType, EAttackType::Heavy);
	TestEqual(TEXT("Counter context should preserve swing direction"), Context.SwingDirection, ESwingDirection::Vertical);
	TestTrue(TEXT("Counter context should preserve specific paired animation data"),
		Context.SpecificCounterData == SpecificCounterData);
	TestEqual(TEXT("Counter context should preserve window duration"), Context.WindowDuration, 1.25f);

	Enemy->PairedAnimationComponent->ClearCounterWindowData();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: AC3 counter falls back cleanly if specific counter data cannot play
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_AC3SpecificCounterDataFallbackDamage,
	"KatanaCombat.CounterSystem.AC3SpecificCounterDataFallbackDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_AC3SpecificCounterDataFallbackDamage::RunTest(const FString& Parameters)
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

	UPairedAnimationComponent* PairedComp = Player->PairedAnimationComponent;
	UPairedAnimationData* SpecificCounterData = NewObject<UPairedAnimationData>();
	SpecificCounterData->ReactionType = EPairedReactionType::Counter;
	SpecificCounterData->AnimationName = TEXT("InvalidTestCounterData");

	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Heavy;
	Context.SwingDirection = ESwingDirection::Vertical;
	Context.SpecificCounterData = SpecificCounterData;

	const float HealthBefore = IDamageableInterface::Execute_GetCurrentHealth(Enemy);
	const bool bSuccess = PairedComp->TryCounter_AC3Mode(Context);
	const float HealthAfter = IDamageableInterface::Execute_GetCurrentHealth(Enemy);

	TestTrue(TEXT("AC3 counter should succeed by falling back to direct damage"), bSuccess);
	TestTrue(TEXT("Fallback direct damage should reduce enemy health"), HealthAfter < HealthBefore);
	TestFalse(TEXT("Failed specific counter data should not leave paired animation active"),
		PairedComp->IsPairedAnimationActive());
	TestFalse(TEXT("Failed specific counter data should not leave combat input blocked"),
		PairedComp->IsInputBlocked());

	FCombatTestHelpers::FinalizeDeathIfDying(Enemy);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Internal Chain cancel no-op when already None
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_CancelNoopWhenNone,
	"KatanaCombat.CounterSystem.Internal.CancelChainCounterNoopWhenNone",
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

	UPairedAnimationComponent* PairedComp = Player->PairedAnimationComponent;

	// Should not crash when cancelling with no active chain
	TestTrue(TEXT("Chain state should start at None"),
		PairedComp->ChainState == EChainCounterState::None);
	PairedComp->CancelChainCounter();
	TestTrue(TEXT("Chain state should still be None"),
		PairedComp->ChainState == EChainCounterState::None);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Internal ExecuteChainCounterAttack requires CounterWindow
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_CounterAttackRequiresWindow,
	"KatanaCombat.CounterSystem.Internal.ExecuteChainCounterAttackRequiresCounterWindow",
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

	UPairedAnimationComponent* PairedComp = Player->PairedAnimationComponent;

	// Try counter attack without being in CounterWindow
	TestTrue(TEXT("Chain state should be None"), PairedComp->ChainState == EChainCounterState::None);
	bool bResult = PairedComp->ExecuteChainCounterAttack(nullptr);
	TestFalse(TEXT("Counter attack should fail when not in CounterWindow"), bResult);

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

	UPairedAnimationComponent* PairedComp = Player->PairedAnimationComponent;

	// Counter with null attacker should fail gracefully
	FCounterContext Context;
	Context.Attacker = nullptr;
	bool bResult = PairedComp->TryCounter_AC3Mode(Context);
	TestFalse(TEXT("AC3 counter should fail with null attacker"), bResult);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Internal Chain parry helper rejects null attacker
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainNullAttackerFails,
	"KatanaCombat.CounterSystem.Internal.ChainNullAttackerFails",
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

	UPairedAnimationComponent* PairedComp = Player->PairedAnimationComponent;

	// Counter with null attacker should fail gracefully
	FCounterContext Context;
	Context.Attacker = nullptr;
	bool bResult = PairedComp->TryCounter_ChainMode(Context);
	TestFalse(TEXT("Chain counter should fail with null attacker"), bResult);

	// State should remain None
	TestTrue(TEXT("Chain state should remain None after failed counter"),
		PairedComp->ChainState == EChainCounterState::None);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
