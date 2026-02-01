// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/PairedAnimationEditorTypes.h"
#include "PairedAnimationAnalysisLibrary.generated.h"

/**
 * Static utility library for paired animation analysis - PURE MATH ONLY.
 *
 * This library contains only stateless pure functions that:
 * - Take primitives, enums, or pure data structs as input
 * - Return primitives, enums, or pure data structs as output
 * - Have no side effects
 * - Do not reference UObjects
 *
 * For operations that require UObject access (mesh components, montages),
 * use UPairedAnimationAnalysisSubsystem instead.
 *
 * Functions provided:
 * - Rotation constraint calculations
 * - Score computations (contact, weighted, consistency)
 * - Angle and direction calculations
 * - Statistical helpers
 */
UCLASS()
class KATANACOMBATEDITOR_API UPairedAnimationAnalysisLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ========================================================================
	// SPATIAL RELATIONSHIP CONSTRAINTS
	// ========================================================================

	/**
	 * Get rotation constraints for a given spatial relationship.
	 * Pure function - no UObject dependencies.
	 *
	 * @param Relationship The spatial relationship type
	 * @param OutMinYaw Minimum yaw angle for the constraint
	 * @param OutMaxYaw Maximum yaw angle for the constraint
	 * @param OutTolerance Angular tolerance for the constraint
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static void GetRelationshipConstraints(
		ESpatialRelationship Relationship,
		float& OutMinYaw,
		float& OutMaxYaw,
		float& OutTolerance);

	/**
	 * Constrain a rotation to match a spatial relationship.
	 * Returns the input rotation adjusted to fall within the relationship's valid range.
	 *
	 * @param InputRotation The rotation to constrain
	 * @param Relationship The spatial relationship to constrain to
	 * @return Constrained rotation
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static FRotator ConstrainRotationToRelationship(
		FRotator InputRotation,
		ESpatialRelationship Relationship);

	/**
	 * Check if a rotation is valid for a given spatial relationship.
	 *
	 * @param Rotation The rotation to check
	 * @param Relationship The spatial relationship
	 * @return true if rotation falls within valid range
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static bool IsRotationValidForRelationship(
		FRotator Rotation,
		ESpatialRelationship Relationship);

	// ========================================================================
	// SCORE CALCULATIONS
	// ========================================================================

	/**
	 * Calculate a contact score based on distance and threshold.
	 * Score is 1.0 when distance <= threshold, falls off linearly to 0 at 3x threshold.
	 *
	 * @param Distance The measured distance
	 * @param Threshold The contact threshold
	 * @return Score in range [0, 1]
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float CalculateContactScore(float Distance, float Threshold);

	/**
	 * Calculate weighted average of scores.
	 *
	 * @param Scores Array of score values
	 * @param Weights Array of weights (must match Scores length)
	 * @return Weighted average, or 0 if arrays empty or mismatched
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float ComputeWeightedScore(
		const TArray<float>& Scores,
		const TArray<float>& Weights);

	/**
	 * Calculate consistency metric from a set of scores.
	 * Higher value means more consistent scores.
	 *
	 * @param Scores Array of score values
	 * @return Consistency in range [0, 1], where 1 = perfectly consistent
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float CalculateConsistency(const TArray<float>& Scores);

	/**
	 * Calculate activity weight for a bone based on its velocity.
	 * Used to weight contact scores by how active the animation is at that frame.
	 *
	 * @param Velocity Bone velocity magnitude
	 * @param MaxVelocity Maximum expected velocity for normalization
	 * @return Weight in range [0.1, 1.0] (never zero to avoid ignoring static poses)
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float CalculateActivityWeight(float Velocity, float MaxVelocity = 500.0f);

	// ========================================================================
	// ANGLE CALCULATIONS
	// ========================================================================

	/**
	 * Calculate the relative angle between two actors based on forward vectors.
	 * Returns the angle in degrees.
	 *
	 * @param AttackerForward Attacker's forward direction
	 * @param VictimForward Victim's forward direction
	 * @return Angle in degrees [0, 180]
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float CalculateRelativeAngle(
		FVector AttackerForward,
		FVector VictimForward);

	/**
	 * Determine spatial relationship from relative angle and attack direction.
	 *
	 * @param RelativeAngle Angle between forward vectors in degrees
	 * @param AttackDirection Direction from attacker to victim (normalized)
	 * @param VictimForward Victim's forward direction (normalized)
	 * @return Inferred spatial relationship
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static ESpatialRelationship InferRelationshipFromAngle(
		float RelativeAngle,
		FVector AttackDirection,
		FVector VictimForward);

	/**
	 * Normalize an angle to the range [-180, 180].
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float NormalizeAngle180(float Angle);

	/**
	 * Normalize an angle to the range [0, 360].
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float NormalizeAngle360(float Angle);

	/**
	 * Calculate confidence score for spatial relationship inference.
	 * Higher confidence when angle is clearly in a relationship zone.
	 *
	 * @param AngleDegrees The angle between attack direction and victim forward
	 * @return Confidence in range [0.5, 1.0]
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float CalculateSpatialInferenceConfidence(float AngleDegrees);

	/**
	 * Check if a yaw angle is within constraint tolerance.
	 * Pure validation function.
	 *
	 * @param TargetYaw The target yaw angle to validate against
	 * @param Tolerance The allowed tolerance in degrees
	 * @param TestYaw The yaw angle to test
	 * @return true if within tolerance
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static bool IsYawWithinConstraint(float TargetYaw, float Tolerance, float TestYaw);

	// ========================================================================
	// STATISTICAL HELPERS
	// ========================================================================

	/**
	 * Calculate mean of an array of values.
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float CalculateMean(const TArray<float>& Values);

	/**
	 * Calculate variance of an array of values.
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float CalculateVariance(const TArray<float>& Values);

	/**
	 * Calculate standard deviation of an array of values.
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float CalculateStandardDeviation(const TArray<float>& Values);

	/**
	 * Find minimum value in array.
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float FindMinimum(const TArray<float>& Values);

	/**
	 * Find maximum value in array.
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float FindMaximum(const TArray<float>& Values);

	// ========================================================================
	// DISTANCE CALCULATIONS
	// ========================================================================

	/**
	 * Calculate position for victim given attacker position and configuration.
	 * Pure geometric calculation.
	 *
	 * @param AttackerLocation Attacker world location
	 * @param AttackerRotation Attacker rotation
	 * @param Distance Separation distance
	 * @return Calculated victim location
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static FVector CalculateVictimPosition(
		FVector AttackerLocation,
		FRotator AttackerRotation,
		float Distance);

	/**
	 * Calculate the midpoint between two locations.
	 * Useful for contact point estimation.
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static FVector CalculateMidpoint(FVector LocationA, FVector LocationB);

	/**
	 * Calculate 2D distance (ignoring Z).
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Math")
	static float CalculateDistance2D(FVector LocationA, FVector LocationB);
};
