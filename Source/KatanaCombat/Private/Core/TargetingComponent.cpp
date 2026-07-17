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
#include "HAL/PlatformTime.h"
#include "Interfaces/DamageableInterface.h"
#include "Interfaces/TeamMemberInterface.h"
#include "Data/CombatSettings.h"
#include "Data/DefenseConfiguration.h"
#include "Data/TargetingSettings.h"
#include "Data/MotionWarpingSettings.h"
#include "Characters/BaseCombatCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "RootMotionModifier.h"

DEFINE_LOG_CATEGORY_STATIC(LogTargeting, Log, All);

namespace
{
struct FAlignmentTelemetryContext
{
	UCombatComponent* Sink = nullptr;
	const FDefenseSequenceContext* Sequence = nullptr;
};

const FDefenseSequenceContext* FindDefenseSequenceForParticipants(
	AActor* Owner,
	AActor* Target)
{
	for (AActor* Candidate : {Owner, Target})
	{
		const UPairedAnimationComponent* Paired = Candidate
			? Candidate->FindComponentByClass<UPairedAnimationComponent>()
			: nullptr;
		if (!Paired)
		{
			continue;
		}
		const FDefenseSequenceContext& Sequence = Paired->GetActiveDefenseSequenceContext();
		if (Sequence.OriginatingInteraction.IsValid()
			&& (Sequence.Defender.Get() == Owner || Sequence.SourceAttacker.Get() == Owner)
			&& (Sequence.Defender.Get() == Target || Sequence.SourceAttacker.Get() == Target))
		{
			return &Sequence;
		}
	}
	return nullptr;
}

FAlignmentTelemetryContext ResolveAlignmentTelemetryContext(
	ACharacter* Owner,
	const FAlignmentRequestSpec& Spec)
{
	FAlignmentTelemetryContext Context;
	Context.Sequence = FindDefenseSequenceForParticipants(Owner, Spec.Target.Get());
	AActor* SinkActor = Context.Sequence && Context.Sequence->Defender.IsValid()
		? Context.Sequence->Defender.Get()
		: Owner;
	Context.Sink = SinkActor ? SinkActor->FindComponentByClass<UCombatComponent>() : nullptr;
	return Context;
}

FDefenseTelemetryRecord MakeAlignmentTelemetry(
	ACharacter* Owner,
	const FAlignmentRequestSpec& Spec,
	const EDefenseTelemetryEvent Event,
	const FAlignmentTelemetryContext& Context)
{
	FDefenseTelemetryRecord Record = Context.Sequence
		? DefenseTelemetry::FromResolution(Context.Sequence->OriginatingResolution, Event)
		: FDefenseTelemetryRecord();
	Record.Event = Event;
	Record.StageGeneration = Context.Sequence
		? Context.Sequence->StageGeneration
		: Spec.OwnerGeneration;
	Record.StageName = Spec.OwnerId;
	Record.AlignmentOwner = Spec.OwnerId;
	Record.AlignmentExecutor = Spec.Executor;
	Record.Candidate = Spec.Target;
	Record.MaximumTurnRate = Spec.MaximumTurnRate;
	Record.RemainingTurnBudget = Spec.RemainingTurnBudget;
	Record.OwnerTransform = Owner ? Owner->GetActorTransform() : FTransform::Identity;
	Record.CounterpartTransform = Spec.Target.IsValid()
		? Spec.Target->GetActorTransform()
		: FTransform::Identity;
	if (Context.Sequence && Context.Sequence->ResponseDeadlineUnscaled > 0.0)
	{
		Record.TimeToDeadline = static_cast<float>(FMath::Max(
			0.0,
			Context.Sequence->ResponseDeadlineUnscaled - FPlatformTime::Seconds()));
	}
	return Record;
}

bool GetPelvisLocation(const ACharacter* Character, FVector& OutLocation)
{
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	static const FName PelvisBone(TEXT("pelvis"));
	if (!Mesh || !Mesh->DoesSocketExist(PelvisBone))
	{
		OutLocation = FVector::ZeroVector;
		return false;
	}
	OutLocation = Mesh->GetSocketLocation(PelvisBone);
	return true;
}
}

UTargetingComponent::UTargetingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    // Configuration now comes from TargetingSettings data asset
    // Debug visualization controlled via Combat.Debug.Targeting CVar
}

void UTargetingComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    FAlignmentRequestRecord* ActiveRecord = AlignmentRequests.Find(ActiveAlignmentRequest);
    if (!ActiveRecord || ActiveRecord->Spec.Executor != EAlignmentExecutor::CharacterMovement)
    {
        return;
    }

    if (ActiveRecord->Spec.Target.IsStale(true))
    {
        const FAlignmentRequestHandle InvalidHandle = ActiveRecord->Handle;
        ReleaseAlignmentRequest(InvalidHandle);
        return;
    }

    if (LastAlignmentExecutionFrame == GFrameCounter || !EnsureAlignmentDependencies())
    {
        return;
    }

    UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
    if (!Movement || !Movement->UpdatedComponent
        || DeltaTime <= 0.0f
        || ActiveRecord->Spec.MaximumTurnRate <= 0.0f
        || ActiveRecord->Spec.RemainingTurnBudget <= 0.0f)
    {
        return;
    }

    const FVector BeforeLocation = OwnerCharacter->GetActorLocation();
	FVector BeforePelvis = FVector::ZeroVector;
	const bool bHadPelvis = GetPelvisLocation(OwnerCharacter, BeforePelvis);
    const FRotator CurrentRotation = OwnerCharacter->GetActorRotation();
    const FRotator DesiredRotation = ResolveAlignmentRotation(ActiveRecord->Spec);
    const float YawError = FMath::FindDeltaAngleDegrees(
        static_cast<float>(CurrentRotation.Yaw),
        static_cast<float>(DesiredRotation.Yaw));
    const float MaximumStep = FMath::Min3(
        ActiveRecord->Spec.MaximumTurnRate * DeltaTime,
        ActiveRecord->Spec.RemainingTurnBudget,
        FMath::Abs(YawError));
    if (MaximumStep <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    FRotator NewRotation = CurrentRotation;
    NewRotation.Yaw += FMath::Sign(YawError) * MaximumStep;
    NewRotation.Normalize();

    LastAlignmentExecutionFrame = GFrameCounter;
    LastAlignmentExecutor = EAlignmentExecutor::CharacterMovement;
    FHitResult SweepHit;
    Movement->MoveUpdatedComponent(
        FVector::ZeroVector,
        NewRotation.Quaternion(),
        true,
        &SweepHit);

    const float AppliedYaw = FMath::Abs(FMath::FindDeltaAngleDegrees(
        static_cast<float>(CurrentRotation.Yaw),
        static_cast<float>(OwnerCharacter->GetActorRotation().Yaw)));
    ActiveRecord = AlignmentRequests.Find(ActiveAlignmentRequest);
    if (ActiveRecord)
    {
        ActiveRecord->Spec.RemainingTurnBudget = FMath::Max(
            0.0f,
            ActiveRecord->Spec.RemainingTurnBudget - AppliedYaw);

		const FAlignmentTelemetryContext TelemetryContext =
			ResolveAlignmentTelemetryContext(OwnerCharacter, ActiveRecord->Spec);
		if (TelemetryContext.Sink)
		{
			FDefenseTelemetryRecord Telemetry = MakeAlignmentTelemetry(
				OwnerCharacter,
				ActiveRecord->Spec,
				EDefenseTelemetryEvent::AlignmentFrame,
				TelemetryContext);
			Telemetry.FrameSimulationDelta = DeltaTime;
			Telemetry.AppliedFrameYaw = AppliedYaw;
			Telemetry.InitialYawError = FMath::Abs(YawError);
			Telemetry.FinalFrameYawError = FMath::Abs(FMath::FindDeltaAngleDegrees(
				OwnerCharacter->GetActorRotation().Yaw,
				DesiredRotation.Yaw));
			Telemetry.RemainingYawError = Telemetry.FinalFrameYawError;
			Telemetry.FrameDisplacement = OwnerCharacter->GetActorLocation() - BeforeLocation;
			Telemetry.UnexpectedDisplacement = Telemetry.FrameDisplacement;
			FVector AfterPelvis = FVector::ZeroVector;
			if (bHadPelvis && GetPelvisLocation(OwnerCharacter, AfterPelvis))
			{
				Telemetry.PelvisDelta =
					(AfterPelvis - BeforePelvis - Telemetry.FrameDisplacement).Size();
			}
			TelemetryContext.Sink->AppendDefenseTelemetry(MoveTemp(Telemetry));
		}
    }

#if WITH_AUTOMATION_TESTS
    ++AlignmentExecutionCount;
#endif
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

    EnsureAlignmentDependencies();
}

void UTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ReleaseAllAlignmentRequests(EAlignmentReleaseReason::ComponentTeardown);

    // Clean up tracking to avoid dangling delegate bindings
    StopWarpTracking();
    StopAttackerPairedWarpTracking();
    StopVictimWarpTracking();

    if (bAlignmentTickPrerequisiteRegistered && OwnerCharacter)
    {
        if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
        {
            PrimaryComponentTick.RemovePrerequisite(Movement, Movement->PrimaryComponentTick);
        }
        bAlignmentTickPrerequisiteRegistered = false;
    }

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

int32 UTargetingComponent::GetAllTargetsInRange(TArray<AActor*>& OutTargets, float MaxRange)
{
#if WITH_AUTOMATION_TESTS
    ++AllTargetsInRangeCallCount;
#endif
    OutTargets.Empty();

    GetActorsInRange(OutTargets, MaxRange);
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
    ReleaseActiveAttackWarp();
    if (!EnsureAlignmentDependencies() || !Config.bEnableWarp)
    {
        return false;
    }

    ACharacter* Owner = OwnerCharacter.Get();
    if (!Owner)
    {
        return false;
    }

    const FVector OwnerLocation = Owner->GetActorLocation();
    const UDefenseConfiguration* DefenseConfig = GetDefault<UDefenseConfiguration>();
    if (const UCombatComponent* Combat = Owner->FindComponentByClass<UCombatComponent>())
    {
        DefenseConfig = Combat->GetEffectiveDefenseConfiguration();
    }

    const float DefenseTurnRate = DefenseConfig && FMath::IsFinite(DefenseConfig->DefenseTurnRate)
        ? FMath::Max(0.0f, DefenseConfig->DefenseTurnRate)
        : 180.0f;
    const float RequestedTurnRate = FMath::IsFinite(Config.RotationSpeed) && Config.RotationSpeed > 0.0f
        ? Config.RotationSpeed
        : DefenseTurnRate;
    const float EffectiveTurnRate = FMath::Min(DefenseTurnRate, RequestedTurnRate);
    const float TurnBudget = DefenseConfig && FMath::IsFinite(DefenseConfig->MaximumAutomaticTurn)
        ? FMath::Max(0.0f, DefenseConfig->MaximumAutomaticTurn)
        : 70.0f;
    if (EffectiveTurnRate <= KINDA_SMALL_NUMBER || TurnBudget <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    FAlignmentRequestSpec Spec;
    Spec.OwnerId = TEXT("ActiveAttackWarp");
    Spec.OwnerGeneration = NextAttackAlignmentGeneration;
    if (NextAttackAlignmentGeneration == MAX_int32)
    {
        NextAttackAlignmentGeneration = 1;
    }
    else
    {
        ++NextAttackAlignmentGeneration;
    }
    Spec.Priority = EDefenseAlignmentPriority::ActiveAttackWarp;
    Spec.Executor = EAlignmentExecutor::MotionWarping;
    Spec.Target = Target;
    Spec.DesiredRotation = TargetRotation;
    Spec.MaximumTurnRate = EffectiveTurnRate;
    Spec.RemainingTurnBudget = TurnBudget;
    Spec.bTrackTargetRotation = Target != nullptr;

    if (Target)
    {
        const FVector TargetLocation = Target->GetActorLocation();
        const float Distance = FVector::Dist(OwnerLocation, TargetLocation);
        const bool bUseTranslation = Distance >= FMath::Max(0.0f, Config.MinWarpDistance);
        Spec.WarpTargetName = bUseTranslation ? Config.TargetWarpName : Config.RotationWarpName;
        Spec.bWarpTranslation = bUseTranslation;
        Spec.MaximumTranslation = bUseTranslation && FMath::IsFinite(Config.MaxWarpDistance)
            ? FMath::Max(0.0f, Config.MaxWarpDistance)
            : 0.0f;
        Spec.DesiredRotation = (TargetLocation - OwnerLocation).Rotation();
    }
    else
    {
        Spec.WarpTargetName = Config.RotationWarpName;
        Spec.bWarpTranslation = false;
        Spec.MaximumTranslation = 0.0f;
    }

    ActiveAttackAlignmentRequest = AcquireAlignmentRequest(Spec);
    if (!ActiveAttackAlignmentRequest.IsValid())
    {
        return false;
    }

    if (Target)
    {
        TrackedWarpTarget = Target;
        bIsTrackingWarpTarget = true;
        MotionWarpingComponent->OnPreUpdate.AddUniqueDynamic(
            this,
            &UTargetingComponent::OnMotionWarpingPreUpdate);
    }

    if (CombatDebug::IsTargetingDebugEnabled())
    {
        UE_LOG(LogTargeting, Log, TEXT("[ATTACK WARP] Acquired %s request '%s' at %.1f deg/s"),
            Spec.bWarpTranslation ? TEXT("targeted") : TEXT("rotation-only"),
            *Spec.WarpTargetName.ToString(),
            Spec.MaximumTurnRate);
        const FVector ForwardDir = Spec.DesiredRotation.Vector() * 200.0f;
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
        ReleaseActiveAttackWarp();
        return;
    }

    // Lazy fetch owner
    ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        ReleaseActiveAttackWarp();
        return;
    }

    FAlignmentRequestSpec Spec;
    if (!GetAlignmentRequestSpec(ActiveAttackAlignmentRequest, Spec))
    {
        ReleaseActiveAttackWarp();
        return;
    }

    AActor* Target = TrackedWarpTarget.Get();
    const FVector OwnerLocation = Owner->GetActorLocation();
    const FVector TargetLocation = Target->GetActorLocation();
    const FRotator LookAtRotation = (TargetLocation - OwnerLocation).Rotation();
    Spec.Target = Target;
    Spec.DesiredRotation = LookAtRotation;
    if (!UpdateAlignmentRequest(ActiveAttackAlignmentRequest, Spec))
    {
        ReleaseActiveAttackWarp();
        return;
    }

    // Debug visualization
    if (CombatDebug::IsTargetingDebugEnabled())
    {
        const FMotionWarpingTarget* PublishedTarget = MotionWarpingComp->FindWarpTarget(Spec.WarpTargetName);
        const FVector WarpLocation = PublishedTarget ? PublishedTarget->GetLocation() : OwnerLocation;
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
}

void UTargetingComponent::ReleaseActiveAttackWarp()
{
    StopWarpTracking();
    if (ActiveAttackAlignmentRequest.IsValid())
    {
        ReleaseAlignmentRequest(ActiveAttackAlignmentRequest);
        ActiveAttackAlignmentRequest = {};
    }
}

void UTargetingComponent::ClearMotionWarp(
    FName WarpTargetName,
    EAlignmentReleaseReason Reason)
{
    if (WarpTargetName == NAME_None
        && Reason != EAlignmentReleaseReason::Death
        && Reason != EAlignmentReleaseReason::ComponentTeardown)
    {
        UE_LOG(LogTargeting, Warning,
            TEXT("Broad motion-warp clear rejected for non-terminal reason %s"),
            *UEnum::GetValueAsString(Reason));
        return;
    }
    if (WarpTargetName != NAME_None && IsAlignmentWarpTargetOwned(WarpTargetName))
    {
        UE_LOG(LogTargeting, Warning,
            TEXT("Motion-warp clear rejected: target '%s' belongs to an alignment request"),
            *WarpTargetName.ToString());
        return;
    }

    if (WarpTargetName == NAME_None)
    {
        ReleaseAllAlignmentRequests(Reason);
    }

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

FAlignmentRequestHandle UTargetingComponent::AcquireAlignmentRequest(const FAlignmentRequestSpec& Spec)
{
    if (!EnsureAlignmentDependencies() || !ValidateAlignmentSpec(Spec))
    {
        return {};
    }

    if (Spec.Executor == EAlignmentExecutor::MotionWarping)
    {
        for (const TPair<FAlignmentRequestHandle, FAlignmentRequestRecord>& Pair : AlignmentRequests)
        {
            if (Pair.Value.Spec.Executor == EAlignmentExecutor::MotionWarping
                && Pair.Value.Spec.WarpTargetName == Spec.WarpTargetName)
            {
                UE_LOG(LogTargeting, Warning,
                    TEXT("Alignment request rejected: warp target '%s' already has an owner"),
                    *Spec.WarpTargetName.ToString());
                return {};
            }
        }

        if (MotionWarpingComponent->FindWarpTarget(Spec.WarpTargetName))
        {
            UE_LOG(LogTargeting, Warning,
                TEXT("Alignment request rejected: warp target '%s' is already registered outside the arbiter"),
                *Spec.WarpTargetName.ToString());
            return {};
        }
    }

    if (AlignmentRequests.IsEmpty() && !CaptureAlignmentRotationSettings())
    {
        return {};
    }

    if (NextAlignmentRequestValue == 0)
    {
        NextAlignmentRequestValue = 1;
    }
    while (AlignmentRequests.Contains(FAlignmentRequestHandle(NextAlignmentRequestValue)))
    {
        ++NextAlignmentRequestValue;
        if (NextAlignmentRequestValue == 0)
        {
            NextAlignmentRequestValue = 1;
        }
    }
    if (NextAlignmentAcquisitionOrder == 0)
    {
        NextAlignmentAcquisitionOrder = 1;
    }

    FAlignmentRequestRecord Record;
    Record.Handle = FAlignmentRequestHandle(NextAlignmentRequestValue++);
    Record.Spec = Spec;
    Record.AcquisitionOrder = NextAlignmentAcquisitionOrder++;
    AlignmentRequests.Add(Record.Handle, Record);
    ReevaluateAlignmentRequests();
	const FAlignmentTelemetryContext TelemetryContext =
		ResolveAlignmentTelemetryContext(OwnerCharacter, Spec);
	if (TelemetryContext.Sink)
	{
		FDefenseTelemetryRecord Telemetry = MakeAlignmentTelemetry(
			OwnerCharacter,
			Spec,
			EDefenseTelemetryEvent::AlignmentRequest,
			TelemetryContext);
		const FRotator DesiredRotation = ResolveAlignmentRotation(Spec);
		Telemetry.InitialYawError = OwnerCharacter
			? FMath::Abs(FMath::FindDeltaAngleDegrees(
				OwnerCharacter->GetActorRotation().Yaw,
				DesiredRotation.Yaw))
			: 0.0f;
		Telemetry.RemainingYawError = Telemetry.InitialYawError;
		TelemetryContext.Sink->AppendDefenseTelemetry(MoveTemp(Telemetry));
	}
    return Record.Handle;
}

bool UTargetingComponent::UpdateAlignmentRequest(
    FAlignmentRequestHandle Handle,
    const FAlignmentRequestSpec& Spec)
{
    FAlignmentRequestRecord* Record = AlignmentRequests.Find(Handle);
    if (!Record || !ValidateAlignmentSpec(Spec))
    {
        return false;
    }

    if (Spec.OwnerId != Record->Spec.OwnerId
        || Spec.OwnerGeneration != Record->Spec.OwnerGeneration
        || Spec.Priority != Record->Spec.Priority
        || Spec.Executor != Record->Spec.Executor
        || Spec.WarpTargetName != Record->Spec.WarpTargetName)
    {
        UE_LOG(LogTargeting, Warning,
            TEXT("Alignment request update rejected: owner identity, priority, executor, and target name are immutable"));
        return false;
    }

    FAlignmentRequestSpec UpdatedSpec = Spec;
    UpdatedSpec.RemainingTurnBudget = FMath::Min(
        Record->Spec.RemainingTurnBudget,
        Spec.RemainingTurnBudget);
    Record->Spec = MoveTemp(UpdatedSpec);
    ReevaluateAlignmentRequests();
    return true;
}

void UTargetingComponent::ReleaseAlignmentRequest(FAlignmentRequestHandle Handle)
{
    const FAlignmentRequestRecord* Record = AlignmentRequests.Find(Handle);
    if (!Record)
    {
        return;
    }

    const FAlignmentRequestRecord ReleasedRecord = *Record;
    RemoveRegisteredAlignmentModifiersForHandle(Handle);
    RemoveAlignmentWarpTarget(ReleasedRecord);
    AlignmentRequests.Remove(Handle);
    if (Handle == ActiveAttackAlignmentRequest)
    {
        StopWarpTracking();
        ActiveAttackAlignmentRequest = {};
    }
    ReevaluateAlignmentRequests();
}

void UTargetingComponent::ReleaseAllAlignmentRequests(EAlignmentReleaseReason Reason)
{
    if (Reason != EAlignmentReleaseReason::Death
        && Reason != EAlignmentReleaseReason::ComponentTeardown)
    {
        UE_LOG(LogTargeting, Warning,
            TEXT("Broad alignment release rejected for non-terminal reason %s"),
            *UEnum::GetValueAsString(Reason));
        return;
    }

    TArray<FAlignmentRequestHandle> Handles;
    AlignmentRequests.GetKeys(Handles);
    for (const FAlignmentRequestHandle Handle : Handles)
    {
        RemoveRegisteredAlignmentModifiersForHandle(Handle);
    }
    for (const TPair<FAlignmentRequestHandle, FAlignmentRequestRecord>& Pair : AlignmentRequests)
    {
        RemoveAlignmentWarpTarget(Pair.Value);
    }
    StopWarpTracking();
    ActiveAttackAlignmentRequest = {};
    AlignmentRequests.Reset();
    ActiveAlignmentRequest = {};
    SetComponentTickEnabled(false);
    RestoreAlignmentRotationSettings();
}

bool UTargetingComponent::GetAlignmentRequestSpec(
    FAlignmentRequestHandle Handle,
    FAlignmentRequestSpec& OutSpec) const
{
    const FAlignmentRequestRecord* Record = AlignmentRequests.Find(Handle);
    if (!Record)
    {
        OutSpec = {};
        return false;
    }

    OutSpec = Record->Spec;
    return true;
}

bool UTargetingComponent::RegisterAlignmentModifier(
    FName WarpTargetName,
    URootMotionModifier_Warp* RuntimeModifier,
    bool bAllowTranslation)
{
    if (!RuntimeModifier || WarpTargetName.IsNone() || !EnsureAlignmentDependencies()
        || RuntimeModifier->GetOwnerComponent() != MotionWarpingComponent)
    {
        return false;
    }

    SynchronizeAlignmentModifiers();
    const FAlignmentRequestRecord* Request = FindAlignmentRequestByWarpTarget(WarpTargetName);
    const bool bRequestAlreadyHasModifier = Request
        && RegisteredAlignmentModifiers.ContainsByPredicate(
            [Request](const FRegisteredAlignmentModifier& Registered)
            {
                const URootMotionModifier_Warp* ExistingModifier = Registered.Modifier.Get();
                return Registered.Handle == Request->Handle
                    && ExistingModifier
                    && ExistingModifier->GetState() != ERootMotionModifierState::MarkedForRemoval;
            });
    if (!Request || Request->Handle != ActiveAlignmentRequest
        || Request->Spec.Executor != EAlignmentExecutor::MotionWarping
        || Request->Spec.MaximumTurnRate <= KINDA_SMALL_NUMBER
        || Request->Spec.RemainingTurnBudget <= KINDA_SMALL_NUMBER
        || bRequestAlreadyHasModifier
        || FindRegisteredAlignmentModifier(RuntimeModifier))
    {
        return false;
    }

    RuntimeModifier->WarpTargetName = Request->Spec.WarpTargetName;
    RuntimeModifier->bWarpTranslation = Request->Spec.bWarpTranslation && bAllowTranslation;
    RuntimeModifier->bWarpRotation = true;
    RuntimeModifier->RotationMethod = EMotionWarpRotationMethod::ConstantRate;
    RuntimeModifier->WarpMaxRotationRate = Request->Spec.MaximumTurnRate;
    RuntimeModifier->OnUpdateDelegate.Unbind();
    RuntimeModifier->OnDeactivateDelegate.Unbind();
    RuntimeModifier->OnUpdateDelegate.BindDynamic(
        this,
        &UTargetingComponent::OnAlignmentModifierUpdated);
    RuntimeModifier->OnDeactivateDelegate.BindDynamic(
        this,
        &UTargetingComponent::OnAlignmentModifierDeactivated);

    FRegisteredAlignmentModifier Record;
    Record.Modifier = RuntimeModifier;
    Record.Handle = Request->Handle;
    Record.LastObservedYaw = OwnerCharacter->GetActorRotation().Yaw;
	Record.LastObservedLocation = OwnerCharacter->GetActorLocation();
	Record.bHasTransformBaseline = true;
	Record.bHasPelvisBaseline = GetPelvisLocation(
		OwnerCharacter,
		Record.LastObservedPelvisLocation);
    Record.bHasYawBaseline = true;
    RegisteredAlignmentModifiers.Add(MoveTemp(Record));
    SynchronizeAlignmentModifiers();
    return true;
}

bool UTargetingComponent::EnsureAlignmentDependencies()
{
    if (!OwnerCharacter)
    {
        OwnerCharacter = Cast<ACharacter>(GetOwner());
    }
    if (!OwnerCharacter)
    {
        return false;
    }

    if (!MotionWarpingComponent)
    {
        MotionWarpingComponent = OwnerCharacter->FindComponentByClass<UMotionWarpingComponent>();
    }

    if (!bAlignmentTickPrerequisiteRegistered)
    {
        if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
        {
            PrimaryComponentTick.AddPrerequisite(Movement, Movement->PrimaryComponentTick);
            bAlignmentTickPrerequisiteRegistered = true;
        }
    }
    return true;
}

bool UTargetingComponent::ValidateAlignmentSpec(const FAlignmentRequestSpec& Spec) const
{
    const bool bFiniteRotation = FMath::IsFinite(static_cast<float>(Spec.DesiredRotation.Pitch))
        && FMath::IsFinite(static_cast<float>(Spec.DesiredRotation.Yaw))
        && FMath::IsFinite(static_cast<float>(Spec.DesiredRotation.Roll));
    const bool bFiniteLimits = FMath::IsFinite(Spec.MaximumTurnRate)
        && FMath::IsFinite(Spec.RemainingTurnBudget)
        && FMath::IsFinite(Spec.MaximumTranslation);
    if (Spec.OwnerId.IsNone()
        || Spec.OwnerGeneration <= 0
        || Spec.Executor == EAlignmentExecutor::None
        || !bFiniteRotation
        || !bFiniteLimits
		|| Spec.TargetRelativeOffset.ContainsNaN()
        || Spec.MaximumTurnRate < 0.0f
        || Spec.RemainingTurnBudget < 0.0f
        || Spec.MaximumTranslation < 0.0f
        || Spec.Target.IsStale(true))
    {
        return false;
    }

    if (!OwnerCharacter || !OwnerCharacter->GetCharacterMovement())
    {
        return false;
    }

    if (Spec.Executor == EAlignmentExecutor::CharacterMovement)
    {
        return Spec.MaximumTurnRate > 0.0f;
    }

    return Spec.Executor == EAlignmentExecutor::MotionWarping
        && MotionWarpingComponent
        && !Spec.WarpTargetName.IsNone();
}

bool UTargetingComponent::CaptureAlignmentRotationSettings()
{
    if (CapturedRotationSettings.bCaptured)
    {
        return true;
    }
    if (!EnsureAlignmentDependencies())
    {
        return false;
    }

    UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
    if (!Movement)
    {
        return false;
    }

    CapturedRotationSettings.bUseControllerRotationYaw = OwnerCharacter->bUseControllerRotationYaw;
    CapturedRotationSettings.bOrientRotationToMovement = Movement->bOrientRotationToMovement;
    CapturedRotationSettings.bUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
    CapturedRotationSettings.bCaptured = true;

    OwnerCharacter->bUseControllerRotationYaw = false;
    Movement->bOrientRotationToMovement = false;
    Movement->bUseControllerDesiredRotation = false;
    return true;
}

void UTargetingComponent::RestoreAlignmentRotationSettings()
{
    if (!CapturedRotationSettings.bCaptured)
    {
        return;
    }

    ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
    if (Owner)
    {
        Owner->bUseControllerRotationYaw = CapturedRotationSettings.bUseControllerRotationYaw;
        if (UCharacterMovementComponent* Movement = Owner->GetCharacterMovement())
        {
            Movement->bOrientRotationToMovement = CapturedRotationSettings.bOrientRotationToMovement;
            Movement->bUseControllerDesiredRotation = CapturedRotationSettings.bUseControllerDesiredRotation;
        }
    }
    CapturedRotationSettings = {};
}

FAlignmentRequestHandle UTargetingComponent::ChooseActiveAlignmentRequest() const
{
    const FAlignmentRequestRecord* BestRecord = nullptr;
    for (const TPair<FAlignmentRequestHandle, FAlignmentRequestRecord>& Pair : AlignmentRequests)
    {
        const FAlignmentRequestRecord& Candidate = Pair.Value;
        if (!BestRecord
            || static_cast<uint8>(Candidate.Spec.Priority) > static_cast<uint8>(BestRecord->Spec.Priority)
            || (Candidate.Spec.Priority == BestRecord->Spec.Priority
                && Candidate.AcquisitionOrder > BestRecord->AcquisitionOrder))
        {
            BestRecord = &Candidate;
        }
    }
    return BestRecord ? BestRecord->Handle : FAlignmentRequestHandle();
}

bool UTargetingComponent::HasSmoothAlignmentRequest() const
{
    for (const TPair<FAlignmentRequestHandle, FAlignmentRequestRecord>& Pair : AlignmentRequests)
    {
        if (Pair.Value.Spec.Executor == EAlignmentExecutor::CharacterMovement)
        {
            return true;
        }
    }
    return false;
}

void UTargetingComponent::ReevaluateAlignmentRequests()
{
    TArray<FAlignmentRequestHandle> InvalidHandles;
    for (const TPair<FAlignmentRequestHandle, FAlignmentRequestRecord>& Pair : AlignmentRequests)
    {
        if (Pair.Value.Spec.Target.IsStale(true))
        {
            InvalidHandles.Add(Pair.Key);
        }
    }
    for (const FAlignmentRequestHandle Handle : InvalidHandles)
    {
        if (const FAlignmentRequestRecord* Record = AlignmentRequests.Find(Handle))
        {
            RemoveRegisteredAlignmentModifiersForHandle(Handle);
            RemoveAlignmentWarpTarget(*Record);
        }
        AlignmentRequests.Remove(Handle);
        if (Handle == ActiveAttackAlignmentRequest)
        {
            StopWarpTracking();
            ActiveAttackAlignmentRequest = {};
        }
    }

    for (const TPair<FAlignmentRequestHandle, FAlignmentRequestRecord>& Pair : AlignmentRequests)
    {
        RemoveAlignmentWarpTarget(Pair.Value);
    }

    ActiveAlignmentRequest = ChooseActiveAlignmentRequest();
    if (const FAlignmentRequestRecord* ActiveRecord = AlignmentRequests.Find(ActiveAlignmentRequest))
    {
        if (ActiveRecord->Spec.Executor == EAlignmentExecutor::MotionWarping)
        {
            ConfigureAlignmentWarpTarget(*ActiveRecord);
            LastAlignmentExecutionFrame = GFrameCounter;
            LastAlignmentExecutor = EAlignmentExecutor::MotionWarping;
        }
    }

    SynchronizeAlignmentModifiers();
    SetComponentTickEnabled(HasSmoothAlignmentRequest());
    if (AlignmentRequests.IsEmpty())
    {
        RestoreAlignmentRotationSettings();
    }
}

void UTargetingComponent::SynchronizeAlignmentModifiers()
{
    TArray<URootMotionModifier_Warp*> ModifiersToRemove;
    for (int32 Index = RegisteredAlignmentModifiers.Num() - 1; Index >= 0; --Index)
    {
        FRegisteredAlignmentModifier& Registered = RegisteredAlignmentModifiers[Index];
        URootMotionModifier_Warp* Modifier = Registered.Modifier.Get();
        const FAlignmentRequestRecord* Request = AlignmentRequests.Find(Registered.Handle);
        if (!Modifier)
        {
            RegisteredAlignmentModifiers.RemoveAtSwap(Index);
            continue;
        }
        if (!Request || Modifier->GetState() == ERootMotionModifierState::MarkedForRemoval)
        {
            ModifiersToRemove.Add(Modifier);
            continue;
        }

        const bool bShouldBeActive = Registered.Handle == ActiveAlignmentRequest
            && Request->Spec.Executor == EAlignmentExecutor::MotionWarping;
        if (bShouldBeActive)
        {
            if (Modifier->GetState() == ERootMotionModifierState::Disabled)
            {
                Registered.LastObservedYaw = OwnerCharacter->GetActorRotation().Yaw;
				Registered.LastObservedLocation = OwnerCharacter->GetActorLocation();
				Registered.bHasTransformBaseline = true;
				Registered.bHasPelvisBaseline = GetPelvisLocation(
					OwnerCharacter,
					Registered.LastObservedPelvisLocation);
				Registered.bHasAnimationRange = false;
                Registered.bHasYawBaseline = true;
                Modifier->SetState(ERootMotionModifierState::Waiting);
            }
        }
        else
        {
            if (Modifier->GetState() == ERootMotionModifierState::Active)
            {
                SynchronizeModifierBudget(Registered);
            }
            Registered.bHasYawBaseline = false;
			Registered.bHasTransformBaseline = false;
			Registered.bHasPelvisBaseline = false;
			Registered.bHasAnimationRange = false;
            if (Modifier->GetState() == ERootMotionModifierState::Active
                || Modifier->GetState() == ERootMotionModifierState::Waiting)
            {
                Modifier->SetState(ERootMotionModifierState::Disabled);
            }
        }
    }

    for (URootMotionModifier_Warp* Modifier : ModifiersToRemove)
    {
        UnregisterAlignmentModifier(Modifier, true);
    }
}

void UTargetingComponent::SynchronizeModifierBudget(FRegisteredAlignmentModifier& Record)
{
    FAlignmentRequestRecord* Request = AlignmentRequests.Find(Record.Handle);
    if (!Request || !OwnerCharacter)
    {
        return;
    }

    const float CurrentYaw = OwnerCharacter->GetActorRotation().Yaw;
    if (Record.bHasYawBaseline)
    {
        const float AppliedYaw = FMath::Abs(FMath::FindDeltaAngleDegrees(
            Record.LastObservedYaw,
            CurrentYaw));
        Request->Spec.RemainingTurnBudget = FMath::Max(
            0.0f,
            Request->Spec.RemainingTurnBudget - AppliedYaw);
    }
    Record.LastObservedYaw = CurrentYaw;
    Record.bHasYawBaseline = true;
}

void UTargetingComponent::RemoveRegisteredAlignmentModifiersForHandle(
    FAlignmentRequestHandle Handle)
{
    TArray<URootMotionModifier_Warp*> OwnedModifiers;
    for (FRegisteredAlignmentModifier& Registered : RegisteredAlignmentModifiers)
    {
        if (Registered.Handle == Handle)
        {
            if (URootMotionModifier_Warp* Modifier = Registered.Modifier.Get())
            {
                SynchronizeModifierBudget(Registered);
                OwnedModifiers.Add(Modifier);
            }
        }
    }
    for (URootMotionModifier_Warp* Modifier : OwnedModifiers)
    {
        UnregisterAlignmentModifier(Modifier, true);
    }
}

void UTargetingComponent::UnregisterAlignmentModifier(
    URootMotionModifier_Warp* Modifier,
    bool bMarkForRemoval)
{
    if (!Modifier)
    {
        return;
    }

    Modifier->OnUpdateDelegate.Unbind();
    Modifier->OnDeactivateDelegate.Unbind();
    RegisteredAlignmentModifiers.RemoveAllSwap(
        [Modifier](const FRegisteredAlignmentModifier& Registered)
        {
            return !Registered.Modifier.IsValid() || Registered.Modifier.Get() == Modifier;
        });
    if (bMarkForRemoval
        && Modifier->GetState() != ERootMotionModifierState::MarkedForRemoval)
    {
        Modifier->SetState(ERootMotionModifierState::MarkedForRemoval);
    }
}

UTargetingComponent::FRegisteredAlignmentModifier*
UTargetingComponent::FindRegisteredAlignmentModifier(URootMotionModifier_Warp* Modifier)
{
    return RegisteredAlignmentModifiers.FindByPredicate(
        [Modifier](const FRegisteredAlignmentModifier& Registered)
        {
            return Registered.Modifier.Get() == Modifier;
        });
}

const UTargetingComponent::FAlignmentRequestRecord*
UTargetingComponent::FindAlignmentRequestByWarpTarget(FName WarpTargetName) const
{
    for (const TPair<FAlignmentRequestHandle, FAlignmentRequestRecord>& Pair : AlignmentRequests)
    {
        if (Pair.Value.Spec.Executor == EAlignmentExecutor::MotionWarping
            && Pair.Value.Spec.WarpTargetName == WarpTargetName)
        {
            return &Pair.Value;
        }
    }
    return nullptr;
}

void UTargetingComponent::OnAlignmentModifierUpdated(
    UMotionWarpingComponent* InMotionWarpingComponent,
    URootMotionModifier* RootMotionModifier)
{
    constexpr float SmallRate = 1.0e-4f;
    URootMotionModifier_Warp* RuntimeModifier = Cast<URootMotionModifier_Warp>(RootMotionModifier);
    FRegisteredAlignmentModifier* Registered = FindRegisteredAlignmentModifier(RuntimeModifier);
    if (!RuntimeModifier || !Registered || InMotionWarpingComponent != MotionWarpingComponent)
    {
        return;
    }

    FAlignmentRequestRecord* Request = AlignmentRequests.Find(Registered->Handle);
    if (!Request || Registered->Handle != ActiveAlignmentRequest)
    {
        RuntimeModifier->SetState(ERootMotionModifierState::Disabled);
        return;
    }

	const float PreviousObservedYaw = Registered->LastObservedYaw;
	const FVector CurrentLocation = OwnerCharacter->GetActorLocation();
	const FVector FrameDisplacement = Registered->bHasTransformBaseline
		? CurrentLocation - Registered->LastObservedLocation
		: FVector::ZeroVector;
	FVector CurrentPelvisLocation = FVector::ZeroVector;
	const bool bHasCurrentPelvis = GetPelvisLocation(OwnerCharacter, CurrentPelvisLocation);
	FVector ExpectedAuthoredDisplacement = FVector::ZeroVector;
	float ObservedSimulationDelta = 0.0f;
	if (Registered->bHasAnimationRange && RuntimeModifier->Animation.IsValid())
	{
		const FTransform AuthoredRootMotion = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(
			RuntimeModifier->Animation.Get(),
			Registered->LastAnimationStartPosition,
			Registered->LastAnimationEndPosition);
		ExpectedAuthoredDisplacement = OwnerCharacter->GetActorQuat().RotateVector(
			AuthoredRootMotion.GetTranslation());
		if (RuntimeModifier->PlayRate > SmallRate)
		{
			ObservedSimulationDelta = FMath::Abs(
				(Registered->LastAnimationEndPosition - Registered->LastAnimationStartPosition)
				/ RuntimeModifier->PlayRate);
		}
	}

    SynchronizeModifierBudget(*Registered);
    const float EffectivePlayRate = RuntimeModifier->PlayRate;
    if (!FMath::IsFinite(EffectivePlayRate)
        || EffectivePlayRate <= SmallRate
        || Request->Spec.MaximumTurnRate <= SmallRate
        || Request->Spec.RemainingTurnBudget <= SmallRate)
    {
        UnregisterAlignmentModifier(RuntimeModifier, false);
        RuntimeModifier->SetState(ERootMotionModifierState::Disabled);
        return;
    }

    float EffectiveTurnRate = Request->Spec.MaximumTurnRate;
    const float AnimationDelta = RuntimeModifier->CurrentPosition - RuntimeModifier->PreviousPosition;
    const float SimulationDelta = FMath::Abs(AnimationDelta / EffectivePlayRate);
    if (SimulationDelta > SmallRate)
    {
        EffectiveTurnRate = FMath::Min(
            EffectiveTurnRate,
            Request->Spec.RemainingTurnBudget / SimulationDelta);
    }
    RuntimeModifier->WarpMaxRotationRate = EffectiveTurnRate / EffectivePlayRate;

	const FAlignmentTelemetryContext TelemetryContext =
		ResolveAlignmentTelemetryContext(OwnerCharacter, Request->Spec);
	if (TelemetryContext.Sink)
	{
		FDefenseTelemetryRecord Telemetry = MakeAlignmentTelemetry(
			OwnerCharacter,
			Request->Spec,
			EDefenseTelemetryEvent::AlignmentFrame,
			TelemetryContext);
		const FRotator DesiredRotation = ResolveAlignmentRotation(Request->Spec);
		Telemetry.FrameSimulationDelta = ObservedSimulationDelta;
		Telemetry.AppliedFrameYaw = Registered->bHasYawBaseline
			? FMath::Abs(FMath::FindDeltaAngleDegrees(
				PreviousObservedYaw,
				OwnerCharacter->GetActorRotation().Yaw))
			: 0.0f;
		Telemetry.InitialYawError = Telemetry.AppliedFrameYaw + FMath::Abs(
			FMath::FindDeltaAngleDegrees(
				OwnerCharacter->GetActorRotation().Yaw,
				DesiredRotation.Yaw));
		Telemetry.FinalFrameYawError = FMath::Abs(FMath::FindDeltaAngleDegrees(
			OwnerCharacter->GetActorRotation().Yaw,
			DesiredRotation.Yaw));
		Telemetry.RemainingYawError = Telemetry.FinalFrameYawError;
		Telemetry.ConfiguredEngineWarpRate = RuntimeModifier->WarpMaxRotationRate;
		Telemetry.FrameDisplacement = FrameDisplacement;
		Telemetry.ExpectedAuthoredDisplacement = ExpectedAuthoredDisplacement;
		Telemetry.ExpectedWarpDisplacement = RuntimeModifier->bWarpTranslation
			? FrameDisplacement - ExpectedAuthoredDisplacement
			: FVector::ZeroVector;
		Telemetry.UnexpectedDisplacement = RuntimeModifier->bWarpTranslation
			? FVector::ZeroVector
			: FrameDisplacement - ExpectedAuthoredDisplacement;
		if (Registered->bHasPelvisBaseline && bHasCurrentPelvis)
		{
			Telemetry.PelvisDelta =
				(CurrentPelvisLocation - Registered->LastObservedPelvisLocation - FrameDisplacement).Size();
		}
		TelemetryContext.Sink->AppendDefenseTelemetry(MoveTemp(Telemetry));
	}

	Registered->LastObservedLocation = CurrentLocation;
	Registered->LastObservedPelvisLocation = CurrentPelvisLocation;
	Registered->bHasTransformBaseline = true;
	Registered->bHasPelvisBaseline = bHasCurrentPelvis;
	Registered->LastAnimationStartPosition = RuntimeModifier->PreviousPosition;
	Registered->LastAnimationEndPosition = RuntimeModifier->CurrentPosition;
	Registered->bHasAnimationRange = true;
}

void UTargetingComponent::OnAlignmentModifierDeactivated(
    UMotionWarpingComponent* InMotionWarpingComponent,
    URootMotionModifier* RootMotionModifier)
{
    URootMotionModifier_Warp* RuntimeModifier = Cast<URootMotionModifier_Warp>(RootMotionModifier);
    FRegisteredAlignmentModifier* Registered = FindRegisteredAlignmentModifier(RuntimeModifier);
    if (!RuntimeModifier || !Registered || InMotionWarpingComponent != MotionWarpingComponent)
    {
        return;
    }

    if (RuntimeModifier->GetState() == ERootMotionModifierState::Disabled
        && AlignmentRequests.Contains(Registered->Handle)
        && Registered->Handle != ActiveAlignmentRequest)
    {
        Registered->bHasYawBaseline = false;
        return;
    }
    UnregisterAlignmentModifier(RuntimeModifier, false);
}

void UTargetingComponent::RemoveAlignmentWarpTarget(const FAlignmentRequestRecord& Record)
{
    if (MotionWarpingComponent
        && Record.Spec.Executor == EAlignmentExecutor::MotionWarping
        && !Record.Spec.WarpTargetName.IsNone())
    {
        MotionWarpingComponent->RemoveWarpTarget(Record.Spec.WarpTargetName);
    }
}

void UTargetingComponent::ConfigureAlignmentWarpTarget(const FAlignmentRequestRecord& Record)
{
    if (!MotionWarpingComponent || !OwnerCharacter
        || Record.Spec.Executor != EAlignmentExecutor::MotionWarping
        || Record.Spec.WarpTargetName.IsNone())
    {
        return;
    }

    const FVector OwnerLocation = OwnerCharacter->GetActorLocation();
    FVector WarpLocation = OwnerLocation;
    if (Record.Spec.bWarpTranslation && Record.Spec.Target.IsValid())
    {
		const AActor* TargetActor = Record.Spec.Target.Get();
		const FVector TargetLocation = TargetActor->GetActorLocation()
			+ TargetActor->GetActorRotation().RotateVector(Record.Spec.TargetRelativeOffset);
		const FVector ToTarget = TargetLocation - OwnerLocation;
        WarpLocation += ToTarget.GetClampedToMaxSize(Record.Spec.MaximumTranslation);
        if (UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent())
        {
            WarpLocation = UDebugUtils::AdjustLocationToGround(
                GetWorld(),
                WarpLocation,
                Capsule->GetScaledCapsuleHalfHeight(),
                OwnerCharacter,
                false);
        }
    }

    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
        Record.Spec.WarpTargetName,
        WarpLocation,
        ResolveAlignmentRotation(Record.Spec));
}

FRotator UTargetingComponent::ResolveAlignmentRotation(const FAlignmentRequestSpec& Spec) const
{
    if (Spec.bTrackTargetRotation && OwnerCharacter && Spec.Target.IsValid())
    {
        FVector ToTarget = Spec.Target->GetActorLocation() - OwnerCharacter->GetActorLocation();
        ToTarget.Z = 0.0f;
        if (!ToTarget.IsNearlyZero())
        {
            return FRotator(0.0, ToTarget.Rotation().Yaw, 0.0);
        }
    }
    return FRotator(0.0, Spec.DesiredRotation.Yaw, 0.0);
}

bool UTargetingComponent::IsAlignmentWarpTargetOwned(FName WarpTargetName) const
{
    if (WarpTargetName.IsNone())
    {
        return false;
    }
    for (const TPair<FAlignmentRequestHandle, FAlignmentRequestRecord>& Pair : AlignmentRequests)
    {
        if (Pair.Value.Spec.Executor == EAlignmentExecutor::MotionWarping
            && Pair.Value.Spec.WarpTargetName == WarpTargetName)
        {
            return true;
        }
    }
    return false;
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

void UTargetingComponent::GetActorsInRange(TArray<AActor*>& OutActors, float MaxRange) const
{
    // Lazy fetch owner for test compatibility
    const ACharacter* Owner = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());

    if (!Owner || !GetWorld())
    {
        return;
    }

    const FVector OwnerLocation = Owner->GetActorLocation();
    const UTargetingSettings* Settings = GetEffectiveSettings();
    const float ConfiguredRadius = Settings ? Settings->MaxTargetDistance : 1000.0f;
    const float SearchRadius = FMath::IsFinite(MaxRange) && MaxRange >= 0.0f
        ? FMath::Min(FMath::Max(0.0f, ConfiguredRadius), MaxRange)
        : FMath::Max(0.0f, ConfiguredRadius);

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
