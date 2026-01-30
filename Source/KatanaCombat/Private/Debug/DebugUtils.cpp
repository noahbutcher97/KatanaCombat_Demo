// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/DebugUtils.h"
#include "Debug/DebugConfig.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnvironment, Log, All);

// ============================================================================
// DIRECTION CONVERSION HELPERS
// ============================================================================

EAttackDirection UDebugUtils::InputDirectionToAttackDirection(EInputDirection InputDir)
{
	switch (InputDir)
	{
		case EInputDirection::Forward:
		case EInputDirection::ForwardLeft:
		case EInputDirection::ForwardRight:
			return EAttackDirection::Forward;

		case EInputDirection::Backward:
		case EInputDirection::BackwardLeft:
		case EInputDirection::BackwardRight:
			return EAttackDirection::Backward;

		case EInputDirection::Left:
			return EAttackDirection::Left;

		case EInputDirection::Right:
			return EAttackDirection::Right;

		case EInputDirection::None:
		default:
			return EAttackDirection::None;
	}
}

FString UDebugUtils::YawToCardinalDirection(float Yaw)
{
	// Normalize to -180 to +180 range
	float NormalizedYaw = NormalizeYaw(Yaw);

	// Convert to 0-360 range for easier compass logic
	if (NormalizedYaw < 0.0f)
	{
		NormalizedYaw += 360.0f;
	}

	// Map to 8 compass directions (45 each)
	if (NormalizedYaw >= 337.5f || NormalizedYaw < 22.5f)
		return TEXT("N");
	else if (NormalizedYaw >= 22.5f && NormalizedYaw < 67.5f)
		return TEXT("NE");
	else if (NormalizedYaw >= 67.5f && NormalizedYaw < 112.5f)
		return TEXT("E");
	else if (NormalizedYaw >= 112.5f && NormalizedYaw < 157.5f)
		return TEXT("SE");
	else if (NormalizedYaw >= 157.5f && NormalizedYaw < 202.5f)
		return TEXT("S");
	else if (NormalizedYaw >= 202.5f && NormalizedYaw < 247.5f)
		return TEXT("SW");
	else if (NormalizedYaw >= 247.5f && NormalizedYaw < 292.5f)
		return TEXT("W");
	else
		return TEXT("NW");
}

// ============================================================================
// FORMATTING HELPERS
// ============================================================================

FString UDebugUtils::FormatRotationDebug(const FRotator& Rotation)
{
	return FString::Printf(TEXT("Yaw=%.1f (%s)"),
		Rotation.Yaw,
		*YawToCardinalDirection(Rotation.Yaw));
}

FString UDebugUtils::FormatVector2DDebug(const FVector2D& Vec)
{
	return FString::Printf(TEXT("(X=%.2f, Y=%.2f) mag=%.2f"),
		Vec.X,
		Vec.Y,
		Vec.Size());
}

FString UDebugUtils::FormatInputDirectionDebug(EInputDirection Direction)
{
	switch (Direction)
	{
		case EInputDirection::None:          return TEXT("None");
		case EInputDirection::Forward:       return TEXT("Forward");
		case EInputDirection::ForwardRight:  return TEXT("ForwardRight");
		case EInputDirection::Right:         return TEXT("Right");
		case EInputDirection::BackwardRight: return TEXT("BackwardRight");
		case EInputDirection::Backward:      return TEXT("Backward");
		case EInputDirection::BackwardLeft:  return TEXT("BackwardLeft");
		case EInputDirection::Left:          return TEXT("Left");
		case EInputDirection::ForwardLeft:   return TEXT("ForwardLeft");
		default:                             return TEXT("Unknown");
	}
}

FString UDebugUtils::FormatAttackDirectionDebug(EAttackDirection Direction)
{
	switch (Direction)
	{
		case EAttackDirection::None:     return TEXT("None");
		case EAttackDirection::Forward:  return TEXT("Forward");
		case EAttackDirection::Backward: return TEXT("Backward");
		case EAttackDirection::Left:     return TEXT("Left");
		case EAttackDirection::Right:    return TEXT("Right");
		default:                         return TEXT("Unknown");
	}
}

// ============================================================================
// CALCULATION HELPERS
// ============================================================================

float UDebugUtils::CalculateYawDelta(float FromYaw, float ToYaw)
{
	float Delta = ToYaw - FromYaw;

	// Normalize to -180 to +180 range (shortest path)
	while (Delta > 180.0f)
	{
		Delta -= 360.0f;
	}
	while (Delta < -180.0f)
	{
		Delta += 360.0f;
	}

	return Delta;
}

float UDebugUtils::NormalizeYaw(float Yaw)
{
	while (Yaw > 180.0f)
	{
		Yaw -= 360.0f;
	}
	while (Yaw < -180.0f)
	{
		Yaw += 360.0f;
	}
	return Yaw;
}

FRotator UDebugUtils::GetMeshRotationOffset(ACharacter* Character)
{
	if (!Character)
	{
		return FRotator::ZeroRotator;
	}

	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (!Mesh)
	{
		return FRotator::ZeroRotator;
	}

	// Calculate difference between actor rotation and mesh rotation
	FRotator ActorRotation = Character->GetActorRotation();
	FRotator MeshRotation = Mesh->GetComponentRotation();

	// Return delta (will be ZeroRotator if no offset)
	return FRotator(
		FMath::Fmod(MeshRotation.Pitch - ActorRotation.Pitch + 180.0f, 360.0f) - 180.0f,
		FMath::Fmod(MeshRotation.Yaw - ActorRotation.Yaw + 180.0f, 360.0f) - 180.0f,
		FMath::Fmod(MeshRotation.Roll - ActorRotation.Roll + 180.0f, 360.0f) - 180.0f
	);
}

// ============================================================================
// WEAPON TRACE DEBUG VISUALIZATION
// ============================================================================

void UDebugUtils::DrawWeaponTrace(
	UWorld* World,
	const FVector& CurrentStart,
	const FVector& CurrentEnd,
	const FVector& PreviousStart,
	const FVector& PreviousEnd,
	float TraceRadius,
	bool bHit,
	const FHitResult& HitResult)
{
	if (!World || !CombatDebug::IsWeaponDebugEnabled())
	{
		return;
	}

	const float DrawDuration = CombatDebug::GetDebugDrawDuration();
	const FColor TraceColor = bHit ? FColor::Red : FColor::Green;
	const FColor PreviousColor = FColor(128, 128, 128); // Gray for previous frame

	// Draw current frame weapon capsule
	DrawDebugCapsule(
		World,
		(CurrentStart + CurrentEnd) * 0.5f,  // Center
		(CurrentEnd - CurrentStart).Size() * 0.5f,  // Half-height
		TraceRadius,
		FQuat::FindBetweenNormals(FVector::UpVector, (CurrentEnd - CurrentStart).GetSafeNormal()),
		TraceColor,
		false,
		DrawDuration);

	// Draw previous frame weapon capsule (dimmer)
	DrawDebugCapsule(
		World,
		(PreviousStart + PreviousEnd) * 0.5f,
		(PreviousEnd - PreviousStart).Size() * 0.5f,
		TraceRadius,
		FQuat::FindBetweenNormals(FVector::UpVector, (PreviousEnd - PreviousStart).GetSafeNormal()),
		PreviousColor,
		false,
		DrawDuration);

	// Draw weapon axis lines (blade direction)
	DrawDebugLine(World, CurrentStart, CurrentEnd, TraceColor, false, DrawDuration, 0, 3.0f);
	DrawDebugLine(World, PreviousStart, PreviousEnd, PreviousColor, false, DrawDuration, 0, 1.5f);

	// Draw socket markers
	DrawDebugSphere(World, CurrentStart, 4.0f, 6, FColor::Blue, false, DrawDuration);
	DrawDebugSphere(World, CurrentEnd, 4.0f, 6, FColor::Red, false, DrawDuration);

	// Draw sweep motion lines (connecting previous to current frame)
	DrawDebugLine(World, PreviousStart, CurrentStart, FColor::Cyan, false, DrawDuration, 0, 1.0f);
	DrawDebugLine(World, PreviousEnd, CurrentEnd, FColor::Cyan, false, DrawDuration, 0, 1.0f);

	// Draw hit info if we hit something
	if (bHit && HitResult.bBlockingHit)
	{
		// Impact point - larger and more visible
		DrawDebugSphere(World, HitResult.ImpactPoint, 8.0f, 8, FColor::Orange, false, DrawDuration);

		// Impact normal
		DrawDebugLine(
			World,
			HitResult.ImpactPoint,
			HitResult.ImpactPoint + HitResult.ImpactNormal * 40.0f,
			FColor::Yellow,
			false,
			DrawDuration,
			0,
			3.0f);

		// Draw hit actor name
		if (HitResult.GetActor())
		{
			DrawDebugString(
				World,
				HitResult.ImpactPoint + FVector(0, 0, 25),
				FString::Printf(TEXT("HIT: %s"), *HitResult.GetActor()->GetName()),
				nullptr,
				FColor::White,
				DrawDuration,
				false);

			// Verbose logging
			if (CombatDebug::IsVerboseLogEnabled())
			{
				UE_LOG(LogDebug, Log, TEXT("[WeaponTrace] Hit: %s at %s"),
					*HitResult.GetActor()->GetName(),
					*HitResult.ImpactPoint.ToString());
			}
		}
	}
}

void UDebugUtils::DrawWeaponSockets(
	UWorld* World,
	const FVector& StartLocation,
	const FVector& EndLocation)
{
	if (!World || !CombatDebug::IsWeaponDebugEnabled())
	{
		return;
	}

	const float DrawDuration = CombatDebug::GetDebugDrawDuration();

	// Draw weapon axis line
	DrawDebugLine(World, StartLocation, EndLocation, FColor::Cyan, false, DrawDuration, 0, 1.5f);

	// Draw socket positions
	DrawDebugSphere(World, StartLocation, 3.0f, 8, FColor::Blue, false, DrawDuration);
	DrawDebugSphere(World, EndLocation, 3.0f, 8, FColor::Red, false, DrawDuration);

	// Labels
	DrawDebugString(World, StartLocation + FVector(0, 0, 10), TEXT("Start"), nullptr, FColor::Blue, DrawDuration, false);
	DrawDebugString(World, EndLocation + FVector(0, 0, 10), TEXT("End"), nullptr, FColor::Red, DrawDuration, false);
}

// ============================================================================
// ENVIRONMENT/SLOPE DEBUG VISUALIZATION
// ============================================================================

void UDebugUtils::DrawFloorNormal(
	UWorld* World,
	const FVector& Location,
	const FVector& FloorNormal,
	const FString& Label)
{
	if (!World || !CombatDebug::IsEnvironmentDebugEnabled())
	{
		return;
	}

	const float DrawDuration = CombatDebug::GetDebugDrawDuration();
	const float SlopeAngle = CalculateSlopeAngle(FloorNormal);
	const bool bIsWalkable = IsSlopeWalkable(FloorNormal);

	// Color based on walkability: green = walkable, orange = steep, red = unwalkable
	FColor NormalColor = bIsWalkable ? FColor::Green : (SlopeAngle < 60.0f ? FColor::Orange : FColor::Red);

	// Draw floor normal arrow
	const FVector NormalEnd = Location + FloorNormal * 75.0f;
	DrawDebugDirectionalArrow(World, Location, NormalEnd, 20.0f, NormalColor, false, DrawDuration, 0, 3.0f);

	// Draw small disc to represent floor plane
	DrawDebugCircle(World, Location, 40.0f, 16, NormalColor, false, DrawDuration, 0, 1.5f,
		FVector::CrossProduct(FloorNormal, FVector::ForwardVector).GetSafeNormal(),
		FloorNormal);

	// Draw info label
	FString InfoLabel = FString::Printf(TEXT("%s%.1f deg"),
		Label.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("%s: "), *Label),
		SlopeAngle);
	DrawDebugString(World, Location + FVector(0, 0, 30), InfoLabel, nullptr, NormalColor, DrawDuration, false);
}

void UDebugUtils::DrawGroundTrace(
	UWorld* World,
	const FVector& TraceStart,
	const FVector& GroundHitLocation,
	bool bHitGround,
	const FVector& AdjustedLocation)
{
	if (!World || !CombatDebug::IsEnvironmentDebugEnabled())
	{
		return;
	}

	const float DrawDuration = CombatDebug::GetDebugDrawDuration();
	const FColor TraceColor = bHitGround ? FColor::Green : FColor::Red;

	// Draw trace line
	DrawDebugLine(World, TraceStart, bHitGround ? GroundHitLocation : TraceStart - FVector(0, 0, 500),
		TraceColor, false, DrawDuration, 0, 1.5f);

	if (bHitGround)
	{
		// Draw ground impact point
		DrawDebugSphere(World, GroundHitLocation, 8.0f, 8, FColor::Yellow, false, DrawDuration);

		// Draw adjusted final position
		DrawDebugSphere(World, AdjustedLocation, 10.0f, 8, FColor::Cyan, false, DrawDuration);

		// Draw connection from ground to adjusted
		DrawDebugLine(World, GroundHitLocation, AdjustedLocation, FColor::Cyan, false, DrawDuration, 0, 1.0f);
	}
}

void UDebugUtils::DrawWarpZAdjustment(
	UWorld* World,
	const FVector& OriginalLocation,
	const FVector& AdjustedLocation)
{
	if (!World || !CombatDebug::IsEnvironmentDebugEnabled())
	{
		return;
	}

	const float DrawDuration = CombatDebug::GetDebugDrawDuration();
	const float ZDelta = AdjustedLocation.Z - OriginalLocation.Z;

	// Only draw if there's a significant adjustment
	if (FMath::Abs(ZDelta) < 1.0f)
	{
		return;
	}

	// Color based on direction: cyan = down (toward ground), orange = up
	FColor AdjustColor = ZDelta < 0 ? FColor::Cyan : FColor::Orange;

	// Draw original position (hollow)
	DrawDebugSphere(World, OriginalLocation, 15.0f, 8, FColor(128, 128, 128), false, DrawDuration);

	// Draw adjusted position (solid)
	DrawDebugSphere(World, AdjustedLocation, 12.0f, 8, AdjustColor, false, DrawDuration);

	// Draw adjustment vector
	DrawDebugLine(World, OriginalLocation, AdjustedLocation, AdjustColor, false, DrawDuration, 0, 3.0f);

	// Label with adjustment amount
	FString AdjustLabel = FString::Printf(TEXT("Z: %+.1f"), ZDelta);
	DrawDebugString(World, (OriginalLocation + AdjustedLocation) * 0.5f + FVector(20, 0, 0),
		AdjustLabel, nullptr, AdjustColor, DrawDuration, false);

	if (CombatDebug::IsVerboseLogEnabled())
	{
		UE_LOG(LogEnvironment, Log, TEXT("[Warp Z Adjust] Original: %s -> Adjusted: %s (Delta: %+.1f)"),
			*OriginalLocation.ToString(), *AdjustedLocation.ToString(), ZDelta);
	}
}

void UDebugUtils::DrawSlopeTransition(
	UWorld* World,
	const FVector& CharacterLocation,
	const FVector& CurrentFloorNormal,
	const FVector& TargetFloorNormal,
	const FVector& TargetLocation)
{
	if (!World || !CombatDebug::IsEnvironmentDebugEnabled())
	{
		return;
	}

	const float DrawDuration = CombatDebug::GetDebugDrawDuration();

	// Draw current floor normal (green)
	DrawFloorNormal(World, CharacterLocation, CurrentFloorNormal, TEXT("Current"));

	// Draw target floor normal (blue)
	const FVector TargetNormalEnd = TargetLocation + TargetFloorNormal * 75.0f;
	DrawDebugDirectionalArrow(World, TargetLocation, TargetNormalEnd, 20.0f, FColor::Blue, false, DrawDuration, 0, 3.0f);

	// Draw transition path
	DrawDebugLine(World, CharacterLocation, TargetLocation, FColor::Yellow, false, DrawDuration, 0, 2.0f);

	// Calculate and display angle difference
	const float CurrentAngle = CalculateSlopeAngle(CurrentFloorNormal);
	const float TargetAngle = CalculateSlopeAngle(TargetFloorNormal);
	const float AngleDelta = TargetAngle - CurrentAngle;

	FString TransitionLabel = FString::Printf(TEXT("Slope: %.1f -> %.1f (%+.1f)"),
		CurrentAngle, TargetAngle, AngleDelta);
	DrawDebugString(World, (CharacterLocation + TargetLocation) * 0.5f + FVector(0, 0, 50),
		TransitionLabel, nullptr, FColor::Yellow, DrawDuration, false);
}

// ============================================================================
// ENVIRONMENTAL AWARENESS HELPERS
// ============================================================================

FGroundSampleResult UDebugUtils::SampleGroundAtLocation(
	UWorld* World,
	const FVector& Location,
	float TraceStartOffset,
	float TraceDistance,
	AActor* ActorToIgnore)
{
	FGroundSampleResult Result;

	if (!World)
	{
		return Result;
	}

	FVector TraceStart = Location + FVector(0.0f, 0.0f, TraceStartOffset);
	FVector TraceEnd = Location - FVector(0.0f, 0.0f, TraceDistance);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.bReturnPhysicalMaterial = false;
	if (ActorToIgnore)
	{
		QueryParams.AddIgnoredActor(ActorToIgnore);
	}

	if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
	{
		Result.bFoundGround = true;
		Result.GroundLocation = HitResult.ImpactPoint;
		Result.GroundNormal = HitResult.ImpactNormal;
		Result.SlopeAngle = CalculateSlopeAngle(HitResult.ImpactNormal);
		Result.bIsWalkable = IsSlopeWalkable(HitResult.ImpactNormal);
	}

	return Result;
}

FVector UDebugUtils::AdjustLocationToGround(
	UWorld* World,
	const FVector& Location,
	float HeightOffset,
	AActor* ActorToIgnore,
	bool bDrawDebug)
{
	if (!World)
	{
		return Location;
	}

	FGroundSampleResult GroundSample = SampleGroundAtLocation(World, Location, 100.0f, 500.0f, ActorToIgnore);

	if (GroundSample.bFoundGround)
	{
		FVector AdjustedLocation = Location;
		AdjustedLocation.Z = GroundSample.GroundLocation.Z + HeightOffset;

		if (bDrawDebug || CombatDebug::IsEnvironmentDebugEnabled())
		{
			DrawGroundTrace(World, Location + FVector(0, 0, 100), GroundSample.GroundLocation, true, AdjustedLocation);
			DrawWarpZAdjustment(World, Location, AdjustedLocation);
		}

		return AdjustedLocation;
	}

	return Location;
}

float UDebugUtils::CalculateSlopeAngle(const FVector& FloorNormal)
{
	// Angle from vertical (UpVector) in degrees
	// 0 = perfectly flat, 90 = vertical wall
	return FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(FloorNormal.GetSafeNormal(), FVector::UpVector)));
}

bool UDebugUtils::IsSlopeWalkable(const FVector& FloorNormal, float WalkableFloorAngle)
{
	return CalculateSlopeAngle(FloorNormal) <= WalkableFloorAngle;
}

FVector UDebugUtils::GetCharacterFloorNormal(ACharacter* Character)
{
	if (!Character)
	{
		return FVector::UpVector;
	}

	UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
	if (!MovementComp)
	{
		return FVector::UpVector;
	}

	// Use CMC's floor finding which is more accurate for grounded characters
	if (MovementComp->IsMovingOnGround())
	{
		FFindFloorResult FloorResult;
		MovementComp->FindFloor(Character->GetActorLocation(), FloorResult, true);

		if (FloorResult.IsWalkableFloor())
		{
			return FloorResult.HitResult.ImpactNormal;
		}
	}

	return FVector::UpVector;
}

bool UDebugUtils::IsCharacterFloating(ACharacter* Character, float FloatThreshold)
{
	if (!Character)
	{
		return false;
	}

	UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
	if (!MovementComp || MovementComp->IsFalling())
	{
		return false; // Already falling, not "floating"
	}

	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!Capsule)
	{
		return false;
	}

	// Sample ground at character location
	FGroundSampleResult GroundSample = SampleGroundAtLocation(
		Character->GetWorld(),
		Character->GetActorLocation(),
		10.0f,  // Small start offset since we're already roughly at ground level
		200.0f, // Don't need to trace far
		Character);

	if (!GroundSample.bFoundGround)
	{
		return true; // No ground found = definitely floating
	}

	// Calculate expected Z vs actual Z
	float ExpectedZ = GroundSample.GroundLocation.Z + Capsule->GetScaledCapsuleHalfHeight();
	float ActualZ = Character->GetActorLocation().Z;

	return (ActualZ - ExpectedZ) > FloatThreshold;
}

bool UDebugUtils::SnapCharacterToGround(ACharacter* Character, float FloatThreshold, bool bDrawDebug)
{
	if (!Character)
	{
		return false;
	}

	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!Capsule)
	{
		return false;
	}

	// Sample ground at character location
	FGroundSampleResult GroundSample = SampleGroundAtLocation(
		Character->GetWorld(),
		Character->GetActorLocation(),
		50.0f,
		300.0f,
		Character);

	if (!GroundSample.bFoundGround || !GroundSample.bIsWalkable)
	{
		return false;
	}

	// Calculate expected Z vs actual Z
	float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	float ExpectedZ = GroundSample.GroundLocation.Z + CapsuleHalfHeight;
	float ActualZ = Character->GetActorLocation().Z;
	float FloatAmount = ActualZ - ExpectedZ;

	// Only snap if floating above threshold
	if (FloatAmount <= FloatThreshold)
	{
		return false;
	}

	// Snap to ground
	FVector SnappedLocation = Character->GetActorLocation();
	SnappedLocation.Z = ExpectedZ;
	Character->SetActorLocation(SnappedLocation);

	if (bDrawDebug || CombatDebug::IsEnvironmentDebugEnabled())
	{
		UWorld* World = Character->GetWorld();
		DrawGroundTrace(World, Character->GetActorLocation() + FVector(0, 0, FloatAmount),
			GroundSample.GroundLocation, true, SnappedLocation);

		if (CombatDebug::IsVerboseLogEnabled())
		{
			UE_LOG(LogEnvironment, Log, TEXT("[Ground Snap] %s snapped %.1f units to ground"),
				*Character->GetName(), FloatAmount);
		}
	}

	return true;
}

// ============================================================================
// PAIRED ANIMATION DEBUG VISUALIZATION
// ============================================================================

void UDebugUtils::DrawWarpTargetCrosshair(
	UWorld* World,
	const FVector& WarpTarget,
	const FVector& CharacterLocation,
	bool bIsAttacker,
	const FString& Label)
{
	if (!World || !CombatDebug::IsPairedAnimWarpDebugEnabled())
	{
		return;
	}

	const float DrawDuration = CombatDebug::GetDebugDrawDuration();

	// Color: Cyan for attacker warp, Magenta for victim warp
	const FColor WarpColor = bIsAttacker ? FColor::Cyan : FColor::Magenta;
	const FColor DimColor = FColor(WarpColor.R / 2, WarpColor.G / 2, WarpColor.B / 2);

	// Draw crosshair at warp target
	const float CrosshairSize = 30.0f;
	DrawDebugLine(World, WarpTarget - FVector(CrosshairSize, 0, 0), WarpTarget + FVector(CrosshairSize, 0, 0),
		WarpColor, false, DrawDuration, 0, 3.0f);
	DrawDebugLine(World, WarpTarget - FVector(0, CrosshairSize, 0), WarpTarget + FVector(0, CrosshairSize, 0),
		WarpColor, false, DrawDuration, 0, 3.0f);
	DrawDebugLine(World, WarpTarget - FVector(0, 0, CrosshairSize), WarpTarget + FVector(0, 0, CrosshairSize),
		WarpColor, false, DrawDuration, 0, 3.0f);

	// Draw outer circle
	DrawDebugCircle(World, WarpTarget, CrosshairSize * 1.5f, 16, WarpColor, false, DrawDuration, 0, 2.0f,
		FVector::ForwardVector, FVector::RightVector);

	// Draw inner sphere (solid feel)
	DrawDebugSphere(World, WarpTarget, 8.0f, 8, WarpColor, false, DrawDuration);

	// Draw arrow from character to warp target
	DrawDebugDirectionalArrow(World, CharacterLocation, WarpTarget, 25.0f, DimColor, false, DrawDuration, 0, 2.0f);

	// Distance calculation
	const float Distance = FVector::Dist(CharacterLocation, WarpTarget);

	// Label with distance
	FString FullLabel = FString::Printf(TEXT("%s (%.0fu)"),
		Label.IsEmpty() ? (bIsAttacker ? TEXT("ATK Warp") : TEXT("VIC Warp")) : *Label,
		Distance);
	DrawDebugString(World, WarpTarget + FVector(0, 0, 40), FullLabel, nullptr, WarpColor, DrawDuration, false);
}

void UDebugUtils::DrawPartnerConnection(
	UWorld* World,
	const FVector& AttackerLocation,
	const FVector& PartnerLocation,
	float Distance,
	float MaxDistance)
{
	if (!World || !CombatDebug::IsPairedAnimPartnerDebugEnabled())
	{
		return;
	}

	const float DrawDuration = CombatDebug::GetDebugDrawDuration();

	// Color based on distance validity: Yellow = OK, Orange = warning, Red = too far
	const float DistanceRatio = Distance / FMath::Max(MaxDistance, 1.0f);
	FColor LineColor;
	if (DistanceRatio <= 0.75f)
	{
		LineColor = FColor::Yellow;
	}
	else if (DistanceRatio <= 1.0f)
	{
		LineColor = FColor::Orange;
	}
	else
	{
		LineColor = FColor::Red;
	}

	// Draw dashed line between partners
	const float DashLength = 20.0f;
	const FVector Direction = (PartnerLocation - AttackerLocation).GetSafeNormal();
	FVector CurrentPos = AttackerLocation;
	bool bDrawSegment = true;
	int32 SegmentCount = 0;
	const int32 MaxSegments = 100; // Safety limit

	while (FVector::Dist(CurrentPos, AttackerLocation) < Distance && SegmentCount < MaxSegments)
	{
		FVector NextPos = CurrentPos + Direction * DashLength;
		if (FVector::Dist(NextPos, AttackerLocation) > Distance)
		{
			NextPos = PartnerLocation;
		}

		if (bDrawSegment)
		{
			DrawDebugLine(World, CurrentPos, NextPos, LineColor, false, DrawDuration, 0, 2.5f);
		}

		CurrentPos = NextPos;
		bDrawSegment = !bDrawSegment;
		SegmentCount++;
	}

	// Draw partner markers
	DrawDebugSphere(World, AttackerLocation + FVector(0, 0, 90), 12.0f, 8, FColor::Green, false, DrawDuration);
	DrawDebugSphere(World, PartnerLocation + FVector(0, 0, 90), 12.0f, 8, FColor::Red, false, DrawDuration);

	// Draw distance label at midpoint
	const FVector Midpoint = (AttackerLocation + PartnerLocation) * 0.5f + FVector(0, 0, 20);
	FString DistanceLabel = FString::Printf(TEXT("%.0f / %.0fu"), Distance, MaxDistance);
	DrawDebugString(World, Midpoint, DistanceLabel, nullptr, LineColor, DrawDuration, false);
}

void UDebugUtils::DrawSyncPoint(
	UWorld* World,
	const FVector& SyncLocation,
	float Progress,
	bool bAtSyncPoint,
	const FName& SyncPointName)
{
	if (!World || !CombatDebug::IsPairedAnimSyncDebugEnabled())
	{
		return;
	}

	const float DrawDuration = CombatDebug::GetDebugDrawDuration();

	// Color: Magenta base, brighter when closer to sync, pulsing white at sync point
	FColor SyncColor;
	if (bAtSyncPoint)
	{
		// Pulsing effect at sync point (simulate with alternating based on time)
		const float PulseValue = FMath::Abs(FMath::Sin(World->GetTimeSeconds() * 8.0f));
		const uint8 Brightness = static_cast<uint8>(200 + PulseValue * 55);
		SyncColor = FColor(Brightness, Brightness, Brightness);
	}
	else
	{
		// Progress-based color (darker magenta -> bright magenta)
		const uint8 Brightness = static_cast<uint8>(100 + Progress * 155);
		SyncColor = FColor(Brightness, 0, Brightness);
	}

	// Draw sync point sphere with size based on progress
	const float BaseRadius = 15.0f;
	const float MaxRadius = 35.0f;
	const float CurrentRadius = FMath::Lerp(BaseRadius, MaxRadius, Progress);
	DrawDebugSphere(World, SyncLocation, CurrentRadius, 12, SyncColor, false, DrawDuration);

	// Draw inner core
	DrawDebugSphere(World, SyncLocation, 8.0f, 8, FColor::White, false, DrawDuration);

	// Draw progress ring
	if (Progress > 0.0f && Progress < 1.0f)
	{
		// Arc showing progress toward sync point
		const int32 NumSegments = FMath::Max(3, static_cast<int32>(Progress * 16));
		for (int32 i = 0; i < NumSegments; ++i)
		{
			const float Angle1 = (static_cast<float>(i) / 16.0f) * 2.0f * PI;
			const float Angle2 = (static_cast<float>(i + 1) / 16.0f) * 2.0f * PI;
			const FVector Point1 = SyncLocation + FVector(FMath::Cos(Angle1), FMath::Sin(Angle1), 0) * (CurrentRadius + 10.0f);
			const FVector Point2 = SyncLocation + FVector(FMath::Cos(Angle2), FMath::Sin(Angle2), 0) * (CurrentRadius + 10.0f);
			DrawDebugLine(World, Point1, Point2, FColor::Cyan, false, DrawDuration, 0, 3.0f);
		}
	}

	// Label
	FString SyncLabel = FString::Printf(TEXT("%s: %.0f%%"),
		SyncPointName.IsNone() ? TEXT("Sync") : *SyncPointName.ToString(),
		Progress * 100.0f);
	if (bAtSyncPoint)
	{
		SyncLabel += TEXT(" [SYNC!]");
	}
	DrawDebugString(World, SyncLocation + FVector(0, 0, CurrentRadius + 25), SyncLabel, nullptr, SyncColor, DrawDuration, false);
}

void UDebugUtils::DrawVulnerabilityIndicator(
	UWorld* World,
	const FVector& TargetLocation,
	const FString& VulnerabilityReason,
	float HealthPercent)
{
	if (!World || !CombatDebug::IsPairedAnimVulnerabilityDebugEnabled())
	{
		return;
	}

	const float DrawDuration = CombatDebug::GetDebugDrawDuration();

	// Position above target head
	const FVector IndicatorPos = TargetLocation + FVector(0, 0, 200);

	// Draw exclamation-style indicator
	const FColor VulnColor = FColor::Red;

	// Draw diamond shape
	const float DiamondSize = 20.0f;
	DrawDebugLine(World, IndicatorPos + FVector(0, 0, DiamondSize), IndicatorPos + FVector(DiamondSize, 0, 0), VulnColor, false, DrawDuration, 0, 4.0f);
	DrawDebugLine(World, IndicatorPos + FVector(DiamondSize, 0, 0), IndicatorPos + FVector(0, 0, -DiamondSize), VulnColor, false, DrawDuration, 0, 4.0f);
	DrawDebugLine(World, IndicatorPos + FVector(0, 0, -DiamondSize), IndicatorPos + FVector(-DiamondSize, 0, 0), VulnColor, false, DrawDuration, 0, 4.0f);
	DrawDebugLine(World, IndicatorPos + FVector(-DiamondSize, 0, 0), IndicatorPos + FVector(0, 0, DiamondSize), VulnColor, false, DrawDuration, 0, 4.0f);

	// Inner exclamation point
	DrawDebugLine(World, IndicatorPos + FVector(0, 0, 10), IndicatorPos + FVector(0, 0, -5), FColor::White, false, DrawDuration, 0, 3.0f);
	DrawDebugPoint(World, IndicatorPos + FVector(0, 0, -12), 8.0f, FColor::White, false, DrawDuration);

	// Reason and health label
	FString Label = FString::Printf(TEXT("FINISHER: %s (%.0f%% HP)"), *VulnerabilityReason, HealthPercent * 100.0f);
	DrawDebugString(World, IndicatorPos + FVector(0, 0, 35), Label, nullptr, VulnColor, DrawDuration, false);

	// Pulsing circle around target
	const float PulseScale = 1.0f + 0.2f * FMath::Sin(World->GetTimeSeconds() * 6.0f);
	DrawDebugCircle(World, TargetLocation + FVector(0, 0, 50), 60.0f * PulseScale, 16, VulnColor, false, DrawDuration, 0, 2.0f,
		FVector::ForwardVector, FVector::RightVector);
}

void UDebugUtils::DrawFinisherRangeCircle(
	UWorld* World,
	const FVector& CenterLocation,
	float MaxRange,
	float CurrentDistance)
{
	if (!World || !CombatDebug::IsPairedAnimWarpDebugEnabled())
	{
		return;
	}

	const float DrawDuration = CombatDebug::GetDebugDrawDuration();

	// Color based on whether target is in range
	const bool bInRange = CurrentDistance <= MaxRange;
	const FColor RangeColor = bInRange ? FColor(0, 200, 0, 128) : FColor(200, 100, 0, 128);

	// Draw range circle on ground
	DrawDebugCircle(World, CenterLocation, MaxRange, 32, RangeColor, false, DrawDuration, 0, 2.0f,
		FVector::ForwardVector, FVector::RightVector);

	// Draw current distance arc if target exists
	if (CurrentDistance > 0.0f)
	{
		DrawDebugCircle(World, CenterLocation, CurrentDistance, 24, FColor::White, false, DrawDuration, 0, 1.5f,
			FVector::ForwardVector, FVector::RightVector);
	}

	// Label
	FString RangeLabel = FString::Printf(TEXT("Range: %.0f / %.0fu %s"),
		CurrentDistance, MaxRange, bInRange ? TEXT("[OK]") : TEXT("[OUT]"));
	DrawDebugString(World, CenterLocation + FVector(MaxRange + 20, 0, 30), RangeLabel, nullptr, RangeColor, DrawDuration, false);
}

void UDebugUtils::DrawWarpOffsetArrow(
	UWorld* World,
	const FVector& FromLocation,
	const FVector& Offset,
	const FString& Label)
{
	if (!World || !CombatDebug::IsPairedAnimWarpDebugEnabled())
	{
		return;
	}

	const float DrawDuration = CombatDebug::GetDebugDrawDuration();
	const FVector ToLocation = FromLocation + Offset;

	// Draw offset arrow
	DrawDebugDirectionalArrow(World, FromLocation, ToLocation, 15.0f, FColor::Orange, false, DrawDuration, 0, 2.5f);

	// Label with offset values
	FString OffsetLabel = FString::Printf(TEXT("%s: (%.0f, %.0f, %.0f)"),
		*Label, Offset.X, Offset.Y, Offset.Z);
	DrawDebugString(World, (FromLocation + ToLocation) * 0.5f + FVector(0, 0, 20), OffsetLabel, nullptr, FColor::Orange, DrawDuration, false);
}

void UDebugUtils::DrawAlignmentValidation(
	UWorld* World,
	const FVector& AttackerLocation,
	const FVector& VictimLocation,
	float ActualDistance,
	float MaxDistance,
	bool bIsAligned)
{
	if (!World || !CombatDebug::IsPairedAnimSyncDebugEnabled())
	{
		return;
	}

	const float DrawDuration = CombatDebug::GetDebugDrawDuration();

	// Color: Green = aligned, Yellow = marginal, Red = misaligned
	FColor AlignColor;
	FString AlignStatus;
	if (bIsAligned && ActualDistance < MaxDistance * 0.5f)
	{
		AlignColor = FColor::Green;
		AlignStatus = TEXT("ALIGNED");
	}
	else if (bIsAligned)
	{
		AlignColor = FColor::Yellow;
		AlignStatus = TEXT("MARGINAL");
	}
	else
	{
		AlignColor = FColor::Red;
		AlignStatus = TEXT("MISALIGNED");
	}

	// Draw line between attacker and victim with thickness based on alignment
	const float Thickness = bIsAligned ? 3.0f : 5.0f;
	DrawDebugLine(World, AttackerLocation, VictimLocation, AlignColor, false, DrawDuration, 0, Thickness);

	// Draw validation spheres at both ends
	DrawDebugSphere(World, AttackerLocation, 10.0f, 8, AlignColor, false, DrawDuration);
	DrawDebugSphere(World, VictimLocation, 10.0f, 8, AlignColor, false, DrawDuration);

	// Midpoint label
	const FVector Midpoint = (AttackerLocation + VictimLocation) * 0.5f;
	FString AlignLabel = FString::Printf(TEXT("%s: %.0f / %.0fu"),
		*AlignStatus, ActualDistance, MaxDistance);
	DrawDebugString(World, Midpoint + FVector(0, 0, 40), AlignLabel, nullptr, AlignColor, DrawDuration, false);
}
