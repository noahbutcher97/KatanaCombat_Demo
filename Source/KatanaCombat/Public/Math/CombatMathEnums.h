// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatMathEnums.generated.h"

/**
 * Distance calculation formula types
 * Different formulas for different use cases
 */
UENUM(BlueprintType)
enum class EDistanceFormula : uint8
{
    /** Standard 3D distance: sqrt(dx² + dy² + dz²) */
    Euclidean       UMETA(DisplayName = "Euclidean (3D)"),

    /** 2D horizontal distance (ignores Z): sqrt(dx² + dy²) */
    Euclidean2D     UMETA(DisplayName = "Euclidean (2D)"),

    /** Grid distance: |dx| + |dy| + |dz| - useful for tile-based */
    Manhattan       UMETA(DisplayName = "Manhattan"),

    /** Max component: max(|dx|, |dy|, |dz|) - useful for box checks */
    Chebyshev       UMETA(DisplayName = "Chebyshev"),

    /** Squared euclidean (faster, no sqrt): dx² + dy² + dz² */
    SquaredEuclidean UMETA(DisplayName = "Squared Euclidean"),
};

/**
 * Spatial query shape types
 */
UENUM(BlueprintType)
enum class ESpatialQueryShape : uint8
{
    /** Sphere query (radius from point) */
    Sphere          UMETA(DisplayName = "Sphere"),

    /** Box query (axis-aligned or oriented) */
    Box             UMETA(DisplayName = "Box"),

    /** Capsule query (cylinder with hemispherical caps) */
    Capsule         UMETA(DisplayName = "Capsule"),

    /** Cone/frustum query (for field of view checks) */
    Cone            UMETA(DisplayName = "Cone"),

    /** Custom convex hull */
    ConvexHull      UMETA(DisplayName = "Convex Hull"),
};

/**
 * Bounding volume types for collision/culling
 */
UENUM(BlueprintType)
enum class EBoundingVolumeType : uint8
{
    /** Axis-Aligned Bounding Box - cheapest, loose fit */
    AABB            UMETA(DisplayName = "AABB"),

    /** Oriented Bounding Box - tighter fit, more expensive */
    OBB             UMETA(DisplayName = "OBB"),

    /** Bounding Sphere - rotation invariant */
    Sphere          UMETA(DisplayName = "Sphere"),

    /** Capsule - good for humanoid characters */
    Capsule         UMETA(DisplayName = "Capsule"),

    /** Convex Hull - tightest fit, most expensive */
    ConvexHull      UMETA(DisplayName = "Convex Hull"),
};

/**
 * Skeletal bone chain types (standard humanoid chains)
 */
UENUM(BlueprintType)
enum class EBoneChainType : uint8
{
    /** No specific chain */
    None            UMETA(DisplayName = "None"),

    /** Spine chain (pelvis to head) */
    Spine           UMETA(DisplayName = "Spine"),

    /** Left arm (clavicle to hand) */
    LeftArm         UMETA(DisplayName = "Left Arm"),

    /** Right arm (clavicle to hand) */
    RightArm        UMETA(DisplayName = "Right Arm"),

    /** Left leg (thigh to foot) */
    LeftLeg         UMETA(DisplayName = "Left Leg"),

    /** Right leg (thigh to foot) */
    RightLeg        UMETA(DisplayName = "Right Leg"),

    /** Neck chain (spine top to head) */
    Neck            UMETA(DisplayName = "Neck"),

    /** Left hand (wrist to fingertips) */
    LeftHand        UMETA(DisplayName = "Left Hand"),

    /** Right hand (wrist to fingertips) */
    RightHand       UMETA(DisplayName = "Right Hand"),

    /** Custom chain (user-defined) */
    Custom          UMETA(DisplayName = "Custom"),
};

/**
 * Handedness for limb queries
 */
UENUM(BlueprintType)
enum class EHandedness : uint8
{
    /** Left side */
    Left            UMETA(DisplayName = "Left"),

    /** Right side */
    Right           UMETA(DisplayName = "Right"),

    /** Either side (closest) */
    Either          UMETA(DisplayName = "Either"),

    /** Both sides */
    Both            UMETA(DisplayName = "Both"),
};

/**
 * Anatomical body region for contact/IK targeting
 */
UENUM(BlueprintType)
enum class EAnatomicalRegion : uint8
{
    /** No specific region */
    None            UMETA(DisplayName = "None"),

    /** Head region */
    Head            UMETA(DisplayName = "Head"),

    /** Neck region */
    Neck            UMETA(DisplayName = "Neck"),

    /** Chest/torso upper */
    Chest           UMETA(DisplayName = "Chest"),

    /** Abdomen/torso lower */
    Abdomen         UMETA(DisplayName = "Abdomen"),

    /** Pelvis/hips */
    Pelvis          UMETA(DisplayName = "Pelvis"),

    /** Upper arm (shoulder to elbow) */
    UpperArm        UMETA(DisplayName = "Upper Arm"),

    /** Lower arm (elbow to wrist) */
    LowerArm        UMETA(DisplayName = "Lower Arm"),

    /** Hand */
    Hand            UMETA(DisplayName = "Hand"),

    /** Upper leg (hip to knee) */
    UpperLeg        UMETA(DisplayName = "Upper Leg"),

    /** Lower leg (knee to ankle) */
    LowerLeg        UMETA(DisplayName = "Lower Leg"),

    /** Foot */
    Foot            UMETA(DisplayName = "Foot"),
};

/**
 * Contact type for paired animations
 */
UENUM(BlueprintType)
enum class EContactType : uint8
{
    /** No contact */
    None            UMETA(DisplayName = "None"),

    /** Weapon to body contact */
    WeaponToBody    UMETA(DisplayName = "Weapon to Body"),

    /** Hand to body contact (grab, push) */
    HandToBody      UMETA(DisplayName = "Hand to Body"),

    /** Foot to body contact (kick) */
    FootToBody      UMETA(DisplayName = "Foot to Body"),

    /** Body to body contact (tackle, grapple) */
    BodyToBody      UMETA(DisplayName = "Body to Body"),

    /** Weapon to weapon contact (parry, clash) */
    WeaponToWeapon  UMETA(DisplayName = "Weapon to Weapon"),

    /** Hand to weapon contact (disarm, catch) */
    HandToWeapon    UMETA(DisplayName = "Hand to Weapon"),
};

/**
 * Stability state for center of mass analysis
 */
UENUM(BlueprintType)
enum class EStabilityState : uint8
{
    /** Fully stable - COM well within support polygon */
    Stable          UMETA(DisplayName = "Stable"),

    /** Marginally stable - COM near edge of support */
    Marginal        UMETA(DisplayName = "Marginal"),

    /** Unstable - COM outside support polygon */
    Unstable        UMETA(DisplayName = "Unstable"),

    /** Falling - no ground support */
    Falling         UMETA(DisplayName = "Falling"),
};

/**
 * IK solver type
 */
UENUM(BlueprintType)
enum class EIKSolverType : uint8
{
    /** Two-bone analytical solver (fast, common for arms/legs) */
    TwoBone         UMETA(DisplayName = "Two Bone"),

    /** FABRIK (Forward And Backward Reaching IK) - iterative, any chain length */
    FABRIK          UMETA(DisplayName = "FABRIK"),

    /** CCD (Cyclic Coordinate Descent) - iterative, good for constrained joints */
    CCD             UMETA(DisplayName = "CCD"),

    /** Full body IK - considers whole skeleton */
    FullBody        UMETA(DisplayName = "Full Body"),
};
