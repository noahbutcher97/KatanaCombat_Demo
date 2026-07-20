// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "ActionQueueTypes.h"
#include "Characters/PlayerCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/AttackData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAttackAlignment_QueuedIntentAndComboReplacement,
	"KatanaCombat.AttackAlignment.QueuedIntentAndComboReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackAlignment_QueuedIntentAndComboReplacement::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, Combat, Targeting);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	if (!Player || !Combat || !Targeting || !Attack)
	{
		AddError(TEXT("Failed to create queued attack-intent fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Attack->WarpConfig.bEnableWarp = true;
	Attack->WarpConfig.AlreadyFacingThreshold = 0.0f;
	Attack->WarpConfig.RotationSpeed = 720.0f;
	Attack->WarpConfig.MaximumAutomaticTurn = 180.0f;

	const FAttackFacingIntent FirstIntent = Combat->BuildAttackFacingIntent(
		FVector2D(1.0f, 0.0f),
		FRotator::ZeroRotator,
		FRotator::ZeroRotator,
		EInputDirection::Right,
		EAttackFacingIntentSource::CameraRelativeInput);
	TestTrue(TEXT("Right input captures a world-space direction"), FirstIntent.bHasWorldDirection);
	TestTrue(TEXT("Right input captures world +Y"),
		FirstIntent.WorldDirection.Equals(FVector::RightVector, 0.001f));
	TestEqual(TEXT("Right input captures 90-degree desired yaw"), FirstIntent.DesiredYaw, 90.0f, 0.1f);

	FQueuedInputAction FirstInput(EInputType::LightAttack, EInputEventType::Press, 1.0f, false);
	FirstInput.FacingIntent = FirstIntent;
	FActionQueueEntry FirstAction(FirstInput, Attack, EActionExecutionMode::Queued);
	Player->SetActorRotation(FRotator(0.0f, -45.0f, 0.0f));
	Combat->SetupAttackWarp(FirstAction);
	const FAlignmentRequestHandle FirstHandle = Targeting->GetActiveAlignmentRequest();
	FAlignmentRequestSpec FirstSpec;
	TestTrue(TEXT("Queued attack acquires its rotation-only request"),
		Targeting->GetAlignmentRequestSpec(FirstHandle, FirstSpec));
	TestEqual(TEXT("Queued attack retains input-edge yaw after actor rotation changes"),
		static_cast<float>(FirstSpec.DesiredRotation.Yaw), 90.0f, 0.1f);
	TestNull(TEXT("Targetless queued intent does not fabricate a target"), FirstSpec.Target.Get());

	const FAttackFacingIntent SecondIntent = Combat->BuildAttackFacingIntent(
		FVector2D(0.0f, -1.0f),
		FRotator::ZeroRotator,
		Player->GetActorRotation(),
		EInputDirection::Backward,
		EAttackFacingIntentSource::CameraRelativeInput);
	FQueuedInputAction SecondInput(EInputType::LightAttack, EInputEventType::Press, 2.0f, true);
	SecondInput.FacingIntent = SecondIntent;
	FActionQueueEntry SecondAction(SecondInput, Attack, EActionExecutionMode::Queued);
	Player->SetActorRotation(FRotator(0.0f, 35.0f, 0.0f));
	Combat->SetupAttackWarp(SecondAction);
	const FAlignmentRequestHandle SecondHandle = Targeting->GetActiveAlignmentRequest();
	FAlignmentRequestSpec SecondSpec;
	TestTrue(TEXT("Combo successor acquires its own rotation request"),
		Targeting->GetAlignmentRequestSpec(SecondHandle, SecondSpec));
	TestTrue(TEXT("Combo successor replaces the prior request"), SecondHandle != FirstHandle);
	TestTrue(TEXT("Combo successor advances alignment generation"),
		SecondSpec.OwnerGeneration > FirstSpec.OwnerGeneration);
	TestEqual(TEXT("Combo successor uses its own physical edge instead of the first edge"),
		FMath::Abs(static_cast<float>(SecondSpec.DesiredRotation.Yaw)), 180.0f, 0.1f);
	TestEqual(TEXT("Combo replacement leaves exactly one attack owner"),
		Targeting->GetAlignmentRequestCountForTesting(), 1);

	Targeting->ReleaseActiveAttackWarp();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAttackAlignment_LocomotionRateIndependent,
	"KatanaCombat.AttackAlignment.LocomotionRateIndependent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackAlignment_LocomotionRateIndependent::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, Combat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(0.0f, 300.0f, 0.0f));
	if (!Player || !Combat || !Targeting || !Enemy || !Player->GetCharacterMovement())
	{
		AddError(TEXT("Failed to create locomotion/attack rotation fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestEqual(TEXT("Player locomotion uses the reviewed 540-degree default"),
		static_cast<float>(Player->GetCharacterMovement()->RotationRate.Yaw), 540.0f, 0.1f);

	FAttackWarpConfig Config;
	Config.RotationSpeed = 720.0f;
	Config.MaximumAutomaticTurn = 180.0f;
	TestTrue(TEXT("Attack alignment acquires independently of locomotion"),
		Targeting->SetupAttackWarp(Enemy, FRotator::ZeroRotator, Config));
	FAlignmentRequestSpec Spec;
	TestTrue(TEXT("Attack alignment request is queryable"),
		Targeting->GetAlignmentRequestSpec(Targeting->GetActiveAlignmentRequest(), Spec));
	TestEqual(TEXT("Attack alignment retains its 720-degree capability"),
		Spec.MaximumTurnRate, 720.0f, 0.1f);
	TestEqual(TEXT("Attack alignment does not overwrite locomotion rate"),
		static_cast<float>(Player->GetCharacterMovement()->RotationRate.Yaw), 540.0f, 0.1f);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAttackAlignment_TerminalNoneIdempotentlyReleasesAlignment,
	"KatanaCombat.AttackAlignment.TerminalNoneIdempotentlyReleasesAlignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackAlignment_TerminalNoneIdempotentlyReleasesAlignment::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	UTargetingComponent* Targeting = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombatAndTargeting(
		World, Combat, Targeting);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(300.0f, 0.0f, 0.0f));
	if (!Player || !Combat || !Targeting || !Enemy)
	{
		AddError(TEXT("Failed to create idempotent terminal-alignment fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	FAttackWarpConfig Config;
	Config.bEnableWarp = true;
	TestTrue(TEXT("Fixture acquires attack alignment while phase is already None"),
		Targeting->SetupAttackWarp(Enemy, FRotator::ZeroRotator, Config));
	TestEqual(TEXT("Fixture starts with one attack alignment owner"),
		Targeting->GetAlignmentRequestCountForTesting(), 1);

	Combat->SetPhase(EAttackPhase::None);

	TestEqual(TEXT("Repeated terminal phase release clears the attack alignment owner"),
		Targeting->GetAlignmentRequestCountForTesting(), 0);
	TestNull(TEXT("Repeated terminal phase release removes the published warp target"),
		Player->MotionWarpingComponent->FindWarpTarget(Config.TargetWarpName));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
