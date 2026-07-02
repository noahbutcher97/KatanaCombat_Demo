// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/SamuraiAnimInstance.h"
#include "Core/HitReactionComponent.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/DamageableInterface.h"
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
    if (!OwnerCharacter || !OwnerCharacter->Implements<UCombatInterface>())
    {
        CombatState = ECombatState::Idle;
        CurrentPhase = EAttackPhase::None;
        bIsAttacking = false;
        bIsBlocking = false;
        bIsGuardBroken = false;
        bIsHoldingAttack = false;
        return;
    }

    CombatState = ICombatInterface::Execute_GetCombatState(OwnerCharacter);
    CurrentPhase = ICombatInterface::Execute_GetCurrentPhase(OwnerCharacter);
    bIsAttacking = ICombatInterface::Execute_IsAttacking(OwnerCharacter);
    bIsBlocking = OwnerCharacter->Implements<UDamageableInterface>() &&
        IDamageableInterface::Execute_IsBlocking(OwnerCharacter);
    bIsGuardBroken = OwnerCharacter->Implements<UDamageableInterface>() &&
        IDamageableInterface::Execute_IsGuardBroken(OwnerCharacter);
    bIsHoldingAttack = CombatState == ECombatState::HoldingLightAttack ||
        CombatState == ECombatState::ChargingHeavyAttack;
}

void USamuraiAnimInstance::UpdateCombo()
{
    // TODO: Connect to CombatComponent when interface is ready
    ComboCount = 0;
    bCanCombo = false;
}

void USamuraiAnimInstance::UpdatePosture()
{
    // DEPRECATED: Posture system removed. Keep defaults for backwards compat.
    PosturePercent = 1.0f;
    bIsPostureLow = false;

    // Update stagger state from HitReactionComponent
    bIsStaggered = HitReactionComponent ? HitReactionComponent->IsStaggered() : false;
}

void USamuraiAnimInstance::UpdateCharge()
{
    // TODO: Connect to CombatComponent hold state when interface is ready
    ChargePercent = 0.0f;
    bIsCharging = false;
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
    // Combo windows are now inferred from phase transitions
    // (see AnimNotify_AttackPhaseTransition and DiscoverCheckpoints)
}

void USamuraiAnimInstance::OnComboWindowClosed()
{
    // Combo windows are now inferred from phase transitions
    // (see AnimNotify_AttackPhaseTransition and DiscoverCheckpoints)
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
