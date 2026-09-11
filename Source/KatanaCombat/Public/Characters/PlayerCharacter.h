// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCombatCharacter.h"
#include "PlayerCharacter.generated.h"

// Forward declarations
class UCombatDebugWidget;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * Player-controlled combat character
 * Inherits combat functionality from BaseCombatCharacter, adds:
 * - Enhanced Input handling
 * - Debug visualization widget
 * - Player-specific movement settings
 *
 * Note: This class is primarily a coordinator for player input.
 * Combat logic lives in components (CombatComponent, TargetingComponent, etc.)
 */
UCLASS()
class KATANACOMBAT_API APlayerCharacter : public ABaseCombatCharacter
{
    GENERATED_BODY()

#if WITH_AUTOMATION_TESTS
    friend class FDefenseAlignment_PlayerLookRoutesManualYaw;
    friend class FPlayerMovementHoldSuppressionPolicyTest;
    friend class FPlayerMovementTerminalSampleTest;
#endif

public:
    APlayerCharacter();

    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual void UnPossessed() override;

    /** Get current movement input for debug visualization */
    virtual FVector2D GetLastMovementInput() const override;

    /** CharacterMovement yaw rate used while orienting locomotion to movement input. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Movement",
        meta = (ClampMin = "1.0", ClampMax = "1080.0", UIMin = "1.0", UIMax = "1080.0", Units = "DegreesPerSecond"))
    float LocomotionRotationRate = 540.0f;

    // ========================================================================
    // DEBUG WIDGET (Player-specific)
    // ========================================================================

    /** Debug visualization widget for combat system */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Debug")
    TObjectPtr<UCombatDebugWidget> CombatDebugWidget;

    // ========================================================================
    // ENHANCED INPUT (Player-specific)
    // ========================================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> LightAttackAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> HeavyAttackAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> BlockAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> EvadeAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> ToggleDebugAction;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ========================================================================
    // INPUT HANDLERS
    // ========================================================================

    /** Movement input (continuous) */
    void Move(const FInputActionValue& Value);

    /** Clear the terminal-aware movement sample. */
    void StopMove(const FInputActionValue& Value);

    /** Look input (continuous) */
    void Look(const FInputActionValue& Value);

    /** Clear held-guard manual yaw intent when the look action stops. */
    void StopLook(const FInputActionValue& Value);

    /** Light attack button pressed */
    void OnLightAttackPressed(const FInputActionValue& Value);

    /** Light attack button released */
    void OnLightAttackReleased(const FInputActionValue& Value);

    /** Light attack input was canceled by Enhanced Input. */
    void OnLightAttackCanceled(const FInputActionValue& Value);

    /** Heavy attack button pressed */
    void OnHeavyAttackPressed(const FInputActionValue& Value);

    /** Heavy attack button released */
    void OnHeavyAttackReleased(const FInputActionValue& Value);

    /** Heavy attack input was canceled by Enhanced Input. */
    void OnHeavyAttackCanceled(const FInputActionValue& Value);

    /** Block button pressed */
    void OnBlockPressed(const FInputActionValue& Value);

    /** Block button released */
    void OnBlockReleased(const FInputActionValue& Value);

    /** Block input was canceled by Enhanced Input. */
    void OnBlockCanceled(const FInputActionValue& Value);

    /** Evade button pressed */
    void OnEvadePressed(const FInputActionValue& Value);

    /** Debug toggle button pressed */
    void OnToggleDebug(const FInputActionValue& Value);

};
