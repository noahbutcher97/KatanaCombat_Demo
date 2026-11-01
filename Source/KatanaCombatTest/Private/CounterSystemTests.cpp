// CounterSystemTests.cpp
// Tests for the Chain counter system (parry -> counter -> finisher)
// Verifies counter window detection, state transitions, and timeout behavior.

#include "CombatTestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/HitReactionComponent.h"
#include "Data/AttackData.h"
#include "Data/DefenseConfiguration.h"
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
namespace
{
/**
 * Give the defender a block tolerance owned by the test, so no shipped tuning value is assumed. The defense
 * resolver's normal-block tolerance is the only angle that decides whether a guarded contact is blocked.
 */
void ApplyFixtureBlockTolerance(ABaseCombatCharacter* Defender, const float BlockTolerance)
{
	UDefenseConfiguration* Configuration = NewObject<UDefenseConfiguration>();
	Configuration->NormalBlockFinalTolerance = BlockTolerance;
	Defender->CombatComponent->DefenseConfigurationOverride = Configuration;
	Defender->SetActorRotation(FRotator::ZeroRotator);
	Defender->HitReactionComponent->DamageResistance = 1.0f;
}
}

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

	if (!Player || !PlayerCombat || !Player->HitReactionComponent || !FrontEnemy || !RearEnemy)
	{
		AddError(TEXT("Failed to create normal block input test actor"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	ApplyFixtureBlockTolerance(Player, 40.0f);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 25.0f;
	Attack->AttackTags.Reset();

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);

	TestTrue(TEXT("Block press with no parry target should enter sustained block"),
		IDamageableInterface::Execute_IsBlocking(Player));
	TestEqual(TEXT("Combat state should report Blocking while block is held"),
		static_cast<int32>(ICombatInterface::Execute_GetCombatState(Player)),
		static_cast<int32>(ECombatState::Blocking));
	TestEqual(TEXT("Normal block should not queue a phantom action"),
		PlayerCombat->GetPendingActionCount(),
		0);

	const float HealthBeforeBlockedHit = Player->CurrentHealth;
	FDefenseResolution FrontResolution;
	TestTrue(TEXT("A front weapon contact is resolved once"),
		FCombatTestHelpers::StrikeWithWeapon(FrontEnemy, Player, Attack, FrontResolution));
	TestEqual(TEXT("Held block resolves a front weapon contact as a normal block"),
		FrontResolution.Decision.Outcome, EDefenseOutcome::NormalBlock);
	TestEqual(TEXT("Normal block should prevent health damage from an incoming hit"),
		Player->CurrentHealth,
		HealthBeforeBlockedHit);

	TestTrue(TEXT("Guard is still held for the rear contact"), PlayerCombat->IsBlocking());
	FDefenseResolution RearResolution;
	TestTrue(TEXT("A rear weapon contact is resolved once"),
		FCombatTestHelpers::StrikeWithWeapon(RearEnemy, Player, Attack, RearResolution));
	TestEqual(TEXT("Held block lets a rear weapon contact land as a hit"),
		RearResolution.Decision.Outcome, EDefenseOutcome::Hit);
	TestEqual(TEXT("The rear contact is outside the block tolerance while guarding"),
		RearResolution.Decision.Reason, EDefenseReason::OutsideBlockTolerance);
	TestEqual(TEXT("Normal block should not prevent rear-angle health damage"),
		Player->CurrentHealth,
		HealthBeforeBlockedHit - Attack->BaseDamage * RearEnemy->WeaponComponent->GetDamageMultiplier());

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

	if (!Player || !PlayerCombat || !Player->HitReactionComponent || !FrontEnemy || !FrontEnemy->WeaponComponent)
	{
		AddError(TEXT("Failed to create null attack data block test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	ApplyFixtureBlockTolerance(Player, 40.0f);
	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);

	// A weapon contact without attack data, built by the weapon as a live trace would, still requesting damage.
	FrontEnemy->WeaponComponent->SetCompatibilityTraceGenerationForTesting(1);
	FDefenseContactRequest Request = FrontEnemy->WeaponComponent->BuildDefenseContactRequestForTesting(
		FCombatTestHelpers::CreateWeaponContactHit(Player, FrontEnemy->GetActorLocation()),
		nullptr);
	Request.HitInfo.Damage = 25.0f;

	const float HealthBeforeHit = Player->CurrentHealth;
	const FDefenseContactReceipt Receipt = FrontEnemy->ResolveWeaponContactCandidate(Player, Request);
	FrontEnemy->FinalizeResolvedWeaponContact(Player, Receipt);

	TestTrue(TEXT("The resolved contact carries no attack data"),
		Receipt.Resolution.ActualContact.HitInfo.AttackData == nullptr);
	TestEqual(TEXT("Null AttackData should preserve normal block behavior"),
		Receipt.Resolution.Decision.Outcome, EDefenseOutcome::NormalBlock);
	TestEqual(TEXT("Null AttackData blocked contact reports no applied damage"), Receipt.AppliedDamage, 0.0f);
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
	AEnemyCharacter* ControlEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(220.0f, 0.0f, 0.0f));

	if (!Player || !PlayerCombat || !Player->HitReactionComponent || !FrontEnemy || !ControlEnemy)
	{
		AddError(TEXT("Failed to create unblockable block test actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	ApplyFixtureBlockTolerance(Player, 40.0f);
	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);

	UAttackData* BlockableAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	BlockableAttack->AttackTags.Reset();
	FDefenseResolution ControlResolution;
	TestTrue(TEXT("The control contact is resolved once"),
		FCombatTestHelpers::StrikeWithWeapon(ControlEnemy, Player, BlockableAttack, ControlResolution));
	TestEqual(TEXT("An untagged attack from the same front bearing is blocked"),
		ControlResolution.Decision.Outcome, EDefenseOutcome::NormalBlock);

	UAttackData* UnblockableAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	UnblockableAttack->AttackTags.Reset();
	UnblockableAttack->AttackTags.AddTag(KatanaCombatGameplayTags::AttackPropertyUnblockable());
	UnblockableAttack->BaseDamage = 25.0f;

	TestTrue(TEXT("Guard is still held for the unblockable contact"), PlayerCombat->IsBlocking());
	const float HealthBeforeHit = Player->CurrentHealth;
	FDefenseResolution UnblockableResolution;
	TestTrue(TEXT("The unblockable contact is resolved once"),
		FCombatTestHelpers::StrikeWithWeapon(FrontEnemy, Player, UnblockableAttack, UnblockableResolution));
	TestEqual(TEXT("The defense resolver rejects the block for attacks tagged unblockable"),
		UnblockableResolution.Decision.Outcome, EDefenseOutcome::UnblockableHit);
	TestEqual(TEXT("Unblockable tagged hit should damage through normal block"),
		Player->CurrentHealth,
		HealthBeforeHit - UnblockableAttack->BaseDamage * FrontEnemy->WeaponComponent->GetDamageMultiplier());

	PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
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

	const FCounterContext& Context = Enemy->PairedAnimationComponent->GetCounterWindowData();

	TestTrue(TEXT("Counter window should be open"), Enemy->PairedAnimationComponent->IsInCounterWindow());
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

