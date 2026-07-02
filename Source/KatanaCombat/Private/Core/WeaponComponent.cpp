// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/WeaponComponent.h"
#include "Core/CombatComponent.h"
#include "Characters/BaseCombatCharacter.h"
#include "Data/AttackData.h"
#include "Data/AttackConfiguration.h"
#include "Data/WeaponData.h"
#include "Data/CombatSettings.h"
#include "Interfaces/TeamMemberInterface.h"
#include "Utilities/WeaponTraceLibrary.h"
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
    HitActors.Empty();

    // Initialize blade trace points at current socket positions.
    // This anchors the first tick's sweep to real weapon movement only.
    PreviousTracePoints = ComputeCurrentTracePoints();
    CachedWeaponTipVelocity = FVector::ZeroVector;

    if (bHitDetectionEnabled)
    {
        // Already tracing (combo blend race) - HitActors cleared and positions
        // refreshed above. No need to re-enable tick.
        return;
    }

    bHitDetectionEnabled = true;
    SetComponentTickEnabled(true);

    // Debug: Log hit detection configuration
    if (CombatDebug::IsWeaponDebugEnabled())
    {
        const FName StartSocket = GetEffectiveStartSocket();
        const FName EndSocket = GetEffectiveEndSocket();
        const bool bUsingWeaponMesh = WeaponData && !WeaponData->bUseCharacterSocketsForTrace;
        const FVector BasePos = PreviousTracePoints.Num() > 0 ? PreviousTracePoints[0] : FVector::ZeroVector;
        const FVector TipPos = PreviousTracePoints.Num() > 1 ? PreviousTracePoints.Last() : BasePos;

        UE_LOG(LogWeaponComponent, Log, TEXT("[%s] Hit detection ENABLED:"), *GetNameSafe(GetOwner()));
        UE_LOG(LogWeaponComponent, Log, TEXT("  - Socket source: %s"), bUsingWeaponMesh ? TEXT("Weapon Mesh") : TEXT("Character Mesh"));
        UE_LOG(LogWeaponComponent, Log, TEXT("  - Start socket: %s -> %s"), *StartSocket.ToString(), *BasePos.ToString());
        UE_LOG(LogWeaponComponent, Log, TEXT("  - End socket: %s -> %s"), *EndSocket.ToString(), *TipPos.ToString());
        UE_LOG(LogWeaponComponent, Log, TEXT("  - Trace radius: %.1f | Points: %d"), GetEffectiveTraceRadius(), GetEffectiveTracePointCount());

        // Warn if positions are identical (likely socket not found)
        if (BasePos.Equals(TipPos, 1.0f))
        {
            UE_LOG(LogWeaponComponent, Warning, TEXT("[%s] Start and End socket locations are identical! Check socket configuration."),
                *GetNameSafe(GetOwner()));
        }
    }
}

void UWeaponComponent::DisableHitDetection()
{
    // Diagnostic: log when Active phase ends with zero hits (potential miss detection issue)
    if (HitActors.Num() == 0 && bHitDetectionEnabled)
    {
        UE_LOG(LogWeaponComponent, Warning, TEXT("[TRACE DIAG] %s: Hit detection window closed with 0 hits. "
            "Check AnimNotify timing, socket positions, or trace radius."),
            *GetOwner()->GetName());
    }
    else if (bHitDetectionEnabled)
    {
        UE_LOG(LogWeaponComponent, Log, TEXT("[TRACE DIAG] %s: Hit detection window closed. Hits: %d"),
            *GetOwner()->GetName(), HitActors.Num());
    }

    bHitDetectionEnabled = false;
    SetComponentTickEnabled(false);

    // Clear hit actors when attack's hit window ends.
    // Previously HitActors persisted after DisableHitDetection, causing:
    // 1. Debug HUD showing stale "Hits: 1" during idle
    // 2. Ignored actor list carrying over across attacks on different targets
    HitActors.Empty();

    // Clear cached velocity so external consumers don't read stale data between attacks
    CachedWeaponTipVelocity = FVector::ZeroVector;
}

void UWeaponComponent::ResetHitActors()
{
    HitActors.Empty();
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

    UWorld* TraceWorld = GetWorld();
    if (!TraceWorld)
    {
        return;
    }

    // ========================================================================
    // COMPUTE CURRENT BLADE TRACE POINTS
    // ========================================================================
    const TArray<FVector> CurrentTracePoints = ComputeCurrentTracePoints();

    if (CurrentTracePoints.Num() == 0 || PreviousTracePoints.Num() != CurrentTracePoints.Num())
    {
        // Mismatch (e.g., WeaponData changed mid-attack) — reinitialize
        PreviousTracePoints = CurrentTracePoints;
        return;
    }

    // Cache tip velocity for external consumers (knockback, VFX alignment)
    if (CurrentTracePoints.Num() > 0 && PreviousTracePoints.Num() > 0)
    {
        CachedWeaponTipVelocity = UWeaponTraceLibrary::ComputeTracePointVelocity(
            PreviousTracePoints.Last(), CurrentTracePoints.Last(), LastDeltaTime);
    }

    // ========================================================================
    // COMPUTE ADAPTIVE SUBSTEP COUNT
    // ========================================================================
    const int32 NumSubsteps = ComputeAdaptiveSubstepCount(CurrentTracePoints);

    // Verbose diagnostics when weapon debug is enabled
    if (CombatDebug::IsWeaponDebugEnabled() && CombatDebug::IsVerboseLogEnabled())
    {
        const float TipVelocity = CachedWeaponTipVelocity.Size();
        UE_LOG(LogWeaponComponent, Verbose, TEXT("[TRACE DIAG] %s: Points=%d Substeps=%d TipVel=%.0f BladeLen=%.1f"),
            *GetOwner()->GetName(),
            CurrentTracePoints.Num(),
            NumSubsteps,
            TipVelocity,
            CurrentTracePoints.Num() >= 2 ? FVector::Dist(CurrentTracePoints[0], CurrentTracePoints.Last()) : 0.0f);
    }

    // ========================================================================
    // SETUP SHARED TRACE PARAMETERS
    // ========================================================================
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter);
    QueryParams.bTraceComplex = false;
    QueryParams.bReturnPhysicalMaterial = true;  // Enable surface type detection for material-dependent FX

    for (AActor* AlreadyHit : HitActors)
    {
        if (AlreadyHit)
        {
            QueryParams.AddIgnoredActor(AlreadyHit);
        }
    }

    const float EffectiveRadius = GetEffectiveTraceRadius();
    bool bAnyHit = false;
    FHitResult FirstHit;

    // ========================================================================
    // MULTI-POINT SUBSTEPPED SWEEP
    // ========================================================================
    // For each substep, sweep each trace point from its interpolated previous
    // position to its interpolated current position. This captures the full
    // arc of the swing — interior blade points trace wider arcs than the tip
    // or base alone.
    for (int32 Step = 0; Step < NumSubsteps; ++Step)
    {
        const float Alpha0 = static_cast<float>(Step) / static_cast<float>(NumSubsteps);
        const float Alpha1 = static_cast<float>(Step + 1) / static_cast<float>(NumSubsteps);

        for (int32 PointIdx = 0; PointIdx < CurrentTracePoints.Num(); ++PointIdx)
        {
            const FVector PrevPos = FMath::Lerp(PreviousTracePoints[PointIdx], CurrentTracePoints[PointIdx], Alpha0);
            const FVector CurrPos = FMath::Lerp(PreviousTracePoints[PointIdx], CurrentTracePoints[PointIdx], Alpha1);

            // Skip degenerate sweeps (point didn't move)
            if (PrevPos.Equals(CurrPos, 0.1f))
            {
                continue;
            }

            TArray<FHitResult> HitResults;
            const bool bHit = TraceWorld->SweepMultiByChannel(
                HitResults,
                PrevPos,
                CurrPos,
                FQuat::Identity,
                TraceChannel,
                FCollisionShape::MakeSphere(EffectiveRadius),
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
                    AActor* HitActor = Hit.GetActor();
                    if (HitActor && HitActor != OwnerCharacter)
                    {
                        ProcessHit(Hit);

                        // Update ignored actors for subsequent sweeps this frame
                        if (WasActorAlreadyHit(HitActor))
                        {
                            QueryParams.AddIgnoredActor(HitActor);
                        }
                    }
                }
            }
        }
    }

    // ========================================================================
    // DEBUG VISUALIZATION
    // ========================================================================
    // Pass base and tip positions for the existing debug drawing
    const FVector CurrentStart = CurrentTracePoints.Num() > 0 ? CurrentTracePoints[0] : FVector::ZeroVector;
    const FVector CurrentTip = CurrentTracePoints.Num() > 1 ? CurrentTracePoints.Last() : CurrentStart;
    const FVector PrevStart = PreviousTracePoints.Num() > 0 ? PreviousTracePoints[0] : FVector::ZeroVector;
    const FVector PrevTip = PreviousTracePoints.Num() > 1 ? PreviousTracePoints.Last() : PrevStart;

    UDebugUtils::DrawWeaponTrace(
        TraceWorld,
        CurrentStart, CurrentTip,
        PrevStart, PrevTip,
        EffectiveRadius,
        bAnyHit,
        bAnyHit ? FirstHit : FHitResult());

    if (CombatDebug::IsWeaponDebugEnabled())
    {
        UE_LOG(LogWeaponComponent, Verbose, TEXT("[%s] Trace: %d points x %d substeps = %d sweeps | TipVel: %.0f u/s | Hits: %d"),
            *GetNameSafe(GetOwner()),
            CurrentTracePoints.Num(),
            NumSubsteps,
            CurrentTracePoints.Num() * NumSubsteps,
            CachedWeaponTipVelocity.Size(),
            HitActors.Num());
    }

    // Store current points for next frame
    PreviousTracePoints = CurrentTracePoints;
}

void UWeaponComponent::ProcessHit(const FHitResult& Hit)
{
    AActor* HitActor = Hit.GetActor();

    if (!HitActor || WasActorAlreadyHit(HitActor) || ShouldIgnoreHitActor(HitActor))
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

    // Enforce max hit count per attack (0 = unlimited)
    UAttackData* AttackData = GetCurrentAttackData();
    if (AttackData && AttackData->MaxHitCount > 0 && HitActors.Num() >= AttackData->MaxHitCount)
    {
        return;
    }

    // Add to hit list (only living actors reach here)
    AddHitActor(HitActor);

    // Broadcast hit event
    OnWeaponHit.Broadcast(HitActor, Hit, AttackData);
}

bool UWeaponComponent::ShouldIgnoreHitActor(AActor* HitActor) const
{
    if (!HitActor || HitActor == OwnerCharacter)
    {
        return true;
    }

    AActor* OwnerActor = OwnerCharacter.Get();
    if (!OwnerActor)
    {
        OwnerActor = GetOwner();
    }

    if (!OwnerActor)
    {
        return false;
    }

    if (OwnerActor->Implements<UTeamMemberInterface>() && HitActor->Implements<UTeamMemberInterface>())
    {
        if (ITeamMemberInterface::Execute_IsFriendlyTo(OwnerActor, HitActor))
        {
            UE_LOG(LogWeaponComponent, Verbose, TEXT("[%s] Ignoring friendly hit target %s"),
                *GetNameSafe(OwnerActor),
                *GetNameSafe(HitActor));
            return true;
        }
    }

    return false;
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
// BLADE TRACE HELPERS
// ============================================================================

TArray<FVector> UWeaponComponent::ComputeCurrentTracePoints() const
{
    const FVector BladeBase = GetSocketLocation(GetEffectiveStartSocket());
    const FVector BladeTip = GetSocketLocation(GetEffectiveEndSocket());
    return UWeaponTraceLibrary::ComputeBladeTracePoints(BladeBase, BladeTip, GetEffectiveTracePointCount());
}

int32 UWeaponComponent::ComputeAdaptiveSubstepCount(const TArray<FVector>& CurrentPoints) const
{
    const float MaxVelocity = UWeaponTraceLibrary::ComputeMaxTracePointVelocity(
        PreviousTracePoints, CurrentPoints, LastDeltaTime);

    return UWeaponTraceLibrary::ComputeAdaptiveSubstepCount(
        MaxVelocity,
        GetEffectiveMinSubsteps(),
        GetEffectiveMaxSubsteps(),
        GetEffectiveSubstepVelocityThreshold());
}

int32 UWeaponComponent::GetEffectiveTracePointCount() const
{
    return WeaponData ? WeaponData->TracePointCount : DefaultTracePointCount;
}

int32 UWeaponComponent::GetEffectiveMinSubsteps() const
{
    return WeaponData ? WeaponData->MinSubsteps : DefaultMinSubsteps;
}

int32 UWeaponComponent::GetEffectiveMaxSubsteps() const
{
    return WeaponData ? WeaponData->MaxSubsteps : DefaultMaxSubsteps;
}

float UWeaponComponent::GetEffectiveSubstepVelocityThreshold() const
{
    return WeaponData ? WeaponData->SubstepVelocityThreshold : DefaultSubstepVelocityThreshold;
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
