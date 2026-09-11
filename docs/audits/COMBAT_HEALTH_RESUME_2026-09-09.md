# Combat Health Resume - 2026-09-09

Follow-up: the user directed preservation of the current finisher authoring and repair of the proof definitions. [Defense Proof Repair](DEFENSE_PROOF_REPAIR_2026-09-09.md) records that work and its newer results. This report retains the initial verification snapshot and its original no-save scope.

## Scope and current status

Authorized work: reconcile existing WIP with the stabilization plan, establish a fresh build/test baseline, classify failures, and prepare/perform the pending lifecycle/input proof. Structural refactors, content deletion, and bulk line-ending normalization are deferred until the behavior checkpoint is established. No existing Content package is authorized for resave by this verification pass.

Status: fresh editor build passed; full automation completed 739 tests with 732 passed and seven failed. Four read-only authoring/manifest commandlet passes completed with zero package saves. A focused verbose replay confirmed the Gate A marker-role mismatch. The test-only retry-bookkeeping correction compiled and its focused replay now records complete, accurate cases while preserving the failed automatic handoff. One intervening run stopped earlier on missing physical normal-block contact; its cause remains open. Interactive two-map acceptance is still open.

- Branch: `codex/combat-action-reaction-stabilization`.
- HEAD: `6e170503b5958d84d4c065cfaf6e18704546df7a` plus pre-existing uncommitted source/tests/content.
- Evidence directory: `Saved/Logs/HealthResume-20260909/`.
- `preflight.json` records capture time and process state. No Unreal Editor process was visible; UEMCP confirmed this project is attached and the editor is unavailable.
- `workspace-status-before.txt` records all modified, deleted, and untracked paths.
- `content-before.json` records SHA-256/existence for 55 dirty or explicitly protected content paths, including both test maps and BP_Player.
- `source-config-before.json` records SHA-256 for 365 source/configuration/tool files; `source-config-before.patch` preserves the tracked source/configuration delta. The untracked input/hold test files are included in the hash manifest.

## Health-report reconciliation

- The retained final July 21 run completed 739 tests: 732 passed and seven failed. The 727 count in the supplied health report does not describe that final log.
- Inspected combat, paired, AI, and new input/hold test modification times precede that final run. Uncommitted changes do not establish that the work was untested; timestamps alone also cannot bind today's bytes to a historical run.
- CombatComponent's attack-warp function selects intent and calls TargetingComponent's alignment/warp executor. The shared function name does not establish duplicated ownership.
- The four apparently unused math libraries expose Blueprint functions. A source-reference search alone is insufficient evidence for removal. Deprecated notifies have editor compatibility consumers.
- Tracked `.gitattributes` already declares Git LFS and `-text` for Unreal binary assets. Explicit text line-ending policy is absent; preserve the binary rules in any later policy change.
- Complexity, churn, comment ratios, friend declarations, and test-file references are maintenance signals, not behavioral coverage or a root-cause diagnosis.

## Verification sequence

1. Run `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Codex/run-agent-baseline.ps1` against the captured source/content state. This performs the no-UBA editor build and the full KatanaCombat automation root.
2. Read the resulting failures and focused input/hold, AI, paired, death, and defense outcomes. Repeat focused tests only to resolve a failure or after a relevant change.
3. Diagnose real-project authoring plan/approval setup using read-only commandlet reports; distinguish content drift, test-fixture coupling, and implementation defects.
4. Reconcile Micro-Plan 04A against existing source/tests. The old July handoff and unchecked plan are not sufficient evidence that implementation is absent.
5. Complete or accurately gate Checkpoints 1C/1D and input/hold scenarios in both maps with named telemetry and visible evidence. Headless automation is not interactive two-map acceptance.
6. Verify source/content preservation and update this report plus the execution handoff with results, proof limits, and exact next action.

## Results

### Build and automation

| Run | Result | Evidence |
|---|---|---|
| Standard no-UBA baseline build | Exit 0; UBT `Target is up to date`, `Result: Succeeded` | `Saved/Logs/Codex-Agent-Baseline-20260909-131711-build.out.log` |
| Standard baseline automation | Startup exit 3; zero tests discovered/completed | Same prefix `-automation.log`; default DDC graph had no writable node |
| Full automation with `-DDC-ForceMemoryCache` | 739 completed, 732 passed, seven failed; process exit 255 | `Saved/Logs/HealthResume-20260909/full-memorycache.log` |
| Test-only diagnostic correction build | Compile/link succeeded, exit 0 | `Saved/Logs/HealthResume-20260909/diagnostic-fix-build.out.log` |
| First post-edit Gate A replay | One failed test, exit 255; normal-block physical contact timed out before the changed path | `Saved/Logs/HealthResume-20260909/gate-a-diagnostic-fix.log` |
| Second post-edit Gate A replay | One failed test, exit 255; automatic handoff remains failed, retry damage/cleanup and complete case ledger pass | `Saved/Logs/HealthResume-20260909/gate-a-diagnostic-fix-replay.log` |

The cache fallback changes only the execution environment. The build was an up-to-date verification, not a clean recompile. The original startup failure remains preserved and is not counted as a combat test failure. The test summary has 41 failure/error lines and 372 automation warnings; seven is the number of failed tests, not the number of diagnostic lines. No project or machine cache settings were edited.

`test-results.json` contains every completed test path/result/log line. `focused-results.json` extracts the following from the same full run; these were not extra executions:

| Suite | Passed / completed |
|---|---|
| `KatanaCombat.CombatInput` | 16/16 |
| `KatanaCombat.EnemyAI` | 54/54 |
| `KatanaCombat.EnemyAI.TargetLifecycle` (subset of EnemyAI) | 11/11 |
| `KatanaCombat.PairedAnimation` | 51/51 |
| `KatanaCombat.DeathSystem` | 14/14 |
| `KatanaCombat.Defense` | 142/143 |
| `KatanaCombat.ActionReaction.Telemetry` | 14/14 |

The failed test names match the final July 21 run. This does not make the branch green or excuse the failures. `tested-binaries.json` records the module DLL hashes used for verification.

### Failure classification

| Failed tests | Current evidence and classification |
|---|---|
| `KatanaCombat.Editor.AssetMigration.DefenseAuthoring.ApplyRefusalDoesNotMutate`, `.ApprovalContractRejectsDrift`, `.PlanIsConcreteAndReadOnly` | Gate A recipe V4 plan construction fails because `/Game/ProjectFiles/Data/PDA/Defense/GateA/DA_Finisher_GateA` differs from the reviewed recipe. These real-project tests depend on a valid current plan before testing approval behavior; their downstream failures do not establish a broken approval validator. |
| `KatanaCombat.Editor.AssetMigration.DefenseMatrixAuthoring.ApprovalRejectsDrift`, `.NotifyFactsBindApproval`, `.PlanIsConcreteAndReadOnly` | Gate B recipe V11 cannot construct its approval dependency hash: `/Game/ProjectFiles/Animation/Montages/Defense/GateA/AM_Finisher_Defender` is absent. The source uses `BuildPackageStateHash` and deliberately rejects a missing required dependency. |
| `KatanaCombat.Defense.GateA.PIEProof` | The selected counter's FinisherReady notify reports `Victim` (1), while the chain policy expects `Attacker` (0). The source montage and instance match. Runtime rejects the marker, then reaches `FinisherReady` on montage completion instead of automatically entering `FinisherActive`. The test retries through public Light input. |

Relevant source: `DefenseProofAuthoringOperation.cpp` `BuildPlan`/`PairedDataMatches`/`BuildCurrentApprovalContract`; `DefenseMatrixAuthoringOperation.cpp` `BuildPlan`/`BuildCurrentApprovalContract`; `AssetAuthoringApprovalService.cpp` `AppendPackageStateFact`; `DefenseGateAPIEProofTests.cpp` `UpdatePerfectParryCounter`/`UpdatePerfectParryFinisher`.

The decisive verbose evidence is `gate-a-diagnostic.log:1964`: `Ignored stage marker generation=3 role=1 expected_role=0 instance=11 expected_instance=11`, with both montage paths equal to `/Game/ProjectFiles/Animation/Montages/Defense/GateA/AM_Counter_Defender.AM_Counter_Defender`, notify index 3 of 4. `PairedAnimationComponent.cpp` `OnDefenseChainStageMarker` checks this driver-role contract. The rejection explains the observed automatic-handoff failure; it does not establish that every other authored dependency is correct.

There was a separate diagnostic defect in the Gate A test fallback. Its automatic-success branch snapshots `PerfectFinisherInitialHealth` and `PerfectFinisherStageGeneration`, but its manual retry branch changed stage without recording either. Later assertions filtered telemetry against the unset generation and reported zero damage/cleanup events. The fresh pre-edit actual telemetry records:

| Sequence | Stage generation | Event |
|---|---|---|
| 78 | 3 | `StageTransition / FinisherReady` |
| 79 | 4 | `StageStart / FinisherActive` after the test's public-input retry |
| 80 | 4 | `StageDamage / FinisherActive` |
| 81 | 4 | `Cleanup / FinisherActive / FinisherCompleted` |

Thus the reported zero counts are not evidence of missing lethal damage or terminal cleanup on this retry. The automatic-continuation failure remains real. The full-run evidence and CSV were copied to `full-gate-a-evidence.json` and `full-gate-a-telemetry.csv` before the focused replay could overwrite the reusable proof output directory. The verbose pre-edit replay is preserved separately as `diagnostic-gate-a-evidence.json` and `diagnostic-gate-a-telemetry.csv`.

The only source edit in this pass is eight added lines in `DefenseGateAPIEProofTests.cpp`: snapshot health before public-input retry, retain the resulting finisher stage generation, and record `CounterToFinisherContinuity` as a failed case when retry succeeds. The existing automatic-handoff error remains. This makes the retry's later damage/cleanup assertions accurate and fills the case ledger without converting the failed handoff into a pass. The full-suite result above precedes this test-only edit; focused verification used the rebuilt test module, whose hashes are retained in `diagnostic-fix-tested-binaries.json`.

The first post-edit run (`gate-a-diagnostic-fix.log`, exit 255) stopped at `NormalBlockAwaitContact`: the physical hit window closed with zero hits, and the fixture timed out without a resolution. It never reached the changed code. The earlier full run and pre-edit focused replay passed this stage. This additional contact failure remains retained in `diagnostic-fix-gate-a-evidence.json` / `diagnostic-fix-gate-a-telemetry.csv`. The second run used the same source and execution arguments with a distinct log path and passed this stage. The contact failure is intermittent across these runs; its cause is not established.

The second post-edit replay (`gate-a-diagnostic-fix-replay.log`, exit 255) reached the corrected path. Its only assertion errors are the unchanged automatic-handoff error and the aggregate requirement that every case pass. `diagnostic-fix-replay-gate-a-evidence.json` records all 12 cases, `complete_case_ledger=true`, `fatal_failure=false`, `CounterToFinisherContinuity=false`, and `PerfectParryCounterFinisher=true`: enemy health 0, pre-finisher health 1, one finisher damage event, one completed cleanup, and one token release. This verifies the diagnostic correction without asserting a green Gate A or erasing the intermittent contact failure.

### Read-only content reconciliation

| Pass | Result |
|---|---|
| `gate-a-authoring-plan.json` | One failed recipe row; one paired-data mismatch; proposes creating the historical Defender montage and rewriting the Attacker montage; zero changes/saves |
| `gate-b-authoring-plan.json` | One failed recipe row due to missing Gate A dependency; also proposes nine attack-variant updates and two matrix-montage updates; zero changes/saves |
| `gate-a-manifest-audit.json` | 27 rows: 23 unchanged, three would change, one failed; zero changes/saves |
| `gate-b-manifest-audit.json` | 82 rows: 78 unchanged, three would change, one failed; zero changes/saves |

Both manifests retain the historical finisher role mapping:

| Role | Manifest/recipe | Current loaded paired data |
|---|---|---|
| Attacker | `Defense/GateA/AM_Finisher_Defender`, section `Finisher` | `Defense/GateA/AM_Finisher_Attacker`, section `Finisher` |
| Victim | `Defense/GateA/AM_Finisher_Attacker`, section `Finisher` | `Defense/GateA/AM_Finisher_Victim`, section `Finisher` |

All montage paths above are under `/Game/ProjectFiles/Animation/Montages/`. The historical Defender package is a pre-existing deletion; the current Victim package is pre-existing untracked WIP.

Both manifests also omit the current Victim montage and the GhostSamurai `GhostSamurai_Ambush01` / `GhostSamurai_Ambushed01` animation dependencies. Both report `LightAttack_1` counter-reference mismatch. Gate B additionally reports undeclared `/Game/ProjectFiles/Data/PDA/Paired/Counters/Counter_LightAttack_1`. Gate B's proposed updates include a small contact-matrix length difference and low-matrix CombatWarp notify timing drift. They are proposals against a failing contract, not an approved migration.

Do not apply the generated plans as a shortcut to green: Gate A would recreate a deliberately absent path and overwrite an existing edited montage. The exact intended counter/finisher role graph must be reconciled first, including marker driver role, sections, dependency closure, and both manifests. Preserve current authoring as the default recommendation; restoring the historical fixture is a separate content decision.

### Micro-Plan 04A reconciliation

The implementation is present but its complete acceptance is not established. The following source and focused tests were inspected; this is a bounded requirement reconciliation, not a full adversarial review of every modified function.

| Requirement | Existing implementation/evidence |
|---|---|
| Explicit input termination | Player Move Completed/Canceled and combat Canceled bindings; `CombatInput.Bindings.TerminalEventsAreExplicit`, `.TerminalEdges.*` |
| Movement suppression without taking movement mode | `APlayerCharacter::Move` calls `SubmitMovementInput`; terminal sample clears; `CombatInput.Movement.*`, `.HoldCleanup.PreservesExternalMovementMode` |
| Hold identity | Closed `EHoldPhase`, source attack/montage identity, release-facing snapshot; `OnHoldWindowStartWithContext`; `.HoldCommitment.ExactNotifyContext` |
| One normal pending slot | Light/Heavy replacement in `TryQueueAction`, copy/remove winner before `ExecuteAction` in `ProcessQueuedActions`; `.NormalSlot.NewestEligibleInputWins`, `.OneExecutionAttemptPerBoundary` |
| Hold protection and continuation | Context-checked `DispatchHoldOwnedFollowUp` and generation-bound easing; `.HoldCommitment.RecoveryInputCannotPreempt`, `.HoldOwnedHandoffPrecedesNormalSlot`, `.StaleEaseGenerationRejected` |
| Paired takeover and terminal release | `CombatInput.PairedTakeover.*`, `.TerminalEdges.ReleaseClearsStateWhilePaired` and the fresh paired/death suites; use the exact test inventory for names |
| Reason-coded observation | Actor/generation input and movement telemetry; 14 telemetry tests passed |

Open: the full hold-phase/input/interruption matrix in both real maps, successful authored follow-up versus pending-input ordering in live playback, retained red evidence for every promised regression, and a complete hostile review. The legacy polling `ProcessQueue(float)` implementation is still present; this is an unchecked cleanup item rather than proof that the event-driven queue tests failed. Micro-Plan 04B is not advanced by this run.

### Interactive proof boundary

The full suite exercised existing headless PIE fixtures in ThirdPerson and DefenseMatrix. Gate A's fresh fixture recorded four enemies, one active token with three queued, public guard/parry input, same-enemy recovery/reattack without a range reset, and the failed automatic counter-to-finisher transition. These are scoped automated results, not manual Checkpoint 1C/1D acceptance.

UEMCP management confirmed the correct attached project and no live editor at preflight. Discovery advertised PIE tools, but no callable PIE/editor tool was exposed to this session. Computer Use initialization returned `Windows Computer Use Sky runtime is unavailable`. No interactive playtest was performed, no visual acceptance is claimed, and no editor was launched solely to leave an uncontrolled session running. The named two-map Checkpoints 1C/1D and hold-input scenarios remain open.

### Remaining work

1. Reconcile the intended current proof content graph. Default recommendation: retain the current Attacker/Victim finisher authoring and update its proof definitions. The user was asked whether to preserve that authoring or restore the historical Gate A fixture; no answer was received during this verification pass. No package-save action follows from elapsed time or from these invalid plans.
2. Prepare the concrete repair across Gate A/B manifests, authoring recipe dependencies, `LightAttack_1` counter selection, and the selected counter's marker-driver role. Inspect both participant roles and their montage notifies before choosing which asset property to change. Include sections, terminal-pose requirements, and animation dependency closure. The intended presentation must determine the repair; do not weaken approval checks or silently reset authored WIP.
3. Run read-only Gate A/B plans and audits on the reconciled definitions, then perform only an explicitly reviewed package-save set. Rerun affected approval tests and Gate A proof. Separately investigate the intermittent physical-contact failure with trace/pose evidence; a successful retry alone does not close it.
4. Complete manual lifecycle/input checkpoints with usable Editor control and retained telemetry. The full hold/interruption matrix and hostile review still gate Micro-Plan 04A acceptance.
5. Review and commit coherent source/test slices only against their actual proof limits; structural refactoring and deletion remain deferred.

### Preservation and changed files

`preservation-final.json` checks all 55 protected Content paths for existence and SHA-256, and compares all Content status entries with the preflight capture: no differences. Of 365 captured source/configuration/tool files, only the eight-line Gate A test edit differs. The remaining 364 captured files match. No migration package was saved and no Content asset was reverted, renamed, recreated, or staged.

This pass changed `Source/KatanaCombatTest/Private/DefenseGateAPIEProofTests.cpp`, added this report, and updated the current-state/next-action portion of `docs/handoffs/2026-07-18-combat-action-reaction-stabilization-execution.md`. Generated build/test evidence is under `Saved/`. No commit was created. `git diff --check` passed for the changed tracked source/handoff files.
