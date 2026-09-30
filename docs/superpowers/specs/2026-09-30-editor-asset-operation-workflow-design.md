# Editor Asset Operation Workflow Design

## Status

Approved design sections (2026-09-30), pending review of this written spec. Part of step 3
of the combat cleanup; it ships between PR 3a and PR 3b of
[2026-09-30-combat-feel-and-attack-reach-design.md](2026-09-30-combat-feel-and-attack-reach-design.md),
whose attack reach bake is this workflow's first new client. It extends, and supersedes where
they conflict, the framework decisions in
[2026-06-22-headless-asset-migration-design.md](2026-06-22-headless-asset-migration-design.md).
This spec does not authorize asset saves.

## Problem

Asset-changing editor actions have grown without a shared framework:

- **Eight headless operations** in `KatanaAssetMigration` share options, row and report
  types but no interface. They have eight different `Run` signatures, a string if-chain
  dispatcher in `FKatanaAssetMigrationRunner::Run`, per-operation branches in
  `ValidateOptions`, a post-save audit block copy-pasted three times, and **four fingerprint
  schemes** (the shared `FKatanaAssetAuthoringApprovalService`, a private copy in
  `DefenseProofAuthoring`, `DefenseProofMigration`'s own plan fingerprint, and
  `AttackDataNotifyMigration`'s canonical-report hash). The shared service depends on
  `FDefenseProofMigrationOperation::CanonicalizeJson`, the wrong direction.
- **Three of the eight can save without an approved plan** (`AttackDataTimingMigration`,
  `CounterChainProofMigration`, `EnemyAIProofAssets`).
- **Details-panel buttons re-implement operation logic.** `FAttackDataCustomization`'s
  "Generate AnimNotifies" shares the notify service with the headless operation but
  diverges: it forces regeneration and dirties the montage on every click, silently rewrites
  timing when it is invalid, and has no plan, preview or dirty gate. "Auto-Calculate
  Timing" edits without a transaction (not undoable) and has semantics unrelated to the
  headless timing operation. The three section dropdowns write fields without undo.
- **Eight separate `UAttackData` validators**, and three deprecated notify generators
  (`GenerateAttackPhaseNotifies`, `GenerateHitDetectionNotifies`,
  `GenerateComboWindowNotify`) plus `BatchGenerateNotifies`, still reachable from Blueprint.

Every new bake or authoring action (the first is the attack reach bake) would otherwise add
another copy of targets, validation, transactions, approval and reporting.

## Goals

- One operation contract for every asset-changing editor action, with identical behavior
  headless and in the editor.
- One approval and fingerprint implementation; **every save requires an approved plan**.
- A reusable details-panel button and Content Browser action so no customization writes its
  own transaction, preview or reporting code.
- Migrate all eight operations and the asset-changing buttons; remove duplicate and
  deprecated paths.
- Existing headless command lines and the existing migration tests keep working.

## Non-Goals

- Navigation or preview-only UI (Open Montage Editor, refresh buttons, timeline transport,
  preview checkboxes, orientation presets, tab and menu registration).
- The Paired Animation Preview's analysis logic (about 2,000 lines inside the widget). It
  belongs to the animation analysis suite migration and is recorded as a follow-up.
- Console commands (`Combat.Capture.*`, telemetry dumps) and the
  `PairedAnimationEvaluation` commandlet, which already shares one evaluation API with its
  button.
- Source-control integration.

## Architecture

### Operation contract

`IKatanaAssetOperation` (editor module, plain C++ interface, registered by name):

```cpp
struct FKatanaAssetOperationDescriptor
{
    FName Name;                          // "AttackDataNotifyMigration"
    int32 Version = 1;                   // bumped when behavior changes; part of every fingerprint
    FText DisplayName;                   // "Generate Phase Notifies"
    FText Description;
    EKatanaAssetOperationTargetKind TargetKind;   // SelectedObjects, TargetList, FixedRecipe, Manifest
    TSubclassOf<UObject> TargetClass;    // for SelectedObjects/TargetList; enables Content Browser entry
    bool bSupportsGlobalScan = false;
    bool bMutatesAssets = true;          // false = read-only (audit/report operations)
    bool bCreatesAssetsOrEditsMaps = false; // Undo cannot remove created packages or all map edits
    EKatanaAssetOperationEntryPoints EntryPoints; // flags: Headless, DetailsButton, ContentBrowser
    TArray<FKatanaAssetOperationParameter> Parameters;  // named, typed, with defaults (e.g. Strategy, Regenerate)
};

class IKatanaAssetOperation
{
public:
    virtual ~IKatanaAssetOperation() = default;
    virtual const FKatanaAssetOperationDescriptor& GetDescriptor() const = 0;
    virtual FKatanaAssetOperationDiagnostics Preflight(const FKatanaAssetOperationRequest&) const = 0;
    virtual FKatanaAssetOperationPlan BuildPlan(const FKatanaAssetOperationRequest&) const = 0;
    virtual bool Apply(const FKatanaAssetOperationPlan&, FKatanaAssetOperationApplyContext&) const = 0;
    virtual bool VerifyAfterSave(const FKatanaAssetOperationPlan&, FKatanaAssetOperationDiagnostics&) const { return true; }
};
```

- **Request:** resolved targets (objects or paths, or the fixed recipe or manifest), mode,
  parameter values.
- **Preflight** is read-only. It returns per-target diagnostics (errors, warnings, info) and
  whether anything would change. It is the Audit mode headless, and drives a button's enabled
  state and tooltip in the editor.
- **Plan** is read-only. It returns the concrete change list (typed rows), the package
  ledger and the **approval contract**: the canonical description of every input and planned
  change that the fingerprint hashes.
- **Apply** mutates in memory only. It calls `Modify()` on everything it changes and never
  saves. Saving belongs to the execution context.
- **VerifyAfterSave** re-runs the operation's audit after a save and must report no changes;
  it replaces the three copy-pasted post-save audit blocks.

### Registry

`FKatanaAssetOperationRegistry` holds operation instances by name. Operations register in
`FKatanaCombatEditorModule::StartupModule`. The commandlet, buttons and Content Browser
entries resolve operations by name. The runner's if-chain and per-operation `ValidateOptions`
branches are replaced by descriptor-driven validation (target kind, supported parameters,
save permission).

### Approval and fingerprints

`FKatanaAssetAuthoringApprovalService` becomes the single implementation:

- `CanonicalizeJson` moves into the service from `FDefenseProofMigrationOperation`.
- The fingerprint is SHA-1 over the canonical approval contract plus package-state hashes
  (existing `BuildPackageStateHash`, which rejects dirty packages where the policy says so).
- The private copies in `DefenseProofAuthoring`, `DefenseProofMigration`'s
  `ComputePlanFingerprint` and `AttackDataNotifyMigration`'s report hash are removed. Their
  contracts are expressed as approval contracts, so each operation's fingerprint still binds
  the same facts it binds today.

**Rule: every save requires an approved plan.** An apply that will save is accepted only with
a plan whose fingerprint is recomputed and matches. Any drift (asset edited, targets changed,
recipe or code changed) rejects the apply before mutation.

### Execution contexts

Both contexts run the same operation code; only the surroundings differ.

**Headless (`KatanaAssetMigration` commandlet):**

- Existing options (`-Operation`, `-Mode`, `-TargetsFile`, `-ReportPath`,
  `-ApprovedPlanReport`, `-ApprovedPlanFingerprint`, `-AllowPackageSave`,
  `-AllowDirtyPackages`, `-AllowGlobalScan`) keep their meaning. Modes keep
  `EKatanaAssetMigrationMode { Audit, Plan, Apply, ApplyAndSave }`.
- `Apply` and `ApplyAndSave` now require an approved plan for **all** operations.
  `AttackDataTimingMigration`, `CounterChainProofMigration` and `EnemyAIProofAssets` gain the
  plan step; `HEADLESS_ASSET_MIGRATIONS.md` is updated.
- Snapshot rollback, the existing `SaveChangedPackages` (preflight, backups, rollback,
  reload) and the JSON report are unchanged.

**Editor:**

- Mutating operations are unavailable during Play In Editor; Preflight reports why.
- Apply runs inside **one `FScopedTransaction`** for the whole batch, so a batch is one undo
  step. Packages are left dirty; nothing is saved. The user saves normally.
- Operations with `bCreatesAssetsOrEditsMaps` are allowed in the editor, but their preview
  shows a prominent warning that Undo does not remove created assets or revert all map edits,
  and lists the packages that will be created or touched.
- Targets may have unsaved edits: in the editor, the fingerprint binds the in-memory approval
  contract (the operation's canonical inputs) instead of on-disk package bytes, so an
  operation can run on an asset being edited. Headless keeps the stricter on-disk hash and
  dirty-package rejection.
- Approval is interactive: a click runs Plan and opens a **change preview** dialog (targets,
  per-row changes, package ledger, warnings). Confirming keeps the plan and its fingerprint
  in memory; Apply recomputes it, and on drift re-opens the preview instead of applying.
- Progress and cancel use a shared `FScopedSlowTask` wrapper that operations report into
  per target; cancel stops before the next target. Because the transaction wraps the batch,
  a cancelled batch is rolled back.
- Results go to a toast notification with a "Show details" link to a `KatanaAssetOperations`
  Message Log page containing the same rows as the headless report. Every editor run also
  writes the same JSON report to `Saved/Logs/AssetOperations/<operation>-<timestamp>.json`,
  so editor changes leave the same record as headless ones.

### Report model

`FKatanaAssetMigrationReport` stays the serialized format. `FKatanaAssetMigrationRow` keeps
its existing fields for compatibility. New operations add typed detail through the existing
`Details` map rather than new row fields. The editor Message Log renders the same rows.

### Reusable editor UI

- **`SAssetOperationButton`** (Slate widget): constructed with an operation name, a target
  provider (the customized objects) and optional fixed parameter values. It owns
  enabled-state from Preflight (with the first diagnostic as tooltip), the Plan preview,
  confirmation, transaction, progress and reporting. Customizations add a button by name and
  write no operation logic.
- **Content Browser actions:** descriptors whose `EntryPoints` include `ContentBrowser` and that have a
  `TargetClass` register a context-menu entry for that asset class through `UToolMenus`. It
  runs the operation on the whole selection with one preview and one transaction. This
  replaces `BatchGenerateNotifies`. `ToolMenus` is added to the editor module's
  dependencies.
- **Multi-selection:** `FAttackDataCustomization` currently returns early for more than one
  object. Operation buttons support multi-selection by passing all customized objects as
  targets.

## Migration

### Operations

All eight become `IKatanaAssetOperation` implementations. Their existing bodies are kept
behind thin adapters in the first pass; the static `Run` entry points remain as facades over
the adapters so the existing tests keep compiling and passing.

| Operation | Change |
| --- | --- |
| `AttackDataNotifyMigration` | Adapter. Parameter `Regenerate` (default false). Its report hash becomes an approval contract. |
| `AttackDataTimingMigration` | Adapter. Parameter `Strategy`: `ClampRecovery` (today's behavior) or `RecalculateFromTypeDefaults` (the Auto-Calculate button's behavior, moved from `UAttackDataTools::AutoCalculateTiming`). Gains approval. |
| `ContentReadinessAudit` | Adapter, `bMutatesAssets = false`. |
| `CounterChainProofMigration` | Adapter. Gains approval. |
| `DefenseProofMigration` | Adapter. Fingerprint moves to the shared service; its post-save audit becomes `VerifyAfterSave`. |
| `DefenseProofAuthoring` | Adapter. Private approval copy removed. |
| `DefenseMatrixAuthoring` | Adapter (already on the shared service). |
| `EnemyAIProofAssets` | Adapter. Gains approval and its first tests. |

### Buttons and fields in `FAttackDataCustomization`

| Today | After |
| --- | --- |
| Generate AnimNotifies | `SAssetOperationButton("AttackDataNotifyMigration")`. No forced regeneration (the `Regenerate` parameter is explicit), no silent timing rewrite: invalid timing is a Preflight error that points to the timing operation. |
| Auto-Calculate Timing | `SAssetOperationButton("AttackDataTimingMigration", Strategy=RecalculateFromTypeDefaults)`. Undoable. |
| Validate | Preflight of the notify operation, shown in the Message Log. |
| Section dropdowns ×3 | Not operations. Write through `IPropertyHandle::SetValue`, which makes them undoable. |
| Open Montage Editor, refresh ×3 | Unchanged (navigation). |

### Consolidation and removals

- The eight `UAttackData` validators collapse into two sources of truth: the notify
  service's analysis (notify and phase timing) and `UAttackData::IsDataValid` (data rules).
  The Timing operation's own checks and the details panel's
  `HasValidNotifyTimingInSection` call the same analysis.
  `UAttackDataTools::ValidateMontageSection`, `ValidateAttackData` and
  `ValidateNotifyGenerationTiming` are Blueprint-callable, so each becomes a thin wrapper
  over those sources when a C++ caller or a Content reference exists, and is removed when
  neither exists (checked with a binary Content scan).
- Removed: `GenerateAttackPhaseNotifies`, `GenerateHitDetectionNotifies`,
  `GenerateComboWindowNotify`, `BatchGenerateNotifies`, `AddNotifyToMontage`,
  `RemoveNotifiesOfType` and the dead `OnPreviewTimelineClicked`. A binary Content scan
  confirms no Blueprint calls them before removal.
- `UAttackDataTools` keeps read-only helpers used by other code; its mutators move to
  operations or services.

## Testing

- Registry: registration, lookup, duplicate-name rejection, descriptor validation.
- Approval: the unified fingerprint binds the same facts each operation bound before
  (regression tests per migrated operation); drift rejects apply; apply without approval is
  rejected for every mutating operation, including the three that previously allowed it.
- Editor context (non-UI layer): one transaction per batch; undo restores every target; a
  cancelled batch rolls back; nothing is saved.
- Parity: for saved (clean) packages, the same operation and targets produce identical plan
  rows headless and through the editor context; the editor fingerprint for a dirty target
  binds its in-memory state.
- Editor gating: mutating operations are unavailable during PIE; asset-creating operations
  show the undo warning; `Version` changes invalidate prior approvals.
- Buttons: the notify button no longer dirties an unchanged montage; invalid timing is
  reported, not rewritten.
- Existing suites (`KatanaAssetMigrationTests`, `AttackDataEditorToolsTests`) pass unchanged
  through the facades.

## Verification

- Full baseline green.
- Headless: `Audit` and `Plan` for every operation against its documented targets produce
  the same rows as before migration (compared against pre-migration reports in
  `Saved/Logs/Commandlets/KatanaAssetMigration/`).
- Editor, manual: each migrated button previews, applies, undoes and redoes; a Content
  Browser multi-selection runs one preview and one undo step.

## Follow-Ups (out of scope)

- Move the Paired Animation Preview's in-widget analysis into the analysis suite (dead
  duplicate optimizers, duplicate distance and rotation buttons, non-cancellable slow task,
  unreachable undo and redo, silent `hand_r` weapon fallback).
- Replace the fat `FKatanaAssetMigrationRow` with a base row plus typed payloads once no test
  depends on the flat fields.
- Scripting access (a Blueprint/Python-callable entry point for Editor Utility widgets and
  editor Python) over the same registry.
