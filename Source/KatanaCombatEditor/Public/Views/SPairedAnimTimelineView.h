// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SSlider;
class STextBlock;
struct FPairedAnimationPreviewModel;

/**
 * PT-11 Phase 2: Timeline view for paired animation preview.
 *
 * Encapsulates the timeline slider, playback controls, and time display.
 * Observes Model state for time/playback info and notifies owner of user input.
 *
 * Does NOT handle Tick/playback advancement - that remains in the main widget
 * since it coordinates with animation updates and visualization.
 */
class KATANACOMBATEDITOR_API SPairedAnimTimelineView : public SCompoundWidget
{
public:
	/** Delegate for when user scrubs the timeline */
	DECLARE_DELEGATE_OneParam(FOnTimelineValueChanged, float);

	/** Delegate for when user clicks play/pause */
	DECLARE_DELEGATE(FOnPlayPauseClicked);

	/** Delegate for stepping forward/backward */
	DECLARE_DELEGATE(FOnStepForward);
	DECLARE_DELEGATE(FOnStepBackward);
	DECLARE_DELEGATE(FOnStepForwardLarge);
	DECLARE_DELEGATE(FOnStepBackwardLarge);

	/** Delegate for reset button */
	DECLARE_DELEGATE(FOnResetClicked);

	SLATE_BEGIN_ARGS(SPairedAnimTimelineView) {}
		/** Reference to the preview model (for reading state) */
		SLATE_ARGUMENT(FPairedAnimationPreviewModel*, Model)
		/** Callback when timeline slider changes */
		SLATE_EVENT(FOnTimelineValueChanged, OnTimelineValueChanged)
		/** Callback when play/pause is clicked */
		SLATE_EVENT(FOnPlayPauseClicked, OnPlayPauseClicked)
		/** Callback for step forward */
		SLATE_EVENT(FOnStepForward, OnStepForward)
		/** Callback for step backward */
		SLATE_EVENT(FOnStepBackward, OnStepBackward)
		/** Callback for large step forward */
		SLATE_EVENT(FOnStepForwardLarge, OnStepForwardLarge)
		/** Callback for large step backward */
		SLATE_EVENT(FOnStepBackwardLarge, OnStepBackwardLarge)
		/** Callback for reset */
		SLATE_EVENT(FOnResetClicked, OnResetClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Update the slider position to match current time (call from owner's Tick) */
	void SyncSliderToModel();

	/** Update the time display text */
	void UpdateTimeDisplay();

	/** Get the underlying slider widget */
	TSharedPtr<SSlider> GetSlider() const { return TimelineSlider; }

private:
	/** Reference to the preview model (not owned) */
	FPairedAnimationPreviewModel* ModelPtr = nullptr;

	/** The timeline slider widget */
	TSharedPtr<SSlider> TimelineSlider;

	/** Time display text */
	TSharedPtr<STextBlock> TimeDisplayText;

	// Delegates
	FOnTimelineValueChanged OnTimelineValueChangedDelegate;
	FOnPlayPauseClicked OnPlayPauseClickedDelegate;
	FOnStepForward OnStepForwardDelegate;
	FOnStepBackward OnStepBackwardDelegate;
	FOnStepForwardLarge OnStepForwardLargeDelegate;
	FOnStepBackwardLarge OnStepBackwardLargeDelegate;
	FOnResetClicked OnResetClickedDelegate;

	// Internal handlers that invoke delegates
	void HandleTimelineValueChanged(float NewValue);
	FReply HandlePlayPauseClicked();
	FReply HandleStepForward();
	FReply HandleStepBackward();
	FReply HandleStepForwardLarge();
	FReply HandleStepBackwardLarge();
	FReply HandleResetClicked();

	// Text helpers
	FText GetTimeDisplayText() const;
	FText GetPlayPauseButtonText() const;
};
