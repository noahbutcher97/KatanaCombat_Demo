// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"

/**
 * Test: ExecuteAttack vs ExecuteComboAttack Separation
 * Verifies ExecuteAttack only works from Idle, combos work from Attacking
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackExecutionTest, "KatanaCombat.CombatComponent.AttackExecution", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackExecutionTest::RunTest(const FString& Parameters)
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

	// Create test attacks
	UAttackData* Attack1 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* Attack2 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	Attack1->NextComboAttack = Attack2;
	CombatComp->DefaultLightAttack = Attack1;

	// Test 1: ExecuteAttack from Idle (valid)
	bool bExecuteSuccess = CombatComp->ExecuteAttack(Attack1);

	TestTrue("ExecuteAttack should work from Idle", bExecuteSuccess);
	TestEqual("Should be in Attacking state",
		CombatComp->GetCombatState(), ECombatState::Attacking);
	TestEqual("Current attack should be Attack1",
		CombatComp->GetCurrentAttack(), Attack1);

	// Test 2: ExecuteAttack from Attacking (invalid)
	bool bExecuteFail = CombatComp->ExecuteAttack(Attack2);

	TestFalse("ExecuteAttack should fail from Attacking state", bExecuteFail);
	TestEqual("Should still have Attack1 as current",
		CombatComp->GetCurrentAttack(), Attack1);

	// Test 3: ExecuteComboAttack from Attacking (valid)
	CombatComp->ExecuteComboAttack(Attack2);

	TestEqual("ExecuteComboAttack should work from Attacking",
		CombatComp->GetCurrentAttack(), Attack2);
	TestEqual("Should still be in Attacking state",
		CombatComp->GetCombatState(), ECombatState::Attacking);

	// Test 4: Null attack data protection
	CombatComp->SetCombatState(ECombatState::Idle);
	bool bNullProtection = CombatComp->ExecuteAttack(nullptr);

	TestFalse("ExecuteAttack should reject null attack data", bNullProtection);
	TestEqual("Should remain in Idle",
		CombatComp->GetCombatState(), ECombatState::Idle);

	// Test 5: CanAttack only returns true in Idle
	CombatComp->SetCombatState(ECombatState::Idle);
	TestTrue("CanAttack should return true in Idle",
		CombatComp->CanAttack());

	CombatComp->SetCombatState(ECombatState::Attacking);
	TestFalse("CanAttack should return false in Attacking",
		CombatComp->CanAttack());

	CombatComp->SetCombatState(ECombatState::Blocking);
	TestFalse("CanAttack should return false in Blocking",
		CombatComp->CanAttack());

	// Test 6: StopCurrentAttack clears attack and returns to Idle
	CombatComp->SetCombatState(ECombatState::Idle);
	CombatComp->ExecuteAttack(Attack1);

	TestEqual("Should have Attack1 active",
		CombatComp->GetCurrentAttack(), Attack1);

	CombatComp->StopCurrentAttack();

	TestNull("Current attack should be cleared",
		CombatComp->GetCurrentAttack());
	TestEqual("Should return to Idle",
		CombatComp->GetCombatState(), ECombatState::Idle);

	// Test 7: ExecuteAttack from other states also fails
	CombatComp->SetCombatState(ECombatState::Blocking);
	bool bFailFromBlock = CombatComp->ExecuteAttack(Attack1);
	TestFalse("ExecuteAttack should fail from Blocking", bFailFromBlock);

	CombatComp->SetCombatState(ECombatState::Evading);
	bool bFailFromEvade = CombatComp->ExecuteAttack(Attack1);
	TestFalse("ExecuteAttack should fail from Evading", bFailFromEvade);

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);

	return true;
}