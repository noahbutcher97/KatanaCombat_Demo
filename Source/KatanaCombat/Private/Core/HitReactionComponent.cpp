
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/HitReactionComponent.h"
#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Core/WeaponComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Data/HitReactionSettings.h"
#include "Data/HitReactionData.h"
#include "Data/CombatSettings.h"
#include "Data/AttackData.h"
#include "Data/DefenseConfiguration.h"
#include "Data/WeaponData.h"
#include "Data/CombatFXData.h"
#include "Characters/BaseCombatCharacter.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "Interfaces/DamageableInterface.h"
#include "GameplayTagContainer.h"
#include "AlphaBlend.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

// Static snapshot name for death pose - use in AnimBP with "Pose Snapshot" node
const FName UHitReactionComponent::DeathPoseSnapshotName = FName(TEXT("DeathPose"));

UHitReactionComponent::UHitReactionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false; // Only tick when stunned
    
    bHasSuperArmor = false;
    bIsInvulnerable = false;
    DamageResistance = 1.0f;
    
    bIsStunned = false;
    StunTimeRemaining = 0.0f;
}

void UHitReactionComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter)
    {
        AnimInstance = OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr;

        // Bind to GLOBAL montage blending out delegate
        // This is more reliable than per-montage delegates
        if (AnimInstance)
        {
            AnimInstance->OnMontageBlendingOut.AddDynamic(this, &UHitReactionComponent::OnAnyMontageBlendingOut);
            UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s bound global OnMontageBlendingOut delegate"),
                *OwnerCharacter->GetName());
        }
    }
}

void UHitReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleasePresentationAlignment(false);
	ReleasePresentationAlignment(true);
	if (AnimInstance)
	{
		AnimInstance->OnMontageBlendingOut.RemoveDynamic(
			this, &UHitReactionComponent::OnAnyMontageBlendingOut);
	}
	Super::EndPlay(EndPlayReason);
}

void UHitReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update i-frame tracking time
    if (bCurrentReactionHasIFrames)
    {
        CurrentReactionTime += DeltaTime;

        // Disable i-frame tracking once window has passed
        if (CurrentReactionTime > CurrentIFrameEnd)
        {
            bCurrentReactionHasIFrames = false;
        }
    }

    if (bIsStunned)
    {
        UpdateStun(DeltaTime);
    }

    // Update stagger timer
    if (bIsStaggered)
    {
        StaggerTimeRemaining -= DeltaTime;
        if (StaggerTimeRemaining <= 0.0f)
        {
            EndStagger();
        }
    }

    // Disable tick if nothing needs tracking
    if (!bIsStunned && !bIsStaggered && !bCurrentReactionHasIFrames)
    {
        SetComponentTickEnabled(false);
    }
}

// ============================================================================
// SETTINGS ACCESS
// ============================================================================

UHitReactionSettings* UHitReactionComponent::GetEffectiveSettings() const
{
    // Priority 1: Component override
    if (HitReactionSettingsOverride)
    {
        return HitReactionSettingsOverride;
    }

    // Priority 2: CombatSettings from owner character
    // Use helper for lazy initialization (works in test environments)
    if (const ABaseCombatCharacter* CombatChar = Cast<ABaseCombatCharacter>(GetOwnerCharacterCached()))
    {
        if (CombatChar->CombatSettings && CombatChar->CombatSettings->HitReactionSettings)
        {
            return CombatChar->CombatSettings->HitReactionSettings;
        }
    }

    // Priority 3: No settings available
    return nullptr;
}

bool UHitReactionComponent::IsInIFrames() const
{
    if (!bCurrentReactionHasIFrames)
    {
        return false;
    }
    return CurrentReactionTime >= CurrentIFrameStart && CurrentReactionTime <= CurrentIFrameEnd;
}

// ============================================================================
// DAMAGE APPLICATION
// ============================================================================

float UHitReactionComponent::ApplyDamage(const FHitReactionInfo& HitInfo)
{
    // Use helper for lazy initialization (works in test environments)
    ACharacter* CharOwner = GetOwnerCharacterCached();
    const FString OwnerName = CharOwner ? CharOwner->GetName() : TEXT("Unknown");
    const FString AttackerName = HitInfo.Attacker ? HitInfo.Attacker->GetName() : TEXT("Unknown");

    UE_LOG(LogTemp, Log, TEXT("[DAMAGE] %s receiving damage from %s (Raw: %.1f)"),
        *OwnerName, *AttackerName, HitInfo.Damage);

    // Check if owner is alive - don't process damage on dead characters
    if (CharOwner)
    {
        if (CharOwner->Implements<UDamageableInterface>())
        {
            if (!IDamageableInterface::Execute_IsAlive(CharOwner))
            {
                UE_LOG(LogTemp, Warning, TEXT("[DAMAGE] %s BLOCKED: Target is dead"),
                    *OwnerName);
                return 0.0f;
            }
        }
    }

    // Check invulnerability
    if (bIsInvulnerable)
    {
        UE_LOG(LogTemp, Log, TEXT("[DAMAGE] %s BLOCKED: Target is invulnerable"),
            *OwnerName);
        return 0.0f;
    }

    // Block non-partner damage during paired animations.
    // The paired animation partner (attacker) must be able to deal damage
    // for sync point health tracking. All other sources are blocked.
    if (bReactionsSuppressed && HitInfo.Attacker != PairedAnimationPartner.Get())
    {
        UE_LOG(LogTemp, Log, TEXT("[DAMAGE] %s BLOCKED: In paired animation, attacker %s is not the partner"),
            *OwnerName, *AttackerName);
        return 0.0f;
    }

    // Check i-frames
    if (IsInIFrames())
    {
        UE_LOG(LogTemp, Log, TEXT("[DAMAGE] %s BLOCKED: Target is in i-frames (%.2f/%.2f-%.2f)"),
            *OwnerName, CurrentReactionTime, CurrentIFrameStart, CurrentIFrameEnd);
        return 0.0f;
    }

	const FCommittedHitReactionDamage Commit = CommitResolvedDamage(HitInfo, DamageResistance);
	UE_LOG(LogTemp, Log, TEXT("[DAMAGE] %s APPLIED: %.1f damage (resistance: %.2f)"),
		*OwnerName, Commit.ResolvedDamage, DamageResistance);
	DispatchCommittedDamage(Commit);
	return Commit.ResolvedDamage;
}

FCommittedHitReactionDamage UHitReactionComponent::CommitResolvedDamage(
	const FHitReactionInfo& HitInfo,
	const float ResistanceSnapshot) const
{
	FCommittedHitReactionDamage Commit;
	Commit.HitInfo = HitInfo;
	Commit.ResolvedDamage = HitInfo.Damage * ResistanceSnapshot;
	Commit.bShouldNotify = true;
	Commit.bShouldPlayReaction = !bHasSuperArmor;
	return Commit;
}

void UHitReactionComponent::DispatchCommittedDamage(
	const FCommittedHitReactionDamage& Commit,
	const FDefenseInteractionId* InteractionId,
	const UCombatComponent* InteractionOwner)
{
	if (!Commit.bShouldNotify)
	{
		return;
	}

	BroadcastCommittedDamage(Commit);
	if (!IsValid(this) || !IsValid(GetOwner()))
	{
		return;
	}
	if (InteractionId
		&& (!InteractionOwner || !InteractionOwner->IsDefenseInteractionFinalized(*InteractionId)))
	{
		return;
	}
	PlayCommittedDamageReaction(Commit);
}

void UHitReactionComponent::PlayCommittedDamageReaction(
	const FCommittedHitReactionDamage& Commit)
{
	if (!Commit.bShouldNotify || !IsValid(this) || !IsValid(GetOwner()))
	{
		return;
	}
	if (Commit.bShouldPlayReaction)
	{
		PlayHitReaction(Commit.HitInfo);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[DAMAGE] %s has super armor - no hit reaction"),
			*GetNameSafe(GetOwner()));
	}
}

void UHitReactionComponent::BroadcastCommittedDamage(
	const FCommittedHitReactionDamage& Commit)
{
	if (Commit.bShouldNotify && IsValid(this) && IsValid(GetOwner()))
	{
		OnDamageReceived.Broadcast(Commit.HitInfo);
	}
}

bool UHitReactionComponent::PlayDefensePresentation(const FDefenseResolution& Resolution)
{
	if (!Resolution.InteractionId.IsValid()
		|| Resolution.InteractionId.Key.Defender.Get() != GetOwner()
		|| Resolution.Decision.Outcome != EDefenseOutcome::NormalBlock
		|| !Resolution.bHasActualContact
		|| !Resolution.ActualContact.bIsValid)
	{
		return false;
	}
#if WITH_AUTOMATION_TESTS
	++DefensePresentationAttemptCountForTesting;
#endif

	bool bPresented = PlayCommittedDefenseMontage(
		Resolution, Resolution.Presentation, false);
	const FHitReactionInfo& HitInfo = Resolution.ActualContact.HitInfo;
	ABaseCombatCharacter* Source = Cast<ABaseCombatCharacter>(HitInfo.Attacker);
	const UWeaponComponent* SourceWeapon = Source ? Source->WeaponComponent.Get() : nullptr;
	const UWeaponData* WeaponData = SourceWeapon ? SourceWeapon->WeaponData.Get() : nullptr;
	const UCombatFXData* FXData = WeaponData ? WeaponData->CombatFXData.Get() : nullptr;
	USoundBase* WeaponAudioFallback = WeaponData ? WeaponData->HitSound.Get() : nullptr;
	UNiagaraSystem* WeaponVFXFallback = WeaponData ? WeaponData->HitVFX.Get() : nullptr;
	const UAttackData* AttackData = Resolution.Decision.SelectedAttack
		? Resolution.Decision.SelectedAttack.Get()
		: HitInfo.AttackData.Get();
	const EAttackType AttackType = AttackData ? AttackData->AttackType : EAttackType::None;

	const FImpactAudioConfig AudioConfig = Resolution.Presentation.bOverrideImpactAudio
		? Resolution.Presentation.ImpactAudio
		: FImpactAudioConfig();
	bPresented = UCinematicEffectsUtilityLibrary::ResolveAndPlayImpactSound(
		GetWorld(),
		AudioConfig,
		FXData,
		AttackType,
		WeaponAudioFallback,
		HitInfo.ImpactPoint,
		true,
		Source) || bPresented;
	if (!IsValid(this) || !IsValid(GetOwner()))
	{
		return bPresented;
	}

	const FImpactVFXConfig VFXConfig = Resolution.Presentation.bOverrideImpactVFX
		? Resolution.Presentation.ImpactVFX
		: FImpactVFXConfig();
	bPresented = UCinematicEffectsUtilityLibrary::ResolveAndSpawnImpactVFX(
		GetWorld(),
		VFXConfig,
		FXData,
		AttackType,
		WeaponVFXFallback,
		HitInfo.ImpactPoint,
		HitInfo.ImpactNormal,
		true,
		HitInfo.BoneName) || bPresented;
	if (!IsValid(this) || !IsValid(GetOwner()))
	{
		return bPresented;
	}

	if (Resolution.Presentation.bOverrideHitstop
		&& Resolution.Presentation.Hitstop.IsActive()
		&& IsValid(Source))
	{
		UCinematicEffectsUtilityLibrary::ApplyHitstop(
			Source,
			GetOwner(),
			Resolution.Presentation.Hitstop,
			true);
		bPresented = true;
	}
	return bPresented;
}

bool UHitReactionComponent::PlayAttackerResponse(const FDefenseResolution& Resolution)
{
	AActor* CommittedAttacker = Resolution.Stage == EDefenseQueryStage::InputIntent
		? Resolution.InteractionId.Key.AttackInstance.Attacker.Get()
		: Resolution.ActualContact.HitInfo.Attacker.Get();
	if (!Resolution.InteractionId.IsValid() || CommittedAttacker != GetOwner())
	{
		return false;
	}
	if (Resolution.Decision.AttackerResponse == EAttackerResponse::None
		|| Resolution.Decision.AttackerResponse == EAttackerResponse::Continue)
	{
		return false;
	}
#if WITH_AUTOMATION_TESTS
	++AttackerResponseAttemptCountForTesting;
#endif

	if (PlayCommittedDefenseMontage(
		Resolution, Resolution.AttackerPresentation, true))
	{
		return true;
	}

	if (Resolution.Decision.AttackerResponse != EAttackerResponse::Recoil)
	{
		return false;
	}

	UAnimInstance* Instance = ResolveAnimInstance();
	UAnimMontage* ActiveMontage = Instance ? Instance->GetCurrentActiveMontage() : nullptr;
	if (!Instance || !ActiveMontage)
	{
		return false;
	}

	const float BlendOutSeconds = FMath::Max(
		0.0f,
		Resolution.AttackerPresentation.BlendOutSeconds);
	Instance->Montage_StopWithBlendOut(FAlphaBlendArgs(BlendOutSeconds), ActiveMontage);
	return true;
}

void UHitReactionComponent::PlayHitReaction(const FHitReactionInfo& HitInfo)
{
    if (!AnimInstance)
    {
        return;
    }

    // Use helper for lazy initialization (works in test environments)
    ACharacter* CharOwner = GetOwnerCharacterCached();

    // Paired animation state suppresses all hit reactions.
    // The victim montage IS the reaction — interrupting it with a hit reaction
    // would cause OnAnyMontageBlendingOut to fire prematurely, applying the
    // death outcome (freeze/ragdoll) at the moment of damage instead of at
    // the end of the animation.
    if (bReactionsSuppressed)
    {
        UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s PlayHitReaction SUPPRESSED: In paired animation state (damage still applies, reaction skipped)"),
            CharOwner ? *CharOwner->GetName() : TEXT("Unknown"));
        return;
    }

    // Check if owner is alive - don't play hit reactions on dead characters
    if (CharOwner && CharOwner->Implements<UDamageableInterface>())
    {
        if (!IDamageableInterface::Execute_IsAlive(CharOwner))
        {
            return;
        }
    }

    // Get relative hit direction
    const EAttackDirection RelativeDir = GetHitDirectionRelativeToFacing(HitInfo.HitDirection);

    // Try settings-based approach first
    if (UHitReactionSettings* Settings = GetEffectiveSettings())
    {
        // Determine intensity from attack
        EAttackType AttackType = EAttackType::Light;
        float Damage = HitInfo.Damage;
        if (HitInfo.AttackData)
        {
            AttackType = HitInfo.AttackData->AttackType;
        }

        // Get current health for damage percentage calculation
        float CurrentHealth = 100.0f; // Default
        if (const ABaseCombatCharacter* CombatChar = Cast<ABaseCombatCharacter>(CharOwner))
        {
            CurrentHealth = CombatChar->CurrentHealth;
        }

        const EHitIntensity Intensity = Settings->GetIntensityFromAttack(AttackType, Damage, CurrentHealth);
        const bool bIsHeavy = (Intensity == EHitIntensity::Heavy);

        // Get directional reaction entry (inline struct, not asset)
        if (const FHitReactionEntry* ReactionEntry = Settings->GetDirectionalReaction(Intensity, RelativeDir))
        {
            if (PlayReactionFromEntry(*ReactionEntry, RelativeDir, bIsHeavy, Intensity))
            {
                // Apply stun from reaction entry
                if (ReactionEntry->StunDuration > 0.0f)
                {
                    ApplyHitStun(ReactionEntry->StunDuration);
                }
                return;
            }
        }
    }

    // Fallback: Legacy approach using component properties
    if (UAnimMontage* ReactionMontage = SelectHitReactionMontage(HitInfo))
    {
        AnimInstance->Montage_Play(ReactionMontage);

        // Determine if heavy based on attack type
        const bool bIsHeavy = HitInfo.AttackData &&
            (HitInfo.AttackData->AttackType == EAttackType::Heavy ||
             HitInfo.AttackData->AttackType == EAttackType::Special);

        OnHitReactionStarted.Broadcast(RelativeDir, bIsHeavy);

        // Apply hitstun if specified in HitInfo
        if (HitInfo.StunDuration > 0.0f)
        {
            ApplyHitStun(HitInfo.StunDuration);
        }
    }
}

void UHitReactionComponent::ApplyHitStun(float Duration)
{
    if (Duration <= 0.0f)
    {
        return;
    }

    // Paired animation state suppresses stun application
    if (bReactionsSuppressed)
    {
        return;
    }

    bIsStunned = true;
    StunTimeRemaining = Duration;
    SetComponentTickEnabled(true);

    OnStunBegin.Broadcast(Duration);
}

void UHitReactionComponent::PlayGuardBrokenReaction()
{
    if (!AnimInstance || !GuardBrokenMontage)
    {
        return;
    }

    // Paired animation state suppresses guard break reactions
    if (bReactionsSuppressed)
    {
        return;
    }

    AnimInstance->Montage_Play(GuardBrokenMontage);
}

bool UHitReactionComponent::PlayFinisherVictimAnimation(FName FinisherName)
{
    if (!AnimInstance)
    {
        return false;
    }
    
    TObjectPtr<UAnimMontage>* FoundMontage = FinisherVictimAnimations.Find(FinisherName);
    if (!FoundMontage || !(*FoundMontage))
    {
        return false;
    }
    
    AnimInstance->Montage_Play(*FoundMontage);
    return true;
}

// ============================================================================
// STATE QUERIES
// ============================================================================

bool UHitReactionComponent::CanBeDamaged() const
{
    if (bIsInvulnerable)
    {
        return false;
    }

    // Can't take damage while dying or dead
    if (ABaseCombatCharacter* CombatChar = Cast<ABaseCombatCharacter>(OwnerCharacter))
    {
        if (CombatChar->IsDeadOrDying())
        {
            return false;
        }
    }

    return true;
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

UAnimMontage* UHitReactionComponent::SelectHitReactionMontage(const FHitReactionInfo& HitInfo) const
{
    // Determine which animation set to use based on stun duration/damage
    // Light reactions for shorter stun, heavy for longer
    const bool bIsHeavyReaction = (HitInfo.StunDuration > 0.3f);
    const FHitReactionAnimSet& AnimSet = bIsHeavyReaction ? HeavyHitReactions : LightHitReactions;
    
    // Determine hit direction relative to character

    // Select directional animation
    switch (const EAttackDirection Direction = GetHitDirectionRelativeToFacing(HitInfo.HitDirection))
    {
        case EAttackDirection::Forward:
            return AnimSet.FrontHit;
        case EAttackDirection::Backward:
            return AnimSet.BackHit;
        case EAttackDirection::Left:
            return AnimSet.LeftHit;
        case EAttackDirection::Right:
            return AnimSet.RightHit;
        default:
            return AnimSet.FrontHit; // Default to front
    }
}

EAttackDirection UHitReactionComponent::GetHitDirectionRelativeToFacing(const FVector& HitDirection) const
{
    // Use helper for lazy initialization (works in test environments)
    ACharacter* CharOwner = GetOwnerCharacterCached();
    if (!CharOwner || HitDirection.IsNearlyZero())
    {
        return EAttackDirection::Forward;
    }

    // Convert to local space
    FVector LocalDirection = CharOwner->GetActorTransform().InverseTransformVector(HitDirection);
    LocalDirection.Z = 0;
    LocalDirection.Normalize();
    
    const float ForwardDot = FVector::DotProduct(LocalDirection, FVector::ForwardVector);
    const float RightDot = FVector::DotProduct(LocalDirection, FVector::RightVector);
    
    // Determine quadrant
    if (FMath::Abs(ForwardDot) > FMath::Abs(RightDot))
    {
        return (ForwardDot > 0) ? EAttackDirection::Forward : EAttackDirection::Backward;
    }
    else
    {
        return (RightDot > 0) ? EAttackDirection::Right : EAttackDirection::Left;
    }
}

void UHitReactionComponent::UpdateStun(float DeltaTime)
{
    StunTimeRemaining -= DeltaTime;
    
    if (StunTimeRemaining <= 0.0f)
    {
        EndStun();
    }
}

void UHitReactionComponent::EndStun()
{
    bIsStunned = false;
    StunTimeRemaining = 0.0f;
    // Note: Do NOT call SetComponentTickEnabled(false) here.
    // TickComponent (line 87) handles tick disable when ALL conditions are cleared
    // (bIsStunned, bIsStaggered, bCurrentReactionHasIFrames). Disabling here
    // would freeze stagger/i-frame countdown if they were active concurrently.

    OnStunEnd.Broadcast();
}

ACharacter* UHitReactionComponent::GetOwnerCharacterCached() const
{
    // Use cached reference if available (set during BeginPlay)
    if (OwnerCharacter)
    {
        return OwnerCharacter;
    }
    // Fallback for test environments where BeginPlay may not be called
    return Cast<ACharacter>(GetOwner());
}

UAnimInstance* UHitReactionComponent::ResolveAnimInstance()
{
	if (IsValid(AnimInstance))
	{
		return AnimInstance;
	}
	ACharacter* Character = GetOwnerCharacterCached();
	AnimInstance = Character && Character->GetMesh()
		? Character->GetMesh()->GetAnimInstance()
		: nullptr;
	return AnimInstance;
}

void UHitReactionComponent::ReleasePresentationAlignment(const bool bAttackerResponse)
{
	FAlignmentRequestHandle& Handle = bAttackerResponse
		? AttackerResponseAlignmentHandle
		: DefensePresentationAlignmentHandle;
	if (Handle.IsValid())
	{
		if (const ABaseCombatCharacter* Character = Cast<ABaseCombatCharacter>(GetOwnerCharacterCached()))
		{
			if (UTargetingComponent* Targeting = Character->GetTargetingComponent())
			{
				Targeting->ReleaseAlignmentRequest(Handle);
			}
		}
		Handle = {};
	}
	if (bAttackerResponse)
	{
		AttackerResponseMontage.Reset();
	}
	else
	{
		DefensePresentationMontage.Reset();
	}
}

bool UHitReactionComponent::PlayCommittedDefenseMontage(
	const FDefenseResolution& Resolution,
	const FDefensePresentationPayload& Payload,
	const bool bAttackerResponse)
{
	if (!IsValid(Payload.Montage) || bReactionsSuppressed)
	{
		return false;
	}
	if (!Payload.MontageSection.IsNone()
		&& Payload.Montage->GetSectionIndex(Payload.MontageSection) == INDEX_NONE)
	{
		return false;
	}

	UAnimInstance* Instance = ResolveAnimInstance();
	ABaseCombatCharacter* Character = Cast<ABaseCombatCharacter>(GetOwnerCharacterCached());
	if (!Instance || !Character)
	{
		return false;
	}

	ReleasePresentationAlignment(bAttackerResponse);
	if (Payload.bEnableRotationWarp)
	{
		AActor* AlignmentTarget = bAttackerResponse
			? Resolution.InteractionId.Key.Defender.Get()
			: Resolution.AlignmentRequest.Target.Get();
		if (!IsValid(AlignmentTarget))
		{
			AlignmentTarget = Resolution.ActualContact.HitInfo.Attacker;
		}
		UTargetingComponent* Targeting = Character->GetTargetingComponent();
		FVector ToTarget = AlignmentTarget
			? AlignmentTarget->GetActorLocation() - Character->GetActorLocation()
			: FVector::ZeroVector;
		ToTarget.Z = 0.0f;
		if (Targeting && IsValid(AlignmentTarget) && !ToTarget.IsNearlyZero())
		{
			const UDefenseConfiguration* Configuration = Character->CombatComponent
				? Character->CombatComponent->GetEffectiveDefenseConfiguration()
				: GetDefault<UDefenseConfiguration>();
			const float MaximumTurnRate = bAttackerResponse
				? (Configuration ? Configuration->DefenseTurnRate : 180.0f)
				: Resolution.AlignmentRequest.MaximumTurnRate;
			const float MaximumAutomaticTurn = Configuration
				? FMath::Max(0.0f, Configuration->MaximumAutomaticTurn)
				: 70.0f;
			const float CurrentYawError = FMath::Abs(FMath::FindDeltaAngleDegrees(
				Character->GetActorRotation().Yaw,
				ToTarget.Rotation().Yaw));
			const float RemainingTurnBudget = bAttackerResponse
				? FMath::Min(MaximumAutomaticTurn, CurrentYawError)
				: Resolution.AlignmentRequest.RemainingTurnBudget;
			if (FMath::IsFinite(MaximumTurnRate)
				&& FMath::IsFinite(RemainingTurnBudget)
				&& MaximumTurnRate > UE_SMALL_NUMBER
				&& RemainingTurnBudget > UE_SMALL_NUMBER)
			{
				int32& NextGeneration = bAttackerResponse
					? NextAttackerResponseAlignmentGeneration
					: NextDefensePresentationAlignmentGeneration;
				const int32 Generation = FMath::Max(1, NextGeneration);
				NextGeneration = NextGeneration == MAX_int32 ? 1 : NextGeneration + 1;

				FAlignmentRequestSpec Spec;
				Spec.OwnerId = bAttackerResponse
					? TEXT("AttackerResponsePresentation")
					: TEXT("DefenseContactPresentation");
				Spec.OwnerGeneration = Generation;
				Spec.Priority = EDefenseAlignmentPriority::BlockContact;
				Spec.Executor = EAlignmentExecutor::MotionWarping;
				Spec.Target = AlignmentTarget;
				Spec.DesiredRotation = ToTarget.Rotation();
				Spec.MaximumTurnRate = FMath::Max(0.0f, MaximumTurnRate);
				Spec.RemainingTurnBudget = FMath::Max(0.0f, RemainingTurnBudget);
				Spec.MaximumTranslation = bAttackerResponse
					? 0.0f
					: FMath::Max(0.0f, Resolution.AlignmentRequest.MaximumTranslation);
				Spec.WarpTargetName = bAttackerResponse
					? TEXT("AttackerResponseTarget")
					: TEXT("DefenseContactTarget");
				Spec.bTrackTargetRotation = true;
				Spec.bWarpTranslation = Spec.MaximumTranslation > UE_SMALL_NUMBER;
				FAlignmentRequestHandle& Handle = bAttackerResponse
					? AttackerResponseAlignmentHandle
					: DefensePresentationAlignmentHandle;
				Handle = Targeting->AcquireAlignmentRequest(Spec);
			}
		}
	}

	const float PlayedLength = Instance->Montage_PlayWithBlendIn(
		Payload.Montage,
		FAlphaBlendArgs(FMath::Max(0.0f, Payload.BlendInSeconds)));
	if (PlayedLength <= 0.0f)
	{
		ReleasePresentationAlignment(bAttackerResponse);
		return false;
	}
	if (!Payload.MontageSection.IsNone()
		&& Payload.Montage->GetSectionIndex(Payload.MontageSection) != INDEX_NONE)
	{
		Instance->Montage_JumpToSection(Payload.MontageSection, Payload.Montage);
	}

	if (bAttackerResponse)
	{
		AttackerResponseMontage = Payload.Montage;
	}
	else
	{
		DefensePresentationMontage = Payload.Montage;
	}
	return true;
}

// ============================================================================
// NEW DATA-DRIVEN API
// ============================================================================

bool UHitReactionComponent::PlayReactionFromEntry(const FHitReactionEntry& ReactionEntry, EAttackDirection Direction, bool bIsHeavy)
{
    // Delegate to overload with inferred intensity
    const EHitIntensity Intensity = bIsHeavy ? EHitIntensity::Heavy : EHitIntensity::Light;
    return PlayReactionFromEntry(ReactionEntry, Direction, bIsHeavy, Intensity);
}

bool UHitReactionComponent::PlayReactionFromEntry(const FHitReactionEntry& ReactionEntry, EAttackDirection Direction, bool bIsHeavy, EHitIntensity Intensity)
{
    if (!AnimInstance)
    {
        return false;
    }

    // Select montage variant with variety (n-2 for 3+, alternation for 2, direct for 1)
    FReactionMontageVariant SelectedVariant;
    if (!SelectMontageWithVariety(ReactionEntry, Intensity, Direction, SelectedVariant))
    {
        return false;
    }

    UAnimMontage* SelectedMontage = SelectedVariant.Montage;
    if (!SelectedMontage)
    {
        return false;
    }

    // Reset and setup i-frame tracking
    CurrentReactionTime = 0.0f;
    bCurrentReactionHasIFrames = ReactionEntry.bHasIFrames;
    CurrentIFrameStart = ReactionEntry.IFrameStart;
    CurrentIFrameEnd = ReactionEntry.IFrameEnd;

    // Enable tick for i-frame tracking if needed
    if (bCurrentReactionHasIFrames)
    {
        SetComponentTickEnabled(true);
    }

    // Play the SELECTED montage (with variety)
    const float Duration = AnimInstance->Montage_Play(SelectedMontage, ReactionEntry.PlayRate);

    if (Duration <= 0.0f)
    {
        bCurrentReactionHasIFrames = false;
        return false;
    }

    // Handle section selection - use the VARIANT's section (per-montage)
    const FName& SectionToUse = SelectedVariant.MontageSection;
    if (SectionToUse != NAME_None)
    {
        if (ReactionEntry.bJumpToSectionStart)
        {
            AnimInstance->Montage_JumpToSection(SectionToUse, SelectedMontage);
        }

        if (ReactionEntry.bUseSectionOnly)
        {
            AnimInstance->Montage_SetNextSection(SectionToUse, NAME_None, SelectedMontage);
        }
    }

    // Broadcast event
    OnHitReactionStarted.Broadcast(Direction, bIsHeavy);

    return true;
}

bool UHitReactionComponent::PlayReactionFromData(UHitReactionData* ReactionData)
{
    if (!ReactionData || !ReactionData->ReactionMontage || !AnimInstance)
    {
        return false;
    }

    // Setup i-frame tracking from data asset
    CurrentReactionTime = 0.0f;
    bCurrentReactionHasIFrames = ReactionData->bHasIFrames;
    CurrentIFrameStart = ReactionData->IFrameStart;
    CurrentIFrameEnd = ReactionData->IFrameEnd;

    // Enable tick for i-frame tracking if needed
    if (bCurrentReactionHasIFrames)
    {
        SetComponentTickEnabled(true);
    }

    // Play montage with section selection
    const float Duration = AnimInstance->Montage_Play(
        ReactionData->ReactionMontage,
        ReactionData->PlayRate);

    if (Duration <= 0.0f)
    {
        bCurrentReactionHasIFrames = false;
        return false;
    }

    // Handle section selection (like AttackData pattern)
    if (ReactionData->MontageSection != NAME_None)
    {
        if (ReactionData->bJumpToSectionStart)
        {
            AnimInstance->Montage_JumpToSection(
                ReactionData->MontageSection,
                ReactionData->ReactionMontage);
        }

        if (ReactionData->bUseSectionOnly)
        {
            AnimInstance->Montage_SetNextSection(
                ReactionData->MontageSection,
                NAME_None,
                ReactionData->ReactionMontage);
        }
    }

    // Broadcast event (Heavy and Special reactions are considered "heavy" for gameplay purposes)
    const bool bIsHeavy = ReactionData->ReactionType != EHitReactionType::Light;
    OnHitReactionStarted.Broadcast(ReactionData->Direction, bIsHeavy);

    return true;
}

bool UHitReactionComponent::PlayPairedReaction(EPairedReactionType PairedType, FName ReactionName)
{
    if (ReactionName == NAME_None)
    {
        return false;
    }

    UHitReactionSettings* Settings = GetEffectiveSettings();
    if (!Settings)
    {
        return false;
    }

    UHitReactionData* ReactionData = Settings->GetPairedReaction(PairedType, ReactionName);
    if (!ReactionData)
    {
        return false;
    }

    return PlayReactionFromData(ReactionData);
}

bool UHitReactionComponent::PlaySpecialReaction(ESpecialReactionType SpecialType)
{
    UHitReactionSettings* Settings = GetEffectiveSettings();
    if (!Settings)
    {
        // Fallback to legacy for guard broken
        if (SpecialType == ESpecialReactionType::GuardBroken && GuardBrokenMontage)
        {
            if (AnimInstance)
            {
                AnimInstance->Montage_Play(GuardBrokenMontage);
                return true;
            }
        }
        return false;
    }

    UHitReactionData* ReactionData = Settings->GetSpecialReaction(SpecialType);
    if (!ReactionData)
    {
        return false;
    }

    return PlayReactionFromData(ReactionData);
}

// ============================================================================
// DEATH REACTION & RAGDOLL
// ============================================================================

bool UHitReactionComponent::PlayDeathReaction(EAttackDirection Direction)
{
    // ========================================================================
    // GUARD: Already dead - death outcome was already applied
    // ========================================================================
    // This can happen when:
    // 1. Victim montage ended first → OnAnyMontageBlendingOut applied outcome
    // 2. Then damage was applied → HandleDeath() called PlayDeathReaction()
    // In this case, the character is already dead (bIsDead = true), so skip.
    if (ABaseCombatCharacter* CombatChar = Cast<ABaseCombatCharacter>(OwnerCharacter))
    {
        if (CombatChar->bIsDead)
        {
            UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s PlayDeathReaction: Character already dead (outcome already applied) - skipping"),
                OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"));
            return true;  // Death already handled
        }
    }

    // Clear reaction history on death (fresh start if revived)
    ClearReactionHistory();

    // Reset ragdoll flag (for respawn scenarios where character was previously ragdolled)
    bRagdollActivated = false;

    // ========================================================================
    // PAIRED ANIMATION DEATH HANDLING
    // If death was caused by a paired animation (finisher, lethal counter),
    // the victim montage IS the death animation. Don't play another one.
    //
    // EnterPairedAnimationState() set up the pending death state BEFORE damage
    // was applied. The victim montage continues playing as the death animation,
    // and OnAnyMontageBlendingOut will call ExitPairedAnimationState() and
    // apply the outcome when it ends.
    // ========================================================================
    if (bDeathHandledByPairedAnimation)
    {
        if (bDeathOutcomePending && PendingDeathMontage)
        {
            // The victim montage is still playing as the death animation.
            // Do NOT apply the outcome now — let the montage continue playing.
            // OnAnyMontageBlendingOut will catch it when it ends and apply the outcome,
            // then call ExitPairedAnimationState() to clean up all flags.
            UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s PlayDeathReaction: Victim montage playing as death animation - deferring to blend-out (Montage=%s, Outcome=%s)"),
                OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"),
                PendingDeathMontage ? *PendingDeathMontage->GetName() : TEXT("nullptr"),
                *UEnum::GetValueAsString(PairedAnimationDeathOutcome));

            // Clear only the death routing flag so we don't re-enter this path
            // (reaction suppression stays active until ExitPairedAnimationState)
            bDeathHandledByPairedAnimation = false;

            return true;
        }
        else
        {
            // Fallback: Montage already stopped or no pending state. Apply outcome directly.
            UE_LOG(LogTemp, Warning, TEXT("[HitReaction] %s PlayDeathReaction: Paired animation death with no pending montage - applying outcome directly (Outcome=%s, BlendTime=%.2f)"),
                OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"),
                *UEnum::GetValueAsString(PairedAnimationDeathOutcome),
                PairedAnimationRagdollBlendTime);

            const EReactionOutcome OutcomeToApply = PairedAnimationDeathOutcome;
            const float BlendTime = PairedAnimationRagdollBlendTime;

            // Clean up all paired animation state
            ExitPairedAnimationState();

            switch (OutcomeToApply)
            {
                case EReactionOutcome::Death:
                    FreezeAtCurrentPose();
                    break;

                case EReactionOutcome::Ragdoll:
                    ActivateRagdoll(BlendTime);
                    break;

                case EReactionOutcome::StandardRecovery:
                default:
                    UE_LOG(LogTemp, Warning, TEXT("[HitReaction] %s StandardRecovery outcome for paired animation death - this is unusual"),
                        OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"));
                    ActivateRagdoll(0.2f);
                    break;
            }

            return true;
        }
    }

    UHitReactionSettings* Settings = GetEffectiveSettings();
    if (!Settings || !AnimInstance)
    {
        UE_LOG(LogTemp, Warning, TEXT("[HitReaction] %s PlayDeathReaction: No settings or AnimInstance, falling back to ragdoll"),
            OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"));
        ActivateRagdoll(0.2f);
        return false;
    }

    const FHitReactionEntry* DeathEntry = Settings->GetDeathReaction(Direction);
    if (!DeathEntry || !DeathEntry->ReactionMontage)
    {
        UE_LOG(LogTemp, Warning, TEXT("[HitReaction] %s PlayDeathReaction: No death entry for direction %s, falling back to ragdoll"),
            OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"),
            *UEnum::GetValueAsString(Direction));
        ActivateRagdoll(0.2f);
        return false;
    }

    // Store the pending outcome to handle when blend-out starts
    PendingDeathOutcome = DeathEntry->Outcome;
    bDeathOutcomePending = true;
    PendingRagdollBlendTime = DeathEntry->RagdollBlendTime;

    // Store the montage we're waiting for (global delegate will filter by this)
    PendingDeathMontage = DeathEntry->ReactionMontage;

    UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s PlayDeathReaction: Setup for montage %s (Outcome: %s)"),
        OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"),
        *DeathEntry->ReactionMontage->GetName(),
        *UEnum::GetValueAsString(PendingDeathOutcome));

    // Play the death animation - global delegate will handle blend-out
    const bool bSuccess = PlayReactionFromEntry(*DeathEntry, Direction, true);

    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s death montage started successfully, waiting for blend-out"),
            OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[HitReaction] %s PlayDeathReaction: PlayReactionFromEntry FAILED, falling back to ragdoll"),
            OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"));
        bDeathOutcomePending = false;
        PendingDeathMontage = nullptr;
        ActivateRagdoll(0.2f);
    }

    return bSuccess;
}

void UHitReactionComponent::OnAnyMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (DefensePresentationMontage.Get() == Montage)
	{
		ReleasePresentationAlignment(false);
	}
	if (AttackerResponseMontage.Get() == Montage)
	{
		ReleasePresentationAlignment(true);
	}

    // Filter: Only process if this is the death montage we're waiting for
    if (!bDeathOutcomePending || !PendingDeathMontage || Montage != PendingDeathMontage)
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[HitReaction] %s DEATH MONTAGE BLENDING OUT! Montage=%s, Interrupted=%s"),
        OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"),
        Montage ? *Montage->GetName() : TEXT("nullptr"),
        bInterrupted ? TEXT("YES") : TEXT("NO"));

    // Capture the outcome before ExitPairedAnimationState clears it
    const EReactionOutcome OutcomeToApply = PendingDeathOutcome;
    const float RagdollBlendTime = PendingRagdollBlendTime;

    // Exit paired animation state — clears all flags (suppression, finisher target,
    // death handling, pending state). This prevents double application if
    // PlayDeathReaction is called later after damage is applied.
    ExitPairedAnimationState();

    UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s Applying death outcome: %s"),
        OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"),
        *UEnum::GetValueAsString(OutcomeToApply));

    switch (OutcomeToApply)
    {
        case EReactionOutcome::Death:
            // Freeze animation at current pose - character stays in death pose permanently
            FreezeAtCurrentPose();
            break;

        case EReactionOutcome::Ragdoll:
            // Activate ragdoll - physics will take over from current pose
            ActivateRagdoll(RagdollBlendTime);
            break;

        case EReactionOutcome::StandardRecovery:
        default:
            UE_LOG(LogTemp, Warning, TEXT("[HitReaction] %s StandardRecovery outcome for death - this is unusual"),
                OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"));
            // Let normal blend-out happen (shouldn't occur for death, but handle gracefully)
            break;
    }
}

void UHitReactionComponent::ActivateRagdoll(float BlendTime)
{
    // Prevent double activation (notify + blend-out could both try to trigger)
    if (bRagdollActivated)
    {
        UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s ActivateRagdoll skipped - already activated"),
            OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"));
        return;
    }

    if (!OwnerCharacter)
    {
        return;
    }

    USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
    if (!Mesh)
    {
        return;
    }

    // Mark as activated to prevent double activation
    bRagdollActivated = true;

    // Save pose snapshot BEFORE ragdoll - enables future recovery from ragdoll
    // (e.g., blend from pre-ragdoll pose to get-up animation)
    if (AnimInstance)
    {
        AnimInstance->SavePoseSnapshot(DeathPoseSnapshotName);
        bHasDeathPoseSnapshot = true;

        // Stop montages with no blend to freeze at current pose before physics
        AnimInstance->StopAllMontages(0.0f);
    }

    // Enable physics simulation on mesh - uses current bone transforms as starting pose
    Mesh->SetSimulatePhysics(true);
    Mesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);

    // CRITICAL: Clear any accumulated velocity to prevent ragdoll "pop"
    // This stops the ragdoll from launching into the air when physics activates
    Mesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
    Mesh->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

    // Set ragdoll collision profile
    Mesh->SetCollisionProfileName(TEXT("Ragdoll"));

    // Disable capsule collision so ragdoll can move freely
    if (UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // Disable movement (after death animation completed with root motion)
    if (UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement())
    {
        MovementComp->DisableMovement();
    }

    // Broadcast ragdoll event
    OnRagdollActivated.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s activated ragdoll (snapshot saved as '%s')"),
        OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"),
        *DeathPoseSnapshotName.ToString());

    // Finalize death - transition from Dying to Dead state
    if (ABaseCombatCharacter* CombatChar = Cast<ABaseCombatCharacter>(OwnerCharacter))
    {
        CombatChar->FinalizeDeath();
    }
}

void UHitReactionComponent::TriggerRagdollFromNotify(float BlendTime)
{
    UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s TriggerRagdollFromNotify called (BlendTime: %.2f)"),
        OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"),
        BlendTime);

    // Only activate if we're in a pending death state with ragdoll outcome
    if (!bDeathOutcomePending)
    {
        UE_LOG(LogTemp, Warning, TEXT("[HitReaction] %s TriggerRagdollFromNotify: No death pending, ignoring"),
            OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"));
        return;
    }

    if (PendingDeathOutcome != EReactionOutcome::Ragdoll)
    {
        UE_LOG(LogTemp, Warning, TEXT("[HitReaction] %s TriggerRagdollFromNotify: Outcome is not Ragdoll (%s), ignoring"),
            OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"),
            *UEnum::GetValueAsString(PendingDeathOutcome));
        return;
    }

    // Clear pending state since we're handling it now (prevents blend-out from re-triggering)
    bDeathOutcomePending = false;
    PendingDeathMontage = nullptr;

    // Activate ragdoll with the blend time from the notify
    ActivateRagdoll(BlendTime);
}

void UHitReactionComponent::FreezeAtCurrentPose()
{
    if (!OwnerCharacter || !AnimInstance)
    {
        return;
    }

    USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
    if (!Mesh)
    {
        return;
    }

    // 1. Save pose snapshot BEFORE stopping anything
    // This captures the exact skeletal pose at the moment of death
    // Can be used later in AnimBP with "Pose Snapshot" node for recovery/revive
    AnimInstance->SavePoseSnapshot(DeathPoseSnapshotName);
    bHasDeathPoseSnapshot = true;

    // 2. Stop all montages with no blend
    AnimInstance->StopAllMontages(0.0f);

    // 3. CRITICAL: Pause animation evaluation on the mesh
    // This prevents the AnimBP state machine from transitioning to idle
    // The mesh will hold its current bone transforms indefinitely
    Mesh->bPauseAnims = true;

    // 4. Disable movement NOW (after death animation completed with root motion)
    if (UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement())
    {
        MovementComp->DisableMovement();
    }

    UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s frozen at death pose (snapshot: '%s', anims paused)"),
        OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"),
        *DeathPoseSnapshotName.ToString());

    // 5. Finalize death - transition from Dying to Dead state
    if (ABaseCombatCharacter* CombatChar = Cast<ABaseCombatCharacter>(OwnerCharacter))
    {
        CombatChar->FinalizeDeath();
    }
}

void UHitReactionComponent::ClearDeathPoseSnapshot()
{
    bHasDeathPoseSnapshot = false;

    // Re-enable animation evaluation (for recovery/revive)
    if (OwnerCharacter)
    {
        if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
        {
            Mesh->bPauseAnims = false;
        }
    }

    // Note: UAnimInstance doesn't have a RemovePoseSnapshot function,
    // but the snapshot will be overwritten on next save or ignored if not used
    UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s death pose snapshot cleared (anims resumed)"),
        OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"));
}

// ============================================================================
// REACTION VARIETY (n-2 randomization)
// ============================================================================

bool UHitReactionComponent::SelectMontageWithVariety(
    const FHitReactionEntry& Entry,
    EHitIntensity Intensity,
    EAttackDirection Direction,
    FReactionMontageVariant& OutVariant)
{
    TArray<FReactionMontageVariant> AllVariants = Entry.GetAllVariants();

    if (AllVariants.Num() == 0)
    {
        return false;
    }

    // === 1 VARIANT: Always play it (no history tracking) ===
    if (AllVariants.Num() == 1)
    {
        OutVariant = AllVariants[0];
        return true;
    }

    // Get history for this intensity+direction
    FReactionHistory& History = ReactionHistoryMap.FindOrAdd(Intensity).FindOrAdd(Direction);

    // === 2 VARIANTS: Simple alternation (back and forth) ===
    if (AllVariants.Num() == 2)
    {
        const int32 LastPlayed = History.GetLastPlayed();
        // Play the other one (or 0 if no history)
        const int32 SelectedIndex = (LastPlayed == 0) ? 1 : 0;
        RecordMontagePlay(SelectedIndex, Intensity, Direction);
        OutVariant = AllVariants[SelectedIndex];
        return true;
    }

    // === 3+ VARIANTS: N-2 randomization ===
    // Build list of valid indices (exclude last 2 played)
    TArray<int32> ValidIndices;
    for (int32 i = 0; i < AllVariants.Num(); ++i)
    {
        bool bExcluded = false;
        for (int32 RecentIdx : History.RecentIndices)
        {
            if (RecentIdx == i)
            {
                bExcluded = true;
                break;
            }
        }
        if (!bExcluded)
        {
            ValidIndices.Add(i);
        }
    }

    // Fallback: if somehow all excluded, pick any (shouldn't happen with 3+)
    if (ValidIndices.Num() == 0)
    {
        ValidIndices.Add(FMath::RandRange(0, AllVariants.Num() - 1));
    }

    // Random select from valid indices
    const int32 SelectedIndex = ValidIndices[FMath::RandRange(0, ValidIndices.Num() - 1)];
    RecordMontagePlay(SelectedIndex, Intensity, Direction);
    OutVariant = AllVariants[SelectedIndex];

    return true;
}

void UHitReactionComponent::RecordMontagePlay(int32 MontageIndex, EHitIntensity Intensity, EAttackDirection Direction)
{
    FReactionHistory& History = ReactionHistoryMap.FindOrAdd(Intensity).FindOrAdd(Direction);
    History.RecordPlayed(MontageIndex);
}

void UHitReactionComponent::ClearReactionHistory()
{
    ReactionHistoryMap.Empty();
}

// ============================================================================
// FINISHER VULNERABILITY
// ============================================================================

bool UHitReactionComponent::IsVulnerableToFinisher() const
{
    // Already a finisher target - can't be targeted again
    if (bIsFinisherTarget)
    {
        return false;
    }

    // Dead or dying characters cannot be finisher targets
    ABaseCombatCharacter* CombatChar = Cast<ABaseCombatCharacter>(GetOwner());
    if (CombatChar && CombatChar->IsDeadOrDying())
    {
        return false;
    }

    // Check all trigger conditions
    return GetFinisherTriggerReason() != EFinisherTriggerReason::None;
}

EFinisherTriggerReason UHitReactionComponent::GetFinisherTriggerReason() const
{
    // Already a finisher target - return None to prevent stacking
    if (bIsFinisherTarget)
    {
        return EFinisherTriggerReason::None;
    }

    // Get owner as combat character for health/posture checks
    ABaseCombatCharacter* CombatChar = Cast<ABaseCombatCharacter>(GetOwner());
    if (!CombatChar)
    {
        return EFinisherTriggerReason::None;
    }

    // Priority 1: Staggered (replaces posture-based guard break)
    if (bIsStaggered)
    {
        return EFinisherTriggerReason::GuardBroken; // Reuse enum value for compatibility
    }

    // Priority 2: Stunned (from heavy attacks)
    if (IsStunned())
    {
        return EFinisherTriggerReason::Stunned;
    }

    // Priority 3: Low Health
    // Default threshold is 25% health
    const float HealthThreshold = 0.25f;
    const float HealthPercent = CombatChar->CurrentHealth / FMath::Max(CombatChar->MaxHealth, 1.0f);
    if (HealthPercent <= HealthThreshold && HealthPercent > 0.0f)
    {
        return EFinisherTriggerReason::LowHealth;
    }

    return EFinisherTriggerReason::None;
}

bool UHitReactionComponent::IsGuardBroken() const
{
    // DEPRECATED: Forwards to IsStaggered for backwards compatibility
    return bIsStaggered;
}

void UHitReactionComponent::ApplyStagger(float Duration)
{
    if (Duration <= 0.0f)
    {
        Duration = DefaultStaggerDuration;
    }

    bIsStaggered = true;
    StaggerTimeRemaining = Duration;

    // Enable tick to count down stagger timer
    SetComponentTickEnabled(true);

    // Broadcast stagger event
    OnStaggered.Broadcast(GetOwner(), Duration);

    // Play guard broken reaction animation (reused for stagger visual)
    PlayGuardBrokenReaction();

    UE_LOG(LogTemp, Log, TEXT("[STAGGER] %s staggered for %.1fs"),
        GetOwner() ? *GetOwner()->GetName() : TEXT("None"), Duration);
}

void UHitReactionComponent::EndStagger()
{
    if (bIsStaggered)
    {
        bIsStaggered = false;
        StaggerTimeRemaining = 0.0f;

        UE_LOG(LogTemp, Log, TEXT("[STAGGER] %s stagger ended"),
            GetOwner() ? *GetOwner()->GetName() : TEXT("None"));
    }
}

// ============================================================================
// PAIRED ANIMATION VICTIM STATE (Lifecycle API)
// ============================================================================

void UHitReactionComponent::EnterPairedAnimationState(UAnimMontage* VictimMontage, EReactionOutcome DeathOutcome, float RagdollBlendTime, bool bIsLethal, AActor* Partner)
{
    const FString OwnerName = OwnerCharacter ? OwnerCharacter->GetName() : TEXT("Unknown");

    // Take exclusive ownership of the reaction pipeline
    bReactionsSuppressed = true;
    bIsFinisherTarget = true;

    // Store partner reference for damage filtering
    // Damage from the partner (attacker) passes through; all other damage is blocked
    PairedAnimationPartner = Partner;

    if (bIsLethal)
    {
        // Register the victim montage as the "death montage" using the same
        // pattern as normal death handling. When the montage ends/blends-out,
        // OnAnyMontageBlendingOut will apply the configured outcome.
        PendingDeathMontage = VictimMontage;
        PendingDeathOutcome = DeathOutcome;
        PendingRagdollBlendTime = RagdollBlendTime;
        bDeathOutcomePending = true;

        // Set the paired animation flag so PlayDeathReaction knows to skip
        // playing a new animation (the victim montage IS the death animation)
        bDeathHandledByPairedAnimation = true;
        PairedAnimationDeathOutcome = DeathOutcome;
        PairedAnimationRagdollBlendTime = RagdollBlendTime;
    }

    UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s EnterPairedAnimationState: Lethal=%s, Montage=%s, Outcome=%s, BlendTime=%.2f"),
        *OwnerName,
        bIsLethal ? TEXT("YES") : TEXT("NO"),
        VictimMontage ? *VictimMontage->GetName() : TEXT("nullptr"),
        *UEnum::GetValueAsString(DeathOutcome),
        RagdollBlendTime);
}

void UHitReactionComponent::ExitPairedAnimationState()
{
    // Only log if we were actually in paired animation state
    if (bReactionsSuppressed || bIsFinisherTarget || bDeathHandledByPairedAnimation)
    {
        UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s ExitPairedAnimationState: Releasing reaction pipeline (Suppressed=%s, FinisherTarget=%s, DeathHandled=%s, PendingOutcome=%s)"),
            OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"),
            bReactionsSuppressed ? TEXT("true") : TEXT("false"),
            bIsFinisherTarget ? TEXT("true") : TEXT("false"),
            bDeathHandledByPairedAnimation ? TEXT("true") : TEXT("false"),
            bDeathOutcomePending ? TEXT("true") : TEXT("false"));
    }

    // Release reaction pipeline ownership
    bReactionsSuppressed = false;
    bIsFinisherTarget = false;
    PairedAnimationPartner.Reset();

    // Clear death handling flags
    bDeathHandledByPairedAnimation = false;
    PairedAnimationDeathOutcome = EReactionOutcome::Ragdoll;
    PairedAnimationRagdollBlendTime = 0.2f;

    // Clear pending death state (prevents OnAnyMontageBlendingOut from applying
    // outcome after a cancelled paired animation)
    bDeathOutcomePending = false;
    PendingDeathMontage = nullptr;
    PendingDeathOutcome = EReactionOutcome::Ragdoll;
    PendingRagdollBlendTime = 0.2f;
}
