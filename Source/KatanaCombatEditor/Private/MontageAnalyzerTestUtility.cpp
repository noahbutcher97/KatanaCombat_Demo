// Copyright Epic Games, Inc. All Rights Reserved.

#include "MontageAnalyzerTestUtility.h"
#include "MontageAnalyzerTools.h"
#include "MontageAnalysisTypes.h"
#include "Data/PairedAnimationData.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

void UMontageAnalyzerTestUtility::AnalyzeMontage(UAnimMontage* Montage, FName SectionName)
{
	if (!Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("MontageAnalyzerTest: No montage provided"));
		return;
	}

	PrintHeader(FString::Printf(TEXT("ANALYZING: %s"), *Montage->GetName()));

	// Basic info
	PrintValue(TEXT("Duration"), FString::Printf(TEXT("%.3f seconds"), Montage->GetPlayLength()));
	PrintValue(TEXT("Blend In"), FString::Printf(TEXT("%.3f seconds"), Montage->BlendIn.GetBlendTime()));
	PrintValue(TEXT("Blend Out"), FString::Printf(TEXT("%.3f seconds"), Montage->BlendOut.GetBlendTime()));

	// Section info
	if (SectionName != NAME_None)
	{
		int32 SectionIndex = Montage->GetSectionIndex(SectionName);
		if (SectionIndex != INDEX_NONE)
		{
			float SectionStart, SectionEnd;
			Montage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
			PrintValue(TEXT("Section"), SectionName.ToString());
			PrintValue(TEXT("Section Start"), FString::Printf(TEXT("%.3f"), SectionStart));
			PrintValue(TEXT("Section End"), FString::Printf(TEXT("%.3f"), SectionEnd));
			PrintValue(TEXT("Section Duration"), FString::Printf(TEXT("%.3f"), SectionEnd - SectionStart));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Section '%s' not found in montage"), *SectionName.ToString());
		}
	}

	// List all sections
	PrintHeader(TEXT("SECTIONS"));
	const TArray<FCompositeSection>& Sections = Montage->CompositeSections;
	for (int32 i = 0; i < Sections.Num(); ++i)
	{
		float Start, End;
		Montage->GetSectionStartAndEndTime(i, Start, End);
		PrintValue(Sections[i].SectionName.ToString(), FString::Printf(TEXT("%.3f - %.3f (%.3f s)"), Start, End, End - Start));
	}
}

void UMontageAnalyzerTestUtility::PrintTimingAnalysis(UAnimMontage* Montage, FName SectionName)
{
	if (!Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("MontageAnalyzerTest: No montage provided"));
		return;
	}

	PrintHeader(FString::Printf(TEXT("TIMING ANALYSIS: %s"), *Montage->GetName()));

	FMontageTimingInfo TimingInfo = UMontageAnalyzerTools::GetMontageTiming(Montage, SectionName);

	PrintValue(TEXT("Total Duration"), FString::Printf(TEXT("%.3f s"), TimingInfo.TotalDuration));
	PrintValue(TEXT("Section Name"), TimingInfo.SectionName != NAME_None ? TimingInfo.SectionName.ToString() : TEXT("(whole montage)"));
	PrintValue(TEXT("Section Start"), FString::Printf(TEXT("%.3f s"), TimingInfo.SectionStartTime));
	PrintValue(TEXT("Section Duration"), FString::Printf(TEXT("%.3f s"), TimingInfo.SectionDuration));
	PrintValue(TEXT("Play Rate"), FString::Printf(TEXT("%.2fx"), TimingInfo.PlayRate));

	// Print notify times if available
	if (TimingInfo.NotifyTimes.Num() > 0)
	{
		PrintHeader(TEXT("NOTIFIES"));
		for (int32 i = 0; i < TimingInfo.NotifyTimes.Num(); ++i)
		{
			FString NotifyName = (i < TimingInfo.NotifyNames.Num()) ? TimingInfo.NotifyNames[i].ToString() : TEXT("Unknown");
			PrintValue(NotifyName, FString::Printf(TEXT("%.3f s"), TimingInfo.NotifyTimes[i]));
		}
	}

	// Find sync point time
	float SyncPointTime = UMontageAnalyzerTools::FindSyncPointTime(Montage, SectionName);
	if (SyncPointTime >= 0.0f)
	{
		PrintValue(TEXT("Sync Point Time"), FString::Printf(TEXT("%.3f s"), SyncPointTime));
	}
	else
	{
		PrintValue(TEXT("Sync Point Time"), TEXT("(none found)"));
	}
}

void UMontageAnalyzerTestUtility::PrintSyncPointInfo(UAnimMontage* Montage)
{
	if (!Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("MontageAnalyzerTest: No montage provided"));
		return;
	}

	PrintHeader(FString::Printf(TEXT("SYNC POINTS: %s"), *Montage->GetName()));

	// Use FindSyncPointTime to search for sync point
	float SyncTime = UMontageAnalyzerTools::FindSyncPointTime(Montage);

	if (SyncTime < 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("No sync points found in montage"));
		return;
	}

	PrintValue(TEXT("Sync Point Time"), FString::Printf(TEXT("%.3f s"), SyncTime));

	// Also list any notifies containing "Sync" in the name
	TArray<FString> NotifyNames = UMontageAnalyzerTools::GetNotifyClassNames(Montage);
	PrintHeader(TEXT("SYNC-RELATED NOTIFIES"));
	for (const FString& Name : NotifyNames)
	{
		if (Name.Contains(TEXT("Sync"), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTemp, Display, TEXT("  - %s"), *Name);
		}
	}
}

void UMontageAnalyzerTestUtility::PrintNotifyList(UAnimMontage* Montage)
{
	if (!Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("MontageAnalyzerTest: No montage provided"));
		return;
	}

	PrintHeader(FString::Printf(TEXT("NOTIFIES: %s"), *Montage->GetName()));

	TArray<FString> NotifyNames = UMontageAnalyzerTools::GetNotifyClassNames(Montage);

	if (NotifyNames.Num() == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("  (no notifies found)"));
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("--- Notify Types ---"));
	for (const FString& Name : NotifyNames)
	{
		UE_LOG(LogTemp, Display, TEXT("  - %s"), *Name);
	}

	// Also print notify event details
	UE_LOG(LogTemp, Display, TEXT("--- Notify Events ---"));
	for (const FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		if (Notify.Notify)
		{
			PrintValue(Notify.Notify->GetClass()->GetName(),
				FString::Printf(TEXT("Time: %.3f"), Notify.GetTriggerTime()));
		}
		else if (Notify.NotifyStateClass)
		{
			PrintValue(Notify.NotifyStateClass->GetClass()->GetName(),
				FString::Printf(TEXT("%.3f - %.3f (%.3f s)"),
					Notify.GetTriggerTime(),
					Notify.GetTriggerTime() + Notify.GetDuration(),
					Notify.GetDuration()));
		}
	}
}

void UMontageAnalyzerTestUtility::PrintRootMotionAnalysis(UAnimMontage* Montage)
{
	if (!Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("MontageAnalyzerTest: No montage provided"));
		return;
	}

	PrintHeader(FString::Printf(TEXT("ROOT MOTION: %s"), *Montage->GetName()));

	bool bHasRootMotion = UMontageAnalyzerTools::HasRootMotion(Montage);
	PrintValue(TEXT("Has Root Motion"), bHasRootMotion ? TEXT("Yes") : TEXT("No"));

	if (!bHasRootMotion)
	{
		return;
	}

	float TotalDistance = UMontageAnalyzerTools::GetRootMotionDistance(Montage);
	PrintValue(TEXT("Total Distance"), FString::Printf(TEXT("%.1f units"), TotalDistance));

	// Get root motion at start, middle, and end
	float Duration = Montage->GetPlayLength();
	FTransform StartTransform = UMontageAnalyzerTools::GetRootMotionAtTime(Montage, 0.0f);
	FTransform MidTransform = UMontageAnalyzerTools::GetRootMotionAtTime(Montage, Duration * 0.5f);
	FTransform EndTransform = UMontageAnalyzerTools::GetRootMotionAtTime(Montage, Duration);

	PrintValue(TEXT("Start Location"), FString::Printf(TEXT("(%.1f, %.1f, %.1f)"),
		StartTransform.GetLocation().X, StartTransform.GetLocation().Y, StartTransform.GetLocation().Z));
	PrintValue(TEXT("Mid Location"), FString::Printf(TEXT("(%.1f, %.1f, %.1f)"),
		MidTransform.GetLocation().X, MidTransform.GetLocation().Y, MidTransform.GetLocation().Z));
	PrintValue(TEXT("End Location"), FString::Printf(TEXT("(%.1f, %.1f, %.1f)"),
		EndTransform.GetLocation().X, EndTransform.GetLocation().Y, EndTransform.GetLocation().Z));

	// Get direction at a few key points
	FVector MidDirection = UMontageAnalyzerTools::GetRootMotionDirectionAtTime(Montage, Duration * 0.5f);
	PrintValue(TEXT("Mid Direction"), FString::Printf(TEXT("(%.2f, %.2f, %.2f)"),
		MidDirection.X, MidDirection.Y, MidDirection.Z));
}

void UMontageAnalyzerTestUtility::PrintBoneTrajectory(UAnimMontage* Montage, FName BoneName, int32 SampleCount)
{
	if (!Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("MontageAnalyzerTest: No montage provided"));
		return;
	}

	PrintHeader(FString::Printf(TEXT("BONE TRAJECTORY: %s - %s"), *Montage->GetName(), *BoneName.ToString()));

	// Note: SampleBoneTrajectory requires a SkeletalMesh which we don't have in this utility
	// We'd need to find a way to get a reference skeleton, or modify the API
	UE_LOG(LogTemp, Warning, TEXT("Bone trajectory analysis requires a SkeletalMesh reference."));
	UE_LOG(LogTemp, Warning, TEXT("To analyze bone trajectories, use AnalyzeMontage() which can accept a skeleton."));

	// We can still check if the section/montage exists
	PrintValue(TEXT("Montage Duration"), FString::Printf(TEXT("%.3f s"), Montage->GetPlayLength()));
	PrintValue(TEXT("Requested Bone"), BoneName.ToString());
	PrintValue(TEXT("Requested Samples"), FString::FromInt(SampleCount));
}

void UMontageAnalyzerTestUtility::RunFullAnalysis(UAnimMontage* Montage)
{
	if (!Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("MontageAnalyzerTest: No montage provided"));
		return;
	}

	UE_LOG(LogTemp, Display, TEXT(""));
	UE_LOG(LogTemp, Display, TEXT("========================================================"));
	UE_LOG(LogTemp, Display, TEXT("FULL MONTAGE ANALYSIS: %s"), *Montage->GetName());
	UE_LOG(LogTemp, Display, TEXT("========================================================"));
	UE_LOG(LogTemp, Display, TEXT(""));

	AnalyzeMontage(Montage);
	UE_LOG(LogTemp, Display, TEXT(""));

	PrintTimingAnalysis(Montage);
	UE_LOG(LogTemp, Display, TEXT(""));

	PrintSyncPointInfo(Montage);
	UE_LOG(LogTemp, Display, TEXT(""));

	PrintNotifyList(Montage);
	UE_LOG(LogTemp, Display, TEXT(""));

	PrintRootMotionAnalysis(Montage);
	UE_LOG(LogTemp, Display, TEXT(""));

	// Validation
	PrintHeader(TEXT("VALIDATION"));
	TArray<FAnalysisMessage> Messages;
	bool bIsValid = UMontageAnalyzerTools::ValidateMontage(Montage, Messages);
	PrintValue(TEXT("Valid"), bIsValid ? TEXT("Yes") : TEXT("No"));

	for (const FAnalysisMessage& Msg : Messages)
	{
		FString Severity;
		switch (Msg.Severity)
		{
		case EAnalysisMessageSeverity::Info: Severity = TEXT("[INFO]"); break;
		case EAnalysisMessageSeverity::Warning: Severity = TEXT("[WARN]"); break;
		case EAnalysisMessageSeverity::Error: Severity = TEXT("[ERROR]"); break;
		}
		UE_LOG(LogTemp, Display, TEXT("  %s %s"), *Severity, *Msg.Message.ToString());
	}

	UE_LOG(LogTemp, Display, TEXT(""));
	UE_LOG(LogTemp, Display, TEXT("========================================================"));
	UE_LOG(LogTemp, Display, TEXT("END ANALYSIS"));
	UE_LOG(LogTemp, Display, TEXT("========================================================"));
}

void UMontageAnalyzerTestUtility::ValidatePairedAnimationData(UPairedAnimationData* PairedData)
{
	if (!PairedData)
	{
		UE_LOG(LogTemp, Error, TEXT("MontageAnalyzerTest: No PairedAnimationData provided"));
		return;
	}

	PrintHeader(FString::Printf(TEXT("VALIDATING: %s"), *PairedData->GetName()));

	TArray<FAnalysisMessage> AllMessages;

	// Check montages exist
	if (!PairedData->AttackerMontage)
	{
		AllMessages.Add(FAnalysisMessage(EAnalysisMessageSeverity::Error, FText::FromString(TEXT("Missing AttackerMontage"))));
	}
	if (!PairedData->VictimMontage)
	{
		AllMessages.Add(FAnalysisMessage(EAnalysisMessageSeverity::Error, FText::FromString(TEXT("Missing VictimMontage"))));
	}

	if (PairedData->AttackerMontage && PairedData->VictimMontage)
	{
		// Validate individual montages
		TArray<FAnalysisMessage> AttackerMessages;
		TArray<FAnalysisMessage> VictimMessages;

		UMontageAnalyzerTools::ValidateMontage(PairedData->AttackerMontage, AttackerMessages);
		UMontageAnalyzerTools::ValidateMontage(PairedData->VictimMontage, VictimMessages);

		for (FAnalysisMessage& Msg : AttackerMessages)
		{
			Msg.Message = FText::FromString(FString::Printf(TEXT("[Attacker] %s"), *Msg.Message.ToString()));
			AllMessages.Add(Msg);
		}
		for (FAnalysisMessage& Msg : VictimMessages)
		{
			Msg.Message = FText::FromString(FString::Printf(TEXT("[Victim] %s"), *Msg.Message.ToString()));
			AllMessages.Add(Msg);
		}

		// Check timing
		float AttackerDuration = PairedData->AttackerMontage->GetPlayLength();
		float VictimDuration = PairedData->VictimMontage->GetPlayLength();

		PrintValue(TEXT("Attacker Duration"), FString::Printf(TEXT("%.3f s"), AttackerDuration));
		PrintValue(TEXT("Victim Duration"), FString::Printf(TEXT("%.3f s"), VictimDuration));
		PrintValue(TEXT("Sync Point Time"), FString::Printf(TEXT("%.3f s"), PairedData->SyncPointTime));

		if (PairedData->SyncPointTime > AttackerDuration)
		{
			AllMessages.Add(FAnalysisMessage(
				EAnalysisMessageSeverity::Error,
				FText::FromString(FString::Printf(TEXT("SyncPointTime (%.3f) exceeds AttackerMontage duration (%.3f)"),
					PairedData->SyncPointTime, AttackerDuration))));
		}

		float DurationDiff = FMath::Abs(AttackerDuration - VictimDuration);
		if (DurationDiff > 0.5f)
		{
			AllMessages.Add(FAnalysisMessage(
				EAnalysisMessageSeverity::Warning,
				FText::FromString(FString::Printf(TEXT("Montage duration mismatch: %.3f seconds difference"), DurationDiff))));
		}

		// Check sections if specified
		if (PairedData->AttackerMontageSection != NAME_None)
		{
			if (!UMontageAnalyzerTools::DoesSectionExist(PairedData->AttackerMontage, PairedData->AttackerMontageSection))
			{
				AllMessages.Add(FAnalysisMessage(
					EAnalysisMessageSeverity::Error,
					FText::FromString(FString::Printf(TEXT("AttackerMontageSection '%s' does not exist"),
						*PairedData->AttackerMontageSection.ToString()))));
			}
		}
		if (PairedData->VictimMontageSection != NAME_None)
		{
			if (!UMontageAnalyzerTools::DoesSectionExist(PairedData->VictimMontage, PairedData->VictimMontageSection))
			{
				AllMessages.Add(FAnalysisMessage(
					EAnalysisMessageSeverity::Error,
					FText::FromString(FString::Printf(TEXT("VictimMontageSection '%s' does not exist"),
						*PairedData->VictimMontageSection.ToString()))));
			}
		}
	}

	// Print results
	int32 ErrorCount = 0;
	int32 WarningCount = 0;

	PrintHeader(TEXT("MESSAGES"));
	if (AllMessages.Num() == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("  (none)"));
	}
	for (const FAnalysisMessage& Msg : AllMessages)
	{
		FString Severity;
		switch (Msg.Severity)
		{
		case EAnalysisMessageSeverity::Info:
			Severity = TEXT("[INFO]");
			break;
		case EAnalysisMessageSeverity::Warning:
			Severity = TEXT("[WARN]");
			WarningCount++;
			break;
		case EAnalysisMessageSeverity::Error:
			Severity = TEXT("[ERROR]");
			ErrorCount++;
			break;
		}
		UE_LOG(LogTemp, Display, TEXT("  %s %s"), *Severity, *Msg.Message.ToString());
	}

	PrintHeader(TEXT("RESULT"));
	if (ErrorCount == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("  VALID - Ready for use (%d warnings)"), WarningCount);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  INVALID - Fix %d error(s) before use"), ErrorCount);
	}
}

void UMontageAnalyzerTestUtility::AnalyzePairedMontageSync(UAnimMontage* AttackerMontage, UAnimMontage* VictimMontage, float ExpectedSyncTime)
{
	if (!AttackerMontage || !VictimMontage)
	{
		UE_LOG(LogTemp, Error, TEXT("MontageAnalyzerTest: Missing attacker or victim montage"));
		return;
	}

	PrintHeader(TEXT("PAIRED MONTAGE SYNC ANALYSIS"));
	PrintValue(TEXT("Attacker"), AttackerMontage->GetName());
	PrintValue(TEXT("Victim"), VictimMontage->GetName());
	PrintValue(TEXT("Expected Sync"), FString::Printf(TEXT("%.3f s"), ExpectedSyncTime));

	// Find sync points
	float AttackerSyncTime = UMontageAnalyzerTools::FindSyncPointTime(AttackerMontage);
	float VictimSyncTime = UMontageAnalyzerTools::FindSyncPointTime(VictimMontage);

	PrintHeader(TEXT("SYNC POINT TIMES"));
	PrintValue(TEXT("Attacker Sync"), AttackerSyncTime >= 0 ?
		FString::Printf(TEXT("%.3f s"), AttackerSyncTime) : TEXT("(not found)"));
	PrintValue(TEXT("Victim Sync"), VictimSyncTime >= 0 ?
		FString::Printf(TEXT("%.3f s"), VictimSyncTime) : TEXT("(not found)"));

	// Duration comparison
	PrintHeader(TEXT("DURATIONS"));
	PrintValue(TEXT("Attacker Duration"), FString::Printf(TEXT("%.3f s"), AttackerMontage->GetPlayLength()));
	PrintValue(TEXT("Victim Duration"), FString::Printf(TEXT("%.3f s"), VictimMontage->GetPlayLength()));

	// Check alignment
	PrintHeader(TEXT("ALIGNMENT ANALYSIS"));
	if (AttackerSyncTime >= 0 && VictimSyncTime >= 0)
	{
		float SyncDiff = FMath::Abs(AttackerSyncTime - VictimSyncTime);
		PrintValue(TEXT("Sync Difference"), FString::Printf(TEXT("%.3f s"), SyncDiff));

		if (SyncDiff < 0.05f)
		{
			UE_LOG(LogTemp, Display, TEXT("  Sync timing is EXCELLENT (< 50ms difference)"));
		}
		else if (SyncDiff < 0.1f)
		{
			UE_LOG(LogTemp, Display, TEXT("  Sync timing is GOOD (< 100ms difference)"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  Sync timing may need adjustment (%.0fms difference)"), SyncDiff * 1000.0f);
		}
	}

	// Compare with expected
	if (ExpectedSyncTime > 0.0f && AttackerSyncTime >= 0.0f)
	{
		float ExpectedDiff = FMath::Abs(ExpectedSyncTime - AttackerSyncTime);
		if (ExpectedDiff > 0.1f)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Expected sync (%.3f) differs from found sync (%.3f) by %.0fms"),
				ExpectedSyncTime, AttackerSyncTime, ExpectedDiff * 1000.0f);
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("  Expected sync matches found sync (within 100ms)"));
		}
	}
}

void UMontageAnalyzerTestUtility::PrintHeader(const FString& Title)
{
	UE_LOG(LogTemp, Display, TEXT(""));
	UE_LOG(LogTemp, Display, TEXT("=== %s ==="), *Title);
}

void UMontageAnalyzerTestUtility::PrintValue(const FString& Key, const FString& Value)
{
	UE_LOG(LogTemp, Display, TEXT("  %s: %s"), *Key, *Value);
}
