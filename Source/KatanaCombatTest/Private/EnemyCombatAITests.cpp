// EnemyCombatAITests.cpp
// Tests the production-shaped basic enemy combat AI surface.

#include "CombatTestHelpers.h"
#include "AI/EnemyCombatAIController.h"
#include "AI/CombatTokenSubsystem.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/TargetingComponent.h"
#include "Core/WeaponComponent.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Debug/DefenseMatrixProofDirector.h"
#include "EnhancedActionKeyMapping.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "StateTree.h"

namespace
{
UCombatTokenSubsystem* CreateTestTokenSubsystem()
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UCombatTokenSubsystem* TokenSubsystem = NewObject<UCombatTokenSubsystem>(GameInstance);
	TokenSubsystem->MaxConcurrentAttackers = 1;
	TokenSubsystem->TokenCooldownPerEnemy = 0.0f;
	return TokenSubsystem;
}

void ConfigureSingleAttack(UEnemyCombatAIComponent* CombatAI, UAttackData* AttackData, float MaxRange = 500.0f)
{
	check(CombatAI);

	FEnemyAttackConfig AttackConfig;
	AttackConfig.AttackData = AttackData;
	AttackConfig.MinRange = 0.0f;
	AttackConfig.MaxRange = MaxRange;

	CombatAI->AvailableAttacks.Reset();
	CombatAI->AvailableAttacks.Add(AttackConfig);
	CombatAI->AttackSelectionMode = EEnemyAttackSelection::Single;
	CombatAI->ApproachConfig.AttackRange = MaxRange;
}

bool HasUsableAttack(const UEnemyCombatAIComponent* CombatAI)
{
	if (!CombatAI)
	{
		return false;
	}

	for (const FEnemyAttackConfig& AttackConfig : CombatAI->AvailableAttacks)
	{
		if (AttackConfig.AttackData)
		{
			return true;
		}
	}

	return false;
}

bool HasInputMapping(const UInputMappingContext* MappingContext, const UInputAction* Action, const FKey& Key)
{
	if (!MappingContext || !Action)
	{
		return false;
	}

	for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
	{
		if (Mapping.Action == Action && Mapping.Key == Key)
		{
			return true;
		}
	}

	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_DefaultTokenBudgetIsSingleAttacker,
	"KatanaCombat.EnemyAI.DefaultTokenBudgetIsSingleAttacker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_DefaultTokenBudgetIsSingleAttacker::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UCombatTokenSubsystem* TokenSubsystem = NewObject<UCombatTokenSubsystem>(GameInstance);

	TestEqual(TEXT("Runtime token subsystem should default to one active attacker for readable proof combat"),
		TokenSubsystem ? TokenSubsystem->MaxConcurrentAttackers : INDEX_NONE,
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_ComponentCreated,
	"KatanaCombat.EnemyAI.ComponentCreated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_ComponentCreated::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	TestNotNull(TEXT("Enemy should own CombatAIComponent"), Enemy ? Enemy->CombatAIComponent.Get() : nullptr);
	TestEqual(TEXT("FindComponentByClass should return the owned AI component"),
		Enemy ? Enemy->FindComponentByClass<UEnemyCombatAIComponent>() : nullptr,
		Enemy ? Enemy->CombatAIComponent.Get() : nullptr);
	TestEqual(TEXT("Enemy should default to the project StateTree AI controller"),
		Enemy ? Enemy->AIControllerClass.Get() : nullptr,
		AEnemyCombatAIController::StaticClass());
	TestEqual(TEXT("Enemy should auto-possess AI when placed or spawned"),
		Enemy ? static_cast<int32>(Enemy->AutoPossessAI) : INDEX_NONE,
		static_cast<int32>(EAutoPossessAI::PlacedInWorldOrSpawned));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_TargetTransitions,
	"KatanaCombat.EnemyAI.TargetTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_TargetTransitions::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));
	UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->CombatAIComponent.Get() : nullptr;

	if (!Player || !Enemy || !CombatAI)
	{
		AddError(TEXT("Failed to create Enemy AI target transition fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TestEqual(TEXT("AI should start idle"),
		static_cast<int32>(CombatAI->CurrentState),
		static_cast<int32>(EEnemyAIState::Idle));

	CombatAI->SetCombatTarget(Player);
	TestEqual(TEXT("Setting a target should enter Circling"),
		static_cast<int32>(CombatAI->CurrentState),
		static_cast<int32>(EEnemyAIState::Circling));
	TestEqual(TEXT("Distance query should measure target distance"), CombatAI->GetDistanceToTarget(), 200.0f);

	CombatAI->SetCombatTarget(nullptr);
	TestEqual(TEXT("Clearing a target while circling should return to Idle"),
		static_cast<int32>(CombatAI->CurrentState),
		static_cast<int32>(EEnemyAIState::Idle));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_TokenGrantSelectsAttack,
	"KatanaCombat.EnemyAI.TokenGrantSelectsAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_TokenGrantSelectsAttack::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->CombatAIComponent.Get() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	UAttackData* AttackData = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	if (!Player || !Enemy || !CombatAI || !TokenSubsystem || !AttackData)
	{
		AddError(TEXT("Failed to create Enemy AI token grant fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	CombatAI->SetTokenSubsystemForTesting(TokenSubsystem);
	CombatAI->SetCombatTarget(Player);
	ConfigureSingleAttack(CombatAI, AttackData);

	TestTrue(TEXT("Configured AI should be able to attempt an attack"), CombatAI->CanAttemptAttack());
	TestTrue(TEXT("Token grant should start the attack sequence"), CombatAI->TryInitiateAttack());
	TestTrue(TEXT("Enemy should hold an attack token"), CombatAI->HasAttackToken());
	TestEqual(TEXT("Token grant should transition to Approaching"),
		static_cast<int32>(CombatAI->CurrentState),
		static_cast<int32>(EEnemyAIState::Approaching));
	TestEqual(TEXT("AI should retain the selected attack"), CombatAI->SelectedAttack.Get(), AttackData);

	CombatAI->OnParried();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_DeathReleasesActiveToken,
	"KatanaCombat.EnemyAI.DeathReleasesActiveToken",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_DeathReleasesActiveToken::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->CombatAIComponent.Get() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	UAttackData* AttackData = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	if (!Player || !Enemy || !CombatAI || !TokenSubsystem || !AttackData)
	{
		AddError(TEXT("Failed to create Enemy AI death token fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	CombatAI->SetTokenSubsystemForTesting(TokenSubsystem);
	CombatAI->SetCombatTarget(Player);
	ConfigureSingleAttack(CombatAI, AttackData);

	TestTrue(TEXT("Enemy should receive an attack token before death"), CombatAI->TryInitiateAttack());
	TestTrue(TEXT("Enemy should hold the active token before death"), CombatAI->HasAttackToken());

	FCombatTestHelpers::DealLethalDamage(Enemy, Player);

	TestFalse(TEXT("Dying enemy should release its attack token immediately"), CombatAI->HasAttackToken());
	TestEqual(TEXT("Token subsystem should have no active attackers after owner death"),
		TokenSubsystem->GetActiveAttackerCount(),
		0);
	TestEqual(TEXT("Enemy AI should enter Dying state after owner death"),
		static_cast<int32>(CombatAI->CurrentState),
		static_cast<int32>(EEnemyAIState::Dying));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_QueuedTokenAdvancesNextEnemy,
	"KatanaCombat.EnemyAI.QueuedTokenAdvancesNextEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_QueuedTokenAdvancesNextEnemy::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* FirstEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* SecondEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));
	UEnemyCombatAIComponent* FirstAI = FirstEnemy ? FirstEnemy->CombatAIComponent.Get() : nullptr;
	UEnemyCombatAIComponent* SecondAI = SecondEnemy ? SecondEnemy->CombatAIComponent.Get() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	UAttackData* AttackData = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	if (!Player || !FirstAI || !SecondAI || !TokenSubsystem || !AttackData)
	{
		AddError(TEXT("Failed to create Enemy AI queued token fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	FirstAI->SetTokenSubsystemForTesting(TokenSubsystem);
	SecondAI->SetTokenSubsystemForTesting(TokenSubsystem);
	FirstAI->SetCombatTarget(Player);
	SecondAI->SetCombatTarget(Player);
	ConfigureSingleAttack(FirstAI, AttackData);
	ConfigureSingleAttack(SecondAI, AttackData);

	TestTrue(TEXT("First enemy should receive the only token"), FirstAI->TryInitiateAttack());
	TestFalse(TEXT("Second enemy should queue when token capacity is full"), SecondAI->TryInitiateAttack());
	TestTrue(TEXT("Second enemy should be waiting for a token"), SecondAI->IsWaitingForToken());

	FirstAI->OnParried();

	TestFalse(TEXT("First enemy should release its token after parry"), FirstAI->HasAttackToken());
	TestTrue(TEXT("Second enemy should receive the released token"), SecondAI->HasAttackToken());
	TestEqual(TEXT("Queued token grant should transition second enemy to Approaching"),
		static_cast<int32>(SecondAI->CurrentState),
		static_cast<int32>(EEnemyAIState::Approaching));

	SecondAI->OnParried();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_QueuedTargetClearReleasesQueue,
	"KatanaCombat.EnemyAI.QueuedTargetClearReleasesQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_QueuedTargetClearReleasesQueue::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* FirstEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* SecondEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));
	UEnemyCombatAIComponent* FirstAI = FirstEnemy ? FirstEnemy->CombatAIComponent.Get() : nullptr;
	UEnemyCombatAIComponent* SecondAI = SecondEnemy ? SecondEnemy->CombatAIComponent.Get() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	UAttackData* AttackData = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	if (!Player || !FirstAI || !SecondAI || !TokenSubsystem || !AttackData)
	{
		AddError(TEXT("Failed to create Enemy AI queued target clear fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	FirstAI->SetTokenSubsystemForTesting(TokenSubsystem);
	SecondAI->SetTokenSubsystemForTesting(TokenSubsystem);
	FirstAI->SetCombatTarget(Player);
	SecondAI->SetCombatTarget(Player);
	ConfigureSingleAttack(FirstAI, AttackData);
	ConfigureSingleAttack(SecondAI, AttackData);

	TestTrue(TEXT("First enemy should receive the only token"), FirstAI->TryInitiateAttack());
	TestFalse(TEXT("Second enemy should queue when token capacity is full"), SecondAI->TryInitiateAttack());
	TestTrue(TEXT("Second enemy should be waiting for a token"), SecondAI->IsWaitingForToken());

	SecondAI->SetCombatTarget(nullptr);

	TestFalse(TEXT("Clearing target should remove queued enemy from token queue"), SecondAI->IsWaitingForToken());
	TestNull(TEXT("Clearing target should clear queued selected attack"), SecondAI->SelectedAttack.Get());
	TestEqual(TEXT("Clearing target should return queued enemy to Idle"),
		static_cast<int32>(SecondAI->CurrentState),
		static_cast<int32>(EEnemyAIState::Idle));

	FirstAI->OnParried();

	TestFalse(TEXT("Cleared queued enemy should not receive the next token"), SecondAI->HasAttackToken());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_QueuedTokenTimeoutRemovesRequest,
	"KatanaCombat.EnemyAI.QueuedTokenTimeoutRemovesRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_QueuedTokenTimeoutRemovesRequest::RunTest(const FString& Parameters)
{
	const FString StateTreePath = TEXT("/Game/ProjectFiles/AI/ST_EnemyCombatProof.ST_EnemyCombatProof");

	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	APlayerController* PlayerController = World ? World->SpawnActor<APlayerController>() : nullptr;
	AEnemyCharacter* TokenHolder = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* QueuedEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));
	UEnemyCombatAIComponent* HolderAI = TokenHolder ? TokenHolder->CombatAIComponent.Get() : nullptr;
	UEnemyCombatAIComponent* QueuedAI = QueuedEnemy ? QueuedEnemy->CombatAIComponent.Get() : nullptr;
	AEnemyCombatAIController* QueuedController = QueuedEnemy
		? Cast<AEnemyCombatAIController>(QueuedEnemy->GetController())
		: nullptr;
	UEnemyStateTreeAIComponent* StateTreeComponent = QueuedController
		? Cast<UEnemyStateTreeAIComponent>(QueuedController->GetStateTreeAIComponent())
		: nullptr;
	UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *StateTreePath));
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	UAttackData* AttackData = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	if (!Player || !PlayerController || !HolderAI || !QueuedAI || !QueuedController || !StateTreeComponent || !StateTree || !TokenSubsystem || !AttackData)
	{
		AddError(TEXT("Failed to create Enemy AI queued token timeout fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	PlayerController->Possess(Player);
	StateTreeComponent->StopLogic(TEXT("Configure queued token timeout test"));
	StateTreeComponent->SetStateTree(StateTree);

	HolderAI->SetTokenSubsystemForTesting(TokenSubsystem);
	QueuedAI->SetTokenSubsystemForTesting(TokenSubsystem);
	HolderAI->SetCombatTarget(Player);
	QueuedAI->SetCombatTarget(Player);
	ConfigureSingleAttack(HolderAI, AttackData);
	ConfigureSingleAttack(QueuedAI, AttackData);

	TestTrue(TEXT("First enemy should occupy the only attack token"), HolderAI->TryInitiateAttack());
	StateTreeComponent->StartLogic();
	StateTreeComponent->TickComponent(0.01f, ELevelTick::LEVELTICK_All, nullptr);

	TestTrue(TEXT("Proof StateTree should be running while the second enemy waits"), StateTreeComponent->IsRunning());
	TestTrue(TEXT("Second enemy should enter the token queue through the proof StateTree"), QueuedAI->IsWaitingForToken());

	StateTreeComponent->TickComponent(3.1f, ELevelTick::LEVELTICK_All, nullptr);

	TestFalse(TEXT("StateTree timeout should remove the pending enemy from the token queue"), QueuedAI->IsWaitingForToken());
	TestNull(TEXT("StateTree timeout should clear the queued attack selection"), QueuedAI->SelectedAttack.Get());
	TestEqual(TEXT("StateTree timeout should preserve the combat target"), QueuedAI->CombatTarget.Get(), static_cast<AActor*>(Player));
	TestTrue(TEXT("Cancelling the queued request should not release another enemy's active token"), HolderAI->HasAttackToken());

	StateTreeComponent->StopLogic(TEXT("Queued token timeout test cleanup"));
	TokenSubsystem->ResetAllTokens();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_QueuedTokenStateTreeStopRemovesRequest,
	"KatanaCombat.EnemyAI.QueuedTokenStateTreeStopRemovesRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_QueuedTokenStateTreeStopRemovesRequest::RunTest(const FString& Parameters)
{
	const FString StateTreePath = TEXT("/Game/ProjectFiles/AI/ST_EnemyCombatProof.ST_EnemyCombatProof");

	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	APlayerController* PlayerController = World ? World->SpawnActor<APlayerController>() : nullptr;
	AEnemyCharacter* TokenHolder = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* QueuedEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150.0f, 0.0f, 0.0f));
	UEnemyCombatAIComponent* HolderAI = TokenHolder ? TokenHolder->CombatAIComponent.Get() : nullptr;
	UEnemyCombatAIComponent* QueuedAI = QueuedEnemy ? QueuedEnemy->CombatAIComponent.Get() : nullptr;
	AEnemyCombatAIController* QueuedController = QueuedEnemy
		? Cast<AEnemyCombatAIController>(QueuedEnemy->GetController())
		: nullptr;
	UEnemyStateTreeAIComponent* StateTreeComponent = QueuedController
		? Cast<UEnemyStateTreeAIComponent>(QueuedController->GetStateTreeAIComponent())
		: nullptr;
	UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *StateTreePath));
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	UAttackData* AttackData = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	if (!Player || !PlayerController || !HolderAI || !QueuedAI || !QueuedController || !StateTreeComponent || !StateTree || !TokenSubsystem || !AttackData)
	{
		AddError(TEXT("Failed to create Enemy AI queued StateTree stop fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	PlayerController->Possess(Player);
	StateTreeComponent->StopLogic(TEXT("Configure queued StateTree stop test"));
	StateTreeComponent->SetStateTree(StateTree);

	HolderAI->SetTokenSubsystemForTesting(TokenSubsystem);
	QueuedAI->SetTokenSubsystemForTesting(TokenSubsystem);
	HolderAI->SetCombatTarget(Player);
	QueuedAI->SetCombatTarget(Player);
	ConfigureSingleAttack(HolderAI, AttackData);
	ConfigureSingleAttack(QueuedAI, AttackData);

	TestTrue(TEXT("First enemy should occupy the only attack token"), HolderAI->TryInitiateAttack());
	StateTreeComponent->StartLogic();
	StateTreeComponent->TickComponent(0.01f, ELevelTick::LEVELTICK_All, nullptr);

	TestTrue(TEXT("Second enemy should enter the token queue before StateTree stop"), QueuedAI->IsWaitingForToken());
	TestNotNull(TEXT("Queued request should retain its selected attack while waiting"), QueuedAI->SelectedAttack.Get());

	StateTreeComponent->StopLogic(TEXT("Cancel queued token request"));

	TestFalse(TEXT("Stopping the StateTree should remove the pending enemy from the token queue"), QueuedAI->IsWaitingForToken());
	TestNull(TEXT("Stopping the StateTree should clear the queued attack selection"), QueuedAI->SelectedAttack.Get());
	TestEqual(TEXT("Stopping the StateTree should preserve the combat target"), QueuedAI->CombatTarget.Get(), static_cast<AActor*>(Player));
	TestTrue(TEXT("Stopping a queued request should not release another enemy's active token"), HolderAI->HasAttackToken());

	TokenSubsystem->ResetAllTokens();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_ExecuteFailureReleasesToken,
	"KatanaCombat.EnemyAI.ExecuteFailureReleasesToken",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_ExecuteFailureReleasesToken::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->CombatAIComponent.Get() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	UAttackData* AttackData = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);

	if (!Player || !Enemy || !CombatAI || !TokenSubsystem || !AttackData)
	{
		AddError(TEXT("Failed to create Enemy AI execute failure fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	CombatAI->SetTokenSubsystemForTesting(TokenSubsystem);
	CombatAI->SetCombatTarget(Player);
	ConfigureSingleAttack(CombatAI, AttackData);

	TestTrue(TEXT("Token grant should start the attack sequence"), CombatAI->TryInitiateAttack());
	TestTrue(TEXT("Enemy should hold an attack token before execution"), CombatAI->HasAttackToken());

	AddExpectedErrorPlain(TEXT("No anim instance"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Synthetic test enemy has no anim instance, so execution should fail cleanly"), CombatAI->ExecuteAttack());
	TestFalse(TEXT("Failed attack execution should release the token"), CombatAI->HasAttackToken());
	TestNull(TEXT("Failed attack execution should clear selected attack"), CombatAI->SelectedAttack.Get());
	TestEqual(TEXT("Failed attack execution should return to Circling when target remains valid"),
		static_cast<int32>(CombatAI->CurrentState),
		static_cast<int32>(EEnemyAIState::Circling));

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_ProofEnemyExecutionSetsCombatCurrentAttack,
	"KatanaCombat.EnemyAI.ProofEnemyExecutionSetsCombatCurrentAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_ProofEnemyExecutionSetsCombatCurrentAttack::RunTest(const FString& Parameters)
{
	const FString EnemyClassPath = TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter.BP_EnemyCharacter_C");

	UClass* EnemyClass = StaticLoadClass(AEnemyCharacter::StaticClass(), nullptr, *EnemyClassPath);
	TestNotNull(TEXT("BP_EnemyCharacter class should load"), EnemyClass);
	if (!EnemyClass)
	{
		return false;
	}

	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);
	AEnemyCharacter* Enemy = World
		? World->SpawnActor<AEnemyCharacter>(EnemyClass, FVector(150.0f, 0.0f, 0.0f), FRotator::ZeroRotator)
		: nullptr;
	UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->FindComponentByClass<UEnemyCombatAIComponent>() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();

	if (!Player || !Enemy || !CombatAI || !Enemy->CombatComponent || !TokenSubsystem)
	{
		AddError(TEXT("Failed to create proof enemy execution fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* AttackData = nullptr;
	for (const FEnemyAttackConfig& AttackConfig : CombatAI->AvailableAttacks)
	{
		if (AttackConfig.AttackData)
		{
			AttackData = AttackConfig.AttackData;
			break;
		}
	}
	TestNotNull(TEXT("Proof enemy should have a configured attack data asset"), AttackData);
	if (!AttackData)
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	CombatAI->SetTokenSubsystemForTesting(TokenSubsystem);
	CombatAI->SetCombatTarget(Player);
	ConfigureSingleAttack(CombatAI, AttackData, 500.0f);

	TestTrue(TEXT("Proof enemy should receive an attack token"), CombatAI->TryInitiateAttack());
	TestTrue(TEXT("Proof enemy attack execution should start"), CombatAI->ExecuteAttack());
	TestEqual(TEXT("Enemy attack execution should route through CombatComponent current attack state"),
		Enemy->CombatComponent->GetCurrentAttack(),
		AttackData);
	const FAttackInstanceId ExecutingAttack =
		Enemy->CombatComponent->BuildAttackExecutionSnapshot().AttackInstance;
	TestTrue(TEXT("Executing AI attack publishes a generation"), ExecutingAttack.IsValid());
	TestTrue(TEXT("Consuming the exact AI attack generation succeeds"),
		Enemy->CombatComponent->ConsumeActiveAttack(
			ExecutingAttack,
			EAttackConsumeReason::PerfectParry));
	TestTrue(TEXT("AI records exact consumed termination"),
		CombatAI->WasAttackGenerationConsumed(ExecutingAttack.AttackGeneration));
	TestFalse(TEXT("Consumed AI attack releases its token"), CombatAI->HasAttackToken());
	TestEqual(TEXT("Perfect-parry consumption enters stagger recovery"),
		CombatAI->CurrentState,
		EEnemyAIState::Staggered);
	TestEqual(TEXT("Consumed AI attack releases ownership once"),
		CombatAI->GetTokenReleaseCountForTesting(), 1);
	TestEqual(TEXT("Consumed AI attack ends once"),
		CombatAI->GetAttackEndBroadcastCountForTesting(), 1);

	const int32 ReleasesBeforeLegacyCallback = CombatAI->GetTokenReleaseCountForTesting();
	const int32 EndsBeforeLegacyCallback = CombatAI->GetAttackEndBroadcastCountForTesting();
	CombatAI->OnParried();
	TestEqual(TEXT("Legacy parry callback cannot release consumed ownership again"),
		CombatAI->GetTokenReleaseCountForTesting(), ReleasesBeforeLegacyCallback);
	TestEqual(TEXT("Legacy parry callback cannot end consumed ownership again"),
		CombatAI->GetAttackEndBroadcastCountForTesting(), EndsBeforeLegacyCallback);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackInterruptionReleasesWarp,
	"KatanaCombat.EnemyAI.AttackInterruptionReleasesWarp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackInterruptionReleasesWarp::RunTest(const FString& Parameters)
{
	const FString EnemyClassPath = TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter.BP_EnemyCharacter_C");

	UClass* EnemyClass = StaticLoadClass(AEnemyCharacter::StaticClass(), nullptr, *EnemyClassPath);
	TestNotNull(TEXT("BP_EnemyCharacter class should load"), EnemyClass);
	if (!EnemyClass)
	{
		return false;
	}

	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);
	AEnemyCharacter* Enemy = World
		? World->SpawnActor<AEnemyCharacter>(EnemyClass, FVector(150.0f, 0.0f, 0.0f), FRotator::ZeroRotator)
		: nullptr;
	UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->FindComponentByClass<UEnemyCombatAIComponent>() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	UTargetingComponent* Targeting = Enemy ? Enemy->GetTargetingComponent() : nullptr;

	if (!Player || !Enemy || !CombatAI || !Targeting || !TokenSubsystem)
	{
		AddError(TEXT("Failed to create attack interruption fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* AttackData = nullptr;
	for (const FEnemyAttackConfig& AttackConfig : CombatAI->AvailableAttacks)
	{
		if (AttackConfig.AttackData)
		{
			AttackData = AttackConfig.AttackData;
			break;
		}
	}
	TestNotNull(TEXT("Proof enemy should have a configured attack data asset"), AttackData);
	if (!AttackData)
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	CombatAI->SetTokenSubsystemForTesting(TokenSubsystem);
	CombatAI->SetCombatTarget(Player);
	ConfigureSingleAttack(CombatAI, AttackData, 500.0f);

	TestTrue(TEXT("Proof enemy should receive an attack token"), CombatAI->TryInitiateAttack());
	TestTrue(TEXT("Proof enemy attack execution should start"), CombatAI->ExecuteAttack());
	TestTrue(TEXT("Executing proof attack should own a regular attack alignment request"),
		Targeting->GetActiveAlignmentRequest().IsValid());

	CombatAI->OnDamaged();

	TestFalse(TEXT("Attack interruption releases the regular attack alignment request before blend-out"),
		Targeting->GetActiveAlignmentRequest().IsValid());
	TestFalse(TEXT("Attack interruption releases its combat token"), CombatAI->HasAttackToken());
	TestEqual(TEXT("Attack interruption enters staggered recovery"),
		CombatAI->CurrentState,
		EEnemyAIState::Staggered);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AbortAttackRestoresReusableState,
	"KatanaCombat.EnemyAI.AbortAttackRestoresReusableState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AbortAttackRestoresReusableState::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString EnemyClassPath = TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter.BP_EnemyCharacter_C");
	UClass* EnemyClass = StaticLoadClass(AEnemyCharacter::StaticClass(), nullptr, *EnemyClassPath);
	if (!TestNotNull(TEXT("BP_EnemyCharacter class should load"), EnemyClass))
	{
		return false;
	}

	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);
	AEnemyCharacter* Enemy = World
		? World->SpawnActor<AEnemyCharacter>(EnemyClass, FVector(150.0f, 0.0f, 0.0f), FRotator::ZeroRotator)
		: nullptr;
	UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->GetCombatAIComponent() : nullptr;
	UTargetingComponent* Targeting = Enemy ? Enemy->GetTargetingComponent() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	UAttackData* AttackData = CombatAI && !CombatAI->AvailableAttacks.IsEmpty()
		? CombatAI->AvailableAttacks[0].AttackData.Get()
		: nullptr;
	if (!Player || !Enemy || !CombatAI || !Targeting || !TokenSubsystem || !AttackData)
	{
		AddError(TEXT("Failed to create reusable attack-abort fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	CombatAI->SetTokenSubsystemForTesting(TokenSubsystem);
	CombatAI->SetCombatTarget(Player);
	ConfigureSingleAttack(CombatAI, AttackData, 500.0f);
	TestTrue(TEXT("Initial attack request receives a token"), CombatAI->TryInitiateAttack());
	TestTrue(TEXT("Initial attack execution starts"), CombatAI->ExecuteAttack());
	TestTrue(TEXT("Initial attack owns a warp request"), Targeting->GetActiveAlignmentRequest().IsValid());
	AddExpectedErrorPlain(TEXT("Socket 'weapon_start' not found"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("Socket 'weapon_end' not found"), EAutomationExpectedErrorFlags::Contains, 1);
	Enemy->CombatComponent->OnPhaseTransition(EAttackPhase::Active);
	TestTrue(TEXT("Active attack enables weapon tracing before abort"),
		Enemy->WeaponComponent->IsHitDetectionEnabled());

	CombatAI->AbortAttack();
	TestFalse(TEXT("Abort releases the token"), CombatAI->HasAttackToken());
	TestNull(TEXT("Abort clears the selected attack"), CombatAI->SelectedAttack.Get());
	TestEqual(TEXT("Abort returns a targeted enemy to Circling"),
		CombatAI->CurrentState, EEnemyAIState::Circling);
	TestFalse(TEXT("Abort releases the active attack warp"),
		Targeting->GetActiveAlignmentRequest().IsValid());
	TestFalse(TEXT("Abort disables weapon tracing before any blend-out frame"),
		Enemy->WeaponComponent->IsHitDetectionEnabled());
	TestEqual(TEXT("Abort retires the combat phase before repositioning"),
		Enemy->CombatComponent->GetCurrentPhase(), EAttackPhase::None);

	const int32 ReleasesAfterFirstAbort = CombatAI->GetTokenReleaseCountForTesting();
	const int32 EndsAfterFirstAbort = CombatAI->GetAttackEndBroadcastCountForTesting();
	CombatAI->AbortAttack();
	TestEqual(TEXT("Repeated abort does not release ownership twice"),
		CombatAI->GetTokenReleaseCountForTesting(), ReleasesAfterFirstAbort);
	TestEqual(TEXT("Repeated abort does not broadcast attack end twice"),
		CombatAI->GetAttackEndBroadcastCountForTesting(), EndsAfterFirstAbort);

	TestTrue(TEXT("Enemy can request another attack after abort"), CombatAI->TryInitiateAttack());
	TestTrue(TEXT("Enemy can execute another attack after abort"), CombatAI->ExecuteAttack());
	CombatAI->AbortAttack();
	TestFalse(TEXT("Second abort releases the new token"), CombatAI->HasAttackToken());
	TestEqual(TEXT("Second abort returns the enemy to Circling"),
		CombatAI->CurrentState, EEnemyAIState::Circling);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_HitReactionStaggerBlocksAttack,
	"KatanaCombat.EnemyAI.HitReactionStaggerBlocksAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_HitReactionStaggerBlocksAttack::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(
		World, FVector::ZeroVector);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(150.0f, 0.0f, 0.0f));
	UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->GetCombatAIComponent() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	UAttackData* AttackData = FCombatTestHelpers::CreateTestAttack();
	if (!World || !Player || !Enemy || !CombatAI || !TokenSubsystem || !AttackData)
	{
		AddError(TEXT("Failed to create stagger eligibility fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	CombatAI->SetTokenSubsystemForTesting(TokenSubsystem);
	CombatAI->SetCombatTarget(Player);
	ConfigureSingleAttack(CombatAI, AttackData, 500.0f);
	CombatAI->CurrentState = EEnemyAIState::Circling;
	Enemy->HitReactionComponent->ApplyStagger(1.5f, false);
	TestFalse(TEXT("A live hit-reaction stagger blocks token acquisition even in a ready AI state"),
		CombatAI->TryInitiateAttack());
	TestFalse(TEXT("Rejected staggered attack owns no token"), CombatAI->HasAttackToken());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseMatrixProofDirector_RestoresFixtureState,
	"KatanaCombat.Defense.GateB.ProofDirectorRestoresFixtureState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseMatrixProofDirector_RestoresFixtureState::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UGameInstance* GameInstance = World ? NewObject<UGameInstance>(GEngine) : nullptr;
	if (World && GameInstance)
	{
		FWorldContext& WorldContext = GEngine->GetWorldContextFromWorldChecked(World);
		WorldContext.OwningGameInstance = GameInstance;
		World->SetGameInstance(GameInstance);
		GameInstance->Init();
	}
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(
		World, FVector(10.0f, 20.0f, 0.0f));
	AEnemyCharacter* SelectedEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(200.0f, -100.0f, 0.0f));
	AEnemyCharacter* CenterEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(225.0f, 0.0f, 0.0f));
	AEnemyCharacter* OtherEnemy = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(250.0f, 100.0f, 0.0f));
	UEnemyCombatAIComponent* SelectedAI = SelectedEnemy ? SelectedEnemy->GetCombatAIComponent() : nullptr;
	UEnemyCombatAIComponent* CenterAI = CenterEnemy ? CenterEnemy->GetCombatAIComponent() : nullptr;
	UEnemyCombatAIComponent* OtherAI = OtherEnemy ? OtherEnemy->GetCombatAIComponent() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = GameInstance
		? GameInstance->GetSubsystem<UCombatTokenSubsystem>()
		: nullptr;
	UAttackData* OriginalSelectedAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* OriginalCenterAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* OriginalOtherAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	UAttackData* CaseAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	if (!Player || !SelectedEnemy || !CenterEnemy || !OtherEnemy
		|| !SelectedAI || !CenterAI || !OtherAI || !TokenSubsystem
		|| !OriginalSelectedAttack || !OriginalCenterAttack || !OriginalOtherAttack || !CaseAttack)
	{
		AddError(TEXT("Failed to create defense-matrix proof-director fixture"));
		if (GameInstance)
		{
			GameInstance->Shutdown();
		}
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	TokenSubsystem->MaxConcurrentAttackers = 1;
	TokenSubsystem->TokenCooldownPerEnemy = 0.0f;

	Player->Tags.AddUnique(TEXT("DefenseMatrix.Player"));
	SelectedEnemy->Tags.AddUnique(TEXT("DefenseMatrix.Anchor.Left"));
	CenterEnemy->Tags.AddUnique(TEXT("DefenseMatrix.Anchor.Center"));
	OtherEnemy->Tags.AddUnique(TEXT("DefenseMatrix.Anchor.Right"));
	SelectedAI->SetTokenSubsystemForTesting(TokenSubsystem);
	CenterAI->SetTokenSubsystemForTesting(TokenSubsystem);
	OtherAI->SetTokenSubsystemForTesting(TokenSubsystem);
	SelectedAI->SetCombatTarget(Player);
	CenterAI->SetCombatTarget(Player);
	OtherAI->SetCombatTarget(Player);
	ConfigureSingleAttack(SelectedAI, OriginalSelectedAttack);
	ConfigureSingleAttack(CenterAI, OriginalCenterAttack);
	ConfigureSingleAttack(OtherAI, OriginalOtherAttack);
	SelectedAI->AttackSelectionMode = EEnemyAttackSelection::Sequential;
	CenterAI->AttackSelectionMode = EEnemyAttackSelection::Single;
	OtherAI->AttackSelectionMode = EEnemyAttackSelection::Random;
	const FTransform OriginalPlayerTransform = Player->GetActorTransform();
	const FTransform OriginalSelectedTransform = SelectedEnemy->GetActorTransform();
	const FTransform OriginalCenterTransform = CenterEnemy->GetActorTransform();
	const FTransform OriginalOtherTransform = OtherEnemy->GetActorTransform();
	Player->SetHealth(87.0f);

	ADefenseMatrixProofDirector* Director = World->SpawnActorDeferred<ADefenseMatrixProofDirector>(
		ADefenseMatrixProofDirector::StaticClass(), FTransform::Identity);
	Director->bAutoStartHandsOffCase = false;
	FDefenseMatrixProofCase ProofCase;
	ProofCase.CaseName = TEXT("NormalBlockHighLeft");
	ProofCase.Attack = CaseAttack;
	ProofCase.AttackerAnchorTag = TEXT("DefenseMatrix.Anchor.Left");
	ProofCase.bApplyDefenderTransform = true;
	ProofCase.DefenderTransform = FTransform(
		FRotator(0.0f, -11.0f, 0.0f), FVector(15.0f, 25.0f, 0.0f));
	ProofCase.bApplyAttackerTransform = true;
	ProofCase.AttackerTransform = FTransform(
		FRotator(0.0f, 37.0f, 0.0f), FVector(135.0f, -42.0f, 0.0f));
	Director->Cases.Add(ProofCase);
	Director->FinishSpawning(FTransform::Identity);

	TestTrue(TEXT("Named proof case starts"), Director->StartNamedCase(ProofCase.CaseName));
	TestTrue(TEXT("Named proof case applies its defender transform before guard alignment"),
		Player->GetActorTransform().Equals(ProofCase.DefenderTransform, 0.1f));
	TestTrue(TEXT("Named proof case applies its attacker transform before attack startup"),
		SelectedEnemy->GetActorTransform().Equals(ProofCase.AttackerTransform, 0.1f));
	TestEqual(TEXT("Selected fixture uses only the case attack"), SelectedAI->AvailableAttacks.Num(), 1);
	TestEqual(TEXT("Selected fixture receives the case attack"),
		SelectedAI->AvailableAttacks[0].AttackData.Get(), CaseAttack);
	TestTrue(TEXT("Selected fixture receives the attack token"), SelectedAI->HasAttackToken());
	TestTrue(TEXT("Proof case begins held guard"), Player->GetCombatComponent()->IsBlocking());
	TestTrue(TEXT("Unselected center fixture cannot attack during the case"),
		CenterAI->AvailableAttacks.IsEmpty());
	TestTrue(TEXT("Unselected right fixture cannot attack during the case"),
		OtherAI->AvailableAttacks.IsEmpty());

	Player->SetActorLocation(FVector(999.0f, 999.0f, 0.0f));
	SelectedEnemy->SetActorLocation(FVector(888.0f, 0.0f, 0.0f));
	CenterEnemy->SetActorLocation(FVector(833.0f, 0.0f, 0.0f));
	OtherEnemy->SetActorLocation(FVector(777.0f, 0.0f, 0.0f));
	Player->SetHealth(50.0f);
	Director->ResetFixture();

	TestTrue(TEXT("Reset restores the player transform"),
		Player->GetActorTransform().Equals(OriginalPlayerTransform, 0.1f));
	TestTrue(TEXT("Reset restores the selected enemy transform"),
		SelectedEnemy->GetActorTransform().Equals(OriginalSelectedTransform, 0.1f));
	TestTrue(TEXT("Reset restores the center enemy transform"),
		CenterEnemy->GetActorTransform().Equals(OriginalCenterTransform, 0.1f));
	TestTrue(TEXT("Reset restores the other enemy transform"),
		OtherEnemy->GetActorTransform().Equals(OriginalOtherTransform, 0.1f));
	TestEqual(TEXT("Reset restores player health"), Player->CurrentHealth, 87.0f);
	TestFalse(TEXT("Reset ends held guard"), Player->GetCombatComponent()->IsBlocking());
	TestFalse(TEXT("Reset releases selected token ownership"), SelectedAI->HasAttackToken());
	TestEqual(TEXT("Reset restores selected attack selection mode"),
		SelectedAI->AttackSelectionMode, EEnemyAttackSelection::Sequential);
	TestEqual(TEXT("Reset restores center attack selection mode"),
		CenterAI->AttackSelectionMode, EEnemyAttackSelection::Single);
	TestEqual(TEXT("Reset restores other attack selection mode"),
		OtherAI->AttackSelectionMode, EEnemyAttackSelection::Random);
	TestEqual(TEXT("Reset restores one selected attack"), SelectedAI->AvailableAttacks.Num(), 1);
	if (SelectedAI->AvailableAttacks.Num() == 1)
	{
		TestEqual(TEXT("Reset restores selected attack catalog"),
			SelectedAI->AvailableAttacks[0].AttackData.Get(), OriginalSelectedAttack);
	}
	TestEqual(TEXT("Reset restores one center attack"), CenterAI->AvailableAttacks.Num(), 1);
	if (CenterAI->AvailableAttacks.Num() == 1)
	{
		TestEqual(TEXT("Reset restores center attack catalog"),
			CenterAI->AvailableAttacks[0].AttackData.Get(), OriginalCenterAttack);
	}
	TestEqual(TEXT("Reset restores one other attack"), OtherAI->AvailableAttacks.Num(), 1);
	if (OtherAI->AvailableAttacks.Num() == 1)
	{
		TestEqual(TEXT("Reset restores other attack catalog"),
			OtherAI->AvailableAttacks[0].AttackData.Get(), OriginalOtherAttack);
	}
	TestEqual(TEXT("Reset restores selected combat target"),
		SelectedAI->CombatTarget.Get(), static_cast<AActor*>(Player));
	TestEqual(TEXT("Reset restores center combat target"),
		CenterAI->CombatTarget.Get(), static_cast<AActor*>(Player));
	TestEqual(TEXT("Reset restores other combat target"),
		OtherAI->CombatTarget.Get(), static_cast<AActor*>(Player));
	TestTrue(TEXT("Reset clears the active case"), Director->ActiveCase.IsNone());

	GameInstance->Shutdown();
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_ProofAssetsLoadAndMapReady,
	"KatanaCombat.EnemyAI.ProofAssetsLoadAndMapReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_ProofAssetsLoadAndMapReady::RunTest(const FString& Parameters)
{
	const FString StateTreePath = TEXT("/Game/ProjectFiles/AI/ST_EnemyCombatProof.ST_EnemyCombatProof");
	const FString ControllerClassPath = TEXT("/Game/ProjectFiles/AI/BP_EnemyCombatAIController.BP_EnemyCombatAIController_C");
	const FString EnemyClassPath = TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter.BP_EnemyCharacter_C");
	const FString MapPackageName = TEXT("/Game/ProjectFiles/Levels/Lvl_ThirdPerson1");

	UStateTree* StateTree = Cast<UStateTree>(StaticLoadObject(UStateTree::StaticClass(), nullptr, *StateTreePath));
	TestNotNull(TEXT("Proof StateTree asset should load"), StateTree);
	if (StateTree)
	{
		TestTrue(TEXT("Proof StateTree should be ready to run"), StateTree->IsReadyToRun());
	}

	UClass* ControllerClass = StaticLoadClass(AEnemyCombatAIController::StaticClass(), nullptr, *ControllerClassPath);
	TestNotNull(TEXT("Proof controller Blueprint class should load"), ControllerClass);
	AEnemyCombatAIController* ControllerCDO = ControllerClass
		? Cast<AEnemyCombatAIController>(ControllerClass->GetDefaultObject())
		: nullptr;
	UEnemyStateTreeAIComponent* StateTreeComponent = ControllerCDO
		? Cast<UEnemyStateTreeAIComponent>(ControllerCDO->GetStateTreeAIComponent())
		: nullptr;
	TestNotNull(TEXT("Proof controller should own UEnemyStateTreeAIComponent"), StateTreeComponent);
	if (StateTreeComponent && StateTree)
	{
		TestTrue(TEXT("Proof controller should assign the proof StateTree"),
			StateTreeComponent->GetAssignedStateTree() == StateTree);
	}

	UClass* EnemyClass = StaticLoadClass(AEnemyCharacter::StaticClass(), nullptr, *EnemyClassPath);
	TestNotNull(TEXT("BP_EnemyCharacter class should load"), EnemyClass);
	AEnemyCharacter* EnemyCDO = EnemyClass ? Cast<AEnemyCharacter>(EnemyClass->GetDefaultObject()) : nullptr;
	TestNotNull(TEXT("BP_EnemyCharacter CDO should be an enemy character"), EnemyCDO);
	if (EnemyCDO && ControllerClass)
	{
		TestEqual(TEXT("BP_EnemyCharacter should default to the proof controller"),
			EnemyCDO->AIControllerClass.Get(),
			ControllerClass);
		TestEqual(TEXT("BP_EnemyCharacter should auto-possess placed/spawned AI"),
			static_cast<int32>(EnemyCDO->AutoPossessAI),
			static_cast<int32>(EAutoPossessAI::PlacedInWorldOrSpawned));
		TestTrue(TEXT("BP_EnemyCharacter should have a usable default attack"),
			HasUsableAttack(EnemyCDO->FindComponentByClass<UEnemyCombatAIComponent>()));
	}

	UInputAction* BlockAction = Cast<UInputAction>(StaticLoadObject(
		UInputAction::StaticClass(),
		nullptr,
		TEXT("/Game/ProjectFiles/Input/Actions/IA_Block.IA_Block")));
	TestNotNull(TEXT("IA_Block should load"), BlockAction);
	if (BlockAction)
	{
		TestEqual(TEXT("IA_Block should be a boolean action"),
			static_cast<int32>(BlockAction->ValueType),
			static_cast<int32>(EInputActionValueType::Boolean));
	}

	UClass* PlayerClass = StaticLoadClass(
		APlayerCharacter::StaticClass(),
		nullptr,
		TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_Player.BP_Player_C"));
	TestNotNull(TEXT("BP_Player class should load"), PlayerClass);
	APlayerCharacter* PlayerCDO = PlayerClass ? Cast<APlayerCharacter>(PlayerClass->GetDefaultObject()) : nullptr;
	TestNotNull(TEXT("BP_Player CDO should be a player character"), PlayerCDO);
	UInputMappingContext* PlayerMappingContext = PlayerCDO ? PlayerCDO->DefaultMappingContext.Get() : nullptr;
	TestNotNull(TEXT("BP_Player should have a default input mapping context"), PlayerMappingContext);
	if (PlayerCDO && BlockAction)
	{
		TestEqual(TEXT("BP_Player should assign IA_Block to BlockAction"),
			PlayerCDO->BlockAction.Get(),
			BlockAction);
	}
	if (PlayerMappingContext && BlockAction)
	{
		TestTrue(TEXT("Player mapping context should map IA_Block to Thumb Mouse Button"),
			HasInputMapping(PlayerMappingContext, BlockAction, EKeys::ThumbMouseButton));
		TestFalse(TEXT("Player mapping context should not map IA_Block to Right Mouse Button because Heavy Attack uses it"),
			HasInputMapping(PlayerMappingContext, BlockAction, EKeys::RightMouseButton));
		TestTrue(TEXT("Player mapping context should map IA_Block to Gamepad Left Shoulder"),
			HasInputMapping(PlayerMappingContext, BlockAction, EKeys::Gamepad_LeftShoulder));
	}

	FString MapFilename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(MapPackageName, MapFilename, FPackageName::GetMapPackageExtension()))
	{
		AddError(FString::Printf(TEXT("Failed to resolve map filename: %s"), *MapPackageName));
		return false;
	}

	UWorld* LoadedWorld = UEditorLoadingAndSavingUtils::LoadMap(MapFilename);
	TestNotNull(TEXT("Lvl_ThirdPerson1 should load in editor automation"), LoadedWorld);
	if (!LoadedWorld)
	{
		return false;
	}

	int32 EnemyCount = 0;
	int32 ReadyEnemyCount = 0;
	for (TActorIterator<AEnemyCharacter> It(LoadedWorld); It; ++It)
	{
		AEnemyCharacter* Enemy = *It;
		if (!Enemy || Enemy->IsTemplate())
		{
			continue;
		}

		++EnemyCount;
		TestEqual(FString::Printf(TEXT("%s should use proof controller"), *Enemy->GetName()),
			Enemy->AIControllerClass.Get(),
			ControllerClass);
		TestEqual(FString::Printf(TEXT("%s should auto-possess AI"), *Enemy->GetName()),
			static_cast<int32>(Enemy->AutoPossessAI),
			static_cast<int32>(EAutoPossessAI::PlacedInWorldOrSpawned));

		UEnemyCombatAIComponent* CombatAI = Enemy->FindComponentByClass<UEnemyCombatAIComponent>();
		TestNotNull(FString::Printf(TEXT("%s should have UEnemyCombatAIComponent"), *Enemy->GetName()), CombatAI);
		if (HasUsableAttack(CombatAI))
		{
			++ReadyEnemyCount;
		}
	}

	TestTrue(TEXT("Lvl_ThirdPerson1 should contain at least four enemy actors for the proof encounter"), EnemyCount >= 4);
	TestEqual(TEXT("All loaded proof enemies should have usable attacks"), ReadyEnemyCount, EnemyCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseMatrixProofMap_LoadedContract,
	"KatanaCombat.Defense.GateB.ProofMapLoadedContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseMatrixProofMap_LoadedContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString MapPackageName = TEXT("/Game/ProjectFiles/Levels/Test/Lvl_DefenseMatrix");
	const FString FixtureSettingsPath = TEXT(
		"/Game/ProjectFiles/Data/PDA/Defense/GateB/DA_CombatSettings_DefenseMatrix.DA_CombatSettings_DefenseMatrix");
	UCombatSettings* FixtureSettings = Cast<UCombatSettings>(StaticLoadObject(
		UCombatSettings::StaticClass(), nullptr, *FixtureSettingsPath));
	TestNotNull(TEXT("Gate B fixture combat settings should load"), FixtureSettings);

	FString MapFilename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(
		MapPackageName, MapFilename, FPackageName::GetMapPackageExtension()))
	{
		AddError(FString::Printf(TEXT("Failed to resolve Gate B map filename: %s"), *MapPackageName));
		return false;
	}
	UWorld* LoadedWorld = UEditorLoadingAndSavingUtils::LoadMap(MapFilename);
	if (!TestNotNull(TEXT("Lvl_DefenseMatrix should load in editor automation"), LoadedWorld))
	{
		return false;
	}

	ADefenseMatrixProofDirector* Director = nullptr;
	APlayerCharacter* FixturePlayer = nullptr;
	TArray<AEnemyCharacter*> FixtureEnemies;
	for (AActor* Actor : LoadedWorld->PersistentLevel->Actors)
	{
		if (ADefenseMatrixProofDirector* DirectorCandidate = Cast<ADefenseMatrixProofDirector>(Actor))
		{
			TestNull(TEXT("Proof map should contain only one director"), Director);
			Director = DirectorCandidate;
		}
		else if (APlayerCharacter* PlayerCandidate = Cast<APlayerCharacter>(Actor))
		{
			if (PlayerCandidate->ActorHasTag(TEXT("DefenseMatrix.Player")))
			{
				TestNull(TEXT("Proof map should contain only one tagged player"), FixturePlayer);
				FixturePlayer = PlayerCandidate;
			}
		}
		else if (AEnemyCharacter* EnemyCandidate = Cast<AEnemyCharacter>(Actor))
		{
			FixtureEnemies.Add(EnemyCandidate);
		}
	}

	TestNotNull(TEXT("Proof map should contain its director"), Director);
	TestNotNull(TEXT("Proof map should contain its tagged player"), FixturePlayer);
	TestEqual(TEXT("Proof map should contain exactly three enemies"), FixtureEnemies.Num(), 3);
	if (!Director || !FixturePlayer || FixtureEnemies.Num() != 3)
	{
		return false;
	}

	TestEqual(TEXT("Proof director permits exactly two concurrent attackers"),
		Director->ProofMaxConcurrentAttackers, 2);
	TestEqual(TEXT("Project token default remains one concurrent attacker"),
		GetDefault<UCombatTokenSubsystem>()->MaxConcurrentAttackers, 1);
	TestTrue(TEXT("Proof map starts a hands-off case"), Director->bAutoStartHandsOffCase);
	TestEqual(TEXT("Proof map hands-off case is the middle-center normal block"),
		Director->HandsOffCase, FName(TEXT("NormalBlockMiddleCenter")));
	TestEqual(TEXT("Proof director exposes eleven deterministic cases"), Director->Cases.Num(), 11);

	const TArray<FName> ExpectedCases = {
		TEXT("NormalBlockHighLeft"), TEXT("NormalBlockHighCenter"), TEXT("NormalBlockHighRight"),
		TEXT("NormalBlockMiddleLeft"), TEXT("NormalBlockMiddleCenter"), TEXT("NormalBlockMiddleRight"),
		TEXT("NormalBlockLowLeft"), TEXT("NormalBlockLowCenter"), TEXT("NormalBlockLowRight"),
		TEXT("UnblockableMiddleCenter"), TEXT("PerfectParryGateARegression")};
	TestTrue(TEXT("Proof director case order is canonical"), Director->GetCaseNames() == ExpectedCases);
	const TArray<FString> ExpectedAttackPaths = {
		TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_HighLeft.DA_GateB_HighLeft"),
		TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_HighCenter.DA_GateB_HighCenter"),
		TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_HighRight.DA_GateB_HighRight"),
		TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_MiddleLeft.DA_GateB_MiddleLeft"),
		TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_MiddleCenter.DA_GateB_MiddleCenter"),
		TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_MiddleRight.DA_GateB_MiddleRight"),
		TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_LowLeft.DA_GateB_LowLeft"),
		TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_LowCenter.DA_GateB_LowCenter"),
		TEXT("/Game/ProjectFiles/Data/PDA/Defense/GateB/Attacks/DA_GateB_LowRight.DA_GateB_LowRight"),
		TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_11.LightAttack_11"),
		TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1")};
	const TArray<FName> ExpectedAnchorTags = {
		TEXT("DefenseMatrix.Anchor.Left"), TEXT("DefenseMatrix.Anchor.Center"),
		TEXT("DefenseMatrix.Anchor.Right"), TEXT("DefenseMatrix.Anchor.Left"),
		TEXT("DefenseMatrix.Anchor.Center"), TEXT("DefenseMatrix.Anchor.Right"),
		TEXT("DefenseMatrix.Anchor.Left"), TEXT("DefenseMatrix.Anchor.Center"),
		TEXT("DefenseMatrix.Anchor.Right"), TEXT("DefenseMatrix.Anchor.Center"),
		TEXT("DefenseMatrix.Anchor.Center")};
	const TArray<float> ExpectedAttackerRadii = {
		185.0f, 185.0f, 185.0f,
		185.0f, 150.0f, 185.0f,
		150.0f, 150.0f, 185.0f,
		185.0f, 185.0f};
	for (const FDefenseMatrixProofCase& ProofCase : Director->Cases)
	{
		TestFalse(FString::Printf(TEXT("%s has a case name"), *ProofCase.CaseName.ToString()),
			ProofCase.CaseName.IsNone());
		TestNotNull(FString::Printf(TEXT("%s has AttackData"), *ProofCase.CaseName.ToString()),
			ProofCase.Attack.Get());
		TestFalse(FString::Printf(TEXT("%s has an attacker anchor"), *ProofCase.CaseName.ToString()),
			ProofCase.AttackerAnchorTag.IsNone());
		TestTrue(FString::Printf(TEXT("%s owns a pre-guard defender transform"),
			*ProofCase.CaseName.ToString()), ProofCase.bApplyDefenderTransform);
		TestFalse(FString::Printf(TEXT("%s has a finite pre-guard defender transform"),
			*ProofCase.CaseName.ToString()), ProofCase.DefenderTransform.ContainsNaN());
		TestTrue(FString::Printf(TEXT("%s owns a pre-attack transform"),
			*ProofCase.CaseName.ToString()), ProofCase.bApplyAttackerTransform);
		TestFalse(FString::Printf(TEXT("%s has a finite pre-attack transform"),
			*ProofCase.CaseName.ToString()), ProofCase.AttackerTransform.ContainsNaN());
	}
	for (int32 Index = 0; Index < Director->Cases.Num(); ++Index)
	{
		const FDefenseMatrixProofCase& ProofCase = Director->Cases[Index];
		TestNotNull(FString::Printf(TEXT("%s attack resolves"),
			*ProofCase.CaseName.ToString()), ProofCase.Attack.Get());
		if (ProofCase.Attack)
		{
			TestEqual(FString::Printf(TEXT("%s uses the canonical attack"),
				*ProofCase.CaseName.ToString()),
				ProofCase.Attack->GetPathName(), ExpectedAttackPaths[Index]);
		}
		TestEqual(FString::Printf(TEXT("%s uses the canonical anchor"),
			*ProofCase.CaseName.ToString()),
			ProofCase.AttackerAnchorTag, ExpectedAnchorTags[Index]);
		TestTrue(FString::Printf(TEXT("%s owns the canonical defender transform"),
			*ProofCase.CaseName.ToString()),
			ProofCase.DefenderTransform.Equals(
				FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 96.0f)), 0.1f));
		const FVector ExpectedAttackerLocation(ExpectedAttackerRadii[Index], 0.0f, 96.0f);
		const FTransform ExpectedAttackerTransform(
			FRotator(0.0f, 180.0f, 0.0f), ExpectedAttackerLocation);
		TestTrue(FString::Printf(TEXT("%s owns the calibrated attacker transform"),
			*ProofCase.CaseName.ToString()),
			ProofCase.AttackerTransform.Equals(ExpectedAttackerTransform, 0.1f));
		TestEqual(FString::Printf(TEXT("%s has the canonical guard-start mode"),
			*ProofCase.CaseName.ToString()),
			ProofCase.bBeginHeldGuard, Index != Director->Cases.Num() - 1);
	}

	TestEqual(TEXT("Fixture player uses Gate B combat settings"),
		FixturePlayer->CombatSettings.Get(), FixtureSettings);
	TSet<FName> EnemyAnchorTags;
	for (AEnemyCharacter* Enemy : FixtureEnemies)
	{
		TestEqual(FString::Printf(TEXT("%s uses Gate B combat settings"), *Enemy->GetName()),
			Enemy->CombatSettings.Get(), FixtureSettings);
		UEnemyCombatAIComponent* CombatAI = Enemy->GetCombatAIComponent();
		TestNotNull(FString::Printf(TEXT("%s has combat AI"), *Enemy->GetName()), CombatAI);
		if (CombatAI)
		{
			TestEqual(FString::Printf(TEXT("%s has all eleven proof attacks"), *Enemy->GetName()),
				CombatAI->AvailableAttacks.Num(), ExpectedAttackPaths.Num());
			for (int32 Index = 0;
				Index < FMath::Min(CombatAI->AvailableAttacks.Num(), ExpectedAttackPaths.Num());
				++Index)
			{
				const UAttackData* Attack = CombatAI->AvailableAttacks[Index].AttackData.Get();
				TestNotNull(FString::Printf(TEXT("%s attack %d resolves"),
					*Enemy->GetName(), Index), Attack);
				if (Attack)
				{
					TestEqual(FString::Printf(TEXT("%s attack %d follows the canonical catalog"),
						*Enemy->GetName(), Index), Attack->GetPathName(), ExpectedAttackPaths[Index]);
				}
			}
			TestEqual(FString::Printf(TEXT("%s defaults to sequential proof selection"), *Enemy->GetName()),
				CombatAI->AttackSelectionMode, EEnemyAttackSelection::Sequential);
		}
		for (const FName Tag : Enemy->Tags)
		{
			if (Tag.ToString().StartsWith(TEXT("DefenseMatrix.Anchor.")))
			{
				EnemyAnchorTags.Add(Tag);
			}
		}
	}
	TestTrue(TEXT("Proof map contains the left attacker anchor"),
		EnemyAnchorTags.Contains(TEXT("DefenseMatrix.Anchor.Left")));
	TestTrue(TEXT("Proof map contains the center attacker anchor"),
		EnemyAnchorTags.Contains(TEXT("DefenseMatrix.Anchor.Center")));
	TestTrue(TEXT("Proof map contains the right attacker anchor"),
		EnemyAnchorTags.Contains(TEXT("DefenseMatrix.Anchor.Right")));
	return true;
}
