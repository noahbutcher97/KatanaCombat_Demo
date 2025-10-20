// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/WeaponComponent.h"
#include "Core/CombatComponent.h"
#include "Data/AttackData.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"

UWeaponComponent::UWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false; // Only tick when hit detection enabled
}

void UWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // Cache owner references
    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter)
    {
        OwnerMesh = OwnerCharacter->GetMesh();
    }
}

void UWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (bHitDetectionEnabled)
    {
        PerformWeaponTrace();
    }
}

// ============================================================================
// HIT DETECTION CONTROL
// ============================================================================

void UWeaponComponent::EnableHitDetection()
{
    if (bHitDetectionEnabled)
    {
        return;
    }
    
    bHitDetectionEnabled = true;
    bFirstTrace = true;
    SetComponentTickEnabled(true);
    
    // Store initial positions
    PreviousStartLocation = GetSocketLocation(WeaponStartSocket);
    PreviousTipLocation = GetSocketLocation(WeaponEndSocket);
}

void UWeaponComponent::DisableHitDetection()
{
    bHitDetectionEnabled = false;
    SetComponentTickEnabled(false);
}

void UWeaponComponent::ResetHitActors()
{
    HitActors.Empty();
    bFirstTrace = true;
}

// ============================================================================
// SOCKET CONFIGURATION
// ============================================================================

void UWeaponComponent::SetWeaponSockets(FName StartSocket, FName EndSocket)
{
    WeaponStartSocket = StartSocket;
    WeaponEndSocket = EndSocket;
}

FVector UWeaponComponent::GetSocketLocation(FName SocketName) const
{
    if (OwnerMesh && OwnerMesh->DoesSocketExist(SocketName))
    {
        return OwnerMesh->GetSocketLocation(SocketName);
    }
    
    // Fallback to character location
    if (OwnerCharacter)
    {
        return OwnerCharacter->GetActorLocation();
    }
    
    return FVector::ZeroVector;
}

// ============================================================================
// HIT QUERIES
// ============================================================================

bool UWeaponComponent::WasActorAlreadyHit(AActor* Actor) const
{
    return HitActors.Contains(Actor);
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

void UWeaponComponent::PerformWeaponTrace()
{
    if (!OwnerCharacter || !OwnerMesh)
    {
        return;
    }
    
    const FVector StartLocation = GetSocketLocation(WeaponStartSocket);
    const FVector EndLocation = GetSocketLocation(WeaponEndSocket);
    
    // Skip first trace to avoid hitting at spawn
    if (bFirstTrace)
    {
        PreviousStartLocation = StartLocation;
        PreviousTipLocation = EndLocation;
        bFirstTrace = false;
        return;
    }
    
    // Setup trace parameters
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter);
    QueryParams.bTraceComplex = false;
    QueryParams.bReturnPhysicalMaterial = false;
    
    // Ignore already hit actors
    for (AActor* HitActor : HitActors)
    {
        if (HitActor)
        {
            QueryParams.AddIgnoredActor(HitActor);
        }
    }
    
    // Perform swept sphere trace from previous to current position
    TArray<FHitResult> HitResults;
    const bool bHit = GetWorld()->SweepMultiByChannel(
        HitResults,
        PreviousTipLocation,
        EndLocation,
        FQuat::Identity,
        TraceChannel,
        FCollisionShape::MakeSphere(TraceRadius),
        QueryParams
    );
    
    // Process all hits
    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            if (Hit.GetActor() && Hit.GetActor() != OwnerCharacter)
            {
                ProcessHit(Hit);
            }
        }
    }
    
    // Debug visualization
    if (bDebugDraw)
    {
        DrawDebugTrace(PreviousTipLocation, EndLocation, bHit, bHit ? HitResults[0] : FHitResult());
    }
    
    // Store current positions for next frame
    PreviousStartLocation = StartLocation;
    PreviousTipLocation = EndLocation;
}

void UWeaponComponent::ProcessHit(const FHitResult& Hit)
{
    AActor* HitActor = Hit.GetActor();
    
    if (!HitActor || WasActorAlreadyHit(HitActor))
    {
        return;
    }
    
    // Add to hit list
    AddHitActor(HitActor);
    
    // Get current attack data
    UAttackData* AttackData = GetCurrentAttackData();
    
    // Broadcast hit event
    OnWeaponHit.Broadcast(HitActor, Hit, AttackData);
}

void UWeaponComponent::AddHitActor(AActor* Actor)
{
    if (Actor && !WasActorAlreadyHit(Actor))
    {
        HitActors.Add(Actor);
    }
}

UAttackData* UWeaponComponent::GetCurrentAttackData() const
{
    if (!OwnerCharacter)
    {
        return nullptr;
    }
    
    // Get combat component from owner
    if (UCombatComponent* CombatComp = OwnerCharacter->FindComponentByClass<UCombatComponent>())
    {
        return CombatComp->GetCurrentAttack();
    }
    
    return nullptr;
}

void UWeaponComponent::DrawDebugTrace(const FVector& Start, const FVector& End, bool bHit, const FHitResult& Hit) const
{
    if (!GetWorld())
    {
        return;
    }
    
    const FColor TraceColor = bHit ? FColor::Red : FColor::Green;
    
    // Draw line from start to end
    DrawDebugLine(GetWorld(), Start, End, TraceColor, false, DebugDrawDuration, 0, 2.0f);
    
    // Draw sphere at tip
    DrawDebugSphere(GetWorld(), End, TraceRadius, 12, TraceColor, false, DebugDrawDuration);
    
    // Draw hit point if we hit something
    if (bHit)
    {
        DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 10.0f, FColor::Orange, false, DebugDrawDuration);
        DrawDebugLine(GetWorld(), Hit.ImpactPoint, Hit.ImpactPoint + Hit.ImpactNormal * 30.0f, 
                     FColor::Yellow, false, DebugDrawDuration, 0, 2.0f);
    }
}