
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/HitReactionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Data/HitReactionSettings.h"
#include "Data/HitReactionData.h"
#include "Data/CombatSettings.h"
#include "Data/AttackData.h"
#include "Characters/BaseCombatCharacter.h"
#include "Interfaces/DamageableInterface.h"
#include "GameplayTagContainer.h"
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
        AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();

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

    // Disable tick if nothing needs tracking
    if (!bIsStunned && !bCurrentReactionHasIFrames)
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

    // Check i-frames
    if (IsInIFrames())
    {
        UE_LOG(LogTemp, Log, TEXT("[DAMAGE] %s BLOCKED: Target is in i-frames (%.2f/%.2f-%.2f)"),
            *OwnerName, CurrentReactionTime, CurrentIFrameStart, CurrentIFrameEnd);
        return 0.0f;
    }

    // Calculate final damage
    float FinalDamage = HitInfo.Damage * DamageResistance;

    UE_LOG(LogTemp, Log, TEXT("[DAMAGE] %s APPLIED: %.1f damage (resistance: %.2f)"),
        *OwnerName, FinalDamage, DamageResistance);

    // Broadcast event
    OnDamageReceived.Broadcast(HitInfo);

    // Play hit reaction if not super armored
    if (!bHasSuperArmor)
    {
        PlayHitReaction(HitInfo);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[DAMAGE] %s has super armor - no hit reaction"),
            *OwnerName);
    }

    return FinalDamage;
}

void UHitReactionComponent::PlayHitReaction(const FHitReactionInfo& HitInfo)
{
    if (!AnimInstance)
    {
        return;
    }

    // Use helper for lazy initialization (works in test environments)
    ACharacter* CharOwner = GetOwnerCharacterCached();

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
    return !bIsInvulnerable;
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
    SetComponentTickEnabled(false);

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
    // Clear reaction history on death (fresh start if revived)
    ClearReactionHistory();

    // Reset ragdoll flag (for respawn scenarios where character was previously ragdolled)
    bRagdollActivated = false;

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
    // Filter: Only process if this is the death montage we're waiting for
    if (!bDeathOutcomePending || !PendingDeathMontage || Montage != PendingDeathMontage)
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[HitReaction] %s DEATH MONTAGE BLENDING OUT! Montage=%s, Interrupted=%s"),
        OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"),
        Montage ? *Montage->GetName() : TEXT("nullptr"),
        bInterrupted ? TEXT("YES") : TEXT("NO"));

    // Clear pending state
    bDeathOutcomePending = false;
    PendingDeathMontage = nullptr;

    UE_LOG(LogTemp, Log, TEXT("[HitReaction] %s Applying death outcome: %s"),
        OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"),
        *UEnum::GetValueAsString(PendingDeathOutcome));

    switch (PendingDeathOutcome)
    {
        case EReactionOutcome::Death:
            // Freeze animation at current pose - character stays in death pose permanently
            FreezeAtCurrentPose();
            break;

        case EReactionOutcome::Ragdoll:
            // Activate ragdoll - physics will take over from current pose
            ActivateRagdoll(PendingRagdollBlendTime);
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

    // Priority 1: Guard Broken (posture depleted)
    // Uses IDamageableInterface for flexibility
    if (IDamageableInterface::Execute_IsGuardBroken(CombatChar))
    {
        return EFinisherTriggerReason::GuardBroken;
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