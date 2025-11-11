// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/SCombatDebugDopeSheet.h"
#include "Core/CombatComponentV2.h"
#include "Rendering/DrawElements.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"

// Color definitions
const FLinearColor SCombatDebugDopeSheet::ComboWindowColor = FLinearColor(0.2f, 0.8f, 0.2f, 0.7f);        // Green
const FLinearColor SCombatDebugDopeSheet::ParryWindowColor = FLinearColor(0.8f, 0.8f, 0.2f, 0.7f);        // Yellow
const FLinearColor SCombatDebugDopeSheet::CancelWindowColor = FLinearColor(0.8f, 0.4f, 0.2f, 0.7f);       // Orange
const FLinearColor SCombatDebugDopeSheet::HoldWindowColor = FLinearColor(0.6f, 0.2f, 0.8f, 0.7f);         // Purple
const FLinearColor SCombatDebugDopeSheet::RecoveryWindowColor = FLinearColor(0.2f, 0.6f, 0.8f, 0.7f);     // Blue
const FLinearColor SCombatDebugDopeSheet::InputPressColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);         // Bright Green
const FLinearColor SCombatDebugDopeSheet::InputReleaseColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);       // Bright Red
const FLinearColor SCombatDebugDopeSheet::ActionPendingColor = FLinearColor(0.7f, 0.7f, 0.7f, 0.8f);      // Light Gray
const FLinearColor SCombatDebugDopeSheet::ActionExecutingColor = FLinearColor(0.2f, 1.0f, 0.2f, 0.9f);    // Bright Green
const FLinearColor SCombatDebugDopeSheet::ActionCompletedColor = FLinearColor(0.2f, 0.2f, 1.0f, 0.6f);    // Blue
const FLinearColor SCombatDebugDopeSheet::ActionCancelledColor = FLinearColor(1.0f, 0.2f, 0.2f, 0.6f);    // Red
const FLinearColor SCombatDebugDopeSheet::PlayheadColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);           // Red
const FLinearColor SCombatDebugDopeSheet::GridLineColor = FLinearColor(0.3f, 0.3f, 0.3f, 0.5f);           // Dark Gray
const FLinearColor SCombatDebugDopeSheet::BackgroundColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.95f);     // Almost Black

void SCombatDebugDopeSheet::Construct(const FArguments& InArgs, UCombatComponentV2* InCombatComponent)
{
	CombatComponent = InCombatComponent;
	ViewRangeMin = InArgs._ViewRangeMin;
	ViewRangeMax = InArgs._ViewRangeMax;
	CurrentTime = InArgs._CurrentTime;

	BuildTracks();
}

int32 SCombatDebugDopeSheet::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// Draw background
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId++,
		AllottedGeometry.ToPaintGeometry(),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		BackgroundColor
	);

	// Draw grid
	DrawGrid(AllottedGeometry, OutDrawElements, LayerId);

	// Draw timeline
	DrawTimeline(AllottedGeometry, OutDrawElements, LayerId);

	// Draw tracks
	DrawTracks(AllottedGeometry, OutDrawElements, LayerId);

	// Draw playhead (on top)
	DrawPlayhead(AllottedGeometry, OutDrawElements, LayerId);

	return LayerId;
}

FVector2D SCombatDebugDopeSheet::ComputeDesiredSize(float) const
{
	const float TotalHeight = TimelineHeight + (Tracks.Num() * (TrackHeight + TrackSpacing)) + 20.0f;
	return FVector2D(800.0f, TotalHeight);
}

void SCombatDebugDopeSheet::RefreshData()
{
	if (!CombatComponent.IsValid())
	{
		return;
	}

	// Rebuild tracks with current data
	BuildTracks();
}

void SCombatDebugDopeSheet::SetViewRange(float Min, float Max)
{
	ViewRangeMin = Min;
	ViewRangeMax = Max;
}

void SCombatDebugDopeSheet::SetCurrentTime(float Time)
{
	CurrentTime = Time;
}

void SCombatDebugDopeSheet::BuildTracks()
{
	Tracks.Empty();

	if (!CombatComponent.IsValid())
	{
		return;
	}

	// Window tracks
	AddWindowTrack(TEXT("Combo Window"), EActionWindowType::Combo, ComboWindowColor);
	AddWindowTrack(TEXT("Parry Window"), EActionWindowType::Parry, ParryWindowColor);
	AddWindowTrack(TEXT("Cancel Window"), EActionWindowType::Cancel, CancelWindowColor);
	AddWindowTrack(TEXT("Hold Window"), EActionWindowType::Hold, HoldWindowColor);
	AddWindowTrack(TEXT("Recovery Window"), EActionWindowType::Recovery, RecoveryWindowColor);

	// Input event track
	AddInputEventTrack();

	// Action queue track
	AddActionQueueTrack();
}

void SCombatDebugDopeSheet::AddWindowTrack(const FString& Name, EActionWindowType WindowType, FLinearColor Color)
{
	if (!CombatComponent.IsValid())
	{
		return;
	}

	FDopeSheetTrack Track(Name, Color, TrackHeight);

	// Find all checkpoints of this type
	const TArray<FTimerCheckpoint>& Checkpoints = CombatComponent->Checkpoints;
	for (const FTimerCheckpoint& Checkpoint : Checkpoints)
	{
		if (Checkpoint.WindowType == WindowType)
		{
			// Add as duration bar
			Track.Events.Add(FDopeSheetEvent(
				Checkpoint.MontageTime,
				Name,
				Color,
				true, // Is duration
				Checkpoint.Duration
			));
		}
	}

	Tracks.Add(Track);
}

void SCombatDebugDopeSheet::AddInputEventTrack()
{
	if (!CombatComponent.IsValid())
	{
		return;
	}

	FDopeSheetTrack Track(TEXT("Input Events"), FLinearColor::White, TrackHeight);

	// Add held inputs (currently pressed)
	const TMap<EInputType, float>& HeldInputs = CombatComponent->HeldInputs;
	for (const TPair<EInputType, float>& Pair : HeldInputs)
	{
		FString InputName = UEnum::GetValueAsString(Pair.Key);
		InputName.RemoveFromStart(TEXT("EInputType::"));

		Track.Events.Add(FDopeSheetEvent(
			Pair.Value,
			InputName + TEXT(" (Press)"),
			InputPressColor,
			false // Marker
		));
	}

	// Note: Release events are not stored, only press events
	// In a full implementation, you'd store a history of press/release pairs

	Tracks.Add(Track);
}

void SCombatDebugDopeSheet::AddActionQueueTrack()
{
	if (!CombatComponent.IsValid())
	{
		return;
	}

	FDopeSheetTrack Track(TEXT("Action Queue"), FLinearColor::White, TrackHeight);

	// Add queued actions
	const TArray<FActionQueueEntry>& ActionQueue = CombatComponent->ActionQueue;
	for (const FActionQueueEntry& Action : ActionQueue)
	{
		FLinearColor StateColor;
		FString StateName;

		switch (Action.State)
		{
			case EActionState::Pending:
				StateColor = ActionPendingColor;
				StateName = TEXT("Pending");
				break;
			case EActionState::Executing:
				StateColor = ActionExecutingColor;
				StateName = TEXT("Executing");
				break;
			case EActionState::Completed:
				StateColor = ActionCompletedColor;
				StateName = TEXT("Completed");
				break;
			case EActionState::Cancelled:
				StateColor = ActionCancelledColor;
				StateName = TEXT("Cancelled");
				break;
			default:
				StateColor = FLinearColor::White;
				StateName = TEXT("Unknown");
				break;
		}

		FString InputName = UEnum::GetValueAsString(Action.InputAction.InputType);
		InputName.RemoveFromStart(TEXT("EInputType::"));

		FString Label = FString::Printf(TEXT("%s (%s)"), *InputName, *StateName);

		Track.Events.Add(FDopeSheetEvent(
			Action.ScheduledTime,
			Label,
			StateColor,
			false // Marker
		));
	}

	Tracks.Add(Track);
}

void SCombatDebugDopeSheet::DrawTimeline(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float TimelineWidth = LocalSize.X - HeaderWidth;

	// Draw timeline background
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId++,
		AllottedGeometry.ToPaintGeometry(
			FVector2f(TimelineWidth, TimelineHeight),
			FSlateLayoutTransform(FVector2f(HeaderWidth, 0.0f))
		),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor(0.1f, 0.1f, 0.1f, 1.0f)
	);

	// Draw time markers
	const FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle("Regular");
	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	const float TimeRange = ViewRangeMax - ViewRangeMin;
	const float TimeStep = FMath::Max(0.5f, FMath::RoundToFloat(TimeRange / 10.0f));

	for (float Time = FMath::FloorToFloat(ViewRangeMin / TimeStep) * TimeStep; Time <= ViewRangeMax; Time += TimeStep)
	{
		const float X = TimeToPixel(Time, TimelineWidth) + HeaderWidth;
		const FString TimeText = FString::Printf(TEXT("%.1fs"), Time);

		// Draw tick mark
		const TArray<FVector2D> LinePoints = {
			FVector2D(X, TimelineHeight - 5.0f),
			FVector2D(X, TimelineHeight)
		};

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			FLinearColor::White,
			true,
			1.0f
		);

		// Draw time text
		const FVector2D TextSize = FontMeasure->Measure(TimeText, FontInfo);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(TextSize),
				FSlateLayoutTransform(FVector2f(X - TextSize.X * 0.5f, 2.0f))
			),
			TimeText,
			FontInfo,
			ESlateDrawEffect::None,
			FLinearColor::White
		);
	}

	LayerId++;
}

void SCombatDebugDopeSheet::DrawTracks(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float TimelineWidth = LocalSize.X - HeaderWidth;
	const FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle("Regular");

	for (int32 TrackIndex = 0; TrackIndex < Tracks.Num(); ++TrackIndex)
	{
		const FDopeSheetTrack& Track = Tracks[TrackIndex];
		const float TrackY = GetTrackYOffset(TrackIndex);

		// Draw track header
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(HeaderWidth, TrackHeight),
				FSlateLayoutTransform(FVector2f(0.0f, TrackY))
			),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FLinearColor(0.15f, 0.15f, 0.15f, 1.0f)
		);

		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(HeaderWidth - 10.0f, TrackHeight - 10.0f),
				FSlateLayoutTransform(FVector2f(5.0f, TrackY + 5.0f))
			),
			Track.TrackName,
			FontInfo,
			ESlateDrawEffect::None,
			Track.TrackColor
		);

		// Draw track background
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(TimelineWidth, TrackHeight),
				FSlateLayoutTransform(FVector2f(HeaderWidth, TrackY))
			),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FLinearColor(0.08f, 0.08f, 0.08f, 1.0f)
		);

		LayerId++;

		// Draw events
		for (const FDopeSheetEvent& Event : Track.Events)
		{
			const float EventX = TimeToPixel(Event.Time, TimelineWidth) + HeaderWidth;

			if (Event.bIsDuration)
			{
				// Draw as bar
				const float EventWidth = (Event.Duration / (ViewRangeMax - ViewRangeMin)) * TimelineWidth;

				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(
						FVector2f(EventWidth, TrackHeight - 4.0f),
						FSlateLayoutTransform(FVector2f(EventX, TrackY + 2.0f))
					),
					FCoreStyle::Get().GetBrush("WhiteBrush"),
					ESlateDrawEffect::None,
					Event.Color
				);
			}
			else
			{
				// Draw as marker (diamond/circle)
				const float MarkerSize = 8.0f;
				const FVector2D MarkerCenter(EventX, TrackY + TrackHeight * 0.5f);

				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(
						FVector2f(MarkerSize, MarkerSize),
						FSlateLayoutTransform(FVector2f(MarkerCenter.X - MarkerSize * 0.5f, MarkerCenter.Y - MarkerSize * 0.5f))
					),
					FCoreStyle::Get().GetBrush("WhiteBrush"),
					ESlateDrawEffect::None,
					Event.Color
				);
			}
		}

		LayerId++;
	}
}

void SCombatDebugDopeSheet::DrawPlayhead(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float TimelineWidth = LocalSize.X - HeaderWidth;
	const float PlayheadX = TimeToPixel(CurrentTime, TimelineWidth) + HeaderWidth;

	// Draw playhead line
	const TArray<FVector2D> LinePoints = {
		FVector2D(PlayheadX, 0.0f),
		FVector2D(PlayheadX, LocalSize.Y)
	};

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId++,
		AllottedGeometry.ToPaintGeometry(),
		LinePoints,
		ESlateDrawEffect::None,
		PlayheadColor,
		true,
		2.0f
	);

	// Draw playhead indicator at top (small box)
	const float IndicatorSize = 6.0f;
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId++,
		AllottedGeometry.ToPaintGeometry(
			FVector2f(IndicatorSize, IndicatorSize),
			FSlateLayoutTransform(FVector2f(PlayheadX - IndicatorSize * 0.5f, 0.0f))
		),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		PlayheadColor
	);
}

void SCombatDebugDopeSheet::DrawGrid(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float TimelineWidth = LocalSize.X - HeaderWidth;

	// Draw vertical grid lines at 0.5s intervals
	const float TimeRange = ViewRangeMax - ViewRangeMin;
	const float GridStep = 0.5f;

	for (float Time = FMath::CeilToFloat(ViewRangeMin / GridStep) * GridStep; Time <= ViewRangeMax; Time += GridStep)
	{
		const float X = TimeToPixel(Time, TimelineWidth) + HeaderWidth;

		const TArray<FVector2D> LinePoints = {
			FVector2D(X, TimelineHeight),
			FVector2D(X, LocalSize.Y)
		};

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			GridLineColor,
			true,
			1.0f
		);
	}

	LayerId++;
}

float SCombatDebugDopeSheet::TimeToPixel(float Time, float AvailableWidth) const
{
	const float TimeRange = ViewRangeMax - ViewRangeMin;
	if (TimeRange <= 0.0f)
	{
		return 0.0f;
	}

	return ((Time - ViewRangeMin) / TimeRange) * AvailableWidth;
}

float SCombatDebugDopeSheet::PixelToTime(float Pixel, float AvailableWidth) const
{
	const float TimeRange = ViewRangeMax - ViewRangeMin;
	return ViewRangeMin + (Pixel / AvailableWidth) * TimeRange;
}

float SCombatDebugDopeSheet::GetTrackYOffset(int32 TrackIndex) const
{
	return TimelineHeight + (TrackIndex * (TrackHeight + TrackSpacing));
}
