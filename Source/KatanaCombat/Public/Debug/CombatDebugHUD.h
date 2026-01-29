// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CombatTypes.h"
#include "CombatDebugHUD.generated.h"

class UCombatComponent;
class ABaseCombatCharacter;

/**
 * Debug arrow data for coordinated 3D/2D rendering
 * 3D renderer draws arrows, HUD draws labels at projected positions
 */
USTRUCT()
struct FDebugArrowData
{
	GENERATED_BODY()

	/** World-space start position */
	FVector WorldStart = FVector::ZeroVector;

	/** World-space end position (where label appears) */
	FVector WorldEnd = FVector::ZeroVector;

	/** Arrow color */
	FColor Color = FColor::White;

	/** Label text */
	FString Label;

	/** Arrow thickness */
	float Thickness = 2.0f;

	/** Arrow head size */
	float ArrowSize = 20.0f;

	/** Is this a dashed arrow? */
	bool bIsDashed = false;

	/** Priority for label collision (higher = more important) */
	int32 LabelPriority = 0;
};

/**
 * Complete debug visualization data - single source of truth
 * Generated once per frame, consumed by both 3D and HUD renderers
 */
USTRUCT()
struct FCombatDebugData
{
	GENERATED_BODY()

	/** Character being debugged */
	UPROPERTY()
	TWeakObjectPtr<ABaseCombatCharacter> Character;

	/** All arrows to render */
	TArray<FDebugArrowData> Arrows;

	/** Arc points for camera-character offset visualization */
	TArray<FVector> ArcPoints;

	/** Arc label position and text */
	FVector ArcLabelPosition = FVector::ZeroVector;
	FString ArcLabel;

	// ========================================================================
	// STATUS INFO (for fixed HUD panel)
	// ========================================================================

	/** Current attack phase */
	EAttackPhase Phase = EAttackPhase::None;

	/** Queue status */
	int32 QueuedCount = 0;
	int32 PendingCount = 0;

	/** Hold state */
	bool bIsHolding = false;
	EInputType HeldInputType = EInputType::None;
	float HoldDuration = 0.0f;
	bool bHoldCompleted = false;

	/** Direction info */
	EInputDirection InputDirection = EInputDirection::None;
	EAttackDirection AttackDirection = EAttackDirection::None;

	/** Movement state */
	bool bMovementDisabled = false;

	/** Current attack name */
	FString CurrentAttackName;

	/** Camera-character yaw delta */
	float CamCharYawDelta = 0.0f;

	/** Is data valid this frame? */
	bool bIsValid = false;
};

/**
 * Combat Debug HUD
 *
 * Renders debug visualization in screen-space for readability.
 * Works in coordination with 3D DrawDebug calls:
 * - 3D system: Draws arrows in world space
 * - This HUD: Draws labels at projected positions + status panel
 *
 * Controlled by CVars in DebugConfig.h:
 * - Combat.Debug.All
 * - Combat.Debug.Direction
 * - Combat.Debug.Phase
 * - etc.
 */
UCLASS()
class KATANACOMBAT_API ACombatDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	ACombatDebugHUD();

	/** Main HUD draw function - called every frame */
	virtual void DrawHUD() override;

	/** Generate debug data for a character (call once per frame) */
	static FCombatDebugData GenerateDebugData(ABaseCombatCharacter* Character);

	/** Draw 3D arrows using DrawDebug functions */
	static void Draw3DArrows(UWorld* World, const FCombatDebugData& Data);

protected:
	// ========================================================================
	// HUD DRAWING HELPERS
	// ========================================================================

	/** Draw the status panel (fixed screen position) */
	void DrawStatusPanel(const FCombatDebugData& Data);

	/** Draw labels at projected arrow positions */
	void DrawArrowLabels(const FCombatDebugData& Data);

	/** Draw arc label if visible */
	void DrawArcLabel(const FCombatDebugData& Data);

	/** Project world position to screen, returns false if behind camera */
	bool ProjectToScreen(const FVector& WorldPos, FVector2D& OutScreenPos) const;

	/** Clamp screen position to viewport with margin */
	FVector2D ClampToViewport(const FVector2D& ScreenPos, float Margin = 20.0f) const;

	/** Get color for attack phase */
	static FColor GetPhaseColor(EAttackPhase Phase);

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/** Margin from screen edges for status panel */
	UPROPERTY(EditDefaultsOnly, Category = "Layout")
	float ScreenMargin = 20.0f;

	/** Line height for status text */
	UPROPERTY(EditDefaultsOnly, Category = "Layout")
	float LineHeight = 18.0f;

	/** Font scale for status panel */
	UPROPERTY(EditDefaultsOnly, Category = "Layout")
	float StatusFontScale = 1.2f;

	/** Font scale for arrow labels */
	UPROPERTY(EditDefaultsOnly, Category = "Layout")
	float LabelFontScale = 1.0f;

	/** Offset for labels from projected arrow endpoint */
	UPROPERTY(EditDefaultsOnly, Category = "Layout")
	FVector2D LabelOffset = FVector2D(10.0f, -10.0f);

private:
	/** Cached debug data for current frame */
	FCombatDebugData CachedDebugData;

	/** Find the player's combat character */
	ABaseCombatCharacter* GetPlayerCombatCharacter() const;
};
