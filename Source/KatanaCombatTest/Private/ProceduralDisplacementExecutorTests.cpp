#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Debug/ActionReactionTelemetry.h"
#include "HAL/IConsoleManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "EngineUtils.h"

namespace
{
struct FDisplacementFixture
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Character = nullptr;

	FDisplacementFixture()
	{
		Box(FVector(0, 0, -10), FVector(2000, 2000, 10));
		Character = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector(0, 0, 100));
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		Movement->bRunPhysicsWithNoController = true;
		Movement->SetMovementMode(MOVE_Walking);
		Character->SetActorRotation(FRotator::ZeroRotator);
		Settle();
	}
	~FDisplacementFixture() { FCombatTestHelpers::DestroyTestWorld(World); }

	UBoxComponent* Box(const FVector& Center, const FVector& Extent, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		auto* Component = NewObject<UBoxComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Actor->SetRootComponent(Component);
		Component->SetBoxExtent(Extent);
		Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Component->SetCollisionObjectType(ECC_WorldStatic);
		Component->SetCollisionResponseToAllChannels(ECR_Block);
		Component->CanCharacterStepUpOn = ECB_Yes;
		Component->RegisterComponent();
		Component->SetWorldLocationAndRotation(Center, Rotation);
		return Component;
	}

	UTargetingComponent* Targeting() const { return Character->TargetingComponent; }
	UCharacterMovementComponent* Movement() const { return Character->GetCharacterMovement(); }

	/** One frame: movement first, then the targeting executor (its registered prerequisite order). */
	void Step(const float ComponentDelta)
	{
		Movement()->TickComponent(ComponentDelta, LEVELTICK_All, nullptr);
		Targeting()->ResetAlignmentExecutionFrameForTesting();
		Targeting()->TickComponent(ComponentDelta, LEVELTICK_All, nullptr);
	}
	void Settle() { for (int32 I = 0; I < 30; ++I) { Movement()->TickComponent(1.f / 60, LEVELTICK_All, nullptr); } }

};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementMovementPremiseTest, "KatanaCombat.Displacement.Executor.MovementPremise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementMovementPremiseTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	TestEqual(TEXT("Character is walking on the floor"), F.Movement()->MovementMode.GetValue(), MOVE_Walking);
	auto Source = MakeShared<FRootMotionSource_ConstantForce>();
	Source->Force = FVector(100, 0, 0);
	Source->Duration = 0.5f;
	Source->AccumulateMode = ERootMotionAccumulateMode::Override;
	F.Movement()->ApplyRootMotionSource(Source);
	const FVector Start = F.Character->GetActorLocation();
	for (int32 I = 0; I < 30; ++I) { F.Movement()->TickComponent(1.f / 60, LEVELTICK_All, nullptr); }
	const double Moved = F.Character->GetActorLocation().X - Start.X;
	TestTrue(FString::Printf(TEXT("Root-motion source moves the character in the test world (moved %.2f cm)"), Moved), Moved > 40.0);
	return true;
}

namespace
{
/** Types from Step 2 exist now, so the request builder lives here rather than in the Step 1 fixture. */
FAlignmentRequestSpec MakePush(const float Distance, const float Duration, const EDisplacementSpeedProfile Profile = EDisplacementSpeedProfile::Linear)
{
	FAlignmentRequestSpec Spec;
	Spec.OwnerId = TEXT("DisplacementTest");
	Spec.OwnerGeneration = 1;
	Spec.Priority = EDefenseAlignmentPriority::HitKnockback;
	Spec.Executor = EAlignmentExecutor::ProceduralDisplacement;
	Spec.bReleaseWhenFinished = true;
	Spec.Displacement.Direction = FVector(1, 0, 0);
	Spec.Displacement.Distance = Distance;
	Spec.Displacement.Duration = Duration;
	Spec.Displacement.SpeedProfile = Profile;
	return Spec;
}

/** A block-contact turn: outranks HitKnockback, so acquiring it suspends a running push. */
FAlignmentRequestSpec MakeBlockContact()
{
	FAlignmentRequestSpec Block;
	Block.OwnerId = TEXT("BlockTest");
	Block.OwnerGeneration = 1;
	Block.Priority = EDefenseAlignmentPriority::BlockContact;
	Block.Executor = EAlignmentExecutor::CharacterMovement;
	Block.DesiredRotation = FRotator::ZeroRotator;
	Block.MaximumTurnRate = 90.f;
	Block.RemainingTurnBudget = 10.f;
	return Block;
}

/** Turns Combat.ActionReaction.Debug on for its scope. Declare it before the fixture, so it is restored on every exit after teardown. */
struct FActionReactionTelemetryOn
{
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.ActionReaction.Debug"));
	int32 Previous = Variable ? Variable->GetInt() : 0;
	FActionReactionTelemetryOn() { if (Variable) { Variable->Set(1, ECVF_SetByCode); } }
	~FActionReactionTelemetryOn() { if (Variable) { Variable->Set(Previous, ECVF_SetByCode); } }
};

/** The fixture push's telemetry rows (owner DisplacementTest), in order. */
TArray<FActionReactionTelemetryRecord> DisplacementRows(const FDisplacementFixture& F)
{
	return F.Character->GetCombatComponent()->GetActionReactionTelemetry().FilterByPredicate(
		[](const FActionReactionTelemetryRecord& Row) { return Row.AlignmentOwner == FName(TEXT("DisplacementTest")); });
}

/** One row's disposition and detail. */
void TestRow(FAutomationTestBase& Test, const TArray<FActionReactionTelemetryRecord>& Rows, const int32 Index,
	const TCHAR* Disposition, const TCHAR* Detail)
{
	if (!Test.TestTrue(FString::Printf(TEXT("Row %d (%s) exists"), Index, Disposition), Rows.IsValidIndex(Index))) { return; }
	Test.TestEqual(FString::Printf(TEXT("Row %d disposition"), Index), Rows[Index].AlignmentDisposition, FName(Disposition));
	Test.TestEqual(FString::Printf(TEXT("Row %d event"), Index), Rows[Index].Event, EActionReactionTelemetryEvent::AlignmentChanged);
	if (Detail)
	{
		Test.TestEqual(FString::Printf(TEXT("Row %d (%s) detail"), Index, Disposition), Rows[Index].Detail, FString(Detail));
	}
}

/** A wall whose face sits Gap cm ahead of the capsule's front (+X), tall and wide enough that the push cannot pass. */
UBoxComponent* WallAhead(FDisplacementFixture& F, const float Gap)
{
	const float Radius = F.Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	return F.Box(FVector(F.Character->GetActorLocation().X + Radius + Gap + 5.f, 0, 100), FVector(5, 500, 200));
}

/** Replace the fixture's large floor with one that ends 20 cm ahead of the character (+X). */
void ReplaceFloorWithLedge(FDisplacementFixture& F)
{
	for (TActorIterator<AActor> It(F.World); It; ++It)
	{
		if (It->GetRootComponent() && It->GetRootComponent()->IsA<UBoxComponent>()) { It->Destroy(); }
	}
	F.Box(FVector(-1000 + 20, 0, -10), FVector(1000, 1000, 10));
	F.Settle();
}

/**
 * UCharacterMovementComponent::StartFalling keeps MOVE_Walking in editor worlds (GIsEditor) until the
 * world has begun play and its TimeSeconds reaches 1. This test world has no game mode, so it never
 * begins play and is never ticked: satisfy that guard while in scope, and undo it before teardown
 * (CleanupWorld warns about a world still marked as begun play). Declare it after the fixture.
 */
struct FEditorFallingGuard
{
	UWorld* World;
	explicit FEditorFallingGuard(UWorld* InWorld) : World(InWorld)
	{
		World->SetBegunPlay(true);
		World->TimeSeconds = 1.0;
	}
	~FEditorFallingGuard() { World->SetBegunPlay(false); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementFlatLinearTest, "KatanaCombat.Displacement.Executor.FlatLinearReachesAndReleases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementFlatLinearTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(MakePush(50.f, 0.25f));
	if (!TestTrue(TEXT("Displacement request acquired"), Handle.IsValid())) { return false; }
	TestTrue(TEXT("Targeting tick enabled while pushing"), F.Targeting()->IsComponentTickEnabled());
	for (int32 I = 0; I < 30; ++I) { F.Step(1.f / 60); }
	const FVector Moved = F.Character->GetActorLocation() - Start;
	TestTrue(FString::Printf(TEXT("Pushed ~50 cm along X (moved %.2f)"), Moved.X), FMath::IsNearlyEqual(Moved.X, 50.0, 4.0));
	TestTrue(TEXT("No lateral drift"), FMath::Abs(Moved.Y) < 1.0);
	TestEqual(TEXT("Released itself when finished"), F.Targeting()->GetAlignmentRequestCountForTesting(), 0);
	TestFalse(TEXT("Tick disabled after release"), F.Targeting()->IsComponentTickEnabled());
	TestFalse(TEXT("Root-motion source removed"), F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement")).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementEaseOutTest, "KatanaCombat.Displacement.Executor.EaseOutFrontLoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementEaseOutTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	F.Targeting()->AcquireAlignmentRequest(MakePush(60.f, 0.24f, EDisplacementSpeedProfile::EaseOut));
	for (int32 I = 0; I < 8; ++I) { F.Step(1.f / 60); } // ~half the duration
	const double Half = F.Character->GetActorLocation().X - Start.X;
	TestTrue(FString::Printf(TEXT("More than half the distance in the first half (%.2f)"), Half), Half > 32.0);
	for (int32 I = 0; I < 20; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("Total ~60 cm"), FMath::IsNearlyEqual(F.Character->GetActorLocation().X - Start.X, 60.0, 5.0));
	return true;
}

namespace
{
/** Push 60 cm over 0.25 s through the movement channel, one Step per FrameDelta(Index), until the request releases itself. */
FVector PushWithFrameTimes(FAutomationTestBase& Test, const TCHAR* Name, const EDisplacementSpeedProfile Profile,
	const TFunctionRef<float(int32)> FrameDelta)
{
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	F.Targeting()->AcquireAlignmentRequest(MakePush(60.f, 0.25f, Profile));
	int32 Frames = 0;
	for (; Frames < 200 && F.Targeting()->GetAlignmentRequestCountForTesting() > 0; ++Frames)
	{
		F.Step(FrameDelta(Frames));
	}
	Test.TestEqual(FString::Printf(TEXT("%s: the push released itself"), Name), F.Targeting()->GetAlignmentRequestCountForTesting(), 0);
	const FVector Moved = F.Character->GetActorLocation() - Start;
	Test.AddInfo(FString::Printf(TEXT("%s: travel X %.3f cm, Y %.3f cm over %d frames"), Name, Moved.X, Moved.Y, Frames));
	return Moved;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementVariableFrameTimesTest, "KatanaCombat.Displacement.Executor.VariableFrameTimesLandOnDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementVariableFrameTimesTest::RunTest(const FString&)
{
	// Each movement step must cover the curve over its own delta, whatever the previous frame's delta was.
	const auto Alternating = [](const int32 Frame) { return Frame % 2 == 0 ? 1.f / 120 : 1.f / 20; };
	// The channel installs in the first frame's targeting tick, so frame 2 is the push's second movement step.
	const auto Hitch = [](const int32 Frame) { return Frame == 2 ? 0.1f : 1.f / 60; };
	struct FCase
	{
		const TCHAR* Name;
		EDisplacementSpeedProfile Profile;
		TFunctionRef<float(int32)> FrameDelta;
	};
	const FCase Cases[] = {
		{TEXT("EaseOut, alternating 1/120 and 1/20 s"), EDisplacementSpeedProfile::EaseOut, Alternating},
		{TEXT("EaseOut, 1/60 s with one 0.1 s hitch"), EDisplacementSpeedProfile::EaseOut, Hitch},
		{TEXT("Linear, alternating 1/120 and 1/20 s"), EDisplacementSpeedProfile::Linear, Alternating},
	};
	for (const FCase& Case : Cases)
	{
		const FVector Moved = PushWithFrameTimes(*this, Case.Name, Case.Profile, Case.FrameDelta);
		TestTrue(FString::Printf(TEXT("%s: lands on 60 cm (moved %.2f)"), Case.Name, Moved.X), FMath::IsNearlyEqual(Moved.X, 60.0, 1.0));
		TestTrue(FString::Printf(TEXT("%s: no lateral drift (%.2f)"), Case.Name, Moved.Y), FMath::Abs(Moved.Y) < 1.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementAnimRootMotionTickTest, "KatanaCombat.Displacement.Executor.AnimRootMotionTickDoesNotConsumePushClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementAnimRootMotionTickTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	F.Targeting()->AcquireAlignmentRequest(MakePush(60.f, 0.25f));
	F.Step(1.f / 60); // installs the movement channel
	F.Step(1.f / 60); // applies the push's first movement step
	// Animation root motion on the next movement step, as TickCharacterPose leaves it when a root-motion
	// montage has just started: character movement applies it and ignores every root-motion source
	// (ApplyRootMotionToVelocity), so none of the push is applied on this step.
	const double BeforeOverride = F.Character->GetActorLocation().X;
	F.Movement()->RootMotionParams.Set(FTransform::Identity);
	F.Step(1.f / 60);
	const double OverriddenStep = F.Character->GetActorLocation().X - BeforeOverride;
	TestTrue(FString::Printf(TEXT("Animation root motion overrode the push on that step (moved %.3f cm)"), OverriddenStep),
		FMath::Abs(OverriddenStep) < 0.1);
	for (int32 I = 0; I < 200 && F.Targeting()->GetAlignmentRequestCountForTesting() > 0; ++I) { F.Step(1.f / 60); }
	TestEqual(TEXT("The push released itself"), F.Targeting()->GetAlignmentRequestCountForTesting(), 0);
	const double Travel = F.Character->GetActorLocation().X - Start.X;
	TestTrue(FString::Printf(TEXT("The overridden step did not consume push time: lands on 60 cm (moved %.2f)"), Travel),
		FMath::IsNearlyEqual(Travel, 60.0, 1.0));
	return true;
}

namespace
{
/**
 * Animation root motion on the next movement step, as TickCharacterPose leaves it while a root-motion animation plays:
 * character movement applies it and ignores every root-motion source (ApplyRootMotionToVelocity), so the push's
 * source skips the step. Identity root motion, so the character itself does not move.
 */
void OverrideNextMovementStep(FDisplacementFixture& F)
{
	F.Movement()->RootMotionParams.Set(FTransform::Identity);
}

/** A finite value within a tolerance of the expected one: this build compares NaN as equal to anything. Named for this
 *  file, because a unity build can share an anonymous namespace with KnockbackPIETests.cpp's IsFiniteAndNear. */
bool IsFiniteDisplacementNear(const double Value, const double Expected, const double Tolerance)
{
	return FMath::IsFinite(Value) && FMath::Abs(Value - Expected) <= Tolerance;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementPushTravelTest, "KatanaCombat.Displacement.Executor.PushTravelOnMovementChannel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementPushTravelTest::RunTest(const FString&)
{
	// The push's own travel (FAlignmentMotionState::PushTravel). Only the movement channel can be driven here: the
	// animation channel needs a root-motion montage playing on an AnimInstance, which the test characters do not
	// have. Its exclusion of kept animation travel is pinned by KatanaCombat.Displacement.Math.PushStepExcludesKeptAnimation,
	// and the modifier's measurement of that travel by KatanaCombat.Displacement.Modifier.WorldDirectionOnCharacter.
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	FAlignmentRequestSpec Spec = MakePush(60.f, 0.25f, EDisplacementSpeedProfile::EaseOut);
	Spec.bReleaseWhenFinished = false; // hold the outcome
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(Spec);
	FAlignmentMotionState State;
	for (int32 I = 0; I < 60 && F.Targeting()->GetAlignmentMotionState(Handle, State) && State.Outcome == EAlignmentMotionOutcome::Running; ++I)
	{
		F.Step(1.f / 60);
	}
	F.Targeting()->GetAlignmentMotionState(Handle, State);
	TestEqual(TEXT("The push reached its end"), State.Outcome, EAlignmentMotionOutcome::Reached);
	const double Moved = F.Character->GetActorLocation().X - Start.X;
	TestTrue(FString::Printf(TEXT("The push's own travel is the requested 60 cm (%.3f; actor moved %.3f)"), State.PushTravel, Moved),
		IsFiniteDisplacementNear(State.PushTravel, 60.0, 1.0));
	// The movement channel keeps no animation root motion, so all travel along the push is the push's own.
	TestTrue(FString::Printf(TEXT("On the movement channel the push's own travel is all the travel (%.4f, %.4f)"), State.PushTravel, State.Travel),
		FMath::IsFinite(State.Travel) && IsFiniteDisplacementNear(State.PushTravel, State.Travel, 1e-6));
	F.Targeting()->ReleaseAlignmentRequest(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementOverriddenPushTest, "KatanaCombat.Displacement.Executor.OverriddenPushEndsCancelled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementOverriddenPushTest::RunTest(const FString&)
{
	const FActionReactionTelemetryOn Telemetry;
	if (!TestNotNull(TEXT("Telemetry CVar exists"), Telemetry.Variable)) { return false; }
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	FAlignmentRequestSpec Spec = MakePush(60.f, 0.25f);
	Spec.bReleaseWhenFinished = false; // hold the outcome
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(Spec);
	// Animation root motion overrides every movement step, as it does while a root-motion animation plays on a
	// character with no motion warping component (or in RootMotionFromEverything): the push clock never advances.
	const float Step = 1.f / 60;
	OverrideNextMovementStep(F);
	F.Step(Step); // installs the movement channel
	FAlignmentMotionState State;
	int32 OverriddenSteps = 0;
	for (; OverriddenSteps < 120 && F.Targeting()->GetAlignmentMotionState(Handle, State)
		&& State.Outcome == EAlignmentMotionOutcome::Running; ++OverriddenSteps)
	{
		OverrideNextMovementStep(F);
		F.Step(Step);
	}
	F.Targeting()->GetAlignmentMotionState(Handle, State);
	TestEqual(TEXT("An override that outlasts the push it had left ends it Cancelled"), State.Outcome, EAlignmentMotionOutcome::Cancelled);
	// The whole push was left (0.25 s, 15 steps); the override must exceed it, so the 16th overridden step ends it.
	const int32 RemainingSteps = FMath::RoundToInt(Spec.Displacement.Duration / Step);
	TestTrue(FString::Printf(TEXT("Cancelled within the push's remaining time plus two steps (%d overridden steps; bound %d)"),
		OverriddenSteps, RemainingSteps + 2), OverriddenSteps <= RemainingSteps + 2);
	const TArray<FActionReactionTelemetryRecord> Rows = DisplacementRows(F);
	TestEqual(TEXT("One terminal row"), Rows.Num(), 1);
	TestRow(*this, Rows, 0, TEXT("Cancelled"), TEXT("AnimationOverride"));
	if (Rows.IsValidIndex(0))
	{
		TestTrue(FString::Printf(TEXT("The row's travel is under 1 cm (%.3f)"), Rows[0].MovementMagnitude),
			FMath::IsFinite(Rows[0].MovementMagnitude) && Rows[0].MovementMagnitude < 1.0f);
	}
	const double Moved = F.Character->GetActorLocation().X - Start.X;
	TestTrue(FString::Printf(TEXT("The overridden push did not move the character (moved %.3f cm)"), Moved), IsFiniteDisplacementNear(Moved, 0.0, 1.0));
	TestFalse(TEXT("The tick stops once the push has ended"), F.Targeting()->IsComponentTickEnabled());
	F.Targeting()->ReleaseAlignmentRequest(Handle); // already ended, so the owner's release adds no row
	TestEqual(TEXT("Still one row after the owner's release"), DisplacementRows(F).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementBriefOverrideTest, "KatanaCombat.Displacement.Executor.BriefOverrideStillReaches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementBriefOverrideTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	FAlignmentRequestSpec Spec = MakePush(60.f, 0.25f);
	Spec.bReleaseWhenFinished = false; // hold the outcome
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(Spec);
	const float Step = 1.f / 60;
	for (int32 I = 0; I < 7; ++I) { F.Step(Step); } // installs, then 0.1 s of the 0.25 s push
	// Two overridden steps mid-push (0.033 s, well under the 0.15 s left). The push clock advancing afterwards must
	// reset the overridden time, or it would outlast the push's last steps and cancel a push that was delivered.
	const double BeforeOverride = F.Character->GetActorLocation().X;
	for (int32 I = 0; I < 2; ++I)
	{
		OverrideNextMovementStep(F);
		F.Step(Step);
	}
	const double Held = F.Character->GetActorLocation().X - BeforeOverride;
	TestTrue(FString::Printf(TEXT("The override held the push for two steps (moved %.3f cm)"), Held), IsFiniteDisplacementNear(Held, 0.0, 0.1));
	FAlignmentMotionState State;
	for (int32 I = 0; I < 60 && F.Targeting()->GetAlignmentMotionState(Handle, State) && State.Outcome == EAlignmentMotionOutcome::Running; ++I)
	{
		F.Step(Step);
	}
	F.Targeting()->GetAlignmentMotionState(Handle, State);
	TestEqual(TEXT("A brief override does not end the push"), State.Outcome, EAlignmentMotionOutcome::Reached);
	const double Travel = F.Character->GetActorLocation().X - Start.X;
	TestTrue(FString::Printf(TEXT("The push resumes and lands on 60 cm (moved %.2f)"), Travel), IsFiniteDisplacementNear(Travel, 60.0, 1.0));
	F.Targeting()->ReleaseAlignmentRequest(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementLastStepOverrideTest, "KatanaCombat.Displacement.Executor.OverrideAtLastStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementLastStepOverrideTest::RunTest(const FString&)
{
	const FActionReactionTelemetryOn Telemetry;
	if (!TestNotNull(TEXT("Telemetry CVar exists"), Telemetry.Variable)) { return false; }
	// The override lands on the push's last step. One overridden step is exactly as long as the push it had left, so
	// it is not longer and the push reaches one step late (at 60 Hz float sums put the two a hair apart, the wrong way
	// round). Two overridden steps outlast it: the push ends Cancelled without its last step (4 cm of a linear push).
	struct FCase
	{
		const TCHAR* Name;
		int32 OverriddenSteps;
		EAlignmentMotionOutcome Outcome;
		const TCHAR* Disposition;
		const TCHAR* Detail;
		double Travel;
	};
	const FCase Cases[] = {
		{TEXT("One overridden step"), 1, EAlignmentMotionOutcome::Reached, TEXT("Reached"), TEXT("DurationReached"), 60.0},
		{TEXT("Two overridden steps"), 2, EAlignmentMotionOutcome::Cancelled, TEXT("Cancelled"), TEXT("AnimationOverride"), 56.0},
	};
	for (const FCase& Case : Cases)
	{
		FDisplacementFixture F;
		const FVector Start = F.Character->GetActorLocation();
		FAlignmentRequestSpec Spec = MakePush(60.f, 0.25f);
		Spec.bReleaseWhenFinished = false; // hold the outcome
		const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(Spec);
		const float Step = 1.f / 60;
		for (int32 I = 0; I < 15; ++I) { F.Step(Step); } // installs, then 14 of the push's 15 steps
		FAlignmentMotionState State;
		F.Targeting()->GetAlignmentMotionState(Handle, State);
		const double Left = Spec.Displacement.Duration - State.Elapsed;
		if (!TestTrue(FString::Printf(TEXT("%s: one step of the push is left (%.7f s)"), Case.Name, Left), IsFiniteDisplacementNear(Left, Step, 1e-4))
			|| !TestEqual(FString::Printf(TEXT("%s: still running"), Case.Name), State.Outcome, EAlignmentMotionOutcome::Running))
		{
			F.Targeting()->ReleaseAlignmentRequest(Handle);
			continue;
		}
		for (int32 I = 0; I < Case.OverriddenSteps; ++I)
		{
			OverrideNextMovementStep(F);
			F.Step(Step);
		}
		for (int32 I = 0; I < 30 && F.Targeting()->GetAlignmentMotionState(Handle, State) && State.Outcome == EAlignmentMotionOutcome::Running; ++I)
		{
			F.Step(Step);
		}
		F.Targeting()->GetAlignmentMotionState(Handle, State);
		TestEqual(FString::Printf(TEXT("%s: outcome"), Case.Name), State.Outcome, Case.Outcome);
		const TArray<FActionReactionTelemetryRecord> Rows = DisplacementRows(F);
		TestEqual(FString::Printf(TEXT("%s: one terminal row"), Case.Name), Rows.Num(), 1);
		TestRow(*this, Rows, 0, Case.Disposition, Case.Detail);
		const double Travel = F.Character->GetActorLocation().X - Start.X;
		TestTrue(FString::Printf(TEXT("%s: travel %.2f cm, expected %.0f"), Case.Name, Travel, Case.Travel), IsFiniteDisplacementNear(Travel, Case.Travel, 1.0));
		F.Targeting()->ReleaseAlignmentRequest(Handle);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementHitstopTest, "KatanaCombat.Displacement.Executor.PausesUnderHitstopDilation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementHitstopTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(MakePush(40.f, 0.2f));
	// Component deltas are already scaled by actor dilation; hitstop freezes at 0.0001.
	for (int32 I = 0; I < 30; ++I) { F.Step(1.f / 60 * 0.0001f); }
	TestTrue(TEXT("Frozen victim does not move"), FVector::Dist2D(F.Character->GetActorLocation(), Start) < 0.5);
	FAlignmentMotionState State;
	TestTrue(TEXT("Request still exists"), F.Targeting()->GetAlignmentMotionState(Handle, State));
	TestEqual(TEXT("Still running while frozen"), State.Outcome, EAlignmentMotionOutcome::Running);
	for (int32 I = 0; I < 25; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("Completes the push after the freeze"), FMath::IsNearlyEqual(F.Character->GetActorLocation().X - Start.X, 40.0, 4.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementWallTest, "KatanaCombat.Displacement.Executor.WallBlocksAndReleases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementWallTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const float Radius = F.Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	F.Box(FVector(F.Character->GetActorLocation().X + Radius + 15.f, 0, 100), FVector(5, 500, 200));
	FAlignmentRequestSpec Spec = MakePush(80.f, 0.3f);
	Spec.bReleaseWhenFinished = false; // hold the outcome: a push that ran out its duration would also end
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(Spec);
	FAlignmentMotionState State;
	for (int32 I = 0; I < 40 && F.Targeting()->GetAlignmentMotionState(Handle, State) && State.Outcome == EAlignmentMotionOutcome::Running; ++I)
	{
		F.Step(1.f / 60);
	}
	F.Targeting()->GetAlignmentMotionState(Handle, State);
	TestEqual(TEXT("The wall blocks the push"), State.Outcome, EAlignmentMotionOutcome::Blocked);
	TestTrue(FString::Printf(TEXT("Blocked before the push's end (%.3f of %.3f s)"), State.Elapsed, Spec.Displacement.Duration),
		State.Elapsed < Spec.Displacement.Duration);
	TestTrue(TEXT("Stopped at the wall"), F.Character->GetActorLocation().X < 20.0);
	F.Targeting()->ReleaseAlignmentRequest(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementWallTimeTest, "KatanaCombat.Displacement.Executor.WallBlockedAfterTimeNotTicks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementWallTimeTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	// Flush against the wall, so the push stalls from its first movement step and its request time at
	// Blocked is the time it spent blocked. At 240 Hz a tick count would end it after 12.5 ms.
	WallAhead(F, 0.1f);
	const double StartX = F.Character->GetActorLocation().X;
	FAlignmentRequestSpec Spec = MakePush(80.f, 0.3f);
	Spec.bReleaseWhenFinished = false; // hold the outcome
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(Spec);
	FAlignmentMotionState State;
	for (int32 I = 0; I < 160 && F.Targeting()->GetAlignmentMotionState(Handle, State) && State.Outcome == EAlignmentMotionOutcome::Running; ++I)
	{
		F.Step(1.f / 240);
	}
	F.Targeting()->GetAlignmentMotionState(Handle, State);
	TestEqual(TEXT("The wall blocks the push"), State.Outcome, EAlignmentMotionOutcome::Blocked);
	// The premise: flush against the wall, the push stalled from its first step. With a gap, approach time would fill
	// the request clock and the Elapsed check below would pass even for a tick count.
	const double Moved = F.Character->GetActorLocation().X - StartX;
	TestTrue(FString::Printf(TEXT("Stalled from its first step (moved %.3f cm)"), Moved), FMath::IsFinite(Moved) && Moved < 0.5);
	TestTrue(FString::Printf(TEXT("Blocked after 0.05 s of request time, not after a tick count (%.4f s)"), State.Elapsed),
		State.Elapsed >= 0.045);
	TestTrue(FString::Printf(TEXT("Blocked before the push's end (%.3f of %.3f s)"), State.Elapsed, Spec.Displacement.Duration),
		State.Elapsed < Spec.Displacement.Duration);
	F.Targeting()->ReleaseAlignmentRequest(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementIntermittentContactTest, "KatanaCombat.Displacement.Executor.IntermittentContactDoesNotBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementIntermittentContactTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	FAlignmentRequestSpec Spec = MakePush(80.f, 0.3f);
	Spec.bReleaseWhenFinished = false; // hold the outcome
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(Spec);
	// Each contact stalls the push for two 60 Hz steps (0.033 s, under the 0.05 s Blocked limit); the free
	// steps between them reset the blocked time, so the two contacts never add up to Blocked.
	const auto Contact = [this, &F](const TCHAR* Name)
	{
		const double Before = F.Character->GetActorLocation().X;
		UBoxComponent* Obstacle = WallAhead(F, 0.1f);
		F.Step(1.f / 60);
		F.Step(1.f / 60);
		const double Stalled = F.Character->GetActorLocation().X - Before;
		TestTrue(FString::Printf(TEXT("%s stalled the push for two steps (moved %.3f cm)"), Name, Stalled), Stalled < 0.2);
		Obstacle->GetOwner()->Destroy();
	};
	for (int32 I = 0; I < 3; ++I) { F.Step(1.f / 60); } // installs, then two free steps
	Contact(TEXT("The first contact"));
	for (int32 I = 0; I < 2; ++I) { F.Step(1.f / 60); }
	Contact(TEXT("The second contact"));
	FAlignmentMotionState State;
	for (int32 I = 0; I < 40 && F.Targeting()->GetAlignmentMotionState(Handle, State) && State.Outcome == EAlignmentMotionOutcome::Running; ++I)
	{
		F.Step(1.f / 60);
	}
	F.Targeting()->GetAlignmentMotionState(Handle, State);
	TestEqual(TEXT("Brief contacts separated by free movement do not end the push"), State.Outcome, EAlignmentMotionOutcome::Reached);
	F.Targeting()->ReleaseAlignmentRequest(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementLedgeTest, "KatanaCombat.Displacement.Executor.LedgeFallsInsteadOfHovering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementLedgeTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	ReplaceFloorWithLedge(F);
	const double StartZ = F.Character->GetActorLocation().Z;
	const float Radius = F.Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FEditorFallingGuard Falling(F.World); // StartFalling's editor-world guard; see FEditorFallingGuard
	F.Targeting()->AcquireAlignmentRequest(MakePush(100.f, 0.25f));
	bool bFell = false;
	for (int32 I = 0; I < 60; ++I)
	{
		F.Step(1.f / 60);
		bFell |= F.Movement()->MovementMode == MOVE_Falling;
	}
	TestTrue(TEXT("Victim entered falling"), bFell);
	// Rolling the capsule over the edge lowers it by up to its radius even while it hovers in
	// Walking mode with no floor; only a real fall takes it further.
	const double Drop = StartZ - F.Character->GetActorLocation().Z;
	const double RequiredDrop = 2.0 * Radius;
	TestTrue(FString::Printf(TEXT("Victim fell well below the edge instead of hovering (dropped %.1f cm, required more than %.1f)"), Drop, RequiredDrop),
		Drop > RequiredDrop);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementFinishVelocityTest, "KatanaCombat.Displacement.Executor.PushEndingMidFallKeepsFallSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementFinishVelocityTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	ReplaceFloorWithLedge(F);
	const FEditorFallingGuard Falling(F.World); // StartFalling's editor-world guard; see FEditorFallingGuard
	// Long enough to leave the floor early and still be pushing well into the fall.
	FAlignmentRequestSpec Spec = MakePush(150.f, 0.4f);
	Spec.bReleaseWhenFinished = false;
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(Spec);
	FAlignmentMotionState State;
	for (int32 I = 0; I < 60 && F.Targeting()->GetAlignmentMotionState(Handle, State) && State.Outcome == EAlignmentMotionOutcome::Running; ++I)
	{
		F.Step(1.f / 60);
	}
	F.Targeting()->GetAlignmentMotionState(Handle, State);
	if (!TestEqual(TEXT("The push reached its end"), State.Outcome, EAlignmentMotionOutcome::Reached)) { return false; }
	if (!TestEqual(TEXT("The push ended mid-fall"), F.Movement()->MovementMode.GetValue(), MOVE_Falling)) { return false; }
	const FVector AtEnd = F.Movement()->Velocity;
	const double OneTickOfGravity = F.Movement()->GetGravityZ() / 60.0;
	TestTrue(FString::Printf(TEXT("Falling for several ticks when the push ended (Z speed %.1f)"), AtEnd.Z), AtEnd.Z < 3.0 * OneTickOfGravity);
	TestTrue(FString::Printf(TEXT("Moving horizontally when the push ended (%.1f)"), AtEnd.Size2D()), AtEnd.Size2D() > 100.0);

	// The executor marked the source at the end of that frame; this movement tick removes it and applies the finish velocity.
	F.Step(1.f / 60);
	TestFalse(TEXT("Source removed"), F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement")).IsValid());
	const FVector After = F.Movement()->Velocity;
	TestTrue(FString::Printf(TEXT("Fall speed kept through removal (Z %.1f before, %.1f after)"), AtEnd.Z, After.Z), After.Z <= AtEnd.Z + 1.0);
	TestTrue(FString::Printf(TEXT("Horizontal speed clamped to zero (%.2f)"), After.Size2D()), After.Size2D() < 1.0);
	F.Targeting()->ReleaseAlignmentRequest(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementRotationSettingsTest, "KatanaCombat.Displacement.Executor.PlayerRotationSettingsUntouched",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementRotationSettingsTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	F.Movement()->bOrientRotationToMovement = true;
	F.Character->bUseControllerRotationYaw = true;
	F.Targeting()->AcquireAlignmentRequest(MakePush(30.f, 0.2f));
	TestTrue(TEXT("Orient-to-movement untouched during a push"), F.Movement()->bOrientRotationToMovement);
	TestTrue(TEXT("Controller yaw untouched during a push"), F.Character->bUseControllerRotationYaw);
	for (int32 I = 0; I < 20; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("Orient-to-movement untouched after a push"), F.Movement()->bOrientRotationToMovement);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementPriorityTest, "KatanaCombat.Displacement.Executor.SuspendsUnderHigherPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementPriorityTest::RunTest(const FString&)
{
	const FActionReactionTelemetryOn Telemetry;
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	// EaseOut, so a resume that replayed the curve's front-loaded start would overshoot; held, so the outcome
	// shows whether the resumed request clock reached the push's end.
	FAlignmentRequestSpec Spec = MakePush(40.f, 0.2f, EDisplacementSpeedProfile::EaseOut);
	Spec.bReleaseWhenFinished = false;
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(Spec);
	// Run mid-push first so a suspension that reset the push clock would be caught on resume.
	for (int32 I = 0; I < 6; ++I) { F.Step(1.f / 60); }
	const double TravelBeforeSuspend = F.Character->GetActorLocation().X - Start.X;
	TestTrue(TEXT("Mid-push before the suspension"), TravelBeforeSuspend > 2.0 && TravelBeforeSuspend < 38.0);
	const FAlignmentRequestHandle BlockHandle = F.Targeting()->AcquireAlignmentRequest(MakeBlockContact());
	if (!TestTrue(TEXT("Block request acquired"), BlockHandle.IsValid())) { return false; }
	const double Suspended = F.Character->GetActorLocation().X;
	// 0.083 s: shorter than the 0.117 s the push has left, so it resumes rather than cancels.
	for (int32 I = 0; I < 5; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("No push while suspended"), FMath::Abs(F.Character->GetActorLocation().X - Suspended) < 1.0);
	// A removed source is only marked; the next movement tick drops it, so check after stepping.
	TestFalse(TEXT("Suspended push has no source"), F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement")).IsValid());
	F.Targeting()->ReleaseAlignmentRequest(BlockHandle);
	for (int32 I = 0; I < 30; ++I) { F.Step(1.f / 60); }
	FAlignmentMotionState State;
	TestTrue(TEXT("The owner still holds the request"), F.Targeting()->GetAlignmentMotionState(Handle, State));
	TestEqual(TEXT("The resumed push reaches its end"), State.Outcome, EAlignmentMotionOutcome::Reached);
	const double Travel = F.Character->GetActorLocation().X - Start.X;
	TestTrue(FString::Printf(TEXT("Push resumes from its clock and lands on 40 cm (moved %.2f)"), Travel), FMath::IsNearlyEqual(Travel, 40.0, 1.0));
	// Telemetry: suspended by the block, resumed after it, then the terminal row.
	const TArray<FActionReactionTelemetryRecord> Rows = DisplacementRows(F);
	TestEqual(TEXT("Three rows"), Rows.Num(), 3);
	TestRow(*this, Rows, 0, TEXT("Suspended"), TEXT("BlockTest"));
	TestRow(*this, Rows, 1, TEXT("Resumed"), nullptr);
	TestRow(*this, Rows, 2, TEXT("Reached"), TEXT("DurationReached"));
	F.Targeting()->ReleaseAlignmentRequest(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementLongSuspensionTest, "KatanaCombat.Displacement.Executor.LongSuspensionDoesNotResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementLongSuspensionTest::RunTest(const FString&)
{
	const FActionReactionTelemetryOn Telemetry;
	FDisplacementFixture F;
	FAlignmentRequestSpec Spec = MakePush(60.f, 0.25f, EDisplacementSpeedProfile::EaseOut);
	Spec.bReleaseWhenFinished = false; // hold the outcome
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(Spec);
	for (int32 I = 0; I < 3; ++I) { F.Step(1.f / 60); } // installs, then 0.033 s of the 0.25 s push
	const FAlignmentRequestHandle BlockHandle = F.Targeting()->AcquireAlignmentRequest(MakeBlockContact());
	if (!TestTrue(TEXT("Block request acquired"), BlockHandle.IsValid())) { return false; }
	// A 1 s suspension, far longer than the 0.217 s the push has left.
	for (int32 I = 0; I < 60; ++I) { F.Step(1.f / 60); }
	F.Targeting()->ReleaseAlignmentRequest(BlockHandle);
	const double Released = F.Character->GetActorLocation().X;
	for (int32 I = 0; I < 30; ++I) { F.Step(1.f / 60); }
	const double After = F.Character->GetActorLocation().X - Released;
	TestTrue(FString::Printf(TEXT("A stale push does not resume (moved %.2f cm)"), After), FMath::Abs(After) < 1.0);
	FAlignmentMotionState State;
	TestTrue(TEXT("The owner still holds the request"), F.Targeting()->GetAlignmentMotionState(Handle, State));
	TestEqual(TEXT("A stale push ends Cancelled"), State.Outcome, EAlignmentMotionOutcome::Cancelled);
	// Telemetry: suspended, then cancelled through the terminal path with its reason; never resumed.
	F.Targeting()->ReleaseAlignmentRequest(Handle); // already ended, so the owner's release adds no row
	const TArray<FActionReactionTelemetryRecord> Rows = DisplacementRows(F);
	TestEqual(TEXT("Two rows"), Rows.Num(), 2);
	TestRow(*this, Rows, 0, TEXT("Suspended"), TEXT("BlockTest"));
	TestRow(*this, Rows, 1, TEXT("Cancelled"), TEXT("StaleSuspension"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementCancelledRowTest, "KatanaCombat.Displacement.Executor.ReleaseWritesCancelledRowWithReason",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementCancelledRowTest::RunTest(const FString&)
{
	const FActionReactionTelemetryOn Telemetry;
	if (!TestNotNull(TEXT("Telemetry CVar exists"), Telemetry.Variable)) { return false; }
	FDisplacementFixture F;
	UCombatComponent* Combat = F.Character->GetCombatComponent();
	if (!TestNotNull(TEXT("Combat component"), Combat)) { return false; }
	// A running push removed by anything but its own outcome writes one Cancelled row with the reason.
	struct FCase
	{
		const TCHAR* Name;
		TFunction<void(UTargetingComponent&, FAlignmentRequestHandle)> Release;
		const TCHAR* Reason;
	};
	const FCase Cases[] = {
		{TEXT("Owner release"), [](UTargetingComponent& T, const FAlignmentRequestHandle H) { T.ReleaseAlignmentRequest(H); }, TEXT("Released")},
		{TEXT("Release with a caller reason"), [](UTargetingComponent& T, const FAlignmentRequestHandle H) { T.ReleaseAlignmentRequest(H, TEXT("Replaced")); }, TEXT("Replaced")},
		{TEXT("Death"), [](UTargetingComponent& T, FAlignmentRequestHandle) { T.ReleaseAllAlignmentRequests(EAlignmentReleaseReason::Death); }, TEXT("Death")},
	};
	for (const FCase& Case : Cases)
	{
		Combat->ClearActionReactionTelemetry();
		const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(MakePush(60.f, 0.5f));
		for (int32 I = 0; I < 3; ++I) { F.Step(1.f / 60); }
		Case.Release(*F.Targeting(), Handle);
		const TArray<FActionReactionTelemetryRecord> Rows = DisplacementRows(F);
		TestEqual(FString::Printf(TEXT("%s: one row"), Case.Name), Rows.Num(), 1);
		TestRow(*this, Rows, 0, TEXT("Cancelled"), Case.Reason);
		TestEqual(FString::Printf(TEXT("%s: request removed"), Case.Name), F.Targeting()->GetAlignmentRequestCountForTesting(), 0);
	}

	// A push that already ended reports nothing more when its owner releases it.
	FAlignmentRequestSpec Held = MakePush(30.f, 0.2f);
	Held.bReleaseWhenFinished = false;
	const FAlignmentRequestHandle HeldHandle = F.Targeting()->AcquireAlignmentRequest(Held);
	for (int32 I = 0; I < 20; ++I) { F.Step(1.f / 60); }
	Combat->ClearActionReactionTelemetry();
	F.Targeting()->ReleaseAlignmentRequest(HeldHandle);
	TestEqual(TEXT("Releasing a finished push writes no row"), DisplacementRows(F).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementReleaseRaceTest, "KatanaCombat.Displacement.Executor.ReleaseAfterFinalStepReportsReached",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementReleaseRaceTest::RunTest(const FString&)
{
	const FActionReactionTelemetryOn Telemetry;
	if (!TestNotNull(TEXT("Telemetry CVar exists"), Telemetry.Variable)) { return false; }
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	const FAlignmentRequestSpec Spec = MakePush(60.f, 0.25f);
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(Spec);
	// The fixture's Step split in two, in the engine's order (character movement, then this component): stop right
	// after the movement tick that applies the push's last step, before the targeting tick that would report it.
	// That is where a release from another actor's tick lands (a second hit's StartKnockback, a death, a paired
	// takeover). The source's own clock says when the step was applied; float sums of 1/60 s can put it on the
	// fifteenth or sixteenth step.
	const float Step = 1.f / 60;
	bool bLastStepApplied = false;
	for (int32 Frame = 0; Frame < 40 && !bLastStepApplied; ++Frame)
	{
		F.Movement()->TickComponent(Step, LEVELTICK_All, nullptr);
		const TSharedPtr<FRootMotionSource> Source = F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement"));
		bLastStepApplied = Source.IsValid() && Source->GetTime() >= Spec.Displacement.Duration;
		if (!bLastStepApplied)
		{
			F.Targeting()->ResetAlignmentExecutionFrameForTesting();
			F.Targeting()->TickComponent(Step, LEVELTICK_All, nullptr);
		}
	}
	if (!TestTrue(TEXT("Character movement applied the push's last step"), bLastStepApplied)) { return false; }
	FAlignmentMotionState State;
	TestTrue(TEXT("The request is still held"), F.Targeting()->GetAlignmentMotionState(Handle, State));
	TestEqual(TEXT("The executor has not seen the last step yet"), State.Outcome, EAlignmentMotionOutcome::Running);
	const double Moved = F.Character->GetActorLocation().X - Start.X;
	TestTrue(FString::Printf(TEXT("The character has moved the whole push (%.2f cm)"), Moved), IsFiniteDisplacementNear(Moved, 60.0, 1.0));

	F.Targeting()->ReleaseAlignmentRequest(Handle, TEXT("Replaced"));
	TestEqual(TEXT("The release removes the request"), F.Targeting()->GetAlignmentRequestCountForTesting(), 0);
	const TArray<FActionReactionTelemetryRecord> Rows = DisplacementRows(F);
	TestEqual(TEXT("One terminal row"), Rows.Num(), 1);
	TestRow(*this, Rows, 0, TEXT("Reached"), TEXT("DurationReached"));
	if (Rows.IsValidIndex(0))
	{
		TestTrue(FString::Printf(TEXT("The row's travel includes the last step (%.2f of 60 cm)"), Rows[0].MovementMagnitude),
			IsFiniteDisplacementNear(Rows[0].MovementMagnitude, 60.0, 1.0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementInvalidTest, "KatanaCombat.Displacement.Executor.InvalidWithoutChannelReleases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementInvalidTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	F.Targeting()->AcquireAlignmentRequest(MakePush(40.f, 0.2f));
	F.Movement()->DisableMovement(); // MOVE_None: no channel can deliver the push
	F.Step(1.f / 60);
	TestEqual(TEXT("Invalid push released itself"), F.Targeting()->GetAlignmentRequestCountForTesting(), 0);
	TestFalse(TEXT("Tick disabled after release"), F.Targeting()->IsComponentTickEnabled());
	TestTrue(TEXT("Nothing moved"), FVector::Dist(F.Character->GetActorLocation(), Start) < 0.5);
	return true;
}

namespace
{
/** Push through the movement channel for a few frames, make the owner unable to deliver, step once: the push must end Invalid. */
void CheckUndeliverableMidPushEndsInvalid(FAutomationTestBase& Test, const TFunctionRef<void(FDisplacementFixture&)> MakeUndeliverable)
{
	FDisplacementFixture F;
	FAlignmentRequestSpec Spec = MakePush(60.f, 0.5f);
	Spec.bReleaseWhenFinished = false;
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(Spec);
	for (int32 I = 0; I < 5; ++I) { F.Step(1.f / 60); }
	FAlignmentMotionState State;
	F.Targeting()->GetAlignmentMotionState(Handle, State);
	if (!Test.TestEqual(TEXT("Pushing before the owner stops moving"), State.Outcome, EAlignmentMotionOutcome::Running)) { return; }
	MakeUndeliverable(F);
	F.Step(1.f / 60);
	Test.TestTrue(TEXT("The owner still holds the request"), F.Targeting()->GetAlignmentMotionState(Handle, State));
	Test.TestEqual(TEXT("An undeliverable push ends Invalid"), State.Outcome, EAlignmentMotionOutcome::Invalid);
	Test.TestFalse(TEXT("The tick stops once the push has ended"), F.Targeting()->IsComponentTickEnabled());
	// Removal only marks a source; a movement component that is not updating keeps it until it runs again.
	const TSharedPtr<FRootMotionSource> Source = F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement"));
	Test.TestTrue(TEXT("No live root-motion source is left behind"),
		!Source.IsValid() || Source->Status.HasFlag(ERootMotionSourceStatusFlags::MarkedForRemoval));
	F.Targeting()->ReleaseAlignmentRequest(Handle);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementDisabledMidPushTest, "KatanaCombat.Displacement.Executor.DisabledMidPushEndsInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementDisabledMidPushTest::RunTest(const FString&)
{
	CheckUndeliverableMidPushEndsInvalid(*this, [](FDisplacementFixture& F) { F.Movement()->DisableMovement(); });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementPhysicsMidPushTest, "KatanaCombat.Displacement.Executor.PhysicsSimulatedMidPushEndsInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementPhysicsMidPushTest::RunTest(const FString&)
{
	CheckUndeliverableMidPushEndsInvalid(*this, [this](FDisplacementFixture& F)
	{
		F.Character->GetCapsuleComponent()->SetSimulatePhysics(true); // e.g. a ragdoll: character movement stops ticking
		TestTrue(TEXT("The capsule simulates physics"), F.Movement()->UpdatedComponent->IsSimulatingPhysics());
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementReleaseTest, "KatanaCombat.Displacement.Executor.ReleaseRemovesChannel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementReleaseTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(MakePush(60.f, 0.5f));
	for (int32 I = 0; I < 5; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("Push is running through the movement channel"), F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement")).IsValid());
	F.Targeting()->ReleaseAlignmentRequest(Handle);
	F.Step(1.f / 60); // the marked source is dropped on the next movement tick
	const double Released = F.Character->GetActorLocation().X;
	for (int32 I = 0; I < 10; ++I) { F.Step(1.f / 60); }
	TestFalse(TEXT("Source removed on release"), F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement")).IsValid());
	TestTrue(TEXT("No drift after release (finish velocity clamps horizontal speed)"), FMath::Abs(F.Character->GetActorLocation().X - Released) < 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementReleaseAllTest, "KatanaCombat.Displacement.Executor.ReleaseAllRemovesChannel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementReleaseAllTest::RunTest(const FString&)
{
	const FActionReactionTelemetryOn Telemetry;
	if (!TestNotNull(TEXT("Telemetry CVar exists"), Telemetry.Variable)) { return false; }
	FDisplacementFixture F;
	F.Character->GetCombatComponent()->ClearActionReactionTelemetry();
	F.Targeting()->AcquireAlignmentRequest(MakePush(60.f, 0.5f));
	for (int32 I = 0; I < 5; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("Push is running through the movement channel"), F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement")).IsValid());
	// Death and component teardown release every request at once, not through the owner's release.
	F.Targeting()->ReleaseAllAlignmentRequests(EAlignmentReleaseReason::Death);
	F.Step(1.f / 60); // the marked source is dropped on the next movement tick
	TestFalse(TEXT("Source removed by the broad release"), F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement")).IsValid());
	const double Released = F.Character->GetActorLocation().X;
	for (int32 I = 0; I < 10; ++I) { F.Step(1.f / 60); }
	TestTrue(FString::Printf(TEXT("No drift after the broad release (moved %.2f cm)"), F.Character->GetActorLocation().X - Released),
		FMath::Abs(F.Character->GetActorLocation().X - Released) < 1.0);
	const TArray<FActionReactionTelemetryRecord> Rows = DisplacementRows(F);
	TestEqual(TEXT("One row"), Rows.Num(), 1);
	TestRow(*this, Rows, 0, TEXT("Cancelled"), TEXT("Death"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementRotationRestoreTest, "KatanaCombat.Displacement.Executor.RotationRestoredWhilePushRuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementRotationRestoreTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	F.Movement()->bOrientRotationToMovement = true;
	F.Character->bUseControllerRotationYaw = true;
	// A rotating request below HitKnockback, so the push runs while it is still held.
	FAlignmentRequestSpec Guard = MakeBlockContact();
	Guard.OwnerId = TEXT("GuardTest");
	Guard.Priority = EDefenseAlignmentPriority::GuardFacing;
	const FAlignmentRequestHandle GuardHandle = F.Targeting()->AcquireAlignmentRequest(Guard);
	if (!TestTrue(TEXT("Rotating request acquired"), GuardHandle.IsValid())) { return false; }
	TestFalse(TEXT("The rotating request takes orient-to-movement while held"), F.Movement()->bOrientRotationToMovement);
	TestFalse(TEXT("The rotating request takes controller yaw while held"), F.Character->bUseControllerRotationYaw);

	const FVector Start = F.Character->GetActorLocation();
	FAlignmentRequestSpec Spec = MakePush(60.f, 0.5f);
	Spec.bReleaseWhenFinished = false; // hold the outcome
	const FAlignmentRequestHandle Push = F.Targeting()->AcquireAlignmentRequest(Spec);
	TestTrue(TEXT("The push outranks the rotating request"), F.Targeting()->GetActiveAlignmentRequest() == Push);
	for (int32 I = 0; I < 5; ++I) { F.Step(1.f / 60); }
	FAlignmentMotionState State;
	F.Targeting()->GetAlignmentMotionState(Push, State);
	TestEqual(TEXT("Pushing while the rotating request is held"), State.Outcome, EAlignmentMotionOutcome::Running);
	TestTrue(FString::Printf(TEXT("The push moved the character (%.2f cm)"), F.Character->GetActorLocation().X - Start.X),
		F.Character->GetActorLocation().X - Start.X > 2.0);

	F.Targeting()->ReleaseAlignmentRequest(GuardHandle);
	TestTrue(TEXT("Orient-to-movement is restored once no rotating request remains"), F.Movement()->bOrientRotationToMovement);
	TestTrue(TEXT("Controller yaw is restored once no rotating request remains"), F.Character->bUseControllerRotationYaw);
	F.Targeting()->GetAlignmentMotionState(Push, State);
	TestEqual(TEXT("The push still runs after the restore"), State.Outcome, EAlignmentMotionOutcome::Running);
	TestTrue(TEXT("The push keeps its channel"), F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement")).IsValid());
	const double BeforeMore = F.Character->GetActorLocation().X;
	for (int32 I = 0; I < 3; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("The push keeps moving the character"), F.Character->GetActorLocation().X - BeforeMore > 1.0);
	TestTrue(TEXT("The restored settings stay restored while it runs"), F.Movement()->bOrientRotationToMovement);
	F.Targeting()->ReleaseAlignmentRequest(Push);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementReachedRowTest, "KatanaCombat.Displacement.Executor.ReachedRowNamesRequestOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementReachedRowTest::RunTest(const FString&)
{
	const FActionReactionTelemetryOn Telemetry;
	if (!TestNotNull(TEXT("Telemetry CVar exists"), Telemetry.Variable)) { return false; }
	FDisplacementFixture F;
	UCombatComponent* Combat = F.Character->GetCombatComponent();
	if (!TestNotNull(TEXT("Combat component"), Combat)) { return false; }
	Combat->ClearActionReactionTelemetry();
	const FVector Start = F.Character->GetActorLocation();
	// Its own owner, so the check below does not rest on the fixture's row filter.
	FAlignmentRequestSpec Spec = MakePush(50.f, 0.25f);
	Spec.OwnerId = TEXT("TerminalRowTest");
	F.Targeting()->AcquireAlignmentRequest(Spec);
	for (int32 I = 0; I < 60 && F.Targeting()->GetAlignmentRequestCountForTesting() > 0; ++I) { F.Step(1.f / 60); }
	TestEqual(TEXT("The push completed and released itself"), F.Targeting()->GetAlignmentRequestCountForTesting(), 0);
	const double Travel = F.Character->GetActorLocation().X - Start.X;
	TestTrue(FString::Printf(TEXT("Lands on 50 cm (moved %.2f)"), Travel), FMath::IsNearlyEqual(Travel, 50.0, 1.0));

	const TArray<FActionReactionTelemetryRecord> Reached = Combat->GetActionReactionTelemetry().FilterByPredicate(
		[](const FActionReactionTelemetryRecord& Row) { return Row.AlignmentDisposition == FName(TEXT("Reached")); });
	if (TestEqual(TEXT("One Reached row"), Reached.Num(), 1))
	{
		TestEqual(TEXT("The terminal row names the request's owner"), Reached[0].AlignmentOwner, FName(TEXT("TerminalRowTest")));
		TestEqual(TEXT("Event"), Reached[0].Event, EActionReactionTelemetryEvent::AlignmentChanged);
		TestEqual(TEXT("Detail"), Reached[0].Detail, FString(TEXT("DurationReached")));
		// Finite first: this build compares NaN as equal to anything.
		TestTrue(FString::Printf(TEXT("Magnitude is the measured travel (%.2f row, %.2f actor)"), Reached[0].MovementMagnitude, Travel),
			FMath::IsFinite(Reached[0].MovementMagnitude)
			&& FMath::IsNearlyEqual(static_cast<double>(Reached[0].MovementMagnitude), Travel, 1.0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementOwnerReleaseTest, "KatanaCombat.Displacement.Executor.OwnerReleasedRequestHoldsOutcome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementOwnerReleaseTest::RunTest(const FString&)
{
	FDisplacementFixture F;
	FAlignmentRequestSpec Spec = MakePush(30.f, 0.2f);
	Spec.bReleaseWhenFinished = false;
	const FAlignmentRequestHandle Handle = F.Targeting()->AcquireAlignmentRequest(Spec);
	for (int32 I = 0; I < 20; ++I) { F.Step(1.f / 60); }
	FAlignmentMotionState State;
	TestTrue(TEXT("An owner-released request remains after finishing"), F.Targeting()->GetAlignmentMotionState(Handle, State));
	TestEqual(TEXT("Its outcome holds at Reached"), State.Outcome, EAlignmentMotionOutcome::Reached);
	TestFalse(TEXT("The tick stops while only a finished request remains"), F.Targeting()->IsComponentTickEnabled());
	const FVector Held = F.Character->GetActorLocation();
	for (int32 I = 0; I < 10; ++I) { F.Step(1.f / 60); } // forced ticks: the finished request must stay inert
	TestTrue(TEXT("A finished request never moves the owner again"), FVector::Dist2D(F.Character->GetActorLocation(), Held) < 0.5);
	F.Targeting()->ReleaseAlignmentRequest(Handle);
	TestEqual(TEXT("The owner's release removes it"), F.Targeting()->GetAlignmentRequestCountForTesting(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplacementPriorityOrderTest, "KatanaCombat.Displacement.Executor.PriorityOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDisplacementPriorityOrderTest::RunTest(const FString&)
{
	TestTrue(TEXT("Knockback beats the victim's attack warp"),
		static_cast<uint8>(EDefenseAlignmentPriority::HitKnockback) > static_cast<uint8>(EDefenseAlignmentPriority::ActiveAttackWarp));
	TestTrue(TEXT("Block contact beats knockback"),
		static_cast<uint8>(EDefenseAlignmentPriority::BlockContact) > static_cast<uint8>(EDefenseAlignmentPriority::HitKnockback));
	return true;
}
