// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/EnemyCombatAIComponent.h"
#include "AI/CombatTokenSubsystem.h"
#include "Characters/BaseCombatCharacter.h"
#include "Data/AttackData.h"
#include "Interfaces/CombatInterface.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
#include "Core/TargetingComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

UEnemyCombatAIComponent::UEnemyCombatAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyCombatAIComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache token subsystem reference
	UWorld* World = GetWorld();
	if (UGameInstance* GI = World ? World->GetGameInstance() : nullptr)
	{
		SetTokenSubsystem(GI->GetSubsystem<UCombatTokenSubsystem>());
	}

	// Initialize circling direction randomly
	CirclingDirection = FMath::RandBool() ? 1 : -1;
	ScheduleCirclingDirectionChange();

	BindOwnerDeathEvents();
}

void UEnemyCombatAIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	++AttackStartupAttempt;
	DefenseChainSuppressions.Reset();
	ConsumedAttackHistory.Reset();
	CompletedAttackHistory.Reset();
	bAttackStartupInProgress = false;
	StartupConsumedEvent.Reset();
	bAttackTerminationCommitted = true;
	if (ActiveAttackInstance.IsValid())
	{
		LastTerminatedAttackInstance = ActiveAttackInstance;
		if (const ABaseCombatCharacter* OwnerCharacter = Cast<ABaseCombatCharacter>(GetOwner()))
		{
			if (UCombatComponent* Combat = OwnerCharacter->CombatComponent.Get())
			{
				Combat->AbortActiveAttack(ActiveAttackInstance);
			}
			if (UTargetingComponent* Targeting = OwnerCharacter->GetTargetingComponent())
			{
				Targeting->ReleaseActiveAttackWarp();
			}
		}
	}
	UnbindAttackConsumption();
	UnbindAttackMontageEnd();
	ActiveAttackInstance = {};
	// Clean up token if we have one
	ReleaseTokenAndCleanup();

	// Unbind from subsystem
	if (TokenSubsystem)
	{
		TokenSubsystem->OnTokenGranted.RemoveDynamic(this, &UEnemyCombatAIComponent::HandleTokenGranted);
	}

	if (ABaseCombatCharacter* OwnerCharacter = Cast<ABaseCombatCharacter>(GetOwner()))
	{
		OwnerCharacter->OnCharacterDying.RemoveDynamic(this, &UEnemyCombatAIComponent::HandleOwnerDying);
		OwnerCharacter->OnCharacterDeath.RemoveDynamic(this, &UEnemyCombatAIComponent::HandleOwnerDying);
	}

	// Clear timers
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
		World->GetTimerManager().ClearTimer(CirclingDirectionTimerHandle);
	}

#if WITH_AUTOMATION_TESTS
	PostExecuteAttackDataHookForTesting = {};
	PostAttackStateTransitionHookForTesting = {};
	PostApproachStateTransitionHookForTesting = {};
	PostCombatAbortHookForTesting = {};
	PostAttackStartedHookForTesting = {};
	PostAttackEndedHookForTesting = {};
#endif

	Super::EndPlay(EndPlayReason);
}

void UEnemyCombatAIComponent::SetTokenSubsystemForTesting(UCombatTokenSubsystem* InTokenSubsystem)
{
	SetTokenSubsystem(InTokenSubsystem);
}

#if WITH_AUTOMATION_TESTS
void UEnemyCombatAIComponent::SetPostExecuteAttackDataHookForTesting(TFunction<void()> Hook)
{
	PostExecuteAttackDataHookForTesting = MoveTemp(Hook);
}

void UEnemyCombatAIComponent::SetPostAttackStateTransitionHookForTesting(TFunction<void()> Hook)
{
	PostAttackStateTransitionHookForTesting = MoveTemp(Hook);
}

void UEnemyCombatAIComponent::SetPostApproachStateTransitionHookForTesting(TFunction<void()> Hook)
{
	PostApproachStateTransitionHookForTesting = MoveTemp(Hook);
}

void UEnemyCombatAIComponent::SetPostCombatAbortHookForTesting(TFunction<void()> Hook)
{
	PostCombatAbortHookForTesting = MoveTemp(Hook);
}

void UEnemyCombatAIComponent::SetPostAttackStartedHookForTesting(TFunction<void()> Hook)
{
	PostAttackStartedHookForTesting = MoveTemp(Hook);
}

void UEnemyCombatAIComponent::SetPostAttackEndedHookForTesting(TFunction<void()> Hook)
{
	PostAttackEndedHookForTesting = MoveTemp(Hook);
}

void UEnemyCombatAIComponent::InvokeAttackMontageEndedForTesting(
	UAnimMontage* Montage,
	const bool bInterrupted,
	const FAttackInstanceId& ExpectedAttack)
{
	OnAttackMontageEnded(Montage, bInterrupted, ExpectedAttack);
}
#endif

// ============================================================================
// COMBAT API
// ============================================================================

bool UEnemyCombatAIComponent::TryInitiateAttack()
{
	BindOwnerDeathEvents();

	if (!CanAttemptAttack())
	{
		return false;
	}

	if (!TokenSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: No token subsystem available"), *GetOwner()->GetName());
		return false;
	}

	AActor* const OwnerActor = GetOwner();
	UCombatTokenSubsystem* const RequestTokenSubsystem = TokenSubsystem.Get();
	AActor* const TargetActor = CombatTarget.Get();
	UAttackData* const AttackData = SelectAttack();
	SelectedAttack = AttackData;
	if (!AttackData || !OwnerActor || !TargetActor || !RequestTokenSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: No attack available"),
			OwnerActor ? *OwnerActor->GetName() : TEXT("None"));
		return false;
	}

	const TWeakObjectPtr<UEnemyCombatAIComponent> ComponentSnapshot(this);
	const TWeakObjectPtr<AActor> OwnerSnapshot(OwnerActor);
	const TWeakObjectPtr<AActor> TargetSnapshot(TargetActor);
	const TWeakObjectPtr<UAttackData> AttackDataSnapshot(AttackData);
	const TWeakObjectPtr<UCombatTokenSubsystem> TokenSubsystemSnapshot(RequestTokenSubsystem);
	const FString OwnerName = OwnerActor->GetName();
	const FString AttackName = AttackData->GetName();
	const uint64 TokenRequestAttempt = ++AttackStartupAttempt;
	const auto HasExpectedRequestOwnership = [this,
		OwnerSnapshot,
		TargetSnapshot,
		AttackDataSnapshot,
		TokenSubsystemSnapshot,
		TokenRequestAttempt]()
		{
			return AttackStartupAttempt == TokenRequestAttempt
				&& OwnerSnapshot.IsValid()
				&& GetOwner() == OwnerSnapshot.Get()
				&& TargetSnapshot.IsValid()
				&& CombatTarget.Get() == TargetSnapshot.Get()
				&& AttackDataSnapshot.IsValid()
				&& SelectedAttack.Get() == AttackDataSnapshot.Get()
				&& TokenSubsystemSnapshot.IsValid()
				&& TokenSubsystem.Get() == TokenSubsystemSnapshot.Get();
		};
	const auto ReleaseOrphanedToken = [OwnerActor, TokenSubsystemSnapshot]()
		{
			if (UCombatTokenSubsystem* SurvivingTokenSubsystem = TokenSubsystemSnapshot.Get())
			{
				SurvivingTokenSubsystem->ReleaseAttackToken(OwnerActor);
			}
		};
	const auto RollbackCurrentGrant = [this, ComponentSnapshot, TokenRequestAttempt]()
		{
			if (!ComponentSnapshot.IsValid() || AttackStartupAttempt != TokenRequestAttempt)
			{
				return;
			}
			if (CurrentState == EEnemyAIState::Idle
				|| CurrentState == EEnemyAIState::Circling
				|| CurrentState == EEnemyAIState::Approaching)
			{
				ReleaseTokenAndReturnToReadyState();
			}
			else
			{
				ReleaseTokenAndCleanup();
			}
		};

	// Request attack token
	bWaitingForTokenGrant = false;
	const bool bTokenGranted = RequestTokenSubsystem->RequestAttackToken(OwnerActor);
	if (!ComponentSnapshot.IsValid())
	{
		ReleaseOrphanedToken();
		return false;
	}
	if (AttackStartupAttempt != TokenRequestAttempt)
	{
		return false;
	}

	if (bTokenGranted)
	{
		if (!HasExpectedRequestOwnership() || !HasAttackToken())
		{
			RollbackCurrentGrant();
			return false;
		}

		SetState(EEnemyAIState::Approaching);
#if WITH_AUTOMATION_TESTS
		TFunction<void()> PostApproachStateTransitionHook = MoveTemp(PostApproachStateTransitionHookForTesting);
		PostApproachStateTransitionHookForTesting = {};
		if (PostApproachStateTransitionHook)
		{
			PostApproachStateTransitionHook();
		}
#endif
		if (!ComponentSnapshot.IsValid())
		{
			ReleaseOrphanedToken();
			return false;
		}
		if (!HasExpectedRequestOwnership()
			|| CurrentState != EEnemyAIState::Approaching
			|| !HasAttackToken())
		{
			RollbackCurrentGrant();
			return false;
		}

		UWorld* const World = GetWorld();
		if (!World)
		{
			RollbackCurrentGrant();
			return false;
		}
		ApproachStartTime = World->GetTimeSeconds();
#if WITH_AUTOMATION_TESTS
		++TokenGrantBroadcastCountForTesting;
#endif
		OnTokenGranted.Broadcast();
		if (!ComponentSnapshot.IsValid())
		{
			ReleaseOrphanedToken();
			return false;
		}
		if (!HasExpectedRequestOwnership()
			|| CurrentState != EEnemyAIState::Approaching
			|| !HasAttackToken())
		{
			RollbackCurrentGrant();
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Token granted, approaching with %s"),
			*OwnerName, *AttackName);
		return true;
	}
	else
	{
		if (!HasExpectedRequestOwnership())
		{
			return false;
		}

		bWaitingForTokenGrant = RequestTokenSubsystem->IsInTokenQueue(OwnerActor);
		if (!bWaitingForTokenGrant)
		{
			SelectedAttack = nullptr;
		}

		UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Token %s"),
			*OwnerName,
			bWaitingForTokenGrant ? TEXT("queued, continuing to circle") : TEXT("request denied"));
		return false;
	}
}

void UEnemyCombatAIComponent::CancelQueuedAttackRequest()
{
	bWaitingForTokenGrant = false;

	if (TokenSubsystem && TokenSubsystem->IsInTokenQueue(GetOwner()))
	{
		TokenSubsystem->RemoveFromQueue(GetOwner());
	}

	if (!HasAttackToken())
	{
		SelectedAttack = nullptr;
	}
}

void UEnemyCombatAIComponent::AbortAttack()
{
	const uint64 AbortAttempt = ++AttackStartupAttempt;
	const TWeakObjectPtr<UEnemyCombatAIComponent> ComponentSnapshot(this);
	CancelQueuedAttackRequest();
	if (CurrentState == EEnemyAIState::Dying)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}

	const bool bOwnsLiveAttack = CurrentState == EEnemyAIState::Attacking
		&& ActiveAttackInstance.IsValid()
		&& !bAttackTerminationCommitted;
	if (bOwnsLiveAttack)
	{
		TerminateActiveAttack(
			ActiveAttackInstance,
			true,
			CombatTarget.IsValid() ? EEnemyAIState::Circling : EEnemyAIState::Idle,
			-1.0f,
			true,
			false);
	}
	else
	{
		bAttackStartupInProgress = false;
		StartupConsumedEvent.Reset();
		bAttackTerminationCommitted = true;
		ActiveAttackInstance = {};
		UnbindAttackConsumption();
		UnbindAttackMontageEnd();
		if (const ABaseCombatCharacter* OwnerCharacter = Cast<ABaseCombatCharacter>(GetOwner()))
		{
			if (UTargetingComponent* Targeting = OwnerCharacter->GetTargetingComponent())
			{
				Targeting->ReleaseActiveAttackWarp();
			}
		}
		if (!ComponentSnapshot.IsValid() || AttackStartupAttempt != AbortAttempt)
		{
			return;
		}

		const EEnemyAIState StateBeforeTokenRelease = CurrentState;
		ReleaseTokenAndCleanup();
		if (!ComponentSnapshot.IsValid()
			|| AttackStartupAttempt != AbortAttempt
			|| CurrentState != StateBeforeTokenRelease
			|| ActiveAttackInstance.IsValid()
			|| HasAttackToken()
			|| bWaitingForTokenGrant)
		{
			return;
		}
		ReturnToReadyState();
	}
}

bool UEnemyCombatAIComponent::ExecuteAttack()
{
	FAttackInstanceId IgnoredAttackInstance;
	return ExecuteAttackWithIdentity(IgnoredAttackInstance);
}

bool UEnemyCombatAIComponent::ExecuteAttackWithIdentity(FAttackInstanceId& OutStartedAttack)
{
	OutStartedAttack = {};
	if (IsDefenseChainSuppressed())
	{
		ReleaseTokenAndReturnToReadyState();
		return false;
	}

	if (CurrentState != EEnemyAIState::Approaching)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: Cannot execute attack - not in Approaching state"),
			*GetOwner()->GetName());
		return false;
	}

	AActor* const OwnerActor = GetOwner();
	UAttackData* const AttackData = SelectedAttack;
	UAnimMontage* const AttackMontage = AttackData ? AttackData->AttackMontage.Get() : nullptr;
	AActor* const TargetActor = CombatTarget.Get();
	if (!OwnerActor || !AttackData || !AttackMontage || !TargetActor || !HasAttackToken())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: Cannot execute attack - invalid startup ownership"),
			OwnerActor ? *OwnerActor->GetName() : TEXT("None"));
		ReleaseTokenAndReturnToReadyState();
		return false;
	}

	// Get anim instance
	ACharacter* OwnerChar = Cast<ACharacter>(OwnerActor);
	if (!OwnerChar)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: Owner is not a character"),
			*OwnerActor->GetName());
		ReleaseTokenAndReturnToReadyState();
		return false;
	}

	UAnimInstance* AnimInstance = OwnerChar->GetMesh() ? OwnerChar->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: No anim instance"),
			*OwnerActor->GetName());
		ReleaseTokenAndReturnToReadyState();
		return false;
	}

	UCombatComponent* CombatComponent = OwnerChar->FindComponentByClass<UCombatComponent>();
	if (!CombatComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: Cannot execute attack - no CombatComponent"),
			*OwnerActor->GetName());
		ReleaseTokenAndReturnToReadyState();
		return false;
	}
	const EInputType AttackInputType = AttackData->AttackType == EAttackType::Heavy
		? EInputType::HeavyAttack
		: EInputType::LightAttack;

	const TWeakObjectPtr<UEnemyCombatAIComponent> ComponentSnapshot(this);
	const TWeakObjectPtr<AActor> OwnerSnapshot(OwnerActor);
	const TWeakObjectPtr<AActor> TargetSnapshot(TargetActor);
	const TWeakObjectPtr<UAttackData> AttackDataSnapshot(AttackData);
	const TWeakObjectPtr<UAnimMontage> AttackMontageSnapshot(AttackMontage);
	const TWeakObjectPtr<UAnimInstance> AnimInstanceSnapshot(AnimInstance);
	const TWeakObjectPtr<UCombatComponent> CombatComponentSnapshot(CombatComponent);
	const TWeakObjectPtr<UCombatTokenSubsystem> TokenSubsystemSnapshot(TokenSubsystem);
	const FString OwnerName = OwnerActor->GetName();
	const FString AttackName = AttackData->GetName();
	const int32 BaseAttackGeneration = CombatComponent->GetCurrentAttackGeneration();
	FAttackInstanceId ExpectedAttackInstance;
	ExpectedAttackInstance.Attacker = OwnerActor;
	ExpectedAttackInstance.AttackGeneration = BaseAttackGeneration + 1;
	const uint64 StartupAttempt = ++AttackStartupAttempt;
	const auto RetireOrphanedStartup =
		[CombatComponentSnapshot, TokenSubsystemSnapshot, OwnerActor, ExpectedAttackInstance]()
		{
			if (UCombatComponent* SurvivingCombat = CombatComponentSnapshot.Get())
			{
				SurvivingCombat->AbortActiveAttack(ExpectedAttackInstance);
			}
			if (UCombatTokenSubsystem* SurvivingTokenSubsystem = TokenSubsystemSnapshot.Get())
			{
				SurvivingTokenSubsystem->ReleaseAttackToken(OwnerActor);
			}
		};
	const auto HasSupersedingAttackOwnership = [this]()
		{
			return CurrentState != EEnemyAIState::Dying
				&& ((ActiveAttackInstance.IsValid() && !bAttackTerminationCommitted)
					|| HasAttackToken()
					|| bWaitingForTokenGrant);
		};
	const auto AreStartupDependenciesValid =
		[OwnerSnapshot,
			AttackDataSnapshot,
			AttackMontageSnapshot,
			AnimInstanceSnapshot,
			CombatComponentSnapshot]()
		{
			ACharacter* const CurrentOwnerCharacter = Cast<ACharacter>(OwnerSnapshot.Get());
			UAttackData* const CurrentAttackData = AttackDataSnapshot.Get();
			UAnimMontage* const CurrentMontage = AttackMontageSnapshot.Get();
			UAnimInstance* const CurrentAnimInstance = AnimInstanceSnapshot.Get();
			UCombatComponent* const CurrentCombat = CombatComponentSnapshot.Get();
			return CurrentOwnerCharacter
				&& CurrentAttackData
				&& CurrentMontage
				&& CurrentAnimInstance
				&& CurrentCombat
				&& CurrentAttackData->AttackMontage.Get() == CurrentMontage
				&& CurrentOwnerCharacter->FindComponentByClass<UCombatComponent>() == CurrentCombat
				&& CurrentOwnerCharacter->GetMesh()
				&& CurrentOwnerCharacter->GetMesh()->GetAnimInstance() == CurrentAnimInstance;
		};

	UnbindAttackConsumption();
	UnbindAttackMontageEnd();
	ActiveAttackInstance = {};
	StartupConsumedEvent.Reset();
	bAttackStartupInProgress = true;
	bAttackTerminationCommitted = false;
	AttackConsumptionSource = CombatComponent;
	AttackConsumedDelegateHandle = CombatComponent->OnAttackConsumedInternal.AddUObject(
		this,
		&UEnemyCombatAIComponent::HandleAttackConsumedInternal);

	// Transition to attacking state
	SetState(EEnemyAIState::Attacking);
#if WITH_AUTOMATION_TESTS
	TFunction<void()> PostStateTransitionHook = MoveTemp(PostAttackStateTransitionHookForTesting);
	PostAttackStateTransitionHookForTesting = {};
	if (PostStateTransitionHook)
	{
		PostStateTransitionHook();
	}
#endif
	if (!ComponentSnapshot.IsValid())
	{
		RetireOrphanedStartup();
		return false;
	}
	if (AttackStartupAttempt != StartupAttempt)
	{
		if (!HasSupersedingAttackOwnership())
		{
			RetireOrphanedStartup();
		}
		return false;
	}
	if (CurrentState != EEnemyAIState::Attacking
		|| !OwnerSnapshot.IsValid()
		|| !TargetSnapshot.IsValid()
		|| !AreStartupDependenciesValid()
		|| CombatComponentSnapshot->GetCurrentAttackGeneration() != BaseAttackGeneration
		|| !HasAttackToken())
	{
		TerminatePendingAttack(
			CombatTarget.IsValid() ? EEnemyAIState::Circling : EEnemyAIState::Idle,
			-1.0f);
		return false;
	}

	CombatComponentSnapshot->SetAttackIntentTarget(TargetSnapshot.Get());
	if (!ComponentSnapshot.IsValid())
	{
		RetireOrphanedStartup();
		return false;
	}
	if (AttackStartupAttempt != StartupAttempt)
	{
		if (!HasSupersedingAttackOwnership())
		{
			RetireOrphanedStartup();
		}
		return false;
	}
	if (CurrentState != EEnemyAIState::Attacking
		|| !OwnerSnapshot.IsValid()
		|| !TargetSnapshot.IsValid()
		|| !AreStartupDependenciesValid()
		|| CombatComponentSnapshot->GetCurrentAttackGeneration() != BaseAttackGeneration
		|| !HasAttackToken())
	{
		TerminatePendingAttack(
			CombatTarget.IsValid() ? EEnemyAIState::Circling : EEnemyAIState::Idle,
			-1.0f);
		return false;
	}
#if WITH_AUTOMATION_TESTS
	TFunction<void()> PostExecuteHook = MoveTemp(PostExecuteAttackDataHookForTesting);
	PostExecuteAttackDataHookForTesting = {};
#endif
	const bool bExecuted = CombatComponentSnapshot->ExecuteAttackData(
		AttackDataSnapshot.Get(),
		TargetSnapshot.Get(),
		AttackInputType);
#if WITH_AUTOMATION_TESTS
	if (PostExecuteHook)
	{
		PostExecuteHook();
	}
#endif

	if (!ComponentSnapshot.IsValid())
	{
		RetireOrphanedStartup();
		return false;
	}
	if (AttackStartupAttempt != StartupAttempt)
	{
		if (!HasSupersedingAttackOwnership())
		{
			RetireOrphanedStartup();
		}
		return false;
	}

	FAttackExecutionSnapshot ExecutionSnapshot;
	if (CombatComponentSnapshot.IsValid())
	{
		ExecutionSnapshot = CombatComponentSnapshot->BuildAttackExecutionSnapshot();
	}
	const bool bSnapshotMatches = ExecutionSnapshot.AttackInstance.IsValid()
		&& ExecutionSnapshot.AttackInstance == ExpectedAttackInstance
		&& ExecutionSnapshot.AttackInstance.Attacker == OwnerSnapshot
		&& ExecutionSnapshot.AttackData == AttackDataSnapshot.Get()
		&& ExecutionSnapshot.ActiveMontage == AttackMontageSnapshot;
	const bool bStartupOwnershipIntact = OwnerSnapshot.IsValid()
		&& TargetSnapshot.IsValid()
		&& AreStartupDependenciesValid()
		&& CurrentState == EEnemyAIState::Attacking
		&& !bAttackTerminationCommitted
		&& HasAttackToken();
	const bool bConsumedDuringStartup = bSnapshotMatches
		&& CombatComponentSnapshot.IsValid()
		&& CombatComponentSnapshot->IsAttackConsumed(ExecutionSnapshot.AttackInstance);
	const bool bReplacementAttackCommitted = ActiveAttackInstance.IsValid()
		&& !(ActiveAttackInstance == ExpectedAttackInstance)
		&& !bAttackTerminationCommitted;
	if (!bSnapshotMatches && bReplacementAttackCommitted)
	{
		return false;
	}
	if (bExecuted && bSnapshotMatches && bConsumedDuringStartup
		&& CurrentState != EEnemyAIState::Dying)
	{
		bAttackStartupInProgress = false;
		FAttackConsumedEvent ConsumedEvent;
		ConsumedEvent.AttackInstance = ExecutionSnapshot.AttackInstance;
		ConsumedEvent.Reason = EAttackConsumeReason::Cancelled;
		if (StartupConsumedEvent.IsSet()
			&& StartupConsumedEvent->AttackInstance == ExecutionSnapshot.AttackInstance)
		{
			ConsumedEvent = StartupConsumedEvent.GetValue();
		}
		StartupConsumedEvent.Reset();
		ActiveAttackInstance = ExecutionSnapshot.AttackInstance;
		LastStartedAttackInstance = ExecutionSnapshot.AttackInstance;
		OutStartedAttack = ExecutionSnapshot.AttackInstance;
		RecordConsumedAttack(ExecutionSnapshot.AttackInstance);
		ActiveAttackAnimInstance = AnimInstanceSnapshot;
		ActiveAttackMontage = AttackMontageSnapshot;
		bAttackTerminationCommitted = false;
		const bool bPerfectParry = ConsumedEvent.Reason == EAttackConsumeReason::PerfectParry;
		TerminateActiveAttack(
			ExecutionSnapshot.AttackInstance,
			true,
			bPerfectParry ? EEnemyAIState::Staggered : EEnemyAIState::Recovering,
			bPerfectParry ? StaggerRecoveryTime : PostAttackRecoveryTime,
			true,
			true);
		return true;
	}
	if (!bExecuted || !bSnapshotMatches || !bStartupOwnershipIntact)
	{
		const bool bOwnsUnexpectedCombatGeneration = bExecuted
			&& ExecutionSnapshot.bAttackActive
			&& ExecutionSnapshot.AttackInstance.IsValid()
			&& ExecutionSnapshot.AttackInstance.Attacker == OwnerSnapshot
			&& ExecutionSnapshot.AttackInstance.AttackGeneration >= ExpectedAttackInstance.AttackGeneration;
		const FAttackInstanceId RejectedAttack = bSnapshotMatches
			? ExpectedAttackInstance
			: (bOwnsUnexpectedCombatGeneration
				? ExecutionSnapshot.AttackInstance
				: FAttackInstanceId{});
		const TWeakObjectPtr<UAnimMontage> RejectedMontage = bOwnsUnexpectedCombatGeneration
			? ExecutionSnapshot.ActiveMontage
			: AttackMontageSnapshot;
		const uint64 RejectionAttempt = ++AttackStartupAttempt;
		bAttackStartupInProgress = false;
		bAttackTerminationCommitted = true;
		StartupConsumedEvent.Reset();
		UnbindAttackConsumption();
		ActiveAttackInstance = {};
		if (RejectedAttack.IsValid() && CombatComponentSnapshot.IsValid())
		{
			CombatComponentSnapshot->AbortActiveAttack(RejectedAttack);
		}
		if (!ComponentSnapshot.IsValid())
		{
			RetireOrphanedStartup();
			return false;
		}
		if (AttackStartupAttempt != RejectionAttempt)
		{
			return false;
		}
		if (RejectedAttack.IsValid() && AnimInstanceSnapshot.IsValid() && RejectedMontage.IsValid())
		{
			AnimInstanceSnapshot->Montage_Stop(0.2f, RejectedMontage.Get());
		}
		if (!ComponentSnapshot.IsValid())
		{
			RetireOrphanedStartup();
			return false;
		}
		if (AttackStartupAttempt != RejectionAttempt)
		{
			return false;
		}
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: Attack startup was rejected or synchronously invalidated"),
			*OwnerName);
		const EEnemyAIState StateBeforeTokenRelease = CurrentState;
		ReleaseTokenAndCleanup();
		if (!ComponentSnapshot.IsValid()
			|| AttackStartupAttempt != RejectionAttempt
			|| CurrentState != StateBeforeTokenRelease
			|| ActiveAttackInstance.IsValid()
			|| HasAttackToken()
			|| bWaitingForTokenGrant)
		{
			return false;
		}
		if (CurrentState == EEnemyAIState::Attacking
			|| CurrentState == EEnemyAIState::Approaching)
		{
			ReturnToReadyState();
		}
		return false;
	}

	bAttackStartupInProgress = false;
	UnbindAttackConsumption();
	StartupConsumedEvent.Reset();
	ActiveAttackInstance = ExecutionSnapshot.AttackInstance;
	if (!ActiveAttackInstance.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[EnemyAI] %s: Attack started without a valid generation"), *OwnerName);
		ReleaseTokenAndReturnToReadyState();
		return false;
	}
	bAttackTerminationCommitted = false;
	LastStartedAttackInstance = ActiveAttackInstance;
	OutStartedAttack = ActiveAttackInstance;
	ActiveAttackAnimInstance = AnimInstanceSnapshot;
	ActiveAttackMontage = AttackMontageSnapshot;
	AttackConsumptionSource = CombatComponentSnapshot;
	AttackConsumedDelegateHandle = CombatComponent->OnAttackConsumedInternal.AddUObject(
		this,
		&UEnemyCombatAIComponent::HandleAttackConsumedInternal);

	// Bind to montage end
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(
		this,
		&UEnemyCombatAIComponent::OnAttackMontageEnded,
		ActiveAttackInstance);
	AnimInstanceSnapshot->Montage_SetEndDelegate(EndDelegate, AttackMontageSnapshot.Get());

#if WITH_AUTOMATION_TESTS
	TFunction<void()> PostAttackStartedHook = MoveTemp(PostAttackStartedHookForTesting);
	PostAttackStartedHookForTesting = {};
#endif

	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Executing attack %s"),
		*OwnerName, *AttackName);
	OnAttackStarted.Broadcast(AttackDataSnapshot.Get());
	if (!ComponentSnapshot.IsValid())
	{
		RetireOrphanedStartup();
		return true;
	}
#if WITH_AUTOMATION_TESTS
	if (PostAttackStartedHook)
	{
		PostAttackStartedHook();
	}
	if (!ComponentSnapshot.IsValid())
	{
		RetireOrphanedStartup();
		return true;
	}
#endif

	if (bAttackTerminationCommitted
		|| CurrentState != EEnemyAIState::Attacking
		|| !(ActiveAttackInstance == ExpectedAttackInstance))
	{
		return true;
	}

	if (CombatComponentSnapshot.IsValid()
		&& CombatComponentSnapshot->IsAttackConsumed(ExpectedAttackInstance))
	{
		RecordConsumedAttack(ExpectedAttackInstance);
		TerminateActiveAttack(
			ExpectedAttackInstance,
			true,
			EEnemyAIState::Recovering,
			PostAttackRecoveryTime,
			true,
			true);
		return true;
	}

	FAttackExecutionSnapshot PostBroadcastSnapshot;
	if (CombatComponentSnapshot.IsValid())
	{
		PostBroadcastSnapshot = CombatComponentSnapshot->BuildAttackExecutionSnapshot();
	}
	const bool bPostBroadcastOwnershipValid = OwnerSnapshot.IsValid()
		&& TargetSnapshot.IsValid()
		&& AttackDataSnapshot.IsValid()
		&& AttackMontageSnapshot.IsValid()
		&& AnimInstanceSnapshot.IsValid()
		&& CombatComponentSnapshot.IsValid()
		&& PostBroadcastSnapshot.AttackInstance == ExpectedAttackInstance
		&& PostBroadcastSnapshot.AttackData == AttackDataSnapshot.Get()
		&& PostBroadcastSnapshot.ActiveMontage == AttackMontageSnapshot
		&& PostBroadcastSnapshot.AttackPhase != EAttackPhase::None;
	if (!bPostBroadcastOwnershipValid)
	{
		const bool bOwnsUnexpectedCombatGeneration = PostBroadcastSnapshot.bAttackActive
			&& PostBroadcastSnapshot.AttackInstance.IsValid()
			&& PostBroadcastSnapshot.AttackInstance.Attacker == OwnerSnapshot
			&& !(PostBroadcastSnapshot.AttackInstance == ExpectedAttackInstance)
			&& PostBroadcastSnapshot.AttackInstance.AttackGeneration > ExpectedAttackInstance.AttackGeneration;
		if (bOwnsUnexpectedCombatGeneration && CombatComponentSnapshot.IsValid())
		{
			CombatComponentSnapshot->AbortActiveAttack(PostBroadcastSnapshot.AttackInstance);
		}
		if (!ComponentSnapshot.IsValid()
			|| CurrentState != EEnemyAIState::Attacking
			|| !(ActiveAttackInstance == ExpectedAttackInstance))
		{
			return true;
		}
		if (bOwnsUnexpectedCombatGeneration
			&& AnimInstanceSnapshot.IsValid()
			&& PostBroadcastSnapshot.ActiveMontage.IsValid())
		{
			AnimInstanceSnapshot->Montage_Stop(0.2f, PostBroadcastSnapshot.ActiveMontage.Get());
		}
		if (!ComponentSnapshot.IsValid()
			|| CurrentState != EEnemyAIState::Attacking
			|| !(ActiveAttackInstance == ExpectedAttackInstance))
		{
			return true;
		}
		const bool bOwnerDying = Cast<ABaseCombatCharacter>(OwnerSnapshot.Get())
			&& CastChecked<ABaseCombatCharacter>(OwnerSnapshot.Get())->IsDeadOrDying();
		const EEnemyAIState TerminalState = bOwnerDying
			? EEnemyAIState::Dying
			: (TargetSnapshot.IsValid() ? EEnemyAIState::Recovering : EEnemyAIState::Idle);
		TerminateActiveAttack(
			ExpectedAttackInstance,
			true,
			TerminalState,
			TerminalState == EEnemyAIState::Recovering ? PostAttackRecoveryTime : -1.0f,
			true,
			false);
	}

	return true;
}

void UEnemyCombatAIComponent::OnCountered()
{
	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Countered by player"), *GetOwner()->GetName());
	if (!TerminateActiveAttack(
		ActiveAttackInstance,
		true,
		EEnemyAIState::Staggered,
		StaggerRecoveryTime,
		true,
		false))
	{
		TerminatePendingAttack(EEnemyAIState::Staggered, StaggerRecoveryTime);
	}
}

void UEnemyCombatAIComponent::OnParried()
{
	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Parried by player"), *GetOwner()->GetName());
	if (!TerminateActiveAttack(
		ActiveAttackInstance,
		true,
		EEnemyAIState::Staggered,
		StaggerRecoveryTime,
		true,
		false))
	{
		TerminatePendingAttack(EEnemyAIState::Staggered, StaggerRecoveryTime);
	}
}

void UEnemyCombatAIComponent::OnDamaged()
{
	// If we're attacking, this interrupts us
	if (CurrentState == EEnemyAIState::Attacking || CurrentState == EEnemyAIState::Approaching)
	{
		UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Damaged during attack, interrupting"), *GetOwner()->GetName());

		if (!TerminateActiveAttack(
			ActiveAttackInstance,
			true,
			EEnemyAIState::Staggered,
			StaggerRecoveryTime,
			true,
			false))
		{
			TerminatePendingAttack(EEnemyAIState::Staggered, StaggerRecoveryTime);
		}
	}
}

void UEnemyCombatAIComponent::OnDeath()
{
	++AttackStartupAttempt;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
		World->GetTimerManager().ClearTimer(CirclingDirectionTimerHandle);
	}

	AActor* const OwnerActor = GetOwner();
	const FString OwnerName = OwnerActor ? OwnerActor->GetName() : TEXT("None");
	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Died"), *OwnerName);
	if (ActiveAttackInstance.IsValid() && !bAttackTerminationCommitted)
	{
		TerminateActiveAttack(
			ActiveAttackInstance,
			true,
			EEnemyAIState::Dying,
			-1.0f,
			true,
			false);
		return;
	}

	bAttackTerminationCommitted = true;
	bAttackStartupInProgress = false;
	StartupConsumedEvent.Reset();
	UnbindAttackConsumption();
	UnbindAttackMontageEnd();
	ActiveAttackInstance = {};
	const EEnemyAIState PreviousState = CurrentState;
	CurrentState = EEnemyAIState::Dying;
	const TWeakObjectPtr<UEnemyCombatAIComponent> ComponentSnapshot(this);
	ReleaseTokenAndCleanup();
	if (!ComponentSnapshot.IsValid() || PreviousState == EEnemyAIState::Dying)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: State %s -> %s"),
		*OwnerName,
		*UEnum::GetValueAsString(PreviousState),
		*UEnum::GetValueAsString(EEnemyAIState::Dying));
	OnAIStateChanged.Broadcast(PreviousState, EEnemyAIState::Dying);
}

void UEnemyCombatAIComponent::SetCombatTarget(AActor* Target)
{
	CombatTarget = Target;

	// Transition from Idle to Circling when we get a target
	if (Target && CurrentState == EEnemyAIState::Idle)
	{
		SetState(EEnemyAIState::Circling);
	}
	else if (!Target)
	{
		if (CurrentState == EEnemyAIState::Idle || CurrentState == EEnemyAIState::Circling || CurrentState == EEnemyAIState::Approaching)
		{
			const TWeakObjectPtr<UEnemyCombatAIComponent> ComponentSnapshot(this);
			const uint64 CleanupAttempt = AttackStartupAttempt;
			const EEnemyAIState StateBeforeTokenRelease = CurrentState;
			ReleaseTokenAndCleanup();
			if (!ComponentSnapshot.IsValid()
				|| AttackStartupAttempt != CleanupAttempt
				|| CurrentState != StateBeforeTokenRelease
				|| ActiveAttackInstance.IsValid()
				|| HasAttackToken()
				|| bWaitingForTokenGrant)
			{
				return;
			}
		}

		if (CurrentState == EEnemyAIState::Circling || CurrentState == EEnemyAIState::Approaching)
		{
			ReturnToReadyState();
		}
	}
}

// ============================================================================
// MOVEMENT API
// ============================================================================

FVector UEnemyCombatAIComponent::GetCirclingDestination() const
{
	AActor* Target = CombatTarget.Get();
	if (!Target)
	{
		return GetOwner()->GetActorLocation();
	}

	// Calculate position on circle around target
	FVector TargetLocation = Target->GetActorLocation();
	FVector ToEnemy = GetOwner()->GetActorLocation() - TargetLocation;
	ToEnemy.Z = 0.0f;

	// Get current angle
	float CurrentAngle = FMath::Atan2(ToEnemy.Y, ToEnemy.X);

	// Add offset based on circling direction and speed
	// This gives us a position slightly ahead on the circle
	float AngleOffset = FMath::DegreesToRadians(30.0f) * CirclingDirection;
	float NewAngle = CurrentAngle + AngleOffset;

	// Calculate new position at circle radius
	FVector NewPosition = TargetLocation;
	NewPosition.X += CirclingConfig.CircleRadius * FMath::Cos(NewAngle);
	NewPosition.Y += CirclingConfig.CircleRadius * FMath::Sin(NewAngle);

	return NewPosition;
}

bool UEnemyCombatAIComponent::IsInAttackRange() const
{
	float Distance = GetDistanceToTarget();
	if (Distance >= MAX_FLT)
	{
		return false;
	}

	// Use selected attack's range if available, otherwise use approach config
	float AttackRange = ApproachConfig.AttackRange;
	if (SelectedAttack)
	{
		// Could use attack-specific range from FEnemyAttackConfig
		// For now use approach config as default
	}

	return Distance <= AttackRange;
}

float UEnemyCombatAIComponent::GetDistanceToTarget() const
{
	AActor* Target = CombatTarget.Get();
	if (!Target)
	{
		return MAX_FLT;
	}

	return FVector::Dist(GetOwner()->GetActorLocation(), Target->GetActorLocation());
}

void UEnemyCombatAIComponent::RandomizeCirclingDirection()
{
	CirclingDirection = FMath::RandBool() ? 1 : -1;
}

// ============================================================================
// QUERIES
// ============================================================================

bool UEnemyCombatAIComponent::HasAttackToken() const
{
	if (!TokenSubsystem)
	{
		return false;
	}
	return TokenSubsystem->HasAttackToken(GetOwner());
}

bool UEnemyCombatAIComponent::IsWaitingForToken() const
{
	if (!TokenSubsystem)
	{
		return false;
	}
	return TokenSubsystem->IsInTokenQueue(GetOwner());
}

bool UEnemyCombatAIComponent::CanAttemptAttack() const
{
	if (IsDefenseChainSuppressed())
	{
		return false;
	}
	if (const ABaseCombatCharacter* OwnerCharacter = Cast<ABaseCombatCharacter>(GetOwner());
		OwnerCharacter
		&& OwnerCharacter->HitReactionComponent
		&& OwnerCharacter->HitReactionComponent->IsStaggered())
	{
		return false;
	}

	// Can only initiate attack from Circling or Idle states
	if (CurrentState != EEnemyAIState::Circling && CurrentState != EEnemyAIState::Idle)
	{
		return false;
	}

	// Need a target
	if (!CombatTarget.IsValid())
	{
		return false;
	}

	// Need available attacks
	if (AvailableAttacks.Num() == 0)
	{
		return false;
	}

	return true;
}

EEnemyAttackExecutionStatus UEnemyCombatAIComponent::GetAttackExecutionStatus(
	const FAttackInstanceId& AttackInstance) const
{
	if (!AttackInstance.IsValid() || AttackInstance.Attacker.Get() != GetOwner())
	{
		return EEnemyAttackExecutionStatus::Invalid;
	}
	if (WasAttackInstanceConsumed(AttackInstance)
		|| WasAttackInstanceCompleted(AttackInstance))
	{
		return EEnemyAttackExecutionStatus::Succeeded;
	}
	if (!bAttackTerminationCommitted
		&& CurrentState == EEnemyAIState::Attacking
		&& ActiveAttackInstance == AttackInstance)
	{
		return EEnemyAttackExecutionStatus::Running;
	}
	return EEnemyAttackExecutionStatus::Failed;
}

// ============================================================================
// INTERNAL
// ============================================================================

void UEnemyCombatAIComponent::SetState(EEnemyAIState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	EEnemyAIState OldState = CurrentState;
	CurrentState = NewState;

	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: State %s -> %s"),
		*GetOwner()->GetName(),
		*UEnum::GetValueAsString(OldState),
		*UEnum::GetValueAsString(NewState));

	OnAIStateChanged.Broadcast(OldState, NewState);
}

UAttackData* UEnemyCombatAIComponent::SelectAttack()
{
	if (AvailableAttacks.Num() == 0)
	{
		return nullptr;
	}

	// Filter attacks by range if we have a target
	TArray<FEnemyAttackConfig*> ValidAttacks;
	float DistanceToTarget = GetDistanceToTarget();

	for (FEnemyAttackConfig& Config : AvailableAttacks)
	{
		if (Config.AttackData && DistanceToTarget >= Config.MinRange && DistanceToTarget <= Config.MaxRange)
		{
			ValidAttacks.Add(&Config);
		}
	}

	// If no valid attacks in range, use all attacks (approach will handle range)
	if (ValidAttacks.Num() == 0)
	{
		for (FEnemyAttackConfig& Config : AvailableAttacks)
		{
			if (Config.AttackData)
			{
				ValidAttacks.Add(&Config);
			}
		}
	}

	if (ValidAttacks.Num() == 0)
	{
		return nullptr;
	}

	switch (AttackSelectionMode)
	{
	case EEnemyAttackSelection::Single:
		return ValidAttacks[0]->AttackData;

	case EEnemyAttackSelection::Sequential:
		{
			int32 Index = SequentialAttackIndex % ValidAttacks.Num();
			SequentialAttackIndex++;
			return ValidAttacks[Index]->AttackData;
		}

	case EEnemyAttackSelection::Contextual:
		// For now, just pick the attack with best range match
		{
			FEnemyAttackConfig* BestMatch = ValidAttacks[0];
			float BestRangeDiff = FMath::Abs(DistanceToTarget - BestMatch->MaxRange);

			for (FEnemyAttackConfig* Config : ValidAttacks)
			{
				float RangeDiff = FMath::Abs(DistanceToTarget - Config->MaxRange);
				if (RangeDiff < BestRangeDiff)
				{
					BestRangeDiff = RangeDiff;
					BestMatch = Config;
				}
			}
			return BestMatch->AttackData;
		}

	case EEnemyAttackSelection::Random:
	default:
		// Weighted random selection
		{
			float TotalWeight = 0.0f;
			for (FEnemyAttackConfig* Config : ValidAttacks)
			{
				TotalWeight += Config->SelectionWeight;
			}

			float RandomValue = FMath::FRand() * TotalWeight;
			float AccumulatedWeight = 0.0f;

			for (FEnemyAttackConfig* Config : ValidAttacks)
			{
				AccumulatedWeight += Config->SelectionWeight;
				if (RandomValue <= AccumulatedWeight)
				{
					return Config->AttackData;
				}
			}

			// Fallback (shouldn't happen)
			return ValidAttacks[0]->AttackData;
		}
	}
}

void UEnemyCombatAIComponent::ReleaseTokenAndCleanup()
{
	bWaitingForTokenGrant = false;
	SelectedAttack = nullptr;

	AActor* const OwnerActor = GetOwner();
	UCombatTokenSubsystem* const CurrentTokenSubsystem = TokenSubsystem.Get();
	if (CurrentTokenSubsystem && OwnerActor)
	{
		if (CurrentTokenSubsystem->HasAttackToken(OwnerActor))
		{
#if WITH_AUTOMATION_TESTS
			++TokenReleaseCountForTesting;
#endif
			CurrentTokenSubsystem->ReleaseAttackToken(OwnerActor);
		}
		else if (CurrentTokenSubsystem->IsInTokenQueue(OwnerActor))
		{
			CurrentTokenSubsystem->RemoveFromQueue(OwnerActor);
		}
	}
}

void UEnemyCombatAIComponent::HandleAttackConsumedInternal(
	const FAttackConsumedEvent& Event)
{
	if (bAttackStartupInProgress)
	{
		if (Event.AttackInstance.IsValid()
			&& Event.AttackInstance.Attacker.Get() == GetOwner())
		{
			StartupConsumedEvent = Event;
		}
		return;
	}

	if (bAttackTerminationCommitted
		|| !ActiveAttackInstance.IsValid()
		|| !(Event.AttackInstance == ActiveAttackInstance))
	{
		return;
	}

	RecordConsumedAttack(Event.AttackInstance);
	const bool bPerfectParry = Event.Reason == EAttackConsumeReason::PerfectParry;
	TerminateActiveAttack(
		Event.AttackInstance,
		true,
		bPerfectParry ? EEnemyAIState::Staggered : EEnemyAIState::Recovering,
		bPerfectParry ? StaggerRecoveryTime : PostAttackRecoveryTime,
		true,
		true);
}

void UEnemyCombatAIComponent::RecordConsumedAttack(const FAttackInstanceId& AttackInstance)
{
	RecordBoundedAttackResult(ConsumedAttackHistory, AttackInstance);
}

void UEnemyCombatAIComponent::RecordCompletedAttack(const FAttackInstanceId& AttackInstance)
{
	RecordBoundedAttackResult(CompletedAttackHistory, AttackInstance);
}

void UEnemyCombatAIComponent::RecordBoundedAttackResult(
	TArray<FAttackInstanceId>& ResultHistory,
	const FAttackInstanceId& AttackInstance)
{
	if (!AttackInstance.IsValid())
	{
		return;
	}

	ResultHistory.RemoveSingle(AttackInstance);
	ResultHistory.Add(AttackInstance);
	if (ResultHistory.Num() > AttackResultHistoryCapacity)
	{
		ResultHistory.RemoveAt(
			0,
			ResultHistory.Num() - AttackResultHistoryCapacity,
			EAllowShrinking::No);
	}
}

bool UEnemyCombatAIComponent::TerminateActiveAttack(
	const FAttackInstanceId ExpectedAttack,
	const bool bInterrupted,
	const EEnemyAIState TerminalState,
	const float RecoveryDuration,
	const bool bStopActiveMontage,
	const bool bExecutionSucceeded)
{
	if (bAttackTerminationCommitted
		|| !ExpectedAttack.IsValid()
		|| !(ActiveAttackInstance == ExpectedAttack))
	{
		return false;
	}

	const TWeakObjectPtr<UEnemyCombatAIComponent> ComponentSnapshot(this);
	const TWeakObjectPtr<UCombatComponent> CombatSnapshot = AttackConsumptionSource;
	const TWeakObjectPtr<UAnimInstance> AnimInstanceSnapshot = ActiveAttackAnimInstance;
	const TWeakObjectPtr<UAnimMontage> MontageSnapshot = ActiveAttackMontage;
	AActor* const OwnerActor = GetOwner();
	const FString OwnerName = OwnerActor ? OwnerActor->GetName() : TEXT("None");
	TWeakObjectPtr<UTargetingComponent> TargetingSnapshot;
	if (const ABaseCombatCharacter* OwnerCharacter = Cast<ABaseCombatCharacter>(OwnerActor))
	{
		TargetingSnapshot = OwnerCharacter->GetTargetingComponent();
	}

	const EEnemyAIState PreviousState = CurrentState;
	const uint64 TerminationAttempt = ++AttackStartupAttempt;
	bAttackTerminationCommitted = true;
	bAttackStartupInProgress = false;
	StartupConsumedEvent.Reset();
	LastTerminatedAttackInstance = ExpectedAttack;
	if (bExecutionSucceeded && !WasAttackInstanceConsumed(ExpectedAttack))
	{
		RecordCompletedAttack(ExpectedAttack);
	}
	ActiveAttackInstance = {};
	CurrentState = TerminalState;
	UnbindAttackConsumption();
	UnbindAttackMontageEnd();
	const auto HasExpectedTerminationOwnership = [this, TerminationAttempt, TerminalState]()
		{
			return AttackStartupAttempt == TerminationAttempt
				&& bAttackTerminationCommitted
				&& !ActiveAttackInstance.IsValid()
				&& CurrentState == TerminalState;
		};

	if (UCombatComponent* Combat = CombatSnapshot.Get())
	{
		Combat->AbortActiveAttack(ExpectedAttack);
	}
#if WITH_AUTOMATION_TESTS
	TFunction<void()> PostCombatAbortHook = MoveTemp(PostCombatAbortHookForTesting);
	PostCombatAbortHookForTesting = {};
	if (PostCombatAbortHook)
	{
		PostCombatAbortHook();
	}
#endif
	if (!ComponentSnapshot.IsValid() || !HasExpectedTerminationOwnership())
	{
		return true;
	}
	if (UTargetingComponent* Targeting = TargetingSnapshot.Get())
	{
		Targeting->ReleaseActiveAttackWarp();
	}
	if (!ComponentSnapshot.IsValid() || !HasExpectedTerminationOwnership())
	{
		return true;
	}
	if (bStopActiveMontage)
	{
		if (UAnimInstance* ActiveAnimInstance = AnimInstanceSnapshot.Get())
		{
			if (UAnimMontage* ActiveMontage = MontageSnapshot.Get())
			{
				ActiveAnimInstance->Montage_Stop(0.2f, ActiveMontage);
			}
		}
	}
	if (!ComponentSnapshot.IsValid() || !HasExpectedTerminationOwnership())
	{
		return true;
	}

	ReleaseTokenAndCleanup();
	if (!ComponentSnapshot.IsValid() || !HasExpectedTerminationOwnership())
	{
		return true;
	}
	if (UWorld* World = GetWorld();
		CurrentState == TerminalState
		&& RecoveryDuration >= 0.0f
		&& TerminalState != EEnemyAIState::Dying)
	{
		World->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			this,
			&UEnemyCombatAIComponent::OnRecoveryComplete,
			FMath::Max(0.0f, RecoveryDuration),
			false);
	}

	if (PreviousState != TerminalState && CurrentState == TerminalState)
	{
		UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: State %s -> %s"),
			*OwnerName,
			*UEnum::GetValueAsString(PreviousState),
			*UEnum::GetValueAsString(TerminalState));
		OnAIStateChanged.Broadcast(PreviousState, TerminalState);
		if (!ComponentSnapshot.IsValid())
		{
			return true;
		}
	}

#if WITH_AUTOMATION_TESTS
	TFunction<void()> PostAttackEndedHook = MoveTemp(PostAttackEndedHookForTesting);
	PostAttackEndedHookForTesting = {};
	++AttackEndBroadcastCountForTesting;
#endif
	OnAttackEnded.Broadcast(bInterrupted);
#if WITH_AUTOMATION_TESTS
	if (ComponentSnapshot.IsValid() && PostAttackEndedHook)
	{
		PostAttackEndedHook();
	}
#endif
	return true;
}

bool UEnemyCombatAIComponent::TerminatePendingAttack(
	const EEnemyAIState TerminalState,
	const float RecoveryDuration)
{
	const bool bOwnsPendingAttack = !ActiveAttackInstance.IsValid()
		&& (CurrentState == EEnemyAIState::Approaching
			|| (CurrentState == EEnemyAIState::Attacking && bAttackStartupInProgress));
	if (!bOwnsPendingAttack)
	{
		return false;
	}

	const TWeakObjectPtr<UEnemyCombatAIComponent> ComponentSnapshot(this);
	TWeakObjectPtr<UTargetingComponent> TargetingSnapshot;
	if (const ABaseCombatCharacter* OwnerCharacter = Cast<ABaseCombatCharacter>(GetOwner()))
	{
		TargetingSnapshot = OwnerCharacter->GetTargetingComponent();
	}

	bAttackTerminationCommitted = true;
	const uint64 TerminationAttempt = ++AttackStartupAttempt;
	bAttackStartupInProgress = false;
	StartupConsumedEvent.Reset();
	UnbindAttackConsumption();
	UnbindAttackMontageEnd();
	if (UTargetingComponent* Targeting = TargetingSnapshot.Get())
	{
		Targeting->ReleaseActiveAttackWarp();
	}
	if (!ComponentSnapshot.IsValid()
		|| AttackStartupAttempt != TerminationAttempt)
	{
		return true;
	}

	const EEnemyAIState StateBeforeTokenRelease = CurrentState;
	ReleaseTokenAndCleanup();
	if (!ComponentSnapshot.IsValid()
		|| AttackStartupAttempt != TerminationAttempt
		|| CurrentState == EEnemyAIState::Dying
		|| CurrentState != StateBeforeTokenRelease
		|| ActiveAttackInstance.IsValid()
		|| HasAttackToken()
		|| bWaitingForTokenGrant)
	{
		return true;
	}
	if (UWorld* World = GetWorld(); RecoveryDuration >= 0.0f)
	{
		World->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			this,
			&UEnemyCombatAIComponent::OnRecoveryComplete,
			FMath::Max(0.0f, RecoveryDuration),
			false);
	}
	SetState(TerminalState);
	return true;
}

bool UEnemyCombatAIComponent::AcquireDefenseChainSuppression(
	const FDefenseInteractionId& InteractionId)
{
	if (!InteractionId.IsValid())
	{
		return false;
	}

	const bool bWasSuppressed = IsDefenseChainSuppressed();
	DefenseChainSuppressions.Add(InteractionId);
	if (!bWasSuppressed)
	{
		ReleaseTokenAndCleanup();
	}
	return true;
}

bool UEnemyCombatAIComponent::ReleaseDefenseChainSuppression(
	const FDefenseInteractionId& InteractionId)
{
	return InteractionId.IsValid()
		&& DefenseChainSuppressions.Remove(InteractionId) > 0;
}

void UEnemyCombatAIComponent::UnbindAttackConsumption()
{
	if (!AttackConsumedDelegateHandle.IsValid())
	{
		AttackConsumptionSource.Reset();
		return;
	}

	const FDelegateHandle DelegateHandle = AttackConsumedDelegateHandle;
	const TWeakObjectPtr<UCombatComponent> Source = AttackConsumptionSource;
	AttackConsumedDelegateHandle.Reset();
	AttackConsumptionSource.Reset();
	if (UCombatComponent* Combat = Source.Get())
	{
		Combat->OnAttackConsumedInternal.Remove(DelegateHandle);
	}
}

void UEnemyCombatAIComponent::UnbindAttackMontageEnd()
{
	const TWeakObjectPtr<UAnimInstance> AnimInstanceSnapshot = ActiveAttackAnimInstance;
	const TWeakObjectPtr<UAnimMontage> MontageSnapshot = ActiveAttackMontage;
	ActiveAttackAnimInstance.Reset();
	ActiveAttackMontage.Reset();
	if (UAnimInstance* AnimInstance = AnimInstanceSnapshot.Get())
	{
		if (UAnimMontage* Montage = MontageSnapshot.Get())
		{
			FOnMontageEnded EmptyDelegate;
			AnimInstance->Montage_SetEndDelegate(EmptyDelegate, Montage);
		}
	}
}

void UEnemyCombatAIComponent::ReleaseTokenAndReturnToReadyState()
{
	const TWeakObjectPtr<UEnemyCombatAIComponent> ComponentSnapshot(this);
	const uint64 CleanupAttempt = AttackStartupAttempt;
	const EEnemyAIState StateBeforeTokenRelease = CurrentState;
	ReleaseTokenAndCleanup();
	if (!ComponentSnapshot.IsValid()
		|| AttackStartupAttempt != CleanupAttempt
		|| CurrentState != StateBeforeTokenRelease
		|| ActiveAttackInstance.IsValid()
		|| HasAttackToken()
		|| bWaitingForTokenGrant)
	{
		return;
	}
	ReturnToReadyState();
}

void UEnemyCombatAIComponent::ReturnToReadyState()
{
	if (CurrentState == EEnemyAIState::Dying)
	{
		return;
	}

	SetState(CombatTarget.IsValid() ? EEnemyAIState::Circling : EEnemyAIState::Idle);
}

void UEnemyCombatAIComponent::OnRecoveryComplete()
{
	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Recovery complete"), *GetOwner()->GetName());

	ReturnToReadyState();
}

void UEnemyCombatAIComponent::HandleTokenGranted(AActor* Attacker)
{
	// Only react if this is us getting the token from queue
	AActor* const OwnerActor = GetOwner();
	if (!OwnerActor || Attacker != OwnerActor)
	{
		return;
	}
	if (IsDefenseChainSuppressed())
	{
		ReleaseTokenAndReturnToReadyState();
		return;
	}

	if (!bWaitingForTokenGrant)
	{
		return;
	}

	AActor* const TargetActor = CombatTarget.Get();
	UAttackData* const AttackData = SelectedAttack.Get();
	UCombatTokenSubsystem* const GrantTokenSubsystem = TokenSubsystem.Get();
	const TWeakObjectPtr<UEnemyCombatAIComponent> ComponentSnapshot(this);
	const TWeakObjectPtr<AActor> OwnerSnapshot(OwnerActor);
	const TWeakObjectPtr<AActor> TargetSnapshot(TargetActor);
	const TWeakObjectPtr<UAttackData> AttackDataSnapshot(AttackData);
	const TWeakObjectPtr<UCombatTokenSubsystem> TokenSubsystemSnapshot(GrantTokenSubsystem);
	const FString OwnerName = OwnerActor->GetName();
	const uint64 GrantAttempt = ++AttackStartupAttempt;
	const auto HasExpectedGrantOwnership = [this,
		OwnerSnapshot,
		TargetSnapshot,
		AttackDataSnapshot,
		TokenSubsystemSnapshot,
		GrantAttempt]()
		{
			return AttackStartupAttempt == GrantAttempt
				&& OwnerSnapshot.IsValid()
				&& GetOwner() == OwnerSnapshot.Get()
				&& TargetSnapshot.IsValid()
				&& CombatTarget.Get() == TargetSnapshot.Get()
				&& AttackDataSnapshot.IsValid()
				&& SelectedAttack.Get() == AttackDataSnapshot.Get()
				&& TokenSubsystemSnapshot.IsValid()
				&& TokenSubsystem.Get() == TokenSubsystemSnapshot.Get();
		};
	const auto ReleaseOrphanedToken = [OwnerActor, TokenSubsystemSnapshot]()
		{
			if (UCombatTokenSubsystem* SurvivingTokenSubsystem = TokenSubsystemSnapshot.Get())
			{
				SurvivingTokenSubsystem->ReleaseAttackToken(OwnerActor);
			}
		};
	const auto RollbackCurrentGrant = [this, ComponentSnapshot, GrantAttempt]()
		{
			if (!ComponentSnapshot.IsValid() || AttackStartupAttempt != GrantAttempt)
			{
				return;
			}
			if (CurrentState == EEnemyAIState::Idle
				|| CurrentState == EEnemyAIState::Circling
				|| CurrentState == EEnemyAIState::Approaching)
			{
				ReleaseTokenAndReturnToReadyState();
			}
			else
			{
				ReleaseTokenAndCleanup();
			}
		};

	bWaitingForTokenGrant = false;
	if (!HasExpectedGrantOwnership() || !HasAttackToken())
	{
		RollbackCurrentGrant();
		return;
	}

	// We were in queue and just got a token
	if (CurrentState == EEnemyAIState::Circling)
	{
		SetState(EEnemyAIState::Approaching);
#if WITH_AUTOMATION_TESTS
		TFunction<void()> PostApproachStateTransitionHook = MoveTemp(PostApproachStateTransitionHookForTesting);
		PostApproachStateTransitionHookForTesting = {};
		if (PostApproachStateTransitionHook)
		{
			PostApproachStateTransitionHook();
		}
#endif
		if (!ComponentSnapshot.IsValid())
		{
			ReleaseOrphanedToken();
			return;
		}
		if (!HasExpectedGrantOwnership()
			|| CurrentState != EEnemyAIState::Approaching
			|| !HasAttackToken())
		{
			RollbackCurrentGrant();
			return;
		}

		UWorld* const World = GetWorld();
		if (!World)
		{
			RollbackCurrentGrant();
			return;
		}
		ApproachStartTime = World->GetTimeSeconds();
#if WITH_AUTOMATION_TESTS
		++TokenGrantBroadcastCountForTesting;
#endif
		OnTokenGranted.Broadcast();
		if (!ComponentSnapshot.IsValid())
		{
			ReleaseOrphanedToken();
			return;
		}
		if (!HasExpectedGrantOwnership()
			|| CurrentState != EEnemyAIState::Approaching
			|| !HasAttackToken())
		{
			RollbackCurrentGrant();
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Token granted from queue, approaching"),
			*OwnerName);
	}
	else
	{
		ReleaseTokenAndReturnToReadyState();
	}
}

void UEnemyCombatAIComponent::OnAttackMontageEnded(
	UAnimMontage* Montage,
	const bool bInterrupted,
	const FAttackInstanceId ExpectedAttack)
{
	if (CurrentState != EEnemyAIState::Attacking
		|| bAttackTerminationCommitted
		|| !ExpectedAttack.IsValid()
		|| !(ActiveAttackInstance == ExpectedAttack)
		|| Montage != ActiveAttackMontage.Get())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Attack montage ended (interrupted: %s)"),
		*GetOwner()->GetName(), bInterrupted ? TEXT("YES") : TEXT("NO"));

	TerminateActiveAttack(
		ExpectedAttack,
		bInterrupted,
		EEnemyAIState::Recovering,
		PostAttackRecoveryTime,
		false,
		!bInterrupted);
}

void UEnemyCombatAIComponent::HandleOwnerDying(AActor* Killer)
{
	OnDeath();
}

void UEnemyCombatAIComponent::BindOwnerDeathEvents()
{
	if (ABaseCombatCharacter* OwnerCharacter = Cast<ABaseCombatCharacter>(GetOwner()))
	{
		OwnerCharacter->OnCharacterDying.AddUniqueDynamic(this, &UEnemyCombatAIComponent::HandleOwnerDying);
		OwnerCharacter->OnCharacterDeath.AddUniqueDynamic(this, &UEnemyCombatAIComponent::HandleOwnerDying);
	}
}

void UEnemyCombatAIComponent::ScheduleCirclingDirectionChange()
{
	if (UWorld* World = GetWorld())
	{
		float Interval = CirclingConfig.DirectionChangeInterval;
		float Variance = CirclingConfig.DirectionChangeVariance;
		float RandomInterval = Interval + FMath::FRandRange(-Variance, Variance);

		World->GetTimerManager().SetTimer(
			CirclingDirectionTimerHandle,
			[this]()
			{
				RandomizeCirclingDirection();
				ScheduleCirclingDirectionChange();
			},
			RandomInterval,
			false);
	}
}

void UEnemyCombatAIComponent::SetTokenSubsystem(UCombatTokenSubsystem* InTokenSubsystem)
{
	if (TokenSubsystem == InTokenSubsystem)
	{
		return;
	}

	if (TokenSubsystem)
	{
		TokenSubsystem->OnTokenGranted.RemoveDynamic(this, &UEnemyCombatAIComponent::HandleTokenGranted);
	}

	bWaitingForTokenGrant = false;
	TokenSubsystem = InTokenSubsystem;

	if (TokenSubsystem)
	{
		TokenSubsystem->OnTokenGranted.AddUniqueDynamic(this, &UEnemyCombatAIComponent::HandleTokenGranted);
	}
}
