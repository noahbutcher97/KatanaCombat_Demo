// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/AttackData.h"
#include "Data/DefenseConfiguration.h"
#include "Data/TargetingSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "MotionWarpingComponent.h"

#include <limits>

namespace
{
FAlignmentRequestSpec MakeSmoothSpec(AActor* Target, float TurnRate, float TurnBudget)
{
	FAlignmentRequestSpec Spec;
	Spec.OwnerId = TEXT("TestSmooth");
	Spec.OwnerGeneration = 1;
	Spec.Priority = EDefenseAlignmentPriority::GuardFacing;
	Spec.Executor = EAlignmentExecutor::CharacterMovement;
	Spec.Target = Target;
	Spec.MaximumTurnRate = TurnRate;
	Spec.RemainingTurnBudget = TurnBudget;
	return Spec;
}

FAlignmentRequestSpec MakeWarpSpec(
	AActor* Target,
	FName OwnerId,
	FName WarpTargetName,
	EDefenseAlignmentPriority Priority)
{
	FAlignmentRequestSpec Spec;
	Spec.OwnerId = OwnerId;
	Spec.OwnerGeneration = 1;
	Spec.Priority = Priority;
	Spec.Executor = EAlignmentExecutor::MotionWarping;
	Spec.Target = Target;
	Spec.MaximumTurnRate = 90.0f;
	Spec.RemainingTurnBudget = 70.0f;
	Spec.WarpTargetName = WarpTargetName;
	return Spec;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAlignment_PriorityAndRestoration,
	"KatanaCombat.Defense.Alignment.PriorityAndRestoration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseAlignment_PriorityAndRestoration::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, Combat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(0.0f, 300.0f, 0.0f));
	if (!Player || !Targeting || !Enemy || !Player->GetCharacterMovement())
	{
		AddError(TEXT("Failed to create alignment priority fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Player->bUseControllerRotationYaw = true;
	Movement->bOrientRotationToMovement = true;
	Movement->bUseControllerDesiredRotation = false;

	const FAlignmentRequestHandle Guard = Targeting->AcquireAlignmentRequest(
		MakeSmoothSpec(Enemy, 90.0f, 70.0f));
	TestTrue(TEXT("Valid smooth request receives an opaque handle"), Guard.IsValid());
	TestTrue(TEXT("First request becomes active"),
		Targeting->GetActiveAlignmentRequest() == Guard);
	TestTrue(TEXT("Smooth ownership enables targeting tick"), Targeting->IsComponentTickEnabled());
	TestFalse(TEXT("Alignment temporarily disables controller actor yaw"),
		Player->bUseControllerRotationYaw);
	TestFalse(TEXT("Alignment temporarily disables movement orientation"),
		Movement->bOrientRotationToMovement);
	TestFalse(TEXT("Alignment keeps controller-desired rotation disabled"),
		Movement->bUseControllerDesiredRotation);
	FAlignmentRequestSpec EqualPrioritySpec = MakeSmoothSpec(Enemy, 90.0f, 70.0f);
	EqualPrioritySpec.OwnerId = TEXT("NewestGuard");
	const FAlignmentRequestHandle NewestGuard = Targeting->AcquireAlignmentRequest(EqualPrioritySpec);
	TestTrue(TEXT("Newest equal-priority request wins deterministically"),
		Targeting->GetActiveAlignmentRequest() == NewestGuard);
	Targeting->ReleaseAlignmentRequest(NewestGuard);
	TestTrue(TEXT("Releasing equal-priority request resumes prior owner"),
		Targeting->GetActiveAlignmentRequest() == Guard);
	FAlignmentRequestSpec EscalatedGuard = MakeSmoothSpec(Enemy, 90.0f, 70.0f);
	EscalatedGuard.Priority = EDefenseAlignmentPriority::Terminal;
	TestFalse(TEXT("An update cannot escalate an existing owner's priority"),
		Targeting->UpdateAlignmentRequest(Guard, EscalatedGuard));
	FAlignmentRequestSpec InvalidOffset = MakeWarpSpec(
		Enemy,
		TEXT("InvalidOffset"),
		TEXT("InvalidOffsetTarget"),
		EDefenseAlignmentPriority::PairedOrParryBridge);
	InvalidOffset.TargetRelativeOffset.X = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("The alignment boundary rejects a nonfinite target-relative offset"),
		Targeting->AcquireAlignmentRequest(InvalidOffset).IsValid());

	const FAlignmentRequestHandle Bridge = Targeting->AcquireAlignmentRequest(MakeWarpSpec(
		Enemy,
		TEXT("TestBridge"),
		TEXT("TestBridgeTarget"),
		EDefenseAlignmentPriority::PairedOrParryBridge));
	TestTrue(TEXT("Higher-priority warp receives an opaque handle"), Bridge.IsValid());
	TestEqual(TEXT("Higher priority suspends rather than destroys guard"),
		Targeting->GetAlignmentRequestCountForTesting(), 2);
	TestTrue(TEXT("Higher-priority request becomes active"),
		Targeting->GetActiveAlignmentRequest() == Bridge);
	TestNotNull(TEXT("Active warp owns its named target"),
		Player->MotionWarpingComponent->FindWarpTarget(TEXT("TestBridgeTarget")));
	Targeting->ClearMotionWarp();
	TestNotNull(TEXT("Non-terminal broad clear cannot erase an owned target"),
		Player->MotionWarpingComponent->FindWarpTarget(TEXT("TestBridgeTarget")));
	Targeting->ClearMotionWarp(TEXT("TestBridgeTarget"));
	TestNotNull(TEXT("Legacy named clear cannot erase an owned target"),
		Player->MotionWarpingComponent->FindWarpTarget(TEXT("TestBridgeTarget")));

	Targeting->ReleaseAlignmentRequest(Guard);
	TestEqual(TEXT("Owner release removes only its request"),
		Targeting->GetAlignmentRequestCountForTesting(), 1);
	TestFalse(TEXT("Motion-warp-only ownership leaves targeting tick disabled"),
		Targeting->IsComponentTickEnabled());
	TestTrue(TEXT("Unrelated active request survives owner release"),
		Targeting->GetActiveAlignmentRequest() == Bridge);

	Targeting->ReleaseAllAlignmentRequests(EAlignmentReleaseReason::OwnerCancelled);
	TestEqual(TEXT("Non-terminal broad release is rejected"),
		Targeting->GetAlignmentRequestCountForTesting(), 1);
	Targeting->ReleaseAlignmentRequest(Bridge);
	TestEqual(TEXT("Last owner release empties request set"),
		Targeting->GetAlignmentRequestCountForTesting(), 0);
	TestTrue(TEXT("Controller actor yaw restores exactly"), Player->bUseControllerRotationYaw);
	TestTrue(TEXT("Movement orientation restores exactly"), Movement->bOrientRotationToMovement);
	TestFalse(TEXT("Controller-desired rotation restores exactly"),
		Movement->bUseControllerDesiredRotation);
	TestNull(TEXT("Owner release removes only its warp target"),
		Player->MotionWarpingComponent->FindWarpTarget(TEXT("TestBridgeTarget")));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAlignment_CappedSweptYaw,
	"KatanaCombat.Defense.Alignment.CappedSweptYawAndBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseAlignment_CappedSweptYaw::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, Combat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(0.0f, 300.0f, 0.0f));
	if (!Player || !Targeting || !Enemy)
	{
		AddError(TEXT("Failed to create capped-yaw fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Player->SetActorRotation(FRotator::ZeroRotator);
	FAlignmentRequestSpec Spec = MakeSmoothSpec(Enemy, 30.0f, 40.0f);
	const FAlignmentRequestHandle Handle = Targeting->AcquireAlignmentRequest(Spec);
	Targeting->ResetAlignmentExecutionFrameForTesting();
	Targeting->TickComponent(0.5f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("First smooth step is rate capped"),
		Player->GetActorRotation().Yaw, 15.0, 0.1);
	const int32 FirstExecutionCount = Targeting->GetAlignmentExecutionCountForTesting();
	Targeting->TickComponent(0.5f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Only one executor may rotate in a frame"),
		Targeting->GetAlignmentExecutionCountForTesting(), FirstExecutionCount);
	TestEqual(TEXT("Second same-frame call cannot add yaw"),
		Player->GetActorRotation().Yaw, 15.0, 0.1);

	Targeting->ResetAlignmentExecutionFrameForTesting();
	Targeting->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Cumulative interaction budget clamps later step"),
		Player->GetActorRotation().Yaw, 40.0, 0.1);
	FAlignmentRequestSpec Updated;
	TestTrue(TEXT("Owned request remains queryable"),
		Targeting->GetAlignmentRequestSpec(Handle, Updated));
	TestEqual(TEXT("Actual applied yaw spends the budget"), Updated.RemainingTurnBudget, 0.0f, 0.1f);

	Spec.RemainingTurnBudget = 40.0f;
	TestTrue(TEXT("Owner may update its request"), Targeting->UpdateAlignmentRequest(Handle, Spec));
	Targeting->GetAlignmentRequestSpec(Handle, Updated);
	TestEqual(TEXT("Refresh cannot reset an existing interaction budget"),
		Updated.RemainingTurnBudget, 0.0f, 0.1f);
	Targeting->ResetAlignmentExecutionFrameForTesting();
	Targeting->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Exhausted budget prevents catch-up rotation"),
		Player->GetActorRotation().Yaw, 40.0, 0.1);

	Targeting->ReleaseAlignmentRequest(Handle);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAlignment_WarpTargetOwnership,
	"KatanaCombat.Defense.Alignment.WarpTargetOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseAlignment_WarpTargetOwnership::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, Combat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(200.0f, 0.0f, 0.0f));
	if (!Player || !Targeting || !Enemy || !Player->MotionWarpingComponent)
	{
		AddError(TEXT("Failed to create warp-ownership fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FAlignmentRequestHandle Attack = Targeting->AcquireAlignmentRequest(MakeWarpSpec(
		Enemy, TEXT("Attack"), TEXT("OwnedAttackTarget"),
		EDefenseAlignmentPriority::ActiveAttackWarp));
	TestNotNull(TEXT("Initial active request publishes its target"),
		Player->MotionWarpingComponent->FindWarpTarget(TEXT("OwnedAttackTarget")));
	const FAlignmentRequestHandle Contact = Targeting->AcquireAlignmentRequest(MakeWarpSpec(
		Enemy, TEXT("Contact"), TEXT("OwnedContactTarget"),
		EDefenseAlignmentPriority::BlockContact));
	TestNull(TEXT("Suspended request withdraws its target"),
		Player->MotionWarpingComponent->FindWarpTarget(TEXT("OwnedAttackTarget")));
	TestNotNull(TEXT("Higher request publishes only its own target"),
		Player->MotionWarpingComponent->FindWarpTarget(TEXT("OwnedContactTarget")));

	const FAlignmentRequestHandle Duplicate = Targeting->AcquireAlignmentRequest(MakeWarpSpec(
		Enemy, TEXT("Duplicate"), TEXT("OwnedContactTarget"),
		EDefenseAlignmentPriority::Terminal));
	TestFalse(TEXT("A warp target name cannot have two owners"), Duplicate.IsValid());
	Player->MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
		TEXT("UnmanagedTarget"), FVector::ZeroVector, FRotator::ZeroRotator);
	const FAlignmentRequestHandle UnmanagedCollision = Targeting->AcquireAlignmentRequest(MakeWarpSpec(
		Enemy, TEXT("UnmanagedCollision"), TEXT("UnmanagedTarget"),
		EDefenseAlignmentPriority::Terminal));
	TestFalse(TEXT("An unmanaged pre-existing warp target cannot be claimed"),
		UnmanagedCollision.IsValid());

	Targeting->ReleaseAlignmentRequest(Contact);
	TestNull(TEXT("Released target is removed"),
		Player->MotionWarpingComponent->FindWarpTarget(TEXT("OwnedContactTarget")));
	TestNotNull(TEXT("Suspended request resumes with its own target"),
		Player->MotionWarpingComponent->FindWarpTarget(TEXT("OwnedAttackTarget")));

	Targeting->ReleaseAllAlignmentRequests(EAlignmentReleaseReason::Death);
	TestEqual(TEXT("Terminal broad release clears registered owners"),
		Targeting->GetAlignmentRequestCountForTesting(), 0);
	TestNull(TEXT("Terminal release removes registered target names"),
		Player->MotionWarpingComponent->FindWarpTarget(TEXT("OwnedAttackTarget")));
	TestNotNull(TEXT("Terminal alignment release preserves unmanaged target names"),
		Player->MotionWarpingComponent->FindWarpTarget(TEXT("UnmanagedTarget")));
	TestTrue(TEXT("Handles are structurally distinct"), Attack != Contact);
	Player->MotionWarpingComponent->RemoveWarpTarget(TEXT("UnmanagedTarget"));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAlignment_InvalidTargetCleanup,
	"KatanaCombat.Defense.Alignment.InvalidTargetCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseAlignment_InvalidTargetCleanup::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, Combat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(0.0f, 300.0f, 0.0f));
	if (!Player || !Targeting || !Enemy)
	{
		AddError(TEXT("Failed to create invalid-target fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FAlignmentRequestHandle Handle = Targeting->AcquireAlignmentRequest(
		MakeSmoothSpec(Enemy, 90.0f, 70.0f));
	TestTrue(TEXT("Fixture owns a smooth request"), Handle.IsValid());
	World->DestroyActor(Enemy);
	Targeting->ResetAlignmentExecutionFrameForTesting();
	Targeting->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Invalid target releases its request before execution"),
		Targeting->GetAlignmentRequestCountForTesting(), 0);
	TestFalse(TEXT("Invalid target cleanup disables targeting tick"),
		Targeting->IsComponentTickEnabled());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAlignment_DeathPathCleanup,
	"KatanaCombat.Defense.Alignment.DeathPathCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseAlignment_DeathPathCleanup::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, Combat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(0.0f, 300.0f, 0.0f));
	if (!Player || !Targeting || !Enemy || !Player->GetCharacterMovement())
	{
		AddError(TEXT("Failed to create death-path fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Player->bUseControllerRotationYaw = true;
	Movement->bOrientRotationToMovement = true;
	Movement->bUseControllerDesiredRotation = false;
	TestTrue(TEXT("Fixture owns a smooth request"),
		Targeting->AcquireAlignmentRequest(MakeSmoothSpec(Enemy, 90.0f, 70.0f)).IsValid());
	TestTrue(TEXT("Lethal damage reaches the production death path"),
		FCombatTestHelpers::DealLethalDamage(Player, Enemy));
	TestEqual(TEXT("Production death clears every alignment request"),
		Targeting->GetAlignmentRequestCountForTesting(), 0);
	TestTrue(TEXT("Production death restores controller yaw"), Player->bUseControllerRotationYaw);
	TestTrue(TEXT("Production death restores movement orientation"),
		Movement->bOrientRotationToMovement);
	TestFalse(TEXT("Production death restores desired rotation exactly"),
		Movement->bUseControllerDesiredRotation);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAlignment_GuardUsesOwnedSmoothRequest,
	"KatanaCombat.Defense.Alignment.GuardUsesOwnedSmoothRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseAlignment_GuardUsesOwnedSmoothRequest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* DefenderCombat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Defender = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, DefenderCombat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(0.0f, 300.0f, 0.0f));
	UCombatComponent* AttackerCombat = Enemy ? Enemy->CombatComponent.Get() : nullptr;
	if (!Defender || !DefenderCombat || !Targeting || !Enemy || !AttackerCombat)
	{
		AddError(TEXT("Failed to create guard-alignment fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingSettings* TargetingSettings = NewObject<UTargetingSettings>();
	TargetingSettings->MaxTargetDistance = 1000.0f;
	TargetingSettings->bRequireLineOfSight = false;
	Targeting->TargetingSettingsOverride = TargetingSettings;
	UDefenseConfiguration* DefenseConfig = NewObject<UDefenseConfiguration>();
	DefenseConfig->DefenseTurnRate = 90.0f;
	DefenseConfig->MaximumAutomaticTurn = 70.0f;
	DefenderCombat->DefenseConfigurationOverride = DefenseConfig;

	AttackerCombat->CurrentAttackData = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	AttackerCombat->CurrentPhase = EAttackPhase::Windup;
	AttackerCombat->AttackStateMachine.AttackGeneration = 77;
	AttackerCombat->SetAttackIntentTarget(Defender);
	FAttackThreatPrediction Prediction;
	Prediction.IntendedTarget = Defender;
	Prediction.PathOrigin = Enemy->GetActorLocation();
	Prediction.PathDirection = (Defender->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal();
	Prediction.PredictedContactPoint = Defender->GetActorLocation();
	Prediction.PredictionSimulationTimestamp = World->GetTimeSeconds();
	Prediction.PredictedContactSimulationTime = World->GetTimeSeconds() + 0.5;
	Prediction.Confidence = EDefensePredictionConfidence::Low;
	Prediction.bPathIntersectsThreatVolume = true;
	AttackerCombat->PublishAttackThreatPrediction(Prediction);

	Defender->SetActorRotation(FRotator::ZeroRotator);
	TestTrue(TEXT("Held guard starts"), DefenderCombat->BeginBlock());
	TestEqual(TEXT("Guard entry performs no direct rotation snap"),
		Defender->GetActorRotation().Yaw, 0.0, 0.1);
	TestEqual(TEXT("Guard owns one alignment request"),
		Targeting->GetAlignmentRequestCountForTesting(), 1);
	Targeting->ResetAlignmentExecutionFrameForTesting();
	Targeting->TickComponent(0.10f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Guard turn uses configured capped rate"),
		Defender->GetActorRotation().Yaw, 9.0, 0.1);

	DefenderCombat->EndBlock();
	TestEqual(TEXT("Guard release relinquishes alignment ownership"),
		Targeting->GetAlignmentRequestCountForTesting(), 0);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAlignment_GuardManualOverridePreservesBudget,
	"KatanaCombat.Defense.Alignment.GuardManualOverridePreservesBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseAlignment_GuardManualOverridePreservesBudget::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* DefenderCombat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Defender = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, DefenderCombat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(0.0f, 300.0f, 0.0f));
	UCombatComponent* AttackerCombat = Enemy ? Enemy->CombatComponent.Get() : nullptr;
	if (!Defender || !DefenderCombat || !Targeting || !Enemy || !AttackerCombat)
	{
		AddError(TEXT("Failed to create guard-manual-override fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingSettings* TargetingSettings = NewObject<UTargetingSettings>();
	TargetingSettings->MaxTargetDistance = 1000.0f;
	TargetingSettings->bRequireLineOfSight = false;
	Targeting->TargetingSettingsOverride = TargetingSettings;
	UDefenseConfiguration* DefenseConfig = NewObject<UDefenseConfiguration>();
	DefenseConfig->DefenseTurnRate = 90.0f;
	DefenseConfig->MaximumAutomaticTurn = 70.0f;
	DefenseConfig->GuardManualOverrideThreshold = 0.25f;
	DefenseConfig->GuardAutoFacingResumeSeconds = 0.10f;
	DefenderCombat->DefenseConfigurationOverride = DefenseConfig;

	AttackerCombat->CurrentAttackData = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	AttackerCombat->CurrentPhase = EAttackPhase::Windup;
	AttackerCombat->AttackStateMachine.AttackGeneration = 78;
	AttackerCombat->SetAttackIntentTarget(Defender);

	Defender->SetActorRotation(FRotator::ZeroRotator);
	TestTrue(TEXT("Held guard starts"), DefenderCombat->BeginBlock());
	TestFalse(TEXT("Guard resume cannot depend on the disabled combat tick"),
		DefenderCombat->IsComponentTickEnabled());
	const FCombatantStableId LockedThreatId = DefenderCombat->GetLockedDefenseThreat().StableId;
	const FAlignmentRequestHandle GuardHandle = Targeting->GetActiveAlignmentRequest();
	TestTrue(TEXT("Guard request is active before manual input"), GuardHandle.IsValid());

	DefenderCombat->SetDefenseManualYawInputForTesting(-1.0f, 10.0);
	TestTrue(TEXT("Manual override retains the same guard request"),
		Targeting->GetActiveAlignmentRequest() == GuardHandle);
	Targeting->ResetAlignmentExecutionFrameForTesting();
	Targeting->TickComponent(0.10f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Manual guard turn uses the configured final-actor rate cap"),
		Defender->GetActorRotation().Yaw, -9.0, 0.1);

	FAlignmentRequestSpec ManualSpec;
	TestTrue(TEXT("Manual guard request remains queryable"),
		Targeting->GetAlignmentRequestSpec(GuardHandle, ManualSpec));
	TestFalse(TEXT("Manual override suspends automatic target tracking"),
		ManualSpec.bTrackTargetRotation);
	TestEqual(TEXT("Manual yaw spends the existing interaction budget"),
		ManualSpec.RemainingTurnBudget, 61.0f, 0.1f);

	DefenderCombat->SetDefenseManualYawInputForTesting(0.0f, 10.0);
	TestTrue(TEXT("Dropping manual input schedules one unscaled resume callback"),
		DefenderCombat->GuardManualResumeTickerHandle.IsValid());
	DefenderCombat->SetDefenseManualYawInputForTesting(0.0f, 10.09);
	Targeting->GetAlignmentRequestSpec(GuardHandle, ManualSpec);
	TestFalse(TEXT("Auto-facing remains suspended before the unscaled delay"),
		ManualSpec.bTrackTargetRotation);

	DefenderCombat->SetDefenseManualYawInputForTesting(0.0f, 10.11);
	TestFalse(TEXT("Manual resume callback ownership clears after auto-facing resumes"),
		DefenderCombat->GuardManualResumeTickerHandle.IsValid());
	FAlignmentRequestSpec ResumedSpec;
	TestTrue(TEXT("Resumed guard request remains queryable"),
		Targeting->GetAlignmentRequestSpec(GuardHandle, ResumedSpec));
	TestTrue(TEXT("Auto-facing resumes after the unscaled delay"),
		ResumedSpec.bTrackTargetRotation);
	TestEqual(TEXT("Resume preserves the locked threat"),
		DefenderCombat->GetLockedDefenseThreat().StableId.Value, LockedThreatId.Value);
	TestEqual(TEXT("Resume does not reset the interaction budget"),
		ResumedSpec.RemainingTurnBudget, 61.0f, 0.1f);

	Targeting->ResetAlignmentExecutionFrameForTesting();
	Targeting->TickComponent(0.10f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Resumed automatic facing uses the same final-actor rate cap"),
		Defender->GetActorRotation().Yaw, 0.0, 0.1);
	Targeting->GetAlignmentRequestSpec(GuardHandle, ResumedSpec);
	TestEqual(TEXT("Automatic facing continues spending the same budget"),
		ResumedSpec.RemainingTurnBudget, 52.0f, 0.1f);

	DefenderCombat->EndBlock();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAlignment_GuardManualThresholdAndPriority,
	"KatanaCombat.Defense.Alignment.GuardManualThresholdAndPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseAlignment_GuardManualThresholdAndPriority::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* DefenderCombat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Defender = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, DefenderCombat, Targeting);
	AEnemyCharacter* GuardThreat = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(0.0f, 300.0f, 0.0f));
	AEnemyCharacter* CommittedTarget = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(0.0f, -300.0f, 0.0f));
	UCombatComponent* AttackerCombat = GuardThreat ? GuardThreat->CombatComponent.Get() : nullptr;
	if (!Defender || !DefenderCombat || !Targeting || !GuardThreat || !CommittedTarget || !AttackerCombat)
	{
		AddError(TEXT("Failed to create guard-manual-priority fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingSettings* TargetingSettings = NewObject<UTargetingSettings>();
	TargetingSettings->MaxTargetDistance = 1000.0f;
	TargetingSettings->bRequireLineOfSight = false;
	Targeting->TargetingSettingsOverride = TargetingSettings;
	UDefenseConfiguration* DefenseConfig = NewObject<UDefenseConfiguration>();
	DefenseConfig->DefenseTurnRate = 90.0f;
	DefenseConfig->MaximumAutomaticTurn = 70.0f;
	DefenseConfig->GuardManualOverrideThreshold = 0.0f;
	DefenderCombat->DefenseConfigurationOverride = DefenseConfig;

	AttackerCombat->CurrentAttackData = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	AttackerCombat->CurrentPhase = EAttackPhase::Windup;
	AttackerCombat->AttackStateMachine.AttackGeneration = 79;
	AttackerCombat->SetAttackIntentTarget(Defender);
	Defender->SetActorRotation(FRotator::ZeroRotator);
	TestTrue(TEXT("Held guard starts"), DefenderCombat->BeginBlock());
	const FAlignmentRequestHandle GuardHandle = Targeting->GetActiveAlignmentRequest();

	DefenderCombat->SetDefenseManualYawInputForTesting(0.0f, 20.0);
	FAlignmentRequestSpec GuardSpec;
	Targeting->GetAlignmentRequestSpec(GuardHandle, GuardSpec);
	TestTrue(TEXT("Zero input never overrides guard even with a zero threshold"),
		GuardSpec.bTrackTargetRotation);

	DefenseConfig->GuardManualOverrideThreshold = 0.25f;
	DefenderCombat->SetDefenseManualYawInputForTesting(0.24f, 20.1);
	Targeting->GetAlignmentRequestSpec(GuardHandle, GuardSpec);
	TestTrue(TEXT("Input below threshold leaves automatic facing active"),
		GuardSpec.bTrackTargetRotation);
	DefenderCombat->SetDefenseManualYawInputForTesting(0.25f, 20.2);
	Targeting->GetAlignmentRequestSpec(GuardHandle, GuardSpec);
	TestFalse(TEXT("Input at threshold activates manual steering"),
		GuardSpec.bTrackTargetRotation);

	FAlignmentRequestSpec CommittedSpec = MakeSmoothSpec(CommittedTarget, 45.0f, 30.0f);
	CommittedSpec.OwnerId = TEXT("CommittedBlockContact");
	CommittedSpec.Priority = EDefenseAlignmentPriority::BlockContact;
	const FAlignmentRequestHandle CommittedHandle = Targeting->AcquireAlignmentRequest(CommittedSpec);
	TestTrue(TEXT("Committed request acquires"), CommittedHandle.IsValid());
	TestTrue(TEXT("Committed alignment retains ownership over manual steering"),
		Targeting->GetActiveAlignmentRequest() == CommittedHandle);
	Targeting->ResetAlignmentExecutionFrameForTesting();
	Targeting->TickComponent(0.10f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Committed request controls final actor yaw at its lower cap"),
		Defender->GetActorRotation().Yaw, -4.5, 0.1);

	Targeting->ReleaseAlignmentRequest(CommittedHandle);
	TestTrue(TEXT("Manual guard request resumes after committed ownership ends"),
		Targeting->GetActiveAlignmentRequest() == GuardHandle);
	DefenderCombat->SetDefenseManualYawInputForTesting(0.25f, 20.3);
	Targeting->ResetAlignmentExecutionFrameForTesting();
	Targeting->TickComponent(0.10f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Resumed manual guard remains capped by defense rate"),
		Defender->GetActorRotation().Yaw, 4.5, 0.1);

	DefenderCombat->EndBlock();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAlignment_PlayerLookRoutesManualYaw,
	"KatanaCombat.Defense.Alignment.PlayerLookRoutesManualYaw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseAlignment_PlayerLookRoutesManualYaw::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* DefenderCombat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Defender = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, DefenderCombat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(0.0f, 300.0f, 0.0f));
	UCombatComponent* AttackerCombat = Enemy ? Enemy->CombatComponent.Get() : nullptr;
	if (!Defender || !DefenderCombat || !Targeting || !Enemy || !AttackerCombat)
	{
		AddError(TEXT("Failed to create player-look guard fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingSettings* TargetingSettings = NewObject<UTargetingSettings>();
	TargetingSettings->MaxTargetDistance = 1000.0f;
	TargetingSettings->bRequireLineOfSight = false;
	Targeting->TargetingSettingsOverride = TargetingSettings;
	UDefenseConfiguration* DefenseConfig = NewObject<UDefenseConfiguration>();
	DefenseConfig->DefenseTurnRate = 90.0f;
	DefenderCombat->DefenseConfigurationOverride = DefenseConfig;

	AttackerCombat->CurrentAttackData = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	AttackerCombat->CurrentPhase = EAttackPhase::Windup;
	AttackerCombat->AttackStateMachine.AttackGeneration = 80;
	AttackerCombat->SetAttackIntentTarget(Defender);
	Defender->SetActorRotation(FRotator::ZeroRotator);
	TestTrue(TEXT("Held guard starts"), DefenderCombat->BeginBlock());
	const FAlignmentRequestHandle GuardHandle = Targeting->GetActiveAlignmentRequest();

	Defender->Look(FInputActionValue(FVector2D(-1.0f, 0.0f)));
	FAlignmentRequestSpec ManualSpec;
	TestTrue(TEXT("Player look keeps the guard request alive"),
		Targeting->GetAlignmentRequestSpec(GuardHandle, ManualSpec));
	TestFalse(TEXT("Player look routes yaw into manual guard steering"),
		ManualSpec.bTrackTargetRotation);
	Targeting->ResetAlignmentExecutionFrameForTesting();
	Targeting->TickComponent(0.10f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Routed player look changes final actor yaw through the capped executor"),
		Defender->GetActorRotation().Yaw, -9.0, 0.1);

	Defender->StopLook(FInputActionValue(FVector2D::ZeroVector));
	Targeting->GetAlignmentRequestSpec(GuardHandle, ManualSpec);
	TestFalse(TEXT("Look completion begins the delayed auto-facing resume"),
		ManualSpec.bTrackTargetRotation);

	DefenderCombat->EndBlock();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
