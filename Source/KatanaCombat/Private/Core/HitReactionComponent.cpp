
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/HitReactionComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

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
    }
}

void UHitReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (bIsStunned)
    {
        UpdateStun(DeltaTime);
    }
}

// ============================================================================
// DAMAGE APPLICATION
// ============================================================================

float UHitReactionComponent::ApplyDamage(const FHitReactionInfo& HitInfo)
{
    // Check invulnerability
    if (bIsInvulnerable)
    {
        return 0.0f;
    }
    
    // Calculate final damage
    float FinalDamage = HitInfo.Damage * DamageResistance;
    
    // Broadcast event
    OnDamageReceived.Broadcast(HitInfo);
    
    // Play hit reaction if not super armored
    if (!bHasSuperArmor)
    {
        PlayHitReaction(HitInfo);
    }
    
    return FinalDamage;
}

void UHitReactionComponent::PlayHitReaction(const FHitReactionInfo& HitInfo)
{
    if (!AnimInstance)
    {
        return;
    }

    if (UAnimMontage* ReactionMontage = SelectHitReactionMontage(HitInfo))
    {
        AnimInstance->Montage_Play(ReactionMontage);
        
        // Determine direction from hit
        EAttackDirection Direction = GetHitDirectionRelativeToFacing(HitInfo.HitDirection);
        
        // Determine if heavy based on stun duration threshold
        const bool bIsHeavy = (HitInfo.StunDuration > 0.3f);
        
        // Broadcast event
        OnHitReactionStarted.Broadcast(Direction, bIsHeavy);
        
        // Apply hitstun if specified
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
    if (!OwnerCharacter || HitDirection.IsNearlyZero())
    {
        return EAttackDirection::Forward;
    }
    
    // Convert to local space
    FVector LocalDirection = OwnerCharacter->GetActorTransform().InverseTransformVector(HitDirection);
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