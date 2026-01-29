// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"

/**
 * Test: Parry Detection
 * Verifies parry window is on ATTACKER's montage and defender checks it
 *
 * Design Rule: Parry window goes on the ATTACKER's animation, not the defender's.
 * Defender queries the attacker's CombatComponent to check if parry is available.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParryDetectionTest, "KatanaCombat.CombatComponent.ParryDetection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FParryDetectionTest::RunTest(const FString& Parameters)
{
	// Setup - create two characters
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* AttackerComp = nullptr;
	UCombatComponent* DefenderComp = nullptr;

	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestCharacterWithCombat(World, AttackerComp);
	APlayerCharacter* Defender = FCombatTestHelpers::CreateTestCharacterWithCombat(World, DefenderComp);

	if (!TestNotNull("Attacker CombatComponent should be created", AttackerComp) ||
		!TestNotNull("Defender CombatComponent should be created", DefenderComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Helper lambda to check if parry window is active
	auto HasActiveParryWindow = [](UCombatComponent* Comp, float Time) -> bool
	{
		TArray<FTimerCheckpoint> Windows = Comp->GetActiveWindows(Time);
		for (const FTimerCheckpoint& Checkpoint : Windows)
		{
			if (Checkpoint.WindowType == EActionWindowType::Parry)
			{
				return true;
			}
		}
		return false;
	};

	// Test 1: Parry window not active by default
	TestFalse("Attacker should not have parry window by default", HasActiveParryWindow(AttackerComp, 0.0f));

	// Test 2: Registering parry checkpoint on attacker activates parry window
	// Parry window at 0.1s with 0.3s duration (active from 0.1 to 0.4)
	AttackerComp->RegisterCheckpoint(EActionWindowType::Parry, 0.1f, 0.3f);

	// At 0.2s (within window), parry should be active
	TestTrue("Attacker should have parry window at 0.2s", HasActiveParryWindow(AttackerComp, 0.2f));

	// Test 3: Defender can query attacker's parry window
	// (In real gameplay, defender would get attacker via targeting system)
	TestFalse("Defender should NOT have parry window (on their own component)", HasActiveParryWindow(DefenderComp, 0.2f));

	// Test 4: Outside parry window timing
	TestFalse("Parry window should not be active at 0.5s (outside 0.1-0.4 range)", HasActiveParryWindow(AttackerComp, 0.5f));

	// Test 5: Before parry window starts
	TestFalse("Parry window should not be active at 0.05s (before 0.1 start)", HasActiveParryWindow(AttackerComp, 0.05f));

	// Cleanup
	World->DestroyActor(Attacker);
	World->DestroyActor(Defender);
	FCombatTestHelpers::DestroyTestWorld(World);

	return true;
}
