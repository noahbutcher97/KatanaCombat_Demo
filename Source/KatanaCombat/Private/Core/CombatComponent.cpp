// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "HAL/PlatformTime.h"
#include "Templates/Atomic.h"
#include "Core/WeaponComponent.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/DamageableInterface.h"
#include "Interfaces/TeamMemberInterface.h"
#include "Data/AttackData.h"
#include "Data/AttackConfiguration.h"
#include "Data/CombatSettings.h"
#include "Data/DefenseConfiguration.h"
#include "Data/PairedAnimationData.h"
#include "Data/TargetingSettings.h"
#include "Data/CombatFXData.h"
#include "NiagaraSystem.h"
#include "Debug/DebugConfig.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Characters/BaseCombatCharacter.h"
#include "Core/TargetingComponent.h"
#include "Core/HitReactionComponent.h"
#include "Defense/DefenseResolver.h"
#include "Defense/DefensePresentationSelector.h"
#include "Utilities/MontageUtilityLibrary.h"
#include "Utilities/CombatGameplayTags.h"
#include "Utilities/CombatUtils.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "Utilities/ProceduralAnimationLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Utilities/PairedAnimationUtilityLibrary.h"
#include "Debug/DebugUtils.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

namespace
{
TAtomic<uint64> GNextCombatantStableId(1);

bool IsCombatOwnerOperational(const UCombatComponent* CombatComponent)
{
	if (!IsValid(CombatComponent))
	{
		return false;
	}

	const ABaseCombatCharacter* Character = Cast<ABaseCombatCharacter>(CombatComponent->GetOwner());
	return IsValid(Character) && !Character->IsDeadOrDying();
}

void RefreshCombatComponentDebugTicks()
{
	if (!IsInGameThread())
	{
		return;
	}

	const bool bEnableDebugTick = CombatDebug::IsCombatStateDebugEnabled();
	for (TObjectIterator<UCombatComponent> It; It; ++It)
	{
		UCombatComponent* CombatComponent = *It;
		if (!IsValid(CombatComponent)
			|| CombatComponent->IsTemplate()
			|| !CombatComponent->IsRegistered()
			|| !CombatComponent->GetWorld())
		{
			continue;
		}

		CombatComponent->SetComponentTickEnabled(
			bEnableDebugTick && IsCombatOwnerOperational(CombatComponent));
	}
}

FAutoConsoleVariableSink GCombatComponentDebugTickSink(
	FConsoleCommandDelegate::CreateStatic(&RefreshCombatComponentDebugTicks));

bool IsAttackStartupContextValid(
	const UCombatComponent* CombatComponent,
	const ABaseCombatCharacter* Character,
	const USkeletalMeshComponent* Mesh,
	const UAnimInstance* AnimInstance,
	const UAttackData* AttackData,
	const UAnimMontage* AttackMontage)
{
	return IsCombatOwnerOperational(CombatComponent)
		&& IsValid(Character)
		&& IsValid(Mesh)
		&& IsValid(AnimInstance)
		&& IsValid(AttackData)
		&& IsValid(AttackMontage)
		&& CombatComponent->GetOwner() == Character
		&& Character->GetMesh() == Mesh
		&& Mesh->GetAnimInstance() == AnimInstance
		&& AttackData->AttackMontage.Get() == AttackMontage;
}

bool IsAttackTaggedUnblockable(const UAttackData* AttackData)
{
	if (!AttackData)
	{
		return false;
	}

	const FGameplayTag UnblockableTag = KatanaCombatGameplayTags::AttackPropertyUnblockable();
	return UnblockableTag.IsValid() && AttackData->AttackTags.HasTag(UnblockableTag);
}

void RequestDefenderThreatRefresh(AActor* Defender, EThreatRefreshReason Reason)
{
	if (UCombatComponent* DefenderCombat = Defender
		? Defender->FindComponentByClass<UCombatComponent>()
		: nullptr)
	{
		DefenderCombat->RefreshGuardThreat(Reason);
	}
}

void SelectPerfectParryPresentation(
	FDefenseResolution& Resolution,
	const UDefenseConfiguration* DefenderConfiguration,
	const UDefenseConfiguration* AttackerConfiguration)
{
	FDefensePresentationSelectionContext Context;
	Context.Outcome = Resolution.Decision.Outcome;
	Context.AttackerResponse = Resolution.Decision.AttackerResponse;
	Context.Height = Resolution.Decision.Height;
	Context.Lane = Resolution.Decision.Lane;
	Context.SwingShape = Resolution.Decision.SwingShape;
	Context.bPairedBridgeUsable = true;
	if (Resolution.Decision.SelectedAttack)
	{
		Context.AttackTags = Resolution.Decision.SelectedAttack->AttackTags;
	}

	const FTableDefensePresentationSelector Selector;
	const FDefensePresentationSelectionResult DefenderSelection =
		Selector.SelectDefender(Context, DefenderConfiguration);
	if (DefenderSelection.bFound)
	{
		Resolution.Presentation = DefenderSelection.Payload;
		Resolution.PresentationRow = DefenderSelection.RowName;
		Resolution.PresentationFallback = DefenderSelection.FallbackLevel;
	}
	if (!Resolution.Presentation.bOverrideImpactAudio)
	{
		Resolution.Presentation.bOverrideImpactAudio = true;
		Resolution.Presentation.ImpactAudio = DefenderConfiguration
			? DefenderConfiguration->DefaultParryImpactAudio
			: FImpactAudioConfig();
	}
	if (!Resolution.Presentation.bOverrideImpactVFX)
	{
		Resolution.Presentation.bOverrideImpactVFX = true;
		Resolution.Presentation.ImpactVFX = DefenderConfiguration
			? DefenderConfiguration->DefaultParryImpactVFX
			: FImpactVFXConfig();
	}
	if (!Resolution.Presentation.bOverrideHitstop
		&& Resolution.Decision.SelectedAttack
		&& Resolution.Decision.SelectedAttack->HitstopConfig.IsActive())
	{
		Resolution.Presentation.bOverrideHitstop = true;
		Resolution.Presentation.Hitstop = Resolution.Decision.SelectedAttack->HitstopConfig;
	}

	FDefensePresentationSelectionResult AttackerSelection =
		Selector.SelectAttacker(Context, AttackerConfiguration);
	const bool bSelectedMontageUsable = AttackerSelection.Payload.Montage
		&& (AttackerSelection.Payload.MontageSection.IsNone()
			|| AttackerSelection.Payload.Montage->GetSectionIndex(
				AttackerSelection.Payload.MontageSection) != INDEX_NONE);
	if (!bSelectedMontageUsable)
	{
		const FDefensePresentationSelectionResult GenericSelection =
			Selector.SelectGenericAttacker(Context, AttackerConfiguration);
		const bool bGenericMontageUsable = GenericSelection.Payload.Montage
			&& (GenericSelection.Payload.MontageSection.IsNone()
				|| GenericSelection.Payload.Montage->GetSectionIndex(
					GenericSelection.Payload.MontageSection) != INDEX_NONE);
		if (bGenericMontageUsable)
		{
			AttackerSelection = GenericSelection;
		}
	}
	if (AttackerSelection.bFound)
	{
		Resolution.AttackerPresentation = AttackerSelection.Payload;
		Resolution.AttackerPresentationRow = AttackerSelection.RowName;
		Resolution.AttackerPresentationFallback = AttackerSelection.FallbackLevel;
	}
}
}

// ============================================================================
// LOG CATEGORY DEFINITION
// ============================================================================

DEFINE_LOG_CATEGORY(LogCombat);

UCombatComponent::UCombatComponent()
{
	// Tick only needed for debug visualization - disabled by default for performance
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCombatComponent::OnRegister()
{
	Super::OnRegister();
	EnsureCombatantStableId();
}

void UCombatComponent::EnsureCombatantStableId()
{
	if (CombatantStableId.IsValid())
	{
		return;
	}

	CombatantStableId.Value = GNextCombatantStableId++;
	if (!CombatantStableId.IsValid())
	{
		CombatantStableId.Value = GNextCombatantStableId++;
	}
}

void UCombatComponent::AppendDefenseTelemetry(FDefenseTelemetryRecord Record)
{
	if (!DefenseTelemetry::IsEnabled())
	{
		return;
	}

	EnsureCombatantStableId();
	if (!Record.Defender.IsValid())
	{
		Record.Defender = GetOwner();
	}
	if (!Record.DefenderStableId.IsValid())
	{
		Record.DefenderStableId = CombatantStableId;
	}
	if (Record.Defender.IsValid() && Record.OwnerTransform.Equals(FTransform::Identity))
	{
		Record.OwnerTransform = Record.Defender->GetActorTransform();
	}
	if (Record.Attacker.IsValid())
	{
		if (!Record.AttackerStableId.IsValid())
		{
			if (const UCombatComponent* AttackerCombat =
				Record.Attacker->FindComponentByClass<UCombatComponent>())
			{
				Record.AttackerStableId = AttackerCombat->GetCombatantStableId();
			}
		}
		if (Record.CounterpartTransform.Equals(FTransform::Identity))
		{
			Record.CounterpartTransform = Record.Attacker->GetActorTransform();
		}
	}
	if (Record.SimulationTimestamp == 0.0)
	{
		Record.SimulationTimestamp = GetWorld()
			? static_cast<double>(GetWorld()->GetTimeSeconds())
			: 0.0;
	}
	if (Record.UnscaledTimestamp == 0.0)
	{
		Record.UnscaledTimestamp = FPlatformTime::Seconds();
	}
	Record.Sequence = ++NextDefenseTelemetrySequence;
	DefenseTelemetryRecords.Add(MoveTemp(Record));
	const int32 Overflow = DefenseTelemetryRecords.Num() - DefenseTelemetryCapacity;
	if (Overflow > 0)
	{
		DefenseTelemetryRecords.RemoveAt(0, Overflow, EAllowShrinking::No);
	}
}

void UCombatComponent::ClearDefenseTelemetry()
{
	DefenseTelemetryRecords.Reset();
	NextDefenseTelemetrySequence = 0;
}

void UCombatComponent::AppendActionReactionTelemetry(FActionReactionTelemetryRecord Record)
{
	if (!ActionReactionTelemetry::IsEnabled())
	{
		return;
	}

	EnsureCombatantStableId();
	if (!Record.Actor.IsValid())
	{
		Record.Actor = GetOwner();
	}
	if (!Record.ActorStableId.IsValid())
	{
		if (Record.Actor.Get() == GetOwner())
		{
			Record.ActorStableId = CombatantStableId;
		}
		else if (const UCombatComponent* ActorCombat =
			Record.Actor->FindComponentByClass<UCombatComponent>())
		{
			Record.ActorStableId = ActorCombat->GetCombatantStableId();
		}
	}
	if (Record.ActorPathSnapshot.IsEmpty() && Record.Actor.IsValid())
	{
		Record.ActorPathSnapshot = Record.Actor->GetPathName();
	}
	if (Record.Counterpart.IsValid() && !Record.CounterpartStableId.IsValid())
	{
		if (const UCombatComponent* CounterpartCombat =
			Record.Counterpart->FindComponentByClass<UCombatComponent>())
		{
			Record.CounterpartStableId = CounterpartCombat->GetCombatantStableId();
		}
	}
	if (Record.CounterpartPathSnapshot.IsEmpty() && Record.Counterpart.IsValid())
	{
		Record.CounterpartPathSnapshot = Record.Counterpart->GetPathName();
	}
	if (Record.SimulationTimestamp == 0.0)
	{
		Record.SimulationTimestamp = GetWorld()
			? static_cast<double>(GetWorld()->GetTimeSeconds())
			: 0.0;
	}
	if (Record.UnscaledTimestamp == 0.0)
	{
		Record.UnscaledTimestamp = FPlatformTime::Seconds();
	}
	ActionReactionTelemetryBuffer.Append(MoveTemp(Record));
}

void UCombatComponent::ClearActionReactionTelemetry()
{
	ActionReactionTelemetryBuffer.Reset();
	LastMovementInputDecisionReason = EActionReactionTelemetryReason::None;
}

FActionReactionTelemetryRecord UCombatComponent::BuildActionReactionTelemetryRecord(
	const EActionReactionTelemetryEvent Event,
	const EActionReactionTelemetryReason Reason) const
{
	FActionReactionTelemetryRecord Record;
	Record.Event = Event;
	Record.Reason = Reason;
	Record.Actor = GetOwner();
	Record.ActorStableId = CombatantStableId;
	Record.AttackGeneration = AttackStateMachine.AttackGeneration;
	Record.PrimaryActionGeneration = AttackStateMachine.AttackGeneration;
	Record.HoldGeneration = HoldState.CurrentHold.HoldID;
	Record.MontageInstanceId = AttackStateMachine.ActiveMontageInstanceId;
	Record.AttackPhase = CurrentPhase;
	Record.InputContext = CurrentInputContext;
	Record.QueueDepth = ActionQueue.Num();
	if (CurrentAttackData)
	{
		Record.AttackDataPath = FSoftObjectPath(CurrentAttackData);
		Record.ActionName = CurrentAttackData->GetFName();
	}
	if (UAnimMontage* ActiveMontage = AttackStateMachine.GetActiveMontage())
	{
		Record.MontagePath = FSoftObjectPath(ActiveMontage);
	}
	return Record;
}

uint64 UCombatComponent::EnsureActionQueueEntryIdentity(FActionQueueEntry& Entry)
{
	if (Entry.QueueEntryId != 0)
	{
		return Entry.QueueEntryId;
	}

	Entry.QueueEntryId = NextActionQueueEntryId++;
	if (NextActionQueueEntryId == 0)
	{
		NextActionQueueEntryId = 1;
	}
	return Entry.QueueEntryId;
}

void UCombatComponent::AppendActionQueueTelemetry(
	const FActionQueueEntry& Entry,
	const EActionReactionTelemetryEvent Event,
	const EActionReactionTelemetryReason Reason)
{
	FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(Event, Reason);
	Telemetry.InputSerial = Entry.InputAction.InputSerial;
	Telemetry.QueueEntryId = Entry.QueueEntryId;
	Telemetry.InputType = Entry.InputAction.InputType;
	Telemetry.InputEvent = Entry.InputAction.EventType;
	Telemetry.InputDirection = Entry.InputAction.FacingIntent.BranchDirection;
	Telemetry.ExecutionMode = Entry.ExecutionMode;
	Telemetry.ActionState = Entry.State;
	if (Entry.AttackData)
	{
		Telemetry.AttackDataPath = FSoftObjectPath(Entry.AttackData);
	}
	AppendActionReactionTelemetry(MoveTemp(Telemetry));
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureCombatantStableId();

	// CRITICAL: Initialize input context to Movement (default state)
	// Ensures clean state on component spawn/respawn
	CurrentInputContext = EInputContext::Movement;

	// Cache owner character (ABaseCombatCharacter for proper CombatSettings access)
	OwnerCharacter = Cast<ABaseCombatCharacter>(GetOwner());
	if (OwnerCharacter)
	{
		OwnerCharacter->OnCharacterDying.AddUniqueDynamic(
			this,
			&UCombatComponent::OnCharacterDeath);
		OwnerCharacter->OnCharacterDeath.AddUniqueDynamic(
			this,
			&UCombatComponent::OnCharacterDeath);

		// Cache combat settings from character
		CombatSettings = OwnerCharacter->CombatSettings;

		// Cache paired animation component (direct member access, more reliable than FindComponentByClass)
		CachedPairedAnimComp = OwnerCharacter->PairedAnimationComponent;

		// Bind to montage event delegates for event-driven phase transitions
		if (USkeletalMeshComponent* OwnerMesh = OwnerCharacter->GetMesh())
		if (UAnimInstance* AnimInstance = OwnerMesh->GetAnimInstance())
		{
			AnimInstance->OnMontageBlendingOut.AddDynamic(this, &UCombatComponent::OnMontageBlendingOut);
			AnimInstance->OnMontageEnded.AddDynamic(this, &UCombatComponent::OnMontageEnded);

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[INIT] Montage event delegates bound (BlendingOut, Ended)"));
			}
		}

		// Validate default attacks are assigned (critical for graceful fallback system)
		#if WITH_EDITOR
		ValidateDefaultAttacks();
		#endif
	}

	// Category CVars and the master override can independently drive this overlay.
	if (CombatDebug::IsCombatStateDebugEnabled())
	{
		SetComponentTickEnabled(true);
	}
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearGuardThreat(EThreatClearReason::ComponentEndPlay);
	DefenseStanceOverrides.Reset();
	ClearActiveContextTags();
	if (DeferredAttackConsumedTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DeferredAttackConsumedTickerHandle);
		DeferredAttackConsumedTickerHandle.Reset();
	}
	PendingAttackConsumedEvents.Reset();

	// Clear timers owned by CombatComponent
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(EaseTimerHandle);
	}

	// Unbind montage delegates to prevent callbacks to destroyed component
	if (OwnerCharacter && OwnerCharacter->GetMesh())
	{
		if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
		{
			AnimInstance->OnMontageBlendingOut.RemoveDynamic(this, &UCombatComponent::OnMontageBlendingOut);
			AnimInstance->OnMontageEnded.RemoveDynamic(this, &UCombatComponent::OnMontageEnded);
		}
	}

	if (OwnerCharacter)
	{
		OwnerCharacter->OnCharacterDying.RemoveDynamic(
			this,
			&UCombatComponent::OnCharacterDeath);
		OwnerCharacter->OnCharacterDeath.RemoveDynamic(
			this,
			&UCombatComponent::OnCharacterDeath);
	}

	// Reset combat state to prevent any lingering effects
	// (Paired animation cleanup is handled by PairedAnimationComponent::EndPlay)
	CurrentAttackData = nullptr;
	ActiveAttackStartupOperationSerial = 0;
	CurrentPhase = EAttackPhase::None;
	bIsBlocking = false;
	CachedPairedAnimComp = nullptr;
	DefenseInteractionCache.Reset();
	OpenAttackWindowRecords.Reset();
	ActiveHitWindow = {};
	ActiveParryWindow = {};
	ActiveCounterWindow = {};
	ConsumedAttackInstance = {};
	bConsumedPendingPresentation = false;

	Super::EndPlay(EndPlayReason);
}

void UCombatComponent::ValidateDefaultAttacks()
{
	// Get attack configuration through the new CombatSettings->DefaultWeaponData->AttackConfiguration path
	UAttackConfiguration* AttackConfig = CombatSettings ? CombatSettings->GetAttackConfiguration() : nullptr;

	if (!CombatSettings || !AttackConfig)
	{
		UE_LOG(LogCombat, Error, TEXT("[VALIDATION] CombatSettings or AttackConfiguration is nullptr on %s! "
		                              "Combat system cannot function. Assign CombatSettings with DefaultWeaponData (containing AttackConfiguration) in Character Blueprint."),
			*GetOwner()->GetName());

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
				FString::Printf(TEXT("⚠️ %s: Missing CombatSettings or DefaultWeaponData!"), *GetOwner()->GetName()));
		}
		return;
	}

	bool bHasErrors = false;

	// Check if default light attack is assigned
	UAttackData* DefaultLight = GetDefaultLightAttack();
	if (!DefaultLight)
	{
		UE_LOG(LogCombat, Error, TEXT("[VALIDATION] DefaultLightAttack is nullptr on %s! "
		                              "Combat system will not work properly. Assign in AttackConfiguration asset."),
			*GetOwner()->GetName());
		bHasErrors = true;
	}

	// Check if default heavy attack is assigned
	UAttackData* DefaultHeavy = GetDefaultHeavyAttack();
	if (!DefaultHeavy)
	{
		UE_LOG(LogCombat, Error, TEXT("[VALIDATION] DefaultHeavyAttack is nullptr on %s! "
		                              "Combat system will not work properly. Assign in AttackConfiguration asset."),
			*GetOwner()->GetName());
		bHasErrors = true;
	}

	// Show on-screen warning in editor PIE sessions
	if (bHasErrors && GEngine)
	{
		const FString ErrorMsg = FString::Printf(
			TEXT("⚠️ CombatComponent on %s: Default attacks not assigned! Fix in AttackConfiguration asset."),
			*GetOwner()->GetName()
		);

		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, ErrorMsg);

		// Log comprehensive fix instructions
		UE_LOG(LogCombat, Warning, TEXT("[VALIDATION] To fix this:"));
		UE_LOG(LogCombat, Warning, TEXT("  1. Open your CombatSettings asset"));
		UE_LOG(LogCombat, Warning, TEXT("  2. Open the AttackConfiguration asset"));
		UE_LOG(LogCombat, Warning, TEXT("  3. Assign DefaultLightAttack and DefaultHeavyAttack in the Details panel"));
	}
	else if (!bHasErrors && GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[VALIDATION] ✓ Default attacks validated successfully"));
	}
}

void UCombatComponent::OnCharacterDeath(AActor* Killer)
{
	(void)Killer;
	// CRITICAL: Full combat state reset on death
	// Prevents state leaks across respawns (hold state, queued actions, input context)

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Warning, TEXT("[DEATH] Character died - resetting all combat state"));
	}

	ClearGuardThreat(EThreatClearReason::OwnerDeath);
	DefenseStanceOverrides.Reset();
	if (ABaseCombatCharacter* Character = GetOwnerCharacter())
	{
		if (UTargetingComponent* Targeting = Character->GetTargetingComponent())
		{
			Targeting->ReleaseAllAlignmentRequests(EAlignmentReleaseReason::Death);
		}
	}

	// PAIRED ANIMATION INTERRUPT: Delegate partner notification and cleanup to PairedAnimComp
	if (CachedPairedAnimComp)
	{
		if (CachedPairedAnimComp->GetPairedPartnerCount() > 0)
		{
			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[DEATH] Notifying %d paired animation partners"), CachedPairedAnimComp->GetPairedPartnerCount());
			}

			// Copy array before iterating (partners will remove themselves when notified)
			TArray<TWeakObjectPtr<AActor>> PartnersCopy = CachedPairedAnimComp->PairedAnimationPartners;
			for (const TWeakObjectPtr<AActor>& PartnerPtr : PartnersCopy)
			{
				if (AActor* Partner = PartnerPtr.Get())
				{
					if (UPairedAnimationComponent* PartnerPairedComp = Partner->FindComponentByClass<UPairedAnimationComponent>())
					{
						PartnerPairedComp->OnPairedPartnerDeath(GetOwner());
					}
				}
			}
		}

		// A lethal sync can precede the victim's first collision notify. Keep its
		// partner link until paired cleanup so that notify can acquire the ignore.
		if (CachedPairedAnimComp->IsExpectedPairedVictimDeath())
		{
			// The retained sequence owns completion or cancellation of this death.
		}
		else if (CachedPairedAnimComp->GetChainState() != EChainCounterState::None
			|| CachedPairedAnimComp->IsPairedAnimationActive())
		{
			CachedPairedAnimComp->CancelPairedAnimation(0.0f);
		}
		else
		{
			CachedPairedAnimComp->ClearPairedPartners();
		}
	}

	// Clear action queue and statistics
	ClearQueue(true);

	// Clear hold state (ease timer, flags, playrate)
	ClearHoldState();

	// Reset directional input buffer
	DirectionalInputBuffer.Reset();

	// Clear held inputs
	HeldInputs.Empty();
	HeldInputSerials.Empty();
	bIsBlocking = false;

	// Reset to idle phase
	SetPhase(EAttackPhase::None);

	// Clear checkpoints
	Checkpoints.Empty();

	// Reset input context to Movement (default)
	SetInputContext(EInputContext::Movement);

	// Death is a terminal teardown boundary for every runtime context owner.
	ClearActiveContextTags();
	VisitedAttacks.Empty();

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[DEATH] ✓ Combat state reset complete - ready for respawn"));
	}
}

void UCombatComponent::SetInputContext(EInputContext NewContext)
{
	if (CurrentInputContext != NewContext)
	{
		const EInputContext OldContext = CurrentInputContext;
		if (CombatDebug::IsHoldDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[INPUT CONTEXT] %s → %s"),
				*UEnum::GetValueAsString(OldContext),
				*UEnum::GetValueAsString(NewContext));
		}

		CurrentInputContext = NewContext;
		FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(
			EActionReactionTelemetryEvent::InputContextChanged,
			EActionReactionTelemetryReason::None);
		Telemetry.InputContext = NewContext;
		Telemetry.Detail = FString::Printf(
			TEXT("%s -> %s"),
			*UEnum::GetValueAsString(OldContext),
			*UEnum::GetValueAsString(NewContext));
		AppendActionReactionTelemetry(MoveTemp(Telemetry));
	}
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Combat flow remains event-driven; guard resume uses an unscaled core ticker.
	// - Input processing: OnInputEvent (immediate)
	// - Queue execution: ProcessQueuedActions (called on phase transitions)
	// - Hold easing: OnEaseTimerTick (timer-based, 60Hz)
	// - Phase tracking: OnPhaseTransition (AnimNotify events)
	//
	// Movement sync remains event-driven (phase transitions, play-rate changes).

	if (CombatDebug::IsCombatStateDebugEnabled())
	{
		DrawDebugInfo();
	}
}

ABaseCombatCharacter* UCombatComponent::GetOwnerCharacter() const
{
	return OwnerCharacter ? OwnerCharacter.Get() : Cast<ABaseCombatCharacter>(GetOwner());
}

// ============================================================================
// DEFENSE INTERACTION COMMIT CACHE
// ============================================================================

EDefenseCommitStatus UCombatComponent::BeginDefenseInteraction(
	const FDefenseInteractionKey& Key,
	FDefenseInteractionId& OutId,
	FDefenseContactReceipt& OutExistingReceipt,
	const bool bAllowNewRegistration)
{
	OutId = FDefenseInteractionId();
	OutExistingReceipt = FDefenseContactReceipt();
	SweepDefenseInteractionCache(FPlatformTime::Seconds());

	if (Key.Defender.Get() != GetOwner())
	{
		return EDefenseCommitStatus::RejectedBeforeRegistration;
	}

	if (FDefenseInteractionCacheRecord* Existing = DefenseInteractionCache.Find(Key))
	{
		OutId = Existing->Id;
		if (!Existing->bFinalized)
		{
			OutExistingReceipt.Resolution.InteractionId = Existing->Id;
			OutExistingReceipt.CommitStatus = EDefenseCommitStatus::InProgress;
			return EDefenseCommitStatus::InProgress;
		}

		OutExistingReceipt = Existing->Receipt;
		OutExistingReceipt.CommitStatus = EDefenseCommitStatus::Cached;
		return EDefenseCommitStatus::Cached;
	}

	if (!bAllowNewRegistration || !Key.IsValid())
	{
		return EDefenseCommitStatus::RejectedBeforeRegistration;
	}

	FDefenseInteractionCacheRecord& Record = DefenseInteractionCache.Add(Key);
	Record.Id.Key = Key;
	Record.Id.Epoch = ++NextDefenseInteractionEpoch;
	OutId = Record.Id;
	return EDefenseCommitStatus::NewCommit;
}

void UCombatComponent::FinalizeDefenseInteraction(
	const FDefenseInteractionId& Id,
	const FDefenseContactReceipt& Receipt)
{
	if (Id.Epoch == 0 || Id.Key.Defender.Get() != GetOwner())
	{
		return;
	}

	FDefenseInteractionCacheRecord* Record = DefenseInteractionCache.Find(Id.Key);
	if (!Record || Record->bFinalized || !(Record->Id == Id))
	{
		return;
	}

	Record->Receipt = Receipt;
	Record->Receipt.Resolution.InteractionId = Id;
	Record->Receipt.CommitStatus = EDefenseCommitStatus::NewCommit;
	Record->bFinalized = true;

	FDefenseTelemetryRecord Telemetry = DefenseTelemetry::FromResolution(
		Record->Receipt.Resolution,
		EDefenseTelemetryEvent::Resolution);
	Telemetry.CacheDisposition = TEXT("Finalized");
	if (Record->Receipt.bAcceptsWeaponHit)
	{
		Telemetry.WeaponDisposition = Record->Receipt.bConsumesHitBudget
			? TEXT("AcceptAndConsume")
			: TEXT("AcceptWithoutConsume");
	}
	else
	{
		Telemetry.WeaponDisposition = TEXT("Reject");
	}
	if (!Telemetry.AttackWindow.IsValid()
		&& LockedDefenseThreat.AttackInstance == Telemetry.AttackInstance)
	{
		Telemetry.AttackWindow = LockedDefenseThreat.ActiveParryWindow;
	}
	AppendDefenseTelemetry(MoveTemp(Telemetry));
}

bool UCombatComponent::IsDefenseInteractionFinalized(const FDefenseInteractionId& Id) const
{
	if (Id.Epoch == 0 || Id.Key.Defender.Get() != GetOwner())
	{
		return false;
	}

	const FDefenseInteractionCacheRecord* Record = DefenseInteractionCache.Find(Id.Key);
	return Record
		&& Record->bFinalized
		&& Record->Id == Id
		&& Record->Receipt.Resolution.InteractionId == Id;
}

void UCombatComponent::MarkDefenseContactSourceTerminal(
	const FContactInstanceId& ContactId,
	const double UnscaledNow)
{
	const bool bHasStructuralIdentity = ContactId.bUsesAttackWindow
		? ContactId.AttackWindow.WindowGeneration > 0
			&& ContactId.AttackWindow.AttackInstance.AttackGeneration > 0
		: ContactId.CompatibilityTrace.TraceGeneration > 0;
	if (!bHasStructuralIdentity || !FMath::IsFinite(UnscaledNow))
	{
		return;
	}

	for (TPair<FDefenseInteractionKey, FDefenseInteractionCacheRecord>& Pair : DefenseInteractionCache)
	{
		FDefenseInteractionCacheRecord& Record = Pair.Value;
		if (Pair.Key.Stage == EDefenseQueryStage::Contact
			&& Pair.Key.ContactInstance == ContactId
			&& !Record.bSourceTerminal)
		{
			Record.bSourceTerminal = true;
			Record.TerminalUnscaledTime = UnscaledNow;
			Record.TerminalSequence = ++NextDefenseTerminalSequence;
		}
	}

	SweepDefenseInteractionCache(UnscaledNow);
}

void UCombatComponent::SweepDefenseInteractionCache(const double UnscaledNow)
{
	if (!FMath::IsFinite(UnscaledNow))
	{
		return;
	}

	for (TPair<FDefenseInteractionKey, FDefenseInteractionCacheRecord>& Pair : DefenseInteractionCache)
	{
		FDefenseInteractionCacheRecord& Record = Pair.Value;
		if (Pair.Key.Stage != EDefenseQueryStage::Contact || Record.bSourceTerminal)
		{
			continue;
		}

		const FContactInstanceId& Contact = Pair.Key.ContactInstance;
		bool bSourceStillValid = false;
		if (Contact.bUsesAttackWindow)
		{
			const ABaseCombatCharacter* SourceCharacter = Cast<ABaseCombatCharacter>(
				Contact.AttackWindow.AttackInstance.Attacker.Get());
			bSourceStillValid = SourceCharacter
				&& SourceCharacter->CombatComponent
				&& SourceCharacter->CombatComponent->GetCurrentAttackGeneration()
					== Contact.AttackWindow.AttackInstance.AttackGeneration;
		}
		else
		{
			const UWeaponComponent* SourceWeapon = Cast<UWeaponComponent>(
				Contact.CompatibilityTrace.WeaponComponent.Get());
			bSourceStillValid = SourceWeapon
				&& SourceWeapon->IsContactInstanceCurrent(Contact);
		}
		if (!bSourceStillValid)
		{
			Record.bSourceTerminal = true;
			Record.TerminalUnscaledTime = UnscaledNow;
			Record.TerminalSequence = ++NextDefenseTerminalSequence;
		}
	}

	for (auto Iterator = DefenseInteractionCache.CreateIterator(); Iterator; ++Iterator)
	{
		const FDefenseInteractionCacheRecord& Record = Iterator.Value();
		if (Record.bFinalized
			&& Record.bSourceTerminal
			&& UnscaledNow - Record.TerminalUnscaledTime >= DefenseInteractionTombstoneSeconds)
		{
			Iterator.RemoveCurrent();
		}
	}

	auto CountTerminalRecords = [this]()
	{
		int32 Count = 0;
		for (const TPair<FDefenseInteractionKey, FDefenseInteractionCacheRecord>& Pair : DefenseInteractionCache)
		{
			Count += Pair.Value.bFinalized && Pair.Value.bSourceTerminal ? 1 : 0;
		}
		return Count;
	};

	while (CountTerminalRecords() > DefenseTerminalInteractionCacheCap)
	{
		const FDefenseInteractionKey* OldestKey = nullptr;
		uint64 OldestSequence = MAX_uint64;
		for (const TPair<FDefenseInteractionKey, FDefenseInteractionCacheRecord>& Pair : DefenseInteractionCache)
		{
			const FDefenseInteractionCacheRecord& Record = Pair.Value;
			if (Record.bFinalized && Record.bSourceTerminal && Record.TerminalSequence < OldestSequence)
			{
				OldestKey = &Pair.Key;
				OldestSequence = Record.TerminalSequence;
			}
		}

		if (!OldestKey)
		{
			break;
		}
		const FDefenseInteractionKey KeyToRemove = *OldestKey;
		DefenseInteractionCache.Remove(KeyToRemove);
	}
}

bool UCombatComponent::GetDebugDraw() const
{
	// General-purpose diagnostics remain behind the master toggle. Category-specific
	// phase, queue, and hold paths call their own predicates directly.
	return CombatDebug::IsDebugEnabled();
}

UAttackData* UCombatComponent::GetDefaultLightAttack() const
{
	// Priority 1: WeaponComponent's effective attack configuration (weapon-specific moveset)
	if (AActor* Owner = GetOwner())
	{
		if (UWeaponComponent* WeaponComp = Owner->FindComponentByClass<UWeaponComponent>())
		{
			if (UAttackConfiguration* WeaponConfig = WeaponComp->GetEffectiveAttackConfiguration())
			{
				if (WeaponConfig->DefaultLightAttack)
				{
					return WeaponConfig->DefaultLightAttack;
				}
			}
		}
	}

	// Priority 2: CombatSettings → DefaultWeaponData → AttackConfiguration → DefaultLightAttack
	if (CombatSettings)
	{
		if (UAttackConfiguration* AttackConfig = CombatSettings->GetAttackConfiguration())
		{
			return AttackConfig->DefaultLightAttack;
		}
	}

	return nullptr;
}

UAttackData* UCombatComponent::GetDefaultHeavyAttack() const
{
	// Priority 1: WeaponComponent's effective attack configuration (weapon-specific moveset)
	if (AActor* Owner = GetOwner())
	{
		if (UWeaponComponent* WeaponComp = Owner->FindComponentByClass<UWeaponComponent>())
		{
			if (UAttackConfiguration* WeaponConfig = WeaponComp->GetEffectiveAttackConfiguration())
			{
				if (WeaponConfig->DefaultHeavyAttack)
				{
					return WeaponConfig->DefaultHeavyAttack;
				}
			}
		}
	}

	// Priority 2: CombatSettings → DefaultWeaponData → AttackConfiguration → DefaultHeavyAttack
	if (CombatSettings)
	{
		if (UAttackConfiguration* AttackConfig = CombatSettings->GetAttackConfiguration())
		{
			return AttackConfig->DefaultHeavyAttack;
		}
	}

	return nullptr;
}

// ============================================================================
// INPUT PROCESSING
// ============================================================================

bool UCombatComponent::SubmitMovementInput(
	const FVector2D& CameraRelativeInput,
	const FRotator& CameraRotation,
	const bool bTerminal)
{
	FVector2D SanitizedInput = bTerminal ? FVector2D::ZeroVector : CameraRelativeInput;
	if (!FMath::IsFinite(static_cast<float>(SanitizedInput.X))
		|| !FMath::IsFinite(static_cast<float>(SanitizedInput.Y)))
	{
		SanitizedInput = FVector2D::ZeroVector;
	}

	const float RawMagnitude = SanitizedInput.Size();
	const float Magnitude = FMath::Clamp(RawMagnitude, 0.0f, 1.0f);
	SanitizedInput = RawMagnitude > UE_SMALL_NUMBER
		? SanitizedInput.GetSafeNormal() * Magnitude
		: FVector2D::ZeroVector;

	FVector WorldDirection = FVector::ZeroVector;
	if (!SanitizedInput.IsNearlyZero() && FMath::IsFinite(static_cast<float>(CameraRotation.Yaw)))
	{
		const FRotator CameraYaw(0.0f, CameraRotation.Yaw, 0.0f);
		WorldDirection =
			FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::X) * SanitizedInput.Y
			+ FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::Y) * SanitizedInput.X;
		WorldDirection.Z = 0.0f;
		WorldDirection = WorldDirection.GetSafeNormal();
		if (WorldDirection.ContainsNaN())
		{
			WorldDirection = FVector::ZeroVector;
		}
	}

	MovementInputSample.CameraRelativeInput = SanitizedInput;
	MovementInputSample.WorldDirection = WorldDirection;
	MovementInputSample.Magnitude = Magnitude;
	MovementInputSample.Serial = NextMovementInputSerial++;
	if (NextMovementInputSerial == 0)
	{
		NextMovementInputSerial = 1;
	}
	MovementInputSample.SimulationTimestamp = GetWorld()
		? static_cast<double>(GetWorld()->GetTimeSeconds())
		: 0.0;

	const bool bHasInput = !bTerminal && Magnitude > UE_SMALL_NUMBER;
	const EActionReactionTelemetryReason DecisionReason =
		ResolveMovementInputDecisionReason(bHasInput);
	RecordMovementInputDecision(DecisionReason);
	return DecisionReason == EActionReactionTelemetryReason::MovementInputAllowed;
}

void UCombatComponent::ClearMovementInputSample()
{
	if (MovementInputSample.CameraRelativeInput.IsNearlyZero()
		&& MovementInputSample.WorldDirection.IsNearlyZero()
		&& MovementInputSample.Magnitude <= UE_SMALL_NUMBER)
	{
		return;
	}

	const FRotator OwnerRotation = GetOwner()
		? GetOwner()->GetActorRotation()
		: FRotator::ZeroRotator;
	SubmitMovementInput(FVector2D::ZeroVector, OwnerRotation, true);
}

EActionReactionTelemetryReason UCombatComponent::ResolveMovementInputDecisionReason(
	const bool bHasInput) const
{
	if (!bHasInput)
	{
		return EActionReactionTelemetryReason::MovementInputCleared;
	}

	const ABaseCombatCharacter* Character = GetOwnerCharacter();
	if (!Character || Character->IsDeadOrDying())
	{
		return EActionReactionTelemetryReason::MovementInputSuppressedByTerminalState;
	}
	if (Character->HitReactionComponent
		&& Character->HitReactionComponent->IsInPairedAnimationState())
	{
		return EActionReactionTelemetryReason::MovementInputSuppressedByPaired;
	}

	if (HoldState.IsHolding() || HoldState.bIsEasing)
	{
		return EActionReactionTelemetryReason::MovementInputSuppressedByHold;
	}

	const UPairedAnimationComponent* PairedAnimComp = CachedPairedAnimComp;
	if (!PairedAnimComp && GetOwner())
	{
		PairedAnimComp = GetOwner()->FindComponentByClass<UPairedAnimationComponent>();
	}

	if (PairedAnimComp && PairedAnimComp->IsInputBlocked())
	{
		return EActionReactionTelemetryReason::MovementInputSuppressedByPaired;
	}

	return EActionReactionTelemetryReason::MovementInputAllowed;
}

void UCombatComponent::RecordMovementInputDecision(
	const EActionReactionTelemetryReason Reason)
{
	if (!ActionReactionTelemetry::IsEnabled())
	{
		LastMovementInputDecisionReason = EActionReactionTelemetryReason::None;
		return;
	}

	if (Reason == LastMovementInputDecisionReason)
	{
		return;
	}
	LastMovementInputDecisionReason = Reason;

	FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(
		EActionReactionTelemetryEvent::MovementInputDecisionChanged,
		Reason);
	Telemetry.InputSerial = MovementInputSample.Serial;
	Telemetry.MovementMagnitude = MovementInputSample.Magnitude;
	Telemetry.MovementDisposition = FName(
		Reason == EActionReactionTelemetryReason::MovementInputAllowed
			? TEXT("Allowed")
			: Reason == EActionReactionTelemetryReason::MovementInputCleared
				? TEXT("Inactive")
				: TEXT("Suppressed"));

	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Telemetry.bRootMotionActive = Character->IsPlayingRootMotion();
		if (const UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Telemetry.CharacterMovementMode = FName(
				*UEnum::GetValueAsString(Movement->MovementMode.GetValue()));
			Telemetry.CharacterCustomMovementMode = Movement->CustomMovementMode;
		}
	}

	AppendActionReactionTelemetry(MoveTemp(Telemetry));
}

bool UCombatComponent::IsMovementInputSuppressed() const
{
	return ResolveMovementInputDecisionReason(true)
		!= EActionReactionTelemetryReason::MovementInputAllowed;
}

void UCombatComponent::OnInputEventWithTransform(
	EInputType InputType,
	EInputEventType EventType,
	FVector2D CameraRelativeInput,
	FRotator CameraRotation,
	FRotator CharacterRotation)
{
	// Get owner character for mesh offset detection
	ACharacter* Character = Cast<ACharacter>(GetOwner());

	// Transform camera-relative input to character-relative direction
	// CRITICAL FIX (2025-11-20): Now passes Character for automatic mesh offset correction
	EInputDirection CharacterRelativeDirection = UCombatUtils::VectorToCharacterRelativeDirection(
		CameraRelativeInput,
		CameraRotation,
		Character,  // Mesh offset detection
		CharacterRotation,
		0.2f  // Dead zone
	);

	const FAttackFacingIntent FacingIntent = BuildAttackFacingIntent(
		CameraRelativeInput,
		CameraRotation,
		CharacterRotation,
		CharacterRelativeDirection,
		EAttackFacingIntentSource::CameraRelativeInput);
	OnInputEventInternal(InputType, EventType, CharacterRelativeDirection, FacingIntent);
}

FAttackFacingIntent UCombatComponent::BuildAttackFacingIntent(
	FVector2D CameraRelativeInput,
	FRotator CameraRotation,
	FRotator CharacterRotation,
	EInputDirection BranchDirection,
	EAttackFacingIntentSource Source) const
{
	FAttackFacingIntent Intent;
	Intent.CapturedFacingYaw = FMath::IsFinite(static_cast<float>(CharacterRotation.Yaw))
		? FRotator::NormalizeAxis(static_cast<float>(CharacterRotation.Yaw))
		: 0.0f;
	Intent.DesiredYaw = Intent.CapturedFacingYaw;
	Intent.BranchDirection = BranchDirection;
	Intent.Source = Source;
	Intent.SimulationTimestamp = GetWorld()
		? static_cast<double>(GetWorld()->GetTimeSeconds())
		: 0.0;

	if (!FMath::IsFinite(static_cast<float>(CameraRelativeInput.X))
		|| !FMath::IsFinite(static_cast<float>(CameraRelativeInput.Y))
		|| !FMath::IsFinite(static_cast<float>(CameraRotation.Yaw)))
	{
		Intent.Source = EAttackFacingIntentSource::CapturedFacing;
		return Intent;
	}

	const float InputMagnitude = FMath::Min(CameraRelativeInput.Size(), 1.0f);
	if (InputMagnitude <= 0.2f)
	{
		Intent.Source = EAttackFacingIntentSource::CapturedFacing;
		return Intent;
	}

	const FVector2D NormalizedInput = CameraRelativeInput.GetSafeNormal();
	const FRotator CameraYaw(0.0f, CameraRotation.Yaw, 0.0f);
	const FVector CameraForward = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::X);
	const FVector CameraRight = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::Y);
	FVector WorldDirection = CameraForward * NormalizedInput.Y
		+ CameraRight * NormalizedInput.X;
	WorldDirection.Z = 0.0f;
	WorldDirection = WorldDirection.GetSafeNormal();
	if (WorldDirection.IsNearlyZero() || WorldDirection.ContainsNaN())
	{
		Intent.Source = EAttackFacingIntentSource::CapturedFacing;
		return Intent;
	}

	Intent.WorldDirection = WorldDirection;
	Intent.DesiredYaw = FRotator::NormalizeAxis(WorldDirection.Rotation().Yaw);
	Intent.bHasWorldDirection = true;
	return Intent;
}

void UCombatComponent::OnInputEventAuto(
	EInputType InputType,
	EInputEventType EventType,
	FVector2D MovementInput,
	bool bCharacterRelative)
{
	// Get the owning character
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		UE_LOG(LogCombat, Error, TEXT("[INPUT AUTO] Owner is not a Character! Cannot auto-transform input."));
		return;
	}

	// Get character rotation (easy - just get owner's rotation)
	FRotator CharacterRotation = Character->GetActorRotation();

	// Get camera rotation from the controller
	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC)
	{
		// Fallback: If no player controller, assume camera faces same direction as character
		// This handles AI-controlled characters gracefully
		UE_LOG(LogCombat, Warning, TEXT("[INPUT AUTO] No PlayerController found, assuming camera = character rotation"));
		OnInputEventWithTransform(InputType, EventType, MovementInput, CharacterRotation, CharacterRotation);
		return;
	}

	// Get camera rotation (control rotation for player-controlled characters)
	FRotator CameraRotation = PC->GetControlRotation();

	// TOGGLE: Character-relative vs Camera-relative
	if (bCharacterRelative)
	{
		// Character-relative: Transform input based on character facing
		OnInputEventWithTransform(InputType, EventType, MovementInput, CameraRotation, CharacterRotation);
	}
	else
	{
		// Camera-relative (legacy): Transform uses camera as reference frame
		// Pass camera rotation as both camera AND character to skip character transformation
		OnInputEventWithTransform(InputType, EventType, MovementInput, CameraRotation, CameraRotation);
	}

	if (CombatDebug::IsHoldDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[INPUT AUTO] Mode=%s, Camera=%.1f°, Character=%.1f°"),
			bCharacterRelative ? TEXT("Character-Relative") : TEXT("Camera-Relative"),
			CameraRotation.Yaw, CharacterRotation.Yaw);
	}
}

void UCombatComponent::OnInputEvent(
	EInputType InputType,
	EInputEventType EventType,
	EInputDirection InputDirection)
{
	FAttackFacingIntent FacingIntent;
	FacingIntent.BranchDirection = InputDirection;
	FacingIntent.SimulationTimestamp = GetWorld()
		? static_cast<double>(GetWorld()->GetTimeSeconds())
		: 0.0;
	if (ABaseCombatCharacter* Character = GetOwnerCharacter())
	{
		FacingIntent.CapturedFacingYaw = FRotator::NormalizeAxis(
			Character->GetActorRotation().Yaw);
		FacingIntent.DesiredYaw = FacingIntent.CapturedFacingYaw;
		if (InputDirection != EInputDirection::None)
		{
			FVector WorldDirection = UCombatUtils::InputDirectionToWorldVector(
				InputDirection, Character);
			WorldDirection.Z = 0.0f;
			WorldDirection = WorldDirection.GetSafeNormal();
			if (!WorldDirection.IsNearlyZero() && !WorldDirection.ContainsNaN())
			{
				FacingIntent.WorldDirection = WorldDirection;
				FacingIntent.DesiredYaw = FRotator::NormalizeAxis(
					WorldDirection.Rotation().Yaw);
				FacingIntent.bHasWorldDirection = true;
				FacingIntent.Source = EAttackFacingIntentSource::CharacterRelativeDirection;
			}
		}
		else
		{
			FacingIntent.Source = EAttackFacingIntentSource::CapturedFacing;
		}
	}
	OnInputEventInternal(InputType, EventType, InputDirection, FacingIntent);
}

void UCombatComponent::OnInputEventInternal(
	EInputType InputType,
	EInputEventType EventType,
	EInputDirection InputDirection,
	const FAttackFacingIntent& FacingIntent)
{
	const uint64 InputSerial = CaptureCombatInput(InputType, EventType, InputDirection);
	if (EventType == EInputEventType::Canceled)
	{
		HeldInputs.Remove(InputType);
		HeldInputSerials.Remove(InputType);
		if (InputType == EInputType::Block)
		{
			EndBlock();
		}
		if (HoldState.IsHolding() && HoldState.GetHeldInputType() == InputType)
		{
			ClearHoldState();
		}

		FinalizeCombatInput(
			InputSerial,
			InputType == EInputType::Block
				? ECombatInputRoute::StatefulControl
				: ECombatInputRoute::NormalQueue,
			ECombatInputDisposition::Consumed,
			EActionReactionTelemetryReason::InputCanceled);
		return;
	}

	if (InputType == EInputType::Block && EventType == EInputEventType::Release)
	{
		EndBlock();
		FinalizeCombatInput(
			InputSerial,
			ECombatInputRoute::StatefulControl,
			ECombatInputDisposition::Consumed,
			EActionReactionTelemetryReason::StatefulControlConsumed);
		return;
	}

	// Test worlds and dynamic spawns can assign owner settings after component BeginPlay.
	if (!CombatSettings)
	{
		if (const ABaseCombatCharacter* Character = GetOwnerCharacter())
		{
			CombatSettings = Character->CombatSettings;
		}
	}

	// Early exit if no CombatSettings
	if (!CombatSettings)
	{
		FinalizeCombatInput(
			InputSerial,
			InputType == EInputType::Block ? ECombatInputRoute::StatefulControl : ECombatInputRoute::NormalQueue,
			ECombatInputDisposition::Rejected,
			EActionReactionTelemetryReason::MissingCombatSettings);
		return;
	}

	// Check if input can be processed (gate stunned/dead/guard broken states)
	if (!CanProcessInput(InputType))
	{
		if (EventType == EInputEventType::Release)
		{
			HeldInputs.Remove(InputType);
			HeldInputSerials.Remove(InputType);
			if (HoldState.IsHolding() && HoldState.GetHeldInputType() == InputType)
			{
				HoldState.CurrentHold.ReleaseInputSerial = InputSerial;
				ClearHoldState();
			}
		}
		FinalizeCombatInput(
			InputSerial,
			InputType == EInputType::Block ? ECombatInputRoute::StatefulControl : ECombatInputRoute::NormalQueue,
			ECombatInputDisposition::Rejected,
			EActionReactionTelemetryReason::CombatStateRejected);
		if (CombatDebug::IsQueueDebugEnabled())
		{
			UE_LOG(LogCombat, Warning, TEXT("[INPUT] Input REJECTED - Cannot process in current combat state"));
		}
		return;
	}

	UPairedAnimationComponent* PairedAnimComp = CachedPairedAnimComp;
	if (!PairedAnimComp)
	{
		PairedAnimComp = GetOwner() ? GetOwner()->FindComponentByClass<UPairedAnimationComponent>() : nullptr;
	}

	if (InputType == EInputType::Block)
	{
		const bool bBlockStarted = BeginBlock();
		const double BlockPressSimulationTime = GetWorld()
			? static_cast<double>(GetWorld()->GetTimeSeconds())
			: 0.0;
		const bool bPerfectParryCommitted = bBlockStarted
			&& EventType == EInputEventType::Press
			&& TryCommitPerfectParry(BlockPressSimulationTime, FPlatformTime::Seconds());
		FinalizeCombatInput(
			InputSerial,
			ECombatInputRoute::StatefulControl,
			(bBlockStarted || bPerfectParryCommitted)
				? ECombatInputDisposition::Consumed
				: ECombatInputDisposition::Rejected,
			(bBlockStarted || bPerfectParryCommitted)
				? EActionReactionTelemetryReason::StatefulControlConsumed
				: EActionReactionTelemetryReason::StatefulControlRejected);
		return;
	}

	if (PairedAnimComp &&
		EventType == EInputEventType::Press &&
		(InputType == EInputType::LightAttack || InputType == EInputType::HeavyAttack) &&
		PairedAnimComp->IsChainWaitingForResponse())
	{
		UAttackData* ChainAttackData = GetAttackForInput(InputType);
		const bool bAdvanced = PairedAnimComp->TryAdvanceChainCounter(ChainAttackData);
		FinalizeCombatInput(
			InputSerial,
			ECombatInputRoute::ChainOnly,
			bAdvanced ? ECombatInputDisposition::Consumed : ECombatInputDisposition::Expired,
			bAdvanced
				? EActionReactionTelemetryReason::ChainAdvanced
				: EActionReactionTelemetryReason::ChainExpired);
		return;
	}

	// ============================================================================
	// CONTEXT-AWARE DIRECTIONAL INPUT SAMPLING (Architectural Fix)
	// ============================================================================
	// Direction is ONLY captured during DirectionalInput context (hold release window)
	// This prevents movement stick deflection during normal combos from triggering directionals
	//
	// Design: Movement input (continuous) != Attack input (discrete)
	// - Movement context: Stick for character movement ONLY, ignored for attacks
	// - DirectionalInput context: Stick sampled at release for directional attacks

	if (InputDirection != EInputDirection::None)
	{
		if (CurrentInputContext == EInputContext::DirectionalInput)
		{
			// Sample direction ONLY at RELEASE event (after hold completion)
			if (EventType == EInputEventType::Release)
			{
				// Capture direction in buffer (discrete sampling, not continuous)
				DirectionalInputBuffer.CaptureDirection(InputDirection, GetWorld()->GetTimeSeconds());

				// CRITICAL: Update HoldEvent direction for directional follow-up resolution
				// Convert EInputDirection → EAttackDirection and store in current hold event
				if (HoldState.IsHolding())
				{
					HoldState.CurrentHold.Direction = UCombatUtils::InputToAttackDirection(InputDirection);

					if (CombatDebug::IsHoldDebugEnabled())
					{
						UE_LOG(LogCombat, Log, TEXT("[DIRECTIONAL] Direction captured at RELEASE: %s (time=%.2f) → HoldEvent.Direction=%s"),
							*UDebugUtils::FormatInputDirectionDebug(InputDirection),
							DirectionalInputBuffer.CaptureTime,
							*UDebugUtils::FormatAttackDirectionDebug(HoldState.CurrentHold.Direction));
					}
				}
				else if (CombatDebug::IsHoldDebugEnabled())
				{
					UE_LOG(LogCombat, Log, TEXT("[DIRECTIONAL] Direction captured at RELEASE: %s (time=%.2f) [NO ACTIVE HOLD]"),
						*UDebugUtils::FormatInputDirectionDebug(InputDirection),
						DirectionalInputBuffer.CaptureTime);
				}
			}
			else if (CombatDebug::IsHoldDebugEnabled())
			{
				// Direction provided during hold, but not capturing until release
				UE_LOG(LogCombat, Verbose, TEXT("[DIRECTIONAL] Direction during hold: %s (awaiting release to capture)"),
					*UDebugUtils::FormatInputDirectionDebug(InputDirection));
			}
		}
		else if (CombatDebug::IsHoldDebugEnabled())
		{
			// Direction provided but context is Movement (ignored for attack purposes)
			UE_LOG(LogCombat, Verbose, TEXT("[DIRECTIONAL] Direction IGNORED (context=%s, not DirectionalInput)"),
				*UEnum::GetValueAsString(CurrentInputContext));
		}

		// DEPRECATED: Keep LastDirectionalInput for backward compatibility (will be removed in future)
		LastDirectionalInput = InputDirection;
		bDirectionalInputConsumed = false;
	}
	else if (CombatDebug::IsHoldDebugEnabled())
	{
		// No direction provided (Blueprint might not be passing movement input)
		UE_LOG(LogCombat, Verbose, TEXT("[INPUT] No directional input provided"));
	}

	// Get current game time
	const float CurrentTime = GetWorld()->GetTimeSeconds();

	// Create input action
	FQueuedInputAction InputAction(InputType, EventType, CurrentTime, bComboWindowActive);
	InputAction.InputSerial = InputSerial;
	InputAction.FacingIntent = FacingIntent;

	// Track press/release pairs
	if (EventType == EInputEventType::Press)
	{
		// Check if we can accept new input (not in commit window, not duplicate)
		if (!CanAcceptNewInput(InputType))
		{
			FinalizeCombatInput(
				InputSerial,
				ECombatInputRoute::NormalQueue,
				ECombatInputDisposition::Rejected,
				EActionReactionTelemetryReason::DuplicatePendingInput);
			return; // Input rejected
		}

		HeldInputs.Add(InputType, CurrentTime);
		HeldInputSerials.Add(InputType, InputSerial);

		if (CombatDebug::IsQueueDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[INPUT] %s PRESSED at %.2f (Combo: %s, Direction: %s)"),
				*UEnum::GetValueAsString(InputType),
				CurrentTime,
				bComboWindowActive ? TEXT("YES") : TEXT("NO"),
				InputDirection != EInputDirection::None ? *UEnum::GetValueAsString(InputDirection) : TEXT("None"));
		}
	}
	else // Release
	{
		if (float* PressTime = HeldInputs.Find(InputType))
		{
			// Found matching press - process as pair
			FQueuedInputAction PressEvent(InputType, EInputEventType::Press, *PressTime, bComboWindowActive);
			ProcessInputPair(PressEvent, InputAction);
			HeldInputs.Remove(InputType);
		}
		HeldInputSerials.Remove(InputType);

		if (CombatDebug::IsQueueDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[INPUT] %s RELEASED at %.2f"),
				*UEnum::GetValueAsString(InputType),
				CurrentTime);
		}

		// Handle hold deactivation
		if (HoldState.IsHolding() && HoldState.GetHeldInputType() == InputType)
		{
			DeactivateHoldWithInputSerial(
				InputSerial,
				FacingIntent,
				InputDirection);
		}
	}

	ECombatInputDisposition Disposition = ECombatInputDisposition::Consumed;
	EActionReactionTelemetryReason DecisionReason = EActionReactionTelemetryReason::InputConsumed;
	if (EventType == EInputEventType::Press)
	{
		Disposition = TryQueueAction(InputAction, nullptr, &DecisionReason)
			? ECombatInputDisposition::Queued
			: ECombatInputDisposition::Rejected;
	}
	FinalizeCombatInput(
		InputSerial,
		ECombatInputRoute::NormalQueue,
		Disposition,
		DecisionReason);

	// Update stats
	QueueStats.TotalInputs++;
}

uint64 UCombatComponent::CaptureCombatInput(
	EInputType InputType,
	EInputEventType EventType,
	EInputDirection InputDirection)
{
	FCombatInputRecord Record;
	Record.Serial = NextCombatInputSerial++;
	if (NextCombatInputSerial == 0)
	{
		NextCombatInputSerial = 1;
	}
	Record.InputType = InputType;
	Record.EventType = EventType;
	Record.Direction = InputDirection;
	Record.SimulationTimestamp = GetWorld() ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
	Record.UnscaledTimestamp = FPlatformTime::Seconds();
	Record.Route = InputType == EInputType::Block
		? ECombatInputRoute::StatefulControl
		: ECombatInputRoute::NormalQueue;
	Record.Disposition = ECombatInputDisposition::Captured;

	CombatInputHistory.Add(Record);
	constexpr int32 MaxInputHistoryRecords = 64;
	if (CombatInputHistory.Num() > MaxInputHistoryRecords)
	{
		CombatInputHistory.RemoveAt(0, CombatInputHistory.Num() - MaxInputHistoryRecords, EAllowShrinking::No);
	}

	FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(
		EActionReactionTelemetryEvent::InputCaptured,
		EActionReactionTelemetryReason::Captured);
	Telemetry.InputSerial = Record.Serial;
	Telemetry.InputType = Record.InputType;
	Telemetry.InputEvent = Record.EventType;
	Telemetry.InputDirection = Record.Direction;
	Telemetry.InputRoute = Record.Route;
	Telemetry.InputDisposition = Record.Disposition;
	AppendActionReactionTelemetry(MoveTemp(Telemetry));

	return Record.Serial;
}

void UCombatComponent::FinalizeCombatInput(
	uint64 Serial,
	ECombatInputRoute Route,
	ECombatInputDisposition Disposition,
	EActionReactionTelemetryReason Reason)
{
	const FCombatInputRecord* FinalizedRecord = nullptr;
	for (int32 Index = CombatInputHistory.Num() - 1; Index >= 0; --Index)
	{
		FCombatInputRecord& Record = CombatInputHistory[Index];
		if (Record.Serial == Serial)
		{
			Record.Route = Route;
			Record.Disposition = Disposition;
			FinalizedRecord = &Record;
			break;
		}
	}

	FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(
		EActionReactionTelemetryEvent::InputFinalized,
		Reason);
	Telemetry.InputSerial = Serial;
	Telemetry.InputRoute = Route;
	Telemetry.InputDisposition = Disposition;
	if (FinalizedRecord)
	{
		Telemetry.InputType = FinalizedRecord->InputType;
		Telemetry.InputEvent = FinalizedRecord->EventType;
		Telemetry.InputDirection = FinalizedRecord->Direction;
	}
	else
	{
		Telemetry.Detail = TEXT("InputHistoryRecordEvicted");
	}
	AppendActionReactionTelemetry(MoveTemp(Telemetry));
}

bool UCombatComponent::CanProcessInput(EInputType InputType) const
{
	const ABaseCombatCharacter* Character = GetOwnerCharacter();
	if (Character
		&& Character->HitReactionComponent
		&& Character->HitReactionComponent->IsInPairedAnimationState())
	{
		return false;
	}

	// Block combat input during paired animations (finishers, counters)
	// This prevents accidental input buffering during cinematics
	if (CachedPairedAnimComp && CachedPairedAnimComp->IsInputBlocked())
	{
		const bool bIsChainResponse =
			(InputType == EInputType::LightAttack || InputType == EInputType::HeavyAttack)
			&& CachedPairedAnimComp->IsChainWaitingForResponse();
		if (bIsChainResponse)
		{
			return true;
		}
		if (CombatDebug::IsQueueDebugEnabled())
		{
			UE_LOG(LogCombat, Verbose, TEXT("[INPUT] Input blocked - paired animation in progress"));
		}
		return false;
	}

	// Accepts input unless explicitly disabled
	// Higher-level systems (death, hit reactions) should disable input at the source
	return true;
}

FAttackWindowInstanceId UCombatComponent::OpenAttackWindow(
	const EAttackWindowKind Kind,
	const FAnimNotifyRuntimeSourceId& NotifySource,
	const int32 MontageInstanceId,
	const float Duration)
{
	FAttackWindowInstanceId Result;
	const bool bSupportedKind = Kind == EAttackWindowKind::Hit
		|| Kind == EAttackWindowKind::Parry
		|| Kind == EAttackWindowKind::Counter;
	if (!bSupportedKind
		|| !NotifySource.IsValid()
		|| MontageInstanceId < 0
		|| !FMath::IsFinite(Duration)
		|| Duration < 0.0f
		|| !CurrentAttackData
		|| CurrentPhase == EAttackPhase::None)
	{
		return Result;
	}

	Result.AttackInstance.Attacker = GetOwner();
	Result.AttackInstance.AttackGeneration = AttackStateMachine.AttackGeneration;
	if (!Result.AttackInstance.IsValid()
		|| ConsumedAttackInstance == Result.AttackInstance)
	{
		return {};
	}

	const double SimulationNow = GetWorld()
		? static_cast<double>(GetWorld()->GetTimeSeconds())
		: 0.0;
	const double SimulationEnd = SimulationNow + static_cast<double>(Duration);
	if (!FMath::IsFinite(SimulationNow) || !FMath::IsFinite(SimulationEnd))
	{
		return {};
	}

	NextAttackWindowGeneration = NextAttackWindowGeneration == MAX_int32
		? 1
		: NextAttackWindowGeneration + 1;
	Result.Kind = Kind;
	Result.WindowGeneration = NextAttackWindowGeneration;
	Result.NotifySource = NotifySource;
	Result.MontageInstanceId = MontageInstanceId;
	Result.SimulationStartTime = SimulationNow;
	Result.SimulationEndTime = SimulationEnd;
	OpenAttackWindowRecords.Add(Result);

	switch (Kind)
	{
		case EAttackWindowKind::Hit:
			ActiveHitWindow = Result;
			break;
		case EAttackWindowKind::Parry:
			ActiveParryWindow = Result;
			break;
		case EAttackWindowKind::Counter:
			ActiveCounterWindow = Result;
			break;
		default:
			return {};
	}

	RequestDefenderThreatRefresh(AttackIntentTarget.Get(), EThreatRefreshReason::WindowChanged);
	return Result;
}

FAttackWindowInstanceId UCombatComponent::RefreshAttackWindow(
	const EAttackWindowKind Kind,
	const FAnimNotifyRuntimeSourceId& NotifySource,
	const int32 MontageInstanceId,
	const float RemainingDuration)
{
	if (!NotifySource.IsValid()
		|| MontageInstanceId < 0
		|| !FMath::IsFinite(RemainingDuration)
		|| RemainingDuration < 0.0f)
	{
		return {};
	}

	const int32 RecordIndex = OpenAttackWindowRecords.IndexOfByPredicate(
		[Kind, &NotifySource, MontageInstanceId](const FAttackWindowInstanceId& Candidate)
		{
			return Candidate.Kind == Kind
				&& Candidate.NotifySource == NotifySource
				&& Candidate.MontageInstanceId == MontageInstanceId;
		});
	if (RecordIndex == INDEX_NONE)
	{
		return {};
	}

	FAttackWindowInstanceId* PublishedWindow = nullptr;
	switch (Kind)
	{
		case EAttackWindowKind::Hit:
			PublishedWindow = &ActiveHitWindow;
			break;
		case EAttackWindowKind::Parry:
			PublishedWindow = &ActiveParryWindow;
			break;
		case EAttackWindowKind::Counter:
			PublishedWindow = &ActiveCounterWindow;
			break;
		default:
			return {};
	}

	const FAttackWindowInstanceId Existing = OpenAttackWindowRecords[RecordIndex];
	if (!PublishedWindow || !(*PublishedWindow == Existing))
	{
		return {};
	}

	const double SimulationNow = GetWorld()
		? static_cast<double>(GetWorld()->GetTimeSeconds())
		: 0.0;
	const double UpdatedEnd = SimulationNow + static_cast<double>(RemainingDuration);
	if (!FMath::IsFinite(SimulationNow) || !FMath::IsFinite(UpdatedEnd))
	{
		return {};
	}

	FAttackWindowInstanceId Updated = Existing;
	Updated.SimulationEndTime = FMath::Max(Updated.SimulationStartTime, UpdatedEnd);
	OpenAttackWindowRecords[RecordIndex] = Updated;
	*PublishedWindow = Updated;
	RequestDefenderThreatRefresh(AttackIntentTarget.Get(), EThreatRefreshReason::WindowChanged);
	return Updated;
}

bool UCombatComponent::CloseAttackWindow(
	const EAttackWindowKind Kind,
	const FAnimNotifyRuntimeSourceId& NotifySource,
	const int32 MontageInstanceId)
{
	if (!NotifySource.IsValid() || MontageInstanceId < 0)
	{
		return false;
	}

	const int32 RecordIndex = OpenAttackWindowRecords.IndexOfByPredicate(
		[Kind, &NotifySource, MontageInstanceId](const FAttackWindowInstanceId& Candidate)
		{
			return Candidate.Kind == Kind
				&& Candidate.NotifySource == NotifySource
				&& Candidate.MontageInstanceId == MontageInstanceId;
		});
	if (RecordIndex == INDEX_NONE)
	{
		return false;
	}

	const FAttackWindowInstanceId ClosingWindow = OpenAttackWindowRecords[RecordIndex];
	OpenAttackWindowRecords.RemoveAt(RecordIndex, 1, EAllowShrinking::No);
	FAttackWindowInstanceId* PublishedWindow = nullptr;
	switch (Kind)
	{
		case EAttackWindowKind::Hit:
			PublishedWindow = &ActiveHitWindow;
			break;
		case EAttackWindowKind::Parry:
			PublishedWindow = &ActiveParryWindow;
			break;
		case EAttackWindowKind::Counter:
			PublishedWindow = &ActiveCounterWindow;
			break;
		default:
			break;
	}

	if (!PublishedWindow || !(*PublishedWindow == ClosingWindow))
	{
		return false;
	}

	*PublishedWindow = {};
	if (Kind == EAttackWindowKind::Parry
		&& PublishedPredictionAttackInstance == ClosingWindow.AttackInstance)
	{
		InvalidateAttackThreatPrediction(EThreatInvalidationReason::WindowChanged);
	}
	else
	{
		RequestDefenderThreatRefresh(AttackIntentTarget.Get(), EThreatRefreshReason::WindowChanged);
	}
	return true;
}

FAttackWindowInstanceId UCombatComponent::GetActiveAttackWindow(const EAttackWindowKind Kind) const
{
	switch (Kind)
	{
		case EAttackWindowKind::Hit:
			return ActiveHitWindow;
		case EAttackWindowKind::Parry:
			return ActiveParryWindow;
		case EAttackWindowKind::Counter:
			return ActiveCounterWindow;
		default:
			return {};
	}
}

bool UCombatComponent::ConsumeActiveAttack(
	const FAttackInstanceId& AttackId,
	const EAttackConsumeReason Reason)
{
	return ConsumeActiveAttackInternal(AttackId, Reason, {});
}

bool UCombatComponent::AbortActiveAttack(const FAttackInstanceId& AttackId)
{
	FAttackInstanceId CurrentAttack;
	CurrentAttack.Attacker = GetOwner();
	CurrentAttack.AttackGeneration = AttackStateMachine.AttackGeneration;
	if (!AttackId.IsValid()
		|| !(AttackId == CurrentAttack)
		|| !CurrentAttackData
		|| CurrentPhase == EAttackPhase::None)
	{
		return false;
	}

	SetPhase(EAttackPhase::None);
	if (ABaseCombatCharacter* Character = GetOwnerCharacter())
	{
		if (UWeaponComponent* Weapon = Character->WeaponComponent.Get();
			Weapon && Weapon->IsHitDetectionEnabled())
		{
			Weapon->DisableHitDetection();
		}
	}
	return true;
}

bool UCombatComponent::ConsumeActiveAttackInternal(
	const FAttackInstanceId& AttackId,
	const EAttackConsumeReason Reason,
	const FDefenseInteractionId& InteractionId)
{
	FAttackInstanceId CurrentAttack;
	CurrentAttack.Attacker = GetOwner();
	CurrentAttack.AttackGeneration = AttackStateMachine.AttackGeneration;
	if (!AttackId.IsValid()
		|| !(AttackId == CurrentAttack)
		|| !CurrentAttackData
		|| CurrentPhase == EAttackPhase::None
		|| ConsumedAttackInstance == AttackId)
	{
		return false;
	}

	// The consumed marker is installed before any cleanup callback can reenter combat.
	ConsumedAttackInstance = AttackId;
	bConsumedPendingPresentation = true;
	const FAttackWindowInstanceId ConsumedHitWindow = ActiveHitWindow;
	ClearPublishedAttackWindowsForAttack(AttackId);
	InvalidateAttackThreatPrediction(EThreatInvalidationReason::AttackConsumed);

	if (ABaseCombatCharacter* Character = GetOwnerCharacter())
	{
		if (UWeaponComponent* Weapon = Character->WeaponComponent.Get())
		{
			if (!Weapon->DisableHitDetectionForAttack(ConsumedHitWindow)
				&& Weapon->IsHitDetectionEnabled())
			{
				Weapon->DisableHitDetection();
			}
		}
		if (UTargetingComponent* Targeting = Character->GetTargetingComponent())
		{
			Targeting->ReleaseActiveAttackWarp();
		}
	}

	for (const FActionQueueEntry& Entry : ActionQueue)
	{
		if (Entry.IsPending())
		{
			FActionQueueEntry CancelledEntry = Entry;
			CancelledEntry.State = EActionState::Cancelled;
			AppendActionQueueTelemetry(
				CancelledEntry,
				EActionReactionTelemetryEvent::QueueCancelled,
				EActionReactionTelemetryReason::AttackConsumed);
		}
	}
	ActionQueue.RemoveAll([](const FActionQueueEntry& Entry)
	{
		return Entry.IsPending();
	});

	FAttackConsumedEvent Event;
	Event.AttackInstance = AttackId;
	Event.Reason = Reason;
	Event.InteractionId = InteractionId;
	PendingAttackConsumedEvents.Add(Event);
	if (!DeferredAttackConsumedTickerHandle.IsValid())
	{
		DeferredAttackConsumedTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(
				this,
				&UCombatComponent::FlushDeferredAttackConsumedEvents));
	}

	OnAttackConsumedInternal.Broadcast(Event);
	return true;
}

bool UCombatComponent::FlushDeferredAttackConsumedEvents(const float DeltaTime)
{
	(void)DeltaTime;
	DeferredAttackConsumedTickerHandle.Reset();
	TArray<FAttackConsumedEvent> Events = MoveTemp(PendingAttackConsumedEvents);
	PendingAttackConsumedEvents.Reset();
	for (const FAttackConsumedEvent& Event : Events)
	{
		OnAttackConsumed.Broadcast(Event);
	}
	return false;
}

bool UCombatComponent::HasRegisteredDefenseContactForAttack(
	const FAttackInstanceId& AttackId) const
{
	if (!AttackId.IsValid())
	{
		return false;
	}

	for (const TPair<FDefenseInteractionKey, FDefenseInteractionCacheRecord>& Pair : DefenseInteractionCache)
	{
		const FDefenseInteractionKey& Key = Pair.Key;
		if (Key.Stage == EDefenseQueryStage::Contact
			&& Key.ContactInstance.bUsesAttackWindow
			&& Key.ContactInstance.AttackWindow.AttackInstance == AttackId)
		{
			return true;
		}
	}
	return false;
}

FAttackExecutionSnapshot UCombatComponent::BuildAttackExecutionSnapshot() const
{
	FAttackExecutionSnapshot Snapshot;
	AActor* Owner = GetOwner();
	const ABaseCombatCharacter* Character = GetOwnerCharacter();
	const double SimulationNow = GetWorld() ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;

	Snapshot.AttackInstance.Attacker = Owner;
	Snapshot.AttackInstance.AttackGeneration = AttackStateMachine.AttackGeneration;
	Snapshot.StableId = CombatantStableId;
	Snapshot.AttackData = CurrentAttackData;
	Snapshot.ActiveMontage = AttackStateMachine.ActiveMontage;
	Snapshot.MontageSection = AttackStateMachine.ActiveSectionName;
	Snapshot.AttackPhase = CurrentPhase;
	if (ActiveParryWindow.IsValid()
		&& ActiveParryWindow.AttackInstance == Snapshot.AttackInstance)
	{
		Snapshot.ActiveParryWindow = ActiveParryWindow;
	}
	if (ActiveCounterWindow.IsValid()
		&& ActiveCounterWindow.AttackInstance == Snapshot.AttackInstance)
	{
		Snapshot.ActiveCounterWindow = ActiveCounterWindow;
	}
	Snapshot.IntendedTarget = AttackIntentTarget;
	Snapshot.AttackerTransform = Owner ? Owner->GetActorTransform() : FTransform::Identity;
	Snapshot.AttackerVelocity = Owner ? Owner->GetVelocity() : FVector::ZeroVector;
	Snapshot.AttackerTeam = Character ? Character->TeamId : ETeamId::Neutral;
	Snapshot.bAttackerAlive = IsValid(Owner) && (!Character || !Character->IsDeadOrDying());
	Snapshot.bAttackerPaired = CachedPairedAnimComp && CachedPairedAnimComp->IsPairedAnimationActive();
	Snapshot.bAttackConsumed = Snapshot.AttackInstance.IsValid()
		&& ConsumedAttackInstance == Snapshot.AttackInstance;
	Snapshot.bAttackIdentityCurrent = Snapshot.AttackInstance.IsValid()
		&& Snapshot.AttackInstance.AttackGeneration == AttackStateMachine.AttackGeneration;
	Snapshot.bAttackActive = Snapshot.bAttackIdentityCurrent
		&& CurrentAttackData
		&& CurrentPhase != EAttackPhase::None
		&& Snapshot.bAttackerAlive
		&& !Snapshot.bAttackConsumed;

	if (const UAttackData* Attack = CurrentAttackData)
	{
		Snapshot.AttackTags = Attack->AttackTags;
		Snapshot.AttackType = Attack->AttackType;
		Snapshot.AuthoredHeight = Attack->DefenseProfile.Height;
		Snapshot.NominalLane = Attack->DefenseProfile.NominalLane;
		Snapshot.SwingShape = Attack->DefenseProfile.SwingShape;
		Snapshot.SourceSocket = Attack->DefenseProfile.SourceContactSocketOverride.IsNone()
			? Attack->AttackHand
			: Attack->DefenseProfile.SourceContactSocketOverride;
		Snapshot.DefenderTargetBone = Attack->GetDefenseTargetBoneFallback();
	}

	if (Character && Snapshot.ActiveMontage.IsValid())
	{
		if (UAnimInstance* AnimInstance = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			const UAnimMontage* Montage = Snapshot.ActiveMontage.Get();
			const float MontagePosition = AnimInstance->Montage_GetPosition(Montage);
			float SectionStart = 0.0f;
			float SectionEnd = Montage ? Montage->GetPlayLength() : 0.0f;
			const int32 SectionIndex = Montage ? Montage->GetSectionIndex(Snapshot.MontageSection) : INDEX_NONE;
			if (Montage && SectionIndex != INDEX_NONE)
			{
				Montage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
			}
			Snapshot.SectionTime = FMath::Max(0.0f, MontagePosition - SectionStart);
		}
	}

	const bool bPredictionBelongsToCurrentAttack = PublishedPredictionAttackInstance.IsValid()
		&& PublishedPredictionAttackInstance == Snapshot.AttackInstance;
	const bool bPredictionTargetMatchesIntent = PublishedAttackThreatPrediction.IntendedTarget.IsValid()
		&& PublishedAttackThreatPrediction.IntendedTarget == AttackIntentTarget;
	if (Snapshot.bAttackActive && bPredictionBelongsToCurrentAttack && bPredictionTargetMatchesIntent)
	{
		Snapshot.PredictedContact.bIsValid = true;
		Snapshot.PredictedContact.IntendedTarget = PublishedAttackThreatPrediction.IntendedTarget;
		Snapshot.PredictedContact.PathOrigin = PublishedAttackThreatPrediction.PathOrigin;
		Snapshot.PredictedContact.PathDirection = PublishedAttackThreatPrediction.PathDirection;
		Snapshot.PredictedContact.ContactPoint = PublishedAttackThreatPrediction.PredictedContactPoint;
		Snapshot.PredictedContact.SourceSocket = PublishedAttackThreatPrediction.SourceSocket;
		Snapshot.PredictedContact.DefenderTargetBone = PublishedAttackThreatPrediction.DefenderTargetBone;
		Snapshot.PredictedContact.ContactSimulationTime = PublishedAttackThreatPrediction.PredictedContactSimulationTime;
		Snapshot.PredictedContact.PredictionSimulationTimestamp = PublishedAttackThreatPrediction.PredictionSimulationTimestamp;
		Snapshot.PredictedContact.Lane = PublishedAttackThreatPrediction.Lane;
		Snapshot.PredictedContact.Height = PublishedAttackThreatPrediction.Height;
		Snapshot.PredictedContact.Confidence = PublishedAttackThreatPrediction.Confidence;
		Snapshot.PredictedContact.bPathIntersectsThreatVolume = PublishedAttackThreatPrediction.bPathIntersectsThreatVolume;

		if (PublishedAttackThreatPrediction.PredictedContactSimulationTime > SimulationNow)
		{
			Snapshot.TimeToPredictedContact = static_cast<float>(
				PublishedAttackThreatPrediction.PredictedContactSimulationTime - SimulationNow);
			if (PublishedAttackThreatPrediction.Confidence == EDefensePredictionConfidence::High)
			{
				Snapshot.TimeToAlignmentDeadline = Snapshot.TimeToPredictedContact;
			}
		}
		Snapshot.bHasCredibleIntent = PublishedAttackThreatPrediction.Confidence == EDefensePredictionConfidence::High
			&& PublishedAttackThreatPrediction.bPathIntersectsThreatVolume;
	}

	return Snapshot;
}

void UCombatComponent::PublishAttackThreatPrediction(const FAttackThreatPrediction& Prediction)
{
	FAttackInstanceId CurrentAttackInstance;
	CurrentAttackInstance.Attacker = GetOwner();
	CurrentAttackInstance.AttackGeneration = AttackStateMachine.AttackGeneration;
	if (!CurrentAttackInstance.IsValid() || !CurrentAttackData || CurrentPhase == EAttackPhase::None)
	{
		InvalidateAttackThreatPrediction(EThreatInvalidationReason::AttackEnded);
		return;
	}

	PublishedAttackThreatPrediction = Prediction;
	PublishedPredictionAttackInstance = CurrentAttackInstance;
	LastThreatInvalidationReason = EThreatInvalidationReason::None;

	const bool bTargetMatches = Prediction.IntendedTarget.IsValid()
		&& Prediction.IntendedTarget == AttackIntentTarget;
	const bool bUsablePath = !Prediction.PathDirection.ContainsNaN()
		&& !Prediction.PathDirection.IsNearlyZero();
	const bool bFiniteTiming = FMath::IsFinite(Prediction.PredictionSimulationTimestamp)
		&& FMath::IsFinite(Prediction.PredictedContactSimulationTime);
	const bool bReviewedFutureDeadline = bFiniteTiming
		&& Prediction.PredictedContactSimulationTime > Prediction.PredictionSimulationTimestamp;
	const bool bCompleteHighEvidence = bTargetMatches
		&& bUsablePath
		&& bReviewedFutureDeadline
		&& Prediction.bPathIntersectsThreatVolume;

	if (PublishedAttackThreatPrediction.Confidence == EDefensePredictionConfidence::High
		&& !bCompleteHighEvidence)
	{
		PublishedAttackThreatPrediction.Confidence = bTargetMatches && bUsablePath
			? EDefensePredictionConfidence::Low
			: EDefensePredictionConfidence::None;
	}

	if (!bTargetMatches || !bUsablePath)
	{
		PublishedPredictionAttackInstance = FAttackInstanceId();
		PublishedAttackThreatPrediction = FAttackThreatPrediction();
	}

	RequestDefenderThreatRefresh(
		Prediction.IntendedTarget.Get(),
		bTargetMatches && bUsablePath
			? EThreatRefreshReason::PredictionPublished
			: EThreatRefreshReason::PredictionInvalidated);
}

bool UCombatComponent::PublishReviewedAttackWindowPrediction(
	const FAttackWindowInstanceId& Window)
{
	ABaseCombatCharacter* Source = GetOwnerCharacter();
	ABaseCombatCharacter* Defender = Cast<ABaseCombatCharacter>(AttackIntentTarget.Get());
	FAttackInstanceId CurrentAttack;
	CurrentAttack.Attacker = GetOwner();
	CurrentAttack.AttackGeneration = AttackStateMachine.AttackGeneration;
	if (!Source
		|| !Defender
		|| !CurrentAttackData
		|| CurrentPhase == EAttackPhase::None
		|| !Window.IsValid()
		|| !(Window.AttackInstance == CurrentAttack)
		|| Window.Kind != EAttackWindowKind::Parry
		|| !(ActiveParryWindow == Window))
	{
		InvalidateAttackThreatPrediction(EThreatInvalidationReason::WindowChanged);
		return false;
	}

	const double SimulationNow = GetWorld()
		? static_cast<double>(GetWorld()->GetTimeSeconds())
		: 0.0;
	const FName SourceSocket = CurrentAttackData->DefenseProfile.SourceContactSocketOverride.IsNone()
		? CurrentAttackData->AttackHand
		: CurrentAttackData->DefenseProfile.SourceContactSocketOverride;
	const FName TargetBone = CurrentAttackData->GetDefenseTargetBoneFallback();

	FVector PathOrigin = FVector::ZeroVector;
	bool bHasSourceContactPoint = false;
	if (!SourceSocket.IsNone())
	{
		if (Source->WeaponComponent && Source->WeaponComponent->HasBegunPlay())
		{
			bHasSourceContactPoint = Source->WeaponComponent->TryGetSocketLocation(
				SourceSocket, PathOrigin);
		}
		if (!bHasSourceContactPoint)
		{
			if (USkeletalMeshComponent* Mesh = Source->GetMesh(); Mesh && Mesh->DoesSocketExist(SourceSocket))
			{
				PathOrigin = Mesh->GetSocketLocation(SourceSocket);
				bHasSourceContactPoint = true;
			}
		}
	}

	FVector PredictedContactPoint = FVector::ZeroVector;
	bool bHasTargetContactPoint = false;
	if (USkeletalMeshComponent* Mesh = Defender->GetMesh(); Mesh && !TargetBone.IsNone()
		&& Mesh->DoesSocketExist(TargetBone))
	{
		PredictedContactPoint = Mesh->GetSocketLocation(TargetBone);
		bHasTargetContactPoint = true;
	}
	if (!bHasSourceContactPoint || !bHasTargetContactPoint)
	{
		InvalidateAttackThreatPrediction(EThreatInvalidationReason::PathChanged);
		return false;
	}

	const FVector Path = PredictedContactPoint - PathOrigin;
	bool bPathIntersectsThreatVolume = false;
	if (!Path.ContainsNaN() && !Path.IsNearlyZero())
	{
		if (const UCapsuleComponent* Capsule = Defender->GetCapsuleComponent())
		{
			const float Radius = Capsule->GetScaledCapsuleRadius();
			const float AxisHalfLength = Capsule->GetScaledCapsuleHalfHeight_WithoutHemisphere();
			const FVector CapsuleCenter = Capsule->GetComponentLocation();
			const FVector CapsuleAxis = Capsule->GetUpVector() * AxisHalfLength;
			FVector ClosestOnPath;
			FVector ClosestOnCapsule;
			FMath::SegmentDistToSegmentSafe(
				PathOrigin,
				PredictedContactPoint,
				CapsuleCenter - CapsuleAxis,
				CapsuleCenter + CapsuleAxis,
				ClosestOnPath,
				ClosestOnCapsule);
			bPathIntersectsThreatVolume = FVector::DistSquared(ClosestOnPath, ClosestOnCapsule)
				<= FMath::Square(FMath::Max(0.0f, Radius));
		}
	}

	FAttackThreatPrediction Prediction;
	Prediction.IntendedTarget = Defender;
	Prediction.PathOrigin = PathOrigin;
	Prediction.PathDirection = Path.GetSafeNormal();
	Prediction.PredictedContactPoint = PredictedContactPoint;
	Prediction.SourceSocket = SourceSocket;
	Prediction.DefenderTargetBone = TargetBone;
	Prediction.PredictedContactSimulationTime = Window.SimulationEndTime;
	Prediction.PredictionSimulationTimestamp = SimulationNow;
	Prediction.Lane = CurrentAttackData->DefenseProfile.NominalLane;
	Prediction.Height = CurrentAttackData->DefenseProfile.Height;
	Prediction.Confidence = Window.SimulationEndTime > SimulationNow
		&& bPathIntersectsThreatVolume
		? EDefensePredictionConfidence::High
		: !Path.IsNearlyZero()
			? EDefensePredictionConfidence::Low
			: EDefensePredictionConfidence::None;
	Prediction.bPathIntersectsThreatVolume = bPathIntersectsThreatVolume;
	PublishAttackThreatPrediction(Prediction);
	const bool bPublishedHighConfidence =
		Prediction.Confidence == EDefensePredictionConfidence::High;
#if WITH_AUTOMATION_TESTS
	if (bPublishedHighConfidence)
	{
		OnReviewedAttackPredictionPublishedForTesting.Broadcast(Window);
	}
#endif
	return bPublishedHighConfidence;
}

void UCombatComponent::InvalidateAttackThreatPrediction(EThreatInvalidationReason Reason)
{
	AActor* PreviousIntendedTarget = AttackIntentTarget.Get();
	PublishedAttackThreatPrediction = FAttackThreatPrediction();
	PublishedPredictionAttackInstance = FAttackInstanceId();
	LastThreatInvalidationReason = Reason;
	RequestDefenderThreatRefresh(PreviousIntendedTarget, EThreatRefreshReason::PredictionInvalidated);
}

void UCombatComponent::SetAttackIntentTarget(AActor* IntendedTarget)
{
	if (AttackIntentTarget.Get() == IntendedTarget)
	{
		return;
	}

	AActor* PreviousIntendedTarget = AttackIntentTarget.Get();
	AttackIntentTarget = IntendedTarget;
	InvalidateAttackThreatPrediction(EThreatInvalidationReason::TargetChanged);
	if (PreviousIntendedTarget != IntendedTarget)
	{
		RequestDefenderThreatRefresh(PreviousIntendedTarget, EThreatRefreshReason::TargetChanged);
	}
}

FDefenseThreatSelectionResult UCombatComponent::SelectDefenseThreat(double SimulationNow)
{
	FDefenseThreatSelectionResult Result;
	const FCombatantStableId PreviousLockedThreatId = LockedDefenseThreatId;
	ABaseCombatCharacter* Defender = GetOwnerCharacter();
	UTargetingComponent* Targeting = Defender ? Defender->GetTargetingComponent() : nullptr;
	if (Targeting && GuardAlignmentRequestHandle.IsValid())
	{
		FAlignmentRequestSpec CurrentAlignment;
		if (Targeting->GetAlignmentRequestSpec(GuardAlignmentRequestHandle, CurrentAlignment))
		{
			RemainingDefenseAutomaticTurn = CurrentAlignment.RemainingTurnBudget;
		}
		else
		{
			GuardAlignmentRequestHandle = {};
		}
	}
	if (!Defender || !Targeting || !FMath::IsFinite(SimulationNow))
	{
		bGuardThreatCandidatesExist = false;
		LockedDefenseThreat = {};
		LockedDefenseThreatActor.Reset();
		LockedDefenseThreatId = {};
		DefenseThreatLockAcquiredSimulationTime = -1.0;
		RemainingDefenseAutomaticTurn = 0.0f;
		return Result;
	}

	const UDefenseConfiguration* DefenseConfig = GetEffectiveDefenseConfiguration();
	const UTargetingSettings* TargetingSettings = Targeting->GetEffectiveSettings();
	const float DefenseRange = DefenseConfig && FMath::IsFinite(DefenseConfig->DefenseThreatRange)
		? FMath::Max(0.0f, DefenseConfig->DefenseThreatRange)
		: 1000.0f;
	const float TargetingRange = TargetingSettings && FMath::IsFinite(TargetingSettings->MaxTargetDistance)
		? FMath::Max(0.0f, TargetingSettings->MaxTargetDistance)
		: 1000.0f;
	const float QueryRange = FMath::Min(DefenseRange, TargetingRange);
	const float QueryRangeSquared = FMath::Square(QueryRange);
	const float MaximumPredictionAge = DefenseConfig
		? FMath::Max(0.0f, DefenseConfig->MaximumHighConfidencePredictionAge)
		: 0.10f;

	TArray<AActor*> EnumeratedTargets;
	Targeting->GetAllTargetsInRange(EnumeratedTargets, QueryRange);
	TArray<FAttackExecutionSnapshot> Candidates;
	Candidates.Reserve(EnumeratedTargets.Num());
	const FVector DefenderLocation = Defender->GetActorLocation();
	const FTransform DefenderTransform = Defender->GetActorTransform();

	for (AActor* Target : EnumeratedTargets)
	{
		if (!IsValid(Target) || Target == Defender
			|| FVector::DistSquared(DefenderLocation, Target->GetActorLocation()) > QueryRangeSquared
			|| !Target->Implements<UTeamMemberInterface>()
			|| !Defender->Implements<UTeamMemberInterface>())
		{
			continue;
		}

		const bool bHostile = ITeamMemberInterface::Execute_IsHostileTo(Target, Defender)
			|| ITeamMemberInterface::Execute_IsHostileTo(Defender, Target);
		const bool bFriendly = ITeamMemberInterface::Execute_IsFriendlyTo(Target, Defender)
			|| ITeamMemberInterface::Execute_IsFriendlyTo(Defender, Target);
		if (!bHostile || bFriendly)
		{
			continue;
		}

		UCombatComponent* AttackerCombat = Target->FindComponentByClass<UCombatComponent>();
		if (!AttackerCombat)
		{
			continue;
		}

		FAttackExecutionSnapshot Candidate = AttackerCombat->BuildAttackExecutionSnapshot();
		Candidate.bIsHostileToDefender = bHostile;
		Candidate.bIsFriendlyToDefender = bFriendly;
		const FVector SourceBearing = Target->GetActorLocation() - DefenderLocation;
		Candidate.RelativeYawDegrees = FDefenseResolver::CalculateDefenderRelativeYaw(
			DefenderTransform, SourceBearing);
		Candidate.DistanceToDefender = SourceBearing.Size();

		Candidate.TimeToPredictedContact = -1.0f;
		Candidate.TimeToParryWindowEnd = -1.0f;
		Candidate.TimeToAlignmentDeadline = -1.0f;
		bool bPredictionRemainsHigh = Candidate.PredictedContact.bIsValid
			&& Candidate.PredictedContact.Confidence == EDefensePredictionConfidence::High;
		if (bPredictionRemainsHigh)
		{
			const double PredictionAge = SimulationNow
				- Candidate.PredictedContact.PredictionSimulationTimestamp;
			bPredictionRemainsHigh = FMath::IsFinite(PredictionAge)
				&& PredictionAge >= 0.0
				&& PredictionAge <= static_cast<double>(MaximumPredictionAge);
			if (!bPredictionRemainsHigh)
			{
				Candidate.PredictedContact.Confidence = EDefensePredictionConfidence::Low;
				Candidate.bHasCredibleIntent = false;
			}
		}
		if (Candidate.PredictedContact.bIsValid
			&& FMath::IsFinite(Candidate.PredictedContact.ContactSimulationTime))
		{
			const double Remaining = Candidate.PredictedContact.ContactSimulationTime - SimulationNow;
			if (Remaining >= 0.0 && Remaining <= static_cast<double>(TNumericLimits<float>::Max()))
			{
				Candidate.TimeToPredictedContact = static_cast<float>(Remaining);
				if (bPredictionRemainsHigh)
				{
					Candidate.TimeToAlignmentDeadline = Candidate.TimeToPredictedContact;
				}
			}
		}

		if (Candidate.ActiveParryWindow.IsValid()
			&& Candidate.ActiveParryWindow.Kind == EAttackWindowKind::Parry
			&& Candidate.ActiveParryWindow.AttackInstance == Candidate.AttackInstance)
		{
			const double Remaining = Candidate.ActiveParryWindow.SimulationEndTime - SimulationNow;
			if (Remaining >= 0.0 && Remaining <= static_cast<double>(TNumericLimits<float>::Max()))
			{
				Candidate.TimeToParryWindowEnd = static_cast<float>(Remaining);
				Candidate.TimeToAlignmentDeadline = Candidate.TimeToAlignmentDeadline >= 0.0f
					? FMath::Min(Candidate.TimeToAlignmentDeadline, Candidate.TimeToParryWindowEnd)
					: Candidate.TimeToParryWindowEnd;
			}
		}

		Candidates.Add(MoveTemp(Candidate));
	}

	FDefenseThreatSelectionContext Context;
	Context.LockedThreatId = LockedDefenseThreatId;
	Context.LockAgeSeconds = DefenseThreatLockAcquiredSimulationTime >= 0.0
		? static_cast<float>(FMath::Max(0.0, SimulationNow - DefenseThreatLockAcquiredSimulationTime))
		: TNumericLimits<float>::Max();
	Context.ThreatLockMinSeconds = DefenseConfig
		? FMath::Max(0.0f, DefenseConfig->ThreatLockMinSeconds)
		: 0.15f;
	Context.ThreatSwitchLeadSeconds = DefenseConfig
		? FMath::Max(0.0f, DefenseConfig->ThreatSwitchLeadSeconds)
		: 0.10f;
	Context.HardGuardConeHalfAngle = DefenseConfig ? DefenseConfig->HardGuardConeHalfAngle : 70.0f;
	Context.MaximumAutomaticTurn = DefenseConfig ? DefenseConfig->MaximumAutomaticTurn : 70.0f;
	Context.RemainingAutomaticTurn = LockedDefenseThreatId.IsValid()
		? RemainingDefenseAutomaticTurn
		: Context.MaximumAutomaticTurn;
	Context.DefenseTurnRate = DefenseConfig ? DefenseConfig->DefenseTurnRate : 180.0f;
	Context.PerfectParryFinalTolerance = DefenseConfig
		? DefenseConfig->PerfectParryFinalTolerance
		: 10.0f;
	Context.CurrentSimulationTime = SimulationNow;
	Context.MaximumHighConfidencePredictionAge = MaximumPredictionAge;

	Result = FDefenseResolver::SelectThreat(Candidates, Context);
	const bool bPreviousThreatStillCandidate = PreviousLockedThreatId.IsValid()
		&& Candidates.ContainsByPredicate(
			[&](const FAttackExecutionSnapshot& Candidate)
			{
				return Candidate.StableId == PreviousLockedThreatId
					&& Candidate.AttackInstance == LockedDefenseThreat.AttackInstance
					&& FDefenseResolver::IsSelectableThreat(Candidate);
			});
	bGuardThreatCandidatesExist = Result.bFound;
	if (!Result.bFound)
	{
		FDefenseTelemetryRecord Telemetry;
		Telemetry.Event = EDefenseTelemetryEvent::ThreatSelection;
		Telemetry.SimulationTimestamp = SimulationNow;
		Telemetry.LockedThreatStableId = PreviousLockedThreatId;
		Telemetry.CandidateDisposition = TEXT("None");
		Telemetry.ThreatSwitchReason = PreviousLockedThreatId.IsValid()
			? TEXT("NoValidCandidate")
			: TEXT("NoCandidate");
		Telemetry.MaximumTurnRate = Context.DefenseTurnRate;
		Telemetry.RemainingTurnBudget = Context.RemainingAutomaticTurn;
		AppendDefenseTelemetry(MoveTemp(Telemetry));
		LockedDefenseThreat = {};
		LockedDefenseThreatActor.Reset();
		LockedDefenseThreatId = {};
		DefenseThreatLockAcquiredSimulationTime = -1.0;
		RemainingDefenseAutomaticTurn = 0.0f;
		return Result;
	}

	const bool bNewThreatInteraction = !LockedDefenseThreatId.IsValid()
		|| Result.SelectedThreat.StableId != LockedDefenseThreatId
		|| !(Result.SelectedThreat.AttackInstance == LockedDefenseThreat.AttackInstance);
	if (bNewThreatInteraction)
	{
		if (GuardAlignmentRequestHandle.IsValid())
		{
			Targeting->ReleaseAlignmentRequest(GuardAlignmentRequestHandle);
			GuardAlignmentRequestHandle = {};
		}
		GuardAlignmentGeneration = GuardAlignmentGeneration == MAX_int32
			? 1
			: GuardAlignmentGeneration + 1;
		DefenseThreatLockAcquiredSimulationTime = SimulationNow;
		RemainingDefenseAutomaticTurn = Context.MaximumAutomaticTurn;
	}
	LockedDefenseThreat = Result.SelectedThreat;
	LockedDefenseThreatId = Result.SelectedThreat.StableId;
	LockedDefenseThreatActor = Result.SelectedThreat.AttackInstance.Attacker;

	FDefenseTelemetryRecord Telemetry;
	Telemetry.Event = EDefenseTelemetryEvent::ThreatSelection;
	Telemetry.SimulationTimestamp = SimulationNow;
	Telemetry.AttackInstance = Result.SelectedThreat.AttackInstance;
	Telemetry.AttackWindow = Result.SelectedThreat.ActiveParryWindow;
	Telemetry.Attacker = Result.SelectedThreat.AttackInstance.Attacker;
	Telemetry.Candidate = Result.SelectedThreat.AttackInstance.Attacker;
	Telemetry.AttackerStableId = Result.SelectedThreat.StableId;
	Telemetry.CandidateStableId = Result.SelectedThreat.StableId;
	Telemetry.LockedThreatStableId = Result.SelectedThreat.StableId;
	Telemetry.CandidateDisposition = bNewThreatInteraction ? TEXT("Selected") : TEXT("Retained");
	Telemetry.ThreatSwitchReason = !PreviousLockedThreatId.IsValid()
		? TEXT("InitialLock")
		: PreviousLockedThreatId == Result.SelectedThreat.StableId
			? TEXT("LockRetained")
			: bPreviousThreatStillCandidate
				? TEXT("EarlierDeadline")
				: TEXT("CurrentInvalid");
	Telemetry.PredictedHeight = Result.SelectedThreat.PredictedContact.Height;
	Telemetry.PredictedLane = Result.SelectedThreat.PredictedContact.Lane;
	Telemetry.PredictedSwing = Result.SelectedThreat.SwingShape;
	Telemetry.PredictedAxis = Result.SelectedThreat.PredictedContact.PathDirection;
	Telemetry.InitialYawError = Result.SelectedThreat.RelativeYawDegrees;
	Telemetry.RemainingYawError = Result.SelectedThreat.RelativeYawDegrees;
	Telemetry.TimeToDeadline = Result.SelectedThreat.TimeToAlignmentDeadline;
	Telemetry.MaximumTurnRate = Context.DefenseTurnRate;
	Telemetry.RemainingTurnBudget = RemainingDefenseAutomaticTurn;
	if (Result.SelectedThreat.AttackData)
	{
		Telemetry.AttackDataPath = FSoftObjectPath(
			Result.SelectedThreat.AttackData->GetPathName());
	}
	AppendDefenseTelemetry(MoveTemp(Telemetry));
	return Result;
}

void UCombatComponent::RefreshGuardThreat(EThreatRefreshReason Reason)
{
	RefreshGuardThreatInternal(Reason, false);
}

void UCombatComponent::RefreshGuardThreatInternal(
	EThreatRefreshReason Reason,
	const bool bForceRevalidation)
{
	if (!bIsBlocking)
	{
		return;
	}
	if (CachedPairedAnimComp && CachedPairedAnimComp->IsPairedAnimationActive())
	{
		ClearGuardThreat(EThreatClearReason::PairedTakeover);
		return;
	}
	if (bGuardThreatRefreshInProgress)
	{
		return;
	}
	if (!bForceRevalidation && LastGuardThreatRefreshFrame == GFrameCounter)
	{
		if (UWorld* World = GetWorld(); World
			&& !World->GetTimerManager().IsTimerActive(CoalescedGuardThreatRefreshTimerHandle))
		{
			CoalescedGuardThreatRefreshTimerHandle = World->GetTimerManager().SetTimerForNextTick(
				this,
				&UCombatComponent::HandleCoalescedGuardThreatRefresh);
		}
		return;
	}

	TGuardValue<bool> RefreshGuard(bGuardThreatRefreshInProgress, true);
	LastGuardThreatRefreshFrame = GFrameCounter;
	const double SimulationNow = GetWorld() ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
	const FDefenseThreatSelectionResult Selection = SelectDefenseThreat(SimulationNow);
	if (!Selection.bFound)
	{
		ClearGuardThreat(EThreatClearReason::NoCandidates);
		return;
	}

	const UDefenseConfiguration* DefenseConfig = GetEffectiveDefenseConfiguration();
	UpdateGuardAlignmentRequest();
	const float Interval = DefenseConfig
		? FMath::Max(0.0f, DefenseConfig->GuardedThreatRefreshSeconds)
		: 0.05f;
	if (UWorld* World = GetWorld(); Interval > 0.0f && World
		&& !World->GetTimerManager().IsTimerActive(GuardThreatRefreshTimerHandle))
	{
		World->GetTimerManager().SetTimer(
			GuardThreatRefreshTimerHandle,
			this,
			&UCombatComponent::HandleGuardThreatRefreshTimer,
			Interval,
			true,
			Interval);
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Verbose, TEXT("[DEFENSE THREAT] Refresh reason=%s selected=%llu"),
			*UEnum::GetValueAsString(Reason),
			LockedDefenseThreatId.Value);
	}
}

void UCombatComponent::UpdateGuardAlignmentRequest()
{
	ABaseCombatCharacter* Defender = GetOwnerCharacter();
	UTargetingComponent* Targeting = Defender ? Defender->GetTargetingComponent() : nullptr;
	if (!Targeting || !LockedDefenseThreatActor.IsValid())
	{
		return;
	}

	if (GuardAlignmentRequestHandle.IsValid())
	{
		FAlignmentRequestSpec CurrentSpec;
		if (Targeting->GetAlignmentRequestSpec(GuardAlignmentRequestHandle, CurrentSpec))
		{
			RemainingDefenseAutomaticTurn = CurrentSpec.RemainingTurnBudget;
		}
		else
		{
			GuardAlignmentRequestHandle = {};
		}
	}

	const UDefenseConfiguration* DefenseConfig = GetEffectiveDefenseConfiguration();
	FAlignmentRequestSpec AlignmentSpec;
	AlignmentSpec.OwnerId = TEXT("GuardFacing");
	AlignmentSpec.OwnerGeneration = FMath::Max(1, GuardAlignmentGeneration);
	AlignmentSpec.Priority = EDefenseAlignmentPriority::GuardFacing;
	AlignmentSpec.Executor = EAlignmentExecutor::CharacterMovement;
	AlignmentSpec.MaximumTurnRate = DefenseConfig
		? FMath::Max(0.0f, DefenseConfig->DefenseTurnRate)
		: 180.0f;
	AlignmentSpec.RemainingTurnBudget = FMath::Max(0.0f, RemainingDefenseAutomaticTurn);
	AlignmentSpec.MaximumTranslation = 0.0f;

	if (bGuardManualOverrideActive)
	{
		const float Threshold = DefenseConfig
			? FMath::Clamp(DefenseConfig->GuardManualOverrideThreshold, 0.0f, 1.0f)
			: 0.25f;
		const float ManualMagnitude = FMath::Abs(DefenseManualYawInput);
		const bool bHasManualDirection = ManualMagnitude > UE_SMALL_NUMBER
			&& ManualMagnitude >= Threshold;
		const float CurrentYaw = static_cast<float>(Defender->GetActorRotation().Yaw);
		AlignmentSpec.Target.Reset();
		AlignmentSpec.bTrackTargetRotation = false;
		AlignmentSpec.DesiredRotation = FRotator(
			0.0f,
			CurrentYaw + (bHasManualDirection ? FMath::Sign(DefenseManualYawInput) * 90.0f : 0.0f),
			0.0f);
	}
	else
	{
		AlignmentSpec.Target = LockedDefenseThreatActor;
		AlignmentSpec.bTrackTargetRotation = true;
	}

	if (GuardAlignmentRequestHandle.IsValid()
		&& !Targeting->UpdateAlignmentRequest(GuardAlignmentRequestHandle, AlignmentSpec))
	{
		Targeting->ReleaseAlignmentRequest(GuardAlignmentRequestHandle);
		GuardAlignmentRequestHandle = {};
	}
	if (!GuardAlignmentRequestHandle.IsValid())
	{
		GuardAlignmentRequestHandle = Targeting->AcquireAlignmentRequest(AlignmentSpec);
	}
}

void UCombatComponent::SetDefenseManualYawInput(float NormalizedYawInput)
{
	SetDefenseManualYawInputAtTime(NormalizedYawInput, FPlatformTime::Seconds());
}

#if WITH_AUTOMATION_TESTS
void UCombatComponent::SetDefenseManualYawInputForTesting(
	float NormalizedYawInput,
	double UnscaledNow)
{
	SetDefenseManualYawInputAtTime(NormalizedYawInput, UnscaledNow);
}
#endif

void UCombatComponent::SetDefenseManualYawInputAtTime(
	float NormalizedYawInput,
	double UnscaledNow)
{
	if (!bIsBlocking || !FMath::IsFinite(UnscaledNow))
	{
		ResetDefenseManualYawOverride();
		return;
	}

	DefenseManualYawInput = FMath::IsFinite(NormalizedYawInput)
		? FMath::Clamp(NormalizedYawInput, -1.0f, 1.0f)
		: 0.0f;
	const UDefenseConfiguration* DefenseConfig = GetEffectiveDefenseConfiguration();
	const float Threshold = DefenseConfig
		? FMath::Clamp(DefenseConfig->GuardManualOverrideThreshold, 0.0f, 1.0f)
		: 0.25f;
	const float ManualMagnitude = FMath::Abs(DefenseManualYawInput);
	if (ManualMagnitude > UE_SMALL_NUMBER && ManualMagnitude >= Threshold)
	{
		CancelGuardManualResume();
		bGuardManualOverrideActive = true;
		GuardManualInputBelowThresholdRealTime = -1.0;
		UpdateGuardAlignmentRequest();
		return;
	}

	if (!bGuardManualOverrideActive)
	{
		return;
	}
	if (GuardManualInputBelowThresholdRealTime < 0.0)
	{
		GuardManualInputBelowThresholdRealTime = UnscaledNow;
	}

	const double ResumeDelay = DefenseConfig
		? static_cast<double>(FMath::Max(0.0f, DefenseConfig->GuardAutoFacingResumeSeconds))
		: 0.10;
	if (UnscaledNow - GuardManualInputBelowThresholdRealTime + UE_DOUBLE_SMALL_NUMBER < ResumeDelay)
	{
		ScheduleGuardManualResume(
			ResumeDelay - (UnscaledNow - GuardManualInputBelowThresholdRealTime));
		UpdateGuardAlignmentRequest();
		return;
	}

	CancelGuardManualResume();
	bGuardManualOverrideActive = false;
	GuardManualInputBelowThresholdRealTime = -1.0;
	RefreshGuardThreatInternal(EThreatRefreshReason::ManualRevalidation, true);
}

void UCombatComponent::ScheduleGuardManualResume(const double DelaySeconds)
{
	if (GuardManualResumeTickerHandle.IsValid())
	{
		return;
	}

	GuardManualResumeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&UCombatComponent::HandleGuardManualResumeTicker),
		static_cast<float>(FMath::Max(0.0, DelaySeconds)));
}

void UCombatComponent::CancelGuardManualResume()
{
	if (!GuardManualResumeTickerHandle.IsValid())
	{
		return;
	}

	FTSTicker::GetCoreTicker().RemoveTicker(GuardManualResumeTickerHandle);
	GuardManualResumeTickerHandle.Reset();
}

bool UCombatComponent::HandleGuardManualResumeTicker(const float DeltaTime)
{
	(void)DeltaTime;
	GuardManualResumeTickerHandle.Reset();
	SetDefenseManualYawInputAtTime(DefenseManualYawInput, FPlatformTime::Seconds());
	return false;
}

void UCombatComponent::ResetDefenseManualYawOverride()
{
	CancelGuardManualResume();
	DefenseManualYawInput = 0.0f;
	GuardManualInputBelowThresholdRealTime = -1.0;
	bGuardManualOverrideActive = false;
}

void UCombatComponent::ClearGuardThreat(EThreatClearReason Reason)
{
	if (GuardAlignmentRequestHandle.IsValid())
	{
		if (ABaseCombatCharacter* Character = GetOwnerCharacter())
		{
			if (UTargetingComponent* Targeting = Character->GetTargetingComponent())
			{
				Targeting->ReleaseAlignmentRequest(GuardAlignmentRequestHandle);
			}
		}
		GuardAlignmentRequestHandle = {};
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GuardThreatRefreshTimerHandle);
		if (Reason != EThreatClearReason::NoCandidates)
		{
			World->GetTimerManager().ClearTimer(CoalescedGuardThreatRefreshTimerHandle);
		}
	}
	LockedDefenseThreat = {};
	LockedDefenseThreatActor.Reset();
	LockedDefenseThreatId = {};
	DefenseThreatLockAcquiredSimulationTime = -1.0;
	RemainingDefenseAutomaticTurn = 0.0f;
	bGuardThreatCandidatesExist = false;
	if (Reason != EThreatClearReason::NoCandidates)
	{
		LastGuardThreatRefreshFrame = MAX_uint64;
		ResetDefenseManualYawOverride();
	}

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Verbose, TEXT("[DEFENSE THREAT] Cleared reason=%s"),
			*UEnum::GetValueAsString(Reason));
	}
}

const UDefenseConfiguration* UCombatComponent::GetEffectiveDefenseConfiguration() const
{
	uint64 NewestOverrideId = 0;
	const UDefenseConfiguration* NewestOverride = nullptr;
	for (const TPair<uint64, TObjectPtr<UDefenseConfiguration>>& Override : DefenseStanceOverrides)
	{
		if (Override.Key > NewestOverrideId && IsValid(Override.Value))
		{
			NewestOverrideId = Override.Key;
			NewestOverride = Override.Value;
		}
	}
	if (NewestOverride)
	{
		return NewestOverride;
	}

	if (IsValid(DefenseConfigurationOverride))
	{
		return DefenseConfigurationOverride;
	}

	const UCombatSettings* EffectiveCombatSettings = CombatSettings;
	if (const ABaseCombatCharacter* Character = GetOwnerCharacter())
	{
		// The owner's current assignment is authoritative for dynamic/test-world setup.
		EffectiveCombatSettings = Character->CombatSettings;
	}
	if (EffectiveCombatSettings && IsValid(EffectiveCombatSettings->DefenseConfiguration))
	{
		return EffectiveCombatSettings->DefenseConfiguration;
	}

	return GetDefault<UDefenseConfiguration>();
}

FDefenseConfigurationOverrideHandle UCombatComponent::AcquireDefenseStanceOverride(
	UDefenseConfiguration* Configuration)
{
	if (!IsValid(Configuration))
	{
		return {};
	}

	uint64 OverrideId = NextDefenseStanceOverrideId++;
	if (OverrideId == 0)
	{
		OverrideId = NextDefenseStanceOverrideId++;
	}
	DefenseStanceOverrides.Add(OverrideId, Configuration);
	return FDefenseConfigurationOverrideHandle(OverrideId);
}

bool UCombatComponent::ReleaseDefenseStanceOverride(FDefenseConfigurationOverrideHandle Handle)
{
	return Handle.IsValid() && DefenseStanceOverrides.Remove(Handle.Value) > 0;
}

void UCombatComponent::HandleGuardThreatRefreshTimer()
{
	RefreshGuardThreat(EThreatRefreshReason::GuardedTimer);
}

void UCombatComponent::HandleCoalescedGuardThreatRefresh()
{
	CoalescedGuardThreatRefreshTimerHandle.Invalidate();
	RefreshGuardThreatInternal(EThreatRefreshReason::ManualRevalidation, true);
}

FDefenseQuery UCombatComponent::BuildDefenseInputQuery(
	const double BlockPressSimulationTime,
	const double BlockPressUnscaledTime) const
{
	FDefenseQuery Query;
	Query.Stage = EDefenseQueryStage::InputIntent;
	Query.Attack = LockedDefenseThreat;
	Query.Defender = GetOwner();
	Query.DefenderStableId = CombatantStableId;
	Query.DefenderTransform = GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity;
	Query.BlockPressSimulationTime = BlockPressSimulationTime;
	Query.BlockPressUnscaledTime = BlockPressUnscaledTime;
	Query.CurrentSimulationTime = BlockPressSimulationTime;
	Query.RelativeYawDegrees = Query.Attack.RelativeYawDegrees;
	Query.TimeToAlignmentDeadline = Query.Attack.TimeToAlignmentDeadline;
	Query.LockedThreatId = LockedDefenseThreatId;
	Query.ThreatLockAgeSeconds = DefenseThreatLockAcquiredSimulationTime >= 0.0
		? static_cast<float>(FMath::Max(
			0.0,
			BlockPressSimulationTime - DefenseThreatLockAcquiredSimulationTime))
		: 0.0f;
	Query.bHasSelectedThreat = Query.Attack.AttackInstance.IsValid();

	const ABaseCombatCharacter* Defender = GetOwnerCharacter();
	Query.DefenderTeam = Defender ? Defender->TeamId : ETeamId::Neutral;
	Query.bDefenderAlive = Defender && !Defender->IsDeadOrDying();
	Query.bDefenderPaired = CachedPairedAnimComp && CachedPairedAnimComp->IsPairedAnimationActive();
	Query.bDefenderCanGuard = Query.bDefenderAlive
		&& !Query.bDefenderPaired
		&& !CurrentAttackData
		&& CurrentPhase == EAttackPhase::None;
	Query.bDefenderGuarding = bIsBlocking;
	Query.bDefenderCanBeDamaged = Defender
		&& Defender->HitReactionComponent
		&& Defender->HitReactionComponent->CanBeDamaged();
	Query.bDefenderInIFrames = Defender
		&& Defender->HitReactionComponent
		&& Defender->HitReactionComponent->IsInIFrames();

	const UDefenseConfiguration* Configuration = GetEffectiveDefenseConfiguration();
	if (Configuration)
	{
		Query.MaximumHighConfidencePredictionAge =
			Configuration->MaximumHighConfidencePredictionAge;
		Query.HardGuardConeHalfAngle = Configuration->HardGuardConeHalfAngle;
		Query.MaximumAutomaticTurn = Configuration->MaximumAutomaticTurn;
		Query.RemainingAutomaticTurn = FMath::Min(
			Configuration->MaximumAutomaticTurn,
			FMath::Max(0.0f, RemainingDefenseAutomaticTurn));
		Query.DefenseTurnRate = Configuration->DefenseTurnRate;
		Query.NormalBlockFinalTolerance = Configuration->NormalBlockFinalTolerance;
		Query.PerfectParryFinalTolerance = Configuration->PerfectParryFinalTolerance;
	}

	if (const UCombatComponent* SourceCombat = Query.Attack.AttackInstance.Attacker.IsValid()
		? Query.Attack.AttackInstance.Attacker->FindComponentByClass<UCombatComponent>()
		: nullptr)
	{
		Query.Attack.bAttackConsumed = SourceCombat->IsAttackConsumed(Query.Attack.AttackInstance);
		Query.Attack.bAttackIdentityCurrent =
			SourceCombat->GetCurrentAttackGeneration() == Query.Attack.AttackInstance.AttackGeneration;
		Query.Attack.bAttackActive = Query.Attack.bAttackIdentityCurrent
			&& SourceCombat->GetCurrentAttack() == Query.Attack.AttackData
			&& !Query.Attack.bAttackConsumed;
	}

	return Query;
}

bool UCombatComponent::TryCommitPerfectParry(
	const double BlockPressSimulationTime,
	const double BlockPressUnscaledTime)
{
	FDefenseQuery Query = BuildDefenseInputQuery(
		BlockPressSimulationTime,
		BlockPressUnscaledTime);
	LastInputDefenseResolution = {};
	LastInputDefenseResolution.Stage = EDefenseQueryStage::InputIntent;
	LastInputDefenseResolution.PredictedContact = Query.Attack.PredictedContact;
	LastInputDefenseResolution.Decision = FDefenseResolver::Resolve(Query);

	if (LastInputDefenseResolution.Decision.Outcome == EDefenseOutcome::PerfectParry
		&& HasRegisteredDefenseContactForAttack(Query.Attack.AttackInstance))
	{
		LastInputDefenseResolution.Decision.Outcome = EDefenseOutcome::GuardEntered;
		LastInputDefenseResolution.Decision.Reason = EDefenseReason::Duplicate;
		LastInputDefenseResolution.Decision.AttackerResponse = EAttackerResponse::None;
		LastInputDefenseResolution.Decision.AlignmentPolicy = EDefenseAlignmentPolicy::GuardFacing;
		LastInputDefenseResolution.Decision.bChainEligible = false;
	}

	if (LastInputDefenseResolution.Decision.Outcome != EDefenseOutcome::PerfectParry)
	{
		return false;
	}

	FDefenseInteractionKey Key;
	Key.Stage = EDefenseQueryStage::InputIntent;
	Key.AttackInstance = Query.Attack.AttackInstance;
	Key.Defender = GetOwner();
	FDefenseContactReceipt ExistingReceipt;
	FDefenseInteractionId InteractionId;
	const EDefenseCommitStatus Registration = BeginDefenseInteraction(
		Key,
		InteractionId,
		ExistingReceipt);
	if (Registration != EDefenseCommitStatus::NewCommit)
	{
		if (Registration == EDefenseCommitStatus::Cached)
		{
			LastInputDefenseResolution = ExistingReceipt.Resolution;
		}
		return Registration == EDefenseCommitStatus::Cached
			&& LastInputDefenseResolution.Decision.Outcome == EDefenseOutcome::PerfectParry;
	}

	LastInputDefenseResolution.InteractionId = InteractionId;
	ABaseCombatCharacter* SourceCharacter = Cast<ABaseCombatCharacter>(
		Query.Attack.AttackInstance.Attacker.Get());
	UCombatComponent* SourceCombat = SourceCharacter
		? SourceCharacter->CombatComponent.Get()
		: nullptr;
	const UDefenseConfiguration* DefenderConfiguration = GetEffectiveDefenseConfiguration();
	const UDefenseConfiguration* AttackerConfiguration = SourceCombat
		? SourceCombat->GetEffectiveDefenseConfiguration()
		: GetDefault<UDefenseConfiguration>();
	SelectPerfectParryPresentation(
		LastInputDefenseResolution,
		DefenderConfiguration,
		AttackerConfiguration);
	const FDefenseResolution CommittedResolution = LastInputDefenseResolution;
	const float ConfiguredStaggerDuration = AttackerConfiguration
		? AttackerConfiguration->ParryStaggerDuration
		: 1.5f;
	const float StaggerDuration = FMath::IsFinite(ConfiguredStaggerDuration)
		&& ConfiguredStaggerDuration > 0.0f
		? ConfiguredStaggerDuration
		: 1.5f;
	TWeakObjectPtr<UCombatComponent> WeakDefenderCombat(this);
	TWeakObjectPtr<ABaseCombatCharacter> WeakDefender(GetOwnerCharacter());
	TWeakObjectPtr<ABaseCombatCharacter> WeakSourceCharacter(SourceCharacter);
	TWeakObjectPtr<UCombatComponent> WeakSourceCombat(SourceCombat);

	if (!SourceCombat
		|| !SourceCombat->ConsumeActiveAttackInternal(
			Query.Attack.AttackInstance,
			EAttackConsumeReason::PerfectParry,
			InteractionId))
	{
		if (!WeakDefenderCombat.IsValid())
		{
			return false;
		}

		LastInputDefenseResolution = CommittedResolution;
		LastInputDefenseResolution.Decision.Outcome = EDefenseOutcome::GuardEntered;
		LastInputDefenseResolution.Decision.Reason = EDefenseReason::Consumed;
		LastInputDefenseResolution.Decision.AttackerResponse = EAttackerResponse::None;
		LastInputDefenseResolution.Decision.AlignmentPolicy = EDefenseAlignmentPolicy::GuardFacing;
		LastInputDefenseResolution.Decision.bChainEligible = false;
		FDefenseContactReceipt DowngradedReceipt;
		DowngradedReceipt.Resolution = LastInputDefenseResolution;
		DowngradedReceipt.CommitStatus = EDefenseCommitStatus::NewCommit;
		FinalizeDefenseInteraction(InteractionId, DowngradedReceipt);
		return false;
	}

	FDefenseInteractionCacheRecord* InteractionRecord = DefenseInteractionCache.Find(InteractionId.Key);
	const bool bExactInteractionInProgress = InteractionRecord
		&& !InteractionRecord->bFinalized
		&& InteractionRecord->Id == InteractionId;
	ABaseCombatCharacter* DefenderCharacter = WeakDefender.Get();
	ABaseCombatCharacter* SurvivingSourceCharacter = WeakSourceCharacter.Get();
	UCombatComponent* SurvivingSourceCombat = WeakSourceCombat.Get();
	const bool bParticipantsStillValid = WeakDefenderCombat.Get() == this
		&& IsValid(DefenderCharacter)
		&& !DefenderCharacter->IsDeadOrDying()
		&& DefenderCharacter->CombatComponent.Get() == this
		&& IsValid(SurvivingSourceCharacter)
		&& !SurvivingSourceCharacter->IsDeadOrDying()
		&& SurvivingSourceCharacter->CombatComponent.Get() == SurvivingSourceCombat
		&& IsValid(SurvivingSourceCombat)
		&& SurvivingSourceCombat->IsAttackConsumed(Query.Attack.AttackInstance)
		&& LastInputDefenseResolution.InteractionId == InteractionId;
	if (!bExactInteractionInProgress || !bParticipantsStillValid)
	{
		if (!WeakDefenderCombat.IsValid())
		{
			return false;
		}

		if (InteractionRecord && InteractionRecord->bFinalized && InteractionRecord->Id == InteractionId)
		{
			LastInputDefenseResolution = InteractionRecord->Receipt.Resolution;
			return false;
		}

		FDefenseResolution InvalidResolution = CommittedResolution;
		InvalidResolution.Decision.Outcome = EDefenseOutcome::IgnoredInvalid;
		InvalidResolution.Decision.Reason = EDefenseReason::InvalidParticipant;
		InvalidResolution.Decision.AttackerResponse = EAttackerResponse::None;
		InvalidResolution.Decision.AlignmentPolicy = EDefenseAlignmentPolicy::None;
		InvalidResolution.Decision.bChainEligible = false;
		InvalidResolution.AlignmentRequest = {};
		InvalidResolution.Presentation = {};
		InvalidResolution.PresentationRow = NAME_None;
		InvalidResolution.PresentationFallback = EDefensePresentationFallbackLevel::NoPresentation;
		InvalidResolution.AttackerPresentation = {};
		InvalidResolution.AttackerPresentationRow = NAME_None;
		InvalidResolution.AttackerPresentationFallback = EDefensePresentationFallbackLevel::NoPresentation;
		LastInputDefenseResolution = InvalidResolution;

		if (bExactInteractionInProgress)
		{
			FDefenseContactReceipt InvalidReceipt;
			InvalidReceipt.Resolution = InvalidResolution;
			InvalidReceipt.CommitStatus = EDefenseCommitStatus::NewCommit;
			FinalizeDefenseInteraction(InteractionId, InvalidReceipt);
		}
		return false;
	}

	LastInputDefenseResolution = CommittedResolution;

	FDefenseContactReceipt Receipt;
	Receipt.Resolution = CommittedResolution;
	Receipt.CommitStatus = EDefenseCommitStatus::NewCommit;
	FinalizeDefenseInteraction(InteractionId, Receipt);
	auto ArePresentationParticipantsCurrent = [&]()
	{
		UCombatComponent* DefenderCombat = WeakDefenderCombat.Get();
		ABaseCombatCharacter* Defender = WeakDefender.Get();
		ABaseCombatCharacter* Source = WeakSourceCharacter.Get();
		UCombatComponent* CurrentSourceCombat = WeakSourceCombat.Get();
		if (!DefenderCombat
			|| DefenderCombat != this
			|| !IsValid(Defender)
			|| Defender->IsDeadOrDying()
			|| Defender->CombatComponent.Get() != DefenderCombat
			|| !IsValid(Source)
			|| Source->IsDeadOrDying()
			|| !CurrentSourceCombat
			|| Source->CombatComponent.Get() != CurrentSourceCombat
			|| !CurrentSourceCombat->IsAttackConsumed(Query.Attack.AttackInstance)
			|| DefenderCombat->LastInputDefenseResolution.InteractionId != InteractionId)
		{
			return false;
		}

		const FDefenseInteractionCacheRecord* FinalizedRecord =
			DefenderCombat->DefenseInteractionCache.Find(InteractionId.Key);
		return FinalizedRecord
			&& FinalizedRecord->bFinalized
			&& FinalizedRecord->Id == InteractionId
			&& FinalizedRecord->Receipt.Resolution.InteractionId == InteractionId;
	};

	UPairedAnimationComponent* DefenseSequence = CachedPairedAnimComp;
	if (!DefenseSequence && GetOwner())
	{
		DefenseSequence = GetOwner()->FindComponentByClass<UPairedAnimationComponent>();
	}
	TWeakObjectPtr<UPairedAnimationComponent> WeakDefenseSequence(DefenseSequence);
	const bool bDefenseSequenceStarted = DefenseSequence
		&& DefenseSequence->BeginDefenseSequence(CommittedResolution);
	DefenseSequence = WeakDefenseSequence.Get();
	if (!ArePresentationParticipantsCurrent())
	{
		return true;
	}
	const bool bPairedBridgeStarted = bDefenseSequenceStarted
		&& DefenseSequence
		&& DefenseSequence->IsPairedAnimationActive();
	FDefenseResolution DirectPresentationResolution = CommittedResolution;
	if (bPairedBridgeStarted)
	{
		// The paired bridge owns both montage roles; direct presentation still owns FX.
		DirectPresentationResolution.Presentation.Montage = nullptr;
		DirectPresentationResolution.AttackerPresentation.Montage = nullptr;
	}

	if (ABaseCombatCharacter* Defender = WeakDefender.Get())
	{
		if (Defender->HitReactionComponent)
		{
			Defender->HitReactionComponent->PlayDefensePresentation(DirectPresentationResolution);
		}
	}
	if (!ArePresentationParticipantsCurrent())
	{
		return true;
	}
	SurvivingSourceCharacter = WeakSourceCharacter.Get();
	if (IsValid(SurvivingSourceCharacter) && SurvivingSourceCharacter->HitReactionComponent)
	{
		TWeakObjectPtr<UHitReactionComponent> WeakSourceHitReaction(
			SurvivingSourceCharacter->HitReactionComponent);
		const bool bResponsePresented =
			bPairedBridgeStarted
			|| SurvivingSourceCharacter->HitReactionComponent->PlayAttackerResponse(
				DirectPresentationResolution);
		if (!ArePresentationParticipantsCurrent())
		{
			return true;
		}
		if (UHitReactionComponent* SourceHitReaction = WeakSourceHitReaction.Get())
		{
			SourceHitReaction->ApplyStagger(
				StaggerDuration,
				!bResponsePresented);
		}
	}

	if (WeakDefenderCombat.IsValid())
	{
		OnDefenseResolvedNative.Broadcast(CommittedResolution);
	}
	return true;
}

bool UCombatComponent::BeginBlock(AActor* ThreatActor)
{
	(void)ThreatActor;
	ABaseCombatCharacter* Character = GetOwnerCharacter();
	if (!Character || Character->IsDeadOrDying())
	{
		return false;
	}

	// Do not turn normal block into a free interrupt for active attacks.
	if (CurrentAttackData || CurrentPhase != EAttackPhase::None)
	{
		return false;
	}

	ResetDefenseManualYawOverride();
	bIsBlocking = true;
	RefreshGuardThreatInternal(EThreatRefreshReason::BlockPressed, true);
	AActor* BlockThreat = LockedDefenseThreatActor.IsValid()
		? LockedDefenseThreatActor.Get()
		: nullptr;

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[BLOCK] %s began blocking%s%s"),
			*Character->GetName(),
			BlockThreat ? TEXT(" toward ") : TEXT(""),
			BlockThreat ? *BlockThreat->GetName() : TEXT(""));
	}

	return true;
}

void UCombatComponent::EndBlock()
{
	if (!bIsBlocking)
	{
		ClearGuardThreat(EThreatClearReason::BlockReleased);
		return;
	}

	bIsBlocking = false;
	ClearGuardThreat(EThreatClearReason::BlockReleased);

	if (GetDebugDraw())
	{
		UE_LOG(LogCombat, Log, TEXT("[BLOCK] %s ended blocking"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
	}
}

bool UCombatComponent::CanBlockAttackFrom(AActor* Attacker) const
{
	if (!bIsBlocking || !Attacker)
	{
		return false;
	}

	const ABaseCombatCharacter* Character = OwnerCharacter.Get();
	if (!Character)
	{
		Character = Cast<ABaseCombatCharacter>(GetOwner());
	}
	if (!Character)
	{
		return false;
	}

	FVector ToAttacker = Attacker->GetActorLocation() - Character->GetActorLocation();
	ToAttacker.Z = 0.0f;
	if (ToAttacker.IsNearlyZero())
	{
		return true;
	}

	const float Dot = FVector::DotProduct(Character->GetActorForwardVector().GetSafeNormal2D(), ToAttacker.GetSafeNormal());
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
	return AngleDegrees <= BlockFacingConeHalfAngle;
}

bool UCombatComponent::CanBlockHit(const FHitReactionInfo& HitInfo) const
{
	if (!CanBlockAttackFrom(HitInfo.Attacker))
	{
		return false;
	}

	if (IsAttackTaggedUnblockable(HitInfo.AttackData))
	{
		return false;
	}

	return true;
}

void UCombatComponent::AddActiveContextTag(FGameplayTag ContextTag)
{
	const FCombatContextLeaseHandle Handle = AcquireContextTagLease(ContextTag, TEXT("LegacyAdapter"));
	if (Handle.IsValid())
	{
		LegacyContextTagLeases.FindOrAdd(ContextTag).Add(Handle);
	}
}

void UCombatComponent::RemoveActiveContextTag(FGameplayTag ContextTag)
{
	if (TArray<FCombatContextLeaseHandle>* Handles = LegacyContextTagLeases.Find(ContextTag))
	{
		if (!Handles->IsEmpty())
		{
			const FCombatContextLeaseHandle Handle = Handles->Pop(EAllowShrinking::No);
			ReleaseContextTagLease(Handle);
		}
		if (Handles->IsEmpty())
		{
			LegacyContextTagLeases.Remove(ContextTag);
		}
	}
}

void UCombatComponent::ClearActiveContextTags()
{
	ActiveContextTagLeases.Reset();
	ActiveContextTagLeaseCounts.Reset();
	LegacyContextTagLeases.Reset();
	ActiveContextTags.Reset();
}

FCombatContextLeaseHandle UCombatComponent::AcquireContextTagLease(
	FGameplayTag ContextTag,
	FName Owner)
{
	if (!ContextTag.IsValid() || Owner.IsNone())
	{
		return {};
	}

	do
	{
		++NextContextTagLeaseId;
	}
	while (NextContextTagLeaseId == 0
		|| ActiveContextTagLeases.Contains(
			FCombatContextLeaseHandle(NextContextTagLeaseId)));

	const FCombatContextLeaseHandle Handle(NextContextTagLeaseId);
	FCombatContextLeaseRecord& Record = ActiveContextTagLeases.Add(Handle);
	Record.Tag = ContextTag;
	Record.Owner = Owner;
	int32& Count = ActiveContextTagLeaseCounts.FindOrAdd(ContextTag);
	++Count;
	ActiveContextTags.AddTag(ContextTag);
	return Handle;
}

void UCombatComponent::ReleaseContextTagLease(FCombatContextLeaseHandle Handle)
{
	FCombatContextLeaseRecord Record;
	if (!Handle.IsValid() || !ActiveContextTagLeases.RemoveAndCopyValue(Handle, Record))
	{
		return;
	}

	if (int32* Count = ActiveContextTagLeaseCounts.Find(Record.Tag))
	{
		--(*Count);
		if (*Count <= 0)
		{
			ActiveContextTagLeaseCounts.Remove(Record.Tag);
			ActiveContextTags.RemoveTag(Record.Tag);
		}
	}
}

bool UCombatComponent::HasActiveContextTag(FGameplayTag ContextTag) const
{
	return ContextTag.IsValid() && ActiveContextTags.HasTag(ContextTag);
}

ECombatState UCombatComponent::GetCombatState() const
{
	const ABaseCombatCharacter* Character = OwnerCharacter.Get();
	if (!Character)
	{
		Character = Cast<ABaseCombatCharacter>(GetOwner());
	}

	if (Character && Character->IsDeadOrDying())
	{
		return ECombatState::Dead;
	}

	if (bIsBlocking)
	{
		return ECombatState::Blocking;
	}

	if (CurrentAttackData)
	{
		if (HoldState.IsHolding())
		{
			if (CurrentAttackInputType == EInputType::HeavyAttack)
			{
				return ECombatState::ChargingHeavyAttack;
			}
			if (CurrentAttackInputType == EInputType::LightAttack)
			{
				return ECombatState::HoldingLightAttack;
			}
		}

		return ECombatState::Attacking;
	}

	return ECombatState::Idle;
}

// ============================================================================
// ACTION QUEUE MANAGEMENT
// ============================================================================

void UCombatComponent::QueueAction(const FQueuedInputAction& InputAction, UAttackData* AttackData)
{
	TryQueueAction(InputAction, AttackData);
}

bool UCombatComponent::TryQueueAction(
	const FQueuedInputAction& InputAction,
	UAttackData* AttackData,
	EActionReactionTelemetryReason* OutDecisionReason)
{
	const auto SetDecisionReason = [OutDecisionReason](
		const EActionReactionTelemetryReason Reason)
	{
		if (OutDecisionReason)
		{
			*OutDecisionReason = Reason;
		}
	};
	SetDecisionReason(EActionReactionTelemetryReason::None);

	const auto EmitQueueRecord = [this, &InputAction](
		const EActionReactionTelemetryEvent Event,
		const EActionReactionTelemetryReason Reason,
		const FActionQueueEntry* Entry)
	{
		FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(Event, Reason);
		Telemetry.InputSerial = InputAction.InputSerial;
		Telemetry.InputType = InputAction.InputType;
		Telemetry.InputEvent = InputAction.EventType;
		Telemetry.InputDirection = InputAction.FacingIntent.BranchDirection;
		if (Entry)
		{
			Telemetry.QueueEntryId = Entry->QueueEntryId;
			Telemetry.ExecutionMode = Entry->ExecutionMode;
			Telemetry.ActionState = Entry->State;
			Telemetry.AttackDataPath = FSoftObjectPath(Entry->AttackData);
		}
		AppendActionReactionTelemetry(MoveTemp(Telemetry));
	};

	// Only queue press events (releases handled separately)
	if (InputAction.EventType != EInputEventType::Press)
	{
		SetDecisionReason(EActionReactionTelemetryReason::CombatStateRejected);
		EmitQueueRecord(
			EActionReactionTelemetryEvent::QueueRejected,
			EActionReactionTelemetryReason::CombatStateRejected,
			nullptr);
		return false;
	}

	// Determine execution mode
	EActionExecutionMode ExecMode = HoldState.IsHolding()
		? EActionExecutionMode::Queued
		: DetermineExecutionMode(InputAction);

	// Get attack data if not provided
	if (!AttackData)
	{
		AttackData = GetAttackForInput(InputAction.InputType);
	}

	// CRITICAL: If we couldn't resolve any attack, bail out gracefully
	// This can happen if CombatSettings is not assigned to the character
	if (!AttackData)
	{
		SetDecisionReason(EActionReactionTelemetryReason::AttackResolutionFailed);
		UE_LOG(LogCombat, Warning, TEXT("[QUEUE] Cannot queue action: No attack resolved. "
		                                "Check that CombatSettings is assigned to the character."));
		EmitQueueRecord(
			EActionReactionTelemetryEvent::QueueRejected,
			EActionReactionTelemetryReason::AttackResolutionFailed,
			nullptr);
		return false;
	}

	// Create queue entry
	FActionQueueEntry Entry(InputAction, AttackData, ExecMode);
	EnsureActionQueueEntryIdentity(Entry);
	Entry.Priority = CalculatePriority(Entry);

	// PHASE 9: Set TargetPhase for event-driven execution
	// Immediate: Execute synchronously (no phase wait)
	// Queued: Execute on Recovery phase transition (when Active ends)
	Entry.TargetPhase = (ExecMode == EActionExecutionMode::Immediate)
		? EAttackPhase::None      // Synchronous execution
		: EAttackPhase::Recovery; // Execute when Active phase ends

	// Immediate mode: Execute synchronously (right now)
	if (ExecMode == EActionExecutionMode::Immediate)
	{
		EmitQueueRecord(
			EActionReactionTelemetryEvent::QueueAccepted,
			EActionReactionTelemetryReason::None,
			&Entry);
		// COMBO-AWARE QUEUE MANAGEMENT:
		// Check if the executing action has combo branches (NextComboAttack, HeavyComboAttack, etc.)
		// If YES: Preserve ONLY valid combo inputs (enables alternating light/heavy chains)
		// If NO: Clear all pending (combo chain ended, starting fresh)

		UAttackData* ExecutingAttack = Entry.AttackData;
		bool bHasComboBranches = ExecutingAttack &&
			(ExecutingAttack->NextComboAttack != nullptr ||
			 ExecutingAttack->HeavyComboAttack != nullptr ||
			 ExecutingAttack->DirectionalFollowUps.Num() > 0 ||
			 ExecutingAttack->HeavyDirectionalFollowUps.Num() > 0);

		if (bHasComboBranches && ActionQueue.Num() > 0)
		{
			// This attack has combo branches - preserve ONLY the FIRST valid combo of each type
			// This prevents button mashing from executing entire combo chains (max 1 Light + 1 Heavy queued)
			TArray<FActionQueueEntry> ValidCombos;
			bool bHasQueuedLight = false;
			bool bHasQueuedHeavy = false;
			int32 CancelledCount = 0;

			for (FActionQueueEntry& QueuedEntry : ActionQueue)
			{
				if (!QueuedEntry.IsPending())
				{
					ValidCombos.Add(QueuedEntry); // Keep non-pending
					continue;
				}

				// Check if this queued action is a valid combo from executing attack
				bool bIsValidCombo = false;
				bool bAlreadyQueued = false;

				if (QueuedEntry.InputAction.InputType == EInputType::LightAttack)
				{
					bIsValidCombo = (ExecutingAttack->NextComboAttack != nullptr);
					bAlreadyQueued = bHasQueuedLight;
					if (bIsValidCombo && !bAlreadyQueued)
					{
						bHasQueuedLight = true; // Mark that we've queued a Light
					}
				}
				else if (QueuedEntry.InputAction.InputType == EInputType::HeavyAttack)
				{
					bIsValidCombo = (ExecutingAttack->HeavyComboAttack != nullptr);
					bAlreadyQueued = bHasQueuedHeavy;
					if (bIsValidCombo && !bAlreadyQueued)
					{
						bHasQueuedHeavy = true; // Mark that we've queued a Heavy
					}
				}

				if (bIsValidCombo && !bAlreadyQueued)
				{
					ValidCombos.Add(QueuedEntry); // Keep FIRST valid combo of this type
				}
				else
				{
					// Cancel: either invalid combo OR duplicate input (spam prevention)
					QueuedEntry.State = EActionState::Cancelled;
					AppendActionQueueTelemetry(
						QueuedEntry,
						EActionReactionTelemetryEvent::QueueCancelled,
						bAlreadyQueued
							? EActionReactionTelemetryReason::DuplicatePendingInput
							: EActionReactionTelemetryReason::InvalidComboBranch);
					QueueStats.ActionsCancelled++;
					CancelledCount++;
				}
			}

			// Replace queue with only valid combos (max 1 Light + 1 Heavy)
			ActionQueue = ValidCombos;

			if (CombatDebug::IsQueueDebugEnabled())
			{
				UE_LOG(LogCombat, Log, TEXT("[QUEUE] Combo-aware clear: Preserved %d valid combos (anti-spam), cancelled %d"),
					ValidCombos.Num(), CancelledCount);
			}
		}
		else if (ActionQueue.Num() > 0)
		{
			// No combo branches - clear all pending (starting fresh attack chain)
			int32 ClearedCount = 0;
			for (FActionQueueEntry& QueuedEntry : ActionQueue)
			{
				if (QueuedEntry.IsPending())
				{
					QueuedEntry.State = EActionState::Cancelled;
					AppendActionQueueTelemetry(
						QueuedEntry,
						EActionReactionTelemetryEvent::QueueCancelled,
						EActionReactionTelemetryReason::FreshChainReset);
					QueueStats.ActionsCancelled++;
					ClearedCount++;
				}
			}

			ActionQueue.Empty();

			if (CombatDebug::IsQueueDebugEnabled() && ClearedCount > 0)
			{
				UE_LOG(LogCombat, Warning, TEXT("[QUEUE] Cleared %d pending actions (no combo branches - chain ended)"), ClearedCount);
			}
		}

		if (CombatDebug::IsQueueDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[QUEUE] Executing IMMEDIATE action: Type=%s"),
				*UEnum::GetValueAsString(InputAction.InputType));
		}

		// Execute immediately
		const bool bExecuted = ExecuteAction(Entry);
		if (bExecuted)
		{
			QueueStats.ActionsExecuted++;
			QueueStats.ImmediateExecutions++;

			if (CombatDebug::IsQueueDebugEnabled())
			{
				UE_LOG(LogCombat, Log, TEXT("[QUEUE] Immediate execution SUCCESS"));
			}
		}
		else
		{
			if (CombatDebug::IsQueueDebugEnabled())
			{
				UE_LOG(LogCombat, Warning, TEXT("[QUEUE] Immediate execution FAILED"));
			}
		}

		SetDecisionReason(bExecuted
			? EActionReactionTelemetryReason::ImmediateExecutionSucceeded
			: EActionReactionTelemetryReason::ImmediateExecutionFailed);
		return bExecuted; // Don't add to queue
	}

	// Queued mode: Schedule for later execution at Active-end
	Entry.ScheduledTime = GetExecutionCheckpoint(Entry);

	// Light/Heavy share one newest-wins pending normal slot.
	if (InputAction.InputType == EInputType::LightAttack
		|| InputAction.InputType == EInputType::HeavyAttack)
	{
		for (int32 Index = ActionQueue.Num() - 1; Index >= 0; --Index)
		{
			FActionQueueEntry& PendingEntry = ActionQueue[Index];
			const EInputType PendingType = PendingEntry.InputAction.InputType;
			if (!PendingEntry.IsPending()
				|| (PendingType != EInputType::LightAttack
					&& PendingType != EInputType::HeavyAttack))
			{
				continue;
			}

			PendingEntry.State = EActionState::Cancelled;
			AppendActionQueueTelemetry(
				PendingEntry,
				EActionReactionTelemetryEvent::QueueCancelled,
				EActionReactionTelemetryReason::PendingInputReplaced);
			if (PendingEntry.InputAction.InputSerial != 0)
			{
				FinalizeCombatInput(
					PendingEntry.InputAction.InputSerial,
					ECombatInputRoute::NormalQueue,
					ECombatInputDisposition::Replaced,
					EActionReactionTelemetryReason::PendingInputReplaced);
			}
			++QueueStats.ActionsCancelled;
			ActionQueue.RemoveAt(Index, 1, EAllowShrinking::No);
		}
	}

	ActionQueue.Add(Entry);
	EmitQueueRecord(
		EActionReactionTelemetryEvent::QueueAccepted,
		EActionReactionTelemetryReason::Queued,
		&Entry);

	// Sort by scheduled time
	SortQueueByTime();

	if (CombatDebug::IsQueueDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[QUEUE] Added queued action: Type=%s, Mode=%s, Scheduled=%.2f, Priority=%d"),
			*UEnum::GetValueAsString(InputAction.InputType),
			*UEnum::GetValueAsString(ExecMode),
			Entry.ScheduledTime,
			Entry.Priority);
	}

	SetDecisionReason(EActionReactionTelemetryReason::Queued);
	return true;
}

void UCombatComponent::ProcessQueuedActions(EAttackPhase TargetPhase)
{
	// PHASE 9: EVENT-DRIVEN QUEUE PROCESSING (NOT tick-based!)
	// Execute actions that are waiting for this phase transition
	// This completely replaces montage-time-based polling

	if (ActionQueue.Num() == 0)
	{
		return;
	}

	int32 WinnerIndex = INDEX_NONE;
	for (int32 Index = ActionQueue.Num() - 1; Index >= 0; --Index)
	{
		const FActionQueueEntry& Candidate = ActionQueue[Index];
		if (Candidate.IsPending() && Candidate.TargetPhase == TargetPhase)
		{
			WinnerIndex = Index;
			break;
		}
	}

	if (WinnerIndex == INDEX_NONE)
	{
		return;
	}

	FActionQueueEntry Winner = ActionQueue[WinnerIndex];
	ActionQueue.RemoveAt(WinnerIndex, 1, EAllowShrinking::No);
	const TWeakObjectPtr<UCombatComponent> ComponentSnapshot(this);
	const bool bExecuted = ExecuteAction(Winner);
	if (!ComponentSnapshot.IsValid())
	{
		return;
	}

	if (bExecuted)
	{
		Winner.State = EActionState::Completed;
		++QueueStats.ActionsExecuted;
		if (Winner.ExecutionMode == EActionExecutionMode::Queued)
		{
			++QueueStats.QueuedExecutions;
		}
	}
	else
	{
		Winner.State = EActionState::Cancelled;
		AppendActionQueueTelemetry(
			Winner,
			EActionReactionTelemetryEvent::QueueCancelled,
			EActionReactionTelemetryReason::ExecutionFailed);
		++QueueStats.ActionsCancelled;
	}
}

void UCombatComponent::ProcessQueue(float CurrentMontageTime)
{
	// DEPRECATED: Tick-based queue processing
	// Replaced by event-driven ProcessQueuedActions(TargetPhase) in Phase 9
	// Keeping this stub for potential backward compatibility or debugging

	if (ActionQueue.Num() == 0)
	{
		return;
	}

	// OLD TICK-BASED LOGIC (no longer used):
	// Process actions that have reached their scheduled time
	for (int32 i = ActionQueue.Num() - 1; i >= 0; --i)
	{
		FActionQueueEntry& Entry = ActionQueue[i];

		if (!Entry.IsPending())
		{
			continue;
		}

		// Check if action is ready to execute
		bool bReadyToExecute = false;

		if (Entry.ScheduledTime < 0.0f)
		{
			// Sentinel value: Find Active-end checkpoint dynamically
			for (const FTimerCheckpoint& Checkpoint : Checkpoints)
			{
				if (Checkpoint.WindowType == EActionWindowType::Combo &&
					Checkpoint.bActive &&
					Checkpoint.Duration == 0.0f &&
					CurrentMontageTime >= Checkpoint.MontageTime)
				{
					bReadyToExecute = true;
					Entry.ScheduledTime = Checkpoint.MontageTime; // Update for logging
					break;
				}
			}
		}
		else if (CurrentMontageTime >= Entry.ScheduledTime)
		{
			// Normal scheduled time reached
			bReadyToExecute = true;
		}

		if (bReadyToExecute)
		{
			// Execute action
			if (ExecuteAction(Entry))
			{
				Entry.State = EActionState::Completed;
				QueueStats.ActionsExecuted++;

				if (Entry.ExecutionMode == EActionExecutionMode::Queued)
				{
					QueueStats.QueuedExecutions++;
				}

				if (CombatDebug::IsQueueDebugEnabled())
				{
					UE_LOG(LogCombat, Log, TEXT("[QUEUE] Executed action at %.2f (scheduled: %.2f)"),
						CurrentMontageTime, Entry.ScheduledTime);
				}

				// Remove from queue after successful execution
				ActionQueue.RemoveAt(i);
			}
			else
			{
				// Execution failed - keep in queue for potential retry
				// Mark as cancelled if it keeps failing
				if (CombatDebug::IsQueueDebugEnabled())
				{
					UE_LOG(LogCombat, Warning, TEXT("[QUEUE] Action execution failed at %.2f, keeping in queue"),
						CurrentMontageTime);
				}
			}
		}
	}
}

bool UCombatComponent::ExecuteAction(FActionQueueEntry& Action)
{
#if WITH_AUTOMATION_TESTS
	++ExecuteActionCallCountForTesting;
#endif
	EnsureActionQueueEntryIdentity(Action);
	const uint64 ActionQueueEntryId = Action.QueueEntryId;
	const FQueuedInputAction ActionInput = Action.InputAction;
	const EActionExecutionMode ActionExecutionMode = Action.ExecutionMode;
	const FSoftObjectPath ActionAttackDataPath = Action.AttackData
		? FSoftObjectPath(Action.AttackData)
		: FSoftObjectPath();
	const auto EmitExecutionRecord = [
		this,
		ActionQueueEntryId,
		ActionInput,
		ActionExecutionMode,
		ActionAttackDataPath](
		const EActionReactionTelemetryEvent Event,
		const EActionReactionTelemetryReason Reason,
		const EActionState ObservedState)
	{
		FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(Event, Reason);
		Telemetry.InputSerial = ActionInput.InputSerial;
		Telemetry.QueueEntryId = ActionQueueEntryId;
		Telemetry.InputType = ActionInput.InputType;
		Telemetry.InputEvent = ActionInput.EventType;
		Telemetry.InputDirection = ActionInput.FacingIntent.BranchDirection;
		Telemetry.ExecutionMode = ActionExecutionMode;
		Telemetry.ActionState = ObservedState;
		Telemetry.AttackDataPath = ActionAttackDataPath;
		AppendActionReactionTelemetry(MoveTemp(Telemetry));
	};

	UAttackData* const ActionAttackData = Action.AttackData;
	const EInputType ActionInputType = Action.InputAction.InputType;
	if (!ActionAttackData || !IsCombatOwnerOperational(this))
	{
		EmitExecutionRecord(
			EActionReactionTelemetryEvent::ActionExecutionStarted,
			EActionReactionTelemetryReason::None,
			Action.State);
		EmitExecutionRecord(
			EActionReactionTelemetryEvent::ActionExecutionFinished,
			EActionReactionTelemetryReason::ExecutionFailed,
			Action.State);
		return false;
	}

	Action.State = EActionState::Executing;
	EmitExecutionRecord(
		EActionReactionTelemetryEvent::ActionExecutionStarted,
		EActionReactionTelemetryReason::None,
		Action.State);

	// Event-driven execution - plays montage directly
	bool bSuccess = false;

	switch (ActionInputType)
	{
		case EInputType::LightAttack:
		case EInputType::HeavyAttack:
		{
			// FINISHER CHECK: Before normal attack, try to execute finisher on vulnerable target
			if (CachedPairedAnimComp && CachedPairedAnimComp->TryExecuteFinisher(ActionAttackData))
			{
				// Finisher was executed - don't play normal attack
				bSuccess = true;
				break;
			}

			// INPUT-1 FIX: Set attack state BEFORE PlayAttackMontage() to prevent race condition.
			// PlayAttackMontage() may stop the current montage (via Montage_Stop or StopAllMontages),
			// which triggers OnMontageEnded callbacks (sync or async). The state machine's
			// PendingComboTransitions counter ensures those callbacks are rejected as stale.
			//
			// We set CurrentAttackData BEFORE PlayAttackMontage so that:
			// 1. The state machine transition (OnComboTransition) increments PendingComboTransitions
			// 2. Any sync or async OnMontageEnded from the old montage is rejected by Rule 0
			// 3. If PlayAttackMontage fails, we revert to the previous state
			UAttackData* PreviousAttackData = CurrentAttackData;
			EInputType PreviousInputType = CurrentAttackInputType;
			const uint64 PreviousStartupOperationSerial =
				ActiveAttackStartupOperationSerial;
			const uint64 StartupOperationSerial = NextAttackStartupOperationSerial++;
			if (NextAttackStartupOperationSerial == 0)
			{
				NextAttackStartupOperationSerial = 1;
			}
			ActiveAttackStartupOperationSerial = StartupOperationSerial;
			CurrentAttackData = ActionAttackData;
			CurrentAttackInputType = ActionInputType;

			// Play normal attack montage
			bSuccess = PlayAttackMontage(ActionAttackData);
			if (!IsValid(this))
			{
				return false;
			}
			const bool bProvisionalStateStillOwned =
				ActiveAttackStartupOperationSerial == StartupOperationSerial
				&& IsValid(ActionAttackData)
				&& CurrentAttackData == ActionAttackData
				&& CurrentAttackInputType == ActionInputType;
			const bool bStartupContextIntact = bProvisionalStateStillOwned
				&& IsCombatOwnerOperational(this)
				&& ActionAttackData->AttackMontage;
			bSuccess = bSuccess && bStartupContextIntact;

			// If successful, discover checkpoints for the new montage
			if (bSuccess)
			{
				// Capture phase BEFORE SetPhase overwrites it (for combo detection below)
				const EAttackPhase PhaseBeforeWindup = CurrentPhase;

				// Transition to Windup phase (event-driven phase management)
				SetPhase(EAttackPhase::Windup);

				DiscoverCheckpoints(ActionAttackData->AttackMontage);

				// CRITICAL FIX: Reset hold state for new attack (clears bActivatedThisAttack)
				HoldState.Reset();

				// MOTION WARP: Setup warp based on context (target or direction)
				SetupAttackWarp(Action);

				// Broadcast attack started event (use pre-Windup phase for accurate combo detection)
				bool bIsCombo = (PhaseBeforeWindup == EAttackPhase::Recovery || PhaseBeforeWindup == EAttackPhase::Active);
				OnAttackStarted.Broadcast(ActionAttackData, ActionInputType, bIsCombo);
				if (!IsValid(this)
					|| !IsCombatOwnerOperational(this)
					|| !IsValid(ActionAttackData)
					|| CurrentAttackData != ActionAttackData
					|| ActiveAttackStartupOperationSerial != StartupOperationSerial)
				{
					if (IsValid(this))
					{
						EmitExecutionRecord(
							EActionReactionTelemetryEvent::ActionExecutionFinished,
							EActionReactionTelemetryReason::ExecutionFailed,
							EActionState::Cancelled);
					}
					return false;
				}

				if (CombatDebug::IsQueueDebugEnabled())
				{
					FString SectionName = ActionAttackData->MontageSection.IsNone() ?
						TEXT("Default") : ActionAttackData->MontageSection.ToString();

					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] ═══════════════════════════════════════"));
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] Attack Data: %s"), *ActionAttackData->GetName());
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] Montage: %s"), *ActionAttackData->AttackMontage->GetName());
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] Section: %s"), *SectionName);
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] Input Type: %s"), *UEnum::GetValueAsString(CurrentAttackInputType));
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] Is Combo: %s"), bIsCombo ? TEXT("YES") : TEXT("NO"));
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] Checkpoints Discovered: %d"), Checkpoints.Num());
					UE_LOG(LogCombat, Log, TEXT("[EXECUTE] ═══════════════════════════════════════"));
				}
			}
			else
			{
				// Revert pre-set state on failure
				if (bProvisionalStateStillOwned)
				{
					CurrentAttackData = PreviousAttackData;
					CurrentAttackInputType = PreviousInputType;
					ActiveAttackStartupOperationSerial = PreviousStartupOperationSerial;
				}

				if (bProvisionalStateStillOwned && CombatDebug::IsQueueDebugEnabled())
				{
					UE_LOG(LogCombat, Warning, TEXT("[EXECUTE] PlayAttackMontage failed - reverted CurrentAttackData to %s"),
						PreviousAttackData ? *PreviousAttackData->GetName() : TEXT("None"));
				}
			}
			break;
		}

		case EInputType::Evade:
			// Handle evade
			break;

		case EInputType::Block:
			// Handle block
			break;

		default:
			break;
	}

	EmitExecutionRecord(
		EActionReactionTelemetryEvent::ActionExecutionFinished,
		bSuccess
			? EActionReactionTelemetryReason::Executed
			: EActionReactionTelemetryReason::ExecutionFailed,
		bSuccess ? EActionState::Completed : EActionState::Cancelled);
	return bSuccess;
}

bool UCombatComponent::ExecuteAttackData(UAttackData* AttackData, AActor* ExplicitWarpTarget, EInputType InputType)
{
	if (!AttackData || !IsCombatOwnerOperational(this))
	{
		return false;
	}

	if (InputType != EInputType::LightAttack && InputType != EInputType::HeavyAttack)
	{
		InputType = (AttackData->AttackType == EAttackType::Heavy)
			? EInputType::HeavyAttack
			: EInputType::LightAttack;
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	FQueuedInputAction InputAction(InputType, EInputEventType::Press, CurrentTime, bComboWindowActive);
	if (ABaseCombatCharacter* Character = GetOwnerCharacter())
	{
		InputAction.FacingIntent = BuildAttackFacingIntent(
			FVector2D::ZeroVector,
			Character->GetActorRotation(),
			Character->GetActorRotation(),
			EInputDirection::None,
			EAttackFacingIntentSource::Programmatic);
	}
	FActionQueueEntry Entry(InputAction, AttackData, EActionExecutionMode::Immediate);
	Entry.Priority = CalculatePriority(Entry);
	Entry.TargetPhase = EAttackPhase::None;

	const TWeakObjectPtr<AActor> PreviousExplicitWarpTarget = ExplicitAttackWarpTarget;
	const TWeakObjectPtr<AActor> PreviousIntentTarget = AttackIntentTarget;
	const TWeakObjectPtr<UCombatComponent> ComponentSnapshot(this);
	ExplicitAttackWarpTarget = ExplicitWarpTarget;
	if (ExplicitWarpTarget)
	{
		SetAttackIntentTarget(ExplicitWarpTarget);
	}
	const bool bExecuted = ExecuteAction(Entry);
	if (!ComponentSnapshot.IsValid())
	{
		return false;
	}
	if (!IsCombatOwnerOperational(this))
	{
		ExplicitAttackWarpTarget.Reset();
		AttackIntentTarget.Reset();
		return false;
	}
	ExplicitAttackWarpTarget = PreviousExplicitWarpTarget;
	if (!bExecuted)
	{
		SetAttackIntentTarget(PreviousIntentTarget.Get());
	}

	return bExecuted;
}

bool UCombatComponent::PlayAttackMontage(UAttackData* AttackData)
{
	UAnimMontage* const AttackMontage = AttackData ? AttackData->AttackMontage.Get() : nullptr;
	if (!AttackData || !AttackMontage)
	{
		if (CombatDebug::IsPhaseDebugEnabled())
		{
			UE_LOG(LogCombat, Warning, TEXT("[MONTAGE] Failed - Invalid AttackData or Montage"));
		}
		return false;
	}

	ABaseCombatCharacter* Character = GetOwnerCharacter();
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!IsCombatOwnerOperational(this) || !Mesh)
	{
		if (CombatDebug::IsPhaseDebugEnabled())
		{
			UE_LOG(LogCombat, Warning, TEXT("[MONTAGE] Failed - No character or mesh"));
		}
		return false;
	}

	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	if (!AnimInstance)
	{
		if (CombatDebug::IsPhaseDebugEnabled())
		{
			UE_LOG(LogCombat, Warning, TEXT("[MONTAGE] Failed - No AnimInstance"));
		}
		return false;
	}

	// PHASE 1 FIX: Clear any active hold state when starting new attack
	// This prevents hold state leaks (easing, movement locks) from previous attack
	ClearHoldState();

	// CRITICAL: Clear directional input buffer for fresh attack
	// Each attack starts with clean directional state (no stale directions from previous attacks)
	DirectionalInputBuffer.Reset();

	// COMBO BLENDING: Calculate blend times procedurally based on animation progress (BUG-2 FIX)
	// Near animation end = fast blend (natural transition), mid-animation = slow blend (smooth interrupt)
	// This replaces preset ComboBlendInTime/ComboBlendOutTime with dynamic calculation

	const float CurrentWorldTime = GetWorld()->GetTimeSeconds();
	const bool bStillInBlendTransition = bInComboBlend || (CurrentWorldTime < BlendTransitionEndTime);

	// Calculate procedural blend from current animation state
	FProceduralBlendResult BlendResult;
	UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();

	if (CurrentMontage)
	{
		const float CurrentPosition = AnimInstance->Montage_GetPosition(CurrentMontage);
		const float MontageLength = CurrentMontage->GetPlayLength();
		BlendResult = UProceduralAnimationLibrary::CalculateProceduralBlend(
			CurrentPosition, MontageLength, ProceduralBlendConfig, bStillInBlendTransition);
	}
	else
	{
		// Fresh attack - no current montage
		BlendResult.bIsFreshAttack = true;
		BlendResult.bUseInstantBlend = true;
		BlendResult.BlendInTime = ProceduralBlendConfig.MinBlendTime;
		BlendResult.BlendOutTime = 0.0f;
	}

	float BlendOutTime = BlendResult.BlendOutTime;
	float BlendInTime = BlendResult.BlendInTime;

	// CRITICAL FIX: Detect if we're still in a blend transition (rapid input during blend)
	if (bStillInBlendTransition)
	{
		if (CombatDebug::IsPhaseDebugEnabled())
		{
			UE_LOG(LogCombat, Warning, TEXT("[BLEND] Rapid input during blend (EndTime=%.2f, Now=%.2f)! Forcing instant cleanup."),
				BlendTransitionEndTime, CurrentWorldTime);
		}

		// Force instant stop of ALL montages to clean up the blend mess
		// This fixes the "half-blended loop" issue when mashing attack during blend-out
		AnimInstance->StopAllMontages(0.0f);
		if (!IsAttackStartupContextValid(
			this, Character, Mesh, AnimInstance, AttackData, AttackMontage))
		{
			return false;
		}
		bInComboBlend = false;
		BlendTransitionEndTime = 0.0f;

		// Override procedural result with instant blend
		BlendResult.bUseInstantBlend = true;
		BlendOutTime = 0.0f;
		BlendInTime = 0.0f;
	}
	else if (CurrentAttackData && !BlendResult.bIsFreshAttack)
	{
		// Normal case: procedural blend times already calculated above
		if (CombatDebug::IsPhaseDebugEnabled() && (BlendOutTime > 0.0f || BlendInTime > 0.0f))
		{
			UE_LOG(LogCombat, Log, TEXT("[BLEND] Procedural combo transition: %s → %s (Progress: %.1f%%, BlendIn: %.3fs, BlendOut: %.3fs, Strategy: %d)"),
				*CurrentAttackData->GetName(),
				*AttackData->GetName(),
				BlendResult.AnimationProgress * 100.0f,
				BlendInTime,
				BlendOutTime,
				static_cast<int32>(BlendResult.UsedStrategy));
		}
	}

	// BLEND-OUT: Stop current montage if blending is requested
	// Note: CurrentMontage already retrieved above for procedural blend calculation
	if (CurrentMontage && BlendOutTime > 0.0f)
	{
		// STATE MACHINE: Notify combo transition (tracks old montage for callback filtering)
		// CRITICAL: Pass section name for same-montage transitions (e.g., Attack_1 → Attack_2 in AM_Light_Combo_1)
		const float BlendDuration = FMath::Max(BlendOutTime, BlendInTime);
		AttackStateMachine.OnComboTransition(AttackMontage, AttackData->MontageSection, BlendDuration, CurrentWorldTime);

		// DEPRECATED: Keep legacy flags in sync for backwards compatibility
		bInComboBlend = true;
		BlendTransitionEndTime = CurrentWorldTime + BlendDuration;

		AnimInstance->Montage_Stop(BlendOutTime, CurrentMontage);
		if (!IsAttackStartupContextValid(
			this, Character, Mesh, AnimInstance, AttackData, AttackMontage))
		{
			return false;
		}

		if (CombatDebug::IsPhaseDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[BLEND] Combo blend started (Gen=%u) - EndTime=%.2f (Duration=%.2fs)"),
				AttackStateMachine.AttackGeneration, BlendTransitionEndTime, BlendDuration);
		}
	}
	else
	{
		// Not a combo - this is a fresh attack start
		// CRITICAL: Pass section name and time for grace period protection
		AttackStateMachine.OnAttackStarted(AttackMontage, AttackData->MontageSection, CurrentWorldTime);

		if (CombatDebug::IsPhaseDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[STATE] New attack started (Gen=%d): %s"),
				AttackStateMachine.AttackGeneration, *AttackData->GetName());
		}
	}

	ConsumedAttackInstance = {};
	bConsumedPendingPresentation = false;
	FAttackInstanceId StartedAttack;
	StartedAttack.Attacker = GetOwner();
	StartedAttack.AttackGeneration = AttackStateMachine.AttackGeneration;
	OpenAttackWindowRecords.RemoveAll([&StartedAttack](const FAttackWindowInstanceId& Candidate)
	{
		return !(Candidate.AttackInstance == StartedAttack);
	});
	if (ActiveHitWindow.IsValid() && !(ActiveHitWindow.AttackInstance == StartedAttack))
	{
		ActiveHitWindow = {};
	}
	if (ActiveParryWindow.IsValid() && !(ActiveParryWindow.AttackInstance == StartedAttack))
	{
		ActiveParryWindow = {};
	}
	if (ActiveCounterWindow.IsValid() && !(ActiveCounterWindow.AttackInstance == StartedAttack))
	{
		ActiveCounterWindow = {};
	}

	// NOTE: bCurrentAttackIsDirectionalFollowUp flag is managed in GetAttackForInput()
	// It's set during resolution based on whether the attack was found in DirectionalFollowUps map

	// BLEND-IN: Play new montage with blend settings
	// Note: OnMontageEnded delegate already bound in BeginPlay() for event-driven phase management
	float PlayRate = 1.0f;
#if WITH_AUTOMATION_TESTS
	PlayRate = AttackMontagePlayRateForTesting;
#endif
	const float StartPosition = 0.0f;

	if (BlendInTime > 0.0f)
	{
		// Play with custom blend-in
		FAlphaBlendArgs BlendIn(BlendInTime);
		AnimInstance->Montage_PlayWithBlendSettings(
			AttackMontage,
			BlendIn,
			PlayRate,
			EMontagePlayReturnType::MontageLength,
			StartPosition,
			false  // Don't stop all montages
		);
	}
	else
	{
		// Play with default blend (instant)
		AnimInstance->Montage_Play(AttackMontage, PlayRate, EMontagePlayReturnType::MontageLength, StartPosition);
	}
	if (!IsAttackStartupContextValid(
		this, Character, Mesh, AnimInstance, AttackData, AttackMontage))
	{
		return false;
	}

	FAnimMontageInstance* ActiveMontageInstance =
		AnimInstance->GetActiveInstanceForMontage(AttackMontage);
	if (!ActiveMontageInstance)
	{
		AttackStateMachine.ForceReset();
		return false;
	}
	AttackStateMachine.SetActiveMontageInstanceId(
		ActiveMontageInstance->GetInstanceID());

	// Clear blend flag - new montage has started playing
	// STATE MACHINE: Notify combo blend complete
	if (AttackStateMachine.IsComboBlending())
	{
		AttackStateMachine.OnComboBlendComplete();

		if (CombatDebug::IsPhaseDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[STATE] Combo blend complete (Gen=%u) - now InProgress"),
				AttackStateMachine.AttackGeneration);
		}
	}

	// DEPRECATED: Keep legacy flags in sync
	if (bInComboBlend)
	{
		bInComboBlend = false;
	}

	// Handle montage sections if specified
	if (!AttackData->MontageSection.IsNone())
	{
		AnimInstance->Montage_JumpToSection(AttackData->MontageSection, AttackMontage);

		// Prevent auto-advance to next section if bUseSectionOnly is true
		if (AttackData->bUseSectionOnly)
		{
			AnimInstance->Montage_SetNextSection(AttackData->MontageSection, NAME_None, AttackMontage);

			if (CombatDebug::IsPhaseDebugEnabled())
			{
				UE_LOG(LogCombat, Log, TEXT("[MONTAGE] Section-only mode: %s (no auto-advance)"),
					*AttackData->MontageSection.ToString());
			}
		}
	}

	if (CombatDebug::IsPhaseDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[MONTAGE] Playing: %s | Section: %s | Delegate bound"),
			*AttackMontage->GetName(),
			*AttackData->MontageSection.ToString());
	}

	return true;
}

void UCombatComponent::ClearQueue(bool bCancelCurrent)
{
#if WITH_AUTOMATION_TESTS
	++ClearQueueCallCountForTesting;
#endif
	if (bCancelCurrent)
	{
		// Cancel all including executing
		for (FActionQueueEntry& Entry : ActionQueue)
		{
			if (Entry.State != EActionState::Completed)
			{
				Entry.State = EActionState::Cancelled;
				AppendActionQueueTelemetry(
					Entry,
					EActionReactionTelemetryEvent::QueueCancelled,
					EActionReactionTelemetryReason::ExplicitQueueClear);
				QueueStats.ActionsCancelled++;
			}
		}
	}
	else
	{
		// Cancel only pending
		for (FActionQueueEntry& Entry : ActionQueue)
		{
			if (Entry.IsPending())
			{
				Entry.State = EActionState::Cancelled;
				AppendActionQueueTelemetry(
					Entry,
					EActionReactionTelemetryEvent::QueueCancelled,
					EActionReactionTelemetryReason::ExplicitQueueClear);
				QueueStats.ActionsCancelled++;
			}
		}
	}

	ActionQueue.Empty();

	// Reset combo state when queue is cleared
	CurrentAttackData = nullptr;
	CurrentAttackInputType = EInputType::None;
	ActiveAttackStartupOperationSerial = 0;
	if (bCancelCurrent)
	{
		bIsBlocking = false;
	}

	// CRITICAL: Clear directional input buffer on queue clear
	// Prevents stale directional input from persisting across combat resets
	DirectionalInputBuffer.Reset();

	if (CombatDebug::IsQueueDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[QUEUE] Cleared (CancelCurrent=%s) - Combo state and directional buffer reset"), bCancelCurrent ? TEXT("YES") : TEXT("NO"));
	}
}

void UCombatComponent::CancelActionsWithPriority(int32 MinPriority)
{
	for (int32 i = ActionQueue.Num() - 1; i >= 0; --i)
	{
		FActionQueueEntry& Entry = ActionQueue[i];

		if (Entry.IsPending() && Entry.Priority < MinPriority)
		{
			const int32 CancelledPriority = Entry.Priority;
			Entry.State = EActionState::Cancelled;
			AppendActionQueueTelemetry(
				Entry,
				EActionReactionTelemetryEvent::QueueCancelled,
				EActionReactionTelemetryReason::PriorityCancelled);
			QueueStats.ActionsCancelled++;
			ActionQueue.RemoveAt(i);

			if (CombatDebug::IsQueueDebugEnabled())
			{
				UE_LOG(LogCombat, Log, TEXT("[QUEUE] Cancelled action (Priority %d < %d)"),
					CancelledPriority, MinPriority);
			}
		}
	}
}

// ============================================================================
// TIMER CHECKPOINT SYSTEM
// ============================================================================

void UCombatComponent::DiscoverCheckpoints(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return;
	}

	// Clear existing checkpoints
	Checkpoints.Empty();

	// Use MontageUtilityLibrary to discover checkpoints from AnimNotifyStates
	int32 NumDiscovered = UMontageUtilityLibrary::DiscoverCheckpoints(Montage, Checkpoints);

	if (CombatDebug::IsQueueDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[CHECKPOINTS] Discovered %d checkpoints from montage: %s"),
			NumDiscovered,
			*Montage->GetName());

		// Log discovered checkpoints
		UMontageUtilityLibrary::LogCheckpoints(Checkpoints, TEXT("DISCOVERY"));
	}

	// Update combo window state if any combo checkpoints were found
	for (const FTimerCheckpoint& Checkpoint : Checkpoints)
	{
		if (Checkpoint.WindowType == EActionWindowType::Combo)
		{
			bComboWindowActive = true;
			ComboWindowStart = Checkpoint.MontageTime;
			ComboWindowDuration = Checkpoint.Duration;
			break;
		}
	}
}

void UCombatComponent::RegisterCheckpoint(EActionWindowType WindowType, float StartTime, float Duration)
{
	FTimerCheckpoint Checkpoint(WindowType, StartTime, Duration);
	Checkpoint.bActive = true;

	Checkpoints.Add(Checkpoint);

	// Update combo window state if this is a combo checkpoint
	if (WindowType == EActionWindowType::Combo)
	{
		bComboWindowActive = true;
		ComboWindowStart = StartTime;
		ComboWindowDuration = Duration;
	}

	if (CombatDebug::IsQueueDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[CHECKPOINTS] Registered: Type=%s, Start=%.2f, Duration=%.2f"),
			*UEnum::GetValueAsString(WindowType),
			StartTime,
			Duration);
	}
}

// ============================================================================
// CHECKPOINT SYSTEM
// ============================================================================

bool UCombatComponent::HasReachedCheckpoint(const FTimerCheckpoint& Checkpoint, float CurrentTime) const
{
	return Checkpoint.bActive && CurrentTime >= Checkpoint.MontageTime;
}

TArray<FTimerCheckpoint> UCombatComponent::GetActiveWindows(float CurrentTime) const
{
	TArray<FTimerCheckpoint> ActiveWindows;
	for (const FTimerCheckpoint& Checkpoint : Checkpoints)
	{
		if (Checkpoint.bActive &&
		    CurrentTime >= Checkpoint.MontageTime &&
		    CurrentTime <= (Checkpoint.MontageTime + Checkpoint.Duration))
		{
			ActiveWindows.Add(Checkpoint);
		}
	}
	return ActiveWindows;
}

float UCombatComponent::GetExecutionCheckpoint(const FActionQueueEntry& Action) const
{
	if (Action.ExecutionMode == EActionExecutionMode::Immediate)
	{
		return 0.0f; // Execute now
	}

	// Queued mode: Execute at Active phase end (Active → Recovery transition)
	// Find the zero-duration Combo checkpoint (registered when transitioning Active → Recovery)
	// This is the phase transition point, NOT the deprecated combo window
	for (const FTimerCheckpoint& Checkpoint : Checkpoints)
	{
		if (Checkpoint.WindowType == EActionWindowType::Combo &&
		    Checkpoint.bActive &&
		    Checkpoint.Duration == 0.0f) // Zero-duration = Active-end marker (phase transition)
		{
			// Return the checkpoint time (Active phase end)
			if (CombatDebug::IsQueueDebugEnabled())
			{
				UE_LOG(LogCombat, Log, TEXT("[CHECKPOINT] Found Active-end checkpoint at %.2f for queued execution"),
					Checkpoint.MontageTime);
			}

			return Checkpoint.MontageTime;
		}
	}

	// Checkpoint not found yet - use sentinel value to indicate "execute at Active-end"
	// The checkpoint will be created when Active→Recovery transition happens via OnPhaseTransition
	if (CombatDebug::IsQueueDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[CHECKPOINT] Active-end checkpoint not found yet, will execute when created"));
	}

	/*TODO: Consider warning if no Active-end checkpoint found after montage ends*/
	return -1.0f; // Sentinel: Execute at Active-end (checkpoint will be created later)
}

// ============================================================================
// HOLD SYSTEM
// ============================================================================

void UCombatComponent::OnHoldWindowStart(EInputType InputType)
{
	// Legacy calls do not carry enough identity to commit a runtime hold.
	if (CombatDebug::IsHoldDebugEnabled())
	{
		UE_LOG(LogCombat, Warning,
			TEXT("[HOLD] Ignored legacy hold-start call for %s; exact notify context is required"),
			*UEnum::GetValueAsString(InputType));
	}
}

bool UCombatComponent::OnHoldWindowStartWithContext(
	const EInputType InputType,
	const FAnimNotifyRuntimeSourceId& NotifySource,
	const int32 MontageInstanceId)
{
	UAnimMontage* ActiveMontage = AttackStateMachine.GetActiveMontage();
	const bool bContextMatches = CurrentAttackData
		&& !HoldState.bActivatedThisAttack
		&& !HoldState.IsHolding()
		&& NotifySource.IsValid()
		&& ActiveMontage
		&& NotifySource.SourceAnimation == FSoftObjectPath(ActiveMontage)
		&& MontageInstanceId >= 0
		&& MontageInstanceId == AttackStateMachine.ActiveMontageInstanceId
		&& AttackStateMachine.AttackGeneration > 0;
	if (!bContextMatches)
	{
		return false;
	}

	if (!HeldInputs.Contains(InputType))
	{
		if (CombatDebug::IsHoldDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[HOLD] Window start, but button not held: %s"),
				*UEnum::GetValueAsString(InputType));
		}
		return false;
	}

	FAttackInstanceId SourceAttackInstance;
	SourceAttackInstance.Attacker = GetOwner();
	SourceAttackInstance.AttackGeneration = AttackStateMachine.AttackGeneration;
	const uint64 PressInputSerial = HeldInputSerials.FindRef(InputType);
	bool bActivated = false;

	// Determine hold behavior based on attack type
	if (CurrentAttackData->AttackType == EAttackType::Heavy)
	{
		// HEAVY ATTACK HOLD: Jump to and loop charge section
		ACharacter* Character = Cast<ACharacter>(GetOwner());
		if (Character && CurrentAttackData->ChargeLoopSection != NAME_None)
		{
			// STEP 1: Jump to the charge section WITH BLEND (smooths transition from attack → charge loop)
			bool bJumped = UMontageUtilityLibrary::JumpToSectionWithBlend(
				Character,
				CurrentAttackData->ChargeLoopSection,
				CurrentAttackData->ChargeLoopBlendTime  // Blends initial attack anim → charge loop
			);

			if (!bJumped)
			{
				if (CombatDebug::IsHoldDebugEnabled())
				{
					UE_LOG(LogCombat, Warning, TEXT("[HOLD] Failed to jump to charge section: %s"),
						*CurrentAttackData->ChargeLoopSection.ToString());
				}
				return false;
			}

			// STEP 2: Set up the loop (section jumps back to itself)
			bool bLooped = UMontageUtilityLibrary::LoopMontageSection(Character, CurrentAttackData->ChargeLoopSection);

			if (bLooped)
			{
				bActivated = ActivateHoldWithInputSerial(
					InputType,
					1.0f,
					PressInputSerial,
					SourceAttackInstance,
					CurrentAttackData,
					NotifySource,
					MontageInstanceId);

				if (bActivated)
				{
					HoldState.MarkHoldCompleted();
				}

				if (CombatDebug::IsHoldDebugEnabled())
				{
					UE_LOG(LogCombat, Log, TEXT("[HOLD] Heavy attack charge loop started and marked completed: jumped to '%s' and looping"),
						*CurrentAttackData->ChargeLoopSection.ToString());
				}
			}
			else if (CombatDebug::IsHoldDebugEnabled())
			{
				UE_LOG(LogCombat, Warning, TEXT("[HOLD] Failed to loop charge section: %s"),
					*CurrentAttackData->ChargeLoopSection.ToString());
			}
		}
		else if (CombatDebug::IsHoldDebugEnabled())
		{
			UE_LOG(LogCombat, Warning, TEXT("[HOLD] Heavy attack has no ChargeLoopSection defined"));
		}
	}
	else if (CurrentAttackData->AttackType == EAttackType::Light)
	{
		bActivated = ActivateHoldWithInputSerial(
			InputType,
			1.0f,
			PressInputSerial,
			SourceAttackInstance,
			CurrentAttackData,
			NotifySource,
			MontageInstanceId);
		if (!bActivated)
		{
			return false;
		}

		// Initialize EASE-IN transition state (1.0 → HoldTargetPlayRate)
		HoldState.bIsEasing = true;
		HoldState.bIsEasingOut = false; // EASE-IN direction
		HoldState.EaseStartTime = GetWorld()->GetTimeSeconds();
		HoldState.EaseStartPlayRate = 1.0f; // Current playrate (normal speed)
		// Start timer for ease updates (60 Hz for smooth transitions)
		// Timer calls OnEaseTimerTick() repeatedly until ease completes or is cancelled
		float TimerInterval = 1.0f / 60.0f; // 60 Hz update rate
		const FTimerDelegate EaseTimerDelegate = FTimerDelegate::CreateUObject(
			this,
			&UCombatComponent::OnEaseTimerTick,
			HoldState.CurrentHold.HoldID);
		GetWorld()->GetTimerManager().SetTimer(
			EaseTimerHandle,
			EaseTimerDelegate,
			TimerInterval,
			true // Loop
		);

		if (CombatDebug::IsHoldDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[HOLD TIMER] Light attack EASE-IN started (1.0 → %.2f over %.2fs using %s @ 60Hz)"),
				CurrentAttackData->HoldTargetPlayRate,
				CurrentAttackData->HoldEaseInDuration,
				*UEnum::GetValueAsString(CurrentAttackData->HoldEaseInType));
		}
	}

	if (bActivated)
	{
		SetInputContext(EInputContext::DirectionalInput);
		if (CombatDebug::IsHoldDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[HOLD] Directional input window OPENED (awaiting release)"));
		}
	}
	return bActivated;
}

void UCombatComponent::ActivateHold(EInputType InputType, float PlayRate)
{
	ActivateHoldWithInputSerial(InputType, PlayRate, 0);
}

bool UCombatComponent::ActivateHoldWithInputSerial(
	EInputType InputType,
	float PlayRate,
	uint64 PressInputSerial,
	const FAttackInstanceId& SourceAttackInstance,
	UAttackData* SourceAttackData,
	const FAnimNotifyRuntimeSourceId& NotifySource,
	int32 MontageInstanceId)
{
	if (!HoldState.Activate(
		InputType,
		GetWorld()->GetTimeSeconds(),
		PlayRate,
		SourceAttackInstance,
		SourceAttackData,
		NotifySource,
		MontageInstanceId))
	{
		return false;
	}
	HoldState.CurrentHold.PressInputSerial = PressInputSerial;
	FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(
		EActionReactionTelemetryEvent::HoldStateChanged,
		EActionReactionTelemetryReason::HoldActivated);
	Telemetry.InputType = InputType;
	Telemetry.InputSerial = PressInputSerial;
	Telemetry.HoldGeneration = HoldState.CurrentHold.HoldID;
	Telemetry.Detail = FString::Printf(TEXT("PlayRate=%.3f"), PlayRate);
	AppendActionReactionTelemetry(MoveTemp(Telemetry));

	// Apply playrate to montage using utility library
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UMontageUtilityLibrary::SetMontagePlayRate(Character, PlayRate);

	if (CombatDebug::IsHoldDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[HOLD] Activated: Input=%s, PlayRate=%.2f"),
			*UEnum::GetValueAsString(InputType),
			PlayRate);
	}
	return true;
}

void UCombatComponent::DeactivateHold()
{
	DeactivateHoldWithInputSerial(0);
}

void UCombatComponent::DeactivateHoldWithInputSerial(
	const uint64 ReleaseInputSerial,
	const FAttackFacingIntent& ReleaseFacingIntent,
	const EInputDirection ReleaseDirection)
{
	if (!HoldState.IsHolding())
	{
		return;
	}

	const int32 ReleasedHoldGeneration = HoldState.CurrentHold.HoldID;
	if (!HoldState.BeginReleaseBlend(
		ReleasedHoldGeneration,
		ReleaseFacingIntent,
		UCombatUtils::InputToAttackDirection(ReleaseDirection)))
	{
		return;
	}
	if (ReleaseInputSerial != 0)
	{
		HoldState.CurrentHold.ReleaseInputSerial = ReleaseInputSerial;
	}

	UAttackData* HoldAttackData = HoldState.CurrentHold.SourceAttackData
		? HoldState.CurrentHold.SourceAttackData.Get()
		: CurrentAttackData.Get();
	if (HoldState.CurrentHold.SourceAttackInstance.IsValid())
	{
		const bool bSourceStillOwnsHold =
			HoldState.CurrentHold.SourceAttackInstance.Attacker.Get() == GetOwner()
			&& HoldState.CurrentHold.SourceAttackInstance.AttackGeneration
				== AttackStateMachine.AttackGeneration
			&& HoldAttackData == CurrentAttackData.Get()
			&& HoldState.CurrentHold.MontageInstanceId
				== AttackStateMachine.ActiveMontageInstanceId;
		if (!bSourceStillOwnsHold)
		{
			FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(
				EActionReactionTelemetryEvent::HoldReleaseRejected,
				EActionReactionTelemetryReason::HoldSourceRejected);
			Telemetry.InputSerial = ReleaseInputSerial;
			Telemetry.HoldGeneration = ReleasedHoldGeneration;
			Telemetry.Detail = TEXT("Release source no longer owns active attack");
			AppendActionReactionTelemetry(MoveTemp(Telemetry));
			ClearHoldState();
			return;
		}
	}

	if (!HoldAttackData)
	{
		FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(
			EActionReactionTelemetryEvent::HoldReleaseRejected,
			EActionReactionTelemetryReason::MissingAttackContext);
		Telemetry.InputType = HoldState.CurrentHold.InputType;
		Telemetry.InputSerial = HoldState.CurrentHold.ReleaseInputSerial != 0
			? HoldState.CurrentHold.ReleaseInputSerial
			: HoldState.CurrentHold.PressInputSerial;
		Telemetry.HoldGeneration = HoldState.CurrentHold.HoldID;
		Telemetry.Detail = TEXT("Release ignored because CurrentAttackData is null");
		AppendActionReactionTelemetry(MoveTemp(Telemetry));
		ClearHoldState();
		return;
	}

	// Clear any existing ease timer (ease-in may still be running)
	if (GetWorld() && EaseTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(EaseTimerHandle);
	}

	// HEAVY ATTACK: Jump to release section (no easing)
	if (HoldAttackData->AttackType == EAttackType::Heavy)
	{
		const EInputType ReleasedInputType = HoldState.CurrentHold.InputType;
		const uint64 ReleasedInputSerial = HoldState.CurrentHold.ReleaseInputSerial != 0
			? HoldState.CurrentHold.ReleaseInputSerial
			: HoldState.CurrentHold.PressInputSerial;
		ACharacter* Character = Cast<ACharacter>(GetOwner());
		bool bReleaseSectionContinues = false;
		if (Character && HoldAttackData->ChargeReleaseSection != NAME_None)
		{
			bReleaseSectionContinues = UMontageUtilityLibrary::JumpToSectionWithBlend(
				Character,
				HoldAttackData->ChargeReleaseSection,
				HoldAttackData->ChargeReleaseBlendTime);

			if (CombatDebug::IsHoldDebugEnabled())
			{
				if (bReleaseSectionContinues)
				{
					UE_LOG(LogCombat, Log, TEXT("[HOLD] Heavy attack released: jumping to release section '%s'"),
						*HoldAttackData->ChargeReleaseSection.ToString());
				}
				else
				{
					UE_LOG(LogCombat, Warning, TEXT("[HOLD] Failed to jump to release section '%s'"),
						*HoldAttackData->ChargeReleaseSection.ToString());
				}
			}
		}

		const bool bFollowUpStarted = !bReleaseSectionContinues
			&& DispatchHoldOwnedFollowUp(ReleasedHoldGeneration);
		const bool bReturnToIdle = !bReleaseSectionContinues && !bFollowUpStarted;
		if (bReturnToIdle)
		{
			if (Character && Character->GetMesh())
			{
				if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
				{
					if (UAnimMontage* CurrentMontage = UMontageUtilityLibrary::GetCurrentMontage(Character))
					{
						AnimInstance->Montage_Stop(
							HoldAttackData->ChargeReleaseBlendTime,
							CurrentMontage);
					}
				}
			}
			CurrentAttackData = nullptr;
			CurrentAttackInputType = EInputType::None;
			ActiveAttackStartupOperationSerial = 0;
			SetPhase(EAttackPhase::None);
			Checkpoints.Empty();
		}

		TerminateHoldIfMatches(ReleasedHoldGeneration);
		FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(
			EActionReactionTelemetryEvent::HoldStateChanged,
			EActionReactionTelemetryReason::HoldReleased);
		Telemetry.InputType = ReleasedInputType;
		Telemetry.InputSerial = ReleasedInputSerial;
		Telemetry.HoldGeneration = ReleasedHoldGeneration;
		Telemetry.Detail = bReleaseSectionContinues
			? TEXT("HeavyReleaseSection")
			: (bFollowUpStarted ? TEXT("HeavyFollowUpStarted") : TEXT("HeavyReleaseToIdle"));
		AppendActionReactionTelemetry(MoveTemp(Telemetry));
		if (bReturnToIdle)
		{
			ProcessQueuedActions(EAttackPhase::Recovery);
		}
		return;
	}

	// LIGHT ATTACK: Begin EASE-OUT transition (HoldTargetPlayRate → 1.0)
	// Reuse the same timer system but reverse the transition

	// CRITICAL FIX: Query ACTUAL montage playrate instead of HoldState.CurrentPlayRate
	// If button released during ease-in, HoldState may not match AnimInstance's actual playrate
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	float CurrentPlayRate = UMontageUtilityLibrary::GetMontagePlayRate(Character);

	// Fallback to HoldState if query fails (shouldn't happen, but safety first)
	if (CurrentPlayRate <= 0.0f)
	{
		CurrentPlayRate = HoldState.CurrentPlayRate;
		if (CombatDebug::IsHoldDebugEnabled())
		{
			UE_LOG(LogCombat, Warning, TEXT("[HOLD] Failed to query montage playrate, using HoldState: %.2f"), CurrentPlayRate);
		}
	}

	// Initialize EASE-OUT transition state
	HoldState.bIsEasing = true;
	HoldState.bIsEasingOut = true; // EASE-OUT direction
	HoldState.EaseStartTime = GetWorld()->GetTimeSeconds();
	HoldState.EaseStartPlayRate = CurrentPlayRate; // Start from ACTUAL current playrate

	if (CombatDebug::IsHoldDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[HOLD] Light attack EASE-OUT starting from ACTUAL playrate: %.2f → 1.0"), CurrentPlayRate);
	}

	// NOTE: We keep HoldState.bIsHolding = true during ease-out
	// This prevents re-activation during the transition
	// Deactivate() will be called when ease-out complete
	// Start timer for EASE-OUT (60 Hz for smooth transitions)
	float TimerInterval = 1.0f / 60.0f; // 60 Hz update rate
	/*TODO: Update to support variable and dynamic framerate*/
	const FTimerDelegate EaseTimerDelegate = FTimerDelegate::CreateUObject(
		this,
		&UCombatComponent::OnEaseTimerTick,
		HoldState.CurrentHold.HoldID);

	GetWorld()->GetTimerManager().SetTimer(
		EaseTimerHandle,
		EaseTimerDelegate,
		TimerInterval,
		true // Loop
	);

	if (CombatDebug::IsHoldDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[HOLD TIMER] EASE-OUT started (%.2f → 1.0 over %.2fs using %s @ 60Hz)"),
			CurrentPlayRate,
			HoldAttackData->HoldEaseOutDuration,
			*UEnum::GetValueAsString(HoldAttackData->HoldEaseOutType));
	}
}

bool UCombatComponent::IsHoldSourceCurrent(const int32 ExpectedHoldGeneration) const
{
	if (!HoldState.CurrentHold.IsValid(ExpectedHoldGeneration))
	{
		return false;
	}

	const FHoldEvent& Hold = HoldState.CurrentHold;
	if (!Hold.SourceAttackInstance.IsValid())
	{
		return Hold.SourceAttackData
			? Hold.SourceAttackData.Get() == CurrentAttackData.Get()
			: CurrentAttackData != nullptr;
	}

	return Hold.SourceAttackInstance.Attacker.Get() == GetOwner()
		&& Hold.SourceAttackInstance.AttackGeneration == AttackStateMachine.AttackGeneration
		&& Hold.SourceAttackData.Get() == CurrentAttackData.Get()
		&& Hold.MontageInstanceId == AttackStateMachine.ActiveMontageInstanceId
		&& AttackStateMachine.GetActiveMontage()
		&& Hold.NotifySource.SourceAnimation
			== FSoftObjectPath(AttackStateMachine.GetActiveMontage());
}

bool UCombatComponent::TerminateHoldIfMatches(const int32 ExpectedHoldGeneration)
{
	if (!HoldState.CurrentHold.IsValid(ExpectedHoldGeneration))
	{
		return false;
	}

	ClearHoldState();
	return true;
}

bool UCombatComponent::DispatchHoldOwnedFollowUp(const int32 ExpectedHoldGeneration)
{
	if (!IsHoldSourceCurrent(ExpectedHoldGeneration)
		|| !HoldState.IsHoldCompleted()
		|| HoldState.CurrentHold.Direction == EAttackDirection::None)
	{
		return false;
	}

	UAttackData* SourceAttackData = HoldState.CurrentHold.SourceAttackData.Get();
	if (!SourceAttackData)
	{
		SourceAttackData = CurrentAttackData.Get();
	}
	if (!SourceAttackData)
	{
		return false;
	}

	const TObjectPtr<UAttackData>* DirectionalAttack =
		SourceAttackData->DirectionalFollowUps.Find(HoldState.CurrentHold.Direction);
	UAttackData* FollowUpAttack = DirectionalAttack ? DirectionalAttack->Get() : nullptr;
	if (!FollowUpAttack || !HoldState.BeginFollowUpHandoff(ExpectedHoldGeneration))
	{
		return false;
	}

	const FHoldEvent HoldSnapshot = HoldState.CurrentHold;
	FQueuedInputAction FollowUpInput(
		HoldSnapshot.InputType,
		EInputEventType::Press,
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f,
		false);
	FollowUpInput.InputSerial = HoldSnapshot.ReleaseInputSerial;
	FollowUpInput.FacingIntent = HoldSnapshot.ReleaseFacingIntent;
	FActionQueueEntry FollowUpEntry(
		FollowUpInput,
		FollowUpAttack,
		EActionExecutionMode::Immediate);
	EnsureActionQueueEntryIdentity(FollowUpEntry);
	FollowUpEntry.Priority = CalculatePriority(FollowUpEntry);
	FollowUpEntry.TargetPhase = EAttackPhase::None;

#if WITH_AUTOMATION_TESTS
	++HoldOwnedDispatchCountForTesting;
#endif

	const TWeakObjectPtr<UCombatComponent> ComponentSnapshot(this);
	const bool bExecuted = ExecuteAction(FollowUpEntry);
	if (!ComponentSnapshot.IsValid())
	{
		return false;
	}

	if (HoldSnapshot.ReleaseInputSerial != 0)
	{
		FinalizeCombatInput(
			HoldSnapshot.ReleaseInputSerial,
			ECombatInputRoute::HoldOwned,
			bExecuted
				? ECombatInputDisposition::Consumed
				: ECombatInputDisposition::Rejected,
			bExecuted
				? EActionReactionTelemetryReason::Executed
				: EActionReactionTelemetryReason::ExecutionFailed);
	}

	if (!bExecuted)
	{
		TerminateHoldIfMatches(ExpectedHoldGeneration);
	}
	return bExecuted;
}

void UCombatComponent::OnEaseTimerTick(const int32 ExpectedHoldGeneration)
{
	// TIMER-BASED EASE TRANSITION (NOT tick-based!)
	// This function is called by FTimerHandle at regular intervals (60 Hz)
	// Handles BOTH ease-in (1.0 → HoldTargetPlayRate) AND ease-out (HoldTargetPlayRate → 1.0)

	if (!HoldState.CurrentHold.IsValid(ExpectedHoldGeneration))
	{
		return;
	}

	if (!IsHoldSourceCurrent(ExpectedHoldGeneration))
	{
		TerminateHoldIfMatches(ExpectedHoldGeneration);
		return;
	}

	UAttackData* HoldAttackData = HoldState.CurrentHold.SourceAttackData
		? HoldState.CurrentHold.SourceAttackData.Get()
		: CurrentAttackData.Get();
	if (!HoldState.bIsEasing || !HoldAttackData)
	{
		// Ease cancelled or completed - clear timer
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(EaseTimerHandle);
		}
		return;
	}

	// Calculate elapsed time since ease started
	float CurrentTime = GetWorld()->GetTimeSeconds();
	float ElapsedTime = CurrentTime - HoldState.EaseStartTime;

	// Determine if we're easing IN or OUT using dedicated flag (NOT playrate comparison)
	// CRITICAL FIX: Using playrate comparison fails when button released during ease-in
	// EASE-IN: Start = 1.0, Target = HoldTargetPlayRate (bIsEasingOut = false)
	// EASE-OUT: Start = current playrate, Target = 1.0 (bIsEasingOut = true)
	bool bIsEasingIn = !HoldState.bIsEasingOut; // Use flag instead of playrate comparison
	float TargetPlayRate = bIsEasingIn ? HoldAttackData->HoldTargetPlayRate : 1.0f;
	float EaseDuration = bIsEasingIn ? HoldAttackData->HoldEaseInDuration : HoldAttackData->HoldEaseOutDuration;
	EEasingType EasingType = bIsEasingIn ? HoldAttackData->HoldEaseInType : HoldAttackData->HoldEaseOutType;

	// Check if ease transition is complete
	if (ElapsedTime >= EaseDuration)
	{
		// Ease complete - ensure we're at final target playrate
		HoldState.bIsEasing = false;
		HoldState.CurrentPlayRate = TargetPlayRate;

		ACharacter* CharacterRef = Cast<ACharacter>(GetOwner());
		UMontageUtilityLibrary::SetMontagePlayRate(CharacterRef, HoldState.CurrentPlayRate);

		// Clear timer - ease complete
		GetWorld()->GetTimerManager().ClearTimer(EaseTimerHandle);

		// If EASE-IN just completed, mark hold as completed (freeze state reached)
		if (bIsEasingIn)
		{
			HoldState.MarkHoldCompleted();

			if (CombatDebug::IsHoldDebugEnabled())
			{
				float TotalHoldDuration = CurrentTime - HoldState.CurrentHold.StartTime;
				UE_LOG(LogCombat, Log, TEXT("[HOLD] Light attack freeze reached - hold marked completed (duration: %.2fs)"), TotalHoldDuration);
			}
		}
		// If EASE-OUT just completed, execute follow-up attack ONLY if hold was completed
		else
		{
			// Calculate total hold duration (from activation to release)
			float TotalHoldDuration = CurrentTime - HoldState.CurrentHold.StartTime;

			const int32 ReleasedHoldGeneration = HoldState.CurrentHold.HoldID;
			const EInputType ReleasedInputType = HoldState.CurrentHold.InputType;
			const uint64 ReleasedInputSerial = HoldState.CurrentHold.ReleaseInputSerial != 0
				? HoldState.CurrentHold.ReleaseInputSerial
				: HoldState.CurrentHold.PressInputSerial;
			const bool bFollowUpStarted = DispatchHoldOwnedFollowUp(ReleasedHoldGeneration);
			TerminateHoldIfMatches(ReleasedHoldGeneration);
			FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(
				EActionReactionTelemetryEvent::HoldStateChanged,
				EActionReactionTelemetryReason::HoldReleased);
			Telemetry.InputType = ReleasedInputType;
			Telemetry.InputSerial = ReleasedInputSerial;
			Telemetry.HoldGeneration = ReleasedHoldGeneration;
			Telemetry.Detail = bFollowUpStarted
				? TEXT("LightEaseOutFollowUpStarted")
				: FString::Printf(TEXT("LightEaseOutNoFollowUp;Duration=%.3f"), TotalHoldDuration);
			AppendActionReactionTelemetry(MoveTemp(Telemetry));
			if (!bFollowUpStarted)
			{
				ProcessQueuedActions(EAttackPhase::Recovery);
			}
		}

		if (CombatDebug::IsHoldDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[HOLD TIMER] %s complete, final playrate: %.2f"),
				bIsEasingIn ? TEXT("EASE-IN") : TEXT("EASE-OUT"),
				HoldState.CurrentPlayRate);
		}
		return;
	}

	// PROCEDURAL EASING: Calculate playrate using CalculateTransitionPlayRate
	// This eliminates need for authored curves!
	float NewPlayRate = UMontageUtilityLibrary::CalculateTransitionPlayRate(
		HoldState.EaseStartPlayRate,         // Start rate (1.0 or HoldTargetPlayRate)
		TargetPlayRate,                       // Target rate (HoldTargetPlayRate or 1.0)
		ElapsedTime,                          // Current time in transition
		EaseDuration,                         // Transition duration (from AttackData)
		EasingType                            // Easing curve (EaseInType or EaseOutType)
	);

	// Update hold state
	HoldState.CurrentPlayRate = NewPlayRate;

	// Apply playrate to montage
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UMontageUtilityLibrary::SetMontagePlayRate(Character, NewPlayRate);

	if (CombatDebug::IsHoldDebugEnabled())
	{
		UE_LOG(LogCombat, Verbose, TEXT("[HOLD TIMER] %s playrate: %.2f → %.2f (%.1f%% complete)"),
			bIsEasingIn ? TEXT("EASE-IN") : TEXT("EASE-OUT"),
			HoldState.EaseStartPlayRate, TargetPlayRate,
			(ElapsedTime / EaseDuration) * 100.0f);
	}
}

// ============================================================================
// PHASE TRANSITION SYSTEM
// ============================================================================

void UCombatComponent::OnPhaseTransition(EAttackPhase NewPhase)
{
	// CRITICAL: Update CurrentPhase FIRST before any other logic
	// This ensures DetermineExecutionMode sees the correct phase for incoming input
	SetPhase(NewPhase);

	// PHASE 9: EVENT-DRIVEN QUEUE PROCESSING
	// Execute queued actions that target this phase transition
	// This replaces tick-based ProcessQueue() polling!
	ProcessQueuedActions(NewPhase);

	if (CombatDebug::IsPhaseDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[PHASE] Phase transition complete: %s (queue processed event-driven)"),
			*UEnum::GetValueAsString(NewPhase));
	}
}

bool UCombatComponent::OnPhaseTransitionWithContext(
	const EAttackPhase NewPhase,
	const FAnimNotifyRuntimeSourceId& NotifySource,
	const int32 MontageInstanceId,
	const float RemainingWindowDuration)
{
	if (!NotifySource.IsValid() || MontageInstanceId < 0)
	{
		return false;
	}

	if (NewPhase == EAttackPhase::Active)
	{
		FAttackInstanceId CurrentAttack;
		CurrentAttack.Attacker = GetOwner();
		CurrentAttack.AttackGeneration = AttackStateMachine.AttackGeneration;
		if (ActiveHitWindow.IsValid()
			&& ActiveHitWindow.AttackInstance == CurrentAttack
			&& ActiveHitWindow.NotifySource == NotifySource
			&& ActiveHitWindow.MontageInstanceId == MontageInstanceId)
		{
			return false;
		}

		const FAttackWindowInstanceId HitWindow = OpenAttackWindow(
			EAttackWindowKind::Hit,
			NotifySource,
			MontageInstanceId,
			RemainingWindowDuration);
		if (!HitWindow.IsValid())
		{
			return false;
		}

		OnPhaseTransition(NewPhase);
		return true;
	}

	if (NewPhase == EAttackPhase::Recovery)
	{
		if (!CloseHitWindowFromPhaseTransition(NotifySource, MontageInstanceId))
		{
			return false;
		}

		OnPhaseTransition(NewPhase);
		return true;
	}

	return false;
}

bool UCombatComponent::CloseHitWindowFromPhaseTransition(
	const FAnimNotifyRuntimeSourceId& CloseSource,
	const int32 MontageInstanceId)
{
	if (!CloseSource.IsValid() || MontageInstanceId < 0)
	{
		return false;
	}

	const int32 RecordIndex = OpenAttackWindowRecords.IndexOfByPredicate(
		[MontageInstanceId](const FAttackWindowInstanceId& Candidate)
		{
			return Candidate.Kind == EAttackWindowKind::Hit
				&& Candidate.MontageInstanceId == MontageInstanceId;
		});
	if (RecordIndex == INDEX_NONE)
	{
		return false;
	}

	const FAttackWindowInstanceId ClosingWindow = OpenAttackWindowRecords[RecordIndex];
	OpenAttackWindowRecords.RemoveAt(RecordIndex, 1, EAllowShrinking::No);
	if (!(ActiveHitWindow == ClosingWindow))
	{
		return false;
	}

	ActiveHitWindow = {};
	RequestDefenderThreatRefresh(AttackIntentTarget.Get(), EThreatRefreshReason::WindowChanged);
	return true;
}

void UCombatComponent::ClearPublishedAttackWindowsForAttack(const FAttackInstanceId& AttackInstance)
{
	if (!AttackInstance.IsValid())
	{
		return;
	}

	if (ActiveHitWindow.AttackInstance == AttackInstance)
	{
		ActiveHitWindow = {};
	}
	if (ActiveParryWindow.AttackInstance == AttackInstance)
	{
		ActiveParryWindow = {};
	}
	if (ActiveCounterWindow.AttackInstance == AttackInstance)
	{
		ActiveCounterWindow = {};
	}
	OpenAttackWindowRecords.RemoveAll([&AttackInstance](const FAttackWindowInstanceId& Candidate)
	{
		return Candidate.AttackInstance == AttackInstance;
	});
}

void UCombatComponent::SetPhase(EAttackPhase NewPhase)
{
	if (CurrentPhase == NewPhase)
	{
		if (NewPhase == EAttackPhase::None)
		{
			ResetTerminalAttackState();
		}
		return;
	}

	EAttackPhase OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;
	FActionReactionTelemetryRecord PhaseTelemetry = BuildActionReactionTelemetryRecord(
		EActionReactionTelemetryEvent::PhaseChanged,
		EActionReactionTelemetryReason::None);
	PhaseTelemetry.AttackPhase = NewPhase;
	PhaseTelemetry.Detail = FString::Printf(
		TEXT("%s -> %s"),
		*UEnum::GetValueAsString(OldPhase),
		*UEnum::GetValueAsString(NewPhase));
	AppendActionReactionTelemetry(MoveTemp(PhaseTelemetry));
	if (NewPhase == EAttackPhase::None)
	{
		ResetTerminalAttackState();
	}

	// STATE MACHINE: Notify phase change
	AttackStateMachine.OnPhaseChanged(NewPhase);

	if (CombatDebug::IsPhaseDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[PHASE] Phase transition: %d → %d (Gen=%u)"),
			static_cast<int32>(OldPhase),
			static_cast<int32>(NewPhase),
			AttackStateMachine.AttackGeneration);
	}

	// Broadcast phase changed event
	OnPhaseChanged.Broadcast(OldPhase, NewPhase);

	// Handle phase-specific logic
	switch (NewPhase)
	{
		case EAttackPhase::Active:
			// ENABLE HIT DETECTION when entering Active phase
			// This is where the weapon can deal damage
			if (ABaseCombatCharacter* Character = GetOwnerCharacter())
			{
				ICombatInterface::Execute_OnEnableHitDetection(Character);

				if (CombatDebug::IsPhaseDebugEnabled())
				{
					UE_LOG(LogCombat, Log, TEXT("[PHASE] Active entered - Hit detection ENABLED"));
				}
			}
			break;

		case EAttackPhase::Recovery:
			// DISABLE HIT DETECTION when entering Recovery phase
			// Active window is over, no more damage
			if (ABaseCombatCharacter* Character = GetOwnerCharacter())
			{
				ICombatInterface::Execute_OnDisableHitDetection(Character);
			}

			if (CombatDebug::IsPhaseDebugEnabled())
			{
				UE_LOG(LogCombat, Log, TEXT("[PHASE] Recovery entered - Hit detection DISABLED"));
			}
			break;

		case EAttackPhase::None:
			// DISABLE HIT DETECTION if we were in Active phase (handles interrupts)
			if (OldPhase == EAttackPhase::Active)
			{
				if (ABaseCombatCharacter* Character = GetOwnerCharacter())
				{
					ICombatInterface::Execute_OnDisableHitDetection(Character);
				}
			}

			if (CombatDebug::IsPhaseDebugEnabled())
			{
				UE_LOG(LogCombat, Log, TEXT("[PHASE] Attack finished - Combo state, hold state, and input context cleared"));
			}
			break;

		default:
			break;
	}
}

void UCombatComponent::ResetTerminalAttackState()
{
	FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(
		EActionReactionTelemetryEvent::TerminalReset,
		EActionReactionTelemetryReason::TerminalCleanup);
	Telemetry.TerminalDisposition = TEXT("AttackStateReset");
	Telemetry.Detail = FString::Printf(
		TEXT("ComboWindow=%s;PendingQueue=%d"),
		bComboWindowActive ? TEXT("Open") : TEXT("Closed"),
		ActionQueue.Num());
	AppendActionReactionTelemetry(MoveTemp(Telemetry));

	FAttackInstanceId EndingAttack;
	EndingAttack.Attacker = GetOwner();
	EndingAttack.AttackGeneration = AttackStateMachine.AttackGeneration;
	ClearPublishedAttackWindowsForAttack(EndingAttack);
	InvalidateAttackThreatPrediction(EThreatInvalidationReason::AttackEnded);
	AttackIntentTarget.Reset();
	if (ABaseCombatCharacter* Character = GetOwnerCharacter())
	{
		if (UTargetingComponent* Targeting = Character->GetTargetingComponent())
		{
			Targeting->ReleaseActiveAttackWarp();
		}
	}

	CurrentAttackData = nullptr;
	CurrentAttackInputType = EInputType::None;
	ActiveAttackStartupOperationSerial = 0;
	bComboWindowActive = false;
	ComboWindowStart = 0.0f;
	ComboWindowDuration = 0.0f;
	Checkpoints.Empty();
	SetInputContext(EInputContext::Movement);
	ClearHoldState();
}
/*TODO: Consider adding OnPhaseEnter/Exit events for more granular control --> Also we know that active phase is when queued input actions can be executed so perhaps having phase specific logic for simple things like this would be prudent*/

void UCombatComponent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (CombatDebug::IsPhaseDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[MONTAGE] Montage blending out: %s | Interrupted: %s"),
			Montage ? *Montage->GetName() : TEXT("None"),
			bInterrupted ? TEXT("YES") : TEXT("NO"));
	}

	// Broadcast montage event
	OnMontageEvent.Broadcast(Montage, bInterrupted, FName("BlendingOut"));

	// Prepare for next attack during blend out (smoother transitions)
	// Phase transition to None happens in OnMontageEnded
	/*TODO: Consider allowing new attack to start here during blend out? Place some helpful checks and transition smoothing here of some sort*/
}

void UCombatComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	const float CurrentWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const auto EmitMontageRecord = [this, Montage, bInterrupted](
		const EActionReactionTelemetryEvent Event,
		const EActionReactionTelemetryReason Reason,
		const TCHAR* Path)
	{
		FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(Event, Reason);
		if (Montage)
		{
			Telemetry.MontagePath = FSoftObjectPath(Montage);
		}
		Telemetry.Detail = FString::Printf(
			TEXT("Path=%s;Interrupted=%s;PendingTransitions=%d"),
			Path,
			bInterrupted ? TEXT("true") : TEXT("false"),
			AttackStateMachine.PendingComboTransitions);
		AppendActionReactionTelemetry(MoveTemp(Telemetry));
	};

	if (CombatDebug::IsPhaseDebugEnabled())
	{
		const float TimeSinceSectionStart = CurrentWorldTime - AttackStateMachine.SectionStartTime;
		UE_LOG(LogCombat, Log, TEXT("[MONTAGE] Montage ended: %s | Interrupted: %s | Gen=%d | Section=%s | TimeSinceStart=%.3fs"),
			Montage ? *Montage->GetName() : TEXT("None"),
			bInterrupted ? TEXT("YES") : TEXT("NO"),
			AttackStateMachine.AttackGeneration,
			*AttackStateMachine.ActiveSectionName.ToString(),
			TimeSinceSectionStart);
	}

	// ========================================================================
	// PAIRED ANIMATION COMPLETION - CHECK FIRST (before attack state filtering)
	// ========================================================================
	// Finisher/counter montages are separate from regular attack montages.
	// The attack state machine filter below would reject finisher montage callbacks
	// because they don't match ActiveMontage. Process paired animation completion
	// BEFORE filtering to ensure finishers complete properly.
	if (CachedPairedAnimComp
		&& Montage
		&& CachedPairedAnimComp->HandleOwnerPairedMontageEnded(Montage, bInterrupted))
	{
		EmitMontageRecord(
			EActionReactionTelemetryEvent::MontageCallbackAccepted,
			bInterrupted
				? EActionReactionTelemetryReason::MontageInterrupted
				: EActionReactionTelemetryReason::MontageCompleted,
			TEXT("PairedAnimation"));
		return;
	}

	// ========================================================================
	// STATE MACHINE CALLBACK FILTERING (INPUT-1 systemic fix)
	// ========================================================================
	// The state machine is the SINGLE AUTHORITY on whether a montage-end callback
	// should be processed. It uses a PendingComboTransitions counter:
	// - Each OnComboTransition() increments the counter (expecting a stale callback)
	// - Each rejected stale callback decrements it (consumed)
	// This handles ALL async blend-out timing issues — same-montage sections,
	// cross-montage combos, any blend duration — without boolean guards or heuristics.
	// ========================================================================

	if (!AttackStateMachine.ShouldProcessMontageEnd(Montage, bInterrupted, CurrentWorldTime))
	{
		EmitMontageRecord(
			EActionReactionTelemetryEvent::MontageCallbackRejected,
			EActionReactionTelemetryReason::StaleMontageCallback,
			TEXT("RegularAttackFilter"));
		if (CombatDebug::IsPhaseDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[STATE] Ignoring montage end callback (Gen=%d, PendingTransitions=%d)"),
				AttackStateMachine.AttackGeneration, AttackStateMachine.PendingComboTransitions);
		}
		// Stale callback — completely ignored. Queue, checkpoints, phase all preserved.
		return;
	}
	const int32 EndingAttackGeneration = AttackStateMachine.AttackGeneration;
	EmitMontageRecord(
		EActionReactionTelemetryEvent::MontageCallbackAccepted,
		bInterrupted
			? EActionReactionTelemetryReason::MontageInterrupted
			: EActionReactionTelemetryReason::MontageCompleted,
		TEXT("RegularAttack"));

	// ========================================================================
	// PROCESS VALID CALLBACK
	// ========================================================================
	if (bInterrupted)
	{
		// If we reach here with bInterrupted=true, the state machine has already
		// confirmed this is NOT a stale combo-transition callback (Rule 0 rejected those).
		// This is a genuine abnormal interrupt (death, stun, knockback, paired anim entry).
		// Reset input state to prevent leaks.
		SetInputContext(EInputContext::Movement);
		DirectionalInputBuffer.Reset();
		ClearHoldState();

		// Defensive phase reset on abnormal interrupt.
		// Normal flow self-corrects via checkpoint notifies, but abnormal interrupts
		// can leave phase stuck on Active/Windup since no more notifies will fire.
		if (CurrentPhase != EAttackPhase::None)
		{
			if (CombatDebug::IsPhaseDebugEnabled())
			{
				UE_LOG(LogCombat, Warning, TEXT("[MONTAGE] Genuine interrupt while phase=%s - forcing phase reset to None"),
					*UEnum::GetValueAsString(CurrentPhase));
			}
			SetPhase(EAttackPhase::None);
		}

		if (CombatDebug::IsPhaseDebugEnabled())
		{
			UE_LOG(LogCombat, Warning, TEXT("[MONTAGE] Genuine interrupt - input state reset"));
		}
	}

	// Broadcast montage event
	OnMontageEvent.Broadcast(Montage, bInterrupted, FName("Ended"));

	// CRITICAL: Execute any pending queued actions BEFORE clearing state
	/*TODO: In recent debug logs, checkpoint discovery has been failing, please ensure that the checkpoint system still works as designed*/
	
	// BUT only if their checkpoint was actually reached (ScheduledTime >= 0)
	// Actions with ScheduledTime=-1.0 mean checkpoint was never registered → discard them
	if (ActionQueue.Num() > 0)
	{
		if (CombatDebug::IsQueueDebugEnabled())
		{
			UE_LOG(LogCombat, Warning, TEXT("[MONTAGE] Montage ended with %d queued actions - checking which are ready"), ActionQueue.Num());
		}

		// Get montage end time to check if actions reached their checkpoint
		float MontageEndTime = 0.0f;
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			if (Character->GetMesh())
			if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
			{
				MontageEndTime = AnimInstance->Montage_GetPosition(Montage);
			}
		}

		// Execute pending actions that reached their checkpoint
		for (int32 i = ActionQueue.Num() - 1; i >= 0; --i)
		{
			if (!ActionQueue.IsValidIndex(i))
			{
				continue;
			}
			FActionQueueEntry Entry = ActionQueue[i];

			if (Entry.IsPending())
			{
				// CRITICAL FIX: Only execute if checkpoint was reached
				// ScheduledTime=-1.0 means Active→Recovery checkpoint never registered → discard
				if (Entry.ScheduledTime < 0.0f)
				{
					if (CombatDebug::IsQueueDebugEnabled())
					{
						UE_LOG(LogCombat, Warning, TEXT("[QUEUE] Discarding action (checkpoint never reached): Type=%s, ScheduledTime=%.2f"),
							*UEnum::GetValueAsString(Entry.InputAction.InputType), Entry.ScheduledTime);
					}

					// Discard action - checkpoint never happened
					Entry.State = EActionState::Cancelled;
					AppendActionQueueTelemetry(
						Entry,
						EActionReactionTelemetryEvent::QueueCancelled,
						EActionReactionTelemetryReason::CheckpointNotReached);
					ActionQueue.RemoveAt(i);
					QueueStats.ActionsCancelled++;
					continue;
				}

				// Only execute if montage reached the scheduled time
				if (MontageEndTime >= Entry.ScheduledTime)
				{
					if (CombatDebug::IsQueueDebugEnabled())
					{
						UE_LOG(LogCombat, Log, TEXT("[QUEUE] Executing action from ended montage: Type=%s, ScheduledTime=%.2f, MontageEndTime=%.2f"),
							*UEnum::GetValueAsString(Entry.InputAction.InputType), Entry.ScheduledTime, MontageEndTime);
					}

					// Startup can synchronously clear or replace the queue through callbacks.
					ActionQueue.RemoveAt(i);
					const TWeakObjectPtr<UCombatComponent> ComponentSnapshot(this);
					const bool bExecuted = ExecuteAction(Entry);
					if (!ComponentSnapshot.IsValid())
					{
						return;
					}
					if (bExecuted)
					{
						QueueStats.ActionsExecuted++;

						// Only execute the first valid action (FIFO)
						break;
					}
					Entry.State = EActionState::Cancelled;
					AppendActionQueueTelemetry(
						Entry,
						EActionReactionTelemetryEvent::QueueCancelled,
						EActionReactionTelemetryReason::ExecutionFailed);
					QueueStats.ActionsCancelled++;
				}
				else
				{
					if (CombatDebug::IsQueueDebugEnabled())
					{
						UE_LOG(LogCombat, Warning, TEXT("[QUEUE] Discarding action (montage ended before checkpoint): Type=%s, ScheduledTime=%.2f, MontageEndTime=%.2f"),
							*UEnum::GetValueAsString(Entry.InputAction.InputType), Entry.ScheduledTime, MontageEndTime);
					}

					// Discard action - montage ended before checkpoint
					Entry.State = EActionState::Cancelled;
					AppendActionQueueTelemetry(
						Entry,
						EActionReactionTelemetryEvent::QueueCancelled,
						EActionReactionTelemetryReason::MontageEndedBeforeCheckpoint);
					ActionQueue.RemoveAt(i);
					QueueStats.ActionsCancelled++;
				}
			}
		}
	}

	// ========================================================================
	// FINAL CLEANUP (only reached for valid callbacks)
	// NOTE: Paired animation completion is now handled at the start of OnMontageEnded,
	// before the state machine filter, to ensure finisher montages complete properly.
	// ========================================================================

	// Broadcasts and queued execution above may synchronously start a successor,
	// including another section of the same montage. Only the exact ending attack
	// is allowed to clear lifecycle state and checkpoints.
	const bool bEndingAttackStillOwnsState =
		AttackStateMachine.AttackGeneration == EndingAttackGeneration
		&& AttackStateMachine.IsOwnerMontage(Montage);
	if (bEndingAttackStillOwnsState)
	{
		AttackStateMachine.OnMontageEndProcessed(Montage, bInterrupted);
		SetPhase(EAttackPhase::None);
	}

	// SLOPE FIX: Safety net - snap character to ground if floating after attack
	// This catches cases where terrain-aware warp target wasn't enough (e.g., pure root motion without warp)
	if (!AttackStateMachine.IsComboBlending())
	{
		ABaseCombatCharacter* Character = GetOwnerCharacter();
		if (Character && UDebugUtils::IsCharacterFloating(Character, 5.0f))
		{
			const bool bSnapped = UDebugUtils::SnapCharacterToGround(Character, 5.0f, GetDebugDraw());
			if (bSnapped && GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[SLOPE FIX] Character snapped to ground after attack montage"));
			}
		}
	}

}

//NOTE:: OnMontageEnded is where queued actions get a last chance to execute if their checkpoint was reached.

void UCombatComponent::ClearHoldState()
{
	// CRITICAL: Complete hold state cleanup when starting new attack or on montage end
	// Prevents state leaks between attacks

	const int32 ReleasedHoldGeneration = HoldState.CurrentHold.HoldID;
	const EInputType ReleasedInputType = HoldState.CurrentHold.InputType;
	const uint64 ReleasedInputSerial = HoldState.CurrentHold.ReleaseInputSerial != 0
		? HoldState.CurrentHold.ReleaseInputSerial
		: HoldState.CurrentHold.PressInputSerial;

	// Cancel any active ease timer
	if (GetWorld() && EaseTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(EaseTimerHandle);

		if (CombatDebug::IsHoldDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[HOLD] Ease timer cleared"));
		}
	}

	// Deactivate hold state
	if (HoldState.IsHolding() || HoldState.bIsEasing)
	{
		HoldState.Deactivate();
		FActionReactionTelemetryRecord Telemetry = BuildActionReactionTelemetryRecord(
			EActionReactionTelemetryEvent::HoldStateChanged,
			EActionReactionTelemetryReason::HoldReleased);
		Telemetry.InputType = ReleasedInputType;
		Telemetry.InputSerial = ReleasedInputSerial;
		Telemetry.HoldGeneration = ReleasedHoldGeneration;
		Telemetry.Detail = TEXT("TerminalOrAttackCleanup");
		AppendActionReactionTelemetry(MoveTemp(Telemetry));

		if (CombatDebug::IsHoldDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[HOLD] Hold state cleared"));
		}
	}

	// CRITICAL: Forcibly restore montage playrate to 1.0
	// This prevents blending artifacts when combo interrupts hold ease mid-transition
	// Without this, new montage starts with wrong playrate (e.g., 0.75) causing "partial blend" visual issues
	ABaseCombatCharacter* Character = GetOwnerCharacter();
	if (Character)
	{
		float CurrentPlayRate = UMontageUtilityLibrary::GetMontagePlayRate(Character);
		if (!FMath::IsNearlyEqual(CurrentPlayRate, 1.0f, 0.01f))
		{
			UMontageUtilityLibrary::SetMontagePlayRate(Character, 1.0f);

			if (CombatDebug::IsHoldDebugEnabled())
			{
				UE_LOG(LogCombat, Log, TEXT("[HOLD] Playrate restored: %.2f → 1.0"), CurrentPlayRate);
			}
		}
	}

	// ARCHITECTURAL FIX: Reset input context to Movement
	// Hold window closed, movement stick no longer interpreted as directional input
	SetInputContext(EInputContext::Movement);

	if (CombatDebug::IsHoldDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[HOLD] Directional input window CLOSED"));
	}

}

void UCombatComponent::SetupAttackWarp(const FActionQueueEntry& Action)
{
	UAttackData* AttackData = Action.AttackData;
	// Early exit if no valid attack data or warp disabled
	if (!AttackData || !AttackData->WarpConfig.bEnableWarp)
	{
		return;
	}

	ABaseCombatCharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		return;
	}

	// Get targeting component for soft aim assist
	UTargetingComponent* Targeting = Character->GetTargetingComponent();
	if (!Targeting)
	{
		return;
	}

	const auto PublishTargetGuidance = [this, Character, AttackData](AActor* Target)
	{
		SetAttackIntentTarget(Target);
		if (!Target)
		{
			return;
		}
		if (AttackData->WarpConfig.TargetRelativeOffset.ContainsNaN())
		{
			InvalidateAttackThreatPrediction(EThreatInvalidationReason::PathChanged);
			return;
		}

		const FVector PredictedContactPoint = Target->GetActorLocation()
			+ Target->GetActorRotation().RotateVector(
				AttackData->WarpConfig.TargetRelativeOffset);
		const FVector Path = PredictedContactPoint - Character->GetActorLocation();
		if (Path.IsNearlyZero())
		{
			return;
		}

		FAttackThreatPrediction Prediction;
		Prediction.IntendedTarget = Target;
		Prediction.PathOrigin = Character->GetActorLocation();
		Prediction.PathDirection = Path.GetSafeNormal();
		Prediction.PredictedContactPoint = PredictedContactPoint;
		Prediction.SourceSocket = AttackData->DefenseProfile.SourceContactSocketOverride.IsNone()
			? AttackData->AttackHand
			: AttackData->DefenseProfile.SourceContactSocketOverride;
		Prediction.DefenderTargetBone = AttackData->GetDefenseTargetBoneFallback();
		Prediction.PredictionSimulationTimestamp = GetWorld()
			? static_cast<double>(GetWorld()->GetTimeSeconds())
			: 0.0;
		Prediction.Lane = AttackData->DefenseProfile.NominalLane;
		Prediction.Height = AttackData->DefenseProfile.Height;
		Prediction.Confidence = EDefensePredictionConfidence::Low;
		Prediction.bPathIntersectsThreatVolume = true;
		PublishAttackThreatPrediction(Prediction);
	};

	if (AActor* ExplicitTarget = ExplicitAttackWarpTarget.Get())
	{
		PublishTargetGuidance(ExplicitTarget);
		const FVector ToTarget = ExplicitTarget->GetActorLocation() - Character->GetActorLocation();
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator TargetRotation = ToTarget.Rotation();
			const bool bSuccess = Targeting->SetupAttackWarp(ExplicitTarget, TargetRotation, AttackData->WarpConfig);

			if (GetDebugDraw())
			{
				if (bSuccess)
				{
					UE_LOG(LogCombat, Log, TEXT("[ATTACK WARP] Explicit target %s setup succeeded (Rotation=%.1f°)"),
						*ExplicitTarget->GetName(), TargetRotation.Yaw);
				}
				else
				{
					UE_LOG(LogCombat, Warning, TEXT("[ATTACK WARP] Explicit target %s setup failed (Rotation=%.1f°)"),
						*ExplicitTarget->GetName(), TargetRotation.Yaw);
				}
			}
		}
		return;
	}

	// Determine direction source and convert to world vector
	EInputDirection FinalDirection = EInputDirection::None;
	FVector WorldDirection = FVector::ZeroVector;
	FString DirectionSource = TEXT("None");

	// PRIORITY 1: Use buffered direction from hold release (if available)
	if (DirectionalInputBuffer.HasValidInput())
	{
		FinalDirection = DirectionalInputBuffer.DirectionAtRelease;
		WorldDirection = UCombatUtils::InputDirectionToWorldVector(FinalDirection, Character);
		DirectionSource = TEXT("HoldRelease");
	}
	// PRIORITY 2: Use the immutable world-space intent from this attack edge.
	else
	{
		const FAttackFacingIntent& FacingIntent = Action.InputAction.FacingIntent;
		if (FacingIntent.IsFinite()
			&& FacingIntent.bHasWorldDirection
			&& !FacingIntent.WorldDirection.IsNearlyZero())
		{
			FinalDirection = FacingIntent.BranchDirection;
			WorldDirection = FacingIntent.WorldDirection.GetSafeNormal2D();
			DirectionSource = TEXT("CapturedAttackEdge");
		}
	}

	// Variables for target and rotation
	AActor* BestTarget = nullptr;
	FRotator TargetRotation = FRotator::ZeroRotator;
	const FAttackWarpConfig& WarpConfig = AttackData->WarpConfig;
	const FRotator CurrentFacing = Character->GetActorRotation();

	// CASE 1 & 2: We have a direction (from hold release or live input)
	if (FinalDirection != EInputDirection::None && !WorldDirection.IsNearlyZero())
	{
		// Use soft aim assist to find best target in direction
		// If found, will use translation+rotation warp; otherwise, rotation-only
		TargetRotation = Targeting->FindBestTargetForDirection(
			WorldDirection,
			BestTarget,  // OutBestTarget is second parameter
			-1.0f,       // Use TargetingSettings defaults for remaining params
			-1.0f,
			-1.0f,
			-1.0f,
			-1.0f
		);
		PublishTargetGuidance(BestTarget);

		// Check if we should skip because already facing this direction (and no target to snap to)
		if (!BestTarget && WarpConfig.AlreadyFacingThreshold > 0.0f)
		{
			const float AngleDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentFacing.Yaw, TargetRotation.Yaw));
			if (AngleDelta <= WarpConfig.AlreadyFacingThreshold)
			{
				if (GetDebugDraw())
				{
					UE_LOG(LogCombat, Verbose, TEXT("[ATTACK WARP] Skipped: Already facing direction (delta=%.1f° <= threshold=%.1f°)"),
						AngleDelta, WarpConfig.AlreadyFacingThreshold);
				}
				// Clear buffer if we used it
				if (DirectionSource == TEXT("HoldRelease"))
				{
					DirectionalInputBuffer.Reset();
				}
				return;
			}
		}
	}
	// CASE 3: No direction input - try to find nearest target within facing cone
	else
	{
		// Only search within the configured facing cone (prevents 180° snaps)
		BestTarget = Targeting->FindNearestTarget(-1.0f, WarpConfig.NoInputFacingCone);
		PublishTargetGuidance(BestTarget);

		if (BestTarget)
		{
			// Warp toward the nearest target
			const FVector ToTarget = BestTarget->GetActorLocation() - Character->GetActorLocation();
			TargetRotation = ToTarget.Rotation();
			DirectionSource = TEXT("NearestTarget");

			// Check if we should skip because already facing the target
			if (WarpConfig.AlreadyFacingThreshold > 0.0f)
			{
				const float AngleDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentFacing.Yaw, TargetRotation.Yaw));
				if (AngleDelta <= WarpConfig.AlreadyFacingThreshold)
				{
					if (GetDebugDraw())
					{
						UE_LOG(LogCombat, Verbose, TEXT("[ATTACK WARP] Skipped: Already facing target %s (delta=%.1f°)"),
							*BestTarget->GetName(), AngleDelta);
					}
					return;
				}
			}

			if (GetDebugDraw())
			{
				UE_LOG(LogCombat, Log, TEXT("[ATTACK WARP] No input direction - using nearest target in %.0f° cone: %s"),
					WarpConfig.NoInputFacingCone, *BestTarget->GetName());
			}
		}
		// CASE 4: No target - preserve the facing captured on this attack edge.
		else
		{
			const FAttackFacingIntent& FacingIntent = Action.InputAction.FacingIntent;
			if (!FacingIntent.IsFinite())
			{
				return;
			}
			TargetRotation = FRotator(0.0f, FacingIntent.CapturedFacingYaw, 0.0f);
			DirectionSource = TEXT("CapturedFacing");
			const float AngleDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(
				CurrentFacing.Yaw, TargetRotation.Yaw));
			if (WarpConfig.AlreadyFacingThreshold > 0.0f
				&& AngleDelta <= WarpConfig.AlreadyFacingThreshold)
			{
				return;
			}
		}
	}

	// Setup the attack warp using unified function
	// If BestTarget exists: translation+rotation warp (uses TargetWarpName)
	// If BestTarget is null: rotation-only warp (uses RotationWarpName)
	const bool bSuccess = Targeting->SetupAttackWarp(
		BestTarget,
		TargetRotation,
		WarpConfig
	);

	if (GetDebugDraw())
	{
		if (bSuccess)
		{
			const bool bHasTarget = BestTarget != nullptr;
			UE_LOG(LogCombat, Log, TEXT("[ATTACK WARP] Set up %s warp: Direction=%s, Source=%s, Target=%s, Rotation=%.1f°"),
				bHasTarget ? TEXT("TARGET") : TEXT("ROTATION"),
				FinalDirection != EInputDirection::None ? *UEnum::GetValueAsString(FinalDirection) : TEXT("N/A"),
				*DirectionSource,
				BestTarget ? *BestTarget->GetName() : TEXT("None"),
				TargetRotation.Yaw);
		}
		else
		{
			UE_LOG(LogCombat, Warning, TEXT("[ATTACK WARP] Setup failed: Direction=%s, Source=%s"),
				FinalDirection != EInputDirection::None ? *UEnum::GetValueAsString(FinalDirection) : TEXT("N/A"),
				*DirectionSource);
		}
	}

	// Clear directional buffer after use (one-shot) - only if we used it
	if (DirectionSource == TEXT("HoldRelease"))
	{
		DirectionalInputBuffer.Reset();
	}
}

// ============================================================================
// STATE QUERIES
// ============================================================================

int32 UCombatComponent::GetPendingActionCount() const
{
	int32 Count = 0;
	for (const FActionQueueEntry& Entry : ActionQueue)
	{
		if (Entry.IsPending())
		{
			Count++;
		}
	}
	return Count;
}

float UCombatComponent::GetHoldDuration() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}
	return HoldState.GetHoldDuration(World->GetTimeSeconds());
}

// ============================================================================
// DEBUG / VISUALIZATION
// ============================================================================

void UCombatComponent::DrawDebugInfo() const
{
	if (!GetOwner() || !CombatDebug::IsCombatStateDebugEnabled())
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	const FVector OwnerLocation = Character->GetActorLocation();
	int32 LineIndex = 0;
	const auto DrawStateLine = [this, &OwnerLocation, &LineIndex](
		const FString& Text,
		const FColor Color)
	{
		const FVector LineOffset(0.0, 0.0, 65.0 + 28.0 * LineIndex++);
		DrawDebugString(GetWorld(), OwnerLocation + LineOffset, Text, nullptr, Color, 0.0f, true);
	};

	if (CombatDebug::IsPhaseDebugEnabled())
	{
		FColor PhaseColor = FColor::White;
		switch (CurrentPhase)
		{
		case EAttackPhase::Windup: PhaseColor = FColor::Orange; break;
		case EAttackPhase::Active: PhaseColor = FColor::Red; break;
		case EAttackPhase::Recovery: PhaseColor = FColor::Yellow; break;
		case EAttackPhase::None: break;
		}
		DrawStateLine(
			FString::Printf(TEXT("Phase: %s"), *UEnum::GetValueAsString(CurrentPhase)),
			PhaseColor);
	}

	if (CombatDebug::IsQueueDebugEnabled())
	{
		DrawStateLine(
			FString::Printf(TEXT("Queue: %d pending | %d total"),
				GetPendingActionCount(), ActionQueue.Num()),
			FColor::Cyan);

		int32 ActionIndex = 0;
		for (const FActionQueueEntry& Entry : ActionQueue)
		{
			if (!Entry.IsPending())
			{
				continue;
			}

			DrawStateLine(
				FString::Printf(TEXT("[%d] %s @ %.2f (%s)"),
					ActionIndex++,
					*UEnum::GetValueAsString(Entry.InputAction.InputType),
					Entry.ScheduledTime,
					*UEnum::GetValueAsString(Entry.ExecutionMode)),
				FColor::Cyan);
		}

		DrawStateLine(
			FString::Printf(TEXT("Stats: %d executed (%d queued + %d immediate) | %d cancelled"),
				QueueStats.ActionsExecuted,
				QueueStats.QueuedExecutions,
				QueueStats.ImmediateExecutions,
				QueueStats.ActionsCancelled),
			FColor::White);

		const float CurrentMontageTime = UMontageUtilityLibrary::GetCurrentMontageTime(Character);
		if (Checkpoints.Num() > 0 && CurrentMontageTime >= 0.0f)
		{
			UMontageUtilityLibrary::DrawCheckpointTimeline(
				GetWorld(), Character, Checkpoints, 0.0f, 150.0f);
			if (Checkpoints.Num() != DebugLastCheckpointCount)
			{
				UMontageUtilityLibrary::LogCheckpoints(Checkpoints, TEXT("DEBUG"));
				DebugLastCheckpointCount = Checkpoints.Num();
			}
		}

		if (bComboWindowActive && CurrentMontageTime >= 0.0f)
		{
			const float ComboWindowEnd = ComboWindowStart + ComboWindowDuration;
			if (CurrentMontageTime < ComboWindowStart)
			{
				DrawStateLine(
					FString::Printf(TEXT("Combo window: opens in %.2fs"),
						ComboWindowStart - CurrentMontageTime),
					FColor::Yellow);
			}
			else if (CurrentMontageTime <= ComboWindowEnd)
			{
				DrawStateLine(
					FString::Printf(TEXT("Combo window: %.2fs remaining"),
						ComboWindowEnd - CurrentMontageTime),
					FColor::Green);
			}
			else
			{
				DrawStateLine(
					FString::Printf(TEXT("Combo window: expired %.2fs ago (stale active flag)"),
						CurrentMontageTime - ComboWindowEnd),
					FColor::Red);
			}
		}
	}

	if (CombatDebug::IsHoldDebugEnabled())
	{
		const FString HoldInfo = HoldState.IsHolding()
			? FString::Printf(TEXT("Hold: %s | %s | Gen %d | %.2fs"),
				*UEnum::GetValueAsString(HoldState.GetHeldInputType()),
				*UEnum::GetValueAsString(HoldState.CurrentHold.Phase),
				HoldState.CurrentHold.HoldID,
				GetHoldDuration())
			: TEXT("Hold: Inactive");
		DrawStateLine(HoldInfo, HoldState.IsHolding() ? FColor::Yellow : FColor::White);

		const EActionReactionTelemetryReason MovementReason =
			ResolveMovementInputDecisionReason(true);
		const bool bMovementAllowed =
			MovementReason == EActionReactionTelemetryReason::MovementInputAllowed;
		const TCHAR* MovementReasonText = TEXT("Allowed");
		switch (MovementReason)
		{
		case EActionReactionTelemetryReason::MovementInputSuppressedByHold:
			MovementReasonText = TEXT("Suppressed: Hold");
			break;
		case EActionReactionTelemetryReason::MovementInputSuppressedByPaired:
			MovementReasonText = TEXT("Suppressed: Paired");
			break;
		case EActionReactionTelemetryReason::MovementInputSuppressedByTerminalState:
			MovementReasonText = TEXT("Suppressed: Terminal");
			break;
		default:
			break;
		}
		DrawStateLine(
			FString::Printf(TEXT("Movement: %s"), MovementReasonText),
			bMovementAllowed ? FColor::Green : FColor::Red);
	}
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

void UCombatComponent::ProcessInputPair(const FQueuedInputAction& PressEvent, const FQueuedInputAction& ReleaseEvent)
{
	// Calculate hold duration
	float HoldDuration = ReleaseEvent.Timestamp - PressEvent.Timestamp;

	if (CombatDebug::IsHoldDebugEnabled())
	{
		UE_LOG(LogCombat, Log, TEXT("[INPUT] Pair processed: %s held for %.2fs"),
			*UEnum::GetValueAsString(PressEvent.InputType),
			HoldDuration);
	}

	// Press/release pair processing logic
	// This could trigger special actions based on hold duration in future
}

EActionExecutionMode UCombatComponent::DetermineExecutionMode(const FQueuedInputAction& InputAction) const
{
	// ============================================================================
	// PHASE-BASED EXECUTION (Smart Queue Management)
	// ============================================================================
	//
	// Input during Windup/Active → Queued (buffered, execute at Active end = "snap" timing)
	// Input during Recovery → IMMEDIATE (interrupt recovery for responsive combos)
	// Input during None (idle) → IMMEDIATE (start new action)
	//
	// This creates the "snappy" feel for queued inputs and "responsive" feel for
	// Recovery inputs, matching the Ghost of Tsushima combat design.
	// ============================================================================

	// During Windup or Active → Queue for Active end (cannot attack now)
	// This gives "snappy" execution - input buffered during windup executes at Active end
	if (CurrentPhase == EAttackPhase::Windup || CurrentPhase == EAttackPhase::Active)
	{
		return EActionExecutionMode::Queued;
	}

	// During Recovery or None → Execute immediately (responsive combos)
	// Recovery input interrupts the recovery animation for fluid combo flow
	return EActionExecutionMode::Immediate;
}

UAttackData* UCombatComponent::GetAttackForInput(EInputType InputType)
{
	// Get default attacks as fallbacks through CombatSettings → DefaultWeaponData → AttackConfiguration
	UAttackData* DefaultLightAttack = GetDefaultLightAttack();
	UAttackData* DefaultHeavyAttack = GetDefaultHeavyAttack();

	// Determine if we should combo: Check combo window OR valid attack continuation
	// This fixes combo progression for both queued AND immediate execution
	bool bShouldCombo = bComboWindowActive;

	// CRITICAL FIX: If we have CurrentAttackData and we're mid-attack (any phase except None),
	// allow combo continuation even if combo window flag hasn't been set yet
	// This handles rapid double-taps that queue input during Windup phase (before combo window opens)
	if (!bShouldCombo && CurrentAttackData && CurrentPhase != EAttackPhase::None)
	{
		bShouldCombo = true;

		if (CombatDebug::IsQueueDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[COMBO] Allowing combo from phase %s (CurrentAttack=%s)"),
				*UEnum::GetValueAsString(CurrentPhase),
				*CurrentAttackData->GetName());
		}
	}

	// BUG-3 FIX: If combo window is active but attack reference is gone, can't continue combo
	// ROOT CAUSE: TIME/EVENT desync - CurrentAttackData cleared by SetPhase(None) at montage end,
	// but bComboWindowActive uses time-based checkpoint expiration (may still be true)
	// Without this fix, rapid taps from idle resolve to default attack instead of combo continuation
	if (bShouldCombo && !CurrentAttackData)
	{
		bShouldCombo = false;

		if (CombatDebug::IsQueueDebugEnabled())
		{
			UE_LOG(LogCombat, Warning, TEXT("[COMBO] BUG-3 FIX: Combo window active but CurrentAttackData is nullptr - falling back to default"));
		}
	}

	// Debug: Log combo resolution context
	if (CombatDebug::IsQueueDebugEnabled())
	{
		UE_LOG(LogCombat, Warning, TEXT("[COMBO DEBUG] GetAttackForInput: Phase=%s, CurrentAttack=%s, ComboWindow=%s, bShouldCombo=%s"),
			*UEnum::GetValueAsString(CurrentPhase),
			CurrentAttackData ? *CurrentAttackData->GetName() : TEXT("nullptr"),
			bComboWindowActive ? TEXT("ACTIVE") : TEXT("Inactive"),
			bShouldCombo ? TEXT("TRUE") : TEXT("FALSE"));
	}

	// ============================================================================
	// DIRECTIONAL INPUT RESOLUTION (Architectural Fix)
	// ============================================================================
	// Use DirectionalInputBuffer (discrete sampling at hold release) instead of
	// LastDirectionalInput (continuous sampling causing semantic conflation)
	//
	// Direction is ONLY available if:
	// 1. Player held attack button (hold window opened)
	// 2. Input context switched to DirectionalInput
	// 3. Player released button WITH direction (captured at release event)
	//
	// If no directional input buffered → AttackDirection = None → resolution
	// uses normal combo chains (Priority 3) instead of directional follow-ups (Priority 2)

	EAttackDirection AttackDirection = EAttackDirection::None;

	if (DirectionalInputBuffer.HasValidInput())
	{
		// Use buffered direction (sampled at release during hold window)
		AttackDirection = UCombatUtils::InputToAttackDirection(DirectionalInputBuffer.DirectionAtRelease);

		if (CombatDebug::IsHoldDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[DIRECTIONAL] Using buffered direction: %s (captured at %.2f) → AttackDirection=%s"),
				*UEnum::GetValueAsString(DirectionalInputBuffer.DirectionAtRelease),
				DirectionalInputBuffer.CaptureTime,
				*UEnum::GetValueAsString(AttackDirection));
		}
	}
	else if (CombatDebug::IsHoldDebugEnabled())
	{
		// No directional input buffered (movement context OR no hold release)
		UE_LOG(LogCombat, Verbose, TEXT("[DIRECTIONAL] No buffered direction (AttackDirection=None)"));
	}

	// ========================================================================
	// CONTEXT-AWARE RESOLUTION
	// ========================================================================

	// Clear visited set at start of resolution (cycle detection)
	VisitedAttacks.Empty();

	// Call context-aware resolution
	// Pass entire HoldState (not just IsHolding()) so resolution can check IsHoldCompleted()
	FAttackResolutionResult Result = UMontageUtilityLibrary::ResolveNextAttackContextual(
		CurrentAttackData,
		InputType,
		AttackDirection,
		HoldState,  // Pass entire HoldState, not just boolean
		bShouldCombo,
		DefaultLightAttack,
		DefaultHeavyAttack,
		ActiveContextTags,  // NEW: Pass runtime context tags
		VisitedAttacks  // NEW: Pass visited set for cycle detection
	);

	// Check for cycle detection error
	if (Result.bCycleDetected)
	{
		UE_LOG(LogCombat, Error, TEXT("[RESOLVE] Cycle detected! Aborting resolution for safety."));
		return nullptr;
	}

	// ARCHITECTURAL FIX: Clear directional input buffer after consumption
	// Buffer is cleared when:
	// 1. Directional follow-up executed (Path=DirectionalFollowUp)
	// 2. Resolution explicitly signals to clear (bShouldClearDirectionalInput)
	// This prevents reusing the same buffered direction for multiple attacks
	if (Result.bShouldClearDirectionalInput || Result.Path == EResolutionPath::DirectionalFollowUp)
	{
		DirectionalInputBuffer.Reset();

		if (CombatDebug::IsHoldDebugEnabled())
		{
			UE_LOG(LogCombat, Log, TEXT("[DIRECTIONAL] Buffer cleared (directional consumed)"));
		}

		// DEPRECATED: Keep LastDirectionalInput clearing for backward compatibility
		LastDirectionalInput = EInputDirection::None;
	}

	// Debug: Log resolution result with path metadata
	if (CombatDebug::IsQueueDebugEnabled() && Result.Attack)
	{
		const TCHAR* PathName = TEXT("Unknown");
		switch (Result.Path)
		{
			case EResolutionPath::Default: PathName = TEXT("Default"); break;
			case EResolutionPath::NormalCombo: PathName = TEXT("NormalCombo"); break;
			case EResolutionPath::DirectionalFollowUp: PathName = TEXT("DirectionalFollowUp"); break;
			case EResolutionPath::ParryCounter: PathName = TEXT("ParryCounter"); break;
			case EResolutionPath::LowHealthFinisher: PathName = TEXT("LowHealthFinisher"); break;
			case EResolutionPath::ContextSensitive: PathName = TEXT("ContextSensitive"); break;
		}

		UE_LOG(LogCombat, Log, TEXT("[RESOLVE] ✓ Resolved to: '%s' (Path=%s, ClearInput=%s)"),
			*Result.Attack->GetName(),
			PathName,
			Result.bShouldClearDirectionalInput ? TEXT("YES") : TEXT("NO"));
	}

	// DEPRECATED: Old consumption flag system (replaced by DirectionalInputBuffer.Reset())
	// Kept for backward compatibility, will be removed when LastDirectionalInput fully deprecated
	if (Result.Path == EResolutionPath::DirectionalFollowUp)
	{
		bDirectionalInputConsumed = true;
	}

	// DEPRECATED: bCurrentAttackIsDirectionalFollowUp flag no longer needed
	// Keeping for backward compatibility only
	bCurrentAttackIsDirectionalFollowUp =
		(Result.Path == EResolutionPath::DirectionalFollowUp);

	return Result.Attack;
}

int32 UCombatComponent::CalculatePriority(const FActionQueueEntry& Action) const
{
	// Base priority on action type
	// Light attacks: 1
	// Heavy attacks: 2
	// Dodges: 3
	// Blocks: 4

	switch (Action.InputAction.InputType)
	{
		case EInputType::LightAttack:
			return 1;

		case EInputType::HeavyAttack:
			return 2;

		case EInputType::Evade:
			return 3;

		case EInputType::Block:
			return 4;

		default:
			return 0;
	}
}

void UCombatComponent::SortQueueByTime()
{
	ActionQueue.Sort([](const FActionQueueEntry& A, const FActionQueueEntry& B)
	{
		return A.ScheduledTime < B.ScheduledTime;
	});
}

FTimerCheckpoint* UCombatComponent::FindCheckpoint(EActionWindowType WindowType)
{
	for (FTimerCheckpoint& Checkpoint : Checkpoints)
	{
		if (Checkpoint.WindowType == WindowType && Checkpoint.bActive)
		{
			return &Checkpoint;
		}
	}
	return nullptr;
}

void UCombatComponent::ClearExpiredCheckpoints(float CurrentTime)
{
	for (int32 i = Checkpoints.Num() - 1; i >= 0; --i)
	{
		FTimerCheckpoint& Checkpoint = Checkpoints[i];

		if (Checkpoint.bActive && CurrentTime > (Checkpoint.MontageTime + Checkpoint.Duration))
		{
			Checkpoint.bActive = false;

			// Update combo window state if this was combo checkpoint
			if (Checkpoint.WindowType == EActionWindowType::Combo)
			{
				bComboWindowActive = false;
			}

			if (CombatDebug::IsQueueDebugEnabled())
			{
				UE_LOG(LogCombat, Log, TEXT("[CHECKPOINTS] Expired: Type=%s at %.2f"),
					*UEnum::GetValueAsString(Checkpoint.WindowType),
					CurrentTime);
			}

			Checkpoints.RemoveAt(i);
		}
	}
}

bool UCombatComponent::CanAcceptNewInput(EInputType InputType) const
{
	// Input is ALWAYS buffered during Windup/Active (queued execution)
	// Commit window only blocks IMMEDIATE execution during None/Recovery, not queuing
	// Therefore, commit window check is NOT in CanAcceptNewInput - it's in QueueAction

	if (InputType == EInputType::LightAttack || InputType == EInputType::HeavyAttack)
	{
		return true;
	}

	// Non-attack routes retain duplicate protection until they receive an explicit policy.
	for (const FActionQueueEntry& Entry : ActionQueue)
	{
		if (Entry.IsPending() && Entry.InputAction.InputType == InputType)
		{
			if (CombatDebug::IsQueueDebugEnabled())
			{
				UE_LOG(LogCombat, Warning, TEXT("[INPUT] Input REJECTED - Already queued action of same type"));
			}
			return false;
		}
	}

	return true;
}

// ============================================================================
// DEBUG VISUALIZATION TEST HELPERS
// ============================================================================

#if WITH_AUTOMATION_TESTS

FDebugVisualizationData UCombatComponent::CalculateDebugVisualizationData(
	const FRotator& CameraRotation,
	const FRotator& CharacterRotation,
	const FVector2D& CameraRelativeInput,
	EInputDirection ResolvedDirection) const
{
	FDebugVisualizationData Data;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return Data;
	}

	const FVector CharacterLocation = Character->GetActorLocation();
	Data.ChestOffset = FVector(0.0f, 0.0f, 90.0f);
	Data.YawDelta = UDebugUtils::CalculateYawDelta(CameraRotation.Yaw, CharacterRotation.Yaw);
	Data.bShowHoldIndicator = HoldState.IsHolding();
	Data.HoldStateLabel = Data.bShowHoldIndicator ? TEXT("⬛ HOLD ACTIVE") : TEXT("");

	// Calculate transformation vectors
	const FVector CameraForward = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::X);
	const FVector CameraRight = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);
	FVector WorldInput = (CameraRight * CameraRelativeInput.X) + (CameraForward * CameraRelativeInput.Y);
	WorldInput.Z = 0.0f;
	WorldInput.Normalize();

	const FRotator InverseCharacterYaw(0.0f, -CharacterRotation.Yaw, 0.0f);
	const FVector CharacterRelativeVec = InverseCharacterYaw.RotateVector(WorldInput);
	const FVector CharacterForward = FRotationMatrix(CharacterRotation).GetScaledAxis(EAxis::X);
	const EAttackDirection AttackDir = UCombatUtils::InputToAttackDirection(ResolvedDirection);

	// Arrow 1: Camera
	{
		FDebugArrowInfo Arrow;
		Arrow.StartPosition = CharacterLocation + Data.ChestOffset;
		Arrow.Length = 180.0f;
		Arrow.EndPosition = Arrow.StartPosition + (CameraForward * Arrow.Length);
		Arrow.LabelPosition = Arrow.EndPosition + FVector(0, 0, 25);
		Arrow.Label = FString::Printf(TEXT("1.CAMERA\n%s"),
			*UDebugUtils::FormatRotationDebug(CameraRotation));
		Arrow.Color = FColor(0, 100, 255);
		Arrow.Thickness = 2.5f;
		Arrow.bIsDashed = false;
		Data.Arrows.Add(Arrow);
	}

	// Arrow 2: Input
	{
		FDebugArrowInfo Arrow;
		Arrow.StartPosition = CharacterLocation + Data.ChestOffset;
		Arrow.Length = 140.0f;
		Arrow.EndPosition = Arrow.StartPosition + (WorldInput * Arrow.Length);
		Arrow.LabelPosition = Arrow.EndPosition + FVector(0, 0, 25);
		Arrow.Label = Data.bShowHoldIndicator ? TEXT("2.INPUT (hold-release)") : TEXT("2.INPUT (continuous)");
		Arrow.Color = Data.bShowHoldIndicator ? FColor(255, 215, 0) : FColor(255, 255, 0);
		Arrow.Thickness = 2.5f;
		Arrow.bIsDashed = Data.bShowHoldIndicator;
		Data.Arrows.Add(Arrow);
	}

	// Arrow 3: Character Forward
	{
		FDebugArrowInfo Arrow;
		Arrow.StartPosition = CharacterLocation + Data.ChestOffset;
		Arrow.Length = 160.0f;
		Arrow.EndPosition = Arrow.StartPosition + (CharacterForward * Arrow.Length);
		Arrow.LabelPosition = Arrow.EndPosition + FVector(0, 0, 25);
		Arrow.Label = FString::Printf(TEXT("CHAR FORWARD\n%s"),
			*UDebugUtils::FormatRotationDebug(CharacterRotation));
		Arrow.Color = FColor(0, 255, 0);
		Arrow.Thickness = 3.0f;
		Arrow.bIsDashed = false;
		Data.Arrows.Add(Arrow);
	}

	// Arrow 4: Character-Relative
	{
		FDebugArrowInfo Arrow;
		Arrow.StartPosition = CharacterLocation + Data.ChestOffset;
		Arrow.Length = 120.0f;
		Arrow.EndPosition = Arrow.StartPosition + (CharacterRelativeVec * Arrow.Length);
		Arrow.LabelPosition = Arrow.EndPosition + FVector(0, 0, 25);
		Arrow.Label = TEXT("4.CHAR-REL");
		Arrow.Color = FColor(255, 165, 0);
		Arrow.Thickness = 2.5f;
		Arrow.bIsDashed = false;
		Data.Arrows.Add(Arrow);
	}

	// Arrow 5: Attack Direction
	{
		FDebugArrowInfo Arrow;
		Arrow.StartPosition = CharacterLocation + Data.ChestOffset;
		FVector FinalDirectionVec = CharacterRelativeVec;
		FinalDirectionVec.Normalize();
		Arrow.Length = 160.0f;
		Arrow.EndPosition = Arrow.StartPosition + (FinalDirectionVec * Arrow.Length);
		Arrow.LabelPosition = Arrow.EndPosition + FVector(0, 0, 30);
		Arrow.Label = FString::Printf(TEXT("5.ATTACK: %s"),
			*UDebugUtils::FormatAttackDirectionDebug(AttackDir));
		Arrow.Color = FColor(255, 0, 255);
		Arrow.Thickness = 4.0f;
		Arrow.bIsDashed = false;
		Data.Arrows.Add(Arrow);
	}

	// Calculate arc points
	const float ArcRadius = 100.0f;
	const float AbsYawDelta = FMath::Abs(Data.YawDelta);
	if (AbsYawDelta > 5.0f)
	{
		const int32 NumArcSegments = FMath::Max(3, FMath::CeilToInt(AbsYawDelta / 10.0f));
		const float StartAngle = CharacterRotation.Yaw * (PI / 180.0f);
		const float AngleStep = (Data.YawDelta * (PI / 180.0f)) / NumArcSegments;

		for (int32 i = 0; i <= NumArcSegments; ++i)
		{
			const float Angle = StartAngle + (AngleStep * i);
			const FVector Point = CharacterLocation + Data.ChestOffset +
				FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * ArcRadius;
			Data.ArcPoints.Add(Point);
		}
	}

	return Data;
}

FColor UCombatComponent::GetPhaseDebugColor() const
{
	switch (CurrentPhase)
	{
		case EAttackPhase::Windup:   return FColor::Orange;
		case EAttackPhase::Active:   return FColor::Red;
		case EAttackPhase::Recovery: return FColor::Yellow;
		default:                      return FColor::White;
	}
}

bool UCombatComponent::ShouldUseDashedArrowForInput() const
{
	return HoldState.IsHolding();
}

#endif // WITH_AUTOMATION_TESTS


// ============================================================================
// FORWARDING WRAPPERS (delegate to UPairedAnimationComponent)
// ============================================================================

bool UCombatComponent::IsInCounterWindow() const
{
	return CachedPairedAnimComp ? CachedPairedAnimComp->IsInCounterWindow() : false;
}

float UCombatComponent::GetCounterWindowProgress() const
{
	return CachedPairedAnimComp ? CachedPairedAnimComp->GetCounterWindowProgress() : 0.0f;
}

const FCounterContext& UCombatComponent::GetCounterWindowData() const
{
	static const FCounterContext DefaultContext;
	return CachedPairedAnimComp ? CachedPairedAnimComp->GetCounterWindowData() : DefaultContext;
}

void UCombatComponent::SetCounterWindowData(EAttackType InAttackType, ESwingDirection InSwingDirection,
											 UPairedAnimationData* InCounterData, float InWindowDuration)
{
	if (CachedPairedAnimComp)
	{
		CachedPairedAnimComp->SetCounterWindowData(InAttackType, InSwingDirection, InCounterData, InWindowDuration);
	}
}

bool UCombatComponent::IsInParryWindow() const
{
	return CachedPairedAnimComp ? CachedPairedAnimComp->IsInParryWindow() : false;
}

void UCombatComponent::SetParryWindowActive(bool bActive)
{
	if (CachedPairedAnimComp)
	{
		CachedPairedAnimComp->SetParryWindowActive(bActive);
	}
}

void UCombatComponent::ClearCounterWindowData()
{
	if (CachedPairedAnimComp)
	{
		CachedPairedAnimComp->ClearCounterWindowData();
	}
}

bool UCombatComponent::TryExecuteFinisher(UAttackData* AttackData)
{
	return CachedPairedAnimComp ? CachedPairedAnimComp->TryExecuteFinisher(AttackData) : false;
}

void UCombatComponent::AddPairedPartner(AActor* Partner)
{
	if (CachedPairedAnimComp)
	{
		CachedPairedAnimComp->AddPairedPartner(Partner);
	}
}

void UCombatComponent::RemovePairedPartner(AActor* Partner)
{
	if (CachedPairedAnimComp)
	{
		CachedPairedAnimComp->RemovePairedPartner(Partner);
	}
}

void UCombatComponent::ClearPairedPartners()
{
	if (CachedPairedAnimComp)
	{
		CachedPairedAnimComp->ClearPairedPartners();
	}
}

bool UCombatComponent::IsPairedPartner(AActor* Actor) const
{
	return CachedPairedAnimComp ? CachedPairedAnimComp->IsPairedPartner(Actor) : false;
}

int32 UCombatComponent::GetPairedPartnerCount() const
{
	return CachedPairedAnimComp ? CachedPairedAnimComp->GetPairedPartnerCount() : 0;
}

bool UCombatComponent::IsInputBlocked() const
{
	return CachedPairedAnimComp ? CachedPairedAnimComp->IsInputBlocked() : false;
}

void UCombatComponent::PrepareForPairedTakeover()
{
	// The paired animation owns the body from here, on both sides: the victim (EnterPairedAnimationState) and the
	// character who starts it (BeginPairedAnimation). The owner's own knockback push would otherwise carry on into
	// the paired montage, or resume after an entry bridge that only suspended it.
	if (ABaseCombatCharacter* Character = GetOwnerCharacter())
	{
		if (Character->HitReactionComponent)
		{
			Character->HitReactionComponent->ReleaseKnockback(TEXT("PairedTakeover"));
		}
	}
	EndBlock();
	SetPhase(EAttackPhase::None);
	ClearQueue(false);
	HeldInputs.Reset();
	HeldInputSerials.Reset();
	DirectionalInputBuffer.Reset();
	SetInputContext(EInputContext::Movement);
	AttackStateMachine.ForceReset();
}

void UCombatComponent::BeginPairedAnimation(UPairedAnimationData* PairedAnimData, EPairedReactionType ReactionType, bool bIsCriticalMoment)
{
	if (CachedPairedAnimComp)
	{
		CachedPairedAnimComp->BeginPairedAnimation(PairedAnimData, ReactionType, bIsCriticalMoment);
	}
}

void UCombatComponent::EndPairedAnimation()
{
	if (CachedPairedAnimComp)
	{
		CachedPairedAnimComp->EndPairedAnimation();
	}
}

void UCombatComponent::TriggerSyncPointEffects(FName SyncPointName)
{
	if (CachedPairedAnimComp)
	{
		CachedPairedAnimComp->TriggerSyncPointEffects(SyncPointName);
	}
}

bool UCombatComponent::IsPairedAnimationActive() const
{
	return CachedPairedAnimComp ? CachedPairedAnimComp->IsPairedAnimationActive() : false;
}

void UCombatComponent::OnPairedPartnerDeath(AActor* DeadPartner)
{
	if (CachedPairedAnimComp)
	{
		CachedPairedAnimComp->OnPairedPartnerDeath(DeadPartner);
	}
}

void UCombatComponent::CancelPairedAnimation(float BlendOutTime)
{
	if (CachedPairedAnimComp)
	{
		CachedPairedAnimComp->CancelPairedAnimation(BlendOutTime);
	}
}

void UCombatComponent::CompletePairedAnimation()
{
	if (CachedPairedAnimComp)
	{
		CachedPairedAnimComp->CompletePairedAnimation();
	}
}
