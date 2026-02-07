// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/TargetingComponent.h"
#include "Debug/DebugConfig.h"
#include "Debug/DebugUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "MotionWarpingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Interfaces/DamageableInterface.h"
#include "Interfaces/TeamMemberInterface.h"
#include "Data/CombatSettings.h"
#include "Data/TargetingSettings.h"
#include "Data/MotionWarpingSettings.h"
#include "Characters/BaseCombatCharacter.h"
#include "Core/CombatComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogTargeting, Log, All);

UTargetingComponent::UTargetingComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    // Configuration now comes from TargetingSettings data asset
    // Debug visualization controlled via Combat.Debug.Targeting CVar
}

// ============================================================================
// SETTINGS ACCESS
// ============================================================================

UTargetingSettings* UTargetingComponent::GetEffectiveSettings() const
{
    // Priority 1: Per-instance override
    if (TargetingSettingsOverride)
    {
        return TargetingSettingsOverride;
    }

    // Priority 2: CombatSettings from owning character
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (const ABaseCombatCharacter* CombatChar = Cast<ABaseCombatCharacter>(Owner))
    {
        if (CombatChar->CombatSettings && CombatChar->CombatSettings->TargetingSettings)
        {
            return CombatChar->CombatSettings->TargetingSettings;
        }
    }

    // No settings available - methods will use hardcoded fallbacks
    return nullptr;
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

void UTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clean up tracking to avoid dangling delegate bindings
    StopWarpTracking();
    StopAttackerPairedWarpTracking();
    StopVictimWarpTracking();

    Super::EndPlay(EndPlayReason);
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

    const UTargetingSettings* Settings = GetEffectiveSettings();
    const bool bCheckLOS = Settings ? Settings->bRequireLineOfSight : true;
    if (bCheckLOS)
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

    // Use provided tolerance, or fall back to settings, or hardcoded default
    float ConeAngle = AngleTolerance;
    if (ConeAngle <= 0.0f)
    {
        const UTargetingSettings* Settings = GetEffectiveSettings();
        ConeAngle = Settings ? Settings->DirectionalConeAngle : 60.0f;
    }

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

    const UTargetingSettings* Settings = GetEffectiveSettings();
    const ECollisionChannel LOSChannel = Settings ? Settings->LineOfSightChannel.GetValue() : ECC_Visibility;

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        Start,
        End,
        LOSChannel,
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
// COUNTER LOCK
// ============================================================================

void UTargetingComponent::LockToCounterTarget(AActor* Target)
{
    if (!Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Targeting] LockToCounterTarget called with null target"));
        return;
    }

    CounterLockedTarget = Target;
    bIsCounterLocked = true;

    UE_LOG(LogTemp, Log, TEXT("[Targeting] %s: Counter locked to %s"),
        *GetOwner()->GetName(), *Target->GetName());
}

void UTargetingComponent::ReleaseCounterLock()
{
    if (bIsCounterLocked)
    {
        UE_LOG(LogTemp, Log, TEXT("[Targeting] %s: Counter lock released (was: %s)"),
            *GetOwner()->GetName(),
            CounterLockedTarget.IsValid() ? *CounterLockedTarget->GetName() : TEXT("invalid"));
    }

    CounterLockedTarget.Reset();
    bIsCounterLocked = false;
}

// ============================================================================
// MOTION WARPING INTEGRATION
// ============================================================================

bool UTargetingComponent::SetupAttackWarp(AActor* Target, const FRotator& TargetRotation, const FAttackWarpConfig& Config)
{
    // Lazy fetch owner for test compatibility
    ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!MotionWarpingComponent || !Owner)
    {
        return false;
    }

    if (!Config.bEnableWarp)
    {
        return false;
    }

    // Stop any previous tracking
    StopWarpTracking();

    // Clear any previous warp targets to prevent stale data
    MotionWarpingComponent->RemoveWarpTarget(Config.TargetWarpName);
    MotionWarpingComponent->RemoveWarpTarget(Config.RotationWarpName);

    const FVector OwnerLocation = Owner->GetActorLocation();

    // CASE 1: Target exists - set up continuous tracking for translation+rotation
    if (Target)
    {
        const FVector TargetLocation = Target->GetActorLocation();
        const float Distance = FVector::Dist(OwnerLocation, TargetLocation);

        // Skip if too close (within min warp distance) - use rotation-only toward target
        if (Distance < Config.MinWarpDistance)
        {
            const FRotator LookAtRotation = (TargetLocation - OwnerLocation).Rotation();
            MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
                Config.RotationWarpName,
                OwnerLocation,
                LookAtRotation
            );

            if (CombatDebug::IsTargetingDebugEnabled())
            {
                UE_LOG(LogTargeting, Log, TEXT("[ATTACK WARP] Target too close (%.1f < %.1f), using ROTATION-ONLY toward target"),
                    Distance, Config.MinWarpDistance);
            }
            return true;
        }

        // Store tracking state for continuous updates
        TrackedWarpTarget = Target;
        ActiveWarpConfig = Config;
        bIsTrackingWarpTarget = true;

        // Bind to OnPreUpdate for continuous tracking
        MotionWarpingComponent->OnPreUpdate.AddDynamic(this, &UTargetingComponent::OnMotionWarpingPreUpdate);

        // Set initial warp target (will be updated each frame)
        FVector WarpLocation = TargetLocation;
        if (Distance > Config.MaxWarpDistance)
        {
            const FVector ToTarget = (TargetLocation - OwnerLocation).GetSafeNormal();
            WarpLocation = OwnerLocation + (ToTarget * Config.MaxWarpDistance);
        }

        // SLOPE FIX: Adjust warp location Z to match terrain height
        const float CapsuleHalfHeight = Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const FVector OriginalWarpLocation = WarpLocation;
        WarpLocation = UDebugUtils::AdjustLocationToGround(
            GetWorld(),
            WarpLocation,
            CapsuleHalfHeight,
            Owner,
            CombatDebug::IsEnvironmentDebugEnabled());

        const FRotator LookAtRotation = (TargetLocation - OwnerLocation).Rotation();
        MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
            Config.TargetWarpName,
            WarpLocation,
            LookAtRotation
        );

        if (CombatDebug::IsTargetingDebugEnabled())
        {
            UE_LOG(LogTargeting, Log, TEXT("[ATTACK WARP] TARGET mode: Tracking %s (Distance: %.1f, Max: %.1f, Z Adj: %+.1f)"),
                *Target->GetName(), Distance, Config.MaxWarpDistance, WarpLocation.Z - OriginalWarpLocation.Z);
        }

        return true;
    }

    // CASE 2: No target - rotation-only warp toward TargetRotation (no continuous updates needed)
    // Uses RotationWarpName which should have bWarpTranslation=false in montage
    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
        Config.RotationWarpName,
        OwnerLocation,  // Same location - no translation
        TargetRotation
    );

    if (CombatDebug::IsTargetingDebugEnabled())
    {
        UE_LOG(LogTargeting, Log, TEXT("[ATTACK WARP] ROTATION-ONLY mode: Facing %.1f°"), TargetRotation.Yaw);

        // Draw debug visualization - direction arrow
        const FVector ForwardDir = TargetRotation.Vector() * 200.0f;
        DrawDebugDirectionalArrow(GetWorld(), OwnerLocation, OwnerLocation + ForwardDir,
            50.0f, FColor::Yellow, false, 1.0f, 0, 3.0f);
    }

    return true;
}

void UTargetingComponent::OnMotionWarpingPreUpdate(UMotionWarpingComponent* MotionWarpingComp)
{
    // Skip if not actively tracking
    if (!bIsTrackingWarpTarget)
    {
        return;
    }

    // Validate target still exists
    if (!TrackedWarpTarget.IsValid())
    {
        if (CombatDebug::IsTargetingDebugEnabled())
        {
            UE_LOG(LogTargeting, Warning, TEXT("[ATTACK WARP] Tracked target destroyed, stopping tracking"));
        }
        StopWarpTracking();
        return;
    }

    // Lazy fetch owner
    ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        StopWarpTracking();
        return;
    }

    AActor* Target = TrackedWarpTarget.Get();
    const FVector OwnerLocation = Owner->GetActorLocation();
    const FVector TargetLocation = Target->GetActorLocation();
    const float Distance = FVector::Dist(OwnerLocation, TargetLocation);

    // Optional: Re-validate target is still in valid range/angle
    // Could add checks here to stop tracking if target moves too far or behind player

    // Calculate warp location (clamped to max distance)
    FVector WarpLocation = TargetLocation;
    if (Distance > ActiveWarpConfig.MaxWarpDistance)
    {
        const FVector ToTarget = (TargetLocation - OwnerLocation).GetSafeNormal();
        WarpLocation = OwnerLocation + (ToTarget * ActiveWarpConfig.MaxWarpDistance);
    }

    // SLOPE FIX: Adjust warp location Z to match terrain height
    const float CapsuleHalfHeight = Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    WarpLocation = UDebugUtils::AdjustLocationToGround(
        GetWorld(),
        WarpLocation,
        CapsuleHalfHeight,
        Owner,
        false); // Don't spam debug every frame during tracking

    // Calculate look-at rotation (face toward target)
    const FRotator LookAtRotation = (TargetLocation - OwnerLocation).Rotation();

    // Update warp target with current positions
    MotionWarpingComp->AddOrUpdateWarpTargetFromLocationAndRotation(
        ActiveWarpConfig.TargetWarpName,
        WarpLocation,
        LookAtRotation
    );

    // Debug visualization
    if (CombatDebug::IsTargetingDebugEnabled())
    {
        DrawDebugLine(GetWorld(), OwnerLocation, WarpLocation, FColor::Green, false, 0.0f, 0, 2.0f);
        DrawDebugSphere(GetWorld(), WarpLocation, 25.0f, 8, FColor::Green, false, 0.0f);
        DrawDebugDirectionalArrow(GetWorld(), OwnerLocation,
            OwnerLocation + (LookAtRotation.Vector() * 150.0f), 30.0f, FColor::Cyan, false, 0.0f, 0, 2.0f);
    }
}

void UTargetingComponent::StopWarpTracking()
{
    if (bIsTrackingWarpTarget && MotionWarpingComponent)
    {
        MotionWarpingComponent->OnPreUpdate.RemoveDynamic(this, &UTargetingComponent::OnMotionWarpingPreUpdate);
    }

    TrackedWarpTarget.Reset();
    bIsTrackingWarpTarget = false;
    ActiveWarpConfig = FAttackWarpConfig();
}

void UTargetingComponent::ClearMotionWarp(FName WarpTargetName)
{
    // Stop continuous tracking (all modes)
    StopWarpTracking();
    StopAttackerPairedWarpTracking();
    StopVictimWarpTracking();

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
// VICTIM WARP (PAIRED ANIMATION VICTIM MODE)
// ============================================================================

bool UTargetingComponent::SetupVictimWarp(AActor* Attacker, const FPairedWarpConfig& Config)
{
    ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());

    // Lazy init for test compatibility: if BeginPlay hasn't run yet, find MotionWarpingComponent now
    if (!MotionWarpingComponent && Owner)
    {
        MotionWarpingComponent = Owner->FindComponentByClass<UMotionWarpingComponent>();
    }

    // Gap 19.1 fix: Log warnings for specific failure conditions instead of silent failure
    if (!MotionWarpingComponent)
    {
        UE_LOG(LogTargeting, Warning, TEXT("[VICTIM WARP] %s has no MotionWarpingComponent - warp tracking disabled. Add MotionWarpingComponent to character."),
            Owner ? *Owner->GetName() : TEXT("Unknown"));
        return false;
    }
    if (!Owner)
    {
        UE_LOG(LogTargeting, Warning, TEXT("[VICTIM WARP] Owner character is null - cannot setup victim warp"));
        return false;
    }
    if (!Attacker)
    {
        UE_LOG(LogTargeting, Warning, TEXT("[VICTIM WARP] %s - Attacker is null, cannot setup victim warp"),
            *Owner->GetName());
        return false;
    }

    // Stop any previous tracking (both modes)
    StopWarpTracking();
    StopVictimWarpTracking();

    // Clear previous warp target
    MotionWarpingComponent->RemoveWarpTarget(Config.WarpTargetName);

    // Store tracking state for continuous updates
    TrackedAttacker = Attacker;
    VictimWarpConfig = Config;
    bIsTrackingAsVictim = true;

    // Bind to OnPreUpdate for continuous tracking
    MotionWarpingComponent->OnPreUpdate.AddDynamic(this, &UTargetingComponent::OnVictimMotionWarpingPreUpdate);

    // Register attacker as paired partner for collision ignore
    if (UCombatComponent* CombatComp = Owner->FindComponentByClass<UCombatComponent>())
    {
        CombatComp->AddPairedPartner(Attacker);
    }

    // Calculate initial victim position (will be updated each frame)
    const FVector AttackerLocation = Attacker->GetActorLocation();
    const FRotator AttackerRotation = Attacker->GetActorRotation();

    // Victim starts at offset from attacker's position
    // RelativeOffset is in attacker-local space (X = forward, Y = right)
    // Uses configurable offset instead of hardcoded value (Gap 18.2 fix)
    FVector WarpLocation = AttackerLocation + AttackerRotation.RotateVector(Config.RelativeOffset);

    // Terrain adjustment to prevent floating
    if (Config.bAdjustToTerrain)
    {
        const float CapsuleHalfHeight = Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        WarpLocation = UDebugUtils::AdjustLocationToGround(GetWorld(), WarpLocation, CapsuleHalfHeight, Owner, false);
    }

    // Calculate rotation (face the attacker)
    FRotator WarpRotation = FRotator::ZeroRotator;
    if (Config.bWarpRotation)
    {
        WarpRotation = (AttackerLocation - WarpLocation).Rotation();
    }

    // Set initial warp target
    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
        Config.WarpTargetName,
        Config.bWarpTranslation ? WarpLocation : Owner->GetActorLocation(),
        WarpRotation
    );

    if (CombatDebug::IsTargetingDebugEnabled())
    {
        UE_LOG(LogTargeting, Log, TEXT("[VICTIM WARP] %s tracking attacker %s, WarpTarget=%s"),
            *Owner->GetName(), *Attacker->GetName(), *Config.WarpTargetName.ToString());
    }

    return true;
}

void UTargetingComponent::ClearVictimWarp()
{
    StopVictimWarpTracking();
}

void UTargetingComponent::OnVictimMotionWarpingPreUpdate(UMotionWarpingComponent* MotionWarpingComp)
{
    // Skip if not actively tracking as victim
    if (!bIsTrackingAsVictim)
    {
        return;
    }

    // Gap 19.3 fix: Bidirectional validity check - verify world is valid (not tearing down)
    UWorld* World = GetWorld();
    if (!World || World->bIsTearingDown)
    {
        StopVictimWarpTracking();
        return;
    }

    // Validate attacker still exists
    if (!TrackedAttacker.IsValid())
    {
        if (CombatDebug::IsTargetingDebugEnabled())
        {
            UE_LOG(LogTargeting, Warning, TEXT("[VICTIM WARP] Tracked attacker destroyed, stopping tracking"));
        }
        StopVictimWarpTracking();
        return;
    }

    ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        StopVictimWarpTracking();
        return;
    }

    AActor* Attacker = TrackedAttacker.Get();
    const FVector AttackerLocation = Attacker->GetActorLocation();
    const FRotator AttackerRotation = Attacker->GetActorRotation();

    // Calculate victim's position relative to attacker's CURRENT location
    // This is the key difference from initial setup - tracks attacker's movement
    // Uses stored config's RelativeOffset instead of hardcoded value (Gap 18.2 fix)
    FVector WarpLocation = AttackerLocation + AttackerRotation.RotateVector(VictimWarpConfig.RelativeOffset);

    // Terrain adjustment
    if (VictimWarpConfig.bAdjustToTerrain)
    {
        const float CapsuleHalfHeight = Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        WarpLocation = UDebugUtils::AdjustLocationToGround(GetWorld(), WarpLocation, CapsuleHalfHeight, Owner, false);
    }

    // Rotation (face the attacker)
    FRotator WarpRotation = Owner->GetActorRotation();
    if (VictimWarpConfig.bWarpRotation)
    {
        WarpRotation = (AttackerLocation - WarpLocation).Rotation();
    }

    // Update warp target with attacker's current position
    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
        VictimWarpConfig.WarpTargetName,
        VictimWarpConfig.bWarpTranslation ? WarpLocation : Owner->GetActorLocation(),
        WarpRotation
    );
}

void UTargetingComponent::StopVictimWarpTracking()
{
    ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());

    if (bIsTrackingAsVictim && MotionWarpingComponent)
    {
        MotionWarpingComponent->OnPreUpdate.RemoveDynamic(this, &UTargetingComponent::OnVictimMotionWarpingPreUpdate);

        // Remove paired partner registration
        if (Owner)
        {
            if (UCombatComponent* CombatComp = Owner->FindComponentByClass<UCombatComponent>())
            {
                if (TrackedAttacker.IsValid())
                {
                    CombatComp->RemovePairedPartner(TrackedAttacker.Get());
                }
            }
        }
    }

    TrackedAttacker.Reset();
    bIsTrackingAsVictim = false;
    VictimWarpConfig = FPairedWarpConfig();
}

// ============================================================================
// ATTACKER PAIRED WARP (PAIRED ANIMATION ATTACKER MODE)
// ============================================================================

bool UTargetingComponent::SetupAttackerPairedWarp(AActor* Victim, const FPairedWarpConfig& Config)
{
    ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());

    // Lazy init for test compatibility: if BeginPlay hasn't run yet, find MotionWarpingComponent now
    if (!MotionWarpingComponent && Owner)
    {
        MotionWarpingComponent = Owner->FindComponentByClass<UMotionWarpingComponent>();
    }

    // Gap 19.1 fix: Log warnings for specific failure conditions instead of silent failure
    if (!MotionWarpingComponent)
    {
        UE_LOG(LogTargeting, Warning, TEXT("[ATTACKER WARP] %s has no MotionWarpingComponent - warp tracking disabled. Add MotionWarpingComponent to character."),
            Owner ? *Owner->GetName() : TEXT("Unknown"));
        return false;
    }
    if (!Owner)
    {
        UE_LOG(LogTargeting, Warning, TEXT("[ATTACKER WARP] Owner character is null - cannot setup attacker warp"));
        return false;
    }
    if (!Victim)
    {
        UE_LOG(LogTargeting, Warning, TEXT("[ATTACKER WARP] %s - Victim is null, cannot setup attacker warp"),
            *Owner->GetName());
        return false;
    }

    // Stop any previous tracking (all modes)
    StopWarpTracking();
    StopAttackerPairedWarpTracking();
    StopVictimWarpTracking();

    // Clear previous warp target
    MotionWarpingComponent->RemoveWarpTarget(Config.WarpTargetName);

    // Store tracking state for continuous updates
    TrackedVictim = Victim;
    AttackerPairedWarpConfig = Config;
    bIsTrackingAsAttacker = true;

    // Bind to OnPreUpdate for continuous tracking
    MotionWarpingComponent->OnPreUpdate.AddDynamic(this, &UTargetingComponent::OnAttackerPairedWarpPreUpdate);

    // Register victim as paired partner for collision ignore
    if (UCombatComponent* CombatComp = Owner->FindComponentByClass<UCombatComponent>())
    {
        CombatComp->AddPairedPartner(Victim);
    }

    // Calculate initial warp position (toward victim with offset, respecting max distance)
    const FVector OwnerLocation = Owner->GetActorLocation();
    const FVector VictimLocation = Victim->GetActorLocation();
    const FRotator VictimRotation = Victim->GetActorRotation();

    // Attacker warps to offset from victim (Gap 18.3 fix)
    // RelativeOffset is in victim's local space
    // Default (0,0,0) = warp directly to victim location
    // Typical use: small negative X to stay in front of victim
    FVector TargetLocation = VictimLocation + VictimRotation.RotateVector(Config.RelativeOffset);
    const float Distance = FVector::Dist(OwnerLocation, TargetLocation);

    // Clamp to max warp distance
    FVector WarpLocation = TargetLocation;
    if (Config.MaxWarpDistance > 0.0f && Distance > Config.MaxWarpDistance)
    {
        const FVector ToTarget = (TargetLocation - OwnerLocation).GetSafeNormal();
        WarpLocation = OwnerLocation + (ToTarget * Config.MaxWarpDistance);
    }

    // Terrain adjustment to prevent floating
    if (Config.bAdjustToTerrain)
    {
        const float CapsuleHalfHeight = Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        WarpLocation = UDebugUtils::AdjustLocationToGround(GetWorld(), WarpLocation, CapsuleHalfHeight, Owner, false);
    }

    // Calculate rotation (face the victim)
    FRotator WarpRotation = Owner->GetActorRotation();
    if (Config.bWarpRotation)
    {
        WarpRotation = (VictimLocation - OwnerLocation).Rotation();
    }

    // Set initial warp target
    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
        Config.WarpTargetName,
        Config.bWarpTranslation ? WarpLocation : OwnerLocation,
        WarpRotation
    );

    if (CombatDebug::IsTargetingDebugEnabled())
    {
        UE_LOG(LogTargeting, Log, TEXT("[ATTACKER PAIRED WARP] %s tracking victim %s, WarpTarget=%s, Distance=%.1f"),
            *Owner->GetName(), *Victim->GetName(), *Config.WarpTargetName.ToString(), Distance);
    }

    return true;
}

void UTargetingComponent::ClearAttackerPairedWarp()
{
    StopAttackerPairedWarpTracking();
}

void UTargetingComponent::OnAttackerPairedWarpPreUpdate(UMotionWarpingComponent* MotionWarpingComp)
{
    // Skip if not actively tracking as attacker
    if (!bIsTrackingAsAttacker)
    {
        return;
    }

    // Gap 19.3 fix: Bidirectional validity check - verify world is valid (not tearing down)
    UWorld* World = GetWorld();
    if (!World || World->bIsTearingDown)
    {
        StopAttackerPairedWarpTracking();
        return;
    }

    // Validate victim still exists
    if (!TrackedVictim.IsValid())
    {
        if (CombatDebug::IsTargetingDebugEnabled())
        {
            UE_LOG(LogTargeting, Warning, TEXT("[ATTACKER PAIRED WARP] Tracked victim destroyed, stopping tracking"));
        }
        StopAttackerPairedWarpTracking();
        return;
    }

    ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        StopAttackerPairedWarpTracking();
        return;
    }

    AActor* Victim = TrackedVictim.Get();
    const FVector OwnerLocation = Owner->GetActorLocation();
    const FVector VictimLocation = Victim->GetActorLocation();
    const FRotator VictimRotation = Victim->GetActorRotation();

    // Calculate warp location with offset from victim (Gap 18.3 fix)
    // Uses stored config's RelativeOffset instead of warping directly to victim
    FVector TargetLocation = VictimLocation + VictimRotation.RotateVector(AttackerPairedWarpConfig.RelativeOffset);
    const float Distance = FVector::Dist(OwnerLocation, TargetLocation);

    // Clamp to max distance
    FVector WarpLocation = TargetLocation;
    if (AttackerPairedWarpConfig.MaxWarpDistance > 0.0f && Distance > AttackerPairedWarpConfig.MaxWarpDistance)
    {
        const FVector ToTarget = (TargetLocation - OwnerLocation).GetSafeNormal();
        WarpLocation = OwnerLocation + (ToTarget * AttackerPairedWarpConfig.MaxWarpDistance);
    }

    // Terrain adjustment
    if (AttackerPairedWarpConfig.bAdjustToTerrain)
    {
        const float CapsuleHalfHeight = Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        WarpLocation = UDebugUtils::AdjustLocationToGround(GetWorld(), WarpLocation, CapsuleHalfHeight, Owner, false);
    }

    // Rotation (face the victim)
    FRotator WarpRotation = Owner->GetActorRotation();
    if (AttackerPairedWarpConfig.bWarpRotation)
    {
        WarpRotation = (VictimLocation - OwnerLocation).Rotation();
    }

    // Update warp target with victim's current position
    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
        AttackerPairedWarpConfig.WarpTargetName,
        AttackerPairedWarpConfig.bWarpTranslation ? WarpLocation : OwnerLocation,
        WarpRotation
    );

    // Debug visualization
    if (CombatDebug::IsTargetingDebugEnabled())
    {
        DrawDebugLine(GetWorld(), OwnerLocation, WarpLocation, FColor::Magenta, false, 0.0f, 0, 2.0f);
        DrawDebugSphere(GetWorld(), WarpLocation, 25.0f, 8, FColor::Magenta, false, 0.0f);
        DrawDebugDirectionalArrow(GetWorld(), OwnerLocation,
            OwnerLocation + (WarpRotation.Vector() * 150.0f), 30.0f, FColor::Purple, false, 0.0f, 0, 2.0f);
    }
}

void UTargetingComponent::StopAttackerPairedWarpTracking()
{
    ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());

    if (bIsTrackingAsAttacker && MotionWarpingComponent)
    {
        MotionWarpingComponent->OnPreUpdate.RemoveDynamic(this, &UTargetingComponent::OnAttackerPairedWarpPreUpdate);

        // Remove paired partner registration
        if (Owner)
        {
            if (UCombatComponent* CombatComp = Owner->FindComponentByClass<UCombatComponent>())
            {
                if (TrackedVictim.IsValid())
                {
                    CombatComp->RemovePairedPartner(TrackedVictim.Get());
                }
            }
        }
    }

    TrackedVictim.Reset();
    bIsTrackingAsAttacker = false;
    AttackerPairedWarpConfig = FPairedWarpConfig();
}

// Legacy function - forwards to SetupAttackWarp
bool UTargetingComponent::SetupMotionWarp(AActor* Target, FName WarpTargetName, float MaxDistance)
{
    if (!Target)
    {
        return false;
    }

    // Create a config with the provided values
    FAttackWarpConfig Config;
    Config.TargetWarpName = WarpTargetName;
    Config.MaxWarpDistance = (MaxDistance > 0.0f) ? MaxDistance : Config.MaxWarpDistance;

    // Get rotation toward target
    ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        return false;
    }

    const FRotator TargetRotation = (Target->GetActorLocation() - Owner->GetActorLocation()).Rotation();
    return SetupAttackWarp(Target, TargetRotation, Config);
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

    // Get effective targeting settings
    const UTargetingSettings* TargetSettings = GetEffectiveSettings();

    // Use provided values or fall back to TargetingSettings defaults
    const float UseMaxRange = (MaxRange > 0.0f) ? MaxRange : (TargetSettings ? TargetSettings->SoftAimRange : 500.0f);
    const float UseGradientAngle = (GradientAngle > 0.0f) ? GradientAngle : (TargetSettings ? TargetSettings->SoftAimCandidateAngle : 45.0f);
    const float UseOppositeAngle = (OppositeAngle > 0.0f) ? OppositeAngle : (TargetSettings ? TargetSettings->OppositeAngleThreshold : 120.0f);
    const float UseAngleWeight = (AngleWeight >= 0.0f) ? AngleWeight : (TargetSettings ? TargetSettings->AngleWeight : 0.7f);
    const float UseDistanceWeight = (DistanceWeight >= 0.0f) ? DistanceWeight : (TargetSettings ? TargetSettings->DistanceWeight : 0.3f);

    const FVector OwnerLocation = Owner->GetActorLocation();
    const FVector NormalizedInput = InputDirection.GetSafeNormal();

    // Get all potential targets in range
    TArray<AActor*> PotentialTargets;
    GetActorsInRange(PotentialTargets);
    FilterByTargetableClass(PotentialTargets);

    const bool bCheckLOS = TargetSettings ? TargetSettings->bRequireLineOfSight : true;
    if (bCheckLOS)
    {
        FilterByLineOfSight(PotentialTargets);
    }

    // Score each target with detailed debug tracking
    float BestScore = -1.0f;
    AActor* BestTarget = nullptr;

    // Debug: Track rejection reasons
    const bool bDebugEnabled = CombatDebug::IsTargetingDebugEnabled();
    TMap<AActor*, FString> RejectionReasons;

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
            if (bDebugEnabled)
            {
                RejectionReasons.Add(Target, FString::Printf(TEXT("OUT OF RANGE (%.1f > %.1f)"), Distance, UseMaxRange));
            }
            continue;
        }

        const FVector ToTargetNorm = ToTarget.GetSafeNormal();
        const float DotProduct = FVector::DotProduct(NormalizedInput, ToTargetNorm);
        const float AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f)));

        // Skip if target is in "opposite" direction
        if (AngleToTarget > UseOppositeAngle)
        {
            if (bDebugEnabled)
            {
                RejectionReasons.Add(Target, FString::Printf(TEXT("OPPOSITE ANGLE (%.1f > %.1f)"), AngleToTarget, UseOppositeAngle));
            }
            continue;
        }

        // Calculate scores
        // Angle score: 1.0 = perfect alignment, 0.0 = at gradient threshold
        const float AngleScore = FMath::Clamp(1.0f - (AngleToTarget / UseGradientAngle), 0.0f, 1.0f);

        // Distance score: 1.0 = at owner location, 0.0 = at max range
        const float DistanceScore = FMath::Clamp(1.0f - (Distance / UseMaxRange), 0.0f, 1.0f);

        // Combined weighted score
        const float TotalScore = (AngleScore * UseAngleWeight) + (DistanceScore * UseDistanceWeight);

        if (bDebugEnabled)
        {
            UE_LOG(LogTargeting, Verbose, TEXT("[SOFT AIM] %s: Angle=%.1f° (Score=%.2f), Dist=%.1f (Score=%.2f), Total=%.3f"),
                *Target->GetName(), AngleToTarget, AngleScore, Distance, DistanceScore, TotalScore);
        }

        if (TotalScore > BestScore)
        {
            BestScore = TotalScore;
            BestTarget = Target;
        }
    }

    // Enhanced debug visualization (CVar-controlled)
    if (bDebugEnabled)
    {
        const float DrawDuration = CombatDebug::GetDebugDrawDuration();

        // Log summary
        UE_LOG(LogTargeting, Log, TEXT("[SOFT AIM] ═══════════════════════════════════════"));
        UE_LOG(LogTargeting, Log, TEXT("[SOFT AIM] Candidates: %d, MaxRange: %.1f, GradientAngle: %.1f°, OppositeAngle: %.1f°"),
            PotentialTargets.Num(), UseMaxRange, UseGradientAngle, UseOppositeAngle);
        UE_LOG(LogTargeting, Log, TEXT("[SOFT AIM] Weights: Angle=%.2f, Distance=%.2f"), UseAngleWeight, UseDistanceWeight);

        // Draw input direction
        const FVector InputEnd = OwnerLocation + (NormalizedInput * 300.0f);
        DrawDebugDirectionalArrow(GetWorld(), OwnerLocation, InputEnd, 40.0f, FColor::Cyan, false, DrawDuration, 0, 3.0f);
        DrawDebugString(GetWorld(), InputEnd + FVector(0, 0, 30), TEXT("INPUT DIR"), nullptr, FColor::Cyan, DrawDuration, true);

        // Draw soft aim range circle
        DrawDebugCircle(GetWorld(), OwnerLocation, UseMaxRange, 32, FColor::Yellow, false, DrawDuration, 0, 2.0f, FVector(1, 0, 0), FVector(0, 1, 0), false);

        // Draw gradient angle cone (candidates within this get higher score)
        const float GradientConeLength = UseMaxRange * 0.7f;
        const FVector GradientRight = FRotationMatrix(NormalizedInput.Rotation()).GetScaledAxis(EAxis::Y);
        const FVector GradientLeftEnd = OwnerLocation + (FRotator(0, -UseGradientAngle, 0).RotateVector(NormalizedInput) * GradientConeLength);
        const FVector GradientRightEnd = OwnerLocation + (FRotator(0, UseGradientAngle, 0).RotateVector(NormalizedInput) * GradientConeLength);
        DrawDebugLine(GetWorld(), OwnerLocation, GradientLeftEnd, FColor::Green, false, DrawDuration, 0, 2.0f);
        DrawDebugLine(GetWorld(), OwnerLocation, GradientRightEnd, FColor::Green, false, DrawDuration, 0, 2.0f);

        // Draw opposite angle cone (beyond this = rejected)
        const FVector OppositeLeftEnd = OwnerLocation + (FRotator(0, -UseOppositeAngle, 0).RotateVector(NormalizedInput) * UseMaxRange * 0.5f);
        const FVector OppositeRightEnd = OwnerLocation + (FRotator(0, UseOppositeAngle, 0).RotateVector(NormalizedInput) * UseMaxRange * 0.5f);
        DrawDebugLine(GetWorld(), OwnerLocation, OppositeLeftEnd, FColor::Red, false, DrawDuration, 0, 1.5f);
        DrawDebugLine(GetWorld(), OwnerLocation, OppositeRightEnd, FColor::Red, false, DrawDuration, 0, 1.5f);

        // Draw rejected targets
        for (const auto& Pair : RejectionReasons)
        {
            if (Pair.Key)
            {
                const FVector TargetLoc = Pair.Key->GetActorLocation();
                DrawDebugSphere(GetWorld(), TargetLoc, 40.0f, 8, FColor::Red, false, DrawDuration);
                DrawDebugLine(GetWorld(), OwnerLocation, TargetLoc, FColor::Red, false, DrawDuration, 0, 1.0f);
                DrawDebugString(GetWorld(), TargetLoc + FVector(0, 0, 80), Pair.Value, nullptr, FColor::Red, DrawDuration, true);
                UE_LOG(LogTargeting, Log, TEXT("[SOFT AIM] REJECTED %s: %s"), *Pair.Key->GetName(), *Pair.Value);
            }
        }

        // Draw accepted targets
        for (AActor* Target : PotentialTargets)
        {
            if (!Target || RejectionReasons.Contains(Target))
            {
                continue;
            }

            const FVector TargetLoc = Target->GetActorLocation();
            const FColor Color = (Target == BestTarget) ? FColor::Green : FColor::Orange;
            DrawDebugSphere(GetWorld(), TargetLoc, (Target == BestTarget) ? 60.0f : 40.0f, 12, Color, false, DrawDuration);
            DrawDebugLine(GetWorld(), OwnerLocation, TargetLoc, Color, false, DrawDuration, 0, (Target == BestTarget) ? 4.0f : 2.0f);

            if (Target == BestTarget)
            {
                DrawDebugString(GetWorld(), TargetLoc + FVector(0, 0, 100),
                    FString::Printf(TEXT("BEST (Score: %.3f)"), BestScore), nullptr, FColor::Green, DrawDuration, true);
                UE_LOG(LogTargeting, Log, TEXT("[SOFT AIM] SELECTED: %s (Score: %.3f)"), *Target->GetName(), BestScore);
            }
        }

        if (!BestTarget)
        {
            UE_LOG(LogTargeting, Log, TEXT("[SOFT AIM] NO TARGET SELECTED"));
        }
        UE_LOG(LogTargeting, Log, TEXT("[SOFT AIM] ═══════════════════════════════════════"));
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

AActor* UTargetingComponent::FindNearestTarget(float MaxRange, float FacingConeAngle)
{
    // Lazy fetch owner for test compatibility
    ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        return nullptr;
    }

    // Get effective targeting settings
    const UTargetingSettings* TargetSettings = GetEffectiveSettings();

    // Use provided range or fall back to settings default
    const float UseMaxRange = (MaxRange > 0.0f) ? MaxRange : (TargetSettings ? TargetSettings->SoftAimRange : 500.0f);
    const FVector OwnerLocation = Owner->GetActorLocation();
    const FVector OwnerForward = Owner->GetActorForwardVector();

    // Get all potential targets in range
    TArray<AActor*> PotentialTargets;
    GetActorsInRange(PotentialTargets);
    FilterByTargetableClass(PotentialTargets);

    const bool bCheckLOS = TargetSettings ? TargetSettings->bRequireLineOfSight : true;
    if (bCheckLOS)
    {
        FilterByLineOfSight(PotentialTargets);
    }

    // Find the nearest target within facing cone
    float NearestDistance = UseMaxRange;
    AActor* NearestTarget = nullptr;

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

        // Check facing cone (if not 180° which means any direction)
        if (FacingConeAngle < 180.0f)
        {
            const FVector ToTargetNorm = ToTarget.GetSafeNormal();
            const float DotProduct = FVector::DotProduct(OwnerForward, ToTargetNorm);
            const float AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f)));

            // Skip if outside facing cone
            if (AngleToTarget > FacingConeAngle)
            {
                continue;
            }
        }

        if (Distance < NearestDistance)
        {
            NearestDistance = Distance;
            NearestTarget = Target;
        }
    }

    // Debug visualization
    if (CombatDebug::IsTargetingDebugEnabled())
    {
        // Draw facing cone
        if (FacingConeAngle < 180.0f)
        {
            const float ConeLength = 200.0f;
            const FVector ConeEnd = OwnerLocation + OwnerForward * ConeLength;
            DrawDebugLine(GetWorld(), OwnerLocation, ConeEnd, FColor::Yellow, false, 0.5f, 0, 1.0f);
        }

        if (NearestTarget)
        {
            DrawDebugLine(GetWorld(), OwnerLocation, NearestTarget->GetActorLocation(),
                FColor::Cyan, false, 0.5f, 0, 2.0f);
            DrawDebugString(GetWorld(), NearestTarget->GetActorLocation() + FVector(0, 0, 100),
                TEXT("NEAREST"), nullptr, FColor::Cyan, 0.5f, true);
        }
    }

    return NearestTarget;
}

// Legacy function - forwards to SetupAttackWarp with rotation-only
bool UTargetingComponent::SetupDirectionalWarp(const FVector& InputDirection, const FAttackWarpConfig& Config)
{
    if (InputDirection.IsNearlyZero())
    {
        return false;
    }

    // Calculate rotation toward input direction and call unified function with no target
    const FRotator TargetRotation = InputDirection.GetSafeNormal().Rotation();
    return SetupAttackWarp(nullptr, TargetRotation, Config);
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
    const UTargetingSettings* Settings = GetEffectiveSettings();
    const float SearchRadius = Settings ? Settings->MaxTargetDistance : 1000.0f;

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Owner);

    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        OwnerLocation,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(SearchRadius),
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
    const UTargetingSettings* Settings = GetEffectiveSettings();
    const bool bCheckLOS = Settings ? Settings->bRequireLineOfSight : true;
    if (bCheckLOS)
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

    // Get settings for debug visualization
    const UTargetingSettings* Settings = GetEffectiveSettings();
    const float DebugMaxDistance = Settings ? Settings->MaxTargetDistance : 1000.0f;
    const float DebugConeAngle = Settings ? Settings->DirectionalConeAngle : 60.0f;

    // Draw search cone
    DrawDebugCone(
        GetWorld(),
        OwnerLocation,
        SearchDirection,
        DebugMaxDistance,
        FMath::DegreesToRadians(DebugConeAngle),
        FMath::DegreesToRadians(DebugConeAngle),
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