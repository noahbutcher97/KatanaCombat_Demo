// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/SkeletalAnalysisLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"

// ============================================================================
// BONE MAPPING HELPERS
// ============================================================================

FName FHumanoidBoneMapping::GetChainTip(EBoneChainType ChainType, EHandedness Hand) const
{
    switch (ChainType)
    {
    case EBoneChainType::Spine:
        return Spine03;
    case EBoneChainType::Neck:
        return Head;
    case EBoneChainType::LeftArm:
        return LeftHand;
    case EBoneChainType::RightArm:
        return RightHand;
    case EBoneChainType::LeftLeg:
        return LeftFoot;
    case EBoneChainType::RightLeg:
        return RightFoot;
    case EBoneChainType::LeftHand:
        return LeftHand;
    case EBoneChainType::RightHand:
        return RightHand;
    default:
        // For sided chains, use handedness
        if (Hand == EHandedness::Left)
        {
            if (ChainType == EBoneChainType::LeftArm || ChainType == EBoneChainType::RightArm)
                return LeftHand;
            if (ChainType == EBoneChainType::LeftLeg || ChainType == EBoneChainType::RightLeg)
                return LeftFoot;
        }
        return RightHand;
    }
}

// ============================================================================
// HIERARCHY BUILDING
// ============================================================================

FSkeletalHierarchy USkeletalAnalysisLibrary::BuildSkeletalHierarchy(USkeletalMeshComponent* MeshComponent)
{
    FSkeletalHierarchy Hierarchy;

    if (!MeshComponent || !MeshComponent->GetSkeletalMeshAsset())
    {
        return Hierarchy;
    }

    const FReferenceSkeleton& RefSkeleton = MeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
    const int32 NumBones = RefSkeleton.GetNum();

    if (NumBones == 0)
    {
        return Hierarchy;
    }

    Hierarchy.Bones.Reserve(NumBones);
    Hierarchy.BoneCount = NumBones;

    // First pass: Create bone info for each bone
    for (int32 i = 0; i < NumBones; ++i)
    {
        FSkeletalBoneInfo BoneInfo;
        BoneInfo.BoneName = RefSkeleton.GetBoneName(i);
        BoneInfo.BoneIndex = i;

        int32 ParentIndex = RefSkeleton.GetParentIndex(i);
        if (ParentIndex != INDEX_NONE)
        {
            BoneInfo.ParentBoneName = RefSkeleton.GetBoneName(ParentIndex);
            BoneInfo.Depth = Hierarchy.Bones[ParentIndex].Depth + 1;

            // Calculate bone length
            FVector ParentPos = MeshComponent->GetBoneLocation(BoneInfo.ParentBoneName, EBoneSpaces::ComponentSpace);
            FVector BonePos = MeshComponent->GetBoneLocation(BoneInfo.BoneName, EBoneSpaces::ComponentSpace);
            BoneInfo.BoneLength = FVector::Dist(ParentPos, BonePos);
        }
        else
        {
            // Root bone
            Hierarchy.RootBoneName = BoneInfo.BoneName;
            BoneInfo.Depth = 0;
            BoneInfo.BoneLength = 0.0f;
        }

        Hierarchy.MaxDepth = FMath::Max(Hierarchy.MaxDepth, BoneInfo.Depth);
        Hierarchy.BoneNameToIndex.Add(BoneInfo.BoneName, Hierarchy.Bones.Num());
        Hierarchy.Bones.Add(BoneInfo);
    }

    // Second pass: Build child arrays
    for (int32 i = 0; i < NumBones; ++i)
    {
        int32 ParentIndex = RefSkeleton.GetParentIndex(i);
        if (ParentIndex != INDEX_NONE)
        {
            Hierarchy.Bones[ParentIndex].ChildBoneNames.Add(Hierarchy.Bones[i].BoneName);
        }
    }

    return Hierarchy;
}

bool USkeletalAnalysisLibrary::ValidateHumanoidHierarchy(
    const FSkeletalHierarchy& Hierarchy,
    const FHumanoidBoneMapping& BoneMapping,
    TArray<FName>& OutMissingBones)
{
    OutMissingBones.Empty();

    // Check all expected bones
    TArray<FName> ExpectedBones = {
        BoneMapping.Pelvis, BoneMapping.Spine01, BoneMapping.Spine02, BoneMapping.Spine03,
        BoneMapping.Neck, BoneMapping.Head,
        BoneMapping.LeftClavicle, BoneMapping.LeftUpperArm, BoneMapping.LeftLowerArm, BoneMapping.LeftHand,
        BoneMapping.RightClavicle, BoneMapping.RightUpperArm, BoneMapping.RightLowerArm, BoneMapping.RightHand,
        BoneMapping.LeftThigh, BoneMapping.LeftCalf, BoneMapping.LeftFoot,
        BoneMapping.RightThigh, BoneMapping.RightCalf, BoneMapping.RightFoot
    };

    for (const FName& BoneName : ExpectedBones)
    {
        if (!Hierarchy.BoneNameToIndex.Contains(BoneName))
        {
            OutMissingBones.Add(BoneName);
        }
    }

    return OutMissingBones.Num() == 0;
}

TArray<FName> USkeletalAnalysisLibrary::GetBoneChildren(
    const FSkeletalHierarchy& Hierarchy,
    FName BoneName,
    bool bRecursive)
{
    TArray<FName> Children;

    const FSkeletalBoneInfo* BoneInfo = Hierarchy.GetBone(BoneName);
    if (!BoneInfo)
    {
        return Children;
    }

    for (const FName& ChildName : BoneInfo->ChildBoneNames)
    {
        Children.Add(ChildName);

        if (bRecursive)
        {
            Children.Append(GetBoneChildren(Hierarchy, ChildName, true));
        }
    }

    return Children;
}

TArray<FName> USkeletalAnalysisLibrary::GetPathToRoot(
    const FSkeletalHierarchy& Hierarchy,
    FName BoneName)
{
    TArray<FName> Path;

    FName CurrentBone = BoneName;
    while (CurrentBone != NAME_None)
    {
        Path.Insert(CurrentBone, 0);

        const FSkeletalBoneInfo* BoneInfo = Hierarchy.GetBone(CurrentBone);
        if (!BoneInfo)
        {
            break;
        }

        CurrentBone = BoneInfo->ParentBoneName;
    }

    return Path;
}

// ============================================================================
// BONE CHAINS
// ============================================================================

FPhysicsBoneChain USkeletalAnalysisLibrary::GetBoneChain(
    USkeletalMeshComponent* MeshComponent,
    EBoneChainType ChainType,
    EHandedness Hand,
    const FHumanoidBoneMapping& BoneMapping)
{
    FPhysicsBoneChain Chain;

    if (!MeshComponent)
    {
        return Chain;
    }

    switch (ChainType)
    {
    case EBoneChainType::Spine:
        Chain.BoneNames = { BoneMapping.Pelvis, BoneMapping.Spine01, BoneMapping.Spine02, BoneMapping.Spine03 };
        Chain.RootBone = BoneMapping.Pelvis;
        Chain.TipBone = BoneMapping.Spine03;
        break;

    case EBoneChainType::Neck:
        Chain.BoneNames = { BoneMapping.Spine03, BoneMapping.Neck, BoneMapping.Head };
        Chain.RootBone = BoneMapping.Spine03;
        Chain.TipBone = BoneMapping.Head;
        break;

    case EBoneChainType::LeftArm:
        Chain.BoneNames = { BoneMapping.LeftClavicle, BoneMapping.LeftUpperArm, BoneMapping.LeftLowerArm, BoneMapping.LeftHand };
        Chain.RootBone = BoneMapping.LeftClavicle;
        Chain.TipBone = BoneMapping.LeftHand;
        break;

    case EBoneChainType::RightArm:
        Chain.BoneNames = { BoneMapping.RightClavicle, BoneMapping.RightUpperArm, BoneMapping.RightLowerArm, BoneMapping.RightHand };
        Chain.RootBone = BoneMapping.RightClavicle;
        Chain.TipBone = BoneMapping.RightHand;
        break;

    case EBoneChainType::LeftLeg:
        Chain.BoneNames = { BoneMapping.LeftThigh, BoneMapping.LeftCalf, BoneMapping.LeftFoot };
        Chain.RootBone = BoneMapping.LeftThigh;
        Chain.TipBone = BoneMapping.LeftFoot;
        break;

    case EBoneChainType::RightLeg:
        Chain.BoneNames = { BoneMapping.RightThigh, BoneMapping.RightCalf, BoneMapping.RightFoot };
        Chain.RootBone = BoneMapping.RightThigh;
        Chain.TipBone = BoneMapping.RightFoot;
        break;

    default:
        return Chain;
    }

    // Calculate total length
    Chain.TotalLength = GetChainLength(MeshComponent, Chain);

    return Chain;
}

FPhysicsBoneChain USkeletalAnalysisLibrary::BuildCustomChain(
    USkeletalMeshComponent* MeshComponent,
    FName StartBone,
    FName EndBone)
{
    FPhysicsBoneChain Chain;

    if (!MeshComponent || !MeshComponent->GetSkeletalMeshAsset())
    {
        return Chain;
    }

    // Build hierarchy to find path
    FSkeletalHierarchy Hierarchy = BuildSkeletalHierarchy(MeshComponent);

    // Get paths to root for both bones
    TArray<FName> StartPath = GetPathToRoot(Hierarchy, StartBone);
    TArray<FName> EndPath = GetPathToRoot(Hierarchy, EndBone);

    // Find common ancestor
    int32 CommonIndex = -1;
    for (int32 i = 0; i < StartPath.Num() && i < EndPath.Num(); ++i)
    {
        if (StartPath[i] == EndPath[i])
        {
            CommonIndex = i;
        }
        else
        {
            break;
        }
    }

    if (CommonIndex < 0)
    {
        return Chain;
    }

    // Build chain from start to common ancestor, then to end
    // For simplicity, we'll use the path from start to end through common ancestor
    // This assumes start is an ancestor of end or vice versa

    // Check if start is ancestor of end
    int32 StartInEndPath = EndPath.Find(StartBone);
    if (StartInEndPath != INDEX_NONE)
    {
        // Start is ancestor of end - extract sub-path
        for (int32 i = StartInEndPath; i < EndPath.Num(); ++i)
        {
            Chain.BoneNames.Add(EndPath[i]);
        }
    }
    else
    {
        // Check if end is ancestor of start
        int32 EndInStartPath = StartPath.Find(EndBone);
        if (EndInStartPath != INDEX_NONE)
        {
            // End is ancestor of start - extract reversed
            for (int32 i = EndInStartPath; i < StartPath.Num(); ++i)
            {
                Chain.BoneNames.Add(StartPath[i]);
            }
            Algo::Reverse(Chain.BoneNames);
        }
    }

    if (Chain.BoneNames.Num() > 0)
    {
        Chain.RootBone = Chain.BoneNames[0];
        Chain.TipBone = Chain.BoneNames.Last();
        Chain.TotalLength = GetChainLength(MeshComponent, Chain);
    }

    return Chain;
}

float USkeletalAnalysisLibrary::GetChainLength(USkeletalMeshComponent* MeshComponent, const FPhysicsBoneChain& Chain)
{
    if (!MeshComponent || Chain.BoneNames.Num() < 2)
    {
        return 0.0f;
    }

    float TotalLength = 0.0f;

    for (int32 i = 1; i < Chain.BoneNames.Num(); ++i)
    {
        FVector PrevPos = MeshComponent->GetBoneLocation(Chain.BoneNames[i - 1], EBoneSpaces::ComponentSpace);
        FVector CurrPos = MeshComponent->GetBoneLocation(Chain.BoneNames[i], EBoneSpaces::ComponentSpace);
        TotalLength += FVector::Dist(PrevPos, CurrPos);
    }

    return TotalLength;
}

TArray<FTransform> USkeletalAnalysisLibrary::GetChainTransforms(
    USkeletalMeshComponent* MeshComponent,
    const FPhysicsBoneChain& Chain)
{
    TArray<FTransform> Transforms;

    if (!MeshComponent)
    {
        return Transforms;
    }

    for (const FName& BoneName : Chain.BoneNames)
    {
        if (!BoneName.IsNone())
        {
            // Use FName-based API with ERelativeTransformSpace
            Transforms.Add(MeshComponent->GetBoneTransform(BoneName, RTS_Component));
        }
    }

    return Transforms;
}

// ============================================================================
// REACH ANALYSIS
// ============================================================================

FReachQueryResult USkeletalAnalysisLibrary::QueryReach(
    USkeletalMeshComponent* MeshComponent,
    const FPhysicsBoneChain& Chain,
    const FVector& TargetLocation,
    float ComfortableReachRatio)
{
    FReachQueryResult Result;

    if (!MeshComponent || !Chain.IsValid())
    {
        return Result;
    }

    // Get chain root position
    FVector RootPos = MeshComponent->GetBoneLocation(Chain.RootBone, EBoneSpaces::WorldSpace);

    // Calculate reach metrics
    Result.DistanceToTarget = FVector::Dist(RootPos, TargetLocation);
    Result.MaxReach = Chain.TotalLength;
    Result.ComfortableReach = Chain.TotalLength * ComfortableReachRatio;

    Result.bIsReachable = Result.DistanceToTarget <= Result.MaxReach;
    Result.bIsComfortablyReachable = Result.DistanceToTarget <= Result.ComfortableReach;

    // Extension ratio: 0 = at root, 1 = fully extended
    Result.ExtensionRatio = Result.MaxReach > 0.0f ? (Result.DistanceToTarget / Result.MaxReach) : 0.0f;
    Result.ExtensionRatio = FMath::Clamp(Result.ExtensionRatio, 0.0f, 1.0f);

    // Direction to target
    Result.DirectionToTarget = (TargetLocation - RootPos).GetSafeNormal();

    return Result;
}

FReachQueryResult USkeletalAnalysisLibrary::QueryLimbReach(
    USkeletalMeshComponent* MeshComponent,
    EBoneChainType ChainType,
    EHandedness Hand,
    const FVector& TargetLocation)
{
    FPhysicsBoneChain Chain = GetBoneChain(MeshComponent, ChainType, Hand);
    return QueryReach(MeshComponent, Chain, TargetLocation);
}

void USkeletalAnalysisLibrary::GetReachEnvelope(
    USkeletalMeshComponent* MeshComponent,
    const FPhysicsBoneChain& Chain,
    FVector& OutCenter,
    float& OutMaxRadius,
    float& OutComfortableRadius)
{
    OutCenter = FVector::ZeroVector;
    OutMaxRadius = 0.0f;
    OutComfortableRadius = 0.0f;

    if (!MeshComponent || !Chain.IsValid())
    {
        return;
    }

    OutCenter = MeshComponent->GetBoneLocation(Chain.RootBone, EBoneSpaces::WorldSpace);
    OutMaxRadius = Chain.TotalLength;
    OutComfortableRadius = Chain.TotalLength * 0.8f;
}

// ============================================================================
// CENTER OF MASS
// ============================================================================

const TMap<EAnatomicalRegion, float>& USkeletalAnalysisLibrary::GetDefaultMassRatios()
{
    // Approximate body segment mass percentages (based on biomechanics research)
    static TMap<EAnatomicalRegion, float> MassRatios = {
        { EAnatomicalRegion::Head, 0.08f },
        { EAnatomicalRegion::Neck, 0.02f },
        { EAnatomicalRegion::Chest, 0.20f },
        { EAnatomicalRegion::Abdomen, 0.15f },
        { EAnatomicalRegion::Pelvis, 0.10f },
        { EAnatomicalRegion::UpperArm, 0.03f },  // Per arm
        { EAnatomicalRegion::LowerArm, 0.02f },
        { EAnatomicalRegion::Hand, 0.01f },
        { EAnatomicalRegion::UpperLeg, 0.10f },  // Per leg
        { EAnatomicalRegion::LowerLeg, 0.05f },
        { EAnatomicalRegion::Foot, 0.02f }
    };

    return MassRatios;
}

FCenterOfMassResult USkeletalAnalysisLibrary::CalculateCenterOfMass(
    USkeletalMeshComponent* MeshComponent,
    const FHumanoidBoneMapping& BoneMapping)
{
    FCenterOfMassResult Result;

    if (!MeshComponent)
    {
        return Result;
    }

    const TMap<EAnatomicalRegion, float>& MassRatios = GetDefaultMassRatios();

    FVector WeightedSum = FVector::ZeroVector;
    float TotalWeight = 0.0f;

    // Body segments to sample (with representative bones)
    TArray<TPair<FName, float>> BoneWeights = {
        { BoneMapping.Head, MassRatios[EAnatomicalRegion::Head] },
        { BoneMapping.Neck, MassRatios[EAnatomicalRegion::Neck] },
        { BoneMapping.Spine03, MassRatios[EAnatomicalRegion::Chest] },
        { BoneMapping.Spine01, MassRatios[EAnatomicalRegion::Abdomen] },
        { BoneMapping.Pelvis, MassRatios[EAnatomicalRegion::Pelvis] },
        { BoneMapping.LeftUpperArm, MassRatios[EAnatomicalRegion::UpperArm] },
        { BoneMapping.LeftLowerArm, MassRatios[EAnatomicalRegion::LowerArm] },
        { BoneMapping.LeftHand, MassRatios[EAnatomicalRegion::Hand] },
        { BoneMapping.RightUpperArm, MassRatios[EAnatomicalRegion::UpperArm] },
        { BoneMapping.RightLowerArm, MassRatios[EAnatomicalRegion::LowerArm] },
        { BoneMapping.RightHand, MassRatios[EAnatomicalRegion::Hand] },
        { BoneMapping.LeftThigh, MassRatios[EAnatomicalRegion::UpperLeg] },
        { BoneMapping.LeftCalf, MassRatios[EAnatomicalRegion::LowerLeg] },
        { BoneMapping.LeftFoot, MassRatios[EAnatomicalRegion::Foot] },
        { BoneMapping.RightThigh, MassRatios[EAnatomicalRegion::UpperLeg] },
        { BoneMapping.RightCalf, MassRatios[EAnatomicalRegion::LowerLeg] },
        { BoneMapping.RightFoot, MassRatios[EAnatomicalRegion::Foot] }
    };

    for (const auto& BoneWeight : BoneWeights)
    {
        int32 BoneIndex = MeshComponent->GetBoneIndex(BoneWeight.Key);
        if (BoneIndex != INDEX_NONE)
        {
            FVector BonePos = MeshComponent->GetBoneLocation(BoneWeight.Key, EBoneSpaces::WorldSpace);
            WeightedSum += BonePos * BoneWeight.Value;
            TotalWeight += BoneWeight.Value;
        }
    }

    if (TotalWeight > 0.0f)
    {
        Result.Location = WeightedSum / TotalWeight;
        Result.TotalMass = TotalWeight;  // Ratio, not actual mass

        // Check stability (COM over feet)
        EStabilityState Stability = CheckStability(MeshComponent, Result.Location, BoneMapping);
        Result.bIsStable = (Stability == EStabilityState::Stable || Stability == EStabilityState::Marginal);
    }

    return Result;
}

FVector USkeletalAnalysisLibrary::CalculateWeightedCenter(
    USkeletalMeshComponent* MeshComponent,
    const TArray<FName>& BoneNames,
    const TArray<float>& BoneWeights)
{
    if (!MeshComponent || BoneNames.Num() == 0)
    {
        return FVector::ZeroVector;
    }

    FVector WeightedSum = FVector::ZeroVector;
    float TotalWeight = 0.0f;

    for (int32 i = 0; i < BoneNames.Num(); ++i)
    {
        int32 BoneIndex = MeshComponent->GetBoneIndex(BoneNames[i]);
        if (BoneIndex != INDEX_NONE)
        {
            FVector BonePos = MeshComponent->GetBoneLocation(BoneNames[i], EBoneSpaces::WorldSpace);
            float Weight = BoneWeights.IsValidIndex(i) ? BoneWeights[i] : 1.0f;
            WeightedSum += BonePos * Weight;
            TotalWeight += Weight;
        }
    }

    return TotalWeight > 0.0f ? (WeightedSum / TotalWeight) : FVector::ZeroVector;
}

EStabilityState USkeletalAnalysisLibrary::CheckStability(
    USkeletalMeshComponent* MeshComponent,
    const FVector& COMLocation,
    const FHumanoidBoneMapping& BoneMapping)
{
    if (!MeshComponent)
    {
        return EStabilityState::Falling;
    }

    // Get foot positions (support polygon)
    FVector LeftFoot = MeshComponent->GetBoneLocation(BoneMapping.LeftFoot, EBoneSpaces::WorldSpace);
    FVector RightFoot = MeshComponent->GetBoneLocation(BoneMapping.RightFoot, EBoneSpaces::WorldSpace);

    // Project COM onto ground plane (XY)
    FVector COMProjected = FVector(COMLocation.X, COMLocation.Y, 0.0f);
    FVector LeftProjected = FVector(LeftFoot.X, LeftFoot.Y, 0.0f);
    FVector RightProjected = FVector(RightFoot.X, RightFoot.Y, 0.0f);

    // Support polygon is approximately between the feet
    FVector SupportCenter = (LeftProjected + RightProjected) * 0.5f;
    float SupportRadius = FVector::Dist2D(LeftProjected, RightProjected) * 0.5f + 15.0f;  // Add margin for foot width

    float DistanceFromCenter = FVector::Dist2D(COMProjected, SupportCenter);

    if (DistanceFromCenter < SupportRadius * 0.5f)
    {
        return EStabilityState::Stable;
    }
    else if (DistanceFromCenter < SupportRadius)
    {
        return EStabilityState::Marginal;
    }
    else
    {
        return EStabilityState::Unstable;
    }
}

// ============================================================================
// ANATOMICAL REGIONS
// ============================================================================

EAnatomicalRegion USkeletalAnalysisLibrary::GetBoneRegion(
    FName BoneName,
    const FHumanoidBoneMapping& BoneMapping)
{
    FString BoneStr = BoneName.ToString().ToLower();

    // Check explicit mappings first
    if (BoneName == BoneMapping.Head) return EAnatomicalRegion::Head;
    if (BoneName == BoneMapping.Neck) return EAnatomicalRegion::Neck;
    if (BoneName == BoneMapping.Spine03) return EAnatomicalRegion::Chest;
    if (BoneName == BoneMapping.Spine01 || BoneName == BoneMapping.Spine02) return EAnatomicalRegion::Abdomen;
    if (BoneName == BoneMapping.Pelvis) return EAnatomicalRegion::Pelvis;

    // Arms
    if (BoneName == BoneMapping.LeftUpperArm || BoneName == BoneMapping.RightUpperArm) return EAnatomicalRegion::UpperArm;
    if (BoneName == BoneMapping.LeftLowerArm || BoneName == BoneMapping.RightLowerArm) return EAnatomicalRegion::LowerArm;
    if (BoneName == BoneMapping.LeftHand || BoneName == BoneMapping.RightHand) return EAnatomicalRegion::Hand;

    // Legs
    if (BoneName == BoneMapping.LeftThigh || BoneName == BoneMapping.RightThigh) return EAnatomicalRegion::UpperLeg;
    if (BoneName == BoneMapping.LeftCalf || BoneName == BoneMapping.RightCalf) return EAnatomicalRegion::LowerLeg;
    if (BoneName == BoneMapping.LeftFoot || BoneName == BoneMapping.RightFoot) return EAnatomicalRegion::Foot;

    // Fall back to string matching
    if (BoneStr.Contains(TEXT("head"))) return EAnatomicalRegion::Head;
    if (BoneStr.Contains(TEXT("neck"))) return EAnatomicalRegion::Neck;
    if (BoneStr.Contains(TEXT("spine"))) return EAnatomicalRegion::Abdomen;
    if (BoneStr.Contains(TEXT("pelvis")) || BoneStr.Contains(TEXT("hip"))) return EAnatomicalRegion::Pelvis;
    if (BoneStr.Contains(TEXT("upperarm")) || BoneStr.Contains(TEXT("shoulder"))) return EAnatomicalRegion::UpperArm;
    if (BoneStr.Contains(TEXT("lowerarm")) || BoneStr.Contains(TEXT("forearm"))) return EAnatomicalRegion::LowerArm;
    if (BoneStr.Contains(TEXT("hand"))) return EAnatomicalRegion::Hand;
    if (BoneStr.Contains(TEXT("thigh")) || BoneStr.Contains(TEXT("upperleg"))) return EAnatomicalRegion::UpperLeg;
    if (BoneStr.Contains(TEXT("calf")) || BoneStr.Contains(TEXT("lowerleg"))) return EAnatomicalRegion::LowerLeg;
    if (BoneStr.Contains(TEXT("foot"))) return EAnatomicalRegion::Foot;

    return EAnatomicalRegion::None;
}

TArray<FName> USkeletalAnalysisLibrary::GetBonesInRegion(
    USkeletalMeshComponent* MeshComponent,
    EAnatomicalRegion Region,
    EHandedness Hand,
    const FHumanoidBoneMapping& BoneMapping)
{
    TArray<FName> Bones;

    if (!MeshComponent)
    {
        return Bones;
    }

    switch (Region)
    {
    case EAnatomicalRegion::Head:
        Bones.Add(BoneMapping.Head);
        break;

    case EAnatomicalRegion::Neck:
        Bones.Add(BoneMapping.Neck);
        break;

    case EAnatomicalRegion::Chest:
        Bones.Add(BoneMapping.Spine03);
        break;

    case EAnatomicalRegion::Abdomen:
        Bones.Add(BoneMapping.Spine01);
        Bones.Add(BoneMapping.Spine02);
        break;

    case EAnatomicalRegion::Pelvis:
        Bones.Add(BoneMapping.Pelvis);
        break;

    case EAnatomicalRegion::UpperArm:
        if (Hand == EHandedness::Left || Hand == EHandedness::Both || Hand == EHandedness::Either)
            Bones.Add(BoneMapping.LeftUpperArm);
        if (Hand == EHandedness::Right || Hand == EHandedness::Both || Hand == EHandedness::Either)
            Bones.Add(BoneMapping.RightUpperArm);
        break;

    case EAnatomicalRegion::LowerArm:
        if (Hand == EHandedness::Left || Hand == EHandedness::Both || Hand == EHandedness::Either)
            Bones.Add(BoneMapping.LeftLowerArm);
        if (Hand == EHandedness::Right || Hand == EHandedness::Both || Hand == EHandedness::Either)
            Bones.Add(BoneMapping.RightLowerArm);
        break;

    case EAnatomicalRegion::Hand:
        if (Hand == EHandedness::Left || Hand == EHandedness::Both || Hand == EHandedness::Either)
            Bones.Add(BoneMapping.LeftHand);
        if (Hand == EHandedness::Right || Hand == EHandedness::Both || Hand == EHandedness::Either)
            Bones.Add(BoneMapping.RightHand);
        break;

    case EAnatomicalRegion::UpperLeg:
        if (Hand == EHandedness::Left || Hand == EHandedness::Both || Hand == EHandedness::Either)
            Bones.Add(BoneMapping.LeftThigh);
        if (Hand == EHandedness::Right || Hand == EHandedness::Both || Hand == EHandedness::Either)
            Bones.Add(BoneMapping.RightThigh);
        break;

    case EAnatomicalRegion::LowerLeg:
        if (Hand == EHandedness::Left || Hand == EHandedness::Both || Hand == EHandedness::Either)
            Bones.Add(BoneMapping.LeftCalf);
        if (Hand == EHandedness::Right || Hand == EHandedness::Both || Hand == EHandedness::Either)
            Bones.Add(BoneMapping.RightCalf);
        break;

    case EAnatomicalRegion::Foot:
        if (Hand == EHandedness::Left || Hand == EHandedness::Both || Hand == EHandedness::Either)
            Bones.Add(BoneMapping.LeftFoot);
        if (Hand == EHandedness::Right || Hand == EHandedness::Both || Hand == EHandedness::Either)
            Bones.Add(BoneMapping.RightFoot);
        break;

    default:
        break;
    }

    return Bones;
}

FName USkeletalAnalysisLibrary::FindClosestBone(
    USkeletalMeshComponent* MeshComponent,
    const FVector& WorldLocation,
    const TArray<FName>& BonesToCheck)
{
    if (!MeshComponent)
    {
        return NAME_None;
    }

    TArray<FName> CandidateBones = BonesToCheck;
    if (CandidateBones.Num() == 0)
    {
        // Use all bones
        if (MeshComponent->GetSkeletalMeshAsset())
        {
            const FReferenceSkeleton& RefSkel = MeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
            for (int32 i = 0; i < RefSkel.GetNum(); ++i)
            {
                CandidateBones.Add(RefSkel.GetBoneName(i));
            }
        }
    }

    FName ClosestBone = NAME_None;
    float ClosestDist = MAX_FLT;

    for (const FName& BoneName : CandidateBones)
    {
        FVector BonePos = MeshComponent->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace);
        float Dist = FVector::DistSquared(BonePos, WorldLocation);

        if (Dist < ClosestDist)
        {
            ClosestDist = Dist;
            ClosestBone = BoneName;
        }
    }

    return ClosestBone;
}

// ============================================================================
// BONE TRANSFORMS
// ============================================================================

FTransform USkeletalAnalysisLibrary::GetBoneWorldTransform(USkeletalMeshComponent* MeshComponent, FName BoneName)
{
    if (!MeshComponent || BoneName.IsNone())
    {
        return FTransform::Identity;
    }

    // Use FName-based API with ERelativeTransformSpace
    return MeshComponent->GetBoneTransform(BoneName, RTS_World);
}

FTransform USkeletalAnalysisLibrary::GetBoneComponentTransform(USkeletalMeshComponent* MeshComponent, FName BoneName)
{
    if (!MeshComponent || BoneName.IsNone())
    {
        return FTransform::Identity;
    }

    // Use FName-based API with ERelativeTransformSpace
    return MeshComponent->GetBoneTransform(BoneName, RTS_Component);
}

float USkeletalAnalysisLibrary::GetDistanceBetweenBones(USkeletalMeshComponent* MeshComponent, FName BoneA, FName BoneB)
{
    if (!MeshComponent)
    {
        return 0.0f;
    }

    FVector PosA = MeshComponent->GetBoneLocation(BoneA, EBoneSpaces::WorldSpace);
    FVector PosB = MeshComponent->GetBoneLocation(BoneB, EBoneSpaces::WorldSpace);

    return FVector::Dist(PosA, PosB);
}

FVector USkeletalAnalysisLibrary::GetDirectionBetweenBones(USkeletalMeshComponent* MeshComponent, FName FromBone, FName ToBone)
{
    if (!MeshComponent)
    {
        return FVector::ZeroVector;
    }

    FVector FromPos = MeshComponent->GetBoneLocation(FromBone, EBoneSpaces::WorldSpace);
    FVector ToPos = MeshComponent->GetBoneLocation(ToBone, EBoneSpaces::WorldSpace);

    return (ToPos - FromPos).GetSafeNormal();
}
