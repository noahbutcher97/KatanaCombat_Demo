// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/SamuraiAnimInstance.h"
// V1 REMOVED: #include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
#include "Interfaces/CombatInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void USamuraiAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    // Cache owner references
    OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());

    if (OwnerCharacter)
    {
        MovementComponent = OwnerCharacter->GetCharacterMovement();
        // V1 REMOVED: CombatComponent = OwnerCharacter->FindComponentByClass<UCombatComponent>();
        HitReactionComponent = OwnerCharacter->FindComponentByClass<UHitReactionComponent>();
    }
}

void USamuraiAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
    Super::NativeUpdateAnimation(DeltaTime);

    // Early exit if references not set
    if (!OwnerCharacter || !MovementComponent)
    {
        return;
    }

    // Update all animation variables
    UpdateAnimationVariables();
}

// ============================================================================
// UPDATE ANIMATION VARIABLES
// ============================================================================

void USamuraiAnimInstance::UpdateAnimationVariables()
{
    UpdateMovement();
    UpdateCombatState();
    UpdateCombo();
    UpdatePosture();
    UpdateCharge();
    UpdateHitReaction();
}

void USamuraiAnimInstance::UpdateMovement()
{
    if (!MovementComponent)
    {
        return;
    }

    // CRITICAL: Don't update locomotion during hold state
    // Prevents animation state machine from reacting to velocity changes
    // while montage is frozen at 0.0 playrate, which causes animation conflicts
    if (bIsHoldingAttack)
    {
        // Keep current Speed/Direction frozen
        // This prevents the state machine from trying to transition to idle/locomotion
        // when player adds movement input during hold
        return;
    }

    // Get velocity and speed
    const FVector Velocity = MovementComponent->Velocity;
    Speed = Velocity.Size2D();

    // Calculate direction relative to character facing
    if (Speed > 0.0f)
    {
        // Get movement direction in world space
        const FVector VelocityNormal = Velocity.GetSafeNormal2D();
        
        // Get character forward and right vectors
        const FVector Forward = OwnerCharacter->GetActorForwardVector();
        const FVector Right = OwnerCharacter->GetActorRightVector();
        
        // Calculate dot products to determine direction
        const float ForwardDot = FVector::DotProduct(VelocityNormal, Forward);
        const float RightDot = FVector::DotProduct(VelocityNormal, Right);
        
        // Convert to angle (-180 to 180 degrees)
        // Atan2 gives us the angle, then convert to degrees
        Direction = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
    }
    else
    {
        Direction = 0.0f;
    }

    // Check if in air
    bIsInAir = MovementComponent->IsFalling();

    // Determine if in combat stance (attacking or recently attacked)
    bIsInCombat = bIsAttacking || bIsBlocking || bIsGuardBroken;
}

void USamuraiAnimInstance::UpdateCombatState()
{
    // V1 REMOVED: Combat state updates not yet migrated to V2
    // if (!CombatComponent)
    // {
        CombatState = ECombatState::Idle;
        CurrentPhase = EAttackPhase::None;
        bIsAttacking = false;
        bIsBlocking = false;
        bIsGuardBroken = false;
        bIsHoldingAttack = false;
        return;
    // }

    // V1 REMOVED: Sync state from combat component
    // CombatState = CombatComponent->GetCombatState();
    // CurrentPhase = CombatComponent->GetCurrentPhase();
    // bIsAttacking = CombatComponent->IsAttacking();
    // bIsBlocking = CombatComponent->IsBlocking();
    // bIsGuardBroken = CombatComponent->IsGuardBroken();
    // bIsHoldingAttack = CombatComponent->IsHolding();
}

void USamuraiAnimInstance::UpdateCombo()
{
    // V1 REMOVED: Combo updates not yet migrated to V2
    // if (!CombatComponent)
    // {
        ComboCount = 0;
        bCanCombo = false;
        return;
    // }

    // V1 REMOVED
    // ComboCount = CombatComponent->GetComboCount();
    // bCanCombo = CombatComponent->CanCombo();
}

void USamuraiAnimInstance::UpdatePosture()
{
    // V1 REMOVED: Posture updates not yet migrated to V2
    // if (!CombatComponent)
    // {
        PosturePercent = 1.0f;
        bIsPostureLow = false;
        return;
    // }

    // V1 REMOVED
    // PosturePercent = CombatComponent->GetPosturePercent();
    // bIsPostureLow = (PosturePercent < 0.4f);
}

void USamuraiAnimInstance::UpdateCharge()
{
    // V1 REMOVED: Charge updates not yet migrated to V2
    // if (!CombatComponent)
    // {
        ChargePercent = 0.0f;
        bIsCharging = false;
        return;
    // }

    // V1 REMOVED: Check if in charging state
    // bIsCharging = (CombatState == ECombatState::ChargingHeavyAttack);

    // V1 REMOVED: Charge calculation
    // if (bIsCharging)
    // {
    //     ChargePercent = 0.5f; // Placeholder - add precise tracking if needed
    // }
    // else
    // {
    //     ChargePercent = 0.0f;
    // }
}

void USamuraiAnimInstance::UpdateHitReaction()
{
    if (!HitReactionComponent)
    {
        bIsStunned = false;
        HitIntensity = 0.0f;
        return;
    }

    bIsStunned = HitReactionComponent->IsStunned();

    // Calculate hit intensity from remaining stun time
    if (bIsStunned)
    {
        const float RemainingStun = HitReactionComponent->GetRemainingStunTime();
        // Intensity decreases as stun time runs out (for blend out)
        HitIntensity = FMath::Clamp(RemainingStun / 0.5f, 0.0f, 1.0f);
    }
    else
    {
        HitIntensity = 0.0f;
    }
}

// ============================================================================
// ANIMNOTIFY ROUTING
// ============================================================================

void USamuraiAnimInstance::OnAttackPhaseBegin(EAttackPhase Phase)
{
    // Route to ICombatInterface on owner
    if (OwnerCharacter && OwnerCharacter->Implements<UCombatInterface>())
    {
        ICombatInterface::Execute_OnAttackPhaseBegin(OwnerCharacter, Phase);
    }
}

void USamuraiAnimInstance::OnAttackPhaseEnd(EAttackPhase Phase)
{
    // Route to ICombatInterface on owner
    if (OwnerCharacter && OwnerCharacter->Implements<UCombatInterface>())
    {
        ICombatInterface::Execute_OnAttackPhaseEnd(OwnerCharacter, Phase);
    }
}

void USamuraiAnimInstance::OnAttackPhaseTransition(EAttackPhase NewPhase)
{
    // Route to ICombatInterface on owner
    if (OwnerCharacter && OwnerCharacter->Implements<UCombatInterface>())
    {
        ICombatInterface::Execute_OnAttackPhaseTransition(OwnerCharacter, NewPhase);
    }
}

void USamuraiAnimInstance::OnComboWindowOpened(float Duration)
{
    // V1 REMOVED: Combo window routing not yet migrated to V2
    // if (CombatComponent)
    // {
    //     CombatComponent->OpenComboWindow(Duration);
    // }
}

void USamuraiAnimInstance::OnComboWindowClosed()
{
    // V1 REMOVED: Combo window routing not yet migrated to V2
    // if (CombatComponent)
    // {
    //     CombatComponent->CloseComboWindow();
    // }
}

void USamuraiAnimInstance::OnEnableHitDetection()
{
    // Route to ICombatInterface on owner
    if (OwnerCharacter && OwnerCharacter->Implements<UCombatInterface>())
    {
        ICombatInterface::Execute_OnEnableHitDetection(OwnerCharacter);
    }
}

void USamuraiAnimInstance::OnDisableHitDetection()
{
    // Route to ICombatInterface on owner
    if (OwnerCharacter && OwnerCharacter->Implements<UCombatInterface>())
    {
        ICombatInterface::Execute_OnDisableHitDetection(OwnerCharacter);
    }
}