// Copyright Epic Games, Inc. All Rights Reserved.
#include "PairedAnimationAnalysisLibrary.h"

FPairedAlignmentEvaluation UPairedAnimationAnalysisLibrary::EvaluateRelativeAlignment(
	const FPairedAlignmentRule& Rule, TConstArrayView<FPairedContactPose> Authored, TConstArrayView<FPairedContactPose> Runtime)
{
	FPairedAlignmentEvaluation Result;
	for (double Value : {Rule.StartSeconds, Rule.EndSeconds, Rule.MaximumSampleGapSeconds,
		Rule.MaximumTranslationCm, Rule.MaximumRotationDegrees, Rule.MaximumVictimTimingErrorSeconds})
	{
		if (!FMath::IsFinite(Value) || Value < 0) { Result.Reason = TEXT("Invalid alignment criterion"); return Result; }
	}
	if (Rule.EndSeconds < Rule.StartSeconds || Rule.MaximumSampleGapSeconds <= 0)
	{
		Result.Reason = TEXT("Invalid alignment interval or sample gap"); return Result;
	}
	// Reuse the contact evaluator's temporal coverage and fresh-point contract.
	FPairedContactRule Coverage;
	Coverage.Name = Rule.Name; Coverage.SourcePoint = TEXT("Attacker:root"); Coverage.TargetPoint = TEXT("Victim:root");
	Coverage.StartSeconds = Rule.StartSeconds; Coverage.EndSeconds = Rule.EndSeconds;
	Coverage.MaximumSampleGapSeconds = Rule.MaximumSampleGapSeconds; Coverage.MaximumGapCm = DBL_MAX;
	for (auto Samples : {Authored, Runtime})
	{
		const auto Check = EvaluateIntendedContact(Coverage, Samples);
		if (Check.Status == EPairedContactResult::Inconclusive) { Result.Reason = Check.Reason; return Result; }
	}
	bool bFailed = false;
	int32 Upper = 0;
	for (const auto& Pose : Runtime)
	{
		if (Pose.TimeSeconds < Rule.StartSeconds || Pose.TimeSeconds > Rule.EndSeconds) { continue; }
		while (Upper < Authored.Num() && Authored[Upper].TimeSeconds < Pose.TimeSeconds) { ++Upper; }
		if (Upper >= Authored.Num() || !FMath::IsFinite(Pose.VictimTimeSeconds))
		{
			Result.Reason = TEXT("Missing authored bracket or original victim montage clock"); return Result;
		}
		const int32 Lower = FMath::Max(0, Upper - 1);
		const auto& Before = Authored[Lower]; const auto& After = Authored[Upper];
		if (!Before.bEligible || !After.bEligible || After.TimeSeconds - Before.TimeSeconds > Rule.MaximumSampleGapSeconds)
		{
			Result.Reason = TEXT("Ineligible authored interpolation bracket"); return Result;
		}
		const double Alpha = Upper == Lower ? 0 : (Pose.TimeSeconds - Before.TimeSeconds) / (After.TimeSeconds - Before.TimeSeconds);
		FTransform Expected[2];
		int32 Role = 0;
		for (const FString Key : {FString(TEXT("Attacker:root")), FString(TEXT("Victim:root"))})
		{
			const auto* A = Before.Points.Find(Key); const auto* B = After.Points.Find(Key);
			if (!A || !B || !A->IsValid() || !B->IsValid()) { Result.Reason = TEXT("Missing authored root bracket"); return Result; }
			Expected[Role++].Blend(*A, *B, Alpha);
		}
		// Root frames orient world-centimetre vectors; their scale metadata must
		// not divide the physical separation or consume a placement budget.
		auto Rigid = [](const FTransform& Transform) { return FTransform(Transform.GetRotation(), Transform.GetLocation()); };
		const FTransform ExpectedRelative = Rigid(Expected[1]).GetRelativeTransform(Rigid(Expected[0]));
		const FTransform ObservedRelative = Rigid(Pose.Points[TEXT("Victim:root")]).GetRelativeTransform(Rigid(Pose.Points[TEXT("Attacker:root")]));
		auto& Observation = Result.Observations.AddDefaulted_GetRef();
		Observation.TimeSeconds = Pose.TimeSeconds; Observation.OriginalTimeSeconds = Pose.OriginalTimeSeconds;
		Observation.TranslationCm = FVector::Distance(ExpectedRelative.GetLocation(), ObservedRelative.GetLocation());
		Observation.RotationDegrees = FMath::RadiansToDegrees(ExpectedRelative.GetRotation().AngularDistance(ObservedRelative.GetRotation()));
		Observation.VictimTimingErrorSeconds = Pose.VictimTimeSeconds - Pose.TimeSeconds;
		Observation.bWithinCriteria = Observation.TranslationCm <= Rule.MaximumTranslationCm
			&& Observation.RotationDegrees <= Rule.MaximumRotationDegrees
			&& FMath::Abs(Observation.VictimTimingErrorSeconds) <= Rule.MaximumVictimTimingErrorSeconds;
		bFailed |= !Observation.bWithinCriteria;
	}
	if (Result.Observations.IsEmpty()) { Result.Reason = TEXT("No alignment observations inside the interval"); return Result; }
	Result.Status = bFailed ? EPairedContactResult::Fail : EPairedContactResult::Pass;
	Result.Reason = bFailed ? TEXT("Relative placement or victim timing exceeds the declared budget") : TEXT("Relative placement and victim timing remain within the declared budget");
	return Result;
}
