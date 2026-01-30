// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatMathTypes.generated.h"

/**
 * Represents a bone in a skeletal hierarchy with parent-child relationships
 * Used for skeletal analysis, reach calculation, and constraint checking
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FSkeletalBoneInfo
{
    GENERATED_BODY()

    /** Bone name */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    FName BoneName = NAME_None;

    /** Parent bone name (NAME_None for root) */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    FName ParentBoneName = NAME_None;

    /** Bone index in skeleton */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    int32 BoneIndex = INDEX_NONE;

    /** Depth in hierarchy (0 = root) */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    int32 Depth = 0;

    /** Length to parent bone (0 for root) */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    float BoneLength = 0.0f;

    /** Child bone names */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    TArray<FName> ChildBoneNames;
};

/**
 * Complete skeletal hierarchy representation
 * Cached for efficient queries during paired animations
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FSkeletalHierarchy
{
    GENERATED_BODY()

    /** Root bone name */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    FName RootBoneName = NAME_None;

    /** All bones in hierarchy order (root first, leaves last) */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    TArray<FSkeletalBoneInfo> Bones;

    /** Quick lookup: bone name -> index in Bones array */
    TMap<FName, int32> BoneNameToIndex;

    /** Total bone count */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    int32 BoneCount = 0;

    /** Maximum depth of hierarchy */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    int32 MaxDepth = 0;

    /** Is hierarchy valid? */
    bool IsValid() const { return BoneCount > 0 && RootBoneName != NAME_None; }

    /** Get bone info by name */
    const FSkeletalBoneInfo* GetBone(FName BoneName) const
    {
        if (const int32* Index = BoneNameToIndex.Find(BoneName))
        {
            return &Bones[*Index];
        }
        return nullptr;
    }
};

/**
 * Represents a chain of bones from root to tip
 * Used for reach calculation and IK chain analysis
 * Note: Named FPhysicsBoneChain to avoid conflict with IKRig's FBoneChain
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FPhysicsBoneChain
{
    GENERATED_BODY()

    /** Ordered bone names from root to tip */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    TArray<FName> BoneNames;

    /** Total length of chain (sum of bone lengths) */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    float TotalLength = 0.0f;

    /** Root bone of chain */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    FName RootBone = NAME_None;

    /** Tip bone of chain (end effector) */
    UPROPERTY(BlueprintReadOnly, Category = "Skeletal")
    FName TipBone = NAME_None;

    /** Number of bones in chain */
    int32 Num() const { return BoneNames.Num(); }

    /** Is chain valid? */
    bool IsValid() const { return BoneNames.Num() > 0 && TotalLength > 0.0f; }
};

/**
 * Result of a reach envelope query
 * Determines if a target is reachable by a limb
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FReachQueryResult
{
    GENERATED_BODY()

    /** Is target within maximum reach? */
    UPROPERTY(BlueprintReadOnly, Category = "Reach")
    bool bIsReachable = false;

    /** Is target within comfortable reach (no full extension)? */
    UPROPERTY(BlueprintReadOnly, Category = "Reach")
    bool bIsComfortablyReachable = false;

    /** Distance to target from chain root */
    UPROPERTY(BlueprintReadOnly, Category = "Reach")
    float DistanceToTarget = 0.0f;

    /** Maximum reach of the chain */
    UPROPERTY(BlueprintReadOnly, Category = "Reach")
    float MaxReach = 0.0f;

    /** Comfortable reach (typically 80% of max) */
    UPROPERTY(BlueprintReadOnly, Category = "Reach")
    float ComfortableReach = 0.0f;

    /** How much extension is required (0 = relaxed, 1 = full extension) */
    UPROPERTY(BlueprintReadOnly, Category = "Reach")
    float ExtensionRatio = 0.0f;

    /** Direction from chain root to target */
    UPROPERTY(BlueprintReadOnly, Category = "Reach")
    FVector DirectionToTarget = FVector::ZeroVector;
};

/**
 * Anatomical joint constraint definition
 * Used for IK solving and self-collision prediction
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FJointConstraint
{
    GENERATED_BODY()

    /** Joint bone name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
    FName BoneName = NAME_None;

    /** Minimum rotation (degrees) for each axis */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
    FRotator MinRotation = FRotator(-180.0f, -180.0f, -180.0f);

    /** Maximum rotation (degrees) for each axis */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
    FRotator MaxRotation = FRotator(180.0f, 180.0f, 180.0f);

    /** Twist axis (typically local X) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
    FVector TwistAxis = FVector(1.0f, 0.0f, 0.0f);

    /** Is this a hinge joint (1 DOF)? */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
    bool bIsHinge = false;

    /** Check if rotation is within constraints */
    bool IsRotationValid(const FRotator& Rotation) const
    {
        return Rotation.Pitch >= MinRotation.Pitch && Rotation.Pitch <= MaxRotation.Pitch &&
               Rotation.Yaw >= MinRotation.Yaw && Rotation.Yaw <= MaxRotation.Yaw &&
               Rotation.Roll >= MinRotation.Roll && Rotation.Roll <= MaxRotation.Roll;
    }
};

/**
 * Center of mass calculation result
 * Used for stability analysis during paired animations
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FCenterOfMassResult
{
    GENERATED_BODY()

    /** World-space center of mass location */
    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    FVector Location = FVector::ZeroVector;

    /** Total mass used in calculation */
    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    float TotalMass = 0.0f;

    /** Is COM over support polygon (stable)? */
    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    bool bIsStable = true;

    /** Distance from COM projection to nearest support edge */
    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    float StabilityMargin = 0.0f;
};

/**
 * Bone transform at a specific animation frame
 * Used for contact point prediction and editor analysis
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FBoneFrameTransform
{
    GENERATED_BODY()

    /** Bone name */
    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    FName BoneName = NAME_None;

    /** Animation time (seconds) */
    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    float Time = 0.0f;

    /** Component-space transform */
    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    FTransform ComponentSpaceTransform = FTransform::Identity;

    /** World-space transform (if calculated) */
    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    FTransform WorldTransform = FTransform::Identity;

    /** Velocity at this frame (if available) */
    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    FVector Velocity = FVector::ZeroVector;
};

/**
 * Contact point prediction result
 * Used for paired animation alignment and IK targeting
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FContactPointPrediction
{
    GENERATED_BODY()

    /** Predicted contact location in world space */
    UPROPERTY(BlueprintReadOnly, Category = "Contact")
    FVector Location = FVector::ZeroVector;

    /** Predicted contact normal */
    UPROPERTY(BlueprintReadOnly, Category = "Contact")
    FVector Normal = FVector::UpVector;

    /** Time at which contact occurs (animation time) */
    UPROPERTY(BlueprintReadOnly, Category = "Contact")
    float ContactTime = 0.0f;

    /** Attacker bone making contact */
    UPROPERTY(BlueprintReadOnly, Category = "Contact")
    FName AttackerBone = NAME_None;

    /** Victim bone receiving contact */
    UPROPERTY(BlueprintReadOnly, Category = "Contact")
    FName VictimBone = NAME_None;

    /** Confidence of prediction (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Contact")
    float Confidence = 0.0f;

    /** Distance between bones at predicted contact */
    UPROPERTY(BlueprintReadOnly, Category = "Contact")
    float ContactDistance = 0.0f;
};

/**
 * Result of a spatial query (sphere, box, cone overlap)
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FSpatialQueryResult
{
    GENERATED_BODY()

    /** Actors found in query */
    UPROPERTY(BlueprintReadOnly, Category = "Spatial")
    TArray<AActor*> Actors;

    /** Components found in query */
    UPROPERTY(BlueprintReadOnly, Category = "Spatial")
    TArray<UPrimitiveComponent*> Components;

    /** Distances to query origin */
    UPROPERTY(BlueprintReadOnly, Category = "Spatial")
    TArray<float> Distances;

    /** Query was successful? */
    UPROPERTY(BlueprintReadOnly, Category = "Spatial")
    bool bSuccess = false;

    /** Number of results */
    int32 Num() const { return Actors.Num(); }
};

/**
 * Distance query result between two skeletal meshes
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FSkeletalDistanceResult
{
    GENERATED_BODY()

    /** Closest bone on first skeleton */
    UPROPERTY(BlueprintReadOnly, Category = "Distance")
    FName BoneA = NAME_None;

    /** Closest bone on second skeleton */
    UPROPERTY(BlueprintReadOnly, Category = "Distance")
    FName BoneB = NAME_None;

    /** Closest point on bone A */
    UPROPERTY(BlueprintReadOnly, Category = "Distance")
    FVector PointA = FVector::ZeroVector;

    /** Closest point on bone B */
    UPROPERTY(BlueprintReadOnly, Category = "Distance")
    FVector PointB = FVector::ZeroVector;

    /** Distance between closest points */
    UPROPERTY(BlueprintReadOnly, Category = "Distance")
    float Distance = 0.0f;

    /** Direction from A to B (normalized) */
    UPROPERTY(BlueprintReadOnly, Category = "Distance")
    FVector Direction = FVector::ZeroVector;
};
