// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Data/PairedAnimationEditorTypes.h"
#include "PairedAnimationAnalysisSubsystem.generated.h"

class UDebugSkelMeshComponent;
class UAnimMontage;

/**
 * Editor subsystem for paired animation analysis.
 *
 * Owns the analysis context and provides methods for:
 * - Configuration evaluation (single-frame and holistic)
 * - Optimization algorithms (distance, rotation)
 * - Spatial relationship inference
 * - Contact point analysis
 *
 * This subsystem handles all UObject interactions, while pure math
 * functions remain in UPairedAnimationAnalysisLibrary.
 *
 * Usage:
 *   UPairedAnimationAnalysisSubsystem* Subsystem = GEditor->GetEditorSubsystem<UPairedAnimationAnalysisSubsystem>();
 *   Subsystem->SetupContext(AttackerMesh, VictimMesh, AttackerMontage, VictimMontage);
 *   FFullOptimizationResult Result = Subsystem->RunFullOptimization();
 */
UCLASS()
class KATANACOMBATEDITOR_API UPairedAnimationAnalysisSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// ========================================================================
	// LIFECYCLE
	// ========================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ========================================================================
	// CONTEXT SETUP
	// ========================================================================

	/**
	 * Initialize the analysis context with mesh components and montages.
	 * Must be called before any analysis functions.
	 *
	 * @param AttackerMesh Skeletal mesh component for the attacker
	 * @param VictimMesh Skeletal mesh component for the victim
	 * @param AttackerMontage Animation montage for the attacker
	 * @param VictimMontage Animation montage for the victim
	 * @return true if context was successfully initialized
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Analysis")
	bool SetupContext(
		UDebugSkelMeshComponent* AttackerMesh,
		UDebugSkelMeshComponent* VictimMesh,
		UAnimMontage* AttackerMontage,
		UAnimMontage* VictimMontage);

	/**
	 * Update context configuration parameters.
	 *
	 * @param Distance Distance between character roots
	 * @param AttackerRotation Attacker yaw rotation
	 * @param VictimRotation Victim yaw rotation
	 * @param VictimTimeOffset Time offset for victim montage
	 * @param ContactThreshold Distance threshold for contact detection
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Analysis")
	void UpdateContextConfiguration(
		float Distance,
		FRotator AttackerRotation,
		FRotator VictimRotation,
		float VictimTimeOffset = 0.0f,
		float ContactThreshold = 50.0f);

	/**
	 * Clear the analysis context, releasing all references.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Analysis")
	void ClearContext();

	/**
	 * Check if the context is valid and ready for analysis.
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Analysis")
	bool IsContextValid() const;

	// ========================================================================
	// CONFIGURATION EVALUATION
	// ========================================================================

	/**
	 * Evaluate a configuration at a single frame.
	 * Used for finding Global Paired Orientation (starting positions).
	 *
	 * @param Distance Distance between character roots
	 * @param AttackerRotation Attacker yaw rotation
	 * @param VictimRotation Victim yaw rotation
	 * @param Time Animation time to evaluate at (0.0 for starting orientation)
	 * @return Evaluation result with score and quality metrics
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Analysis")
	FConfigurationEvaluationResult EvaluateConfigurationAtFrame(
		float Distance,
		FRotator AttackerRotation,
		FRotator VictimRotation,
		float Time = 0.0f);

	/**
	 * Evaluate a configuration holistically across the full animation.
	 * Used for analyzing pose drift and contact quality over time.
	 *
	 * @param Distance Distance between character roots
	 * @param AttackerRotation Attacker yaw rotation
	 * @param VictimRotation Victim yaw rotation
	 * @param NumSamples Number of frames to sample (default 30)
	 * @return Holistic evaluation with per-frame scores
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Analysis")
	FHolisticEvaluationResult EvaluateConfigurationHolistic(
		float Distance,
		FRotator AttackerRotation,
		FRotator VictimRotation,
		int32 NumSamples = 30);

	/**
	 * Build a full contact analysis timeline.
	 * Samples contact quality at regular intervals across the animation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Analysis")
	FContactAnalysisTimeline BuildContactTimeline(int32 NumSamples = 30);

	// ========================================================================
	// OPTIMIZATION
	// ========================================================================

	/**
	 * Find optimal distance for Global Paired Orientation.
	 * Evaluates at reference frame (t=0).
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Optimization")
	FDistanceOptimizationResult FindOptimalDistance(
		float MinDistance = 50.0f,
		float MaxDistance = 400.0f,
		int32 Steps = 50,
		float ReferenceTime = 0.0f);

	/**
	 * Find optimal rotation for a character.
	 * Evaluates at reference frame (t=0).
	 *
	 * @param bOptimizeAttacker true = optimize attacker rotation, false = optimize victim rotation
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Optimization")
	FRotationOptimizationResult FindOptimalRotation(
		bool bOptimizeAttacker,
		int32 Steps = 36,
		float ReferenceTime = 0.0f);

	/**
	 * Run full optimization to find Global Paired Orientation.
	 * Finds optimal distance, then attacker rotation, then victim rotation.
	 * All evaluated at reference frame (t=0).
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Optimization")
	FFullOptimizationResult RunFullOptimization(
		float MinDistance = 50.0f,
		float MaxDistance = 400.0f,
		int32 DistanceSteps = 50,
		int32 RotationSteps = 36,
		float ReferenceTime = 0.0f);

	// ========================================================================
	// SPATIAL RELATIONSHIP
	// ========================================================================

	/**
	 * Infer the intended spatial relationship from animation data.
	 * Analyzes contact points and bone orientations at the sync point.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Analysis")
	FSpatialRelationshipInference InferSpatialRelationship(float AnalysisTime = 0.0f);

	/**
	 * Set the spatial relationship constraint for optimization.
	 * When set, optimization will constrain rotations based on the relationship.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Analysis")
	void SetSpatialRelationshipConstraint(ESpatialRelationship Relationship);

	/**
	 * Get the current spatial relationship constraint.
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Analysis")
	ESpatialRelationship GetSpatialRelationshipConstraint() const;

	// ========================================================================
	// CONTACT ANALYSIS
	// ========================================================================

	/**
	 * Compute closest bone distance between two skeletons at current pose.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Analysis")
	float ComputeClosestBoneDistance(FName& OutAttackerBone, FName& OutVictimBone);

	/**
	 * Analyze contact at a specific frame.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Analysis")
	FContactAnalysisSnapshot AnalyzeContactAtFrame(float Time);

	// ========================================================================
	// MESH UTILITIES
	// ========================================================================

	/**
	 * Apply a configuration to the mesh components.
	 * Sets positions and rotations based on provided values.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Utility")
	void ApplyConfiguration(
		float Distance,
		FRotator AttackerRotation,
		FRotator VictimRotation);

	/**
	 * Update mesh animations to a specific time.
	 * Refreshes bone transforms for accurate position queries.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paired Animation|Utility")
	void UpdateAnimationTime(float Time);

	/**
	 * Get bone world location from attacker mesh.
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Utility")
	FVector GetAttackerBoneWorldLocation(FName BoneName) const;

	/**
	 * Get bone world location from victim mesh.
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Utility")
	FVector GetVictimBoneWorldLocation(FName BoneName) const;

	/**
	 * Get all bone names from the attacker skeleton.
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Utility")
	TArray<FName> GetAttackerBoneNames() const;

	/**
	 * Get all bone names from the victim skeleton.
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Utility")
	TArray<FName> GetVictimBoneNames() const;

	// ========================================================================
	// CONTEXT ACCESS (for advanced use cases)
	// ========================================================================

	/**
	 * Get read-only access to the current context.
	 * Prefer using the subsystem methods over direct context access.
	 */
	UFUNCTION(BlueprintPure, Category = "Paired Animation|Analysis")
	const FPairedAnimationAnalysisContext& GetContext() const { return Context; }

protected:
	/** Internal analysis context - owns the UObject references */
	FPairedAnimationAnalysisContext Context;

	/** Current spatial relationship constraint for optimization */
	ESpatialRelationship SpatialConstraint = ESpatialRelationship::Inferred;

	/** Cached holistic analysis for optimization consistency */
	TOptional<FHolisticEvaluationResult> CachedHolisticAnalysis;

	/** Whether cached analysis needs refresh */
	bool bAnalysisCacheDirty = true;

	// ========================================================================
	// INTERNAL HELPERS
	// ========================================================================

	/** Validate context before analysis operations */
	bool ValidateContextForAnalysis() const;

	/** Get rotation constraints based on current spatial relationship */
	void GetCurrentRotationConstraints(float& OutMinYaw, float& OutMaxYaw, float& OutTolerance) const;

	/** Invalidate cached analysis (call when context changes) */
	void InvalidateCache();
};
