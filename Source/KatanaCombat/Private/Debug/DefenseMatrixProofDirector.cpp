// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/DefenseMatrixProofDirector.h"

#include "AI/CombatTokenSubsystem.h"
#include "AI/EnemyAITypes.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Characters/BaseCombatCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Data/AttackData.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
const FName PlayerFixtureTag = TEXT("DefenseMatrix.Player");
const FName LeftAnchorTag = TEXT("DefenseMatrix.Anchor.Left");
const FName CenterAnchorTag = TEXT("DefenseMatrix.Anchor.Center");
const FName RightAnchorTag = TEXT("DefenseMatrix.Anchor.Right");
}

ADefenseMatrixProofDirector::ADefenseMatrixProofDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADefenseMatrixProofDirector::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UCombatTokenSubsystem* Tokens =
			GameInstance->GetSubsystem<UCombatTokenSubsystem>())
		{
			PreviousMaxConcurrentAttackers = Tokens->MaxConcurrentAttackers;
			bCapturedTokenPolicy = true;
			Tokens->MaxConcurrentAttackers = FMath::Max(1, ProofMaxConcurrentAttackers);
		}
	}

	CaptureFixtureState();

	if (bAutoStartHandsOffCase && !HandsOffCase.IsNone())
	{
		HandsOffTimerHandle = GetWorldTimerManager().SetTimerForNextTick(
			this, &ADefenseMatrixProofDirector::StartHandsOffCase);
	}
}

void ADefenseMatrixProofDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(HandsOffTimerHandle);
	if (bCapturedTokenPolicy)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UCombatTokenSubsystem* Tokens =
				GameInstance->GetSubsystem<UCombatTokenSubsystem>())
			{
				Tokens->ResetAllTokens();
				Tokens->MaxConcurrentAttackers = PreviousMaxConcurrentAttackers;
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

bool ADefenseMatrixProofDirector::StartNamedCase(const FName CaseName)
{
	const FDefenseMatrixProofCase* SelectedCase = FindCase(CaseName);
	if (!SelectedCase || !ValidateCaseForStart(*SelectedCase))
	{
		return false;
	}
	CaptureFixtureState();
	if (!bFixtureStateCaptured)
	{
		return false;
	}
	GetWorldTimerManager().ClearTimer(HandsOffTimerHandle);
	ResetFixture();
	if (!bLastResetComplete)
	{
		return false;
	}

	APlayerCharacter* Player = FindFixturePlayer();
	AEnemyCharacter* SelectedEnemy = FindFixtureEnemy(SelectedCase->AttackerAnchorTag);
	ClearEnemyAttackConfigs();

	if (!Player || !SelectedEnemy || !ConfigureEnemyForCase(SelectedEnemy, *SelectedCase))
	{
		ResetFixture();
		return false;
	}
	if (SelectedCase->bApplyDefenderTransform)
	{
		Player->SetActorTransform(
			SelectedCase->DefenderTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	if (SelectedCase->bApplyAttackerTransform)
	{
		SelectedEnemy->SetActorTransform(
			SelectedCase->AttackerTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	Player->SetHealth(Player->MaxHealth);

	UEnemyCombatAIComponent* SelectedAI = SelectedEnemy->GetCombatAIComponent();
	UCombatComponent* PlayerCombat = Player->GetCombatComponent();
	if (!SelectedAI || !PlayerCombat)
	{
		ResetFixture();
		return false;
	}

	SelectedAI->SetCombatTarget(Player);
	if (SelectedCase->bBeginHeldGuard)
	{
		if (!PlayerCombat->BeginBlock(SelectedEnemy))
		{
			ResetFixture();
			return false;
		}
	}
	else
	{
		PlayerCombat->EndBlock();
	}
	const bool bAttackRequested = SelectedAI->TryInitiateAttack();
	if (!bAttackRequested && !SelectedAI->IsWaitingForToken())
	{
		ResetFixture();
		return false;
	}
	ActiveCase = CaseName;
	return true;
}

bool ADefenseMatrixProofDirector::StartNamedThreatPair(
	const FName FirstCaseName,
	const FName SecondCaseName)
{
	const FDefenseMatrixProofCase* FirstCase = FindCase(FirstCaseName);
	const FDefenseMatrixProofCase* SecondCase = FindCase(SecondCaseName);
	if (!FirstCase || !SecondCase || FirstCase == SecondCase
		|| FirstCase->AttackerAnchorTag == SecondCase->AttackerAnchorTag
		|| !ValidateCaseForStart(*FirstCase)
		|| !ValidateCaseForStart(*SecondCase)
		|| (FirstCase->bApplyDefenderTransform
			&& SecondCase->bApplyDefenderTransform
			&& !FirstCase->DefenderTransform.Equals(SecondCase->DefenderTransform, 0.1f)))
	{
		return false;
	}

	CaptureFixtureState();
	if (!bFixtureStateCaptured)
	{
		return false;
	}
	GetWorldTimerManager().ClearTimer(HandsOffTimerHandle);
	ResetFixture();
	if (!bLastResetComplete)
	{
		return false;
	}

	APlayerCharacter* Player = FindFixturePlayer();
	AEnemyCharacter* FirstEnemy = FindFixtureEnemy(FirstCase->AttackerAnchorTag);
	AEnemyCharacter* SecondEnemy = FindFixtureEnemy(SecondCase->AttackerAnchorTag);
	ClearEnemyAttackConfigs();
	if (!Player || !FirstEnemy || !SecondEnemy
		|| FirstCase->Attack->BaseDamage + SecondCase->Attack->BaseDamage >= Player->MaxHealth
		|| !ConfigureEnemyForCase(FirstEnemy, *FirstCase)
		|| !ConfigureEnemyForCase(SecondEnemy, *SecondCase))
	{
		ResetFixture();
		return false;
	}

	if (FirstCase->bApplyDefenderTransform)
	{
		Player->SetActorTransform(FirstCase->DefenderTransform, false, nullptr,
			ETeleportType::TeleportPhysics);
	}
	// Pair proofs retain the authored left/right anchor transforms so both threats
	// remain physically distinct; single-case transforms are contact calibrations.
	Player->SetHealth(Player->MaxHealth);
	UHitReactionComponent* PlayerHitReaction = Player->HitReactionComponent.Get();
	if (!PlayerHitReaction)
	{
		ResetFixture();
		return false;
	}
	PlayerHitReaction->DamageResistance = 0.0f;
	PlayerHitReaction->bHasSuperArmor = true;
	if (UCombatComponent* PlayerCombat = Player->GetCombatComponent())
	{
		PlayerCombat->EndBlock();
	}

	UEnemyCombatAIComponent* FirstAI = FirstEnemy->GetCombatAIComponent();
	UEnemyCombatAIComponent* SecondAI = SecondEnemy->GetCombatAIComponent();
	if (!FirstAI || !SecondAI)
	{
		ResetFixture();
		return false;
	}
	FirstAI->SetCombatTarget(Player);
	SecondAI->SetCombatTarget(Player);
	const bool bFirstRequested = FirstAI->TryInitiateAttack();
	const bool bSecondRequested = SecondAI->TryInitiateAttack();
	if ((!bFirstRequested && !FirstAI->IsWaitingForToken())
		|| (!bSecondRequested && !SecondAI->IsWaitingForToken()))
	{
		ResetFixture();
		return false;
	}

	ActiveCase = FName(*FString::Printf(
		TEXT("ThreatPair:%s:%s"), *FirstCaseName.ToString(), *SecondCaseName.ToString()));
	return true;
}

void ADefenseMatrixProofDirector::ResetFixture()
{
	bLastResetComplete = false;
	CaptureFixtureState();
	if (!bFixtureStateCaptured)
	{
		ActiveCase = NAME_None;
		return;
	}
	APlayerCharacter* Player = FindFixturePlayer();
	if (Player)
	{
		if (UPairedAnimationComponent* Paired = Player->PairedAnimationComponent.Get())
		{
			Paired->CancelPairedAnimation(0.0f);
		}
		if (UCombatComponent* Combat = Player->GetCombatComponent())
		{
			Combat->EndBlock();
		}
		if (UHitReactionComponent* HitReaction = Player->HitReactionComponent.Get())
		{
			HitReaction->EndStagger();
		}
	}

	for (const auto& Pair : OriginalEnemyStates)
	{
		if (UEnemyCombatAIComponent* CombatAI = Pair.Key.Get())
		{
			if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CombatAI->GetOwner()))
			{
				if (UPairedAnimationComponent* Paired = Enemy->PairedAnimationComponent.Get())
				{
					Paired->CancelPairedAnimation(0.0f);
				}
			}
			CombatAI->AbortAttack();
		}
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UCombatTokenSubsystem* Tokens =
			GameInstance->GetSubsystem<UCombatTokenSubsystem>())
		{
			Tokens->ResetAllTokens();
		}
	}
	for (const auto& Pair : OriginalEnemyStates)
	{
		if (UEnemyCombatAIComponent* CombatAI = Pair.Key.Get())
		{
			const FEnemyFixtureState& State = Pair.Value;
			if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(CombatAI->GetOwner()))
			{
				Enemy->SetActorTransform(
					State.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
				if (UHitReactionComponent* HitReaction = Enemy->HitReactionComponent.Get())
				{
					HitReaction->EndStagger();
				}
			}
			CombatAI->AvailableAttacks = State.Attacks;
			CombatAI->AttackSelectionMode = State.SelectionMode;
			CombatAI->SetCombatTarget(State.CombatTarget.Get());
		}
	}
	if (Player)
	{
		Player->SetActorTransform(
			OriginalPlayerTransform, false, nullptr, ETeleportType::TeleportPhysics);
		if (UHitReactionComponent* HitReaction = Player->HitReactionComponent.Get())
		{
			HitReaction->DamageResistance = OriginalPlayerDamageResistance;
			HitReaction->bHasSuperArmor = bOriginalPlayerHasSuperArmor;
		}
		if (!Player->IsDeadOrDying())
		{
			Player->SetHealth(OriginalPlayerHealth);
		}
	}
	ActiveCase = NAME_None;
	bool bEnemiesRestored = true;
	for (const auto& Pair : OriginalEnemyStates)
	{
		const UEnemyCombatAIComponent* CombatAI = Pair.Key.Get();
		const AEnemyCharacter* Enemy = CombatAI
			? Cast<AEnemyCharacter>(CombatAI->GetOwner())
			: nullptr;
		const UPairedAnimationComponent* EnemyPaired = Enemy
			? Enemy->PairedAnimationComponent.Get()
			: nullptr;
		bEnemiesRestored = bEnemiesRestored && CombatAI
			&& !CombatAI->HasAttackToken()
			&& !CombatAI->IsWaitingForToken()
			&& !CombatAI->IsAttacking()
			&& EnemyPaired
			&& EnemyPaired->GetChainState() == EChainCounterState::None
			&& !EnemyPaired->HasActiveChainTarget()
			&& !EnemyPaired->IsPairedAnimationActive();
	}
	const UCombatTokenSubsystem* Tokens = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UCombatTokenSubsystem>()
		: nullptr;
	const UCombatComponent* PlayerCombat = Player ? Player->GetCombatComponent() : nullptr;
	const UPairedAnimationComponent* PlayerPaired = Player
		? Player->PairedAnimationComponent.Get()
		: nullptr;
	const bool bPlayerOwnershipRestored = PlayerCombat && PlayerPaired
		&& !PlayerCombat->IsBlocking()
		&& PlayerPaired->GetChainState() == EChainCounterState::None
		&& !PlayerPaired->HasActiveChainTarget()
		&& !PlayerPaired->IsPairedAnimationActive();
	bLastResetComplete = Player && !Player->IsDeadOrDying()
		&& FMath::IsNearlyEqual(Player->CurrentHealth, OriginalPlayerHealth)
		&& Player->HitReactionComponent
		&& FMath::IsNearlyEqual(Player->HitReactionComponent->DamageResistance,
			OriginalPlayerDamageResistance)
		&& Player->HitReactionComponent->bHasSuperArmor == bOriginalPlayerHasSuperArmor
		&& bPlayerOwnershipRestored && bEnemiesRestored && Tokens
		&& Tokens->GetActiveAttackerCount() == 0
		&& Tokens->GetQueueLength() == 0;
}

TArray<FName> ADefenseMatrixProofDirector::GetCaseNames() const
{
	TArray<FName> Result;
	Result.Reserve(Cases.Num());
	for (const FDefenseMatrixProofCase& Case : Cases)
	{
		Result.Add(Case.CaseName);
	}
	return Result;
}

APlayerCharacter* ADefenseMatrixProofDirector::GetFixturePlayer() const
{
	return FindFixturePlayer();
}

AEnemyCharacter* ADefenseMatrixProofDirector::GetFixtureEnemy(const FName AnchorTag) const
{
	return FindFixtureEnemy(AnchorTag);
}

void ADefenseMatrixProofDirector::StartHandsOffCase()
{
	HandsOffTimerHandle.Invalidate();
	StartNamedCase(HandsOffCase);
}

APlayerCharacter* ADefenseMatrixProofDirector::FindFixturePlayer() const
{
	if (FixturePlayer.IsValid())
	{
		return FixturePlayer.Get();
	}
	APlayerCharacter* Match = nullptr;
	int32 MatchCount = 0;
	for (TActorIterator<APlayerCharacter> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(PlayerFixtureTag))
		{
			Match = *It;
			++MatchCount;
		}
	}
	return MatchCount == 1 ? Match : nullptr;
}

AEnemyCharacter* ADefenseMatrixProofDirector::FindFixtureEnemy(const FName AnchorTag) const
{
	if (AnchorTag.IsNone())
	{
		return nullptr;
	}
	AEnemyCharacter* Match = nullptr;
	int32 MatchCount = 0;
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(AnchorTag) && It->GetCombatAIComponent())
		{
			Match = *It;
			++MatchCount;
		}
	}
	return MatchCount == 1 ? Match : nullptr;
}

const FDefenseMatrixProofCase* ADefenseMatrixProofDirector::FindCase(
	const FName CaseName) const
{
	const FDefenseMatrixProofCase* Match = nullptr;
	int32 MatchCount = 0;
	for (const FDefenseMatrixProofCase& Candidate : Cases)
	{
		if (Candidate.CaseName == CaseName)
		{
			Match = &Candidate;
			++MatchCount;
		}
	}
	return MatchCount == 1 ? Match : nullptr;
}

bool ADefenseMatrixProofDirector::ValidateCaseForStart(
	const FDefenseMatrixProofCase& ProofCase) const
{
	const APlayerCharacter* Player = FindFixturePlayer();
	return ProofCase.Attack && !ProofCase.AttackerAnchorTag.IsNone()
		&& Player && !Player->IsDeadOrDying() && Player->MaxHealth > 0.0f
		&& ProofCase.Attack->BaseDamage > 0.0f
		&& ProofCase.Attack->BaseDamage < Player->MaxHealth
		&& (!ProofCase.bApplyDefenderTransform
			|| !ProofCase.DefenderTransform.ContainsNaN())
		&& (!ProofCase.bApplyAttackerTransform
			|| !ProofCase.AttackerTransform.ContainsNaN())
		&& FindFixtureEnemy(ProofCase.AttackerAnchorTag);
}

bool ADefenseMatrixProofDirector::ConfigureEnemyForCase(
	AEnemyCharacter* Enemy,
	const FDefenseMatrixProofCase& ProofCase) const
{
	UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->GetCombatAIComponent() : nullptr;
	if (!CombatAI || !ProofCase.Attack)
	{
		return false;
	}
	FEnemyAttackConfig AttackConfig;
	AttackConfig.AttackData = ProofCase.Attack;
	AttackConfig.SelectionWeight = 1.0f;
	AttackConfig.MinRange = 0.0f;
	AttackConfig.MaxRange = 350.0f;
	CombatAI->AvailableAttacks = {AttackConfig};
	CombatAI->AttackSelectionMode = EEnemyAttackSelection::Single;
	return true;
}

void ADefenseMatrixProofDirector::ClearEnemyAttackConfigs()
{
	for (const auto& Pair : OriginalEnemyStates)
	{
		if (UEnemyCombatAIComponent* CombatAI = Pair.Key.Get())
		{
			CombatAI->AvailableAttacks.Reset();
		}
	}
}

void ADefenseMatrixProofDirector::CaptureFixtureState()
{
	if (bFixtureStateCaptured)
	{
		return;
	}

	APlayerCharacter* Player = FindFixturePlayer();
	if (!Player || Player->IsDeadOrDying() || Player->CurrentHealth <= 0.0f
		|| Player->MaxHealth <= 0.0f || !Player->HitReactionComponent)
	{
		return;
	}

	OriginalEnemyStates.Reset();
	for (const FName AnchorTag : {LeftAnchorTag, CenterAnchorTag, RightAnchorTag})
	{
		AEnemyCharacter* Enemy = FindFixtureEnemy(AnchorTag);
		UEnemyCombatAIComponent* CombatAI = Enemy ? Enemy->GetCombatAIComponent() : nullptr;
		if (!Enemy || !CombatAI)
		{
			OriginalEnemyStates.Reset();
			return;
		}
		FEnemyFixtureState State;
		State.Attacks = CombatAI->AvailableAttacks;
		State.SelectionMode = CombatAI->AttackSelectionMode;
		State.CombatTarget = CombatAI->CombatTarget;
		State.ActorTransform = Enemy->GetActorTransform();
		OriginalEnemyStates.Add(CombatAI, MoveTemp(State));
	}
	if (OriginalEnemyStates.Num() != 3)
	{
		return;
	}

	FixturePlayer = Player;
	OriginalPlayerTransform = Player->GetActorTransform();
	OriginalPlayerHealth = Player->CurrentHealth;
	OriginalPlayerDamageResistance = Player->HitReactionComponent->DamageResistance;
	bOriginalPlayerHasSuperArmor = Player->HitReactionComponent->bHasSuperArmor;
	bFixtureStateCaptured = true;
}
