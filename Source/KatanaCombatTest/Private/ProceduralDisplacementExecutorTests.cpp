#include "CombatTestHelpers.h"
#include "Core/TargetingComponent.h"
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
	FDisplacementFixture F;
	const FVector Start = F.Character->GetActorLocation();
	F.Targeting()->AcquireAlignmentRequest(MakePush(40.f, 0.2f));
	F.Step(1.f / 60);
	FAlignmentRequestSpec Block;
	Block.OwnerId = TEXT("BlockTest"); Block.OwnerGeneration = 1;
	Block.Priority = EDefenseAlignmentPriority::BlockContact;
	Block.Executor = EAlignmentExecutor::CharacterMovement;
	Block.DesiredRotation = FRotator::ZeroRotator; Block.MaximumTurnRate = 90.f; Block.RemainingTurnBudget = 10.f;
	const FAlignmentRequestHandle BlockHandle = F.Targeting()->AcquireAlignmentRequest(Block);
	if (!TestTrue(TEXT("Block request acquired"), BlockHandle.IsValid())) { return false; }
	const double Suspended = F.Character->GetActorLocation().X;
	for (int32 I = 0; I < 10; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("No push while suspended"), FMath::Abs(F.Character->GetActorLocation().X - Suspended) < 1.0);
	// A removed source is only marked; the next movement tick drops it, so check after stepping.
	TestFalse(TEXT("Suspended push has no source"), F.Movement()->GetRootMotionSource(TEXT("KatanaProceduralDisplacement")).IsValid());
	F.Targeting()->ReleaseAlignmentRequest(BlockHandle);
	for (int32 I = 0; I < 30; ++I) { F.Step(1.f / 60); }
	TestTrue(TEXT("Push resumes and completes"), FMath::IsNearlyEqual(F.Character->GetActorLocation().X - Start.X, 40.0, 4.0));
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
