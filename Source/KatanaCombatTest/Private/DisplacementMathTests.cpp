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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMathStepSpeedTest, "KatanaCombat.Displacement.Math.StepSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMathStepSpeedTest::RunTest(const FString&)
{
	TestEqual(TEXT("Linear speed is constant"), DisplacementMath::StepSpeed(EDisplacementSpeedProfile::Linear, 50.0, 0.25, 0.1, 1.0 / 60), 200.0, 1e-6);
	// Stepping at StepSpeed for each step lands exactly on the curve, including a final step that crosses the end.
	double Covered = 0.0;
	for (double T = 0.0; T < 0.25; T += 1.0 / 60)
	{
		Covered += DisplacementMath::StepSpeed(EDisplacementSpeedProfile::EaseOut, 60.0, 0.25, T, 1.0 / 60) / 60;
	}
	TestEqual(TEXT("EaseOut steps sum to the distance"), Covered, 60.0, 1e-6);
	TestTrue(TEXT("A zero step is finite"), FMath::IsFinite(DisplacementMath::StepSpeed(EDisplacementSpeedProfile::EaseOut, 60.0, 0.25, 0.1, 0.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMathChannelTest, "KatanaCombat.Displacement.Math.ChannelSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMathChannelTest::RunTest(const FString&)
{
	TestEqual(TEXT("Root-motion montage with warping uses the animation channel"), DisplacementMath::SelectChannel(true, true), EDisplacementChannel::Animation);
	TestEqual(TEXT("No root motion uses movement"), DisplacementMath::SelectChannel(false, true), EDisplacementChannel::Movement);
	TestEqual(TEXT("Root motion without warping cannot modify the animation"), DisplacementMath::SelectChannel(true, false), EDisplacementChannel::Movement);
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
