// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/EnemyCombatAIComponent.h"
#include "AI/CombatTokenSubsystem.h"
#include "Characters/BaseCombatCharacter.h"
#include "Data/AttackData.h"
#include "Interfaces/CombatInterface.h"
#include "Core/CombatComponent.h"
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

	Super::EndPlay(EndPlayReason);
}

void UEnemyCombatAIComponent::SetTokenSubsystemForTesting(UCombatTokenSubsystem* InTokenSubsystem)
{
	SetTokenSubsystem(InTokenSubsystem);
}

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

	// Select which attack we'll use
	SelectedAttack = SelectAttack();
	if (!SelectedAttack)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: No attack available"), *GetOwner()->GetName());
		return false;
	}

	// Request attack token
	bWaitingForTokenGrant = false;
	bool bTokenGranted = TokenSubsystem->RequestAttackToken(GetOwner());

	if (bTokenGranted)
	{
		// Got token immediately - start approaching
		SetState(EEnemyAIState::Approaching);
		ApproachStartTime = GetWorld()->GetTimeSeconds();
		OnTokenGranted.Broadcast();

		UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Token granted, approaching with %s"),
			*GetOwner()->GetName(), *SelectedAttack->GetName());
		return true;
	}
	else
	{
		bWaitingForTokenGrant = TokenSubsystem->IsInTokenQueue(GetOwner());
		if (!bWaitingForTokenGrant)
		{
			SelectedAttack = nullptr;
		}

		UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Token %s"),
			*GetOwner()->GetName(),
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

bool UEnemyCombatAIComponent::ExecuteAttack()
{
	if (CurrentState != EEnemyAIState::Approaching)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: Cannot execute attack - not in Approaching state"),
			*GetOwner()->GetName());
		return false;
	}

	if (!SelectedAttack || !SelectedAttack->AttackMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: Cannot execute attack - no valid attack data"),
			*GetOwner()->GetName());
		ReleaseTokenAndCleanup();
		ReturnToReadyState();
		return false;
	}

	// Get anim instance
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: Owner is not a character"),
			*GetOwner()->GetName());
		ReleaseTokenAndCleanup();
		ReturnToReadyState();
		return false;
	}

	UAnimInstance* AnimInstance = OwnerChar->GetMesh() ? OwnerChar->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: No anim instance"),
			*GetOwner()->GetName());
		ReleaseTokenAndCleanup();
		ReturnToReadyState();
		return false;
	}

	UCombatComponent* CombatComponent = OwnerChar->FindComponentByClass<UCombatComponent>();
	if (!CombatComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: Cannot execute attack - no CombatComponent"),
			*GetOwner()->GetName());
		ReleaseTokenAndCleanup();
		ReturnToReadyState();
		return false;
	}

	// Transition to attacking state
	SetState(EEnemyAIState::Attacking);

	const EInputType AttackInputType = SelectedAttack->AttackType == EAttackType::Heavy
		? EInputType::HeavyAttack
		: EInputType::LightAttack;
	CombatComponent->SetAttackIntentTarget(CombatTarget.Get());
	if (!CombatComponent->ExecuteAttackData(SelectedAttack, CombatTarget.Get(), AttackInputType))
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] %s: Failed to execute attack through CombatComponent"),
			*GetOwner()->GetName());
		ReleaseTokenAndCleanup();
		ReturnToReadyState();
		return false;
	}

	// Bind to montage end
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UEnemyCombatAIComponent::OnAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, SelectedAttack->AttackMontage);

	// Broadcast attack started
	OnAttackStarted.Broadcast(SelectedAttack);

	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Executing attack %s"),
		*GetOwner()->GetName(), *SelectedAttack->GetName());

	return true;
}

void UEnemyCombatAIComponent::OnCountered()
{
	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Countered by player"), *GetOwner()->GetName());

	// Stop current montage
	if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		if (UAnimInstance* AnimInstance = OwnerChar->GetMesh() ? OwnerChar->GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInstance->StopAllMontages(0.2f);
		}
	}

	// Release token
	ReleaseTokenAndCleanup();

	// In AC3 mode, counter typically kills the enemy
	// In Chain mode, counter deals damage and staggers
	// For now, transition to staggered state
	SetState(EEnemyAIState::Staggered);

	// Start stagger recovery timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			this,
			&UEnemyCombatAIComponent::OnRecoveryComplete,
			StaggerRecoveryTime,
			false);
	}

	OnAttackEnded.Broadcast(true);
}

void UEnemyCombatAIComponent::OnParried()
{
	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Parried by player"), *GetOwner()->GetName());

	// Stop current montage
	if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		if (UAnimInstance* AnimInstance = OwnerChar->GetMesh() ? OwnerChar->GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInstance->StopAllMontages(0.2f);
		}
	}

	// Release token
	ReleaseTokenAndCleanup();

	// Transition to staggered
	SetState(EEnemyAIState::Staggered);

	// Start stagger recovery timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			this,
			&UEnemyCombatAIComponent::OnRecoveryComplete,
			StaggerRecoveryTime,
			false);
	}

	OnAttackEnded.Broadcast(true);
}

void UEnemyCombatAIComponent::OnDamaged()
{
	// If we're attacking, this interrupts us
	if (CurrentState == EEnemyAIState::Attacking || CurrentState == EEnemyAIState::Approaching)
	{
		UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Damaged during attack, interrupting"), *GetOwner()->GetName());

		// Stop montage
		if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
		{
			if (UAnimInstance* AnimInstance = OwnerChar->GetMesh() ? OwnerChar->GetMesh()->GetAnimInstance() : nullptr)
			{
				AnimInstance->StopAllMontages(0.2f);
			}
		}

		// Release token
		ReleaseTokenAndCleanup();

		// Transition to staggered
		SetState(EEnemyAIState::Staggered);

		// Start recovery timer
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				RecoveryTimerHandle,
				this,
				&UEnemyCombatAIComponent::OnRecoveryComplete,
				StaggerRecoveryTime,
				false);
		}

		OnAttackEnded.Broadcast(true);
	}
}

void UEnemyCombatAIComponent::OnDeath()
{
	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Died"), *GetOwner()->GetName());

	// Release token
	ReleaseTokenAndCleanup();

	// Transition to dying
	SetState(EEnemyAIState::Dying);

	// Clear all timers
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
		World->GetTimerManager().ClearTimer(CirclingDirectionTimerHandle);
	}
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
			ReleaseTokenAndCleanup();
		}

		if (CurrentState == EEnemyAIState::Circling || CurrentState == EEnemyAIState::Approaching)
		{
			SetState(EEnemyAIState::Idle);
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

	if (TokenSubsystem)
	{
		if (TokenSubsystem->HasAttackToken(GetOwner()))
		{
			TokenSubsystem->ReleaseAttackToken(GetOwner());
		}
		else if (TokenSubsystem->IsInTokenQueue(GetOwner()))
		{
			TokenSubsystem->RemoveFromQueue(GetOwner());
		}
	}

	SelectedAttack = nullptr;
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
	if (Attacker != GetOwner())
	{
		return;
	}

	if (!bWaitingForTokenGrant)
	{
		return;
	}

	bWaitingForTokenGrant = false;

	// We were in queue and just got a token
	if (CurrentState == EEnemyAIState::Circling && CombatTarget.IsValid())
	{
		SetState(EEnemyAIState::Approaching);
		ApproachStartTime = GetWorld()->GetTimeSeconds();
		OnTokenGranted.Broadcast();

		UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Token granted from queue, approaching"),
			*GetOwner()->GetName());
	}
	else
	{
		ReleaseTokenAndCleanup();
		ReturnToReadyState();
	}
}

void UEnemyCombatAIComponent::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (CurrentState != EEnemyAIState::Attacking)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s: Attack montage ended (interrupted: %s)"),
		*GetOwner()->GetName(), bInterrupted ? TEXT("YES") : TEXT("NO"));

	// Release token
	ReleaseTokenAndCleanup();

	// Transition to recovery
	SetState(EEnemyAIState::Recovering);

	// Start recovery timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			this,
			&UEnemyCombatAIComponent::OnRecoveryComplete,
			PostAttackRecoveryTime,
			false);
	}

	OnAttackEnded.Broadcast(bInterrupted);
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
