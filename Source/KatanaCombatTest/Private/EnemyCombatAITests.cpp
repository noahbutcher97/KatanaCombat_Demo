// EnemyCombatAITests.cpp
// Tests the production-shaped basic enemy combat AI surface.

#include "CombatTestHelpers.h"
#include "AI/EnemyCombatAIController.h"
#include "AI/CombatTokenSubsystem.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Animation/AnimInstance.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
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

struct FEnemyAttackLifecycleFixture
{
	UWorld* World = nullptr;
	APlayerCharacter* Player = nullptr;
	AEnemyCharacter* Enemy = nullptr;
	UEnemyCombatAIComponent* CombatAI = nullptr;
	UCombatTokenSubsystem* TokenSubsystem = nullptr;
	UAttackData* AttackData = nullptr;

	bool IsValid() const
	{
		return World && Player && Enemy && CombatAI && Enemy->CombatComponent
			&& TokenSubsystem && AttackData && AttackData->AttackMontage;
	}

	void Destroy() const
	{
		FCombatTestHelpers::DestroyTestWorld(World);
	}
};

FEnemyAttackLifecycleFixture CreateEnemyAttackLifecycleFixture()
{
	FEnemyAttackLifecycleFixture Fixture;
	const FString EnemyClassPath =
		TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter.BP_EnemyCharacter_C");
	UClass* EnemyClass = StaticLoadClass(AEnemyCharacter::StaticClass(), nullptr, *EnemyClassPath);
	Fixture.World = EnemyClass ? FCombatTestHelpers::CreateTestWorld() : nullptr;
	Fixture.Player = FCombatTestHelpers::CreateTestPlayerCharacter(
		Fixture.World,
		FVector::ZeroVector);
	Fixture.Enemy = Fixture.World
		? Fixture.World->SpawnActor<AEnemyCharacter>(
			EnemyClass,
			FVector(150.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator)
		: nullptr;
	Fixture.CombatAI = Fixture.Enemy ? Fixture.Enemy->GetCombatAIComponent() : nullptr;
	Fixture.TokenSubsystem = CreateTestTokenSubsystem();
	Fixture.AttackData = Fixture.CombatAI && !Fixture.CombatAI->AvailableAttacks.IsEmpty()
		? Fixture.CombatAI->AvailableAttacks[0].AttackData.Get()
		: nullptr;
	if (Fixture.CombatAI && Fixture.TokenSubsystem && Fixture.AttackData && Fixture.Player)
	{
		Fixture.CombatAI->SetTokenSubsystemForTesting(Fixture.TokenSubsystem);
		Fixture.CombatAI->SetCombatTarget(Fixture.Player);
		ConfigureSingleAttack(Fixture.CombatAI, Fixture.AttackData, 500.0f);
	}
	return Fixture;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_ResetAllTokensReentrantMutation,
	"KatanaCombat.EnemyAI.Tokens.ResetAllTokensReentrantMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_ResetAllTokensReentrantMutation::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AActor* FirstAttacker = World ? World->SpawnActor<AActor>() : nullptr;
	AActor* SecondAttacker = World ? World->SpawnActor<AActor>() : nullptr;
	AActor* QueuedAttacker = World ? World->SpawnActor<AActor>() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	if (!TestTrue(TEXT("Reentrant token reset fixture should be valid"),
		World && FirstAttacker && SecondAttacker && QueuedAttacker && TokenSubsystem))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TokenSubsystem->MaxConcurrentAttackers = 2;
	TestTrue(TEXT("First attacker receives a token"),
		TokenSubsystem->RequestAttackToken(FirstAttacker));
	TestTrue(TEXT("Second attacker receives a token"),
		TokenSubsystem->RequestAttackToken(SecondAttacker));
	TestFalse(TEXT("Third attacker waits while both tokens are occupied"),
		TokenSubsystem->RequestAttackToken(QueuedAttacker));

	bool bReentrantResetRan = false;
	int32 ActiveCountObservedDuringRelease = INDEX_NONE;
	int32 QueueLengthObservedDuringRelease = INDEX_NONE;
	const TWeakObjectPtr<UCombatTokenSubsystem> WeakTokenSubsystem = TokenSubsystem;
	TokenSubsystem->SetPostTokenReleasedHookForTesting(
		[WeakTokenSubsystem,
			&bReentrantResetRan,
			&ActiveCountObservedDuringRelease,
			&QueueLengthObservedDuringRelease](AActor*)
		{
			if (UCombatTokenSubsystem* ReentrantTokenSubsystem = WeakTokenSubsystem.Get())
			{
				ActiveCountObservedDuringRelease = ReentrantTokenSubsystem->GetActiveAttackerCount();
				QueueLengthObservedDuringRelease = ReentrantTokenSubsystem->GetQueueLength();
				bReentrantResetRan = true;
				ReentrantTokenSubsystem->ResetAllTokens();
			}
		});

	TokenSubsystem->ResetAllTokens();

	TestTrue(TEXT("Release callback reenters token reset"), bReentrantResetRan);
	TestEqual(TEXT("Release callbacks observe cleared active ownership"),
		ActiveCountObservedDuringRelease, 0);
	TestEqual(TEXT("Release callbacks observe an already-cleared queue"),
		QueueLengthObservedDuringRelease, 0);
	TestEqual(TEXT("Each snapshotted token holder receives one release broadcast"),
		TokenSubsystem->GetTokenReleaseBroadcastCountForTesting(), 2);
	TestEqual(TEXT("Reentrant reset leaves no active token owners"),
		TokenSubsystem->GetActiveAttackerCount(), 0);
	TestEqual(TEXT("Reentrant reset leaves no queued token requests"),
		TokenSubsystem->GetQueueLength(), 0);

	FCombatTestHelpers::DestroyTestWorld(World);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_TargetClearTokenReleaseReentrantReplacement,
	"KatanaCombat.EnemyAI.TargetTransitions.TokenReleaseReentrantReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_TargetClearTokenReleaseReentrantReplacement::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Target replacement fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Initial attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	const TWeakObjectPtr<UEnemyCombatAIComponent> WeakCombatAI = Fixture.CombatAI;
	const TWeakObjectPtr<AActor> WeakTarget = Fixture.Player;
	Fixture.TokenSubsystem->SetPostTokenReleasedHookForTesting(
		[WeakCombatAI, WeakTarget](AActor*)
		{
			if (UEnemyCombatAIComponent* ReentrantCombatAI = WeakCombatAI.Get())
			{
				ReentrantCombatAI->SetCombatTarget(WeakTarget.Get());
			}
		});

	Fixture.CombatAI->SetCombatTarget(nullptr);

	TestTrue(TEXT("Token-release reentry should retain the replacement target"),
		Fixture.CombatAI->CombatTarget.Get() == static_cast<AActor*>(Fixture.Player));
	TestEqual(TEXT("A replacement target without attack ownership should return to Circling"),
		Fixture.CombatAI->CurrentState, EEnemyAIState::Circling);
	TestFalse(TEXT("Target replacement should not restore released token ownership"),
		Fixture.CombatAI->HasAttackToken());
	TestNull(TEXT("Target replacement should not restore the cleared attack selection"),
		Fixture.CombatAI->SelectedAttack.Get());

	Fixture.Destroy();
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_ImmediateTokenGrantCallbackAbort,
	"KatanaCombat.EnemyAI.AttackRequest.ImmediateTokenGrantCallbackAbort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_ImmediateTokenGrantCallbackAbort::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Immediate grant callback fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	Fixture.CombatAI->OnTokenGranted.AddDynamic(
		Fixture.CombatAI,
		&UEnemyCombatAIComponent::AbortAttack);

	TestFalse(TEXT("A grant callback abort should invalidate the outer attack request"),
		Fixture.CombatAI->TryInitiateAttack());
	TestFalse(TEXT("The aborted grant should not retain token ownership"),
		Fixture.CombatAI->HasAttackToken());
	TestNull(TEXT("The aborted grant should clear attack selection"),
		Fixture.CombatAI->SelectedAttack.Get());
	TestEqual(TEXT("The aborted grant should return to a target-aware ready state"),
		Fixture.CombatAI->CurrentState, EEnemyAIState::Circling);

	Fixture.Destroy();
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_QueuedGrantStateTransitionAbort,
	"KatanaCombat.EnemyAI.AttackRequest.QueuedGrantStateTransitionAbort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_QueuedGrantStateTransitionAbort::RunTest(const FString& Parameters)
{
	(void)Parameters;
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
		AddError(TEXT("Failed to create queued grant state-transition fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	FirstAI->SetTokenSubsystemForTesting(TokenSubsystem);
	SecondAI->SetTokenSubsystemForTesting(TokenSubsystem);
	FirstAI->SetCombatTarget(Player);
	SecondAI->SetCombatTarget(Player);
	ConfigureSingleAttack(FirstAI, AttackData);
	ConfigureSingleAttack(SecondAI, AttackData);

	TestTrue(TEXT("First enemy receives the only token"), FirstAI->TryInitiateAttack());
	TestFalse(TEXT("Second enemy queues while the token is occupied"), SecondAI->TryInitiateAttack());
	const TWeakObjectPtr<UEnemyCombatAIComponent> WeakSecondAI = SecondAI;
	SecondAI->SetPostApproachStateTransitionHookForTesting(
		[WeakSecondAI]()
		{
			if (UEnemyCombatAIComponent* ReentrantCombatAI = WeakSecondAI.Get())
			{
				ReentrantCombatAI->AbortAttack();
			}
		});

	FirstAI->OnParried();

	TestEqual(TEXT("A cancelled queued grant must not publish a stale component grant event"),
		SecondAI->GetTokenGrantBroadcastCountForTesting(), 0);
	TestFalse(TEXT("Cancelled queued grant should not retain its token"), SecondAI->HasAttackToken());
	TestFalse(TEXT("Cancelled queued grant should leave the queue"), SecondAI->IsWaitingForToken());
	TestNull(TEXT("Cancelled queued grant should clear attack selection"), SecondAI->SelectedAttack.Get());
	TestEqual(TEXT("Cancelled queued grant should return to Circling"),
		SecondAI->CurrentState, EEnemyAIState::Circling);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackStartupReentrantSelectionClear,
	"KatanaCombat.EnemyAI.AttackStartup.ReentrantSelectionClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackStartupReentrantSelectionClear::RunTest(const FString& Parameters)
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
		? World->SpawnActor<AEnemyCharacter>(
			EnemyClass,
			FVector(150.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator)
		: nullptr;
	UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->GetCombatAIComponent() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	UAttackData* AttackData = CombatAI && !CombatAI->AvailableAttacks.IsEmpty()
		? CombatAI->AvailableAttacks[0].AttackData.Get()
		: nullptr;
	if (!Player || !Enemy || !CombatAI || !Enemy->CombatComponent || !TokenSubsystem || !AttackData)
	{
		AddError(TEXT("Failed to create reentrant attack-start fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	CombatAI->SetTokenSubsystemForTesting(TokenSubsystem);
	CombatAI->SetCombatTarget(Player);
	ConfigureSingleAttack(CombatAI, AttackData, 500.0f);
	TestTrue(TEXT("Attack request receives a token"), CombatAI->TryInitiateAttack());

	const TWeakObjectPtr<UEnemyCombatAIComponent> WeakCombatAI = CombatAI;
	CombatAI->SetPostExecuteAttackDataHookForTesting([WeakCombatAI]()
	{
		if (UEnemyCombatAIComponent* ReentrantCombatAI = WeakCombatAI.Get())
		{
			ReentrantCombatAI->AbortAttack();
		}
	});

	TestFalse(TEXT("A synchronously aborted startup does not report an active attack"),
		CombatAI->ExecuteAttack());
	TestNull(TEXT("Reentrant cleanup clears the selected attack"), CombatAI->SelectedAttack.Get());
	TestFalse(TEXT("Reentrant cleanup releases the attack token"), CombatAI->HasAttackToken());
	TestEqual(TEXT("Reentrant cleanup releases token ownership exactly once"),
		CombatAI->GetTokenReleaseCountForTesting(), 1);
	TestEqual(TEXT("A provisional attack does not broadcast a committed attack end"),
		CombatAI->GetAttackEndBroadcastCountForTesting(), 0);
	TestEqual(TEXT("Reentrant cleanup returns the targeted enemy to Circling"),
		CombatAI->CurrentState, EEnemyAIState::Circling);
	TestEqual(TEXT("Reentrant cleanup retires the CombatComponent attack phase"),
		Enemy->CombatComponent->GetCurrentPhase(), EAttackPhase::None);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackStartupSynchronousConsume,
	"KatanaCombat.EnemyAI.AttackStartup.SynchronousConsume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackStartupSynchronousConsume::RunTest(const FString& Parameters)
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
		? World->SpawnActor<AEnemyCharacter>(
			EnemyClass,
			FVector(150.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator)
		: nullptr;
	UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->GetCombatAIComponent() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	UAttackData* AttackData = CombatAI && !CombatAI->AvailableAttacks.IsEmpty()
		? CombatAI->AvailableAttacks[0].AttackData.Get()
		: nullptr;
	if (!Player || !Enemy || !CombatAI || !Enemy->CombatComponent || !TokenSubsystem || !AttackData)
	{
		AddError(TEXT("Failed to create synchronous-consume attack-start fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	CombatAI->SetTokenSubsystemForTesting(TokenSubsystem);
	CombatAI->SetCombatTarget(Player);
	ConfigureSingleAttack(CombatAI, AttackData, 500.0f);
	TestTrue(TEXT("Attack request receives a token"), CombatAI->TryInitiateAttack());

	FAttackInstanceId ConsumedAttack;
	const TWeakObjectPtr<UCombatComponent> WeakCombat = Enemy->CombatComponent;
	CombatAI->SetPostExecuteAttackDataHookForTesting([WeakCombat, &ConsumedAttack]()
	{
		if (UCombatComponent* Combat = WeakCombat.Get())
		{
			ConsumedAttack = Combat->BuildAttackExecutionSnapshot().AttackInstance;
			Combat->ConsumeActiveAttack(ConsumedAttack, EAttackConsumeReason::Cancelled);
		}
	});

	TestTrue(TEXT("A synchronously consumed attack still reports a started generation"),
		CombatAI->ExecuteAttack());
	TestTrue(TEXT("The hook consumed a valid attack generation"), ConsumedAttack.IsValid());
	TestTrue(TEXT("The AI records the synchronously consumed generation"),
		CombatAI->WasAttackGenerationConsumed(ConsumedAttack.AttackGeneration));
	TestTrue(TEXT("Synchronous consumption preserves the exact started identity"),
		CombatAI->GetLastStartedAttackInstance() == ConsumedAttack);
	TestFalse(TEXT("A synchronously consumed attack is no longer active"),
		CombatAI->GetActiveAttackInstance().IsValid());
	TestTrue(TEXT("StateTree observes synchronous consumption as success"),
		CombatAI->GetAttackExecutionStatus(ConsumedAttack)
			== EEnemyAttackExecutionStatus::Succeeded);
	TestFalse(TEXT("Synchronous consumption releases the attack token"), CombatAI->HasAttackToken());
	TestEqual(TEXT("Synchronous consumption enters recovery"),
		CombatAI->CurrentState, EEnemyAIState::Recovering);
	TestEqual(TEXT("Synchronous consumption releases token ownership exactly once"),
		CombatAI->GetTokenReleaseCountForTesting(), 1);
	TestEqual(TEXT("Synchronous consumption ends attack ownership exactly once"),
		CombatAI->GetAttackEndBroadcastCountForTesting(), 1);
	TestEqual(TEXT("Synchronous consumption retires the CombatComponent attack phase"),
		Enemy->CombatComponent->GetCurrentPhase(), EAttackPhase::None);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_ConsumedResultSurvivesNewerConsumption,
	"KatanaCombat.EnemyAI.AttackLifecycle.ConsumedResultSurvivesNewerConsumption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_ConsumedResultSurvivesNewerConsumption::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Consumed-result history fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	FAttackInstanceId FirstConsumedAttack;
	TestTrue(TEXT("First attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	const TWeakObjectPtr<UCombatComponent> WeakCombat = Fixture.Enemy->CombatComponent;
	Fixture.CombatAI->SetPostExecuteAttackDataHookForTesting(
		[WeakCombat, &FirstConsumedAttack]()
		{
			if (UCombatComponent* Combat = WeakCombat.Get())
			{
				FirstConsumedAttack = Combat->BuildAttackExecutionSnapshot().AttackInstance;
				Combat->ConsumeActiveAttack(FirstConsumedAttack, EAttackConsumeReason::Cancelled);
			}
		});
	FAttackInstanceId FirstStartedAttack;
	TestTrue(TEXT("First consumed attack reports its exact identity"),
		Fixture.CombatAI->ExecuteAttackWithIdentity(FirstStartedAttack));
	TestTrue(TEXT("First started and consumed identities match"),
		FirstConsumedAttack.IsValid() && FirstStartedAttack == FirstConsumedAttack);

	Fixture.CombatAI->AbortAttack();
	FAttackInstanceId SecondConsumedAttack;
	TestTrue(TEXT("Second attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	Fixture.CombatAI->SetPostExecuteAttackDataHookForTesting(
		[WeakCombat, &SecondConsumedAttack]()
		{
			if (UCombatComponent* Combat = WeakCombat.Get())
			{
				SecondConsumedAttack = Combat->BuildAttackExecutionSnapshot().AttackInstance;
				Combat->ConsumeActiveAttack(SecondConsumedAttack, EAttackConsumeReason::Cancelled);
			}
		});
	FAttackInstanceId SecondStartedAttack;
	TestTrue(TEXT("Second consumed attack reports its exact identity"),
		Fixture.CombatAI->ExecuteAttackWithIdentity(SecondStartedAttack));
	TestTrue(TEXT("Second started and consumed identities match"),
		SecondConsumedAttack.IsValid() && SecondStartedAttack == SecondConsumedAttack);
	TestTrue(TEXT("The second consumed identity is newer"),
		SecondConsumedAttack.Attacker == FirstConsumedAttack.Attacker
			&& SecondConsumedAttack.AttackGeneration > FirstConsumedAttack.AttackGeneration);

	TestTrue(TEXT("The first consumed identity remains recorded"),
		Fixture.CombatAI->WasAttackInstanceConsumed(FirstConsumedAttack));
	TestTrue(TEXT("The first consumed identity remains a successful task result"),
		Fixture.CombatAI->GetAttackExecutionStatus(FirstConsumedAttack)
			== EEnemyAttackExecutionStatus::Succeeded);
	TestTrue(TEXT("The second consumed identity is a successful task result"),
		Fixture.CombatAI->GetAttackExecutionStatus(SecondConsumedAttack)
			== EEnemyAttackExecutionStatus::Succeeded);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackStartupOwnerDeath,
	"KatanaCombat.EnemyAI.AttackStartup.OwnerDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackStartupOwnerDeath::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AddExpectedErrorPlain(
		TEXT("Can't remove TextureShare 'defaultsharename' - not exist"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	const FString EnemyClassPath = TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter.BP_EnemyCharacter_C");
	UClass* EnemyClass = StaticLoadClass(AEnemyCharacter::StaticClass(), nullptr, *EnemyClassPath);
	if (!TestNotNull(TEXT("BP_EnemyCharacter class should load"), EnemyClass))
	{
		return false;
	}

	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector::ZeroVector);
	AEnemyCharacter* Enemy = World
		? World->SpawnActor<AEnemyCharacter>(
			EnemyClass,
			FVector(150.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator)
		: nullptr;
	UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->GetCombatAIComponent() : nullptr;
	UCombatTokenSubsystem* TokenSubsystem = CreateTestTokenSubsystem();
	UAttackData* AttackData = CombatAI && !CombatAI->AvailableAttacks.IsEmpty()
		? CombatAI->AvailableAttacks[0].AttackData.Get()
		: nullptr;
	if (!Player || !Enemy || !CombatAI || !Enemy->CombatComponent || !TokenSubsystem || !AttackData)
	{
		AddError(TEXT("Failed to create owner-death attack-start fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	CombatAI->SetTokenSubsystemForTesting(TokenSubsystem);
	CombatAI->SetCombatTarget(Player);
	ConfigureSingleAttack(CombatAI, AttackData, 500.0f);
	TestTrue(TEXT("Attack request receives a token"), CombatAI->TryInitiateAttack());

	bool bLethalDamageApplied = false;
	const TWeakObjectPtr<AEnemyCharacter> WeakEnemy = Enemy;
	const TWeakObjectPtr<APlayerCharacter> WeakPlayer = Player;
	CombatAI->SetPostExecuteAttackDataHookForTesting([WeakEnemy, WeakPlayer, &bLethalDamageApplied]()
	{
		if (AEnemyCharacter* ReentrantEnemy = WeakEnemy.Get())
		{
			bLethalDamageApplied = FCombatTestHelpers::DealLethalDamage(
				ReentrantEnemy,
				WeakPlayer.Get());
		}
	});

	TestFalse(TEXT("An attack invalidated by synchronous owner death does not remain active"),
		CombatAI->ExecuteAttack());
	TestTrue(TEXT("Startup invalidation uses the production lethal-damage path"),
		bLethalDamageApplied);
	TestEqual(TEXT("Owner death remains terminal after startup returns"),
		CombatAI->CurrentState, EEnemyAIState::Dying);
	TestFalse(TEXT("Owner death releases the attack token"), CombatAI->HasAttackToken());
	TestEqual(TEXT("Owner death releases token ownership exactly once"),
		CombatAI->GetTokenReleaseCountForTesting(), 1);
	TestEqual(TEXT("A provisional startup does not broadcast a committed attack end"),
		CombatAI->GetAttackEndBroadcastCountForTesting(), 0);
	TestEqual(TEXT("Owner death retires the CombatComponent attack phase"),
		Enemy->CombatComponent->GetCurrentPhase(), EAttackPhase::None);

	World->Tick(ELevelTick::LEVELTICK_All, CombatAI->PostAttackRecoveryTime + 0.1f);
	TestEqual(TEXT("No recovery timer can revive a dying owner"),
		CombatAI->CurrentState, EEnemyAIState::Dying);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_LethalMontageStopDoesNotContinue,
	"KatanaCombat.EnemyAI.AttackStartup.LethalMontageStopDoesNotContinue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_LethalMontageStopDoesNotContinue::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Lethal montage-stop fixture should be valid"), Fixture.IsValid())
		|| !TestNotNull(TEXT("Enemy should have a hit-reaction component"),
			Fixture.Enemy ? Fixture.Enemy->HitReactionComponent.Get() : nullptr))
	{
		Fixture.Destroy();
		return false;
	}

	UAnimInstance* AnimInstance = Fixture.Enemy->GetMesh()
		? Fixture.Enemy->GetMesh()->GetAnimInstance()
		: nullptr;
	if (!TestNotNull(TEXT("Enemy should have an animation instance"), AnimInstance))
	{
		Fixture.Destroy();
		return false;
	}
	UAttackData* PreviousAttackData = NewObject<UAttackData>();
	PreviousAttackData->AttackMontage = Fixture.AttackData->AttackMontage;
	Fixture.Enemy->CombatComponent->SeedAttackWindowStateForTesting(
		PreviousAttackData,
		EAttackPhase::Recovery,
		Fixture.Enemy->CombatComponent->GetCurrentAttackGeneration());

	TestTrue(TEXT("Existing montage should start"),
		AnimInstance->Montage_Play(Fixture.AttackData->AttackMontage) > 0.0f);
	TestEqual(TEXT("Fixture should expose the montage that attack startup will stop"),
		AnimInstance->GetCurrentActiveMontage(), Fixture.AttackData->AttackMontage.Get());

	bool bLethalBlendOutObserved = false;
	const TWeakObjectPtr<AEnemyCharacter> WeakEnemy = Fixture.Enemy;
	const TWeakObjectPtr<APlayerCharacter> WeakPlayer = Fixture.Player;
	FOnMontageBlendingOutStarted LethalBlendOutDelegate;
	LethalBlendOutDelegate.BindLambda(
		[WeakEnemy, WeakPlayer, &bLethalBlendOutObserved](UAnimMontage*, bool)
		{
			bLethalBlendOutObserved = true;
			FCombatTestHelpers::DealLethalDamage(WeakEnemy.Get(), WeakPlayer.Get());
		});
	AnimInstance->Montage_SetBlendingOutDelegate(
		LethalBlendOutDelegate,
		Fixture.AttackData->AttackMontage);

	TestTrue(TEXT("Attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	TestFalse(TEXT("Attack startup invalidated by lethal montage stop must fail"),
		Fixture.CombatAI->ExecuteAttack());
	TestTrue(TEXT("Attack startup must traverse the synchronous lethal blend-out callback"),
		bLethalBlendOutObserved);
	TestTrue(TEXT("Lethal blend-out finalizes the owner"), Fixture.Enemy->IsDeadOrDying());
	TestEqual(TEXT("Dead owner cannot enter a new combat phase"),
		Fixture.Enemy->CombatComponent->GetCurrentPhase(), EAttackPhase::None);
	TestNull(TEXT("Dead owner cannot retain new attack data"),
		Fixture.Enemy->CombatComponent->GetCurrentAttack());
	TestFalse(TEXT("Dead owner cannot restart the stopped attack montage"),
		AnimInstance->Montage_IsPlaying(Fixture.AttackData->AttackMontage));
	TestFalse(TEXT("Lethal callback releases token ownership"), Fixture.CombatAI->HasAttackToken());
	TestEqual(TEXT("Lethal callback remains terminal for AI state"),
		Fixture.CombatAI->CurrentState, EEnemyAIState::Dying);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_QueuedLethalMontageStopIsSafe,
	"KatanaCombat.EnemyAI.AttackStartup.QueuedLethalMontageStopIsSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_QueuedLethalMontageStopIsSafe::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Queued lethal montage-stop fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	UCombatComponent* Combat = Fixture.Enemy->CombatComponent.Get();
	UAnimInstance* AnimInstance = Fixture.Enemy->GetMesh()
		? Fixture.Enemy->GetMesh()->GetAnimInstance()
		: nullptr;
	if (!TestNotNull(TEXT("Queued fixture should have an animation instance"), AnimInstance))
	{
		Fixture.Destroy();
		return false;
	}

	Combat->SeedAttackWindowStateForTesting(
		Fixture.AttackData,
		EAttackPhase::Windup,
		Combat->GetCurrentAttackGeneration());
	FScriptDelegate CombatDeathResetDelegate;
	CombatDeathResetDelegate.BindUFunction(Combat, TEXT("OnCharacterDeath"));
	Fixture.Enemy->OnCharacterDying.AddUnique(CombatDeathResetDelegate);
	TestTrue(TEXT("Queued fixture montage should start"),
		AnimInstance->Montage_Play(Fixture.AttackData->AttackMontage) > 0.0f);

	bool bLethalBlendOutObserved = false;
	const TWeakObjectPtr<AEnemyCharacter> WeakEnemy = Fixture.Enemy;
	const TWeakObjectPtr<APlayerCharacter> WeakPlayer = Fixture.Player;
	FOnMontageBlendingOutStarted LethalBlendOutDelegate;
	LethalBlendOutDelegate.BindLambda(
		[WeakEnemy, WeakPlayer, &bLethalBlendOutObserved](UAnimMontage*, bool)
		{
			bLethalBlendOutObserved = true;
			FCombatTestHelpers::DealLethalDamage(WeakEnemy.Get(), WeakPlayer.Get());
		});
	AnimInstance->Montage_SetBlendingOutDelegate(
		LethalBlendOutDelegate,
		Fixture.AttackData->AttackMontage);

	const float CurrentTime = Fixture.World->GetTimeSeconds();
	Combat->QueueAction(FQueuedInputAction(
		EInputType::LightAttack,
		EInputEventType::Press,
		CurrentTime,
		false), Fixture.AttackData);
	TestEqual(TEXT("Windup input should be queued for recovery"), Combat->GetQueueSize(), 1);

	Combat->ProcessQueuedActions(EAttackPhase::Recovery);
	TestTrue(TEXT("Queued execution traverses the synchronous lethal callback"),
		bLethalBlendOutObserved);
	TestTrue(TEXT("Queued lethal callback finalizes the owner"), Fixture.Enemy->IsDeadOrDying());
	TestTrue(TEXT("Terminal teardown leaves no queued entry"), Combat->IsQueueEmpty());
	TestEqual(TEXT("Terminal teardown leaves no active phase"),
		Combat->GetCurrentPhase(), EAttackPhase::None);
	TestFalse(TEXT("Terminal teardown cannot restart the attack montage"),
		AnimInstance->Montage_IsPlaying(Fixture.AttackData->AttackMontage));

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackStartupStateTransitionReplacement,
	"KatanaCombat.EnemyAI.AttackStartup.StateTransitionReentrantReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackStartupStateTransitionReplacement::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("State-transition replacement fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Initial attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	bool bReplacementRequested = false;
	bool bReplacementStarted = false;
	FAttackInstanceId ReplacementAttack;
	const TWeakObjectPtr<UEnemyCombatAIComponent> WeakCombatAI = Fixture.CombatAI;
	Fixture.CombatAI->SetPostAttackStateTransitionHookForTesting(
		[WeakCombatAI, &bReplacementRequested, &bReplacementStarted, &ReplacementAttack]()
		{
			if (UEnemyCombatAIComponent* ReentrantCombatAI = WeakCombatAI.Get())
			{
				ReentrantCombatAI->AbortAttack();
				bReplacementRequested = ReentrantCombatAI->TryInitiateAttack();
				bReplacementStarted = bReplacementRequested
					&& ReentrantCombatAI->ExecuteAttackWithIdentity(ReplacementAttack);
			}
		});

	FAttackInstanceId OuterAttack;
	TestFalse(TEXT("Superseded state-transition startup does not execute again"),
		Fixture.CombatAI->ExecuteAttackWithIdentity(OuterAttack));
	TestFalse(TEXT("Superseded state-transition startup publishes no identity"),
		OuterAttack.IsValid());
	TestTrue(TEXT("State-transition callback starts a replacement"),
		bReplacementRequested && bReplacementStarted && ReplacementAttack.IsValid());
	TestTrue(TEXT("AI ownership remains on the replacement"),
		Fixture.CombatAI->GetActiveAttackInstance() == ReplacementAttack);
	TestTrue(TEXT("Combat ownership remains on the same replacement"),
		Fixture.Enemy->CombatComponent->BuildAttackExecutionSnapshot().AttackInstance
			== ReplacementAttack);
	TestTrue(TEXT("Replacement retains the attack token"), Fixture.CombatAI->HasAttackToken());

	Fixture.CombatAI->AbortAttack();
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackStartupProvisionalOwnerDestroyed,
	"KatanaCombat.EnemyAI.AttackStartup.ProvisionalOwnerDestroyed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackStartupProvisionalOwnerDestroyed::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Provisional owner-destruction fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	const TWeakObjectPtr<AEnemyCharacter> WeakOwner = Fixture.Enemy;
	Fixture.CombatAI->SetPostExecuteAttackDataHookForTesting([WeakOwner]()
	{
		if (AEnemyCharacter* Owner = WeakOwner.Get())
		{
			Owner->Destroy();
		}
	});

	TestFalse(TEXT("Provisional startup cannot survive owner EndPlay"),
		Fixture.CombatAI->ExecuteAttack());
	TestFalse(TEXT("The provisional callback destroys the owner"), WeakOwner.IsValid());
	TestEqual(TEXT("Provisional EndPlay releases token ownership"),
		Fixture.TokenSubsystem->GetActiveAttackerCount(), 0);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackStartupTargetDestroyed,
	"KatanaCombat.EnemyAI.AttackStartup.TargetDestroyed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackStartupTargetDestroyed::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Startup target-destruction fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	const TWeakObjectPtr<AActor> WeakTarget(Fixture.Player);
	Fixture.CombatAI->SetPostExecuteAttackDataHookForTesting([WeakTarget]()
	{
		if (AActor* Target = WeakTarget.Get())
		{
			Target->Destroy();
		}
	});

	TestFalse(TEXT("A startup whose target is destroyed does not commit AI ownership"),
		Fixture.CombatAI->ExecuteAttack());
	TestFalse(TEXT("The execution callback destroys the target"), WeakTarget.IsValid());
	TestFalse(TEXT("Rejected startup releases token ownership"), Fixture.CombatAI->HasAttackToken());
	TestEqual(TEXT("Rejected startup releases its token exactly once"),
		Fixture.CombatAI->GetTokenReleaseCountForTesting(), 1);
	TestEqual(TEXT("Rejected startup does not broadcast a committed attack end"),
		Fixture.CombatAI->GetAttackEndBroadcastCountForTesting(), 0);
	TestEqual(TEXT("Target loss returns the enemy to Idle"),
		Fixture.CombatAI->CurrentState, EEnemyAIState::Idle);
	TestEqual(TEXT("Rejected startup retires the CombatComponent phase"),
		Fixture.Enemy->CombatComponent->GetCurrentPhase(), EAttackPhase::None);
	TestFalse(TEXT("Rejected startup publishes no started attack identity"),
		Fixture.CombatAI->GetLastStartedAttackInstance().IsValid());

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackStartupReentrantReplacement,
	"KatanaCombat.EnemyAI.AttackStartup.ReentrantReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackStartupReentrantReplacement::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Reentrant replacement fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Initial attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	bool bReplacementRequested = false;
	bool bReplacementStarted = false;
	FAttackInstanceId ReplacementAttack;
	const TWeakObjectPtr<UEnemyCombatAIComponent> WeakCombatAI = Fixture.CombatAI;
	Fixture.CombatAI->SetPostExecuteAttackDataHookForTesting(
		[WeakCombatAI, &bReplacementRequested, &bReplacementStarted, &ReplacementAttack]()
		{
			if (UEnemyCombatAIComponent* ReentrantCombatAI = WeakCombatAI.Get())
			{
				ReentrantCombatAI->AbortAttack();
				bReplacementRequested = ReentrantCombatAI->TryInitiateAttack();
				bReplacementStarted = bReplacementRequested && ReentrantCombatAI->ExecuteAttack();
				ReplacementAttack = ReentrantCombatAI->GetActiveAttackInstance();
			}
		});

	TestFalse(TEXT("The superseded outer startup does not claim replacement ownership"),
		Fixture.CombatAI->ExecuteAttack());
	TestTrue(TEXT("The reentrant replacement receives a token"), bReplacementRequested);
	TestTrue(TEXT("The reentrant replacement starts"), bReplacementStarted);
	TestTrue(TEXT("The replacement owns an exact active identity"), ReplacementAttack.IsValid());
	TestTrue(TEXT("Outer cleanup cannot end the replacement attack"), Fixture.CombatAI->IsAttacking());
	TestTrue(TEXT("Outer cleanup cannot release the replacement token"),
		Fixture.CombatAI->HasAttackToken());
	TestTrue(TEXT("The exact replacement remains active"),
		Fixture.CombatAI->GetActiveAttackInstance() == ReplacementAttack);
	TestEqual(TEXT("Only provisional ownership was released"),
		Fixture.CombatAI->GetTokenReleaseCountForTesting(), 1);
	TestEqual(TEXT("The provisional outer startup publishes no attack end"),
		Fixture.CombatAI->GetAttackEndBroadcastCountForTesting(), 0);

	Fixture.CombatAI->AbortAttack();
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AbortTokenReleaseReentrantReplacement,
	"KatanaCombat.EnemyAI.AttackLifecycle.AbortTokenReleaseReentrantReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AbortTokenReleaseReentrantReplacement::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Abort reentry fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Initial attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	bool bReplacementRequested = false;
	const TWeakObjectPtr<UEnemyCombatAIComponent> WeakCombatAI = Fixture.CombatAI;
	const TWeakObjectPtr<AActor> WeakTarget = Fixture.Player;
	Fixture.TokenSubsystem->SetPostTokenReleasedHookForTesting(
		[WeakCombatAI, WeakTarget, &bReplacementRequested](AActor*)
		{
			if (UEnemyCombatAIComponent* ReentrantCombatAI = WeakCombatAI.Get())
			{
				ReentrantCombatAI->SetCombatTarget(nullptr);
				ReentrantCombatAI->SetCombatTarget(WeakTarget.Get());
				bReplacementRequested = ReentrantCombatAI->TryInitiateAttack();
			}
		});

	Fixture.CombatAI->AbortAttack();
	TestTrue(TEXT("Token-release reentry requests replacement ownership"), bReplacementRequested);
	TestEqual(TEXT("Outer abort cannot overwrite replacement approach state"),
		Fixture.CombatAI->CurrentState, EEnemyAIState::Approaching);
	TestTrue(TEXT("Replacement retains its newly acquired token"), Fixture.CombatAI->HasAttackToken());
	TestEqual(TEXT("Replacement retains its selected attack"),
		Fixture.CombatAI->SelectedAttack.Get(), Fixture.AttackData);

	Fixture.CombatAI->AbortAttack();
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_PendingTerminationTokenReleaseReentrantReplacement,
	"KatanaCombat.EnemyAI.AttackLifecycle.PendingTerminationTokenReleaseReentrantReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_PendingTerminationTokenReleaseReentrantReplacement::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Pending termination reentry fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Initial attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	bool bReplacementRequested = false;
	const TWeakObjectPtr<UEnemyCombatAIComponent> WeakCombatAI = Fixture.CombatAI;
	const TWeakObjectPtr<AActor> WeakTarget = Fixture.Player;
	Fixture.TokenSubsystem->SetPostTokenReleasedHookForTesting(
		[WeakCombatAI, WeakTarget, &bReplacementRequested](AActor*)
		{
			if (UEnemyCombatAIComponent* ReentrantCombatAI = WeakCombatAI.Get())
			{
				ReentrantCombatAI->SetCombatTarget(nullptr);
				ReentrantCombatAI->SetCombatTarget(WeakTarget.Get());
				bReplacementRequested = ReentrantCombatAI->TryInitiateAttack();
			}
		});

	Fixture.CombatAI->OnDamaged();
	TestTrue(TEXT("Pending termination callback requests replacement ownership"), bReplacementRequested);
	TestEqual(TEXT("Pending termination cannot overwrite replacement approach state"),
		Fixture.CombatAI->CurrentState, EEnemyAIState::Approaching);
	TestTrue(TEXT("Replacement retains its newly acquired token"), Fixture.CombatAI->HasAttackToken());
	TestEqual(TEXT("Replacement retains its selected attack"),
		Fixture.CombatAI->SelectedAttack.Get(), Fixture.AttackData);

	Fixture.CombatAI->AbortAttack();
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_ActiveTerminationCombatAbortReentrantReplacement,
	"KatanaCombat.EnemyAI.AttackLifecycle.ActiveTerminationCombatAbortReentrantReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_ActiveTerminationCombatAbortReentrantReplacement::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	UTargetingComponent* Targeting = Fixture.Enemy ? Fixture.Enemy->GetTargetingComponent() : nullptr;
	if (!TestTrue(TEXT("Active termination reentry fixture should be valid"), Fixture.IsValid() && Targeting))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Initial attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	FAttackInstanceId InitialAttack;
	TestTrue(TEXT("Initial attack starts"), Fixture.CombatAI->ExecuteAttackWithIdentity(InitialAttack));
	TestTrue(TEXT("Initial attack owns alignment"), Targeting->GetActiveAlignmentRequest().IsValid());

	bool bReplacementRequested = false;
	bool bReplacementStarted = false;
	FAttackInstanceId ReplacementAttack;
	const TWeakObjectPtr<UEnemyCombatAIComponent> WeakCombatAI = Fixture.CombatAI;
	Fixture.CombatAI->SetPostCombatAbortHookForTesting(
		[WeakCombatAI, &bReplacementRequested, &bReplacementStarted, &ReplacementAttack]()
		{
			if (UEnemyCombatAIComponent* ReentrantCombatAI = WeakCombatAI.Get())
			{
				ReentrantCombatAI->AbortAttack();
				bReplacementRequested = ReentrantCombatAI->TryInitiateAttack();
				bReplacementStarted = bReplacementRequested
					&& ReentrantCombatAI->ExecuteAttackWithIdentity(ReplacementAttack);
			}
		});

	Fixture.CombatAI->OnDamaged();

	TestTrue(TEXT("Combat-abort callback starts a replacement attack"),
		bReplacementRequested && bReplacementStarted && ReplacementAttack.IsValid());
	TestTrue(TEXT("Replacement generation is newer than the terminated attack"),
		ReplacementAttack.Attacker == InitialAttack.Attacker
			&& ReplacementAttack.AttackGeneration > InitialAttack.AttackGeneration);
	TestTrue(TEXT("Outer termination must preserve replacement AI ownership"),
		Fixture.CombatAI->GetActiveAttackInstance() == ReplacementAttack
			&& Fixture.CombatAI->IsAttacking());
	TestTrue(TEXT("Outer termination must preserve replacement token ownership"),
		Fixture.CombatAI->HasAttackToken());
	TestTrue(TEXT("Outer termination must preserve replacement alignment ownership"),
		Targeting->GetActiveAlignmentRequest().IsValid());

	Fixture.CombatAI->AbortAttack();
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackStartupCombatComponentDestroyed,
	"KatanaCombat.EnemyAI.AttackStartup.CombatComponentDestroyedAfterStateTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackStartupCombatComponentDestroyed::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("CombatComponent destruction fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	UCombatComponent* const DestroyedCombat = Fixture.Enemy->CombatComponent;
	const TWeakObjectPtr<UCombatComponent> WeakCombat = DestroyedCombat;
	Fixture.CombatAI->SetPostAttackStateTransitionHookForTesting([WeakCombat]()
	{
		if (UCombatComponent* Combat = WeakCombat.Get())
		{
			Combat->DestroyComponent();
		}
	});

	FAttackInstanceId StartedAttack;
	TestFalse(TEXT("Startup rejects a destroyed snapshotted CombatComponent"),
		Fixture.CombatAI->ExecuteAttackWithIdentity(StartedAttack));
	TestFalse(TEXT("Destroyed CombatComponent publishes no attack identity"), StartedAttack.IsValid());
	TestFalse(TEXT("CombatComponent is invalid after the state-transition callback"), WeakCombat.IsValid());
	TestEqual(TEXT("Startup never invokes the invalidated CombatComponent"),
		DestroyedCombat->GetCurrentAttackGeneration(), 0);
	TestFalse(TEXT("Rejected startup releases token ownership"), Fixture.CombatAI->HasAttackToken());
	TestEqual(TEXT("Rejected startup returns to the targeted ready state"),
		Fixture.CombatAI->CurrentState, EEnemyAIState::Circling);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackStartupDirectGenerationReplacement,
	"KatanaCombat.EnemyAI.AttackStartup.DirectCombatGenerationReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackStartupDirectGenerationReplacement::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Direct generation replacement fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	bool bReplacementStarted = false;
	FAttackInstanceId ReplacementAttack;
	const TWeakObjectPtr<UCombatComponent> WeakCombat = Fixture.Enemy->CombatComponent;
	const TWeakObjectPtr<UAttackData> WeakAttackData = Fixture.AttackData;
	const TWeakObjectPtr<AActor> WeakTarget = Fixture.Player;
	Fixture.CombatAI->SetPostExecuteAttackDataHookForTesting(
		[WeakCombat, WeakAttackData, WeakTarget, &bReplacementStarted, &ReplacementAttack]()
		{
			UCombatComponent* Combat = WeakCombat.Get();
			UAttackData* AttackData = WeakAttackData.Get();
			AActor* Target = WeakTarget.Get();
			if (!Combat || !AttackData || !Target)
			{
				return;
			}

			const FAttackInstanceId OuterAttack = Combat->BuildAttackExecutionSnapshot().AttackInstance;
			Combat->AbortActiveAttack(OuterAttack);
			bReplacementStarted = Combat->ExecuteAttackData(
				AttackData,
				Target,
				AttackData->AttackType == EAttackType::Heavy
					? EInputType::HeavyAttack
					: EInputType::LightAttack);
			ReplacementAttack = Combat->BuildAttackExecutionSnapshot().AttackInstance;
		});

	FAttackInstanceId OuterAttack;
	TestFalse(TEXT("Outer startup cannot claim a direct replacement generation"),
		Fixture.CombatAI->ExecuteAttackWithIdentity(OuterAttack));
	TestFalse(TEXT("Outer startup publishes no replacement identity"), OuterAttack.IsValid());
	TestTrue(TEXT("The direct callback starts a newer CombatComponent generation"),
		bReplacementStarted && ReplacementAttack.IsValid());
	TestEqual(TEXT("Unowned direct replacement is retired instead of orphaned"),
		Fixture.Enemy->CombatComponent->GetCurrentPhase(), EAttackPhase::None);
	TestFalse(TEXT("Rollback releases AI token ownership"), Fixture.CombatAI->HasAttackToken());
	TestFalse(TEXT("Rollback leaves no AI attack identity"),
		Fixture.CombatAI->GetActiveAttackInstance().IsValid());

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackStartedDirectGenerationReplacement,
	"KatanaCombat.EnemyAI.AttackLifecycle.AttackStartedDirectCombatGenerationReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackStartedDirectGenerationReplacement::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Committed direct generation replacement fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	bool bReplacementStarted = false;
	FAttackInstanceId ReplacementAttack;
	const TWeakObjectPtr<UCombatComponent> WeakCombat = Fixture.Enemy->CombatComponent;
	const TWeakObjectPtr<UAttackData> WeakAttackData = Fixture.AttackData;
	const TWeakObjectPtr<AActor> WeakTarget = Fixture.Player;
	Fixture.CombatAI->SetPostAttackStartedHookForTesting(
		[WeakCombat, WeakAttackData, WeakTarget, &bReplacementStarted, &ReplacementAttack]()
		{
			UCombatComponent* Combat = WeakCombat.Get();
			UAttackData* AttackData = WeakAttackData.Get();
			AActor* Target = WeakTarget.Get();
			if (!Combat || !AttackData || !Target)
			{
				return;
			}

			const FAttackInstanceId CommittedAttack = Combat->BuildAttackExecutionSnapshot().AttackInstance;
			Combat->AbortActiveAttack(CommittedAttack);
			bReplacementStarted = Combat->ExecuteAttackData(
				AttackData,
				Target,
				AttackData->AttackType == EAttackType::Heavy
					? EInputType::HeavyAttack
					: EInputType::LightAttack);
			ReplacementAttack = Combat->BuildAttackExecutionSnapshot().AttackInstance;
		});

	FAttackInstanceId StartedAttack;
	TestTrue(TEXT("The outer invocation reports its committed generation"),
		Fixture.CombatAI->ExecuteAttackWithIdentity(StartedAttack));
	TestTrue(TEXT("Outer invocation committed a distinct identity"),
		StartedAttack.IsValid() && !(StartedAttack == ReplacementAttack));
	TestTrue(TEXT("The post-start callback creates a direct replacement"),
		bReplacementStarted && ReplacementAttack.IsValid());
	TestEqual(TEXT("Post-start validation retires the unowned replacement"),
		Fixture.Enemy->CombatComponent->GetCurrentPhase(), EAttackPhase::None);
	TestFalse(TEXT("Post-start rollback releases token ownership"), Fixture.CombatAI->HasAttackToken());
	TestEqual(TEXT("Post-start rollback enters recovery"),
		Fixture.CombatAI->CurrentState, EEnemyAIState::Recovering);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_StaleMontageCompletion,
	"KatanaCombat.EnemyAI.AttackLifecycle.StaleMontageCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_StaleMontageCompletion::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Stale montage fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("First attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	TestTrue(TEXT("First attack starts"), Fixture.CombatAI->ExecuteAttack());
	const FAttackInstanceId FirstAttack =
		Fixture.Enemy->CombatComponent->BuildAttackExecutionSnapshot().AttackInstance;
	TestTrue(TEXT("First attack has an exact generation"), FirstAttack.IsValid());
	Fixture.CombatAI->AbortAttack();

	TestTrue(TEXT("Replacement attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	TestTrue(TEXT("Replacement attack starts"), Fixture.CombatAI->ExecuteAttack());
	const FAttackInstanceId ReplacementAttack =
		Fixture.Enemy->CombatComponent->BuildAttackExecutionSnapshot().AttackInstance;
	TestTrue(TEXT("Replacement attack has a newer exact generation"),
		ReplacementAttack.IsValid()
			&& ReplacementAttack.Attacker == FirstAttack.Attacker
			&& ReplacementAttack.AttackGeneration > FirstAttack.AttackGeneration);

	const int32 ReleasesBeforeStaleCallback = Fixture.CombatAI->GetTokenReleaseCountForTesting();
	const int32 EndsBeforeStaleCallback = Fixture.CombatAI->GetAttackEndBroadcastCountForTesting();
	TestTrue(TEXT("The aborted generation remains a failed exact task result"),
		Fixture.CombatAI->GetAttackExecutionStatus(FirstAttack)
			== EEnemyAttackExecutionStatus::Failed);
	TestTrue(TEXT("The replacement generation is independently running"),
		Fixture.CombatAI->GetAttackExecutionStatus(ReplacementAttack)
			== EEnemyAttackExecutionStatus::Running);
	Fixture.CombatAI->InvokeAttackMontageEndedForTesting(
		Fixture.AttackData->AttackMontage,
		false,
		FirstAttack);

	TestTrue(TEXT("A stale montage callback cannot end the replacement attack"),
		Fixture.CombatAI->IsAttacking());
	TestTrue(TEXT("The replacement attack keeps its token"), Fixture.CombatAI->HasAttackToken());
	TestEqual(TEXT("A stale callback cannot release replacement token ownership"),
		Fixture.CombatAI->GetTokenReleaseCountForTesting(), ReleasesBeforeStaleCallback);
	TestEqual(TEXT("A stale callback cannot broadcast a replacement attack end"),
		Fixture.CombatAI->GetAttackEndBroadcastCountForTesting(), EndsBeforeStaleCallback);
	TestEqual(TEXT("The CombatComponent still owns the replacement generation"),
		Fixture.Enemy->CombatComponent->BuildAttackExecutionSnapshot().AttackInstance.AttackGeneration,
		ReplacementAttack.AttackGeneration);
	TestTrue(TEXT("The stale callback cannot change replacement task status"),
		Fixture.CombatAI->GetAttackExecutionStatus(ReplacementAttack)
			== EEnemyAttackExecutionStatus::Running);

	Fixture.CombatAI->AbortAttack();
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_DuplicateMontageCompletion,
	"KatanaCombat.EnemyAI.AttackLifecycle.DuplicateMontageCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_DuplicateMontageCompletion::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Duplicate completion fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	TestTrue(TEXT("Attack starts"), Fixture.CombatAI->ExecuteAttack());
	const FAttackInstanceId Attack =
		Fixture.Enemy->CombatComponent->BuildAttackExecutionSnapshot().AttackInstance;
	Fixture.CombatAI->InvokeAttackMontageEndedForTesting(
		Fixture.AttackData->AttackMontage,
		false,
		Attack);
	Fixture.CombatAI->InvokeAttackMontageEndedForTesting(
		Fixture.AttackData->AttackMontage,
		false,
		Attack);

	TestFalse(TEXT("Duplicate completion releases the token"), Fixture.CombatAI->HasAttackToken());
	TestEqual(TEXT("Duplicate completion releases ownership once"),
		Fixture.CombatAI->GetTokenReleaseCountForTesting(), 1);
	TestEqual(TEXT("Duplicate completion broadcasts one attack end"),
		Fixture.CombatAI->GetAttackEndBroadcastCountForTesting(), 1);
	TestEqual(TEXT("Completed attack enters recovery"),
		Fixture.CombatAI->CurrentState, EEnemyAIState::Recovering);
	TestTrue(TEXT("StateTree observes natural completion as success"),
		Fixture.CombatAI->GetAttackExecutionStatus(Attack)
			== EEnemyAttackExecutionStatus::Succeeded);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_CompletedResultSurvivesNewerCompletion,
	"KatanaCombat.EnemyAI.AttackLifecycle.CompletedResultSurvivesNewerCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_CompletedResultSurvivesNewerCompletion::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Reentrant completion fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("First attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	FAttackInstanceId FirstAttack;
	TestTrue(TEXT("First attack starts with an exact identity"),
		Fixture.CombatAI->ExecuteAttackWithIdentity(FirstAttack));

	bool bSecondAttackRequested = false;
	bool bSecondAttackStarted = false;
	FAttackInstanceId SecondAttack;
	const TWeakObjectPtr<UEnemyCombatAIComponent> WeakCombatAI = Fixture.CombatAI;
	const TWeakObjectPtr<UAnimMontage> WeakAttackMontage = Fixture.AttackData->AttackMontage;
	Fixture.CombatAI->SetPostAttackEndedHookForTesting(
		[WeakCombatAI, WeakAttackMontage, &bSecondAttackRequested, &bSecondAttackStarted, &SecondAttack]()
		{
			UEnemyCombatAIComponent* ReentrantCombatAI = WeakCombatAI.Get();
			UAnimMontage* AttackMontage = WeakAttackMontage.Get();
			if (!ReentrantCombatAI || !AttackMontage)
			{
				return;
			}

			ReentrantCombatAI->AbortAttack();
			bSecondAttackRequested = ReentrantCombatAI->TryInitiateAttack();
			bSecondAttackStarted = bSecondAttackRequested
				&& ReentrantCombatAI->ExecuteAttackWithIdentity(SecondAttack);
			if (bSecondAttackStarted)
			{
				ReentrantCombatAI->InvokeAttackMontageEndedForTesting(
					AttackMontage,
					false,
					SecondAttack);
			}
		});

	Fixture.CombatAI->InvokeAttackMontageEndedForTesting(
		Fixture.AttackData->AttackMontage,
		false,
		FirstAttack);

	TestTrue(TEXT("Attack-end reentry completes a newer attack"),
		bSecondAttackRequested && bSecondAttackStarted && SecondAttack.IsValid());
	TestTrue(TEXT("The second completion has a newer exact identity"),
		SecondAttack.Attacker == FirstAttack.Attacker
			&& SecondAttack.AttackGeneration > FirstAttack.AttackGeneration);
	TestTrue(TEXT("The first completed identity remains a successful task result"),
		Fixture.CombatAI->GetAttackExecutionStatus(FirstAttack)
			== EEnemyAttackExecutionStatus::Succeeded);
	TestTrue(TEXT("The second completed identity is also a successful task result"),
		Fixture.CombatAI->GetAttackExecutionStatus(SecondAttack)
			== EEnemyAttackExecutionStatus::Succeeded);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_ProductionMontageInterruption,
	"KatanaCombat.EnemyAI.AttackLifecycle.ProductionMontageInterruption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_ProductionMontageInterruption::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Production montage fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	FAttackInstanceId Attack;
	TestTrue(TEXT("Attack starts with an exact identity"),
		Fixture.CombatAI->ExecuteAttackWithIdentity(Attack));
	UAnimInstance* AnimInstance = Fixture.Enemy->GetMesh()
		? Fixture.Enemy->GetMesh()->GetAnimInstance()
		: nullptr;
	if (!TestNotNull(TEXT("Enemy should have an animation instance"), AnimInstance))
	{
		Fixture.Destroy();
		return false;
	}
	FOnMontageEnded* InstalledDelegate = AnimInstance->Montage_GetEndedDelegate(
		Fixture.AttackData->AttackMontage);
	if (!TestNotNull(TEXT("Production montage should expose its installed end delegate"),
		InstalledDelegate))
	{
		Fixture.Destroy();
		return false;
	}
	TestTrue(TEXT("Production montage end delegate should be bound"), InstalledDelegate->IsBound());
	FOnMontageEnded BoundDelegate = *InstalledDelegate;
	BoundDelegate.Execute(Fixture.AttackData->AttackMontage, true);

	TestFalse(TEXT("Production interruption releases the token"), Fixture.CombatAI->HasAttackToken());
	TestEqual(TEXT("Production interruption broadcasts one attack end"),
		Fixture.CombatAI->GetAttackEndBroadcastCountForTesting(), 1);
	TestTrue(TEXT("Interrupted production callback reports failure"),
		Fixture.CombatAI->GetAttackExecutionStatus(Attack)
			== EEnemyAttackExecutionStatus::Failed);

	FOnMontageEnded* RetiredDelegate = AnimInstance->Montage_GetEndedDelegate(
		Fixture.AttackData->AttackMontage);
	TestTrue(TEXT("Termination removes the production montage end delegate"),
		!RetiredDelegate || !RetiredDelegate->IsBound());

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackStartedTargetDestroyed,
	"KatanaCombat.EnemyAI.AttackLifecycle.AttackStartedTargetDestroyed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackStartedTargetDestroyed::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Attack-started destruction fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	const TWeakObjectPtr<AActor> WeakTarget(Fixture.Player);
	Fixture.CombatAI->SetPostAttackStartedHookForTesting([WeakTarget]()
	{
		if (AActor* Target = WeakTarget.Get())
		{
			Target->Destroy();
		}
	});

	TestTrue(TEXT("The attack commits before the started callback runs"),
		Fixture.CombatAI->ExecuteAttack());
	const FAttackInstanceId StartedAttack = Fixture.CombatAI->GetLastStartedAttackInstance();
	TestTrue(TEXT("The started callback ran after an exact generation committed"),
		StartedAttack.IsValid());
	TestFalse(TEXT("The started callback destroys the target"), WeakTarget.IsValid());
	TestFalse(TEXT("Target destruction retires attack token ownership"),
		Fixture.CombatAI->HasAttackToken());
	TestEqual(TEXT("Target destruction ends committed ownership once"),
		Fixture.CombatAI->GetAttackEndBroadcastCountForTesting(), 1);
	TestEqual(TEXT("Target destruction leaves the enemy idle"),
		Fixture.CombatAI->CurrentState, EEnemyAIState::Idle);
	TestEqual(TEXT("Target destruction retires the CombatComponent phase"),
		Fixture.Enemy->CombatComponent->GetCurrentPhase(), EAttackPhase::None);
	TestFalse(TEXT("Target destruction clears active attack ownership"),
		Fixture.CombatAI->GetActiveAttackInstance().IsValid());
	TestTrue(TEXT("StateTree observes participant destruction as failure"),
		Fixture.CombatAI->GetAttackExecutionStatus(StartedAttack)
			== EEnemyAttackExecutionStatus::Failed);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackStartedReentrantReplacement,
	"KatanaCombat.EnemyAI.AttackLifecycle.AttackStartedReentrantReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackStartedReentrantReplacement::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Attack-started replacement fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Initial attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	bool bReplacementRequested = false;
	bool bReplacementStarted = false;
	FAttackInstanceId ReplacementAttack;
	const TWeakObjectPtr<UEnemyCombatAIComponent> WeakCombatAI = Fixture.CombatAI;
	Fixture.CombatAI->SetPostAttackStartedHookForTesting(
		[WeakCombatAI, &bReplacementRequested, &bReplacementStarted, &ReplacementAttack]()
		{
			if (UEnemyCombatAIComponent* ReentrantCombatAI = WeakCombatAI.Get())
			{
				ReentrantCombatAI->AbortAttack();
				bReplacementRequested = ReentrantCombatAI->TryInitiateAttack();
				bReplacementStarted = bReplacementRequested
					&& ReentrantCombatAI->ExecuteAttackWithIdentity(ReplacementAttack);
			}
		});

	FAttackInstanceId OuterAttack;
	TestTrue(TEXT("The outer attack commits before its started callback"),
		Fixture.CombatAI->ExecuteAttackWithIdentity(OuterAttack));
	TestTrue(TEXT("The outer invocation returns its own exact identity"), OuterAttack.IsValid());
	TestTrue(TEXT("The callback starts a replacement"),
		bReplacementRequested && bReplacementStarted && ReplacementAttack.IsValid());
	TestTrue(TEXT("The replacement generation is newer than the outer generation"),
		ReplacementAttack.Attacker == OuterAttack.Attacker
			&& ReplacementAttack.AttackGeneration > OuterAttack.AttackGeneration);
	TestTrue(TEXT("Shared last-started state belongs to the replacement"),
		Fixture.CombatAI->GetLastStartedAttackInstance() == ReplacementAttack);
	TestTrue(TEXT("The outer StateTree identity resolves as failed"),
		Fixture.CombatAI->GetAttackExecutionStatus(OuterAttack)
			== EEnemyAttackExecutionStatus::Failed);
	TestTrue(TEXT("The replacement StateTree identity remains running"),
		Fixture.CombatAI->GetAttackExecutionStatus(ReplacementAttack)
			== EEnemyAttackExecutionStatus::Running);
	TestTrue(TEXT("The replacement keeps token ownership"), Fixture.CombatAI->HasAttackToken());
	TestEqual(TEXT("Only the outer committed attack ended"),
		Fixture.CombatAI->GetAttackEndBroadcastCountForTesting(), 1);

	Fixture.CombatAI->AbortAttack();
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatAI_AttackStartedOwnerDestroyed,
	"KatanaCombat.EnemyAI.AttackLifecycle.AttackStartedOwnerDestroyed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatAI_AttackStartedOwnerDestroyed::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FEnemyAttackLifecycleFixture Fixture = CreateEnemyAttackLifecycleFixture();
	if (!TestTrue(TEXT("Attack-started owner-destruction fixture should be valid"), Fixture.IsValid()))
	{
		Fixture.Destroy();
		return false;
	}

	TestTrue(TEXT("Attack request receives a token"), Fixture.CombatAI->TryInitiateAttack());
	const TWeakObjectPtr<AActor> WeakOwner(Fixture.Enemy);
	Fixture.CombatAI->SetPostAttackStartedHookForTesting([WeakOwner]()
	{
		if (AActor* Owner = WeakOwner.Get())
		{
			Owner->Destroy();
		}
	});

	TestTrue(TEXT("The attack commits before owner-destruction callback runs"),
		Fixture.CombatAI->ExecuteAttack());
	TestFalse(TEXT("The started callback destroys the owner"), WeakOwner.IsValid());
	TestEqual(TEXT("EndPlay releases destroyed-owner token ownership"),
		Fixture.TokenSubsystem->GetActiveAttackerCount(), 0);

	Fixture.Destroy();
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
