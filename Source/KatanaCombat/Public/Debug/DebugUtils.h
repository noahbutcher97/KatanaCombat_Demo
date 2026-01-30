// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatTypes.h"
#include "DebugUtils.generated.h"

/**
 * Result of a ground sampling operation
 * Contains floor position, normal, and validity info
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FGroundSampleResult
{
	GENERATED_BODY()

	/** Was valid ground found? */
	UPROPERTY(BlueprintReadOnly, Category = "Environment")
	bool bFoundGround = false;

	/** World location of ground surface */
	UPROPERTY(BlueprintReadOnly, Category = "Environment")
	FVector GroundLocation = FVector::ZeroVector;

	/** Normal of the ground surface */
	UPROPERTY(BlueprintReadOnly, Category = "Environment")
	FVector GroundNormal = FVector::UpVector;

	/** Slope angle in degrees (0 = flat, 90 = vertical wall) */
	UPROPERTY(BlueprintReadOnly, Category = "Environment")
	float SlopeAngle = 0.0f;

	/** Is the slope walkable by standard character movement? */
	UPROPERTY(BlueprintReadOnly, Category = "Environment")
	bool bIsWalkable = true;
};

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

	// ========================================================================
	// ENVIRONMENT/SLOPE DEBUG VISUALIZATION
	// ========================================================================

	/**
	 * Draw floor normal and slope angle at character location
	 * Shows the surface normal and angle from vertical
	 *
	 * @param World - World context
	 * @param Location - Location to visualize floor at
	 * @param FloorNormal - Normal of the floor surface
	 * @param Label - Optional label to display (e.g., "Player", "Target")
	 */
	static void DrawFloorNormal(
		UWorld* World,
		const FVector& Location,
		const FVector& FloorNormal,
		const FString& Label = TEXT(""));

	/**
	 * Draw ground trace visualization
	 * Shows trace from start to ground hit, useful for debugging ground detection
	 *
	 * @param World - World context
	 * @param TraceStart - Start location of trace
	 * @param GroundHitLocation - Where ground was found
	 * @param bHitGround - Whether ground was found
	 * @param AdjustedLocation - Final adjusted location (ground + offset)
	 */
	static void DrawGroundTrace(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& GroundHitLocation,
		bool bHitGround,
		const FVector& AdjustedLocation);

	/**
	 * Draw warp location Z adjustment
	 * Visualizes when warp target is adjusted for terrain height
	 *
	 * @param World - World context
	 * @param OriginalLocation - Original warp target (before adjustment)
	 * @param AdjustedLocation - Adjusted warp target (after terrain sampling)
	 */
	static void DrawWarpZAdjustment(
		UWorld* World,
		const FVector& OriginalLocation,
		const FVector& AdjustedLocation);

	/**
	 * Draw slope transition visualization
	 * Shows when character is transitioning between slopes of different angles
	 *
	 * @param World - World context
	 * @param CharacterLocation - Current character location
	 * @param CurrentFloorNormal - Floor normal at current position
	 * @param TargetFloorNormal - Floor normal at target position
	 * @param TargetLocation - Target position being moved toward
	 */
	static void DrawSlopeTransition(
		UWorld* World,
		const FVector& CharacterLocation,
		const FVector& CurrentFloorNormal,
		const FVector& TargetFloorNormal,
		const FVector& TargetLocation);

	// ========================================================================
	// ENVIRONMENTAL AWARENESS HELPERS
	// ========================================================================

	/**
	 * Sample ground at a world location
	 * Performs downward trace to find floor and calculate slope info
	 *
	 * @param World - World context
	 * @param Location - XY position to sample ground at
	 * @param TraceStartOffset - How far above Location to start trace (default 100)
	 * @param TraceDistance - How far down to trace (default 500)
	 * @param ActorToIgnore - Actor to exclude from trace (typically the character)
	 * @return Ground sample result with floor info
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Environment")
	static FGroundSampleResult SampleGroundAtLocation(
		UWorld* World,
		const FVector& Location,
		float TraceStartOffset = 100.0f,
		float TraceDistance = 500.0f,
		AActor* ActorToIgnore = nullptr);

	/**
	 * Adjust a location's Z to match ground height
	 * Returns the location with Z adjusted to be on the ground + offset
	 *
	 * @param World - World context
	 * @param Location - Location to adjust
	 * @param HeightOffset - Offset above ground (typically capsule half-height)
	 * @param ActorToIgnore - Actor to exclude from trace
	 * @param bDrawDebug - Whether to draw debug visualization
	 * @return Adjusted location, or original if no ground found
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Environment")
	static FVector AdjustLocationToGround(
		UWorld* World,
		const FVector& Location,
		float HeightOffset,
		AActor* ActorToIgnore = nullptr,
		bool bDrawDebug = false);

	/**
	 * Calculate slope angle from floor normal
	 *
	 * @param FloorNormal - Normal of the floor surface
	 * @return Slope angle in degrees (0 = flat, 90 = vertical)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Environment")
	static float CalculateSlopeAngle(const FVector& FloorNormal);

	/**
	 * Check if a slope is walkable (within walkable floor angle)
	 *
	 * @param FloorNormal - Normal of the floor surface
	 * @param WalkableFloorAngle - Maximum walkable angle (default 45 degrees)
	 * @return True if slope is walkable
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Environment")
	static bool IsSlopeWalkable(const FVector& FloorNormal, float WalkableFloorAngle = 45.0f);

	/**
	 * Get the floor normal at an actor's location using CharacterMovementComponent
	 * More accurate than manual traces for grounded characters
	 *
	 * @param Character - Character to query floor for
	 * @return Floor normal, or UpVector if not on ground
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Environment")
	static FVector GetCharacterFloorNormal(ACharacter* Character);

	/**
	 * Check if character is currently floating (capsule above walkable floor)
	 * Useful for detecting when character needs ground snap after motion warp
	 *
	 * @param Character - Character to check
	 * @param FloatThreshold - Distance above ground to consider "floating" (default 10 units)
	 * @return True if character is floating above ground
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Environment")
	static bool IsCharacterFloating(ACharacter* Character, float FloatThreshold = 10.0f);

	/**
	 * Snap character to ground if floating
	 * Performs ground trace and adjusts character Z position if needed
	 *
	 * @param Character - Character to snap
	 * @param FloatThreshold - Distance above ground to trigger snap (default 5 units)
	 * @param bDrawDebug - Whether to draw debug visualization
	 * @return True if character was snapped, false if already grounded or no ground found
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Environment")
	static bool SnapCharacterToGround(ACharacter* Character, float FloatThreshold = 5.0f, bool bDrawDebug = false);

	// ========================================================================
	// PAIRED ANIMATION DEBUG VISUALIZATION
	// ========================================================================

	/**
	 * Draw warp target crosshair visualization
	 * Shows target location with directional arrows and distance
	 *
	 * @param World - World context
	 * @param WarpTarget - Target location for warp
	 * @param CharacterLocation - Current character location
	 * @param bIsAttacker - True for attacker warp, false for victim warp
	 * @param Label - Optional label (e.g., "Attacker Warp", "Victim Warp")
	 */
	static void DrawWarpTargetCrosshair(
		UWorld* World,
		const FVector& WarpTarget,
		const FVector& CharacterLocation,
		bool bIsAttacker,
		const FString& Label = TEXT(""));

	/**
	 * Draw partner connection line
	 * Shows dashed line between paired animation partners
	 *
	 * @param World - World context
	 * @param AttackerLocation - Attacker position
	 * @param PartnerLocation - Partner (victim) position
	 * @param Distance - Current distance between partners
	 * @param MaxDistance - Maximum allowed distance
	 */
	static void DrawPartnerConnection(
		UWorld* World,
		const FVector& AttackerLocation,
		const FVector& PartnerLocation,
		float Distance,
		float MaxDistance);

	/**
	 * Draw sync point visualization
	 * Shows pulsing sphere at sync point location with timing info
	 *
	 * @param World - World context
	 * @param SyncLocation - Location of sync point (midpoint between partners)
	 * @param Progress - Sync point progress (0-1, 1 = at sync point)
	 * @param bAtSyncPoint - True if currently at sync point
	 * @param SyncPointName - Name of sync point for label
	 */
	static void DrawSyncPoint(
		UWorld* World,
		const FVector& SyncLocation,
		float Progress,
		bool bAtSyncPoint,
		const FName& SyncPointName);

	/**
	 * Draw finisher vulnerability indicator above target
	 * Shows reason for vulnerability (health, guard break, stun)
	 *
	 * @param World - World context
	 * @param TargetLocation - Location of vulnerable target
	 * @param VulnerabilityReason - Why target is vulnerable
	 * @param HealthPercent - Current health percentage
	 */
	static void DrawVulnerabilityIndicator(
		UWorld* World,
		const FVector& TargetLocation,
		const FString& VulnerabilityReason,
		float HealthPercent);

	/**
	 * Draw finisher range circle around attacker
	 * Shows maximum distance for finisher initiation
	 *
	 * @param World - World context
	 * @param CenterLocation - Attacker location (center of circle)
	 * @param MaxRange - Maximum finisher range
	 * @param CurrentDistance - Current distance to target (for color coding)
	 */
	static void DrawFinisherRangeCircle(
		UWorld* World,
		const FVector& CenterLocation,
		float MaxRange,
		float CurrentDistance);

	/**
	 * Draw warp offset arrow
	 * Shows the configured offset direction and magnitude
	 *
	 * @param World - World context
	 * @param FromLocation - Start location (character position)
	 * @param Offset - Offset vector in world space
	 * @param Label - Label for the offset
	 */
	static void DrawWarpOffsetArrow(
		UWorld* World,
		const FVector& FromLocation,
		const FVector& Offset,
		const FString& Label = TEXT("Offset"));

	/**
	 * Draw alignment validation indicator
	 * Shows whether sync point alignment is within tolerance
	 *
	 * @param World - World context
	 * @param AttackerLocation - Attacker position
	 * @param VictimLocation - Victim position
	 * @param ActualDistance - Current distance between them
	 * @param MaxDistance - Maximum allowed distance
	 * @param bIsAligned - Whether alignment is acceptable
	 */
	static void DrawAlignmentValidation(
		UWorld* World,
		const FVector& AttackerLocation,
		const FVector& VictimLocation,
		float ActualDistance,
		float MaxDistance,
		bool bIsAligned);
};
