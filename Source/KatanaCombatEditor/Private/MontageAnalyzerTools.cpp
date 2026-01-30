// Copyright Epic Games, Inc. All Rights Reserved.

#include "MontageAnalyzerTools.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "BoneIndices.h"

// ============================================================================
// TIMING ANALYSIS
// ============================================================================

FMontageTimingInfo UMontageAnalyzerTools::GetMontageTiming(UAnimMontage* Montage, FName SectionName)
{
	FMontageTimingInfo Result;

	if (!Montage)
	{
		return Result;
	}

	Result.TotalDuration = Montage->GetPlayLength();
	Result.PlayRate = Montage->RateScale;

	if (SectionName != NAME_None)
	{
		int32 SectionIndex = Montage->GetSectionIndex(SectionName);
		if (SectionIndex != INDEX_NONE)
		{
			Result.SectionName = SectionName;
			Result.SectionStartTime = Montage->GetAnimCompositeSection(SectionIndex).GetTime();
			Result.SectionDuration = Montage->GetSectionLength(SectionIndex);
		}
	}
	else
	{
		Result.SectionDuration = Result.TotalDuration;
	}

	// Extract notify times
	float RangeStart = (SectionName != NAME_None) ? Result.SectionStartTime : 0.0f;
	float RangeEnd = RangeStart + Result.SectionDuration;

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		float NotifyTime = NotifyEvent.GetTriggerTime();
		if (NotifyTime >= RangeStart && NotifyTime <= RangeEnd)
		{
			Result.NotifyTimes.Add(NotifyTime - RangeStart);
			Result.NotifyNames.Add(NotifyEvent.NotifyName);
		}
	}

	return Result;
}

float UMontageAnalyzerTools::GetMontageDuration(UAnimMontage* Montage, FName SectionName)
{
	if (!Montage)
	{
		return 0.0f;
	}

	if (SectionName != NAME_None)
	{
		int32 SectionIndex = Montage->GetSectionIndex(SectionName);
		if (SectionIndex != INDEX_NONE)
		{
			return Montage->GetSectionLength(SectionIndex);
		}
		return 0.0f;
	}

	return Montage->GetPlayLength();
}

float UMontageAnalyzerTools::GetSectionStartTime(UAnimMontage* Montage, FName SectionName)
{
	if (!Montage || SectionName == NAME_None)
	{
		return 0.0f;
	}

	int32 SectionIndex = Montage->GetSectionIndex(SectionName);
	if (SectionIndex != INDEX_NONE)
	{
		return Montage->GetAnimCompositeSection(SectionIndex).GetTime();
	}

	return 0.0f;
}

TArray<FName> UMontageAnalyzerTools::GetAllSectionNames(UAnimMontage* Montage)
{
	TArray<FName> SectionNames;

	if (!Montage)
	{
		return SectionNames;
	}

	for (int32 i = 0; i < Montage->CompositeSections.Num(); ++i)
	{
		SectionNames.Add(Montage->CompositeSections[i].SectionName);
	}

	return SectionNames;
}

float UMontageAnalyzerTools::FindSyncPointTime(UAnimMontage* Montage, FName SectionName)
{
	if (!Montage)
	{
		return -1.0f;
	}

	float RangeStart = 0.0f;
	float RangeEnd = Montage->GetPlayLength();

	if (SectionName != NAME_None)
	{
		int32 SectionIndex = Montage->GetSectionIndex(SectionName);
		if (SectionIndex != INDEX_NONE)
		{
			RangeStart = Montage->GetAnimCompositeSection(SectionIndex).GetTime();
			RangeEnd = RangeStart + Montage->GetSectionLength(SectionIndex);
		}
	}

	// Search for notifies with "Sync" in the class name
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		float NotifyTime = NotifyEvent.GetTriggerTime();
		if (NotifyTime < RangeStart || NotifyTime > RangeEnd)
		{
			continue;
		}

		FString ClassName;
		if (NotifyEvent.Notify)
		{
			ClassName = NotifyEvent.Notify->GetClass()->GetName();
		}
		else if (NotifyEvent.NotifyStateClass)
		{
			ClassName = NotifyEvent.NotifyStateClass->GetName();
		}

		if (ClassName.Contains(TEXT("Sync"), ESearchCase::IgnoreCase) ||
			ClassName.Contains(TEXT("PairedAnimation"), ESearchCase::IgnoreCase))
		{
			return NotifyTime;
		}
	}

	return -1.0f;
}

// ============================================================================
// NOTIFY ANALYSIS
// ============================================================================

TArray<FAnimNotifyEvent> UMontageAnalyzerTools::GetNotifiesInRange(UAnimMontage* Montage, float StartTime, float EndTime)
{
	TArray<FAnimNotifyEvent> Result;

	if (!Montage)
	{
		return Result;
	}

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		float NotifyTime = NotifyEvent.GetTriggerTime();
		if (NotifyTime >= StartTime && NotifyTime <= EndTime)
		{
			Result.Add(NotifyEvent);
		}
	}

	return Result;
}

TArray<FAnimNotifyEvent> UMontageAnalyzerTools::GetNotifiesOfClass(UAnimMontage* Montage, UClass* NotifyClass, FName SectionName)
{
	TArray<FAnimNotifyEvent> Result;

	if (!Montage || !NotifyClass)
	{
		return Result;
	}

	float RangeStart = 0.0f;
	float RangeEnd = Montage->GetPlayLength();

	if (SectionName != NAME_None)
	{
		int32 SectionIndex = Montage->GetSectionIndex(SectionName);
		if (SectionIndex != INDEX_NONE)
		{
			RangeStart = Montage->GetAnimCompositeSection(SectionIndex).GetTime();
			RangeEnd = RangeStart + Montage->GetSectionLength(SectionIndex);
		}
	}

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		float NotifyTime = NotifyEvent.GetTriggerTime();
		if (NotifyTime < RangeStart || NotifyTime > RangeEnd)
		{
			continue;
		}

		if (NotifyEvent.Notify && NotifyEvent.Notify->IsA(NotifyClass))
		{
			Result.Add(NotifyEvent);
		}
		else if (NotifyEvent.NotifyStateClass && NotifyEvent.NotifyStateClass->IsA(NotifyClass))
		{
			Result.Add(NotifyEvent);
		}
	}

	return Result;
}

bool UMontageAnalyzerTools::HasNotifyOfClass(UAnimMontage* Montage, UClass* NotifyClass)
{
	if (!Montage || !NotifyClass)
	{
		return false;
	}

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (NotifyEvent.Notify && NotifyEvent.Notify->IsA(NotifyClass))
		{
			return true;
		}
		if (NotifyEvent.NotifyStateClass && NotifyEvent.NotifyStateClass->IsA(NotifyClass))
		{
			return true;
		}
	}

	return false;
}

TArray<FString> UMontageAnalyzerTools::GetNotifyClassNames(UAnimMontage* Montage)
{
	TSet<FString> UniqueNames;

	if (Montage)
	{
		for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
		{
			if (NotifyEvent.Notify)
			{
				UniqueNames.Add(NotifyEvent.Notify->GetClass()->GetName());
			}
			if (NotifyEvent.NotifyStateClass)
			{
				UniqueNames.Add(NotifyEvent.NotifyStateClass->GetName());
			}
		}
	}

	return UniqueNames.Array();
}

// ============================================================================
// BONE TRAJECTORY ANALYSIS
// ============================================================================

FBoneTrajectoryData UMontageAnalyzerTools::SampleBoneTrajectory(
	UAnimMontage* Montage,
	USkeletalMesh* SkeletalMesh,
	FName BoneName,
	int32 SampleCount,
	FName SectionName)
{
	FBoneTrajectoryData Result;
	Result.BoneName = BoneName;
	Result.TrajectoryBounds.Init();

	if (!Montage || !SkeletalMesh || SampleCount < 2)
	{
		return Result;
	}

	// Get time range
	float StartTime = 0.0f;
	float EndTime = Montage->GetPlayLength();

	if (SectionName != NAME_None)
	{
		int32 SectionIndex = Montage->GetSectionIndex(SectionName);
		if (SectionIndex != INDEX_NONE)
		{
			StartTime = Montage->GetAnimCompositeSection(SectionIndex).GetTime();
			EndTime = StartTime + Montage->GetSectionLength(SectionIndex);
		}
	}

	float Duration = EndTime - StartTime;
	float TimeStep = Duration / (SampleCount - 1);

	// Sample trajectory
	FVector PreviousPosition = FVector::ZeroVector;
	bool bHasPrevious = false;

	for (int32 i = 0; i < SampleCount; ++i)
	{
		float Time = StartTime + (i * TimeStep);

		FBoneTrajectorySample Sample;
		Sample.Time = Time;
		Sample.Transform = GetBoneTransformAtTime(Montage, SkeletalMesh, BoneName, Time);

		FVector Position = Sample.Transform.GetLocation();

		// Calculate velocity from previous sample
		if (bHasPrevious)
		{
			Sample.Velocity = (Position - PreviousPosition) / TimeStep;
			Sample.Speed = Sample.Velocity.Size();

			Result.TotalDistance += FVector::Dist(Position, PreviousPosition);

			if (Sample.Speed > Result.MaxSpeed)
			{
				Result.MaxSpeed = Sample.Speed;
				Result.TimeOfMaxSpeed = Time;
			}
		}

		Result.Samples.Add(Sample);
		Result.TrajectoryBounds += Position;

		PreviousPosition = Position;
		bHasPrevious = true;
	}

	return Result;
}

FVector UMontageAnalyzerTools::GetBoneVelocityAtTime(
	UAnimMontage* Montage,
	USkeletalMesh* SkeletalMesh,
	FName BoneName,
	float Time,
	float DeltaTime)
{
	if (!Montage || !SkeletalMesh || DeltaTime <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	float HalfDelta = DeltaTime * 0.5f;
	float StartTime = FMath::Max(0.0f, Time - HalfDelta);
	float EndTime = FMath::Min(Montage->GetPlayLength(), Time + HalfDelta);

	FTransform StartTransform = GetBoneTransformAtTime(Montage, SkeletalMesh, BoneName, StartTime);
	FTransform EndTransform = GetBoneTransformAtTime(Montage, SkeletalMesh, BoneName, EndTime);

	float ActualDelta = EndTime - StartTime;
	if (ActualDelta <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	return (EndTransform.GetLocation() - StartTransform.GetLocation()) / ActualDelta;
}

float UMontageAnalyzerTools::GetMaxBoneSpeed(
	UAnimMontage* Montage,
	USkeletalMesh* SkeletalMesh,
	FName BoneName,
	FName SectionName)
{
	FBoneTrajectoryData Trajectory = SampleBoneTrajectory(Montage, SkeletalMesh, BoneName, 30, SectionName);
	return Trajectory.MaxSpeed;
}

FTransform UMontageAnalyzerTools::GetBoneTransformAtTime(
	UAnimMontage* Montage,
	USkeletalMesh* SkeletalMesh,
	FName BoneName,
	float Time)
{
	if (!Montage || !SkeletalMesh)
	{
		return FTransform::Identity;
	}

	// Get the skeleton
	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	if (!Skeleton)
	{
		return FTransform::Identity;
	}

	// Find bone index
	int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
	if (BoneIndex == INDEX_NONE)
	{
		return FTransform::Identity;
	}

	// Evaluate pose
	TArray<FTransform> BoneTransforms;
	if (!EvaluatePoseAtTime(Montage, SkeletalMesh, Time, BoneTransforms))
	{
		return FTransform::Identity;
	}

	if (BoneIndex < BoneTransforms.Num())
	{
		return BoneTransforms[BoneIndex];
	}

	return FTransform::Identity;
}

TMap<FName, FBoneTrajectoryData> UMontageAnalyzerTools::SampleMultipleBoneTrajectories(
	UAnimMontage* Montage,
	USkeletalMesh* SkeletalMesh,
	const TArray<FName>& BoneNames,
	int32 SampleCount,
	FName SectionName)
{
	TMap<FName, FBoneTrajectoryData> Results;

	if (!Montage || !SkeletalMesh || SampleCount < 2 || BoneNames.Num() == 0)
	{
		return Results;
	}

	// Initialize results
	for (const FName& BoneName : BoneNames)
	{
		FBoneTrajectoryData& Data = Results.Add(BoneName);
		Data.BoneName = BoneName;
		Data.TrajectoryBounds.Init();
	}

	// Get time range
	float StartTime = 0.0f;
	float EndTime = Montage->GetPlayLength();

	if (SectionName != NAME_None)
	{
		int32 SectionIndex = Montage->GetSectionIndex(SectionName);
		if (SectionIndex != INDEX_NONE)
		{
			StartTime = Montage->GetAnimCompositeSection(SectionIndex).GetTime();
			EndTime = StartTime + Montage->GetSectionLength(SectionIndex);
		}
	}

	float Duration = EndTime - StartTime;
	float TimeStep = Duration / (SampleCount - 1);

	// Previous positions for velocity calculation
	TMap<FName, FVector> PreviousPositions;
	bool bHasPrevious = false;

	// Sample all bones at each time
	for (int32 i = 0; i < SampleCount; ++i)
	{
		float Time = StartTime + (i * TimeStep);

		// Evaluate pose once for all bones
		TArray<FTransform> BoneTransforms;
		if (!EvaluatePoseAtTime(Montage, SkeletalMesh, Time, BoneTransforms))
		{
			continue;
		}

		USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
		if (!Skeleton)
		{
			continue;
		}

		for (const FName& BoneName : BoneNames)
		{
			int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
			if (BoneIndex == INDEX_NONE || BoneIndex >= BoneTransforms.Num())
			{
				continue;
			}

			FBoneTrajectoryData& Data = Results[BoneName];
			FBoneTrajectorySample Sample;
			Sample.Time = Time;
			Sample.Transform = BoneTransforms[BoneIndex];

			FVector Position = Sample.Transform.GetLocation();

			if (bHasPrevious && PreviousPositions.Contains(BoneName))
			{
				FVector PrevPos = PreviousPositions[BoneName];
				Sample.Velocity = (Position - PrevPos) / TimeStep;
				Sample.Speed = Sample.Velocity.Size();

				Data.TotalDistance += FVector::Dist(Position, PrevPos);

				if (Sample.Speed > Data.MaxSpeed)
				{
					Data.MaxSpeed = Sample.Speed;
					Data.TimeOfMaxSpeed = Time;
				}
			}

			Data.Samples.Add(Sample);
			Data.TrajectoryBounds += Position;
			PreviousPositions.FindOrAdd(BoneName) = Position;
		}

		bHasPrevious = true;
	}

	return Results;
}

// ============================================================================
// ROOT MOTION ANALYSIS
// ============================================================================

float UMontageAnalyzerTools::GetRootMotionDistance(UAnimMontage* Montage, FName SectionName)
{
	if (!Montage)
	{
		return 0.0f;
	}

	FTransform TotalTransform = FTransform::Identity;

	// Get time range
	float StartTime = 0.0f;
	float EndTime = Montage->GetPlayLength();

	if (SectionName != NAME_None)
	{
		int32 SectionIndex = Montage->GetSectionIndex(SectionName);
		if (SectionIndex != INDEX_NONE)
		{
			StartTime = Montage->GetAnimCompositeSection(SectionIndex).GetTime();
			EndTime = StartTime + Montage->GetSectionLength(SectionIndex);
		}
	}

	// Sample root motion
	const int32 SampleCount = 30;
	float Duration = EndTime - StartTime;
	float TimeStep = Duration / SampleCount;

	float TotalDistance = 0.0f;
	FVector PreviousPosition = FVector::ZeroVector;

	for (int32 i = 0; i <= SampleCount; ++i)
	{
		float Time = StartTime + (i * TimeStep);
		FTransform RootTransform = GetRootMotionAtTime(Montage, Time);
		FVector Position = RootTransform.GetLocation();

		if (i > 0)
		{
			TotalDistance += FVector::Dist(Position, PreviousPosition);
		}

		PreviousPosition = Position;
	}

	return TotalDistance;
}

FTransform UMontageAnalyzerTools::GetRootMotionAtTime(UAnimMontage* Montage, float Time)
{
	if (!Montage)
	{
		return FTransform::Identity;
	}

	// Extract root motion up to the specified time
	FTransform RootMotion = FTransform::Identity;

	for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
		{
			UAnimSequence* Sequence = Cast<UAnimSequence>(Segment.GetAnimReference());
			if (!Sequence)
			{
				continue;
			}

			float SegmentStart = Segment.StartPos;
			float SegmentEnd = SegmentStart + (Segment.GetLength() * Segment.AnimPlayRate);

			if (Time <= SegmentStart)
			{
				continue;
			}

			float LocalTime = FMath::Clamp(Time - SegmentStart, 0.0f, Segment.GetLength());

			// Extract root motion from sequence using UE5.6 FAnimExtractContext API
			if (Sequence->HasRootMotion())
			{
				FAnimExtractContext ExtractionContext(static_cast<double>(LocalTime));
				FTransform SegmentRootMotion = Sequence->ExtractRootMotion(ExtractionContext);
				RootMotion = SegmentRootMotion * RootMotion;
			}
		}
	}

	return RootMotion;
}

bool UMontageAnalyzerTools::HasRootMotion(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return false;
	}

	for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
		{
			UAnimSequence* Sequence = Cast<UAnimSequence>(Segment.GetAnimReference());
			if (Sequence && Sequence->HasRootMotion())
			{
				return true;
			}
		}
	}

	return false;
}

FVector UMontageAnalyzerTools::GetRootMotionDirectionAtTime(UAnimMontage* Montage, float Time)
{
	if (!Montage)
	{
		return FVector::ForwardVector;
	}

	// Get root motion at time and slightly after
	float DeltaTime = 0.016f;
	FTransform Transform1 = GetRootMotionAtTime(Montage, Time);
	FTransform Transform2 = GetRootMotionAtTime(Montage, Time + DeltaTime);

	FVector Direction = Transform2.GetLocation() - Transform1.GetLocation();
	if (Direction.IsNearlyZero())
	{
		return FVector::ForwardVector;
	}

	return Direction.GetSafeNormal();
}

// ============================================================================
// VALIDATION
// ============================================================================

bool UMontageAnalyzerTools::ValidateMontage(UAnimMontage* Montage, TArray<FAnalysisMessage>& OutMessages)
{
	if (!Montage)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("Montage is null"))
		));
		return false;
	}

	bool bHasErrors = false;

	// Check for animation references
	bool bHasAnySequences = false;
	for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
		{
			if (Segment.GetAnimReference())
			{
				bHasAnySequences = true;
				break;
			}
		}
	}

	if (!bHasAnySequences)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("Montage has no animation sequences"))
		));
		bHasErrors = true;
	}

	// Check duration
	if (Montage->GetPlayLength() <= 0.0f)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("Montage has zero or negative duration"))
		));
		bHasErrors = true;
	}

	// Check for missing sections if referenced
	if (Montage->CompositeSections.Num() == 0)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Warning,
			FText::FromString(TEXT("Montage has no composite sections defined"))
		));
	}

	// Check for notifies
	if (Montage->Notifies.Num() == 0)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Warning,
			FText::FromString(TEXT("Montage has no animation notifies"))
		));
	}

	return !bHasErrors;
}

bool UMontageAnalyzerTools::DoesSectionExist(UAnimMontage* Montage, FName SectionName)
{
	if (!Montage || SectionName == NAME_None)
	{
		return false;
	}

	return Montage->GetSectionIndex(SectionName) != INDEX_NONE;
}

bool UMontageAnalyzerTools::DoesBoneExist(USkeletalMesh* SkeletalMesh, FName BoneName)
{
	if (!SkeletalMesh)
	{
		return false;
	}

	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	if (!Skeleton)
	{
		return false;
	}

	return Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName) != INDEX_NONE;
}

// ============================================================================
// COMPLETE ANALYSIS
// ============================================================================

FMontageAnalysisResult UMontageAnalyzerTools::AnalyzeMontage(
	UAnimMontage* Montage,
	USkeletalMesh* SkeletalMesh,
	const TArray<FName>& BonesToAnalyze,
	FName SectionName)
{
	FMontageAnalysisResult Result;
	Result.AnalyzedMontage = Montage;

	if (!Montage)
	{
		Result.Messages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("Montage is null"))
		));
		return Result;
	}

	// Get timing info
	Result.TimingInfo = GetMontageTiming(Montage, SectionName);

	// Validate montage
	ValidateMontage(Montage, Result.Messages);

	// Analyze bone trajectories if skeleton provided
	if (SkeletalMesh)
	{
		TArray<FName> BonesToTrack = BonesToAnalyze;
		if (BonesToTrack.Num() == 0)
		{
			BonesToTrack = GetDefaultAnalysisBones();
		}

		// Filter to bones that exist
		TArray<FName> ValidBones;
		for (const FName& BoneName : BonesToTrack)
		{
			if (DoesBoneExist(SkeletalMesh, BoneName))
			{
				ValidBones.Add(BoneName);
			}
		}

		if (ValidBones.Num() > 0)
		{
			Result.BoneTrajectories = SampleMultipleBoneTrajectories(
				Montage,
				SkeletalMesh,
				ValidBones,
				30,
				SectionName
			);
		}
	}

	Result.bAnalysisSuccessful = !Result.HasErrors();

	return Result;
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

UAnimSequence* UMontageAnalyzerTools::GetAnimSequenceAtTime(UAnimMontage* Montage, float Time)
{
	if (!Montage)
	{
		return nullptr;
	}

	for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
		{
			float SegmentStart = Segment.StartPos;
			float SegmentEnd = SegmentStart + Segment.GetLength();

			if (Time >= SegmentStart && Time <= SegmentEnd)
			{
				return Cast<UAnimSequence>(Segment.GetAnimReference());
			}
		}
	}

	return nullptr;
}

float UMontageAnalyzerTools::MontageTimeToSequenceTime(UAnimMontage* Montage, float MontageTime)
{
	if (!Montage)
	{
		return 0.0f;
	}

	for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
		{
			float SegmentStart = Segment.StartPos;
			float SegmentEnd = SegmentStart + Segment.GetLength();

			if (MontageTime >= SegmentStart && MontageTime <= SegmentEnd)
			{
				float LocalTime = (MontageTime - SegmentStart) * Segment.AnimPlayRate;
				return Segment.AnimStartTime + LocalTime;
			}
		}
	}

	return 0.0f;
}

TArray<FName> UMontageAnalyzerTools::GetDefaultAnalysisBones()
{
	return TArray<FName>{
		FName(TEXT("hand_r")),
		FName(TEXT("hand_l")),
		FName(TEXT("foot_r")),
		FName(TEXT("foot_l")),
		FName(TEXT("head")),
		FName(TEXT("pelvis")),
		FName(TEXT("WeaponStart")),
		FName(TEXT("WeaponEnd"))
	};
}

bool UMontageAnalyzerTools::EvaluatePoseAtTime(
	UAnimMontage* Montage,
	USkeletalMesh* SkeletalMesh,
	float Time,
	TArray<FTransform>& OutBoneTransforms)
{
	if (!Montage || !SkeletalMesh)
	{
		return false;
	}

	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	if (!Skeleton)
	{
		return false;
	}

	// Get reference pose
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	OutBoneTransforms = RefSkeleton.GetRefBonePose();

	// Find the anim sequence at this time
	for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
		{
			float SegmentStart = Segment.StartPos;
			float SegmentEnd = SegmentStart + Segment.GetLength();

			if (Time >= SegmentStart && Time <= SegmentEnd)
			{
				UAnimSequence* Sequence = Cast<UAnimSequence>(Segment.GetAnimReference());
				if (!Sequence)
				{
					continue;
				}

				// Calculate local time in sequence
				float LocalTime = (Time - SegmentStart) * Segment.AnimPlayRate;
				LocalTime = FMath::Clamp(LocalTime + Segment.AnimStartTime, 0.0f, Sequence->GetPlayLength());

				// Get bone transforms from sequence - UE5.6 requires FAnimExtractContext
				FAnimExtractContext ExtractionContext(static_cast<double>(LocalTime));
				for (int32 BoneIndex = 0; BoneIndex < OutBoneTransforms.Num(); ++BoneIndex)
				{
					FTransform BoneTransform;
					Sequence->GetBoneTransform(BoneTransform, FSkeletonPoseBoneIndex(BoneIndex), ExtractionContext, false);
					OutBoneTransforms[BoneIndex] = BoneTransform;
				}

				return true;
			}
		}
	}

	return true; // Return reference pose if no sequence found
}
