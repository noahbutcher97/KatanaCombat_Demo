// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/CombatMathEnums.h"
#include "Math/CombatMathTypes.h"
#include "SkeletalAnalysisLibrary.generated.h"

// Forward declarations
class USkeletalMeshComponent;
class UAnimInstance;

/**
 * Configuration for standard humanoid skeleton bone names
 * Allows remapping for different skeleton standards
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FHumanoidBoneMapping
{
    GENERATED_BODY()

    // Spine chain
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName Pelvis = TEXT("pelvis");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName Spine01 = TEXT("spine_01");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName Spine02 = TEXT("spine_02");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName Spine03 = TEXT("spine_03");

    // Head/Neck
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName Neck = TEXT("neck_01");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName Head = TEXT("head");

    // Left Arm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName LeftClavicle = TEXT("clavicle_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName LeftUpperArm = TEXT("upperarm_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName LeftLowerArm = TEXT("lowerarm_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName LeftHand = TEXT("hand_l");

    // Right Arm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName RightClavicle = TEXT("clavicle_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName RightUpperArm = TEXT("upperarm_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName RightLowerArm = TEXT("lowerarm_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName RightHand = TEXT("hand_r");

    // Left Leg
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName LeftThigh = TEXT("thigh_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName LeftCalf = TEXT("calf_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName LeftFoot = TEXT("foot_l");

    // Right Leg
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName RightThigh = TEXT("thigh_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName RightCalf = TEXT("calf_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Mapping")
    FName RightFoot = TEXT("foot_r");

    /** Get bone name by chain type and handedness */
    FName GetChainTip(EBoneChainType ChainType, EHandedness Hand = EHandedness::Right) const;
};

/**
 * Skeletal Analysis Utility Library
 *
 * Static utility functions for skeletal mesh analysis:
 * - Build skeletal hierarchies from mesh components
 * - Extract bone chains for IK and reach analysis
 * - Calculate reach envelopes for limbs
 * - Compute center of mass for stability analysis
 * - Get anatomical regions for contact point targeting
 *
 * Design: Foundation for paired animation contact point alignment
 * and procedural IK corrections.
 */
UCLASS()
class KATANACOMBAT_API USkeletalAnalysisLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ========================================================================
    // HIERARCHY BUILDING
    // ========================================================================

    /**
     * Build complete skeletal hierarchy from mesh component
     * Caches parent-child relationships and bone lengths
     *
     * @param MeshComponent - Skeletal mesh to analyze
     * @return Complete skeletal hierarchy
     */
    UFUNCTION(BlueprintCallable, Category = "Skeletal|Hierarchy")
    static FSkeletalHierarchy BuildSkeletalHierarchy(USkeletalMeshComponent* MeshComponent);

    /**
     * Validate that hierarchy contains expected humanoid bones
     *
     * @param Hierarchy - Hierarchy to validate
     * @param BoneMapping - Expected bone names
     * @param OutMissingBones - Names of missing bones
     * @return True if all expected bones are present
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Hierarchy")
    static bool ValidateHumanoidHierarchy(
        const FSkeletalHierarchy& Hierarchy,
        const FHumanoidBoneMapping& BoneMapping,
        TArray<FName>& OutMissingBones);

    /**
     * Get all children of a bone (recursive)
     *
     * @param Hierarchy - Skeletal hierarchy
     * @param BoneName - Parent bone name
     * @param bRecursive - Include grandchildren and beyond
     * @return Array of child bone names
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Hierarchy")
    static TArray<FName> GetBoneChildren(
        const FSkeletalHierarchy& Hierarchy,
        FName BoneName,
        bool bRecursive = true);

    /**
     * Get path from root to specified bone
     *
     * @param Hierarchy - Skeletal hierarchy
     * @param BoneName - Target bone name
     * @return Array of bone names from root to target
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Hierarchy")
    static TArray<FName> GetPathToRoot(
        const FSkeletalHierarchy& Hierarchy,
        FName BoneName);

    // ========================================================================
    // BONE CHAINS
    // ========================================================================

    /**
     * Get standard bone chain by type
     *
     * @param MeshComponent - Skeletal mesh
     * @param ChainType - Type of chain to get
     * @param Hand - For arm/leg chains, which side
     * @param BoneMapping - Custom bone name mapping (uses default if not provided)
     * @return Bone chain from root to tip
     */
    UFUNCTION(BlueprintCallable, Category = "Skeletal|Chains")
    static FPhysicsBoneChain GetBoneChain(
        USkeletalMeshComponent* MeshComponent,
        EBoneChainType ChainType,
        EHandedness Hand = EHandedness::Right,
        const FHumanoidBoneMapping& BoneMapping = FHumanoidBoneMapping());

    /**
     * Build custom bone chain from start to end bone
     *
     * @param MeshComponent - Skeletal mesh
     * @param StartBone - Root of chain
     * @param EndBone - Tip of chain
     * @return Custom bone chain
     */
    UFUNCTION(BlueprintCallable, Category = "Skeletal|Chains")
    static FPhysicsBoneChain BuildCustomChain(
        USkeletalMeshComponent* MeshComponent,
        FName StartBone,
        FName EndBone);

    /**
     * Calculate total length of a bone chain
     *
     * @param MeshComponent - Skeletal mesh
     * @param Chain - Bone chain to measure
     * @return Total length in world units
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Chains")
    static float GetChainLength(
        USkeletalMeshComponent* MeshComponent,
        const FPhysicsBoneChain& Chain);

    /**
     * Get bone transforms along chain at current pose
     *
     * @param MeshComponent - Skeletal mesh
     * @param Chain - Bone chain
     * @return Array of bone transforms in component space
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Chains")
    static TArray<FTransform> GetChainTransforms(
        USkeletalMeshComponent* MeshComponent,
        const FPhysicsBoneChain& Chain);

    // ========================================================================
    // REACH ANALYSIS
    // ========================================================================

    /**
     * Check if target is reachable by bone chain
     *
     * @param MeshComponent - Skeletal mesh
     * @param Chain - Bone chain to check
     * @param TargetLocation - World space target
     * @param ComfortableReachRatio - Ratio of max reach considered comfortable (default 0.8)
     * @return Reach query result with details
     */
    UFUNCTION(BlueprintCallable, Category = "Skeletal|Reach")
    static FReachQueryResult QueryReach(
        USkeletalMeshComponent* MeshComponent,
        const FPhysicsBoneChain& Chain,
        const FVector& TargetLocation,
        float ComfortableReachRatio = 0.8f);

    /**
     * Check if target is reachable by standard limb
     *
     * @param MeshComponent - Skeletal mesh
     * @param ChainType - Which limb to check
     * @param Hand - For arm chains, which arm
     * @param TargetLocation - World space target
     * @return Reach query result
     */
    UFUNCTION(BlueprintCallable, Category = "Skeletal|Reach")
    static FReachQueryResult QueryLimbReach(
        USkeletalMeshComponent* MeshComponent,
        EBoneChainType ChainType,
        EHandedness Hand,
        const FVector& TargetLocation);

    /**
     * Calculate reach envelope for bone chain (max reach sphere)
     *
     * @param MeshComponent - Skeletal mesh
     * @param Chain - Bone chain
     * @return Center (chain root) and radius (total chain length)
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Reach")
    static void GetReachEnvelope(
        USkeletalMeshComponent* MeshComponent,
        const FPhysicsBoneChain& Chain,
        FVector& OutCenter,
        float& OutMaxRadius,
        float& OutComfortableRadius);

    // ========================================================================
    // CENTER OF MASS
    // ========================================================================

    /**
     * Calculate approximate center of mass for skeleton
     * Uses bone positions weighted by approximate body segment masses
     *
     * @param MeshComponent - Skeletal mesh
     * @param BoneMapping - Bone name mapping
     * @return Center of mass result with stability info
     */
    UFUNCTION(BlueprintCallable, Category = "Skeletal|Physics")
    static FCenterOfMassResult CalculateCenterOfMass(
        USkeletalMeshComponent* MeshComponent,
        const FHumanoidBoneMapping& BoneMapping = FHumanoidBoneMapping());

    /**
     * Calculate center of mass for specific bones
     *
     * @param MeshComponent - Skeletal mesh
     * @param BoneNames - Bones to include
     * @param BoneWeights - Optional weights (equal if empty)
     * @return Weighted center position
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Physics")
    static FVector CalculateWeightedCenter(
        USkeletalMeshComponent* MeshComponent,
        const TArray<FName>& BoneNames,
        const TArray<float>& BoneWeights);

    /**
     * Check if center of mass is over support polygon (feet)
     *
     * @param MeshComponent - Skeletal mesh
     * @param COMLocation - Center of mass location
     * @param BoneMapping - Bone name mapping
     * @return Stability state
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Physics")
    static EStabilityState CheckStability(
        USkeletalMeshComponent* MeshComponent,
        const FVector& COMLocation,
        const FHumanoidBoneMapping& BoneMapping = FHumanoidBoneMapping());

    // ========================================================================
    // ANATOMICAL REGIONS
    // ========================================================================

    /**
     * Get anatomical region for bone
     *
     * @param BoneName - Bone to classify
     * @param BoneMapping - Bone name mapping
     * @return Anatomical region
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Anatomy")
    static EAnatomicalRegion GetBoneRegion(
        FName BoneName,
        const FHumanoidBoneMapping& BoneMapping = FHumanoidBoneMapping());

    /**
     * Get all bones in anatomical region
     *
     * @param MeshComponent - Skeletal mesh
     * @param Region - Region to query
     * @param Hand - For sided regions (arm, leg)
     * @param BoneMapping - Bone name mapping
     * @return Array of bone names in region
     */
    UFUNCTION(BlueprintCallable, Category = "Skeletal|Anatomy")
    static TArray<FName> GetBonesInRegion(
        USkeletalMeshComponent* MeshComponent,
        EAnatomicalRegion Region,
        EHandedness Hand = EHandedness::Both,
        const FHumanoidBoneMapping& BoneMapping = FHumanoidBoneMapping());

    /**
     * Find closest bone to world location
     *
     * @param MeshComponent - Skeletal mesh
     * @param WorldLocation - Location to check
     * @param BonesToCheck - Bones to consider (all if empty)
     * @return Closest bone name
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Anatomy")
    static FName FindClosestBone(
        USkeletalMeshComponent* MeshComponent,
        const FVector& WorldLocation,
        const TArray<FName>& BonesToCheck);

    // ========================================================================
    // BONE TRANSFORMS
    // ========================================================================

    /**
     * Get bone transform in world space
     *
     * @param MeshComponent - Skeletal mesh
     * @param BoneName - Bone name
     * @return World space transform (identity if not found)
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Transform")
    static FTransform GetBoneWorldTransform(
        USkeletalMeshComponent* MeshComponent,
        FName BoneName);

    /**
     * Get bone transform in component space
     *
     * @param MeshComponent - Skeletal mesh
     * @param BoneName - Bone name
     * @return Component space transform (identity if not found)
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Transform")
    static FTransform GetBoneComponentTransform(
        USkeletalMeshComponent* MeshComponent,
        FName BoneName);

    /**
     * Get distance between two bones
     *
     * @param MeshComponent - Skeletal mesh
     * @param BoneA - First bone
     * @param BoneB - Second bone
     * @return Distance between bone positions
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Transform")
    static float GetDistanceBetweenBones(
        USkeletalMeshComponent* MeshComponent,
        FName BoneA,
        FName BoneB);

    /**
     * Get direction from one bone to another
     *
     * @param MeshComponent - Skeletal mesh
     * @param FromBone - Source bone
     * @param ToBone - Target bone
     * @return Normalized direction vector
     */
    UFUNCTION(BlueprintPure, Category = "Skeletal|Transform")
    static FVector GetDirectionBetweenBones(
        USkeletalMeshComponent* MeshComponent,
        FName FromBone,
        FName ToBone);

private:
    /** Default body segment mass ratios (percentage of total body mass) */
    static const TMap<EAnatomicalRegion, float>& GetDefaultMassRatios();
};
