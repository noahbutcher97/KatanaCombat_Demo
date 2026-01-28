// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/DirectionDebugLibrary.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

// ============================================================================
// CORE FORMATTING HELPERS
// ============================================================================

FString UDirectionDebugLibrary::YawToCardinalDirection(float Yaw)
{
	// Normalize to -180 to +180 range
	float NormalizedYaw = NormalizeYaw(Yaw);

	// Convert to 0-360 range for easier compass logic
	if (NormalizedYaw < 0.0f)
	{
		NormalizedYaw += 360.0f;
	}

	// Map to 8 compass directions (45° each)
	// N: 337.5° to 22.5° (0° ± 22.5°)
	if (NormalizedYaw >= 337.5f || NormalizedYaw < 22.5f)
		return TEXT("N");
	// NE: 22.5° to 67.5°
	else if (NormalizedYaw >= 22.5f && NormalizedYaw < 67.5f)
		return TEXT("NE");
	// E: 67.5° to 112.5°
	else if (NormalizedYaw >= 67.5f && NormalizedYaw < 112.5f)
		return TEXT("E");
	// SE: 112.5° to 157.5°
	else if (NormalizedYaw >= 112.5f && NormalizedYaw < 157.5f)
		return TEXT("SE");
	// S: 157.5° to 202.5°
	else if (NormalizedYaw >= 157.5f && NormalizedYaw < 202.5f)
		return TEXT("S");
	// SW: 202.5° to 247.5°
	else if (NormalizedYaw >= 202.5f && NormalizedYaw < 247.5f)
		return TEXT("SW");
	// W: 247.5° to 292.5°
	else if (NormalizedYaw >= 247.5f && NormalizedYaw < 292.5f)
		return TEXT("W");
	// NW: 292.5° to 337.5°
	else
		return TEXT("NW");
}

FString UDirectionDebugLibrary::FormatRotationDebug(const FRotator& Rotation)
{
	return FString::Printf(TEXT("Yaw=%.1f° (%s)"),
		Rotation.Yaw,
		*YawToCardinalDirection(Rotation.Yaw));
}

FString UDirectionDebugLibrary::FormatVector2DDebug(const FVector2D& Vec)
{
	return FString::Printf(TEXT("(X=%.2f, Y=%.2f) magnitude=%.2f"),
		Vec.X,
		Vec.Y,
		Vec.Size());
}

FString UDirectionDebugLibrary::FormatInputDirectionDebug(EInputDirection Direction)
{
	switch (Direction)
	{
		case EInputDirection::None:
			return TEXT("None");
		case EInputDirection::Forward:
			return TEXT("Forward");
		case EInputDirection::ForwardRight:
			return TEXT("ForwardRight");
		case EInputDirection::Right:
			return TEXT("Right");
		case EInputDirection::BackwardRight:
			return TEXT("BackwardRight");
		case EInputDirection::Backward:
			return TEXT("Backward");
		case EInputDirection::BackwardLeft:
			return TEXT("BackwardLeft");
		case EInputDirection::Left:
			return TEXT("Left");
		case EInputDirection::ForwardLeft:
			return TEXT("ForwardLeft");
		default:
			return TEXT("Unknown");
	}
}

FString UDirectionDebugLibrary::FormatAttackDirectionDebug(EAttackDirection Direction)
{
	switch (Direction)
	{
		case EAttackDirection::None:
			return TEXT("None");
		case EAttackDirection::Forward:
			return TEXT("Forward");
		case EAttackDirection::Backward:
			return TEXT("Backward");
		case EAttackDirection::Left:
			return TEXT("Left");
		case EAttackDirection::Right:
			return TEXT("Right");
		default:
			return TEXT("Unknown");
	}
}

// ============================================================================
// CALCULATION HELPERS
// ============================================================================

float UDirectionDebugLibrary::CalculateYawDelta(float FromYaw, float ToYaw)
{
	// Calculate raw delta
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

FRotator UDirectionDebugLibrary::GetMeshRotationOffset(ACharacter* Character)
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
// DEBUG VISUALIZATION
// ============================================================================

void UDirectionDebugLibrary::DrawDirectionTransformDebug(
	UWorld* World,
	ACharacter* Character,
	const FVector& CharacterLocation,
	const FRotator& CameraRotation,
	const FRotator& CharacterRotation,
	const FVector2D& CameraRelativeInput,
	const FVector& WorldInput,
	const FVector& CharacterRelativeVec,
	EInputDirection CharacterRelativeDirection,
	EAttackDirection AttackDirection,
	bool bIsHoldActive)
{
	if (!World || !Character)
	{
		return;
	}

	
	const FRotator FlatCameraRotation = FRotator(0.0f, CameraRotation.Yaw, 0.0f);
	// Configuration
	const FVector ChestOffset(0.0f, 0.0f, 90.0f); // Chest height
	const float DebugDuration = 0.0f; // Single frame (updates each tick from character)
	const float YawDelta = CalculateYawDelta(FlatCameraRotation.Yaw, CharacterRotation.Yaw);

	// CRITICAL FIX (2025-11-20): Detect mesh offset for debug display
	FRotator MeshOffset = GetMeshRotationOffset(Character);
	const bool bHasMeshOffset = !MeshOffset.IsNearlyZero(0.1f);

	// Priority-based colors
	const FColor ColorCamera(0, 100, 255);        // Blue (medium)
	const FColor ColorInputContinuous(255, 255, 0); // Yellow (medium) - solid
	const FColor ColorInputHold(255, 215, 0);     // Gold (medium) - dashed
	const FColor ColorCharRelative(255, 165, 0);  // Orange (medium)
	const FColor ColorAttack(255, 0, 255);        // Magenta (bright/thick)
	const FColor ColorCharacter(0, 255, 0);       // Green (bright)

	// ========================================================================
	// ARROW 1: CAMERA (Blue, #1)
	// ========================================================================
	
	const FVector CameraForward = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::X);
	const float CameraArrowLength = 180.0f;
	const FVector CameraArrowEnd = CharacterLocation + ChestOffset + (CameraForward * CameraArrowLength);

	DrawDebugDirectionalArrow(World,
		CharacterLocation + ChestOffset,
		CameraArrowEnd,
		20.0f, ColorCamera, false, DebugDuration, 0, 2.5f);
	DrawDebugString(World, CameraArrowEnd + FVector(0, 0, 25),
		FString::Printf(TEXT("1.CAMERA\n%s"), *FormatRotationDebug(FlatCameraRotation)),
		Character, FColor::White, DebugDuration, true, 1.2f);

	// ========================================================================
	// ARROW 2: INPUT (Yellow solid OR Gold dashed, #2)
	// ========================================================================
	const float InputArrowLength = 140.0f;
	const FVector InputArrowEnd = CharacterLocation + ChestOffset + (WorldInput * InputArrowLength);
	const FColor InputColor = bIsHoldActive ? ColorInputHold : ColorInputContinuous;
	const FString InputLabel = bIsHoldActive ? TEXT("2.INPUT (hold-release)") : TEXT("2.INPUT (continuous)");

	if (bIsHoldActive)
	{
		// Dashed arrow for hold-release input
		const int32 NumSegments = 7;
		const float SegmentLength = InputArrowLength / NumSegments;
		for (int32 i = 0; i < NumSegments; i += 2) // Draw every other segment
		{
			FVector SegmentStart = CharacterLocation + ChestOffset + (WorldInput * SegmentLength * i);
			FVector SegmentEnd = CharacterLocation + ChestOffset + (WorldInput * SegmentLength * (i + 1));
			DrawDebugDirectionalArrow(World,
				SegmentStart, SegmentEnd,
				10.0f, InputColor, false, DebugDuration, 0, 2.5f);
		}
	}
	else
	{
		// Solid arrow for continuous input
		DrawDebugDirectionalArrow(World,
			CharacterLocation + ChestOffset,
			InputArrowEnd,
			20.0f, InputColor, false, DebugDuration, 0, 2.5f);
	}

	DrawDebugString(World, InputArrowEnd + FVector(0, 0, 25),
		InputLabel,
		Character, FColor::White, DebugDuration, true, 1.2f);

	// ========================================================================
	// ARROW 3: CHARACTER FORWARD (Green, implicit #3 for spatial reference)
	// ========================================================================
	const FVector CharacterForward = FRotationMatrix(CharacterRotation).GetScaledAxis(EAxis::X);
	const float CharForwardLength = 160.0f;
	const FVector CharForwardEnd = CharacterLocation + ChestOffset + (CharacterForward * CharForwardLength);

	DrawDebugDirectionalArrow(World,
		CharacterLocation + ChestOffset,
		CharForwardEnd,
		25.0f, ColorCharacter, false, DebugDuration, 0, 3.0f);
	DrawDebugString(World, CharForwardEnd + FVector(0, 0, 25),
		FString::Printf(TEXT("CHAR FORWARD\n%s"), *FormatRotationDebug(CharacterRotation)),
		Character, FColor::White, DebugDuration, true, 1.2f);

	// ========================================================================
	// ARROW 4: CHARACTER-RELATIVE (Orange, #4)
	// ========================================================================
	const float CharRelLength = 120.0f;
	const FVector CharRelEnd = CharacterLocation + ChestOffset + (CharacterRelativeVec * CharRelLength);

	DrawDebugDirectionalArrow(World,
		CharacterLocation + ChestOffset,
		CharRelEnd,
		20.0f, ColorCharRelative, false, DebugDuration, 0, 2.5f);
	DrawDebugString(World, CharRelEnd + FVector(0, 0, 25),
		TEXT("4.CHAR-REL"),
		Character, FColor::White, DebugDuration, true, 1.2f);

	// ========================================================================
	// ARROW 5: FINAL ATTACK (Magenta, #5, BRIGHT/THICK)
	// ========================================================================
	FVector FinalDirectionVec = CharacterRelativeVec;
	FinalDirectionVec.Normalize();
	const float AttackArrowLength = 160.0f;
	const FVector AttackArrowEnd = CharacterLocation + ChestOffset + (FinalDirectionVec * AttackArrowLength);

	DrawDebugDirectionalArrow(World,
		CharacterLocation + ChestOffset,
		AttackArrowEnd,
		30.0f, ColorAttack, false, DebugDuration, 0, 4.0f);
	DrawDebugString(World, AttackArrowEnd + FVector(0, 0, 30),
		FString::Printf(TEXT("5.ATTACK: %s"),
			*FormatAttackDirectionDebug(AttackDirection)),
		Character, ColorAttack, DebugDuration, true, 1.5f);

	// ========================================================================
	// ANGULAR ARC: Camera-Character Offset (using line segments)
	// ========================================================================
	const float ArcRadius = 100.0f;
	const float AbsYawDelta = FMath::Abs(YawDelta);
	if (AbsYawDelta > 5.0f) // Only draw if offset is significant
	{
		const int32 NumArcSegments = FMath::Max(3, FMath::CeilToInt(AbsYawDelta / 10.0f)); // 1 segment per 10 degrees
		const float StartAngle = CharacterRotation.Yaw * (PI / 180.0f);
		const float AngleStep = (YawDelta * (PI / 180.0f)) / NumArcSegments;

		// Draw arc as connected line segments
		for (int32 i = 0; i < NumArcSegments; ++i)
		{
			const float Angle1 = StartAngle + (AngleStep * i);
			const float Angle2 = StartAngle + (AngleStep * (i + 1));

			const FVector Point1 = CharacterLocation + ChestOffset +
				FVector(FMath::Cos(Angle1), FMath::Sin(Angle1), 0) * ArcRadius;
			const FVector Point2 = CharacterLocation + ChestOffset +
				FVector(FMath::Cos(Angle2), FMath::Sin(Angle2), 0) * ArcRadius;

			DrawDebugLine(World,
				Point1, Point2,
				FColor(255, 255, 255, 128), // White semi-transparent
				false,
				DebugDuration,
				0,
				1.5f);
		}

		// Arc label OUTSIDE the arc (at 130% radius) with Z offset for visibility
		// Position at midpoint of the arc, lifted above the arc plane
		const float MidAngle = StartAngle + ((YawDelta * (PI / 180.0f)) / 2.0f);
		const float LabelRadius = ArcRadius * 1.3f; // Outside the arc
		const float LabelZOffset = 15.0f; // Lift above arc plane
		const FVector ArcLabelPoint = CharacterLocation + ChestOffset +
			FVector(FMath::Cos(MidAngle), FMath::Sin(MidAngle), 0) * LabelRadius +
			FVector(0, 0, LabelZOffset);

		// Draw connecting line from arc to label for clarity
		const FVector ArcMidPoint = CharacterLocation + ChestOffset +
			FVector(FMath::Cos(MidAngle), FMath::Sin(MidAngle), 0) * ArcRadius;
		DrawDebugLine(World,
			ArcMidPoint, ArcLabelPoint,
			FColor(200, 200, 200, 180), // Light gray
			false, DebugDuration, 0, 1.0f);

		// Label with cyan color for better visibility against other elements
		DrawDebugString(World, ArcLabelPoint,
			FString::Printf(TEXT("CAM-CHAR Δ%.0f°"), AbsYawDelta),
			nullptr, FColor::Cyan, DebugDuration, true, 1.2f);
	}

	// ========================================================================
	// SUMMARY PANEL - Always visible above character's head
	// Shows key info regardless of facing direction
	// ========================================================================
	const float SummaryBaseZ = 150.0f; // Well above chest height
	const float SummaryLineSpacing = 18.0f;
	int32 SummaryLine = 0;

	// Resolved Direction (most important - large and bright)
	DrawDebugString(World, CharacterLocation + FVector(0, 0, SummaryBaseZ + SummaryLineSpacing * SummaryLine++),
		FString::Printf(TEXT("► ATTACK: %s"), *FormatAttackDirectionDebug(AttackDirection)),
		nullptr, FColor::Magenta, DebugDuration, true, 1.6f);

	// Input Direction
	DrawDebugString(World, CharacterLocation + FVector(0, 0, SummaryBaseZ + SummaryLineSpacing * SummaryLine++),
		FString::Printf(TEXT("  Input: %s"), *FormatInputDirectionDebug(CharacterRelativeDirection)),
		nullptr, FColor::Orange, DebugDuration, true, 1.2f);

	// Camera-Character Delta (if significant)
	if (AbsYawDelta > 5.0f)
	{
		DrawDebugString(World, CharacterLocation + FVector(0, 0, SummaryBaseZ + SummaryLineSpacing * SummaryLine++),
			FString::Printf(TEXT("  Cam-Char: Δ%.0f°"), YawDelta),
			nullptr, FColor::Cyan, DebugDuration, true, 1.2f);
	}

	// Hold state indicator
	if (bIsHoldActive)
	{
		DrawDebugString(World, CharacterLocation + FVector(0, 0, SummaryBaseZ + SummaryLineSpacing * SummaryLine++),
			TEXT("  [HOLD ACTIVE]"),
			nullptr, ColorInputHold, DebugDuration, true, 1.2f);
	}

	// Mesh offset indicator
	if (bHasMeshOffset)
	{
		DrawDebugString(World, CharacterLocation + FVector(0, 0, SummaryBaseZ + SummaryLineSpacing * SummaryLine++),
			FString::Printf(TEXT("  Mesh Offset: %.0f°"), MeshOffset.Yaw),
			nullptr, FColor::Green, DebugDuration, true, 1.0f);
	}
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

float UDirectionDebugLibrary::NormalizeYaw(float Yaw)
{
	// Normalize to -180 to +180 range
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
