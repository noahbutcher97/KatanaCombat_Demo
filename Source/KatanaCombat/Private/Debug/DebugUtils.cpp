// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/DebugUtils.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

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
