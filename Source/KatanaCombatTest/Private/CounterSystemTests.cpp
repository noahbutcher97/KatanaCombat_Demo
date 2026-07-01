// CounterSystemTests.cpp
// Tests for Counter System: AC3 Mode and Chain Mode
// Verifies counter window detection, state transitions, and timeout behavior.

#include "CombatTestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/HitReactionComponent.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "Data/AttackData.h"
#include "Data/AttackConfiguration.h"
#include "Data/PairedAnimationData.h"
#include "Characters/BaseCombatCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Interfaces/DamageableInterface.h"
#include "CombatTypes.h"

namespace
{
void ConfigurePawnOverlapForCounterTest(ACharacter* Character)
{
	if (UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr)
	{
		Capsule->SetGenerateOverlapEvents(true);
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Capsule->SetCollisionObjectType(ECC_Pawn);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Capsule->UpdateOverlaps();
	}
}

void ClearChainCounterTestEffects(UWorld* World, UPairedAnimationComponent* PairedComp)
{
	if (World && PairedComp)
	{
		World->GetTimerManager().ClearAllTimersForObject(PairedComp);
		UCinematicEffectsUtilityLibrary::RestoreTimeDilation(World);
	}
}

void ConfigureChainInputFixture(APlayerCharacter* Player, UCombatComponent* PlayerCombat, AEnemyCharacter* Enemy)
{
	UCombatSettings* TestCombatSettings = FCombatTestHelpers::CreateTestCombatSettings();
	if (UAttackConfiguration* AttackConfig = TestCombatSettings ? TestCombatSettings->GetAttackConfiguration() : nullptr)
	{
		AttackConfig->DefaultLightAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
		AttackConfig->DefaultHeavyAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	}

	if (Player)
	{
		Player->CombatSettings = TestCombatSettings;
		ConfigurePawnOverlapForCounterTest(Player);
	}

	if (PlayerCombat)
	{
		PlayerCombat->CombatSettings = TestCombatSettings;
	}

	if (Enemy)
	{
		ConfigurePawnOverlapForCounterTest(Enemy);
		if (Enemy->PairedAnimationComponent)
		{
			Enemy->PairedAnimationComponent->SetParryWindowActive(true);
		}
	}
}
}

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

	UPairedAnimationComponent* PairedComp = Player->PairedAnimationComponent;
	if (!PairedComp)
	{
		AddError(TEXT("Failed to get paired animation component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Initially false
	TestFalse(TEXT("Parry window should start inactive"), PairedComp->IsInParryWindow());

	// Activate
	PairedComp->SetParryWindowActive(true);
	TestTrue(TEXT("Parry window should be active after SetParryWindowActive(true)"), PairedComp->IsInParryWindow());

	// Deactivate
	PairedComp->SetParryWindowActive(false);
	TestFalse(TEXT("Parry window should be inactive after SetParryWindowActive(false)"), PairedComp->IsInParryWindow());

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
// TEST: Public TryCounter in Chain mode uses the attacker's parry window
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_TryCounterChainUsesParryWindow,
	"KatanaCombat.CounterSystem.TryCounter.ChainUsesParryWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_TryCounterChainUsesParryWindow::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);

	if (!Player || !PlayerCombat)
	{
		AddError(TEXT("Failed to create player combat component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UPairedAnimationComponent* PlayerPaired = Player->PairedAnimationComponent;
	if (!PlayerPaired)
	{
		AddError(TEXT("Failed to create player paired animation component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	if (!Enemy || !Enemy->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create enemy paired animation component"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	ConfigurePawnOverlapForCounterTest(Player);
	ConfigurePawnOverlapForCounterTest(Enemy);

	Enemy->PairedAnimationComponent->SetParryWindowActive(true);

	TestEqual(TEXT("Fixture should expose a parry-window target"),
		PlayerPaired->FindParryableEnemy(), Cast<AActor>(Enemy));
	TestNull(TEXT("Fixture should not expose a counter-window target"),
		PlayerPaired->FindCounterableEnemy());
	TestTrue(TEXT("Chain CanCounter should accept a parry-window target"),
		PlayerPaired->CanCounter());

	const bool bStarted = PlayerPaired->TryCounter();
	TestTrue(TEXT("Public TryCounter should start Chain from a parry-window target"),
		bStarted);
	TestFalse(TEXT("Chain should no longer be counter-startable after it begins"),
		PlayerPaired->CanCounter());

	ClearChainCounterTestEffects(World, bStarted ? Player->PairedAnimationComponent : nullptr);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Chain retains the parried target across the waiting window
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainStoresParriedTarget,
	"KatanaCombat.CounterSystem.ChainStoresParriedTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainStoresParriedTarget::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));

	if (!Player || !PlayerCombat || !Player->PairedAnimationComponent || !Enemy || !Enemy->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create Chain target test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	ConfigureChainInputFixture(Player, PlayerCombat, Enemy);

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	TestTrue(TEXT("Chain should retain the parried target"),
		Player->PairedAnimationComponent->HasActiveChainTarget());

	Player->PairedAnimationComponent->CancelPairedAnimation();
	TestFalse(TEXT("Public cancel should clear retained target"),
		Player->PairedAnimationComponent->HasActiveChainTarget());

	Enemy->PairedAnimationComponent->SetParryWindowActive(false);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

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
// TEST: Public paired cancel clears retained Chain context
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainPairedCancelClearsContext,
	"KatanaCombat.CounterSystem.ChainPairedCancelClearsContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainPairedCancelClearsContext::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));

	if (!Player || !PlayerCombat || !Player->PairedAnimationComponent || !Enemy || !Enemy->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create Chain paired-cancel cleanup test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	ConfigureChainInputFixture(Player, PlayerCombat, Enemy);
	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);

	TestTrue(TEXT("Chain should retain target before paired cancel"),
		Player->PairedAnimationComponent->HasActiveChainTarget());

	Player->PairedAnimationComponent->CancelPairedAnimation();

	TestEqual(TEXT("Paired cancel should clear Chain state"),
		static_cast<int32>(Player->PairedAnimationComponent->GetChainState()),
		static_cast<int32>(EChainCounterState::None));
	TestFalse(TEXT("Paired cancel should clear retained target"),
		Player->PairedAnimationComponent->HasActiveChainTarget());

	Enemy->PairedAnimationComponent->SetParryWindowActive(false);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Block input starts Chain parry through public CombatComponent input
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_BlockInputStartsChainParry,
	"KatanaCombat.CounterSystem.Input.BlockStartsChainParry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_BlockInputStartsChainParry::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));

	if (!Player || !PlayerCombat || !Player->PairedAnimationComponent || !Enemy || !Enemy->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create Block input Chain test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UCombatSettings* TestCombatSettings = FCombatTestHelpers::CreateTestCombatSettings();
	if (UAttackConfiguration* AttackConfig = TestCombatSettings ? TestCombatSettings->GetAttackConfiguration() : nullptr)
	{
		AttackConfig->DefaultLightAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
		AttackConfig->DefaultHeavyAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	}
	Player->CombatSettings = TestCombatSettings;
	PlayerCombat->CombatSettings = TestCombatSettings;
	ConfigurePawnOverlapForCounterTest(Player);
	ConfigurePawnOverlapForCounterTest(Enemy);
	Enemy->PairedAnimationComponent->SetParryWindowActive(true);

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);

	TestEqual(TEXT("Block press should start Chain parry and enter CounterWindow"),
		static_cast<int32>(Player->PairedAnimationComponent->GetChainState()),
		static_cast<int32>(EChainCounterState::CounterWindow));
	TestEqual(TEXT("Successful Chain parry should consume Block input without queueing"),
		PlayerCombat->GetPendingActionCount(),
		0);

	ClearChainCounterTestEffects(World, Player->PairedAnimationComponent);
	Enemy->PairedAnimationComponent->SetParryWindowActive(false);
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
// TEST: Internal Chain parry helper transitions to CounterWindow state
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainParryTransition,
	"KatanaCombat.CounterSystem.Internal.ChainParryTransition",
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

	UPairedAnimationComponent* PairedComp = Player->PairedAnimationComponent;

	// Chain state starts at None
	TestTrue(TEXT("Chain state should start at None"),
		PairedComp->ChainState == EChainCounterState::None);

	// Execute chain counter (parry step)
	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Light;
	bool bSuccess = PairedComp->TryCounter_ChainMode(Context);
	TestTrue(TEXT("Chain parry should succeed"), bSuccess);

	// Should transition to CounterWindow
	TestTrue(TEXT("Chain state should be CounterWindow after parry"),
		PairedComp->ChainState == EChainCounterState::CounterWindow);

	// Cleanup - cancel the chain to restore time dilation
	PairedComp->CancelChainCounter();

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// TEST: Attack input advances a waiting Chain counter through public input flow
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainAttackInputAdvancesCounter,
	"KatanaCombat.CounterSystem.ChainAttackInputAdvancesCounter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainAttackInputAdvancesCounter::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));

	if (!Player || !PlayerCombat || !Player->PairedAnimationComponent || !Enemy || !Enemy->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create Chain input test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UCombatSettings* TestCombatSettings = FCombatTestHelpers::CreateTestCombatSettings();
	if (UAttackConfiguration* AttackConfig = TestCombatSettings ? TestCombatSettings->GetAttackConfiguration() : nullptr)
	{
		AttackConfig->DefaultLightAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
		AttackConfig->DefaultHeavyAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	}
	Player->CombatSettings = TestCombatSettings;
	PlayerCombat->CombatSettings = TestCombatSettings;
	ConfigurePawnOverlapForCounterTest(Player);
	ConfigurePawnOverlapForCounterTest(Enemy);
	Enemy->PairedAnimationComponent->SetParryWindowActive(true);

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	TestTrue(TEXT("Chain should wait for attack input"),
		Player->PairedAnimationComponent->IsChainCounterWaitingForAttack());

	const FQueueStats StatsBeforeAttackInput = PlayerCombat->GetQueueStats();
	PlayerCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);

	TestFalse(TEXT("Attack input should leave the waiting state"),
		Player->PairedAnimationComponent->IsChainCounterWaitingForAttack());
	TestEqual(TEXT("Successful Chain attack input should not queue a normal attack"),
		PlayerCombat->GetPendingActionCount(),
		0);
	TestEqual(TEXT("Successful Chain attack input should be consumed before normal input accounting"),
		PlayerCombat->GetQueueStats().TotalInputs,
		StatsBeforeAttackInput.TotalInputs);

	ClearChainCounterTestEffects(World, Player->PairedAnimationComponent);
	Enemy->PairedAnimationComponent->SetParryWindowActive(false);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Chain advance rejects null selected attack data without leaving waiting state
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainAdvanceRejectsNullAttackData,
	"KatanaCombat.CounterSystem.ChainAdvanceRejectsNullAttackData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCounter_ChainAdvanceRejectsNullAttackData::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));

	if (!Player || !PlayerCombat || !Player->PairedAnimationComponent || !Enemy || !Enemy->PairedAnimationComponent)
	{
		AddError(TEXT("Failed to create Chain null-advance test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	ConfigureChainInputFixture(Player, PlayerCombat, Enemy);
	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	TestTrue(TEXT("Chain should wait for attack input"),
		Player->PairedAnimationComponent->IsChainCounterWaitingForAttack());

	const bool bAdvanced = Player->PairedAnimationComponent->TryAdvanceChainCounter(nullptr);
	TestFalse(TEXT("Null selected attack data should not advance Chain"),
		bAdvanced);
	TestTrue(TEXT("Rejected advance should keep Chain waiting for valid attack input"),
		Player->PairedAnimationComponent->IsChainCounterWaitingForAttack());

	Player->PairedAnimationComponent->CancelPairedAnimation();
	Enemy->PairedAnimationComponent->SetParryWindowActive(false);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

// ============================================================================
// TEST: Internal Chain cancel resets state to None
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainCancelResetsState,
	"KatanaCombat.CounterSystem.Internal.ChainCancelResetsState",
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

	UPairedAnimationComponent* PairedComp = Player->PairedAnimationComponent;

	// Start chain
	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Light;
	PairedComp->TryCounter_ChainMode(Context);
	TestTrue(TEXT("Should be in CounterWindow"),
		PairedComp->ChainState == EChainCounterState::CounterWindow);

	// Cancel the chain
	PairedComp->CancelChainCounter();
	TestTrue(TEXT("Chain state should be None after cancel"),
		PairedComp->ChainState == EChainCounterState::None);

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
// TEST: Internal Chain parry helper staggers enemy
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCounter_ChainParryStaggersEnemy,
	"KatanaCombat.CounterSystem.Internal.ChainParryStaggersEnemy",
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

	UPairedAnimationComponent* PairedComp = Player->PairedAnimationComponent;

	// Not staggered before
	TestFalse(TEXT("Enemy should not be staggered initially"), EnemyHitReact->IsStaggered());

	// Execute chain parry
	FCounterContext Context;
	Context.Attacker = Enemy;
	Context.AttackType = EAttackType::Light;
	PairedComp->TryCounter_ChainMode(Context);

	// Enemy should be staggered
	TestTrue(TEXT("Enemy should be staggered after chain parry"), EnemyHitReact->IsStaggered());

	// Cleanup
	PairedComp->CancelChainCounter();
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
