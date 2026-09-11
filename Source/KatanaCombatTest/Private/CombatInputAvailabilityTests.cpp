// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "ActionQueueTypes.h"
#include "Animation/AnimMontage.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Data/AttackData.h"
#include "Data/PairedAnimationData.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
bool HasActionBinding(
	const UEnhancedInputComponent* InputComponent,
	const UInputAction* Action,
	ETriggerEvent TriggerEvent)
{
	if (!InputComponent || !Action)
	{
		return false;
	}

	for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : InputComponent->GetActionEventBindings())
	{
		if (Binding && Binding->GetAction() == Action && Binding->GetTriggerEvent() == TriggerEvent)
		{
			return true;
		}
	}

	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlayerInputTerminalBindingsTest,
	"KatanaCombat.CombatInput.Bindings.TerminalEventsAreExplicit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlayerInputTerminalBindingsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UEnhancedInputComponent* InputComponent = NewObject<UEnhancedInputComponent>(Player);
	if (!TestNotNull(TEXT("Player should exist"), Player)
		|| !TestNotNull(TEXT("Enhanced input component should exist"), InputComponent))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Player->MoveAction = NewObject<UInputAction>(Player);
	Player->LightAttackAction = NewObject<UInputAction>(Player);
	Player->HeavyAttackAction = NewObject<UInputAction>(Player);
	Player->BlockAction = NewObject<UInputAction>(Player);
	Player->SetupPlayerInputComponent(InputComponent);

	TestTrue(TEXT("Move should bind Triggered"),
		HasActionBinding(InputComponent, Player->MoveAction, ETriggerEvent::Triggered));
	TestTrue(TEXT("Move should bind Completed"),
		HasActionBinding(InputComponent, Player->MoveAction, ETriggerEvent::Completed));
	TestTrue(TEXT("Move should bind Canceled"),
		HasActionBinding(InputComponent, Player->MoveAction, ETriggerEvent::Canceled));
	TestTrue(TEXT("Light attack should bind Canceled"),
		HasActionBinding(InputComponent, Player->LightAttackAction, ETriggerEvent::Canceled));
	TestTrue(TEXT("Heavy attack should bind Canceled"),
		HasActionBinding(InputComponent, Player->HeavyAttackAction, ETriggerEvent::Canceled));
	TestTrue(TEXT("Block should bind Canceled"),
		HasActionBinding(InputComponent, Player->BlockAction, ETriggerEvent::Canceled));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlayerMovementHoldSuppressionPolicyTest,
	"KatanaCombat.CombatInput.Movement.HoldSuppressesApplicationWithoutChangingMode",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlayerMovementHoldSuppressionPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	UCharacterMovementComponent* Movement = Player ? Player->GetCharacterMovement() : nullptr;
	if (!TestNotNull(TEXT("Player should exist"), Player)
		|| !TestNotNull(TEXT("Combat component should exist"), Combat)
		|| !TestNotNull(TEXT("Player controller should exist"), Controller)
		|| !TestNotNull(TEXT("Movement component should exist"), Movement))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Controller->Possess(Player);
	Movement->SetMovementMode(MOVE_Flying);
	Combat->HoldState.Activate(EInputType::LightAttack, World->GetTimeSeconds(), 0.1f);
	Player->Move(FInputActionValue(FVector2D(0.0f, 1.0f)));

	TestTrue(TEXT("Suppressed movement should remain the canonical directional sample"),
		Player->GetLastMovementInput().Equals(FVector2D(0.0f, 1.0f)));
	TestTrue(TEXT("Committed hold should suppress movement application"),
		Player->GetPendingMovementInputVector().IsNearlyZero());
	TestEqual(TEXT("Hold policy must preserve externally owned movement mode"),
		Movement->MovementMode.GetValue(), MOVE_Flying);

	Combat->HoldState.Deactivate();
	Player->ConsumeMovementInputVector();
	Player->Move(FInputActionValue(FVector2D(0.0f, 1.0f)));
	TestFalse(TEXT("Movement should apply after the hold terminates"),
		Player->GetPendingMovementInputVector().IsNearlyZero());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlayerMovementTerminalSampleTest,
	"KatanaCombat.CombatInput.Movement.TerminalEventsClearCanonicalSample",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlayerMovementTerminalSampleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	if (!TestNotNull(TEXT("Player should exist"), Player)
		|| !TestNotNull(TEXT("Combat component should exist"), Combat))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Player->Move(FInputActionValue(FVector2D(1.0f, 0.0f)));
	TestFalse(TEXT("Triggered movement should update the canonical sample"),
		Combat->GetMovementInputSample().CameraRelativeInput.IsNearlyZero());

	Player->StopMove(FInputActionValue(FVector2D::ZeroVector));
	TestTrue(TEXT("Terminal movement should clear the combat-owned sample"),
		Combat->GetMovementInputSample().CameraRelativeInput.IsNearlyZero());
	TestTrue(TEXT("Terminal movement should clear the debug-facing sample"),
		Player->GetLastMovementInput().IsNearlyZero());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatInputCanceledClearsHoldWithoutFollowUpTest,
	"KatanaCombat.CombatInput.TerminalEdges.CanceledAttackClearsHoldWithoutFollowUp",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatInputCanceledClearsHoldWithoutFollowUpTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	if (!TestNotNull(TEXT("Player should exist"), Player)
		|| !TestNotNull(TEXT("Combat component should exist"), Combat))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* SourceAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* FollowUp = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	SourceAttack->DirectionalFollowUps.Add(EAttackDirection::Right, FollowUp);
	Combat->CurrentAttackData = SourceAttack;
	Combat->CurrentInputContext = EInputContext::DirectionalInput;
	Combat->HoldState.Activate(EInputType::LightAttack, World->GetTimeSeconds(), 0.1f);
	Combat->HoldState.MarkHoldCompleted();
	Combat->HoldState.CurrentHold.Direction = EAttackDirection::Right;
	Combat->HeldInputs.Add(EInputType::LightAttack, World->GetTimeSeconds());
	Combat->HeldInputSerials.Add(EInputType::LightAttack, 71);

	Combat->OnInputEvent(
		EInputType::LightAttack,
		EInputEventType::Canceled,
		EInputDirection::Right);

	TestFalse(TEXT("Canceled attack should clear physical held state"),
		Combat->HeldInputs.Contains(EInputType::LightAttack));
	TestFalse(TEXT("Canceled attack should clear held input identity"),
		Combat->HeldInputSerials.Contains(EInputType::LightAttack));
	TestFalse(TEXT("Canceled attack should terminate its hold"), Combat->HoldState.IsHolding());
	TestEqual(TEXT("Canceled attack must not enqueue a directional follow-up"),
		Combat->ActionQueue.Num(), 0);
	TestEqual(TEXT("Canceled attack must not replace the active attack"),
		Combat->CurrentAttackData.Get(), SourceAttack);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatBlockCanceledClearsGuardTest,
	"KatanaCombat.CombatInput.TerminalEdges.CanceledBlockEndsGuard",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatBlockCanceledClearsGuardTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	if (!TestNotNull(TEXT("Player should exist"), Player)
		|| !TestNotNull(TEXT("Combat component should exist"), Combat))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Combat->bIsBlocking = true;
	Combat->OnInputEvent(EInputType::Block, EInputEventType::Canceled);
	TestFalse(TEXT("Canceled block should end guard intent"), Combat->IsBlocking());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatInputPairedTakeoverTest,
	"KatanaCombat.CombatInput.PairedTakeover.RetiresRegularLifecycle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatInputPairedTakeoverTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UPairedAnimationComponent* Paired = Player ? Player->PairedAnimationComponent.Get() : nullptr;
	if (!TestNotNull(TEXT("Player should have combat component"), Combat)
		|| !TestNotNull(TEXT("Player should have paired animation component"), Paired))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* ActiveAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	Combat->CurrentAttackData = ActiveAttack;
	Combat->CurrentPhase = EAttackPhase::Active;
	Combat->AttackStateMachine.OnAttackStarted(
		ActiveAttack->AttackMontage,
		NAME_None,
		World->GetTimeSeconds());

	FQueuedInputAction PendingInput(
		EInputType::HeavyAttack,
		EInputEventType::Press,
		World->GetTimeSeconds());
	PendingInput.InputSerial = 42;
	Combat->ActionQueue.Add(FActionQueueEntry(
		PendingInput,
		nullptr,
		EActionExecutionMode::Queued));
	Combat->HoldState.Activate(EInputType::LightAttack, World->GetTimeSeconds(), 0.1f);
	Combat->HoldState.CurrentHold.PressInputSerial = 41;
	Combat->HeldInputs.Add(EInputType::LightAttack, World->GetTimeSeconds());
	Combat->HeldInputSerials.Add(EInputType::LightAttack, 41);

	UPairedAnimationData* PairedData = NewObject<UPairedAnimationData>();
	PairedData->bApplySlowMotion = false;
	Paired->BeginPairedAnimation(PairedData, EPairedReactionType::Finisher, false);

	TestTrue(TEXT("Paired sequence should become active"), Paired->IsPairedAnimationActive());
	TestNull(TEXT("Paired takeover should retire regular attack data"), Combat->GetCurrentAttack());
	TestEqual(TEXT("Paired takeover should retire the regular phase"),
		Combat->GetCurrentPhase(), EAttackPhase::None);
	TestNull(TEXT("Paired takeover should retire the regular montage owner"),
		Combat->AttackStateMachine.GetActiveMontage());
	TestEqual(TEXT("Paired takeover should discard pending normal actions"),
		Combat->ActionQueue.Num(), 0);
	TestFalse(TEXT("Paired takeover should terminate the active hold"), Combat->HoldState.IsHolding());
	TestFalse(TEXT("Paired takeover should clear physical held input"),
		Combat->HeldInputs.Contains(EInputType::LightAttack));
	TestFalse(TEXT("Paired takeover should clear held input identity"),
		Combat->HeldInputSerials.Contains(EInputType::LightAttack));

	Paired->EndPairedAnimation();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatInputPairedVictimTakeoverTest,
	"KatanaCombat.CombatInput.PairedTakeover.RetiresVictimRegularLifecycle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatInputPairedVictimTakeoverTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UCombatComponent* Combat = Victim ? Victim->CombatComponent.Get() : nullptr;
	UHitReactionComponent* HitReaction = Victim ? Victim->HitReactionComponent.Get() : nullptr;
	if (!TestNotNull(TEXT("Victim should have combat component"), Combat)
		|| !TestNotNull(TEXT("Victim should have hit reaction component"), HitReaction))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* ActiveAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	Combat->CurrentAttackData = ActiveAttack;
	Combat->CurrentPhase = EAttackPhase::Active;
	Combat->AttackStateMachine.OnAttackStarted(
		ActiveAttack->AttackMontage,
		NAME_None,
		World->GetTimeSeconds());

	HitReaction->EnterPairedAnimationState(
		NewObject<UAnimMontage>(),
		EReactionOutcome::Ragdoll,
		0.2f,
		false);

	TestNull(TEXT("Victim paired takeover should retire regular attack data"),
		Combat->GetCurrentAttack());
	TestEqual(TEXT("Victim paired takeover should retire the regular phase"),
		Combat->GetCurrentPhase(), EAttackPhase::None);
	TestNull(TEXT("Victim paired takeover should retire the regular montage owner"),
		Combat->AttackStateMachine.GetActiveMontage());
	TestFalse(TEXT("Paired victim ownership should reject new attack input"),
		Combat->CanProcessInput(EInputType::LightAttack));
	TestTrue(TEXT("Paired victim ownership should suppress movement application"),
		Combat->IsMovementInputSuppressed());

	HitReaction->ExitPairedAnimationState();
	TestTrue(TEXT("Releasing paired victim ownership should restore attack input"),
		Combat->CanProcessInput(EInputType::LightAttack));
	TestFalse(TEXT("Releasing paired victim ownership should restore movement application"),
		Combat->IsMovementInputSuppressed());
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatFailedAttackStartupRollsBackProvisionalStateTest,
	"KatanaCombat.CombatInput.Startup.MissingMontageRollsBackProvisionalState",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatFailedAttackStartupRollsBackProvisionalStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UCombatComponent* Combat = Player ? Player->CombatComponent.Get() : nullptr;
	if (!TestNotNull(TEXT("Player should have combat component"), Combat))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* MissingMontageAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	MissingMontageAttack->AttackMontage = nullptr;
	FQueuedInputAction Input(
		EInputType::LightAttack,
		EInputEventType::Press,
		World->GetTimeSeconds(),
		false);
	FActionQueueEntry Entry(Input, MissingMontageAttack, EActionExecutionMode::Immediate);

	TestFalse(TEXT("Attack startup without a montage fails"), Combat->ExecuteAction(Entry));
	TestNull(TEXT("Failed startup releases provisional attack ownership"), Combat->GetCurrentAttack());
	TestFalse(TEXT("Failed startup cannot leave the component reporting an active attack"),
		Combat->IsAttacking());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatInputRejectedReleaseCleanupTest,
	"KatanaCombat.CombatInput.TerminalEdges.ReleaseClearsStateWhilePaired",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatInputRejectedReleaseCleanupTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UPairedAnimationComponent* Paired = Player ? Player->PairedAnimationComponent.Get() : nullptr;
	if (!TestNotNull(TEXT("Player should have combat component"), Combat)
		|| !TestNotNull(TEXT("Player should have paired animation component"), Paired))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FPairedSequenceLeaseHandle InputLease =
		Paired->AcquireInputOwnership(TEXT("ReleaseCleanupTest"), 1);
	Combat->CachedPairedAnimComp = Paired;
	Combat->HoldState.Activate(EInputType::LightAttack, World->GetTimeSeconds(), 0.1f);
	Combat->HoldState.CurrentHold.PressInputSerial = 51;
	Combat->HeldInputs.Add(EInputType::LightAttack, World->GetTimeSeconds());
	Combat->HeldInputSerials.Add(EInputType::LightAttack, 51);

	Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Release);

	TestFalse(TEXT("Rejected release should still clear physical held input"),
		Combat->HeldInputs.Contains(EInputType::LightAttack));
	TestFalse(TEXT("Rejected release should still clear held input identity"),
		Combat->HeldInputSerials.Contains(EInputType::LightAttack));
	TestFalse(TEXT("Rejected release should terminate its matching hold"),
		Combat->HoldState.IsHolding());
	TestEqual(TEXT("Rejected release must not enqueue a follow-up"),
		Combat->ActionQueue.Num(), 0);

	Paired->ReleaseInputOwnership(InputLease);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatHoldCleanupPreservesMovementModeTest,
	"KatanaCombat.CombatInput.HoldCleanup.PreservesExternalMovementMode",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCombatHoldCleanupPreservesMovementModeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	UCharacterMovementComponent* Movement = Player ? Player->GetCharacterMovement() : nullptr;
	if (!TestNotNull(TEXT("Player should have combat component"), Combat)
		|| !TestNotNull(TEXT("Player should have character movement"), Movement))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Movement->SetMovementMode(MOVE_Flying);
	Combat->HoldState.Activate(EInputType::LightAttack, World->GetTimeSeconds(), 0.1f);

	Combat->ClearHoldState();

	TestFalse(TEXT("Hold cleanup should terminate the hold"), Combat->HoldState.IsHolding());
	TestEqual(TEXT("Hold cleanup must not overwrite movement mode owned by another system"),
		Movement->MovementMode.GetValue(), MOVE_Flying);
	TestFalse(TEXT("Hold cleanup should release the derived movement policy"),
		Combat->IsMovementInputSuppressed());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPairedEndPreservesExternalMovementModeTest,
	"KatanaCombat.PairedAnimation.Movement.EndPreservesExternalMovementMode",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPairedEndPreservesExternalMovementModeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UPairedAnimationComponent* Paired = Player ? Player->PairedAnimationComponent.Get() : nullptr;
	UCharacterMovementComponent* Movement = Player ? Player->GetCharacterMovement() : nullptr;
	if (!TestNotNull(TEXT("Player should have paired animation component"), Paired)
		|| !TestNotNull(TEXT("Player should have character movement"), Movement))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Movement->DisableMovement();
	UPairedAnimationData* PairedData = NewObject<UPairedAnimationData>();
	PairedData->bApplySlowMotion = false;
	Paired->BeginPairedAnimation(PairedData, EPairedReactionType::Finisher, false);
	Paired->EndPairedAnimation();

	TestEqual(TEXT("Paired end must not restore a movement mode it did not acquire"),
		Movement->MovementMode.GetValue(), MOVE_None);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPairedEndReleasesOwnedMovementLeaseTest,
	"KatanaCombat.PairedAnimation.Movement.EndReleasesOwnedNotifyLease",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPairedEndReleasesOwnedMovementLeaseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UPairedAnimationComponent* Paired = Player ? Player->PairedAnimationComponent.Get() : nullptr;
	UCharacterMovementComponent* Movement = Player ? Player->GetCharacterMovement() : nullptr;
	if (!TestNotNull(TEXT("Player should have paired animation component"), Paired)
		|| !TestNotNull(TEXT("Player should have character movement"), Movement))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	Movement->SetMovementMode(MOVE_Walking);

	FAnimNotifyRuntimeSourceId IndependentSource;
	IndependentSource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Paired/IndependentCollision"));
	IndependentSource.NotifyEventIndex = 1;
	TestTrue(TEXT("Independent collision-only notify acquires its own lease"),
		Paired->BeginPairedCollisionNotify(
			IndependentSource, 10, true, true, false, false, false, 150.0f));

	UPairedAnimationData* PairedData = NewObject<UPairedAnimationData>();
	PairedData->bApplySlowMotion = false;
	Paired->BeginPairedAnimation(PairedData, EPairedReactionType::Finisher, false);

	FAnimNotifyRuntimeSourceId FinisherSource;
	FinisherSource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Paired/FinisherCollision"));
	FinisherSource.NotifyEventIndex = 2;
	TestTrue(TEXT("Active finisher collision notify acquires movement ownership"),
		Paired->BeginPairedCollisionNotify(
			FinisherSource, 20, true, true, false, true, false, 150.0f));
	TestEqual(TEXT("Finisher notify disables movement"),
		Movement->MovementMode.GetValue(), MOVE_None);
	TestEqual(TEXT("Independent and finisher leases coexist"),
		Paired->GetActivePairedStateLeaseCount(), 2);

	Paired->EndPairedAnimation();

	TestEqual(TEXT("Finisher terminal cleanup releases only its owned notify lease"),
		Paired->GetActivePairedStateLeaseCount(), 1);
	TestEqual(TEXT("Releasing the finisher movement owner restores its captured mode"),
		Movement->MovementMode.GetValue(), MOVE_Walking);

	Paired->EndPairedCollisionNotify(IndependentSource, 10);
	TestEqual(TEXT("Independent notify remains exactly releasable after finisher cleanup"),
		Paired->GetActivePairedStateLeaseCount(), 0);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPairedCancelReleasesParticipantMovementLeasesTest,
	"KatanaCombat.PairedAnimation.Movement.CancelReleasesBothParticipantNotifyLeases",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPairedCancelReleasesParticipantMovementLeasesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(
		World,
		FVector(150.0f, 0.0f, 0.0f));
	UPairedAnimationComponent* AttackerPaired = Attacker
		? Attacker->PairedAnimationComponent.Get()
		: nullptr;
	UPairedAnimationComponent* VictimPaired = Victim
		? Victim->PairedAnimationComponent.Get()
		: nullptr;
	if (!TestNotNull(TEXT("Attacker should have paired animation component"), AttackerPaired)
		|| !TestNotNull(TEXT("Victim should have paired animation component"), VictimPaired))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	Attacker->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Victim->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	AttackerPaired->AddPairedPartner(Victim);
	VictimPaired->AddPairedPartner(Attacker);
	UPairedAnimationData* PairedData = NewObject<UPairedAnimationData>();
	PairedData->bApplySlowMotion = false;
	AttackerPaired->BeginPairedAnimation(PairedData, EPairedReactionType::Finisher, false);

	FAnimNotifyRuntimeSourceId AttackerSource;
	AttackerSource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Paired/AttackerCollision"));
	AttackerSource.NotifyEventIndex = 3;
	FAnimNotifyRuntimeSourceId VictimSource;
	VictimSource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Paired/VictimCollision"));
	VictimSource.NotifyEventIndex = 4;
	TestTrue(TEXT("Attacker collision notify acquires movement ownership"),
		AttackerPaired->BeginPairedCollisionNotify(
			AttackerSource, 30, true, true, false, true, false, 150.0f));
	TestTrue(TEXT("Victim collision notify acquires movement ownership"),
		VictimPaired->BeginPairedCollisionNotify(
			VictimSource, 40, true, true, false, true, false, 150.0f));
	TestEqual(TEXT("Attacker movement is disabled during the finisher"),
		Attacker->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
	TestEqual(TEXT("Victim movement is disabled during the finisher"),
		Victim->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);

	AttackerPaired->CancelPairedAnimation(0.0f);

	TestEqual(TEXT("Cancel releases the attacker's finisher notify lease"),
		AttackerPaired->GetActivePairedStateLeaseCount(), 0);
	TestEqual(TEXT("Cancel releases the victim's finisher notify lease"),
		VictimPaired->GetActivePairedStateLeaseCount(), 0);
	TestEqual(TEXT("Cancel restores attacker movement"),
		Attacker->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);
	TestEqual(TEXT("Cancel restores victim movement"),
		Victim->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPairedPartnerDeathReleasesParticipantMovementLeaseTest,
	"KatanaCombat.PairedAnimation.Movement.PartnerDeathReleasesRemovedParticipantLease",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPairedPartnerDeathReleasesParticipantMovementLeaseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(
		World,
		FVector(150.0f, 0.0f, 0.0f));
	UPairedAnimationComponent* AttackerPaired = Attacker
		? Attacker->PairedAnimationComponent.Get()
		: nullptr;
	UPairedAnimationComponent* VictimPaired = Victim
		? Victim->PairedAnimationComponent.Get()
		: nullptr;
	if (!TestNotNull(TEXT("Attacker should have paired animation component"), AttackerPaired)
		|| !TestNotNull(TEXT("Victim should have paired animation component"), VictimPaired))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Attacker->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Victim->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	AttackerPaired->AddPairedPartner(Victim);
	VictimPaired->AddPairedPartner(Attacker);
	UPairedAnimationData* PairedData = NewObject<UPairedAnimationData>();
	PairedData->bApplySlowMotion = false;
	AttackerPaired->BeginPairedAnimation(PairedData, EPairedReactionType::Finisher, false);

	FAnimNotifyRuntimeSourceId VictimSource;
	VictimSource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Paired/UnexpectedDeathCollision"));
	VictimSource.NotifyEventIndex = 5;
	TestTrue(TEXT("Victim collision notify acquires movement ownership"),
		VictimPaired->BeginPairedCollisionNotify(
			VictimSource, 50, true, true, false, true, false, 150.0f));
	TestEqual(TEXT("Victim movement is disabled during the finisher"),
		Victim->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);

	AttackerPaired->OnPairedPartnerDeath(Victim);

	TestFalse(TEXT("Unexpected partner death cancels the owner lifecycle"),
		AttackerPaired->IsPairedAnimationActive());
	TestEqual(TEXT("Partner removal releases the victim's operation-owned notify lease"),
		VictimPaired->GetActivePairedStateLeaseCount(), 0);
	TestEqual(TEXT("Participant movement baseline is restored after cancellation"),
		Victim->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPairedCompletionExpectedPartnerDeathTest,
	"KatanaCombat.PairedAnimation.Completion.ExpectedVictimDeathDoesNotCancel",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPairedCompletionExpectedPartnerDeathTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(
		World,
		FVector(150.0f, 0.0f, 0.0f));
	UPairedAnimationComponent* Paired = Attacker ? Attacker->PairedAnimationComponent.Get() : nullptr;
	UPairedAnimationComponent* VictimPaired = Victim ? Victim->PairedAnimationComponent.Get() : nullptr;
	if (!TestNotNull(TEXT("Attacker should have paired animation component"), Paired)
		|| !TestNotNull(TEXT("Victim should exist"), Victim)
		|| !TestNotNull(TEXT("Victim should have paired animation component"), VictimPaired))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UPairedAnimationData* PairedData = NewObject<UPairedAnimationData>();
	PairedData->bApplySlowMotion = false;
	PairedData->bIsLethal = true;
	PairedData->BaseDamage = 100.0f;
	PairedData->DamageMultiplier = 1.0f;
	PairedData->VictimDeathOutcome = EReactionOutcome::Ragdoll;
	PairedData->RagdollBlendTime = 0.0f;
	PairedData->VictimMontage = NewObject<UAnimMontage>(PairedData);
	Victim->CurrentHealth = 15.0f;
	Victim->HitReactionComponent->EnterPairedAnimationState(
		PairedData->VictimMontage,
		PairedData->VictimDeathOutcome,
		PairedData->RagdollBlendTime,
		true,
		Attacker);
	Paired->AddPairedPartner(Victim);
	VictimPaired->AddPairedPartner(Attacker);
	Paired->BeginPairedAnimation(PairedData, EPairedReactionType::Finisher, false);
	Attacker->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Victim->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	FAnimNotifyRuntimeSourceId AttackerCollisionSource;
	AttackerCollisionSource.SourceAnimation = FSoftObjectPath(
		TEXT("/Game/Test/Paired/LethalAttackerCollision"));
	AttackerCollisionSource.NotifyEventIndex = 6;
	FAnimNotifyRuntimeSourceId VictimCollisionSource;
	VictimCollisionSource.SourceAnimation = FSoftObjectPath(
		TEXT("/Game/Test/Paired/LethalVictimCollision"));
	VictimCollisionSource.NotifyEventIndex = 5;
	TestTrue(TEXT("Lethal attacker collision notify acquires movement ownership"),
		Paired->BeginPairedCollisionNotify(
			AttackerCollisionSource, 60, true, true, true, true, false, 150.0f));
	TestTrue(TEXT("Lethal victim collision notify acquires movement ownership"),
		VictimPaired->BeginPairedCollisionNotify(
			VictimCollisionSource, 50, true, true, true, true, false, 150.0f));
	TestEqual(TEXT("Attacker movement is disabled during lethal paired ownership"),
		Attacker->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
	Paired->CurrentFinisherVictim = Victim;
	Paired->bCompletingPairedAnimation = true;

	Paired->OnPairedPartnerDeath(Victim);

	TestTrue(TEXT("Expected completion death should preserve paired ownership until canonical cleanup"),
		Paired->IsPairedAnimationActive());
	TestTrue(TEXT("Expected completion death should preserve the partner link until canonical cleanup"),
		Paired->IsPairedPartner(Victim));
	TestTrue(TEXT("Expected completion death should preserve input ownership until canonical cleanup"),
		Paired->IsInputBlocked());
	TestEqual(TEXT("Expected completion death should preserve the completing victim"),
		Paired->CurrentFinisherVictim.Get(), static_cast<AActor*>(Victim));

	Paired->bCompletingPairedAnimation = false;
	Paired->CompletePairedAnimation();

	TestEqual(TEXT("Lethal finisher reduces victim health to zero"), Victim->CurrentHealth, 0.0f);
	TestTrue(TEXT("Successful lethal completion applies the configured terminal death outcome"),
		Victim->IsDead());
	TestFalse(TEXT("Successful lethal completion cannot strand the victim in Dying"),
		Victim->IsDying());
	TestFalse(TEXT("Successful lethal completion releases victim reaction ownership"),
		Victim->HitReactionComponent->IsInPairedAnimationState());
	TestEqual(TEXT("Successful lethal completion releases the victim's paired notify lease"),
		VictimPaired->GetActivePairedStateLeaseCount(), 0);
	TestEqual(TEXT("Terminal death movement ownership wins over paired baseline restoration"),
		Victim->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
	TestEqual(TEXT("Terminal death collision ownership wins over paired baseline restoration"),
		Victim->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Successful lethal completion releases attacker input ownership"),
		Paired->IsInputBlocked());
	TestFalse(TEXT("Successful lethal completion clears the attacker paired lifecycle"),
		Paired->IsPairedAnimationActive());
	TestEqual(TEXT("Successful lethal completion releases the attacker's paired notify lease"),
		Paired->GetActivePairedStateLeaseCount(), 0);
	TestEqual(TEXT("Successful lethal completion restores executor movement"),
		Attacker->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPairedVictimOutcomeRequiresCommittedDeathTest,
	"KatanaCombat.PairedAnimation.Completion.VictimOutcomeRequiresCommittedDeath",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPairedVictimOutcomeRequiresCommittedDeathTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(
		World,
		FVector(150.0f, 0.0f, 0.0f));
	UHitReactionComponent* HitReaction = Victim ? Victim->HitReactionComponent.Get() : nullptr;
	if (!TestNotNull(TEXT("Attacker should exist"), Attacker)
		|| !TestNotNull(TEXT("Victim should have hit reaction component"), HitReaction))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAnimMontage* VictimMontage = NewObject<UAnimMontage>();
	HitReaction->EnterPairedAnimationState(
		VictimMontage,
		EReactionOutcome::Ragdoll,
		0.0f,
		true,
		Attacker);

	HitReaction->OnAnyMontageBlendingOut(VictimMontage, false);

	TestFalse(TEXT("Victim montage completion cannot create death before lethal health commit"),
		Victim->IsDeadOrDying());
	TestTrue(TEXT("Premature victim montage completion preserves paired reaction ownership"),
		HitReaction->IsInPairedAnimationState());

	Victim->ModifyHealth(-Victim->CurrentHealth, Attacker);
	TestTrue(TEXT("Lethal health commit moves the victim into Dying"), Victim->IsDying());
	TestFalse(TEXT("Pending paired outcome has not finalized before canonical completion"),
		Victim->IsDead());

	HitReaction->CompletePairedAnimationState();

	TestTrue(TEXT("Canonical completion applies the retained terminal outcome"), Victim->IsDead());
	TestFalse(TEXT("Canonical completion cannot strand the victim in Dying"), Victim->IsDying());
	TestFalse(TEXT("Canonical completion releases paired reaction ownership"),
		HitReaction->IsInPairedAnimationState());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPairedVictimRagdollNotifyRequiresCommittedDeathTest,
	"KatanaCombat.PairedAnimation.Completion.RagdollNotifyRequiresCommittedDeath",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPairedVictimRagdollNotifyRequiresCommittedDeathTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(
		World,
		FVector(150.0f, 0.0f, 0.0f));
	UHitReactionComponent* HitReaction = Victim ? Victim->HitReactionComponent.Get() : nullptr;
	if (!TestNotNull(TEXT("Attacker should exist"), Attacker)
		|| !TestNotNull(TEXT("Victim should have hit reaction component"), HitReaction))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	HitReaction->EnterPairedAnimationState(
		NewObject<UAnimMontage>(),
		EReactionOutcome::Ragdoll,
		0.0f,
		true,
		Attacker);
	HitReaction->TriggerRagdollFromNotify(0.0f);

	TestFalse(TEXT("Ragdoll notify cannot create death before lethal health commit"),
		Victim->IsDeadOrDying());
	TestTrue(TEXT("Premature ragdoll notify preserves paired reaction ownership"),
		HitReaction->IsInPairedAnimationState());

	HitReaction->ExitPairedAnimationState();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPairedVictimRecoveryOutcomeFailsClosedTest,
	"KatanaCombat.PairedAnimation.Completion.LethalRecoveryOutcomeFailsClosed",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPairedVictimRecoveryOutcomeFailsClosedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(
		World,
		FVector(150.0f, 0.0f, 0.0f));
	UHitReactionComponent* HitReaction = Victim ? Victim->HitReactionComponent.Get() : nullptr;
	if (!TestNotNull(TEXT("Attacker should exist"), Attacker)
		|| !TestNotNull(TEXT("Victim should have hit reaction component"), HitReaction))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	HitReaction->EnterPairedAnimationState(
		NewObject<UAnimMontage>(),
		EReactionOutcome::StandardRecovery,
		0.0f,
		true,
		Attacker);
	Victim->ModifyHealth(-Victim->CurrentHealth, Attacker);
	HitReaction->CompletePairedAnimationState();

	TestTrue(TEXT("Invalid lethal recovery outcome should fail closed to terminal death"),
		Victim->IsDead());
	TestFalse(TEXT("Invalid lethal recovery outcome cannot strand the victim in Dying"),
		Victim->IsDying());
	TestFalse(TEXT("Fail-closed terminal handling releases paired reaction ownership"),
		HitReaction->IsInPairedAnimationState());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPairedVictimFreezeWithoutAnimInstanceFailsClosedTest,
	"KatanaCombat.PairedAnimation.Completion.LethalFreezeWithoutAnimInstanceFailsClosed",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPairedVictimFreezeWithoutAnimInstanceFailsClosedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(
		World,
		FVector(150.0f, 0.0f, 0.0f));
	UHitReactionComponent* HitReaction = Victim ? Victim->HitReactionComponent.Get() : nullptr;
	if (!TestNotNull(TEXT("Attacker should exist"), Attacker)
		|| !TestNotNull(TEXT("Victim should have hit reaction component"), HitReaction))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestNull(TEXT("Headless fixture should not have an AnimInstance"),
		Victim->GetMesh() ? Victim->GetMesh()->GetAnimInstance() : nullptr);
	HitReaction->EnterPairedAnimationState(
		NewObject<UAnimMontage>(),
		EReactionOutcome::Death,
		0.0f,
		true,
		Attacker);
	Victim->ModifyHealth(-Victim->CurrentHealth, Attacker);
	HitReaction->CompletePairedAnimationState();

	TestTrue(TEXT("Lethal freeze without an AnimInstance should still finalize death"),
		Victim->IsDead());
	TestFalse(TEXT("Lethal freeze fallback cannot strand the victim in Dying"),
		Victim->IsDying());
	TestFalse(TEXT("Lethal freeze fallback releases paired reaction ownership"),
		HitReaction->IsInPairedAnimationState());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
