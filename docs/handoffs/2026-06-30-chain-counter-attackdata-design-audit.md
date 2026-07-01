# Chain Counter And AttackData Design Audit

Date: 2026-06-30
Branch: `feature/paired-animation-component`
Scope: adversarial analysis, audit, and design review before implementation planning.

## Executive Verdict

Do not move directly into implementation planning from the current draft plan. Chain Counter should stay in scope and should be treated as canonical branch behavior, but the current design contract has several hard gaps that can let source tests pass while player-facing behavior still fails.

The highest-risk issue is that the public Chain Counter entry point is internally inconsistent: `CanCounter()` validates an enemy in `ParryWindow`, but `TryCounter()` then fetches an enemy in `CounterWindow`. The existing tests mostly call protected Chain methods directly, so they do not prove the public flow.

AttackData migration is also in scope, but it should start as a readiness/audit expansion, not broad automatic seeding. The system can safely report missing parry/counter/paired requirements before it tries to guess frame ranges or save assets.

## Evidence Reviewed

- `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`
- `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`
- `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- `Source/KatanaCombat/Public/Data/AttackData.h`
- `Source/KatanaCombat/Public/Data/PairedAnimationData.h`
- `Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp`
- `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.cpp`
- `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`
- `docs/architecture/ARCHITECTURE_QUICK.md`
- `docs/specs/PAIRED_ANIMATION_SPEC.md`
- `docs/plans/gap-tracker.md`
- `CLAUDE.md`

No Editor, commandlet, or automation test was run for this audit. Asset behavior remains unproven.

## Adversarial Analysis

Assume a future implementation claims "Chain Counter works" because focused state tests pass. That claim can still be false through these paths:

1. The player presses Block during an enemy `ParryWindow`, but no real input route calls `TryCounter()`. `OnInputEvent()` queues all press actions, and `ExecuteAction()` has an empty `EInputType::Block` case.
2. If something calls `TryCounter()` manually, Chain mode can still fail because it selects `FindCounterableEnemy()` after `CanCounter()` selected `FindParryableEnemy()`.
3. If Chain is started through a direct test, attack input during `CounterWindow` currently goes through normal attack queue logic. No source path routes it to `ExecuteChainCounterAttack()`.
4. If a counter paired animation is added, `BeginPairedAnimation()` blocks input. A design that expects another player attack input before finisher will stall unless the continuation is automatic or input blocking is explicitly relaxed.
5. If the chain reaches `ExecuteChainFinisher()`, it uses `CachedCombatComponent->GetCurrentAttack()`. After a parry sequence this can be null, stale, or unrelated to the chosen counter input.
6. If a paired counter step uses `UPairedAnimationData`, `CompletePairedAnimation()` applies damage, and `UPairedAnimationData::bIsLethal` defaults true. The counter step can kill before the finisher unless damage semantics are explicitly separated.
7. If the paired counter/finisher is interrupted, `CancelPairedAnimation()` resets paired state but does not explicitly clear Chain context because no active Chain context exists yet.

## Audit Findings

### A1 - Chain Target Model Is Contradictory

Severity: P0

`CanCounter()` checks `FindParryableEnemy()` in Chain mode, but `TryCounter()` always uses `FindCounterableEnemy()` and `GetEnemyCounterContext()` before dispatching to Chain. Evidence: `PairedAnimationComponent.cpp` lines 507-536 and 558-569.

Required correction: split AC3 and Chain target/context construction. Chain must use a parryable attacker and a parry context, while AC3 continues to use counter-window context.

### A2 - Block Input Does Not Start Parry/Chain

Severity: P0

The docs say Parry is contextual Block: defender presses Block, checks nearby enemies for `ParryWindow`, and starts parry if valid. Runtime code does not currently show that route. `OnInputEvent()` queues input at lines 443-577, and `ExecuteAction()` leaves `EInputType::Block` empty at lines 1043-1045.

Required correction: Block press should attempt Chain parry before normal block behavior. On success, consume the block input and do not enqueue a normal block action.

### A3 - Attack Input Does Not Advance Chain

Severity: P0

`ExecuteChainCounterAttack()` is protected/internal and existing tests call it directly. Normal light/heavy input first tries `TryExecuteFinisher(Action.AttackData)`, then plays a normal montage. Evidence: `CombatComponent.cpp` lines 960-986 and `CounterSystemTests.cpp` lines 467-474.

Required correction: attack press while Chain is in `CounterWindow` must call a public Chain advance API and must not enter the normal action queue on success.

### A4 - Chain Finisher Uses Ambient CurrentAttack

Severity: P0

`ExecuteChainFinisher()` calls `TryExecuteFinisher(CurrentAttack)`, where `CurrentAttack` comes from `CachedCombatComponent->GetCurrentAttack()`. Evidence: `PairedAnimationComponent.cpp` lines 898-909. This is not a reliable source for a parry-counter finisher.

Required correction: store the chosen counter input `UAttackData` in active chain context and use that explicit data for both counter and finisher stages.

### A5 - Paired Counter Completion Has No Handoff

Severity: P0

`CompletePairedAnimation()` cleans up paired state and returns to normal combat. It has no Chain continuation hook. Evidence: `PairedAnimationComponent.cpp` lines 1365-1428 and cleanup after that block. If Chain counter uses a paired montage, completion will not automatically reach finisher.

Required correction: introduce an explicit Chain paired step state, for example `CounterPairedActive`, and on successful completion either auto-continue to finisher using stored chain context or enter a deliberately unblocked `FinisherReady` state.

### A6 - Counter Step Can Apply Lethal Damage

Severity: P0

`CompletePairedAnimation()` applies damage for active paired data and marks `bWasCounter` when the reaction type is Counter. `UPairedAnimationData::bIsLethal` defaults true. Evidence: `PairedAnimationComponent.cpp` lines 1383-1428 and `PairedAnimationData.h` lines 256-268.

Required correction: define counter-step damage semantics. Recommended: Chain counter paired steps are nonlethal by default and either suppress completion damage or require explicit nonlethal `CounterData`. The finisher step remains the lethal paired animation.

### A7 - Cancel/Death Cleanup Does Not Own Chain Context

Severity: P1

`CancelChainCounter()` clears only `ChainState` and timeout. `CancelPairedAnimation()` clears paired state but does not clear Chain state or future Chain context. Evidence: `PairedAnimationComponent.cpp` lines 923-950 and 1289-1363.

Required correction: all interruption paths that cancel paired animation, partner death, owner death, timeout, montage interruption, or invalid target must clear Chain target, selected attack, paired data, timers, and continuation flags.

### A8 - AttackData Migration Scope Is Too Narrow For Branch Readiness

Severity: P1

Current notify generation analyzes phase transitions and optional hold start, and removes legacy phase/hold/combo/toggle notifies. It does not audit parry windows, counter windows, paired sync/collision notifies, or paired data references. Evidence: `AttackDataNotifyGenerationService.cpp` lines 72-87, 217-228, and 265-284.

Required correction: expand migration reporting first. It should identify readiness gaps for `FinisherData`, `CounterData`, montage sections, parry/counter windows, and paired sync/collision notifies without guessing timings or saving assets by default.

### A9 - Existing Tests Are White-Box Heavy

Severity: P1

Several Chain tests call `TryCounter_ChainMode()` and `ExecuteChainCounterAttack()` directly, bypassing public target selection and input routing. Evidence: `CounterSystemTests.cpp` lines 411-423, 459-474, 582-585, and 701-705.

Required correction: add public-flow tests for Block input, Chain `TryCounter()`, attack input advance, target retention, paired completion, cancellation, and damage semantics.

### A10 - Docs Are Directionally Correct But Stale In Places

Severity: P2

Architecture docs correctly state that phases are exclusive, windows overlap, input is always buffered, and Parry is contextual Block. They also say Chain state machine is scaffolded and needs animations. Gap tracker states the full flow is still partial. Evidence: `ARCHITECTURE_QUICK.md` lines 66-78 and 184-203, `CLAUDE.md` lines 482-496, `gap-tracker.md` lines 384-393.

Required correction: when implementation planning resumes, docs should call Chain Counter "canonical but incomplete" until public runtime flow, paired animation continuation, and asset audit proof exist.

## Design Review

### Canonical Runtime Contract

1. Block press is the only parry entry input. `UCombatComponent` should ask `UPairedAnimationComponent` to attempt Chain parry when the defender presses Block.
2. Chain parry target comes from an attacker-side `AnimNotifyState_ParryWindow`, not `AnimNotifyState_CounterWindow`.
3. The successful parry creates an active Chain context with:
   - parried attacker target;
   - source parry metadata if available;
   - selected counter `UAttackData` once attack input is pressed;
   - selected `CounterData` and `FinisherData`;
   - current Chain step;
   - continuation/damage policy flags.
4. Attack input during Chain `CounterWindow` is reserved for Chain. It should select the pressed `UAttackData`, then advance through counter and finisher using stored context.
5. Counter paired animation completion should not require a second player input while paired input blocking is active. Recommended behavior: auto-continue from counter paired completion into finisher if `FinisherData` is available and validation passes.
6. Chain finisher should use the stored parried attacker as the target. It should not rely on current hard-lock, soft aim, or ambient `CurrentAttack`.
7. Counter step and finisher step must have distinct damage semantics. The counter step staggers, poses, or deals nonlethal damage; the finisher step performs lethal paired damage.

### Data Ownership

Do not use the enemy's current `UAttackData::CounterData` as the defender's chosen counter attack unless the property is deliberately redefined. The current `AttackData.h` comment says `CounterData` is the paired animation for this attack when this attack is performed as a parry counter. That points to the defender's selected attack data, not necessarily the attacker's data.

Enemy-side parry context can provide target identity and optional pose metadata. Defender-side selected attack data should provide `CounterData` and `FinisherData`.

### Migration Contract

The migration tool should be expanded in two phases:

1. Read-only readiness audit:
   - default attack phase transitions present;
   - stale legacy state notifies present;
   - hold start present only when current canon requires it;
   - parry/counter windows present where the attack is configured to expose defensive response;
   - `CounterData` and `FinisherData` references are present when flags say they should be;
   - paired data has attacker/victim montages, valid sections, sync requirements, and collision requirements.
2. Apply mode:
   - safe to add or regenerate phase transition notifies and current hold-start point notifies.
   - not safe to auto-place parry, counter, sync, or collision windows without explicit timing policy or asset-authored metadata.

### Proof Gates Before Implementation Planning

Before implementation planning resumes, settle these design decisions:

1. Is Chain counter one follow-up attack input after parry, with automatic finisher continuation, or does it require a second finisher input after counter?
2. What exact field owns counter-step damage policy: paired data flag, active Chain context flag, or new paired animation purpose enum?
3. Should Chain use `UAttackData::CounterData` only from the defender's selected attack, or can enemy-side notify `CounterData` override it?
4. Does a Chain parry make the target finisher-vulnerable through stagger alone, or should Chain bypass normal finisher vulnerability checks once the parry context is valid?
5. Which AttackData fields declare that parry/counter windows are expected, so migration can report missing windows without guessing for every attack?

## Review Of Existing Draft Plan

`docs/superpowers/plans/2026-06-30-chain-counter-attackdata-branch-alignment.md` is useful but should remain draft until this audit is folded in.

Required changes to that plan:

- Replace the proposed parry context helper that pulls `EnemyCombat->GetCurrentAttack()->CounterData` with an active Chain context that separates enemy pose/source data from defender selected attack data.
- Add a Block-input start task before or alongside public `TryCounter()` fixes.
- Change `ExecuteChainCounterAttack()` to accept or resolve the selected `UAttackData`, not use ambient `CurrentAttack`.
- Add a paired counter completion task that auto-continues or deliberately unblocks input according to the chosen design.
- Add a damage semantics task before any paired counter playback task.
- Add cancel/death cleanup tests that prove Chain context clears when paired animation is interrupted.
- Keep AttackData migration audit-first for parry/counter/paired readiness; do not auto-seed those windows in the first implementation pass.

## Current Recommendation

Next action should be a short design correction pass, not implementation. Update the spec/plan to lock the Chain Counter contract above, especially input count, damage semantics, data ownership, and migration audit scope. After that, implementation planning can be concrete and test-driven instead of patching around ambiguous behavior.
