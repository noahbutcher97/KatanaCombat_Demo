// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/AttackConfiguration.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Data/DefenseConfiguration.h"
#include "Data/TargetingSettings.h"
#include "Defense/DefenseResolver.h"
#include "Utilities/CombatGameplayTags.h"

namespace
{
UCombatSettings* ConfigureDefenseInput(
	APlayerCharacter* Player,
	UCombatComponent* Combat,
	bool bAddAttacks = true)
{
	UCombatSettings* Settings = FCombatTestHelpers::CreateTestCombatSettings();
	if (bAddAttacks)
	{
		if (UAttackConfiguration* Attacks = Settings ? Settings->GetAttackConfiguration() : nullptr)
		{
			Attacks->DefaultLightAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
			Attacks->DefaultHeavyAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
		}
	}

	if (Player)
	{
		Player->CombatSettings = Settings;
	}
	if (Combat)
	{
		Combat->CombatSettings = Settings;
	}
	return Settings;
}

void EnableCounterOverlap(ACharacter* Character)
{
	if (UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr)
	{
		Capsule->SetGenerateOverlapEvents(true);
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Capsule->SetCollisionObjectType(ECC_Pawn);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Capsule->UpdateOverlaps();
	}
}

const FCombatInputRecord* LastRecord(const UCombatComponent* Combat)
{
	const TArray<FCombatInputRecord>& History = Combat->GetCombatInputHistory();
	return History.IsEmpty() ? nullptr : &History.Last();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseInput_UnconditionalCapture,
	"KatanaCombat.Defense.Input.UnconditionalCaptureBeforeSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseInput_UnconditionalCapture::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	if (!Player || !Combat)
	{
		AddError(TEXT("Failed to create unconditional-capture fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Player->CombatSettings = nullptr;
	Combat->CombatSettings = nullptr;
	Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press, EInputDirection::Forward);

	const FCombatInputRecord* Record = LastRecord(Combat);
	TestNotNull(TEXT("Input must be captured before settings lookup"), Record);
	if (Record)
	{
		TestEqual(TEXT("Captured input type"), Record->InputType, EInputType::LightAttack);
		TestEqual(TEXT("Captured direction"), Record->Direction, EInputDirection::Forward);
		TestEqual(TEXT("Missing settings rejects the captured edge"), Record->Disposition, ECombatInputDisposition::Rejected);
	}

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseInput_BlockEdgesTerminal,
	"KatanaCombat.Defense.Input.BlockEdgesAreStatefulAndTerminal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseInput_BlockEdgesTerminal::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	ConfigureDefenseInput(Player, Combat);

	Combat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	Combat->OnInputEvent(EInputType::Block, EInputEventType::Release);

	const TArray<FCombatInputRecord>& History = Combat->GetCombatInputHistory();
	TestEqual(TEXT("Both Block edges are captured"), History.Num(), 2);
	if (History.Num() == 2)
	{
		TestEqual(TEXT("Block press route"), History[0].Route, ECombatInputRoute::StatefulControl);
		TestEqual(TEXT("Block press disposition"), History[0].Disposition, ECombatInputDisposition::Consumed);
		TestEqual(TEXT("Block release route"), History[1].Route, ECombatInputRoute::StatefulControl);
		TestEqual(TEXT("Block release disposition"), History[1].Disposition, ECombatInputDisposition::Consumed);
	}
	TestEqual(TEXT("Block edges never enter action queue"), Combat->GetPendingActionCount(), 0);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseInput_RejectedBlockPressTerminal,
	"KatanaCombat.Defense.Input.RejectedBlockPressIsTerminal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseInput_RejectedBlockPressTerminal::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	ConfigureDefenseInput(Player, Combat);
	Combat->SetPhase(EAttackPhase::Windup);

	Combat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	const FCombatInputRecord* Record = LastRecord(Combat);
	TestNotNull(TEXT("Rejected Block press is retained"), Record);
	if (Record)
	{
		TestEqual(TEXT("Rejected press remains stateful"), Record->Route, ECombatInputRoute::StatefulControl);
		TestEqual(TEXT("Rejected press is terminal"), Record->Disposition, ECombatInputDisposition::Rejected);
	}
	TestFalse(TEXT("Rejected press does not enter guard"), Combat->IsBlocking());
	TestEqual(TEXT("Rejected press does not queue"), Combat->GetPendingActionCount(), 0);

	Combat->SetPhase(EAttackPhase::None);
	TestFalse(TEXT("Later eligibility cannot replay rejected input"), Combat->IsBlocking());
	TestEqual(TEXT("No new edge was fabricated"), Combat->GetCombatInputHistory().Num(), 1);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseInput_HeldBlockDoesNotRetroactivelyParry,
	"KatanaCombat.Defense.Input.HeldBlockDoesNotRetroactivelyParry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseInput_HeldBlockDoesNotRetroactivelyParry::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	ConfigureDefenseInput(Player, Combat);
	EnableCounterOverlap(Player);
	EnableCounterOverlap(Enemy);

	Combat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	Enemy->PairedAnimationComponent->SetParryWindowActive(true);

	TestTrue(TEXT("Held input remains normal guard"), Combat->IsBlocking());
	TestEqual(TEXT("A later window does not start Chain"), Player->PairedAnimationComponent->GetChainState(), EChainCounterState::None);
	TestEqual(TEXT("No synthetic retry edge is created"), Combat->GetCombatInputHistory().Num(), 1);

	Combat->OnInputEvent(EInputType::Block, EInputEventType::Release);
	Enemy->PairedAnimationComponent->SetParryWindowActive(false);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseInput_NewBlockPressRetriesParry,
	"KatanaCombat.Defense.Input.NewBlockPressRetriesParry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseInput_NewBlockPressRetriesParry::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	ConfigureDefenseInput(Player, Combat);
	EnableCounterOverlap(Player);
	EnableCounterOverlap(Enemy);

	Combat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	Enemy->PairedAnimationComponent->SetParryWindowActive(true);
	Combat->OnInputEvent(EInputType::Block, EInputEventType::Press);

	TestEqual(TEXT("A second physical press is captured"), Combat->GetCombatInputHistory().Num(), 2);
	TestTrue(TEXT("Second press starts the current parry path"), Player->PairedAnimationComponent->GetChainState() != EChainCounterState::None);
	const FCombatInputRecord* Record = LastRecord(Combat);
	if (Record)
	{
		TestEqual(TEXT("Retry remains stateful"), Record->Route, ECombatInputRoute::StatefulControl);
		TestEqual(TEXT("Successful retry is consumed"), Record->Disposition, ECombatInputDisposition::Consumed);
	}

	Player->PairedAnimationComponent->CancelPairedAnimation();
	Enemy->PairedAnimationComponent->SetParryWindowActive(false);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseInput_ChainPreflightFailureExpires,
	"KatanaCombat.Defense.Input.ChainPreflightFailureExpires",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseInput_ChainPreflightFailureExpires::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	ConfigureDefenseInput(Player, Combat);
	EnableCounterOverlap(Player);
	EnableCounterOverlap(Enemy);
	Enemy->PairedAnimationComponent->SetParryWindowActive(true);
	Combat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	Player->PairedAnimationComponent->ChainState = EChainCounterState::CounterWindow;
	ConfigureDefenseInput(Player, Combat, false);

	const int32 QueueSizeBefore = Combat->GetPendingActionCount();
	AddExpectedErrorPlain(TEXT("Default Light attack is nullptr"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("Cannot resolve attack: No default AND no current attack"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("Cannot advance: selected attack data is null"), EAutomationExpectedErrorFlags::Contains, 1);
	Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);

	TestTrue(TEXT("Failed preflight leaves response window active"), Player->PairedAnimationComponent->IsChainCounterWaitingForAttack());
	TestEqual(TEXT("Failed Chain route cannot enter normal queue"), Combat->GetPendingActionCount(), QueueSizeBefore);
	const FCombatInputRecord* Record = LastRecord(Combat);
	if (Record)
	{
		TestEqual(TEXT("Response-window attack owns Chain route"), Record->Route, ECombatInputRoute::ChainOnly);
		TestEqual(TEXT("Failed one-shot route expires"), Record->Disposition, ECombatInputDisposition::Expired);
	}

	Player->PairedAnimationComponent->CancelPairedAnimation();
	Enemy->PairedAnimationComponent->SetParryWindowActive(false);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseInput_HistoryIsBoundedAndMonotonic,
	"KatanaCombat.Defense.Input.HistoryIsBoundedAndMonotonic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseInput_HistoryIsBoundedAndMonotonic::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	Player->CombatSettings = nullptr;
	Combat->CombatSettings = nullptr;

	for (int32 Index = 0; Index < 70; ++Index)
	{
		Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	}

	const TArray<FCombatInputRecord>& History = Combat->GetCombatInputHistory();
	TestEqual(TEXT("History retains only the newest records"), History.Num(), 64);
	if (History.Num() == 64)
	{
		TestEqual(TEXT("Oldest retained serial"), History[0].Serial, static_cast<uint64>(7));
		TestEqual(TEXT("Newest retained serial"), History.Last().Serial, static_cast<uint64>(70));
		for (int32 Index = 1; Index < History.Num(); ++Index)
		{
			TestEqual(TEXT("Retained serials stay monotonic"), History[Index].Serial, History[Index - 1].Serial + 1);
		}
	}

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseThreat_StableIdsAreUnique,
	"KatanaCombat.Defense.Threat.StableIdsAreUnique",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseThreat_StableIdsAreUnique::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* PlayerCombat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, PlayerCombat);
	AEnemyCharacter* Enemy = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	UCombatComponent* EnemyCombat = Enemy ? Enemy->FindComponentByClass<UCombatComponent>() : nullptr;
	if (!Player || !PlayerCombat || !Enemy || !EnemyCombat)
	{
		AddError(TEXT("Failed to create stable-ID fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FCombatantStableId PlayerId = PlayerCombat->GetCombatantStableId();
	const FCombatantStableId EnemyId = EnemyCombat->GetCombatantStableId();
	TestTrue(TEXT("Player stable ID is assigned at registration"), PlayerId.IsValid());
	TestTrue(TEXT("Enemy stable ID is assigned at registration"), EnemyId.IsValid());
	TestNotEqual(TEXT("Combatants receive unique stable IDs"), PlayerId.Value, EnemyId.Value);

	FCombatantStableId ExplicitId;
	ExplicitId.Value = 7001;
	PlayerCombat->SetCombatantStableIdForTesting(ExplicitId);
	TestEqual(TEXT("Tests can force deterministic tie-break IDs"), PlayerCombat->GetCombatantStableId().Value, ExplicitId.Value);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseThreat_AttackSnapshotPublication,
	"KatanaCombat.Defense.Threat.AttackSnapshotPublication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseThreat_AttackSnapshotPublication::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	AEnemyCharacter* Defender = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 50.0f, 0.0f));
	AEnemyCharacter* OtherDefender = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, -50.0f, 0.0f));
	if (!Player || !Combat || !Defender || !OtherDefender)
	{
		AddError(TEXT("Failed to create attack-publication fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	Attack->DefenseProfile.Height = EAttackHeight::High;
	Attack->DefenseProfile.NominalLane = EIncomingAttackLane::Right;
	Attack->DefenseProfile.SwingShape = ESwingDirection::Vertical;
	Attack->DefenseProfile.SourceContactSocketOverride = TEXT("weapon_tip");
	Attack->DefenseProfile.DefenderTargetBoneFallback = TEXT("spine_03");
	Attack->AttackTags.AddTag(KatanaCombatGameplayTags::AttackDefenseParryable());

	Combat->CurrentAttackData = Attack;
	Combat->CurrentPhase = EAttackPhase::Windup;
	Combat->AttackStateMachine.AttackGeneration = 17;
	Combat->AttackStateMachine.ActiveSectionName = TEXT("Attack_1");
	Combat->SetAttackIntentTarget(Defender);

	const double Now = World->GetTimeSeconds();
	FAttackThreatPrediction Prediction;
	Prediction.IntendedTarget = Defender;
	Prediction.PathOrigin = Player->GetActorLocation();
	Prediction.PathDirection = (Defender->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal();
	Prediction.PredictedContactPoint = Defender->GetActorLocation();
	Prediction.SourceSocket = TEXT("weapon_tip");
	Prediction.DefenderTargetBone = TEXT("spine_03");
	Prediction.PredictionSimulationTimestamp = Now;
	Prediction.PredictedContactSimulationTime = Now + 0.25;
	Prediction.Lane = EIncomingAttackLane::Right;
	Prediction.Height = EAttackHeight::High;
	Prediction.Confidence = EDefensePredictionConfidence::High;
	Prediction.bPathIntersectsThreatVolume = true;
	Combat->PublishAttackThreatPrediction(Prediction);

	const FAttackExecutionSnapshot Snapshot = Combat->BuildAttackExecutionSnapshot();
	TestEqual(TEXT("Snapshot generation"), Snapshot.AttackInstance.AttackGeneration, 17);
	TestEqual(TEXT("Snapshot attacker"), Snapshot.AttackInstance.Attacker.Get(), Cast<AActor>(Player));
	TestEqual(TEXT("Snapshot data"), Snapshot.AttackData.Get(), Attack);
	TestEqual(TEXT("Snapshot target"), Snapshot.IntendedTarget.Get(), Cast<AActor>(Defender));
	TestEqual(TEXT("Snapshot authored height"), Snapshot.AuthoredHeight, EAttackHeight::High);
	TestEqual(TEXT("Snapshot authored lane"), Snapshot.NominalLane, EIncomingAttackLane::Right);
	TestEqual(TEXT("Snapshot swing"), Snapshot.SwingShape, ESwingDirection::Vertical);
	TestEqual(TEXT("Snapshot source socket"), Snapshot.SourceSocket, FName(TEXT("weapon_tip")));
	TestEqual(TEXT("Snapshot target bone"), Snapshot.DefenderTargetBone, FName(TEXT("spine_03")));
	TestTrue(TEXT("Current attack is active"), Snapshot.bAttackActive);
	TestTrue(TEXT("Current identity is published as current"), Snapshot.bAttackIdentityCurrent);
	TestTrue(TEXT("Complete prediction is retained"), Snapshot.PredictedContact.bIsValid);
	TestEqual(TEXT("Complete prediction remains high confidence"), Snapshot.PredictedContact.Confidence, EDefensePredictionConfidence::High);
	TestTrue(TEXT("Complete prediction establishes credible intent"), Snapshot.bHasCredibleIntent);
	TestTrue(TEXT("Predicted contact time is relative to now"), Snapshot.TimeToPredictedContact > 0.0f);

	Combat->SetAttackIntentTarget(OtherDefender);
	const FAttackExecutionSnapshot Retargeted = Combat->BuildAttackExecutionSnapshot();
	TestEqual(TEXT("Retarget updates immutable snapshot"), Retargeted.IntendedTarget.Get(), Cast<AActor>(OtherDefender));
	TestFalse(TEXT("Retarget invalidates old prediction"), Retargeted.PredictedContact.bIsValid);

	Combat->SetAttackIntentTarget(Defender);
	Combat->PublishAttackThreatPrediction(Prediction);
	++Combat->AttackStateMachine.AttackGeneration;
	TestFalse(TEXT("Prediction cannot cross an attack generation"), Combat->BuildAttackExecutionSnapshot().PredictedContact.bIsValid);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseThreat_HighConfidenceRequiresCompleteEvidence,
	"KatanaCombat.Defense.Threat.HighConfidenceRequiresCompleteEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseThreat_HighConfidenceRequiresCompleteEvidence::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	AEnemyCharacter* Defender = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200.0f, 0.0f, 0.0f));
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	Combat->CurrentAttackData = Attack;
	Combat->CurrentPhase = EAttackPhase::Active;
	Combat->AttackStateMachine.AttackGeneration = 8;
	Combat->SetAttackIntentTarget(Defender);

	FAttackThreatPrediction Incomplete;
	Incomplete.IntendedTarget = Defender;
	Incomplete.PathOrigin = Player->GetActorLocation();
	Incomplete.PathDirection = FVector::ForwardVector;
	Incomplete.PredictionSimulationTimestamp = World->GetTimeSeconds();
	Incomplete.Confidence = EDefensePredictionConfidence::High;
	Incomplete.bPathIntersectsThreatVolume = false;
	Combat->PublishAttackThreatPrediction(Incomplete);

	const FAttackExecutionSnapshot Snapshot = Combat->BuildAttackExecutionSnapshot();
	TestTrue(TEXT("Incomplete prediction remains available for guard guidance"), Snapshot.PredictedContact.bIsValid);
	TestEqual(TEXT("Incomplete evidence is downgraded"), Snapshot.PredictedContact.Confidence, EDefensePredictionConfidence::Low);
	TestFalse(TEXT("Downgraded prediction cannot establish perfect-parry intent"), Snapshot.bHasCredibleIntent);

	Combat->InvalidateAttackThreatPrediction(EThreatInvalidationReason::PathChanged);
	TestFalse(TEXT("Explicit invalidation removes prediction"), Combat->BuildAttackExecutionSnapshot().PredictedContact.bIsValid);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseThreat_ComponentSelectionOwnership,
	"KatanaCombat.Defense.Threat.ComponentSelectionOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseThreat_ComponentSelectionOwnership::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* DefenderCombat = nullptr;
	APlayerCharacter* Defender = FCombatTestHelpers::CreateTestCharacterWithCombat(World, DefenderCombat);
	AEnemyCharacter* EnemyA = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(300.0f, 50.0f, 0.0f));
	AEnemyCharacter* EnemyB = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(300.0f, -50.0f, 0.0f));
	UCombatComponent* CombatA = EnemyA ? EnemyA->CombatComponent.Get() : nullptr;
	UCombatComponent* CombatB = EnemyB ? EnemyB->CombatComponent.Get() : nullptr;
	UTargetingComponent* Targeting = Defender ? Defender->TargetingComponent.Get() : nullptr;
	if (!Defender || !DefenderCombat || !EnemyA || !EnemyB || !CombatA || !CombatB || !Targeting)
	{
		AddError(TEXT("Failed to create component threat-selection fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UTargetingSettings* TargetingSettings = NewObject<UTargetingSettings>();
	TargetingSettings->MaxTargetDistance = 1500.0f;
	TargetingSettings->bRequireLineOfSight = false;
	Targeting->TargetingSettingsOverride = TargetingSettings;

	UDefenseConfiguration* DefenseConfig = NewObject<UDefenseConfiguration>();
	DefenseConfig->DefenseThreatRange = 800.0f;
	DefenseConfig->ThreatLockMinSeconds = 0.15f;
	DefenseConfig->ThreatSwitchLeadSeconds = 0.10f;
	DefenseConfig->GuardedThreatRefreshSeconds = 0.05f;
	DefenseConfig->MaximumHighConfidencePredictionAge = 0.10f;
	DefenderCombat->DefenseConfigurationOverride = DefenseConfig;

	FCombatantStableId IdA;
	IdA.Value = 20;
	FCombatantStableId IdB;
	IdB.Value = 5;
	CombatA->SetCombatantStableIdForTesting(IdA);
	CombatB->SetCombatantStableIdForTesting(IdB);

	UAttackData* AttackA = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	UAttackData* AttackB = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	const double StartTime = World->GetTimeSeconds();
	auto PublishThreat = [&](UCombatComponent* Combat, AEnemyCharacter* Enemy,
		double ContactTime, double PublishedTime, EDefensePredictionConfidence Confidence)
	{
		Combat->CurrentAttackData = Combat == CombatA ? AttackA : AttackB;
		Combat->CurrentPhase = EAttackPhase::Windup;
		Combat->AttackStateMachine.AttackGeneration = Combat == CombatA ? 101 : 202;
		Combat->SetAttackIntentTarget(Defender);

		FAttackThreatPrediction Prediction;
		Prediction.IntendedTarget = Defender;
		Prediction.PathOrigin = Enemy->GetActorLocation();
		Prediction.PathDirection = (Defender->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal();
		Prediction.PredictedContactPoint = Defender->GetActorLocation();
		Prediction.PredictionSimulationTimestamp = PublishedTime;
		Prediction.PredictedContactSimulationTime = ContactTime;
		Prediction.Confidence = Confidence;
		Prediction.bPathIntersectsThreatVolume = true;
		Combat->PublishAttackThreatPrediction(Prediction);
	};

	PublishThreat(CombatA, EnemyA, StartTime + 0.30, StartTime, EDefensePredictionConfidence::High);
	PublishThreat(CombatB, EnemyB, StartTime + 0.50, StartTime, EDefensePredictionConfidence::High);
	Targeting->ResetAllTargetsInRangeCallCountForTesting();
	FDefenseThreatSelectionResult Result = DefenderCombat->SelectDefenseThreat(StartTime);
	TestTrue(TEXT("Initial selection finds an active hostile threat"), Result.bFound);
	TestEqual(TEXT("Earlier contact wins initial selection"), Result.SelectedThreat.StableId.Value, IdA.Value);
	TestEqual(TEXT("One selection opportunity enumerates candidates once"),
		Targeting->GetAllTargetsInRangeCallCountForTesting(), 1);

	PublishThreat(CombatA, EnemyA, StartTime + 0.50, StartTime, EDefensePredictionConfidence::High);
	PublishThreat(CombatB, EnemyB, StartTime + 0.20, StartTime, EDefensePredictionConfidence::High);
	Result = DefenderCombat->SelectDefenseThreat(StartTime + 0.05);
	TestEqual(TEXT("Minimum lock age suppresses an otherwise valid switch"),
		Result.SelectedThreat.StableId.Value, IdA.Value);
	PublishThreat(CombatA, EnemyA, StartTime + 0.50, StartTime + 0.15,
		EDefensePredictionConfidence::High);
	PublishThreat(CombatB, EnemyB, StartTime + 0.20, StartTime + 0.15,
		EDefensePredictionConfidence::High);
	Result = DefenderCombat->SelectDefenseThreat(StartTime + 0.16);
	TestEqual(TEXT("Earlier threat replaces lock after minimum age"),
		Result.SelectedThreat.StableId.Value, IdB.Value);

	CombatB->CurrentPhase = EAttackPhase::None;
	Result = DefenderCombat->SelectDefenseThreat(StartTime + 0.17);
	TestEqual(TEXT("Invalid current threat switches immediately inside lock minimum"),
		Result.SelectedThreat.StableId.Value, IdA.Value);

	DefenderCombat->ClearGuardThreat(EThreatClearReason::NoCandidates);
	PublishThreat(CombatA, EnemyA, StartTime + 0.60, StartTime + 0.16,
		EDefensePredictionConfidence::Low);
	PublishThreat(CombatB, EnemyB, StartTime + 0.25, StartTime - 1.0,
		EDefensePredictionConfidence::High);
	Result = DefenderCombat->SelectDefenseThreat(StartTime + 0.20);
	TestEqual(TEXT("Stale high-confidence candidate remains available for guard guidance"),
		Result.SelectedThreat.StableId.Value, IdB.Value);
	TestEqual(TEXT("Stale prediction is downgraded in the selected immutable snapshot"),
		Result.SelectedThreat.PredictedContact.Confidence, EDefensePredictionConfidence::Low);
	TestFalse(TEXT("Stale prediction loses credible-intent priority"),
		Result.SelectedThreat.bHasCredibleIntent);

	const int32 EnumerationBeforeResolve = Targeting->GetAllTargetsInRangeCallCountForTesting();
	FDefenseQuery GuardQuery;
	GuardQuery.Stage = EDefenseQueryStage::InputIntent;
	GuardQuery.Attack = Result.SelectedThreat;
	GuardQuery.Defender = Defender;
	GuardQuery.bDefenderAlive = true;
	GuardQuery.bDefenderCanGuard = true;
	GuardQuery.bHasSelectedThreat = true;
	const FDefenseDecision GuardDecision = FDefenseResolver::Resolve(GuardQuery);
	TestEqual(TEXT("Failed perfect-parry eligibility downgrades to held guard"),
		GuardDecision.Outcome, EDefenseOutcome::GuardEntered);
	TestEqual(TEXT("Guard fallback preserves selected identity"),
		GuardDecision.LockedThreatId.Value, Result.SelectedThreat.StableId.Value);
	TestEqual(TEXT("Guard fallback does not enumerate a second time"),
		Targeting->GetAllTargetsInRangeCallCountForTesting(), EnumerationBeforeResolve);

	DefenderCombat->bIsBlocking = true;
	DefenderCombat->LastGuardThreatRefreshFrame = MAX_uint64;
	Targeting->ResetAllTargetsInRangeCallCountForTesting();
	DefenderCombat->RefreshGuardThreat(EThreatRefreshReason::PredictionPublished);
	DefenderCombat->RefreshGuardThreat(EThreatRefreshReason::WindowChanged);
	TestEqual(TEXT("Same-frame event refresh requests coalesce"),
		Targeting->GetAllTargetsInRangeCallCountForTesting(), 1);
	TestTrue(TEXT("Guarded refresh runs a simulation-time timer while candidates exist"),
		World->GetTimerManager().IsTimerActive(DefenderCombat->GuardThreatRefreshTimerHandle));

	DefenderCombat->EndBlock();
	TestFalse(TEXT("Block release cancels guarded refresh"),
		World->GetTimerManager().IsTimerActive(DefenderCombat->GuardThreatRefreshTimerHandle));
	TestFalse(TEXT("Block release clears the locked threat"),
		DefenderCombat->LockedDefenseThreatId.IsValid());

	EnemyB->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	DefenderCombat->ClearGuardThreat(EThreatClearReason::NoCandidates);
	Result = DefenderCombat->SelectDefenseThreat(StartTime + 0.21);
	TestNotEqual(TEXT("Defense range cap excludes a targeting-visible distant threat"),
		Result.SelectedThreat.StableId.Value, IdB.Value);

	EnemyA->SetActorLocation(FVector(1100.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	DefenderCombat->bIsBlocking = true;
	DefenderCombat->LastGuardThreatRefreshFrame = MAX_uint64;
	Targeting->ResetAllTargetsInRangeCallCountForTesting();
	DefenderCombat->RefreshGuardThreat(EThreatRefreshReason::PredictionInvalidated);
	DefenderCombat->RefreshGuardThreat(EThreatRefreshReason::TargetChanged);
	TestEqual(TEXT("Same-frame no-candidate events still coalesce"),
		Targeting->GetAllTargetsInRangeCallCountForTesting(), 1);
	TestFalse(TEXT("No-candidate refresh does not leave a timer running"),
		World->GetTimerManager().IsTimerActive(DefenderCombat->GuardThreatRefreshTimerHandle));

	EnemyA->SetActorLocation(FVector(300.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	PublishThreat(CombatA, EnemyA, StartTime + 0.60, StartTime + 0.21,
		EDefensePredictionConfidence::High);
	TestEqual(TEXT("Late same-frame publication remains coalesced synchronously"),
		Targeting->GetAllTargetsInRangeCallCountForTesting(), 1);
	TestTrue(TEXT("Late same-frame publication schedules one next-tick refresh"),
		World->GetTimerManager().IsTimerActive(
			DefenderCombat->CoalescedGuardThreatRefreshTimerHandle));
	DefenderCombat->HandleCoalescedGuardThreatRefresh();
	TestEqual(TEXT("Coalesced late publication receives one next-tick selection"),
		Targeting->GetAllTargetsInRangeCallCountForTesting(), 2);
	TestEqual(TEXT("Next-tick refresh acquires the newly published threat"),
		DefenderCombat->LockedDefenseThreatId.Value, IdA.Value);
	DefenderCombat->EndBlock();

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseInput_NormalQueuePreservesBuffering,
	"KatanaCombat.Defense.Input.NormalQueuePreservesBuffering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseInput_NormalQueuePreservesBuffering::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	ConfigureDefenseInput(Player, Combat);
	Combat->SetPhase(EAttackPhase::Windup);

	Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	Combat->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Press);

	TestEqual(TEXT("Normal inputs retain existing queue behavior"), Combat->GetPendingActionCount(), 2);
	const TArray<FCombatInputRecord>& History = Combat->GetCombatInputHistory();
	TestEqual(TEXT("Both normal inputs are recorded"), History.Num(), 2);
	for (const FCombatInputRecord& Record : History)
	{
		TestEqual(TEXT("Normal input route"), Record.Route, ECombatInputRoute::NormalQueue);
		TestEqual(TEXT("Accepted normal input disposition"), Record.Disposition, ECombatInputDisposition::Queued);
	}

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
