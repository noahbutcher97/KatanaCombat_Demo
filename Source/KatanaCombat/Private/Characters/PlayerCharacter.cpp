// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/PlayerCharacter.h"
#include "Core/CombatComponent.h"
#include "Debug/CombatDebugWidget.h"
#include "Debug/DebugConfig.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
// Debug visualization handled by ACombatDebugHUD

APlayerCharacter::APlayerCharacter()
{
    // Set default team for player
    TeamId = ETeamId::Player;

    // Create debug widget (player-specific)
    CombatDebugWidget = CreateDefaultSubobject<UCombatDebugWidget>(TEXT("CombatDebugWidget"));

    // Configure character movement (default for third-person combat)
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, LocomotionRotationRate, 0.0f);
    GetCharacterMovement()->MaxWalkSpeed = 600.0f;

    // Don't rotate camera with controller
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
}

void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    const float ValidatedRotationRate = FMath::IsFinite(LocomotionRotationRate)
        ? FMath::Clamp(LocomotionRotationRate, 1.0f, 1080.0f)
        : 540.0f;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, ValidatedRotationRate, 0.0f);

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
}

void APlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (CombatComponent)
    {
        CombatComponent->ClearMovementInputSample();
    }

    Super::EndPlay(EndPlayReason);
}

void APlayerCharacter::UnPossessed()
{
    if (CombatComponent)
    {
        CombatComponent->ClearMovementInputSample();
    }

    Super::UnPossessed();
}

FVector2D APlayerCharacter::GetLastMovementInput() const
{
    return CombatComponent
        ? CombatComponent->GetMovementInputSample().CameraRelativeInput
        : FVector2D::ZeroVector;
}

void APlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Debug visualization is now handled by ACombatDebugHUD
    // Enable with CVar: Combat.Debug.Direction 1 (or Combat.Debug.All 1)
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // Movement
        if (MoveAction)
        {
            EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);
            EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopMove);
            EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &APlayerCharacter::StopMove);
        }

        // Looking
        if (LookAction)
        {
            EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);
            EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopLook);
            EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Canceled, this, &APlayerCharacter::StopLook);
        }

        // Light Attack (Started = pressed, Completed = released)
        if (LightAttackAction)
        {
            EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Started, this, &APlayerCharacter::OnLightAttackPressed);
            EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnLightAttackReleased);
            EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Canceled, this, &APlayerCharacter::OnLightAttackCanceled);
        }

        // Heavy Attack (Started = pressed, Completed = released)
        if (HeavyAttackAction)
        {
            EnhancedInputComponent->BindAction(HeavyAttackAction, ETriggerEvent::Started, this, &APlayerCharacter::OnHeavyAttackPressed);
            EnhancedInputComponent->BindAction(HeavyAttackAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnHeavyAttackReleased);
            EnhancedInputComponent->BindAction(HeavyAttackAction, ETriggerEvent::Canceled, this, &APlayerCharacter::OnHeavyAttackCanceled);
        }

        // Block (Started = pressed, Completed = released)
        if (BlockAction)
        {
            EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Started, this, &APlayerCharacter::OnBlockPressed);
            EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnBlockReleased);
            EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Canceled, this, &APlayerCharacter::OnBlockCanceled);
        }

        // Evade
        if (EvadeAction)
        {
            EnhancedInputComponent->BindAction(EvadeAction, ETriggerEvent::Started, this, &APlayerCharacter::OnEvadePressed);
        }

        // Toggle Debug Overlay
        if (ToggleDebugAction)
        {
            EnhancedInputComponent->BindAction(ToggleDebugAction, ETriggerEvent::Started, this, &APlayerCharacter::OnToggleDebug);
        }
    }
}

// ============================================================================
// INPUT HANDLERS
// ============================================================================

void APlayerCharacter::Move(const FInputActionValue& Value)
{
    FVector2D MovementVector = Value.Get<FVector2D>();
    const FRotator CameraRotation = Controller
        ? Controller->GetControlRotation()
        : GetActorRotation();
    const bool bMayApplyMovement = CombatComponent
        ? CombatComponent->SubmitMovementInput(MovementVector, CameraRotation)
        : !MovementVector.ContainsNaN();
    if (CombatComponent)
    {
        MovementVector = CombatComponent->GetMovementInputSample().CameraRelativeInput;
    }

    if (Controller && bMayApplyMovement && !MovementVector.IsZero())
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

void APlayerCharacter::StopMove(const FInputActionValue& Value)
{
    (void)Value;
    if (CombatComponent)
    {
        CombatComponent->ClearMovementInputSample();
    }
}

void APlayerCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (CombatComponent)
    {
        const float NormalizedYaw = FMath::IsFinite(LookAxisVector.X)
            ? FMath::Clamp(LookAxisVector.X, -1.0f, 1.0f)
            : 0.0f;
        CombatComponent->SetDefenseManualYawInput(NormalizedYaw);
    }

    if (Controller)
    {
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
}

void APlayerCharacter::StopLook(const FInputActionValue& Value)
{
    (void)Value;
    if (CombatComponent)
    {
        CombatComponent->SetDefenseManualYawInput(0.0f);
    }
}

void APlayerCharacter::OnLightAttackPressed(const FInputActionValue& Value)
{
    (void)Value;
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::LightAttack, EInputEventType::Press, GetLastMovementInput());
    }
}

void APlayerCharacter::OnLightAttackReleased(const FInputActionValue& Value)
{
    (void)Value;
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::LightAttack, EInputEventType::Release, GetLastMovementInput());
    }
}

void APlayerCharacter::OnLightAttackCanceled(const FInputActionValue& Value)
{
    (void)Value;
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::LightAttack, EInputEventType::Canceled, GetLastMovementInput());
    }
}

void APlayerCharacter::OnHeavyAttackPressed(const FInputActionValue& Value)
{
    (void)Value;
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::HeavyAttack, EInputEventType::Press, GetLastMovementInput());
    }
}

void APlayerCharacter::OnHeavyAttackReleased(const FInputActionValue& Value)
{
    (void)Value;
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::HeavyAttack, EInputEventType::Release, GetLastMovementInput());
    }
}

void APlayerCharacter::OnHeavyAttackCanceled(const FInputActionValue& Value)
{
    (void)Value;
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::HeavyAttack, EInputEventType::Canceled, GetLastMovementInput());
    }
}

void APlayerCharacter::OnBlockPressed(const FInputActionValue& Value)
{
    (void)Value;
    if (CombatComponent)
    {
        CombatComponent->OnInputEvent(EInputType::Block, EInputEventType::Press);
    }
}

void APlayerCharacter::OnBlockReleased(const FInputActionValue& Value)
{
    (void)Value;
    if (CombatComponent)
    {
        CombatComponent->OnInputEvent(EInputType::Block, EInputEventType::Release);
    }
}

void APlayerCharacter::OnBlockCanceled(const FInputActionValue& Value)
{
    (void)Value;
    if (CombatComponent)
    {
        CombatComponent->OnInputEvent(EInputType::Block, EInputEventType::Canceled);
    }
}

void APlayerCharacter::OnEvadePressed(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnInputEvent(EInputType::Evade, EInputEventType::Press);
    }
}

void APlayerCharacter::OnToggleDebug(const FInputActionValue& Value)
{
    if (CombatDebugWidget)
    {
        CombatDebugWidget->ToggleDebugOverlay();
    }
}
