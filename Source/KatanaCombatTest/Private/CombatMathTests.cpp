// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Utilities/CombatMath.h"
#include "Core/TargetingComponent.h"
#include "Core/HitReactionComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Characters/EnemyCharacter.h"

// ============================================================================
// Pure CombatMath contract
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMathAngleBetweenTest, "KatanaCombat.CombatMath.AngleBetween", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatMathAngleBetweenTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Parallel"), CombatMath::AngleBetweenDegrees(FVector(1, 0, 0), FVector(5, 0, 0)), 0.0, 1e-6);
	TestEqual(TEXT("Perpendicular"), CombatMath::AngleBetweenDegrees(FVector(1, 0, 0), FVector(0, 3, 0)), 90.0, 1e-6);
	TestEqual(TEXT("Opposite"), CombatMath::AngleBetweenDegrees(FVector(1, 0, 0), FVector(-2, 0, 0)), 180.0, 1e-6);
	TestEqual(TEXT("Unnormalized inputs"), CombatMath::AngleBetweenDegrees(FVector(10, 10, 0), FVector(0, 0.5, 0)), 45.0, 1e-4);
	TestEqual(TEXT("Degenerate input reads as aligned"), CombatMath::AngleBetweenDegrees(FVector::ZeroVector, FVector(1, 0, 0)), 0.0, 1e-6);

	// Nearly-identical non-axis unit vectors: the raw dot can round past 1.
	const FVector A = FVector(0.3, 0.7, 0.2).GetSafeNormal();
	const double NearlySame = CombatMath::AngleBetweenDegrees(A, A * 1.0000001);
	TestFalse(TEXT("Never NaN for nearly identical directions"), FMath::IsNaN(NearlySame));
	TestEqual(TEXT("Nearly identical directions are ~0 degrees"), NearlySame, 0.0, 1e-3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMathConeTest, "KatanaCombat.CombatMath.IsWithinCone", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatMathConeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("On-axis is inside"), CombatMath::IsWithinCone(FVector(1, 0, 0), FVector(1, 0, 0), 30.0));
	TestTrue(TEXT("Unnormalized forward still works"), CombatMath::IsWithinCone(FVector(5, 0, 0), FVector(100, 20, 0), 30.0));
	TestTrue(TEXT("Boundary is inclusive"), CombatMath::IsWithinCone(FVector(1, 0, 0), FVector(1, 1, 0), 45.0 + 1e-6));
	TestFalse(TEXT("Outside the half angle"), CombatMath::IsWithinCone(FVector(1, 0, 0), FVector(0, 1, 0), 60.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMathSignedYawTest, "KatanaCombat.CombatMath.SignedYaw", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatMathSignedYawTest::RunTest(const FString& Parameters)
{
	const FVector Forward(1, 0, 0);
	TestEqual(TEXT("Ahead"), CombatMath::SignedYawDegrees(Forward, FVector(1, 0, 0)), 0.0, 1e-6);
	TestEqual(TEXT("Right is positive"), CombatMath::SignedYawDegrees(Forward, FVector(0, 1, 0)), 90.0, 1e-6);
	TestEqual(TEXT("Left is negative"), CombatMath::SignedYawDegrees(Forward, FVector(0, -1, 0)), -90.0, 1e-6);
	TestEqual(TEXT("Behind is +180"), CombatMath::SignedYawDegrees(Forward, FVector(-1, 0, 0)), 180.0, 1e-6);
	TestEqual(TEXT("Height is ignored"), CombatMath::SignedYawDegrees(Forward, FVector(1, 0, 500)), 0.0, 1e-6);
	TestEqual(TEXT("Rotated forward"), CombatMath::SignedYawDegrees(FVector(0, 1, 0), FVector(-1, 0, 0)), 90.0, 1e-6);
	TestEqual(TEXT("Bearing of world +Y"), CombatMath::BearingDegrees(FVector(0, 10, 0)), 90.0, 1e-6);
	TestEqual(TEXT("Degenerate reads as aligned"), CombatMath::SignedYawDegrees(Forward, FVector(0, 0, 1)), 0.0, 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMathFlatDirectionTest, "KatanaCombat.CombatMath.FlatDirection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatMathFlatDirectionTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Drops height and normalizes"),
		CombatMath::FlatDirection(FVector(0, 0, 0), FVector(10, 0, 300)).Equals(FVector(1, 0, 0), 1e-6));
	TestTrue(TEXT("Vertically stacked points have no flat direction"),
		CombatMath::FlatDirection(FVector(5, 5, 0), FVector(5, 5, 100)).IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMathClassifyTest, "KatanaCombat.CombatMath.Classify", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatMathClassifyTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Forward"), CombatMath::ClassifyLocalDirection(FVector(1, 0.2, 0)), EAttackDirection::Forward);
	TestEqual(TEXT("Backward"), CombatMath::ClassifyLocalDirection(FVector(-1, -0.2, 0)), EAttackDirection::Backward);
	TestEqual(TEXT("Right"), CombatMath::ClassifyLocalDirection(FVector(0.2, 1, 0)), EAttackDirection::Right);
	TestEqual(TEXT("Left"), CombatMath::ClassifyLocalDirection(FVector(0.2, -1, 0)), EAttackDirection::Left);
	TestEqual(TEXT("Height does not change the sector"), CombatMath::ClassifyLocalDirection(FVector(1, 0.2, 50)), EAttackDirection::Forward);
	TestEqual(TEXT("Degenerate falls back to Forward"), CombatMath::ClassifyLocalDirection(FVector::ZeroVector), EAttackDirection::Forward);

	// Facing rotated 90 degrees (looking down world +Y): world +Y is in front.
	const FTransform FacingRight(FRotator(0, 90, 0), FVector(100, 100, 0));
	TestEqual(TEXT("Relative to rotated facing: ahead"), CombatMath::ClassifyRelativeToFacing(FacingRight, FVector(0, 1, 0)), EAttackDirection::Forward);
	TestEqual(TEXT("Relative to rotated facing: world +X is on the left"), CombatMath::ClassifyRelativeToFacing(FacingRight, FVector(1, 0, 0)), EAttackDirection::Left);
	return true;
}

// ============================================================================
// Component regressions fixed by routing through CombatMath
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetingConeUnnormalizedDirectionTest, "KatanaCombat.Targeting.Cone.UnnormalizedDirection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTargetingConeUnnormalizedDirectionTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(300, 60, 0));
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Enemy"), Enemy))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingComponent* Targeting = Player->TargetingComponent;
	// Enemy is ~11 degrees off forward. Callers pass stick/camera vectors of any length.
	TestTrue(TEXT("Long direction vector still finds a target inside the cone"),
		Targeting->IsTargetInCone(Enemy, FVector(5, 0, 0), 30.0f));
	TestTrue(TEXT("Short direction vector still finds a target inside the cone"),
		Targeting->IsTargetInCone(Enemy, FVector(0.5, 0, 0), 30.0f));
	TestFalse(TEXT("Target outside a narrow cone is rejected"),
		Targeting->IsTargetInCone(Enemy, FVector(0, 1, 0), 30.0f));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetingSignedFlatAngleTest, "KatanaCombat.Targeting.Angle.SignedAndFlat", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTargetingSignedFlatAngleTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);
	AEnemyCharacter* Right = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(0, 200, 0));
	AEnemyCharacter* Left = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(0, -200, 0));
	AEnemyCharacter* Above = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200, 0, 200));
	if (!TestNotNull(TEXT("Player"), Player) || !Right || !Left || !Above)
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingComponent* Targeting = Player->TargetingComponent;
	TestEqual(TEXT("Right is +90"), Targeting->GetAngleToTarget(Right), 90.0f, 0.5f);
	TestEqual(TEXT("Left is -90"), Targeting->GetAngleToTarget(Left), -90.0f, 0.5f);
	// A target on a ledge straight ahead is ahead: height must not add yaw.
	TestEqual(TEXT("Elevated target straight ahead is 0"), Targeting->GetAngleToTarget(Above), 0.0f, 0.5f);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitReactionClassifyRotatedVictimTest, "KatanaCombat.HitReaction.Direction.RotatedVictim", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FHitReactionClassifyRotatedVictimTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Victim"), Victim))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Victim faces world +Y. Direction values point from the victim toward the attacker.
	Victim->SetActorRotation(FRotator(0, 90, 0));
	UHitReactionComponent* HitReaction = Victim->HitReactionComponent;
	TestEqual(TEXT("Attacker in front"), HitReaction->GetHitDirectionRelativeToFacing(FVector(0, 1, 0)), EAttackDirection::Forward);
	TestEqual(TEXT("Attacker behind"), HitReaction->GetHitDirectionRelativeToFacing(FVector(0, -1, 0)), EAttackDirection::Backward);
	TestEqual(TEXT("Attacker on the victim's right"), HitReaction->GetHitDirectionRelativeToFacing(FVector(-1, 0, 0)), EAttackDirection::Right);
	TestEqual(TEXT("Attacker on the victim's left"), HitReaction->GetHitDirectionRelativeToFacing(FVector(1, 0, 0)), EAttackDirection::Left);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
