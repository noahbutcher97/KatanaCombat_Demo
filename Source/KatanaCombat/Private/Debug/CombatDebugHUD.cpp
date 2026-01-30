// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/CombatDebugHUD.h"
#include "Debug/DebugConfig.h"
#include "Debug/DebugUtils.h"
#include "Utilities/CombatUtils.h"
#include "Core/CombatComponent.h"
#include "Core/WeaponComponent.h"
#include "Core/TargetingComponent.h"
#include "Core/HitReactionComponent.h"
#include "Data/AttackData.h"
#include "Data/TargetingSettings.h"
#include "Data/WeaponData.h"
#include "Data/PairedAnimationTypes.h"
#include "Characters/BaseCombatCharacter.h"
#include "Interfaces/DamageableInterface.h"
#include "Interfaces/CombatInterface.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
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
	const bool bAnyDebugEnabled = CombatDebug::IsDebugEnabled() ||
		CombatDebug::IsDirectionDebugEnabled() ||
		CombatDebug::IsPairedAnimDebugEnabled();

	if (!bAnyDebugEnabled)
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

	// ========================================================================
	// PAIRED ANIMATION DEBUG
	// ========================================================================
	if (CombatDebug::IsPairedAnimDebugEnabled())
	{
		// Generate paired animation debug data
		CachedPairedAnimData = GeneratePairedAnimDebugData(Character);

		// Draw 3D visualization
		if (CachedPairedAnimData.bIsValid)
		{
			Draw3DPairedAnimVisualization(GetWorld(), Character, CachedPairedAnimData);
		}

		// Draw HUD panel (always show if paired anim debug is enabled)
		DrawPairedAnimPanel(CachedPairedAnimData);
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
	// WEAPON/HIT DETECTION STATE
	// ========================================================================
	if (UWeaponComponent* WeaponComp = Character->WeaponComponent)
	{
		Data.bHitDetectionActive = WeaponComp->IsHitDetectionEnabled();
		Data.HitActorCount = WeaponComp->GetHitActorCount();

		if (const UWeaponData* WepData = WeaponComp->WeaponData)
		{
			Data.bUsingWeaponMeshSockets = !WepData->bUseCharacterSocketsForTrace;
		}
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

	// ========================================================================
	// TARGET/ENEMY INFO
	// ========================================================================
	if (UTargetingComponent* TargetComp = Character->TargetingComponent)
	{
		if (AActor* Target = TargetComp->GetCurrentTarget())
		{
			Data.CurrentTarget = Target;
			Data.TargetName = Target->GetName();

			// Get health info via interface
			if (Target->Implements<UDamageableInterface>())
			{
				Data.TargetCurrentHealth = IDamageableInterface::Execute_GetCurrentHealth(Target);
				Data.TargetMaxHealth = IDamageableInterface::Execute_GetMaxHealth(Target);
				Data.bTargetIsAlive = IDamageableInterface::Execute_IsAlive(Target);
			}

			// Get death state flags from combat character
			if (ABaseCombatCharacter* TargetCombat = Cast<ABaseCombatCharacter>(Target))
			{
				Data.bTargetIsDying = TargetCombat->bIsDying;
				Data.bTargetIsDead = TargetCombat->bIsDead;
			}
		}
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
	Y += LineHeight;

	// Weapon/Hit detection state (only when weapon debug enabled)
	if (CombatDebug::IsWeaponDebugEnabled())
	{
		const FString HitDetectText = FString::Printf(TEXT("Hit Detection: %s | Hits: %d"),
			Data.bHitDetectionActive ? TEXT("ACTIVE") : TEXT("OFF"),
			Data.HitActorCount);
		Canvas->SetDrawColor(Data.bHitDetectionActive ? FColor::Red : FColor::White);
		Canvas->DrawText(GEngine->GetSmallFont(), HitDetectText, X, Y, StatusFontScale, StatusFontScale);
		Y += LineHeight;

		const FString SocketText = FString::Printf(TEXT("Sockets: %s"),
			Data.bUsingWeaponMeshSockets ? TEXT("Weapon Mesh") : TEXT("Character Mesh"));
		Canvas->SetDrawColor(FColor::Cyan);
		Canvas->DrawText(GEngine->GetSmallFont(), SocketText, X, Y, StatusFontScale, StatusFontScale);
		Y += LineHeight;
	}

	// ========================================================================
	// TARGET/ENEMY INFO
	// ========================================================================
	if (Data.CurrentTarget.IsValid())
	{
		Y += LineHeight; // Spacer

		// Target name header
		const FString TargetText = FString::Printf(TEXT("TARGET: %s"), *Data.TargetName);
		Canvas->SetDrawColor(FColor::Yellow);
		Canvas->DrawText(GEngine->GetSmallFont(), TargetText, X, Y, StatusFontScale, StatusFontScale);
		Y += LineHeight;

		// Health bar
		const float HealthPercent = Data.TargetMaxHealth > 0.0f ?
			(Data.TargetCurrentHealth / Data.TargetMaxHealth) * 100.0f : 0.0f;
		const FString HealthText = FString::Printf(TEXT("  Health: %.0f/%.0f (%.0f%%)"),
			Data.TargetCurrentHealth, Data.TargetMaxHealth, HealthPercent);
		Canvas->SetDrawColor(Data.bTargetIsAlive ? FColor::Green : FColor::Red);
		Canvas->DrawText(GEngine->GetSmallFont(), HealthText, X, Y, StatusFontScale, StatusFontScale);
		Y += LineHeight;

		// Death state (two-stage: Dying → Dead)
		FString StateText;
		FColor StateColor;
		if (Data.bTargetIsDead)
		{
			StateText = TEXT("DEAD");
			StateColor = FColor::Red;
		}
		else if (Data.bTargetIsDying)
		{
			StateText = TEXT("DYING");
			StateColor = FColor::Orange;
		}
		else
		{
			StateText = TEXT("Alive");
			StateColor = FColor::Green;
		}
		const FString AliveText = FString::Printf(TEXT("  State: %s"), *StateText);
		Canvas->SetDrawColor(StateColor);
		Canvas->DrawText(GEngine->GetSmallFont(), AliveText, X, Y, StatusFontScale, StatusFontScale);
	}
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

// ============================================================================
// PAIRED ANIMATION DEBUG
// ============================================================================

FPairedAnimDebugData ACombatDebugHUD::GeneratePairedAnimDebugData(ABaseCombatCharacter* Character)
{
	FPairedAnimDebugData Data;
	Data.bIsValid = false;

	if (!Character)
	{
		return Data;
	}

	UCombatComponent* CombatComp = Character->GetCombatComponent();
	UTargetingComponent* TargetComp = Character->TargetingComponent;
	UHitReactionComponent* HitReactionComp = Character->HitReactionComponent;

	if (!CombatComp)
	{
		return Data;
	}

	Data.bIsValid = true;

	// ========================================================================
	// STATE INFO
	// ========================================================================
	// Use Execute_ pattern for BlueprintNativeEvent interface calls
	const ECombatState CombatState = ICombatInterface::Execute_GetCombatState(Character);
	Data.bInPairedAnimation = (CombatState == ECombatState::Finishing);

	// Determine role
	if (CombatComp->PairedAnimationPartners.Num() > 0)
	{
		Data.Role = TEXT("ATTACKER");
		Data.StateDescription = TEXT("EXECUTING_FINISHER");
	}
	else if (HitReactionComp && HitReactionComp->IsFinisherTarget())
	{
		Data.Role = TEXT("VICTIM");
		Data.StateDescription = TEXT("RECEIVING_FINISHER");
		Data.bInPairedAnimation = true;
	}
	else if (Data.bInPairedAnimation)
	{
		Data.Role = TEXT("ATTACKER");
		Data.StateDescription = TEXT("FINISHING");
	}
	else
	{
		Data.Role = TEXT("NONE");
		Data.StateDescription = TEXT("IDLE");
	}

	// ========================================================================
	// PARTNER INFO
	// ========================================================================
	for (const TWeakObjectPtr<AActor>& PartnerRef : CombatComp->PairedAnimationPartners)
	{
		if (AActor* Partner = PartnerRef.Get())
		{
			Data.AllPartners.Add(Partner);
			Data.PartnerNames.Add(Partner->GetName());

			if (!Data.PrimaryPartner.IsValid())
			{
				Data.PrimaryPartner = Partner;
			}
		}
	}

	// ========================================================================
	// WARP TRACKING (from TargetingComponent)
	// ========================================================================
	if (TargetComp)
	{
		Data.bAttackerWarpActive = TargetComp->IsTrackingAsAttacker();
		Data.bVictimWarpActive = TargetComp->IsTrackingAsVictim();

		// Get warp targets if active
		if (Data.bAttackerWarpActive && Data.PrimaryPartner.IsValid())
		{
			Data.AttackerWarpTarget = Data.PrimaryPartner->GetActorLocation();
			Data.DistanceToWarpTarget = FVector::Dist(Character->GetActorLocation(), Data.AttackerWarpTarget);
		}

		if (Data.bVictimWarpActive)
		{
			// Victim warp target is calculated relative to attacker
			// For now, show current position as warp is continuous
			Data.VictimWarpTarget = Character->GetActorLocation();
		}

		// Get max warp distance from targeting settings
		if (UTargetingSettings* TargetingSettings = TargetComp->GetEffectiveSettings())
		{
			Data.MaxWarpDistance = TargetingSettings->SoftAimRange;
		}
	}

	// Calculate partner distance
	if (Data.PrimaryPartner.IsValid())
	{
		Data.CurrentPartnerDistance = FVector::Dist(Character->GetActorLocation(), Data.PrimaryPartner->GetActorLocation());
	}

	// ========================================================================
	// VULNERABILITY INFO (check current target - hard-lock or soft-aim fallback)
	// ========================================================================
	if (TargetComp)
	{
		// Try hard-locked target first
		AActor* Target = TargetComp->GetCurrentTarget();

		// Fallback to soft-aim target if no hard-lock
		if (!Target)
		{
			const FVector FacingDirection = Character->GetActorForwardVector();
			TargetComp->FindBestTargetForDirection(
				FacingDirection,
				Target,  // Out parameter
				-1.0f,   // Use defaults
				-1.0f,
				-1.0f,
				-1.0f,
				-1.0f
			);
			Data.bUsingSoftAimTarget = Target != nullptr;
		}

		if (Target)
		{
			Data.TrackedTargetName = Target->GetName();

			if (UHitReactionComponent* TargetHitReaction = Target->FindComponentByClass<UHitReactionComponent>())
			{
				Data.bTargetVulnerable = TargetHitReaction->IsVulnerableToFinisher();
				Data.VulnerabilityReason = TargetHitReaction->GetFinisherTriggerReason();
				Data.bTargetGuardBroken = TargetHitReaction->IsGuardBroken();
				Data.bTargetStunned = TargetHitReaction->IsStunned();
				Data.bTargetIsFinisherTarget = TargetHitReaction->IsFinisherTarget();

				// Get health info
				if (Target->Implements<UDamageableInterface>())
				{
					const float CurrentHealth = IDamageableInterface::Execute_GetCurrentHealth(Target);
					const float MaxHealth = IDamageableInterface::Execute_GetMaxHealth(Target);
					Data.TargetHealthPercent = MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
				}
			}

			// Calculate distance to target
			Data.CurrentPartnerDistance = FVector::Dist(Character->GetActorLocation(), Target->GetActorLocation());
		}
	}

	// ========================================================================
	// EFFECTS INFO
	// ========================================================================
	if (UWorld* World = Character->GetWorld())
	{
		if (AWorldSettings* WorldSettings = World->GetWorldSettings())
		{
			Data.TimeDilationScale = WorldSettings->TimeDilation;
			Data.bSlowMotionActive = Data.TimeDilationScale < 1.0f;
		}
	}

	// Hitstop (check actor custom time dilation)
	Data.bHitstopActive = Character->CustomTimeDilation < 0.01f;

	// ========================================================================
	// INPUT BLOCKING
	// ========================================================================
	Data.bInputBlocked = CombatComp->IsInputBlocked();

	return Data;
}

void ACombatDebugHUD::Draw3DPairedAnimVisualization(UWorld* World, ABaseCombatCharacter* Character, const FPairedAnimDebugData& Data)
{
	if (!World || !Character)
	{
		return;
	}

	const FVector CharLocation = Character->GetActorLocation();

	// ========================================================================
	// WARP TARGET VISUALIZATION
	// ========================================================================
	if (CombatDebug::IsPairedAnimWarpDebugEnabled())
	{
		if (Data.bAttackerWarpActive && Data.PrimaryPartner.IsValid())
		{
			UDebugUtils::DrawWarpTargetCrosshair(World, Data.AttackerWarpTarget, CharLocation, true, TEXT("ATK Warp"));
		}

		if (Data.bVictimWarpActive)
		{
			UDebugUtils::DrawWarpTargetCrosshair(World, Data.VictimWarpTarget, CharLocation, false, TEXT("VIC Warp"));
		}

		// Draw finisher range circle
		if (Data.MaxWarpDistance > 0.0f)
		{
			UDebugUtils::DrawFinisherRangeCircle(World, CharLocation, Data.MaxWarpDistance, Data.CurrentPartnerDistance);
		}
	}

	// ========================================================================
	// PARTNER CONNECTION VISUALIZATION
	// ========================================================================
	if (CombatDebug::IsPairedAnimPartnerDebugEnabled())
	{
		for (const TWeakObjectPtr<AActor>& PartnerRef : Data.AllPartners)
		{
			if (AActor* Partner = PartnerRef.Get())
			{
				const FVector PartnerLocation = Partner->GetActorLocation();
				const float Distance = FVector::Dist(CharLocation, PartnerLocation);
				UDebugUtils::DrawPartnerConnection(World, CharLocation, PartnerLocation, Distance, Data.MaxWarpDistance);
			}
		}
	}

	// ========================================================================
	// SYNC POINT VISUALIZATION
	// ========================================================================
	if (CombatDebug::IsPairedAnimSyncDebugEnabled() && Data.bInPairedAnimation)
	{
		if (Data.PrimaryPartner.IsValid())
		{
			// Sync point is at midpoint between characters
			const FVector PartnerLoc = Data.PrimaryPartner->GetActorLocation();
			const FVector SyncLocation = (CharLocation + PartnerLoc) * 0.5f + FVector(0, 0, 50);

			// Calculate progress based on montage position (if available)
			const float Progress = Data.SyncPointTime > 0.0f ? FMath::Clamp(Data.MontagePosition / Data.SyncPointTime, 0.0f, 1.0f) : 0.5f;
			const bool bAtSync = Progress >= 0.95f;

			UDebugUtils::DrawSyncPoint(World, SyncLocation, Progress, bAtSync, Data.CurrentSyncPointName);

			// Draw alignment validation
			UDebugUtils::DrawAlignmentValidation(World, CharLocation, PartnerLoc,
				Data.AlignmentDistance > 0.0f ? Data.AlignmentDistance : Data.CurrentPartnerDistance,
				Data.MaxAlignmentDistance,
				Data.bAlignmentOK);
		}
	}

	// ========================================================================
	// VULNERABILITY INDICATOR
	// ========================================================================
	if (CombatDebug::IsPairedAnimVulnerabilityDebugEnabled())
	{
		// Check all potential targets for vulnerability
		if (UTargetingComponent* TargetComp = Character->TargetingComponent)
		{
			if (AActor* Target = TargetComp->GetCurrentTarget())
			{
				if (UHitReactionComponent* TargetHitReaction = Target->FindComponentByClass<UHitReactionComponent>())
				{
					if (TargetHitReaction->IsVulnerableToFinisher())
					{
						FString ReasonStr;
						switch (TargetHitReaction->GetFinisherTriggerReason())
						{
							case EFinisherTriggerReason::LowHealth: ReasonStr = TEXT("LOW HEALTH"); break;
							case EFinisherTriggerReason::GuardBroken: ReasonStr = TEXT("GUARD BROKEN"); break;
							case EFinisherTriggerReason::Stunned: ReasonStr = TEXT("STUNNED"); break;
							default: ReasonStr = TEXT("VULNERABLE"); break;
						}

						float HealthPercent = 1.0f;
						if (Target->Implements<UDamageableInterface>())
						{
							const float CurrentHealth = IDamageableInterface::Execute_GetCurrentHealth(Target);
							const float MaxHealth = IDamageableInterface::Execute_GetMaxHealth(Target);
							HealthPercent = MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
						}

						UDebugUtils::DrawVulnerabilityIndicator(World, Target->GetActorLocation(), ReasonStr, HealthPercent);
					}
				}
			}
		}
	}
}

void ACombatDebugHUD::DrawPairedAnimPanel(const FPairedAnimDebugData& Data)
{
	if (!Canvas)
	{
		return;
	}

	// Position in top-right corner
	const float PanelWidth = 320.0f;
	float X = Canvas->SizeX - PanelWidth - ScreenMargin;
	float Y = ScreenMargin;

	// Panel background (semi-transparent)
	const FLinearColor PanelBgColor(0.0f, 0.0f, 0.0f, 0.7f);
	Canvas->SetDrawColor(FColor::Black);

	// ========================================================================
	// HEADER
	// ========================================================================
	Canvas->SetDrawColor(FColor::Cyan);
	Canvas->DrawText(GEngine->GetSmallFont(), TEXT("=== PAIRED ANIMATION DEBUG ==="), X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight * 1.5f;

	// ========================================================================
	// STATE INFO
	// ========================================================================
	// State
	FColor StateColor = Data.bInPairedAnimation ? FColor::Green : FColor::White;
	FString StateText = FString::Printf(TEXT("State: %s"), *Data.StateDescription);
	Canvas->SetDrawColor(StateColor);
	Canvas->DrawText(GEngine->GetSmallFont(), StateText, X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	// Role
	FColor RoleColor = Data.Role == TEXT("ATTACKER") ? FColor::Green :
					   (Data.Role == TEXT("VICTIM") ? FColor::Red : FColor::White);
	FString RoleText = FString::Printf(TEXT("Role: %s"), *Data.Role);
	Canvas->SetDrawColor(RoleColor);
	Canvas->DrawText(GEngine->GetSmallFont(), RoleText, X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	// Partner info
	if (Data.PartnerNames.Num() > 0)
	{
		FString PartnerText = FString::Printf(TEXT("Partner: %s"), *Data.PartnerNames[0]);
		Canvas->SetDrawColor(FColor::Yellow);
		Canvas->DrawText(GEngine->GetSmallFont(), PartnerText, X, Y, StatusFontScale, StatusFontScale);
		Y += LineHeight;
	}

	Y += LineHeight * 0.5f; // Spacer

	// ========================================================================
	// WARP TRACKING
	// ========================================================================
	Canvas->SetDrawColor(FColor::Cyan);
	Canvas->DrawText(GEngine->GetSmallFont(), TEXT("--- Warp Tracking ---"), X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	// Attacker warp
	FString AttackerWarpText = FString::Printf(TEXT("  Attacker Warp: %s"),
		Data.bAttackerWarpActive ? TEXT("ACTIVE") : TEXT("---"));
	Canvas->SetDrawColor(Data.bAttackerWarpActive ? FColor::Green : FColor::White);
	Canvas->DrawText(GEngine->GetSmallFont(), AttackerWarpText, X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	// Victim warp
	FString VictimWarpText = FString::Printf(TEXT("  Victim Warp: %s"),
		Data.bVictimWarpActive ? TEXT("ACTIVE") : TEXT("---"));
	Canvas->SetDrawColor(Data.bVictimWarpActive ? FColor::Magenta : FColor::White);
	Canvas->DrawText(GEngine->GetSmallFont(), VictimWarpText, X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	// Distance
	FColor DistColor = Data.CurrentPartnerDistance <= Data.MaxWarpDistance ? FColor::Green : FColor::Red;
	FString DistText = FString::Printf(TEXT("  Distance: %.0f / %.0fu"),
		Data.CurrentPartnerDistance, Data.MaxWarpDistance);
	Canvas->SetDrawColor(DistColor);
	Canvas->DrawText(GEngine->GetSmallFont(), DistText, X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	Y += LineHeight * 0.5f; // Spacer

	// ========================================================================
	// VULNERABILITY
	// ========================================================================
	Canvas->SetDrawColor(FColor::Cyan);
	Canvas->DrawText(GEngine->GetSmallFont(), TEXT("--- Target Vulnerability ---"), X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	// Show tracked target name
	if (!Data.TrackedTargetName.IsEmpty())
	{
		FString TargetText = FString::Printf(TEXT("  Target: %s%s"),
			*Data.TrackedTargetName,
			Data.bUsingSoftAimTarget ? TEXT(" (soft-aim)") : TEXT(" (locked)"));
		Canvas->SetDrawColor(FColor::Yellow);
		Canvas->DrawText(GEngine->GetSmallFont(), TargetText, X, Y, StatusFontScale, StatusFontScale);
		Y += LineHeight;
	}
	else
	{
		Canvas->SetDrawColor(FColor(128, 128, 128));  // Gray
		Canvas->DrawText(GEngine->GetSmallFont(), TEXT("  Target: (none in range)"), X, Y, StatusFontScale, StatusFontScale);
		Y += LineHeight;
	}

	// Vulnerable status
	FString VulnText = FString::Printf(TEXT("  Vulnerable: %s"),
		Data.bTargetVulnerable ? TEXT("YES") : TEXT("NO"));
	Canvas->SetDrawColor(Data.bTargetVulnerable ? FColor::Red : FColor::White);
	Canvas->DrawText(GEngine->GetSmallFont(), VulnText, X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	// Vulnerability reason
	if (Data.bTargetVulnerable)
	{
		FString ReasonStr;
		switch (Data.VulnerabilityReason)
		{
			case EFinisherTriggerReason::LowHealth: ReasonStr = TEXT("Low Health"); break;
			case EFinisherTriggerReason::GuardBroken: ReasonStr = TEXT("Guard Broken (N/I)"); break;  // Not Implemented
			case EFinisherTriggerReason::Stunned: ReasonStr = TEXT("Stunned (N/I)"); break;  // Not Implemented
			default: ReasonStr = TEXT("Unknown"); break;
		}
		FString ReasonText = FString::Printf(TEXT("  Reason: %s"), *ReasonStr);
		Canvas->SetDrawColor(FColor::Orange);
		Canvas->DrawText(GEngine->GetSmallFont(), ReasonText, X, Y, StatusFontScale, StatusFontScale);
		Y += LineHeight;
	}
	else
	{
		// Show available trigger - only Low Health works currently
		Canvas->SetDrawColor(FColor(128, 128, 128)); // Gray
		Canvas->DrawText(GEngine->GetSmallFont(), TEXT("  (Only LowHealth trigger active)"), X, Y, LabelFontScale, LabelFontScale);
		Y += LineHeight;
	}

	// Health
	FString HealthText = FString::Printf(TEXT("  Health: %.0f%% (Threshold: %.0f%%)"),
		Data.TargetHealthPercent * 100.0f, Data.HealthThreshold * 100.0f);
	Canvas->SetDrawColor(Data.TargetHealthPercent <= Data.HealthThreshold ? FColor::Red : FColor::Green);
	Canvas->DrawText(GEngine->GetSmallFont(), HealthText, X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	// Finisher target mutex
	if (Data.bTargetIsFinisherTarget)
	{
		Canvas->SetDrawColor(FColor::Yellow);
		Canvas->DrawText(GEngine->GetSmallFont(), TEXT("  [ALREADY FINISHER TARGET]"), X, Y, StatusFontScale, StatusFontScale);
		Y += LineHeight;
	}

	Y += LineHeight * 0.5f; // Spacer

	// ========================================================================
	// EFFECTS
	// ========================================================================
	Canvas->SetDrawColor(FColor::Cyan);
	Canvas->DrawText(GEngine->GetSmallFont(), TEXT("--- Effects Active ---"), X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	// Slow motion
	FString SlowMoText = FString::Printf(TEXT("  SlowMo: %s (%.2fx)"),
		Data.bSlowMotionActive ? TEXT("ACTIVE") : TEXT("---"),
		Data.TimeDilationScale);
	Canvas->SetDrawColor(Data.bSlowMotionActive ? FColor::Yellow : FColor::White);
	Canvas->DrawText(GEngine->GetSmallFont(), SlowMoText, X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	// Hitstop
	FString HitstopText = FString::Printf(TEXT("  Hitstop: %s"),
		Data.bHitstopActive ? TEXT("FROZEN") : TEXT("---"));
	Canvas->SetDrawColor(Data.bHitstopActive ? FColor::Red : FColor::White);
	Canvas->DrawText(GEngine->GetSmallFont(), HitstopText, X, Y, StatusFontScale, StatusFontScale);
	Y += LineHeight;

	// Input blocked
	FString InputText = FString::Printf(TEXT("  Input: %s"),
		Data.bInputBlocked ? TEXT("BLOCKED") : TEXT("Enabled"));
	Canvas->SetDrawColor(Data.bInputBlocked ? FColor::Orange : FColor::Green);
	Canvas->DrawText(GEngine->GetSmallFont(), InputText, X, Y, StatusFontScale, StatusFontScale);
}
