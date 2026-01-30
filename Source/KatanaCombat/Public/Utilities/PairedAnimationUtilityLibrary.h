// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/PairedAnimationTypes.h"
#include "PairedAnimationUtilityLibrary.generated.h"

// Forward declarations
class UPairedAnimationData;
class USkeletalMeshComponent;
class UCapsuleComponent;

/**
 * Result of obstacle validation for paired animations
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FPairedAnimationValidation
{
    GENERATED_BODY()

    /** Is the position valid for paired animation? */
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    bool bIsValid = false;

    /** Reason for validation failure (empty if valid) */
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    FString FailureReason;

    /** Suggested adjusted position if original failed */
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    FTransform SuggestedTransform;

    /** Distance between attacker and victim */
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    float Distance = 0.0f;

    /** Angle from attacker's forward to victim */
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    float Angle = 0.0f;
};

/**
 * Paired Animation Utility Library
 *
 * Static utility functions for paired animation calculations:
 * - Victim positioning relative to attacker
 * - Obstacle validation (prevent clipping through walls)
 * - Terrain adjustment (prevent floating)
 * - Contact point calculation
 * - Distance and angle validation
 *
 * Design: Supports AC3-style paired animations with Ghost of Tsushima
 * environmental awareness
 */
UCLASS()
class KATANACOMBAT_API UPairedAnimationUtilityLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ========================================================================
    // POSITION CALCULATION
    // ========================================================================

    /**
     * Calculate victim's world transform for paired animation
     * Applies relative offset and rotation from attacker
     *
     * @param AttackerTransform - Attacker's current world transform
     * @param RelativePosition - Victim offset in attacker's local space (from PairedAnimationData)
     * @param VictimFacingMode - -1 = face attacker, 1 = face away, 0 = use RelativeRotation
     * @param RelativeRotation - Fixed rotation if VictimFacingMode == 0
     * @return Victim's target world transform
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Position")
    static FTransform CalculateVictimTransform(
        const FTransform& AttackerTransform,
        const FVector& RelativePosition,
        int32 VictimFacingMode = -1,
        const FRotator& RelativeRotation = FRotator::ZeroRotator);

    /**
     * Calculate victim transform from paired animation data
     * Convenience wrapper using UPairedAnimationData
     *
     * @param AttackerTransform - Attacker's current world transform
     * @param AnimationData - Paired animation configuration
     * @return Victim's target world transform
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Position")
    static FTransform CalculateVictimTransformFromData(
        const FTransform& AttackerTransform,
        const UPairedAnimationData* AnimationData);

    /**
     * Get interpolated position between current and target
     * Useful for smooth victim repositioning
     *
     * @param CurrentTransform - Current transform
     * @param TargetTransform - Target transform
     * @param Alpha - Interpolation factor (0-1)
     * @return Interpolated transform
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Position")
    static FTransform InterpolateTransform(
        const FTransform& CurrentTransform,
        const FTransform& TargetTransform,
        float Alpha);

    // ========================================================================
    // VALIDATION
    // ========================================================================

    /**
     * Validate if paired animation is possible between two actors
     * Checks distance, obstacles, and terrain
     *
     * @param World - World context
     * @param AttackerLocation - Attacker position
     * @param VictimLocation - Victim position
     * @param AnimationData - Paired animation configuration
     * @param ClearanceRadius - Radius for obstacle check (default 50)
     * @return Validation result with details
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Validation")
    static FPairedAnimationValidation ValidatePairedAnimation(
        UWorld* World,
        const FVector& AttackerLocation,
        const FVector& VictimLocation,
        const UPairedAnimationData* AnimationData,
        float ClearanceRadius = 50.0f);

    /**
     * Check if path between attacker and victim is clear
     * Prevents warping through walls
     *
     * @param World - World context
     * @param Start - Start position (typically attacker)
     * @param End - End position (typically victim target)
     * @param ClearanceRadius - Radius for sweep trace
     * @param ActorsToIgnore - Actors to exclude from trace
     * @return True if path is clear
     */
    UFUNCTION(BlueprintCallable, Category = "Paired Animation|Validation")
    static bool IsPathClear(
        UWorld* World,
        const FVector& Start,
        const FVector& End,
        float ClearanceRadius,
        const TArray<AActor*>& ActorsToIgnore);

    /**
     * Check if position has enough clearance for character
     * Validates there's room for the animation
     *
     * @param World - World context
     * @param Location - Position to check
     * @param CapsuleRadius - Character capsule radius
     * @param CapsuleHalfHeight - Character capsule half-height
     * @param ActorToIgnore - Actor to exclude from overlap check
     * @return True if position is clear
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Validation")
    static bool IsPositionClear(
        UWorld* World,
        const FVector& Location,
        float CapsuleRadius,
        float CapsuleHalfHeight,
        AActor* ActorToIgnore = nullptr);

    /**
     * Check if victim is in valid angle range relative to attacker
     * Prevents triggering finishers on enemies behind you
     *
     * @param AttackerTransform - Attacker's transform
     * @param VictimLocation - Victim's location
     * @param MaxAngle - Maximum angle from attacker's forward (degrees)
     * @return True if victim is within angle range
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Validation")
    static bool IsVictimInAngleRange(
        const FTransform& AttackerTransform,
        const FVector& VictimLocation,
        float MaxAngle = 90.0f);

    // ========================================================================
    // TERRAIN ADJUSTMENT
    // ========================================================================

    /**
     * Adjust transform to match terrain height
     * Prevents floating during paired animations
     *
     * @param World - World context
     * @param Transform - Transform to adjust
     * @param HeightOffset - Offset above ground (typically capsule half-height)
     * @param ActorToIgnore - Actor to exclude from ground trace
     * @return Adjusted transform with corrected Z
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Terrain")
    static FTransform AdjustTransformToTerrain(
        UWorld* World,
        const FTransform& Transform,
        float HeightOffset,
        AActor* ActorToIgnore = nullptr);

    /**
     * Get terrain-adjusted victim position
     * Full calculation from attacker position to terrain-adjusted victim target
     *
     * @param World - World context
     * @param AttackerTransform - Attacker's current transform
     * @param AnimationData - Paired animation configuration
     * @param VictimCapsuleHalfHeight - Victim's capsule half-height for ground offset
     * @param VictimActor - Victim actor to ignore in traces
     * @return Terrain-adjusted victim transform
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Terrain")
    static FTransform GetTerrainAdjustedVictimTransform(
        UWorld* World,
        const FTransform& AttackerTransform,
        const UPairedAnimationData* AnimationData,
        float VictimCapsuleHalfHeight,
        AActor* VictimActor = nullptr);

    // ========================================================================
    // CONTACT POINTS
    // ========================================================================

    /**
     * Get bone location for contact point alignment
     * Used for IK contact point calculations (Batman-style)
     *
     * @param Mesh - Skeletal mesh component
     * @param BoneName - Name of bone to get location for
     * @return World location of bone, or zero vector if not found
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Contact")
    static FVector GetBoneWorldLocation(
        USkeletalMeshComponent* Mesh,
        FName BoneName);

    /**
     * Calculate contact point between attacker and victim
     * Used for determining impact location during paired animations
     *
     * @param AttackerMesh - Attacker's skeletal mesh
     * @param AttackerBoneName - Attacker's contact bone (e.g., "weapon_r" or "hand_r")
     * @param VictimMesh - Victim's skeletal mesh
     * @param VictimBoneName - Victim's contact bone (e.g., "spine_03" or "chest")
     * @return World location for contact point (midpoint between bones)
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Contact")
    static FVector CalculateContactPoint(
        USkeletalMeshComponent* AttackerMesh,
        FName AttackerBoneName,
        USkeletalMeshComponent* VictimMesh,
        FName VictimBoneName);

    // ========================================================================
    // DISTANCE CALCULATIONS
    // ========================================================================

    /**
     * Get horizontal distance between two actors (ignores Z)
     *
     * @param Location1 - First location
     * @param Location2 - Second location
     * @return 2D horizontal distance
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Distance")
    static float GetHorizontalDistance(
        const FVector& Location1,
        const FVector& Location2);

    /**
     * Check if victim is within trigger distance range
     *
     * @param AttackerLocation - Attacker position
     * @param VictimLocation - Victim position
     * @param AnimationData - Paired animation configuration
     * @return True if within min/max trigger distance
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Distance")
    static bool IsInTriggerRange(
        const FVector& AttackerLocation,
        const FVector& VictimLocation,
        const UPairedAnimationData* AnimationData);

    /**
     * Check if victim is close enough to warp to position
     *
     * @param VictimLocation - Current victim location
     * @param TargetLocation - Target warp location
     * @param MaxWarpDistance - Maximum allowed warp distance
     * @return True if warp distance is within limit
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Distance")
    static bool IsWithinWarpDistance(
        const FVector& VictimLocation,
        const FVector& TargetLocation,
        float MaxWarpDistance);

    // ========================================================================
    // WARP TARGET SETUP
    // ========================================================================

    /**
     * Calculate motion warping target for paired animation
     * Returns transform suitable for MotionWarpingComponent::AddOrUpdateWarpTargetFromLocationAndRotation
     *
     * @param TargetActor - Actor to warp toward
     * @param WarpConfig - Warp configuration
     * @param SourceActor - Actor doing the warping (for offset calculation)
     * @return Warp target transform
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Warp")
    static FTransform CalculateWarpTarget(
        AActor* TargetActor,
        const FPairedWarpConfig& WarpConfig,
        AActor* SourceActor = nullptr);

    /**
     * Calculate attacker's warp target for paired animation
     * Attacker typically only needs rotation warp toward victim
     *
     * @param VictimLocation - Victim's position
     * @param AttackerTransform - Attacker's current transform
     * @param WarpConfig - Attacker's warp configuration
     * @return Warp target for attacker
     */
    UFUNCTION(BlueprintPure, Category = "Paired Animation|Warp")
    static FTransform CalculateAttackerWarpTarget(
        const FVector& VictimLocation,
        const FTransform& AttackerTransform,
        const FPairedWarpConfig& WarpConfig);
};
