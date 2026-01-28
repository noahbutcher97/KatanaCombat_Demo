// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Core/TargetingComponent.h"
#include "Characters/SamuraiCharacter.h"
#include "Characters/EnemyCharacter.h"

/**
 * Test: FindBestTargetForDirection - Enemy in Front
 * Verifies soft aim assist finds enemies directly ahead
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetingFindEnemyAheadTest, "KatanaCombat.Targeting.FindEnemyAhead", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTargetingFindEnemyAheadTest::RunTest(const FString& Parameters)
{
	// Setup: Player at origin, enemy 300 units ahead
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ASamuraiCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(300.0f, 0.0f, 0.0f));

	if (!TestNotNull("Player should be created", Player) ||
		!TestNotNull("Enemy should be created", Enemy))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingComponent* Targeting = Player->TargetingComponent;
	if (!TestNotNull("TargetingComponent should exist", Targeting))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Test: Find target with forward input direction
	AActor* BestTarget = nullptr;
	FVector ForwardDirection = Player->GetActorForwardVector();
	FRotator TargetRotation = Targeting->FindBestTargetForDirection(
		ForwardDirection,
		BestTarget,  // OutBestTarget is now second parameter
		500.0f,      // MaxRange
		45.0f,       // GradientAngle
		120.0f,      // OppositeAngle
		0.7f,        // AngleWeight
		0.3f         // DistanceWeight
	);

	TestNotNull("Should find enemy ahead", BestTarget);
	if (BestTarget)
	{
		TestEqual("Best target should be the enemy", BestTarget, Cast<AActor>(Enemy));
	}

	// Cleanup
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: FindBestTargetForDirection - Enemy Behind (Ignored)
 * Verifies soft aim assist ignores enemies in opposite direction
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetingIgnoreEnemyBehindTest, "KatanaCombat.Targeting.IgnoreEnemyBehind", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTargetingIgnoreEnemyBehindTest::RunTest(const FString& Parameters)
{
	// Setup: Player at origin, enemy 300 units behind
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ASamuraiCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(-300.0f, 0.0f, 0.0f));

	if (!TestNotNull("Player should be created", Player) ||
		!TestNotNull("Enemy should be created", Enemy))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingComponent* Targeting = Player->TargetingComponent;
	if (!TestNotNull("TargetingComponent should exist", Targeting))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Test: Find target with forward input - should NOT find enemy behind
	AActor* BestTarget = nullptr;
	FVector ForwardDirection = Player->GetActorForwardVector();
	Targeting->FindBestTargetForDirection(
		ForwardDirection,
		BestTarget,
		500.0f,
		45.0f,
		120.0f,  // 120° threshold - enemy at 180° should be ignored
		0.7f,
		0.3f
	);

	TestNull("Should NOT find enemy behind (>120° from forward)", BestTarget);

	// Cleanup
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: FindBestTargetForDirection - Multiple Enemies Scoring
 * Verifies scoring picks closest + most aligned enemy
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetingMultipleEnemyScoringTest, "KatanaCombat.Targeting.MultipleEnemyScoring", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTargetingMultipleEnemyScoringTest::RunTest(const FString& Parameters)
{
	// Setup: Player at origin, enemies at various positions
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ASamuraiCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);

	// Enemy A: Far but perfectly aligned (500 units ahead)
	AEnemyCharacter* EnemyFar = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(500.0f, 0.0f, 0.0f));
	// Enemy B: Close but slightly off-axis (200 units ahead, 100 right - ~27° angle)
	AEnemyCharacter* EnemyClose = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 100.0f, 0.0f));

	if (!TestNotNull("Player should be created", Player))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingComponent* Targeting = Player->TargetingComponent;
	if (!TestNotNull("TargetingComponent should exist", Targeting))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Test with higher angle weight - should prefer perfectly aligned far enemy
	AActor* BestTarget = nullptr;
	FVector ForwardDirection = Player->GetActorForwardVector();
	Targeting->FindBestTargetForDirection(
		ForwardDirection,
		BestTarget,
		600.0f,  // MaxRange includes far enemy
		45.0f,
		120.0f,
		0.9f,    // High angle weight - prefer alignment
		0.1f     // Low distance weight
	);

	TestNotNull("Should find a target", BestTarget);
	if (BestTarget)
	{
		TestEqual("With high angle weight, should prefer aligned far enemy",
			BestTarget, Cast<AActor>(EnemyFar));
	}

	// Test with higher distance weight - should prefer close enemy
	BestTarget = nullptr;
	Targeting->FindBestTargetForDirection(
		ForwardDirection,
		BestTarget,
		600.0f,
		45.0f,
		120.0f,
		0.1f,    // Low angle weight
		0.9f     // High distance weight - prefer closeness
	);

	TestNotNull("Should find a target", BestTarget);
	if (BestTarget)
	{
		TestEqual("With high distance weight, should prefer close enemy",
			BestTarget, Cast<AActor>(EnemyClose));
	}

	// Cleanup
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: FindBestTargetForDirection - No Targets Returns Input Direction
 * Verifies system returns input direction rotation when no targets found
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetingNoTargetReturnsInputDirTest, "KatanaCombat.Targeting.NoTargetReturnsInputDir", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTargetingNoTargetReturnsInputDirTest::RunTest(const FString& Parameters)
{
	// Setup: Player alone, no enemies
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ASamuraiCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);

	if (!TestNotNull("Player should be created", Player))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingComponent* Targeting = Player->TargetingComponent;
	if (!TestNotNull("TargetingComponent should exist", Targeting))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Test: Query with no enemies - should return input direction rotation
	AActor* BestTarget = nullptr;
	FVector InputDirection = FVector(1.0f, 1.0f, 0.0f).GetSafeNormal(); // 45° right-forward
	FRotator TargetRotation = Targeting->FindBestTargetForDirection(
		InputDirection,
		BestTarget,
		500.0f,
		45.0f,
		120.0f,
		0.7f,
		0.3f
	);

	TestNull("Should NOT find any target", BestTarget);

	// Rotation should be toward input direction
	FRotator ExpectedRotation = InputDirection.Rotation();
	TestTrue("Returned rotation should match input direction",
		TargetRotation.Equals(ExpectedRotation, 1.0f));

	// Cleanup
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: FindBestTargetForDirection - Out of Range
 * Verifies enemies beyond max range are ignored
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetingOutOfRangeTest, "KatanaCombat.Targeting.OutOfRange", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTargetingOutOfRangeTest::RunTest(const FString& Parameters)
{
	// Setup: Player at origin, enemy 600 units ahead (beyond 500 max range)
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ASamuraiCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(600.0f, 0.0f, 0.0f));

	if (!TestNotNull("Player should be created", Player) ||
		!TestNotNull("Enemy should be created", Enemy))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingComponent* Targeting = Player->TargetingComponent;
	if (!TestNotNull("TargetingComponent should exist", Targeting))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Test: Query with max range 500 - enemy at 600 should be ignored
	AActor* BestTarget = nullptr;
	FVector ForwardDirection = Player->GetActorForwardVector();
	Targeting->FindBestTargetForDirection(
		ForwardDirection,
		BestTarget,
		500.0f,  // MaxRange - enemy at 600 is out of range
		45.0f,
		120.0f,
		0.7f,
		0.3f
	);

	TestNull("Should NOT find enemy out of range", BestTarget);

	// Cleanup
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: GetAngleToTarget - Accuracy
 * Verifies angle calculation is accurate for various target positions
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetingAngleAccuracyTest, "KatanaCombat.Targeting.AngleAccuracy", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTargetingAngleAccuracyTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ASamuraiCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);

	if (!TestNotNull("Player should be created", Player))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingComponent* Targeting = Player->TargetingComponent;

	// Test various positions
	struct FAngleTestCase
	{
		FVector Position;
		float ExpectedAngle;
		FString Description;
	};

	TArray<FAngleTestCase> TestCases = {
		{ FVector(100.0f, 0.0f, 0.0f),   0.0f,   TEXT("Directly ahead") },
		{ FVector(0.0f, 100.0f, 0.0f),   90.0f,  TEXT("Directly right") },
		{ FVector(0.0f, -100.0f, 0.0f),  -90.0f, TEXT("Directly left") },
		{ FVector(-100.0f, 0.0f, 0.0f),  180.0f, TEXT("Directly behind") },
		{ FVector(100.0f, 100.0f, 0.0f), 45.0f,  TEXT("45 degrees right") },
	};

	for (const FAngleTestCase& TestCase : TestCases)
	{
		AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, TestCase.Position);
		float Angle = Targeting->GetAngleToTarget(Enemy);

		TestTrue(FString::Printf(TEXT("%s: expected %.1f, got %.1f"),
			*TestCase.Description, TestCase.ExpectedAngle, Angle),
			FMath::IsNearlyEqual(FMath::Abs(Angle), FMath::Abs(TestCase.ExpectedAngle), 5.0f));

		World->DestroyActor(Enemy);
	}

	// Cleanup
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: GetDistanceToTarget - Accuracy
 * Verifies distance calculation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetingDistanceAccuracyTest, "KatanaCombat.Targeting.DistanceAccuracy", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTargetingDistanceAccuracyTest::RunTest(const FString& Parameters)
{
	// Setup
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ASamuraiCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(300.0f, 400.0f, 0.0f));

	if (!TestNotNull("Player should be created", Player) ||
		!TestNotNull("Enemy should be created", Enemy))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingComponent* Targeting = Player->TargetingComponent;

	// Distance should be sqrt(300^2 + 400^2) = 500
	float Distance = Targeting->GetDistanceToTarget(Enemy);
	TestTrue("Distance should be 500 (3-4-5 triangle)",
		FMath::IsNearlyEqual(Distance, 500.0f, 1.0f));

	// Cleanup
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
