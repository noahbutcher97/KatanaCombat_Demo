// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/SPairedAnimTimelineView.h"
#include "Data/PairedAnimationEditorTypes.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "PairedAnimTimelineView"

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static const FNumberFormattingOptions& GetNumberFormat(int32 DecimalPlaces)
{
	static TArray<FNumberFormattingOptions> CachedFormats = []() {
		TArray<FNumberFormattingOptions> Formats;
		Formats.SetNum(5);
		for (int32 i = 0; i < 5; ++i)
		{
			Formats[i].SetMaximumFractionalDigits(i);
		}
		return Formats;
	}();

	const int32 ClampedPlaces = FMath::Clamp(DecimalPlaces, 0, 4);
	return CachedFormats[ClampedPlaces];
}

// ============================================================================
// CONSTRUCTION
// ============================================================================

void SPairedAnimTimelineView::Construct(const FArguments& InArgs)
{
	ModelPtr = InArgs._Model;
	OnTimelineValueChangedDelegate = InArgs._OnTimelineValueChanged;
	OnPlayPauseClickedDelegate = InArgs._OnPlayPauseClicked;
	OnStepForwardDelegate = InArgs._OnStepForward;
	OnStepBackwardDelegate = InArgs._OnStepBackward;
	OnStepForwardLargeDelegate = InArgs._OnStepForwardLarge;
	OnStepBackwardLargeDelegate = InArgs._OnStepBackwardLarge;
	OnResetClickedDelegate = InArgs._OnResetClicked;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SNew(SVerticalBox)

			// Time display
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SAssignNew(TimeDisplayText, STextBlock)
				.Text(this, &SPairedAnimTimelineView::GetTimeDisplayText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			// Timeline slider
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SAssignNew(TimelineSlider, SSlider)
				.Value(0.0f)
				.OnValueChanged(this, &SPairedAnimTimelineView::HandleTimelineValueChanged)
			]

			// Playback controls
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("StepBackLarge", "<<"))
					.ToolTipText(LOCTEXT("StepBackLargeTip", "Step back 0.1s"))
					.OnClicked(this, &SPairedAnimTimelineView::HandleStepBackwardLarge)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("StepBack", "<"))
					.ToolTipText(LOCTEXT("StepBackTip", "Step back 1 frame"))
					.OnClicked(this, &SPairedAnimTimelineView::HandleStepBackward)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(this, &SPairedAnimTimelineView::GetPlayPauseButtonText)
					.OnClicked(this, &SPairedAnimTimelineView::HandlePlayPauseClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("StepFwd", ">"))
					.ToolTipText(LOCTEXT("StepFwdTip", "Step forward 1 frame"))
					.OnClicked(this, &SPairedAnimTimelineView::HandleStepForward)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("StepFwdLarge", ">>"))
					.ToolTipText(LOCTEXT("StepFwdLargeTip", "Step forward 0.1s"))
					.OnClicked(this, &SPairedAnimTimelineView::HandleStepForwardLarge)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Reset", "Reset"))
					.OnClicked(this, &SPairedAnimTimelineView::HandleResetClicked)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNullWidget::NullWidget
				]

				// Speed control
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.0f, 0.0f, 2.0f, 0.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("Speed", "Speed"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SSpinBox<float>)
					.MinValue(0.1f)
					.MaxValue(2.0f)
					.Value_Lambda([this]() { return ModelPtr ? ModelPtr->PlaybackSpeed : 1.0f; })
					.OnValueChanged_Lambda([this](float Val) { if (ModelPtr) ModelPtr->PlaybackSpeed = Val; })
					.MinDesiredWidth(60.0f)
				]

				// Loop toggle
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 2.0f, 0.0f)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return (ModelPtr && ModelPtr->bLoopPlayback) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { if (ModelPtr) ModelPtr->bLoopPlayback = (State == ECheckBoxState::Checked); })
					.ToolTipText(LOCTEXT("LoopTip", "Loop playback"))
					[
						SNew(STextBlock).Text(LOCTEXT("Loop", "Loop"))
					]
				]
			]
		]
	];
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

void SPairedAnimTimelineView::SyncSliderToModel()
{
	if (!TimelineSlider.IsValid() || !ModelPtr)
	{
		return;
	}

	if (ModelPtr->MaxDuration > ModelPtr->MinTime)
	{
		float Range = ModelPtr->MaxDuration - ModelPtr->MinTime;
		TimelineSlider->SetValue((ModelPtr->CurrentTime - ModelPtr->MinTime) / Range);
	}
}

void SPairedAnimTimelineView::UpdateTimeDisplay()
{
	// Text is bound via delegate, so this just forces invalidation if needed
	if (TimeDisplayText.IsValid())
	{
		TimeDisplayText->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

// ============================================================================
// INTERNAL HANDLERS
// ============================================================================

void SPairedAnimTimelineView::HandleTimelineValueChanged(float NewValue)
{
	OnTimelineValueChangedDelegate.ExecuteIfBound(NewValue);
}

FReply SPairedAnimTimelineView::HandlePlayPauseClicked()
{
	OnPlayPauseClickedDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SPairedAnimTimelineView::HandleStepForward()
{
	OnStepForwardDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SPairedAnimTimelineView::HandleStepBackward()
{
	OnStepBackwardDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SPairedAnimTimelineView::HandleStepForwardLarge()
{
	OnStepForwardLargeDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SPairedAnimTimelineView::HandleStepBackwardLarge()
{
	OnStepBackwardLargeDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SPairedAnimTimelineView::HandleResetClicked()
{
	OnResetClickedDelegate.ExecuteIfBound();
	return FReply::Handled();
}

// ============================================================================
// TEXT HELPERS
// ============================================================================

FText SPairedAnimTimelineView::GetTimeDisplayText() const
{
	if (!ModelPtr)
	{
		return LOCTEXT("NoModel", "No model");
	}

	return FText::Format(
		LOCTEXT("TimeDisplay", "{0} / {1}s  |  Attacker: {2}s  Victim: {3}s"),
		FText::AsNumber(ModelPtr->CurrentTime, &GetNumberFormat(2)),
		FText::AsNumber(ModelPtr->MaxDuration, &GetNumberFormat(2)),
		FText::AsNumber(ModelPtr->GetAttackerTime(), &GetNumberFormat(2)),
		FText::AsNumber(ModelPtr->GetVictimTime(), &GetNumberFormat(2)));
}

FText SPairedAnimTimelineView::GetPlayPauseButtonText() const
{
	if (ModelPtr && ModelPtr->bIsPlaying)
	{
		return LOCTEXT("Pause", "||");
	}
	return LOCTEXT("Play", ">");
}

#undef LOCTEXT_NAMESPACE
