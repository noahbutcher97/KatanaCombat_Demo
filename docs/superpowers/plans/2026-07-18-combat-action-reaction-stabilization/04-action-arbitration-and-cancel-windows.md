# Micro-Plan 04B: Action Arbitration And Cancel Windows

**Spec:** `docs/superpowers/specs/2026-07-18-combat-action-reaction-stabilization/04-action-arbitration-and-cancel-windows.md`

**Goal:** Add a pure voluntary-action policy, split guard intent/activity, route movement intent, and implement exact-generation cancel windows.

**Prerequisite:** Micro-Plan 04A is green and committed. Its one-slot normal buffer, `HoldOwned` route, movement sampling, intent telemetry, and hold generations are authoritative and must not be reimplemented here.

## Task 1: Lock The Decision Matrix With Red Tests

**Create:**

- `Source/KatanaCombatTest/Private/CombatActionResolverTests.cpp`
- `Source/KatanaCombatTest/Private/CombatCancelWindowTests.cpp`

**Modify:**

- `Source/KatanaCombatTest/Private/InputBufferingTests.cpp`
- `Source/KatanaCombatTest/Private/DefenseInputThreatTests.cpp`

- [ ] Add table-driven tests for every primary action versus Movement, Guard, Light, Heavy, and Evade intent.
- [ ] Cover execute, coexist, queue, reject, exact-generation cancel, terminal paired/death, and minimum uncancellable time.
- [ ] Add stale Notify End, overlapping notify, interrupted montage, owner death, and component EndPlay cases.
- [ ] Prove every edge is recorded before eligibility, movement dead-zone crossing emits once, held movement is reconsidered at window open, and Chain-only input does not fall through.
- [ ] Prove committed 04A hold phases queue ordinary attacks unless an exact matching cancel window permits preemption.
- [ ] Build and capture focused red results.

## Task 2: Implement Pure Contracts

**Create:**

- `Source/KatanaCombat/Public/Action/CombatActionResolver.h`
- `Source/KatanaCombat/Private/Action/CombatActionResolver.cpp`

**Modify:** `Source/KatanaCombat/Public/CombatTypes.h`

- [ ] Add closed primary-action, guard transition, cancel-flag, snapshot, query, and decision types not already owned by 04A, including finite cancel blend-out seconds.
- [ ] Keep cross-component reflected contracts in `CombatTypes.h`; keep resolver implementation world-free.
- [ ] Implement the complete table as pure policy with finite time checks and generation matching.
- [ ] Do not include montage pointers as action identity.

## Task 3: Add Generation-Safe Cancel Notify

**Create:**

- `Source/KatanaCombat/Public/Animation/AnimNotifyState_CancelWindow.h`
- `Source/KatanaCombat/Private/Animation/AnimNotifyState_CancelWindow.cpp`

**Modify:**

- `Source/KatanaCombat/Public/Core/CombatComponent.h`
- `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- `Source/KatanaCombat/Private/Utilities/MontageUtilityLibrary.cpp`

- [ ] Open/close records using primary owner/generation, montage instance, and `FAnimNotifyRuntimeSourceId`.
- [ ] Route Notify Begin/End through context-validating APIs; reject compatibility calls that cannot identify an active owner.
- [ ] Close matching records on montage terminal, primary terminal, death, and EndPlay.
- [ ] Enforce minimum uncancellable time independently of notify placement.
- [ ] Commit action termination and release owned alignment/root motion before `Montage_StopWithBlendOut`; normal movement/guard cancels use the authored 0.12-second default rather than a pose-snapping zero blend.
- [ ] Update timing inspection to report cancel windows as current runtime support rather than future scaffolding.

## Task 4: Project State And Dispatch Exact Cancellation

**Modify:**

- `Source/KatanaCombat/Public/Core/CombatComponent.h`
- `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- `Source/KatanaCombat/Public/Core/HitReactionComponent.h`
- `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`

- [ ] Implement `BuildCombatActionSnapshot()` by querying current owners in terminal priority order; do not store a mirrored primary-state enum.
- [ ] Add owner-specific cancel dispatch that verifies the decision's exact generation before stopping anything.
- [ ] Reconcile `GetCombatState()` as a compatibility projection including paired and reaction states.
- [ ] Keep paired Chain response routing unchanged and highest priority where the paired spec requires it.

## Task 5: Split Guard And Integrate Movement Cancellation

**Modify:**

- `Source/KatanaCombat/Public/Core/CombatComponent.h`
- `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- `Source/KatanaCombat/Private/Animation/SamuraiAnimInstance.cpp`
- `Source/KatanaCombat/Public/Data/CombatSettings.h`

- [ ] Replace `bIsBlocking` authority with `bGuardInputHeld` and `bGuardActive`; preserve compatibility accessors with explicit semantics.
- [ ] Capture Block Press/Release independent from whether guard can activate.
- [ ] Suspend active guard on exact hard-reaction start and resume only at a valid boundary while held.
- [ ] Reuse 04A movement samples/dead-zone edges and re-evaluate held movement when an exact cancel window opens.
- [ ] Add finite/clamped `MovementIntentDeadZone = 0.20` to CombatSettings if 04A has not already done so, and audit `IA_Move`, `IMC_Default`, and `IMC_Combat` modifiers for stricter/conflicting dead zones without saving them.
- [ ] Preserve 04A's rule that ordinary suppression never changes CharacterMovement mode and movement is never enqueued per tick.

## Task 6: Verify And Commit Runtime Support

- [ ] Add validator checks for finite minimum lock, finite/clamped blend-out, and nonempty allowed-intent cancel windows.
- [ ] Build; run pure action, input, defense, paired, and phase/window tests using synthetic exact-generation windows.
- [ ] Hostile-review duplicate state, stale generations, early notify escape, guard-release races, death, Chain fallthrough, and montage callback reentry.
- [ ] Commit source/tests: `Implement generation-safe combat cancel windows`.

## Task 7: Author Reviewed Cancel Windows

**Create:**

- `Source/KatanaCombatEditor/Public/Commandlets/Operations/CombatCancelWindowMigrationOperation.h`
- `Source/KatanaCombatEditor/Private/Commandlets/Operations/CombatCancelWindowMigrationOperation.cpp`
- `Tools/Codex/manifests/combat-cancel-windows.json`

**Modify:**

- `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp`
- `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`

- [ ] Audit active AttackData and HitReaction settings, resolving exact montage sections and all existing action-window notifies without saving.
- [ ] Commit migration source/tests before writing assets: `Add combat cancel window migration`.
- [ ] Produce a reviewed plan for attack Recovery cancellation and full-body reaction movement/guard cancellation after the minimum lock. Every row records section, start/end, allowed intents, and blend-out.
- [ ] Reject duration-only inference, overlapping duplicate records, missing source identity, dirty/loaded packages, and either user map/external-actor path.
- [ ] Apply only the approved rows, rerun audit for convergence, and stage exact montage packages.
- [ ] Commit binary timing separately: `Author combat cancel windows`.

## Task 8: Validate Real Assets And Behavior

- [ ] Build; run `KatanaCombat.CombatAction`, input, defense, paired, phase/window, and StateTree suites.
- [ ] PIE-test movement and guard at closed/open attack and reaction windows, including already-held movement.
- [ ] Re-run the hostile review against real montage timing and capture any corrective source/asset commit separately.

**Gate:** Do not integrate reaction policy until voluntary cancellation works without bypassing 04A hold ownership or becoming a second lifecycle state machine.
