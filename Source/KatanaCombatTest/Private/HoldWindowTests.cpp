// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"

/**
 * Test: Hold Window Button State Detection
 * Verifies hold checks button state at window start, not duration
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldWindowTest, "KatanaCombat.CombatComponent.HoldWindow", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldWindowTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ACharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Create holdable attack
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	Attack->bCanHold = true;
	CombatComp->DefaultLightAttack = Attack;

	// Test 1: Button HELD when window opens → Enter hold state
	CombatComp->OnLightAttackPressed();  // Hold button down
	CombatComp->ExecuteAttack(Attack);
	CombatComp->CurrentAttackInputType = EInputType::LightAttack;

	CombatComp->OpenHoldWindow(0.5f);

	TestTrue("Should enter hold state when button held",
		CombatComp->IsHolding());
	TestEqual("Should be in HoldingLightAttack state",
		CombatComp->GetCombatState(), ECombatState::HoldingLightAttack);

	// Test 2: Button NOT held when window opens → Continue normal
	CombatComp->SetCombatState(ECombatState::Idle);
	CombatComp->bIsHolding = false;

	CombatComp->OnLightAttackPressed();
	CombatComp->OnLightAttackReleased();  // Release before window
	CombatComp->ExecuteAttack(Attack);
	CombatComp->CurrentAttackInputType = EInputType::LightAttack;

	CombatComp->OpenHoldWindow(0.5f);

	TestFalse("Should NOT enter hold state when button released",
		CombatComp->IsHolding());
	TestEqual("Should remain in Attacking state",
		CombatComp->GetCombatState(), ECombatState::Attacking);

	// Test 3: Wrong input type held → Don't enter hold
	CombatComp->SetCombatState(ECombatState::Idle);
	CombatComp->bIsHolding = false;

	CombatComp->OnHeavyAttackPressed();  // Wrong button
	CombatComp->ExecuteAttack(Attack);
	CombatComp->CurrentAttackInputType = EInputType::LightAttack;  // Attack was light

	CombatComp->OpenHoldWindow(0.5f);

	TestFalse("Should NOT hold when wrong button pressed",
		CombatComp->IsHolding());

	// Test 4: Attack with bCanHold = false → Don't enter hold
	CombatComp->SetCombatState(ECombatState::Idle);
	UAttackData* NonHoldableAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	NonHoldableAttack->bCanHold = false;

	CombatComp->OnLightAttackPressed();
	CombatComp->ExecuteAttack(NonHoldableAttack);
	CombatComp->CurrentAttackInputType = EInputType::LightAttack;

	CombatComp->OpenHoldWindow(0.5f);

	TestFalse("Should NOT hold when attack doesn't allow holding",
		CombatComp->IsHolding());

	// Test 5: Heavy attacks can also trigger hold
	CombatComp->SetCombatState(ECombatState::Idle);
	CombatComp->bIsHolding = false;

	UAttackData* HeavyAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	HeavyAttack->bCanHold = true;

	CombatComp->OnHeavyAttackPressed();
	CombatComp->ExecuteAttack(HeavyAttack);
	CombatComp->CurrentAttackInputType = EInputType::HeavyAttack;

	CombatComp->OpenHoldWindow(0.5f);

	TestTrue("Heavy attacks can also enter hold state",
		CombatComp->IsHolding());

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);

	return true;
}