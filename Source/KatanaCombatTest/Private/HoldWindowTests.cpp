// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"

/**
 * Test: Hold State - Default Not Holding
 * Verifies IsHolding returns false by default
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldDefaultNotHoldingTest, "KatanaCombat.CombatComponent.Hold.DefaultNotHolding", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldDefaultNotHoldingTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ASamuraiCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Default state should not be holding
	TestFalse("Should not be holding by default", CombatComp->IsHolding());

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: ActivateHold - Sets Holding State
 * Verifies ActivateHold() sets the holding flag
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActivateHoldSetsStateTest, "KatanaCombat.CombatComponent.Hold.ActivateSetsState", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FActivateHoldSetsStateTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ASamuraiCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Activate hold
	CombatComp->ActivateHold(EInputType::HeavyAttack, 0.5f);

	// Should now be holding
	TestTrue("Should be holding after ActivateHold", CombatComp->IsHolding());

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Hold State Tracking - Tracks Input Type
 * Verifies hold state correctly tracks the input type and can be replaced
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeactivateHoldClearsStateTest, "KatanaCombat.CombatComponent.Hold.StateTracking", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeactivateHoldClearsStateTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ASamuraiCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Test hold activation and state tracking
	CombatComp->ActivateHold(EInputType::HeavyAttack, 0.5f);
	TestTrue("Should be holding after ActivateHold", CombatComp->IsHolding());

	// Test that hold input type is correctly tracked
	TestEqual("Hold input type should be HeavyAttack",
		CombatComp->GetHoldInputType(), EInputType::HeavyAttack);

	// Activate a different hold type (should replace)
	CombatComp->ActivateHold(EInputType::LightAttack, 1.0f);
	TestTrue("Should still be holding after second ActivateHold", CombatComp->IsHolding());
	TestEqual("Hold input type should now be LightAttack",
		CombatComp->GetHoldInputType(), EInputType::LightAttack);

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Hold Duration - Starts at Zero
 * Verifies hold duration starts at zero when hold is activated
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldDurationStartsZeroTest, "KatanaCombat.CombatComponent.Hold.DurationStartsZero", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldDurationStartsZeroTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ASamuraiCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Activate hold
	CombatComp->ActivateHold(EInputType::HeavyAttack, 0.5f);

	// Duration should start at or near zero
	TestTrue("Hold duration should start at zero",
		FMath::IsNearlyZero(CombatComp->GetHoldDuration(), 0.01f));

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: OnHoldWindowStart - Does Not Crash
 * Verifies OnHoldWindowStart() doesn't crash when called without proper input state
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldWindowStartNoCrashTest, "KatanaCombat.CombatComponent.Hold.WindowStartNoCrash", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHoldWindowStartNoCrashTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ASamuraiCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Call should not crash even without proper input state
	CombatComp->OnHoldWindowStart(EInputType::LightAttack);

	// Verify component is still valid
	TestTrue("Component should still be valid after OnHoldWindowStart",
		IsValid(CombatComp));

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Multiple Hold Activations - Last One Wins
 * Verifies that re-activating hold updates the state
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMultipleHoldActivationsTest, "KatanaCombat.CombatComponent.Hold.MultipleActivations", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMultipleHoldActivationsTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ASamuraiCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Activate with LightAttack
	CombatComp->ActivateHold(EInputType::LightAttack, 0.5f);
	TestTrue("Should be holding after first activation", CombatComp->IsHolding());

	// Re-activate with HeavyAttack (should overwrite)
	CombatComp->ActivateHold(EInputType::HeavyAttack, 0.25f);
	TestTrue("Should still be holding after second activation", CombatComp->IsHolding());

	// Cleanup
	World->DestroyActor(Character);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
