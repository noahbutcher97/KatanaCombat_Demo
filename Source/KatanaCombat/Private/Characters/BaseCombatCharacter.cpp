// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/BaseCombatCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Core/WeaponComponent.h"
#include "Utilities/WeaponTraceLibrary.h"
#include "Core/HitReactionComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Defense/DefenseResolver.h"
#include "Defense/DefensePresentationSelector.h"
#include "Data/AttackData.h"
#include "Data/DefenseConfiguration.h"
#include "Data/WeaponData.h"
#include "Data/CombatSettings.h"
#include "Data/CombatFXData.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "MotionWarpingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
bool DefenseOutcomeAcceptsWeaponHit(const EDefenseOutcome Outcome)
{
	return Outcome == EDefenseOutcome::NormalBlock
		|| Outcome == EDefenseOutcome::Hit
		|| Outcome == EDefenseOutcome::UnblockableHit;
}

bool IsUsableDefenseVector(const FVector& Vector)
{
	return FMath::IsFinite(Vector.X)
		&& FMath::IsFinite(Vector.Y)
		&& FMath::IsFinite(Vector.Z);
}

bool DoesContactIdentityBelongToSource(
	const FContactInstanceId& ContactId,
	const ABaseCombatCharacter* Source)
{
	if (!Source || !ContactId.IsValid())
	{
		return false;
	}
	if (ContactId.bUsesAttackWindow)
	{
		return ContactId.AttackWindow.AttackInstance.Attacker.Get() == Source;
	}

	const UWeaponComponent* ContactWeapon = Cast<UWeaponComponent>(
		ContactId.CompatibilityTrace.WeaponComponent.Get());
	return ContactWeapon
		&& Source->WeaponComponent.Get() == ContactWeapon
		&& ContactWeapon->GetOwner() == Source;
}

FDefensePresentationSelectionContext BuildPresentationContext(const FDefenseResolution& Resolution)
{
	FDefensePresentationSelectionContext Context;
	Context.Outcome = Resolution.Decision.Outcome;
	Context.AttackerResponse = Resolution.Decision.AttackerResponse;
	Context.Height = Resolution.Decision.Height;
	Context.Lane = Resolution.Decision.Lane;
	Context.SwingShape = Resolution.Decision.SwingShape;
	if (Resolution.Decision.SelectedAttack)
	{
		Context.AttackTags = Resolution.Decision.SelectedAttack->AttackTags;
	}
	return Context;
}

void CommitNormalBlockPresentation(
	FDefenseResolution& Resolution,
	const UDefenseConfiguration* DefenderConfiguration,
	const UDefenseConfiguration* AttackerConfiguration)
{
	const FTableDefensePresentationSelector Selector;
	const FDefensePresentationSelectionContext Context = BuildPresentationContext(Resolution);
	const FDefensePresentationSelectionResult DefenderSelection =
		Selector.SelectDefender(Context, DefenderConfiguration);
	const FDefensePresentationSelectionResult GenericDefenderSelection =
		Selector.SelectGenericDefender(Context, DefenderConfiguration);
	if (DefenderSelection.bFound)
	{
		Resolution.Presentation = DefenderSelection.Payload;
		Resolution.PresentationRow = DefenderSelection.RowName;
		Resolution.PresentationFallback = DefenderSelection.FallbackLevel;
	}

	const UAttackData* AttackData = Resolution.Decision.SelectedAttack;
	const bool bExactRowAudio = DefenderSelection.bFound
		&& DefenderSelection.FallbackLevel == EDefensePresentationFallbackLevel::Exact
		&& DefenderSelection.Payload.bOverrideImpactAudio
		&& DefenderSelection.Payload.ImpactAudio.ImpactSound;
	const bool bAttackAudio = AttackData
		&& AttackData->DefenseProfile.bOverrideBlockedImpactAudio
		&& AttackData->DefenseProfile.BlockedImpactAudio.ImpactSound;
	const bool bGenericRowAudio = GenericDefenderSelection.bFound
		&& GenericDefenderSelection.Payload.bOverrideImpactAudio
		&& GenericDefenderSelection.Payload.ImpactAudio.ImpactSound;
	Resolution.Presentation.bOverrideImpactAudio = true;
	if (bExactRowAudio)
	{
		Resolution.Presentation.ImpactAudio = DefenderSelection.Payload.ImpactAudio;
	}
	else if (bAttackAudio)
	{
		Resolution.Presentation.ImpactAudio = AttackData->DefenseProfile.BlockedImpactAudio;
	}
	else if (bGenericRowAudio)
	{
		Resolution.Presentation.ImpactAudio = GenericDefenderSelection.Payload.ImpactAudio;
	}
	else
	{
		Resolution.Presentation.ImpactAudio = DefenderConfiguration
			? DefenderConfiguration->DefaultBlockImpactAudio
			: FImpactAudioConfig();
	}

	const bool bExactRowVFX = DefenderSelection.bFound
		&& DefenderSelection.FallbackLevel == EDefensePresentationFallbackLevel::Exact
		&& DefenderSelection.Payload.bOverrideImpactVFX
		&& DefenderSelection.Payload.ImpactVFX.ImpactVFX;
	const bool bAttackVFX = AttackData
		&& AttackData->DefenseProfile.bOverrideBlockedImpactVFX
		&& AttackData->DefenseProfile.BlockedImpactVFX.ImpactVFX;
	const bool bGenericRowVFX = GenericDefenderSelection.bFound
		&& GenericDefenderSelection.Payload.bOverrideImpactVFX
		&& GenericDefenderSelection.Payload.ImpactVFX.ImpactVFX;
	Resolution.Presentation.bOverrideImpactVFX = true;
	if (bExactRowVFX)
	{
		Resolution.Presentation.ImpactVFX = DefenderSelection.Payload.ImpactVFX;
	}
	else if (bAttackVFX)
	{
		Resolution.Presentation.ImpactVFX = AttackData->DefenseProfile.BlockedImpactVFX;
	}
	else if (bGenericRowVFX)
	{
		Resolution.Presentation.ImpactVFX = GenericDefenderSelection.Payload.ImpactVFX;
	}
	else
	{
		Resolution.Presentation.ImpactVFX = DefenderConfiguration
			? DefenderConfiguration->DefaultBlockImpactVFX
			: FImpactVFXConfig();
	}

	if (!Resolution.Presentation.bOverrideHitstop
		&& AttackData
		&& AttackData->HitstopConfig.IsActive())
	{
		Resolution.Presentation.bOverrideHitstop = true;
		Resolution.Presentation.Hitstop = AttackData->HitstopConfig;
	}

	FDefensePresentationSelectionResult AttackerSelection =
		Selector.SelectAttacker(Context, AttackerConfiguration);
	const bool bAttackerMontageUsable = AttackerSelection.Payload.Montage
		&& (AttackerSelection.Payload.MontageSection.IsNone()
			|| AttackerSelection.Payload.Montage->GetSectionIndex(
				AttackerSelection.Payload.MontageSection) != INDEX_NONE);
	if (!bAttackerMontageUsable)
	{
		const FDefensePresentationSelectionResult GenericAttackerSelection =
			Selector.SelectGenericAttacker(Context, AttackerConfiguration);
		const bool bGenericMontageUsable = GenericAttackerSelection.Payload.Montage
			&& (GenericAttackerSelection.Payload.MontageSection.IsNone()
				|| GenericAttackerSelection.Payload.Montage->GetSectionIndex(
					GenericAttackerSelection.Payload.MontageSection) != INDEX_NONE);
		if (bGenericMontageUsable)
		{
			AttackerSelection = GenericAttackerSelection;
		}
	}
	if (AttackerSelection.bFound)
	{
		Resolution.AttackerPresentation = AttackerSelection.Payload;
		Resolution.AttackerPresentationRow = AttackerSelection.RowName;
		Resolution.AttackerPresentationFallback = AttackerSelection.FallbackLevel;
	}

	if (DefenderSelection.bAmbiguous
		|| GenericDefenderSelection.bAmbiguous
		|| AttackerSelection.bAmbiguous)
	{
		UE_LOG(LogCombat, Warning,
			TEXT("Ambiguous defense presentation resolved deterministically for interaction epoch %llu"),
			Resolution.InteractionId.Epoch);
	}
}
}

ABaseCombatCharacter::ABaseCombatCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create combat components
    CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
    TargetingComponent = CreateDefaultSubobject<UTargetingComponent>(TEXT("TargetingComponent"));
    WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
    HitReactionComponent = CreateDefaultSubobject<UHitReactionComponent>(TEXT("HitReactionComponent"));
    PairedAnimationComponent = CreateDefaultSubobject<UPairedAnimationComponent>(TEXT("PairedAnimationComponent"));
    MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

    // Initialize health
    CurrentHealth = MaxHealth;
}

void ABaseCombatCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Initialize health to max
    CurrentHealth = MaxHealth;

    // Bind to weapon hit event for damage processing
    if (WeaponComponent)
    {
        WeaponComponent->OnWeaponHit.AddDynamic(this, &ABaseCombatCharacter::OnWeaponHitTarget);
    }
}

void ABaseCombatCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TPair<FDefenseInteractionId, FTSTicker::FDelegateHandle>& Pair : PendingDefenseFallbackTickers)
	{
		FTSTicker::RemoveTicker(Pair.Value);
	}
	PendingDefenseFallbackTickers.Reset();
	PendingDefenseGameplayCommits.Reset();
	ActiveDefenseDeathDispatchInteraction.Reset();

	if (WeaponComponent)
	{
		WeaponComponent->OnWeaponHit.RemoveDynamic(this, &ABaseCombatCharacter::OnWeaponHitTarget);
	}
	Super::EndPlay(EndPlayReason);
}

// ============================================================================
// HEALTH UTILITIES
// ============================================================================

float ABaseCombatCharacter::ModifyHealth(float Delta, AActor* DamageInstigator)
{
	const FSilentHealthCommit Commit = CommitHealthDeltaSilently(Delta, DamageInstigator);
	DispatchCommittedHealth(Commit);
	return Commit.ActualDelta;
}

void ABaseCombatCharacter::SetHealth(float NewHealth, AActor* DamageInstigator)
{
    const float Delta = NewHealth - CurrentHealth;
    ModifyHealth(Delta, DamageInstigator);
}

FSilentHealthCommit ABaseCombatCharacter::CommitHealthDeltaSilently(
	const float Delta,
	AActor* DamageInstigator)
{
	FSilentHealthCommit Commit;
	Commit.OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth + Delta, 0.0f, MaxHealth);
	Commit.NewHealth = CurrentHealth;
	Commit.ActualDelta = Commit.NewHealth - Commit.OldHealth;
	Commit.bHealthChanged = !FMath::IsNearlyZero(Commit.ActualDelta);
	Commit.DamageInstigator = DamageInstigator;

	if (Commit.bHealthChanged
		&& Commit.NewHealth <= 0.0f
		&& Commit.OldHealth > 0.0f
		&& !bIsDead
		&& !bIsDying)
	{
		bIsDying = true;
		bCommittedDyingDispatchPending = true;
		Commit.bNewlyDying = true;
	}
	return Commit;
}

bool ABaseCombatCharacter::IsDefenseDispatchValid(
	const FDefenseInteractionId* InteractionId) const
{
	if (!IsValid(this))
	{
		return false;
	}
	return !InteractionId
		|| (CombatComponent && CombatComponent->IsDefenseInteractionFinalized(*InteractionId));
}

void ABaseCombatCharacter::DispatchCommittedHealth(
	const FSilentHealthCommit& Commit,
	const FDefenseInteractionId* InteractionId)
{
	if (!Commit.bHealthChanged || !IsDefenseDispatchValid(InteractionId))
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[HEALTH] %s: %.1f -> %.1f (delta: %.1f, max: %.1f)"),
		*GetName(), Commit.OldHealth, Commit.NewHealth, Commit.ActualDelta, MaxHealth);
	OnHealthChanged.Broadcast(Commit.NewHealth, MaxHealth);
	if (!IsDefenseDispatchValid(InteractionId))
	{
		return;
	}

	if (Commit.bNewlyDying)
	{
		AActor* Killer = Commit.DamageInstigator.Get();
		UE_LOG(LogTemp, Warning, TEXT("[HEALTH] %s DIED! Killed by %s"),
			*GetName(), Killer ? *Killer->GetName() : TEXT("Unknown"));
		if (InteractionId)
		{
			const TOptional<FDefenseInteractionId> PreviousInteraction = ActiveDefenseDeathDispatchInteraction;
			ActiveDefenseDeathDispatchInteraction = *InteractionId;
			HandleDeath(Killer);
			if (IsValid(this))
			{
				ActiveDefenseDeathDispatchInteraction = PreviousInteraction;
			}
		}
		else
		{
			HandleDeath(Killer);
		}
	}
}

void ABaseCombatCharacter::HandleDeath_Implementation(AActor* Killer)
{
    // ========================================================================
    // GUARD: Already dead or dying - don't process death twice
    // ========================================================================
    // This can happen when:
    // 1. Finisher victim montage ended → OnAnyMontageBlendingOut applied death outcome
    // 2. FinalizeDeath() was called → bIsDead = true
    // 3. CompletePairedAnimation() applies damage → HandleDeath called again
    // In this case, death was already processed, so skip.
    if (bIsDead)
    {
        UE_LOG(LogTemp, Log, TEXT("[DEATH] %s HandleDeath called but already DEAD - skipping"),
            *GetName());
        return;
    }

	if (bIsDying && !bCommittedDyingDispatchPending)
    {
        UE_LOG(LogTemp, Log, TEXT("[DEATH] %s HandleDeath called but already DYING - skipping"),
            *GetName());
        return;
    }

	if (!bIsDying)
	{
		bIsDying = true;
	}
	bCommittedDyingDispatchPending = false;
	const FDefenseInteractionId* InteractionId = ActiveDefenseDeathDispatchInteraction.IsSet()
		? &ActiveDefenseDeathDispatchInteraction.GetValue()
		: nullptr;
	DispatchCommittedDying(Killer, InteractionId);
}

void ABaseCombatCharacter::DispatchCommittedDying(
	AActor* Killer,
	const FDefenseInteractionId* InteractionId)
{
	if (!IsDefenseDispatchValid(InteractionId))
	{
		return;
	}

    UE_LOG(LogTemp, Log, TEXT("[DEATH] %s entering DYING state (killed by %s)"),
        *GetName(),
        Killer ? *Killer->GetName() : TEXT("Unknown"));

    // Broadcast dying event - systems can react to "dying" state
    OnCharacterDying.Broadcast(Killer);
	if (!IsDefenseDispatchValid(InteractionId))
	{
		return;
	}

    // Calculate death direction from killer
    EAttackDirection DeathDirection = EAttackDirection::Forward;
    if (Killer && HitReactionComponent)
    {
        // Direction FROM killer TO victim (used to determine which way victim was facing killer)
        FVector ToKiller = (Killer->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        DeathDirection = HitReactionComponent->GetHitDirectionRelativeToFacing(ToKiller);
    }

    // Play death reaction through HitReactionComponent
    // This sets up pending death outcome - the outcome will be applied when:
    // 1. Death animation ends (OnAnyMontageBlendingOut), OR
    // 2. AnimNotify_ActivateRagdoll fires (early ragdoll)
    // At that point, HitReactionComponent calls FinalizeDeath() to transition to DEAD state
    if (HitReactionComponent)
    {
        HitReactionComponent->PlayDeathReaction(DeathDirection);
    }
	if (!IsDefenseDispatchValid(InteractionId))
	{
		return;
	}

    // Disable combat component tick (combat is blocked during dying)
    if (CombatComponent)
    {
        CombatComponent->SetComponentTickEnabled(false);
    }
    if (TargetingComponent)
    {
        TargetingComponent->ReleaseAllAlignmentRequests(EAlignmentReleaseReason::Death);
    }
}

void ABaseCombatCharacter::FinalizeDeath()
{
    // ========================================================================
    // TWO-STAGE DEATH: Transition from DYING to DEAD
    // ========================================================================
    // Called by HitReactionComponent when death animation completes
    // (either via OnAnyMontageBlendingOut or AnimNotify_ActivateRagdoll)
    // ========================================================================

    if (bIsDead)
    {
        // Already dead - prevent double finalization
        UE_LOG(LogTemp, Warning, TEXT("[DEATH] %s FinalizeDeath called but already DEAD"),
            *GetName());
        return;
    }

    if (!bIsDying)
    {
        // Not dying - something is wrong
        UE_LOG(LogTemp, Warning, TEXT("[DEATH] %s FinalizeDeath called but not DYING"),
            *GetName());
    }

    // Transition to DEAD state
    bIsDead = true;

    // Disable capsule collision so other characters can walk over the corpse.
    // The mesh collision is handled separately by the death outcome:
    // - Ragdoll: mesh gets PhysicsOnly collision (ActivateRagdoll)
    // - Freeze: mesh keeps its collision but capsule is the blocking culprit
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    UE_LOG(LogTemp, Log, TEXT("[DEATH] %s entering DEAD state (death finalized, capsule collision disabled)"),
        *GetName());

    // Broadcast death event - character is now truly dead
    OnCharacterDeath.Broadcast(nullptr);  // Killer not tracked to finalize
}

// ============================================================================
// ITeamMemberInterface IMPLEMENTATION
// ============================================================================

ETeamId ABaseCombatCharacter::GetTeamId_Implementation() const
{
    return TeamId;
}

bool ABaseCombatCharacter::IsHostileTo_Implementation(AActor* Other) const
{
    if (!Other)
    {
        return false;
    }

    // If other doesn't implement team interface, assume hostile
    if (!Other->Implements<UTeamMemberInterface>())
    {
        return true;
    }

    const ETeamId OtherTeam = ITeamMemberInterface::Execute_GetTeamId(Other);

    // Neutrals are hostile to everyone except neutrals
    if (TeamId == ETeamId::Neutral)
    {
        return OtherTeam != ETeamId::Neutral;
    }

    // Player is hostile to enemies, enemies are hostile to player
    if (TeamId == ETeamId::Player)
    {
        return OtherTeam == ETeamId::Enemy;
    }

    if (TeamId == ETeamId::Enemy)
    {
        return OtherTeam == ETeamId::Player || OtherTeam == ETeamId::Ally;
    }

    // Allies are hostile to enemies
    if (TeamId == ETeamId::Ally)
    {
        return OtherTeam == ETeamId::Enemy;
    }

    return false;
}

bool ABaseCombatCharacter::IsFriendlyTo_Implementation(AActor* Other) const
{
    if (!Other)
    {
        return false;
    }

    // If other doesn't implement team interface, not friendly
    if (!Other->Implements<UTeamMemberInterface>())
    {
        return false;
    }

    const ETeamId OtherTeam = ITeamMemberInterface::Execute_GetTeamId(Other);

    // Same team is always friendly
    if (TeamId == OtherTeam)
    {
        return true;
    }

    // Player and ally are friendly to each other
    if ((TeamId == ETeamId::Player && OtherTeam == ETeamId::Ally) ||
        (TeamId == ETeamId::Ally && OtherTeam == ETeamId::Player))
    {
        return true;
    }

    return false;
}

// ============================================================================
// NATIVE RICH CONTACT TRANSPORT
// ============================================================================

FActualDefenseContact ABaseCombatCharacter::BuildActualDefenseContact(
	const FDefenseContactRequest& Request) const
{
	FActualDefenseContact Contact;
	Contact.HitInfo = Request.HitInfo;
	ABaseCombatCharacter* Source = Cast<ABaseCombatCharacter>(Request.HitInfo.Attacker);
	Contact.bIsValid = Request.ContactId.IsValid()
		&& IsValid(Source)
		&& Source != this
		&& DoesContactIdentityBelongToSource(Request.ContactId, Source)
		&& IsUsableDefenseVector(Request.HitInfo.ImpactPoint)
		&& IsUsableDefenseVector(Request.TraceStart)
		&& IsUsableDefenseVector(Request.TraceEnd);

	Contact.SourceBearing = Source
		? (Source->GetActorLocation() - GetActorLocation()).GetSafeNormal()
		: FVector::ZeroVector;
	Contact.TraceStart = Request.TraceStart;
	Contact.TraceEnd = Request.TraceEnd;
	Contact.IncomingTrajectory = Request.HitInfo.WeaponVelocity;
	if (!IsUsableDefenseVector(Contact.IncomingTrajectory)
		|| Contact.IncomingTrajectory.IsNearlyZero())
	{
		Contact.IncomingTrajectory = Request.TraceEnd - Request.TraceStart;
	}

	Contact.SourceSocket = Request.ActiveSourceSocket;
	if (Contact.SourceSocket.IsNone())
	{
		Contact.SourceSocket = Request.Query.Attack.PredictedContact.SourceSocket;
	}
	if (Contact.SourceSocket.IsNone() && Request.HitInfo.AttackData)
	{
		Contact.SourceSocket = Request.HitInfo.AttackData->DefenseProfile.SourceContactSocketOverride;
	}
	const UDefenseConfiguration* Configuration = CombatComponent
		? CombatComponent->GetEffectiveDefenseConfiguration()
		: nullptr;

	const EIncomingAttackLane AuthoredLane = Request.HitInfo.AttackData
		? Request.HitInfo.AttackData->DefenseProfile.NominalLane
		: Request.Query.Attack.NominalLane;
	const FDefenseLaneResolution Lane = FDefenseResolver::ResolveIncomingLane(
		Request.HitInfo.WeaponVelocity,
		Request.TraceStart,
		Request.TraceEnd,
		AuthoredLane,
		GetActorTransform(),
		Configuration ? Configuration->CenterLaneHalfAngle : 12.0f);
	Contact.Lane = Lane.Lane;
	Contact.LaneProvenance = Lane.Provenance;
	Contact.IncomingTrajectory = Lane.IncomingTrajectory;

	const EAttackHeight AuthoredHeight = Request.HitInfo.AttackData
		? Request.HitInfo.AttackData->DefenseProfile.Height
		: Request.Query.Attack.AuthoredHeight;
	Contact.ResolvedTargetBone = Request.HitInfo.BoneName;
	if (Contact.ResolvedTargetBone.IsNone()
		&& Request.Query.Attack.PredictedContact.bIsValid)
	{
		Contact.ResolvedTargetBone = Request.Query.Attack.PredictedContact.DefenderTargetBone;
	}
	if (Contact.ResolvedTargetBone.IsNone())
	{
		Contact.ResolvedTargetBone = Request.Query.Attack.DefenderTargetBone;
	}
	if (Contact.ResolvedTargetBone.IsNone() && Request.HitInfo.AttackData)
	{
		Contact.ResolvedTargetBone = Request.HitInfo.AttackData->GetDefenseTargetBoneFallback();
	}

	TArray<FName> ParentBoneChain;
	if (!Request.HitInfo.BoneName.IsNone())
	{
		if (const USkeletalMeshComponent* CharacterMesh = GetMesh())
		{
			TSet<FName> VisitedBones;
			FName ParentBone = CharacterMesh->GetParentBone(Request.HitInfo.BoneName);
			while (!ParentBone.IsNone() && !VisitedBones.Contains(ParentBone))
			{
				ParentBoneChain.Add(ParentBone);
				VisitedBones.Add(ParentBone);
				ParentBone = CharacterMesh->GetParentBone(ParentBone);
			}
		}
	}

	const FDefenseHeightResolution HeightResolution = Configuration
		? Configuration->ResolveHeight(
			Request.HitInfo.BoneName,
			ParentBoneChain,
			AuthoredHeight)
		: FDefenseHeightResolution(
			AuthoredHeight,
			EDefenseHeightProvenance::Authored,
			NAME_None);
	Contact.Height = HeightResolution.Height;
	Contact.HeightProvenance = HeightResolution.Provenance;
	Contact.HeightSourceBone = HeightResolution.MatchedBone;
	return Contact;
}

void ABaseCombatCharacter::PopulateDefenseContactQuery(
	FDefenseQuery& Query,
	const FDefenseContactRequest& Request,
	const FActualDefenseContact& ActualContact) const
{
	Query.Stage = EDefenseQueryStage::Contact;
	Query.Defender = const_cast<ABaseCombatCharacter*>(this);
	Query.DefenderTransform = GetActorTransform();
	Query.DefenderTeam = TeamId;
	Query.ActualContact = ActualContact;
	Query.bHasActualContact = ActualContact.bIsValid;
	Query.bContactIdentityValid = Request.ContactId.IsValid();
	Query.bDefenderAlive = !IsDeadOrDying();
	Query.bDefenderPaired = PairedAnimationComponent
		&& PairedAnimationComponent->IsPairedAnimationActive();
	Query.bDefenderCanGuard = Query.bDefenderAlive && !Query.bDefenderPaired;
	Query.bDefenderGuarding = CombatComponent && CombatComponent->IsGuardHeldForDefense();
	Query.bDefenderCanBeDamaged = HitReactionComponent && HitReactionComponent->CanBeDamaged();
	Query.bDefenderInIFrames = HitReactionComponent && HitReactionComponent->IsInIFrames();
	Query.bFriendlyFireEnabled = false;
	Query.RelativeYawDegrees = FDefenseResolver::CalculateDefenderRelativeYaw(
		Query.DefenderTransform,
		ActualContact.SourceBearing);
	Query.CurrentSimulationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const UDefenseConfiguration* Configuration = CombatComponent
		? CombatComponent->GetEffectiveDefenseConfiguration()
		: GetDefault<UDefenseConfiguration>();
	if (Configuration)
	{
		Query.HardGuardConeHalfAngle = Configuration->HardGuardConeHalfAngle;
		Query.MaximumAutomaticTurn = Configuration->MaximumAutomaticTurn;
		Query.RemainingAutomaticTurn = Configuration->MaximumAutomaticTurn;
		Query.DefenseTurnRate = Configuration->DefenseTurnRate;
		Query.NormalBlockFinalTolerance = Configuration->NormalBlockFinalTolerance;
		Query.PerfectParryFinalTolerance = Configuration->PerfectParryFinalTolerance;
		Query.MaximumHighConfidencePredictionAge = Configuration->MaximumHighConfidencePredictionAge;
	}

	AActor* Source = Request.HitInfo.Attacker;
	UAttackData* AttackData = Request.HitInfo.AttackData;
	Query.Attack.AttackData = AttackData;
	Query.Attack.AttackType = AttackData ? AttackData->AttackType : EAttackType::None;
	Query.Attack.AttackTags = AttackData ? AttackData->AttackTags : FGameplayTagContainer();
	Query.Attack.AuthoredHeight = AttackData
		? AttackData->DefenseProfile.Height
		: Query.Attack.AuthoredHeight;
	Query.Attack.NominalLane = AttackData
		? AttackData->DefenseProfile.NominalLane
		: Query.Attack.NominalLane;
	Query.Attack.SwingShape = AttackData
		? AttackData->DefenseProfile.SwingShape
		: Query.Attack.SwingShape;
	Query.Attack.SourceSocket = ActualContact.SourceSocket;
	Query.Attack.DefenderTargetBone = ActualContact.ResolvedTargetBone;
	Query.Attack.AttackerTransform = Source ? Source->GetActorTransform() : FTransform::Identity;
	Query.Attack.AttackerVelocity = Source ? Source->GetVelocity() : FVector::ZeroVector;
	Query.Attack.bAttackerAlive = IsValid(Source) && Source != this;

	const ABaseCombatCharacter* SourceCharacter = Cast<ABaseCombatCharacter>(Source);
	if (SourceCharacter)
	{
		Query.Attack.AttackerTeam = SourceCharacter->TeamId;
		Query.Attack.bAttackerAlive = !SourceCharacter->IsDeadOrDying();
		Query.Attack.bAttackerPaired = SourceCharacter->PairedAnimationComponent
			&& SourceCharacter->PairedAnimationComponent->IsPairedAnimationActive();
	}
	else if (Source && Source->Implements<UTeamMemberInterface>())
	{
		Query.Attack.AttackerTeam = ITeamMemberInterface::Execute_GetTeamId(Source);
	}

	if (Source && Source->Implements<UTeamMemberInterface>() && Implements<UTeamMemberInterface>())
	{
		const bool bBothNonNeutral = Query.Attack.AttackerTeam != ETeamId::Neutral
			&& Query.DefenderTeam != ETeamId::Neutral;
		const bool bExplicitlyHostile = ITeamMemberInterface::Execute_IsHostileTo(
			Source, const_cast<ABaseCombatCharacter*>(this))
			|| ITeamMemberInterface::Execute_IsHostileTo(
				const_cast<ABaseCombatCharacter*>(this), Source);
		const bool bDefaultFriendly = Query.Attack.AttackerTeam == Query.DefenderTeam
			|| (Query.Attack.AttackerTeam == ETeamId::Player && Query.DefenderTeam == ETeamId::Ally)
			|| (Query.Attack.AttackerTeam == ETeamId::Ally && Query.DefenderTeam == ETeamId::Player);
		Query.Attack.bIsHostileToDefender = bExplicitlyHostile;
		Query.Attack.bIsFriendlyToDefender = bBothNonNeutral
			&& bDefaultFriendly
			&& !bExplicitlyHostile;
	}

	if (Request.ContactId.bUsesAttackWindow)
	{
		Query.Attack.AttackInstance = Request.ContactId.AttackWindow.AttackInstance;
	}
	else
	{
		const UWeaponComponent* ContactWeapon = Cast<UWeaponComponent>(
			Request.ContactId.CompatibilityTrace.WeaponComponent.Get());
		const bool bCompatibilityIdentityCurrent = DoesContactIdentityBelongToSource(
			Request.ContactId,
			SourceCharacter)
			&& ContactWeapon
			&& ContactWeapon->IsContactInstanceCurrent(Request.ContactId);
		Query.Attack.bAttackConsumed = Query.Attack.bAttackConsumed
			|| !bCompatibilityIdentityCurrent;
	}

	if (Query.Attack.AttackInstance.IsValid())
	{
		const UCombatComponent* SourceCombat = SourceCharacter
			? SourceCharacter->CombatComponent.Get()
			: nullptr;
		Query.Attack.bAttackIdentityCurrent = SourceCombat
			&& Query.Attack.AttackInstance.Attacker.Get() == Source
			&& SourceCombat->GetCurrentAttackGeneration() == Query.Attack.AttackInstance.AttackGeneration;
		Query.Attack.bAttackActive = Query.Attack.bAttackIdentityCurrent
			&& SourceCombat->GetCurrentAttack() == AttackData;
	}
	else
	{
		Query.Attack.bAttackIdentityCurrent = false;
		Query.Attack.bAttackActive = Query.Attack.bAttackerAlive;
	}
}

FDefenseGameplayCommitResult ABaseCombatCharacter::CommitResolvedDefenseDamage(
	const FDefenseResolution& Resolution,
	const float ResistanceSnapshot)
{
	FDefenseGameplayCommitResult Result;
	Result.HitInfo = Resolution.ActualContact.HitInfo;
	if (Resolution.Decision.DamageDisposition != EDefenseDamageDisposition::ApplyRequestedDamage
		|| !HitReactionComponent)
	{
		return Result;
	}

	const FCommittedHitReactionDamage DamageCommit = HitReactionComponent->CommitResolvedDamage(
		Result.HitInfo,
		ResistanceSnapshot);
	Result.ResolvedDamage = FMath::IsFinite(DamageCommit.ResolvedDamage)
		? FMath::Max(0.0f, DamageCommit.ResolvedDamage)
		: 0.0f;
	Result.bDispatchDamage = DamageCommit.bShouldNotify;
	Result.bPlayHitReaction = DamageCommit.bShouldPlayReaction;
	Result.Health = CommitHealthDeltaSilently(-Result.ResolvedDamage, Result.HitInfo.Attacker);
	return Result;
}

FDefenseContactReceipt ABaseCombatCharacter::ResolveAndCommitCombatContact(
	const FDefenseContactRequest& Request)
{
	FDefenseContactReceipt Receipt;
	if (!CombatComponent)
	{
		return Receipt;
	}

	FDefenseInteractionKey Key;
	Key.Stage = EDefenseQueryStage::Contact;
	Key.ContactInstance = Request.ContactId;
	Key.Defender = this;
	FDefenseInteractionId InteractionId;
	const ABaseCombatCharacter* RequestSource = Cast<ABaseCombatCharacter>(
		Request.HitInfo.Attacker);
	const EDefenseCommitStatus Registration = CombatComponent->BeginDefenseInteraction(
		Key,
		InteractionId,
		Receipt,
		DoesContactIdentityBelongToSource(Request.ContactId, RequestSource));
	if (Registration != EDefenseCommitStatus::NewCommit)
	{
		Receipt.CommitStatus = Registration;
		return Receipt;
	}

	FDefenseQuery Query = Request.Query;
	FActualDefenseContact ActualContact = BuildActualDefenseContact(Request);
	PopulateDefenseContactQuery(Query, Request, ActualContact);
	const bool bTargetStillValid = IsValid(this) && IsValid(CombatComponent);
	const bool bSourceStillValid = IsValid(Request.HitInfo.Attacker);
	if (!bTargetStillValid || !bSourceStillValid)
	{
		ActualContact.bIsValid = false;
		Query.ActualContact = ActualContact;
		Query.bHasActualContact = false;
		Query.bDefenderAlive = Query.bDefenderAlive && bTargetStillValid;
		Query.Attack.bAttackerAlive = Query.Attack.bAttackerAlive && bSourceStillValid;
	}
	Receipt.Resolution.InteractionId = InteractionId;
	Receipt.Resolution.Stage = EDefenseQueryStage::Contact;
	Receipt.Resolution.PredictedContact = Query.Attack.PredictedContact;
	Receipt.Resolution.ActualContact = ActualContact;
	Receipt.Resolution.bHasActualContact = ActualContact.bIsValid;
	Receipt.Resolution.Decision = FDefenseResolver::Resolve(Query);
	if (Receipt.Resolution.Decision.Outcome == EDefenseOutcome::NormalBlock)
	{
		const UDefenseConfiguration* DefenderConfiguration =
			CombatComponent->GetEffectiveDefenseConfiguration();
		const UDefenseConfiguration* AttackerConfiguration = RequestSource
			&& RequestSource->CombatComponent
			? RequestSource->CombatComponent->GetEffectiveDefenseConfiguration()
			: GetDefault<UDefenseConfiguration>();
		CommitNormalBlockPresentation(
			Receipt.Resolution,
			DefenderConfiguration,
			AttackerConfiguration);

		Receipt.Resolution.AlignmentRequest.OwnerInteraction = InteractionId;
		Receipt.Resolution.AlignmentRequest.Policy = EDefenseAlignmentPolicy::BlockContact;
		Receipt.Resolution.AlignmentRequest.Target = Request.HitInfo.Attacker;
		Receipt.Resolution.AlignmentRequest.MaximumTurnRate = DefenderConfiguration
			? FMath::Max(0.0f, DefenderConfiguration->DefenseTurnRate)
			: 180.0f;
		const float MaximumTurn = DefenderConfiguration
			? FMath::Max(0.0f, DefenderConfiguration->MaximumAutomaticTurn)
			: 70.0f;
		Receipt.Resolution.AlignmentRequest.RemainingTurnBudget = FMath::Min(
			MaximumTurn,
			FMath::Abs(Receipt.Resolution.Decision.MeasuredYawDegrees));
		Receipt.Resolution.AlignmentRequest.MaximumTranslation = DefenderConfiguration
			? FMath::Max(0.0f, DefenderConfiguration->NormalBlockTranslationAllowance)
			: 0.0f;
	}
	Receipt.CommitStatus = EDefenseCommitStatus::NewCommit;
	Receipt.bAcceptsWeaponHit = DefenseOutcomeAcceptsWeaponHit(
		Receipt.Resolution.Decision.Outcome);
	Receipt.bConsumesHitBudget = Receipt.bAcceptsWeaponHit;
	if (!bTargetStillValid)
	{
		return Receipt;
	}

	const float ResistanceSnapshot = HitReactionComponent
		? HitReactionComponent->DamageResistance
		: 0.0f;
	FDefenseGameplayCommitResult Gameplay = CommitResolvedDefenseDamage(
		Receipt.Resolution,
		ResistanceSnapshot);
	Receipt.AppliedDamage = FMath::Max(0.0f, -Gameplay.Health.ActualDelta);
	Gameplay.Receipt = Receipt;
	CombatComponent->FinalizeDefenseInteraction(InteractionId, Receipt);
	PendingDefenseGameplayCommits.Add(InteractionId, MoveTemp(Gameplay));
	ScheduleDefenseContactFallback(InteractionId);
#if WITH_AUTOMATION_TESTS
	if (AActor* ActorToDestroy = ActorToDestroyAfterDefenseCommitForTesting.Get())
	{
		ActorToDestroy->Destroy();
	}
#endif
	return Receipt;
}

FDefenseContactReceipt ABaseCombatCharacter::ResolveWeaponContactCandidate(
	AActor* Target,
	const FDefenseContactRequest& Request)
{
	FDefenseContactReceipt Rejected;
	ABaseCombatCharacter* TargetCharacter = Cast<ABaseCombatCharacter>(Target);
	if (!IsValid(TargetCharacter) || TargetCharacter == this)
	{
		return Rejected;
	}

	FDefenseContactRequest CanonicalRequest = Request;
	CanonicalRequest.HitInfo.Attacker = this;
	return TargetCharacter->ResolveAndCommitCombatContact(CanonicalRequest);
}

void ABaseCombatCharacter::ScheduleDefenseContactFallback(
	const FDefenseInteractionId& InteractionId)
{
	UWorld* World = GetWorld();
	if (!World
		|| InteractionId.Epoch == 0
		|| InteractionId.Key.Defender.Get() != this
		|| PendingDefenseFallbackTickers.Contains(InteractionId))
	{
		return;
	}

	const TWeakObjectPtr<ABaseCombatCharacter> WeakTarget(this);
	const TWeakObjectPtr<UWorld> WeakWorld(World);
	const FTSTicker::FDelegateHandle Handle = FTSTicker::GetCoreTicker().AddTicker(
		TEXT("DefenseContactFallback"),
		0.0f,
		[WeakTarget, WeakWorld, InteractionId](float)
		{
			ABaseCombatCharacter* Target = WeakTarget.Get();
			if (!Target)
			{
				return false;
			}

			// The ticker removes itself by returning false; remove the retained handle
			// before flushing so normal cancellation never removes an executing ticker.
			Target->PendingDefenseFallbackTickers.Remove(InteractionId);
			if (!WeakWorld.IsValid() || Target->GetWorld() != WeakWorld.Get())
			{
				return false;
			}
			Target->FlushCommittedDefenseContact(InteractionId);
			return false;
		});
	PendingDefenseFallbackTickers.Add(
		InteractionId,
		Handle);
}

void ABaseCombatCharacter::CancelDefenseContactFallback(
	const FDefenseInteractionId& InteractionId)
{
	if (const FTSTicker::FDelegateHandle* Handle = PendingDefenseFallbackTickers.Find(InteractionId))
	{
		FTSTicker::RemoveTicker(*Handle);
		PendingDefenseFallbackTickers.Remove(InteractionId);
	}
}

bool ABaseCombatCharacter::TryClaimDefenseContactSourceFinalization(
	const FDefenseInteractionId& InteractionId,
	const ABaseCombatCharacter* ClaimingSource,
	FDefenseContactReceipt& OutCanonicalReceipt)
{
	OutCanonicalReceipt = FDefenseContactReceipt();
	FDefenseGameplayCommitResult* Pending = PendingDefenseGameplayCommits.Find(InteractionId);
	if (!Pending
		|| Pending->bSourceFinalizationClaimed
		|| Pending->Receipt.Resolution.ActualContact.HitInfo.Attacker != ClaimingSource
		|| !IsDefenseDispatchValid(&InteractionId))
	{
		return false;
	}

	Pending->bSourceFinalizationClaimed = true;
	OutCanonicalReceipt = Pending->Receipt;
	return true;
}

void ABaseCombatCharacter::FlushCommittedDefenseContact(
	const FDefenseInteractionId& InteractionId)
{
	FDefenseGameplayCommitResult* Pending = PendingDefenseGameplayCommits.Find(InteractionId);
	if (!Pending)
	{
		return;
	}

	FDefenseGameplayCommitResult Commit = MoveTemp(*Pending);
	PendingDefenseGameplayCommits.Remove(InteractionId);
	CancelDefenseContactFallback(InteractionId);
	if (!IsDefenseDispatchValid(&InteractionId))
	{
		return;
	}

	if (Commit.bDispatchDamage && HitReactionComponent)
	{
		FCommittedHitReactionDamage DamageCommit;
		DamageCommit.HitInfo = Commit.HitInfo;
		DamageCommit.ResolvedDamage = Commit.ResolvedDamage;
		DamageCommit.bShouldNotify = true;
		DamageCommit.bShouldPlayReaction = Commit.bPlayHitReaction;
		HitReactionComponent->PlayCommittedDamageReaction(DamageCommit);
		if (!IsDefenseDispatchValid(&InteractionId))
		{
			return;
		}
		HitReactionComponent->BroadcastCommittedDamage(DamageCommit);
		if (!IsDefenseDispatchValid(&InteractionId))
		{
			return;
		}
	}

	DispatchCommittedHealth(Commit.Health, &InteractionId);
	if (!IsDefenseDispatchValid(&InteractionId))
	{
		return;
	}
	if (CombatComponent)
	{
		CombatComponent->OnDefenseResolvedNative.Broadcast(Commit.Receipt.Resolution);
	}
}

void ABaseCombatCharacter::FinalizeResolvedWeaponContact(
	AActor* Target,
	const FDefenseContactReceipt& Receipt)
{
	if (Receipt.CommitStatus != EDefenseCommitStatus::NewCommit
		|| !Receipt.Resolution.InteractionId.IsValid())
	{
		return;
	}

	TWeakObjectPtr<ABaseCombatCharacter> WeakSource(this);
	TWeakObjectPtr<ABaseCombatCharacter> WeakTarget(Cast<ABaseCombatCharacter>(Target));
	FDefenseContactReceipt CanonicalReceipt;
	if (!WeakTarget.IsValid()
		|| !WeakTarget->CombatComponent
		|| !WeakTarget->CombatComponent->IsDefenseInteractionFinalized(
			Receipt.Resolution.InteractionId)
		|| !WeakTarget->TryClaimDefenseContactSourceFinalization(
			Receipt.Resolution.InteractionId,
			this,
			CanonicalReceipt))
	{
		return;
	}

	if (CanonicalReceipt.bAcceptsWeaponHit)
	{
		PlayResolvedWeaponImpact(Target, CanonicalReceipt);
		if (!WeakSource.IsValid()
			|| !WeakTarget.IsValid()
			|| !WeakTarget->CombatComponent
			|| !WeakTarget->CombatComponent->IsDefenseInteractionFinalized(
				CanonicalReceipt.Resolution.InteractionId))
		{
			return;
		}
	}

	WeakTarget->FlushCommittedDefenseContact(CanonicalReceipt.Resolution.InteractionId);
	if (!WeakSource.IsValid()
		|| !WeakTarget.IsValid()
		|| !WeakTarget->CombatComponent
		|| !WeakTarget->CombatComponent->IsDefenseInteractionFinalized(
			CanonicalReceipt.Resolution.InteractionId))
	{
		return;
	}

	if (CanonicalReceipt.bAcceptsWeaponHit && CombatComponent)
	{
		CombatComponent->OnAttackHit.Broadcast(
			Target,
			CanonicalReceipt.Resolution.ActualContact.HitInfo);
	}
}

void ABaseCombatCharacter::PlayResolvedWeaponImpact(
	AActor* Target,
	const FDefenseContactReceipt& Receipt)
{
	auto IsPresentationCurrent = [this, Target, &Receipt]()
	{
		const ABaseCombatCharacter* TargetCharacter = Cast<ABaseCombatCharacter>(Target);
		return IsValid(this)
			&& IsValid(TargetCharacter)
			&& TargetCharacter->CombatComponent
			&& TargetCharacter->CombatComponent->IsDefenseInteractionFinalized(
				Receipt.Resolution.InteractionId);
	};

	if (!Receipt.bAcceptsWeaponHit || !IsPresentationCurrent())
	{
		return;
	}

#if WITH_AUTOMATION_TESTS
	++ResolvedWeaponImpactAttemptCountForTesting;
	AcceptedHitCountObservedDuringImpactForTesting = WeaponComponent
		? WeaponComponent->GetAcceptedHitCountForTesting()
		: INDEX_NONE;
	if (bDestroyTargetDuringResolvedWeaponImpactForTesting && IsValid(Target))
	{
		Target->Destroy();
	}
	if (bDestroySelfDuringResolvedWeaponImpactForTesting)
	{
		Destroy();
	}
	if (bDestroyTargetDuringResolvedWeaponImpactForTesting
		|| bDestroySelfDuringResolvedWeaponImpactForTesting)
	{
		return;
	}
#endif

	if (Receipt.Resolution.Decision.Outcome == EDefenseOutcome::NormalBlock)
	{
		ABaseCombatCharacter* TargetCharacter = Cast<ABaseCombatCharacter>(Target);
		if (TargetCharacter && TargetCharacter->HitReactionComponent)
		{
			TargetCharacter->HitReactionComponent->PlayDefensePresentation(Receipt.Resolution);
			if (!IsPresentationCurrent())
			{
				return;
			}
		}
		if (HitReactionComponent)
		{
			HitReactionComponent->PlayAttackerResponse(Receipt.Resolution);
		}
		return;
	}

	const FHitReactionInfo& HitInfo = Receipt.Resolution.ActualContact.HitInfo;
	UAttackData* AttackData = HitInfo.AttackData;
	if (!AttackData)
	{
		return;
	}

	const bool bWasBlocked = Receipt.Resolution.Decision.Outcome == EDefenseOutcome::NormalBlock;
	const UCombatFXData* FXData = nullptr;
	USoundBase* WeaponAudioFallback = nullptr;
	UNiagaraSystem* WeaponVFXFallback = nullptr;
	if (WeaponComponent && WeaponComponent->WeaponData)
	{
		FXData = WeaponComponent->WeaponData->CombatFXData;
		WeaponAudioFallback = WeaponComponent->WeaponData->HitSound;
		WeaponVFXFallback = WeaponComponent->WeaponData->HitVFX;
	}

	UCinematicEffectsUtilityLibrary::ResolveAndPlayImpactSound(
		GetWorld(),
		AttackData->ImpactAudioConfig,
		FXData,
		AttackData->AttackType,
		WeaponAudioFallback,
		HitInfo.ImpactPoint,
		bWasBlocked,
		this);
	if (!IsPresentationCurrent())
	{
		return;
	}

	UCinematicEffectsUtilityLibrary::ResolveAndSpawnImpactVFX(
		GetWorld(),
		AttackData->ImpactVFXConfig,
		FXData,
		AttackData->AttackType,
		WeaponVFXFallback,
		HitInfo.ImpactPoint,
		HitInfo.ImpactNormal,
		bWasBlocked,
		HitInfo.BoneName);
	if (!IsPresentationCurrent())
	{
		return;
	}

	if (AttackData->HitstopConfig.IsActive())
	{
		UCinematicEffectsUtilityLibrary::ApplyHitstop(
			this,
			Target,
			AttackData->HitstopConfig,
			bWasBlocked);
	}
}

// ============================================================================
// IDamageableInterface IMPLEMENTATION (Health Queries)
// ============================================================================

float ABaseCombatCharacter::GetCurrentHealth_Implementation() const
{
    return CurrentHealth;
}

float ABaseCombatCharacter::GetMaxHealth_Implementation() const
{
    return MaxHealth;
}

bool ABaseCombatCharacter::IsAlive_Implementation() const
{
    // Alive = not dying AND not dead (health doesn't matter once dying starts)
    return !IsDeadOrDying();
}

// ============================================================================
// IDamageableInterface IMPLEMENTATION (Combat)
// ============================================================================

float ABaseCombatCharacter::ApplyDamage_Implementation(const FHitReactionInfo& HitInfo)
{
    if (!HitReactionComponent)
    {
        return 0.0f;
    }

    if (CombatComponent && CombatComponent->CanBlockHit(HitInfo))
    {
        UE_LOG(LogTemp, Log, TEXT("[DAMAGE] %s blocked %.1f incoming damage"),
            *GetName(), HitInfo.Damage);
        return 0.0f;
    }

    const float DamageDealt = HitReactionComponent->ApplyDamage(HitInfo);

    // Modify health by damage amount
    ModifyHealth(-DamageDealt, HitInfo.Attacker);

    return DamageDealt;
}

bool ABaseCombatCharacter::ApplyPostureDamage_Implementation(float PostureDamage, AActor* Attacker)
{
    // DEPRECATED: Posture system removed. Use HitReactionComponent::ApplyStagger() instead.
    return false;
}

bool ABaseCombatCharacter::CanBeDamaged_Implementation() const
{
    return HitReactionComponent ? HitReactionComponent->CanBeDamaged() : (CurrentHealth > 0.0f);
}

bool ABaseCombatCharacter::IsBlocking_Implementation() const
{
    return CombatComponent ? CombatComponent->IsBlocking() : false;
}

bool ABaseCombatCharacter::IsGuardBroken_Implementation() const
{
    // DEPRECATED: Forwards to IsStaggered for backwards compatibility
    return IsStaggered_Implementation();
}

bool ABaseCombatCharacter::IsStaggered_Implementation() const
{
    return HitReactionComponent ? HitReactionComponent->IsStaggered() : false;
}

bool ABaseCombatCharacter::ExecuteFinisher_Implementation(AActor* Attacker, UAttackData* FinisherData)
{
    if (!HitReactionComponent || !FinisherData)
    {
        return false;
    }

    // Check if we're in a finishable state (staggered or stunned)
    if (!IsStaggered_Implementation() && (!HitReactionComponent || !HitReactionComponent->IsStunned()))
    {
        return false;
    }

    // Play victim animation
    const FName FinisherName = FinisherData->MontageSection;
    return HitReactionComponent->PlayFinisherVictimAnimation(FinisherName);
}

void ABaseCombatCharacter::OnAttackParried_Implementation(AActor* Parrier)
{
    // Play parried reaction animation
    if (HitReactionComponent)
    {
        HitReactionComponent->PlayGuardBrokenReaction();
    }
}

void ABaseCombatCharacter::OpenCounterWindow_Implementation(float Duration)
{
    // TODO: Migrate counter window system
}

float ABaseCombatCharacter::GetCurrentPosture_Implementation() const
{
    // DEPRECATED: Posture system removed
    return 0.0f;
}

float ABaseCombatCharacter::GetMaxPosture_Implementation() const
{
    // DEPRECATED: Posture system removed
    return 100.0f;
}

bool ABaseCombatCharacter::IsInCounterWindow_Implementation() const
{
    return PairedAnimationComponent ? PairedAnimationComponent->IsInCounterWindow() : false;
}

// ============================================================================
// ICombatInterface IMPLEMENTATION
// ============================================================================

bool ABaseCombatCharacter::CanPerformAttack_Implementation() const
{
    // TODO: Delegate to CombatComponent when migrated
    return CombatComponent != nullptr && IsAlive_Implementation();
}

ECombatState ABaseCombatCharacter::GetCombatState_Implementation() const
{
    return CombatComponent ? CombatComponent->GetCombatState() : ECombatState::Idle;
}

bool ABaseCombatCharacter::IsAttacking_Implementation() const
{
    return CombatComponent ? CombatComponent->IsAttacking() : false;
}

UAttackData* ABaseCombatCharacter::GetCurrentAttack_Implementation() const
{
    return CombatComponent ? CombatComponent->GetCurrentAttack() : nullptr;
}

EAttackPhase ABaseCombatCharacter::GetCurrentPhase_Implementation() const
{
    return CombatComponent ? CombatComponent->GetCurrentPhase() : EAttackPhase::None;
}

void ABaseCombatCharacter::OnEnableHitDetection_Implementation()
{
    if (WeaponComponent)
    {
        WeaponComponent->EnableHitDetection();
    }
}

void ABaseCombatCharacter::OnDisableHitDetection_Implementation()
{
    if (WeaponComponent)
    {
        WeaponComponent->DisableHitDetection();
    }
}

void ABaseCombatCharacter::OnAttackPhaseBegin_Implementation(EAttackPhase Phase)
{
    // Legacy notify state - phase transitions now use OnAttackPhaseTransition
}

void ABaseCombatCharacter::OnAttackPhaseEnd_Implementation(EAttackPhase Phase)
{
    // Legacy notify state - phase transitions now use OnAttackPhaseTransition
}

void ABaseCombatCharacter::OnAttackPhaseTransition_Implementation(EAttackPhase NewPhase)
{
    if (CombatComponent)
    {
        CombatComponent->OnPhaseTransition(NewPhase);
    }
}

bool ABaseCombatCharacter::IsInParryWindow_Implementation() const
{
    return PairedAnimationComponent ? PairedAnimationComponent->IsInParryWindow() : false;
}

void ABaseCombatCharacter::OnHoldWindowStart_Implementation(EInputType InputType)
{
    if (CombatComponent)
    {
        CombatComponent->OnHoldWindowStart(InputType);
    }
}

// ============================================================================
// WEAPON HIT PROCESSING
// ============================================================================

void ABaseCombatCharacter::OnWeaponHitTarget(AActor* HitActor, const FHitResult& HitResult, UAttackData* AttackData)
{
    if (!HitActor || !AttackData)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[HIT] %s hit %s with %s"),
        *GetName(),
        *HitActor->GetName(),
        *AttackData->GetName());

    if (Implements<UTeamMemberInterface>() && HitActor->Implements<UTeamMemberInterface>() &&
        ITeamMemberInterface::Execute_IsFriendlyTo(this, HitActor))
    {
        UE_LOG(LogCombat, Verbose, TEXT("[HIT] %s SKIPPED: Target %s is friendly"),
            *GetName(), *HitActor->GetName());
        return;
    }

    // Skip dead/dying actors entirely - no damage, no reactions
    if (ABaseCombatCharacter* CombatChar = Cast<ABaseCombatCharacter>(HitActor))
    {
        if (CombatChar->IsDeadOrDying())
        {
            UE_LOG(LogTemp, Warning, TEXT("[HIT] %s SKIPPED: Target %s is dead or dying"),
                *GetName(), *HitActor->GetName());
            return;
        }
    }

    // Check if victim is in i-frames (invulnerable during hit reaction)
    if (UHitReactionComponent* VictimHitReaction = HitActor->FindComponentByClass<UHitReactionComponent>())
    {
        if (VictimHitReaction->IsInIFrames())
        {
            UE_LOG(LogTemp, Log, TEXT("[HIT] %s SKIPPED: Target %s is in i-frames"),
                *GetName(), *HitActor->GetName());
            return;
        }
    }

    // Check if target implements IDamageableInterface
    if (HitActor->Implements<UDamageableInterface>())
    {
        // Get weapon damage multiplier
        float WeaponMultiplier = 1.0f;
        if (WeaponComponent)
        {
            WeaponMultiplier = WeaponComponent->GetDamageMultiplier();
        }

        // Build hit reaction info
        FHitReactionInfo HitInfo;
        HitInfo.Attacker = this;

        // HitDirection convention: points FROM victim TOWARD attacker (direction the hit came from)
        // Used by victim's HitReactionComponent to select correct directional animation
        // Prefer weapon tip velocity (negated to match convention) over position-based fallback
        if (WeaponComponent && WeaponComponent->GetWeaponTipVelocity().SizeSquared() > KINDA_SMALL_NUMBER)
        {
            // Negate: weapon travels attacker→victim, convention needs victim→attacker
            HitInfo.HitDirection = -WeaponComponent->GetWeaponTipVelocity().GetSafeNormal();
        }
        else
        {
            // Fallback: position-based (attacker pos - victim pos = victim→attacker)
            HitInfo.HitDirection = (GetActorLocation() - HitActor->GetActorLocation()).GetSafeNormal();
        }

        HitInfo.AttackData = AttackData;
        HitInfo.Damage = AttackData->BaseDamage * WeaponMultiplier;
        HitInfo.StunDuration = AttackData->HitStunDuration;
        HitInfo.ImpactPoint = HitResult.ImpactPoint;
        HitInfo.ImpactNormal = HitResult.ImpactNormal;
        HitInfo.BoneName = HitResult.BoneName;

        // Surface type from physical material on hit geometry
        HitInfo.SurfaceType = UWeaponTraceLibrary::MapPhysicalMaterialToSurfaceType(
            HitResult.PhysMaterial.Get());

        // Hit confidence: blade position + weapon speed quality metric
        if (WeaponComponent)
        {
            const FVector BladeBase = WeaponComponent->GetSocketLocation(WeaponComponent->GetEffectiveStartSocketName());
            const FVector BladeTip = WeaponComponent->GetSocketLocation(WeaponComponent->GetEffectiveEndSocketName());
            HitInfo.HitConfidence = UWeaponTraceLibrary::ComputeHitConfidence(
                WeaponComponent->GetWeaponTipVelocity(),
                HitResult.ImpactPoint,
                BladeBase,
                BladeTip);
        }

        // HIT-1: Populate extended hit metadata
        HitInfo.DistanceToTarget = FVector::Dist(GetActorLocation(), HitActor->GetActorLocation());
        HitInfo.PhaseWhenHit = CombatComponent ? CombatComponent->GetCurrentPhase() : EAttackPhase::None;

        // Use real weapon tip velocity from per-frame socket position tracking
        if (WeaponComponent)
        {
            HitInfo.WeaponVelocity = WeaponComponent->GetWeaponTipVelocity();
        }
        else
        {
            // Fallback: approximate from hit direction and damage magnitude
            HitInfo.WeaponVelocity = HitInfo.HitDirection * HitInfo.Damage;
        }

        // Populate animation time from current montage
        if (GetMesh())
        {
            if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
            {
                if (UAnimMontage* CurrentMontage = AnimInst->GetCurrentActiveMontage())
                {
                    HitInfo.AnimationTime = AnimInst->Montage_GetPosition(CurrentMontage);
                }
            }
        }

        // HIT-1: Populate bWasCounter from attacker's counter window state
        HitInfo.bWasCounter = CombatComponent ? CombatComponent->IsInCounterWindow() : false;

        // Compute block state once for damage, audio, and hitstop.
        bool bWasBlocked = IDamageableInterface::Execute_IsBlocking(HitActor);
        if (ABaseCombatCharacter* HitCombatCharacter = Cast<ABaseCombatCharacter>(HitActor))
        {
            bWasBlocked = HitCombatCharacter->CombatComponent &&
                HitCombatCharacter->CombatComponent->CanBlockHit(HitInfo);
        }

        UE_LOG(LogCombat, Log, TEXT("[HIT] %s applying %.1f damage to %s (blocked: %s)"),
            *GetName(), HitInfo.Damage, *HitActor->GetName(),
            bWasBlocked ? TEXT("YES") : TEXT("NO"));

        // Apply damage via interface
        IDamageableInterface::Execute_ApplyDamage(HitActor, HitInfo);

        // ============================================================
        // IMPACT AUDIO (Hit Sound) - 4-Tier Resolution
        // ============================================================
        // Gather FX data once for both audio and VFX
        const UCombatFXData* FXData = nullptr;
        USoundBase* WeaponAudioFallback = nullptr;
        UNiagaraSystem* WeaponVFXFallback = nullptr;
        if (WeaponComponent && WeaponComponent->WeaponData)
        {
            FXData = WeaponComponent->WeaponData->CombatFXData;
            WeaponAudioFallback = WeaponComponent->WeaponData->HitSound;
            WeaponVFXFallback = WeaponComponent->WeaponData->HitVFX;
        }

        UCinematicEffectsUtilityLibrary::ResolveAndPlayImpactSound(
            GetWorld(),
            AttackData->ImpactAudioConfig,
            FXData,
            AttackData->AttackType,
            WeaponAudioFallback,
            HitResult.ImpactPoint,
            bWasBlocked,
            this);

        // ============================================================
        // IMPACT VFX (Hit Effect) - 4-Tier Resolution
        // ============================================================
        UCinematicEffectsUtilityLibrary::ResolveAndSpawnImpactVFX(
            GetWorld(),
            AttackData->ImpactVFXConfig,
            FXData,
            AttackData->AttackType,
            WeaponVFXFallback,
            HitResult.ImpactPoint,
            HitResult.ImpactNormal,
            bWasBlocked,
            HitInfo.BoneName);

        // ============================================================
        // BROADCAST HIT DELEGATE
        // ============================================================
        if (CombatComponent)
        {
            CombatComponent->OnAttackHit.Broadcast(HitActor, HitInfo);
        }

        // ============================================================
        // HITSTOP (Per-Hit Impact Freeze)
        // ============================================================
        // Apply after damage so victim's hit reaction montage has started.
        // Both attacker and victim freeze; camera and VFX continue.
        if (AttackData->HitstopConfig.IsActive())
        {
            UCinematicEffectsUtilityLibrary::ApplyHitstop(
                this,       // Attacker
                HitActor,   // Victim
                AttackData->HitstopConfig,
                bWasBlocked);  // Reuse pre-computed value
        }
    }
    else
    {
        UE_LOG(LogCombat, Warning, TEXT("[HIT] %s SKIPPED: Target %s doesn't implement IDamageableInterface"),
            *GetName(), *HitActor->GetName());
    }
}
