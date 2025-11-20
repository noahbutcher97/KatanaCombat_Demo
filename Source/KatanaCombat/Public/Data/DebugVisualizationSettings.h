// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DebugVisualizationSettings.generated.h"

/**
 * Debug Visualization Settings
 *
 * Comprehensive configuration struct for all debug visualization in the combat system.
 * Designed to be embedded in CombatSettings data asset for designer-friendly editing.
 *
 * Features:
 * - Master toggles for each debug system
 * - Configurable colors, sizes, positions, and visual properties
 * - Hot-reloadable without recompilation
 * - Organized into logical categories
 * - ClampMin/ClampMax for value safety
 * - UIMin/UIMax for slider ranges
 *
 * Usage:
 * Embedded in UCombatSettings with ShowOnlyInnerProperties for clean editor UX.
 * All debug drawing functions accept const FDebugVisualizationSettings& parameter.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FDebugVisualizationSettings
{
	GENERATED_BODY()

	// ========================================================================
	// MASTER TOGGLES
	// ========================================================================

	/** Enable/disable directional transformation arrow debug visualization */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Master Toggles",
		meta = (ToolTip = "Show numbered arrows visualizing Camera→Input→Character-Relative→Attack transformation pipeline"))
	bool bShowDirectionArrows = true;

	/** Enable/disable checkpoint timeline debug visualization */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Master Toggles",
		meta = (ToolTip = "Show horizontal timeline with phase windows, checkpoints, and current position marker"))
	bool bShowCheckpointTimeline = true;

	/** Enable/disable phase indicator text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Master Toggles",
		meta = (ToolTip = "Show current attack phase (Windup/Active/Recovery) above character"))
	bool bShowPhaseInfo = true;

	/** Enable/disable hold state indicator */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Master Toggles",
		meta = (ToolTip = "Show '⬛ HOLD ACTIVE' text when hold mechanic is active"))
	bool bShowHoldIndicator = true;

	/** Enable/disable angular arc between camera and character rotation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Master Toggles",
		meta = (ToolTip = "Show arc visualization of camera-character yaw offset with angle label"))
	bool bShowAngularArc = true;

	// ========================================================================
	// SPATIAL CONFIGURATION
	// ========================================================================

	/** Height above character origin for arrow visualization (chest level) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spatial",
		meta = (ClampMin = "0.0", ClampMax = "200.0", UIMin = "50.0", UIMax = "150.0",
			ToolTip = "Z-offset from character location for all arrows (default 90 = chest height)"))
	float ArrowOriginHeight = 90.0f;

	/** Height above character origin for phase text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spatial",
		meta = (ClampMin = "0.0", ClampMax = "300.0", UIMin = "100.0", UIMax = "250.0",
			ToolTip = "Z-offset from character location for phase indicator text"))
	float PhaseTextHeight = 150.0f;

	/** Height above character origin for hold state indicator */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spatial",
		meta = (ClampMin = "0.0", ClampMax = "300.0", UIMin = "150.0", UIMax = "250.0",
			ToolTip = "Z-offset from character location for hold active indicator"))
	float HoldIndicatorHeight = 200.0f;

	/** Vertical offset above arrow endpoints for text labels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spatial",
		meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "10.0", UIMax = "50.0",
			ToolTip = "Z-offset above arrow tip for arrow labels (prevents overlap with arrow)"))
	float ArrowLabelOffset = 25.0f;

	/** Vertical offset above attack arrow endpoint for attack label */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spatial",
		meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "10.0", UIMax = "50.0",
			ToolTip = "Z-offset above attack arrow tip (can be different for emphasis)"))
	float AttackLabelOffset = 30.0f;

	/** Height above character origin for checkpoint timeline visualization */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spatial",
		meta = (ClampMin = "0.0", ClampMax = "400.0", UIMin = "200.0", UIMax = "350.0",
			ToolTip = "Z-offset from character location for horizontal timeline"))
	float TimelineHeight = 250.0f;

	// ========================================================================
	// ARROW PROPERTIES - Lengths
	// ========================================================================

	/** Length of Camera arrow (blue, #1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Lengths",
		meta = (ClampMin = "50.0", ClampMax = "500.0", UIMin = "100.0", UIMax = "300.0",
			ToolTip = "Length of camera forward direction arrow"))
	float CameraArrowLength = 180.0f;

	/** Length of Input arrow (yellow/gold, #2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Lengths",
		meta = (ClampMin = "50.0", ClampMax = "500.0", UIMin = "100.0", UIMax = "300.0",
			ToolTip = "Length of world input direction arrow"))
	float InputArrowLength = 140.0f;

	/** Length of Character Forward arrow (green, reference) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Lengths",
		meta = (ClampMin = "50.0", ClampMax = "500.0", UIMin = "100.0", UIMax = "300.0",
			ToolTip = "Length of character forward direction arrow"))
	float CharacterForwardLength = 160.0f;

	/** Length of Character-Relative arrow (orange, #4) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Lengths",
		meta = (ClampMin = "50.0", ClampMax = "500.0", UIMin = "100.0", UIMax = "300.0",
			ToolTip = "Length of character-relative direction arrow"))
	float CharacterRelativeLength = 120.0f;

	/** Length of Attack arrow (magenta, #5, final result) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Lengths",
		meta = (ClampMin = "50.0", ClampMax = "500.0", UIMin = "100.0", UIMax = "300.0",
			ToolTip = "Length of final attack direction arrow"))
	float AttackArrowLength = 160.0f;

	// ========================================================================
	// ARROW PROPERTIES - Visual
	// ========================================================================

	/** Arrow head size for Camera arrow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Visual",
		meta = (ClampMin = "5.0", ClampMax = "50.0", UIMin = "10.0", UIMax = "30.0",
			ToolTip = "Size of arrowhead triangle for camera arrow"))
	float CameraArrowHeadSize = 20.0f;

	/** Arrow head size for Input arrow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Visual",
		meta = (ClampMin = "5.0", ClampMax = "50.0", UIMin = "10.0", UIMax = "30.0",
			ToolTip = "Size of arrowhead triangle for input arrow"))
	float InputArrowHeadSize = 20.0f;

	/** Arrow head size for Character Forward arrow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Visual",
		meta = (ClampMin = "5.0", ClampMax = "50.0", UIMin = "10.0", UIMax = "30.0",
			ToolTip = "Size of arrowhead triangle for character forward arrow"))
	float CharacterForwardHeadSize = 25.0f;

	/** Arrow head size for Character-Relative arrow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Visual",
		meta = (ClampMin = "5.0", ClampMax = "50.0", UIMin = "10.0", UIMax = "30.0",
			ToolTip = "Size of arrowhead triangle for character-relative arrow"))
	float CharacterRelativeHeadSize = 20.0f;

	/** Arrow head size for Attack arrow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Visual",
		meta = (ClampMin = "5.0", ClampMax = "50.0", UIMin = "10.0", UIMax = "40.0",
			ToolTip = "Size of arrowhead triangle for attack arrow (larger for emphasis)"))
	float AttackArrowHeadSize = 30.0f;

	/** Line thickness for Camera arrow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Visual",
		meta = (ClampMin = "0.5", ClampMax = "10.0", UIMin = "1.0", UIMax = "5.0",
			ToolTip = "Line thickness for camera arrow"))
	float CameraArrowThickness = 2.5f;

	/** Line thickness for Input arrow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Visual",
		meta = (ClampMin = "0.5", ClampMax = "10.0", UIMin = "1.0", UIMax = "5.0",
			ToolTip = "Line thickness for input arrow"))
	float InputArrowThickness = 2.5f;

	/** Line thickness for Character Forward arrow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Visual",
		meta = (ClampMin = "0.5", ClampMax = "10.0", UIMin = "1.0", UIMax = "5.0",
			ToolTip = "Line thickness for character forward arrow"))
	float CharacterForwardThickness = 3.0f;

	/** Line thickness for Character-Relative arrow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Visual",
		meta = (ClampMin = "0.5", ClampMax = "10.0", UIMin = "1.0", UIMax = "5.0",
			ToolTip = "Line thickness for character-relative arrow"))
	float CharacterRelativeThickness = 2.5f;

	/** Line thickness for Attack arrow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Visual",
		meta = (ClampMin = "0.5", ClampMax = "10.0", UIMin = "2.0", UIMax = "8.0",
			ToolTip = "Line thickness for attack arrow (thicker for emphasis)"))
	float AttackArrowThickness = 4.0f;

	/** Number of segments for dashed arrow (hold input style) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow Visual",
		meta = (ClampMin = "3", ClampMax = "15", UIMin = "5", UIMax = "11",
			ToolTip = "Number of dash segments for hold-release input arrow (odd numbers work best)"))
	int32 DashedArrowSegments = 7;

	// ========================================================================
	// COLORS
	// ========================================================================

	/** Color for Camera arrow (default: blue) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors",
		meta = (ToolTip = "Color for camera forward direction arrow"))
	FLinearColor CameraArrowColor = FLinearColor(0.0f, 0.39f, 1.0f, 1.0f); // RGB(0, 100, 255)

	/** Color for Input arrow - continuous input (default: yellow) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors",
		meta = (ToolTip = "Color for world input arrow when continuous input is active"))
	FLinearColor InputArrowColorContinuous = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f); // RGB(255, 255, 0)

	/** Color for Input arrow - hold input (default: gold) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors",
		meta = (ToolTip = "Color for world input arrow when hold mechanic is active"))
	FLinearColor InputArrowColorHold = FLinearColor(1.0f, 0.84f, 0.0f, 1.0f); // RGB(255, 215, 0)

	/** Color for Character-Relative arrow (default: orange) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors",
		meta = (ToolTip = "Color for character-relative direction arrow"))
	FLinearColor CharacterRelativeArrowColor = FLinearColor(1.0f, 0.65f, 0.0f, 1.0f); // RGB(255, 165, 0)

	/** Color for Attack arrow (default: magenta) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors",
		meta = (ToolTip = "Color for final attack direction arrow"))
	FLinearColor AttackArrowColor = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f); // RGB(255, 0, 255)

	/** Color for Character Forward arrow (default: green) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors",
		meta = (ToolTip = "Color for character forward reference arrow"))
	FLinearColor CharacterForwardColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f); // RGB(0, 255, 0)

	/** Color for text labels (default: white) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors",
		meta = (ToolTip = "Color for all debug text labels"))
	FLinearColor TextColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); // RGB(255, 255, 255)

	/** Color for angular arc (default: white semi-transparent) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors",
		meta = (ToolTip = "Color for angular arc showing camera-character offset"))
	FLinearColor ArcColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.5f); // RGB(255, 255, 255, 128)

	// ========================================================================
	// ARC VISUALIZATION
	// ========================================================================

	/** Radius of angular arc showing camera-character offset */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc Visualization",
		meta = (ClampMin = "50.0", ClampMax = "300.0", UIMin = "75.0", UIMax = "150.0",
			ToolTip = "Radius of arc drawn between character and camera rotation"))
	float ArcRadius = 100.0f;

	/** Minimum angle (degrees) before arc is shown */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc Visualization",
		meta = (ClampMin = "0.0", ClampMax = "45.0", UIMin = "1.0", UIMax = "15.0",
			ToolTip = "Don't draw arc if camera-character offset is below this threshold"))
	float ArcMinAngle = 5.0f;

	/** Line thickness for angular arc */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc Visualization",
		meta = (ClampMin = "0.5", ClampMax = "5.0", UIMin = "1.0", UIMax = "3.0",
			ToolTip = "Thickness of arc line segments"))
	float ArcThickness = 1.5f;

	/** Scale of arc midpoint label (showing angle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc Visualization",
		meta = (ClampMin = "0.5", ClampMax = "3.0", UIMin = "0.8", UIMax = "1.5",
			ToolTip = "Text scale for angle label at arc midpoint"))
	float ArcLabelScale = 1.0f;

	/** Radius multiplier for arc label position (1.0 = on arc, <1.0 = inside, >1.0 = outside) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc Visualization",
		meta = (ClampMin = "0.3", ClampMax = "1.5", UIMin = "0.5", UIMax = "1.2",
			ToolTip = "Position of angle label relative to arc radius (0.7 = 70% of radius from center)"))
	float ArcLabelRadiusMultiplier = 0.7f;

	// ========================================================================
	// TEXT CONFIGURATION
	// ========================================================================

	/** Text scale for arrow labels (#1.CAMERA, #2.INPUT, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text",
		meta = (ClampMin = "0.5", ClampMax = "3.0", UIMin = "0.8", UIMax = "1.8",
			ToolTip = "Text scale for numbered arrow labels"))
	float ArrowLabelScale = 1.2f;

	/** Text scale for attack direction label (#5.ATTACK: Forward) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text",
		meta = (ClampMin = "0.5", ClampMax = "3.0", UIMin = "1.0", UIMax = "2.0",
			ToolTip = "Text scale for final attack direction label (can be larger for emphasis)"))
	float AttackLabelScale = 1.5f;

	/** Text scale for phase indicator */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text",
		meta = (ClampMin = "0.5", ClampMax = "3.0", UIMin = "0.8", UIMax = "1.8",
			ToolTip = "Text scale for phase indicator (Windup/Active/Recovery)"))
	float PhaseTextScale = 1.2f;

	/** Text scale for hold state indicator */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text",
		meta = (ClampMin = "0.5", ClampMax = "3.0", UIMin = "1.0", UIMax = "2.0",
			ToolTip = "Text scale for hold active indicator"))
	float HoldIndicatorScale = 1.5f;

	// ========================================================================
	// TIMELINE CONFIGURATION
	// ========================================================================

	/** Width of checkpoint timeline visualization (horizontal extent) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline",
		meta = (ClampMin = "200.0", ClampMax = "1000.0", UIMin = "300.0", UIMax = "600.0",
			ToolTip = "Horizontal width of timeline bar"))
	float TimelineWidth = 400.0f;

	/** Height of checkpoint markers on timeline */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline",
		meta = (ClampMin = "5.0", ClampMax = "50.0", UIMin = "10.0", UIMax = "30.0",
			ToolTip = "Vertical height of checkpoint tick marks"))
	float TimelineCheckpointHeight = 20.0f;

	/** Height of phase window bars on timeline */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline",
		meta = (ClampMin = "5.0", ClampMax = "50.0", UIMin = "10.0", UIMax = "30.0",
			ToolTip = "Vertical height of phase window bars (Windup/Active/Recovery)"))
	float TimelinePhaseBarHeight = 15.0f;

	/** Line thickness for timeline main bar */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline",
		meta = (ClampMin = "0.5", ClampMax = "5.0", UIMin = "1.0", UIMax = "3.0",
			ToolTip = "Thickness of timeline horizontal bar"))
	float TimelineBarThickness = 2.0f;

	/** Line thickness for checkpoint markers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline",
		meta = (ClampMin = "0.5", ClampMax = "5.0", UIMin = "1.0", UIMax = "3.0",
			ToolTip = "Thickness of checkpoint tick marks"))
	float TimelineCheckpointThickness = 1.5f;

	/** Text scale for checkpoint labels on timeline */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline",
		meta = (ClampMin = "0.5", ClampMax = "2.0", UIMin = "0.6", UIMax = "1.2",
			ToolTip = "Text scale for checkpoint time labels"))
	float TimelineLabelScale = 0.8f;

	/** Debug duration for single-frame updates (0.0 = one frame, >0 = persistent) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline",
		meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "5.0",
			ToolTip = "How long debug shapes persist (0.0 = single frame, updated each tick)"))
	float DebugDuration = 0.0f;

	// ========================================================================
	// HELPER FUNCTIONS
	// ========================================================================

	/**
	 * Convert FLinearColor to FColor for DrawDebug functions
	 * Handles gamma correction properly
	 */
	FORCEINLINE FColor ToFColor(const FLinearColor& LinearColor) const
	{
		return LinearColor.ToFColor(true); // true = sRGB conversion
	}
};
