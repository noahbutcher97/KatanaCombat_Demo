# Micro-Spec 04B: Action Arbitration And Cancel Windows

## Purpose

Make movement, guard, attack, and evade priority explicit without creating a second lifecycle-owning combat state machine. Implement the currently scaffolded cancel-window concept as an exact-generation runtime contract. This spec depends on the capture, one-slot buffer, movement routing, and exact hold ownership defined by [Micro-Spec 04A](04a-input-availability-and-hold-commitment.md).

## Precedent

Lyra's [ability activation groups](https://dev.epicgames.com/documentation/en-us/unreal-engine/abilities-in-lyra-in-unreal-engine) distinguish independent actions, replaceable exclusives, and blocking exclusives. Unreal [Gameplay Abilities](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-gameplay-abilities-in-unreal-engine) also separate cancellation and blocking tags. KatanaCombat adopts the explicit decision vocabulary while keeping its existing component architecture and input queue.

## Canonical Snapshot

`UCombatComponent::BuildCombatActionSnapshot()` gathers state once in this priority:

1. death/dying;
2. owning paired sequence;
3. active stagger/full-body reaction;
4. active attack and phase;
5. evade when implemented;
6. none.

The snapshot also carries guard-held, guard-active, normalized movement magnitude, action owner, action generation, exact hold phase/generation, and the exact cancel-window policy. It is a projection of current owners, not separately mutable lifecycle state.

`GetCombatState()` remains a legacy animation/interface projection and must include `HitStunned`, `Finishing`, and `Dead` correctly. Gameplay decisions use the richer snapshot.

## Resolver Contract

`FCombatActionResolver::Resolve(const FCombatActionQuery&)` is pure. Query includes one intent (`Movement`, `Guard`, `LightAttack`, `HeavyAttack`, or `Evade`), the canonical snapshot, event edge/held state, and queue context. Decision includes:

- disposition: `Reject`, `ExecuteNow`, `Coexist`, `Queue`, or `CancelPrimaryThenExecute`;
- reason code;
- exact generation eligible for cancellation;
- whether guard intent should be retained, activated, suspended, or cleared.

The resolver performs no input capture, component lookup, montage operation, movement call, or delegate broadcast.

## Cancel Window Contract

Add `UAnimNotifyState_CancelWindow` and a bitmask `ECancelIntentFlags` with `Movement`, `Guard`, `LightAttack`, `HeavyAttack`, and `Evade`.

Notify Begin opens a record keyed by:

- primary action owner and generation;
- montage instance ID;
- notify runtime source identity;
- allowed-intent mask.

The record also carries a finite authored blend-out duration, initially 0.12 seconds. The resolver returns that value with `CancelPrimaryThenExecute`; the owning component commits terminal state and releases its alignment/root-motion ownership before stopping the montage with the selected blend. Direct zero-blend cancellation is reserved for terminal death/teardown paths.

Notify End closes only that record. Montage end, action terminal, owner death, and component end-play close matching records idempotently. A late End cannot close a newer action's window.

An action may also publish `MinimumUncancellableSeconds`. The resolver rejects cancel requests before that simulation-time boundary even if a malformed or early notify is open. The initial full-body-reaction default is 0.12 seconds; authored data may lengthen it but may not bypass finite/range validation.

Cancel windows are animation-owned timing, while the allowed intent mask is authored semantics. Existing `FAttackPhaseTimingOverride` cancel timing remains editor-generation data; runtime authority is the notify.

Active attack and full-body-reaction montages are migrated through audit/plan/apply. The operation resolves montages and sections from active AttackData and HitReaction settings, reports existing timing, and writes only individually reviewed cancel windows. It does not blanket-seed every montage or infer a window from duration alone.

## Input Semantics

- Every input edge is captured before action eligibility, preserving the existing global rule.
- Normal attack buffering uses the single last-input-wins slot defined by 04A. `ChainOnly` and `HoldOwned` routes cannot be displaced by that slot.
- A committed hold is part of the active Attack snapshot. Ordinary input queues behind it unless the resolver returns `CancelPrimaryThenExecute` for a matching hold/attack generation and authored allowed intent.
- Movement crossing the CombatSettings `MovementIntentDeadZone` (initially 0.20 after Enhanced Input modifiers) creates one intent edge. The input-asset audit must report any stricter modifier so effective behavior is intentional.
- Opening a movement-enabled cancel window checks currently held movement once.
- Holding movement does not create per-tick queue entries.
- A movement edge received during the minimum uncancellable interval remains held intent; it is reconsidered when the first valid cancel boundary opens rather than executing immediately.
- Block Press always records `bGuardInputHeld`; `bGuardActive` changes only when the decision permits.
- A full-body reaction suspends guard activity but retains held intent. Guard resumes at an allowed boundary only if Block remains held.
- Death and owning paired sequences reject all voluntary cancellation except their existing explicit Chain inputs.

## Initial Policy Matrix

| Primary action | Movement | Guard | Attack | Evade |
|---|---|---|---|---|
| None | coexist | activate overlay | execute | execute when implemented |
| Attack with committed hold, no matching cancel | suppress/retain | hold intent only | replace pending normal slot | reject/queue by authored policy |
| Attack with committed hold, matching cancel | cancel if allowed | cancel/activate if allowed | cancel if allowed; otherwise queue | cancel if allowed |
| Attack, window closed | locomotion suppressed by action | hold intent only | queue by existing combo rules | reject/queue by authored policy |
| Attack, matching cancel open | cancel if allowed | cancel/activate if allowed | cancel or queue if allowed | cancel if allowed |
| Full-body reaction, window closed | held only | hold intent only | reject | reject |
| Full-body reaction, matching cancel open | cancel if allowed | cancel/activate if allowed | reject unless explicitly authored | cancel if allowed |
| Stagger/paired/death | reject | hold or clear according to terminal rule | reject except owning Chain route | reject |

Movement cancel blends out the owned primary montage, closes its generation, then allows CharacterMovement input. It never calls `SetActorRotation` or directly clears another component's state.

## Acceptance

- Pure matrix tests cover every primary-action/intent combination and reason code.
- Notify tests cover stale End, interrupted montage, overlapping notify instances, and exact-generation cleanup.
- Input tests prove capture-before-eligibility, one movement edge, held-state boundary evaluation, guard suspend/resume, and no Chain input regression.
- Hold tests prove no ordinary montage starts during 04A committed phases without a matching exact-generation cancel decision, while the one pending normal input remains observable.
- PIE proves movement and guard can cancel only at authored windows and cannot escape minimum reaction lock sections.
- A post-apply audit proves every accepted proof attack/full-body reaction has the reviewed cancel policy and no unrelated package was saved.
