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
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 180, 0.0f);
    GetCharacterMovement()->MaxWalkSpeed = 600.0f;

    // Don't rotate camera with controller
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
}

void APlayerCharacter::BeginPlay()
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
        }

        // Looking
        if (LookAction)
        {
            EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);
        }

        // Light Attack (Started = pressed, Completed = released)
        if (LightAttackAction)
        {
            EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Started, this, &APlayerCharacter::OnLightAttackPressed);
            EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnLightAttackReleased);
        }

        // Heavy Attack (Started = pressed, Completed = released)
        if (HeavyAttackAction)
        {
            EnhancedInputComponent->BindAction(HeavyAttackAction, ETriggerEvent::Started, this, &APlayerCharacter::OnHeavyAttackPressed);
            EnhancedInputComponent->BindAction(HeavyAttackAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnHeavyAttackReleased);
        }

        // Block (Started = pressed, Completed = released)
        if (BlockAction)
        {
            EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Started, this, &APlayerCharacter::OnBlockPressed);
            EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnBlockReleased);
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
    const FVector2D MovementVector = Value.Get<FVector2D>();

    // Cache for directional input (used by attack handlers)
    LastMovementInput = MovementVector;

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

void APlayerCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller)
    {
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
}

void APlayerCharacter::OnLightAttackPressed(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::LightAttack, EInputEventType::Press, LastMovementInput);
    }
}

void APlayerCharacter::OnLightAttackReleased(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::LightAttack, EInputEventType::Release, LastMovementInput);
    }
}

void APlayerCharacter::OnHeavyAttackPressed(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::HeavyAttack, EInputEventType::Press, LastMovementInput);
    }
}

void APlayerCharacter::OnHeavyAttackReleased(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::HeavyAttack, EInputEventType::Release, LastMovementInput);
    }
}

void APlayerCharacter::OnBlockPressed(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnInputEvent(EInputType::Block, EInputEventType::Press);
    }
}

void APlayerCharacter::OnBlockReleased(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnInputEvent(EInputType::Block, EInputEventType::Release);
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
