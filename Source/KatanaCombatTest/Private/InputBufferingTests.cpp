// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"

/**
 * Test: Input Buffering (Hybrid Responsive + Snappy)
 * Verifies input always buffers, combo window only modifies timing
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputBufferingTest, "KatanaCombat.CombatComponent.InputBuffering", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInputBufferingTest::RunTest(const FString& Parameters)
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

	// Create attack chain
	UAttackData* Attack1 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* Attack2 = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	Attack1->NextComboAttack = Attack2;
	CombatComp->DefaultLightAttack = Attack1;

	// Test 1: Input buffered OUTSIDE combo window (responsive path)
	CombatComp->ExecuteAttack(Attack1);
	CombatComp->OnAttackPhaseBegin(EAttackPhase::Active);

	// Press attack while NOT in combo window
	CombatComp->OnLightAttackPressed();

	TestTrue("Input should be buffered",
		CombatComp->bLightAttackBuffered);
	TestFalse("Should NOT be tagged as combo-window input",
		CombatComp->bLightAttackInComboWindow);

	// Test 2: Input buffered INSIDE combo window (snappy path)
	CombatComp->SetCombatState(ECombatState::Idle);
	CombatComp->bLightAttackBuffered = false;
	CombatComp->bLightAttackInComboWindow = false;

	CombatComp->ExecuteAttack(Attack1);
	CombatComp->OnAttackPhaseBegin(EAttackPhase::Active);
	CombatComp->OpenComboWindow(0.6f);  // Open combo window

	// Press attack during combo window
	CombatComp->OnLightAttackPressed();

	TestTrue("Input should be buffered",
		CombatComp->bLightAttackBuffered);
	TestTrue("Should be tagged as combo-window input",
		CombatComp->bLightAttackInComboWindow);

	// Test 3: Combo window doesn't GATE buffering, only affects TIMING
	CombatComp->SetCombatState(ECombatState::Idle);
	CombatComp->ExecuteAttack(Attack1);
	CombatComp->OnAttackPhaseBegin(EAttackPhase::Active);

	// Input OUTSIDE window still buffers
	CombatComp->CloseComboWindow();
	CombatComp->OnLightAttackPressed();

	TestTrue("Input should buffer even outside combo window",
		CombatComp->bLightAttackBuffered);

	// Test 4: Heavy attacks also buffer correctly
	CombatComp->SetCombatState(ECombatState::Idle);
	CombatComp->bHeavyAttackBuffered = false;

	CombatComp->ExecuteAttack(Attack1);
	CombatComp->OnAttackPhaseBegin(EAttackPhase::Active);
	CombatComp->OnHeavyAttackPressed();

	TestTrue("Heavy attack should be buffered",
		CombatComp->bHeavyAttackBuffered);

	// Test 5: Input NOT buffered when in Idle state (executes immediately instead)
	CombatComp->SetCombatState(ECombatState::Idle);
	CombatComp->bLightAttackBuffered = false;

	CombatComp->OnLightAttackPressed();

	// Should have executed immediately, not buffered
	TestEqual("Should have started attacking",
		CombatComp->GetCombatState(), ECombatState::Attacking);
	TestFalse("Input should not be buffered when can execute immediately",
		CombatComp->bLightAttackBuffered);

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);

	return true;
}