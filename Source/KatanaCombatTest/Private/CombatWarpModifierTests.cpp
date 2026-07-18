// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Animation/AnimNotifyState_CombatWarp.h"
#include "Animation/AnimMontage.h"
#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/DefenseConfiguration.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"

#include <limits>

namespace
{
FAlignmentRequestSpec MakeOwnedWarpSpec(
	AActor* Target,
	FName OwnerId,
	FName WarpTargetName,
	EDefenseAlignmentPriority Priority,
	float TurnRate = 180.0f,
	float TurnBudget = 70.0f,
	bool bWarpTranslation = false)
{
	FAlignmentRequestSpec Spec;
	Spec.OwnerId = OwnerId;
	Spec.OwnerGeneration = 1;
	Spec.Priority = Priority;
	Spec.Executor = EAlignmentExecutor::MotionWarping;
	Spec.Target = Target;
	Spec.MaximumTurnRate = TurnRate;
	Spec.RemainingTurnBudget = TurnBudget;
	Spec.MaximumTranslation = bWarpTranslation ? 400.0f : 0.0f;
	Spec.WarpTargetName = WarpTargetName;
	Spec.bTrackTargetRotation = Target != nullptr;
	Spec.bWarpTranslation = bWarpTranslation;
	return Spec;
}

URootMotionModifier_Warp* AddCombatWarpModifier(
	UAnimNotifyState_CombatWarp* Notify,
	UMotionWarpingComponent* MotionWarping,
	UAnimSequenceBase* Animation)
{
	return Cast<URootMotionModifier_Warp>(Notify->AddRootMotionModifier_Implementation(
		MotionWarping,
		Animation,
		0.0f,
		1.0f));
}

void UpdateModifier(
	URootMotionModifier_Warp* Modifier,
	UAnimSequenceBase* Animation,
	float PreviousPosition,
	float CurrentPosition,
	float PlayRate,
	float DeltaSeconds)
{
	FMotionWarpingUpdateContext Context;
	Context.Animation = Animation;
	Context.PreviousPosition = PreviousPosition;
	Context.CurrentPosition = CurrentPosition;
	Context.Weight = 1.0f;
	Context.PlayRate = PlayRate;
	Context.DeltaSeconds = DeltaSeconds;
	Modifier->Update(Context);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatWarp_CloneAndRateNormalization,
	"KatanaCombat.Defense.Alignment.CombatWarp.CloneAndRateNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatWarp_CloneAndRateNormalization::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, Combat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(300.0f, 0.0f, 0.0f));
	if (!Player || !Targeting || !Enemy || !Player->MotionWarpingComponent)
	{
		AddError(TEXT("Failed to create combat-warp fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FAlignmentRequestHandle Handle = Targeting->AcquireAlignmentRequest(MakeOwnedWarpSpec(
		Enemy,
		TEXT("RateNormalization"),
		TEXT("AttackTarget"),
		EDefenseAlignmentPriority::ActiveAttackWarp,
		180.0f,
		70.0f,
		true));
	TestTrue(TEXT("Fixture owns the published warp target"), Handle.IsValid());

	UAnimNotifyState_CombatWarp* Notify = NewObject<UAnimNotifyState_CombatWarp>();
	UAnimMontage* Animation = NewObject<UAnimMontage>();
	URootMotionModifier_Warp* Template = Cast<URootMotionModifier_Warp>(Notify->RootMotionModifier);
	if (!Template)
	{
		AddError(TEXT("Combat warp notify did not create a warp template"));
		Targeting->ReleaseAlignmentRequest(Handle);
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FName TemplateTargetName = Template->WarpTargetName;
	const bool bTemplateWarpTranslation = Template->bWarpTranslation;
	const EMotionWarpRotationMethod TemplateRotationMethod = Template->RotationMethod;
	const float TemplateRotationRate = Template->WarpMaxRotationRate;
	URootMotionModifier_Warp* RuntimeModifier = AddCombatWarpModifier(
		Notify, Player->MotionWarpingComponent, Animation);
	TestNotNull(TEXT("Owned target creates a runtime modifier"), RuntimeModifier);
	if (!RuntimeModifier)
	{
		Targeting->ReleaseAlignmentRequest(Handle);
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestTrue(TEXT("Runtime modifier is an engine-owned clone"), RuntimeModifier != Template);
	TestEqual(TEXT("Clone uses the request-owned target"),
		RuntimeModifier->WarpTargetName, FName(TEXT("AttackTarget")));
	TestTrue(TEXT("Clone inherits request translation policy"), RuntimeModifier->bWarpTranslation);
	TestEqual(TEXT("Clone uses the engine constant-rate method"),
		RuntimeModifier->RotationMethod, EMotionWarpRotationMethod::ConstantRate);
	TestEqual(TEXT("Only the clone is registered"),
		Targeting->GetRegisteredAlignmentModifierCountForTesting(), 1);
	URootMotionModifier_Warp* DuplicateModifier = AddCombatWarpModifier(
		Notify, Player->MotionWarpingComponent, Animation);
	TestNull(TEXT("Overlapping windows cannot register two executors for one request"),
		DuplicateModifier);
	TestEqual(TEXT("Duplicate-window rejection preserves the original registration"),
		Targeting->GetRegisteredAlignmentModifierCountForTesting(), 1);

	UpdateModifier(RuntimeModifier, Animation, 0.10f, 0.15f, 0.5f, 0.10f);
	TestEqual(TEXT("Half-rate playback receives the inverse engine cap"),
		RuntimeModifier->WarpMaxRotationRate, 360.0f, 0.1f);
	UpdateModifier(RuntimeModifier, Animation, 0.15f, 0.25f, 1.0f, 0.10f);
	TestEqual(TEXT("Normal playback receives the configured capability"),
		RuntimeModifier->WarpMaxRotationRate, 180.0f, 0.1f);
	UpdateModifier(RuntimeModifier, Animation, 0.25f, 0.45f, 2.0f, 0.10f);
	TestEqual(TEXT("Double-rate playback receives the inverse engine cap"),
		RuntimeModifier->WarpMaxRotationRate, 90.0f, 0.1f);

	World->GetWorldSettings()->SetTimeDilation(0.25f);
	UpdateModifier(RuntimeModifier, Animation, 0.45f, 0.50f, 1.0f, 0.05f);
	TestEqual(TEXT("World dilation does not multiply the configured capability"),
		RuntimeModifier->WarpMaxRotationRate, 180.0f, 0.1f);
	Player->CustomTimeDilation = 0.5f;
	UpdateModifier(RuntimeModifier, Animation, 0.50f, 0.525f, 1.0f, 0.025f);
	TestEqual(TEXT("Actor dilation does not multiply the configured capability"),
		RuntimeModifier->WarpMaxRotationRate, 180.0f, 0.1f);
	UpdateModifier(RuntimeModifier, Animation, 0.525f, 0.525f, -1.0f, 0.0f);
	TestEqual(TEXT("Reverse playback disables the owned modifier"),
		RuntimeModifier->GetState(), ERootMotionModifierState::Disabled);
	TestEqual(TEXT("Disabled playback unregisters only that clone"),
		Targeting->GetRegisteredAlignmentModifierCountForTesting(), 0);
	URootMotionModifier_Warp* PausedModifier = AddCombatWarpModifier(
		Notify, Player->MotionWarpingComponent, Animation);
	TestNotNull(TEXT("The active owner may register a later valid window"), PausedModifier);
	if (PausedModifier)
	{
		UpdateModifier(PausedModifier, Animation, 0.55f, 0.55f, 0.0f, 0.0f);
		TestEqual(TEXT("Zero-rate playback disables the owned modifier"),
			PausedModifier->GetState(), ERootMotionModifierState::Disabled);
		TestEqual(TEXT("Zero-rate playback unregisters only that clone"),
			Targeting->GetRegisteredAlignmentModifierCountForTesting(), 0);
	}

	TestEqual(TEXT("Runtime target configuration never mutates the template target"),
		Template->WarpTargetName, TemplateTargetName);
	TestEqual(TEXT("Runtime translation configuration never mutates the template"),
		Template->bWarpTranslation, bTemplateWarpTranslation);
	TestEqual(TEXT("Runtime rotation method never mutates the template"),
		Template->RotationMethod, TemplateRotationMethod);
	TestEqual(TEXT("Runtime rate changes never mutate the template"),
		Template->WarpMaxRotationRate, TemplateRotationRate);

	Targeting->ReleaseAlignmentRequest(Handle);
	Player->MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
		TEXT("AttackTarget"), FVector::ZeroVector, FRotator::ZeroRotator);
	TestNull(TEXT("An unmanaged target cannot create a combat-warp modifier"),
		AddCombatWarpModifier(Notify, Player->MotionWarpingComponent, Animation));
	TestEqual(TEXT("Unmanaged target failure creates no registry entry"),
		Targeting->GetRegisteredAlignmentModifierCountForTesting(), 0);
	Player->MotionWarpingComponent->RemoveWarpTarget(TEXT("AttackTarget"));
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatWarp_SuspensionBudgetAndOwnerRelease,
	"KatanaCombat.Defense.Alignment.CombatWarp.SuspensionBudgetAndOwnerRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatWarp_SuspensionBudgetAndOwnerRelease::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, Combat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(300.0f, 0.0f, 0.0f));
	if (!Player || !Targeting || !Enemy || !Player->MotionWarpingComponent
		|| !Player->GetCharacterMovement())
	{
		AddError(TEXT("Failed to create combat-warp ownership fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Player->bUseControllerRotationYaw = true;
	Movement->bOrientRotationToMovement = true;
	Movement->bUseControllerDesiredRotation = false;
	const FAlignmentRequestHandle Attack = Targeting->AcquireAlignmentRequest(MakeOwnedWarpSpec(
		Enemy,
		TEXT("AttackOwner"),
		TEXT("AttackTarget"),
		EDefenseAlignmentPriority::ActiveAttackWarp,
		120.0f,
		70.0f));
	UAnimNotifyState_CombatWarp* AttackNotify = NewObject<UAnimNotifyState_CombatWarp>();
	UAnimMontage* Animation = NewObject<UAnimMontage>();
	URootMotionModifier_Warp* AttackModifier = AddCombatWarpModifier(
		AttackNotify, Player->MotionWarpingComponent, Animation);
	if (!Attack.IsValid() || !AttackModifier)
	{
		AddError(TEXT("Failed to register lower-priority attack modifier"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	UpdateModifier(AttackModifier, Animation, 0.10f, 0.20f, 1.0f, 0.10f);
	TestEqual(TEXT("Lower-priority modifier activates"),
		AttackModifier->GetState(), ERootMotionModifierState::Active);

	const FAlignmentRequestHandle Contact = Targeting->AcquireAlignmentRequest(MakeOwnedWarpSpec(
		Enemy,
		TEXT("ContactOwner"),
		TEXT("DefenseContactTarget"),
		EDefenseAlignmentPriority::BlockContact,
		90.0f,
		40.0f));
	TestEqual(TEXT("Higher priority suspends the lower modifier"),
		AttackModifier->GetState(), ERootMotionModifierState::Disabled);
	TestEqual(TEXT("Suspension retains clone registration for possible resume"),
		Targeting->GetRegisteredAlignmentModifierCountForTesting(), 1);
	Targeting->ReleaseAlignmentRequest(Contact);
	TestEqual(TEXT("Removing the higher owner returns the lower modifier to waiting"),
		AttackModifier->GetState(), ERootMotionModifierState::Waiting);
	UpdateModifier(AttackModifier, Animation, 0.20f, 0.30f, 1.0f, 0.10f);
	TestEqual(TEXT("A still-valid lower modifier resumes"),
		AttackModifier->GetState(), ERootMotionModifierState::Active);

	Player->SetActorRotation(FRotator(0.0, 20.0, 0.0));
	UpdateModifier(AttackModifier, Animation, 0.30f, 0.40f, 1.0f, 0.10f);
	FAlignmentRequestSpec UpdatedAttackSpec;
	TestTrue(TEXT("Resumed request remains queryable"),
		Targeting->GetAlignmentRequestSpec(Attack, UpdatedAttackSpec));
	TestEqual(TEXT("Observed composed yaw spends the request budget"),
		UpdatedAttackSpec.RemainingTurnBudget, 50.0f, 0.1f);

	const FAlignmentRequestHandle ContactAgain = Targeting->AcquireAlignmentRequest(MakeOwnedWarpSpec(
		Enemy,
		TEXT("ContactOwnerAgain"),
		TEXT("DefenseContactTarget"),
		EDefenseAlignmentPriority::BlockContact,
		90.0f,
		40.0f));
	UAnimNotifyState_CombatWarp* ContactNotify = NewObject<UAnimNotifyState_CombatWarp>();
	ContactNotify->TargetWarpName = TEXT("DefenseContactTarget");
	ContactNotify->RotationWarpName = TEXT("UnusedContactRotation");
	URootMotionModifier_Warp* ContactModifier = AddCombatWarpModifier(
		ContactNotify, Player->MotionWarpingComponent, Animation);
	if (!ContactAgain.IsValid() || !ContactModifier)
	{
		AddError(TEXT("Failed to register higher-priority contact modifier"));
		Targeting->ReleaseAllAlignmentRequests(EAlignmentReleaseReason::Death);
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	UpdateModifier(ContactModifier, Animation, 0.40f, 0.50f, 1.0f, 0.10f);
	Targeting->ReleaseAlignmentRequest(Attack);
	TestEqual(TEXT("Lower-owner release removes only its clone"),
		Targeting->GetRegisteredAlignmentModifierCountForTesting(), 1);
	TestEqual(TEXT("Released lower clone is marked for engine removal"),
		AttackModifier->GetState(), ERootMotionModifierState::MarkedForRemoval);
	TestTrue(TEXT("Unrelated higher modifier remains usable"),
		ContactModifier->GetState() != ERootMotionModifierState::MarkedForRemoval);
	TestNotNull(TEXT("Unrelated higher target survives lower-owner release"),
		Player->MotionWarpingComponent->FindWarpTarget(TEXT("DefenseContactTarget")));

	Targeting->ReleaseAlignmentRequest(ContactAgain);
	TestEqual(TEXT("Last owner release unregisters the final clone"),
		Targeting->GetRegisteredAlignmentModifierCountForTesting(), 0);
	TestEqual(TEXT("Last owner release marks its clone for removal"),
		ContactModifier->GetState(), ERootMotionModifierState::MarkedForRemoval);
	TestTrue(TEXT("Last owner release restores controller yaw"), Player->bUseControllerRotationYaw);
	TestTrue(TEXT("Last owner release restores movement orientation"),
		Movement->bOrientRotationToMovement);
	TestFalse(TEXT("Last owner release restores desired rotation exactly"),
		Movement->bUseControllerDesiredRotation);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatWarp_TerminalFrameBudgetAndSuccessor,
	"KatanaCombat.Defense.Alignment.CombatWarp.TerminalFrameBudgetAndSuccessor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatWarp_TerminalFrameBudgetAndSuccessor::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, Combat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(300.0f, 0.0f, 0.0f));
	if (!Player || !Targeting || !Enemy || !Player->MotionWarpingComponent)
	{
		AddError(TEXT("Failed to create terminal-frame combat-warp fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FAlignmentRequestHandle Handle = Targeting->AcquireAlignmentRequest(MakeOwnedWarpSpec(
		Enemy,
		TEXT("TerminalFrameOwner"),
		TEXT("AttackTarget"),
		EDefenseAlignmentPriority::ActiveAttackWarp,
		180.0f,
		70.0f));
	UAnimNotifyState_CombatWarp* Notify = NewObject<UAnimNotifyState_CombatWarp>();
	UAnimMontage* Animation = NewObject<UAnimMontage>();
	URootMotionModifier_Warp* Modifier = AddCombatWarpModifier(
		Notify, Player->MotionWarpingComponent, Animation);
	if (!Handle.IsValid() || !Modifier)
	{
		AddError(TEXT("Failed to register terminal-frame combat-warp modifier"));
		Targeting->ReleaseAlignmentRequest(Handle);
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UpdateModifier(Modifier, Animation, 0.0f, 0.1f, 1.0f, 0.1f);
	Player->SetActorRotation(FRotator(0.0f, 20.0f, 0.0f));
	UpdateModifier(Modifier, Animation, 0.1f, 0.2f, 1.0f, 0.1f);
	FAlignmentRequestSpec RequestSpec;
	TestTrue(TEXT("Terminal-frame request remains queryable before deactivation"),
		Targeting->GetAlignmentRequestSpec(Handle, RequestSpec));
	TestEqual(TEXT("Observed update spends the first twenty degrees"),
		RequestSpec.RemainingTurnBudget, 50.0f, 0.1f);

	Player->SetActorRotation(FRotator(0.0f, 35.0f, 0.0f));
	Modifier->SetState(ERootMotionModifierState::MarkedForRemoval);
	TestEqual(TEXT("Terminal deactivation unregisters the finished modifier"),
		Targeting->GetRegisteredAlignmentModifierCountForTesting(), 0);
	TestTrue(TEXT("Request survives terminal modifier deactivation"),
		Targeting->GetAlignmentRequestSpec(Handle, RequestSpec));
	TestEqual(TEXT("Terminal frame spends its final fifteen degrees"),
		RequestSpec.RemainingTurnBudget, 35.0f, 0.1f);

	URootMotionModifier_Warp* Successor = AddCombatWarpModifier(
		Notify, Player->MotionWarpingComponent, Animation);
	TestNotNull(TEXT("Active owner may register a successor modifier"), Successor);
	if (Successor)
	{
		UpdateModifier(Successor, Animation, 0.2f, 0.4f, 1.0f, 0.2f);
		TestEqual(TEXT("Successor cap uses the reconciled remaining budget"),
			Successor->WarpMaxRotationRate, 175.0f, 0.1f);
	}

	Targeting->ReleaseAlignmentRequest(Handle);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatWarp_SetupAttackWarpOwnedLifecycle,
	"KatanaCombat.Defense.Alignment.CombatWarp.SetupAttackWarpOwnedLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatWarp_SetupAttackWarpOwnedLifecycle::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, Combat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(300.0f, 0.0f, 0.0f));
	if (!Player || !Combat || !Targeting || !Enemy || !Player->MotionWarpingComponent)
	{
		AddError(TEXT("Failed to create SetupAttackWarp fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UDefenseConfiguration* DefenseConfig = NewObject<UDefenseConfiguration>();
	DefenseConfig->DefenseTurnRate = 180.0f;
	DefenseConfig->MaximumAutomaticTurn = 70.0f;
	Combat->DefenseConfigurationOverride = DefenseConfig;
	FAttackWarpConfig Config;
	Config.bEnableWarp = true;
	Config.MinWarpDistance = 50.0f;
	Config.MaxWarpDistance = 400.0f;
	Config.RotationSpeed = 720.0f;
	Config.TargetRelativeOffset = FVector(25.0f, -40.0f, 0.0f);
	Enemy->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));

	Combat->SetPhase(EAttackPhase::Windup);
	TestTrue(TEXT("Targeted attack warp acquires arbiter ownership"),
		Targeting->SetupAttackWarp(Enemy, FRotator::ZeroRotator, Config));
	TestEqual(TEXT("SetupAttackWarp owns exactly one request"),
		Targeting->GetAlignmentRequestCountForTesting(), 1);
	FAlignmentRequestSpec ActiveSpec;
	TestTrue(TEXT("Active attack-warp request is queryable"),
		Targeting->GetAlignmentRequestSpec(Targeting->GetActiveAlignmentRequest(), ActiveSpec));
	TestEqual(TEXT("Attack warp cannot raise the defense turn capability"),
		ActiveSpec.MaximumTurnRate, 180.0f, 0.1f);
	TestEqual(TEXT("Attack warp inherits the cumulative automatic-turn budget"),
		ActiveSpec.RemainingTurnBudget, 70.0f, 0.1f);
	TestEqual(TEXT("Attack warp uses its explicit arbiter priority"),
		ActiveSpec.Priority, EDefenseAlignmentPriority::ActiveAttackWarp);
	TestTrue(TEXT("Attack warp preserves the authored target-relative contact offset"),
		ActiveSpec.TargetRelativeOffset.Equals(Config.TargetRelativeOffset, 0.01f));
	const FMotionWarpingTarget* PublishedTarget =
		Player->MotionWarpingComponent->FindWarpTarget(Config.TargetWarpName);
	TestNotNull(TEXT("Owned targeted request publishes its target"), PublishedTarget);
	if (PublishedTarget)
	{
		const FVector ExpectedContactPoint = Enemy->GetActorLocation()
			+ Enemy->GetActorRotation().RotateVector(Config.TargetRelativeOffset);
		TestTrue(TEXT("Published warp target rotates the local offset into world X"),
			FMath::IsNearlyEqual(PublishedTarget->Location.X, ExpectedContactPoint.X, 0.01));
		TestTrue(TEXT("Published warp target rotates the local offset into world Y"),
			FMath::IsNearlyEqual(PublishedTarget->Location.Y, ExpectedContactPoint.Y, 0.01));
		const double ExpectedFacingYaw =
			(Enemy->GetActorLocation() - Player->GetActorLocation()).Rotation().Yaw;
		TestTrue(TEXT("Contact offset does not skew attacker facing away from the target actor"),
			FMath::Abs(FMath::FindDeltaAngleDegrees(
				PublishedTarget->Rotation.Yaw, ExpectedFacingYaw)) <= 0.01);
	}

	Config.RotationSpeed = 90.0f;
	TestTrue(TEXT("A replacement rotation-only warp releases the previous owner"),
		Targeting->SetupAttackWarp(nullptr, FRotator(0.0, 45.0, 0.0), Config));
	TestEqual(TEXT("Replacement does not leak an additional request"),
		Targeting->GetAlignmentRequestCountForTesting(), 1);
	TestTrue(TEXT("Replacement request is queryable"),
		Targeting->GetAlignmentRequestSpec(Targeting->GetActiveAlignmentRequest(), ActiveSpec));
	TestEqual(TEXT("An attack may lower the effective turn capability"),
		ActiveSpec.MaximumTurnRate, 90.0f, 0.1f);
	TestNull(TEXT("Replacement removes the prior owner's target"),
		Player->MotionWarpingComponent->FindWarpTarget(Config.TargetWarpName));
	TestNotNull(TEXT("Replacement publishes only its rotation target"),
		Player->MotionWarpingComponent->FindWarpTarget(Config.RotationWarpName));

	Combat->SetPhase(EAttackPhase::None);
	TestEqual(TEXT("Canonical attack termination releases attack-warp ownership"),
		Targeting->GetAlignmentRequestCountForTesting(), 0);
	TestNull(TEXT("Canonical attack termination removes the owned target"),
		Player->MotionWarpingComponent->FindWarpTarget(Config.RotationWarpName));

	Config.TargetRelativeOffset.X = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("A non-finite contact offset cannot acquire attack-warp ownership"),
		Targeting->SetupAttackWarp(Enemy, FRotator::ZeroRotator, Config));
	TestEqual(TEXT("Rejected contact offset leaves no alignment request"),
		Targeting->GetAlignmentRequestCountForTesting(), 0);
	TestNull(TEXT("Rejected contact offset publishes no warp target"),
		Player->MotionWarpingComponent->FindWarpTarget(Config.TargetWarpName));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
