# Combat Action And Reaction Stabilization Design

Date: 2026-07-19
Status: Reconciled after full-debug input, hold, queue, alignment evidence, and a read-only live-steering precedent audit; runtime implementation has not started

## Purpose

This document defines the target contract for the crash, attack-facing, enemy-orbit, action-cancellation, hit-reaction, trade, and animation-layering work discovered during the post-defense playtest. Current source remains authoritative until each implementation slice lands and passes its proof gates.

This design extends, rather than replaces:

- `docs/superpowers/specs/2026-07-16-defense-interaction-design.md`
- `docs/superpowers/specs/2026-07-02-combat-semantics-ownership-design.md`
- `docs/specs/PAIRED_ANIMATION_SPEC.md`

## Goals

- Eliminate the `UEnemyCombatAIComponent::ExecuteAttack()` reentrant null dereference and close equivalent stale-callback paths.
- Give regular attacks their own authored rotation rate and turn budget while retaining the existing owned Motion Warping executor.
- Carry the physical attack edge's world-space facing intent through queue delay so targetless attacks visibly turn toward player input.
- Let live player movement input shape an active regular attack's warped turn through a bounded per-attack policy without retargeting or moving the translation destination.
- Make enemy orbit movement stable, navigable, purposeful, and compatible with concurrent StateTree token tasks.
- Route movement, guard, attack, and evade intent through one narrow action policy without creating another lifecycle-owning state machine.
- Distinguish captured, queued, suppressed, applied, and visibly consumed input so every apparent lockout has an inspectable reason.
- Protect committed light holds and their directional handoff from ordinary montage replacement unless an authored cancel window permits it.
- Separate voluntary cancellation, external interruption, and animation coexistence.
- Resolve hit reactions deterministically from impact pressure, active-action resistance, attack policy, contact geometry, and terminal state.
- Support non-interrupting trades with additive flinches and interrupting hits with full-body reactions.
- Preserve guard locomotion and visibly distinguish guard, normal block impact, and perfect parry.
- Prove behavior through focused automation, asset validation, telemetry, and Editor/PIE playtests.

## Non-Goals

- Adopting Gameplay Ability System, Behavior Trees, EQS, RVO, or Detour Crowd as a prerequisite.
- Porting OperationPhoenix/OnSight's GAS, replication, raw-input polling, or ability-lifecycle implementation.
- Creating a universal action lease framework or another gameplay actor component.
- Letting montage slot conflicts decide gameplay priority.
- Reintroducing a posture meter or accumulating impact pressure across hits.
- Allowing additive flinches to alter weapon traces or damage in the first pass.
- Reauthoring imported animation packs in place.
- Claiming multiplayer replication or packaged-build acceptance.
- Resaving `Lvl_DefenseMatrix`, `Lvl_ThirdPerson1`, or their external actors as part of this work.

## Empirical Starting Point

- `ExecuteAttackData()` can synchronously end an attack, clear `SelectedAttack`, or destroy a participant before `ExecuteAttack()` dereferences `SelectedAttack->AttackMontage`.
- `UTargetingComponent::SetupAttackWarp()` caps an attack's authored `RotationSpeed` with defense `DefenseTurnRate` and `MaximumAutomaticTurn`.
- `FActionQueueEntry` does not retain attack direction. `SetupAttackWarp()` rereads mutable `LastMovementInput` at execution, so queued attacks can use a newer or stale facing sample.
- active targeted warp refresh hard-faces the selected actor, while targetless rotation has no terminal-aware live input path; neither supports per-attack steering behavior.
- player locomotion yaw is hard-coded to 180 degrees/second, while `APlayerCharacter::Move()` bypasses combat action policy.
- circling computes a point 30 degrees from the current radial angle, forces the exact circle radius, and submits `MoveToLocation` every 0.25 seconds. Unreal's `AAIController::MoveToLocation()` aborts the active request before issuing the next one.
- circling direction is randomized periodically, movement results are ignored, and authored `CircleSpeed` and `ApproachSpeed` are unused.
- `EActionWindowType::Cancel` exists, but no cancel notify or runtime allowed-intent policy exists.
- `Move()` reaches `AddMovementInput()` independently of combat policy, while light-hold easing sets CharacterMovement to `MOVE_None`; attack input remains routable during that movement suppression.
- an immediate Recovery-phase attack starts a new montage through `PlayAttackMontage()`, whose first action is to clear any committed hold. The hold has no attack-generation-qualified ownership barrier.
- duplicate normal attacks can be rejected as already queued, while queue processing walks newest-to-oldest and can start multiple matching entries at one phase boundary.
- hold cleanup forces `MOVE_Walking` even though paired animation and death can independently own movement mode.
- Move lacks Completed/Canceled bindings, and attack/Block lack Canceled cleanup, allowing stale direction or physical-held state after terminal Enhanced Input transitions.
- the 2026-07-19 PIE capture strongly corroborates multi-start queue processing: Heavy and Light both resolve against `LightAttack_3`, then both montage checkpoint sets are discovered at one phase boundary. The same capture proves normal Light is accepted while `DA_DirectionalAttack_F` is current.
- that capture cannot assign every checkpoint to an actor/generation or distinguish capture, decision, application, and visible result because component input/queue/hold/movement diagnostics were disabled and current checkpoint records omit owner identity.
- the later `Combat.Debug.All` capture preserved at `Saved/Logs/KatanaCombat-backup-2026.07.19-15.23.35.log` directly records hold activation immediately followed by Recovery queue execution and hold clear (2643-2726), 119 same-type queue rejections including one entry retained until terminal cleanup (8598-11583), and eight balanced movement-disable/enable pairs. Runtime reproduction is complete; implementation proof is not.
- the same full-debug capture records seven player `RotationTarget` requests with no attack target. The notify selects rotation-only mode, but character yaw commonly changes only 12-24 degrees toward 90-135-degree requests before interruption/replacement. Request setup works; attack-owned reachability and actual-yaw acceptance do not.
- damage currently chooses reaction/no-reaction from `bHasSuperArmor`; `StaggerPower` has no runtime consumer.
- current reaction montages are full-body `DefaultSlot` assets. Guard is also full-body, so movement continues while locomotion animation is visually replaced.
- `FHitReactionInfo` contains rich immutable contact data but no attacker/defender action snapshots or committed reaction decision.
- ordinary hit stun currently makes a target finisher-eligible, conflating action lock with stagger vulnerability.

## Primary Precedents

| Problem | Primary precedent | Project decision |
|---|---|---|
| Coexist, replace, and block semantics | Lyra activation groups and ability tag relationships | Borrow the explicit outcomes, not GAS or Lyra's ability runtime. |
| Gameplay versus presentation | Lyra damage execution and Gameplay Cues | Commit damage/action effects first; select and play cosmetic response afterward. |
| Concurrent animation | Montage groups, Slots, layered animation, blend masks, and additive animation | Retain one primary full-body lane; add non-root-motion guard and additive-reaction lanes. |
| Authored attack alignment | Motion Warping notify windows | Keep `UTargetingComponent` as the single owned executor; remove the accidental defense cap. |
| Input-shaped warped turns | OnSight angular blend policies and UE 5.6 modifier update order | Keep pure configurable modes, but separate steering from acquisition and update only the exact Katana alignment owner. |
| StateTree movement | StateTree concurrent task semantics | Orbit remains `Running`; token task completion owns the transition. |
| Tactical point selection | EQS generators/tests | Defer EQS until stable deterministic orbit proves a need for tactical scoring. |
| Crowd avoidance | Navigation RVO and Detour guidance | Use neither initially; never enable both. Stabilize path requests first. |
| Content safety | Data Validation and Animation Modifiers | Audit/plan/apply with package allowlists and validators; never blanket-save content. |
| Reentrant lifecycle | UE montage and Gameplay Ability cancellation source | Snapshot before external calls, mutate before broadcasts, revalidate after callbacks, and close exact generations once. |

Primary references are listed in each micro-spec. Version-specific conclusions were checked against installed UE 5.6 source, including `AIController.cpp`, `AnimInstance.cpp`, `RootMotionModifier.cpp`, and `AbilitySystemComponent_Abilities.cpp`.

The centralized research record is [Research References](2026-07-18-combat-action-reaction-stabilization/RESEARCH_REFERENCES.md).

## Target Ownership

| Owner | Responsibility |
|---|---|
| `UCombatComponent` | Capture input and the one terminal-aware movement/steering sample, expose the canonical action snapshot, own the one-slot normal buffer and exact hold continuation, resolve voluntary intent, own guard intent/activity and cancel-window records, and dispatch exact owner cancellation. |
| `FCombatActionResolver` | Purely decide reject, execute, coexist, queue, or cancel-then-execute. It owns no montage, timer, or state. |
| `UHitReactionComponent` | Build and commit reaction queries, own reaction generation/lifetime, apply action lock/stagger, and play selected presentation. |
| `FHitReactionResolver` | Purely decide reaction class, interruption, pressure/resistance comparison, and presentation key. |
| `UEnemyCombatAIComponent` | Own attack token lifecycle, AI combat state, orbit direction, movement-speed mode, and explicit interruption response. |
| StateTree tasks | Adapt component state to StateTree run status and path requests. They do not own combat outcomes. |
| `UTargetingComponent` | Preflight and execute attack/defense alignment under exact owner generations; consume a value copy of CombatComponent's live sample only for the exact regular player-attack modifier. |
| `USamuraiAnimInstance` | Compose locomotion, guard overlay, additive reaction, and primary full-body presentation. It resolves no gameplay. |

## Action Model

One scalar combat state cannot represent locomotion, held guard, and an additive flinch at the same time. The canonical snapshot therefore contains:

- one exclusive primary action: `None`, `Attack`, `Evade`, `FullBodyReaction`, `Stagger`, `Paired`, or `Death`;
- owner and generation of that primary action;
- attack phase and open cancel policy where applicable;
- guard input intent and guard active state as separate booleans;
- current movement intent and dead-zone state;
- non-owning additive presentation state;
- exact hold phase/generation when the active attack has committed a hold.

Existing components retain their lifecycle. `UCombatComponent::BuildCombatActionSnapshot()` projects their state once; it does not mirror it into a second state machine. `GetCombatState()` remains a compatibility projection with terminal/paired/reaction/attack/guard priority.

`FCombatActionResolver` accepts the immutable snapshot plus one input intent. Its closed dispositions are:

- `Reject`
- `ExecuteNow`
- `Coexist`
- `Queue`
- `CancelPrimaryThenExecute`

Voluntary cancellation is permitted only by the active generation's open cancel policy. External hit interruption never consults cancel windows; it uses reaction pressure versus action resistance. Death and paired terminal states reject voluntary cancellation.

Guard uses two states:

- `bGuardInputHeld`: physical intent retained until release.
- `bGuardActive`: defense eligibility and presentation currently permitted.

A hard reaction suspends active guard without fabricating a release. If Block remains held, guard may reactivate only at an allowed reaction boundary. Additive flinches coexist with guard.

Movement is captured as held intent. Crossing the configured dead zone requests a decision once; an open cancel window also re-evaluates currently held movement. Per-frame queue entries are prohibited.

Input handling has four observable layers: physical capture, policy decision, action/movement application, and resulting CharacterMovement/root-motion/montage behavior. A bounded reason-coded record distinguishes them. Continuous movement may be applied each frame when allowed, but history is emitted only for dead-zone edges or decision changes.

Normal Light/Heavy input uses one last-input-wins pending slot carrying the physical edge's immutable world-space facing intent. Facing resolves through explicit target, canonical soft target, captured nonzero world direction, then attack-edge facing; terminal-zero input preserves facing instead of reusing stale movement. Chain and committed-hold continuations have separate route ownership. Once a hold-start notify commits a matching active attack generation, ordinary attacks may replace the pending normal slot but may not start a montage or clear the hold until an exact cancel decision or hold terminal boundary. Release snapshots current direction once; a valid hold-owned follow-up starts exactly once before the normal slot becomes eligible. Hold suppression gates `AddMovementInput()` and does not change or restore CharacterMovement mode.

Immutable facing is the startup/queue reference, not a ban on intentional steering. During the exact regular player attack's warp window, a separate terminal-aware world-space movement sample may offset only the rotation target through `Disabled`, `Weighted`, `DeadZoneCurve`, or `ConeClamp` policy. The offset is bounded by authored deviation, response rate, remaining time/rate/budget, and final tolerance, then freezes at the effective damage deadline. It never changes acquisition, target identity, translation, action generation, montage, AI, defense, or paired/sync alignment.

## Reaction Model

At the target-authorized damage boundary, one `FHitReactionQuery` combines:

- immutable `FHitReactionInfo`;
- committed damage and damage/max-health ratio;
- attacker and defender action snapshots gathered once;
- attack `StaggerPower` interpreted as normalized impact pressure;
- a closed authored impact policy;
- current action-phase interruption resistance;
- existing `EAttackHeight`, `EIncomingAttackLane`, bone, point, normal, surface, and confidence data;
- death, paired, invulnerability, and compatibility super-armor state.

The closed reaction classes are `DamageOnly`, `AdditiveFlinch`, `FullBodyReaction`, `Stagger`, `Knockdown`, and `Death`. `EReactionOutcome` remains the post-animation recovery/death/ragdoll contract and is not reused for impact classification.

Default resolution is deterministic:

| Condition | Result |
|---|---|
| lethal committed damage | `Death`; no nonlethal reaction starts |
| paired/terminal policy suppresses response | `DamageOnly` or paired-owned death path |
| explicit `DamageOnly` policy or compatibility super armor | `DamageOnly` |
| explicit stagger/knockdown policy | requested class, subject to terminal guards |
| pressure greater than active action resistance | `FullBodyReaction` and interrupt primary action |
| pressure less than or equal to resistance | `AdditiveFlinch`; primary action and AI token continue |
| selected presentation missing | preserve gameplay decision; use deterministic fallback or no cosmetic response |

Pressure does not accumulate and is not random. Damage ratio may choose cosmetic intensity but does not alter interruption by default. Attack actions expose per-phase resistance; paired/death are non-interruptible through this path. `bHasSuperArmor` remains a deprecated compatibility override until asset inventory proves removal is safe.

Only explicit `Stagger` creates finisher vulnerability. Ordinary reaction action-lock time does not. Low-health finishers remain independently governed by the paired-animation spec.

## Animation Composition

- Existing `DefaultGroup.DefaultSlot` remains the intentional primary full-body lane for the first migration.
- `GuardOverlay.Guard` is non-root-motion and layered over locomotion.
- `AdditiveReaction.Reaction` is non-root-motion and coexists with the primary lane.
- Perfect parry uses presentation distinct from held guard and normal block impact.
- Full-body heavy reactions retain interruption and optional root motion.
- Additive reaction masks must keep active weapon-socket trajectory drift within a validated tolerance. Trace or damage modification from flinch is deferred.
- Reaction selection uses the existing three heights and three incoming lanes, with bone-derived provenance and deterministic sparse fallback.

Project-owned derived assets belong under `Content/ProjectFiles/Animation/`. Imported source assets under vendor directories are read-only inputs.

## Event And Cleanup Ordering

For every attack, cancellation, and reaction transition:

1. Capture immutable inputs and weak participants.
2. Register or verify the exact owner generation.
3. Commit gameplay state, damage, exact action interruption, and token disposition. Internal lifecycle notifications needed to complete that commit occur here and are generation-filtered.
4. Revalidate after any call that can play/stop a montage, broadcast, apply damage, or destroy an actor.
5. Start optional presentation.
6. Broadcast public/observable delegates last.

Cleanup is idempotent. Notify End, montage end, owner death, StateTree exit, and component end-play may close only the generation they own. Additive damage does not release an AI attack token; an explicit committed interrupt, parry, death, or normal attack completion does exactly once.

## Implementation Order

1. Crash and attack-lifecycle hardening.
2. Attack alignment, bounded live steering, and locomotion rotation ownership.
3. Stable enemy orbit and StateTree movement.
4. Input availability, hold commitment, action arbitration, and real cancel windows.
5. Hit-reaction resolution, trades, and finisher-vulnerability separation.
6. Animation layering and allowlisted content migration.
7. Full validation, visible proof, and documentation reconciliation.

Each step has its own spec and implementation plan. A step may not claim the next step's behavior as proof.

## Micro-Spec Index

- [01 Lifecycle Crash Hardening](2026-07-18-combat-action-reaction-stabilization/01-lifecycle-crash-hardening.md)
- [02 Alignment And Locomotion Rotation](2026-07-18-combat-action-reaction-stabilization/02-alignment-and-locomotion-rotation.md)
- [03 Enemy Orbit And StateTree Movement](2026-07-18-combat-action-reaction-stabilization/03-enemy-orbit-and-state-tree-movement.md)
- [04A Input Availability And Hold Commitment](2026-07-18-combat-action-reaction-stabilization/04a-input-availability-and-hold-commitment.md)
- [04B Action Arbitration And Cancel Windows](2026-07-18-combat-action-reaction-stabilization/04-action-arbitration-and-cancel-windows.md)
- [05 Hit Reactions And Trades](2026-07-18-combat-action-reaction-stabilization/05-hit-reactions-and-trades.md)
- [06 Animation Layering And Content Migration](2026-07-18-combat-action-reaction-stabilization/06-animation-layering-and-content-migration.md)
- [07 Validation And Visible Proof](2026-07-18-combat-action-reaction-stabilization/07-validation-and-visible-proof.md)

The hostile review and resulting plan changes are recorded in `docs/superpowers/plans/2026-07-18-combat-action-reaction-stabilization/ADVERSARIAL_AUDIT.md`.

## Acceptance

The design is ready to implement only when every micro-spec maps to a micro-plan, every plan has a failing-test-first path and commit boundary, all high/medium adversarial findings are resolved in the documents, and the plan explicitly preserves current user map WIP. Runtime acceptance additionally requires a clean editor build, all `KatanaCombat` automation tests, data validation, and Editor/PIE evidence for crash freedom, static and live-steered attack alignment, orbit stability, reason-coded input availability, committed-hold continuity, movement cancellation, guard locomotion, distinct parry presentation, additive trades, full-body interruption, and exact AI token cleanup.
