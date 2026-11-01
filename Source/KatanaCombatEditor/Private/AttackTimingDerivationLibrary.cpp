// Copyright Epic Games, Inc. All Rights Reserved.

#include "AttackTimingDerivationLibrary.h"

namespace
{
	/** Same section membership as UAttackData::ValidateChargedHoldOrdering and notify generation: [start, end). */
	bool IsInSection(const float Time, const float SectionStart, const float SectionEnd)
	{
		return Time >= SectionStart && Time < SectionEnd;
	}

	FString InputName(const EInputType InputType)
	{
		return StaticEnum<EInputType>()->GetNameStringByValue(static_cast<int64>(InputType));
	}

	FString PhaseName(const EAttackPhase Phase)
	{
		return StaticEnum<EAttackPhase>()->GetNameStringByValue(static_cast<int64>(Phase));
	}

	FString JoinTimes(const TArray<FAttackTimingNotifyMarker>& Markers)
	{
		TArray<FString> Times;
		for (const FAttackTimingNotifyMarker& Marker : Markers)
		{
			Times.Add(FString::Printf(TEXT("%.4fs"), Marker.TriggerTime));
		}
		return FString::Join(Times, TEXT(", "));
	}

	FString JoinHolds(const TArray<FAttackTimingNotifyMarker>& Holds)
	{
		TArray<FString> Entries;
		for (const FAttackTimingNotifyMarker& Hold : Holds)
		{
			Entries.Add(FString::Printf(TEXT("%s at %.4fs"), *InputName(Hold.HoldInputType), Hold.TriggerTime));
		}
		return FString::Join(Entries, TEXT(", "));
	}

	bool IsSameValue(const float A, const float B)
	{
		return FMath::IsNearlyEqual(A, B, UAttackTimingDerivationLibrary::TimingEqualityToleranceSeconds);
	}
}

FAttackTimingDerivation UAttackTimingDerivationLibrary::DeriveAttackTiming(
	const TArray<FAttackTimingNotifyMarker>& Notifies,
	const float SectionStart,
	const float SectionEnd,
	const FAttackTimingHoldRule& HoldRule)
{
	FAttackTimingDerivation Result;
	Result.SectionStart = SectionStart;
	Result.SectionEnd = SectionEnd;

	if (!FMath::IsFinite(SectionStart) || !FMath::IsFinite(SectionEnd) || SectionEnd <= SectionStart)
	{
		Result.AddError(EAttackTimingDerivationIssue::InvalidSectionRange, FString::Printf(
			TEXT("The section range %.4fs - %.4fs is empty or invalid, so there is no timing to read"),
			SectionStart, SectionEnd));
		return Result;
	}

	TArray<FAttackTimingNotifyMarker> Actives;
	TArray<FAttackTimingNotifyMarker> Recoveries;
	TArray<FAttackTimingNotifyMarker> Holds;
	for (const FAttackTimingNotifyMarker& Marker : Notifies)
	{
		if (!IsInSection(Marker.TriggerTime, SectionStart, SectionEnd))
		{
			continue;
		}

		switch (Marker.Kind)
		{
		case EAttackTimingNotifyKind::ActiveTransition:
			Actives.Add(Marker);
			break;
		case EAttackTimingNotifyKind::RecoveryTransition:
			Recoveries.Add(Marker);
			break;
		case EAttackTimingNotifyKind::HoldWindowStart:
			Holds.Add(Marker);
			break;
		case EAttackTimingNotifyKind::OtherPhaseTransition:
			Result.AddWarning(EAttackTimingDerivationIssue::OtherPhaseTransition, FString::Printf(
				TEXT("The phase transition to %s at %.4fs is not a timing source; regenerating notifies removes it"),
				*PhaseName(Marker.TransitionPhase), Marker.TriggerTime));
			break;
		case EAttackTimingNotifyKind::LegacyPhaseState:
			Result.AddWarning(EAttackTimingDerivationIssue::LegacyPhaseStateIgnored, FString::Printf(
				TEXT("The deprecated AnimNotifyState_AttackPhase (%s) at %.4fs is not read; timing comes only from AnimNotify_AttackPhaseTransition"),
				*PhaseName(Marker.TransitionPhase), Marker.TriggerTime));
			break;
		}
	}

	const auto ByTime = [](const FAttackTimingNotifyMarker& A, const FAttackTimingNotifyMarker& B)
	{
		return A.TriggerTime < B.TriggerTime;
	};
	Actives.StableSort(ByTime);
	Recoveries.StableSort(ByTime);
	Holds.StableSort(ByTime);

	// Phases: the timing fields describe one Active window, so exactly one transition into and one out of it.
	bool bPhasesUsable = true;
	if (Actives.IsEmpty())
	{
		Result.AddError(EAttackTimingDerivationIssue::MissingActiveTransition,
			TEXT("The section has no AnimNotify_AttackPhaseTransition to Active, so Windup has no end; add one in the montage"));
		bPhasesUsable = false;
	}
	else if (Actives.Num() > 1)
	{
		Result.AddError(EAttackTimingDerivationIssue::MultipleActiveTransitions, FString::Printf(
			TEXT("The section has %d transitions to Active (%s); the timing fields describe one Active window, so regenerating would drop the others"),
			Actives.Num(), *JoinTimes(Actives)));
		bPhasesUsable = false;
	}

	if (Recoveries.IsEmpty())
	{
		Result.AddError(EAttackTimingDerivationIssue::MissingRecoveryTransition,
			TEXT("The section has no AnimNotify_AttackPhaseTransition to Recovery, so Active has no end; add one in the montage"));
		bPhasesUsable = false;
	}
	else if (Recoveries.Num() > 1)
	{
		Result.AddError(EAttackTimingDerivationIssue::MultipleRecoveryTransitions, FString::Printf(
			TEXT("The section has %d transitions to Recovery (%s); the timing fields describe one Active window, so regenerating would drop the others"),
			Recoveries.Num(), *JoinTimes(Recoveries)));
		bPhasesUsable = false;
	}

	if (bPhasesUsable)
	{
		const float ActiveTime = Actives[0].TriggerTime;
		const float RecoveryTime = Recoveries[0].TriggerTime;
		const float Windup = ActiveTime - SectionStart;
		const float Active = RecoveryTime - ActiveTime;
		if (Windup <= 0.0f)
		{
			Result.AddError(EAttackTimingDerivationIssue::ActiveAtSectionStart, FString::Printf(
				TEXT("The Active transition at %.4fs is at the section start, so Windup would be zero; notify generation needs a positive Windup"),
				ActiveTime));
		}
		if (Active <= 0.0f)
		{
			Result.AddError(EAttackTimingDerivationIssue::RecoveryNotAfterActive, FString::Printf(
				TEXT("The Recovery transition at %.4fs is not after the Active transition at %.4fs, so Active would not be positive"),
				RecoveryTime, ActiveTime));
		}
		if (Windup > 0.0f && Active > 0.0f)
		{
			Result.bHasPhaseTiming = true;
			Result.WindupDuration = Windup;
			Result.ActiveDuration = Active;
			Result.RecoveryDuration = SectionEnd - RecoveryTime;
		}
	}

	if (!HoldRule.bUsesHold)
	{
		if (!Holds.IsEmpty())
		{
			Result.AddWarning(EAttackTimingDerivationIssue::HoldWindowStartNotUsed, FString::Printf(
				TEXT("The section has hold notifies (%s), but this attack does not use a hold, so HoldWindowStart is kept; regenerating notifies removes them"),
				*JoinHolds(Holds)));
		}
		return Result;
	}

	// Any hold notify starts a charge when its own button is held, whatever input it names, so a charged heavy
	// needs every one of them before Active. Mirrors UAttackData::ValidateChargedHoldOrdering, first Active included.
	if (HoldRule.bHoldMustPrecedeActive && !Actives.IsEmpty())
	{
		const float FirstActive = Actives[0].TriggerTime;
		for (const FAttackTimingNotifyMarker& Hold : Holds)
		{
			if (Hold.TriggerTime >= FirstActive)
			{
				Result.AddError(EAttackTimingDerivationIssue::ChargedHoldNotBeforeActive, FString::Printf(
					TEXT("The hold notify (%s) at %.4fs is not before the Active transition at %.4fs; a held %s there enters the charge loop already Active. Move every hold notify in the section before Active in the montage"),
					*InputName(Hold.HoldInputType), Hold.TriggerTime, FirstActive, *InputName(Hold.HoldInputType)));
			}
		}
	}

	const FAttackTimingNotifyMarker* Described = Holds.FindByPredicate([&HoldRule](const FAttackTimingNotifyMarker& Hold)
	{
		return Hold.HoldInputType == HoldRule.GeneratedHoldInput;
	});
	if (!Described)
	{
		Result.AddError(EAttackTimingDerivationIssue::MissingHoldWindowStart, Holds.IsEmpty()
			? FString::Printf(
				TEXT("The attack uses a hold, but the section has no AnimNotify_HoldWindowStart; add one checking %s in the montage"),
				*InputName(HoldRule.GeneratedHoldInput))
			: FString::Printf(
				TEXT("The attack uses a hold, but no hold notify in the section checks %s, the input notify generation writes for it (found %s)"),
				*InputName(HoldRule.GeneratedHoldInput), *JoinHolds(Holds)));
	}
	else
	{
		// The earliest matching notify is where a hold of the attack's own button starts; the field holds one value.
		Result.bHasHoldWindowStart = true;
		Result.HoldWindowStart = Described->TriggerTime - SectionStart;
		for (const FAttackTimingNotifyMarker& Hold : Holds)
		{
			if (&Hold != Described)
			{
				Result.AddWarning(EAttackTimingDerivationIssue::UndescribedHoldWindowStart, FString::Printf(
					TEXT("The hold notify (%s) at %.4fs is not described by HoldWindowStart, which comes from the %s hold at %.4fs; regenerating notifies removes it"),
					*InputName(Hold.HoldInputType), Hold.TriggerTime,
					*InputName(Described->HoldInputType), Described->TriggerTime));
			}
		}
	}

	Result.AddWarning(EAttackTimingDerivationIssue::HoldWindowDurationNotDerived,
		TEXT("HoldWindowDuration is kept: a hold starts at a point notify, so the montage has no hold duration to read"));
	return Result;
}

FAttackPhaseTimingOverride UAttackTimingDerivationLibrary::MergeDerivedTiming(
	const FAttackPhaseTimingOverride& CurrentTiming,
	const FAttackTimingDerivation& Derivation)
{
	FAttackPhaseTimingOverride Merged = CurrentTiming;
	if (!Derivation.IsValid())
	{
		return Merged;
	}

	Merged.WindupDuration = Derivation.WindupDuration;
	Merged.ActiveDuration = Derivation.ActiveDuration;
	Merged.RecoveryDuration = Derivation.RecoveryDuration;
	if (Derivation.bHasHoldWindowStart)
	{
		Merged.HoldWindowStart = Derivation.HoldWindowStart;
	}
	return Merged;
}

bool UAttackTimingDerivationLibrary::IsSameTiming(const FAttackPhaseTimingOverride& A, const FAttackPhaseTimingOverride& B)
{
	return IsSameValue(A.WindupDuration, B.WindupDuration)
		&& IsSameValue(A.ActiveDuration, B.ActiveDuration)
		&& IsSameValue(A.RecoveryDuration, B.RecoveryDuration)
		&& IsSameValue(A.HoldWindowStart, B.HoldWindowStart)
		&& IsSameValue(A.HoldWindowDuration, B.HoldWindowDuration);
}
