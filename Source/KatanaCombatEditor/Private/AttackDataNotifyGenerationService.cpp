// Copyright Epic Games, Inc. All Rights Reserved.

#include "AttackDataNotifyGenerationService.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "Animation/AnimNotify_HoldWindowStart.h"
#include "Animation/AnimNotify_ToggleHitDetection.h"
#include "Animation/AnimNotifyState_AttackPhase.h"
#include "Animation/AnimNotifyState_ComboWindow.h"
#include "Animation/AnimNotifyState_CounterWindow.h"
#include "Animation/AnimNotifyState_HoldWindow.h"
#include "Animation/AnimNotifyState_ParryWindow.h"
#include "Data/AttackData.h"
#include "Data/PairedAnimationData.h"
#include "Utilities/CombatGameplayTags.h"

namespace
{
	constexpr float NotifyTimeToleranceSeconds = 0.001f;

	bool IsTimeInSection(float Time, float SectionStart, float SectionEnd)
	{
		return Time >= SectionStart && Time < SectionEnd;
	}

	bool IsExpectedNotifyTime(float ActualTime, float ExpectedTime)
	{
		return FMath::IsNearlyEqual(ActualTime, ExpectedTime, NotifyTimeToleranceSeconds);
	}

	EInputType GetExpectedHoldInputType(const UAttackData* AttackData)
	{
		return AttackData && AttackData->AttackType == EAttackType::Heavy
			? EInputType::HeavyAttack
			: EInputType::LightAttack;
	}

	void AddPointNotify(UAnimMontage* Montage, UAnimNotify* Notify, float Time)
	{
		FAnimNotifyEvent Event;
		Event.Notify = Notify;
		Event.SetTime(Time);
		Event.TriggerTimeOffset = EAnimEventTriggerOffsets::OffsetBefore;
		Event.TrackIndex = 0;
		Montage->Notifies.Add(Event);
	}

	void AddLegacyNotify(FAttackDataNotifyAnalysis& Analysis, int32 NotifyIndex, const TCHAR* NotifyName)
	{
		Analysis.LegacyNotifyIndices.Add(NotifyIndex);
		Analysis.LegacyNotifiesFound.AddUnique(NotifyName);
	}

	void AddCanonicalNotify(FAttackDataNotifyAnalysis& Analysis, int32 NotifyIndex, const TCHAR* NotifyName)
	{
		Analysis.CanonicalNotifyIndices.Add(NotifyIndex);
		Analysis.CanonicalNotifiesFound.AddUnique(NotifyName);
	}

	void AddStaleCanonicalNotify(FAttackDataNotifyAnalysis& Analysis, int32 NotifyIndex, const TCHAR* NotifyName)
	{
		Analysis.StaleCanonicalNotifyIndices.AddUnique(NotifyIndex);
		Analysis.StaleCanonicalNotifiesFound.AddUnique(NotifyName);
	}

	void AddUniqueIndices(TArray<int32>& Destination, const TArray<int32>& Source)
	{
		for (const int32 Index : Source)
		{
			Destination.AddUnique(Index);
		}
	}

	TArray<FString> GameplayTagContainerToStrings(const FGameplayTagContainer& Tags)
	{
		TArray<FString> Values;
		TArray<FGameplayTag> TagArray;
		Tags.GetGameplayTagArray(TagArray);
		for (const FGameplayTag& Tag : TagArray)
		{
			Values.Add(Tag.ToString());
		}
		Values.Sort();
		return Values;
	}
}

bool FAttackDataNotifyGenerationService::ShouldGenerateHoldWindowStart(const UAttackData* AttackData)
{
	if (!AttackData)
	{
		return false;
	}
	if (AttackData->AttackType == EAttackType::Light)
	{
		return AttackData->bCanHold;
	}
	if (AttackData->AttackType == EAttackType::Heavy)
	{
		return AttackData->ChargeLoopSection != NAME_None;
	}
	return false;
}

FAttackDataNotifyAnalysis FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(const UAttackData* AttackData)
{
	FAttackDataNotifyAnalysis Analysis;
	Analysis.AttackData = AttackData;
	if (!AttackData)
	{
		Analysis.Errors.Add(TEXT("AttackData is null"));
		return Analysis;
	}

	Analysis.AttackTags = GameplayTagContainerToStrings(AttackData->AttackTags);
	Analysis.RequiredContextTags = GameplayTagContainerToStrings(AttackData->RequiredContextTags);
	Analysis.bHasRequiredContextTags = !AttackData->RequiredContextTags.IsEmpty();

	const FGameplayTag UnblockableTag = KatanaCombatGameplayTags::AttackPropertyUnblockable();
	Analysis.bHasUnblockableTag = UnblockableTag.IsValid() && AttackData->AttackTags.HasTag(UnblockableTag);

	Analysis.Montage = AttackData->AttackMontage;
	Analysis.SectionName = AttackData->MontageSection;
	if (!Analysis.Montage)
	{
		Analysis.Errors.Add(TEXT("AttackData has no AttackMontage"));
		return Analysis;
	}

	AttackData->GetSectionTimeRange(Analysis.SectionStart, Analysis.SectionEnd);
	if (Analysis.SectionEnd <= Analysis.SectionStart)
	{
		Analysis.Errors.Add(TEXT("AttackData montage section has invalid time range"));
		return Analysis;
	}

	Analysis.SectionLength = Analysis.SectionEnd - Analysis.SectionStart;
	const FAttackPhaseTimingOverride& Timing = AttackData->ManualTiming;
	Analysis.WindupDuration = Timing.WindupDuration;
	Analysis.ActiveDuration = Timing.ActiveDuration;
	Analysis.RecoveryDuration = Timing.RecoveryDuration;
	Analysis.TimingTotal = Timing.WindupDuration + Timing.ActiveDuration + Timing.RecoveryDuration;
	Analysis.HoldWindowStart = Timing.HoldWindowStart;
	if (Timing.WindupDuration <= 0.0f || Timing.ActiveDuration <= 0.0f || Timing.RecoveryDuration < 0.0f)
	{
		Analysis.Errors.Add(TEXT("Windup and Active must be positive, and Recovery cannot be negative"));
		return Analysis;
	}

	if (Analysis.TimingTotal > Analysis.SectionLength + KINDA_SMALL_NUMBER)
	{
		Analysis.Errors.Add(FString::Printf(TEXT("Timing total %.3fs exceeds section length %.3fs"), Analysis.TimingTotal, Analysis.SectionLength));
		return Analysis;
	}

	Analysis.bShouldHaveHoldStart = ShouldGenerateHoldWindowStart(AttackData);
	if (Analysis.bShouldHaveHoldStart &&
		(Timing.HoldWindowStart < 0.0f || Timing.HoldWindowStart > Analysis.SectionLength + KINDA_SMALL_NUMBER))
	{
		Analysis.Errors.Add(FString::Printf(TEXT("Hold window start %.3fs is outside section length %.3fs"), Timing.HoldWindowStart, Analysis.SectionLength));
		return Analysis;
	}

	Analysis.ActiveTransitionTime = Analysis.SectionStart + Timing.WindupDuration;
	Analysis.RecoveryTransitionTime = Analysis.ActiveTransitionTime + Timing.ActiveDuration;
	Analysis.HoldStartTime = Analysis.SectionStart + Timing.HoldWindowStart;
	const EInputType ExpectedHoldInputType = GetExpectedHoldInputType(AttackData);

	for (int32 Index = 0; Index < Analysis.Montage->Notifies.Num(); ++Index)
	{
		const FAnimNotifyEvent& Event = Analysis.Montage->Notifies[Index];
		const float NotifyTime = Event.GetTriggerTime();
		if (!IsTimeInSection(NotifyTime, Analysis.SectionStart, Analysis.SectionEnd))
		{
			continue;
		}

		if (Event.NotifyStateClass && Event.NotifyStateClass->IsA(UAnimNotifyState_AttackPhase::StaticClass()))
		{
			AddLegacyNotify(Analysis, Index, TEXT("AnimNotifyState_AttackPhase"));
		}
		else if (Event.NotifyStateClass && Event.NotifyStateClass->IsA(UAnimNotifyState_HoldWindow::StaticClass()))
		{
			AddLegacyNotify(Analysis, Index, TEXT("AnimNotifyState_HoldWindow"));
		}
		else if (Event.NotifyStateClass && Event.NotifyStateClass->IsA(UAnimNotifyState_ComboWindow::StaticClass()))
		{
			AddLegacyNotify(Analysis, Index, TEXT("AnimNotifyState_ComboWindow"));
		}
		else if (Event.NotifyStateClass && Event.NotifyStateClass->IsA(UAnimNotifyState_ParryWindow::StaticClass()))
		{
			Analysis.bHasParryWindow = true;
		}
		else if (Event.NotifyStateClass && Event.NotifyStateClass->IsA(UAnimNotifyState_CounterWindow::StaticClass()))
		{
			Analysis.bHasCounterWindow = true;
		}
		else if (Event.Notify && Event.Notify->IsA(UAnimNotify_ToggleHitDetection::StaticClass()))
		{
			AddLegacyNotify(Analysis, Index, TEXT("AnimNotify_ToggleHitDetection"));
		}

		if (const UAnimNotify_AttackPhaseTransition* Transition = Cast<UAnimNotify_AttackPhaseTransition>(Event.Notify))
		{
			if (Transition->TransitionToPhase == EAttackPhase::Active)
			{
				AddCanonicalNotify(Analysis, Index, TEXT("AnimNotify_AttackPhaseTransition(Active)"));
				if (IsExpectedNotifyTime(NotifyTime, Analysis.ActiveTransitionTime) && !Analysis.bHasActiveTransition)
				{
					Analysis.bHasActiveTransition = true;
				}
				else
				{
					AddStaleCanonicalNotify(Analysis, Index, TEXT("AnimNotify_AttackPhaseTransition(Active)"));
				}
			}
			else if (Transition->TransitionToPhase == EAttackPhase::Recovery)
			{
				AddCanonicalNotify(Analysis, Index, TEXT("AnimNotify_AttackPhaseTransition(Recovery)"));
				if (IsExpectedNotifyTime(NotifyTime, Analysis.RecoveryTransitionTime) && !Analysis.bHasRecoveryTransition)
				{
					Analysis.bHasRecoveryTransition = true;
				}
				else
				{
					AddStaleCanonicalNotify(Analysis, Index, TEXT("AnimNotify_AttackPhaseTransition(Recovery)"));
				}
			}
			else
			{
				AddCanonicalNotify(Analysis, Index, TEXT("AnimNotify_AttackPhaseTransition"));
				AddStaleCanonicalNotify(Analysis, Index, TEXT("AnimNotify_AttackPhaseTransition"));
			}
		}
		else if (const UAnimNotify_HoldWindowStart* HoldWindowStart = Cast<UAnimNotify_HoldWindowStart>(Event.Notify))
		{
			AddCanonicalNotify(Analysis, Index, TEXT("AnimNotify_HoldWindowStart"));
			if (Analysis.bShouldHaveHoldStart &&
				IsExpectedNotifyTime(NotifyTime, Analysis.HoldStartTime) &&
				HoldWindowStart->InputType == ExpectedHoldInputType &&
				!Analysis.bHasHoldStart)
			{
				Analysis.bHasHoldStart = true;
			}
			else
			{
				AddStaleCanonicalNotify(Analysis, Index, TEXT("AnimNotify_HoldWindowStart"));
			}
		}
	}

	if (!Analysis.bHasActiveTransition)
	{
		Analysis.CanonicalNotifiesMissing.Add(TEXT("AnimNotify_AttackPhaseTransition(Active)"));
	}
	if (!Analysis.bHasRecoveryTransition)
	{
		Analysis.CanonicalNotifiesMissing.Add(TEXT("AnimNotify_AttackPhaseTransition(Recovery)"));
	}
	if (Analysis.bShouldHaveHoldStart && !Analysis.bHasHoldStart)
	{
		Analysis.CanonicalNotifiesMissing.Add(TEXT("AnimNotify_HoldWindowStart"));
	}

	Analysis.bCounterVariantHasData = AttackData->bHasCounterVariant && AttackData->CounterData != nullptr;
	Analysis.bFinisherHasData = AttackData->bCanTriggerFinisher && AttackData->FinisherData != nullptr;

	if (AttackData->bHasCounterVariant && !AttackData->CounterData)
	{
		Analysis.BranchReadinessWarnings.Add(TEXT("Counter variant is enabled but CounterData is null"));
	}
	if (AttackData->bCanTriggerFinisher && !AttackData->FinisherData)
	{
		Analysis.BranchReadinessWarnings.Add(TEXT("Finisher trigger is enabled but FinisherData is null"));
	}
	if (AttackData->CounterData && !Analysis.bHasCounterWindow && !Analysis.bHasParryWindow)
	{
		Analysis.BranchReadinessWarnings.Add(TEXT("CounterData is set but montage section has no parry or counter window"));
	}
	if (AttackData->CounterData && AttackData->CounterData->bIsLethal)
	{
		Analysis.BranchReadinessWarnings.Add(TEXT("CounterData is lethal; Chain counter steps are nonlethal by default unless runtime policy explicitly allows lethal counter data"));
	}

	Analysis.bValid = true;
	return Analysis;
}

FAttackDataNotifyPlan FAttackDataNotifyGenerationService::BuildAttackDataNotifyPlan(const FAttackDataNotifyAnalysis& Analysis, bool bRegenerateCanonicalNotifies)
{
	FAttackDataNotifyPlan Plan;
	Plan.Montage = Analysis.Montage;
	Plan.SectionName = Analysis.SectionName;
	Plan.ActiveTransitionTime = Analysis.ActiveTransitionTime;
	Plan.RecoveryTransitionTime = Analysis.RecoveryTransitionTime;
	Plan.HoldStartTime = Analysis.HoldStartTime;
	AddUniqueIndices(Plan.RemovalNotifyIndices, Analysis.LegacyNotifyIndices);
	AddUniqueIndices(Plan.RemovalNotifyIndices, Analysis.StaleCanonicalNotifyIndices);
	Plan.PlannedRemovals = Analysis.LegacyNotifiesFound;
	for (const FString& NotifyName : Analysis.StaleCanonicalNotifiesFound)
	{
		Plan.PlannedRemovals.AddUnique(NotifyName);
	}

	if (!Analysis.bValid)
	{
		Plan.Errors = Analysis.Errors;
		return Plan;
	}

	if (bRegenerateCanonicalNotifies)
	{
		AddUniqueIndices(Plan.RemovalNotifyIndices, Analysis.CanonicalNotifyIndices);
		for (const FString& NotifyName : Analysis.CanonicalNotifiesFound)
		{
			Plan.PlannedRemovals.AddUnique(NotifyName);
		}
	}

	Plan.bAddActiveTransition = bRegenerateCanonicalNotifies || !Analysis.bHasActiveTransition;
	Plan.bAddRecoveryTransition = bRegenerateCanonicalNotifies || !Analysis.bHasRecoveryTransition;
	Plan.bAddHoldStart = Analysis.bShouldHaveHoldStart && (bRegenerateCanonicalNotifies || !Analysis.bHasHoldStart);
	if (Analysis.AttackData)
	{
		Plan.HoldInputType = GetExpectedHoldInputType(Analysis.AttackData);
	}

	if (Plan.bAddActiveTransition)
	{
		Plan.PlannedAdditions.Add(TEXT("AnimNotify_AttackPhaseTransition(Active)"));
	}
	if (Plan.bAddRecoveryTransition)
	{
		Plan.PlannedAdditions.Add(TEXT("AnimNotify_AttackPhaseTransition(Recovery)"));
	}
	if (Plan.bAddHoldStart)
	{
		Plan.PlannedAdditions.Add(TEXT("AnimNotify_HoldWindowStart"));
	}

	Plan.bValid = true;
	return Plan;
}

bool FAttackDataNotifyGenerationService::ApplyAttackDataNotifyPlan(UAttackData* AttackData, const FAttackDataNotifyPlan& Plan)
{
	if (!AttackData || !AttackData->AttackMontage || !Plan.bValid || !Plan.Montage)
	{
		return false;
	}

	UAnimMontage* Montage = AttackData->AttackMontage;
	if (Montage != Plan.Montage)
	{
		return false;
	}

	Montage->Modify();

	TArray<int32> SortedRemovalIndices = Plan.RemovalNotifyIndices;
	SortedRemovalIndices.Sort(TGreater<int32>());
	for (const int32 RemovalIndex : SortedRemovalIndices)
	{
		if (Montage->Notifies.IsValidIndex(RemovalIndex))
		{
			Montage->Notifies.RemoveAt(RemovalIndex);
		}
	}

	if (Plan.bAddActiveTransition)
	{
		UAnimNotify_AttackPhaseTransition* ActiveTransition = NewObject<UAnimNotify_AttackPhaseTransition>(Montage);
		ActiveTransition->TransitionToPhase = EAttackPhase::Active;
		AddPointNotify(Montage, ActiveTransition, Plan.ActiveTransitionTime);
	}

	if (Plan.bAddRecoveryTransition)
	{
		UAnimNotify_AttackPhaseTransition* RecoveryTransition = NewObject<UAnimNotify_AttackPhaseTransition>(Montage);
		RecoveryTransition->TransitionToPhase = EAttackPhase::Recovery;
		AddPointNotify(Montage, RecoveryTransition, Plan.RecoveryTransitionTime);
	}

	if (Plan.bAddHoldStart)
	{
		UAnimNotify_HoldWindowStart* HoldStart = NewObject<UAnimNotify_HoldWindowStart>(Montage);
		HoldStart->InputType = Plan.HoldInputType;
		AddPointNotify(Montage, HoldStart, Plan.HoldStartTime);
	}

	Montage->SortNotifies();
	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();
	return true;
}
