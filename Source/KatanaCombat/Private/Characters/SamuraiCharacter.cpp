
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/SamuraiCharacter.h"
// V1 REMOVED: #include "Core/CombatComponent.h" - V1 CombatComponent fully deprecated
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
#include "Utilities/DirectionDebugLibrary.h"

ASamuraiCharacter::ASamuraiCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create combat components
    // V1 REMOVED: CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
    CombatComponentV2 = CreateDefaultSubobject<UCombatComponentV2>(TEXT("CombatComponentV2"));
    CombatDebugWidget = CreateDefaultSubobject<UCombatDebugWidget>(TEXT("CombatDebugWidget"));
    TargetingComponent = CreateDefaultSubobject<UTargetingComponent>(TEXT("TargetingComponent"));
    WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
    HitReactionComponent = CreateDefaultSubobject<UHitReactionComponent>(TEXT("HitReactionComponent"));
    MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

    // Configure character movement (default for third-person combat)
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 180, 0.0f);
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
     // Debug Visualization: Run on Tick to update arrows in real-time
    if (CombatComponentV2 && CombatComponentV2->GetDebugDraw())
    {
        // 1. Gather Transformations
        const FRotator CameraRotation = GetControlRotation();
        const FRotator CharacterRotation = GetActorRotation();
        const FVector2D CameraRelativeInput = LastMovementInput;

        // Camera-relative to world space conversion
        // CRITICAL: Flatten camera rotation to yaw-only to prevent pitch/roll from corrupting WorldInput
        const FRotator FlatCameraRotation = FRotator(0.0f, CameraRotation.Yaw, 0.0f);
        const FVector CameraForward = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::X);
        const FVector CameraRight = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::Y);
        FVector WorldInput = (CameraRight * CameraRelativeInput.X) + (CameraForward * CameraRelativeInput.Y);
        WorldInput.Z = 0.0f;
        WorldInput.Normalize();

        // World to character-relative conversion
        const FRotator InverseCharacterYaw(0.0f, -CharacterRotation.Yaw, 0.0f);
        const FVector CharacterRelativeVec = InverseCharacterYaw.RotateVector(WorldInput);
        const FVector2D CharacterRelative2D(CharacterRelativeVec.X, CharacterRelativeVec.Y);

        // 2. Resolve Direction Enum
        const EInputDirection CharacterRelativeDirection = GetDirectionalInputFromMovement(CharacterRelative2D);

        // 3. Draw Debug if we have valid input
        if (CharacterRelativeDirection != EInputDirection::None)
        {
            // Simple mapping for AttackDir (Forward/Back/Left/Right)
            EAttackDirection AttackDir = EAttackDirection::Forward;
            if (CharacterRelativeDirection == EInputDirection::Backward || 
                CharacterRelativeDirection == EInputDirection::BackwardLeft || 
                CharacterRelativeDirection == EInputDirection::BackwardRight)
            {
                AttackDir = EAttackDirection::Backward;
            }
            else if (CharacterRelativeDirection == EInputDirection::Left)
            {
                AttackDir = EAttackDirection::Left;
            }
            else if (CharacterRelativeDirection == EInputDirection::Right)
            {
                AttackDir = EAttackDirection::Right;
            }

            const FVector CharacterLocation = GetActorLocation();
            if (!CharacterLocation.IsNearlyZero(1.0f))  // Validate not at world origin
            {
                UDirectionDebugLibrary::DrawDirectionTransformDebug(
                    GetWorld(),
                    this,  // Pass character reference for text anchoring
                    CharacterLocation,
                    CameraRotation,
                    CharacterRotation,
                    CameraRelativeInput,
                    WorldInput,
                    CharacterRelativeVec,
                    CharacterRelativeDirection,
                    AttackDir,
                    CombatComponentV2->IsHolding()
                );
            }
        }
    }
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
            EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Started, this, &ASamuraiCharacter::OnLightAttackPressed);
            EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Completed, this, &ASamuraiCharacter::OnLightAttackReleased);
        }

        // Heavy Attack (Started = pressed, Completed = released)
        if (HeavyAttackAction)
        {
            EnhancedInputComponent->BindAction(HeavyAttackAction, ETriggerEvent::Started, this, &ASamuraiCharacter::OnHeavyAttackPressed);
            EnhancedInputComponent->BindAction(HeavyAttackAction, ETriggerEvent::Completed, this, &ASamuraiCharacter::OnHeavyAttackReleased);
        }

        // Block (Started = pressed, Completed = released)
        if (BlockAction)
        {
            EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Started, this, &ASamuraiCharacter::OnBlockPressed);
            EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Completed, this, &ASamuraiCharacter::OnBlockReleased);
        }

        // Evade
        if (EvadeAction)
        {
            EnhancedInputComponent->BindAction(EvadeAction, ETriggerEvent::Started, this, &ASamuraiCharacter::OnEvadePressed);
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

    // Cache for directional input (used by attack handlers)
    LastMovementInput = MovementVector;

    // V1 REMOVED: Forward movement input to V1 combat component
    // if (CombatComponent)
    // {
    //     CombatComponent->SetMovementInput(MovementVector);
    // }

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

void ASamuraiCharacter::OnLightAttackPressed(const FInputActionValue& Value)
{
    // Check CombatSettings for V2 system enabled
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        // Use auto-transform helper with character-relative transformation (default)
        // This automatically handles camera/character rotation for directional attacks
        CombatComponentV2->OnInputEventAuto(EInputType::LightAttack, EInputEventType::Press, LastMovementInput);
    }
    // V1 REMOVED: V1 fallback removed
    // else if (CombatComponent)
    // {
    //     CombatComponent->OnLightAttackPressed();
    // }
}

void ASamuraiCharacter::OnLightAttackReleased(const FInputActionValue& Value)
{
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        // Use auto-transform helper with character-relative transformation (default)
        CombatComponentV2->OnInputEventAuto(EInputType::LightAttack, EInputEventType::Release, LastMovementInput);
    }
    // V1 REMOVED: V1 fallback removed
    // else if (CombatComponent)
    // {
    //     CombatComponent->OnLightAttackReleased();
    // }
}

void ASamuraiCharacter::OnHeavyAttackPressed(const FInputActionValue& Value)
{
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        // Use auto-transform helper with character-relative transformation (default)
        CombatComponentV2->OnInputEventAuto(EInputType::HeavyAttack, EInputEventType::Press, LastMovementInput);
    }
    // V1 REMOVED: V1 fallback removed
    // else if (CombatComponent)
    // {
    //     CombatComponent->OnHeavyAttackPressed();
    // }
}

void ASamuraiCharacter::OnHeavyAttackReleased(const FInputActionValue& Value)
{
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        // Use auto-transform helper with character-relative transformation (default)
        CombatComponentV2->OnInputEventAuto(EInputType::HeavyAttack, EInputEventType::Release, LastMovementInput);
    }
    // V1 REMOVED: V1 fallback removed
    // else if (CombatComponent)
    // {
    //     CombatComponent->OnHeavyAttackReleased();
    // }
}

void ASamuraiCharacter::OnBlockPressed(const FInputActionValue& Value)
{
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        CombatComponentV2->OnInputEvent(EInputType::Block, EInputEventType::Press);
    }
    // V1 REMOVED: V1 fallback removed (blocking not yet migrated to V2)
    // else if (CombatComponent)
    // {
    //     CombatComponent->OnBlockPressed();
    // }
}

void ASamuraiCharacter::OnBlockReleased(const FInputActionValue& Value)
{
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        CombatComponentV2->OnInputEvent(EInputType::Block, EInputEventType::Release);
    }
    // V1 REMOVED: V1 fallback removed (blocking not yet migrated to V2)
    // else if (CombatComponent)
    // {
    //     CombatComponent->OnBlockReleased();
    // }
}

void ASamuraiCharacter::OnEvadePressed(const FInputActionValue& Value)
{
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        CombatComponentV2->OnInputEvent(EInputType::Evade, EInputEventType::Press);
    }
    // V1 REMOVED: V1 fallback removed (evade not yet migrated to V2)
    // else if (CombatComponent)
    // {
    //     CombatComponent->OnEvadePressed();
    // }
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
    // V1 REMOVED: return CombatComponent ? CombatComponent->CanAttack() : false;
    return false; // TODO: Migrate to V2
}

ECombatState ASamuraiCharacter::GetCombatState_Implementation() const
{
    // V1 REMOVED: return CombatComponent ? CombatComponent->GetCombatState() : ECombatState::Idle;
    return ECombatState::Idle; // TODO: Migrate to V2
}

bool ASamuraiCharacter::IsAttacking_Implementation() const
{
    // V1 REMOVED: return CombatComponent ? CombatComponent->IsAttacking() : false;
    return false; // TODO: Migrate to V2
}

UAttackData* ASamuraiCharacter::GetCurrentAttack_Implementation() const
{
    // V1 REMOVED: return CombatComponent ? CombatComponent->GetCurrentAttack() : nullptr;
    return nullptr; // TODO: Migrate to V2
}

EAttackPhase ASamuraiCharacter::GetCurrentPhase_Implementation() const
{
    // V1 REMOVED: return CombatComponent ? CombatComponent->GetCurrentPhase() : EAttackPhase::None;
    return EAttackPhase::None; // TODO: Migrate to V2
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
    // V1 REMOVED: Phase callbacks not yet migrated to V2
    // if (CombatComponent)
    // {
    //     CombatComponent->OnAttackPhaseBegin(Phase);
    // }
}

void ASamuraiCharacter::OnAttackPhaseEnd_Implementation(EAttackPhase Phase)
{
    // V1 REMOVED: Phase callbacks not yet migrated to V2
    // if (CombatComponent)
    // {
    //     CombatComponent->OnAttackPhaseEnd(Phase);
    // }
}

void ASamuraiCharacter::OnAttackPhaseTransition_Implementation(EAttackPhase NewPhase)
{
    // V1 REMOVED: Forward to V1 system removed
    // if (CombatComponent)
    // {
    //     CombatComponent->OnAttackPhaseTransition(NewPhase);
    // }

    // Forward to V2 if enabled
    if (CombatSettings && CombatSettings->bUseV2System && CombatComponentV2)
    {
        CombatComponentV2->OnPhaseTransition(NewPhase);
    }
}

bool ASamuraiCharacter::IsInParryWindow_Implementation() const
{
    // V1 REMOVED: return CombatComponent ? CombatComponent->IsInParryWindow() : false;
    return false; // TODO: Migrate parry system to V2
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

    // V1 REMOVED: Blocking logic not yet migrated to V2
    // if (CombatComponent && CombatComponent->IsBlocking())
    // {
    //     const float PostureDamage = HitInfo.AttackData ? HitInfo.AttackData->PostureDamage : 10.0f;
    //     if (CombatComponent->ApplyPostureDamage(PostureDamage))
    //     {
    //         return HitReactionComponent->ApplyDamage(HitInfo);
    //     }
    //     return 0.0f;
    // }

    // Not blocking - take full damage (blocking removed)
    return HitReactionComponent->ApplyDamage(HitInfo);
}

bool ASamuraiCharacter::ApplyPostureDamage_Implementation(float PostureDamage, AActor* Attacker)
{
    // V1 REMOVED: Posture system not yet migrated to V2
    // if (CombatComponent)
    // {
    //     return CombatComponent->ApplyPostureDamage(PostureDamage);
    // }

    return false; // TODO: Migrate posture system to V2
}

bool ASamuraiCharacter::CanBeDamaged_Implementation() const
{
    return HitReactionComponent ? HitReactionComponent->CanBeDamaged() : true;
}

bool ASamuraiCharacter::IsBlocking_Implementation() const
{
    // V1 REMOVED: return CombatComponent ? CombatComponent->IsBlocking() : false;
    return false; // TODO: Migrate blocking to V2
}

bool ASamuraiCharacter::IsGuardBroken_Implementation() const
{
    // V1 REMOVED: return CombatComponent ? CombatComponent->IsGuardBroken() : false;
    return false; // TODO: Migrate guard break to V2
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
    // V1 REMOVED: Stop current attack not yet migrated to V2
    // if (CombatComponent)
    // {
    //     CombatComponent->StopCurrentAttack();
    // }

    // V1 REMOVED: Counter window not yet migrated to V2
    // if (CombatComponent)
    // {
    //     CombatComponent->OpenCounterWindow(1.5f); // Duration from CombatSettings
    // }

    // Play parried reaction animation
    if (HitReactionComponent)
    {
        HitReactionComponent->PlayGuardBrokenReaction();
    }
}

void ASamuraiCharacter::OpenCounterWindow_Implementation(float Duration)
{
    // V1 REMOVED: Counter window not yet migrated to V2
    // if (CombatComponent)
    // {
    //     CombatComponent->OpenCounterWindow(Duration);
    // }
}

float ASamuraiCharacter::GetCurrentPosture_Implementation() const
{
    // V1 REMOVED: return CombatComponent ? CombatComponent->GetCurrentPosture() : 0.0f;
    return 0.0f; // TODO: Migrate posture system to V2
}

float ASamuraiCharacter::GetMaxPosture_Implementation() const
{
    // V1 REMOVED: return CombatComponent ? CombatComponent->GetMaxPosture() : 100.0f;
    return 100.0f; // TODO: Migrate posture system to V2
}

bool ASamuraiCharacter::IsInCounterWindow_Implementation() const
{
    // V1 REMOVED: return CombatComponent ? CombatComponent->IsInCounterWindow() : false;
    return false; // TODO: Migrate counter window to V2
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
        HitInfo.bWasCounter = false; // V1 REMOVED: Counter window not migrated to V2 yet
        HitInfo.ImpactPoint = HitResult.ImpactPoint;

        // V1 REMOVED: Counter damage multiplier not yet migrated to V2
        // if (HitInfo.bWasCounter && IDamageableInterface::Execute_IsInCounterWindow(HitActor))
        // {
        //     HitInfo.Damage *= AttackData->CounterDamageMultiplier;
        // }

        // Apply damage via interface
        const float DamageDealt = IDamageableInterface::Execute_ApplyDamage(HitActor, HitInfo);

        // V1 REMOVED: Hit event broadcast not yet migrated to V2
        // if (CombatComponent)
        // {
        //     CombatComponent->OnAttackHit.Broadcast(HitActor, DamageDealt);
        // }
    }
}

// ============================================================================
// DIRECTIONAL INPUT HELPERS
// ============================================================================

EInputDirection ASamuraiCharacter::GetDirectionalInputFromMovement(const FVector2D& MovementVector) const
{
    // Minimum magnitude threshold to register directional input (deadzone)
    constexpr float MinMagnitude = 0.25f;

    // Check if input is below deadzone threshold
    if (MovementVector.SizeSquared() < MinMagnitude * MinMagnitude)
    {
        return EInputDirection::None;
    }

    // Calculate angle in degrees (0° = right, 90° = forward, 180° = left, 270° = backward)
    // Note: FVector2D(X, Y) where X = right, Y = forward
    float Angle = FMath::Atan2(MovementVector.Y, MovementVector.X) * (180.0f / PI);

    // Normalize to [0, 360) range
    if (Angle < 0)
    {
        Angle += 360.0f;
    }

    // Convert angle to 8-way directional input
    // Each direction covers a 45° slice (22.5° on each side of cardinal/diagonal)
    // Right: 337.5° - 22.5° (0°)
    if (Angle >= 337.5f || Angle < 22.5f)
    {
        return EInputDirection::Right;
    }
    // ForwardRight: 22.5° - 67.5° (45°)
    else if (Angle >= 22.5f && Angle < 67.5f)
    {
        return EInputDirection::ForwardRight;
    }
    // Forward: 67.5° - 112.5° (90°)
    else if (Angle >= 67.5f && Angle < 112.5f)
    {
        return EInputDirection::Forward;
    }
    // ForwardLeft: 112.5° - 157.5° (135°)
    else if (Angle >= 112.5f && Angle < 157.5f)
    {
        return EInputDirection::ForwardLeft;
    }
    // Left: 157.5° - 202.5° (180°)
    else if (Angle >= 157.5f && Angle < 202.5f)
    {
        return EInputDirection::Left;
    }
    // BackwardLeft: 202.5° - 247.5° (225°)
    else if (Angle >= 202.5f && Angle < 247.5f)
    {
        return EInputDirection::BackwardLeft;
    }
    // Backward: 247.5° - 292.5° (270°)
    else if (Angle >= 247.5f && Angle < 292.5f)
    {
        return EInputDirection::Backward;
    }
    // BackwardRight: 292.5° - 337.5° (315°)
    else // Angle >= 292.5f && Angle < 337.5f
    {
        return EInputDirection::BackwardRight;
    }
}