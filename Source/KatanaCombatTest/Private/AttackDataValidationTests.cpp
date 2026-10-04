// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "Animation/AnimNotify_HoldWindowStart.h"
#include "Data/AttackData.h"
#include "Misc/DataValidation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataCycleValidationDeduplicatesBranchesTest,
	"KatanaCombat.Data.AttackData.Validation.CycleDeduplicatesBranches",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataCycleValidationDeduplicatesBranchesTest::RunTest(const FString& Parameters)
{
	UAttackData* Root = NewObject<UAttackData>(GetTransientPackage(), TEXT("CycleRoot"));
	UAttackData* Child = NewObject<UAttackData>(GetTransientPackage(), TEXT("CycleChild"));

	Root->NextComboAttack = Child;
	Root->HeavyComboAttack = Child;
	Child->NextComboAttack = Root;

	TSet<const UAttackData*> Visited;
	TArray<FText> Errors;
	const bool bFoundCycle = Root->DetectCycles(Visited, Errors);

	TestTrue(TEXT("Cycle should be detected"), bFoundCycle);
	TestEqual(TEXT("One root validation should report the cycle once"), Errors.Num(), 1);
	TestTrue(TEXT("Cycle error should identify the validated root"),
		Errors.Num() == 1 && Errors[0].ToString().StartsWith(TEXT("CycleRoot:")));
	TestEqual(TEXT("Traversal path should be empty after validation"), Visited.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataCycleValidationAllowsAcyclicFanInTest,
	"KatanaCombat.Data.AttackData.Validation.AllowsAcyclicFanIn",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataCycleValidationAllowsAcyclicFanInTest::RunTest(const FString& Parameters)
{
	UAttackData* Root = NewObject<UAttackData>(GetTransientPackage(), TEXT("FanInRoot"));
	UAttackData* SharedChild = NewObject<UAttackData>(GetTransientPackage(), TEXT("SharedChild"));

	Root->NextComboAttack = SharedChild;
	Root->HeavyComboAttack = SharedChild;

	TSet<const UAttackData*> Visited;
	TArray<FText> Errors;
	const bool bFoundCycle = Root->DetectCycles(Visited, Errors);

	TestFalse(TEXT("Shared acyclic descendants should not be treated as a cycle"), bFoundCycle);
	TestEqual(TEXT("Acyclic traversal should not report errors"), Errors.Num(), 0);
	TestEqual(TEXT("Traversal path should be empty after validation"), Visited.Num(), 0);

	return true;
}

namespace
{
/** A transient charged attack: section Attack [0, LoopStart) then Loop; nothing comes from shipped content. */
UAttackData* CreateChargedHoldAttack(
	const TCHAR* Name,
	const EAttackType AttackType,
	const float ActiveTime,
	const float HoldTime,
	const float LoopStart,
	const float ReleaseActiveTime)
{
	UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage());
	Montage->SetCompositeLength(LoopStart * 2.0f);
	FCompositeSection AttackSection;
	AttackSection.SectionName = TEXT("Attack");
	AttackSection.SetTime(0.0f);
	Montage->CompositeSections.Add(AttackSection);
	FCompositeSection LoopSection;
	LoopSection.SectionName = TEXT("Loop");
	LoopSection.SetTime(LoopStart);
	Montage->CompositeSections.Add(LoopSection);

	const auto AddPhase = [Montage](const EAttackPhase Phase, const float Time)
	{
		UAnimNotify_AttackPhaseTransition* Notify = NewObject<UAnimNotify_AttackPhaseTransition>(Montage);
		Notify->TransitionToPhase = Phase;
		FAnimNotifyEvent Event;
		Event.Notify = Notify;
		Event.SetTime(Time);
		Montage->Notifies.Add(Event);
	};
	AddPhase(EAttackPhase::Active, ActiveTime);
	UAnimNotify_HoldWindowStart* Hold = NewObject<UAnimNotify_HoldWindowStart>(Montage);
	Hold->InputType = AttackType == EAttackType::Heavy ? EInputType::HeavyAttack : EInputType::LightAttack;
	FAnimNotifyEvent HoldEvent;
	HoldEvent.Notify = Hold;
	HoldEvent.SetTime(HoldTime);
	Montage->Notifies.Add(HoldEvent);
	AddPhase(EAttackPhase::Active, ReleaseActiveTime);

	UAttackData* Attack = NewObject<UAttackData>(GetTransientPackage(), Name);
	Attack->AttackType = AttackType;
	Attack->AttackMontage = Montage;
	Attack->MontageSection = TEXT("Attack");
	Attack->ChargeLoopSection = TEXT("Loop");
	return Attack;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataChargedHoldOrderingTest,
	"KatanaCombat.Data.AttackData.Validation.ChargedHoldStartsBeforeActive",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataChargedHoldOrderingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float LoopStart = 1.0f;
	constexpr float ReleaseActive = 1.5f;
	const auto Validate = [](const UAttackData* Attack, FDataValidationContext& Context)
	{
		return Attack->IsDataValid(Context);
	};
	const auto HasErrorFor = [](const FDataValidationContext& Context, const FString& AssetName)
	{
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			const FString Message = Issue.Message.ToString();
			if (Issue.Severity == EMessageSeverity::Error
				&& Message.StartsWith(AssetName + TEXT(":"))
				&& Message.Contains(TEXT("Charged hold")))
			{
				return true;
			}
		}
		return false;
	};

	UAttackData* HoldAfterActive = CreateChargedHoldAttack(
		TEXT("ChargedHoldAfterActive"), EAttackType::Heavy, 0.30f, 0.40f, LoopStart, ReleaseActive);
	FDataValidationContext AfterContext;
	TestEqual(TEXT("Asset validation fails a charged hold that starts after Active"),
		Validate(HoldAfterActive, AfterContext), EDataValidationResult::Invalid);
	TestTrue(TEXT("The charged-hold error names the validated asset"),
		HasErrorFor(AfterContext, TEXT("ChargedHoldAfterActive")));

	UAttackData* HoldAtActive = CreateChargedHoldAttack(
		TEXT("ChargedHoldAtActive"), EAttackType::Heavy, 0.30f, 0.30f, LoopStart, ReleaseActive);
	FDataValidationContext TieContext;
	TestEqual(TEXT("A charged hold sharing Active's time fails; track order would decide it"),
		Validate(HoldAtActive, TieContext), EDataValidationResult::Invalid);
	TestTrue(TEXT("The tied charged hold reports the ordering error"),
		HasErrorFor(TieContext, TEXT("ChargedHoldAtActive")));

	UAttackData* HoldBeforeActive = CreateChargedHoldAttack(
		TEXT("ChargedHoldBeforeActive"), EAttackType::Heavy, 0.40f, 0.30f, LoopStart, ReleaseActive);
	FDataValidationContext BeforeContext;
	TestEqual(TEXT("A charged hold before Active passes, ignoring the next section's Active"),
		Validate(HoldBeforeActive, BeforeContext), EDataValidationResult::Valid);
	TestEqual(TEXT("The early charged hold reports no error"), BeforeContext.GetNumErrors(), 0u);

	UAttackData* LightHoldAfterActive = CreateChargedHoldAttack(
		TEXT("LightHoldAfterActive"), EAttackType::Light, 0.30f, 0.40f, LoopStart, ReleaseActive);
	FDataValidationContext LightContext;
	TestEqual(TEXT("A light hold eases in place and is not held to the charged ordering"),
		Validate(LightHoldAfterActive, LightContext), EDataValidationResult::Valid);
	TestFalse(TEXT("A light hold reports no charged-hold error"),
		HasErrorFor(LightContext, TEXT("LightHoldAfterActive")));
	return true;
}

#endif
