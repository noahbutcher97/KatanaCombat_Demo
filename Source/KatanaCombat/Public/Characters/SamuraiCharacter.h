// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCombatCharacter.h"
#include "SamuraiCharacter.generated.h"

// Forward declarations
class UCombatDebugWidget;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * Player-controlled samurai character
 * Inherits combat functionality from BaseCombatCharacter, adds:
 * - Enhanced Input handling
 * - Debug visualization widget
 * - Player-specific movement settings
 *
 * Note: This class is primarily a coordinator for player input.
 * Combat logic lives in components (CombatComponent, TargetingComponent, etc.)
 */
UCLASS()
class KATANACOMBAT_API ASamuraiCharacter : public ABaseCombatCharacter
{
    GENERATED_BODY()

public:
    ASamuraiCharacter();

    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

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

    // ========================================================================
    // INPUT HANDLERS
    // ========================================================================

    /** Movement input (continuous) */
    void Move(const FInputActionValue& Value);

    /** Look input (continuous) */
    void Look(const FInputActionValue& Value);

    /** Light attack button pressed */
    void OnLightAttackPressed(const FInputActionValue& Value);

    /** Light attack button released */
    void OnLightAttackReleased(const FInputActionValue& Value);

    /** Heavy attack button pressed */
    void OnHeavyAttackPressed(const FInputActionValue& Value);

    /** Heavy attack button released */
    void OnHeavyAttackReleased(const FInputActionValue& Value);

    /** Block button pressed */
    void OnBlockPressed(const FInputActionValue& Value);

    /** Block button released */
    void OnBlockReleased(const FInputActionValue& Value);

    /** Evade button pressed */
    void OnEvadePressed(const FInputActionValue& Value);

    /** Debug toggle button pressed */
    void OnToggleDebug(const FInputActionValue& Value);

    // ========================================================================
    // DIRECTIONAL INPUT HELPERS
    // ========================================================================

    /** Last captured movement vector (for directional input) */
    FVector2D LastMovementInput;
};
