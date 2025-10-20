// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/TargetingComponent.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

UTargetingComponent::UTargetingComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    
    DirectionalConeAngle = 60.0f;
    MaxTargetDistance = 1000.0f;
    bRequireLineOfSight = true;
    LineOfSightChannel = ECC_Visibility;
    bDebugDraw = false;
    
    CurrentTarget = nullptr;
}

void UTargetingComponent::BeginPlay()
{
    Super::BeginPlay();
    
    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter)
    {
        MotionWarpingComponent = OwnerCharacter->FindComponentByClass<UMotionWarpingComponent>();
    }
}

// ============================================================================
// TARGETING - PRIMARY API
// ============================================================================

AActor* UTargetingComponent::FindTarget(EAttackDirection Direction)
{
    if (!OwnerCharacter)
    {
        return nullptr;
    }
    
    const FVector SearchDirection = GetDirectionVector(Direction, false);
    return FindBestTarget(SearchDirection);
}

AActor* UTargetingComponent::FindTargetInDirection(const FVector& DirectionVector)
{
    if (!OwnerCharacter || DirectionVector.IsNearlyZero())
    {
        return nullptr;
    }
    
    FVector NormalizedDirection = DirectionVector;
    NormalizedDirection.Normalize();
    
    return FindBestTarget(NormalizedDirection);
}

int32 UTargetingComponent::GetAllTargetsInRange(TArray<AActor*>& OutTargets)
{
    OutTargets.Empty();
    
    GetActorsInRange(OutTargets);
    FilterByTargetableClass(OutTargets);
    
    if (bRequireLineOfSight)
    {
        FilterByLineOfSight(OutTargets);
    }
    
    return OutTargets.Num();
}

// ============================================================================
// TARGETING - UTILITY QUERIES
// ============================================================================

bool UTargetingComponent::IsTargetInCone(AActor* Target, const FVector& Direction, float AngleTolerance) const
{
    if (!OwnerCharacter || !Target || Direction.IsNearlyZero())
    {
        return false;
    }
    
    const float ConeAngle = (AngleTolerance > 0.0f) ? AngleTolerance : DirectionalConeAngle;
    
    const FVector ToTarget = (Target->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal();
    const float DotProduct = FVector::DotProduct(Direction, ToTarget);
    const float Angle = FMath::RadiansToDegrees(FMath::Acos(DotProduct));
    
    return Angle <= ConeAngle;
}

bool UTargetingComponent::HasLineOfSightTo(AActor* Target) const
{
    if (!OwnerCharacter || !Target || !GetWorld())
    {
        return false;
    }
    
    const FVector Start = OwnerCharacter->GetActorLocation();
    const FVector End = Target->GetActorLocation();
    
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter);
    QueryParams.AddIgnoredActor(Target);
    
    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        Start,
        End,
        LineOfSightChannel,
        QueryParams
    );
    
    return !bHit; // No hit means clear line of sight
}

FVector UTargetingComponent::GetDirectionVector(EAttackDirection Direction, bool bUseCamera) const
{
    if (!OwnerCharacter)
    {
        return FVector::ForwardVector;
    }

    if (Direction == EAttackDirection::None || Direction == EAttackDirection::Forward)
    {
        if (bUseCamera)
        {
            if (const APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
            {
                FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
                CameraRotation.Pitch = 0.0f;
                CameraRotation.Roll = 0.0f;
                return FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::X);
            }
        }
        
        return OwnerCharacter->GetActorForwardVector();
    }
    
    FVector BaseForward = OwnerCharacter->GetActorForwardVector();
    FVector BaseRight = OwnerCharacter->GetActorRightVector();
    
    switch (Direction)
    {
        case EAttackDirection::Forward:
            return BaseForward;
        case EAttackDirection::Backward:
            return -BaseForward;
        case EAttackDirection::Left:
            return -BaseRight;
        case EAttackDirection::Right:
            return BaseRight;
        default:
            return BaseForward;
    }
}

float UTargetingComponent::GetAngleToTarget(AActor* Target) const
{
    if (!OwnerCharacter || !Target)
    {
        return 0.0f;
    }
    
    const FVector Forward = OwnerCharacter->GetActorForwardVector();
    const FVector ToTarget = (Target->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal();
    
    const float DotProduct = FVector::DotProduct(Forward, ToTarget);
    const float CrossZ = FVector::CrossProduct(Forward, ToTarget).Z;
    
    float Angle = FMath::RadiansToDegrees(FMath::Acos(DotProduct));
    if (CrossZ < 0.0f)
    {
        Angle = -Angle;
    }
    
    return Angle;
}

float UTargetingComponent::GetDistanceToTarget(AActor* Target) const
{
    if (!OwnerCharacter || !Target)
    {
        return 0.0f;
    }
    
    return FVector::Dist(OwnerCharacter->GetActorLocation(), Target->GetActorLocation());
}

// ============================================================================
// CURRENT TARGET MANAGEMENT
// ============================================================================

void UTargetingComponent::SetCurrentTarget(AActor* NewTarget)
{
    CurrentTarget = NewTarget;
}

void UTargetingComponent::ClearCurrentTarget()
{
    CurrentTarget = nullptr;
}

// ============================================================================
// MOTION WARPING INTEGRATION
// ============================================================================

bool UTargetingComponent::SetupMotionWarp(AActor* Target, FName WarpTargetName, float MaxDistance)
{
    if (!MotionWarpingComponent || !Target || !OwnerCharacter)
    {
        return false;
    }
    
    const FVector WarpLocation = CalculateWarpLocation(Target, MaxDistance);
    const FRotator LookAtRotation = (Target->GetActorLocation() - OwnerCharacter->GetActorLocation()).Rotation();
    
    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
        WarpTargetName,
        WarpLocation,
        LookAtRotation
    );
    
    return true;
}

void UTargetingComponent::ClearMotionWarp(FName WarpTargetName)
{
    if (!MotionWarpingComponent)
    {
        return;
    }
    
    if (WarpTargetName == NAME_None)
    {
        MotionWarpingComponent->RemoveAllWarpTargets();
    }
    else
    {
        MotionWarpingComponent->RemoveWarpTarget(WarpTargetName);
    }
}

// ============================================================================
// INTERNAL HELPERS - TARGET FINDING
// ============================================================================

void UTargetingComponent::GetActorsInRange(TArray<AActor*>& OutActors) const
{
    if (!OwnerCharacter || !GetWorld())
    {
        return;
    }
    
    const FVector OwnerLocation = OwnerCharacter->GetActorLocation();
    
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter);
    
    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        OwnerLocation,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(MaxTargetDistance),
        QueryParams
    );
    
    for (const FOverlapResult& Overlap : Overlaps)
    {
        if (AActor* Actor = Overlap.GetActor())
        {
            OutActors.Add(Actor);
        }
    }
}

void UTargetingComponent::FilterByTargetableClass(TArray<AActor*>& InOutActors) const
{
    if (TargetableClasses.Num() == 0)
    {
        return; // No filter if empty
    }
    
    InOutActors.RemoveAll([this](const AActor* Actor)
    {
        if (!Actor)
        {
            return true;
        }
        
        for (const TSubclassOf<AActor>& TargetClass : TargetableClasses)
        {
            if (Actor->IsA(TargetClass))
            {
                return false; // Keep it
            }
        }
        
        return true; // Remove it
    });
}

void UTargetingComponent::FilterByCone(TArray<AActor*>& InOutActors, const FVector& Direction) const
{
    InOutActors.RemoveAll([this, &Direction](const AActor* Actor)
    {
        return !IsTargetInCone(const_cast<AActor*>(Actor), Direction, -1.0f);
    });
}

void UTargetingComponent::FilterByLineOfSight(TArray<AActor*>& InOutActors) const
{
    InOutActors.RemoveAll([this](const AActor* Actor)
    {
        return !HasLineOfSightTo(const_cast<AActor*>(Actor));
    });
}

void UTargetingComponent::SortByDistance(TArray<AActor*>& InOutActors) const
{
    if (!OwnerCharacter)
    {
        return;
    }
    
    const FVector OwnerLocation = OwnerCharacter->GetActorLocation();
    
    InOutActors.Sort([&OwnerLocation](const AActor& A, const AActor& B)
    {
        const float DistA = FVector::DistSquared(OwnerLocation, A.GetActorLocation());
        const float DistB = FVector::DistSquared(OwnerLocation, B.GetActorLocation());
        return DistA < DistB;
    });
}

AActor* UTargetingComponent::FindBestTarget(const FVector& Direction) const
{
    TArray<AActor*> PotentialTargets;
    
    // Get all actors in range
    GetActorsInRange(PotentialTargets);
    
    // Filter by targetable class
    FilterByTargetableClass(PotentialTargets);
    
    // Filter by directional cone
    FilterByCone(PotentialTargets, Direction);
    
    // Filter by line of sight
    if (bRequireLineOfSight)
    {
        FilterByLineOfSight(PotentialTargets);
    }
    
    // Sort by distance
    SortByDistance(PotentialTargets);
    
    // Debug visualization
    if (bDebugDraw)
    {
        AActor* SelectedTarget = (PotentialTargets.Num() > 0) ? PotentialTargets[0] : nullptr;
        DrawDebugTargeting(PotentialTargets, SelectedTarget, Direction);
    }
    
    return (PotentialTargets.Num() > 0) ? PotentialTargets[0] : nullptr;
}

// ============================================================================
// INTERNAL HELPERS - MOTION WARPING
// ============================================================================

FVector UTargetingComponent::CalculateWarpLocation(AActor* Target, float MaxDistance) const
{
    if (!OwnerCharacter || !Target)
    {
        return FVector::ZeroVector;
    }
    
    const FVector OwnerLocation = OwnerCharacter->GetActorLocation();
    const FVector TargetLocation = Target->GetActorLocation();
    const FVector ToTarget = TargetLocation - OwnerLocation;
    const float Distance = ToTarget.Size();
    
    // If max distance not specified, use target location directly
    if (MaxDistance <= 0.0f)
    {
        return TargetLocation;
    }
    
    // If target is within max distance, use target location
    if (Distance <= MaxDistance)
    {
        return TargetLocation;
    }
    
    // Clamp to max distance
    return OwnerLocation + (ToTarget.GetSafeNormal() * MaxDistance);
}

// ============================================================================
// DEBUG VISUALIZATION
// ============================================================================

void UTargetingComponent::DrawDebugTargeting(const TArray<AActor*>& PotentialTargets, AActor* SelectedTarget, const FVector& SearchDirection) const
{
    if (!GetWorld() || !OwnerCharacter)
    {
        return;
    }
    
    const FVector OwnerLocation = OwnerCharacter->GetActorLocation();
    
    // Draw search cone
    DrawDebugCone(
        GetWorld(),
        OwnerLocation,
        SearchDirection,
        MaxTargetDistance,
        FMath::DegreesToRadians(DirectionalConeAngle),
        FMath::DegreesToRadians(DirectionalConeAngle),
        12,
        FColor::Yellow,
        false,
        0.1f
    );
    
    // Draw potential targets
    for (AActor* Target : PotentialTargets)
    {
        if (!Target)
        {
            continue;
        }
        
        const FColor Color = (Target == SelectedTarget) ? FColor::Green : FColor::Orange;
        DrawDebugSphere(GetWorld(), Target->GetActorLocation(), 50.0f, 12, Color, false, 0.1f);
        DrawDebugLine(GetWorld(), OwnerLocation, Target->GetActorLocation(), Color, false, 0.1f);
    }
}

// ============================================================================
// HELPER METHOD FROM OLD IMPLEMENTATION
// ============================================================================

EAttackDirection UTargetingComponent::GetAttackDirectionFromInput(FVector InputDirection) const
{
    if (!OwnerCharacter || InputDirection.IsNearlyZero())
    {
        return EAttackDirection::Forward;
    }
    
    // Convert to local space
    FVector LocalInput = OwnerCharacter->GetActorTransform().InverseTransformVector(InputDirection);
    LocalInput.Z = 0;
    LocalInput.Normalize();
    
    // Determine cardinal direction
    const float ForwardDot = FVector::DotProduct(LocalInput, FVector::ForwardVector);
    const float RightDot = FVector::DotProduct(LocalInput, FVector::RightVector);
    
    // Use absolute values to determine which axis is dominant
    if (FMath::Abs(ForwardDot) > FMath::Abs(RightDot))
    {
        return (ForwardDot > 0) ? EAttackDirection::Forward : EAttackDirection::Backward;
    }
    else
    {
        return (RightDot > 0) ? EAttackDirection::Right : EAttackDirection::Left;
    }
}