// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "Animation/AnimNotify_HoldWindowStart.h"
#include "AssetRegistry/AssetRegistryModule.h"
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
struct FHoldNotifyPlacement
{
	EInputType InputType = EInputType::HeavyAttack;
	float Time = 0.0f;
};

/** A transient charged attack: section Attack [0, LoopStart) then Loop; nothing comes from shipped content. */
UAttackData* CreateChargedHoldAttack(
	const TCHAR* Name,
	const EAttackType AttackType,
	const float ActiveTime,
	const TArray<FHoldNotifyPlacement>& Holds,
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
	for (const FHoldNotifyPlacement& Placement : Holds)
	{
		UAnimNotify_HoldWindowStart* Hold = NewObject<UAnimNotify_HoldWindowStart>(Montage);
		Hold->InputType = Placement.InputType;
		FAnimNotifyEvent HoldEvent;
		HoldEvent.Notify = Hold;
		HoldEvent.SetTime(Placement.Time);
		Montage->Notifies.Add(HoldEvent);
	}
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
		TEXT("ChargedHoldAfterActive"), EAttackType::Heavy, 0.30f, {{EInputType::HeavyAttack, 0.40f}}, LoopStart,
		ReleaseActive);
	FDataValidationContext AfterContext;
	TestEqual(TEXT("Asset validation fails a charged hold that starts after Active"),
		Validate(HoldAfterActive, AfterContext), EDataValidationResult::Invalid);
	TestTrue(TEXT("The charged-hold error names the validated asset"),
		HasErrorFor(AfterContext, TEXT("ChargedHoldAfterActive")));

	UAttackData* HoldAtActive = CreateChargedHoldAttack(
		TEXT("ChargedHoldAtActive"), EAttackType::Heavy, 0.30f, {{EInputType::HeavyAttack, 0.30f}}, LoopStart,
		ReleaseActive);
	FDataValidationContext TieContext;
	TestEqual(TEXT("A charged hold sharing Active's time fails; track order would decide it"),
		Validate(HoldAtActive, TieContext), EDataValidationResult::Invalid);
	TestTrue(TEXT("The tied charged hold reports the ordering error"),
		HasErrorFor(TieContext, TEXT("ChargedHoldAtActive")));

	UAttackData* HoldBeforeActive = CreateChargedHoldAttack(
		TEXT("ChargedHoldBeforeActive"), EAttackType::Heavy, 0.40f, {{EInputType::HeavyAttack, 0.30f}}, LoopStart,
		ReleaseActive);
	FDataValidationContext BeforeContext;
	TestEqual(TEXT("A charged hold before Active passes, ignoring the next section's Active"),
		Validate(HoldBeforeActive, BeforeContext), EDataValidationResult::Valid);
	TestEqual(TEXT("The early charged hold reports no error"), BeforeContext.GetNumErrors(), 0u);

	UAttackData* LightHoldAfterActive = CreateChargedHoldAttack(
		TEXT("LightHoldAfterActive"), EAttackType::Light, 0.30f, {{EInputType::LightAttack, 0.40f}}, LoopStart,
		ReleaseActive);
	FDataValidationContext LightContext;
	TestEqual(TEXT("A light hold eases in place and is not held to the charged ordering"),
		Validate(LightHoldAfterActive, LightContext), EDataValidationResult::Valid);
	TestFalse(TEXT("A light hold reports no charged-hold error"),
		HasErrorFor(LightContext, TEXT("LightHoldAfterActive")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataChargedHoldEveryInputTest,
	"KatanaCombat.Data.AttackData.Validation.ChargedHoldChecksEveryHoldInput",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAttackDataChargedHoldEveryInputTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// OnHoldWindowStartWithContext starts a heavy's charge from any hold notify whose button is held at that
	// notify; it never compares the notify's input with the Heavy. So a later notify charges after Active
	// whenever an earlier notify's button was not held, whatever input either notify names.
	constexpr float LoopStart = 1.0f;
	constexpr float ReleaseActive = 1.5f;
	constexpr float Active = 0.30f;
	const auto CountChargedHoldErrors = [](const FDataValidationContext& Context, const FString& AssetName,
		const FString& Detail)
	{
		int32 Count = 0;
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			const FString Message = Issue.Message.ToString();
			if (Issue.Severity == EMessageSeverity::Error
				&& Message.StartsWith(AssetName + TEXT(":"))
				&& Message.Contains(TEXT("Charged hold"))
				&& Message.Contains(Detail))
			{
				++Count;
			}
		}
		return Count;
	};

	UAttackData* LateHeavyBehindEarlyLight = CreateChargedHoldAttack(
		TEXT("LateHeavyBehindEarlyLight"), EAttackType::Heavy, Active,
		{{EInputType::LightAttack, 0.20f}, {EInputType::HeavyAttack, 0.40f}}, LoopStart, ReleaseActive);
	FDataValidationContext LateHeavyContext;
	TestEqual(TEXT("An early Light hold does not hide a Heavy hold after Active"),
		LateHeavyBehindEarlyLight->IsDataValid(LateHeavyContext), EDataValidationResult::Invalid);
	TestEqual(TEXT("The error names the late Heavy hold"),
		CountChargedHoldErrors(LateHeavyContext, TEXT("LateHeavyBehindEarlyLight"), TEXT("HeavyAttack")), 1);

	UAttackData* LateLightBehindEarlyHeavy = CreateChargedHoldAttack(
		TEXT("LateLightBehindEarlyHeavy"), EAttackType::Heavy, Active,
		{{EInputType::HeavyAttack, 0.20f}, {EInputType::LightAttack, 0.40f}}, LoopStart, ReleaseActive);
	FDataValidationContext LateLightContext;
	TestEqual(TEXT("A Light hold after Active fails too; a held Light starts the heavy's charge"),
		LateLightBehindEarlyHeavy->IsDataValid(LateLightContext), EDataValidationResult::Invalid);
	TestEqual(TEXT("The error names the late Light hold"),
		CountChargedHoldErrors(LateLightContext, TEXT("LateLightBehindEarlyHeavy"), TEXT("LightAttack")), 1);

	UAttackData* TwoLateHolds = CreateChargedHoldAttack(
		TEXT("TwoLateHolds"), EAttackType::Heavy, Active,
		{{EInputType::HeavyAttack, 0.20f}, {EInputType::LightAttack, 0.40f}, {EInputType::Evade, 0.45f}},
		LoopStart, ReleaseActive);
	FDataValidationContext TwoLateContext;
	TestEqual(TEXT("Two late holds behind an early one fail"),
		TwoLateHolds->IsDataValid(TwoLateContext), EDataValidationResult::Invalid);
	TestEqual(TEXT("Every late hold is reported, one error each"),
		CountChargedHoldErrors(TwoLateContext, TEXT("TwoLateHolds"), TEXT("")), 2);

	UAttackData* EveryHoldBeforeActive = CreateChargedHoldAttack(
		TEXT("EveryHoldBeforeActive"), EAttackType::Heavy, Active,
		{{EInputType::LightAttack, 0.20f}, {EInputType::HeavyAttack, 0.25f}}, LoopStart, ReleaseActive);
	FDataValidationContext EarlyContext;
	TestEqual(TEXT("Holds of any input all before Active pass"),
		EveryHoldBeforeActive->IsDataValid(EarlyContext), EDataValidationResult::Valid);
	TestEqual(TEXT("Holds all before Active report no error"), EarlyContext.GetNumErrors(), 0u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackDataShippedChargedHoldOrderingTest,
	"KatanaCombat.Data.AttackData.Validation.ShippedChargedHoldsStartBeforeActive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackDataShippedChargedHoldOrderingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// Invariant over shipped content only: every hold notify that can start a charge precedes Active.
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
		TArray<FText> Errors;
		Attack->ValidateChargedHoldOrdering(Errors);
		for (const FText& Error : Errors)
		{
			AddError(FString::Printf(TEXT("%s: %s"), *Asset.GetObjectPathString(), *Error.ToString()));
		}
	}
	AddInfo(FString::Printf(TEXT("Checked hold ordering on %d shipped charged heavy attacks"), ChargedHeavies));
	return true;
}

#endif
