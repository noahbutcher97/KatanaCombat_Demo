// EnemyCombatAITests.cpp
// Tests the production-shaped basic enemy combat AI surface.

#include "CombatTestHelpers.h"
#include "AI/CombatTokenSubsystem.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Characters/EnemyCharacter.h"
#include "Data/AttackData.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

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
