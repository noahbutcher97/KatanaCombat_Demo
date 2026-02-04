// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/WeaponComponent.h"
#include "Core/CombatComponent.h"
#include "Characters/BaseCombatCharacter.h"
#include "Data/AttackData.h"
#include "Data/AttackConfiguration.h"
#include "Data/WeaponData.h"
#include "Data/CombatSettings.h"
#include "Debug/DebugConfig.h"
#include "Debug/DebugUtils.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponComponent, Log, All);

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

    // Initialize from WeaponData if set
    if (WeaponData)
    {
        InitializeFromWeaponData(WeaponData, true);
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

    // Clear hit actors from previous attack - critical for allowing re-hits on new attacks
    HitActors.Empty();

    // Store initial positions
    PreviousStartLocation = GetSocketLocation(GetEffectiveStartSocket());
    PreviousTipLocation = GetSocketLocation(GetEffectiveEndSocket());

    // Debug: Log hit detection configuration
    if (CombatDebug::IsWeaponDebugEnabled())
    {
        const FName StartSocket = GetEffectiveStartSocket();
        const FName EndSocket = GetEffectiveEndSocket();
        const bool bUsingWeaponMesh = WeaponData && !WeaponData->bUseCharacterSocketsForTrace;

        UE_LOG(LogWeaponComponent, Log, TEXT("[%s] Hit detection ENABLED:"), *GetNameSafe(GetOwner()));
        UE_LOG(LogWeaponComponent, Log, TEXT("  - Socket source: %s"), bUsingWeaponMesh ? TEXT("Weapon Mesh") : TEXT("Character Mesh"));
        UE_LOG(LogWeaponComponent, Log, TEXT("  - Start socket: %s -> %s"), *StartSocket.ToString(), *PreviousStartLocation.ToString());
        UE_LOG(LogWeaponComponent, Log, TEXT("  - End socket: %s -> %s"), *EndSocket.ToString(), *PreviousTipLocation.ToString());
        UE_LOG(LogWeaponComponent, Log, TEXT("  - Trace radius: %.1f"), GetEffectiveTraceRadius());

        // Warn if positions are identical (likely socket not found)
        if (PreviousStartLocation.Equals(PreviousTipLocation, 1.0f))
        {
            UE_LOG(LogWeaponComponent, Warning, TEXT("[%s] Start and End socket locations are identical! Check socket configuration."),
                *GetNameSafe(GetOwner()));
        }
    }
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
    // Priority 1: Check spawned weapon mesh for sockets (when NOT using character sockets)
    // This supports weapons with sockets defined on the weapon static mesh itself
    if (SpawnedWeaponMesh && WeaponData && !WeaponData->bUseCharacterSocketsForTrace)
    {
        if (SpawnedWeaponMesh->DoesSocketExist(SocketName))
        {
            return SpawnedWeaponMesh->GetSocketLocation(SocketName);
        }
        else
        {
            // Log warning if socket not found on weapon mesh
            UE_LOG(LogWeaponComponent, Warning, TEXT("[%s] Socket '%s' not found on weapon mesh! Check socket names on static mesh."),
                *GetNameSafe(GetOwner()), *SocketName.ToString());
        }
    }

    // Priority 2: Check character skeletal mesh for sockets
    if (OwnerMesh && OwnerMesh->DoesSocketExist(SocketName))
    {
        return OwnerMesh->GetSocketLocation(SocketName);
    }

    // Fallback to character location
    if (OwnerCharacter)
    {
        UE_LOG(LogWeaponComponent, Warning, TEXT("[%s] Socket '%s' not found on character or weapon mesh! Falling back to actor location."),
            *GetNameSafe(GetOwner()), *SocketName.ToString());
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

    const FVector StartLocation = GetSocketLocation(GetEffectiveStartSocket());
    const FVector EndLocation = GetSocketLocation(GetEffectiveEndSocket());

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

    // Calculate capsule parameters for weapon-length trace
    // The capsule spans from WeaponStart to WeaponEnd with the trace radius
    const float EffectiveRadius = GetEffectiveTraceRadius();
    const FVector WeaponAxis = EndLocation - StartLocation;
    const float WeaponLength = WeaponAxis.Size();
    const float HalfHeight = WeaponLength * 0.5f;
    const FVector WeaponCenter = StartLocation + WeaponAxis * 0.5f;

    // Calculate capsule rotation to align with weapon axis
    const FQuat CapsuleRotation = FQuat::FindBetweenNormals(FVector::UpVector, WeaponAxis.GetSafeNormal());

    // Perform capsule sweep from previous center to current center
    // This sweeps the entire blade volume through space
    const FVector PreviousWeaponAxis = PreviousTipLocation - PreviousStartLocation;
    const FVector PreviousCenter = PreviousStartLocation + PreviousWeaponAxis * 0.5f;

    TArray<FHitResult> HitResults;
    const bool bHit = GetWorld()->SweepMultiByChannel(
        HitResults,
        PreviousCenter,
        WeaponCenter,
        CapsuleRotation,
        TraceChannel,
        FCollisionShape::MakeCapsule(EffectiveRadius, HalfHeight),
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

    // Debug visualization - pass full weapon geometry
    UDebugUtils::DrawWeaponTrace(
        GetWorld(),
        StartLocation,
        EndLocation,
        PreviousStartLocation,
        PreviousTipLocation,
        EffectiveRadius,
        bHit,
        bHit && HitResults.Num() > 0 ? HitResults[0] : FHitResult());

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

    // Filter out dead/dying actors at the trace level - they shouldn't count as hits
    if (const ABaseCombatCharacter* CombatChar = Cast<ABaseCombatCharacter>(HitActor))
    {
        if (CombatChar->IsDeadOrDying())
        {
            return;
        }
    }

    // Add to hit list (only living actors reach here)
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

    // Get current attack from CombatComponent
    if (UCombatComponent* CombatComp = OwnerCharacter->FindComponentByClass<UCombatComponent>())
    {
        return CombatComp->GetCurrentAttack();
    }

    return nullptr;
}

// ============================================================================
// WEAPON EQUIP STATE
// ============================================================================

void UWeaponComponent::Equip()
{
    if (bIsEquipped)
    {
        return;
    }

    bIsEquipped = true;

    // Move mesh to equipped socket
    if (SpawnedWeaponMesh && WeaponData)
    {
        AttachMeshToSocket(WeaponData->EquippedSocket);
        UE_LOG(LogWeaponComponent, Log, TEXT("[%s] Weapon equipped to socket: %s"),
            *GetNameSafe(GetOwner()), *WeaponData->EquippedSocket.ToString());
    }
}

void UWeaponComponent::Holster()
{
    if (!bIsEquipped)
    {
        return;
    }

    bIsEquipped = false;

    // Move mesh to holstered socket
    if (SpawnedWeaponMesh && WeaponData)
    {
        AttachMeshToSocket(WeaponData->HolsteredSocket);
        UE_LOG(LogWeaponComponent, Log, TEXT("[%s] Weapon holstered to socket: %s"),
            *GetNameSafe(GetOwner()), *WeaponData->HolsteredSocket.ToString());
    }
}

bool UWeaponComponent::PlayEquipAnimation()
{
    if (!WeaponData || !WeaponData->EquipMontage || !OwnerCharacter)
    {
        UE_LOG(LogWeaponComponent, Warning, TEXT("[%s] Cannot play equip animation - missing WeaponData, EquipMontage, or Owner"),
            *GetNameSafe(GetOwner()));
        return false;
    }

    UAnimInstance* AnimInstance = OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance)
    {
        UE_LOG(LogWeaponComponent, Warning, TEXT("[%s] Cannot play equip animation - no AnimInstance or Mesh"),
            *GetNameSafe(GetOwner()));
        return false;
    }

    // Play the montage - AnimNotify_WeaponEquip in the montage will call Equip() at the right frame
    const float Duration = AnimInstance->Montage_Play(WeaponData->EquipMontage, WeaponData->EquipPlayRate);

    UE_LOG(LogWeaponComponent, Log, TEXT("[%s] Playing equip animation: %s (Duration: %.2f)"),
        *GetNameSafe(GetOwner()),
        *WeaponData->EquipMontage->GetName(),
        Duration);

    return Duration > 0.0f;
}

bool UWeaponComponent::PlayHolsterAnimation()
{
    if (!WeaponData || !WeaponData->HolsterMontage || !OwnerCharacter)
    {
        UE_LOG(LogWeaponComponent, Warning, TEXT("[%s] Cannot play holster animation - missing WeaponData, HolsterMontage, or Owner"),
            *GetNameSafe(GetOwner()));
        return false;
    }

    UAnimInstance* AnimInstance = OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance)
    {
        UE_LOG(LogWeaponComponent, Warning, TEXT("[%s] Cannot play holster animation - no AnimInstance or Mesh"),
            *GetNameSafe(GetOwner()));
        return false;
    }

    // Play the montage - AnimNotify_WeaponHolster in the montage will call Holster() at the right frame
    const float Duration = AnimInstance->Montage_Play(WeaponData->HolsterMontage, WeaponData->HolsterPlayRate);

    UE_LOG(LogWeaponComponent, Log, TEXT("[%s] Playing holster animation: %s (Duration: %.2f)"),
        *GetNameSafe(GetOwner()),
        *WeaponData->HolsterMontage->GetName(),
        Duration);

    return Duration > 0.0f;
}

void UWeaponComponent::InitializeFromWeaponData(UWeaponData* NewWeaponData, bool bStartEquipped)
{
    // Clean up existing weapon
    DestroyWeaponMesh();

    // Update weapon data reference
    WeaponData = NewWeaponData;

    if (!WeaponData)
    {
        UE_LOG(LogWeaponComponent, Log, TEXT("[%s] Weapon cleared"), *GetNameSafe(GetOwner()));
        return;
    }

    // Spawn mesh
    SpawnWeaponMesh();

    // Set initial state
    bIsEquipped = bStartEquipped;
    if (SpawnedWeaponMesh)
    {
        AttachMeshToSocket(bIsEquipped ? WeaponData->EquippedSocket : WeaponData->HolsteredSocket);
    }

    UE_LOG(LogWeaponComponent, Log, TEXT("[%s] Initialized weapon: %s (Equipped: %s)"),
        *GetNameSafe(GetOwner()),
        *WeaponData->GetDisplayNameString(),
        bIsEquipped ? TEXT("Yes") : TEXT("No"));
}

// ============================================================================
// ATTACK CONFIGURATION ACCESS
// ============================================================================

UAttackConfiguration* UWeaponComponent::GetEffectiveAttackConfiguration() const
{
    // Priority 1: WeaponData's attack configuration
    if (WeaponData && WeaponData->AttackConfiguration)
    {
        return WeaponData->AttackConfiguration;
    }

    // Priority 2: CombatSettings' attack configuration
    if (UCombatSettings* Settings = GetOwnerCombatSettings())
    {
        return Settings->AttackConfiguration;
    }

    return nullptr;
}

float UWeaponComponent::GetDamageMultiplier() const
{
    if (WeaponData)
    {
        return WeaponData->DamageMultiplier;
    }
    return 1.0f;
}

float UWeaponComponent::GetWeaponReach() const
{
    if (WeaponData)
    {
        return WeaponData->WeaponReach;
    }
    return 150.0f; // Default reach
}

// ============================================================================
// INTERNAL HELPERS - WEAPON DATA
// ============================================================================

FName UWeaponComponent::GetEffectiveStartSocket() const
{
    if (WeaponData)
    {
        return WeaponData->TraceStartSocket;
    }
    return WeaponStartSocket;
}

FName UWeaponComponent::GetEffectiveEndSocket() const
{
    if (WeaponData)
    {
        return WeaponData->TraceEndSocket;
    }
    return WeaponEndSocket;
}

float UWeaponComponent::GetEffectiveTraceRadius() const
{
    if (WeaponData)
    {
        return WeaponData->TraceRadius;
    }
    return TraceRadius;
}

void UWeaponComponent::SpawnWeaponMesh()
{
    if (!WeaponData || !OwnerCharacter)
    {
        return;
    }

    // Check if mesh asset is valid
    if (WeaponData->WeaponMesh.IsNull())
    {
        UE_LOG(LogWeaponComponent, Warning, TEXT("[%s] WeaponData '%s' has no mesh assigned"),
            *GetNameSafe(GetOwner()), *WeaponData->GetDisplayNameString());
        return;
    }

    // Load mesh synchronously (could be async in production)
    UStaticMesh* MeshAsset = WeaponData->WeaponMesh.LoadSynchronous();
    if (!MeshAsset)
    {
        UE_LOG(LogWeaponComponent, Warning, TEXT("[%s] Failed to load weapon mesh for '%s'"),
            *GetNameSafe(GetOwner()), *WeaponData->GetDisplayNameString());
        return;
    }

    // Create mesh component
    SpawnedWeaponMesh = NewObject<UStaticMeshComponent>(OwnerCharacter, TEXT("WeaponMesh"));
    if (!SpawnedWeaponMesh)
    {
        UE_LOG(LogWeaponComponent, Error, TEXT("[%s] Failed to create weapon mesh component"),
            *GetNameSafe(GetOwner()));
        return;
    }

    // Configure mesh component
    SpawnedWeaponMesh->SetStaticMesh(MeshAsset);
    SpawnedWeaponMesh->SetWorldScale3D(WeaponData->MeshScale);
    SpawnedWeaponMesh->SetRelativeTransform(WeaponData->MeshAttachOffset);
    SpawnedWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SpawnedWeaponMesh->RegisterComponent();

    UE_LOG(LogWeaponComponent, Log, TEXT("[%s] Spawned weapon mesh: %s"),
        *GetNameSafe(GetOwner()), *MeshAsset->GetName());
}

void UWeaponComponent::DestroyWeaponMesh()
{
    if (SpawnedWeaponMesh)
    {
        SpawnedWeaponMesh->DestroyComponent();
        SpawnedWeaponMesh = nullptr;
    }
}

void UWeaponComponent::AttachMeshToSocket(FName SocketName)
{
    if (!SpawnedWeaponMesh || !OwnerMesh)
    {
        return;
    }

    SpawnedWeaponMesh->AttachToComponent(
        OwnerMesh,
        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        SocketName
    );

    // Apply offset transform after attachment
    if (WeaponData)
    {
        SpawnedWeaponMesh->SetRelativeTransform(WeaponData->MeshAttachOffset);
    }
}

UCombatSettings* UWeaponComponent::GetOwnerCombatSettings() const
{
    if (!OwnerCharacter)
    {
        return nullptr;
    }

    // Try to get CombatSettings from BaseCombatCharacter via property reflection
    // This avoids hard dependency on BaseCombatCharacter header
    if (FProperty* SettingsProp = OwnerCharacter->GetClass()->FindPropertyByName(TEXT("CombatSettings")))
    {
        if (FObjectProperty* ObjProp = CastField<FObjectProperty>(SettingsProp))
        {
            return Cast<UCombatSettings>(ObjProp->GetObjectPropertyValue_InContainer(OwnerCharacter));
        }
    }

    return nullptr;
}