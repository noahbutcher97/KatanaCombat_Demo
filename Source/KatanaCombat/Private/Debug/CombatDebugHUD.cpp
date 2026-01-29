// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/CombatDebugHUD.h"
#include "Debug/DebugConfig.h"
#include "Debug/DebugUtils.h"
#include "Utilities/CombatUtils.h"
#include "Core/CombatComponent.h"
#include "Data/AttackData.h"
#include "Characters/BaseCombatCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"

ACombatDebugHUD::ACombatDebugHUD()
{
	// Default configuration
	ScreenMargin = 20.0f;
	LineHeight = 18.0f;
	StatusFontScale = 1.2f;
	LabelFontScale = 1.0f;
	LabelOffset = FVector2D(10.0f, -10.0f);
}

void ACombatDebugHUD::DrawHUD()
{
	Super::DrawHUD();

	// Check if any debug is enabled
	if (!CombatDebug::IsDebugEnabled() && !CombatDebug::IsDirectionDebugEnabled())
	{
		return;
	}

	// Get player's combat character
	ABaseCombatCharacter* Character = GetPlayerCombatCharacter();
	if (!Character)
	{
		return;
	}

	// Generate debug data (single source of truth)
	CachedDebugData = GenerateDebugData(Character);
	if (!CachedDebugData.bIsValid)
	{
		return;
	}

	// Draw 3D arrows in world space
	if (CombatDebug::IsDirectionDebugEnabled())
	{
		Draw3DArrows(GetWorld(), CachedDebugData);
	}

	// Draw HUD elements
	if (CombatDebug::IsDebugEnabled())
	{
		DrawStatusPanel(CachedDebugData);
	}

	if (CombatDebug::IsDirectionDebugEnabled())
	{
		DrawArrowLabels(CachedDebugData);
		DrawArcLabel(CachedDebugData);
	}
}

FCombatDebugData ACombatDebugHUD::GenerateDebugData(ABaseCombatCharacter* Character)
{
	FCombatDebugData Data;
	Data.bIsValid = false;

	if (!Character)
	{
		return Data;
	}

	UCombatComponent* CombatComp = Character->GetCombatComponent();
	if (!CombatComp)
	{
		return Data;
	}

	Data.Character = Character;
	Data.bIsValid = true;

	// Get character transforms
	const FVector CharLocation = Character->GetActorLocation();
	const FVector ChestOffset(0.0f, 0.0f, 90.0f);
	const FVector ChestPos = CharLocation + ChestOffset;

	// Get rotations
	const FRotator CharRotation = Character->GetActorRotation();
	FRotator CameraRotation = FRotator::ZeroRotator;
	if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		CameraRotation = PC->GetControlRotation();
	}
	const FRotator FlatCameraRotation(0.0f, CameraRotation.Yaw, 0.0f);

	// Calculate yaw delta
	Data.CamCharYawDelta = UDebugUtils::CalculateYawDelta(FlatCameraRotation.Yaw, CharRotation.Yaw);

	// ========================================================================
	// STATUS INFO
	// ========================================================================
	Data.Phase = CombatComp->GetCurrentPhase();
	Data.QueuedCount = CombatComp->GetQueueSize();
	Data.PendingCount = CombatComp->GetPendingActionCount();
	Data.bIsHolding = CombatComp->IsHolding();
	Data.HeldInputType = CombatComp->GetHoldInputType();
	Data.HoldDuration = CombatComp->GetHoldDuration();
	Data.bHoldCompleted = CombatComp->HoldState.CurrentHold.bCompleted;
	Data.bMovementDisabled = CombatComp->IsAttacking(); // Approximate: attacking = movement restricted

	if (const UAttackData* CurrentAttack = CombatComp->GetCurrentAttack())
	{
		Data.CurrentAttackName = CurrentAttack->GetName();
	}

	// ========================================================================
	// DIRECTIONAL INPUT - Context-aware sampling
	// ========================================================================
	// During hold: Sample LIVE input (preview where attack will go)
	// During attack: Show captured direction
	// Idle: No direction arrow (not useful)

	bool bShowAttackArrow = false;
	bool bIsLivePreview = false;

	if (Data.bIsHolding)
	{
		// LIVE PREVIEW: Sample current movement input during hold
		// This lets player see where their attack will go before releasing
		// Uses GetLastMovementInput() which works with Enhanced Input System
		FVector2D MovementInput = Character->GetLastMovementInput();

		// Convert to character-relative direction
		if (MovementInput.Size() > 0.2f)
		{
			Data.InputDirection = UCombatUtils::VectorToCharacterRelativeDirection(
				MovementInput,
				CameraRotation,
				Character,
				CharRotation,
				0.2f);
			bShowAttackArrow = true;
			bIsLivePreview = true;
		}
	}
	else if (CombatComp->IsAttacking())
	{
		// DURING ATTACK: Show the captured direction that was used
		Data.InputDirection = CombatComp->LastDirectionalInput;
		bShowAttackArrow = (Data.InputDirection != EInputDirection::None);
	}
	// else: Idle - don't show attack arrow (no useful info)

	Data.AttackDirection = UDebugUtils::InputDirectionToAttackDirection(Data.InputDirection);

	// ========================================================================
	// ARROW DATA
	// ========================================================================

	// Arrow 1: Camera Forward (Blue)
	{
		FDebugArrowData Arrow;
		const FVector CameraForward = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::X);
		Arrow.WorldStart = ChestPos;
		Arrow.WorldEnd = ChestPos + (CameraForward * 180.0f);
		Arrow.Color = FColor(0, 100, 255);
		Arrow.Label = FString::Printf(TEXT("CAMERA\n%s"),
			*UDebugUtils::FormatRotationDebug(FlatCameraRotation));
		Arrow.Thickness = 2.5f;
		Arrow.ArrowSize = 20.0f;
		Arrow.LabelPriority = 1;
		Data.Arrows.Add(Arrow);
	}

	// Arrow 2: Character Forward (Green)
	{
		FDebugArrowData Arrow;
		const FVector CharForward = FRotationMatrix(CharRotation).GetScaledAxis(EAxis::X);
		Arrow.WorldStart = ChestPos;
		Arrow.WorldEnd = ChestPos + (CharForward * 160.0f);
		Arrow.Color = FColor(0, 255, 0);
		Arrow.Label = FString::Printf(TEXT("CHAR\n%s"),
			*UDebugUtils::FormatRotationDebug(CharRotation));
		Arrow.Thickness = 3.0f;
		Arrow.ArrowSize = 25.0f;
		Arrow.LabelPriority = 2;
		Data.Arrows.Add(Arrow);
	}

	// Arrow 3: Attack Direction (Magenta) - contextual display
	// - During hold: Live preview (dashed, shows current stick direction)
	// - During attack: Captured direction (solid)
	// - Idle: Hidden (no useful info)
	if (bShowAttackArrow)
	{
		FDebugArrowData Arrow;

		// Calculate attack direction vector
		FVector AttackDirVec = UCombatUtils::InputDirectionToWorldVector(Data.InputDirection, Character);
		AttackDirVec.Normalize();

		Arrow.WorldStart = ChestPos;
		Arrow.WorldEnd = ChestPos + (AttackDirVec * 160.0f);
		Arrow.Color = bIsLivePreview ? FColor(255, 100, 255) : FColor(255, 0, 255); // Lighter magenta for preview
		Arrow.Label = FString::Printf(TEXT("%s\n%s"),
			bIsLivePreview ? TEXT("PREVIEW") : TEXT("ATTACK"),
			*UDebugUtils::FormatAttackDirectionDebug(Data.AttackDirection));
		Arrow.Thickness = 4.0f;
		Arrow.ArrowSize = 30.0f;
		Arrow.bIsDashed = bIsLivePreview; // Dashed during preview, solid during attack
		Arrow.LabelPriority = 3;
		Data.Arrows.Add(Arrow);
	}

	// ========================================================================
	// ARC DATA (Camera-Character offset)
	// ========================================================================
	const float AbsYawDelta = FMath::Abs(Data.CamCharYawDelta);
	if (AbsYawDelta > 5.0f)
	{
		const float ArcRadius = 100.0f;
		const int32 NumSegments = FMath::Max(3, FMath::CeilToInt(AbsYawDelta / 10.0f));
		const float StartAngle = CharRotation.Yaw * (PI / 180.0f);
		const float AngleStep = (Data.CamCharYawDelta * (PI / 180.0f)) / NumSegments;

		for (int32 i = 0; i <= NumSegments; ++i)
		{
			const float Angle = StartAngle + (AngleStep * i);
			const FVector Point = ChestPos + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * ArcRadius;
			Data.ArcPoints.Add(Point);
		}

		// Arc label position
		const float MidAngle = StartAngle + ((Data.CamCharYawDelta * (PI / 180.0f)) / 2.0f);
		Data.ArcLabelPosition = ChestPos +
			FVector(FMath::Cos(MidAngle), FMath::Sin(MidAngle), 0) * (ArcRadius * 1.3f) +
			FVector(0, 0, 15.0f);
		Data.ArcLabel = FString::Printf(TEXT("CAM-CHAR %.0f"), AbsYawDelta);
	}

	return Data;
}

void ACombatDebugHUD::Draw3DArrows(UWorld* World, const FCombatDebugData& Data)
{
	if (!World)
	{
		return;
	}

	const float Duration = 0.0f; // Single frame

	// Draw arrows
	for (const FDebugArrowData& Arrow : Data.Arrows)
	{
		if (Arrow.bIsDashed)
		{
			// Draw dashed arrow
			const FVector Dir = (Arrow.WorldEnd - Arrow.WorldStart).GetSafeNormal();
			const float Length = (Arrow.WorldEnd - Arrow.WorldStart).Size();
			const int32 NumSegments = 7;
			const float SegmentLength = Length / NumSegments;

			for (int32 i = 0; i < NumSegments; i += 2)
			{
				FVector SegStart = Arrow.WorldStart + (Dir * SegmentLength * i);
				FVector SegEnd = Arrow.WorldStart + (Dir * SegmentLength * FMath::Min(i + 1, NumSegments));
				DrawDebugLine(World, SegStart, SegEnd, Arrow.Color, false, Duration, 0, Arrow.Thickness);
			}
			// Draw arrowhead
			DrawDebugDirectionalArrow(World,
				Arrow.WorldEnd - (Dir * Arrow.ArrowSize), Arrow.WorldEnd,
				Arrow.ArrowSize, Arrow.Color, false, Duration, 0, Arrow.Thickness);
		}
		else
		{
			DrawDebugDirectionalArrow(World,
				Arrow.WorldStart, Arrow.WorldEnd,
				Arrow.ArrowSize, Arrow.Color, false, Duration, 0, Arrow.Thickness);
		}
	}

	// Draw arc
	for (int32 i = 0; i < Data.ArcPoints.Num() - 1; ++i)
	{
		DrawDebugLine(World, Data.ArcPoints[i], Data.ArcPoints[i + 1],
			FColor(255, 255, 255, 180), false, Duration, 0, 1.5f);
	}
}

void ACombatDebugHUD::DrawStatusPanel(const FCombatDebugData& Data)
{
	if (!Canvas)
	{
		return;
	}

	float X = ScreenMargin;
	float Y = ScreenMargin;

	// Phase
	const FColor PhaseColor = GetPhaseColor(Data.Phase);
	const FString PhaseText = FString::Printf(TEXT("Phase: %s"),
		*UEnum::GetValueAsString(Data.Phase));
	Canvas->SetDrawColor(PhaseColor);
	Canvas->DrawText(GEngine->GetSmallFont(), PhaseText, X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	// Queue
	const FString QueueText = FString::Printf(TEXT("Queue: %d pending | %d total"),
		Data.PendingCount, Data.QueuedCount);
	Canvas->SetDrawColor(FColor::Cyan);
	Canvas->DrawText(GEngine->GetSmallFont(), QueueText, X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	// Hold state (if holding)
	if (Data.bIsHolding)
	{
		const FString HoldText = FString::Printf(TEXT("HOLDING: %s (%.2fs) [%s]"),
			*UEnum::GetValueAsString(Data.HeldInputType),
			Data.HoldDuration,
			Data.bHoldCompleted ? TEXT("COMPLETE") : TEXT("..."));
		Canvas->SetDrawColor(FColor::Yellow);
		Canvas->DrawText(GEngine->GetSmallFont(), HoldText, X, Y, StatusFontScale, StatusFontScale);
		Y += LineHeight;
	}

	// Current attack
	if (!Data.CurrentAttackName.IsEmpty())
	{
		const FString AttackText = FString::Printf(TEXT("Attack: %s"), *Data.CurrentAttackName);
		Canvas->SetDrawColor(FColor::Orange);
		Canvas->DrawText(GEngine->GetSmallFont(), AttackText, X, Y, StatusFontScale, StatusFontScale);
		Y += LineHeight;
	}

	// Direction info
	if (Data.InputDirection != EInputDirection::None)
	{
		const FString DirText = FString::Printf(TEXT("Direction: %s -> %s"),
			*UDebugUtils::FormatInputDirectionDebug(Data.InputDirection),
			*UDebugUtils::FormatAttackDirectionDebug(Data.AttackDirection));
		Canvas->SetDrawColor(FColor::Magenta);
		Canvas->DrawText(GEngine->GetSmallFont(), DirText, X, Y, StatusFontScale, StatusFontScale);
		Y += LineHeight;
	}

	// Movement state
	const FString MovementText = FString::Printf(TEXT("Movement: %s"),
		Data.bMovementDisabled ? TEXT("DISABLED") : TEXT("Enabled"));
	Canvas->SetDrawColor(Data.bMovementDisabled ? FColor::Red : FColor::Green);
	Canvas->DrawText(GEngine->GetSmallFont(), MovementText, X, Y, StatusFontScale, StatusFontScale);
}

void ACombatDebugHUD::DrawArrowLabels(const FCombatDebugData& Data)
{
	if (!Canvas)
	{
		return;
	}

	for (const FDebugArrowData& Arrow : Data.Arrows)
	{
		FVector2D ScreenPos;
		if (ProjectToScreen(Arrow.WorldEnd, ScreenPos))
		{
			// Apply offset
			ScreenPos += LabelOffset;

			// Clamp to viewport
			ScreenPos = ClampToViewport(ScreenPos, ScreenMargin);

			// Draw label
			Canvas->SetDrawColor(Arrow.Color);
			Canvas->DrawText(GEngine->GetTinyFont(), Arrow.Label,
				ScreenPos.X, ScreenPos.Y, LabelFontScale, LabelFontScale);
		}
	}
}

void ACombatDebugHUD::DrawArcLabel(const FCombatDebugData& Data)
{
	if (!Canvas || Data.ArcLabel.IsEmpty())
	{
		return;
	}

	FVector2D ScreenPos;
	if (ProjectToScreen(Data.ArcLabelPosition, ScreenPos))
	{
		Canvas->SetDrawColor(FColor::Cyan);
		Canvas->DrawText(GEngine->GetTinyFont(), Data.ArcLabel,
			ScreenPos.X, ScreenPos.Y, LabelFontScale, LabelFontScale);
	}
}

bool ACombatDebugHUD::ProjectToScreen(const FVector& WorldPos, FVector2D& OutScreenPos) const
{
	if (!PlayerOwner)
	{
		return false;
	}

	return PlayerOwner->ProjectWorldLocationToScreen(WorldPos, OutScreenPos, true);
}

FVector2D ACombatDebugHUD::ClampToViewport(const FVector2D& ScreenPos, float Margin) const
{
	if (!Canvas)
	{
		return ScreenPos;
	}

	const float MaxX = Canvas->SizeX - Margin;
	const float MaxY = Canvas->SizeY - Margin;

	return FVector2D(
		FMath::Clamp(ScreenPos.X, Margin, MaxX),
		FMath::Clamp(ScreenPos.Y, Margin, MaxY)
	);
}

FColor ACombatDebugHUD::GetPhaseColor(EAttackPhase Phase)
{
	switch (Phase)
	{
		case EAttackPhase::Windup:   return FColor::Orange;
		case EAttackPhase::Active:   return FColor::Red;
		case EAttackPhase::Recovery: return FColor::Yellow;
		default:                     return FColor::White;
	}
}

ABaseCombatCharacter* ACombatDebugHUD::GetPlayerCombatCharacter() const
{
	if (!PlayerOwner)
	{
		return nullptr;
	}

	return Cast<ABaseCombatCharacter>(PlayerOwner->GetPawn());
}
