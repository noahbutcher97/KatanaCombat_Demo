// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "AttackDataNotifyGenerationService.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "Animation/AnimNotify_HoldWindowStart.h"
#include "Animation/AnimNotifyState_ComboWindow.h"
#include "Animation/AnimNotifyState_CounterWindow.h"
#include "Animation/AnimNotifyState_HoldWindow.h"
#include "Animation/AnimNotifyState_ParryWindow.h"
#include "Data/PairedAnimationData.h"
#include "Data/AttackData.h"
#include "Commandlets/KatanaAssetMigrationRunner.h"
#include "Commandlets/Operations/AttackDataNotifyMigrationOperation.h"
#include "Commandlets/Operations/AttackDataTimingMigrationOperation.h"
#include "Commandlets/Operations/ContentReadinessAuditOperation.h"
#include "Commandlets/Operations/CounterChainProofMigrationOperation.h"
#include "Commandlets/KatanaAssetMigrationTypes.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/CombatGameplayTags.h"

#if PLATFORM_WINDOWS
#include <direct.h>
#endif

namespace KatanaAssetMigrationTest
{
	UAnimMontage* CreateMontage()
	{
		UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage());
		Montage->SetCompositeLength(2.0f);

		FCompositeSection TargetSection;
		TargetSection.SectionName = TEXT("Target");
		TargetSection.SetTime(0.0f);
		Montage->CompositeSections.Add(TargetSection);

		FCompositeSection OtherSection;
		OtherSection.SectionName = TEXT("Other");
		OtherSection.SetTime(1.0f);
		Montage->CompositeSections.Add(OtherSection);
		return Montage;
	}

	UAttackData* CreateAttackData(UAnimMontage* Montage)
	{
		UAttackData* AttackData = NewObject<UAttackData>(GetTransientPackage());
		AttackData->AttackMontage = Montage;
		AttackData->MontageSection = TEXT("Target");
		AttackData->AttackType = EAttackType::Light;
		AttackData->bCanHold = true;
		AttackData->ManualTiming.WindupDuration = 0.30f;
		AttackData->ManualTiming.ActiveDuration = 0.20f;
		AttackData->ManualTiming.RecoveryDuration = 0.50f;
		AttackData->ManualTiming.HoldWindowStart = 0.45f;
		return AttackData;
	}

	void AddSemanticTags(UAttackData* AttackData)
	{
		AttackData->AttackTags.AddTag(KatanaCombatGameplayTags::AttackPropertyUnblockable());
		AttackData->RequiredContextTags.AddTag(KatanaCombatGameplayTags::ContextParryCounter());
	}

	void ExpectSemanticRowFields(FAutomationTestBase& Test, const FKatanaAssetMigrationRow& Row, const TCHAR* Label)
	{
		Test.TestTrue(FString::Printf(TEXT("%s row should include unblockable attack tag"), Label),
			Row.AttackTags.Contains(TEXT("Attack.Property.Unblockable")));
		Test.TestTrue(FString::Printf(TEXT("%s row should include parry counter context tag"), Label),
			Row.RequiredContextTags.Contains(TEXT("Context.ParryCounter")));
		Test.TestTrue(FString::Printf(TEXT("%s row should flag required context tags"), Label),
			Row.bHasRequiredContextTags);
		Test.TestTrue(FString::Printf(TEXT("%s row should flag unblockable tag"), Label),
			Row.bHasUnblockableTag);
	}

	template <typename NotifyStateType>
	void AddStateNotify(UAnimMontage* Montage, float Time, float Duration)
	{
		NotifyStateType* NotifyState = NewObject<NotifyStateType>(Montage);
		FAnimNotifyEvent Event;
		Event.NotifyStateClass = NotifyState;
		Event.SetTime(Time);
		Event.SetDuration(Duration);
		Montage->Notifies.Add(Event);
	}

	template <typename NotifyType>
	NotifyType* AddPointNotify(UAnimMontage* Montage, float Time)
	{
		NotifyType* Notify = NewObject<NotifyType>(Montage);
		FAnimNotifyEvent Event;
		Event.Notify = Notify;
		Event.SetTime(Time);
		Montage->Notifies.Add(Event);
		return Notify;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAssetMigrationModeDefaultsToAuditTest,
	"KatanaCombat.Editor.AssetMigration.Options.DefaultModeAudit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAssetMigrationModeDefaultsToAuditTest::RunTest(const FString& Parameters)
{
	const FKatanaAssetMigrationOptions Options;
	TestEqual(TEXT("Default mode should be Audit"), static_cast<int32>(Options.Mode), static_cast<int32>(EKatanaAssetMigrationMode::Audit));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAssetMigrationModeParsingTest,
	"KatanaCombat.Editor.AssetMigration.Options.ModeParsing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAssetMigrationModeParsingTest::RunTest(const FString& Parameters)
{
	EKatanaAssetMigrationMode Mode = EKatanaAssetMigrationMode::Audit;

	TestTrue(TEXT("Plan should parse"), TryParseKatanaAssetMigrationMode(TEXT("Plan"), Mode));
	TestEqual(TEXT("Plan enum"), static_cast<int32>(Mode), static_cast<int32>(EKatanaAssetMigrationMode::Plan));

	TestTrue(TEXT("ApplyAndSave should parse"), TryParseKatanaAssetMigrationMode(TEXT("ApplyAndSave"), Mode));
	TestEqual(TEXT("ApplyAndSave enum"), static_cast<int32>(Mode), static_cast<int32>(EKatanaAssetMigrationMode::ApplyAndSave));

	TestFalse(TEXT("Unknown mode should fail"), TryParseKatanaAssetMigrationMode(TEXT("DestroyEverything"), Mode));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAttackDataNotifyPlanReadOnlyTest,
	"KatanaCombat.Editor.AssetMigration.AttackDataNotify.PlanReadOnly",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAttackDataNotifyPlanReadOnlyTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);
	KatanaAssetMigrationTest::AddStateNotify<UAnimNotifyState_ComboWindow>(Montage, 0.50f, 0.20f);

	const int32 NotifyCountBefore = Montage->Notifies.Num();
	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);
	const FAttackDataNotifyPlan Plan = FAttackDataNotifyGenerationService::BuildAttackDataNotifyPlan(Analysis);

	TestTrue(TEXT("Plan should detect required changes"), Plan.HasChanges());
	TestEqual(TEXT("Plan mode should not mutate notify count"), Montage->Notifies.Num(), NotifyCountBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAttackDataNotifyApplyTargetSectionOnlyTest,
	"KatanaCombat.Editor.AssetMigration.AttackDataNotify.ApplyTargetSectionOnly",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAttackDataNotifyApplyTargetSectionOnlyTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);
	KatanaAssetMigrationTest::AddStateNotify<UAnimNotifyState_ComboWindow>(Montage, 0.50f, 0.20f);
	KatanaAssetMigrationTest::AddStateNotify<UAnimNotifyState_ComboWindow>(Montage, 1.20f, 0.20f);

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);
	const FAttackDataNotifyPlan Plan = FAttackDataNotifyGenerationService::BuildAttackDataNotifyPlan(Analysis);

	TestTrue(TEXT("Apply should succeed"), FAttackDataNotifyGenerationService::ApplyAttackDataNotifyPlan(AttackData, Plan));

	int32 ComboStateCount = 0;
	int32 PhaseTransitionCount = 0;
	int32 HoldStartCount = 0;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (Event.NotifyStateClass && Event.NotifyStateClass->IsA(UAnimNotifyState_ComboWindow::StaticClass()))
		{
			++ComboStateCount;
			TestTrue(TEXT("Remaining combo state should be outside target section"), Event.GetTriggerTime() >= 1.0f);
		}
		if (Event.Notify && Event.Notify->IsA(UAnimNotify_AttackPhaseTransition::StaticClass()))
		{
			++PhaseTransitionCount;
		}
		if (Event.Notify && Event.Notify->IsA(UAnimNotify_HoldWindowStart::StaticClass()))
		{
			++HoldStartCount;
		}
	}

	TestEqual(TEXT("Outside-section combo state preserved"), ComboStateCount, 1);
	TestEqual(TEXT("Active and Recovery transitions added"), PhaseTransitionCount, 2);
	TestEqual(TEXT("Hold start added"), HoldStartCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAttackDataNotifyOperationReseedsStaleCanonicalTest,
	"KatanaCombat.Editor.AssetMigration.AttackDataNotify.OperationReseedsStaleCanonical",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAttackDataNotifyOperationReseedsStaleCanonicalTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);

	UAnimNotify_AttackPhaseTransition* StaleActive =
		KatanaAssetMigrationTest::AddPointNotify<UAnimNotify_AttackPhaseTransition>(Montage, 0.10f);
	StaleActive->TransitionToPhase = EAttackPhase::Active;
	UAnimNotify_AttackPhaseTransition* StaleRecovery =
		KatanaAssetMigrationTest::AddPointNotify<UAnimNotify_AttackPhaseTransition>(Montage, 0.80f);
	StaleRecovery->TransitionToPhase = EAttackPhase::Recovery;
	KatanaAssetMigrationTest::AddPointNotify<UAnimNotify_HoldWindowStart>(Montage, 0.20f);

	FKatanaAssetMigrationRow Row;
	FAttackDataNotifyMigrationOperation Operation;
	TestTrue(TEXT("Apply should succeed and reseed stale canonical notifies"),
		Operation.Run(AttackData, EKatanaAssetMigrationMode::Apply, Row));
	TestEqual(TEXT("Apply should report Changed"), LexToString(Row.Status), FString(TEXT("Changed")));

	int32 PhaseTransitionCount = 0;
	int32 HoldStartCount = 0;
	bool bActiveAtExpectedTime = false;
	bool bRecoveryAtExpectedTime = false;
	bool bHoldStartAtExpectedTime = false;

	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (const UAnimNotify_AttackPhaseTransition* Transition =
			Cast<UAnimNotify_AttackPhaseTransition>(Event.Notify))
		{
			++PhaseTransitionCount;
			bActiveAtExpectedTime |=
				Transition->TransitionToPhase == EAttackPhase::Active &&
				FMath::IsNearlyEqual(Event.GetTriggerTime(), 0.30f);
			bRecoveryAtExpectedTime |=
				Transition->TransitionToPhase == EAttackPhase::Recovery &&
				FMath::IsNearlyEqual(Event.GetTriggerTime(), 0.50f);
		}

		if (Event.Notify && Event.Notify->IsA(UAnimNotify_HoldWindowStart::StaticClass()))
		{
			++HoldStartCount;
			bHoldStartAtExpectedTime |= FMath::IsNearlyEqual(Event.GetTriggerTime(), 0.45f);
		}
	}

	TestEqual(TEXT("Stale phase transitions should be replaced, not duplicated"), PhaseTransitionCount, 2);
	TestEqual(TEXT("Stale hold start should be replaced, not duplicated"), HoldStartCount, 1);
	TestTrue(TEXT("Active transition should be reseeded from current timing"), bActiveAtExpectedTime);
	TestTrue(TEXT("Recovery transition should be reseeded from current timing"), bRecoveryAtExpectedTime);
	TestTrue(TEXT("Hold start should be reseeded from current timing"), bHoldStartAtExpectedTime);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAttackDataNotifyInvalidManualTimingDoesNotPlanTest,
	"KatanaCombat.Editor.AssetMigration.AttackDataNotify.InvalidManualTimingDoesNotPlan",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAttackDataNotifyInvalidManualTimingDoesNotPlanTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);
	AttackData->ManualTiming.WindupDuration = 0.0f;

	const int32 NotifyCountBefore = Montage->Notifies.Num();
	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);
	const FAttackDataNotifyPlan Plan = FAttackDataNotifyGenerationService::BuildAttackDataNotifyPlan(Analysis);

	TestFalse(TEXT("Invalid manual timing should fail analysis"), Analysis.bValid);
	TestFalse(TEXT("Invalid manual timing should not produce a valid plan"), Plan.bValid);
	TestEqual(TEXT("Invalid timing analysis should not mutate notify count"), Montage->Notifies.Num(), NotifyCountBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAttackDataNotifyOperationPlanDoesNotMutateTest,
	"KatanaCombat.Editor.AssetMigration.AttackDataNotify.OperationPlanDoesNotMutate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAttackDataNotifyOperationPlanDoesNotMutateTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);
	KatanaAssetMigrationTest::AddStateNotify<UAnimNotifyState_HoldWindow>(Montage, 0.45f, 0.10f);

	FKatanaAssetMigrationRow Row;
	FAttackDataNotifyMigrationOperation Operation;
	const int32 NotifyCountBefore = Montage->Notifies.Num();

	TestTrue(TEXT("Plan operation should succeed"), Operation.Run(AttackData, EKatanaAssetMigrationMode::Plan, Row));
	TestEqual(TEXT("Plan operation should not mutate notify count"), Montage->Notifies.Num(), NotifyCountBefore);
	TestEqual(TEXT("Plan status should be WouldChange"), LexToString(Row.Status), FString(TEXT("WouldChange")));
	TestTrue(TEXT("Plan should include removals"), Row.PlannedRemovals.Num() > 0);
	TestTrue(TEXT("Plan should include additions"), Row.PlannedAdditions.Num() > 0);
	TestTrue(TEXT("Plan should report changed packages"), Row.ChangedPackages.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAssetMigrationApplyAndSaveRequiresFlagTest,
	"KatanaCombat.Editor.AssetMigration.Runner.ApplyAndSaveRequiresSaveFlag",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAssetMigrationApplyAndSaveRequiresFlagTest::RunTest(const FString& Parameters)
{
	FKatanaAssetMigrationOptions Options;
	Options.Operation = TEXT("AttackDataNotifyMigration");
	Options.Mode = EKatanaAssetMigrationMode::ApplyAndSave;
	Options.TargetsFile = TEXT("Config/AssetMigrations/AttackDataNotifyTargets.txt");

	TArray<FString> Errors;
	TestFalse(TEXT("ApplyAndSave without AllowPackageSave should fail"),
		FKatanaAssetMigrationRunner::ValidateOptions(Options, Errors));
	TestTrue(TEXT("Validation error should be present"), Errors.Contains(TEXT("ApplyAndSave requires -AllowPackageSave")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAssetMigrationAllowTimingMutationRejectedTest,
	"KatanaCombat.Editor.AssetMigration.Runner.AllowTimingMutationRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAssetMigrationAllowTimingMutationRejectedTest::RunTest(const FString& Parameters)
{
	FKatanaAssetMigrationOptions Options;
	Options.Operation = TEXT("AttackDataNotifyMigration");
	Options.Mode = EKatanaAssetMigrationMode::Audit;
	Options.bAllowGlobalScan = true;
	Options.bAllowTimingMutation = true;

	TArray<FString> Errors;
	TestFalse(TEXT("AttackData notify migration should reject unsupported timing mutation flag"),
		FKatanaAssetMigrationRunner::ValidateOptions(Options, Errors));

	bool bFoundExpectedError = false;
	for (const FString& Error : Errors)
	{
		bFoundExpectedError |= Error.Contains(TEXT("AllowTimingMutation")) &&
			Error.Contains(TEXT("not supported"));
	}
	TestTrue(TEXT("Validation should explain that AllowTimingMutation is unsupported"), bFoundExpectedError);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAssetMigrationReportJsonFieldsTest,
	"KatanaCombat.Editor.AssetMigration.Runner.ReportJsonFields",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAssetMigrationReportJsonFieldsTest::RunTest(const FString& Parameters)
{
	FKatanaAssetMigrationReport Report;
	Report.Operation = TEXT("AttackDataNotifyMigration");
	Report.Mode = EKatanaAssetMigrationMode::Plan;
	FKatanaAssetMigrationRow Row;
	Row.InputTarget = TEXT("/Game/Test/DA_Test.DA_Test");
	Row.PackageName = TEXT("/Game/Test/DA_Test");
	Row.ObjectPath = TEXT("/Game/Test/DA_Test.DA_Test");
	Row.AssetClass = TEXT("/Script/KatanaCombat.AttackData");
	Row.AttackData = Row.InputTarget;
	Row.Montage = TEXT("/Game/Test/AM_Test.AM_Test");
	Row.Section = TEXT("Target");
	Row.Status = EKatanaAssetMigrationStatus::WouldChange;
	Row.bPackageFileExists = true;
	Row.bLoaded = true;
	Row.bAttackDataSectionValid = true;
	Row.AttackTags.Add(TEXT("Attack.Property.Unblockable"));
	Row.RequiredContextTags.Add(TEXT("Context.ParryCounter"));
	Row.bHasRequiredContextTags = true;
	Row.bHasUnblockableTag = true;
	Row.PlannedAdditions.Add(TEXT("AnimNotify_AttackPhaseTransition(Active)"));
	Report.Rows.Add(Row);
	FKatanaAssetMigrationRunner::Summarize(Report);

	const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("KatanaAssetMigrationReportTest.json"));
	TArray<FString> Errors;
	TestTrue(TEXT("Report should write"), FKatanaAssetMigrationRunner::WriteReport(Report, ReportPath, Errors));

	FString JsonText;
	TestTrue(TEXT("Report should be readable"), FFileHelper::LoadFileToString(JsonText, *ReportPath));
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	TestTrue(TEXT("Report should parse"), FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid());
	TestEqual(TEXT("schema_version"), JsonObject->GetIntegerField(TEXT("schema_version")), 1);
	TestEqual(TEXT("operation"), JsonObject->GetStringField(TEXT("operation")), FString(TEXT("AttackDataNotifyMigration")));
	TestTrue(TEXT("rows present"), JsonObject->HasTypedField<EJson::Array>(TEXT("rows")));
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	TestTrue(TEXT("rows should read"), JsonObject->TryGetArrayField(TEXT("rows"), Rows) && Rows && Rows->Num() == 1);
	if (Rows && Rows->Num() == 1)
	{
		const TSharedPtr<FJsonObject> RowObject = (*Rows)[0]->AsObject();
		TestTrue(TEXT("row should include package_name"),
			RowObject->HasField(TEXT("package_name")));
		TestTrue(TEXT("row should include object_path"),
			RowObject->HasField(TEXT("object_path")));
		TestTrue(TEXT("row should include asset_class"),
			RowObject->HasField(TEXT("asset_class")));
		TestTrue(TEXT("row should include package_file_exists"),
			RowObject->HasField(TEXT("package_file_exists")));
		TestTrue(TEXT("row should include loaded"),
			RowObject->HasField(TEXT("loaded")));
		TestTrue(TEXT("row should include map_loaded"),
			RowObject->HasField(TEXT("map_loaded")));
		TestTrue(TEXT("row should include attack_data_section_valid"),
			RowObject->HasField(TEXT("attack_data_section_valid")));
		TestTrue(TEXT("row should include paired_animation_valid"),
			RowObject->HasField(TEXT("paired_animation_valid")));
		TestTrue(TEXT("row should include paired_attacker_section_valid"),
			RowObject->HasField(TEXT("paired_attacker_section_valid")));
		TestTrue(TEXT("row should include paired_victim_section_valid"),
			RowObject->HasField(TEXT("paired_victim_section_valid")));
		TestTrue(TEXT("row should include stale canonical notify field"),
			RowObject->HasTypedField<EJson::Array>(TEXT("stale_canonical_notifies_found")));
		TestTrue(TEXT("row should include section length"),
			RowObject->HasField(TEXT("section_length")));
		TestTrue(TEXT("row should include windup duration"),
			RowObject->HasField(TEXT("windup_duration")));
		TestTrue(TEXT("row should include active duration"),
			RowObject->HasField(TEXT("active_duration")));
		TestTrue(TEXT("row should include recovery duration"),
			RowObject->HasField(TEXT("recovery_duration")));
		TestTrue(TEXT("row should include timing total"),
			RowObject->HasField(TEXT("timing_total")));
		TestTrue(TEXT("row should include proposed recovery duration"),
			RowObject->HasField(TEXT("proposed_recovery_duration")));
		TestTrue(TEXT("row should include proposed timing total"),
			RowObject->HasField(TEXT("proposed_timing_total")));
		TestTrue(TEXT("row should include branch readiness warnings"),
			RowObject->HasTypedField<EJson::Array>(TEXT("branch_readiness_warnings")));
		TestTrue(TEXT("row should include attack_tags"),
			RowObject->HasTypedField<EJson::Array>(TEXT("attack_tags")));
		TestTrue(TEXT("row should include required_context_tags"),
			RowObject->HasTypedField<EJson::Array>(TEXT("required_context_tags")));
		TestTrue(TEXT("row should include has_required_context_tags"),
			RowObject->HasField(TEXT("has_required_context_tags")));
		TestTrue(TEXT("row should include has_unblockable_tag"),
			RowObject->HasField(TEXT("has_unblockable_tag")));

		const TArray<TSharedPtr<FJsonValue>>* AttackTags = nullptr;
		TestTrue(TEXT("row attack_tags should read"),
			RowObject->TryGetArrayField(TEXT("attack_tags"), AttackTags) && AttackTags && AttackTags->Num() == 1);
		if (AttackTags && AttackTags->Num() == 1)
		{
			TestEqual(TEXT("row should serialize unblockable attack tag"),
				(*AttackTags)[0]->AsString(),
				FString(TEXT("Attack.Property.Unblockable")));
		}

		const TArray<TSharedPtr<FJsonValue>>* RequiredContextTags = nullptr;
		TestTrue(TEXT("row required_context_tags should read"),
			RowObject->TryGetArrayField(TEXT("required_context_tags"), RequiredContextTags) &&
			RequiredContextTags &&
			RequiredContextTags->Num() == 1);
		if (RequiredContextTags && RequiredContextTags->Num() == 1)
		{
			TestEqual(TEXT("row should serialize parry counter context tag"),
				(*RequiredContextTags)[0]->AsString(),
				FString(TEXT("Context.ParryCounter")));
		}

		TestTrue(TEXT("row should serialize has_required_context_tags true"),
			RowObject->GetBoolField(TEXT("has_required_context_tags")));
		TestTrue(TEXT("row should serialize has_unblockable_tag true"),
			RowObject->GetBoolField(TEXT("has_unblockable_tag")));
		TestTrue(TEXT("row should include has_parry_window"),
			RowObject->HasField(TEXT("has_parry_window")));
		TestTrue(TEXT("row should include has_counter_window"),
			RowObject->HasField(TEXT("has_counter_window")));
		TestTrue(TEXT("row should include counter_variant_has_data"),
			RowObject->HasField(TEXT("counter_variant_has_data")));
		TestTrue(TEXT("row should include finisher_has_data"),
			RowObject->HasField(TEXT("finisher_has_data")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAssetMigrationRowsSemanticTagsTest,
	"KatanaCombat.Editor.AssetMigration.Rows.SemanticTags",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAssetMigrationRowsSemanticTagsTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);
	KatanaAssetMigrationTest::AddSemanticTags(AttackData);

	FKatanaAssetMigrationRow NotifyRow;
	const FAttackDataNotifyMigrationOperation NotifyOperation;
	TestTrue(TEXT("Notify migration audit should succeed"),
		NotifyOperation.Run(AttackData, EKatanaAssetMigrationMode::Audit, NotifyRow));
	KatanaAssetMigrationTest::ExpectSemanticRowFields(*this, NotifyRow, TEXT("NotifyMigration"));

	FKatanaAssetMigrationRow TimingRow;
	const FAttackDataTimingMigrationOperation TimingOperation;
	TestTrue(TEXT("Timing migration plan should succeed"),
		TimingOperation.Run(AttackData, EKatanaAssetMigrationMode::Plan, TimingRow));
	KatanaAssetMigrationTest::ExpectSemanticRowFields(*this, TimingRow, TEXT("TimingMigration"));

	FKatanaAssetMigrationRow ReadinessRow;
	const FContentReadinessAuditOperation ReadinessOperation;
	TestTrue(TEXT("Content readiness audit should succeed"),
		ReadinessOperation.RunLoadedObject(TEXT("/Game/Test/DA_Attack.DA_Attack"), AttackData, false, ReadinessRow));
	KatanaAssetMigrationTest::ExpectSemanticRowFields(*this, ReadinessRow, TEXT("ContentReadiness"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAttackDataNotifyOperationReportsTimingFailureDetailsTest,
	"KatanaCombat.Editor.AssetMigration.AttackDataNotify.OperationReportsTimingFailureDetails",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAttackDataNotifyOperationReportsTimingFailureDetailsTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);
	AttackData->ManualTiming.WindupDuration = 0.80f;
	AttackData->ManualTiming.ActiveDuration = 0.40f;
	AttackData->ManualTiming.RecoveryDuration = 0.10f;

	FKatanaAssetMigrationRow Row;
	const FAttackDataNotifyMigrationOperation Operation;
	TestFalse(TEXT("Operation should fail invalid manual timing"),
		Operation.Run(AttackData, EKatanaAssetMigrationMode::Audit, Row));
	TestEqual(TEXT("Status should be Failed"), LexToString(Row.Status), FString(TEXT("Failed")));
	TestEqual(TEXT("Section length should be reported"), Row.SectionLength, 1.0f);
	TestEqual(TEXT("Windup duration should be reported"), Row.WindupDuration, 0.80f);
	TestEqual(TEXT("Active duration should be reported"), Row.ActiveDuration, 0.40f);
	TestEqual(TEXT("Recovery duration should be reported"), Row.RecoveryDuration, 0.10f);
	TestEqual(TEXT("Timing total should be reported"), Row.TimingTotal, 1.30f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAttackDataTimingOperationPlanClampsRecoveryTest,
	"KatanaCombat.Editor.AssetMigration.AttackDataTiming.OperationPlanClampsRecovery",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAttackDataTimingOperationPlanClampsRecoveryTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);
	AttackData->ManualTiming.RecoveryDuration = 0.80f;

	FKatanaAssetMigrationRow Row;
	const FAttackDataTimingMigrationOperation Operation;
	TestTrue(TEXT("Plan should succeed"), Operation.Run(AttackData, EKatanaAssetMigrationMode::Plan, Row));
	TestEqual(TEXT("Plan status should be WouldChange"), LexToString(Row.Status), FString(TEXT("WouldChange")));
	TestEqual(TEXT("Current recovery should be reported"), Row.RecoveryDuration, 0.80f);
	TestEqual(TEXT("Proposed recovery should clamp to section budget"), Row.ProposedRecoveryDuration, 0.50f);
	TestEqual(TEXT("Manual timing should not mutate in Plan mode"), AttackData->ManualTiming.RecoveryDuration, 0.80f);
	TestTrue(TEXT("Plan should report changed package"), Row.ChangedPackages.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAttackDataTimingOperationApplyClampsRecoveryTest,
	"KatanaCombat.Editor.AssetMigration.AttackDataTiming.OperationApplyClampsRecovery",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAttackDataTimingOperationApplyClampsRecoveryTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);
	AttackData->ManualTiming.RecoveryDuration = 0.80f;

	FKatanaAssetMigrationRow Row;
	const FAttackDataTimingMigrationOperation Operation;
	TestTrue(TEXT("Apply should succeed"), Operation.Run(AttackData, EKatanaAssetMigrationMode::Apply, Row));
	TestEqual(TEXT("Apply status should be Changed"), LexToString(Row.Status), FString(TEXT("Changed")));
	TestEqual(TEXT("Manual timing recovery should be clamped"), AttackData->ManualTiming.RecoveryDuration, 0.50f);
	TestEqual(TEXT("Proposed timing total should match section length"), Row.ProposedTimingTotal, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAttackDataTimingOperationFailsWhenWindupActiveExceedSectionTest,
	"KatanaCombat.Editor.AssetMigration.AttackDataTiming.OperationFailsWhenWindupActiveExceedSection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAttackDataTimingOperationFailsWhenWindupActiveExceedSectionTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);
	AttackData->ManualTiming.WindupDuration = 0.80f;
	AttackData->ManualTiming.ActiveDuration = 0.30f;
	AttackData->ManualTiming.RecoveryDuration = 0.10f;

	FKatanaAssetMigrationRow Row;
	const FAttackDataTimingMigrationOperation Operation;
	TestFalse(TEXT("Operation should fail when windup plus active exceed section"),
		Operation.Run(AttackData, EKatanaAssetMigrationMode::Apply, Row));
	TestEqual(TEXT("Status should be Failed"), LexToString(Row.Status), FString(TEXT("Failed")));
	TestTrue(TEXT("Failure should explain recovery clamp cannot preserve phases"),
		Row.Errors.Num() > 0 && Row.Errors[0].Contains(TEXT("recovery clamp cannot preserve attack phases")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAssetMigrationTimingOperationAcceptedByRunnerTest,
	"KatanaCombat.Editor.AssetMigration.Runner.TimingOperationAccepted",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAssetMigrationTimingOperationAcceptedByRunnerTest::RunTest(const FString& Parameters)
{
	FKatanaAssetMigrationOptions Options;
	Options.Operation = TEXT("AttackDataTimingMigration");
	Options.Mode = EKatanaAssetMigrationMode::Plan;
	Options.bAllowGlobalScan = true;

	TArray<FString> Errors;
	TestTrue(TEXT("Runner should accept AttackDataTimingMigration"),
		FKatanaAssetMigrationRunner::ValidateOptions(Options, Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAssetMigrationContentReadinessAcceptedByRunnerTest,
	"KatanaCombat.Editor.AssetMigration.Runner.ContentReadinessAccepted",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAssetMigrationContentReadinessAcceptedByRunnerTest::RunTest(const FString& Parameters)
{
	FKatanaAssetMigrationOptions Options;
	Options.Operation = FContentReadinessAuditOperation::OperationName;
	Options.Mode = EKatanaAssetMigrationMode::Audit;
	Options.TargetsFile = TEXT("Config/AssetMigrations/BranchCriticalContentTargets.txt");

	TArray<FString> Errors;
	TestTrue(TEXT("Runner should accept read-only ContentReadinessAudit"),
		FKatanaAssetMigrationRunner::ValidateOptions(Options, Errors));

	Options.Mode = EKatanaAssetMigrationMode::Apply;
	Errors.Reset();
	TestFalse(TEXT("Runner should reject mutating ContentReadinessAudit mode"),
		FKatanaAssetMigrationRunner::ValidateOptions(Options, Errors));

	bool bFoundExpectedError = false;
	for (const FString& Error : Errors)
	{
		bFoundExpectedError |= Error.Contains(TEXT("read-only"));
	}
	TestTrue(TEXT("Validation should explain that ContentReadinessAudit is read-only"), bFoundExpectedError);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAssetMigrationCounterChainProofAcceptedByRunnerTest,
	"KatanaCombat.Editor.AssetMigration.Runner.CounterChainProofAccepted",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAssetMigrationCounterChainProofAcceptedByRunnerTest::RunTest(const FString& Parameters)
{
	FKatanaAssetMigrationOptions Options;
	Options.Operation = FCounterChainProofMigrationOperation::OperationName;
	Options.Mode = EKatanaAssetMigrationMode::Plan;
	Options.TargetsFile = TEXT("Config/AssetMigrations/CounterChainProofTargets.txt");

	TArray<FString> Errors;
	TestTrue(TEXT("Runner should accept CounterChainProofMigration"),
		FKatanaAssetMigrationRunner::ValidateOptions(Options, Errors));

	Options.bAllowGlobalScan = true;
	Errors.Reset();
	TestFalse(TEXT("CounterChainProofMigration should reject global scan"),
		FKatanaAssetMigrationRunner::ValidateOptions(Options, Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaCounterChainProofPlanDoesNotMutateTest,
	"KatanaCombat.Editor.AssetMigration.CounterChainProof.PlanDoesNotMutate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaCounterChainProofPlanDoesNotMutateTest::RunTest(const FString& Parameters)
{
	UAnimMontage* AttackMontage = KatanaAssetMigrationTest::CreateMontage();
	UAnimMontage* VictimMontage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(AttackMontage);
	UPairedAnimationData* TemplateData = NewObject<UPairedAnimationData>(GetTransientPackage());
	TemplateData->AttackerMontage = AttackMontage;
	TemplateData->AttackerMontageSection = TEXT("Target");
	TemplateData->VictimMontage = VictimMontage;
	TemplateData->VictimMontageSection = TEXT("Target");

	FCounterChainProofTargetSpec Spec;
	Spec.InputTarget = TEXT("/Game/Test/Attack.Attack|/Game/Test/Counter.Counter|/Game/Test/Template.Template");
	Spec.AttackDataObjectPath = TEXT("/Game/Test/Attack.Attack");
	Spec.CounterDataPackageName = TEXT("/Game/Test/Counter");
	Spec.CounterDataObjectPath = TEXT("/Game/Test/Counter.Counter");
	Spec.TemplatePackageName = TEXT("/Game/Test/Template");
	Spec.TemplateObjectPath = TEXT("/Game/Test/Template.Template");

	FKatanaAssetMigrationRow Row;
	const FCounterChainProofMigrationOperation Operation;
	const int32 NotifyCountBefore = AttackMontage->Notifies.Num();
	TestTrue(TEXT("Plan should succeed"), Operation.RunLoadedObjects(
		Spec,
		AttackData,
		nullptr,
		TemplateData,
		EKatanaAssetMigrationMode::Plan,
		Row));

	TestEqual(TEXT("Plan should report WouldChange"), LexToString(Row.Status), FString(TEXT("WouldChange")));
	TestEqual(TEXT("Plan should not mutate notifies"), AttackMontage->Notifies.Num(), NotifyCountBefore);
	TestFalse(TEXT("Plan should not enable counter variant"), AttackData->bHasCounterVariant);
	TestEqual(TEXT("Plan should identify three additions"), Row.PlannedAdditions.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaCounterChainProofApplySeedsCounterDataAndWindowTest,
	"KatanaCombat.Editor.AssetMigration.CounterChainProof.ApplySeedsCounterDataAndWindow",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaCounterChainProofApplySeedsCounterDataAndWindowTest::RunTest(const FString& Parameters)
{
	UAnimMontage* AttackMontage = KatanaAssetMigrationTest::CreateMontage();
	UAnimMontage* VictimMontage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(AttackMontage);
	KatanaAssetMigrationTest::AddSemanticTags(AttackData);
	UPairedAnimationData* ExistingCounterData = NewObject<UPairedAnimationData>(GetTransientPackage());
	UPairedAnimationData* TemplateData = NewObject<UPairedAnimationData>(GetTransientPackage());
	TemplateData->AttackerMontage = AttackMontage;
	TemplateData->AttackerMontageSection = TEXT("Target");
	TemplateData->VictimMontage = VictimMontage;
	TemplateData->VictimMontageSection = TEXT("Target");
	TemplateData->bIsLethal = true;

	FCounterChainProofTargetSpec Spec;
	Spec.InputTarget = TEXT("/Game/Test/Attack.Attack|/Game/Test/Counter.Counter|/Game/Test/Template.Template");
	Spec.AttackDataObjectPath = TEXT("/Game/Test/Attack.Attack");
	Spec.CounterDataPackageName = TEXT("/Game/Test/Counter");
	Spec.CounterDataObjectPath = TEXT("/Game/Test/Counter.Counter");
	Spec.TemplatePackageName = TEXT("/Game/Test/Template");
	Spec.TemplateObjectPath = TEXT("/Game/Test/Template.Template");

	FKatanaAssetMigrationRow Row;
	const FCounterChainProofMigrationOperation Operation;
	TestTrue(TEXT("Apply should succeed"), Operation.RunLoadedObjects(
		Spec,
		AttackData,
		ExistingCounterData,
		TemplateData,
		EKatanaAssetMigrationMode::Apply,
		Row));

	TestEqual(TEXT("Apply should report Changed"), LexToString(Row.Status), FString(TEXT("Changed")));
	TestTrue(TEXT("AttackData should enable counter variant"), AttackData->bHasCounterVariant);
	TestTrue(TEXT("AttackData should link counter data"), AttackData->CounterData == ExistingCounterData);
	TestEqual(TEXT("Counter data should be typed as Counter"), static_cast<int32>(ExistingCounterData->ReactionType), static_cast<int32>(EPairedReactionType::Counter));
	TestFalse(TEXT("Counter data should be nonlethal"), ExistingCounterData->bIsLethal);
	TestTrue(TEXT("Counter data should remain valid"), ExistingCounterData->IsValid());

	bool bFoundCounterWindow = false;
	for (const FAnimNotifyEvent& Event : AttackMontage->Notifies)
	{
		if (const UAnimNotifyState_CounterWindow* CounterWindow = Cast<UAnimNotifyState_CounterWindow>(Event.NotifyStateClass))
		{
			bFoundCounterWindow |= CounterWindow->CounterData == ExistingCounterData &&
				CounterWindow->AttackType == AttackData->AttackType &&
				FMath::IsNearlyEqual(Event.GetTriggerTime(), 0.0f);
		}
	}
	TestTrue(TEXT("Apply should seed a specific CounterWindow"), bFoundCounterWindow);
	TestTrue(TEXT("Row should report counter window after apply"), Row.bHasCounterWindow);
	TestTrue(TEXT("Row should report counter variant data after apply"), Row.bCounterVariantHasData);
	KatanaAssetMigrationTest::ExpectSemanticRowFields(*this, Row, TEXT("CounterChainProof"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaContentReadinessAttackDataAuditReportsNotifyDebtTest,
	"KatanaCombat.Editor.AssetMigration.ContentReadiness.AttackDataReportsNotifyDebt",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaContentReadinessAttackDataAuditReportsNotifyDebtTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);

	FKatanaAssetMigrationRow Row;
	const FContentReadinessAuditOperation Operation;
	TestTrue(TEXT("Loaded AttackData audit should succeed"),
		Operation.RunLoadedObject(TEXT("/Game/Test/DA_Attack.DA_Attack"), AttackData, false, Row));
	TestTrue(TEXT("AttackData should be reported loaded"), Row.bLoaded);
	TestTrue(TEXT("AttackData section should be valid"), Row.bAttackDataSectionValid);
	TestEqual(TEXT("AttackData with missing canonical notifies should report WouldChange"),
		LexToString(Row.Status), FString(TEXT("WouldChange")));
	TestTrue(TEXT("Audit should carry notify readiness warning"), Row.Warnings.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaContentReadinessPairedDataFailsMissingVictimMontageTest,
	"KatanaCombat.Editor.AssetMigration.ContentReadiness.PairedDataFailsMissingVictimMontage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaContentReadinessPairedDataFailsMissingVictimMontageTest::RunTest(const FString& Parameters)
{
	UPairedAnimationData* PairedData = NewObject<UPairedAnimationData>(GetTransientPackage());
	PairedData->AttackerMontage = KatanaAssetMigrationTest::CreateMontage();

	FKatanaAssetMigrationRow Row;
	const FContentReadinessAuditOperation Operation;
	TestFalse(TEXT("Paired data audit should fail when victim montage is missing"),
		Operation.RunLoadedObject(TEXT("/Game/Test/DA_Paired.DA_Paired"), PairedData, false, Row));
	TestEqual(TEXT("Status should be Failed"), LexToString(Row.Status), FString(TEXT("Failed")));
	TestFalse(TEXT("Paired data should not be valid"), Row.bPairedAnimationValid);
	TestTrue(TEXT("Failure should name missing victim montage"),
		Row.Errors.Contains(TEXT("VictimMontage is required but unset")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaContentReadinessAttackDataFailsInvalidCounterDataTest,
	"KatanaCombat.Editor.AssetMigration.ContentReadiness.AttackDataFailsInvalidCounterData",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaContentReadinessAttackDataFailsInvalidCounterDataTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);
	AttackData->bHasCounterVariant = true;
	AttackData->CounterData = NewObject<UPairedAnimationData>(GetTransientPackage());

	FKatanaAssetMigrationRow Row;
	const FContentReadinessAuditOperation Operation;
	TestFalse(TEXT("AttackData audit should fail when enabled CounterData is invalid"),
		Operation.RunLoadedObject(TEXT("/Game/Test/DA_Attack.DA_Attack"), AttackData, false, Row));
	TestEqual(TEXT("Status should be Failed"), LexToString(Row.Status), FString(TEXT("Failed")));

	bool bFoundCounterDataError = false;
	for (const FString& Error : Row.Errors)
	{
		bFoundCounterDataError |= Error.Contains(TEXT("CounterData")) && Error.Contains(TEXT("runtime validation failed"));
	}
	TestTrue(TEXT("Failure should name invalid CounterData"), bFoundCounterDataError);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAttackDataNotifyOperationReportsBranchReadinessTest,
	"KatanaCombat.Editor.AssetMigration.AttackDataNotify.OperationReportsBranchReadiness",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAttackDataNotifyOperationReportsBranchReadinessTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = KatanaAssetMigrationTest::CreateMontage();
	UAttackData* AttackData = KatanaAssetMigrationTest::CreateAttackData(Montage);
	AttackData->bHasCounterVariant = true;
	AttackData->CounterData = NewObject<UPairedAnimationData>(GetTransientPackage());
	KatanaAssetMigrationTest::AddStateNotify<UAnimNotifyState_ParryWindow>(Montage, 0.20f, 0.15f);
	KatanaAssetMigrationTest::AddStateNotify<UAnimNotifyState_CounterWindow>(Montage, 0.35f, 0.15f);

	FKatanaAssetMigrationRow Row;
	const FAttackDataNotifyMigrationOperation Operation;
	TestTrue(TEXT("Audit operation should succeed"), Operation.Run(AttackData, EKatanaAssetMigrationMode::Audit, Row));
	TestTrue(TEXT("Row should report parry window"), Row.bHasParryWindow);
	TestTrue(TEXT("Row should report counter window"), Row.bHasCounterWindow);
	TestTrue(TEXT("Row should report counter variant data"), Row.bCounterVariantHasData);
	TestFalse(TEXT("Row should not report finisher data"), Row.bFinisherHasData);
	TestTrue(TEXT("Row should carry branch readiness warning"),
		Row.BranchReadinessWarnings.Contains(TEXT("CounterData is lethal; Chain counter steps are nonlethal by default unless runtime policy explicitly allows lethal counter data")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAssetMigrationReportPathProjectRelativeTest,
	"KatanaCombat.Editor.AssetMigration.Runner.ReportPathProjectRelative",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAssetMigrationReportPathProjectRelativeTest::RunTest(const FString& Parameters)
{
	FKatanaAssetMigrationReport Report;
	Report.Operation = TEXT("AttackDataNotifyMigration");
	Report.Mode = EKatanaAssetMigrationMode::Audit;
	FKatanaAssetMigrationRunner::Summarize(Report);

	const FString RelativeReportPath = TEXT("Saved/Automation/KatanaAssetMigrationRelativeReport.json");
	const FString ExpectedProjectPath = FPaths::Combine(FPaths::ProjectDir(), RelativeReportPath);
	const FString AlternateCwd = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("AlternateCwd"));
	const FString UnexpectedCwdPath = FPaths::Combine(AlternateCwd, RelativeReportPath);

	IFileManager::Get().MakeDirectory(*AlternateCwd, true);
	IFileManager::Get().Delete(*ExpectedProjectPath);
	IFileManager::Get().Delete(*UnexpectedCwdPath);

	const FString OriginalCwd = FPlatformProcess::GetCurrentWorkingDirectory();
	_wchdir(*AlternateCwd);

	TArray<FString> Errors;
	const bool bWroteReport = FKatanaAssetMigrationRunner::WriteReport(Report, RelativeReportPath, Errors);

	_wchdir(*OriginalCwd);

	TestTrue(TEXT("Relative report should write"), bWroteReport);
	TestTrue(TEXT("Relative report path should resolve under project dir"),
		FPaths::FileExists(ExpectedProjectPath));
	TestFalse(TEXT("Relative report path should not resolve under process cwd"),
		FPaths::FileExists(UnexpectedCwdPath));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAssetMigrationTargetPathNormalizationTest,
	"KatanaCombat.Editor.AssetMigration.Runner.TargetPathNormalization",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAssetMigrationTargetPathNormalizationTest::RunTest(const FString& Parameters)
{
	FString ObjectPath;
	FString Error;

	TestTrue(TEXT("Object path should normalize"), FKatanaAssetMigrationRunner::NormalizeAttackDataTargetObjectPath(
		TEXT("/Game/Combat/DA_Test.DA_Test"), ObjectPath, Error));
	TestEqual(TEXT("Object path should stay unchanged"), ObjectPath, FString(TEXT("/Game/Combat/DA_Test.DA_Test")));

	TestTrue(TEXT("Package path should normalize"), FKatanaAssetMigrationRunner::NormalizeAttackDataTargetObjectPath(
		TEXT("/Game/Combat/DA_Test"), ObjectPath, Error));
	TestEqual(TEXT("Package path should append asset name"), ObjectPath, FString(TEXT("/Game/Combat/DA_Test.DA_Test")));

	FString PackageName;
	TestTrue(TEXT("Content object path should normalize"), FKatanaAssetMigrationRunner::NormalizeContentTargetObjectPath(
		TEXT("/Game/Combat/BP_Player.BP_Player"), PackageName, ObjectPath, Error));
	TestEqual(TEXT("Content object path should populate package name"), PackageName, FString(TEXT("/Game/Combat/BP_Player")));
	TestEqual(TEXT("Content object path should stay unchanged"), ObjectPath, FString(TEXT("/Game/Combat/BP_Player.BP_Player")));

	TestTrue(TEXT("Content package path should normalize"), FKatanaAssetMigrationRunner::NormalizeContentTargetObjectPath(
		TEXT("/Game/Combat/BP_Player"), PackageName, ObjectPath, Error));
	TestEqual(TEXT("Content package path should append asset name"), ObjectPath, FString(TEXT("/Game/Combat/BP_Player.BP_Player")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatanaAssetMigrationRunnerInvalidTargetWritesReportTest,
	"KatanaCombat.Editor.AssetMigration.Runner.InvalidTargetWritesReport",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FKatanaAssetMigrationRunnerInvalidTargetWritesReportTest::RunTest(const FString& Parameters)
{
	const FString TargetsFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("KatanaAssetMigrationMissingTargets.txt"));
	const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("KatanaAssetMigrationMissingTargetsReport.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(TargetsFile), true);

	TArray<FString> TargetLines;
	TargetLines.Add(TEXT("/Game/KatanaCombat/Missing/DA_Missing"));
	TestTrue(TEXT("Targets file should write"), FFileHelper::SaveStringArrayToFile(TargetLines, *TargetsFile));

	FKatanaAssetMigrationOptions Options;
	Options.Operation = TEXT("AttackDataNotifyMigration");
	Options.Mode = EKatanaAssetMigrationMode::Audit;
	Options.TargetsFile = TargetsFile;
	Options.ReportPath = ReportPath;

	FKatanaAssetMigrationRunner Runner;
	AddExpectedErrorPlain(TEXT("LoadPackage: SkipPackage: /Game/KatanaCombat/Missing/DA_Missing"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("Failed to find object 'AttackData /Game/KatanaCombat/Missing/DA_Missing.DA_Missing'"), EAutomationExpectedErrorFlags::Contains, 1);
	const EKatanaAssetMigrationExitCode ExitCode = Runner.Run(Options);
	TestEqual(TEXT("Invalid target should return row failure"), static_cast<int32>(ExitCode), static_cast<int32>(EKatanaAssetMigrationExitCode::RowFailure));

	FString ReportJson;
	TestTrue(TEXT("Failure report should be written"), FFileHelper::LoadFileToString(ReportJson, *ReportPath));
	TestTrue(TEXT("Report should name missing target"), ReportJson.Contains(TEXT("/Game/KatanaCombat/Missing/DA_Missing")));
	return true;
}

#endif // WITH_EDITOR
