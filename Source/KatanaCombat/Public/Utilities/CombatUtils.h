// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatTypes.h"
#include "CombatUtils.generated.h"

class ACharacter;

/**
 * Combat Utilities
 *
 * BlueprintFunctionLibrary providing core combat system utility functions.
 * Used for direction conversion, rotation calculations, and coordinate transforms.
 *
 * Key Functions:
 * - Direction conversion (8-way input to 4-way attack, vector to direction enum)
 * - Rotation helpers (mesh-compensated rotation, camera-to-character transforms)
 * - World space conversions (input direction to world vector/rotation)
 */
UCLASS()
class KATANACOMBAT_API UCombatUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ========================================================================
	// DIRECTION CONVERSION
	// ========================================================================

	/**
	 * Convert 8-way input direction to 4-way attack direction
	 * Diagonal inputs map to their primary direction based on design rules:
	 * - ForwardLeft/ForwardRight -> Forward
	 * - BackwardLeft/BackwardRight -> Backward
	 *
	 * @param InputDir - 8-way input direction
	 * @return 4-way attack direction
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Utils")
	static EAttackDirection InputToAttackDirection(EInputDirection InputDir);

	/**
	 * Calculate 8-way input direction from 2D input vector
	 * Uses 45-degree sectors centered on each direction
	 *
	 * @param InputVector - Normalized 2D input (X=right, Y=forward)
	 * @param DeadZone - Minimum magnitude to register input (default 0.2)
	 * @return 8-way direction enum, or None if below dead zone
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Utils")
	static EInputDirection VectorToInputDirection(const FVector2D& InputVector, float DeadZone = 0.2f);

	/**
	 * Convert EInputDirection enum to world space direction vector
	 * Used by directional attack system for attack direction from buffered input
	 *
	 * @param Direction - 8-way input direction
	 * @param Character - Character to get rotation from
	 * @return World space direction vector (normalized), or ZeroVector if None
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Utils")
	static FVector InputDirectionToWorldVector(EInputDirection Direction, const ACharacter* Character);

	/**
	 * Convert EInputDirection enum to world rotation (yaw only)
	 * Convenience wrapper for motion warp targets
	 *
	 * @param Direction - 8-way input direction
	 * @param Character - Character to get rotation from
	 * @return World rotation (yaw only), or character's rotation if None
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Utils")
	static FRotator InputDirectionToWorldRotation(EInputDirection Direction, const ACharacter* Character);

	// ========================================================================
	// COORDINATE TRANSFORMS
	// ========================================================================

	/**
	 * Convert camera-relative input to character-relative direction
	 * Handles the full transform pipeline: Camera Space -> World Space -> Character Space
	 *
	 * @param CameraRelativeInput - 2D input from gamepad (X=right, Y=forward relative to camera)
	 * @param CameraRotation - Current camera rotation (only Yaw is used)
	 * @param Character - Character for mesh offset detection (can be nullptr)
	 * @param CharacterRotation - Character actor rotation
	 * @param DeadZone - Minimum magnitude to register input (default 0.2)
	 * @return 8-way direction relative to character's facing
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Utils")
	static EInputDirection VectorToCharacterRelativeDirection(
		const FVector2D& CameraRelativeInput,
		const FRotator& CameraRotation,
		const ACharacter* Character,
		const FRotator& CharacterRotation,
		float DeadZone = 0.2f);

	// ========================================================================
	// ROTATION HELPERS
	// ========================================================================

	/**
	 * Get character rotation for directional calculations
	 * Optionally includes mesh relative rotation offset
	 *
	 * For standard characters using bOrientRotationToMovement, the actor rotation
	 * IS the visual facing direction. Mesh relative rotation is typically model
	 * correction (e.g., -90 degrees for Y-forward models), not intentional offset.
	 *
	 * @param Character - Character to query
	 * @param bIncludeMeshOffset - If true, adds mesh relative rotation (rare)
	 * @return Actor rotation, optionally with mesh offset
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Utils")
	static FRotator GetMeshCompensatedRotation(const ACharacter* Character, bool bIncludeMeshOffset = false);

	/**
	 * Get mesh rotation offset from actor rotation
	 * Returns the mesh component's relative rotation
	 *
	 * @param Character - Character to query
	 * @return Mesh relative rotation (ZeroRotator if invalid)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Utils")
	static FRotator GetMeshRotationOffset(const ACharacter* Character);
};
