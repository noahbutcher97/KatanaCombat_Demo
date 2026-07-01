# Headless Asset Migration Commandlet Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a reusable headless asset migration framework and first AttackData notify migration operation with read-only audit/plan modes, explicit save gates, and durable JSON reports.

**Architecture:** Keep migration behavior in testable C++ service classes and keep commandlet code thin. The service analyzes and applies notify plans on loaded `UAttackData` and `UAnimMontage` objects; the runner parses options, dispatches operations, writes reports, and saves packages only when explicitly allowed. `UAttackDataTools` should delegate to the same service so editor UI and commandlet behavior cannot drift.

**Tech Stack:** Unreal Engine 5.6 C++, `KatanaCombatEditor` editor module, `KatanaCombatTest` automation module, `UCommandlet`, `AssetRegistry`, `Json`, PowerShell verification commands.

## Global Constraints

- Preserve unrelated workspace WIP; inspect `git status --short` before edits.
- Do not touch, resave, rename, delete, or mass-add binary assets under `Content/` in this implementation plan.
- Explicit targets are required unless `-AllowGlobalScan` is present.
- `TargetsFile` entries must accept both object paths such as `/Game/Folder/DA_Attack.DA_Attack` and package paths such as `/Game/Folder/DA_Attack`.
- `ApplyAndSave` requires `-AllowPackageSave`.
- Reject packages that were dirty before migration apply unless `-AllowDirtyPackages` is present; packages dirtied only by this commandlet may be saved when `-AllowPackageSave` is present.
- Do not auto-calculate or rewrite AttackData timing. `-AllowTimingMutation` is reserved for a separate explicit operation and is rejected by `AttackDataNotifyMigration`.
- Plan rows must include the packages that would change while still leaving package dirty state unchanged.
- No binary asset saves during automation tests.
- No deletion of legacy C++ notify classes.
- Keep runtime combat behavior unchanged.
- Use `git add` by explicit path only if the user authorizes commits during execution.
- When adding tests to `KatanaAssetMigrationTests.cpp`, keep all `#include` lines in the top include block; append only test declarations and helpers below existing tests.

---

## File Map

- Create: `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationTypes.h`
  Shared modes, statuses, options, report rows, summaries, and parse helpers.
- Create: `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationCommandlet.h`
  `UCommandlet` declaration required by UHT.
- Create: `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationCommandlet.cpp`
  Thin `Main()` wrapper for `-run=KatanaAssetMigration`.
- Create: `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationRunner.h`
  Public runner interface used by tests and the commandlet.
- Create: `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp`
  Option parsing, validation, target resolution, operation dispatch, JSON report writing, and package saving.
- Create: `Source/KatanaCombatEditor/Public/Commandlets/Operations/AttackDataNotifyMigrationOperation.h`
  Public operation interface used by tests and the runner.
- Create: `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.cpp`
  First registered migration operation.
- Create: `Source/KatanaCombatEditor/Public/AttackDataNotifyGenerationService.h`
  Public service interface so tests and editor helpers can use the same behavior.
- Create: `Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp`
  Analyze, plan, and apply logic for canonical AttackData notifies.
- Modify: `Source/KatanaCombatEditor/Public/AttackDataTools.h`
  Keep public/deprecated helper surface stable; remove private-only helpers only if compile proves there are no call sites and tests still cover the behavior.
- Modify: `Source/KatanaCombatEditor/Private/AttackDataTools.cpp`
  Delegate `GenerateAllNotifies` mutation to the service while preserving validation, transaction, logging, and Blueprint API.
- Modify: `Source/KatanaCombatEditor/KatanaCombatEditor.build.cs`
  Add `Json` dependency.
- Modify: `Source/KatanaCombatTest/KatanaCombatTest.build.cs`
  Add `Json` dependency for report verification tests.
- Create: `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`
  Focused service, operation, parser, report, and save-gate tests.
- Create: `docs/guides/HEADLESS_ASSET_MIGRATIONS.md`
  Operator guide for audit, plan, apply, and save modes.
- Modify: `AGENTS.md`
  Add a short pointer to the migration guide and asset-save safety rule.

---

### Task 1: Shared Migration Types And Parser Tests

**Files:**
- Create: `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationTypes.h`
- Create: `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`
- Modify: `Source/KatanaCombatEditor/KatanaCombatEditor.build.cs`
- Modify: `Source/KatanaCombatTest/KatanaCombatTest.build.cs`

**Interfaces:**
- Produces: `EKatanaAssetMigrationMode`, `EKatanaAssetMigrationStatus`, `EKatanaAssetMigrationExitCode`
- Produces: `FKatanaAssetMigrationOptions`, `FKatanaAssetMigrationRow`, `FKatanaAssetMigrationSummary`, `FKatanaAssetMigrationReport`
- Produces: `bool TryParseKatanaAssetMigrationMode(const FString& Value, EKatanaAssetMigrationMode& OutMode)`
- Produces: `FString LexToString(EKatanaAssetMigrationMode Mode)` and `FString LexToString(EKatanaAssetMigrationStatus Status)`

- [ ] **Step 1: Inspect scoped status**

Run:
```powershell
git status --short -- Source/KatanaCombatEditor Source/KatanaCombatTest docs/superpowers/plans docs/superpowers/specs
```
Expected: existing AttackData editor/test WIP plus the spec/plan files. Do not touch `Content/`.

- [ ] **Step 2: Add module dependencies**

In `Source/KatanaCombatEditor/KatanaCombatEditor.build.cs`, add `"Json"` to `PrivateDependencyModuleNames`.

In `Source/KatanaCombatTest/KatanaCombatTest.build.cs`, add `"Json"` to the editor-only `PrivateDependencyModuleNames` array:
```csharp
PrivateDependencyModuleNames.AddRange(new string[]
{
	"UnrealEd",
	"KatanaCombatEditor",
	"Json"
});
```

- [ ] **Step 3: Write the failing parser/default tests**

Create `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp` with:
```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Commandlets/KatanaAssetMigrationTypes.h"

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

#endif
```

- [ ] **Step 4: Run tests to verify RED**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -Progress -NoHotReload
```
Expected: compile fails because `Commandlets/KatanaAssetMigrationTypes.h` does not exist.

- [ ] **Step 5: Add shared type header**

Create `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationTypes.h`:
```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EKatanaAssetMigrationMode : uint8
{
	Audit,
	Plan,
	Apply,
	ApplyAndSave
};

enum class EKatanaAssetMigrationStatus : uint8
{
	Unchanged,
	WouldChange,
	Changed,
	Saved,
	Failed
};

enum class EKatanaAssetMigrationExitCode : int32
{
	Success = 0,
	RowFailure = 1,
	InvalidArguments = 2,
	SaveFailure = 3
};

struct FKatanaAssetMigrationOptions
{
	FString Operation;
	EKatanaAssetMigrationMode Mode = EKatanaAssetMigrationMode::Audit;
	FString TargetsFile;
	FString ReportPath;
	bool bAllowGlobalScan = false;
	bool bAllowPackageSave = false;
	bool bAllowDirtyPackages = false;
	bool bAllowTimingMutation = false;
};

struct FKatanaAssetMigrationRow
{
	FString InputTarget;
	FString AttackData;
	FString Montage;
	FString Section;
	EKatanaAssetMigrationStatus Status = EKatanaAssetMigrationStatus::Unchanged;
	TArray<FString> LegacyNotifiesFound;
	TArray<FString> CanonicalNotifiesMissing;
	TArray<FString> PlannedRemovals;
	TArray<FString> PlannedAdditions;
	TArray<FString> ChangedPackages;
	TArray<FString> SavedPackages;
	TArray<FString> Warnings;
	TArray<FString> Errors;
};

struct FKatanaAssetMigrationSummary
{
	int32 Targets = 0;
	int32 WouldChange = 0;
	int32 Changed = 0;
	int32 Unchanged = 0;
	int32 Failed = 0;
	int32 Saved = 0;
};

struct FKatanaAssetMigrationReport
{
	int32 SchemaVersion = 1;
	FString Operation;
	EKatanaAssetMigrationMode Mode = EKatanaAssetMigrationMode::Audit;
	FKatanaAssetMigrationSummary Summary;
	TArray<FKatanaAssetMigrationRow> Rows;
};

inline FString LexToString(EKatanaAssetMigrationMode Mode)
{
	switch (Mode)
	{
	case EKatanaAssetMigrationMode::Audit: return TEXT("Audit");
	case EKatanaAssetMigrationMode::Plan: return TEXT("Plan");
	case EKatanaAssetMigrationMode::Apply: return TEXT("Apply");
	case EKatanaAssetMigrationMode::ApplyAndSave: return TEXT("ApplyAndSave");
	}

	return TEXT("Audit");
}

inline FString LexToString(EKatanaAssetMigrationStatus Status)
{
	switch (Status)
	{
	case EKatanaAssetMigrationStatus::Unchanged: return TEXT("Unchanged");
	case EKatanaAssetMigrationStatus::WouldChange: return TEXT("WouldChange");
	case EKatanaAssetMigrationStatus::Changed: return TEXT("Changed");
	case EKatanaAssetMigrationStatus::Saved: return TEXT("Saved");
	case EKatanaAssetMigrationStatus::Failed: return TEXT("Failed");
	}

	return TEXT("Failed");
}

inline bool TryParseKatanaAssetMigrationMode(const FString& Value, EKatanaAssetMigrationMode& OutMode)
{
	if (Value.Equals(TEXT("Audit"), ESearchCase::IgnoreCase))
	{
		OutMode = EKatanaAssetMigrationMode::Audit;
		return true;
	}
	if (Value.Equals(TEXT("Plan"), ESearchCase::IgnoreCase))
	{
		OutMode = EKatanaAssetMigrationMode::Plan;
		return true;
	}
	if (Value.Equals(TEXT("Apply"), ESearchCase::IgnoreCase))
	{
		OutMode = EKatanaAssetMigrationMode::Apply;
		return true;
	}
	if (Value.Equals(TEXT("ApplyAndSave"), ESearchCase::IgnoreCase))
	{
		OutMode = EKatanaAssetMigrationMode::ApplyAndSave;
		return true;
	}

	return false;
}
```

- [ ] **Step 6: Run parser tests to verify GREEN**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration.Options;Quit" -NullRHI -NoSplash -Unattended -nopause -stdout
```
Expected: `KatanaCombat.Editor.AssetMigration.Options` tests pass.

---

### Task 2: AttackData Notify Generation Service

**Files:**
- Create: `Source/KatanaCombatEditor/Public/AttackDataNotifyGenerationService.h`
- Create: `Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp`
- Modify: `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`

**Interfaces:**
- Consumes: `UAttackData`, `UAnimMontage`, canonical notify classes.
- Produces: `FAttackDataNotifyAnalysis`, `FAttackDataNotifyPlan`
- Produces: `FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(const UAttackData*)`
- Produces: `FAttackDataNotifyGenerationService::BuildAttackDataNotifyPlan(const FAttackDataNotifyAnalysis&)`
- Produces: `FAttackDataNotifyGenerationService::ApplyAttackDataNotifyPlan(UAttackData*, const FAttackDataNotifyPlan&)`

- [ ] **Step 1: Add failing non-mutating plan and scoped apply tests**

Move these includes into the top include block of `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`, then append the helper namespace and tests below the parser tests:
```cpp
#include "AttackDataNotifyGenerationService.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "Animation/AnimNotify_HoldWindowStart.h"
#include "Animation/AnimNotify_ToggleHitDetection.h"
#include "Animation/AnimNotifyState_AttackPhase.h"
#include "Animation/AnimNotifyState_ComboWindow.h"
#include "Animation/AnimNotifyState_HoldWindow.h"
#include "Data/AttackData.h"

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
```

- [ ] **Step 2: Run service tests to verify RED**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -Progress -NoHotReload
```
Expected: compile fails because `AttackDataNotifyGenerationService.h` does not exist.

- [ ] **Step 3: Create service header**

Create `Source/KatanaCombatEditor/Public/AttackDataNotifyGenerationService.h`:
```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"

class UAttackData;
class UAnimMontage;

struct FAttackDataNotifyAnalysis
{
	TWeakObjectPtr<const UAttackData> AttackData;
	TWeakObjectPtr<UAnimMontage> Montage;
	FName SectionName = NAME_None;
	float SectionStart = 0.0f;
	float SectionEnd = 0.0f;
	float ActiveTransitionTime = 0.0f;
	float RecoveryTransitionTime = 0.0f;
	float HoldStartTime = 0.0f;
	bool bValid = false;
	bool bShouldHaveHoldStart = false;
	TArray<FString> LegacyNotifiesFound;
	TArray<FString> CanonicalNotifiesMissing;
	TArray<FString> Errors;
	TArray<int32> LegacyNotifyIndices;
	TArray<int32> ExistingCanonicalNotifyIndices;
};

struct FAttackDataNotifyPlan
{
	TWeakObjectPtr<UAnimMontage> Montage;
	FName SectionName = NAME_None;
	TArray<int32> RemovalNotifyIndices;
	TArray<FString> PlannedRemovals;
	TArray<FString> PlannedAdditions;
	TArray<FString> Errors;
	float ActiveTransitionTime = 0.0f;
	float RecoveryTransitionTime = 0.0f;
	float HoldStartTime = 0.0f;
	bool bValid = false;
	bool bAddHoldStart = false;

	bool HasChanges() const
	{
		return RemovalNotifyIndices.Num() > 0 || PlannedAdditions.Num() > 0;
	}
};

class KATANACOMBATEDITOR_API FAttackDataNotifyGenerationService
{
public:
	static FAttackDataNotifyAnalysis AnalyzeAttackDataNotifies(const UAttackData* AttackData);
	static FAttackDataNotifyPlan BuildAttackDataNotifyPlan(const FAttackDataNotifyAnalysis& Analysis);
	static bool ApplyAttackDataNotifyPlan(UAttackData* AttackData, const FAttackDataNotifyPlan& Plan);
	static bool ShouldGenerateHoldWindowStart(const UAttackData* AttackData);
};
```

- [ ] **Step 4: Implement service**

Create `Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp`. Use these implementation rules:
```cpp
#include "AttackDataNotifyGenerationService.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "Animation/AnimNotify_HoldWindowStart.h"
#include "Animation/AnimNotify_ToggleHitDetection.h"
#include "Animation/AnimNotifyState_AttackPhase.h"
#include "Animation/AnimNotifyState_ComboWindow.h"
#include "Animation/AnimNotifyState_HoldWindow.h"
#include "Data/AttackData.h"

namespace
{
	bool IsTimeInSection(float Time, const FAttackDataNotifyAnalysis& Analysis)
	{
		return Time >= Analysis.SectionStart && Time < Analysis.SectionEnd;
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
```

Then implement `AnalyzeAttackDataNotifies` so it:
- Fails with `Errors.Add(TEXT("AttackData is null"))` when `AttackData` is null.
- Fails with `Errors.Add(TEXT("AttackData has no AttackMontage"))` when `AttackMontage` is null.
- Uses `AttackData->GetSectionTimeRange(SectionStart, SectionEnd)`.
- Fails when `SectionEnd <= SectionStart`.
- Fails when `ManualTiming.WindupDuration <= 0.0f`, `ManualTiming.ActiveDuration <= 0.0f`, or `ManualTiming.RecoveryDuration < 0.0f`.
- Fails when total phase duration exceeds section length.
- Sets transition times from manual timing.
- Scans only events whose trigger time is inside the section.
- Adds legacy classes and indices for `UAnimNotifyState_AttackPhase`, `UAnimNotify_ToggleHitDetection`, `UAnimNotifyState_HoldWindow`, and `UAnimNotifyState_ComboWindow`.
- Adds canonical missing strings for missing Active transition, Recovery transition, and hold start.

Implement `BuildAttackDataNotifyPlan` so it copies removal indices and planned strings from valid analysis, adds transition additions for missing transition classes, and adds hold-start addition when required.

Implement `ApplyAttackDataNotifyPlan` so it:
- Returns false if `AttackData`, `AttackMontage`, or `Plan.bValid` is false.
- Calls `Montage->Modify()` before mutating notifies. Do not call `AttackData->Modify()` unless a future operation actually changes `UAttackData` fields.
- Removes planned indices in descending order.
- Adds `UAnimNotify_AttackPhaseTransition` for Active and Recovery when planned.
- Adds `UAnimNotify_HoldWindowStart` with `LightAttack` or `HeavyAttack` input when planned.
- Calls `Montage->SortNotifies()`, `Montage->RefreshCacheData()`, and `Montage->MarkPackageDirty()`.

- [ ] **Step 5: Run service tests to verify GREEN**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration.AttackDataNotify;Quit" -NullRHI -NoSplash -Unattended -nopause -stdout
```
Expected: service tests pass.

---

### Task 3: AttackData Notify Migration Operation

**Files:**
- Create: `Source/KatanaCombatEditor/Public/Commandlets/Operations/AttackDataNotifyMigrationOperation.h`
- Create: `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.cpp`
- Modify: `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`

**Interfaces:**
- Consumes: `FAttackDataNotifyGenerationService`
- Produces: `FAttackDataNotifyMigrationOperation::OperationName`
- Produces: `bool FAttackDataNotifyMigrationOperation::Run(UAttackData* AttackData, EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const`

- [ ] **Step 1: Add failing operation mode test**

Move the include into the top include block, then append:
```cpp
#include "Commandlets/Operations/AttackDataNotifyMigrationOperation.h"

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
	return true;
}
```

- [ ] **Step 2: Run operation test to verify RED**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -Progress -NoHotReload
```
Expected: compile fails because `AttackDataNotifyMigrationOperation.h` does not exist.

- [ ] **Step 3: Add operation declaration**

Create `Source/KatanaCombatEditor/Public/Commandlets/Operations/AttackDataNotifyMigrationOperation.h`:
```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/KatanaAssetMigrationTypes.h"

class UAttackData;

class KATANACOMBATEDITOR_API FAttackDataNotifyMigrationOperation
{
public:
	static const FString OperationName;

	bool Run(UAttackData* AttackData, EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const;
};
```

- [ ] **Step 4: Add operation implementation**

Create `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.cpp`:
```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/Operations/AttackDataNotifyMigrationOperation.h"

#include "AttackDataNotifyGenerationService.h"
#include "Animation/AnimMontage.h"
#include "Data/AttackData.h"

const FString FAttackDataNotifyMigrationOperation::OperationName = TEXT("AttackDataNotifyMigration");

bool FAttackDataNotifyMigrationOperation::Run(UAttackData* AttackData, EKatanaAssetMigrationMode Mode, FKatanaAssetMigrationRow& OutRow) const
{
	OutRow = FKatanaAssetMigrationRow();
	OutRow.InputTarget = AttackData ? AttackData->GetPathName() : FString();
	OutRow.AttackData = AttackData ? AttackData->GetPathName() : FString();
	OutRow.Montage = (AttackData && AttackData->AttackMontage) ? AttackData->AttackMontage->GetPathName() : FString();
	OutRow.Section = AttackData ? AttackData->MontageSection.ToString() : FString();

	const FAttackDataNotifyAnalysis Analysis = FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);
	OutRow.LegacyNotifiesFound = Analysis.LegacyNotifiesFound;
	OutRow.CanonicalNotifiesMissing = Analysis.CanonicalNotifiesMissing;

	if (!Analysis.bValid)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors = Analysis.Errors;
		return false;
	}

	if (Mode == EKatanaAssetMigrationMode::Audit)
	{
		OutRow.Status = (Analysis.LegacyNotifiesFound.Num() > 0 || Analysis.CanonicalNotifiesMissing.Num() > 0)
			? EKatanaAssetMigrationStatus::WouldChange
			: EKatanaAssetMigrationStatus::Unchanged;
		return true;
	}

	const FAttackDataNotifyPlan Plan = FAttackDataNotifyGenerationService::BuildAttackDataNotifyPlan(Analysis);
	OutRow.PlannedRemovals = Plan.PlannedRemovals;
	OutRow.PlannedAdditions = Plan.PlannedAdditions;

	if (!Plan.bValid)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors = Plan.Errors;
		return false;
	}

	if (!Plan.HasChanges())
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Unchanged;
		return true;
	}

	if (AttackData && AttackData->AttackMontage)
	{
		OutRow.ChangedPackages.AddUnique(AttackData->AttackMontage->GetOutermost()->GetName());
	}

	if (Mode == EKatanaAssetMigrationMode::Plan)
	{
		OutRow.Status = EKatanaAssetMigrationStatus::WouldChange;
		return true;
	}

	if (!FAttackDataNotifyGenerationService::ApplyAttackDataNotifyPlan(AttackData, Plan))
	{
		OutRow.Status = EKatanaAssetMigrationStatus::Failed;
		OutRow.Errors.Add(TEXT("ApplyAttackDataNotifyPlan failed"));
		return false;
	}

	OutRow.Status = EKatanaAssetMigrationStatus::Changed;
	return true;
}
```

- [ ] **Step 5: Run operation test to verify GREEN**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration.AttackDataNotify.Operation;Quit" -NullRHI -NoSplash -Unattended -nopause -stdout
```
Expected: operation tests pass.

---

### Task 4: Runner Option Validation And JSON Reports

**Files:**
- Create: `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationRunner.h`
- Create: `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp`
- Modify: `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`

**Interfaces:**
- Produces: `FKatanaAssetMigrationRunner::ParseOptions(const FString&, FKatanaAssetMigrationOptions&, TArray<FString>&)`
- Produces: `FKatanaAssetMigrationRunner::ValidateOptions(const FKatanaAssetMigrationOptions&, TArray<FString>&)`
- Produces: `FKatanaAssetMigrationRunner::NormalizeAttackDataTargetObjectPath(const FString&, FString&, FString&)`
- Produces: `FKatanaAssetMigrationRunner::WriteReport(const FKatanaAssetMigrationReport&, const FString&, TArray<FString>&)`
- Produces: `FKatanaAssetMigrationRunner::Summarize(FKatanaAssetMigrationReport&)`

- [ ] **Step 1: Add failing validation and report tests**

Move these includes into the top include block, then append:
```cpp
#include "Commandlets/KatanaAssetMigrationRunner.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
	Row.AttackData = Row.InputTarget;
	Row.Montage = TEXT("/Game/Test/AM_Test.AM_Test");
	Row.Section = TEXT("Target");
	Row.Status = EKatanaAssetMigrationStatus::WouldChange;
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
	return true;
}
```

- [ ] **Step 2: Run runner tests to verify RED**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -Progress -NoHotReload
```
Expected: compile fails because `KatanaAssetMigrationRunner.h` does not exist.

- [ ] **Step 3: Add runner declaration**

Create `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationRunner.h`:
```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/KatanaAssetMigrationTypes.h"

class UAttackData;

class KATANACOMBATEDITOR_API FKatanaAssetMigrationRunner
{
public:
	static bool ParseOptions(const FString& Params, FKatanaAssetMigrationOptions& OutOptions, TArray<FString>& OutErrors);
	static bool ValidateOptions(const FKatanaAssetMigrationOptions& Options, TArray<FString>& OutErrors);
	static bool NormalizeAttackDataTargetObjectPath(const FString& TargetString, FString& OutObjectPath, FString& OutError);
	static void Summarize(FKatanaAssetMigrationReport& Report);
	static bool WriteReport(const FKatanaAssetMigrationReport& Report, const FString& ReportPath, TArray<FString>& OutErrors);

	EKatanaAssetMigrationExitCode Run(const FKatanaAssetMigrationOptions& Options);

private:
	bool LoadTargets(const FKatanaAssetMigrationOptions& Options, TArray<UAttackData*>& OutTargets, TArray<FKatanaAssetMigrationRow>& OutFailedRows) const;
	bool RunAttackDataNotifyMigration(const FKatanaAssetMigrationOptions& Options, const TArray<UAttackData*>& Targets, FKatanaAssetMigrationReport& OutReport) const;
	bool SaveChangedPackages(const FKatanaAssetMigrationOptions& Options, const TSet<FString>& InitiallyDirtyPackages, FKatanaAssetMigrationReport& Report) const;
};
```

- [ ] **Step 4: Implement parsing, validation, summarize, and report writing**

Create `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp` with:
```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/KatanaAssetMigrationRunner.h"

#include "Commandlets/Operations/AttackDataNotifyMigrationOperation.h"
#include "Data/AttackData.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FKatanaAssetMigrationRunner::ParseOptions(const FString& Params, FKatanaAssetMigrationOptions& OutOptions, TArray<FString>& OutErrors)
{
	FString ModeString;
	FParse::Value(*Params, TEXT("Operation="), OutOptions.Operation);
	FParse::Value(*Params, TEXT("Mode="), ModeString);
	FParse::Value(*Params, TEXT("TargetsFile="), OutOptions.TargetsFile);
	FParse::Value(*Params, TEXT("ReportPath="), OutOptions.ReportPath);
	OutOptions.bAllowGlobalScan = FParse::Param(*Params, TEXT("AllowGlobalScan"));
	OutOptions.bAllowPackageSave = FParse::Param(*Params, TEXT("AllowPackageSave"));
	OutOptions.bAllowDirtyPackages = FParse::Param(*Params, TEXT("AllowDirtyPackages"));
	OutOptions.bAllowTimingMutation = FParse::Param(*Params, TEXT("AllowTimingMutation"));

	if (!ModeString.IsEmpty() && !TryParseKatanaAssetMigrationMode(ModeString, OutOptions.Mode))
	{
		OutErrors.Add(FString::Printf(TEXT("Unknown mode '%s'"), *ModeString));
		return false;
	}

	return ValidateOptions(OutOptions, OutErrors);
}

bool FKatanaAssetMigrationRunner::ValidateOptions(const FKatanaAssetMigrationOptions& Options, TArray<FString>& OutErrors)
{
	if (Options.Operation.IsEmpty())
	{
		OutErrors.Add(TEXT("Missing -Operation"));
	}
	else if (!Options.Operation.Equals(FAttackDataNotifyMigrationOperation::OperationName, ESearchCase::IgnoreCase))
	{
		OutErrors.Add(FString::Printf(TEXT("Unknown operation '%s'"), *Options.Operation));
	}

	if (!Options.bAllowGlobalScan && Options.TargetsFile.IsEmpty())
	{
		OutErrors.Add(TEXT("Explicit targets are required unless -AllowGlobalScan is present"));
	}

	if (Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave && !Options.bAllowPackageSave)
	{
		OutErrors.Add(TEXT("ApplyAndSave requires -AllowPackageSave"));
	}

	return OutErrors.Num() == 0;
}

bool FKatanaAssetMigrationRunner::NormalizeAttackDataTargetObjectPath(const FString& TargetString, FString& OutObjectPath, FString& OutError)
{
	FString TrimmedTarget = TargetString;
	TrimmedTarget.TrimStartAndEndInline();
	if (TrimmedTarget.IsEmpty())
	{
		OutError = TEXT("Target path is empty");
		return false;
	}

	if (TrimmedTarget.Contains(TEXT(".")))
	{
		OutObjectPath = TrimmedTarget;
		return true;
	}

	if (!FPackageName::IsValidLongPackageName(TrimmedTarget))
	{
		OutError = FString::Printf(TEXT("Target package path is invalid: %s"), *TrimmedTarget);
		return false;
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(TrimmedTarget);
	if (AssetName.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Target package path has no asset name: %s"), *TrimmedTarget);
		return false;
	}

	OutObjectPath = FString::Printf(TEXT("%s.%s"), *TrimmedTarget, *AssetName);
	return true;
}
```

Continue with `Summarize` and `WriteReport`:
```cpp
void FKatanaAssetMigrationRunner::Summarize(FKatanaAssetMigrationReport& Report)
{
	Report.Summary = FKatanaAssetMigrationSummary();
	Report.Summary.Targets = Report.Rows.Num();
	for (const FKatanaAssetMigrationRow& Row : Report.Rows)
	{
		switch (Row.Status)
		{
		case EKatanaAssetMigrationStatus::WouldChange: ++Report.Summary.WouldChange; break;
		case EKatanaAssetMigrationStatus::Changed: ++Report.Summary.Changed; break;
		case EKatanaAssetMigrationStatus::Saved: ++Report.Summary.Saved; break;
		case EKatanaAssetMigrationStatus::Failed: ++Report.Summary.Failed; break;
		case EKatanaAssetMigrationStatus::Unchanged: ++Report.Summary.Unchanged; break;
		}
	}
}

static TArray<TSharedPtr<FJsonValue>> ToJsonArray(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const FString& Value : Values)
	{
		Result.Add(MakeShared<FJsonValueString>(Value));
	}
	return Result;
}

bool FKatanaAssetMigrationRunner::WriteReport(const FKatanaAssetMigrationReport& Report, const FString& ReportPath, TArray<FString>& OutErrors)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema_version"), Report.SchemaVersion);
	Root->SetStringField(TEXT("operation"), Report.Operation);
	Root->SetStringField(TEXT("mode"), LexToString(Report.Mode));

	TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("targets"), Report.Summary.Targets);
	Summary->SetNumberField(TEXT("would_change"), Report.Summary.WouldChange);
	Summary->SetNumberField(TEXT("changed"), Report.Summary.Changed);
	Summary->SetNumberField(TEXT("unchanged"), Report.Summary.Unchanged);
	Summary->SetNumberField(TEXT("failed"), Report.Summary.Failed);
	Summary->SetNumberField(TEXT("saved"), Report.Summary.Saved);
	Root->SetObjectField(TEXT("summary"), Summary);

	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const FKatanaAssetMigrationRow& Row : Report.Rows)
	{
		TSharedRef<FJsonObject> RowObject = MakeShared<FJsonObject>();
		RowObject->SetStringField(TEXT("input_target"), Row.InputTarget);
		RowObject->SetStringField(TEXT("attack_data"), Row.AttackData);
		RowObject->SetStringField(TEXT("montage"), Row.Montage);
		RowObject->SetStringField(TEXT("section"), Row.Section);
		RowObject->SetStringField(TEXT("status"), LexToString(Row.Status));
		RowObject->SetArrayField(TEXT("legacy_notifies_found"), ToJsonArray(Row.LegacyNotifiesFound));
		RowObject->SetArrayField(TEXT("canonical_notifies_missing"), ToJsonArray(Row.CanonicalNotifiesMissing));
		RowObject->SetArrayField(TEXT("planned_removals"), ToJsonArray(Row.PlannedRemovals));
		RowObject->SetArrayField(TEXT("planned_additions"), ToJsonArray(Row.PlannedAdditions));
		RowObject->SetArrayField(TEXT("changed_packages"), ToJsonArray(Row.ChangedPackages));
		RowObject->SetArrayField(TEXT("saved_packages"), ToJsonArray(Row.SavedPackages));
		RowObject->SetArrayField(TEXT("warnings"), ToJsonArray(Row.Warnings));
		RowObject->SetArrayField(TEXT("errors"), ToJsonArray(Row.Errors));
		Rows.Add(MakeShared<FJsonValueObject>(RowObject));
	}
	Root->SetArrayField(TEXT("rows"), Rows);

	FString JsonText;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		OutErrors.Add(TEXT("Failed to serialize JSON report"));
		return false;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	if (!FFileHelper::SaveStringToFile(JsonText, *ReportPath))
	{
		OutErrors.Add(FString::Printf(TEXT("Failed to write report '%s'"), *ReportPath));
		return false;
	}

	return true;
}
```

- [ ] **Step 5: Run runner tests to verify GREEN**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration.Runner;Quit" -NullRHI -NoSplash -Unattended -nopause -stdout
```
Expected: runner tests pass.

---

### Task 5: Runner Target Loading, Dispatch, And Save Gate

**Files:**
- Modify: `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp`
- Modify: `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`

**Interfaces:**
- Consumes: `FKatanaAssetMigrationRunner::Run`
- Produces: target loading from `TargetsFile` and `AllowGlobalScan`
- Produces: package save attempt only for `ApplyAndSave` plus `AllowPackageSave`

- [ ] **Step 1: Add failing dispatch/report test**

Append the test body below the existing runner tests:
```cpp
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
	const EKatanaAssetMigrationExitCode ExitCode = Runner.Run(Options);
	TestEqual(TEXT("Invalid target should return row failure"), static_cast<int32>(ExitCode), static_cast<int32>(EKatanaAssetMigrationExitCode::RowFailure));

	FString ReportJson;
	TestTrue(TEXT("Failure report should be written"), FFileHelper::LoadFileToString(ReportJson, *ReportPath));
	TestTrue(TEXT("Report should name missing target"), ReportJson.Contains(TEXT("/Game/KatanaCombat/Missing/DA_Missing")));
	return true;
}
```

- [ ] **Step 2: Implement target loading**

In `KatanaAssetMigrationRunner.cpp`, add includes:
```cpp
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
```

Implement `LoadTargets`:
```cpp
bool FKatanaAssetMigrationRunner::LoadTargets(const FKatanaAssetMigrationOptions& Options, TArray<UAttackData*>& OutTargets, TArray<FKatanaAssetMigrationRow>& OutFailedRows) const
{
	TArray<FString> TargetStrings;
	if (!Options.TargetsFile.IsEmpty())
	{
		if (!FFileHelper::LoadFileToStringArray(TargetStrings, *Options.TargetsFile))
		{
			FKatanaAssetMigrationRow Row;
			Row.Status = EKatanaAssetMigrationStatus::Failed;
			Row.Errors.Add(FString::Printf(TEXT("Failed to read TargetsFile '%s'"), *Options.TargetsFile));
			OutFailedRows.Add(Row);
			return false;
		}
	}

	if (Options.bAllowGlobalScan)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().SearchAllAssets(true);
		TArray<FAssetData> AssetDataList;
		AssetRegistryModule.Get().GetAssetsByClass(UAttackData::StaticClass()->GetClassPathName(), AssetDataList, true);
		for (const FAssetData& AssetData : AssetDataList)
		{
			if (UAttackData* AttackData = Cast<UAttackData>(AssetData.GetAsset()))
			{
				OutTargets.AddUnique(AttackData);
			}
		}
	}

	for (FString TargetString : TargetStrings)
	{
		TargetString.TrimStartAndEndInline();
		if (TargetString.IsEmpty() || TargetString.StartsWith(TEXT("#")))
		{
			continue;
		}

		FString ObjectPath;
		FString Error;
		if (!NormalizeAttackDataTargetObjectPath(TargetString, ObjectPath, Error))
		{
			FKatanaAssetMigrationRow Row;
			Row.InputTarget = TargetString;
			Row.Status = EKatanaAssetMigrationStatus::Failed;
			Row.Errors.Add(Error);
			OutFailedRows.Add(Row);
			continue;
		}

		UObject* Object = StaticLoadObject(UAttackData::StaticClass(), nullptr, *ObjectPath);
		UAttackData* AttackData = Cast<UAttackData>(Object);
		if (!AttackData)
		{
			FKatanaAssetMigrationRow Row;
			Row.InputTarget = TargetString;
			Row.Status = EKatanaAssetMigrationStatus::Failed;
			Row.Errors.Add(FString::Printf(TEXT("Target did not load as UAttackData: %s"), *ObjectPath));
			OutFailedRows.Add(Row);
			continue;
		}

		OutTargets.AddUnique(AttackData);
	}

	return OutFailedRows.Num() == 0;
}
```

- [ ] **Step 3: Implement dispatch and save gate**

Add `RunAttackDataNotifyMigration`, `SaveChangedPackages`, and `Run`:
```cpp
bool FKatanaAssetMigrationRunner::RunAttackDataNotifyMigration(const FKatanaAssetMigrationOptions& Options, const TArray<UAttackData*>& Targets, FKatanaAssetMigrationReport& OutReport) const
{
	FAttackDataNotifyMigrationOperation Operation;
	OutReport.Operation = FAttackDataNotifyMigrationOperation::OperationName;
	OutReport.Mode = Options.Mode;

	for (UAttackData* Target : Targets)
	{
		FKatanaAssetMigrationRow Row;
		Operation.Run(Target, Options.Mode, Row);
		OutReport.Rows.Add(Row);
	}

	Summarize(OutReport);
	return OutReport.Summary.Failed == 0;
}

static void SnapshotInitiallyDirtyPackages(const TArray<UAttackData*>& Targets, TSet<FString>& OutDirtyPackages)
{
	for (const UAttackData* Target : Targets)
	{
		if (!Target)
		{
			continue;
		}

		if (const UPackage* AttackDataPackage = Target->GetOutermost())
		{
			if (AttackDataPackage->IsDirty())
			{
				OutDirtyPackages.Add(AttackDataPackage->GetName());
			}
		}

		if (Target->AttackMontage)
		{
			if (const UPackage* MontagePackage = Target->AttackMontage->GetOutermost())
			{
				if (MontagePackage->IsDirty())
				{
					OutDirtyPackages.Add(MontagePackage->GetName());
				}
			}
		}
	}
}

bool FKatanaAssetMigrationRunner::SaveChangedPackages(const FKatanaAssetMigrationOptions& Options, const TSet<FString>& InitiallyDirtyPackages, FKatanaAssetMigrationReport& Report) const
{
	if (Options.Mode != EKatanaAssetMigrationMode::ApplyAndSave)
	{
		return true;
	}

	if (!Options.bAllowPackageSave)
	{
		return false;
	}

	for (FKatanaAssetMigrationRow& Row : Report.Rows)
	{
		if (Row.Status != EKatanaAssetMigrationStatus::Changed)
		{
			continue;
		}

		for (const FString& PackageName : Row.ChangedPackages)
		{
			UPackage* Package = FindPackage(nullptr, *PackageName);
			if (!Package)
			{
				Row.Errors.Add(FString::Printf(TEXT("Changed package was not loaded: %s"), *PackageName));
				Row.Status = EKatanaAssetMigrationStatus::Failed;
				continue;
			}

			if (InitiallyDirtyPackages.Contains(PackageName) && !Options.bAllowDirtyPackages)
			{
				Row.Errors.Add(FString::Printf(TEXT("Package was dirty before migration and was refused without -AllowDirtyPackages: %s"), *PackageName));
				Row.Status = EKatanaAssetMigrationStatus::Failed;
				continue;
			}

			FString PackageFileName;
			if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, PackageFileName, FPackageName::GetAssetPackageExtension()))
			{
				Row.Errors.Add(FString::Printf(TEXT("Failed to resolve package filename: %s"), *PackageName));
				Row.Status = EKatanaAssetMigrationStatus::Failed;
				continue;
			}

			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs))
			{
				Row.Errors.Add(FString::Printf(TEXT("Failed to save package: %s"), *PackageName));
				Row.Status = EKatanaAssetMigrationStatus::Failed;
				continue;
			}

			Row.SavedPackages.Add(PackageName);
		}

		if (Row.Errors.Num() == 0 && Row.SavedPackages.Num() > 0)
		{
			Row.Status = EKatanaAssetMigrationStatus::Saved;
		}
	}

	Summarize(Report);
	return Report.Summary.Failed == 0;
}

EKatanaAssetMigrationExitCode FKatanaAssetMigrationRunner::Run(const FKatanaAssetMigrationOptions& Options)
{
	TArray<FString> Errors;
	if (!ValidateOptions(Options, Errors))
	{
		return EKatanaAssetMigrationExitCode::InvalidArguments;
	}

	TArray<UAttackData*> Targets;
	TArray<FKatanaAssetMigrationRow> FailedRows;
	LoadTargets(Options, Targets, FailedRows);
	TSet<FString> InitiallyDirtyPackages;
	SnapshotInitiallyDirtyPackages(Targets, InitiallyDirtyPackages);

	FKatanaAssetMigrationReport Report;
	Report.Operation = Options.Operation;
	Report.Mode = Options.Mode;
	Report.Rows.Append(FailedRows);

	if (Options.Operation.Equals(FAttackDataNotifyMigrationOperation::OperationName, ESearchCase::IgnoreCase))
	{
		FKatanaAssetMigrationReport OperationReport;
		RunAttackDataNotifyMigration(Options, Targets, OperationReport);
		Report.Rows.Append(OperationReport.Rows);
	}

	Summarize(Report);
	bool bSaveFailed = false;
	if (Report.Summary.Failed == 0 && Options.Mode == EKatanaAssetMigrationMode::ApplyAndSave)
	{
		bSaveFailed = !SaveChangedPackages(Options, InitiallyDirtyPackages, Report);
	}

	bool bReportFailed = false;
	if (!Options.ReportPath.IsEmpty())
	{
		TArray<FString> ReportErrors;
		if (!WriteReport(Report, Options.ReportPath, ReportErrors))
		{
			bReportFailed = true;
			for (const FString& ReportError : ReportErrors)
			{
				UE_LOG(LogTemp, Error, TEXT("%s"), *ReportError);
			}
		}
	}

	if (bSaveFailed)
	{
		return EKatanaAssetMigrationExitCode::SaveFailure;
	}

	return (Report.Summary.Failed > 0 || bReportFailed)
		? EKatanaAssetMigrationExitCode::RowFailure
		: EKatanaAssetMigrationExitCode::Success;
}
```

- [ ] **Step 4: Run runner suite**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration.Runner;Quit" -NullRHI -NoSplash -Unattended -nopause -stdout
```
Expected: runner tests pass.

---

### Task 6: Commandlet Shell

**Files:**
- Create: `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationCommandlet.h`
- Create: `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationCommandlet.cpp`

**Interfaces:**
- Consumes: `FKatanaAssetMigrationRunner`
- Produces: `UKatanaAssetMigrationCommandlet` runnable through `-run=KatanaAssetMigration`

- [ ] **Step 1: Add commandlet header**

Create `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationCommandlet.h`:
```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "KatanaAssetMigrationCommandlet.generated.h"

UCLASS()
class KATANACOMBATEDITOR_API UKatanaAssetMigrationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UKatanaAssetMigrationCommandlet();

	virtual int32 Main(const FString& Params) override;
};
```

- [ ] **Step 2: Add commandlet implementation**

Create `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationCommandlet.cpp`:
```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/KatanaAssetMigrationCommandlet.h"

#include "Commandlets/KatanaAssetMigrationRunner.h"

DEFINE_LOG_CATEGORY_STATIC(LogKatanaAssetMigrationCommandlet, Log, All);

UKatanaAssetMigrationCommandlet::UKatanaAssetMigrationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UKatanaAssetMigrationCommandlet::Main(const FString& Params)
{
	FKatanaAssetMigrationOptions Options;
	TArray<FString> Errors;
	if (!FKatanaAssetMigrationRunner::ParseOptions(Params, Options, Errors))
	{
		for (const FString& Error : Errors)
		{
			UE_LOG(LogKatanaAssetMigrationCommandlet, Error, TEXT("%s"), *Error);
		}
		return static_cast<int32>(EKatanaAssetMigrationExitCode::InvalidArguments);
	}

	FKatanaAssetMigrationRunner Runner;
	const EKatanaAssetMigrationExitCode Result = Runner.Run(Options);
	return static_cast<int32>(Result);
}
```

- [ ] **Step 3: Build commandlet**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -Progress -NoHotReload
```
Expected: exit code 0.

- [ ] **Step 4: Smoke invalid-argument exit code**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=NoSuchOperation -AllowGlobalScan -unattended -nopause -NullRHI -nosplash -stdout
```
Expected: process returns exit code `2` and logs `Unknown operation`.

---

### Task 7: Reuse Service From AttackDataTools

**Files:**
- Modify: `Source/KatanaCombatEditor/Private/AttackDataTools.cpp`
- Modify: `Source/KatanaCombatEditor/Public/AttackDataTools.h`

**Interfaces:**
- Consumes: `FAttackDataNotifyGenerationService`
- Preserves: `UAttackDataTools::GenerateAllNotifies(UAttackData*)`

- [ ] **Step 1: Refactor implementation**

In `AttackDataTools.cpp`, include the service:
```cpp
#include "AttackDataNotifyGenerationService.h"
```

Inside `GenerateAllNotifiesInternal`, replace direct legacy-removal plus `GenerateAttackPhaseNotifies` calls with:
```cpp
bool UAttackDataTools::GenerateAllNotifiesInternal(UAttackData* AttackData)
{
	if (!AttackData || !AttackData->AttackMontage)
	{
		return false;
	}

	const FAttackDataNotifyAnalysis Analysis =
		FAttackDataNotifyGenerationService::AnalyzeAttackDataNotifies(AttackData);
	const FAttackDataNotifyPlan Plan =
		FAttackDataNotifyGenerationService::BuildAttackDataNotifyPlan(Analysis);

	return FAttackDataNotifyGenerationService::ApplyAttackDataNotifyPlan(AttackData, Plan);
}
```

Keep `ValidateNotifyGenerationTiming`, transaction setup, snapshot restore, and logging in `GenerateAllNotifies`.

- [ ] **Step 2: Preserve helper surface unless removal is proven safe**

After refactor and build, keep direct/deprecated Blueprint helper behavior stable. Remove private helper declarations and definitions only when `rg` and the compiler both prove there are no call sites. Keep `GenerateAttackPhaseNotifies`, `GenerateHoldWindowStartNotify`, `ShouldGenerateHoldWindowStart`, `AddNotifyToMontage`, `AddNotifyStateToMontage`, and `RemoveNotifiesOfType` if any public/deprecated helper still depends on them.

- [ ] **Step 3: Run existing AttackData editor tool tests**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AttackDataTools;Quit" -NullRHI -NoSplash -Unattended -nopause -stdout
```
Expected: all `KatanaCombat.Editor.AttackDataTools` tests pass.

---

### Task 8: Docs, Operator Guide, And Final Verification

**Files:**
- Create: `docs/guides/HEADLESS_ASSET_MIGRATIONS.md`
- Modify: `AGENTS.md`
- Review: all touched files

**Interfaces:**
- Produces: operator instructions for audit, plan, apply, and apply-and-save.
- Produces: final evidence for source readiness without claiming asset migration is complete.

- [ ] **Step 1: Add operator guide**

Create `docs/guides/HEADLESS_ASSET_MIGRATIONS.md`:
```markdown
# Headless Asset Migrations

Use `KatanaAssetMigration` for repeatable editor-only asset migration passes. Start in `Audit` or `Plan` mode. Do not run `ApplyAndSave` unless the exact target list has been reviewed.

## AttackData Notify Migration

Audit all AttackData assets:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=Audit -AllowGlobalScan -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-audit.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Plan a reviewed target list:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=Plan -TargetsFile="Config/AssetMigrations/AttackDataNotifyTargets.txt" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-plan.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Apply in memory without saving:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=Apply -TargetsFile="Config/AssetMigrations/AttackDataNotifyTargets.txt" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-apply.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Apply and save only reviewed targets:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=ApplyAndSave -TargetsFile="Config/AssetMigrations/AttackDataNotifyTargets.txt" -AllowPackageSave -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-save.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Use `-AllowDirtyPackages` only after confirming the affected package was already dirty before the commandlet and the current owner accepts saving that pre-existing state.

The commandlet writes JSON rows with target, montage, section, status, planned removals, planned additions, changed packages, saved packages, warnings, and errors.
```

- [ ] **Step 2: Update `AGENTS.md`**

Add one bullet under `Codex Workflow`:
```markdown
- `docs/guides/HEADLESS_ASSET_MIGRATIONS.md`: commandlet workflow for audit/plan/apply asset migrations with explicit package-save gates.
```

- [ ] **Step 3: Run focused tests**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AssetMigration;Quit" -NullRHI -NoSplash -Unattended -nopause -stdout
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Editor.AttackDataTools;Quit" -NullRHI -NoSplash -Unattended -nopause -stdout
```
Expected: all focused tests pass.

- [ ] **Step 4: Run build and full automation baseline**

Run:
```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -Progress -NoHotReload
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat;Quit" -NullRHI -NoSplash -Unattended -nopause -stdout
powershell -NoProfile -ExecutionPolicy Bypass -File ".agents\skills\katana-verify\scripts\summarize-automation-log.ps1"
```
Expected: build exit code 0, automation summary has 0 failures/errors. Record warning count without claiming warnings were fixed.

- [ ] **Step 5: Run diff and stale-reference checks**

Run:
```powershell
git diff --check -- Source/KatanaCombatEditor Source/KatanaCombatTest AGENTS.md docs/guides docs/superpowers/specs docs/superpowers/plans
rg -n "T[B]D|T[O]DO|F[I]XME|similar[ ]to|fill[ ]in|appropriate[ ]error[ ]handling|implement[ ]later" docs/superpowers/plans/2026-06-22-headless-asset-migration-commandlet.md
git status --short -- Source/KatanaCombatEditor Source/KatanaCombatTest AGENTS.md docs/guides docs/superpowers/specs docs/superpowers/plans
```
Expected: no whitespace errors; no plan placeholder hits; status lists only intentional source/docs/test files in this feature scope.

---

## Adversarial Review Findings - 2026-06-22

- **Module boundary risk:** tests cannot rely on editor `Private/` headers. Runner and operation headers must live under `Source/KatanaCombatEditor/Public/Commandlets/` and exported classes must use `KATANACOMBATEDITOR_API`.
- **False save failure risk:** checking `Package->IsDirty()` after apply would reject every successfully migrated package. Snapshot dirty state before apply and reject only packages that were already dirty before the commandlet, unless `-AllowDirtyPackages` is present.
- **Plan evidence risk:** a `Plan` row without `ChangedPackages` does not satisfy the spec. Operation rows must report the montage package that would change before returning `WouldChange`.
- **Target resolution risk:** `TargetsFile` package paths would fail if the loader only used `StaticLoadObject` on raw input. Normalize package paths to object paths and test the normalization path.
- **Asset registry risk:** global scans in commandlets can return incomplete results unless the registry is fully searched first. Call `SearchAllAssets(true)` before `GetAssetsByClass`.
- **Timing mutation risk:** current generation service must fail invalid manual timing rather than silently auto-calculating values. `-AllowTimingMutation` is reserved for future compatibility and rejected by this operation.
- **Report reliability risk:** ignoring `WriteReport` failures creates false green commandlet exits with no durable evidence. Treat report write failure as a nonzero `RowFailure` exit.
- **Operator safety risk:** documenting `-AllowDirtyPackages` in the routine save command normalizes an override. Keep the normal `ApplyAndSave` command to `-AllowPackageSave` only.
- **Helper removal risk:** removing AttackData tool helpers during the service refactor could break deprecated Blueprint-callable paths. Preserve helper surface unless direct call-site checks and tests prove removal is safe.

---

## Acceptance Criteria

- `KatanaCombat.Editor.AssetMigration` automation tests pass.
- Existing `KatanaCombat.Editor.AttackDataTools` tests still pass.
- `KatanaCombatEditor Win64 Development` builds.
- Full `Automation RunTests KatanaCombat;Quit` completes with zero failures/errors.
- `KatanaAssetMigration` rejects unknown operations with exit code `2`.
- `ApplyAndSave` is refused without `-AllowPackageSave`.
- `Plan` mode does not mutate transient montage notifies.
- `Apply` mutates only the configured target section in transient tests.
- Target normalization accepts both `/Game/Path/Asset.Asset` and `/Game/Path/Asset`.
- Plan-mode rows include `changed_packages` for assets that would change.
- Packages dirty before apply are refused without `-AllowDirtyPackages`; packages dirtied by the commandlet are eligible for save when `-AllowPackageSave` is present.
- Report write failure produces a nonzero exit instead of a false success.
- Invalid manual timing fails analysis and does not trigger auto-calculation in the headless operation.
- JSON report includes schema version, operation, mode, summary, and required row fields.
- No binary assets under `Content/` are modified by implementation tests.

## Self-Review Checklist

- Spec coverage: tasks cover shared commandlet types, pure migration service, operation dispatch, runner safety gates, report schema, commandlet shell, existing UI reuse, docs, and verification.
- Type consistency: task names consistently use `FKatanaAssetMigrationRunner`, `FAttackDataNotifyGenerationService`, and `FAttackDataNotifyMigrationOperation`.
- Scope boundary: no task writes or saves `Content/` assets.
- Residual risk: target resolution may need adjustment after first real audit against project assets; that adjustment must remain in commandlet/runner code and must not loosen package-save gates.
