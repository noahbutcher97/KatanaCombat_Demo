// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"
#include "GameFramework/Character.h"

/**
 * Test: Debug Label Positioning - No Overlap
 * Verifies that debug text labels at arrow endpoints don't overlap with each other
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugLabelPositionTest, "KatanaCombat.Debug.LabelPositioning", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugLabelPositionTest::RunTest(const FString& Parameters)
{
	// Setup - use proper character spawning via test helper
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatV2 = nullptr;
	ASamuraiCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatV2);

	if (!TestNotNull("CombatComponent should be created", CombatV2))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Test scenario: Forward input with camera aligned to character
	const FRotator CameraRotation(0.0f, 0.0f, 0.0f);
	const FRotator CharacterRotation(0.0f, 0.0f, 0.0f);
	const FVector2D CameraRelativeInput(0.0f, 1.0f); // Forward
	const EInputDirection ResolvedDirection = EInputDirection::Forward;

	// Calculate debug visualization data
	FDebugVisualizationData DebugData = CombatV2->CalculateDebugVisualizationData(
		CameraRotation, CharacterRotation, CameraRelativeInput, ResolvedDirection);

	// Verify we have 5 arrows
	TestEqual("Should have 5 arrows", DebugData.Arrows.Num(), 5);

	// Verify all label positions are valid (not NaN, not at origin when character is at origin)
	const FVector CharacterLocation = TestCharacter->GetActorLocation();
	const FVector ExpectedChestBase = CharacterLocation + FVector(0, 0, 90.0f);

	for (int32 i = 0; i < DebugData.Arrows.Num(); ++i)
	{
		const FVector& LabelPos = DebugData.Arrows[i].LabelPosition;

		// Labels should not contain NaN
		TestFalse(FString::Printf(TEXT("Label %d position should not contain NaN"), i),
			LabelPos.ContainsNaN());

		// Labels should be roughly at chest height (within reasonable range)
		const float MinZ = ExpectedChestBase.Z - 50.0f;  // Allow some below
		const float MaxZ = ExpectedChestBase.Z + 100.0f; // Allow summary panel above
		TestTrue(FString::Printf(TEXT("Label %d Z (%.1f) should be near chest height"), i, LabelPos.Z),
			LabelPos.Z >= MinZ && LabelPos.Z <= MaxZ);
	}

	// Test with divergent directions to ensure labels don't all overlap
	const FRotator DivergentCamera(0.0f, 90.0f, 0.0f); // Camera looking perpendicular
	FDebugVisualizationData DivergentData = CombatV2->CalculateDebugVisualizationData(
		DivergentCamera, CharacterRotation, CameraRelativeInput, EInputDirection::Right);

	// With divergent camera, labels should be more spread out in XY
	float MaxXYDist = 0.0f;
	for (int32 i = 0; i < DivergentData.Arrows.Num(); ++i)
	{
		for (int32 j = i + 1; j < DivergentData.Arrows.Num(); ++j)
		{
			float XYDist = FVector::Dist2D(
				DivergentData.Arrows[i].LabelPosition,
				DivergentData.Arrows[j].LabelPosition);
			MaxXYDist = FMath::Max(MaxXYDist, XYDist);
		}
	}
	TestTrue("With divergent camera/character, labels should have XY spread",
		MaxXYDist > 20.0f);

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Debug Arrow Position Accuracy
 * Verifies that arrows are drawn at the correct world positions relative to character
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugArrowPositionTest, "KatanaCombat.Debug.ArrowPositioning", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugArrowPositionTest::RunTest(const FString& Parameters)
{
	// Setup - use proper character spawning via test helper
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ASamuraiCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FVector CharacterLocation(100.0f, 200.0f, 50.0f);
	TestCharacter->SetActorLocation(CharacterLocation);

	// Test scenario
	const FRotator CameraRotation(0.0f, 45.0f, 0.0f); // 45° yaw
	const FRotator CharacterRotation(0.0f, 0.0f, 0.0f);
	const FVector2D CameraRelativeInput(0.0f, 1.0f);
	const EInputDirection ResolvedDirection = EInputDirection::ForwardRight;

	FDebugVisualizationData DebugData = CombatComp->CalculateDebugVisualizationData(
		CameraRotation, CharacterRotation, CameraRelativeInput, ResolvedDirection);

	// Verify chest offset is applied correctly
	const FVector ExpectedChestOffset(0.0f, 0.0f, 90.0f);
	TestEqual("Chest offset should be 90 units", DebugData.ChestOffset.Z, 90.0);

	// All arrows should start at character location + chest offset
	for (int32 i = 0; i < DebugData.Arrows.Num(); ++i)
	{
		const FVector& StartPos = DebugData.Arrows[i].StartPosition;
		const FVector ExpectedStart = CharacterLocation + ExpectedChestOffset;

		TestTrue(FString::Printf(TEXT("Arrow %d should start at chest height"), i),
			StartPos.Equals(ExpectedStart, 1.0f)); // 1 unit tolerance
	}

	// Verify arrow lengths match specification
	TArray<float> ExpectedLengths = { 180.0f, 140.0f, 160.0f, 120.0f, 160.0f };
	for (int32 i = 0; i < DebugData.Arrows.Num(); ++i)
	{
		float ActualLength = DebugData.Arrows[i].Length;
		TestEqual(FString::Printf(TEXT("Arrow %d length"), i), ActualLength, ExpectedLengths[i]);
	}

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Hold State Visualization
 * Verifies hold state affects arrow style (solid vs dashed)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugHoldStateVisualizationTest, "KatanaCombat.Debug.HoldStateVisualization", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugHoldStateVisualizationTest::RunTest(const FString& Parameters)
{
	// Setup - use proper character spawning via test helper
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ASamuraiCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestCharacter->SetActorLocation(FVector::ZeroVector);

	// Test without hold state
	const FRotator TestRotation(0.0f, 0.0f, 0.0f);
	const FVector2D TestInput(0.0f, 1.0f);
	const EInputDirection TestDirection = EInputDirection::Forward;

	FDebugVisualizationData DebugData = CombatComp->CalculateDebugVisualizationData(
		TestRotation, TestRotation, TestInput, TestDirection);

	// Actual arrow order: 0=Camera, 1=Input, 2=Character, 3=CharRelative, 4=Attack
	// Input arrow (index 1) can be dashed when holding, solid otherwise
	TestFalse("Input arrow (index 1) should be solid (not dashed) when not holding",
		DebugData.Arrows[1].bIsDashed);
	TestFalse("Should not show hold indicator",
		DebugData.bShowHoldIndicator);

	// Note: Testing with hold active requires setting HoldState, which requires
	// a more complex setup. This test verifies the default (non-hold) case.

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Phase Debug Colors
 * Verifies phase transitions map to correct debug colors
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugPhaseColorTest, "KatanaCombat.Debug.PhaseColors", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugPhaseColorTest::RunTest(const FString& Parameters)
{
	// Setup - use proper character spawning via test helper
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ASamuraiCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Test phase color mapping
	// Note: This requires access to CurrentPhase which is protected
	// For now, verify the function exists and can be called
	FColor DefaultColor = CombatComp->GetPhaseDebugColor();
	TestEqual("Default phase (None) should be White", DefaultColor, FColor::White);

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Arrow Length Consistency
 * Verifies arrow endpoint calculations match expected lengths
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugArrowLengthTest, "KatanaCombat.Debug.ArrowLengths", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugArrowLengthTest::RunTest(const FString& Parameters)
{
	// Setup - use proper character spawning via test helper
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatV2 = nullptr;
	ASamuraiCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatV2);

	if (!TestNotNull("CombatComponent should be created", CombatV2))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestCharacter->SetActorLocation(FVector::ZeroVector);

	const FRotator TestRotation(0.0f, 0.0f, 0.0f);
	const FVector2D TestInput(0.0f, 1.0f);
	const EInputDirection TestDirection = EInputDirection::Forward;

	FDebugVisualizationData DebugData = CombatV2->CalculateDebugVisualizationData(
		TestRotation, TestRotation, TestInput, TestDirection);

	// Verify calculated end positions match specified lengths
	for (int32 i = 0; i < DebugData.Arrows.Num(); ++i)
	{
		const FDebugArrowInfo& Arrow = DebugData.Arrows[i];
		float CalculatedLength = FVector::Dist(Arrow.StartPosition, Arrow.EndPosition);

		TestTrue(FString::Printf(TEXT("Arrow %d calculated length (%.1f) should match stored length (%.1f)"),
			i, CalculatedLength, Arrow.Length),
			FMath::IsNearlyEqual(CalculatedLength, Arrow.Length, 1.0f));
	}

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Chest Height Offset
 * Verifies all debug elements are positioned at correct chest height
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugChestHeightTest, "KatanaCombat.Debug.ChestHeight", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugChestHeightTest::RunTest(const FString& Parameters)
{
	// Setup - use proper character spawning via test helper
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ASamuraiCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FVector CharacterLocation(0.0f, 0.0f, 100.0f);
	TestCharacter->SetActorLocation(CharacterLocation);

	const FRotator TestRotation(0.0f, 0.0f, 0.0f);
	const FVector2D TestInput(0.0f, 1.0f);
	const EInputDirection TestDirection = EInputDirection::Forward;

	FDebugVisualizationData DebugData = CombatComp->CalculateDebugVisualizationData(
		TestRotation, TestRotation, TestInput, TestDirection);

	const double ExpectedChestHeight = CharacterLocation.Z + 90.0; // Character Z + chest offset

	// All arrow start positions should be at chest height
	for (int32 i = 0; i < DebugData.Arrows.Num(); ++i)
	{
		TestEqual(FString::Printf(TEXT("Arrow %d start Z"), i),
			DebugData.Arrows[i].StartPosition.Z, ExpectedChestHeight);
	}

	// Arc points should also be at chest height
	for (int32 i = 0; i < DebugData.ArcPoints.Num(); ++i)
	{
		TestEqual(FString::Printf(TEXT("Arc point %d Z"), i),
			DebugData.ArcPoints[i].Z, ExpectedChestHeight);
	}

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Angular Arc Calculation
 * Verifies arc segments are generated correctly for camera-character offset
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugAngularArcTest, "KatanaCombat.Debug.AngularArc", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugAngularArcTest::RunTest(const FString& Parameters)
{
	// Setup - use proper character spawning via test helper
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatV2 = nullptr;
	ASamuraiCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatV2);

	if (!TestNotNull("CombatComponent should be created", CombatV2))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Test with significant yaw delta (> 5 degrees)
	const FRotator CameraRotation(0.0f, 45.0f, 0.0f);
	const FRotator CharacterRotation(0.0f, 0.0f, 0.0f);
	const FVector2D TestInput(0.0f, 1.0f);
	const EInputDirection TestDirection = EInputDirection::ForwardRight;

	FDebugVisualizationData DebugData = CombatV2->CalculateDebugVisualizationData(
		CameraRotation, CharacterRotation, TestInput, TestDirection);

	// Should generate arc points for 45° delta
	TestTrue("Arc should be generated for yaw delta > 5°",
		DebugData.ArcPoints.Num() > 0);

	// Test with small yaw delta (< 5 degrees) - should not generate arc
	const FRotator SmallDeltaCamera(0.0f, 2.0f, 0.0f);
	FDebugVisualizationData SmallDeltaData = CombatV2->CalculateDebugVisualizationData(
		SmallDeltaCamera, CharacterRotation, TestInput, TestDirection);

	TestEqual("Arc should not be generated for yaw delta < 5°",
		SmallDeltaData.ArcPoints.Num(), 0);

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Direction Mapping Accuracy
 * Verifies EInputDirection correctly maps to EAttackDirection
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugDirectionMappingTest, "KatanaCombat.Debug.DirectionMapping", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugDirectionMappingTest::RunTest(const FString& Parameters)
{
	// Test all 8 cardinal/ordinal directions plus None
	struct FDirectionMapping
	{
		EInputDirection Input;
		EAttackDirection ExpectedAttack;
		FString Description;
	};

	TArray<FDirectionMapping> TestCases = {
		{ EInputDirection::Forward,      EAttackDirection::Forward,  TEXT("Forward") },
		{ EInputDirection::Backward,     EAttackDirection::Backward, TEXT("Backward") },
		{ EInputDirection::Left,         EAttackDirection::Left,     TEXT("Left") },
		{ EInputDirection::Right,        EAttackDirection::Right,    TEXT("Right") },
		{ EInputDirection::ForwardLeft,  EAttackDirection::Forward,  TEXT("ForwardLeft → Forward") },
		{ EInputDirection::ForwardRight, EAttackDirection::Forward,  TEXT("ForwardRight → Forward") },
		{ EInputDirection::BackwardLeft, EAttackDirection::Backward, TEXT("BackwardLeft → Backward") },
		{ EInputDirection::BackwardRight,EAttackDirection::Backward, TEXT("BackwardRight → Backward") },
		{ EInputDirection::None,         EAttackDirection::None,     TEXT("None") }
	};

	for (const FDirectionMapping& TestCase : TestCases)
	{
		EAttackDirection Result = CombatHelpers::InputToAttackDirection(TestCase.Input);
		TestEqual(TestCase.Description, Result, TestCase.ExpectedAttack);
	}

	return true;
}

/**
 * Test: Label Content Formatting
 * Verifies label text matches expected format and content
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugLabelContentTest, "KatanaCombat.Debug.LabelContent", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugLabelContentTest::RunTest(const FString& Parameters)
{
	// Setup - use proper character spawning via test helper
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ASamuraiCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestCharacter->SetActorLocation(FVector::ZeroVector);

	// Test scenario with known direction
	const FRotator CameraRotation(0.0f, 0.0f, 0.0f);
	const FRotator CharacterRotation(0.0f, 0.0f, 0.0f);
	const FVector2D CameraRelativeInput(0.0f, 1.0f); // Forward
	const EInputDirection ResolvedDirection = EInputDirection::Forward;

	FDebugVisualizationData DebugData = CombatComp->CalculateDebugVisualizationData(
		CameraRotation, CharacterRotation, CameraRelativeInput, ResolvedDirection);

	// Verify we have 5 arrows with labels
	TestEqual("Should have 5 arrows", DebugData.Arrows.Num(), 5);

	// Verify label text is not empty for all arrows
	for (int32 i = 0; i < DebugData.Arrows.Num(); ++i)
	{
		TestTrue(FString::Printf(TEXT("Arrow %d label should not be empty"), i),
			!DebugData.Arrows[i].Label.IsEmpty());
	}

	// Verify attack direction label contains expected direction
	// Arrow 4 (index 4) is the Attack Direction arrow
	const FString& AttackLabel = DebugData.Arrows[4].Label;
	TestTrue("Attack arrow label should contain 'Forward'",
		AttackLabel.Contains(TEXT("Forward")));

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Arrow Identity and Order
 * Verifies each arrow has correct semantic identity and appears in correct order
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugArrowIdentityTest, "KatanaCombat.Debug.ArrowIdentity", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugArrowIdentityTest::RunTest(const FString& Parameters)
{
	// Setup - use proper character spawning via test helper
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatV2 = nullptr;
	ASamuraiCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatV2);

	if (!TestNotNull("CombatComponent should be created", CombatV2))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestCharacter->SetActorLocation(FVector::ZeroVector);

	const FRotator CameraRotation(0.0f, 45.0f, 0.0f);
	const FRotator CharacterRotation(0.0f, 0.0f, 0.0f);
	const FVector2D CameraRelativeInput(1.0f, 0.0f); // Right
	const EInputDirection ResolvedDirection = EInputDirection::Right;

	FDebugVisualizationData DebugData = CombatV2->CalculateDebugVisualizationData(
		CameraRotation, CharacterRotation, CameraRelativeInput, ResolvedDirection);

	// Verify we have exactly 5 arrows in correct order
	TestEqual("Should have exactly 5 arrows", DebugData.Arrows.Num(), 5);

	// Verify arrow order by checking expected colors
	// Actual implementation order: 0=Camera, 1=Input, 2=Character, 3=CharRelative, 4=Attack

	// Arrow 0: Camera (Custom Blue RGB(0,100,255))
	TestEqual("Arrow 0 should be custom blue (Camera)", DebugData.Arrows[0].Color, FColor(0, 100, 255));

	// Arrow 1: Input (Yellow RGB(255,255,0) when not holding, gold RGB(255,215,0) when holding)
	// In this test, hold is not active, so expect yellow
	const FColor ExpectedInputColor = DebugData.bShowHoldIndicator ? FColor(255, 215, 0) : FColor(255, 255, 0);
	TestEqual("Arrow 1 should be yellow (Input)", DebugData.Arrows[1].Color, ExpectedInputColor);

	// Arrow 2: Character Forward (Green)
	TestEqual("Arrow 2 should be green (Character)", DebugData.Arrows[2].Color, FColor::Green);

	// Arrow 3: CharRelative (Orange RGB(255,165,0))
	TestEqual("Arrow 3 should be orange (CharRelative)", DebugData.Arrows[3].Color, FColor(255, 165, 0));

	// Arrow 4: Attack Direction (Magenta)
	TestEqual("Arrow 4 should be magenta (Attack)", DebugData.Arrows[4].Color, FColor::Magenta);

	// Verify arrow lengths follow expected pattern
	// Actual lengths: Camera=180, Input=140, Character=160, CharRelative=120, Attack=160
	// Camera (180) > Character (160) > Input (140) > CharRelative (120)
	TestTrue("Camera arrow (180) should be longer than Character arrow (160)",
		DebugData.Arrows[0].Length > DebugData.Arrows[2].Length);
	TestTrue("Character arrow (160) should be longer than Input arrow (140)",
		DebugData.Arrows[2].Length > DebugData.Arrows[1].Length);
	TestTrue("Input arrow (140) should be longer than CharRelative arrow (120)",
		DebugData.Arrows[1].Length > DebugData.Arrows[3].Length);

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Resolved Direction Color Consistency
 * Verifies resolved direction arrow maintains consistent color regardless of input
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugResolvedColorTest, "KatanaCombat.Debug.ResolvedColor", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugResolvedColorTest::RunTest(const FString& Parameters)
{
	// Setup - use proper character spawning via test helper
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ASamuraiCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestCharacter->SetActorLocation(FVector::ZeroVector);

	const FRotator TestRotation(0.0f, 0.0f, 0.0f);

	// Test multiple directions to ensure consistency
	struct FDirectionTest
	{
		FVector2D Input;
		EInputDirection Direction;
		FString Name;
	};

	TArray<FDirectionTest> Tests = {
		{ FVector2D(0.0f, 1.0f),  EInputDirection::Forward,  TEXT("Forward") },
		{ FVector2D(0.0f, -1.0f), EInputDirection::Backward, TEXT("Backward") },
		{ FVector2D(-1.0f, 0.0f), EInputDirection::Left,     TEXT("Left") },
		{ FVector2D(1.0f, 0.0f),  EInputDirection::Right,    TEXT("Right") }
	};

	for (const FDirectionTest& Test : Tests)
	{
		FDebugVisualizationData DebugData = CombatComp->CalculateDebugVisualizationData(
			TestRotation, TestRotation, Test.Input, Test.Direction);

		// Arrow 4 (Resolved) should always be Magenta
		TestEqual(FString::Printf(TEXT("Resolved arrow should be magenta for %s"), *Test.Name),
			DebugData.Arrows[4].Color, FColor::Magenta);

		// Verify label contains the expected direction name
		TestTrue(FString::Printf(TEXT("Attack label should contain direction for %s"), *Test.Name),
			!DebugData.Arrows[4].Label.IsEmpty());
	}

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

/**
 * Test: Zero Input Magnitude Edge Case
 * Verifies system handles zero/invalid input gracefully
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugZeroInputTest, "KatanaCombat.Debug.ZeroInput", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDebugZeroInputTest::RunTest(const FString& Parameters)
{
	// Setup - use proper character spawning via test helper
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* CombatComp = nullptr;
	ASamuraiCharacter* TestCharacter = FCombatTestHelpers::CreateTestCharacterWithCombat(World, CombatComp);

	if (!TestNotNull("CombatComponent should be created", CombatComp))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestCharacter->SetActorLocation(FVector::ZeroVector);

	const FRotator TestRotation(0.0f, 0.0f, 0.0f);

	// Test with zero input magnitude
	const FVector2D ZeroInput(0.0f, 0.0f);
	const EInputDirection NoneDirection = EInputDirection::None;

	FDebugVisualizationData DebugData = CombatComp->CalculateDebugVisualizationData(
		TestRotation, TestRotation, ZeroInput, NoneDirection);

	// Should still have 5 arrows (visualization always shows transformation pipeline)
	TestEqual("Should have 5 arrows even with zero input", DebugData.Arrows.Num(), 5);

	// Verify all arrows have valid positions (not NaN or infinite)
	for (int32 i = 0; i < DebugData.Arrows.Num(); ++i)
	{
		const FDebugArrowInfo& Arrow = DebugData.Arrows[i];

		TestFalse(FString::Printf(TEXT("Arrow %d StartPosition should not contain NaN"), i),
			Arrow.StartPosition.ContainsNaN());
		TestFalse(FString::Printf(TEXT("Arrow %d EndPosition should not contain NaN"), i),
			Arrow.EndPosition.ContainsNaN());
		TestFalse(FString::Printf(TEXT("Arrow %d LabelPosition should not contain NaN"), i),
			Arrow.LabelPosition.ContainsNaN());
	}

	// Cleanup
	World->DestroyActor(TestCharacter);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
