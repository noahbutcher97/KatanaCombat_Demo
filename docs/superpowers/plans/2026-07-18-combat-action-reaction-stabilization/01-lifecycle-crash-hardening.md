# Micro-Plan 01: Lifecycle Crash Hardening

**Spec:** `docs/superpowers/specs/2026-07-18-combat-action-reaction-stabilization/01-lifecycle-crash-hardening.md`

**Goal:** Make enemy attack start, consume, montage completion, death, and StateTree observation exact-generation and reentrancy safe.

## Task 1: Capture The Red Regression

**Modify:**

- `Source/KatanaCombat/Public/AI/EnemyCombatAIComponent.h`
- `Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp`
- `Source/KatanaCombatTest/Private/EnemyCombatAITests.cpp`

**Read:** `Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp`, `Source/KatanaCombat/Private/AI/EnemyCombatStateTreeTasks.cpp`

- [x] Under `WITH_AUTOMATION_TESTS`, add a one-shot `SetPostExecuteAttackDataHookForTesting(TFunction<void()> Hook)` seam. Invoke and clear it immediately after `ExecuteAttackData()` returns and before any post-call read; the hook may call the production `AbortAttack()` path and must not add a test-only early return.
- [x] Add `KatanaCombat.EnemyAI.AttackStartup.ReentrantSelectionClear` using that seam to clear selected attack through production cleanup.
- [x] Add cases for synchronous consume, owner death, stale montage completion, duplicate termination, and `OnAttackStarted` participant destruction.
- [x] Assert no crash, one token release, one consumed generation, terminal death precedence, and no newer-generation termination.
- [x] Build, then run only `Automation RunTests KatanaCombat.EnemyAI.AttackStartup.ReentrantSelectionClear;Quit` in a dedicated `UnrealEditor-Cmd.exe` process. Record the expected failing assertion or access violation, crash context, and exit code before running any other test process.

The red crash process is disposable and isolated from the baseline/final test process. Do not deliberately crash a shared Editor session or continue using binaries from the crashed process.

## Task 2: Snapshot And Revalidate Startup

**Modify:**

- `Source/KatanaCombat/Public/AI/EnemyCombatAIComponent.h`
- `Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp`

- [x] Snapshot selected attack, montage, target, owner, anim instance, combat component, and token state before `ExecuteAttackData()`.
- [x] Never dereference mutable `SelectedAttack` after that call.
- [x] Build the returned `FAttackExecutionSnapshot`, verify current generation and `CombatComponent->IsAttackConsumed()`, and branch to exact terminal cleanup if invalid/consumed.
- [x] Bind montage completion to the snapshot montage and capture the exact `FAttackInstanceId` in component-owned state.
- [x] Commit internal ownership before `OnAttackStarted`; revalidate after broadcasting.
- [x] Keep cleanup idempotent and ensure `ReleaseTokenAndCleanup()` cannot release a newer token/action.

## Task 3: Harden StateTree Observation

**Modify:**

- `Source/KatanaCombat/Public/AI/EnemyCombatStateTreeTasks.h`
- `Source/KatanaCombat/Private/AI/EnemyCombatStateTreeTasks.cpp`
- `Source/KatanaCombatTest/Private/EnemyCombatAITests.cpp`

- [x] Store the full attack instance or equivalent owner+generation, not an unqualified generation assumption.
- [x] Make `EnterState()` handle a synchronously terminal attack without reading cleared component state.
- [x] Make `Tick()` distinguish consumed-success, normal recovery-success, death/invalid terminal, and replacement-generation failure.
- [x] Ensure task exit cancels only pending token requests; it must not release an active token owned by the attack lifecycle.

## Task 4: Verify And Commit

- [x] Build with `-NoUBA -MaxParallelActions=1`.
- [x] Run `KatanaCombat.EnemyAI`, `KatanaCombat.Defense`, `KatanaCombat.DeathSystem`, and `KatanaCombat.CombatComponent.MemorySafety`.
- [ ] Run post-fix Checkpoint 1 in both authored maps and inspect for access violations, post-death continuation, queue faults, and duplicate or imbalanced token logs.
- [x] Run the hostile lifecycle checklist: nulls, stale generation, duplicate callback, participant destruction, delegate reentry, and EndPlay.
- [x] Reproduce lethal montage-stop reentry inside `CombatComponent`, prevent post-death montage restart, and make event-driven queue removal safe when death clears the queue synchronously.
- [x] Inspect exact diff and preserved WIP.
- [ ] Commit: `Fix reentrant enemy attack startup`.

## Evidence

- Isolated red crash: `Saved/Logs/Codex-Red-ReentrantSelectionClear-20260719-153508.log`, exit `3`, access violation at `0x50`.
- Additional red runs: synchronous consumption, stale lifecycle completion, state-transition reentry, StateTree adoption, production-bound montage interruption, token-release replacement, dependency invalidation, and pre/post-commit direct generation replacement all failed deterministically before their fixes.
- Focused green: `Saved/Logs/Codex-AttackStartup-FinalClosure-20260720-091912.log` (`9/9`), `Saved/Logs/Codex-AttackLifecycle-FinalClosure-20260720-091934.log` (`9/9`), `Saved/Logs/Codex-EnemyAI-FinalReviewClosure-20260720-092013.log` (`33/33`), and `Saved/Logs/Codex-Defense-FinalReviewClosure-20260720-092029.log` (`139/139`).
- Full baseline: `Saved/Logs/Codex-Agent-Baseline-20260720-102629-automation.out.log` (`664/664`, zero failures/errors, explicit success marker, exit `0`).
- Reopened red proof: `Codex-Red-ImmediateGrantAbort-20260720-103919.log`, `Codex-Red-QueuedGrantStateAbort-20260720-104330.log`, `Codex-Red-ActiveTerminationReentry-20260720-104753.log`, `Codex-Red-TargetClearReentry-20260720-103529.log`, `Codex-Red-ResultRetention-20260720-105433.log`, and `Codex-Red-TokenResetReentry-20260720-105948.log` each reproduced its isolated callback or identity failure before the fix.
- Reopened focused green: `Saved/Logs/Codex-EnemyAI-FinalLifecycleClosure-20260720-111326.log` (`39/39`) and `Saved/Logs/Codex-Defense-PostLifecycleAudit-20260720-110349.log` (`139/139`), both with zero failures and exit `0`.
- Final no-UBA baseline: `Saved/Logs/Codex-Agent-Baseline-20260720-111400-automation.log` (`670/670`, zero failures/errors, explicit success marker, exit `0`). Its `347` automation warnings match the prior baseline count. The build also used `-NoHotReloadFromIDE` because the UE 5.6 global Live Coding mutex remained present with no Editor process.
- Post-playtest red proof: `Saved/Logs/Codex-Red-LethalBlendCallback-PreviousState-20260720.log` reproduced post-death attack montage restart; `Saved/Logs/Codex-Red-QueuedLethalBlendCallback-BoundDeathReset-20260720.log` reproduced `TArray::RemoveAt` on a queue synchronously cleared by death (exit `3`).
- Post-playtest focused green: `Saved/Logs/Codex-Green-LethalBlendCallback-20260720.log` and `Saved/Logs/Codex-Green-QueuedLethalBlendCallback-20260720.log` (`1/1` each, exit `0`); `Saved/Logs/Codex-Focused-DefenseTelemetry-20260720.log` (`4/4`, exit `0`) proves `Combat.Debug.All` enables bounded defense telemetry.
- Current final no-UBA baseline: `Saved/Logs/Codex-Agent-Baseline-20260720-144041-automation.log` (`673/673`, zero failures/errors, explicit success marker, exit `0`). Its four-warning increase is fully explained by the two new lethal fixtures emitting health-death and missing-reaction-setting warnings, each mirrored by the automation controller.
- Manual evidence is partial: `Lvl_DefenseMatrix` ran about 6:58 with three enemies and balanced 80 grants/releases; three `Lvl_ThirdPerson1` sessions exercised four enemies with no crash. These runs preceded the montage-stop fix, landed no parry, and therefore do not close the post-fix acceptance gate.
- Proof limit: StateTree non-adoption has a source architecture guard and broad Defense PIE coverage, not a directly instantiated task-context unit test. A post-fix two-map interactive repetition with a defense telemetry dump remains open.
- All eight preserved user-asset packages match the current pre-verification snapshot after the final baseline. The user-edited `AM_Light_Combo_1.uasset` no longer matches its historical planning-time hash and remains untouched outside this slice.

## Reopened Adversarial Closure

- [x] Reproduce and harden immediate and queued token-grant callback invalidation before reading selected attack or reporting ownership.
- [x] Reproduce and harden active termination against replacement attacks started by combat, warp, montage, token, or state callbacks.
- [x] Preserve terminal result lookup for an older exact attack identity when a callback completes a newer identity first.
- [x] Snapshot token reset iteration before broadcasting release callbacks that may mutate token ownership.
- [x] Preserve a target assigned reentrantly during target-clear token release and settle into its correct ready state.
- [x] Reject attack continuation when montage stop/play callbacks make the owner terminal, without restoring stale pre-death attack state.
- [x] Remove event-driven queue entries before callback-capable execution so terminal teardown cannot invalidate the active entry reference.
- [x] Rerun focused lifecycle tests, `KatanaCombat.EnemyAI`, `KatanaCombat.Defense`, and the complete no-UBA baseline before marking Checkpoint 1 ready.

**Gate:** Do not start alignment/orbit work while any attack-start crash or unexplained token imbalance remains.
