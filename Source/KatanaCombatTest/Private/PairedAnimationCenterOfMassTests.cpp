#include "Misc/AutomationTest.h"
#include "PairedAnimationAnalysisLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedCenterOfMassWeightingTest, "KatanaCombat.Editor.PairedEvaluation.CenterOfMassWeighting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedCenterOfMassWeightingTest::RunTest(const FString&)
{
	const TArray<FVector> Locations = { FVector(0, 0, 0), FVector(100, 0, 0) };
	TestTrue(TEXT("Equal weights average"),
		UPairedAnimationAnalysisLibrary::CalculateWeightedCenter(Locations, { 1.0f, 1.0f }).Equals(FVector(50, 0, 0), 1e-4));
	TestTrue(TEXT("Heavier location pulls the center"),
		UPairedAnimationAnalysisLibrary::CalculateWeightedCenter(Locations, { 3.0f, 1.0f }).Equals(FVector(25, 0, 0), 1e-4));
	TestTrue(TEXT("Missing weights count as 1"),
		UPairedAnimationAnalysisLibrary::CalculateWeightedCenter(Locations, {}).Equals(FVector(50, 0, 0), 1e-4));
	TestTrue(TEXT("Nonpositive total weight returns zero"),
		UPairedAnimationAnalysisLibrary::CalculateWeightedCenter(Locations, { 0.0f, 0.0f }).IsZero());
	TestTrue(TEXT("No locations returns zero"),
		UPairedAnimationAnalysisLibrary::CalculateWeightedCenter({}, {}).IsZero());

	TArray<FName> Bones;
	TArray<float> Weights;
	UPairedAnimationAnalysisLibrary::GetHumanoidMassWeights(Bones, Weights);
	TestEqual(TEXT("One weight per segment bone"), Bones.Num(), Weights.Num());
	TestTrue(TEXT("Covers the pelvis and both feet"),
		Bones.Contains(FName(TEXT("pelvis"))) && Bones.Contains(FName(TEXT("foot_l"))) && Bones.Contains(FName(TEXT("foot_r"))));
	float Total = 0.0f;
	for (const float Weight : Weights)
	{
		Total += Weight;
	}
	TestEqual(TEXT("Segment mass fractions sum to about one body"), Total, 1.01f, 0.02f);

	// Coverage gate: a skeleton sharing only one common name must not use the segment path.
	FVector Center;
	TestFalse(TEXT("No matched segments fails the coverage gate"),
		UPairedAnimationAnalysisLibrary::CalculateSegmentCenterOfMass({}, {}, Total, 0.8f, Center));
	TArray<FVector> AllLocations;
	for (int32 Index = 0; Index < Bones.Num(); ++Index)
	{
		AllLocations.Add(FVector(Index * 10.0f, 0.0f, 0.0f));
	}
	TestTrue(TEXT("All segments present passes the coverage gate"),
		UPairedAnimationAnalysisLibrary::CalculateSegmentCenterOfMass(AllLocations, Weights, Total, 0.8f, Center));
	TestTrue(TEXT("Passing result equals the weighted center"),
		Center.Equals(UPairedAnimationAnalysisLibrary::CalculateWeightedCenter(AllLocations, Weights), 1e-3));

	const int32 PelvisIndex = Bones.IndexOfByKey(FName(TEXT("pelvis")));
	TestFalse(TEXT("Only the pelvis (10% of mass) fails the coverage gate"),
		UPairedAnimationAnalysisLibrary::CalculateSegmentCenterOfMass(
			{ FVector(1, 2, 3) }, { Weights[PelvisIndex] }, Total, 0.8f, Center));
	TestTrue(TEXT("Failed gate leaves the center at zero"), Center.IsZero());
	return true;
}
