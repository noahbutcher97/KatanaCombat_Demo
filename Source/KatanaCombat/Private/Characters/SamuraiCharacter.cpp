// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/SamuraiCharacter.h"
#include "Core/CombatComponent.h"
#include "Debug/CombatDebugWidget.h"
#include "Debug/DebugConfig.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Utilities/DirectionDebugLibrary.h"

ASamuraiCharacter::ASamuraiCharacter()
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
}

void ASamuraiCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Debug Visualization: Run on Tick to update arrows in real-time
    // CVar: Combat.Debug.Direction 1 (or Combat.Debug.All 1)
    if (CombatComponent && CombatDebug::IsDirectionDebugEnabled())
    {
        // 1. Gather Transformations
        const FRotator CameraRotation = GetControlRotation();
        const FRotator ActorRotation = GetActorRotation();
        const FVector2D CameraRelativeInput = LastMovementInput;

        // CRITICAL: Use mesh-compensated character rotation (actor + mesh offset)
        // Character meshes are often rotated -90° in editor, so actor rotation != mesh forward
        const FRotator MeshCompensatedRotation = CombatHelpers::GetMeshCompensatedRotation(this);

        // Camera-relative to world space conversion
        // CRITICAL: Flatten camera rotation to yaw-only to prevent pitch/roll from corrupting WorldInput
        const FRotator FlatCameraRotation = FRotator(0.0f, CameraRotation.Yaw, 0.0f);
        const FVector CameraForward = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::X);
        const FVector CameraRight = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::Y);
        FVector WorldInput = (CameraRight * CameraRelativeInput.X) + (CameraForward * CameraRelativeInput.Y);
        WorldInput.Z = 0.0f;
        WorldInput.Normalize();

        // World to character-relative conversion (using mesh-compensated rotation)
        const FRotator InverseCharacterYaw(0.0f, -MeshCompensatedRotation.Yaw, 0.0f);
        const FVector CharacterRelativeVec = InverseCharacterYaw.RotateVector(WorldInput);
        const FVector2D CharacterRelative2D(CharacterRelativeVec.X, CharacterRelativeVec.Y);

        // 2. Resolve Direction Enum (using canonical CombatHelpers function)
        const EInputDirection CharacterRelativeDirection = CombatHelpers::VectorToInputDirection(CharacterRelative2D);

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
                    this,
                    CharacterLocation,
                    CameraRotation,
                    MeshCompensatedRotation,
                    CameraRelativeInput,
                    WorldInput,
                    CharacterRelativeVec,
                    CharacterRelativeDirection,
                    AttackDir,
                    CombatComponent->IsHolding()
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
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::LightAttack, EInputEventType::Press, LastMovementInput);
    }
}

void ASamuraiCharacter::OnLightAttackReleased(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::LightAttack, EInputEventType::Release, LastMovementInput);
    }
}

void ASamuraiCharacter::OnHeavyAttackPressed(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::HeavyAttack, EInputEventType::Press, LastMovementInput);
    }
}

void ASamuraiCharacter::OnHeavyAttackReleased(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnInputEventAuto(EInputType::HeavyAttack, EInputEventType::Release, LastMovementInput);
    }
}

void ASamuraiCharacter::OnBlockPressed(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnInputEvent(EInputType::Block, EInputEventType::Press);
    }
}

void ASamuraiCharacter::OnBlockReleased(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnInputEvent(EInputType::Block, EInputEventType::Release);
    }
}

void ASamuraiCharacter::OnEvadePressed(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnInputEvent(EInputType::Evade, EInputEventType::Press);
    }
}

void ASamuraiCharacter::OnToggleDebug(const FInputActionValue& Value)
{
    if (CombatDebugWidget)
    {
        CombatDebugWidget->ToggleDebugOverlay();
    }
}
