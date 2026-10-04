#include "Misc/AutomationTest.h"
#include "Animation/RootMotionModifier_ProceduralDisplacement.h"
#include "CombatTestHelpers.h"
#include "MotionWarpingComponent.h"
#include "Components/SkeletalMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralDisplacementBlendTest, "KatanaCombat.Displacement.Modifier.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProceduralDisplacementBlendTest::RunTest(const FString&)
{
	const FTransform Authored(FRotator(0, 15, 0), FVector(-3, 1, 0));
	const FVector Push(5, 0, 0);
	const FTransform Added = URootMotionModifier_ProceduralDisplacement::ApplyDisplacement(Authored, Push, EDisplacementAnimationBlend::AddToAnimation);
	TestTrue(TEXT("Add keeps the authored step and adds the push"), Added.GetTranslation().Equals(FVector(2, 1, 0), 1e-4));
	TestTrue(TEXT("Add keeps the authored rotation"), Added.GetRotation().Equals(Authored.GetRotation(), 1e-4));
	const FTransform Replaced = URootMotionModifier_ProceduralDisplacement::ApplyDisplacement(Authored, Push, EDisplacementAnimationBlend::ReplaceAnimation);
	TestTrue(TEXT("Replace substitutes the translation"), Replaced.GetTranslation().Equals(Push, 1e-4));
	TestTrue(TEXT("Replace keeps the authored rotation"), Replaced.GetRotation().Equals(Authored.GetRotation(), 1e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralDisplacementElapsedTest, "KatanaCombat.Displacement.Modifier.ElapsedTracksDeltaSeconds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProceduralDisplacementElapsedTest::RunTest(const FString&)
{
	auto* Modifier = NewObject<URootMotionModifier_ProceduralDisplacement>();
	FProceduralDisplacement Displacement;
	Displacement.Direction = FVector(1, 0, 0); Displacement.Distance = 30.f; Displacement.Duration = 0.2f;
	Modifier->Configure(Displacement, 0.05);
	TestEqual(TEXT("Starts at the resumed elapsed time"), Modifier->GetElapsed(), 0.05, 1e-9);
	Modifier->ProcessRootMotion(FTransform::Identity, 0.0f);
	TestEqual(TEXT("A frozen frame (hitstop) does not advance"), Modifier->GetElapsed(), 0.05, 1e-9);
	Modifier->ProcessRootMotion(FTransform::Identity, 0.1f);
	TestEqual(TEXT("Advances by DeltaSeconds"), Modifier->GetElapsed(), 0.15, 1e-6);
	TestEqual(TEXT("No owner mesh means no measured animation travel"), Modifier->ConsumeAnimationTravel(), 0.0, 1e-9);
	Modifier->ProcessRootMotion(FTransform::Identity, 1.0f);
	TestTrue(TEXT("Clamped at the duration"), Modifier->IsComplete());
	const FTransform After = Modifier->ProcessRootMotion(FTransform(FVector(4, 0, 0)), 0.1f);
	TestTrue(TEXT("A complete modifier passes animation through"), After.GetTranslation().Equals(FVector(4, 0, 0), 1e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralDisplacementDirectionTest, "KatanaCombat.Displacement.Modifier.WorldDirectionOnCharacter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProceduralDisplacementDirectionTest::RunTest(const FString&)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector(0, 0, 100));
	Character->SetActorRotation(FRotator(0, 37, 0)); // the mesh also carries its own yaw offset
	UMotionWarpingComponent* Warping = Character->MotionWarpingComponent;
	// The test world initializes actors before spawning, so InitializeComponent created the adapter.
	if (TestNotNull(TEXT("Character has motion warping"), Warping)
		&& TestNotNull(TEXT("Motion warping adapter initialized"), Warping->GetOwnerAdapter()))
	{
		const FTransform MeshTransform = Character->GetMesh()->GetComponentTransform();
		FProceduralDisplacement Displacement;
		Displacement.Direction = FVector(0, 1, 0);
		Displacement.Distance = 10.f;
		Displacement.Duration = 1.f;
		Displacement.AnimationBlend = EDisplacementAnimationBlend::ReplaceAnimation;

		auto* Replacing = NewObject<URootMotionModifier_ProceduralDisplacement>(Warping);
		Replacing->Configure(Displacement, 0.0);
		const FTransform Local = Replacing->ProcessRootMotion(FTransform::Identity, 0.5f);
		TestTrue(TEXT("Mesh-local delta converts back to the world push"),
			MeshTransform.TransformVector(Local.GetTranslation()).Equals(FVector(0, 5, 0), 1e-3));

		Displacement.AnimationBlend = EDisplacementAnimationBlend::AddToAnimation;
		auto* Adding = NewObject<URootMotionModifier_ProceduralDisplacement>(Warping);
		Adding->Configure(Displacement, 0.0);
		const FVector AnimationLocal = MeshTransform.InverseTransformVector(FVector(0, 2, 0));
		Adding->ProcessRootMotion(FTransform(AnimationLocal), 0.5f);
		TestEqual(TEXT("Kept animation travel is measured in world space"), Adding->ConsumeAnimationTravel(), 2.0, 1e-3);
		TestEqual(TEXT("Consuming resets the measurement"), Adding->ConsumeAnimationTravel(), 0.0, 1e-9);
	}
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
