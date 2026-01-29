// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/DebugUtils.h"
#include "Debug/DebugConfig.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"

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
