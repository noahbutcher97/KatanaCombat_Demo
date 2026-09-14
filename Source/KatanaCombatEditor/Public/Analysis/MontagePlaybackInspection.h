#pragma once

#include "Animation/AnimMontage.h"
#include "Dom/JsonObject.h"

/** Read-only source/segment clock and authored blend metadata for capture and authoring reports. */
namespace MontagePlaybackInspection
{
inline TSharedRef<FJsonObject> Snapshot(const UAnimMontage& Montage, FName Section)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("montage"), Montage.GetPathName());
	Result->SetNumberField(TEXT("rate_scale"), Montage.RateScale);
	Result->SetNumberField(TEXT("duration_s"), Montage.GetPlayLength());
	Result->SetNumberField(TEXT("blend_in_s"), Montage.BlendIn.GetBlendTime());
	Result->SetNumberField(TEXT("blend_out_s"), Montage.BlendOut.GetBlendTime());
	Result->SetStringField(TEXT("requested_section"), Section.ToString());
	const int32 SectionIndex = Montage.GetSectionIndex(Section);
	Result->SetBoolField(TEXT("section_found"), SectionIndex != INDEX_NONE);
	if (SectionIndex != INDEX_NONE)
	{
		float Start, End; Montage.GetSectionStartAndEndTime(SectionIndex, Start, End);
		Result->SetNumberField(TEXT("section_start_s"), Start); Result->SetNumberField(TEXT("section_end_s"), End);
	}
	TArray<TSharedPtr<FJsonValue>> Slots;
	for (const auto& Track : Montage.SlotAnimTracks)
	{
		auto Slot = MakeShared<FJsonObject>(); Slot->SetStringField(TEXT("slot"), Track.SlotName.ToString());
		TArray<TSharedPtr<FJsonValue>> Segments;
		for (const auto& Segment : Track.AnimTrack.AnimSegments)
		{
			auto Row = MakeShared<FJsonObject>();
			const UAnimSequenceBase* Source = Segment.GetAnimReference();
			Row->SetStringField(TEXT("source"), Source ? Source->GetPathName() : FString());
			Row->SetNumberField(TEXT("track_start_s"), Segment.StartPos);
			Row->SetNumberField(TEXT("source_start_s"), Segment.AnimStartTime);
			Row->SetNumberField(TEXT("source_end_s"), Segment.AnimEndTime);
			Row->SetNumberField(TEXT("segment_play_rate"), Segment.AnimPlayRate);
			Row->SetNumberField(TEXT("source_rate_scale"), Source ? Source->RateScale : 0);
			Row->SetNumberField(TEXT("loop_count"), Segment.LoopingCount);
			Segments.Add(MakeShared<FJsonValueObject>(Row));
		}
		Slot->SetArrayField(TEXT("segments"), Segments); Slots.Add(MakeShared<FJsonValueObject>(Slot));
	}
	Result->SetArrayField(TEXT("slots"), Slots);
	return Result;
}
}
