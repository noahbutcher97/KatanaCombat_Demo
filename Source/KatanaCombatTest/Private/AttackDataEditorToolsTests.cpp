// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "AttackDataTools.h"
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
	UAnimMontage* CreateTransientMontageWithSections()
	{
		UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage());
		Montage->SetCompositeLength(2.0f);

		FCompositeSection TargetSection;
		TargetSection.SectionName = TEXT("Target");
		TargetSection.SetTime(0.0f);
		Montage->CompositeSections.Add(TargetSection);

		FCompositeSection NextSection;
		NextSection.SectionName = TEXT("Next");
		NextSection.SetTime(1.0f);
		Montage->CompositeSections.Add(NextSection);

		return Montage;
	}

	UAttackData* CreateValidLightAttackData(UAnimMontage* Montage)
	{
		UAttackData* AttackData = NewObject<UAttackData>(GetTransientPackage());
		AttackData->AttackMontage = Montage;
		AttackData->MontageSection = TEXT("Target");
		AttackData->AttackType = EAttackType::Light;
		AttackData->bCanHold = true;
		AttackData->ComboInputWindow = 0.25f;
		AttackData->ManualTiming.WindupDuration = 0.30f;
		AttackData->ManualTiming.ActiveDuration = 0.20f;
		AttackData->ManualTiming.RecoveryDuration = 0.50f;
		AttackData->ManualTiming.HoldWindowStart = 0.45f;
		AttackData->ManualTiming.HoldWindowDuration = 0.10f;
		return AttackData;
	}

	template <typename NotifyType>
	void AddPointNotify(UAnimMontage* Montage, float Time)
	{
		NotifyType* Notify = NewObject<NotifyType>(Montage);
		FAnimNotifyEvent NotifyEvent;
		NotifyEvent.Notify = Notify;
		NotifyEvent.SetTime(Time);
		Montage->Notifies.Add(NotifyEvent);
	}

	template <typename NotifyStateType>
	void AddStateNotify(UAnimMontage* Montage, float Time, float Duration)
	{
		NotifyStateType* NotifyState = NewObject<NotifyStateType>(Montage);
		FAnimNotifyEvent NotifyEvent;
		NotifyEvent.NotifyStateClass = NotifyState;
		NotifyEvent.SetTime(Time);
		NotifyEvent.SetDuration(Duration);
		Montage->Notifies.Add(NotifyEvent);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataGenerateAllNotifiesCurrentDefaultsTest,
	"KatanaCombat.Editor.AttackDataTools.GenerateAllNotifies.CurrentDefaults",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataGenerateAllNotifiesCurrentDefaultsTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);

	AddPointNotify<UAnimNotify_ToggleHitDetection>(Montage, 0.40f);
	AddStateNotify<UAnimNotifyState_AttackPhase>(Montage, 0.10f, 0.20f);
	AddStateNotify<UAnimNotifyState_HoldWindow>(Montage, 0.45f, 0.10f);
	AddStateNotify<UAnimNotifyState_ComboWindow>(Montage, 0.50f, 0.20f);

	TestTrue(TEXT("GenerateAllNotifies should succeed with valid manual timing"),
		UAttackDataTools::GenerateAllNotifies(AttackData));

	bool bHasActiveTransition = false;
	bool bHasRecoveryTransition = false;
	bool bHasHoldWindowStart = false;
	bool bHasDeprecatedHitToggle = false;
	bool bHasDeprecatedAttackPhase = false;
	bool bHasDeprecatedHoldState = false;
	bool bHasExplicitComboState = false;

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (const UAnimNotify_AttackPhaseTransition* PhaseTransition =
			Cast<UAnimNotify_AttackPhaseTransition>(NotifyEvent.Notify))
		{
			bHasActiveTransition |= PhaseTransition->TransitionToPhase == EAttackPhase::Active;
			bHasRecoveryTransition |= PhaseTransition->TransitionToPhase == EAttackPhase::Recovery;
		}

		if (const UAnimNotify_HoldWindowStart* HoldStart =
			Cast<UAnimNotify_HoldWindowStart>(NotifyEvent.Notify))
		{
			bHasHoldWindowStart |= HoldStart->InputType == EInputType::LightAttack;
		}

		bHasDeprecatedHitToggle |= NotifyEvent.Notify &&
			NotifyEvent.Notify->IsA(UAnimNotify_ToggleHitDetection::StaticClass());
		bHasDeprecatedAttackPhase |= NotifyEvent.NotifyStateClass &&
			NotifyEvent.NotifyStateClass->IsA(UAnimNotifyState_AttackPhase::StaticClass());
		bHasDeprecatedHoldState |= NotifyEvent.NotifyStateClass &&
			NotifyEvent.NotifyStateClass->IsA(UAnimNotifyState_HoldWindow::StaticClass());
		bHasExplicitComboState |= NotifyEvent.NotifyStateClass &&
			NotifyEvent.NotifyStateClass->IsA(UAnimNotifyState_ComboWindow::StaticClass());
	}

	TestTrue(TEXT("Generated notifies should include transition to Active"), bHasActiveTransition);
	TestTrue(TEXT("Generated notifies should include transition to Recovery"), bHasRecoveryTransition);
	TestTrue(TEXT("Generated notifies should include event-driven hold start"), bHasHoldWindowStart);
	TestFalse(TEXT("Default generation should remove deprecated hit-detection toggles"), bHasDeprecatedHitToggle);
	TestFalse(TEXT("Default generation should remove deprecated attack phase states"), bHasDeprecatedAttackPhase);
	TestFalse(TEXT("Default generation should remove deprecated hold window states"), bHasDeprecatedHoldState);
	TestFalse(TEXT("Default generation should not seed explicit combo window states"), bHasExplicitComboState);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataGenerateAllNotifiesReseedsExistingCanonicalTimingTest,
	"KatanaCombat.Editor.AttackDataTools.GenerateAllNotifies.ReseedsExistingCanonicalTiming",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataGenerateAllNotifiesReseedsExistingCanonicalTimingTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);

	UAnimNotify_AttackPhaseTransition* StaleActive = NewObject<UAnimNotify_AttackPhaseTransition>(Montage);
	StaleActive->TransitionToPhase = EAttackPhase::Active;
	FAnimNotifyEvent StaleActiveEvent;
	StaleActiveEvent.Notify = StaleActive;
	StaleActiveEvent.SetTime(0.10f);
	Montage->Notifies.Add(StaleActiveEvent);

	UAnimNotify_AttackPhaseTransition* StaleRecovery = NewObject<UAnimNotify_AttackPhaseTransition>(Montage);
	StaleRecovery->TransitionToPhase = EAttackPhase::Recovery;
	FAnimNotifyEvent StaleRecoveryEvent;
	StaleRecoveryEvent.Notify = StaleRecovery;
	StaleRecoveryEvent.SetTime(0.80f);
	Montage->Notifies.Add(StaleRecoveryEvent);

	AddPointNotify<UAnimNotify_HoldWindowStart>(Montage, 0.20f);

	TestTrue(TEXT("GenerateAllNotifies should succeed when reseeding existing canonical notifies"),
		UAttackDataTools::GenerateAllNotifies(AttackData));

	int32 PhaseTransitionCount = 0;
	int32 HoldStartCount = 0;
	bool bActiveAtExpectedTime = false;
	bool bRecoveryAtExpectedTime = false;
	bool bHoldStartAtExpectedTime = false;

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (const UAnimNotify_AttackPhaseTransition* Transition =
			Cast<UAnimNotify_AttackPhaseTransition>(NotifyEvent.Notify))
		{
			++PhaseTransitionCount;
			bActiveAtExpectedTime |=
				Transition->TransitionToPhase == EAttackPhase::Active &&
				FMath::IsNearlyEqual(NotifyEvent.GetTriggerTime(), 0.30f);
			bRecoveryAtExpectedTime |=
				Transition->TransitionToPhase == EAttackPhase::Recovery &&
				FMath::IsNearlyEqual(NotifyEvent.GetTriggerTime(), 0.50f);
		}

		if (NotifyEvent.Notify && NotifyEvent.Notify->IsA(UAnimNotify_HoldWindowStart::StaticClass()))
		{
			++HoldStartCount;
			bHoldStartAtExpectedTime |= FMath::IsNearlyEqual(NotifyEvent.GetTriggerTime(), 0.45f);
		}
	}

	TestEqual(TEXT("Existing phase transitions should be replaced, not duplicated"), PhaseTransitionCount, 2);
	TestEqual(TEXT("Existing hold start should be replaced, not duplicated"), HoldStartCount, 1);
	TestTrue(TEXT("Active transition should be reseeded from current timing"), bActiveAtExpectedTime);
	TestTrue(TEXT("Recovery transition should be reseeded from current timing"), bRecoveryAtExpectedTime);
	TestTrue(TEXT("Hold start should be reseeded from current timing"), bHoldStartAtExpectedTime);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataGenerateAllNotifiesPreservesOutsideSectionTest,
	"KatanaCombat.Editor.AttackDataTools.GenerateAllNotifies.PreservesOutsideSection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataGenerateAllNotifiesPreservesOutsideSectionTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);

	AddStateNotify<UAnimNotifyState_ComboWindow>(Montage, 0.50f, 0.20f);
	AddStateNotify<UAnimNotifyState_ComboWindow>(Montage, 1.20f, 0.20f);

	TestTrue(TEXT("GenerateAllNotifies should succeed for target section"),
		UAttackDataTools::GenerateAllNotifies(AttackData));

	int32 ComboStateCount = 0;
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (NotifyEvent.NotifyStateClass &&
			NotifyEvent.NotifyStateClass->IsA(UAnimNotifyState_ComboWindow::StaticClass()))
		{
			++ComboStateCount;
			TestTrue(TEXT("Only outside-section combo state should remain"),
				NotifyEvent.GetTriggerTime() >= 1.0f);
		}
	}

	TestEqual(TEXT("Outside-section explicit combo state should be preserved"), ComboStateCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataGenerateAllNotifiesZeroHoldDurationStillSeedsStartTest,
	"KatanaCombat.Editor.AttackDataTools.GenerateAllNotifies.ZeroHoldDurationStillSeedsStart",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataGenerateAllNotifiesZeroHoldDurationStillSeedsStartTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);
	AttackData->ManualTiming.HoldWindowDuration = 0.0f;

	TestTrue(TEXT("GenerateAllNotifies should not require legacy hold duration for point notify generation"),
		UAttackDataTools::GenerateAllNotifies(AttackData));

	bool bHasHoldWindowStart = false;
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (const UAnimNotify_HoldWindowStart* HoldStart =
			Cast<UAnimNotify_HoldWindowStart>(NotifyEvent.Notify))
		{
			bHasHoldWindowStart |= HoldStart->InputType == EInputType::LightAttack;
		}
	}

	TestTrue(TEXT("Holdable light attacks should seed the event-driven hold start even with zero legacy duration"),
		bHasHoldWindowStart);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataGenerateAllNotifiesRejectsInvalidTimingWithoutMutationTest,
	"KatanaCombat.Editor.AttackDataTools.GenerateAllNotifies.RejectsInvalidTimingWithoutMutation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataGenerateAllNotifiesRejectsInvalidTimingWithoutMutationTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);
	AttackData->ManualTiming.WindupDuration = 0.80f;
	AttackData->ManualTiming.ActiveDuration = 0.40f;
	AttackData->ManualTiming.RecoveryDuration = 0.10f;

	AddStateNotify<UAnimNotifyState_ComboWindow>(Montage, 0.50f, 0.20f);
	const int32 NotifyCountBefore = Montage->Notifies.Num();

	TestFalse(TEXT("GenerateAllNotifies should reject timing outside section length"),
		UAttackDataTools::GenerateAllNotifies(AttackData));
	TestEqual(TEXT("Invalid generation should not mutate notify count"), Montage->Notifies.Num(), NotifyCountBefore);

	bool bLegacyComboStillPresent = false;
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		bLegacyComboStillPresent |= NotifyEvent.NotifyStateClass &&
			NotifyEvent.NotifyStateClass->IsA(UAnimNotifyState_ComboWindow::StaticClass());
	}

	TestTrue(TEXT("Invalid generation should preserve existing notifies"), bLegacyComboStillPresent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataGenerateAllNotifiesRejectsMissingSectionWithoutMutationTest,
	"KatanaCombat.Editor.AttackDataTools.GenerateAllNotifies.RejectsMissingSectionWithoutMutation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataGenerateAllNotifiesRejectsMissingSectionWithoutMutationTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);
	AttackData->MontageSection = TEXT("Missing");

	AddStateNotify<UAnimNotifyState_ComboWindow>(Montage, 0.50f, 0.20f);
	const int32 NotifyCountBefore = Montage->Notifies.Num();

	TestFalse(TEXT("GenerateAllNotifies should reject missing montage section"),
		UAttackDataTools::GenerateAllNotifies(AttackData));
	TestEqual(TEXT("Missing section should not mutate notify count"), Montage->Notifies.Num(), NotifyCountBefore);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataGenerateAllNotifiesRestoresAutoTimingOnFailureTest,
	"KatanaCombat.Editor.AttackDataTools.GenerateAllNotifies.RestoresAutoTimingOnFailure",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataGenerateAllNotifiesRestoresAutoTimingOnFailureTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);
	AttackData->AttackType = EAttackType::Heavy;
	AttackData->ChargeLoopSection = TEXT("ChargeLoop");
	AttackData->ManualTiming.WindupDuration = 0.0f;
	AttackData->ManualTiming.ActiveDuration = 0.0f;
	AttackData->ManualTiming.RecoveryDuration = 0.0f;
	AttackData->ManualTiming.HoldWindowStart = 2.0f;

	const FAttackPhaseTimingOverride TimingBefore = AttackData->ManualTiming;
	const int32 NotifyCountBefore = Montage->Notifies.Num();

	TestFalse(TEXT("GenerateAllNotifies should reject invalid auto-calculated hold timing"),
		UAttackDataTools::GenerateAllNotifies(AttackData));
	TestEqual(TEXT("Failed generation should restore notify count"), Montage->Notifies.Num(), NotifyCountBefore);
	TestEqual(TEXT("Windup duration should be restored"), AttackData->ManualTiming.WindupDuration, TimingBefore.WindupDuration);
	TestEqual(TEXT("Active duration should be restored"), AttackData->ManualTiming.ActiveDuration, TimingBefore.ActiveDuration);
	TestEqual(TEXT("Recovery duration should be restored"), AttackData->ManualTiming.RecoveryDuration, TimingBefore.RecoveryDuration);
	TestEqual(TEXT("Hold window start should be restored"), AttackData->ManualTiming.HoldWindowStart, TimingBefore.HoldWindowStart);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataEffectiveTimingUsesPhaseTransitionsTest,
	"KatanaCombat.Editor.AttackData.EffectiveTiming.UsesPhaseTransitions",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataEffectiveTimingUsesPhaseTransitionsTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);
	AttackData->bUseAnimNotifyTiming = true;
	AttackData->ManualTiming.WindupDuration = 0.11f;
	AttackData->ManualTiming.ActiveDuration = 0.12f;
	AttackData->ManualTiming.RecoveryDuration = 0.13f;

	UAnimNotify_AttackPhaseTransition* ActiveNotify = NewObject<UAnimNotify_AttackPhaseTransition>(Montage);
	ActiveNotify->TransitionToPhase = EAttackPhase::Active;
	FAnimNotifyEvent ActiveEvent;
	ActiveEvent.Notify = ActiveNotify;
	ActiveEvent.SetTime(0.30f);
	Montage->Notifies.Add(ActiveEvent);

	UAnimNotify_AttackPhaseTransition* RecoveryNotify = NewObject<UAnimNotify_AttackPhaseTransition>(Montage);
	RecoveryNotify->TransitionToPhase = EAttackPhase::Recovery;
	FAnimNotifyEvent RecoveryEvent;
	RecoveryEvent.Notify = RecoveryNotify;
	RecoveryEvent.SetTime(0.50f);
	Montage->Notifies.Add(RecoveryEvent);

	float Windup = 0.0f;
	float Active = 0.0f;
	float Recovery = 0.0f;
	AttackData->GetEffectiveTiming(Windup, Active, Recovery);

	TestEqual(TEXT("Windup should come from Active transition"), Windup, 0.30f);
	TestEqual(TEXT("Active should come from Recovery transition"), Active, 0.20f);
	TestEqual(TEXT("Recovery should extend to section end"), Recovery, 0.50f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataNotifyAnalysisReportsCounterReadinessTest,
	"KatanaCombat.Editor.AttackDataTools.Analysis.CounterReadiness",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataNotifyAnalysisReportsCounterReadinessTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);
	AttackData->bHasCounterVariant = true;
	AttackData->CounterData = NewObject<UPairedAnimationData>(AttackData);
	AttackData->CounterData->bIsLethal = false;

	AddStateNotify<UAnimNotifyState_ParryWindow>(Montage, 0.10f, 0.20f);
	AddStateNotify<UAnimNotifyState_CounterWindow>(Montage, 0.15f, 0.25f);

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);

	TestTrue(TEXT("Analysis should detect parry window"), Analysis.bHasParryWindow);
	TestTrue(TEXT("Analysis should detect counter window"), Analysis.bHasCounterWindow);
	TestTrue(TEXT("Analysis should detect counter variant data"), Analysis.bCounterVariantHasData);
	TestEqual(TEXT("Valid counter setup should have no branch readiness warnings"), Analysis.BranchReadinessWarnings.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataNotifyAnalysisReportsSemanticTagsTest,
	"KatanaCombat.Editor.AttackDataTools.Analysis.SemanticTags",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataNotifyAnalysisReportsSemanticTagsTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);
	AttackData->AttackTags.AddTag(KatanaCombatGameplayTags::AttackPropertyUnblockable());
	AttackData->RequiredContextTags.AddTag(KatanaCombatGameplayTags::ContextParryCounter());

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);

	TestTrue(TEXT("Analysis should carry unblockable attack tag"),
		Analysis.AttackTags.Contains(TEXT("Attack.Property.Unblockable")));
	TestTrue(TEXT("Analysis should carry parry counter context tag"),
		Analysis.RequiredContextTags.Contains(TEXT("Context.ParryCounter")));
	TestTrue(TEXT("Analysis should flag required context tags"), Analysis.bHasRequiredContextTags);
	TestTrue(TEXT("Analysis should flag unblockable attack tag"), Analysis.bHasUnblockableTag);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataNotifyAnalysisReportsSemanticTagsInvalidMontageTest,
	"KatanaCombat.Editor.AttackDataTools.Analysis.SemanticTagsInvalidMontage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataNotifyAnalysisReportsSemanticTagsInvalidMontageTest::RunTest(const FString& Parameters)
{
	UAttackData* AttackData = CreateValidLightAttackData(nullptr);
	AttackData->AttackTags.AddTag(KatanaCombatGameplayTags::AttackPropertyUnblockable());
	AttackData->RequiredContextTags.AddTag(KatanaCombatGameplayTags::ContextParryCounter());

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);

	TestFalse(TEXT("Analysis should remain invalid without a montage"), Analysis.bValid);
	TestTrue(TEXT("Invalid analysis should still carry unblockable attack tag"),
		Analysis.AttackTags.Contains(TEXT("Attack.Property.Unblockable")));
	TestTrue(TEXT("Invalid analysis should still carry parry counter context tag"),
		Analysis.RequiredContextTags.Contains(TEXT("Context.ParryCounter")));
	TestTrue(TEXT("Invalid analysis should still flag required context tags"), Analysis.bHasRequiredContextTags);
	TestTrue(TEXT("Invalid analysis should still flag unblockable attack tag"), Analysis.bHasUnblockableTag);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataNotifyAnalysisWarnsMissingCounterDataTest,
	"KatanaCombat.Editor.AttackDataTools.Analysis.MissingCounterDataWarning",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataNotifyAnalysisWarnsMissingCounterDataTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);
	AttackData->bHasCounterVariant = true;
	AttackData->CounterData = nullptr;

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);

	TestTrue(TEXT("Analysis should warn when counter variant lacks CounterData"),
		Analysis.BranchReadinessWarnings.Contains(TEXT("Counter variant is enabled but CounterData is null")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataNotifyAnalysisWarnsLethalCounterDataTest,
	"KatanaCombat.Editor.AttackDataTools.Analysis.LethalCounterDataWarning",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataNotifyAnalysisWarnsLethalCounterDataTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = CreateTransientMontageWithSections();
	UAttackData* AttackData = CreateValidLightAttackData(Montage);
	AttackData->bHasCounterVariant = true;
	AttackData->CounterData = NewObject<UPairedAnimationData>(AttackData);
	AttackData->CounterData->bIsLethal = true;

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);

	TestTrue(TEXT("Analysis should warn when CounterData is authored lethal"),
		Analysis.BranchReadinessWarnings.Contains(TEXT("CounterData is lethal; Chain counter steps are nonlethal by default unless runtime policy explicitly allows lethal counter data")));
	return true;
}

#endif // WITH_EDITOR
