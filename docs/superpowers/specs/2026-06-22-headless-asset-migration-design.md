# Headless Asset Migration Design

## Status

Approved design for implementation planning. This spec does not authorize asset writes by itself; `Content/` packages remain untouched until an apply-and-save command is explicitly run against named targets.

## Problem

The AttackData notify canon cleanup made source and tests align around phase-transition notifies, event-driven hold starts, and no default combo/hit-toggle/hold-state seeding. A string audit still found older montage assets with legacy notify references. Migrating those assets manually in the editor is possible, but it is hard to repeat, hard to audit, and risky in a workspace with large unrelated `Content/` WIP.

We need a headless, report-driven migration path that can first audit and plan changes, then optionally apply and save only explicitly targeted packages. The first migration operation is AttackData notify canon migration, but the design should support later asset migrations without building one-off commandlets each time.

## Goals

- Add a reusable editor-module commandlet entrypoint for asset migrations.
- Keep operation logic testable outside command-line parsing and package saving.
- Support read-only audit and plan modes before any mutation.
- Require explicit safety flags for package saves and broad scans.
- Produce durable JSON reports suitable for code review, CI, and future agents.
- Implement AttackData notify migration as the first concrete operation.

## Non-Goals

- No default global rewrite of `Content/`.
- No binary asset saves during unit tests.
- No deletion of legacy C++ notify classes.
- No runtime combat behavior changes.
- No Git-status enforcement inside Unreal; Git preflight belongs in an external wrapper or human review.
- No editor UI replacement in this pass.

## Architecture

Use two layers.

The pure migration layer analyzes loaded assets and optionally applies a prepared plan. It has no command-line parsing, no package saving, no source-control assumptions, and no modal UI. For AttackData, this becomes `FAttackDataNotifyGenerationService` with methods equivalent to:

```cpp
FAttackDataNotifyAnalysis AnalyzeAttackDataNotifies(const UAttackData* AttackData);
FAttackDataNotifyPlan BuildAttackDataNotifyPlan(const FAttackDataNotifyAnalysis& Analysis);
bool ApplyAttackDataNotifyPlan(UAttackData* AttackData, const FAttackDataNotifyPlan& Plan);
```

The commandlet layer handles options, target resolution, safety gates, package loading, operation dispatch, report writing, and package saving. It should be thin enough that most behavior is covered by automation tests on the service and runner.

## Proposed Files

- `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationTypes.h`
  Shared enums and structs for mode, status, target, row result, and summary.
- `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationCommandlet.cpp`
  `UCommandlet` entrypoint for `-run=KatanaAssetMigration`.
- `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.h/.cpp`
  Option parsing, validation, target loading, operation dispatch, report writing, and save orchestration.
- `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.h/.cpp`
  First concrete operation.
- `Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.h/.cpp`
  Analyze, plan, and apply logic shared by commandlet and future editor UI wiring.

`UAttackDataTools::GenerateAllNotifies` can later delegate to the service, but the commandlet should not blindly call the current UI-facing helper because audit and plan modes must be truly read-only.

## Command Shape

```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" `
  -run=KatanaAssetMigration `
  -Operation=AttackDataNotifyMigration `
  -Mode=Audit `
  -TargetsFile=Config/AssetMigrations/AttackDataNotifyTargets.txt `
  -ReportPath=Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-report.json `
  -unattended -nopause -NullRHI -nosplash -stdout
```

Default mode is `Audit`. `TargetsFile` entries are object paths or package paths for `UAttackData` assets. A later implementation may support direct `-Target=` values, but file-based target lists are the default because they are reviewable.

## Modes

- `Audit`: load targets, report current state, do not compute a mutation plan as authoritative work.
- `Plan`: compute exact removals, additions, changed packages, and warnings without mutating packages.
- `Apply`: apply the exact plan in memory but do not save packages.
- `ApplyAndSave`: apply the exact plan and save changed target packages.

## Safety Rules

- Explicit targets are required unless `-AllowGlobalScan` is present.
- `ApplyAndSave` requires `-AllowPackageSave`.
- The runner may save only packages named by the operation result as changed packages.
- Dirty loaded packages are rejected unless `-AllowDirtyPackages` is present.
- AttackData timing is read as-is. `AttackDataNotifyMigration` must not auto-calculate or rewrite timing; `-AllowTimingMutation` is reserved for a future explicit timing operation and is rejected here.
- Plan/apply behavior must be deterministic: apply uses the same planned removals and additions reported by plan mode.
- Operation failures should produce row-level errors and a nonzero exit code without continuing into package saves for failed rows.

## AttackData Notify Migration Rules

For each target `UAttackData`, inspect only its configured montage section. The operation may remove these legacy default-seeding notifies from that section:

- `UAnimNotifyState_AttackPhase`
- `UAnimNotify_ToggleHitDetection`
- `UAnimNotifyState_HoldWindow`
- `UAnimNotifyState_ComboWindow`

The operation must preserve parry windows, counter windows, action-window base-derived specialized notifies, paired sync notifies, paired collision notifies, and unrelated sections.

Planned additions are:

- `UAnimNotify_AttackPhaseTransition` to `Active` at end of windup.
- `UAnimNotify_AttackPhaseTransition` to `Recovery` at end of active.
- `UAnimNotify_HoldWindowStart` when the current canonical hold-generation rules apply.

Existing canonical default notifies are valid only when their type, time, and configured values match the current AttackData-derived defaults. Stale or duplicate canonical defaults are removed and reseeded just like legacy default notifies.

If timing is invalid, section lookup fails, or required assets are missing, the row fails without mutating notifies. If an asset has a hand-authored explicit combo window that should survive, it must be excluded from the target list for this migration pass or handled by a future explicit override rule.

## Report Schema

Reports are JSON with `schema_version: 1`.

```json
{
  "schema_version": 1,
  "operation": "AttackDataNotifyMigration",
  "mode": "Plan",
  "summary": {
    "targets": 5,
    "would_change": 5,
    "changed": 0,
    "unchanged": 0,
    "failed": 0,
    "saved": 0
  },
  "rows": []
}
```

Each row includes `input_target`, `attack_data`, `montage`, `section`, `status`, `legacy_notifies_found`, `stale_canonical_notifies_found`, `canonical_notifies_missing`, `planned_removals`, `planned_additions`, `changed_packages`, `saved_packages`, `warnings`, and `errors`.

## Exit Codes

- `0`: all rows completed for the selected mode.
- `1`: one or more target rows failed.
- `2`: invalid arguments or safety gate refusal.
- `3`: package save failure after successful mutation.

## Test Strategy

Add focused automation coverage before implementation logic is trusted:

- Parser defaults to `Audit`.
- `ApplyAndSave` without `-AllowPackageSave` fails with exit code `2`.
- Unknown operation fails with exit code `2`.
- Plan mode does not mutate transient montage notifies.
- Apply mode mutates only planned target-section notifies.
- AttackData migration refuses timing mutation unless explicitly enabled.
- Report JSON contains the required top-level fields and row fields.
- Save orchestration never attempts to save a package outside operation-reported changed packages.

The verification ladder for implementation is: build `KatanaCombatEditor`, run focused commandlet/service automation, run `KatanaCombat.Editor.AttackDataTools` to guard existing UI behavior, then run the standard KatanaCombat automation baseline.

## Future Extension

New migrations should add an operation class and service, then register the operation name with the runner. The framework should stay migration-focused: deterministic target resolution, explicit modes, safety gates, and durable reports. It should not grow into a broad arbitrary asset command executor.

## Self-Review

- No placeholder requirements remain.
- The first operation is concrete and scoped to AttackData notify migration.
- Read-only modes are explicitly non-mutating.
- Asset saves require both explicit mode and explicit save permission.
- The design separates testable migration logic from commandlet plumbing.
- The main residual risk is intentional explicit combo-window notifies; the spec handles this by requiring target-list exclusion unless a future override rule is designed.
