# Editor Asset Operation Workflow Design

## Status

Design approved section by section (2026-09-30), revised after an independent review.
Pending review of this written spec. Part of step 3 of the combat cleanup; see the delivery
order in [2026-09-30-combat-feel-and-attack-reach-design.md](2026-09-30-combat-feel-and-attack-reach-design.md).
It ships in two PRs: **C1** before the attack reach bake (3b) and **C2** after it. It
extends, and supersedes where they conflict, the framework decisions in
[2026-06-22-headless-asset-migration-design.md](2026-06-22-headless-asset-migration-design.md).
This spec does not authorize asset saves.

## Problem

Asset-changing editor actions have grown without a shared framework:

- **Eight headless operations** in `KatanaAssetMigration` share option, row and report
  types but no interface: instance `Run` methods with five different signatures, a string
  if-chain dispatcher in `FKatanaAssetMigrationRunner::Run`, per-operation branches in
  `ValidateOptions`, a post-save audit block copy-pasted three times, and **four fingerprint
  schemes** (the shared `FKatanaAssetAuthoringApprovalService`, a private copy in
  `DefenseProofAuthoring`, `DefenseProofMigration`'s plan fingerprint, and
  `AttackDataNotifyMigration`'s canonical-report hash). The shared service depends on
  `FDefenseProofMigrationOperation::CanonicalizeJson`, the wrong direction.
- **Three of the eight can save without an approved plan** (`AttackDataTimingMigration`,
  `CounterChainProofMigration`, `EnemyAIProofAssets`).
- **Details-panel buttons re-implement operation logic.** "Generate AnimNotifies" forces
  regeneration and dirties the montage on every click, silently rewrites invalid timing, and
  has no plan, preview or dirty gate. "Auto-Calculate Timing" edits without a transaction.
  The three section dropdowns write fields without undo.
- **Eight separate `UAttackData` validators**, and deprecated notify generators still
  reachable from Blueprint.

## Goals

- One operation contract for every asset-changing editor action, with identical behavior
  headless and in the editor.
- One approval and fingerprint implementation; **every save requires an approved plan**.
- A reusable details-panel button and Content Browser action, so no customization writes its
  own transaction, preview or reporting code.
- Migrate all eight operations and the asset-changing buttons; remove duplicate and
  deprecated paths.

## Non-Goals

- Navigation or preview-only UI (Open Montage Editor, refresh buttons, timeline transport,
  preview checkboxes, orientation presets, tab and menu registration).
- The Paired Animation Preview's in-widget analysis (a follow-up for the analysis suite).
- Console commands and the `PairedAnimationEvaluation` commandlet.
- Source-control integration.

## Intended Breaks

Stated explicitly so they are reviewed, not discovered:

- **Every existing approval fingerprint value changes.** The descriptor `Version` joins every
  fingerprint and the four schemes merge into one. Prior plan reports cannot approve an
  apply; regenerate plans after C1. Pre/post-migration comparisons compare plan rows, not
  fingerprints.
- **The three approval-free operations now require an approved plan to save.** Their
  command examples in `docs/guides/HEADLESS_ASSET_MIGRATIONS.md` (the timing, counter-chain
  and enemy-AI sections) and `docs/plans/counter-chain-rollout-inventory.md` are updated.
  No script, skill or agent tooling invokes them.
- **Tests that call moved internals are updated**: `KatanaAssetMigrationTests` calls to
  `ComputePlanFingerprint`, `CanonicalizeJson`, `FinalizePlanReportFingerprint` and
  `ComputeApprovalFingerprint`, and the test pinning the "requires -AllowPackageSave" error
  text.
- **Button behavior tests are rewritten**: `AttackDataEditorToolsTests`
  `ReseedsExistingCanonicalTiming` and `RestoresAutoTimingOnFailure` pin the regenerate and
  silent-timing behavior this spec removes.

## Architecture (C1 and C2)

### Operation contract (C1)

`IKatanaAssetOperation` (editor module, plain C++ interface, registered by name):

```cpp
struct FKatanaAssetOperationDescriptor
{
    FName Name;                          // "AttackDataNotifyMigration"
    int32 Version = 1;                   // bumped when behavior changes; part of every fingerprint
    FText DisplayName;
    FText Description;
    EKatanaAssetOperationTargetKind TargetKind;   // SelectedObjects, TargetList, FixedRecipe, Manifest
    TSubclassOf<UObject> TargetClass;    // for SelectedObjects/TargetList
    bool bSupportsGlobalScan = false;
    bool bMutatesAssets = true;          // false = read-only (audit/report operations)
    bool bCreatesAssetsOrEditsMaps = false; // Undo cannot remove created packages or all map edits
    bool bLoadsMaps = false;             // Preflight/Plan load a map (editor must not switch levels)
    EKatanaAssetOperationEntryPoints EntryPoints; // flags: Headless, DetailsButton, ContentBrowser
    TArray<FKatanaAssetOperationParameter> Parameters;  // named, typed, with defaults
};

class IKatanaAssetOperation
{
public:
    virtual ~IKatanaAssetOperation() = default;
    virtual const FKatanaAssetOperationDescriptor& GetDescriptor() const = 0;
    virtual FKatanaAssetOperationDiagnostics Preflight(const FKatanaAssetOperationRequest&) const = 0;
    virtual FKatanaAssetOperationPlan BuildPlan(const FKatanaAssetOperationRequest&) const = 0;
    virtual bool Apply(const FKatanaAssetOperationPlan&, FKatanaAssetOperationApplyContext&) const = 0;
    virtual bool VerifyAfterApply(const FKatanaAssetOperationPlan&, FKatanaAssetOperationDiagnostics&) const = 0;
};
```

- **Preflight** is read-only: per-target diagnostics and whether anything would change. It
  is Audit mode headless and drives a button's enabled state in the editor.
- **Plan** is read-only: the change list (typed rows), package ledger and the **approval
  contract** (canonical inputs and planned changes the fingerprint hashes).
- **Apply** mutates in memory only, calling `Modify()` on everything it changes. It never
  saves.
- **VerifyAfterApply** re-runs the operation's audit against the in-memory result and must
  report no remaining changes. Headless it also runs after save, replacing the three
  copy-pasted post-save audit blocks; in the editor it runs after Apply.

### Registry (C1)

`FKatanaAssetOperationRegistry` holds operations by name; they register in
`FKatanaCombatEditorModule::StartupModule`. The runner's if-chain and per-operation
`ValidateOptions` branches become descriptor-driven validation. Duplicate names are
rejected.

### Approval and fingerprints (C1)

`FKatanaAssetAuthoringApprovalService` becomes the only implementation. `CanonicalizeJson`
moves into it. The fingerprint is SHA-1 over the canonical approval contract, the descriptor
`Version`, and package-state hashes (`BuildPackageStateHash`). Each migrated operation's
contract binds the same facts its old scheme bound (regression-tested per operation).

**Rule: every save requires an approved plan**, recomputed and matched at apply time. Drift
of any bound input rejects the apply before mutation.

### Headless context (C1)

- Existing options keep their meaning (`-Operation`, `-Mode`, `-TargetsFile`, `-ReportPath`,
  `-ApprovedPlanReport`, `-ApprovedPlanFingerprint`, `-AllowPackageSave`,
  `-AllowDirtyPackages`, `-AllowGlobalScan`, `-AllowTimingMutation`); modes keep
  `EKatanaAssetMigrationMode { Audit, Plan, Apply, ApplyAndSave }`.
- Snapshot rollback, `SaveChangedPackages` (preflight, backups, rollback, reload), the
  stricter on-disk package hash, dirty-package rejection and the JSON report are unchanged.

### Report model (C1)

`FKatanaAssetMigrationReport` stays the serialized format, and `FKatanaAssetMigrationRow`
keeps its fields; new detail goes into the existing `Details` map.

### Editor context (C2)

- Mutating operations are unavailable during Play In Editor (Preflight says why).
- **Preflight results are cached per (operation, targets, parameters)** and invalidated on
  property change or package dirty-state change, so Slate attributes never run Preflight per
  frame.
- A click runs Plan and opens a **change preview** (targets, per-row changes, package
  ledger, warnings). Confirming keeps the plan and its fingerprint in memory; Apply
  recomputes it and re-opens the preview on drift.
- Apply runs inside **one `FScopedTransaction`** for the whole batch (one undo step),
  followed by `VerifyAfterApply`. Packages are left dirty; the user saves.
- **Cancel:** a cancelled or failed batch ends its transaction and immediately undoes it.
  `FScopedTransaction::Cancel` does not restore objects (`UTransBuffer::Cancel` only discards
  the record), so it is never used for rollback.
- Targets may have unsaved edits: the editor fingerprint binds the in-memory approval
  contract instead of on-disk bytes. Headless keeps the on-disk hash.
- Operations with `bCreatesAssetsOrEditsMaps` run in the editor with a prominent preview
  warning that Undo does not remove created assets or revert all map edits, listing the
  affected packages. Operations with `bLoadsMaps` never switch the user's open level: they
  run in the editor only when the target map is the open map, otherwise Preflight directs the
  user to run them headless.
- Progress and cancel use a shared `FScopedSlowTask` wrapper that operations report into per
  target.
- Results go to a toast with a "Show details" link to a `KatanaAssetOperations` Message Log
  page, and the same JSON report is written to
  `Saved/Logs/AssetOperations/<operation>-<timestamp>.json`.

### Reusable editor UI (C2)

- **`SAssetOperationButton`**: constructed with an operation name, a target provider and
  optional fixed parameters. It owns enabled state (cached Preflight), preview,
  confirmation, transaction, progress and reporting.
- **Content Browser actions**: descriptors whose `EntryPoints` include `ContentBrowser`
  register a context-menu entry for their `TargetClass` through `UToolMenus`, running on the
  whole selection with one preview and one transaction. `ToolMenus` is added to the editor
  module's dependencies.
- **Multi-selection**: `FAttackDataCustomization` currently returns early for more than one
  object; operation buttons pass all customized objects as targets.

## C1 Scope

- Contract, registry, unified approval service, headless context.
- All eight operations as adapters over their existing bodies. Their instance `Run` methods
  remain as thin facades where tests still call them.
- `AttackDataTimingMigration` gains a `Strategy` parameter: `ClampRecovery` (today) or
  `RecalculateFromTypeDefaults` (the Auto-Calculate behavior, moved from
  `UAttackDataTools::AutoCalculateTiming`).
- `AttackDataNotifyMigration` gains an explicit `Regenerate` parameter (default false).
- Approval for the three operations that lacked it; `EnemyAIProofAssets` gets its first
  tests.
- The documentation and test updates listed under Intended Breaks.

## C2 Scope

- Editor context, `SAssetOperationButton`, Content Browser actions.
- `FAttackDataCustomization`:

| Today | After |
| --- | --- |
| Generate AnimNotifies | Operation button (`AttackDataNotifyMigration`). No forced regeneration; invalid timing is a Preflight error pointing to the timing operation. |
| Auto-Calculate Timing | Operation button (`AttackDataTimingMigration`, `Strategy=RecalculateFromTypeDefaults`). Undoable. |
| Validate | Preflight of the notify operation, shown in the Message Log. |
| Section dropdowns ×3 | Not operations; write through `IPropertyHandle::SetValue` (undoable). |
| Open Montage Editor, refresh ×3 | Unchanged (navigation). |

- The reach bake (3b) gains its details-panel button and Content Browser entry.
- **Validator consolidation.** Two sources of truth: the notify service's analysis (notify
  and phase timing, editor module) and `UAttackData::IsDataValid` (data rules, runtime
  module). `HasValidNotifyTimingInSection` lives in the runtime module and cannot call the
  editor service, so the section-timing check it shares with the service moves into a
  runtime-module pure helper both call. The Timing operation's checks call the service.
  `ValidateNotifyGenerationTiming` (private) is replaced by the service analysis.
  `UAttackDataTools::ValidateMontageSection` and `ValidateAttackData` (Blueprint-callable)
  become thin wrappers when a C++ caller or Content reference exists (binary Content scan),
  otherwise they are removed.
- **Removals**: `GenerateAttackPhaseNotifies`, `GenerateHitDetectionNotifies`,
  `GenerateComboWindowNotify`, `BatchGenerateNotifies` (Blueprint-callable; removed after a
  binary Content scan finds no callers), the private helpers `AddNotifyToMontage` and
  `RemoveNotifiesOfType`, and the dead `OnPreviewTimelineClicked`.

## Testing

- **C1**: registry (registration, lookup, duplicates, descriptor validation); approval
  (each migrated operation's contract binds the same facts as before; drift rejects;
  unapproved save rejected for every mutating operation); adapters produce the same plan
  rows as the pre-migration operations for their documented targets.
- **C2**: editor context without UI: one transaction per batch; undo restores every target;
  cancel ends and undoes the transaction; nothing saved; PIE gating; `bLoadsMaps` refusal;
  Preflight caching invalidation; parity for saved packages (same plan rows headless and in
  the editor). The project has no automation test using `FScopedTransaction`; these tests
  use the editor transaction buffer (`GEditor->Trans`) directly, and manual verification is
  recorded if the automation environment cannot host them.
- Button behavior: the notify button no longer dirties an unchanged montage; invalid timing
  is reported, not rewritten.

## Verification

- Full baseline green for each PR.
- C1 headless: `Audit` and `Plan` for every operation against its documented targets produce
  the same rows as before migration (rows compared, fingerprints excluded).
- C2 editor, manual: each migrated button previews, applies, undoes and redoes; a Content
  Browser multi-selection runs one preview and one undo step; a cancelled batch leaves no
  edits.

## Follow-Ups

- Move the Paired Animation Preview's in-widget analysis into the analysis suite (dead
  duplicate optimizers, duplicate distance and rotation buttons, non-cancellable slow task,
  unreachable undo and redo, silent `hand_r` weapon fallback).
- Replace the flat `FKatanaAssetMigrationRow` with a base row plus typed payloads.
- Scripting access (Blueprint/Python entry point) over the same registry.
