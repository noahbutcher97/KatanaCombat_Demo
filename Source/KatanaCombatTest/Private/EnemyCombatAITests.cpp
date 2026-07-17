// EnemyCombatAITests.cpp
// Tests the production-shaped basic enemy combat AI surface.

#include "CombatTestHelpers.h"
#include "AI/EnemyCombatAIController.h"
#include "AI/CombatTokenSubsystem.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Core/CombatComponent.h"
#include "Data/AttackData.h"
#include "EnhancedActionKeyMapping.h"
#include "Engine/BlueprintGeneratedClass.h"
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
	TestEqual(TEXT("Consumed AI attack enters recovery"),
		CombatAI->CurrentState,
		EEnemyAIState::Recovering);
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
