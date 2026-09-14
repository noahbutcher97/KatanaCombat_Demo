#include "CombatTestHelpers.h"
#include "Core/TargetingComponent.h"
#include "Utilities/AlignmentMotionLibrary.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include <limits>

namespace
{
struct FAlignmentFixture
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UTargetingComponent* Targeting() const { return Character->TargetingComponent; }
	FAlignmentRequestSpec Spec;
	FAlignmentFixture()
	{
		Character->GetCharacterMovement()->DisableMovement();
		Character->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Spec.OwnerId = TEXT("BoundedAlignmentTest"); Spec.OwnerGeneration = -1;
		Spec.Executor = EAlignmentExecutor::BoundedMovement;
		Spec.MotionLimits.Duration = 1;
		Spec.MotionLimits.TranslationSpeed = 120;
		Spec.MotionLimits.TurnRate = 90;
		Spec.MotionLimits.PositionTolerance = .01f; Spec.MotionLimits.YawTolerance = .01f;
		Spec.BoundedGoal = FTransform(FRotator(0, 60, 0), FVector(90, 0, 0));
	}
	void Step(float Delta) { Targeting()->ResetAlignmentExecutionFrameForTesting(); Targeting()->TickComponent(Delta, LEVELTICK_All, nullptr); }
	~FAlignmentFixture() { FCombatTestHelpers::DestroyTestWorld(World); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoundedAlignmentCadence, "KatanaCombat.Targeting.BoundedAlignment.FrameCadence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBoundedAlignmentCadence::RunTest(const FString&)
{
	for (const float Delta : {1.f/20, 1.f/60, 1.f/120})
	{
		FAlignmentFixture F;
		const auto Handle = F.Targeting()->AcquireAlignmentRequest(F.Spec);
		if (!TestTrue(TEXT("Bounded request acquired"), Handle.IsValid())) { return false; }
		FAlignmentMotionState State;
		for (int I=0; I<200; ++I)
		{
			const FVector Before = F.Character->GetActorLocation(); const double Yaw = F.Character->GetActorRotation().Yaw;
			F.Step(Delta);
			TestTrue(TEXT("Translation respects simulation rate"), FVector::Distance(Before, F.Character->GetActorLocation()) <= 120*Delta+.01);
			TestTrue(TEXT("Yaw respects simulation rate"), FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, F.Character->GetActorRotation().Yaw)) <= 90*Delta+.01);
			F.Targeting()->GetAlignmentMotionState(Handle, State);
			if (State.Outcome != EAlignmentMotionOutcome::Running) { break; }
		}
		TestEqual(TEXT("All cadences reach the goal"), State.Outcome, EAlignmentMotionOutcome::Reached);
		TestTrue(TEXT("Travel is measured from actual movement"), FMath::IsNearlyEqual(State.Travel, 90., .02));
		TestTrue(TEXT("Turn is measured from actual movement"), FMath::IsNearlyEqual(State.Turn, 60., .02));
		F.Targeting()->ReleaseAlignmentRequest(Handle);
		TestFalse(TEXT("Last bounded request releases tick"), F.Targeting()->IsComponentTickEnabled());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoundedAlignmentBudgets, "KatanaCombat.Targeting.BoundedAlignment.BudgetsAndMovingTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBoundedAlignmentBudgets::RunTest(const FString&)
{
	for (const int Mode : {0,1,2})
	{
		FAlignmentFixture F;
		AActor* Target = F.World->SpawnActor<AActor>(); Target->SetActorLocation(FVector(300,0,0));
		// A root is needed for an independently moving target transform.
		auto* Root = NewObject<USceneComponent>(Target); Target->SetRootComponent(Root); Root->RegisterComponent();
		Target->SetActorLocation(FVector(300,0,0));
		F.Spec.Target = Target; F.Spec.BoundedGoal = FTransform::Identity;
		if (Mode==0) { F.Spec.MotionLimits.TravelBudget = 20; }
		if (Mode==1) { F.Spec.MotionLimits.Duration = .1f; }
		const auto Handle = F.Targeting()->AcquireAlignmentRequest(F.Spec);
		FAlignmentMotionState State;
		for (int I=0; I<80; ++I)
		{
			Target->AddActorWorldOffset(FVector(2,0,0)); F.Step(.02f);
			F.Targeting()->GetAlignmentMotionState(Handle, State);
			if (State.Outcome != EAlignmentMotionOutcome::Running) { break; }
		}
		TestEqual(TEXT("A receding target cannot consume unlimited motion/time"), State.Outcome, EAlignmentMotionOutcome::Exhausted);
		TestTrue(TEXT("Travel cannot exceed its budget"), State.Travel <= F.Spec.MotionLimits.TravelBudget+.01);
		TestTrue(TEXT("Time cannot exceed its budget"), State.Elapsed <= F.Spec.MotionLimits.Duration+.0001);
		const FVector Before = F.Character->GetActorLocation(); F.Step(1);
		TestTrue(TEXT("Exhaustion cannot add movement"), Before.Equals(F.Character->GetActorLocation()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoundedAlignmentObstacles, "KatanaCombat.Targeting.BoundedAlignment.ObstacleAndTargetLoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBoundedAlignmentObstacles::RunTest(const FString&)
{
	FAlignmentFixture F;
	AActor* Wall = F.World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(5,100,200));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->SetCollisionObjectType(ECC_WorldStatic); Box->RegisterComponent(); Wall->SetActorLocation(FVector(80,0,0));
	auto Handle = F.Targeting()->AcquireAlignmentRequest(F.Spec); F.Step(1);
	FAlignmentMotionState State; F.Targeting()->GetAlignmentMotionState(Handle,State);
	TestEqual(TEXT("Swept environment obstruction is explicit"),State.Outcome,EAlignmentMotionOutcome::Blocked);
	TestTrue(TEXT("Character did not cross the wall"),F.Character->GetActorLocation().X < 80);
	F.Targeting()->ReleaseAlignmentRequest(Handle);
	F.Spec.Target=Wall; Handle=F.Targeting()->AcquireAlignmentRequest(F.Spec); Wall->Destroy(); F.Step(.1f);
	F.Targeting()->GetAlignmentMotionState(Handle,State);
	TestEqual(TEXT("Target loss cannot become a world-origin target"),State.Outcome,EAlignmentMotionOutcome::Invalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoundedAlignmentValidation, "KatanaCombat.Targeting.BoundedAlignment.InvalidAndPaused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBoundedAlignmentValidation::RunTest(const FString&)
{
	FAlignmentFixture F;
	F.Spec.MotionLimits.TranslationSpeed=std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite policy cannot acquire ownership"),F.Targeting()->AcquireAlignmentRequest(F.Spec).IsValid());
	F.Spec.MotionLimits.TranslationSpeed=120;
	const auto Handle=F.Targeting()->AcquireAlignmentRequest(F.Spec); F.Step(0);
	FAlignmentMotionState State; F.Targeting()->GetAlignmentMotionState(Handle,State);
	TestEqual(TEXT("Zero simulation delta spends no deadline"),State.Elapsed,0.);
	TestTrue(TEXT("Zero delta cannot move"),F.Character->GetActorLocation().IsNearlyZero());
	F.Step(.2f); const FVector Position=F.Character->GetActorLocation();
	FAlignmentRequestSpec Extended=F.Spec; Extended.MotionLimits.Duration=10;
	TestFalse(TEXT("An update cannot replenish the accepted deadline"),F.Targeting()->UpdateAlignmentRequest(Handle,Extended));
	F.Targeting()->TickComponent(.2f,LEVELTICK_All,nullptr);
	TestTrue(TEXT("Repeated call in one frame cannot double movement"),F.Character->GetActorLocation().Equals(Position));
	F.Spec.MotionLimits.TravelBudget=1;
	TestFalse(TEXT("Preflight rejects unreachable distance"),AlignmentMotion::CanReach(FTransform::Identity,F.Spec.BoundedGoal,F.Spec.MotionLimits));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoundedAlignmentClock, "KatanaCombat.Targeting.BoundedAlignment.ActorTimeDilation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBoundedAlignmentClock::RunTest(const FString&)
{
	for (const float Scale : {.25f, 1.f, 2.f})
	{
		FAlignmentFixture F; F.Character->CustomTimeDilation = Scale;
		const auto Handle = F.Targeting()->AcquireAlignmentRequest(F.Spec);
		F.Step(.1f * Scale); // ExecuteTickHelper supplies actor-dilated component time.
		FAlignmentMotionState State; F.Targeting()->GetAlignmentMotionState(Handle, State);
		TestTrue(TEXT("All actor rates spend the same world simulation time"),FMath::IsNearlyEqual(State.Elapsed,.1,1.e-6));
		TestTrue(TEXT("All actor rates retain the same translation bound"),FMath::IsNearlyEqual(F.Character->GetActorLocation().X,12.,.001));
		F.Character->CustomTimeDilation = 0; const FVector Before=F.Character->GetActorLocation(); F.Step(0);
		F.Targeting()->GetAlignmentMotionState(Handle, State);
		TestEqual(TEXT("A zero actor rate fails closed instead of owning an indefinite reservation"),State.Outcome,EAlignmentMotionOutcome::Invalid);
		TestTrue(TEXT("Unsupported zero actor rate cannot move"),F.Character->GetActorLocation().Equals(Before));
	}
	return true;
}
