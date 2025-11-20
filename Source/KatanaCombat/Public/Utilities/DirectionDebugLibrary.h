// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatTypes.h"
#include "DirectionDebugLibrary.generated.h"

/**
 * Direction Debug Library
 *
 * Blueprint function library providing modular, reusable helpers for debugging
 * direction transformations in the combat system.
 *
 * Features:
 * - Cardinal direction conversion (yaw → "N", "NE", "E", etc.)
 * - Rotation/Vector formatting for debug output
 * - Yaw delta calculations (handles wrapping)
 * - Mesh rotation offset detection
 * - Complete transformation pipeline visualization
 * - Optional visual debugging (draw arrows in-world)
 *
 * Benefits:
 * - Separation of concerns (debug utilities separate from combat logic)
 * - Reusability (can be used by any system dealing with directions)
 * - Blueprint exposure (designers can use these in animation blueprints)
 * - Consistent formatting (single source of truth for direction display)
 * - Stateless design (pure functions, no side effects)
 *
 * Usage Example:
 * @code
 * FString DebugInfo = UDirectionDebugLibrary::VisualizeDirectionTransform(
 *     CameraRotation, CharacterRotation, RawInput, ResolvedInputDir, FinalAttackDir);
 * UE_LOG(LogCombat, Log, TEXT("%s"), *DebugInfo);
 * @endcode
 */
UCLASS()
class KATANACOMBAT_API UDirectionDebugLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================================
	// CORE FORMATTING HELPERS
	// ============================================================================

	/**
	 * Convert yaw angle to cardinal/ordinal direction string
	 *
	 * Maps yaw to 8 compass directions:
	 * - N  (337.5° to 22.5°)
	 * - NE (22.5° to 67.5°)
	 * - E  (67.5° to 112.5°)
	 * - SE (112.5° to 157.5°)
	 * - S  (157.5° to 202.5° or -157.5° to -202.5°)
	 * - SW (202.5° to 247.5° or -112.5° to -157.5°)
	 * - W  (247.5° to 292.5° or -67.5° to -112.5°)
	 * - NW (292.5° to 337.5° or -22.5° to -67.5°)
	 *
	 * @param Yaw - Yaw angle in degrees (will be normalized to -180 to +180 range)
	 * @return Cardinal direction string (e.g., "N", "NE", "E", etc.)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug|Direction")
	static FString YawToCardinalDirection(float Yaw);

	/**
	 * Format rotation for debug display
	 * Shows yaw angle with cardinal direction
	 *
	 * @param Rotation - Rotation to format
	 * @return Formatted string (e.g., "Yaw=45.0° (NE)")
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug|Direction")
	static FString FormatRotationDebug(const FRotator& Rotation);

	/**
	 * Format Vector2D for debug display
	 * Shows X/Y components and magnitude
	 *
	 * @param Vec - Vector to format
	 * @return Formatted string (e.g., "(X=0.71, Y=0.71) magnitude=1.00")
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug|Direction")
	static FString FormatVector2DDebug(const FVector2D& Vec);

	/**
	 * Format EInputDirection enum for debug display
	 * Converts enum to readable string
	 *
	 * @param Direction - Input direction enum
	 * @return Formatted string (e.g., "Forward", "ForwardRight", etc.)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug|Direction")
	static FString FormatInputDirectionDebug(EInputDirection Direction);

	/**
	 * Format EAttackDirection enum for debug display
	 * Converts enum to readable string
	 *
	 * @param Direction - Attack direction enum
	 * @return Formatted string (e.g., "Forward", "Backward", "Left", "Right", "None")
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug|Direction")
	static FString FormatAttackDirectionDebug(EAttackDirection Direction);

	// ============================================================================
	// CALCULATION HELPERS
	// ============================================================================

	/**
	 * Calculate shortest angular distance between two yaw angles
	 * Handles wrapping correctly (returns -180 to +180)
	 *
	 * Example:
	 * - CalculateYawDelta(10.0f, 350.0f) returns -20.0f (not 340.0f)
	 * - CalculateYawDelta(350.0f, 10.0f) returns 20.0f (not -340.0f)
	 *
	 * @param FromYaw - Starting yaw angle in degrees
	 * @param ToYaw - Ending yaw angle in degrees
	 * @return Shortest angular distance in degrees (-180 to +180)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug|Direction")
	static float CalculateYawDelta(float FromYaw, float ToYaw);

	/**
	 * Get mesh rotation offset from character actor rotation
	 * Useful for detecting when mesh is rotated relative to root component
	 *
	 * @param Character - Character to query
	 * @return Rotation difference between actor and mesh (ZeroRotator if no offset or invalid)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug|Direction")
	static FRotator GetMeshRotationOffset(ACharacter* Character);

	// ============================================================================
	// DEBUG VISUALIZATION
	// ============================================================================

	/**
	 * Draw comprehensive directional input transformation debug visualization
	 * Shows numbered pipeline: Camera → Input → Character-Relative → Attack
	 *
	 * Features:
	 * - 5 numbered arrows showing transformation pipeline
	 * - Solid vs dashed arrows for continuous vs hold-release input
	 * - Angular arc showing camera-character offset
	 * - Context-aware filtering and color-coding
	 * - Real-time updates
	 *
	 * @param World - World context for drawing
	 * @param Character - Character actor reference (for text anchoring)
	 * @param CharacterLocation - Character's world position
	 * @param CameraRotation - Camera rotation (for camera-relative input)
	 * @param CharacterRotation - Character rotation (for character-relative conversion)
	 * @param CameraRelativeInput - Raw input vector in camera space
	 * @param WorldInput - Input converted to world space
	 * @param CharacterRelativeVec - Input converted to character-relative space
	 * @param CharacterRelativeDirection - Final resolved input direction enum
	 * @param AttackDirection - Final attack direction enum
	 * @param bIsHoldActive - Whether hold mechanic is currently active
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug|Direction")
	static void DrawDirectionTransformDebug(
		UWorld* World,
		ACharacter* Character,
		const FVector& CharacterLocation,
		const FRotator& CameraRotation,
		const FRotator& CharacterRotation,
		const FVector2D& CameraRelativeInput,
		const FVector& WorldInput,
		const FVector& CharacterRelativeVec,
		EInputDirection CharacterRelativeDirection,
		EAttackDirection AttackDirection,
		bool bIsHoldActive);

private:
	// ============================================================================
	// INTERNAL HELPERS
	// ============================================================================

	/**
	 * Normalize yaw angle to -180 to +180 range
	 */
	static float NormalizeYaw(float Yaw);
};
