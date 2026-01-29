// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/CombatUtils.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

// ============================================================================
// DIRECTION CONVERSION
// ============================================================================

EAttackDirection UCombatUtils::InputToAttackDirection(EInputDirection InputDir)
{
	switch (InputDir)
	{
		case EInputDirection::Forward:
		case EInputDirection::ForwardRight:
		case EInputDirection::ForwardLeft:
			return EAttackDirection::Forward;

		case EInputDirection::Backward:
		case EInputDirection::BackwardRight:
		case EInputDirection::BackwardLeft:
			return EAttackDirection::Backward;

		case EInputDirection::Right:
			return EAttackDirection::Right;

		case EInputDirection::Left:
			return EAttackDirection::Left;

		case EInputDirection::None:
		default:
			return EAttackDirection::None;
	}
}

EInputDirection UCombatUtils::VectorToInputDirection(const FVector2D& InputVector, float DeadZone)
{
	if (InputVector.Size() < DeadZone)
	{
		return EInputDirection::None;
	}

	// Calculate angle in degrees
	// Unreal 2D character space convention: X=Forward/Backward, Y=Right/Left
	// Atan2(X, Y) gives: 0=Right, 90=Forward, 180=Left, 270=Backward
	float Angle = FMath::Atan2(InputVector.X, InputVector.Y) * (180.0f / PI);

	// Normalize to 0-360 range
	if (Angle < 0.0f)
	{
		Angle += 360.0f;
	}

	// Map angle to 8-way direction (45-degree sectors)
	if (Angle >= 337.5f || Angle < 22.5f)
		return EInputDirection::Right;
	else if (Angle >= 22.5f && Angle < 67.5f)
		return EInputDirection::ForwardRight;
	else if (Angle >= 67.5f && Angle < 112.5f)
		return EInputDirection::Forward;
	else if (Angle >= 112.5f && Angle < 157.5f)
		return EInputDirection::ForwardLeft;
	else if (Angle >= 157.5f && Angle < 202.5f)
		return EInputDirection::Left;
	else if (Angle >= 202.5f && Angle < 247.5f)
		return EInputDirection::BackwardLeft;
	else if (Angle >= 247.5f && Angle < 292.5f)
		return EInputDirection::Backward;
	else // 292.5f - 337.5f
		return EInputDirection::BackwardRight;
}

FVector UCombatUtils::InputDirectionToWorldVector(EInputDirection Direction, const ACharacter* Character)
{
	if (!Character || Direction == EInputDirection::None)
	{
		return FVector::ZeroVector;
	}

	const FRotator CharRotation = Character->GetActorRotation();
	const FVector Forward = CharRotation.Vector();
	const FVector Right = FRotationMatrix(CharRotation).GetScaledAxis(EAxis::Y);

	switch (Direction)
	{
		case EInputDirection::Forward:
			return Forward;
		case EInputDirection::ForwardRight:
			return (Forward + Right).GetSafeNormal();
		case EInputDirection::Right:
			return Right;
		case EInputDirection::BackwardRight:
			return (-Forward + Right).GetSafeNormal();
		case EInputDirection::Backward:
			return -Forward;
		case EInputDirection::BackwardLeft:
			return (-Forward - Right).GetSafeNormal();
		case EInputDirection::Left:
			return -Right;
		case EInputDirection::ForwardLeft:
			return (Forward - Right).GetSafeNormal();
		default:
			return Forward;
	}
}

FRotator UCombatUtils::InputDirectionToWorldRotation(EInputDirection Direction, const ACharacter* Character)
{
	FVector WorldDir = InputDirectionToWorldVector(Direction, Character);
	if (WorldDir.IsNearlyZero())
	{
		return Character ? Character->GetActorRotation() : FRotator::ZeroRotator;
	}
	return WorldDir.Rotation();
}

// ============================================================================
// COORDINATE TRANSFORMS
// ============================================================================

EInputDirection UCombatUtils::VectorToCharacterRelativeDirection(
	const FVector2D& CameraRelativeInput,
	const FRotator& CameraRotation,
	const ACharacter* Character,
	const FRotator& CharacterRotation,
	float DeadZone)
{
	// Early exit for zero input
	if (CameraRelativeInput.Size() < DeadZone)
	{
		return EInputDirection::None;
	}

	// STEP 0: Get mesh-compensated character rotation
	FRotator MeshCompensatedRotation = CharacterRotation;
	if (Character)
	{
		MeshCompensatedRotation = GetMeshCompensatedRotation(Character);
	}

	// STEP 1: Convert camera-relative 2D input to world space 3D vector
	// Flatten camera rotation to yaw-only to prevent pitch/roll corruption
	const FRotator FlatCameraRotation = FRotator(0.0f, CameraRotation.Yaw, 0.0f);
	const FVector CameraForward = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::X);
	const FVector CameraRight = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::Y);

	// Combine: InputX * CameraRight + InputY * CameraForward
	FVector WorldInput = (CameraRight * CameraRelativeInput.X) + (CameraForward * CameraRelativeInput.Y);
	WorldInput.Z = 0.0f;
	WorldInput.Normalize();

	// STEP 2: Convert world space to character-relative space
	const FRotator InverseCharacterYaw(0.0f, -MeshCompensatedRotation.Yaw, 0.0f);
	const FVector CharacterRelative = InverseCharacterYaw.RotateVector(WorldInput);

	// STEP 3: Project to 2D
	const FVector2D CharacterRelative2D(CharacterRelative.X, CharacterRelative.Y);

	// STEP 4: Convert to 8-way direction
	return VectorToInputDirection(CharacterRelative2D, DeadZone);
}

// ============================================================================
// ROTATION HELPERS
// ============================================================================

FRotator UCombatUtils::GetMeshCompensatedRotation(const ACharacter* Character, bool bIncludeMeshOffset)
{
	if (!Character)
	{
		return FRotator::ZeroRotator;
	}

	FRotator ActorRotation = Character->GetActorRotation();

	// Default: Return actor rotation directly
	if (!bIncludeMeshOffset)
	{
		return ActorRotation;
	}

	// Optional: Include mesh offset (rare case)
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (!Mesh)
	{
		return ActorRotation;
	}

	FRotator MeshRelativeRotation = Mesh->GetRelativeRotation();
	FRotator TrueRotation = ActorRotation;
	TrueRotation.Yaw += MeshRelativeRotation.Yaw;

	// Normalize yaw to -180 to +180 range
	TrueRotation.Yaw = FMath::Fmod(TrueRotation.Yaw + 180.0f, 360.0f) - 180.0f;

	return TrueRotation;
}

FRotator UCombatUtils::GetMeshRotationOffset(const ACharacter* Character)
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

	return Mesh->GetRelativeRotation();
}
