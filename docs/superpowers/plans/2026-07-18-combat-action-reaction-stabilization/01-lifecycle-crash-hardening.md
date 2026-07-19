# Micro-Plan 01: Lifecycle Crash Hardening

**Spec:** `docs/superpowers/specs/2026-07-18-combat-action-reaction-stabilization/01-lifecycle-crash-hardening.md`

**Goal:** Make enemy attack start, consume, montage completion, death, and StateTree observation exact-generation and reentrancy safe.

## Task 1: Capture The Red Regression

**Modify:**

- `Source/KatanaCombat/Public/AI/EnemyCombatAIComponent.h`
- `Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp`
- `Source/KatanaCombatTest/Private/EnemyCombatAITests.cpp`

**Read:** `Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp`, `Source/KatanaCombat/Private/AI/EnemyCombatStateTreeTasks.cpp`

- [ ] Under `WITH_AUTOMATION_TESTS`, add a one-shot `SetPostExecuteAttackDataHookForTesting(TFunction<void()> Hook)` seam. Invoke and clear it immediately after `ExecuteAttackData()` returns and before any post-call read; the hook may call the production `AbortAttack()` path and must not add a test-only early return.
- [ ] Add `KatanaCombat.EnemyAI.AttackStartup.ReentrantSelectionClear` using that seam to clear selected attack through production cleanup.
- [ ] Add cases for synchronous consume, owner death, stale montage completion, duplicate termination, and `OnAttackStarted` participant destruction.
- [ ] Assert no crash, one token release, one consumed generation, terminal death precedence, and no newer-generation termination.
- [ ] Build, then run only `Automation RunTests KatanaCombat.EnemyAI.AttackStartup.ReentrantSelectionClear;Quit` in a dedicated `UnrealEditor-Cmd.exe` process. Record the expected failing assertion or access violation, crash context, and exit code before running any other test process.

The red crash process is disposable and isolated from the baseline/final test process. Do not deliberately crash a shared Editor session or continue using binaries from the crashed process.

## Task 2: Snapshot And Revalidate Startup

**Modify:**

- `Source/KatanaCombat/Public/AI/EnemyCombatAIComponent.h`
- `Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp`

- [ ] Snapshot selected attack, montage, target, owner, anim instance, combat component, and token state before `ExecuteAttackData()`.
- [ ] Never dereference mutable `SelectedAttack` after that call.
- [ ] Build the returned `FAttackExecutionSnapshot`, verify current generation and `CombatComponent->IsAttackConsumed()`, and branch to exact terminal cleanup if invalid/consumed.
- [ ] Bind montage completion to the snapshot montage and capture the exact `FAttackInstanceId` in component-owned state.
- [ ] Commit internal ownership before `OnAttackStarted`; revalidate after broadcasting.
- [ ] Keep cleanup idempotent and ensure `ReleaseTokenAndCleanup()` cannot release a newer token/action.

## Task 3: Harden StateTree Observation

**Modify:**

- `Source/KatanaCombat/Public/AI/EnemyCombatStateTreeTasks.h`
- `Source/KatanaCombat/Private/AI/EnemyCombatStateTreeTasks.cpp`
- `Source/KatanaCombatTest/Private/EnemyCombatAITests.cpp`

- [ ] Store the full attack instance or equivalent owner+generation, not an unqualified generation assumption.
- [ ] Make `EnterState()` handle a synchronously terminal attack without reading cleared component state.
- [ ] Make `Tick()` distinguish consumed-success, normal recovery-success, death/invalid terminal, and replacement-generation failure.
- [ ] Ensure task exit cancels only pending token requests; it must not release an active token owned by the attack lifecycle.

## Task 4: Verify And Commit

- [ ] Build with `-NoUBA -MaxParallelActions=1`.
- [ ] Run `KatanaCombat.EnemyAI`, `KatanaCombat.Defense`, `KatanaCombat.DeathSystem`, and `KatanaCombat.CombatComponent.MemorySafety`.
- [ ] Run a five-minute repeated Defense Matrix PIE attack scenario and inspect for access violations and duplicate token logs.
- [ ] Run the hostile lifecycle checklist: nulls, stale generation, duplicate callback, participant destruction, delegate reentry, and EndPlay.
- [ ] Inspect exact diff and preserved WIP.
- [ ] Commit: `Fix reentrant enemy attack startup`.

**Gate:** Do not start alignment/orbit work while any attack-start crash or unexplained token imbalance remains.
