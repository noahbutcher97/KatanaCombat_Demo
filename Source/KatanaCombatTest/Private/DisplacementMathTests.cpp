#include "Misc/AutomationTest.h"
#include "Utilities/DisplacementMath.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMathProgressTest, "KatanaCombat.Displacement.Math.Progress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMathProgressTest::RunTest(const FString&)
{
	TestEqual(TEXT("Linear midpoint"), DisplacementMath::Progress(EDisplacementSpeedProfile::Linear, 0.5), 0.5, 1e-9);
	TestEqual(TEXT("EaseOut midpoint is three quarters"), DisplacementMath::Progress(EDisplacementSpeedProfile::EaseOut, 0.5), 0.75, 1e-9);
	TestEqual(TEXT("Clamped below"), DisplacementMath::Progress(EDisplacementSpeedProfile::EaseOut, -1.0), 0.0, 1e-9);
	TestEqual(TEXT("Clamped above"), DisplacementMath::Progress(EDisplacementSpeedProfile::Linear, 2.0), 1.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMathPartitionTest, "KatanaCombat.Displacement.Math.PartitionsSumToDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMathPartitionTest::RunTest(const FString&)
{
	for (const EDisplacementSpeedProfile Profile : {EDisplacementSpeedProfile::Linear, EDisplacementSpeedProfile::EaseOut})
	{
		double Sum = 0.0;
		double T = 0.0;
		for (const double Step : {0.013, 0.05, 0.001, 0.07, 0.2})
		{
			Sum += DisplacementMath::DistanceBetween(Profile, 60.0, 0.25, T, T + Step);
			T += Step;
		}
		TestEqual(TEXT("Uneven partitions sum to the distance"), Sum, 60.0, 1e-6);
	}
	TestTrue(TEXT("EaseOut covers more than half in the first half"),
		DisplacementMath::DistanceBetween(EDisplacementSpeedProfile::EaseOut, 60.0, 0.25, 0.0, 0.125) > 30.0);
	TestEqual(TEXT("Zero duration is a jump at the start"),
		DisplacementMath::DistanceBetween(EDisplacementSpeedProfile::Linear, 10.0, 0.0, 0.0, 0.1), 10.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMathChannelTest, "KatanaCombat.Displacement.Math.ChannelSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMathChannelTest::RunTest(const FString&)
{
	TestEqual(TEXT("Advancing root-motion montage with warping uses the animation channel"), DisplacementMath::SelectChannel(true, true, true), EDisplacementChannel::Animation);
	TestEqual(TEXT("No root motion uses movement"), DisplacementMath::SelectChannel(false, true, false), EDisplacementChannel::Movement);
	TestEqual(TEXT("Root motion without warping cannot modify the animation"), DisplacementMath::SelectChannel(true, false, true), EDisplacementChannel::Movement);
	TestEqual(TEXT("A montage that is not advancing (holding its last pose with auto blend-out disabled, or paused) extracts no root motion, so movement carries the push"),
		DisplacementMath::SelectChannel(true, true, false), EDisplacementChannel::Movement);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMathPushStepTest, "KatanaCombat.Displacement.Math.PushStepExcludesKeptAnimation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMathPushStepTest::RunTest(const FString&)
{
	// Finite first: this build compares NaN as equal to anything.
	const auto Is = [this](const TCHAR* What, const double Actual, const double Expected)
	{
		TestTrue(FString::Printf(TEXT("%s (%f, expected %f)"), What, Actual, Expected),
			FMath::IsFinite(Actual) && FMath::Abs(Actual - Expected) <= 1e-9);
	};
	// In every case the push commanded 4 cm over the step, so the cap (pinned below) never decides these.
	const double Commanded = 4.0;
	Is(TEXT("Movement channel: the step is the push"), DisplacementMath::PushStep(4.0, 0.0, Commanded), 4.0);
	Is(TEXT("Animation channel: the kept animation travel is not push"), DisplacementMath::PushStep(10.0, 6.0, Commanded), 4.0);
	Is(TEXT("A reaction stepping back against the push still leaves the push"), DisplacementMath::PushStep(-2.0, -6.0, Commanded), 4.0);
	Is(TEXT("A wall the kept animation pressed into: no push"), DisplacementMath::PushStep(0.0, 3.0, Commanded), 0.0);
	Is(TEXT("A wall the kept animation stepped away from: no push"), DisplacementMath::PushStep(-3.0, -3.0, Commanded), 0.0);
	Is(TEXT("Backward movement is not push travel"), DisplacementMath::PushStep(-1.0, 0.0, Commanded), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMathPushStepCapTest, "KatanaCombat.Displacement.Math.PushStepCappedAtCommandedStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMathPushStepCapTest::RunTest(const FString&)
{
	// Finite first: this build compares NaN as equal to anything.
	const auto Is = [this](const TCHAR* What, const double Actual, const double Expected)
	{
		TestTrue(FString::Printf(TEXT("%s (%f, expected %f)"), What, Actual, Expected),
			FMath::IsFinite(Actual) && FMath::Abs(Actual - Expected) <= 1e-9);
	};
	// Collision that clips the step: the animation's kept travel is -3 cm and the push commanded +1 cm, so the free step
	// is -2 cm; a wall behind the character holds it at 0. Progress less kept travel reads 3 cm, but the push only
	// commanded 1 cm.
	Is(TEXT("A wall clipping a backward reaction: the push's own 1 cm, not the 3 cm the animation lost"),
		DisplacementMath::PushStep(0.0, -3.0, 1.0), 1.0);
	Is(TEXT("Any clipped backward reaction gives at most the commanded step"), DisplacementMath::PushStep(0.0, -10.0, 1.0), 1.0);
	Is(TEXT("A step short of its commanded distance passes through"), DisplacementMath::PushStep(2.5, 0.0, 4.0), 2.5);
	Is(TEXT("Movement with no push time behind it is not push"), DisplacementMath::PushStep(5.0, 0.0, 0.0), 0.0);
	Is(TEXT("A negative commanded step counts as none"), DisplacementMath::PushStep(3.0, 0.0, -1.0), 0.0);
	Is(TEXT("Still never negative under the cap"), DisplacementMath::PushStep(-2.0, 0.0, 4.0), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMathValidityTest, "KatanaCombat.Displacement.Math.Validity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMathValidityTest::RunTest(const FString&)
{
	FProceduralDisplacement Valid;
	Valid.Direction = FVector(1, 0, 0); Valid.Distance = 25.f; Valid.Duration = 0.2f;
	TestTrue(TEXT("Horizontal unit direction is valid"), DisplacementMath::IsValid(Valid));
	FProceduralDisplacement Tilted = Valid; Tilted.Direction = FVector(0.8, 0, 0.6);
	TestFalse(TEXT("Vertical component is rejected"), DisplacementMath::IsValid(Tilted));
	FProceduralDisplacement Zero = Valid; Zero.Distance = 0.f;
	TestFalse(TEXT("Zero distance is rejected"), DisplacementMath::IsValid(Zero));
	FProceduralDisplacement World = Valid; World.Clock = EDisplacementClock::WorldTime;
	TestFalse(TEXT("WorldTime is not supported until the paired entry step"), DisplacementMath::IsValid(World));
	FProceduralDisplacement NaN = Valid; NaN.Duration = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite duration is rejected"), DisplacementMath::IsValid(NaN));
	return true;
}
