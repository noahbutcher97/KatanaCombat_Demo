// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/PairedAnimationComponent.h"
#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Core/HitReactionComponent.h"
#include "Characters/BaseCombatCharacter.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/DamageableInterface.h"
#include "Interfaces/TeamMemberInterface.h"
#include "Data/PairedAnimationData.h"
#include "Data/AttackData.h"
#include "Data/CombatFXData.h"
#include "Data/TargetingSettings.h"
#include "Debug/DebugConfig.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "Utilities/PairedAnimationUtilityLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraSystem.h"
#include "Engine/OverlapResult.h"

// ============================================================================
// LOG CATEGORY DEFINITION
// ============================================================================

DEFINE_LOG_CATEGORY(LogPairedAnim);

// ============================================================================
// CONSTRUCTION
// ============================================================================

UPairedAnimationComponent::UPairedAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void UPairedAnimationComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedOwnerCharacter = Cast<ABaseCombatCharacter>(GetOwner());
	if (CachedOwnerCharacter)
	{
		CachedCombatComponent = CachedOwnerCharacter->FindComponentByClass<UCombatComponent>();
	}
}

void UPairedAnimationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Cancel any active paired animation (clears partners, restores time dilation, stops warp tracking)
	if (IsPairedAnimationActive())
	{
		CancelPairedAnimation(0.0f);  // Immediate cancel, no blend
	}

	// Clear any remaining paired partners
	ClearPairedPartners();

	// Clear timer handles
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SlowMotionRestoreHandle);
		GetWorld()->GetTimerManager().ClearTimer(ChainTimeoutHandle);
	}

	// Reset state
	ActivePairedAnimData = nullptr;
	CurrentFinisherVictim.Reset();
	bBlockCombatInput = false;
	bCompletingPairedAnimation = false;
	bCounterWindowActive = false;
	bParryWindowActive = false;
	ClearChainContext();
	CounterWindowData.Reset();

	Super::EndPlay(EndPlayReason);
}

// ============================================================================
// CONFIGURATION / CACHED REFERENCES
// ============================================================================

ABaseCombatCharacter* UPairedAnimationComponent::GetOwnerCharacter() const
{
	return CachedOwnerCharacter;
}

bool UPairedAnimationComponent::GetDebugDraw() const
{
	return CombatDebug::IsPairedAnimDebugEnabled();
}

// ============================================================================
// FINISHER EXECUTION
// ============================================================================

bool UPairedAnimationComponent::TryExecuteFinisher(UAttackData* AttackData)
{
	// Validate attack has finisher data
	if (!AttackData || !AttackData->FinisherData)
	{
		return false;
	}

	// Get owner character
	ABaseCombatCharacter* AttackerCharacter = GetOwnerCharacter();
	if (!AttackerCharacter)
	{
		return false;
	}

	// Get targeting component to find current target
	UTargetingComponent* TargetingComp = AttackerCharacter->GetTargetingComponent();
	if (!TargetingComp)
	{
		return false;
	}

	// Get current target - try hard-lock first, then fall back to soft-aim
	AActor* TargetActor = TargetingComp->GetCurrentTarget();
	if (!TargetActor)
	{
		// No hard-locked target - try soft-aim to find nearest enemy in facing direction
		const FVector FacingDirection = AttackerCharacter->GetActorForwardVector();
		TargetingComp->FindBestTargetForDirection(
			FacingDirection,
			TargetActor,
			-1.0f, -1.0f, -1.0f, -1.0f, -1.0f
		);

		if (!TargetActor)
		{
			return false;
		}

		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[FINISHER] No hard-lock, using soft-aim target: %s"),
				*TargetActor->GetName());
		}
	}

	// ========================================================================
	// FINISHER DISTANCE VALIDATION (Gap 16.2)
	// ========================================================================
	const float DistanceToTarget = FVector::Dist(
		AttackerCharacter->GetActorLocation(),
		TargetActor->GetActorLocation()
	);

	float MaxFinisherRange = 500.0f;  // Fallback value
	if (const UTargetingSettings* TargetingSettings = TargetingComp->GetEffectiveSettings())
	{
		MaxFinisherRange = TargetingSettings->SoftAimRange;
	}

	if (DistanceToTarget > MaxFinisherRange)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[FINISHER] Target %s too far: %.1f > %.1f (max range)"),
				*TargetActor->GetName(), DistanceToTarget, MaxFinisherRange);
		}
		return false;
	}

	// ========================================================================
	// GAP 19.6 FIX: Validate path is clear before executing finisher
	// ========================================================================
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(GetOwner());
	ActorsToIgnore.Add(TargetActor);

	const float PathClearanceRadius = 30.0f;
	if (!UPairedAnimationUtilityLibrary::IsPathClear(
		GetWorld(),
		AttackerCharacter->GetActorLocation(),
		TargetActor->GetActorLocation(),
		PathClearanceRadius,
		ActorsToIgnore))
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[FINISHER] Path to target %s is blocked by obstacle"),
				*TargetActor->GetName());
		}
		return false;
	}

	// Get target's hit reaction component
	UHitReactionComponent* TargetHitReaction = TargetActor->FindComponentByClass<UHitReactionComponent>();
	if (!TargetHitReaction)
	{
		return false;
	}

	// Check if target is vulnerable to finisher
	if (!TargetHitReaction->IsVulnerableToFinisher())
	{
		return false;
	}

	// Get finisher trigger reason for logging/context
	EFinisherTriggerReason TriggerReason = TargetHitReaction->GetFinisherTriggerReason();

	// Always log finisher execution for diagnostics (this is a major combat event)
	{
		ABaseCombatCharacter* TargetCombatChar = Cast<ABaseCombatCharacter>(TargetActor);
		UE_LOG(LogPairedAnim, Warning, TEXT("[FINISHER] EXECUTING on %s — Reason: %s, Health: %.1f/%.1f, Stunned: %s, Staggered: %s, IsDying: %s"),
			*TargetActor->GetName(),
			*UEnum::GetValueAsString(TriggerReason),
			TargetCombatChar ? TargetCombatChar->CurrentHealth : -1.0f,
			TargetCombatChar ? TargetCombatChar->MaxHealth : -1.0f,
			TargetHitReaction->IsStunned() ? TEXT("YES") : TEXT("NO"),
			TargetHitReaction->IsStaggered() ? TEXT("YES") : TEXT("NO"),
			TargetCombatChar ? (TargetCombatChar->IsDeadOrDying() ? TEXT("YES") : TEXT("NO")) : TEXT("N/A"));
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[FINISHER] Executing finisher: %s"), *AttackData->FinisherData->GetDisplayName());
		UE_LOG(LogPairedAnim, Log, TEXT("[FINISHER] Target: %s"), *TargetActor->GetName());
		UE_LOG(LogPairedAnim, Log, TEXT("[FINISHER] Trigger Reason: %s"), *UEnum::GetValueAsString(TriggerReason));
	}

	return TryStartPairedAnimationWithTarget(TargetActor, AttackData->FinisherData, EPairedReactionType::Finisher);
}

bool UPairedAnimationComponent::TryStartPairedAnimationWithTarget(AActor* TargetActor, UPairedAnimationData* PairedAnimData, EPairedReactionType ReactionType)
{
	if (!TargetActor || !PairedAnimData)
	{
		return false;
	}

	ABaseCombatCharacter* AttackerCharacter = GetOwnerCharacter();
	if (!AttackerCharacter)
	{
		return false;
	}

	UTargetingComponent* TargetingComp = AttackerCharacter->GetTargetingComponent();
	if (!TargetingComp)
	{
		return false;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(GetOwner());
	ActorsToIgnore.Add(TargetActor);

	const float PathClearanceRadius = 30.0f;
	if (!UPairedAnimationUtilityLibrary::IsPathClear(
		GetWorld(),
		AttackerCharacter->GetActorLocation(),
		TargetActor->GetActorLocation(),
		PathClearanceRadius,
		ActorsToIgnore))
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED START] Path to target %s is blocked by obstacle"),
				*TargetActor->GetName());
		}
		return false;
	}

	UHitReactionComponent* TargetHitReaction = TargetActor->FindComponentByClass<UHitReactionComponent>();
	if (!TargetHitReaction)
	{
		return false;
	}

	bool bAttackerMontageSuccess = false;
	bool bVictimMontageSuccess = false;

	UPairedAnimationComponent* TargetPairedComp = TargetActor->FindComponentByClass<UPairedAnimationComponent>();
	const bool bTreatAsLethal = ShouldTreatPairedAnimationAsLethal(ReactionType, PairedAnimData);
	if (ReactionType == EPairedReactionType::Counter && PairedAnimData->bIsLethal && !bTreatAsLethal)
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-CHAIN] Counter paired data is authored lethal but runtime policy treats counter steps as nonlethal"));
	}

	TargetHitReaction->EnterPairedAnimationState(
		PairedAnimData->VictimMontage,
		PairedAnimData->VictimDeathOutcome,
		PairedAnimData->RagdollBlendTime,
		bTreatAsLethal,
		GetOwner());

	CurrentFinisherVictim = TargetActor;

	AddPairedPartner(TargetActor);
	if (TargetPairedComp)
	{
		TargetPairedComp->AddPairedPartner(GetOwner());
	}

	BeginPairedAnimation(PairedAnimData, ReactionType, true);

	ACharacter* AttackerChar = Cast<ACharacter>(AttackerCharacter);
	if (AttackerChar && PairedAnimData->AttackerMontage)
	{
		UAnimInstance* AttackerAnimInstance = AttackerChar->GetMesh() ? AttackerChar->GetMesh()->GetAnimInstance() : nullptr;
		if (AttackerAnimInstance)
		{
			const float MontageLength = AttackerAnimInstance->Montage_Play(
				PairedAnimData->AttackerMontage,
				1.0f,
				EMontagePlayReturnType::MontageLength,
				0.0f,
				true
			);

			bAttackerMontageSuccess = (MontageLength > 0.0f);

			if (bAttackerMontageSuccess)
			{
				if (!PairedAnimData->AttackerMontageSection.IsNone())
				{
					AttackerAnimInstance->Montage_JumpToSection(
						PairedAnimData->AttackerMontageSection,
						PairedAnimData->AttackerMontage
					);
					AttackerAnimInstance->Montage_SetNextSection(
						PairedAnimData->AttackerMontageSection,
						NAME_None,
						PairedAnimData->AttackerMontage
					);
				}

				TargetingComp->SetupAttackerPairedWarp(TargetActor, PairedAnimData->AttackerWarpConfig);

				if (CachedCombatComponent)
				{
					CachedCombatComponent->SetPhase(EAttackPhase::Active);
				}

				if (GetDebugDraw())
				{
					FString SectionInfo = PairedAnimData->AttackerMontageSection.IsNone()
						? TEXT("(full)")
						: *PairedAnimData->AttackerMontageSection.ToString();
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED START] Attacker montage playing: %s Section: %s"),
						*PairedAnimData->AttackerMontage->GetName(), *SectionInfo);
				}
			}
		}
	}

	ACharacter* VictimChar = Cast<ACharacter>(TargetActor);
	if (VictimChar && PairedAnimData->VictimMontage)
	{
		UAnimInstance* VictimAnimInstance = VictimChar->GetMesh() ? VictimChar->GetMesh()->GetAnimInstance() : nullptr;
		if (VictimAnimInstance)
		{
			const float StartPosition = FMath::Max(0.0f, -PairedAnimData->VictimStartOffset);

			const float MontageLength = VictimAnimInstance->Montage_Play(
				PairedAnimData->VictimMontage,
				1.0f,
				EMontagePlayReturnType::MontageLength,
				StartPosition,
				true
			);

			bVictimMontageSuccess = (MontageLength > 0.0f);

			if (bVictimMontageSuccess)
			{
				if (!PairedAnimData->VictimMontageSection.IsNone())
				{
					VictimAnimInstance->Montage_JumpToSection(
						PairedAnimData->VictimMontageSection,
						PairedAnimData->VictimMontage
					);
					VictimAnimInstance->Montage_SetNextSection(
						PairedAnimData->VictimMontageSection,
						NAME_None,
						PairedAnimData->VictimMontage
					);
				}

				if (UTargetingComponent* VictimTargeting = TargetActor->FindComponentByClass<UTargetingComponent>())
				{
					VictimTargeting->SetupVictimWarp(GetOwner(), PairedAnimData->VictimWarpConfig);
				}

				if (GetDebugDraw())
				{
					FString SectionInfo = PairedAnimData->VictimMontageSection.IsNone()
						? TEXT("(full)")
						: *PairedAnimData->VictimMontageSection.ToString();
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED START] Victim montage playing: %s Section: %s (StartPos: %.2f)"),
						*PairedAnimData->VictimMontage->GetName(), *SectionInfo, StartPosition);
				}
			}
		}
	}

	if (!bAttackerMontageSuccess || !bVictimMontageSuccess)
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED START] Execution failed for %s - rolling back (Attacker: %s, Victim: %s)"),
			*PairedAnimData->GetDisplayName(),
			bAttackerMontageSuccess ? TEXT("OK") : TEXT("FAILED"),
			bVictimMontageSuccess ? TEXT("OK") : TEXT("FAILED"));

		TargetHitReaction->ExitPairedAnimationState();
		CurrentFinisherVictim.Reset();

		ClearPairedPartners();
		if (TargetPairedComp)
		{
			TargetPairedComp->ClearPairedPartners();
		}

		EndPairedAnimation();

		if (bAttackerMontageSuccess && AttackerChar && AttackerChar->GetMesh())
		{
			if (UAnimInstance* AnimInst = AttackerChar->GetMesh()->GetAnimInstance())
			{
				AnimInst->Montage_Stop(0.1f);
			}
		}
		if (bVictimMontageSuccess && VictimChar && VictimChar->GetMesh())
		{
			if (UAnimInstance* AnimInst = VictimChar->GetMesh()->GetAnimInstance())
			{
				AnimInst->Montage_Stop(0.1f);
			}
		}

		TargetingComp->ClearAttackerPairedWarp();
		if (UTargetingComponent* VictimTargeting = TargetActor->FindComponentByClass<UTargetingComponent>())
		{
			VictimTargeting->ClearVictimWarp();
		}

		return false;
	}

	return true;
}

bool UPairedAnimationComponent::ShouldTreatPairedAnimationAsLethal(
	EPairedReactionType ReactionType,
	const UPairedAnimationData* PairedAnimData) const
{
	if (!PairedAnimData)
	{
		return false;
	}

	if (ReactionType == EPairedReactionType::Counter && !bAllowLethalCounterPairedData)
	{
		return false;
	}

	return PairedAnimData->bIsLethal;
}

// ============================================================================
// COUNTER WINDOW STATE
// ============================================================================

void UPairedAnimationComponent::SetCounterWindowData(EAttackType InAttackType, ESwingDirection InSwingDirection,
                                             UPairedAnimationData* InCounterData, float InWindowDuration)
{
	bCounterWindowActive = true;

	CounterWindowData.Attacker = GetOwner();
	CounterWindowData.AttackType = InAttackType;
	CounterWindowData.SwingDirection = InSwingDirection;
	CounterWindowData.SpecificCounterData = InCounterData;
	CounterWindowData.TimeInWindow = 0.0f;
	CounterWindowData.WindowDuration = InWindowDuration;

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER] Counter window opened: Type=%s, Swing=%s, Duration=%.2f"),
			*UEnum::GetValueAsString(InAttackType),
			*UEnum::GetValueAsString(InSwingDirection),
			InWindowDuration);
	}
}

void UPairedAnimationComponent::ClearCounterWindowData()
{
	if (bCounterWindowActive && GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER] Counter window closed"));
	}

	bCounterWindowActive = false;
	CounterWindowData.Reset();
}

void UPairedAnimationComponent::SetParryWindowActive(bool bActive)
{
	if (bParryWindowActive == bActive)
	{
		return;
	}

	bParryWindowActive = bActive;

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PARRY] Parry window %s on %s"),
			bActive ? TEXT("OPENED") : TEXT("CLOSED"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("None"));
	}
}

// ============================================================================
// COUNTER SYSTEM API
// ============================================================================

bool UPairedAnimationComponent::TryCounter()
{
	if (!CanCounter())
	{
		return false;
	}

	const bool bUseChainParryTarget = CounterMode == ECounterSystemMode::Chain;
	AActor* Target = bUseChainParryTarget ? FindParryableEnemy() : FindCounterableEnemy();
	if (!Target)
	{
		UE_LOG(LogPairedAnim, Verbose, TEXT("[COUNTER] TryCounter failed: No %s enemy found"),
			bUseChainParryTarget ? TEXT("parryable") : TEXT("counterable"));
		return false;
	}

	FCounterContext Context = bUseChainParryTarget
		? GetEnemyParryContext(Target)
		: GetEnemyCounterContext(Target);
	if (!Context.Attacker)
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER] TryCounter failed: Invalid counter context"));
		return false;
	}

	switch (CounterMode)
	{
	case ECounterSystemMode::AC3:
		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER] Executing AC3 counter-kill against %s"), *Target->GetName());
		return TryCounter_AC3Mode(Context);

	case ECounterSystemMode::Chain:
		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER] Executing Chain parry against %s"), *Target->GetName());
		return TryCounter_ChainMode(Context);

	default:
		return false;
	}
}

bool UPairedAnimationComponent::CanCounter() const
{
	// Must be in a state that allows countering
	ECombatState State = ICombatInterface::Execute_GetCombatState(GetOwner());
	if (State != ECombatState::Idle && State != ECombatState::Blocking)
	{
		return false;
	}

	// Must not be in a paired animation
	if (bCompletingPairedAnimation || bBlockCombatInput)
	{
		return false;
	}

	// Chain mode: Check chain state allows countering
	if (CounterMode == ECounterSystemMode::Chain)
	{
		if (ChainState != EChainCounterState::None)
		{
			return false;
		}
		return FindParryableEnemy() != nullptr;
	}

	// AC3 mode: Must have a counterable enemy nearby
	return FindCounterableEnemy() != nullptr;
}

AActor* UPairedAnimationComponent::FindCounterableEnemy() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	UTargetingComponent* Targeting = Owner->FindComponentByClass<UTargetingComponent>();
	float SearchRange = 400.0f;
	if (Targeting)
	{
		if (const UTargetingSettings* Settings = Targeting->GetEffectiveSettings())
		{
			SearchRange = Settings->SoftAimRange;
		}
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);

	Owner->GetWorld()->OverlapMultiByChannel(
		Overlaps,
		Owner->GetActorLocation(),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(SearchRange),
		QueryParams
	);

	AActor* BestTarget = nullptr;
	float BestDistance = FLT_MAX;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OtherActor = Overlap.GetActor();
		if (!OtherActor)
		{
			continue;
		}

		// Check if this is an enemy (different team)
		if (OtherActor->Implements<UTeamMemberInterface>() && Owner->Implements<UTeamMemberInterface>())
		{
			ETeamId OtherTeam = ITeamMemberInterface::Execute_GetTeamId(OtherActor);
			ETeamId MyTeam = ITeamMemberInterface::Execute_GetTeamId(Owner);
			if (OtherTeam == MyTeam)
			{
				continue;
			}
		}

		// Check if enemy has an active counter window. PairedAnimationComponent owns
		// the state after CP-2; CombatComponent fallback keeps older delegates safe.
		const UPairedAnimationComponent* EnemyPaired = OtherActor->FindComponentByClass<UPairedAnimationComponent>();
		const UCombatComponent* EnemyCombat = OtherActor->FindComponentByClass<UCombatComponent>();
		const bool bEnemyInCounterWindow = EnemyPaired
			? EnemyPaired->IsInCounterWindow()
			: (EnemyCombat && EnemyCombat->IsInCounterWindow());
		if (!bEnemyInCounterWindow)
		{
			continue;
		}

		float Distance = FVector::Dist(Owner->GetActorLocation(), OtherActor->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestTarget = OtherActor;
		}
	}

	return BestTarget;
}

FCounterContext UPairedAnimationComponent::GetEnemyCounterContext(AActor* Enemy) const
{
	FCounterContext Context;

	if (!Enemy)
	{
		return Context;
	}

	if (const UPairedAnimationComponent* EnemyPaired = Enemy->FindComponentByClass<UPairedAnimationComponent>())
	{
		if (!EnemyPaired->IsInCounterWindow())
		{
			return Context;
		}

		Context = EnemyPaired->GetCounterWindowData();
		return Context;
	}

	const UCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UCombatComponent>();
	if (!EnemyCombat || !EnemyCombat->IsInCounterWindow())
	{
		return Context;
	}

	Context = EnemyCombat->GetCounterWindowData();
	return Context;
}

FCounterContext UPairedAnimationComponent::GetEnemyParryContext(AActor* Enemy) const
{
	FCounterContext Context;

	if (!Enemy)
	{
		return Context;
	}

	const UPairedAnimationComponent* EnemyPaired = Enemy->FindComponentByClass<UPairedAnimationComponent>();
	const UCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UCombatComponent>();
	const bool bEnemyInParryWindow = EnemyPaired
		? EnemyPaired->IsInParryWindow()
		: (EnemyCombat && EnemyCombat->IsInParryWindow());
	if (!bEnemyInParryWindow)
	{
		return Context;
	}

	Context.Attacker = Enemy;

	if (const UAttackData* CurrentAttack = EnemyCombat ? EnemyCombat->GetCurrentAttack() : nullptr)
	{
		Context.AttackType = CurrentAttack->AttackType;
	}

	return Context;
}

AActor* UPairedAnimationComponent::FindParryableEnemy() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	UTargetingComponent* Targeting = Owner->FindComponentByClass<UTargetingComponent>();
	float SearchRange = 400.0f;
	if (Targeting)
	{
		if (const UTargetingSettings* Settings = Targeting->GetEffectiveSettings())
		{
			SearchRange = Settings->SoftAimRange;
		}
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);

	Owner->GetWorld()->OverlapMultiByChannel(
		Overlaps,
		Owner->GetActorLocation(),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(SearchRange),
		QueryParams
	);

	AActor* BestTarget = nullptr;
	float BestDistance = FLT_MAX;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OtherActor = Overlap.GetActor();
		if (!OtherActor)
		{
			continue;
		}

		if (OtherActor->Implements<UTeamMemberInterface>() && Owner->Implements<UTeamMemberInterface>())
		{
			ETeamId OtherTeam = ITeamMemberInterface::Execute_GetTeamId(OtherActor);
			ETeamId MyTeam = ITeamMemberInterface::Execute_GetTeamId(Owner);
			if (OtherTeam == MyTeam)
			{
				continue;
			}
		}

		// Check if enemy has an active PARRY window (not counter window).
		const UPairedAnimationComponent* EnemyPaired = OtherActor->FindComponentByClass<UPairedAnimationComponent>();
		const UCombatComponent* EnemyCombat = OtherActor->FindComponentByClass<UCombatComponent>();
		const bool bEnemyInParryWindow = EnemyPaired
			? EnemyPaired->IsInParryWindow()
			: (EnemyCombat && EnemyCombat->IsInParryWindow());
		if (!bEnemyInParryWindow)
		{
			continue;
		}

		float Distance = FVector::Dist(Owner->GetActorLocation(), OtherActor->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestTarget = OtherActor;
		}
	}

	return BestTarget;
}

// ============================================================================
// COUNTER SYSTEM IMPLEMENTATIONS
// ============================================================================

bool UPairedAnimationComponent::TryCounter_AC3Mode(const FCounterContext& Context)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Context.Attacker)
	{
		return false;
	}

	// If counter data is specified on the notify, use that animation
	if (Context.SpecificCounterData)
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER-AC3] Using specific counter animation: %s"),
			*Context.SpecificCounterData->GetName());

		if (TryStartPairedAnimationWithTarget(Context.Attacker.Get(), Context.SpecificCounterData, EPairedReactionType::Counter))
		{
			return true;
		}

		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-AC3] Specific counter animation failed to start; falling back to direct counter damage"));
	}

	// AC3 Mode fallback: instant counter-kill via slow-mo and direct lethal damage.
	UWorld* World = GetWorld();
	if (World)
	{
		UCinematicEffectsUtilityLibrary::ApplySlowMotion(World, 0.2f);
	}

	// No specific counter data — stagger enemy and apply lethal damage directly
	if (ABaseCombatCharacter* EnemyChar = Cast<ABaseCombatCharacter>(Context.Attacker.Get()))
	{
		if (UHitReactionComponent* EnemyHitReact = EnemyChar->FindComponentByClass<UHitReactionComponent>())
		{
			EnemyHitReact->ApplyStagger(2.0f);
		}

		// Apply lethal damage — direction is FROM attacker TO victim (hit travels toward enemy)
		FHitReactionInfo HitInfo;
		HitInfo.Attacker = Owner;
		HitInfo.HitDirection = (Context.Attacker->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
		HitInfo.Damage = IDamageableInterface::Execute_GetCurrentHealth(Context.Attacker.Get()) + 1.0f;
		HitInfo.bWasCounter = true;
		HitInfo.PhaseWhenHit = EAttackPhase::Active;
		HitInfo.ImpactPoint = Context.Attacker->GetActorLocation();

		IDamageableInterface::Execute_ApplyDamage(Context.Attacker.Get(), HitInfo);

		// Restore time dilation after direct counter-kill (no paired animation to manage it)
		if (World)
		{
			UCinematicEffectsUtilityLibrary::RestoreTimeDilation(World);
		}

		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER-AC3] Counter-kill applied to %s"), *Context.Attacker->GetName());
		return true;
	}

	// Failed to find valid target — restore time dilation
	if (World)
	{
		UCinematicEffectsUtilityLibrary::RestoreTimeDilation(World);
	}
	return false;
}

bool UPairedAnimationComponent::TryCounter_ChainMode(const FCounterContext& Context)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Context.Attacker)
	{
		return false;
	}

	ActiveChainContext = Context;
	ActiveChainTarget = Context.Attacker;
	ActiveChainAttackData = nullptr;
	bContinueChainAfterCounterPairedAnimation = false;

	// Chain Mode Step 1: Parry
	// TODO: When parry animations are available, play parry montage here and wait for
	// montage completion before transitioning to CounterWindow. Currently transitions
	// immediately since no parry animation exists yet.
	ChainState = EChainCounterState::ParryActive;

	// Notify the enemy that their attack was parried
	if (Context.Attacker->Implements<UDamageableInterface>())
	{
		IDamageableInterface::Execute_OnAttackParried(Context.Attacker.Get(), Owner);
	}

	// Stagger the enemy briefly
	if (ABaseCombatCharacter* EnemyChar = Cast<ABaseCombatCharacter>(Context.Attacker.Get()))
	{
		if (UHitReactionComponent* EnemyHitReact = EnemyChar->FindComponentByClass<UHitReactionComponent>())
		{
			EnemyHitReact->ApplyStagger(2.0f);
		}
	}

	// Apply slow-mo for cinematic feel
	if (UWorld* World = GetWorld())
	{
		UCinematicEffectsUtilityLibrary::ApplySlowMotion(World, 0.3f);
	}

	// Transition to CounterWindow state
	ChainState = EChainCounterState::CounterWindow;

	// Set timeout: player has 2s to press attack for the counter
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ChainTimeoutHandle,
			this,
			&UPairedAnimationComponent::OnChainTimeout,
			2.0f,
			false
		);
	}

	UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER-CHAIN] Parry successful! Player is now in Countering state. Press attack to continue chain."));
	return true;
}

bool UPairedAnimationComponent::TryAdvanceChainCounter(UAttackData* SelectedAttackData)
{
	if (ChainState != EChainCounterState::CounterWindow)
	{
		return false;
	}

	if (!SelectedAttackData)
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-CHAIN] Cannot advance: selected attack data is null"));
		return false;
	}

	return ExecuteChainCounterAttack(SelectedAttackData);
}

bool UPairedAnimationComponent::ExecuteChainCounterAttack(UAttackData* ChainAttackData)
{
	if (ChainState != EChainCounterState::CounterWindow)
	{
		return false;
	}

	if (!ChainAttackData)
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-CHAIN] Cannot execute counter attack: selected attack data is null"));
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChainTimeoutHandle);
	}

	ActiveChainAttackData = ChainAttackData;
	ChainState = EChainCounterState::CounterActive;

	UPairedAnimationData* CounterPairedData = ChainAttackData->CounterData;
	if (!CounterPairedData && bAllowNotifyCounterDataFallback)
	{
		CounterPairedData = ActiveChainContext.SpecificCounterData;
	}

	if (CounterPairedData && ActiveChainTarget.IsValid())
	{
		bContinueChainAfterCounterPairedAnimation = true;
		if (TryStartPairedAnimationWithTarget(ActiveChainTarget.Get(), CounterPairedData, EPairedReactionType::Counter))
		{
			return true;
		}

		bContinueChainAfterCounterPairedAnimation = false;
		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-CHAIN] Authored counter paired animation failed to start; continuing to finisher readiness"));
	}

	ChainState = EChainCounterState::FinisherReady;

	UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER-CHAIN] Counter attack executed. Finisher is ready."));

	ExecuteChainFinisher();
	return true;
}

bool UPairedAnimationComponent::ExecuteChainFinisher()
{
	if (ChainState != EChainCounterState::FinisherReady)
	{
		return false;
	}

	UAttackData* ChainAttack = ActiveChainAttackData ? ActiveChainAttackData.Get() : nullptr;

	bool bSuccess = false;
	if (ChainAttack && ChainAttack->FinisherData && ActiveChainTarget.IsValid())
	{
		bSuccess = TryStartPairedAnimationWithTarget(ActiveChainTarget.Get(), ChainAttack->FinisherData, EPairedReactionType::Finisher);
	}
	else
	{
		bSuccess = TryExecuteFinisher(ChainAttack);
	}

	ClearChainContext();

	if (bSuccess)
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER-CHAIN] Chain finisher executed successfully!"));
	}
	else
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[COUNTER-CHAIN] Chain finisher failed - no valid target or animation"));
	}

	return bSuccess;
}

void UPairedAnimationComponent::CancelChainCounter()
{
	if (ChainState == EChainCounterState::None)
	{
		return;
	}

	EChainCounterState PrevState = ChainState;
	ClearChainContext();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChainTimeoutHandle);
	}

	if (UWorld* World = GetWorld())
	{
		UCinematicEffectsUtilityLibrary::RestoreTimeDilation(World);
	}

	UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER-CHAIN] Chain cancelled from state %s"),
		*UEnum::GetValueAsString(PrevState));
}

void UPairedAnimationComponent::ClearChainContext()
{
	ChainState = EChainCounterState::None;
	ActiveChainContext.Reset();
	ActiveChainTarget.Reset();
	ActiveChainAttackData = nullptr;
	bContinueChainAfterCounterPairedAnimation = false;
}

void UPairedAnimationComponent::OnChainTimeout()
{
	UE_LOG(LogPairedAnim, Log, TEXT("[COUNTER-CHAIN] Chain timed out!"));
	CancelChainCounter();
}

// ============================================================================
// PAIRED ANIMATION PARTNER TRACKING
// ============================================================================

void UPairedAnimationComponent::AddPairedPartner(AActor* Partner)
{
	if (!Partner)
	{
		return;
	}

	for (const TWeakObjectPtr<AActor>& Existing : PairedAnimationPartners)
	{
		if (Existing.Get() == Partner)
		{
			return;
		}
	}

	PairedAnimationPartners.Add(Partner);

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED] Added partner: %s (Total: %d)"),
			*Partner->GetName(), PairedAnimationPartners.Num());
	}
}

void UPairedAnimationComponent::RemovePairedPartner(AActor* Partner)
{
	if (!Partner)
	{
		return;
	}

	for (int32 i = PairedAnimationPartners.Num() - 1; i >= 0; --i)
	{
		if (PairedAnimationPartners[i].Get() == Partner)
		{
			PairedAnimationPartners.RemoveAt(i);

			if (GetDebugDraw())
			{
				UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED] Removed partner: %s (Remaining: %d)"),
					*Partner->GetName(), PairedAnimationPartners.Num());
			}
			return;
		}
	}
}

void UPairedAnimationComponent::ClearPairedPartners()
{
	const int32 Count = PairedAnimationPartners.Num();
	PairedAnimationPartners.Empty();

	if (GetDebugDraw() && Count > 0)
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED] Cleared all partners (was %d)"), Count);
	}
}

bool UPairedAnimationComponent::IsPairedPartner(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& Partner : PairedAnimationPartners)
	{
		if (Partner.Get() == Actor)
		{
			return true;
		}
	}

	return false;
}

// ============================================================================
// PAIRED ANIMATION EFFECT HANDLING
// ============================================================================

void UPairedAnimationComponent::BeginPairedAnimation(UPairedAnimationData* PairedAnimData, EPairedReactionType ReactionType, bool bIsCriticalMoment)
{
	if (!PairedAnimData)
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED EFFECTS] BeginPairedAnimation called with null PairedAnimData"));
		return;
	}

	ActivePairedAnimData = PairedAnimData;
	ActivePairedReactionType = ReactionType;
	bBlockCombatInput = true;

	if (bIsCriticalMoment && PairedAnimData->bApplySlowMotion)
	{
		UCinematicEffectsUtilityLibrary::ApplySlowMotion(GetWorld(), PairedAnimData->SlowMotionScale);

		if (UWorld* World = GetWorld())
		{
			if (SlowMotionRestoreHandle.IsValid())
			{
				World->GetTimerManager().ClearTimer(SlowMotionRestoreHandle);
			}

			World->GetTimerManager().SetTimer(
				SlowMotionRestoreHandle,
				this,
				&UPairedAnimationComponent::OnSlowMotionTimerExpired,
				PairedAnimData->SlowMotionDuration,
				false
			);

			if (GetDebugDraw())
			{
				UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Slow motion applied: Scale=%.2f, Duration=%.2fs"),
					PairedAnimData->SlowMotionScale, PairedAnimData->SlowMotionDuration);
			}
		}
	}

	OnPairedAnimationStarted.Broadcast(ReactionType, bIsCriticalMoment);

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Started paired animation: %s (Type: %d, Critical: %d, SlowMo: %d)"),
			*PairedAnimData->GetDisplayName(),
			static_cast<int32>(ReactionType),
			bIsCriticalMoment,
			PairedAnimData->bApplySlowMotion);
	}
}

void UPairedAnimationComponent::EndPairedAnimation()
{
	const EPairedReactionType ReactionType = ActivePairedReactionType;

	if (SlowMotionRestoreHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SlowMotionRestoreHandle);
		}
	}

	UCinematicEffectsUtilityLibrary::RestoreTimeDilation(GetWorld());

	ActivePairedAnimData = nullptr;
	ActivePairedReactionType = EPairedReactionType::None;
	bBlockCombatInput = false;

	// BUG-1 FIX: Explicit movement restoration
	if (ABaseCombatCharacter* Character = GetOwnerCharacter())
	{
		if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
		{
			if (MovementComp->MovementMode == MOVE_None)
			{
				MovementComp->SetMovementMode(MOVE_Walking);

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Restored movement mode (was MOVE_None)"));
				}
			}
		}
		bMovementCurrentlyDisabled = false;
	}

	OnPairedAnimationEnded.Broadcast(ReactionType);

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Ended paired animation (Type: %d)"),
			static_cast<int32>(ReactionType));
	}
}

void UPairedAnimationComponent::TriggerSyncPointEffects(FName SyncPointName)
{
	// Play camera shake if configured
	if (ActivePairedAnimData && ActivePairedAnimData->ImpactCameraShake)
	{
		UCinematicEffectsUtilityLibrary::PlayCameraShakeOnActor(GetOwner(), ActivePairedAnimData->ImpactCameraShake);

		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Camera shake played: %s"),
				*ActivePairedAnimData->ImpactCameraShake->GetName());
		}
	}

	if (!ActivePairedAnimData)
	{
		OnPairedAnimationSyncPoint.Broadcast(ActivePairedReactionType, SyncPointName);
		return;
	}

	AActor* Owner = GetOwner();
	AActor* Partner = PairedAnimationPartners.Num() > 0
		? PairedAnimationPartners[0].Get()
		: nullptr;

	// Calculate contact point for VFX (midpoint between attacker and victim)
	FVector ContactPoint = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	FVector ImpactNormal = FVector::UpVector;
	if (Owner && Partner)
	{
		ContactPoint = (Owner->GetActorLocation() + Partner->GetActorLocation()) * 0.5f;
		ImpactNormal = (Partner->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
		if (ImpactNormal.IsNearlyZero())
		{
			ImpactNormal = FVector::UpVector;
		}
	}

	// ================================================================
	// PAIRED ANIMATION AUDIO
	// ================================================================
	if (ActivePairedAnimData->ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(), ActivePairedAnimData->ImpactSound,
			ContactPoint, FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f,
			nullptr, nullptr, Owner);

		UE_LOG(LogCombatFX, Verbose, TEXT("[PAIRED FX] Impact sound: %s at %s"),
			*ActivePairedAnimData->ImpactSound->GetName(), *ContactPoint.ToString());
	}

	if (ActivePairedAnimData->VictimReactionSound && Partner)
	{
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(), ActivePairedAnimData->VictimReactionSound,
			Partner->GetActorLocation(), FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f,
			nullptr, nullptr, Partner);

		UE_LOG(LogCombatFX, Verbose, TEXT("[PAIRED FX] Victim reaction sound: %s"),
			*ActivePairedAnimData->VictimReactionSound->GetName());
	}

	if (ActivePairedAnimData->AttackerVoiceLine && Owner)
	{
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(), ActivePairedAnimData->AttackerVoiceLine,
			Owner->GetActorLocation(), FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f,
			nullptr, nullptr, Owner);

		UE_LOG(LogCombatFX, Verbose, TEXT("[PAIRED FX] Attacker voice line: %s"),
			*ActivePairedAnimData->AttackerVoiceLine->GetName());
	}

	// ================================================================
	// PAIRED ANIMATION VFX
	// ================================================================
	if (ActivePairedAnimData->ImpactVFX)
	{
		FImpactVFXConfig VFXConfig;
		VFXConfig.ImpactVFX = ActivePairedAnimData->ImpactVFX;
		VFXConfig.ScaleMultiplier = 1.0f;
		VFXConfig.bAlignToSurface = true;
		VFXConfig.bUseWeaponFallback = false;

		UCinematicEffectsUtilityLibrary::SpawnImpactVFX(
			GetWorld(),
			VFXConfig,
			nullptr,
			ContactPoint,
			ImpactNormal,
			NAME_None);

		UE_LOG(LogCombatFX, Verbose, TEXT("[PAIRED FX] Impact VFX: %s at %s"),
			*ActivePairedAnimData->ImpactVFX->GetName(), *ContactPoint.ToString());
	}

	OnPairedAnimationSyncPoint.Broadcast(ActivePairedReactionType, SyncPointName);

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Sync point: %s (Type: %d, Audio: %s/%s/%s, VFX: %s)"),
			*SyncPointName.ToString(),
			static_cast<int32>(ActivePairedReactionType),
			ActivePairedAnimData->ImpactSound ? TEXT("Impact") : TEXT("-"),
			ActivePairedAnimData->VictimReactionSound ? TEXT("Victim") : TEXT("-"),
			ActivePairedAnimData->AttackerVoiceLine ? TEXT("Voice") : TEXT("-"),
			ActivePairedAnimData->ImpactVFX ? TEXT("Yes") : TEXT("No"));
	}
}

void UPairedAnimationComponent::OnSlowMotionTimerExpired()
{
	UCinematicEffectsUtilityLibrary::RestoreTimeDilation(GetWorld());

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED EFFECTS] Slow motion timer expired - time dilation restored"));
	}
}

// ============================================================================
// PAIRED ANIMATION INTERRUPT HANDLING
// ============================================================================

void UPairedAnimationComponent::OnPairedPartnerDeath(AActor* DeadPartner)
{
	if (!DeadPartner)
	{
		return;
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED INTERRUPT] Partner %s died during paired animation"),
			*DeadPartner->GetName());
	}

	if (!IsPairedPartner(DeadPartner))
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED INTERRUPT] %s was not a tracked partner, ignoring"),
				*DeadPartner->GetName());
		}
		return;
	}

	RemovePairedPartner(DeadPartner);

	if (IsPairedAnimationActive())
	{
		CancelPairedAnimation();
	}
}

void UPairedAnimationComponent::CancelPairedAnimation(float BlendOutTime)
{
	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED INTERRUPT] Cancelling paired animation (BlendOutTime: %.2fs)"),
			BlendOutTime);
	}

	// GAP 18.10 FIX: Clear victim warp tracking on all partners BEFORE clearing partners
	// GAP 18.7 FIX: Clear bIsFinisherTarget flag on all partners
	for (const TWeakObjectPtr<AActor>& PartnerRef : PairedAnimationPartners)
	{
		if (AActor* Partner = PartnerRef.Get())
		{
			if (UTargetingComponent* PartnerTargeting = Partner->FindComponentByClass<UTargetingComponent>())
			{
				PartnerTargeting->ClearVictimWarp();
				PartnerTargeting->ClearAttackerPairedWarp();

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED INTERRUPT] Cleared warp tracking on partner %s"),
						*Partner->GetName());
				}
			}

			if (UHitReactionComponent* PartnerHitReaction = Partner->FindComponentByClass<UHitReactionComponent>())
			{
				PartnerHitReaction->ExitPairedAnimationState();

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED INTERRUPT] Exited paired animation state on %s"),
						*Partner->GetName());
				}
			}
		}
	}

	// Stop any playing montage on the owner
	if (AActor* Owner = GetOwner())
	{
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			if (Character->GetMesh())
			if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
			{
				AnimInstance->Montage_Stop(BlendOutTime);

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED INTERRUPT] Montage stopped on %s"),
						*Owner->GetName());
				}
			}
		}
	}

	CurrentFinisherVictim.Reset();
	bCompletingPairedAnimation = false;
	ClearPairedPartners();
	EndPairedAnimation();

	// Reset to idle phase via CombatComponent
	if (CachedCombatComponent)
	{
		CachedCombatComponent->SetPhase(EAttackPhase::None);
		CachedCombatComponent->ClearQueue(false);
	}

	if (ChainState != EChainCounterState::None)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ChainTimeoutHandle);
			UCinematicEffectsUtilityLibrary::RestoreTimeDilation(World);
		}
		ClearChainContext();
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED INTERRUPT] Paired animation cancelled - state reset"));
	}
}

void UPairedAnimationComponent::CompletePairedAnimation()
{
	// GUARD: PREVENT DOUBLE EXECUTION (Gap 20.4)
	if (bCompletingPairedAnimation)
	{
		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED COMPLETE] Already completing - ignoring duplicate call"));
		}
		return;
	}
	bCompletingPairedAnimation = true;
	const bool bShouldContinueChainAfterCounter =
		bContinueChainAfterCounterPairedAnimation &&
		ChainState == EChainCounterState::CounterActive &&
		ActivePairedReactionType == EPairedReactionType::Counter;
	const bool bTreatAsLethal = ShouldTreatPairedAnimationAsLethal(ActivePairedReactionType, ActivePairedAnimData);

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] Completing paired animation successfully"));
	}

	// ========================================================================
	// APPLY FINISHER DAMAGE TO VICTIM
	// ========================================================================
	AActor* Victim = CurrentFinisherVictim.Get();
	if (Victim && ActivePairedAnimData)
	{
		const float FinalDamage = ActivePairedAnimData->BaseDamage * ActivePairedAnimData->DamageMultiplier;

		if (GetDebugDraw())
		{
			UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] Applying damage to %s: %.1f (Base: %.1f x Mult: %.2f, Lethal: %s)"),
				*Victim->GetName(),
				FinalDamage,
				ActivePairedAnimData->BaseDamage,
				ActivePairedAnimData->DamageMultiplier,
				bTreatAsLethal ? TEXT("YES") : TEXT("NO"));
		}

		if (Victim->Implements<UDamageableInterface>())
		{
			FHitReactionInfo HitInfo;
			HitInfo.Attacker = GetOwner();
			HitInfo.HitDirection = (Victim->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
			HitInfo.AttackData = nullptr;
			HitInfo.ImpactPoint = Victim->GetActorLocation();
			HitInfo.bWasCounter = (ActivePairedReactionType == EPairedReactionType::Counter);
			HitInfo.StunDuration = 0.0f;

			if (bTreatAsLethal)
			{
				const float MaxHealth = IDamageableInterface::Execute_GetMaxHealth(Victim);
				const float CurrentHealth = IDamageableInterface::Execute_GetCurrentHealth(Victim);
				HitInfo.Damage = FMath::Max(FinalDamage, CurrentHealth + 1.0f);

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] LETHAL finisher: Applying %.1f damage (victim has %.1f/%.1f health)"),
						HitInfo.Damage, CurrentHealth, MaxHealth);
				}
			}
			else
			{
				HitInfo.Damage = FinalDamage;
			}

			const float ActualDamage = IDamageableInterface::Execute_ApplyDamage(Victim, HitInfo);

			if (GetDebugDraw())
			{
				UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] Damage applied: %.1f actual (%.1f requested)"),
					ActualDamage, HitInfo.Damage);
			}
		}
		else
		{
			UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED COMPLETE] Victim %s does not implement IDamageableInterface - no damage applied"),
				*Victim->GetName());
		}
	}
	else if (GetDebugDraw())
	{
		if (!Victim)
		{
			UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED COMPLETE] No victim tracked - cannot apply damage"));
		}
		if (!ActivePairedAnimData)
		{
			UE_LOG(LogPairedAnim, Warning, TEXT("[PAIRED COMPLETE] No ActivePairedAnimData - cannot apply damage"));
		}
	}

	// ========================================================================
	// CLEANUP STATE
	// ========================================================================

	// Clear victim's finisher target flag and warp tracking
	for (const TWeakObjectPtr<AActor>& PartnerRef : PairedAnimationPartners)
	{
		if (AActor* Partner = PartnerRef.Get())
		{
			if (UTargetingComponent* PartnerTargeting = Partner->FindComponentByClass<UTargetingComponent>())
			{
				PartnerTargeting->ClearVictimWarp();
				PartnerTargeting->ClearAttackerPairedWarp();

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] Cleared warp tracking on partner %s"),
						*Partner->GetName());
				}
			}

			if (UHitReactionComponent* PartnerHitReaction = Partner->FindComponentByClass<UHitReactionComponent>())
			{
				PartnerHitReaction->ExitPairedAnimationState();

				if (GetDebugDraw())
				{
					UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] Exited paired animation state on %s"),
						*Partner->GetName());
				}
			}
		}
	}

	// Clear our own warp tracking
	if (ABaseCombatCharacter* Character = GetOwnerCharacter())
	{
		if (UTargetingComponent* TargetingComp = Character->GetTargetingComponent())
		{
			TargetingComp->ClearAttackerPairedWarp();
		}
	}

	CurrentFinisherVictim.Reset();
	ClearPairedPartners();
	EndPairedAnimation();

	// Reset to idle phase via CombatComponent
	if (CachedCombatComponent)
	{
		CachedCombatComponent->SetPhase(EAttackPhase::None);
		CachedCombatComponent->ClearQueue(false);
	}

	// Clear guard flag now that completion is finished
	bCompletingPairedAnimation = false;

	if (bShouldContinueChainAfterCounter)
	{
		bContinueChainAfterCounterPairedAnimation = false;
		ChainState = EChainCounterState::FinisherReady;
		ExecuteChainFinisher();
		return;
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogPairedAnim, Log, TEXT("[PAIRED COMPLETE] Paired animation completed - state reset"));
	}
}
