// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/TargetingComponent.h"
#include "Debug/DebugConfig.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Interfaces/DamageableInterface.h"
#include "Interfaces/TeamMemberInterface.h"
#include "Data/CombatSettings.h"
#include "Characters/BaseCombatCharacter.h"

UTargetingComponent::UTargetingComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    DirectionalConeAngle = 60.0f;
    MaxTargetDistance = 1000.0f;
    bRequireLineOfSight = true;
    LineOfSightChannel = ECC_Visibility;
    // Debug visualization is now controlled via Combat.Debug.Targeting CVar

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
    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        return nullptr;
    }

    const FVector SearchDirection = GetDirectionVector(Direction, false);
    return FindBestTarget(SearchDirection);
}

AActor* UTargetingComponent::FindTargetInDirection(const FVector& DirectionVector)
{
    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner || DirectionVector.IsNearlyZero())
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
    if (!Target || Direction.IsNearlyZero())
    {
        return false;
    }

    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        return false;
    }

    const float ConeAngle = (AngleTolerance > 0.0f) ? AngleTolerance : DirectionalConeAngle;

    const FVector ToTarget = (Target->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
    const float DotProduct = FVector::DotProduct(Direction, ToTarget);
    const float Angle = FMath::RadiansToDegrees(FMath::Acos(DotProduct));

    return Angle <= ConeAngle;
}

bool UTargetingComponent::HasLineOfSightTo(AActor* Target) const
{
    if (!Target || !GetWorld())
    {
        return false;
    }

    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        return false;
    }

    const FVector Start = Owner->GetActorLocation();
    const FVector End = Target->GetActorLocation();

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Owner);
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
    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        return FVector::ForwardVector;
    }

    if (Direction == EAttackDirection::None || Direction == EAttackDirection::Forward)
    {
        if (bUseCamera)
        {
            if (const APlayerController* PC = Cast<APlayerController>(Owner->GetController()))
            {
                FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
                CameraRotation.Pitch = 0.0f;
                CameraRotation.Roll = 0.0f;
                return FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::X);
            }
        }

        return Owner->GetActorForwardVector();
    }

    FVector BaseForward = Owner->GetActorForwardVector();
    FVector BaseRight = Owner->GetActorRightVector();
    
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
    if (!Target)
    {
        return 0.0f;
    }

    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        return 0.0f;
    }

    const FVector Forward = Owner->GetActorForwardVector();
    const FVector ToTarget = (Target->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
    
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
    if (!Target)
    {
        return 0.0f;
    }

    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        return 0.0f;
    }

    return FVector::Dist(Owner->GetActorLocation(), Target->GetActorLocation());
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
    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!MotionWarpingComponent || !Target || !Owner)
    {
        return false;
    }

    const FVector WarpLocation = CalculateWarpLocation(Target, MaxDistance);
    const FRotator LookAtRotation = (Target->GetActorLocation() - Owner->GetActorLocation()).Rotation();
    
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
// SOFT AIM ASSIST
// ============================================================================

FRotator UTargetingComponent::FindBestTargetForDirection(
    const FVector& InputDirection,
    AActor*& OutBestTarget,
    float MaxRange,
    float GradientAngle,
    float OppositeAngle,
    float AngleWeight,
    float DistanceWeight)
{
    OutBestTarget = nullptr;

    // Lazy fetch owner for test compatibility
    ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());

    if (!Owner || InputDirection.IsNearlyZero())
    {
        return Owner ? Owner->GetActorRotation() : FRotator::ZeroRotator;
    }

    // Get CombatSettings from owner if it's a BaseCombatCharacter
    const UCombatSettings* Settings = nullptr;
    if (ABaseCombatCharacter* CombatChar = Cast<ABaseCombatCharacter>(Owner))
    {
        Settings = CombatChar->CombatSettings;
    }

    // Use provided values or fall back to CombatSettings defaults
    const float UseMaxRange = (MaxRange > 0.0f) ? MaxRange : (Settings ? Settings->DirectionalTargetingRange : 500.0f);
    const float UseGradientAngle = (GradientAngle > 0.0f) ? GradientAngle : (Settings ? Settings->GradientAngleThreshold : 45.0f);
    const float UseOppositeAngle = (OppositeAngle > 0.0f) ? OppositeAngle : (Settings ? Settings->OppositeAngleThreshold : 120.0f);
    const float UseAngleWeight = (AngleWeight >= 0.0f) ? AngleWeight : (Settings ? Settings->DirectionalAngleWeight : 0.7f);
    const float UseDistanceWeight = (DistanceWeight >= 0.0f) ? DistanceWeight : (Settings ? Settings->DirectionalDistanceWeight : 0.3f);

    const FVector OwnerLocation = Owner->GetActorLocation();
    const FVector NormalizedInput = InputDirection.GetSafeNormal();

    // Get all potential targets in range
    TArray<AActor*> PotentialTargets;
    GetActorsInRange(PotentialTargets);
    FilterByTargetableClass(PotentialTargets);

    if (bRequireLineOfSight)
    {
        FilterByLineOfSight(PotentialTargets);
    }

    // Score each target
    float BestScore = -1.0f;
    AActor* BestTarget = nullptr;

    for (AActor* Target : PotentialTargets)
    {
        if (!Target)
        {
            continue;
        }

        const FVector ToTarget = Target->GetActorLocation() - OwnerLocation;
        const float Distance = ToTarget.Size();

        // Skip if out of range
        if (Distance > UseMaxRange)
        {
            continue;
        }

        const FVector ToTargetNorm = ToTarget.GetSafeNormal();
        const float DotProduct = FVector::DotProduct(NormalizedInput, ToTargetNorm);
        const float AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f)));

        // Skip if target is in "opposite" direction
        if (AngleToTarget > UseOppositeAngle)
        {
            continue;
        }

        // Calculate scores
        // Angle score: 1.0 = perfect alignment, 0.0 = at gradient threshold
        const float AngleScore = FMath::Clamp(1.0f - (AngleToTarget / UseGradientAngle), 0.0f, 1.0f);

        // Distance score: 1.0 = at owner location, 0.0 = at max range
        const float DistanceScore = FMath::Clamp(1.0f - (Distance / UseMaxRange), 0.0f, 1.0f);

        // Combined weighted score
        const float TotalScore = (AngleScore * UseAngleWeight) + (DistanceScore * UseDistanceWeight);

        if (TotalScore > BestScore)
        {
            BestScore = TotalScore;
            BestTarget = Target;
        }
    }

    // Debug visualization (CVar-controlled)
    if (CombatDebug::IsTargetingDebugEnabled())
    {
        DrawDebugTargeting(PotentialTargets, BestTarget, NormalizedInput);
    }

    OutBestTarget = BestTarget;

    // Return rotation toward best target if found, otherwise toward input direction
    if (BestTarget)
    {
        return (BestTarget->GetActorLocation() - OwnerLocation).Rotation();
    }
    else
    {
        return NormalizedInput.Rotation();
    }
}

bool UTargetingComponent::SetupDirectionalWarp(const FVector& InputDirection, const FDirectionalWarpConfig& Config)
{
    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!MotionWarpingComponent || !Owner || InputDirection.IsNearlyZero())
    {
        return false;
    }

    if (!Config.bEnableDirectionalWarp)
    {
        return false;
    }

    // Calculate rotation toward input direction
    const FRotator TargetRotation = InputDirection.GetSafeNormal().Rotation();

    // For rotation-only warping, we use owner's location but different rotation
    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
        Config.DirectionalWarpTargetName,
        Owner->GetActorLocation(),
        TargetRotation
    );

    return true;
}

// ============================================================================
// INTERNAL HELPERS - TARGET FINDING
// ============================================================================

void UTargetingComponent::GetActorsInRange(TArray<AActor*>& OutActors) const
{
    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());

    if (!Owner || !GetWorld())
    {
        return;
    }

    const FVector OwnerLocation = Owner->GetActorLocation();

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Owner);

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
        AActor* Actor = Overlap.GetActor();
        if (!Actor)
        {
            continue;
        }

        // Must implement IDamageableInterface (can be targeted)
        if (!Actor->Implements<UDamageableInterface>())
        {
            continue;
        }

        // Must be alive
        if (!IDamageableInterface::Execute_IsAlive(Actor))
        {
            continue;
        }

        // Check team hostility (if owner implements ITeamMemberInterface)
        if (Owner->Implements<UTeamMemberInterface>())
        {
            if (!ITeamMemberInterface::Execute_IsHostileTo(Owner, Actor))
            {
                continue; // Skip friendly actors
            }
        }

        OutActors.Add(Actor);
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
    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        return;
    }

    const FVector OwnerLocation = Owner->GetActorLocation();
    
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
    
    // Debug visualization (CVar-controlled)
    if (CombatDebug::IsTargetingDebugEnabled())
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
    if (!Target)
    {
        return FVector::ZeroVector;
    }

    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        return FVector::ZeroVector;
    }

    const FVector OwnerLocation = Owner->GetActorLocation();
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
    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!GetWorld() || !Owner)
    {
        return;
    }

    const FVector OwnerLocation = Owner->GetActorLocation();
    const float DrawDuration = CombatDebug::GetDebugDrawDuration();

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
        DrawDuration
    );

    // Draw potential targets
    for (AActor* Target : PotentialTargets)
    {
        if (!Target)
        {
            continue;
        }

        const FColor Color = (Target == SelectedTarget) ? FColor::Green : FColor::Orange;
        DrawDebugSphere(GetWorld(), Target->GetActorLocation(), 50.0f, 12, Color, false, DrawDuration);
        DrawDebugLine(GetWorld(), OwnerLocation, Target->GetActorLocation(), Color, false, DrawDuration);
    }
}

// ============================================================================
// HELPER METHOD FROM OLD IMPLEMENTATION
// ============================================================================

EAttackDirection UTargetingComponent::GetAttackDirectionFromInput(FVector InputDirection) const
{
    if (InputDirection.IsNearlyZero())
    {
        return EAttackDirection::Forward;
    }

    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        return EAttackDirection::Forward;
    }

    // Convert to local space
    FVector LocalInput = Owner->GetActorTransform().InverseTransformVector(InputDirection);
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