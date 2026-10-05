// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "AttackDataNotifyGenerationService.h"
#include "AttackDataTools.h"
#include "AttackTimingDerivationLibrary.h"
#include "AttackTimingDerivationService.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "Animation/AnimNotify_HoldWindowStart.h"
#include "Animation/AnimNotifyState_AttackPhase.h"
#include "Animation/AnimNotifyState_ParryWindow.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/AttackData.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "Misc/DataValidation.h"
#include "UObject/Package.h"

namespace
{
	using FMarker = FAttackTimingNotifyMarker;

	constexpr float TimeTolerance = 1.0e-4f;

	FAttackTimingHoldRule NoHold()
	{
		return FAttackTimingHoldRule();
	}

	FAttackTimingHoldRule LightHold()
	{
		FAttackTimingHoldRule Rule;
		Rule.bUsesHold = true;
		Rule.GeneratedHoldInput = EInputType::LightAttack;
		return Rule;
	}

	FAttackTimingHoldRule ChargedHeavyHold()
	{
		FAttackTimingHoldRule Rule;
		Rule.bUsesHold = true;
		Rule.GeneratedHoldInput = EInputType::HeavyAttack;
		Rule.bHoldMustPrecedeActive = true;
		return Rule;
	}

	int32 CountErrors(const FAttackTimingDerivation& Derivation, const EAttackTimingDerivationIssue Code)
	{
		return Derivation.Errors.FilterByPredicate([Code](const FAttackTimingDerivationIssue& Issue)
		{
			return Issue.Code == Code;
		}).Num();
	}

	int32 CountWarnings(const FAttackTimingDerivation& Derivation, const EAttackTimingDerivationIssue Code)
	{
		return Derivation.Warnings.FilterByPredicate([Code](const FAttackTimingDerivationIssue& Issue)
		{
			return Issue.Code == Code;
		}).Num();
	}

	FString DescribeErrors(const FAttackTimingDerivation& Derivation)
	{
		TArray<FString> Messages;
		for (const FAttackTimingDerivationIssue& Issue : Derivation.Errors)
		{
			Messages.Add(Issue.Message);
		}
		return FString::Join(Messages, TEXT(" | "));
	}

	/** A montage with an attack section "Attack" over [0, LoopStart) and a "Loop" section after it. */
	UAnimMontage* CreateFixtureMontage(UObject* Outer, const float LoopStart)
	{
		UAnimMontage* Montage = NewObject<UAnimMontage>(Outer, NAME_None, RF_Transactional);
		Montage->SetCompositeLength(LoopStart * 2.0f);
		FCompositeSection AttackSection;
		AttackSection.SectionName = TEXT("Attack");
		AttackSection.SetTime(0.0f);
		Montage->CompositeSections.Add(AttackSection);
		FCompositeSection LoopSection;
		LoopSection.SectionName = TEXT("Loop");
		LoopSection.SetTime(LoopStart);
		Montage->CompositeSections.Add(LoopSection);
		return Montage;
	}

	void AddTransition(UAnimMontage* Montage, const EAttackPhase Phase, const float Time)
	{
		UAnimNotify_AttackPhaseTransition* Notify = NewObject<UAnimNotify_AttackPhaseTransition>(Montage);
		Notify->TransitionToPhase = Phase;
		FAnimNotifyEvent Event;
		Event.Notify = Notify;
		Event.SetTime(Time);
		Montage->Notifies.Add(Event);
	}

	void AddHold(UAnimMontage* Montage, const EInputType InputType, const float Time)
	{
		UAnimNotify_HoldWindowStart* Notify = NewObject<UAnimNotify_HoldWindowStart>(Montage);
		Notify->InputType = InputType;
		FAnimNotifyEvent Event;
		Event.Notify = Notify;
		Event.SetTime(Time);
		Montage->Notifies.Add(Event);
	}

	template <typename NotifyStateType>
	NotifyStateType* AddState(UAnimMontage* Montage, const float Time, const float Duration)
	{
		NotifyStateType* NotifyState = NewObject<NotifyStateType>(Montage);
		FAnimNotifyEvent Event;
		Event.NotifyStateClass = NotifyState;
		Event.SetTime(Time);
		Event.SetDuration(Duration);
		Montage->Notifies.Add(Event);
		return NotifyState;
	}

	/** A charged heavy on the fixture's "Attack" section that loops on "Loop"; ManualTiming keeps its defaults. */
	UAttackData* CreateChargedHeavy(UObject* Outer, UAnimMontage* Montage, const TCHAR* Name)
	{
		UAttackData* Attack = NewObject<UAttackData>(Outer, Name, RF_Transactional);
		Attack->AttackType = EAttackType::Heavy;
		Attack->AttackMontage = Montage;
		Attack->MontageSection = TEXT("Attack");
		Attack->ChargeLoopSection = TEXT("Loop");
		return Attack;
	}

	/** Package outside the transient package, so marking an asset dirty is observable. Never saved. */
	UPackage* CreateScratchPackage(const TCHAR* Purpose)
	{
		const FString PackageName = FString::Printf(TEXT("/Temp/AttackTimingDerivation/%s_%s"),
			Purpose, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* Package = CreatePackage(*PackageName);
		Package->SetDirtyFlag(false);
		return Package;
	}

	void DiscardScratchPackage(UPackage* Package)
	{
		if (Package)
		{
			Package->SetDirtyFlag(false);
		}
	}

	bool HasNotifyAt(const UAnimMontage* Montage, const float Time, const TFunctionRef<bool(const FAnimNotifyEvent&)> Predicate)
	{
		return Montage->Notifies.ContainsByPredicate([Time, &Predicate](const FAnimNotifyEvent& Event)
		{
			return FMath::IsNearlyEqual(Event.GetTriggerTime(), Time, 0.001f) && Predicate(Event);
		});
	}

	bool IsTransition(const FAnimNotifyEvent& Event, const EAttackPhase Phase)
	{
		const UAnimNotify_AttackPhaseTransition* Transition = Cast<UAnimNotify_AttackPhaseTransition>(Event.Notify);
		return Transition && Transition->TransitionToPhase == Phase;
	}

	bool IsHold(const FAnimNotifyEvent& Event, const EInputType InputType)
	{
		const UAnimNotify_HoldWindowStart* Hold = Cast<UAnimNotify_HoldWindowStart>(Event.Notify);
		return Hold && Hold->InputType == InputType;
	}
}

// ============================================================================
// PURE LIBRARY
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationPhasesTest,
	"KatanaCombat.Editor.AttackTimingDerivation.Library.DerivesPhasesFromTransitions",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationPhasesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// Section [1.0, 2.5): Windup ends at the Active transition, Active at Recovery, Recovery at the section end.
	const FAttackTimingDerivation Derivation = UAttackTimingDerivationLibrary::DeriveAttackTiming(
		{FMarker::Transition(EAttackPhase::Recovery, 1.7f), FMarker::Transition(EAttackPhase::Active, 1.3f)},
		1.0f, 2.5f, NoHold());

	TestTrue(TEXT("Both transitions derive the phases"), Derivation.IsValid());
	TestEqual(TEXT("No errors"), Derivation.Errors.Num(), 0);
	TestEqual(TEXT("No warnings for an attack without a hold"), Derivation.Warnings.Num(), 0);
	TestEqual(TEXT("Windup runs from the section start to Active"), Derivation.WindupDuration, 0.3f, TimeTolerance);
	TestEqual(TEXT("Active runs from Active to Recovery"), Derivation.ActiveDuration, 0.4f, TimeTolerance);
	TestEqual(TEXT("Recovery runs from Recovery to the section end"), Derivation.RecoveryDuration, 0.8f, TimeTolerance);
	TestFalse(TEXT("An attack without a hold derives no hold start"), Derivation.bHasHoldWindowStart);
	TestTrue(TEXT("Derived durations are finite"),
		FMath::IsFinite(Derivation.WindupDuration) && FMath::IsFinite(Derivation.ActiveDuration)
		&& FMath::IsFinite(Derivation.RecoveryDuration));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationSectionFilterTest,
	"KatanaCombat.Editor.AttackTimingDerivation.Library.ReadsOnlyTheAttackSection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationSectionFilterTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// Section [1.0, 2.0). The previous section's transitions, one exactly at the section end (which belongs to the
	// next section) and the next section's transitions must not count as duplicates.
	const TArray<FMarker> Notifies = {
		FMarker::Transition(EAttackPhase::Active, 0.3f),
		FMarker::Transition(EAttackPhase::Recovery, 0.6f),
		FMarker::Hold(EInputType::HeavyAttack, 0.2f),
		FMarker::Transition(EAttackPhase::Active, 1.4f),
		FMarker::Transition(EAttackPhase::Recovery, 1.6f),
		FMarker::Hold(EInputType::HeavyAttack, 1.1f),
		FMarker::Transition(EAttackPhase::Active, 2.0f),
		FMarker::Hold(EInputType::HeavyAttack, 2.0f),
		FMarker::Transition(EAttackPhase::Recovery, 2.5f),
	};
	const FAttackTimingDerivation Derivation =
		UAttackTimingDerivationLibrary::DeriveAttackTiming(Notifies, 1.0f, 2.0f, ChargedHeavyHold());

	TestTrue(FString::Printf(TEXT("Only the section's own notifies are read (%s)"), *DescribeErrors(Derivation)),
		Derivation.IsValid());
	TestEqual(TEXT("Windup is relative to the section start"), Derivation.WindupDuration, 0.4f, TimeTolerance);
	TestEqual(TEXT("Active comes from the section's own pair"), Derivation.ActiveDuration, 0.2f, TimeTolerance);
	TestEqual(TEXT("Recovery ends at the section end"), Derivation.RecoveryDuration, 0.4f, TimeTolerance);
	TestTrue(TEXT("The section's hold is derived"), Derivation.bHasHoldWindowStart);
	TestEqual(TEXT("Hold start is relative to the section start"), Derivation.HoldWindowStart, 0.1f, TimeTolerance);
	TestEqual(TEXT("Holds in other sections are not reported as extra holds"),
		CountWarnings(Derivation, EAttackTimingDerivationIssue::UndescribedHoldWindowStart), 0);

	const FAttackTimingDerivation EmptyRange =
		UAttackTimingDerivationLibrary::DeriveAttackTiming(Notifies, 2.0f, 2.0f, NoHold());
	TestTrue(TEXT("An empty section range is reported"),
		EmptyRange.HasError(EAttackTimingDerivationIssue::InvalidSectionRange));
	TestFalse(TEXT("An empty section range derives nothing"), EmptyRange.bHasPhaseTiming);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationMissingTransitionsTest,
	"KatanaCombat.Editor.AttackTimingDerivation.Library.ReportsMissingTransitions",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationMissingTransitionsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FAttackTimingDerivation NoActive = UAttackTimingDerivationLibrary::DeriveAttackTiming(
		{FMarker::Transition(EAttackPhase::Recovery, 0.6f)}, 0.0f, 1.0f, NoHold());
	TestTrue(TEXT("A missing Active transition is reported"),
		NoActive.HasError(EAttackTimingDerivationIssue::MissingActiveTransition));
	TestFalse(TEXT("Without Active nothing is derived"), NoActive.bHasPhaseTiming);
	TestEqual(TEXT("No Windup value is invented"), NoActive.WindupDuration, 0.0f);

	const FAttackTimingDerivation NoRecovery = UAttackTimingDerivationLibrary::DeriveAttackTiming(
		{FMarker::Transition(EAttackPhase::Active, 0.3f)}, 0.0f, 1.0f, NoHold());
	TestTrue(TEXT("A missing Recovery transition is reported"),
		NoRecovery.HasError(EAttackTimingDerivationIssue::MissingRecoveryTransition));
	TestFalse(TEXT("Without Recovery nothing is derived"), NoRecovery.bHasPhaseTiming);
	TestEqual(TEXT("No Active value is invented"), NoRecovery.ActiveDuration, 0.0f);

	// Deprecated phase states are reported but never read, so they do not stand in for transitions.
	const FAttackTimingDerivation LegacyOnly = UAttackTimingDerivationLibrary::DeriveAttackTiming(
		{FMarker::LegacyPhase(EAttackPhase::Windup, 0.0f), FMarker::LegacyPhase(EAttackPhase::Active, 0.3f),
			FMarker::LegacyPhase(EAttackPhase::Recovery, 0.6f)},
		0.0f, 1.0f, NoHold());
	TestTrue(TEXT("Legacy phase states do not supply Active"),
		LegacyOnly.HasError(EAttackTimingDerivationIssue::MissingActiveTransition));
	TestTrue(TEXT("Legacy phase states do not supply Recovery"),
		LegacyOnly.HasError(EAttackTimingDerivationIssue::MissingRecoveryTransition));
	TestEqual(TEXT("Each ignored legacy phase state is named"),
		CountWarnings(LegacyOnly, EAttackTimingDerivationIssue::LegacyPhaseStateIgnored), 3);

	const FAttackTimingDerivation OtherPhases = UAttackTimingDerivationLibrary::DeriveAttackTiming(
		{FMarker::Transition(EAttackPhase::Windup, 0.1f), FMarker::Transition(EAttackPhase::Active, 0.3f),
			FMarker::Transition(EAttackPhase::Recovery, 0.6f), FMarker::Transition(EAttackPhase::None, 0.9f)},
		0.0f, 1.0f, NoHold());
	TestTrue(TEXT("Transitions to other phases do not block the derivation"), OtherPhases.IsValid());
	TestEqual(TEXT("Transitions to other phases are reported"),
		CountWarnings(OtherPhases, EAttackTimingDerivationIssue::OtherPhaseTransition), 2);
	TestEqual(TEXT("Transitions to other phases do not move Windup"), OtherPhases.WindupDuration, 0.3f, TimeTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationPhaseOrderTest,
	"KatanaCombat.Editor.AttackTimingDerivation.Library.RejectsAmbiguousOrDegeneratePhases",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationPhaseOrderTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	struct FCase
	{
		const TCHAR* Name;
		TArray<FMarker> Notifies;
		EAttackTimingDerivationIssue Expected;
	};
	const FCase Cases[] = {
		{TEXT("Active at the section start"),
			{FMarker::Transition(EAttackPhase::Active, 1.0f), FMarker::Transition(EAttackPhase::Recovery, 1.4f)},
			EAttackTimingDerivationIssue::ActiveAtSectionStart},
		{TEXT("Recovery before Active"),
			{FMarker::Transition(EAttackPhase::Active, 1.5f), FMarker::Transition(EAttackPhase::Recovery, 1.3f)},
			EAttackTimingDerivationIssue::RecoveryNotAfterActive},
		{TEXT("Recovery at Active"),
			{FMarker::Transition(EAttackPhase::Active, 1.3f), FMarker::Transition(EAttackPhase::Recovery, 1.3f)},
			EAttackTimingDerivationIssue::RecoveryNotAfterActive},
		{TEXT("Two Active transitions"),
			{FMarker::Transition(EAttackPhase::Active, 1.2f), FMarker::Transition(EAttackPhase::Active, 1.5f),
				FMarker::Transition(EAttackPhase::Recovery, 1.7f)},
			EAttackTimingDerivationIssue::MultipleActiveTransitions},
		{TEXT("Two Recovery transitions"),
			{FMarker::Transition(EAttackPhase::Active, 1.2f), FMarker::Transition(EAttackPhase::Recovery, 1.4f),
				FMarker::Transition(EAttackPhase::Recovery, 1.7f)},
			EAttackTimingDerivationIssue::MultipleRecoveryTransitions},
	};
	for (const FCase& Case : Cases)
	{
		const FAttackTimingDerivation Derivation =
			UAttackTimingDerivationLibrary::DeriveAttackTiming(Case.Notifies, 1.0f, 2.0f, NoHold());
		TestTrue(FString::Printf(TEXT("%s: reported"), Case.Name), Derivation.HasError(Case.Expected));
		TestFalse(FString::Printf(TEXT("%s: nothing derived"), Case.Name), Derivation.bHasPhaseTiming);
		TestFalse(FString::Printf(TEXT("%s: invalid"), Case.Name), Derivation.IsValid());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationHoldStartTest,
	"KatanaCombat.Editor.AttackTimingDerivation.Library.DerivesHoldStart",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationHoldStartTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FAttackTimingDerivation Light = UAttackTimingDerivationLibrary::DeriveAttackTiming(
		{FMarker::Transition(EAttackPhase::Active, 1.3f), FMarker::Transition(EAttackPhase::Recovery, 1.5f),
			FMarker::Hold(EInputType::LightAttack, 1.45f)},
		1.0f, 2.0f, LightHold());
	TestTrue(TEXT("A light hold after Active is fine; it eases in place"), Light.IsValid());
	TestTrue(TEXT("The light hold start is derived"), Light.bHasHoldWindowStart);
	TestEqual(TEXT("The light hold start is section-relative"), Light.HoldWindowStart, 0.45f, TimeTolerance);
	TestTrue(TEXT("HoldWindowDuration is reported as not derivable"),
		Light.HasWarning(EAttackTimingDerivationIssue::HoldWindowDurationNotDerived));

	const FAttackTimingDerivation Charged = UAttackTimingDerivationLibrary::DeriveAttackTiming(
		{FMarker::Transition(EAttackPhase::Active, 0.3f), FMarker::Transition(EAttackPhase::Recovery, 0.6f),
			FMarker::Hold(EInputType::HeavyAttack, 0.2f)},
		0.0f, 1.0f, ChargedHeavyHold());
	TestTrue(TEXT("A charged hold before Active derives"), Charged.IsValid());
	TestEqual(TEXT("The charged hold start is read from the Heavy hold"), Charged.HoldWindowStart, 0.2f, TimeTolerance);
	TestTrue(TEXT("The derived charged hold precedes the derived Windup end"),
		Charged.HoldWindowStart < Charged.WindupDuration);

	const FAttackTimingDerivation Unused = UAttackTimingDerivationLibrary::DeriveAttackTiming(
		{FMarker::Transition(EAttackPhase::Active, 0.3f), FMarker::Transition(EAttackPhase::Recovery, 0.6f),
			FMarker::Hold(EInputType::LightAttack, 0.4f)},
		0.0f, 1.0f, NoHold());
	TestTrue(TEXT("Hold notifies on an attack without a hold do not block the derivation"), Unused.IsValid());
	TestFalse(TEXT("An attack without a hold derives no hold start"), Unused.bHasHoldWindowStart);
	TestTrue(TEXT("The unused hold notify is reported"),
		Unused.HasWarning(EAttackTimingDerivationIssue::HoldWindowStartNotUsed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationMissingHoldTest,
	"KatanaCombat.Editor.AttackTimingDerivation.Library.ReportsMissingHold",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationMissingHoldTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TArray<FMarker> Phases = {
		FMarker::Transition(EAttackPhase::Active, 0.3f), FMarker::Transition(EAttackPhase::Recovery, 0.6f)};

	const FAttackTimingDerivation NoHoldNotify =
		UAttackTimingDerivationLibrary::DeriveAttackTiming(Phases, 0.0f, 1.0f, ChargedHeavyHold());
	TestTrue(TEXT("A charged heavy with no hold notify is reported"),
		NoHoldNotify.HasError(EAttackTimingDerivationIssue::MissingHoldWindowStart));
	TestFalse(TEXT("No hold start is invented"), NoHoldNotify.bHasHoldWindowStart);
	TestFalse(TEXT("A missing hold blocks the whole derivation"), NoHoldNotify.IsValid());

	TArray<FMarker> LightOnly = Phases;
	LightOnly.Add(FMarker::Hold(EInputType::LightAttack, 0.2f));
	const FAttackTimingDerivation WrongInput =
		UAttackTimingDerivationLibrary::DeriveAttackTiming(LightOnly, 0.0f, 1.0f, ChargedHeavyHold());
	TestTrue(TEXT("A heavy whose only hold checks Light is reported; generation would write a Heavy hold"),
		WrongInput.HasError(EAttackTimingDerivationIssue::MissingHoldWindowStart));
	TestTrue(TEXT("The reason names the hold that was found"),
		WrongInput.Errors.ContainsByPredicate([](const FAttackTimingDerivationIssue& Issue)
		{
			return Issue.Message.Contains(TEXT("LightAttack at 0.2000s"));
		}));

	TArray<FMarker> NextSectionOnly = Phases;
	NextSectionOnly.Add(FMarker::Hold(EInputType::HeavyAttack, 1.2f));
	const FAttackTimingDerivation OutsideSection =
		UAttackTimingDerivationLibrary::DeriveAttackTiming(NextSectionOnly, 0.0f, 1.0f, ChargedHeavyHold());
	TestTrue(TEXT("A hold in the next section does not count"),
		OutsideSection.HasError(EAttackTimingDerivationIssue::MissingHoldWindowStart));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationMultipleHoldsTest,
	"KatanaCombat.Editor.AttackTimingDerivation.Library.ResolvesMultipleHolds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationMultipleHoldsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TArray<FMarker> Phases = {
		FMarker::Transition(EAttackPhase::Active, 0.3f), FMarker::Transition(EAttackPhase::Recovery, 0.6f)};

	TArray<FMarker> TwoHeavy = Phases;
	TwoHeavy.Append({FMarker::Hold(EInputType::HeavyAttack, 0.2f), FMarker::Hold(EInputType::HeavyAttack, 0.1f)});
	const FAttackTimingDerivation Earliest =
		UAttackTimingDerivationLibrary::DeriveAttackTiming(TwoHeavy, 0.0f, 1.0f, ChargedHeavyHold());
	TestTrue(TEXT("Two Heavy holds before Active derive"), Earliest.IsValid());
	TestEqual(TEXT("The earliest Heavy hold is where the charge starts"), Earliest.HoldWindowStart, 0.1f, TimeTolerance);
	TestEqual(TEXT("The later Heavy hold is reported as not described"),
		CountWarnings(Earliest, EAttackTimingDerivationIssue::UndescribedHoldWindowStart), 1);

	TArray<FMarker> MixedInputs = Phases;
	MixedInputs.Append({FMarker::Hold(EInputType::LightAttack, 0.1f), FMarker::Hold(EInputType::HeavyAttack, 0.2f)});
	const FAttackTimingDerivation Mixed =
		UAttackTimingDerivationLibrary::DeriveAttackTiming(MixedInputs, 0.0f, 1.0f, ChargedHeavyHold());
	TestTrue(TEXT("An early Light hold and an early Heavy hold derive"), Mixed.IsValid());
	TestEqual(TEXT("The hold start comes from the input generation writes, not the earliest of any input"),
		Mixed.HoldWindowStart, 0.2f, TimeTolerance);
	TestEqual(TEXT("The Light hold is reported as not described"),
		CountWarnings(Mixed, EAttackTimingDerivationIssue::UndescribedHoldWindowStart), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationChargedOrderingTest,
	"KatanaCombat.Editor.AttackTimingDerivation.Library.ChargedHoldsMustPrecedeActive",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationChargedOrderingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TArray<FMarker> Phases = {
		FMarker::Transition(EAttackPhase::Active, 0.3f), FMarker::Transition(EAttackPhase::Recovery, 0.6f)};

	TArray<FMarker> HoldAfter = Phases;
	HoldAfter.Add(FMarker::Hold(EInputType::HeavyAttack, 0.4f));
	const FAttackTimingDerivation After =
		UAttackTimingDerivationLibrary::DeriveAttackTiming(HoldAfter, 0.0f, 1.0f, ChargedHeavyHold());
	TestEqual(TEXT("A charged hold after Active is refused"),
		CountErrors(After, EAttackTimingDerivationIssue::ChargedHoldNotBeforeActive), 1);
	TestFalse(TEXT("A charged hold after Active derives nothing writable"), After.IsValid());

	TArray<FMarker> HoldAt = Phases;
	HoldAt.Add(FMarker::Hold(EInputType::HeavyAttack, 0.3f));
	const FAttackTimingDerivation At =
		UAttackTimingDerivationLibrary::DeriveAttackTiming(HoldAt, 0.0f, 1.0f, ChargedHeavyHold());
	TestTrue(TEXT("A charged hold at Active is refused, as asset validation does"),
		At.HasError(EAttackTimingDerivationIssue::ChargedHoldNotBeforeActive));

	// Any hold notify starts a charge when its own button is held, so a late Light hold fails a charged heavy
	// even though the Heavy hold that HoldWindowStart describes is early.
	TArray<FMarker> LateLight = Phases;
	LateLight.Append({FMarker::Hold(EInputType::HeavyAttack, 0.2f), FMarker::Hold(EInputType::LightAttack, 0.4f),
		FMarker::Hold(EInputType::Evade, 0.45f)});
	const FAttackTimingDerivation LateOther =
		UAttackTimingDerivationLibrary::DeriveAttackTiming(LateLight, 0.0f, 1.0f, ChargedHeavyHold());
	TestEqual(TEXT("Every late hold of any input is reported, one error each"),
		CountErrors(LateOther, EAttackTimingDerivationIssue::ChargedHoldNotBeforeActive), 2);
	TestFalse(TEXT("A late hold of another input blocks the derivation"), LateOther.IsValid());

	// The ordering rule is the charged heavy's; a light hold after Active eases in place.
	FAttackTimingHoldRule LightRule = LightHold();
	TArray<FMarker> LightAfter = Phases;
	LightAfter.Add(FMarker::Hold(EInputType::LightAttack, 0.4f));
	const FAttackTimingDerivation Light =
		UAttackTimingDerivationLibrary::DeriveAttackTiming(LightAfter, 0.0f, 1.0f, LightRule);
	TestTrue(TEXT("A light hold after Active derives"), Light.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationMergeTest,
	"KatanaCombat.Editor.AttackTimingDerivation.Library.MergesOnlyDerivedFields",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationMergeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FAttackPhaseTimingOverride Current;
	Current.WindupDuration = 0.11f;
	Current.ActiveDuration = 0.22f;
	Current.RecoveryDuration = 0.33f;
	Current.HoldWindowStart = 0.44f;
	Current.HoldWindowDuration = 0.55f;

	const FAttackTimingDerivation WithoutHold = UAttackTimingDerivationLibrary::DeriveAttackTiming(
		{FMarker::Transition(EAttackPhase::Active, 0.3f), FMarker::Transition(EAttackPhase::Recovery, 0.6f)},
		0.0f, 1.0f, NoHold());
	const FAttackPhaseTimingOverride Merged = UAttackTimingDerivationLibrary::MergeDerivedTiming(Current, WithoutHold);
	TestEqual(TEXT("Windup is replaced"), Merged.WindupDuration, 0.3f, TimeTolerance);
	TestEqual(TEXT("Active is replaced"), Merged.ActiveDuration, 0.3f, TimeTolerance);
	TestEqual(TEXT("Recovery is replaced"), Merged.RecoveryDuration, 0.4f, TimeTolerance);
	TestEqual(TEXT("An underived hold start is kept"), Merged.HoldWindowStart, 0.44f);
	TestEqual(TEXT("HoldWindowDuration is always kept"), Merged.HoldWindowDuration, 0.55f);

	const FAttackTimingDerivation Invalid = UAttackTimingDerivationLibrary::DeriveAttackTiming(
		{FMarker::Transition(EAttackPhase::Active, 0.3f)}, 0.0f, 1.0f, NoHold());
	TestTrue(TEXT("An invalid derivation changes nothing"),
		UAttackTimingDerivationLibrary::IsSameTiming(
			UAttackTimingDerivationLibrary::MergeDerivedTiming(Current, Invalid), Current));

	FAttackPhaseTimingOverride Nearly = Current;
	Nearly.WindupDuration += 0.5f * UAttackTimingDerivationLibrary::TimingEqualityToleranceSeconds;
	TestTrue(TEXT("Values within the tolerance are the same timing"),
		UAttackTimingDerivationLibrary::IsSameTiming(Current, Nearly));
	Nearly.HoldWindowDuration += 0.01f;
	TestFalse(TEXT("A difference in any field is a different timing"),
		UAttackTimingDerivationLibrary::IsSameTiming(Current, Nearly));
	return true;
}

// ============================================================================
// SERVICE: FIXTURE MONTAGES
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationServiceReadsMontageTest,
	"KatanaCombat.Editor.AttackTimingDerivation.Service.ReadsMontageNotifies",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationServiceReadsMontageTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UAnimMontage* Montage = CreateFixtureMontage(GetTransientPackage(), 1.0f);
	AddTransition(Montage, EAttackPhase::Active, 0.35f);
	AddTransition(Montage, EAttackPhase::Recovery, 0.6f);
	AddHold(Montage, EInputType::HeavyAttack, 0.2f);
	AddState<UAnimNotifyState_ParryWindow>(Montage, 0.1f, 0.2f);
	// The loop section's own phases belong to the release, not to the attack.
	AddTransition(Montage, EAttackPhase::Active, 1.2f);
	AddHold(Montage, EInputType::HeavyAttack, 1.1f);
	UAnimNotifyState_AttackPhase* LegacyState = AddState<UAnimNotifyState_AttackPhase>(Montage, 0.0f, 0.35f);
	LegacyState->Phase = EAttackPhase::Windup;

	UAttackData* Attack = CreateChargedHeavy(GetTransientPackage(), Montage, TEXT("ChargedHeavyFixture"));
	const FAttackTimingDerivation Derivation = FAttackTimingDerivationService::DeriveTimingFromMontage(Attack);
	TestTrue(FString::Printf(TEXT("The attack section's notifies derive (%s)"), *DescribeErrors(Derivation)),
		Derivation.IsValid());
	TestEqual(TEXT("Section start is read from the montage"), Derivation.SectionStart, 0.0f, TimeTolerance);
	TestEqual(TEXT("Section end is the next section's start"), Derivation.SectionEnd, 1.0f, TimeTolerance);
	TestEqual(TEXT("Windup"), Derivation.WindupDuration, 0.35f, TimeTolerance);
	TestEqual(TEXT("Active"), Derivation.ActiveDuration, 0.25f, TimeTolerance);
	TestEqual(TEXT("Recovery"), Derivation.RecoveryDuration, 0.4f, TimeTolerance);
	TestEqual(TEXT("Hold start"), Derivation.HoldWindowStart, 0.2f, TimeTolerance);
	TestTrue(TEXT("The deprecated phase state is named, not read"),
		Derivation.HasWarning(EAttackTimingDerivationIssue::LegacyPhaseStateIgnored));
	TestTrue(TEXT("Read-only derivation leaves the timing untouched"),
		UAttackTimingDerivationLibrary::IsSameTiming(Attack->ManualTiming, FAttackPhaseTimingOverride()));

	const FAttackTimingHoldRule Rule = FAttackTimingDerivationService::MakeHoldRule(Attack);
	TestTrue(TEXT("A heavy with a charge loop uses a hold"), Rule.bUsesHold);
	TestEqual(TEXT("Its hold checks the input generation writes"), Rule.GeneratedHoldInput,
		FAttackDataNotifyGenerationService::GetGeneratedHoldInputType(Attack));
	TestTrue(TEXT("Its holds must precede Active"), Rule.bHoldMustPrecedeActive);

	UAttackData* NoLoop = CreateChargedHeavy(GetTransientPackage(), Montage, TEXT("HeavyWithoutChargeLoop"));
	NoLoop->ChargeLoopSection = NAME_None;
	const FAttackTimingDerivation Uncharged = FAttackTimingDerivationService::DeriveTimingFromMontage(NoLoop);
	TestTrue(TEXT("A heavy without a charge loop derives its phases"), Uncharged.IsValid());
	TestFalse(TEXT("A heavy without a charge loop derives no hold start"), Uncharged.bHasHoldWindowStart);

	UAttackData* MissingSection = CreateChargedHeavy(GetTransientPackage(), Montage, TEXT("MissingSectionFixture"));
	MissingSection->MontageSection = TEXT("NotInTheMontage");
	TestTrue(TEXT("A section the montage lacks is reported"),
		FAttackTimingDerivationService::DeriveTimingFromMontage(MissingSection)
			.HasError(EAttackTimingDerivationIssue::MissingSection));

	UAttackData* NoMontage = CreateChargedHeavy(GetTransientPackage(), nullptr, TEXT("NoMontageFixture"));
	TestTrue(TEXT("A missing montage is reported"),
		FAttackTimingDerivationService::DeriveTimingFromMontage(NoMontage)
			.HasError(EAttackTimingDerivationIssue::MissingMontage));
	TestTrue(TEXT("Missing attack data is reported"),
		FAttackTimingDerivationService::DeriveTimingFromMontage(nullptr)
			.HasError(EAttackTimingDerivationIssue::MissingAttackData));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationServiceRoundTripTest,
	"KatanaCombat.Editor.AttackTimingDerivation.Service.RoundTripLetsNotifiesRegenerate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationServiceRoundTripTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!TestNotNull(TEXT("The editor is available"), GEditor) || !TestNotNull(TEXT("The editor has a transaction buffer"), GEditor ? GEditor->Trans.Get() : nullptr))
	{
		return false;
	}

	// The montage is authored; the attack keeps the ManualTiming defaults, whose hold (0.5 s) comes after their
	// Windup (0.3 s), so notify generation refuses it until the timing describes the montage.
	UPackage* Package = CreateScratchPackage(TEXT("RoundTrip"));
	UAnimMontage* Montage = CreateFixtureMontage(Package, 1.0f);
	AddTransition(Montage, EAttackPhase::Active, 0.35f);
	AddTransition(Montage, EAttackPhase::Recovery, 0.6f);
	AddHold(Montage, EInputType::HeavyAttack, 0.2f);
	UAttackData* Attack = CreateChargedHeavy(Package, Montage, TEXT("ChargedHeavyRoundTrip"));
	const FAttackPhaseTimingOverride Defaults = Attack->ManualTiming;
	const TArray<FAnimNotifyEvent> AuthoredNotifies = Montage->Notifies;

	TestFalse(TEXT("Before: notify generation refuses the default timing"),
		FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(Attack).bValid);
	Package->SetDirtyFlag(false);

	const int32 QueueBefore = GEditor->Trans->GetQueueLength();
	const FAttackTimingApplyResult Applied = FAttackTimingDerivationService::ApplyTimingFromMontage(Attack);
	TestEqual(FString::Printf(TEXT("The derived timing is written (%s)"), *DescribeErrors(Applied.Derivation)),
		Applied.Outcome, EAttackTimingApplyOutcome::Applied);
	TestEqual(TEXT("Windup matches the Active notify"), Attack->ManualTiming.WindupDuration, 0.35f, TimeTolerance);
	TestEqual(TEXT("Active matches the Recovery notify"), Attack->ManualTiming.ActiveDuration, 0.25f, TimeTolerance);
	TestEqual(TEXT("Recovery runs to the section end"), Attack->ManualTiming.RecoveryDuration, 0.4f, TimeTolerance);
	TestEqual(TEXT("Hold start matches the hold notify"), Attack->ManualTiming.HoldWindowStart, 0.2f, TimeTolerance);
	TestEqual(TEXT("HoldWindowDuration is kept"), Attack->ManualTiming.HoldWindowDuration, Defaults.HoldWindowDuration);
	TestTrue(TEXT("The result reports the values written"),
		UAttackTimingDerivationLibrary::IsSameTiming(Applied.NewTiming, Attack->ManualTiming));
	TestTrue(TEXT("The result reports the values replaced"),
		UAttackTimingDerivationLibrary::IsSameTiming(Applied.PreviousTiming, Defaults));
	TestTrue(TEXT("The asset is marked dirty"), Package->IsDirty());
	TestEqual(TEXT("The write is one undoable transaction"), GEditor->Trans->GetQueueLength(), QueueBefore + 1);
	TestTrue(TEXT("The montage is not touched"), Montage->Notifies.Num() == AuthoredNotifies.Num());

	TArray<FText> OrderingErrors;
	TestTrue(TEXT("After: the charged-hold ordering check passes"), Attack->ValidateChargedHoldOrdering(OrderingErrors));
	FDataValidationContext ValidationContext;
	TestEqual(TEXT("After: asset validation passes"), Attack->IsDataValid(ValidationContext), EDataValidationResult::Valid);
	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(Attack);
	TestTrue(TEXT("After: notify generation accepts the timing"), Analysis.bValid);
	TestEqual(TEXT("After: the authored notifies are exactly what generation would write"),
		Analysis.CanonicalNotifiesMissing.Num() + Analysis.StaleCanonicalNotifiesFound.Num(), 0);
	TestFalse(TEXT("After: an add-missing migration plan has nothing to do"),
		FAttackDataNotifyGenerationService::BuildAttackDataNotifyPlan(Analysis, false).HasChanges());

	const int32 QueueAfterApply = GEditor->Trans->GetQueueLength();
	const FAttackTimingApplyResult Again = FAttackTimingDerivationService::ApplyTimingFromMontage(Attack);
	TestEqual(TEXT("Running it again finds nothing to change"), Again.Outcome, EAttackTimingApplyOutcome::Unchanged);
	TestEqual(TEXT("An unchanged run records no transaction"), GEditor->Trans->GetQueueLength(), QueueAfterApply);

	const FText UndoTitle = GEditor->Trans->GetUndoContext(false).Title;
	if (TestTrue(TEXT("The newest undo entry is the derivation"),
		UndoTitle.ToString() == TEXT("Derive Attack Timing From Montage")))
	{
		GEditor->UndoTransaction();
		TestTrue(TEXT("Undo restores the previous timing"),
			UAttackTimingDerivationLibrary::IsSameTiming(Attack->ManualTiming, Defaults));
		GEditor->RedoTransaction();
		TestTrue(TEXT("Redo writes the derived timing again"),
			UAttackTimingDerivationLibrary::IsSameTiming(Attack->ManualTiming, Applied.NewTiming));
	}

	TestTrue(TEXT("After: notify generation regenerates the montage"), UAttackDataTools::GenerateAllNotifies(Attack));
	TestTrue(TEXT("Regenerated Active lands on the authored time"),
		HasNotifyAt(Montage, 0.35f, [](const FAnimNotifyEvent& Event) { return IsTransition(Event, EAttackPhase::Active); }));
	TestTrue(TEXT("Regenerated Recovery lands on the authored time"),
		HasNotifyAt(Montage, 0.6f, [](const FAnimNotifyEvent& Event) { return IsTransition(Event, EAttackPhase::Recovery); }));
	TestTrue(TEXT("Regenerated hold lands on the authored time and checks Heavy"),
		HasNotifyAt(Montage, 0.2f, [](const FAnimNotifyEvent& Event) { return IsHold(Event, EInputType::HeavyAttack); }));
	TArray<FText> RegeneratedErrors;
	TestTrue(TEXT("The regenerated montage passes the charged-hold ordering check"),
		Attack->ValidateChargedHoldOrdering(RegeneratedErrors));

	DiscardScratchPackage(Package);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationServiceRefusalTest,
	"KatanaCombat.Editor.AttackTimingDerivation.Service.RefusalWritesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationServiceRefusalTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!TestNotNull(TEXT("The editor is available"), GEditor) || !TestNotNull(TEXT("The editor has a transaction buffer"), GEditor ? GEditor->Trans.Get() : nullptr))
	{
		return false;
	}

	struct FCase
	{
		const TCHAR* Name;
		float Active;
		float Recovery;
		float Hold;
		EAttackTimingDerivationIssue Expected;
	};
	const FCase Cases[] = {
		{TEXT("Hold after Active"), 0.3f, 0.6f, 0.4f, EAttackTimingDerivationIssue::ChargedHoldNotBeforeActive},
		{TEXT("No Recovery"), 0.3f, -1.0f, 0.2f, EAttackTimingDerivationIssue::MissingRecoveryTransition},
		{TEXT("No hold"), 0.3f, 0.6f, -1.0f, EAttackTimingDerivationIssue::MissingHoldWindowStart},
	};
	for (const FCase& Case : Cases)
	{
		UPackage* Package = CreateScratchPackage(TEXT("Refusal"));
		UAnimMontage* Montage = CreateFixtureMontage(Package, 1.0f);
		AddTransition(Montage, EAttackPhase::Active, Case.Active);
		if (Case.Recovery >= 0.0f)
		{
			AddTransition(Montage, EAttackPhase::Recovery, Case.Recovery);
		}
		if (Case.Hold >= 0.0f)
		{
			AddHold(Montage, EInputType::HeavyAttack, Case.Hold);
		}
		UAttackData* Attack = CreateChargedHeavy(Package, Montage, TEXT("RefusedChargedHeavy"));
		const FAttackPhaseTimingOverride Before = Attack->ManualTiming;
		Package->SetDirtyFlag(false);
		const int32 QueueBefore = GEditor->Trans->GetQueueLength();

		FAttackTimingApplyResult Result;
		TestFalse(FString::Printf(TEXT("%s: the tool entry point reports failure"), Case.Name),
			UAttackDataTools::DeriveTimingFromMontage(Attack, Result));
		TestEqual(FString::Printf(TEXT("%s: refused"), Case.Name), Result.Outcome, EAttackTimingApplyOutcome::Refused);
		TestTrue(FString::Printf(TEXT("%s: the reason is reported"), Case.Name), Result.Derivation.HasError(Case.Expected));
		TestTrue(FString::Printf(TEXT("%s: the timing is untouched"), Case.Name),
			UAttackTimingDerivationLibrary::IsSameTiming(Attack->ManualTiming, Before));
		TestFalse(FString::Printf(TEXT("%s: the asset is not dirtied"), Case.Name), Package->IsDirty());
		TestEqual(FString::Printf(TEXT("%s: no transaction is recorded"), Case.Name),
			GEditor->Trans->GetQueueLength(), QueueBefore);
		TestTrue(FString::Printf(TEXT("%s: the description gives the reason"), Case.Name),
			FAttackTimingDerivationService::DescribeResult(Result).Contains(Result.Derivation.Errors[0].Message));
		DiscardScratchPackage(Package);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationBatchTest,
	"KatanaCombat.Editor.AttackTimingDerivation.Service.BatchIsOneUndo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationBatchTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!TestNotNull(TEXT("The editor is available"), GEditor) || !TestNotNull(TEXT("The editor has a transaction buffer"), GEditor ? GEditor->Trans.Get() : nullptr))
	{
		return false;
	}

	UPackage* Package = CreateScratchPackage(TEXT("Batch"));
	UAnimMontage* Good = CreateFixtureMontage(Package, 1.0f);
	AddTransition(Good, EAttackPhase::Active, 0.35f);
	AddTransition(Good, EAttackPhase::Recovery, 0.6f);
	AddHold(Good, EInputType::HeavyAttack, 0.2f);
	UAnimMontage* Late = CreateFixtureMontage(Package, 1.0f);
	AddTransition(Late, EAttackPhase::Active, 0.3f);
	AddTransition(Late, EAttackPhase::Recovery, 0.6f);
	AddHold(Late, EInputType::HeavyAttack, 0.45f);
	UAttackData* First = CreateChargedHeavy(Package, Good, TEXT("BatchFirst"));
	UAttackData* Second = CreateChargedHeavy(Package, Good, TEXT("BatchSecond"));
	UAttackData* Refused = CreateChargedHeavy(Package, Late, TEXT("BatchRefused"));
	const FAttackPhaseTimingOverride Defaults = First->ManualTiming;

	const int32 QueueBefore = GEditor->Trans->GetQueueLength();
	TArray<FAttackTimingApplyResult> Results;
	int32 AppliedCount = 0;
	int32 UnchangedCount = 0;
	int32 RefusedCount = 0;
	TestFalse(TEXT("A batch with a refused asset reports it"),
		UAttackDataTools::BatchDeriveTimingFromMontage({First, Second, Refused}, Results, AppliedCount, UnchangedCount, RefusedCount));
	TestEqual(TEXT("One result per asset"), Results.Num(), 3);
	TestEqual(TEXT("Two assets are written"), AppliedCount, 2);
	TestEqual(TEXT("None already matched"), UnchangedCount, 0);
	TestEqual(TEXT("One asset is refused"), RefusedCount, 1);
	TestTrue(TEXT("The refused asset keeps its timing"),
		UAttackTimingDerivationLibrary::IsSameTiming(Refused->ManualTiming, Defaults));
	TestEqual(TEXT("The batch is one undoable transaction"), GEditor->Trans->GetQueueLength(), QueueBefore + 1);
	TestTrue(TEXT("The summary counts every outcome"),
		FAttackTimingDerivationService::DescribeResults(Results).StartsWith(TEXT("2 written, 0 already matching, 1 not changed.")));

	if (TestTrue(TEXT("The newest undo entry is the batch"),
		GEditor->Trans->GetUndoContext(false).Title.ToString() == TEXT("Derive Attack Timing From Montage (Selected Assets)")))
	{
		GEditor->UndoTransaction();
		TestTrue(TEXT("One undo restores the first asset"),
			UAttackTimingDerivationLibrary::IsSameTiming(First->ManualTiming, Defaults));
		TestTrue(TEXT("One undo restores the second asset"),
			UAttackTimingDerivationLibrary::IsSameTiming(Second->ManualTiming, Defaults));
	}

	const int32 QueueBeforeRefusedBatch = GEditor->Trans->GetQueueLength();
	TArray<FAttackTimingApplyResult> RefusedOnly;
	UAttackDataTools::BatchDeriveTimingFromMontage({Refused}, RefusedOnly, AppliedCount, UnchangedCount, RefusedCount);
	TestEqual(TEXT("A batch that writes nothing leaves no undo entry"),
		GEditor->Trans->GetQueueLength(), QueueBeforeRefusedBatch);

	DiscardScratchPackage(Package);
	return true;
}

// ============================================================================
// SHIPPED CONTENT INVARIANT
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTimingDerivationShippedChargedHeaviesTest,
	"KatanaCombat.Editor.AttackTimingDerivation.ShippedChargedHeaviesDeriveOrderedTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackTimingDerivationShippedChargedHeaviesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// Invariant over shipped content: every charged heavy's montage describes timing the action can write, with
	// the hold before Windup ends, and notify generation accepts that timing. Shipped assets are only read; the
	// action runs on a transient copy that still plays the shipped montage. Values are logged, never asserted.
	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();
	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByClass(UAttackData::StaticClass()->GetClassPathName(), Assets, true);
	int32 ChargedHeavies = 0;
	for (const FAssetData& Asset : Assets)
	{
		if (!Asset.PackageName.ToString().StartsWith(TEXT("/Game/")))
		{
			continue;
		}
		const UAttackData* Attack = Cast<UAttackData>(Asset.GetAsset());
		if (!Attack
			|| Attack->AttackType != EAttackType::Heavy
			|| Attack->ChargeLoopSection.IsNone()
			|| !Attack->AttackMontage)
		{
			continue;
		}

		++ChargedHeavies;
		const FString AssetName = Asset.AssetName.ToString();
		const FAttackTimingDerivation Derivation = FAttackTimingDerivationService::DeriveTimingFromMontage(Attack);
		if (!Derivation.IsValid())
		{
			AddError(FString::Printf(TEXT("%s: its montage section does not describe writable timing: %s"),
				*Asset.GetObjectPathString(), *DescribeErrors(Derivation)));
			continue;
		}
		if (!Derivation.bHasHoldWindowStart || !(Derivation.HoldWindowStart < Derivation.WindupDuration))
		{
			AddError(FString::Printf(TEXT("%s: the derived hold start does not precede the end of Windup"),
				*Asset.GetObjectPathString()));
		}

		const FAttackDataNotifyAnalysis StoredAnalysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(Attack);
		AddInfo(FString::Printf(TEXT("%s with its stored timing: notify generation %s%s"), *AssetName,
			StoredAnalysis.bValid ? TEXT("accepts it") : TEXT("refuses it: "),
			*FString::Join(StoredAnalysis.Errors, TEXT(" | "))));

		UAttackData* Copy = DuplicateObject<UAttackData>(Attack, GetTransientPackage());
		Copy->ClearFlags(RF_Public | RF_Standalone);
		const FAttackTimingApplyResult Result = FAttackTimingDerivationService::ApplyTimingFromMontage(Copy);
		TestTrue(FString::Printf(TEXT("%s: the action writes or already matches"), *AssetName),
			Result.Outcome != EAttackTimingApplyOutcome::Refused);
		const FAttackDataNotifyAnalysis DerivedAnalysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(Copy);
		if (!DerivedAnalysis.bValid)
		{
			AddError(FString::Printf(TEXT("%s: notify generation refuses the derived timing: %s"),
				*Asset.GetObjectPathString(), *FString::Join(DerivedAnalysis.Errors, TEXT(" | "))));
		}
		TArray<FText> OrderingErrors;
		TestTrue(FString::Printf(TEXT("%s: the charged-hold ordering check passes"), *AssetName),
			Copy->ValidateChargedHoldOrdering(OrderingErrors));
		AddInfo(FString::Printf(TEXT("%s with the derived timing: notify generation %s; missing generated notifies %d, notifies regeneration would replace %d"),
			*AssetName, DerivedAnalysis.bValid ? TEXT("accepts it") : TEXT("refuses it"),
			DerivedAnalysis.CanonicalNotifiesMissing.Num(), DerivedAnalysis.StaleCanonicalNotifiesFound.Num()));
		TArray<FString> Lines;
		FAttackTimingDerivationService::DescribeResult(Result).ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			AddInfo(Line);
		}
	}
	AddInfo(FString::Printf(TEXT("Derived timing for %d shipped charged heavy attacks"), ChargedHeavies));
	return true;
}

#endif // WITH_EDITOR
