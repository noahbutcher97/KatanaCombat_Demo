// Copyright Epic Games, Inc. All Rights Reserved.

#include "AttackTimingDerivationService.h"

#include "AttackDataNotifyGenerationService.h"
#include "AttackTimingDerivationLibrary.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "Animation/AnimNotify_HoldWindowStart.h"
#include "Animation/AnimNotifyState_AttackPhase.h"
#include "Data/AttackData.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Templates/UnrealTemplate.h"

#define LOCTEXT_NAMESPACE "AttackTimingDerivationService"

DEFINE_LOG_CATEGORY_STATIC(LogAttackTimingDerivation, Log, All);

namespace
{
	FString SectionLabel(const FName Section)
	{
		return Section.IsNone() ? FString(TEXT("(entire montage)")) : FString::Printf(TEXT("'%s'"), *Section.ToString());
	}

	FString AssetLabel(const FAttackTimingApplyResult& Result)
	{
		return Result.AttackData.IsEmpty() ? FString(TEXT("(no attack data)")) : FPackageName::ObjectPathToObjectName(Result.AttackData);
	}

	void AppendField(FString& Out, const TCHAR* Name, const float Before, const float After, const bool bDerived)
	{
		if (!bDerived)
		{
			Out += FString::Printf(TEXT("  %s: %.4fs (kept)\n"), Name, Before);
		}
		else if (FMath::IsNearlyEqual(Before, After, UAttackTimingDerivationLibrary::TimingEqualityToleranceSeconds))
		{
			Out += FString::Printf(TEXT("  %s: %.4fs (unchanged)\n"), Name, After);
		}
		else
		{
			Out += FString::Printf(TEXT("  %s: %.4fs -> %.4fs\n"), Name, Before, After);
		}
	}

	void AppendIssues(FString& Out, const TCHAR* Heading, const TArray<FAttackTimingDerivationIssue>& Issues)
	{
		if (Issues.IsEmpty())
		{
			return;
		}
		Out += FString::Printf(TEXT("  %s:\n"), Heading);
		for (const FAttackTimingDerivationIssue& Issue : Issues)
		{
			Out += FString::Printf(TEXT("  - %s\n"), *Issue.Message);
		}
	}
}

TArray<FAttackTimingNotifyMarker> FAttackTimingDerivationService::CollectTimingMarkers(const UAnimMontage* Montage)
{
	TArray<FAttackTimingNotifyMarker> Markers;
	if (!Montage)
	{
		return Markers;
	}

	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const float TriggerTime = Event.GetTriggerTime();
		if (const UAnimNotify_AttackPhaseTransition* Transition = Cast<UAnimNotify_AttackPhaseTransition>(Event.Notify))
		{
			Markers.Add(FAttackTimingNotifyMarker::Transition(Transition->TransitionToPhase, TriggerTime));
		}
		else if (const UAnimNotify_HoldWindowStart* Hold = Cast<UAnimNotify_HoldWindowStart>(Event.Notify))
		{
			Markers.Add(FAttackTimingNotifyMarker::Hold(Hold->InputType, TriggerTime));
		}
		else if (const UAnimNotifyState_AttackPhase* LegacyPhase = Cast<UAnimNotifyState_AttackPhase>(Event.NotifyStateClass))
		{
			Markers.Add(FAttackTimingNotifyMarker::LegacyPhase(LegacyPhase->Phase, TriggerTime));
		}
	}
	return Markers;
}

FAttackTimingHoldRule FAttackTimingDerivationService::MakeHoldRule(const UAttackData* AttackData)
{
	FAttackTimingHoldRule Rule;
	if (!AttackData)
	{
		return Rule;
	}

	Rule.bUsesHold = FAttackDataNotifyGenerationService::ShouldGenerateHoldWindowStart(AttackData);
	Rule.GeneratedHoldInput = FAttackDataNotifyGenerationService::GetGeneratedHoldInputType(AttackData);
	// The condition UAttackData::ValidateChargedHoldOrdering and the generation service's charged-hold check share.
	Rule.bHoldMustPrecedeActive = AttackData->AttackType == EAttackType::Heavy && !AttackData->ChargeLoopSection.IsNone();
	return Rule;
}

FAttackTimingDerivation FAttackTimingDerivationService::DeriveTimingFromMontage(const UAttackData* AttackData)
{
	FAttackTimingDerivation Derivation;
	if (!AttackData)
	{
		Derivation.AddError(EAttackTimingDerivationIssue::MissingAttackData, TEXT("No attack data was given"));
		return Derivation;
	}

	const UAnimMontage* Montage = AttackData->AttackMontage;
	if (!Montage)
	{
		Derivation.AddError(EAttackTimingDerivationIssue::MissingMontage,
			TEXT("The attack has no AttackMontage, so there are no notifies to read"));
		return Derivation;
	}

	if (!AttackData->MontageSection.IsNone() && Montage->GetSectionIndex(AttackData->MontageSection) == INDEX_NONE)
	{
		Derivation.AddError(EAttackTimingDerivationIssue::MissingSection, FString::Printf(
			TEXT("The montage '%s' has no section '%s'"), *Montage->GetName(), *AttackData->MontageSection.ToString()));
		return Derivation;
	}

	float SectionStart = 0.0f;
	float SectionEnd = 0.0f;
	AttackData->GetSectionTimeRange(SectionStart, SectionEnd);
	return UAttackTimingDerivationLibrary::DeriveAttackTiming(
		CollectTimingMarkers(Montage), SectionStart, SectionEnd, MakeHoldRule(AttackData));
}

FAttackTimingApplyResult FAttackTimingDerivationService::ApplyTimingFromMontage(UAttackData* AttackData)
{
	FAttackTimingApplyResult Result;
	Result.Derivation = DeriveTimingFromMontage(AttackData);
	if (!AttackData)
	{
		return Result;
	}

	Result.AttackData = AttackData->GetPathName();
	Result.Montage = AttackData->AttackMontage ? AttackData->AttackMontage->GetName() : FString();
	Result.Section = AttackData->MontageSection;
	Result.PreviousTiming = AttackData->ManualTiming;
	Result.NewTiming = AttackData->ManualTiming;
	if (!Result.Derivation.IsValid())
	{
		return Result;
	}

	const FAttackPhaseTimingOverride Derived =
		UAttackTimingDerivationLibrary::MergeDerivedTiming(AttackData->ManualTiming, Result.Derivation);

	// Run notify generation's own analysis against the derived timing before anything is written, so an applied
	// result is always one generation accepts. The probe swaps the value in place and restores it on scope exit;
	// nothing is recorded or dirtied.
	{
		TGuardValue<FAttackPhaseTimingOverride> ProbeTiming(AttackData->ManualTiming, Derived);
		const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);
		if (!Analysis.bValid)
		{
			for (const FString& Error : Analysis.Errors)
			{
				Result.Derivation.AddError(EAttackTimingDerivationIssue::NotifyGenerationRefuses,
					FString::Printf(TEXT("Notify generation would refuse the derived timing: %s"), *Error));
			}
			if (Analysis.Errors.IsEmpty())
			{
				Result.Derivation.AddError(EAttackTimingDerivationIssue::NotifyGenerationRefuses,
					TEXT("Notify generation would refuse the derived timing"));
			}
			return Result;
		}
	}

	if (UAttackTimingDerivationLibrary::IsSameTiming(Derived, AttackData->ManualTiming))
	{
		Result.Outcome = EAttackTimingApplyOutcome::Unchanged;
		return Result;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeriveAttackTimingFromMontage", "Derive Attack Timing From Montage"));
	FProperty* TimingProperty = FindFProperty<FProperty>(
		UAttackData::StaticClass(), GET_MEMBER_NAME_CHECKED(UAttackData, ManualTiming));
	AttackData->Modify();
	AttackData->PreEditChange(TimingProperty);
	AttackData->ManualTiming = Derived;
	FPropertyChangedEvent ChangedEvent(TimingProperty, EPropertyChangeType::ValueSet);
	AttackData->PostEditChangeProperty(ChangedEvent);
	AttackData->MarkPackageDirty();

	Result.NewTiming = Derived;
	Result.Outcome = EAttackTimingApplyOutcome::Applied;
	UE_LOG(LogAttackTimingDerivation, Log, TEXT("%s"), *DescribeResult(Result));
	return Result;
}

TArray<FAttackTimingApplyResult> FAttackTimingDerivationService::BatchApplyTimingFromMontage(TConstArrayView<UAttackData*> Attacks)
{
	TArray<FAttackTimingApplyResult> Results;
	Results.Reserve(Attacks.Num());

	FScopedTransaction Transaction(
		LOCTEXT("DeriveAttackTimingFromMontageBatch", "Derive Attack Timing From Montage (Selected Assets)"));
	bool bAnyApplied = false;
	for (UAttackData* AttackData : Attacks)
	{
		FAttackTimingApplyResult& Result = Results.Add_GetRef(ApplyTimingFromMontage(AttackData));
		bAnyApplied |= Result.Outcome == EAttackTimingApplyOutcome::Applied;
	}

	if (!bAnyApplied)
	{
		// Nothing was recorded; do not leave an empty entry in the undo history.
		Transaction.Cancel();
	}
	return Results;
}

FString FAttackTimingDerivationService::DescribeResult(const FAttackTimingApplyResult& Result)
{
	FString Out;
	switch (Result.Outcome)
	{
	case EAttackTimingApplyOutcome::Applied:
		Out += FString::Printf(TEXT("%s: timing written from %s section %s.\n"),
			*AssetLabel(Result), *Result.Montage, *SectionLabel(Result.Section));
		break;
	case EAttackTimingApplyOutcome::Unchanged:
		Out += FString::Printf(TEXT("%s: timing already matches %s section %s; nothing written.\n"),
			*AssetLabel(Result), *Result.Montage, *SectionLabel(Result.Section));
		break;
	case EAttackTimingApplyOutcome::Refused:
		Out += FString::Printf(TEXT("%s: timing not changed.\n"), *AssetLabel(Result));
		break;
	}

	if (Result.Outcome != EAttackTimingApplyOutcome::Refused)
	{
		const FAttackTimingDerivation& Derivation = Result.Derivation;
		Out += FString::Printf(TEXT("  Section: %.4fs - %.4fs\n"), Derivation.SectionStart, Derivation.SectionEnd);
		AppendField(Out, TEXT("WindupDuration"), Result.PreviousTiming.WindupDuration, Result.NewTiming.WindupDuration, true);
		AppendField(Out, TEXT("ActiveDuration"), Result.PreviousTiming.ActiveDuration, Result.NewTiming.ActiveDuration, true);
		AppendField(Out, TEXT("RecoveryDuration"), Result.PreviousTiming.RecoveryDuration, Result.NewTiming.RecoveryDuration, true);
		AppendField(Out, TEXT("HoldWindowStart"), Result.PreviousTiming.HoldWindowStart, Result.NewTiming.HoldWindowStart,
			Derivation.bHasHoldWindowStart);
		AppendField(Out, TEXT("HoldWindowDuration"), Result.PreviousTiming.HoldWindowDuration, Result.NewTiming.HoldWindowDuration,
			false);
	}

	AppendIssues(Out, TEXT("Reasons"), Result.Derivation.Errors);
	AppendIssues(Out, TEXT("Notes"), Result.Derivation.Warnings);
	return Out;
}

FString FAttackTimingDerivationService::DescribeResults(TConstArrayView<FAttackTimingApplyResult> Results)
{
	int32 Applied = 0;
	int32 Unchanged = 0;
	int32 Refused = 0;
	for (const FAttackTimingApplyResult& Result : Results)
	{
		Applied += Result.Outcome == EAttackTimingApplyOutcome::Applied ? 1 : 0;
		Unchanged += Result.Outcome == EAttackTimingApplyOutcome::Unchanged ? 1 : 0;
		Refused += Result.Outcome == EAttackTimingApplyOutcome::Refused ? 1 : 0;
	}

	FString Out = FString::Printf(TEXT("%d written, %d already matching, %d not changed.\n"), Applied, Unchanged, Refused);
	for (const FAttackTimingApplyResult& Result : Results)
	{
		Out += TEXT("\n");
		Out += DescribeResult(Result);
	}
	return Out;
}

#undef LOCTEXT_NAMESPACE
