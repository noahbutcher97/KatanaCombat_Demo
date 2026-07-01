# Chain Counter And AttackData Empirical Audit

Date: 2026-06-30
Branch: `feature/paired-animation-component`
Scope: read-only/source-backed audit before revising the Chain Counter and AttackData migration spec.

## Executive Verdict

The branch builds and the current automation suite is green, but the green signal does not prove the intended player-facing Chain Counter loop. The current source has useful scaffolding for AC3 counters, Chain state transitions, paired finishers, notify migration, and commandlet reporting. The public runtime path from player input to parry to counter to finisher is still not behavior-complete.

AttackData migration is not generally broken at the source/tooling level: the commandlet runs and writes a valid audit report. It is currently narrow by design and only audits phase-transition/hold-start notify canon. It does not audit or seed parry windows, counter windows, `CounterData`, paired sync/collision notifies, or paired montage readiness.

Asset evidence is the largest readiness gap. The current asset inventory shows finisher references and paired finisher montage notifies, but no `CounterData`, no `AnimNotifyState_ParryWindow`, and no `AnimNotifyState_CounterWindow` strings in the scanned project AttackData/paired/montage assets. Counter Chain should remain canonical branch scope, but it cannot be claimed ready until both public runtime flow and asset readiness are proven.

## Empirical Commands Run

Baseline build and automation:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "Tools\Codex\run-agent-baseline.ps1"
```

Result:
- Build exit: 0
- Automation exit: 0
- Completed automation result lines: 391
- Failures/errors: 0
- Automation warnings: 439
- Summary: `Saved/Logs/Codex-Agent-Baseline-20260630-092023-automation-summary.json`

Read-only commandlet audit:

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" `
  -run=KatanaAssetMigration `
  -Operation=AttackDataNotifyMigration `
  -Mode=Audit `
  -AllowGlobalScan `
  -ReportPath="Saved/Logs/CodexAudit/chain-counter-attackdata-empirical-audit.json" `
  -unattended -nopause -NullRHI -nosplash -stdout
```

Result:
- Commandlet exit: 1
- Report written: yes
- Targets: 20
- Would change: 18
- Failed: 2
- Changed/saved packages: 0
- Failed rows:
  - `LightAttack_9`, section `Attack_3`: timing total `1.000s` exceeds section length `0.867s`
  - `LightAttack_6`, section `Attack_3`: timing total `1.000s` exceeds section length `0.861s`

## Current Canon From Docs

The current canon is consistent across `AGENTS.md`, `CLAUDE.md`, and `ARCHITECTURE_QUICK.md`:

- Phases are exclusive; windows may overlap.
- Input is always buffered; combo windows affect timing, not capture.
- Parry is contextual Block: defender checks the attacker's parry window.
- Hold checks current button state at the window boundary, not duration.
- `AnimNotifyState_HoldWindow` and default `AnimNotifyState_ComboWindow` seeding are sunset. They may still exist as legacy/manual override classes, but normal AttackData seeding should not emit them.
- `AnimNotify_HoldWindowStart`, `AnimNotify_AttackPhaseTransition`, `AnimNotifyState_ParryWindow`, `AnimNotifyState_CounterWindow`, and paired sync/collision notifies remain current where applicable.

Important doc caveat: `CLAUDE.md` labels Counter Chain as scaffolded/code complete but needing animations, and marks SpecificCounterData wiring as a planned architecture gap. `docs/plans/gap-tracker.md` still marks the parry/counter/finisher chain partial.

## Runtime Source Findings

### Public Chain Counter Flow Is Contradictory

`UPairedAnimationComponent::CanCounter()` validates Chain mode by looking for a parryable enemy. `UPairedAnimationComponent::TryCounter()` then always retrieves a counter-window enemy and a counter-window context before dispatching to Chain mode. That means the public Chain entry path can pass one target predicate and then fetch from a different target model.

Spec implication: split public entry into AC3 counter-window handling and Chain parry-window handling. Chain context must start from the attacker in `AnimNotifyState_ParryWindow`, not from `AnimNotifyState_CounterWindow`.

### Block Input Does Not Start Parry

`UCombatComponent::OnInputEvent()` queues press actions after normal input processing. `ExecuteAction()` has an empty `EInputType::Block` case. `APlayerCharacter` forwards block press/release into `OnInputEvent`, but no observed public route calls `TryCounter()` on Block press.

Spec implication: Block press should attempt Chain parry first. On success it should consume the input and avoid normal block queue handling. On failure it can continue to normal block behavior once that behavior exists.

### Attack Input Does Not Advance Chain

`ExecuteChainCounterAttack()` is internal/protected and current Chain tests call it directly. Normal Light/Heavy execution first attempts `TryExecuteFinisher(Action.AttackData)`, then plays a normal attack montage. There is no public "attack input during Chain CounterWindow" route.

Spec implication: add a public Chain advance API that accepts the selected `UAttackData` from the player's attack input and reserves Light/Heavy input while Chain state is `CounterWindow`.

### Chain Uses Ambient CurrentAttack

`ExecuteChainFinisher()` uses `CachedCombatComponent->GetCurrentAttack()` and passes that to `TryExecuteFinisher()`. During a parry sequence this can be null, stale, or unrelated to the attack input chosen for the counter step.

Spec implication: introduce explicit active Chain context with target, source parry attack metadata, selected counter attack, selected `CounterData`, selected `FinisherData`, and continuation flags.

### Paired Counter Completion Has No Chain Handoff

`CompletePairedAnimation()` cleans up paired animation state and returns to normal combat. It does not continue Chain state after a paired counter montage completes. Since `BeginPairedAnimation()` blocks combat input, a design that requires another input during a paired counter is suspect.

Spec implication: model paired counter as a Chain state, for example `CounterPairedActive`, and on successful completion either auto-continue to finisher or enter a deliberately unblocked `FinisherReady` state.

### Damage Semantics Are Ambiguous

`UPairedAnimationData::bIsLethal` defaults true, and `CompletePairedAnimation()` applies lethal damage when `bIsLethal` is true. A Chain counter step that uses paired data can kill before the finisher unless data or runtime rules prevent it.

Spec implication: define counter-step damage separately from finisher damage. Recommended: Chain counter paired steps are nonlethal by default; the finisher remains the lethal paired animation.

## AttackData And Migration Findings

### Tooling Strengths

The editor and commandlet migration code has good safety properties:

- Audit/Plan modes are read-only.
- `ApplyAndSave` requires `-AllowPackageSave`.
- `-AllowTimingMutation` is rejected for AttackData notify migration.
- Plan mode does not mutate notifies.
- Apply is section-scoped.
- Invalid timing does not produce a valid plan.
- Existing tests cover current default seeding, stale canonical reseeding, outside-section preservation, missing section rejection, invalid timing rejection, and save-gate validation.

### Tooling Scope Gap

The current generation service analyzes and plans only:

- `AnimNotify_AttackPhaseTransition(Active)`
- `AnimNotify_AttackPhaseTransition(Recovery)`
- optional `AnimNotify_HoldWindowStart`
- removal/reseed of legacy or stale default phase/hold/combo/hit-toggle notifies

It does not inspect:

- `AttackData::CounterData`
- `AttackData::FinisherData` readiness beyond property presence in binary strings
- `AnimNotifyState_ParryWindow`
- `AnimNotifyState_CounterWindow`
- paired sync or collision notifies
- paired montage section existence
- paired montage skeleton compatibility
- lethal/nonlethal paired counter semantics
- Chain Counter public-flow readiness

Spec implication: do not turn the current migration commandlet into a broad auto-seeder yet. First add a readiness reporting layer with explicit fields for parry/counter/paired gaps and keep all saves behind the existing package-save gates.

## Asset Inventory Findings

Scanned roots:

- `Content/ProjectFiles/Data/PDA/Attack`
- `Content/ProjectFiles/Data/PDA/Paired`
- `Content/ProjectFiles/Animation/Montages`

Observed asset counts:

- Attack configurations: 1
- Directional AttackData under `New/`: 5
- Heavy AttackData under `New/`: 4
- Light AttackData under `New/`: 11
- Paired finishers: 6

Binary string scan results:

| Signal | Files |
| --- | ---: |
| `FinisherData` | 11 |
| `CounterData` | 0 |
| `AnimNotifyState_ParryWindow` | 0 |
| `AnimNotifyState_CounterWindow` | 0 |
| `AnimNotifyState_PairedAnimationSync` | 4 |
| `AnimNotifyState_PairedAnimationCollision` | 2 |
| `AnimNotify_HoldWindowStart` | 4 |
| `AnimNotifyState_HoldWindow` | 5 |
| `AnimNotifyState_ComboWindow` | 5 |
| `AnimNotify_AttackPhaseTransition` | 14 |
| `AnimNotify_ToggleHitDetection` | 0 |

Interpretation:

- New light attacks appear to reference finisher data.
- No scanned asset shows counter data or parry/counter window notifies.
- Paired finisher montage assets show sync notifies; attacker paired finisher montages show collision notifies.
- Legacy hold/combo state notifies still exist in older montages and should be treated as migration/audit targets, not as current default authoring.

Limitation: binary string scans are useful but not authoritative structured asset reads. The next implementation should use editor-side asset inspection/commandlet reports for exact property values.

## Test Coverage Findings

The baseline is green, but current test coverage can still allow false confidence:

- `CounterSystemTests.cpp` directly calls `TryCounter_ChainMode()` and `ExecuteChainCounterAttack()`, bypassing public input routing and public target selection.
- AC3 specific counter data fallback is intentionally tested as success after paired counter animation fails to start.
- Chain counter attack test treats "finisher failed, state reset to None" as acceptable.
- Parry detection tests validate attacker-window timing through checkpoint registration, not the full player Block input path.
- Paired animation tests cover many state, data, input-blocking, sync, and utility primitives, but not a real asset-backed counter-to-finisher Chain loop.
- Automation warnings include null default combo attack asset warnings, which align with the current asset reorg and should not be ignored for branch readiness.

Spec implication: add public-flow tests before implementation is called complete. Minimum tests:

1. Block press during enemy parry window starts Chain parry.
2. `TryCounter()` in Chain mode uses parryable target, not counter-window target.
3. Attack press during Chain `CounterWindow` advances through the public Chain API.
4. Selected `UAttackData` is retained and used for counter/finisher selection.
5. Paired counter completion continues or deliberately unblocks according to spec.
6. Counter paired data does not kill unless explicitly allowed.
7. Cancel, timeout, partner death, owner death, montage interruption, and invalid target clear Chain context.

## Adversarial False-Green Paths

The branch can build and tests can pass while the feature remains broken if:

- Tests call protected Chain methods and never exercise real input.
- Block input is queued but never routed to parry.
- Chain public target selection validates parry window but fetches counter window.
- Attack input during Chain plays a normal attack instead of advancing the Chain.
- `CurrentAttack` happens to be non-null in a test but is stale in gameplay.
- Paired counter data defaults lethal and kills before a finisher.
- `CompletePairedAnimation()` clears paired state without notifying Chain state.
- Asset tests use transient objects and never verify real `.uasset` montages.
- Binary asset moves leave old tracked assets deleted and new replacement assets untracked, making branch proof non-reproducible after checkout.
- Migration audit only reports phase/hold defaults, so "no migration gaps" would miss all counter/paired readiness gaps.

## Spec Inputs Before Implementation Planning

The next spec pass should explicitly define:

1. The canonical Chain state machine, including public input entry points and paired-animation substates.
2. An `FActiveChainCounterContext` or equivalent runtime data model.
3. Which data owns counter paired animation selection: attacker notify `CounterData`, defender selected `UAttackData::CounterData`, or a deterministic priority order.
4. Counter-step damage semantics and default nonlethal behavior.
5. Completion/cancel ownership between paired animation lifecycle and Chain state.
6. Asset readiness report schema for AttackData, parry/counter windows, paired data, sync/collision notifies, sections, and lethal flags.
7. Migration boundaries: report first, explicit target list next, save only with reviewed target file and save gate.
8. Proof ladder: build, focused public-flow tests, commandlet readiness audit, then editor or UEMCP asset-backed gameplay proof.

## Immediate Recommendation

Revise the spec before implementation. Treat Chain Counter and AttackData migration as one branch-alignment effort, but split implementation into two lanes:

1. Runtime lane: make public Chain input flow and active Chain context correct.
2. Tooling lane: expand AttackData migration into a readiness auditor before any broad asset seeding.

Do not start asset saves until the readiness report can show exactly which assets need parry/counter/paired authoring and why.
