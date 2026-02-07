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

    // ====================================================================
    // WEAPON DATA RESOLUTION (follows same pattern as other components)
    // ====================================================================
    // Priority 1: WeaponData set directly on component (per-instance override)
    // Priority 2: CombatSettings->DefaultWeaponData (global default)
    // Priority 3: No weapon (nullptr)

    UWeaponData* ResolvedWeaponData = WeaponData;  // Priority 1: Direct override

    if (!ResolvedWeaponData)
    {
        // Priority 2: Get from CombatSettings
        if (UCombatSettings* Settings = GetOwnerCombatSettings())
        {
            ResolvedWeaponData = Settings->DefaultWeaponData;

            if (ResolvedWeaponData)
            {
                UE_LOG(LogWeaponComponent, Log, TEXT("[%s] Using WeaponData from CombatSettings: %s"),
                    *GetNameSafe(GetOwner()), *ResolvedWeaponData->GetDisplayNameString());
            }
        }
    }
    else
    {
        UE_LOG(LogWeaponComponent, Log, TEXT("[%s] Using WeaponData override: %s"),
            *GetNameSafe(GetOwner()), *ResolvedWeaponData->GetDisplayNameString());
    }

    // Initialize from resolved weapon data
    if (ResolvedWeaponData)
    {
        InitializeFromWeaponData(ResolvedWeaponData, true);
    }
}

void UWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    LastDeltaTime = DeltaTime;

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
    // Always clear hit actors for the new attack, even if already enabled.
    // During combo blends, the new montage's Active phase notify fires BEFORE
    // the old montage's OnMontageEnded callback (which disables hit detection).
    // Without this, enemies hit by the previous attack remain in HitActors
    // and are skipped by the weapon trace for the new attack.
    HitActors.Empty();

    if (bHitDetectionEnabled)
    {
        // Already tracing (combo blend race) - HitActors cleared above, reset trace state
        bFirstTrace = true;
        PreviousStartLocation = GetSocketLocation(GetEffectiveStartSocket());
        PreviousTipLocation = GetSocketLocation(GetEffectiveEndSocket());
        return;
    }

    bHitDetectionEnabled = true;
    bFirstTrace = true;
    SetComponentTickEnabled(true);

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

    // HIT-1 FIX: Log error instead of silently falling back to actor center.
    // Falling back to character center produces incorrect hit directions and misleading VFX.
    if (OwnerCharacter)
    {
        UE_LOG(LogWeaponComponent, Error, TEXT("[%s] Socket '%s' not found on character or weapon mesh! "
            "OwnerMesh=%s, SpawnedWeapon=%s. Returning actor location as fallback - hit traces will be inaccurate."),
            *GetNameSafe(GetOwner()), *SocketName.ToString(),
            OwnerMesh ? *OwnerMesh->GetName() : TEXT("null"),
            SpawnedWeaponMesh ? *SpawnedWeaponMesh->GetName() : TEXT("null"));
        return OwnerCharacter->GetActorLocation();
    }

    UE_LOG(LogWeaponComponent, Warning, TEXT("Socket '%s' not found and no owner character! Returning zero vector."),
        *SocketName.ToString());
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

    const FVector CurrentStartLocation = GetSocketLocation(GetEffectiveStartSocket());
    const FVector CurrentTipLocation = GetSocketLocation(GetEffectiveEndSocket());

    // Skip first trace to avoid hitting at spawn
    if (bFirstTrace)
    {
        PreviousStartLocation = CurrentStartLocation;
        PreviousTipLocation = CurrentTipLocation;
        CachedWeaponTipVelocity = FVector::ZeroVector;
        bFirstTrace = false;
        return;
    }

    // Compute real weapon tip velocity from frame-to-frame position delta
    if (LastDeltaTime > KINDA_SMALL_NUMBER)
    {
        CachedWeaponTipVelocity = (CurrentTipLocation - PreviousTipLocation) / LastDeltaTime;
    }

    // Setup trace parameters (shared across all substeps)
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

    const float EffectiveRadius = GetEffectiveTraceRadius();
    bool bAnyHit = false;
    FHitResult FirstHit;

    // Substep interpolation: subdivide the frame into N intermediate sweeps
    // This prevents thin targets from slipping between frames at low FPS
    const int32 NumSteps = FMath::Max(1, SubstepCount);

    for (int32 Step = 0; Step < NumSteps; ++Step)
    {
        // Interpolation range for this substep
        const float Alpha0 = static_cast<float>(Step) / static_cast<float>(NumSteps);
        const float Alpha1 = static_cast<float>(Step + 1) / static_cast<float>(NumSteps);

        // Interpolate weapon positions for this substep
        const FVector StepPrevStart = FMath::Lerp(PreviousStartLocation, CurrentStartLocation, Alpha0);
        const FVector StepPrevTip = FMath::Lerp(PreviousTipLocation, CurrentTipLocation, Alpha0);
        const FVector StepCurrStart = FMath::Lerp(PreviousStartLocation, CurrentStartLocation, Alpha1);
        const FVector StepCurrTip = FMath::Lerp(PreviousTipLocation, CurrentTipLocation, Alpha1);

        // Calculate capsule for this substep's end position
        const FVector WeaponAxis = StepCurrTip - StepCurrStart;
        const float WeaponLength = WeaponAxis.Size();
        const float HalfHeight = FMath::Max(WeaponLength * 0.5f, EffectiveRadius + 1.0f);
        const FVector WeaponCenter = StepCurrStart + WeaponAxis * 0.5f;
        const FQuat CapsuleRotation = FQuat::FindBetweenNormals(FVector::UpVector, WeaponAxis.GetSafeNormal());

        // Sweep from previous substep center to current substep center
        const FVector PrevWeaponAxis = StepPrevTip - StepPrevStart;
        const FVector PrevCenter = StepPrevStart + PrevWeaponAxis * 0.5f;

        TArray<FHitResult> HitResults;
        UWorld* TraceWorld = GetWorld();
        if (!TraceWorld)
        {
            break;
        }
        const bool bHit = TraceWorld->SweepMultiByChannel(
            HitResults,
            PrevCenter,
            WeaponCenter,
            CapsuleRotation,
            TraceChannel,
            FCollisionShape::MakeCapsule(EffectiveRadius, HalfHeight),
            QueryParams
        );

        if (bHit)
        {
            if (!bAnyHit && HitResults.Num() > 0)
            {
                FirstHit = HitResults[0];
            }
            bAnyHit = true;

            for (const FHitResult& Hit : HitResults)
            {
                if (Hit.GetActor() && Hit.GetActor() != OwnerCharacter)
                {
                    ProcessHit(Hit);

                    // Update ignored actors for subsequent substeps
                    if (WasActorAlreadyHit(Hit.GetActor()))
                    {
                        QueryParams.AddIgnoredActor(Hit.GetActor());
                    }
                }
            }
        }
    }

    // Debug visualization - pass full weapon geometry (final frame positions)
    UDebugUtils::DrawWeaponTrace(
        GetWorld(),
        CurrentStartLocation,
        CurrentTipLocation,
        PreviousStartLocation,
        PreviousTipLocation,
        EffectiveRadius,
        bAnyHit,
        bAnyHit ? FirstHit : FHitResult());

    // Store current positions for next frame
    PreviousStartLocation = CurrentStartLocation;
    PreviousTipLocation = CurrentTipLocation;
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
    // Priority 1: WeaponData's attack configuration (weapon override)
    if (WeaponData && WeaponData->AttackConfiguration)
    {
        return WeaponData->AttackConfiguration;
    }

    // Priority 2: CombatSettings → DefaultWeaponData → AttackConfiguration (global default)
    if (UCombatSettings* Settings = GetOwnerCombatSettings())
    {
        return Settings->GetAttackConfiguration();
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