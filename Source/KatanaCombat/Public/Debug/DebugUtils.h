// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatTypes.h"
#include "DebugUtils.generated.h"

/**
 * Debug Utilities
 *
 * Static utility functions for debugging the combat system.
 * Provides formatting, calculation, and conversion helpers.
 *
 * Features:
 * - Cardinal direction conversion (yaw -> "N", "NE", "E", etc.)
 * - Rotation/Vector formatting for debug output
 * - Yaw delta calculations (handles wrapping)
 * - Mesh rotation offset detection
 * - Direction enum conversions
 *
 * Note: Visual debug rendering is now handled by ACombatDebugHUD.
 * This class provides the data/formatting utilities only.
 */
UCLASS()
class KATANACOMBAT_API UDebugUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ========================================================================
	// DIRECTION CONVERSION HELPERS
	// ========================================================================

	/**
	 * Convert EInputDirection to EAttackDirection
	 * Maps 8-way input to 4-way attack directions
	 *
	 * Mapping:
	 * - Forward, ForwardLeft, ForwardRight -> Forward
	 * - Backward, BackwardLeft, BackwardRight -> Backward
	 * - Left -> Left
	 * - Right -> Right
	 * - None -> None
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	static EAttackDirection InputDirectionToAttackDirection(EInputDirection InputDir);

	/**
	 * Convert yaw angle to cardinal/ordinal direction string
	 *
	 * Maps yaw to 8 compass directions:
	 * - N  (337.5 to 22.5)
	 * - NE (22.5 to 67.5)
	 * - E  (67.5 to 112.5)
	 * - SE (112.5 to 157.5)
	 * - S  (157.5 to 202.5)
	 * - SW (202.5 to 247.5)
	 * - W  (247.5 to 292.5)
	 * - NW (292.5 to 337.5)
	 *
	 * @param Yaw - Yaw angle in degrees (will be normalized)
	 * @return Cardinal direction string (e.g., "N", "NE", "E", etc.)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	static FString YawToCardinalDirection(float Yaw);

	// ========================================================================
	// FORMATTING HELPERS
	// ========================================================================

	/**
	 * Format rotation for debug display
	 * Shows yaw angle with cardinal direction
	 *
	 * @param Rotation - Rotation to format
	 * @return Formatted string (e.g., "Yaw=45.0 (NE)")
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	static FString FormatRotationDebug(const FRotator& Rotation);

	/**
	 * Format Vector2D for debug display
	 * Shows X/Y components and magnitude
	 *
	 * @param Vec - Vector to format
	 * @return Formatted string (e.g., "(X=0.71, Y=0.71) mag=1.00")
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	static FString FormatVector2DDebug(const FVector2D& Vec);

	/**
	 * Format EInputDirection enum for debug display
	 *
	 * @param Direction - Input direction enum
	 * @return Formatted string (e.g., "Forward", "ForwardRight", etc.)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	static FString FormatInputDirectionDebug(EInputDirection Direction);

	/**
	 * Format EAttackDirection enum for debug display
	 *
	 * @param Direction - Attack direction enum
	 * @return Formatted string (e.g., "Forward", "Backward", "Left", "Right")
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	static FString FormatAttackDirectionDebug(EAttackDirection Direction);

	// ========================================================================
	// CALCULATION HELPERS
	// ========================================================================

	/**
	 * Calculate shortest angular distance between two yaw angles
	 * Handles wrapping correctly (returns -180 to +180)
	 *
	 * @param FromYaw - Starting yaw angle in degrees
	 * @param ToYaw - Ending yaw angle in degrees
	 * @return Shortest angular distance in degrees (-180 to +180)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	static float CalculateYawDelta(float FromYaw, float ToYaw);

	/**
	 * Normalize yaw angle to -180 to +180 range
	 *
	 * @param Yaw - Yaw angle in degrees
	 * @return Normalized yaw (-180 to +180)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	static float NormalizeYaw(float Yaw);

	/**
	 * Get mesh rotation offset from character actor rotation
	 * Useful for detecting when mesh is rotated relative to root component
	 *
	 * @param Character - Character to query
	 * @return Rotation difference between actor and mesh
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	static FRotator GetMeshRotationOffset(ACharacter* Character);

	// ========================================================================
	// WEAPON TRACE DEBUG VISUALIZATION
	// ========================================================================

	/**
	 * Draw weapon trace debug visualization
	 * Shows capsule sweep from previous to current weapon position along full blade length
	 *
	 * @param World - World context
	 * @param CurrentStart - Current frame weapon start socket position
	 * @param CurrentEnd - Current frame weapon end socket position
	 * @param PreviousStart - Previous frame weapon start socket position
	 * @param PreviousEnd - Previous frame weapon end socket position
	 * @param TraceRadius - Capsule trace radius
	 * @param bHit - Whether trace hit something
	 * @param HitResult - Hit result (only used if bHit is true)
	 */
	static void DrawWeaponTrace(
		UWorld* World,
		const FVector& CurrentStart,
		const FVector& CurrentEnd,
		const FVector& PreviousStart,
		const FVector& PreviousEnd,
		float TraceRadius,
		bool bHit,
		const FHitResult& HitResult = FHitResult());

	/**
	 * Draw weapon socket positions for debugging
	 * Shows start and end socket locations with labels
	 *
	 * @param World - World context
	 * @param StartLocation - Weapon start socket position
	 * @param EndLocation - Weapon end socket position
	 */
	static void DrawWeaponSockets(
		UWorld* World,
		const FVector& StartLocation,
		const FVector& EndLocation);
};
