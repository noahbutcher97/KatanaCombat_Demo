# Micro-Plan 04A: Input Availability And Hold Commitment

**Spec:** `docs/superpowers/specs/2026-07-18-combat-action-reaction-stabilization/04a-input-availability-and-hold-commitment.md`

**Goal:** Make asymmetric input availability observable, establish one deterministic normal-attack buffer, and protect the exact light-hold/follow-up lifecycle from unauthorised montage replacement.

## Task 1: Capture The Current Failures With Red Tests

**Create:**

- `Source/KatanaCombatTest/Private/CombatInputAvailabilityTests.cpp`
- `Source/KatanaCombatTest/Private/HoldCommitmentTests.cpp`

**Modify:**

- `Source/KatanaCombatTest/Private/HoldWindowTests.cpp`
- `Source/KatanaCombatTest/Private/InputBufferingTests.cpp`
- `Source/KatanaCombatTest/Private/InputQueueAdvancedTests.cpp`

- [ ] Prove a committed Recovery-phase light hold is cleared by a normal immediate attack in current source.
- [ ] Convert the full-debug 2643-2726 hold-clear ordering into a deterministic red regression; use exact owner/generation state, not log adjacency.
- [ ] Prove hold easing sets `MOVE_None` and cleanup can force `MOVE_Walking` over another owner.
- [ ] Prove same-type pending input is rejected, including a long-lived entry equivalent to the 8598-11583 capture; prove newest entries are processed first and more than one matching entry can start at one boundary.
- [ ] Preserve Micro-Plan 02's regression proving a terminal Move sample is cleared; prove the cleared live-steering sample cannot leak into hold release direction or a later action edge.
- [ ] Replace tests that accept live-hold replacement; retain duration checks only as diagnostics.
- [ ] Convert the 2026-07-19 same-frame Heavy/Light checkpoint sequence into a deterministic one-winner regression; do not rely on ownerless log ordering as the test oracle.
- [ ] Build and capture focused red evidence before production edits.

## Task 2: Add Exact Hold And Intent Contracts

**Modify:**

- `Source/KatanaCombat/Public/ActionQueueTypes.h`
- `Source/KatanaCombat/Public/CombatTypes.h`
- `Source/KatanaCombat/Public/Core/CombatComponent.h`

- [ ] Add closed hold phases and exact source attack/montage/hold generation identity.
- [ ] Add `HoldOwned` routing and a bounded intent record with immutable world-facing context, disposition/reason, input serial, and observed movement/montage context.
- [ ] Distinguish physical Release from Cancel; make all terminal cleanup idempotent.
- [ ] Keep hold duration out of eligibility and route decisions.

## Task 3: Make The Normal Buffer Deterministic

**Modify:** `Source/KatanaCombat/Private/Core/CombatComponent.cpp`

- [ ] Replace the multi-entry normal attack queue with one pending normal slot or an equivalent invariant-enforced representation.
- [ ] Preserve the winning edge's Micro-Spec 02 facing intent through queue delay and replacement; replacement cannot splice old direction into a new edge.
- [ ] Record replacement of the prior edge instead of duplicate-type rejection.
- [ ] Select and remove one winner before any montage-start call; never continue iterating mutable queue state after reentry.
- [ ] Preserve Chain-only no-fallthrough behavior and current attack-data resolution semantics.

## Task 4: Implement Hold Commitment And Owned Handoff

**Modify:**

- `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- `Source/KatanaCombat/Private/Utilities/MontageUtilityLibrary.cpp`

- [ ] Validate hold-start notify against the active attack generation and capture immutable source data.
- [ ] Route normal attack presses during committed phases to the pending slot without montage execution.
- [ ] On release, snapshot normalized world direction, desired yaw, branch direction, and source data before easing, then execute at most one exact-generation `HoldOwned` follow-up.
- [ ] Give hold-owned handoff precedence over the pending normal slot; route the pending input through the follow-up's later policy.
- [ ] Close matching hold state on early release, no follow-up, montage failure/interruption, external interrupt, paired entry, death, owner destruction, and EndPlay.

## Task 5: Route Movement Without Owning Movement Mode

**Modify:**

- `Source/KatanaCombat/Public/Characters/PlayerCharacter.h`
- `Source/KatanaCombat/Private/Characters/PlayerCharacter.cpp`
- `Source/KatanaCombat/Public/Core/CombatComponent.h`
- `Source/KatanaCombat/Private/Core/CombatComponent.cpp`

- [ ] Reuse Micro-Plan 02's Move Triggered/Completed/Canceled bindings and canonical terminal-aware world sample; do not add a second movement or steering cache.
- [ ] Bind Light/Heavy/Block Canceled to physical-state cleanup without fabricating a hold follow-up.
- [ ] Pass normalized movement sample and dead-zone transitions through CombatComponent before applying movement.
- [ ] Remove hold-path `DisableMovement()`/`SetMovementMode(MOVE_Walking)` and the unsafely mirrored `bMovementCurrentlyDisabled` authority.
- [ ] Re-evaluate held movement at hold terminal and preserve paired/death movement mode exactly.

## Task 6: Make Lockout Reasons Visible

**Modify:**

- `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- `Source/KatanaCombat/Private/Utilities/DebugUtils.cpp`
- `Source/KatanaCombat/Public/Debug/DebugConfig.h`
- `Source/KatanaCombat/Private/Debug/DebugConfig.cpp`

- [ ] Record every combat edge's final route, disposition, and reason.
- [ ] Record movement dead-zone and policy changes only, including CMC mode, root-motion state, primary action, and hold generation.
- [ ] Include actor identity, attack instance, montage instance/source, queue entry, and hold generation on action-start and action-terminal records so concurrent actors cannot be confused.
- [ ] Route Queue, Hold, and Phase diagnostics through their category-specific CVar predicates; keep `Combat.Debug.All` as the master override.
- [ ] Extend the existing combat debug overlay/log output without adding per-frame history spam.

## Task 7: Verify And Commit

- [ ] Build and run `KatanaCombat.CombatComponent.Hold`, `KatanaCombat.CombatComponent.InputBuffering`, `KatanaCombat.InputQueue`, and the new input/hold roots.
- [ ] Run paired input and attack-state-machine regressions.
- [ ] PIE the full hold matrix in both test levels with `Combat.Debug.All 1`, reason records visible, and the resulting rotated log retained as evidence.
- [ ] Hostile-review stale generations, input flush, reentrant montage callbacks, absent phase notifies, no configured follow-up, death/paired mode ownership, and one-winner queue behavior.
- [ ] Run `git diff --check`, inspect exact files, and confirm preserved map hashes.
- [ ] Commit source/tests/docs: `Stabilize input availability and hold commitment`.

**Gate:** Do not implement Micro-Plan 04B cancel windows until normal attacks cannot bypass the hold owner and every observed lockout has a reason-coded trace.
