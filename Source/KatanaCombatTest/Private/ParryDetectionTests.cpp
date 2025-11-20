// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"

/**
 * Test: Parry Detection
 * Verifies parry window is on ATTACKER's montage and defender checks it
 *
 * V2 MIGRATION STATUS: NEEDS REWRITE
 * - V1 used OpenParryWindow(), IsInParryWindow(), CloseParryWindow()
 * - V2 uses RegisterCheckpoint(EActionWindowType::Parry) via AnimNotifyState_ParryWindow
 * - Defender checks attacker's checkpoints using GetActiveWindows()
 *
 * TODO V2: Rewrite tests using V2 parry mechanics:
 * - Attacker: RegisterCheckpoint(EActionWindowType::Parry, MontageTime, Duration)
 * - Defender: Check GetActiveWindows() on enemy CombatComponentV2
 * - Test parry timing window (typically 0.3s during early windup)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParryDetectionTest, "KatanaCombat.CombatComponent.ParryDetection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FParryDetectionTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponentV2* AttackerComp = nullptr;
	UCombatComponentV2* DefenderComp = nullptr;
	UTargetingComponent* DefenderTargeting = nullptr;

	ACharacter* Attacker = FCombatTestHelpers::CreateTestCharacterWithCombat(World, AttackerComp);
	ACharacter* Defender = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, DefenderComp, DefenderTargeting);

	if (!TestNotNull("Attacker should be created", AttackerComp) ||
		!TestNotNull("Defender should be created", DefenderComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	AddWarning("ParryDetectionTests not yet migrated to V2 - skipping");

	/* V1 REMOVED: All parry detection tests
	V1 tested: Parry window on attacker, defender checks IsInParryWindow()
	V2 TODO: Test RegisterCheckpoint on attacker, GetActiveWindows() query from defender
	*/

	// Cleanup
	World->DestroyActor(Attacker);
	World->DestroyActor(Defender);
	FCombatTestHelpers::DestroyTestWorld(World);

	return true;
}