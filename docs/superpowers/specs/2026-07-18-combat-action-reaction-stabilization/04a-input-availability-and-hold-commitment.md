# Micro-Spec 04A: Input Availability And Hold Commitment

## Purpose

Make every apparent input lockout explainable and prevent ordinary attacks from bypassing the authored light-hold lifecycle. This slice establishes capture, decision, application, queue, and hold-ownership contracts before general voluntary cancel windows are added in Micro-Spec 04B.

## Empirical Defects

Current source has several independently observable behaviors:

- `APlayerCharacter::Move()` sends input directly to `AddMovementInput()`, while `UCombatComponent` independently calls `DisableMovement()` during light-hold ease-in/freeze. The Enhanced Input callback can run even though CharacterMovement is `MOVE_None`.
- hold cleanup later forces `MOVE_Walking`, despite paired animation and death also owning movement-mode changes.
- attack input remains routable during a hold. Recovery-phase input executes immediately, and `PlayAttackMontage()` unconditionally clears the active hold before starting the new montage.
- Windup/Active input can wait for a phase transition that a near-zero-rate hold montage delays. A second input of the same type is then rejected as already queued.
- the queue claims FIFO/last-input-wins, but duplicate types are rejected and phase processing walks newest-to-oldest while allowing multiple starts at one boundary.
- a hold follow-up is inserted into the normal queue as a synthetic press, so normal queue policy can reorder or displace hold-owned progression.
- `IA_Move` is bound only to `Triggered`; its terminal `Completed`/`Canceled` value is not recorded. `LastMovementInput` can therefore remain nonzero and select a stale directional follow-up.
- Light, Heavy, and Block bind `Completed` but not `Canceled`, so an input flush or mapping-context change can leave physical-held state latched.

These findings explain the source mechanisms. The full-debug capture below now reproduces the principal runtime paths; focused automation and post-fix PIE remain required.

### PIE Corroboration (2026-07-19)

`Saved/Logs/KatanaCombat-backup-2026.07.19-14.49.54.log` preserves the relevant PIE capture:

- Heavy resolves while `LightAttack_3` is current at line 2777, then Light resolves against the same current attack at line 2802. At the next boundary, `AM_Heavy_Katana_Event` and `AM_Light_Combo_2` checkpoints are both discovered in the same frame at lines 2810 and 2817, with no intervening enemy attack start. This strongly corroborates multi-start queue processing and immediate montage replacement, but checkpoint logs need owner/generation identity before this is treated as standalone proof.
- `DA_DirectionalAttack_F` is active at line 2852 and a normal Light resolves against it at line 2864. This proves input acceptance during the directional follow-up, not its exact execution mode or cancellation time.
- The capture has no `[INPUT]`, `[QUEUE]`, `[PHASE]`, `[MONTAGE]`, `[MOVEMENT]`, or `[HOLD]` transition records. It therefore cannot attribute each reported lockout to capture failure, policy rejection, delayed execution, montage replacement, or CharacterMovement suppression.

The current `UCombatComponent::GetDebugDraw()` consults only `Combat.Debug.All`; category-specific Queue/Hold/Phase CVars do not enable its guarded logs. The telemetry implementation must use category-appropriate predicates and always include actor, action generation, hold generation, route, disposition, and reason.

### Full-Debug PIE Corroboration (2026-07-19)

`Saved/Logs/KatanaCombat-backup-2026.07.19-15.23.35.log` was captured with `Combat.Debug.All` enabled after the reported defects were reproduced:

- lines 2643-2726 show Light queued during `LightAttack_1`; the hold activates at 2674, Recovery starts at 2678, hold state is cleared at 2688-2691, and `Attack_2` starts at 2695-2697. The same ordering repeats across later sessions. This confirms ordinary queued input bypasses the newly committed hold at the Recovery boundary.
- all eight `Movement DISABLED` records have later `Movement ENABLED` partners (24952/25192 through 61901/61948). The capture confirms intentional movement-only suppression while attack routing remains live, but does not reproduce a leaked `MOVE_None` after release.
- 119 attack presses are rejected as `Already queued action of same type`. In one long case, the entry is queued at 8598, 21 later presses are rejected from 8671 through 10845, and terminal cleanup does not clear it until 11583. This directly explains attack-only lockout while locomotion remains available.

The full log still interleaves several actors in generic phase/montage records. Physical player input and owner-qualified Combat Warp records are attributable, but final telemetry must close the remaining owner/generation ambiguity rather than relying on temporal adjacency.

## Four-Layer Input Contract

The system must distinguish:

1. **Capture:** the physical Enhanced Input edge or movement sample reached gameplay code.
2. **Decision:** the canonical snapshot resolved it to execute, queue, coexist, suppress/retain, replace, expire, or reject.
3. **Application:** the selected owner started an action or applied movement input.
4. **Observed result:** CharacterMovement mode, root motion, and montage ownership allowed visible motion/presentation.

`Captured` never means executed, and `Queued` never means lost. A bounded `FCombatIntentRecord` records intent, edge, immutable world-space facing context, route, disposition, reason, primary owner/generation, hold generation, movement mode, and whether root motion was active. Movement records are emitted only on dead-zone edges or a decision/reason change, not every frame.

Enhanced Input terminal handling is explicit:

- Move `Triggered` updates the current normalized sample; Move `Completed` and `Canceled` set it to zero and emit the dead-zone exit once.
- attack `Completed` is a physical release and may resolve a hold;
- attack `Canceled` clears physical-held state and terminates matching hold ownership without selecting a follow-up;
- Block `Canceled` behaves as release for guard intent cleanup.

## Normal Attack Buffer

There is one pending normal-attack slot. A newer eligible Light/Heavy press replaces the older pending normal attack and records the older edge as `Replaced`; same-type input is not silently rejected merely because a slot is occupied. The winning slot retains the physical edge's immutable alignment intent from Micro-Spec 02. Exactly one action may start from one arbitration boundary.

Chain-only and hold-owned progression are not normal-buffer entries:

- Chain input retains its existing no-fallthrough route.
- a directional hold follow-up is an internal `HoldOwned` continuation tied to the source hold generation.
- the hold-owned continuation wins its handoff before the pending normal slot. Once the follow-up attack starts, the pending normal input follows that attack's ordinary queue/cancel rules.

## Hold Commitment

`FHoldState` becomes an attack-qualified substate, not a second primary action state machine. Its identity contains the source `FAttackInstanceId`, montage instance/source identity, monotonically increasing hold generation, input type, and captured source AttackData.

Closed phases are:

- `Inactive`
- `EaseIn`
- `FrozenAwaitingRelease`
- `ReleaseBlend`
- `FollowUpHandoff`

The hold commits only when the authored hold-start notify fires for the active attack generation and the matching button is still held. Physical hold duration remains diagnostic only and never decides eligibility.

Once committed, ordinary attack input cannot directly start a montage or clear the hold. It is captured into the one pending normal slot unless terminal policy rejects it. Voluntary preemption requires a matching exact-generation cancel window from Micro-Spec 04B.

On physical release, normalized world direction, desired yaw, derived branch direction, and source data are sampled once from the current post-modifier movement value, and the state enters `ReleaseBlend`. If the hold reached completion and a valid directional continuation exists, `FollowUpHandoff` starts it exactly once. If no continuation exists or release occurred before completion, the hold terminates cleanly and the normal slot may execute at that explicit boundary.

Death, owning paired entry, a committed external interruption, montage failure/interruption, owner destruction, and EndPlay terminate only the matching hold generation and cannot execute its follow-up.

## Hold Policy Matrix

| Hold phase | Movement | Ordinary Light/Heavy | Physical release | External terminal/interrupt |
|---|---|---|---|---|
| Inactive | normal action policy | normal buffer policy | cleanup only | owner policy |
| EaseIn | suppress and retain by default | replace pending normal slot | begin release blend, no follow-up unless completed | terminate exact hold |
| FrozenAwaitingRelease | suppress and retain by default | replace pending normal slot | snapshot direction and begin release blend | terminate exact hold |
| ReleaseBlend | suppress and retain by default | replace pending normal slot | idempotent | terminate exact hold |
| FollowUpHandoff | suppress until handoff commits | replace pending normal slot | idempotent | terminate exact hold |

An authored 04B cancel window may override the voluntary columns after its minimum commitment boundary. It never overrides death/paired/external interruption policy.

## Movement Ownership

Ordinary hold suppression does not call `DisableMovement()`, `SetMovementMode()`, `SetIgnoreMoveInput()`, or restore `MOVE_Walking`. `APlayerCharacter` asks CombatComponent whether the current movement sample may be applied, then calls `AddMovementInput()` only when permitted. Continuous samples may drive movement, but only dead-zone edges participate in cancellation policy.

Paired sequences and terminal death retain their existing explicit movement-mode ownership. Later full-body reaction ownership is introduced in Micro-Spec 05. No owner may restore a mode it did not acquire; a hold ending while paired/dead cannot resurrect locomotion.

## Acceptance

- Red tests reproduce movement-suppressed/attack-routable hold behavior, recovery-phase hold preemption, duplicate queue rejection, multi-start boundary risk, and stale movement direction.
- Table tests cover every hold phase against Movement, Light, Heavy, release, cancel, paired, interruption, and death.
- A normal attack never starts during committed hold phases without an exact matching 04B cancel decision.
- one hold continuation starts once and before the one pending normal attack; stale generations cannot start either.
- every captured combat edge and movement decision change has a terminal disposition and reason.
- hold suppression never changes CharacterMovement mode; paired/death mode remains unchanged across hold cleanup.
- `HoldWindowTests` no longer assert that a second activation may replace a live hold; duration tests state that duration is telemetry only.
- PIE proves held movement, released movement, Light/Heavy presses, early/complete release, directional/no-direction release, damage, and paired entry at each hold phase without unexplained lockout.
- the PIE evidence record can distinguish concurrent player/enemy montages from multiple starts by the same owner and generation.
