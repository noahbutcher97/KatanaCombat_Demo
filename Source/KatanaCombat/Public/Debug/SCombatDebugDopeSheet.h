// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "ActionQueueTypes.h"

class UCombatComponentV2;

/**
 * Visual event marker for dope sheet
 */
struct FDopeSheetEvent
{
	float Time;
	FString Label;
	FLinearColor Color;
	bool bIsDuration; // true for bars, false for markers
	float Duration;

	FDopeSheetEvent()
		: Time(0.0f)
		, Color(FLinearColor::White)
		, bIsDuration(false)
		, Duration(0.0f)
	{
	}

	FDopeSheetEvent(float InTime, const FString& InLabel, FLinearColor InColor, bool bInIsDuration = false, float InDuration = 0.0f)
		: Time(InTime)
		, Label(InLabel)
		, Color(InColor)
		, bIsDuration(bInIsDuration)
		, Duration(InDuration)
	{
	}
};

/**
 * Track data for dope sheet
 */
struct FDopeSheetTrack
{
	FString TrackName;
	TArray<FDopeSheetEvent> Events;
	FLinearColor TrackColor;
	float Height;

	FDopeSheetTrack()
		: TrackColor(FLinearColor::Gray)
		, Height(30.0f)
	{
	}

	FDopeSheetTrack(const FString& InName, FLinearColor InColor, float InHeight = 30.0f)
		: TrackName(InName)
		, TrackColor(InColor)
		, Height(InHeight)
	{
	}
};

/**
 * Slate dope sheet widget for V2 combat system visualization
 *
 * Shows timeline with:
 * - Window tracks (Combo, Parry, Cancel, Hold, Recovery)
 * - Input event markers (Press/Release)
 * - Action queue visualization (Pending/Executing/Completed)
 * - Playhead showing current montage time
 */
class KATANACOMBAT_API SCombatDebugDopeSheet : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCombatDebugDopeSheet)
		: _ViewRangeMin(0.0f)
		, _ViewRangeMax(5.0f)
		, _CurrentTime(0.0f)
		{}

		/** Minimum visible time */
		SLATE_ARGUMENT(float, ViewRangeMin)

		/** Maximum visible time */
		SLATE_ARGUMENT(float, ViewRangeMax)

		/** Current playback time */
		SLATE_ARGUMENT(float, CurrentTime)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, UCombatComponentV2* InCombatComponent);

	/** SWidget overrides */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float) const override;

	/** Update data from combat component */
	void RefreshData();

	/** Set view range */
	void SetViewRange(float Min, float Max);

	/** Set current playback time */
	void SetCurrentTime(float Time);

private:
	/** Combat component being visualized */
	TWeakObjectPtr<UCombatComponentV2> CombatComponent;

	/** Timeline tracks */
	TArray<FDopeSheetTrack> Tracks;

	/** View range */
	float ViewRangeMin;
	float ViewRangeMax;

	/** Current playback time */
	float CurrentTime;

	/** Track heights */
	static constexpr float TrackHeight = 30.0f;
	static constexpr float TrackSpacing = 5.0f;
	static constexpr float TimelineHeight = 20.0f;
	static constexpr float HeaderWidth = 150.0f;

	/** Colors */
	static const FLinearColor ComboWindowColor;
	static const FLinearColor ParryWindowColor;
	static const FLinearColor CancelWindowColor;
	static const FLinearColor HoldWindowColor;
	static const FLinearColor RecoveryWindowColor;
	static const FLinearColor InputPressColor;
	static const FLinearColor InputReleaseColor;
	static const FLinearColor ActionPendingColor;
	static const FLinearColor ActionExecutingColor;
	static const FLinearColor ActionCompletedColor;
	static const FLinearColor ActionCancelledColor;
	static const FLinearColor PlayheadColor;
	static const FLinearColor GridLineColor;
	static const FLinearColor BackgroundColor;

	/** Helper functions */
	void BuildTracks();
	void AddWindowTrack(const FString& Name, EActionWindowType WindowType, FLinearColor Color);
	void AddInputEventTrack();
	void AddActionQueueTrack();

	/** Drawing helpers */
	void DrawTimeline(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;
	void DrawTracks(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;
	void DrawPlayhead(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;
	void DrawGrid(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;

	/** Convert time to screen X position */
	float TimeToPixel(float Time, float AvailableWidth) const;

	/** Convert screen X position to time */
	float PixelToTime(float Pixel, float AvailableWidth) const;

	/** Get track Y offset */
	float GetTrackYOffset(int32 TrackIndex) const;
};
