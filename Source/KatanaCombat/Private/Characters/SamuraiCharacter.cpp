
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/SamuraiCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/CombatComponentV2.h"
#include "Core/TargetingComponent.h"
#include "Core/WeaponComponent.h"
#include "Core/HitReactionComponent.h"
#include "Debug/CombatDebugWidget.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "ActionQueueTypes.h"
#include "MotionWarpingComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"

ASamuraiCharacter::ASamuraiCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create combat components
    CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
    CombatComponentV2 = CreateDefaultSubobject<UCombatComponentV2>(TEXT("CombatComponentV2"));
    CombatDebugWidget = CreateDefaultSubobject<UCombatDebugWidget>(TEXT("CombatDebugWidget"));
    TargetingComponent = CreateDefaultSubobject<UTargetingComponent>(TEXT("TargetingComponent"));
    WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
    HitReactionComponent = CreateDefaultSubobject<UHitReactionComponent>(TEXT("HitReactionComponent"));
    MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

    // Configure character movement (default for third-person combat)
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    GetCharacterMovement()->MaxWalkSpeed = 600.0f;

    // Don't rotate camera with controller
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
}

void ASamuraiCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Setup Enhanced Input
    if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            if (DefaultMappingContext)
            {
                Subsystem->AddMappingContext(DefaultMappingContext, 0);
            }
        }
    }

    // Bind to weapon hit event for damage processing
    if (WeaponComponent)
    {
        WeaponComponent->OnWeaponHit.AddDynamic(this, &ASamuraiCharacter::OnWeaponHitTarget);
    }
}

void ASamuraiCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ASamuraiCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // Movement
        if (MoveAction)
        {
            EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASamuraiCharacter::Move);
        }

        // Looking
        if (LookAction)
        {
            EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASamuraiCharacter::Look);
        }

        // Light Attack (Started = pressed, Completed = released)
        if (LightAttackAction)
        {
            EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Started, this, &ASamuraiCharacter::OnLightAttackStarted);
            EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Completed, this, &ASamuraiCharacter::OnLightAttackCompleted);
        }

        // Heavy Attack (Started = pressed, Completed = released)
        if (HeavyAttackAction)
        {
            EnhancedInputComponent->BindAction(HeavyAttackAction, ETriggerEvent::Started, this, &ASamuraiCharacter::OnHeavyAttackStarted);
            EnhancedInputComponent->BindAction(HeavyAttackAction, ETriggerEvent::Completed, this, &ASamuraiCharacter::OnHeavyAttackCompleted);
        }

        // Block (Started = pressed, Completed = released)
        if (BlockAction)
        {
            EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Started, this, &ASamuraiCharacter::OnBlockStarted);
            EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Completed, this, &ASamuraiCharacter::OnBlockCompleted);
        }

        // Evade
        if (EvadeAction)
        {
            EnhancedInputComponent->BindAction(EvadeAction, ETriggerEvent::Started, this, &ASamuraiCharacter::OnEvadeStarted);
        }

        // Toggle Debug Overlay
        if (ToggleDebugAction)
        {
            EnhancedInputComponent->BindAction(ToggleDebugAction, ETriggerEvent::Started, this, &ASamuraiCharacter::OnToggleDebug);
        }
    }
}

// ============================================================================
// INPUT HANDLERS
// ============================================================================

void ASamuraiCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D MovementVector = Value.Get<FVector2D>();

    // Forward movement input to combat component for directional attacks
    if (CombatComponent)
    {
        CombatComponent->SetMovementInput(MovementVector);
    }

    if (Controller && !MovementVector.IsZero())
    {
        // Find out which way is forward
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        // Get forward vector
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        
        // Get right vector
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        // Add movement
        AddMovementInput(ForwardDirection, MovementVector.Y);
        AddMovementInput(RightDirection, MovementVector.X);
    }
}

void ASamuraiCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller)
    {
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
}

void ASamuraiCharacter::OnLightAttackStarted(const FInputActionValue& Value)
{
    // Check CombatSettings for V2 system enabled
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        CombatComponentV2->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
    }
    else if (CombatComponent)
    {
        CombatComponent->OnLightAttackPressed();
    }
}

void ASamuraiCharacter::OnLightAttackCompleted(const FInputActionValue& Value)
{
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        CombatComponentV2->OnInputEvent(EInputType::LightAttack, EInputEventType::Release);
    }
    else if (CombatComponent)
    {
        CombatComponent->OnLightAttackReleased();
    }
}

void ASamuraiCharacter::OnHeavyAttackStarted(const FInputActionValue& Value)
{
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        CombatComponentV2->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Press);
    }
    else if (CombatComponent)
    {
        CombatComponent->OnHeavyAttackPressed();
    }
}

void ASamuraiCharacter::OnHeavyAttackCompleted(const FInputActionValue& Value)
{
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        CombatComponentV2->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Release);
    }
    else if (CombatComponent)
    {
        CombatComponent->OnHeavyAttackReleased();
    }
}

void ASamuraiCharacter::OnBlockStarted(const FInputActionValue& Value)
{
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        CombatComponentV2->OnInputEvent(EInputType::Block, EInputEventType::Press);
    }
    else if (CombatComponent)
    {
        CombatComponent->OnBlockPressed();
    }
}

void ASamuraiCharacter::OnBlockCompleted(const FInputActionValue& Value)
{
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        CombatComponentV2->OnInputEvent(EInputType::Block, EInputEventType::Release);
    }
    else if (CombatComponent)
    {
        CombatComponent->OnBlockReleased();
    }
}

void ASamuraiCharacter::OnEvadeStarted(const FInputActionValue& Value)
{
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        CombatComponentV2->OnInputEvent(EInputType::Evade, EInputEventType::Press);
    }
    else if (CombatComponent)
    {
        CombatComponent->OnEvadePressed();
    }
}

void ASamuraiCharacter::OnToggleDebug(const FInputActionValue& Value)
{
    if (CombatDebugWidget)
    {
        CombatDebugWidget->ToggleDebugOverlay();
    }
}

// ============================================================================
// ICombatInterface IMPLEMENTATION
// ============================================================================

bool ASamuraiCharacter::CanPerformAttack_Implementation() const
{
    return CombatComponent ? CombatComponent->CanAttack() : false;
}

ECombatState ASamuraiCharacter::GetCombatState_Implementation() const
{
    return CombatComponent ? CombatComponent->GetCombatState() : ECombatState::Idle;
}

bool ASamuraiCharacter::IsAttacking_Implementation() const
{
    return CombatComponent ? CombatComponent->IsAttacking() : false;
}

UAttackData* ASamuraiCharacter::GetCurrentAttack_Implementation() const
{
    return CombatComponent ? CombatComponent->GetCurrentAttack() : nullptr;
}

EAttackPhase ASamuraiCharacter::GetCurrentPhase_Implementation() const
{
    return CombatComponent ? CombatComponent->GetCurrentPhase() : EAttackPhase::None;
}

void ASamuraiCharacter::OnEnableHitDetection_Implementation()
{
    if (WeaponComponent)
    {
        WeaponComponent->EnableHitDetection();
    }
}

void ASamuraiCharacter::OnDisableHitDetection_Implementation()
{
    if (WeaponComponent)
    {
        WeaponComponent->DisableHitDetection();
    }
}

void ASamuraiCharacter::OnAttackPhaseBegin_Implementation(EAttackPhase Phase)
{
    if (CombatComponent)
    {
        CombatComponent->OnAttackPhaseBegin(Phase);
    }
}

void ASamuraiCharacter::OnAttackPhaseEnd_Implementation(EAttackPhase Phase)
{
    if (CombatComponent)
    {
        CombatComponent->OnAttackPhaseEnd(Phase);
    }
}

void ASamuraiCharacter::OnAttackPhaseTransition_Implementation(EAttackPhase NewPhase)
{
    // Forward to V1 system (always runs for compatibility)
    if (CombatComponent)
    {
        CombatComponent->OnAttackPhaseTransition(NewPhase);
    }

    // Also forward to V2 if enabled
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        CombatComponentV2->OnPhaseTransition(NewPhase);
    }
}

bool ASamuraiCharacter::IsInParryWindow_Implementation() const
{
    return CombatComponent ? CombatComponent->IsInParryWindow() : false;
}

void ASamuraiCharacter::OnHoldWindowStart_Implementation(EInputType InputType)
{
    // V2-only feature - forward to V2 system if enabled
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        CombatComponentV2->OnHoldWindowStart(InputType);
    }
}

// ============================================================================
// IDamageableInterface IMPLEMENTATION
// ============================================================================

float ASamuraiCharacter::ApplyDamage_Implementation(const FHitReactionInfo& HitInfo)
{
    if (!HitReactionComponent)
    {
        return 0.0f;
    }

    // Check if blocking
    if (CombatComponent && CombatComponent->IsBlocking())
    {
        // Apply posture damage instead of health damage
        const float PostureDamage = HitInfo.AttackData ? HitInfo.AttackData->PostureDamage : 10.0f;
        
        if (CombatComponent->ApplyPostureDamage(PostureDamage))
        {
            // Guard was broken - now take the damage
            return HitReactionComponent->ApplyDamage(HitInfo);
        }
        
        // Successfully blocked
        return 0.0f;
    }

    // Not blocking - take full damage
    return HitReactionComponent->ApplyDamage(HitInfo);
}

bool ASamuraiCharacter::ApplyPostureDamage_Implementation(float PostureDamage, AActor* Attacker)
{
    if (CombatComponent)
    {
        return CombatComponent->ApplyPostureDamage(PostureDamage);
    }
    
    return false;
}

bool ASamuraiCharacter::CanBeDamaged_Implementation() const
{
    return HitReactionComponent ? HitReactionComponent->CanBeDamaged() : true;
}

bool ASamuraiCharacter::IsBlocking_Implementation() const
{
    return CombatComponent ? CombatComponent->IsBlocking() : false;
}

bool ASamuraiCharacter::IsGuardBroken_Implementation() const
{
    return CombatComponent ? CombatComponent->IsGuardBroken() : false;
}

bool ASamuraiCharacter::ExecuteFinisher_Implementation(AActor* Attacker, UAttackData* FinisherData)
{
    if (!HitReactionComponent || !FinisherData)
    {
        return false;
    }

    // Check if we're in a finishable state (guard broken or stunned)
    if (!IsGuardBroken_Implementation() && (!HitReactionComponent || !HitReactionComponent->IsStunned()))
    {
        return false;
    }

    // Play victim animation
    // Note: FinisherData should have a name that matches the finisher animation map
    const FName FinisherName = FinisherData->MontageSection; // Use section name as finisher ID
    return HitReactionComponent->PlayFinisherVictimAnimation(FinisherName);
}

void ASamuraiCharacter::OnAttackParried_Implementation(AActor* Parrier)
{
    // Stop current attack
    if (CombatComponent)
    {
        CombatComponent->StopCurrentAttack();
    }

    // Open counter window (vulnerable to counter attacks)
    if (CombatComponent)
    {
        CombatComponent->OpenCounterWindow(1.5f); // Duration from CombatSettings
    }

    // Play parried reaction animation
if (HitReactionComponent)
    {
        HitReactionComponent->PlayGuardBrokenReaction();
    }
}

void ASamuraiCharacter::OpenCounterWindow_Implementation(float Duration)
{
    if (CombatComponent)
    {
        CombatComponent->OpenCounterWindow(Duration);
    }
}

float ASamuraiCharacter::GetCurrentPosture_Implementation() const
{
    return CombatComponent ? CombatComponent->GetCurrentPosture() : 0.0f;
}

float ASamuraiCharacter::GetMaxPosture_Implementation() const
{
    return CombatComponent ? CombatComponent->GetMaxPosture() : 100.0f;
}

bool ASamuraiCharacter::IsInCounterWindow_Implementation() const
{
    return CombatComponent ? CombatComponent->IsInCounterWindow() : false;
}

// ============================================================================
// WEAPON HIT PROCESSING
// ============================================================================


void ASamuraiCharacter::OnWeaponHitTarget(AActor* HitActor, const FHitResult& HitResult, UAttackData* AttackData)
{ 
    if (!HitActor || !AttackData)
    {
        return;
    }

    // Check if target implements IDamageableInterface
    if (HitActor->Implements<UDamageableInterface>())
    {

        
        // Build hit reaction info
        FHitReactionInfo HitInfo;
        HitInfo.Attacker = this;
        HitInfo.HitDirection = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        HitInfo.AttackData = AttackData;
        HitInfo.Damage = AttackData->BaseDamage;
        HitInfo.StunDuration = AttackData->HitStunDuration;
        HitInfo.bWasCounter = CombatComponent ? CombatComponent->IsInCounterWindow() : false;
        HitInfo.ImpactPoint = HitResult.ImpactPoint;

        // Apply counter damage multiplier if applicable
        if (HitInfo.bWasCounter && IDamageableInterface::Execute_IsInCounterWindow(HitActor))
        {
            HitInfo.Damage *= AttackData->CounterDamageMultiplier;
        }

        // Apply damage via interface
        const float DamageDealt = IDamageableInterface::Execute_ApplyDamage(HitActor, HitInfo);

        // Broadcast hit event
        if (CombatComponent)
        {
            CombatComponent->OnAttackHit.Broadcast(HitActor, DamageDealt);
        }
    }
}