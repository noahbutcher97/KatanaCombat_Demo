// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Core/WeaponComponent.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "MotionWarpingComponent.h"

UCombatComponent::UCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    
    CurrentState = ECombatState::Idle;
    CurrentPhase = EAttackPhase::None;
    CurrentAttackData = nullptr;
    
    CurrentPosture = 100.0f;
    ComboCount = 0;
    bCanCombo = false;
    bIsInCounterWindow = false;
    
    bLightAttackBuffered = false;
    bHeavyAttackBuffered = false;
    bEvadeBuffered = false;
    bLightAttackHeld = false;
    bHeavyAttackHeld = false;
    
    bIsCharging = false;
    CurrentChargeTime = 0.0f;
    bIsHolding = false;
    CurrentHoldTime = 0.0f;
    
    bDebugDraw = false;
    CombatSettings = nullptr;
    DefaultLightAttack = nullptr;
    DefaultHeavyAttack = nullptr;
}

void UCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    
    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter)
    {
        AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
        TargetingComponent = OwnerCharacter->FindComponentByClass<UTargetingComponent>();
        MotionWarpingComponent = OwnerCharacter->FindComponentByClass<UMotionWarpingComponent>();
        WeaponComponent = OwnerCharacter->FindComponentByClass<UWeaponComponent>();
    }
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    RegeneratePosture(DeltaTime);
    
    if (bIsCharging)
    {
        UpdateHeavyCharging(DeltaTime);
    }
    
    if (bIsHolding)
    {
        UpdateLightHold(DeltaTime);
    }
}

// ============================================================================
// STATE MACHINE
// ============================================================================

bool UCombatComponent::CanTransitionTo(ECombatState NewState) const
{
    // Define valid state transitions
    switch (CurrentState)
    {
        case ECombatState::Idle:
            return true; // Can transition anywhere from idle
            
        case ECombatState::Attacking:
            return NewState == ECombatState::Evading || 
                   NewState == ECombatState::Idle ||
                   NewState == ECombatState::HitStunned;
            
        case ECombatState::Blocking:
            return NewState != ECombatState::Attacking;
            
        case ECombatState::GuardBroken:
        case ECombatState::HitStunned:
            return NewState == ECombatState::Idle;
            
        default:
            return true;
    }
}

void UCombatComponent::SetCombatState(ECombatState NewState)
{
    if (CurrentState == NewState)
    {
        return;
    }
    
    if (!CanTransitionTo(NewState))
    {
        return;
    }
    
    CurrentState = NewState;
    OnCombatStateChanged.Broadcast(NewState);
}

bool UCombatComponent::IsAttacking() const
{
    return CurrentState == ECombatState::Attacking ||
           CurrentState == ECombatState::HoldingLightAttack ||
           CurrentState == ECombatState::ChargingHeavyAttack;
}

// ============================================================================
// ATTACK EXECUTION
// ============================================================================

bool UCombatComponent::ExecuteAttack(UAttackData* AttackData)
{
    if (!AttackData || !CanAttack())
    {
        return false;
    }
    
    return PlayAttackMontage(AttackData);
}

bool UCombatComponent::CanAttack() const
{
    return CurrentState == ECombatState::Idle;
}

void UCombatComponent::StopCurrentAttack()
{
    if (AnimInstance && CurrentAttackData && CurrentAttackData->AttackMontage)
    {
        AnimInstance->Montage_Stop(0.2f, CurrentAttackData->AttackMontage);
    }
    
    SetCombatState(ECombatState::Idle);
    CurrentAttackData = nullptr;
}

// ============================================================================
// COMBO SYSTEM
// ============================================================================

void UCombatComponent::ResetCombo()
{
    ComboCount = 0;
    bCanCombo = false;
    
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ComboResetTimer);
    }
}

void UCombatComponent::OpenComboWindow(float Duration)
{
    bCanCombo = true;
    
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(ComboResetTimer, this, &UCombatComponent::CloseComboWindow, Duration, false);
    }
}

void UCombatComponent::CloseComboWindow()
{
    bCanCombo = false;
}

// ============================================================================
// POSTURE SYSTEM
// ============================================================================

float UCombatComponent::GetMaxPosture() const
{
    return CombatSettings ? CombatSettings->MaxPosture : 100.0f;
}

float UCombatComponent::GetPosturePercent() const
{
    const float MaxPosture = GetMaxPosture();
    return (MaxPosture > 0.0f) ? (CurrentPosture / MaxPosture) : 0.0f;
}

bool UCombatComponent::ApplyPostureDamage(float Amount)
{
    if (Amount <= 0.0f)
    {
        return false;
    }
    
    CurrentPosture = FMath::Max(0.0f, CurrentPosture - Amount);
    OnPostureChanged.Broadcast(CurrentPosture);
    
    if (CurrentPosture <= 0.0f)
    {
        TriggerGuardBreak();
        return true;
    }
    
    return false;
}

void UCombatComponent::TriggerGuardBreak()
{
    SetCombatState(ECombatState::GuardBroken);
    OnGuardBroken.Broadcast();
    
    const float RecoveryDuration = CombatSettings ? CombatSettings->GuardBreakStunDuration : 2.0f;
    
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(GuardBreakRecoveryTimer, this, &UCombatComponent::RecoverFromGuardBreak, RecoveryDuration, false);
    }
}

void UCombatComponent::RegeneratePosture(float DeltaTime)
{
    if (CurrentPosture >= GetMaxPosture())
    {
        return;
    }
    
    const float RegenRate = GetCurrentPostureRegenRate();
    CurrentPosture = FMath::Min(GetMaxPosture(), CurrentPosture + (RegenRate * DeltaTime));
    OnPostureChanged.Broadcast(CurrentPosture);
}

float UCombatComponent::GetCurrentPostureRegenRate() const
{
    if (!CombatSettings)
    {
        return 20.0f;
    }
    
    switch (CurrentState)
    {
        case ECombatState::Attacking:
        case ECombatState::HoldingLightAttack:
        case ECombatState::ChargingHeavyAttack:
            return CombatSettings->PostureRegenRate_Attacking;
            
        case ECombatState::Blocking:
            return 0.0f;
            
        case ECombatState::Idle:
            return CombatSettings->PostureRegenRate_Idle;
            
        default:
            return CombatSettings->PostureRegenRate_NotBlocking;
    }
}

void UCombatComponent::RecoverFromGuardBreak()
{
    const float RecoveryPercent = CombatSettings ? CombatSettings->GuardBreakRecoveryPercent : 0.5f;
    CurrentPosture = GetMaxPosture() * RecoveryPercent;
    
    SetCombatState(ECombatState::Idle);
    OnPostureChanged.Broadcast(CurrentPosture);
}

// ============================================================================
// BLOCKING & PARRY
// ============================================================================

bool UCombatComponent::CanBlock() const
{
    return CurrentState == ECombatState::Idle;
}

void UCombatComponent::StartBlocking()
{
    if (CanBlock())
    {
        SetCombatState(ECombatState::Blocking);
    }
}

void UCombatComponent::StopBlocking()
{
    if (CurrentState == ECombatState::Blocking)
    {
        SetCombatState(ECombatState::Idle);
    }
}

bool UCombatComponent::TryParry()
{
    if (CurrentState != ECombatState::Blocking)
    {
        return false;
    }
    
    // Parry logic would check timing against incoming attack
    // For now, just trigger the state and event
    SetCombatState(ECombatState::Parrying);
    
    // Reset to idle after brief parry window
    if (GetWorld())
    {
        FTimerHandle ParryTimer;
        GetWorld()->GetTimerManager().SetTimer(ParryTimer, [this]()
        {
            SetCombatState(ECombatState::Idle);
        }, 0.3f, false);
    }
    
    return true;
}

// ============================================================================
// COUNTER WINDOWS
// ============================================================================

void UCombatComponent::OpenCounterWindow(float Duration)
{
    bIsInCounterWindow = true;
    
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(CounterWindowTimer, this, &UCombatComponent::CloseCounterWindow, Duration, false);
    }
}

void UCombatComponent::CloseCounterWindow()
{
    bIsInCounterWindow = false;
    
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(CounterWindowTimer);
    }
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

void UCombatComponent::OnLightAttackPressed()
{
    bLightAttackHeld = true;
    
    if (CanAttack())
    {
        if (DefaultLightAttack)
        {
            ExecuteAttack(DefaultLightAttack);
        }
    }
    else if (bCanCombo && CurrentAttackData && CurrentAttackData->NextComboAttack)
    {
        ExecuteComboAttack(CurrentAttackData->NextComboAttack);
    }
    else
    {
        bLightAttackBuffered = true;
    }
}

void UCombatComponent::OnLightAttackReleased()
{
    bLightAttackHeld = false;
    
    if (bIsHolding)
    {
        ReleaseHeldLight();
    }
}

void UCombatComponent::OnHeavyAttackPressed()
{
    bHeavyAttackHeld = true;
    
    if (CanAttack())
    {
        if (DefaultHeavyAttack)
        {
            bIsCharging = true;
            CurrentChargeTime = 0.0f;
            SetCombatState(ECombatState::ChargingHeavyAttack);
            CurrentAttackData = DefaultHeavyAttack;
            
            if (AnimInstance && DefaultHeavyAttack->AttackMontage)
            {
                AnimInstance->Montage_Play(DefaultHeavyAttack->AttackMontage, DefaultHeavyAttack->ChargeTimeScale);
            }
        }
    }
    else if (bCanCombo && CurrentAttackData && CurrentAttackData->HeavyComboAttack)
    {
        ExecuteComboAttack(CurrentAttackData->HeavyComboAttack);
    }
    else
    {
        bHeavyAttackBuffered = true;
    }
}

void UCombatComponent::OnHeavyAttackReleased()
{
    bHeavyAttackHeld = false;
    
    if (bIsCharging)
    {
        ReleaseChargedHeavy();
    }
}

void UCombatComponent::OnBlockPressed()
{
    StartBlocking();
}

void UCombatComponent::OnBlockReleased()
{
    StopBlocking();
}

void UCombatComponent::OnEvadePressed()
{
    if (CurrentState == ECombatState::Idle || CurrentState == ECombatState::Blocking)
    {
        SetCombatState(ECombatState::Evading);
        
        // Reset after evade duration
        if (GetWorld())
        {
            FTimerHandle EvadeTimer;
            GetWorld()->GetTimerManager().SetTimer(EvadeTimer, [this]()
            {
                SetCombatState(ECombatState::Idle);
            }, 0.5f, false);
        }
    }
}

// ============================================================================
// ATTACK PHASE CALLBACKS
// ============================================================================

void UCombatComponent::OnAttackPhaseBegin(EAttackPhase Phase)
{
    CurrentPhase = Phase;
    
    switch (Phase)
    {
        case EAttackPhase::Active:
            if (WeaponComponent)
            {
                WeaponComponent->EnableHitDetection();
            }
            break;
            
        case EAttackPhase::Hold:
            if (bLightAttackHeld && CurrentAttackData && CurrentAttackData->bCanHoldAtEnd)
            {
                bIsHolding = true;
                CurrentHoldTime = 0.0f;
                SetCombatState(ECombatState::HoldingLightAttack);
                
                if (AnimInstance && CurrentAttackData->AttackMontage)
                {
                    AnimInstance->Montage_Pause(CurrentAttackData->AttackMontage);
                }
            }
            break;
            
        case EAttackPhase::Recovery:
            OpenComboWindow(CombatSettings ? CombatSettings->ComboInputWindow : 0.6f);
            break;
            
        default:
            break;
    }
}

void UCombatComponent::OnAttackPhaseEnd(EAttackPhase Phase)
{
    switch (Phase)
    {
        case EAttackPhase::Active:
            if (WeaponComponent)
            {
                WeaponComponent->DisableHitDetection();
            }
            break;
            
        case EAttackPhase::Recovery:
            SetCombatState(ECombatState::Idle);
            ProcessBufferedInputs();
            break;
            
        default:
            break;
    }
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

void UCombatComponent::HandleComboInput()
{
    ProcessBufferedInputs();
}

void UCombatComponent::ExecuteComboAttack(UAttackData* NextAttack)
{
    if (!NextAttack)
    {
        return;
    }
    
    ComboCount++;
    bCanCombo = false;
    CurrentAttackData = NextAttack;
    
    PlayAttackMontage(NextAttack);
}

void UCombatComponent::ExecuteDirectionalFollowUp(EAttackDirection Direction)
{
    if (!CurrentAttackData)
    {
        return;
    }
    
    UAttackData* FollowUp = CurrentAttackData->DirectionalFollowUps.FindRef(Direction);
    
    if (FollowUp)
    {
        CurrentAttackData = FollowUp;
        PlayAttackMontage(FollowUp);
    }
}

void UCombatComponent::ProcessBufferedInputs()
{
    if (bEvadeBuffered)
    {
        bEvadeBuffered = false;
        OnEvadePressed();
    }
    else if (bLightAttackBuffered)
    {
        bLightAttackBuffered = false;
        OnLightAttackPressed();
    }
    else if (bHeavyAttackBuffered)
    {
        bHeavyAttackBuffered = false;
        OnHeavyAttackPressed();
    }
}

void UCombatComponent::ClearInputBuffers()
{
    bLightAttackBuffered = false;
    bHeavyAttackBuffered = false;
    bEvadeBuffered = false;
}

void UCombatComponent::SetupMotionWarping(UAttackData* AttackData)
{
    if (!MotionWarpingComponent || !AttackData || !AttackData->MotionWarpingConfig.bUseMotionWarping || !TargetingComponent)
    {
        return;
    }
    
    AActor* Target = TargetingComponent->GetCurrentTarget();
    if (!Target)
    {
        return;
    }
    
    const FVector TargetLocation = Target->GetActorLocation();
    const FRotator LookAtRotation = (TargetLocation - OwnerCharacter->GetActorLocation()).Rotation();
    
    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
        AttackData->MotionWarpingConfig.MotionWarpingTargetName,
        TargetLocation,
        LookAtRotation
    );
}


bool UCombatComponent::PlayAttackMontage(UAttackData* AttackData)
{
    if (!AttackData || !AnimInstance || !AttackData->AttackMontage)
    {
        return false;
    }
    
    CurrentAttackData = AttackData;
    SetCombatState(ECombatState::Attacking);
    
    if (WeaponComponent)
    {
        WeaponComponent->ResetHitActors();
    }
    
    SetupMotionWarping(AttackData);
    
    AnimInstance->Montage_Play(AttackData->AttackMontage);
    
    if (AttackData->MontageSection != NAME_None && AttackData->bJumpToSectionStart)
    {
        AnimInstance->Montage_JumpToSection(AttackData->MontageSection, AttackData->AttackMontage);
    }
    
    return true;
}

void UCombatComponent::ResetComboChain()
{
    ResetCombo();
}

void UCombatComponent::UpdateHeavyCharging(float DeltaTime)
{
    if (!CurrentAttackData)
    {
        return;
    }
    
    CurrentChargeTime += DeltaTime;
    
    if (CurrentChargeTime >= CurrentAttackData->MaxChargeTime)
    {
        ReleaseChargedHeavy();
    }
}

void UCombatComponent::ReleaseChargedHeavy()
{
    if (!bIsCharging)
    {
        return;
    }
    
    bIsCharging = false;
    SetCombatState(ECombatState::Attacking);
    
    if (AnimInstance && CurrentAttackData && CurrentAttackData->AttackMontage)
    {
        AnimInstance->Montage_SetPlayRate(CurrentAttackData->AttackMontage, 1.0f);
    }
}

void UCombatComponent::UpdateLightHold(float DeltaTime)
{
    if (!CurrentAttackData)
    {
        return;
    }
    
    CurrentHoldTime += DeltaTime;
    
    if (CurrentAttackData->bEnforceMaxHoldTime && CurrentHoldTime >= CurrentAttackData->MaxHoldTime)
    {
        ReleaseHeldLight();
    }
}

void UCombatComponent::ReleaseHeldLight()
{
    if (!bIsHolding)
    {
        return;
    }
    
    bIsHolding = false;
    
    if (AnimInstance && CurrentAttackData && CurrentAttackData->AttackMontage)
    {
        AnimInstance->Montage_Resume(CurrentAttackData->AttackMontage);
    }
    
    // Determine direction from stored movement input
    EAttackDirection InputDirection = EAttackDirection::Forward; // Default
    
    if (TargetingComponent)
    {
        if (!StoredMovementInput.IsNearlyZero())
        {
            // Convert 2D input to world direction and get attack direction
            const FVector WorldInput = GetWorldSpaceMovementInput();
            InputDirection = TargetingComponent->GetAttackDirectionFromInput(WorldInput);
        }
        // else: no input means forward attack (default)
    }
    
    // Execute directional follow-up
    ExecuteDirectionalFollowUp(InputDirection);
}

void UCombatComponent::SetMovementInput(FVector2D Input)
{
    StoredMovementInput = Input;
}


FVector UCombatComponent::GetWorldSpaceMovementInput() const
{
    if (!OwnerCharacter || StoredMovementInput.IsNearlyZero())
    {
        return FVector::ZeroVector;
    }
    
    const APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
    if (!PC)
    {
        return OwnerCharacter->GetActorForwardVector();
    }
    
    FRotator ControlRotation = PC->GetControlRotation();
    ControlRotation.Pitch = 0.0f;
    ControlRotation.Roll = 0.0f;
    
    const FVector Forward = FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::Y);
    
    FVector WorldInput = (Forward * StoredMovementInput.Y) + (Right * StoredMovementInput.X);
    WorldInput.Normalize();
    
    return WorldInput;
}