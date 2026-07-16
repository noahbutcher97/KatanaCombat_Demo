# Defense Interaction Design

Date: 2026-07-16
Status: Draft for written-spec review

## Purpose

This document defines the canonical runtime contract for guard entry, normal block, perfect parry, attacker recoil, defensive alignment, and the transition from parry into the existing Chain Counter system. It elaborates the ownership rules in `2026-07-02-combat-semantics-ownership-design.md` and the Chain Counter contract in `docs/specs/PAIRED_ANIMATION_SPEC.md`.

The design is deliberately narrower than a universal combat transaction. It adds a typed defense boundary while preserving the five existing runtime components and `FHitReactionInfo` compatibility.

## Goals

- Resolve each incoming attack against one defender exactly once.
- Make normal block available while Block is held, independent of parry timing.
- Make perfect parry edge-triggered by Block Press during an attacker-owned parry window.
- Select one stable incoming attack in multi-attacker situations.
- Use one configurable rotation capability for smooth facing and Motion Warping.
- Select block, parry, recoil, VFX, and audio presentation from attack context.
- Preserve attack identity and paired-sequence context across asynchronous animation callbacks.
- Transition from perfect parry through paired counter and finisher without a teardown gap.
- Produce automation, asset-validation, telemetry, and PIE evidence for claimed behavior.

## Non-Goals

- Replacing the five-component architecture or creating another actor component.
- Replacing every damage call with a universal transaction hierarchy.
- Reintroducing posture as the primary defense system.
- Migrating the paired-animation system to Contextual Animation.
- Requiring Chooser for the first implementation slice.
- Adding multiplayer replication in this slice. The contracts must remain authority-compatible.
- Bulk-seeding parry, counter, sync, or collision notifies without reviewed asset timing.
- Defining guard-break gameplay before the project has a canonical non-posture guard resource or explicit guard-break rule.

## Canonical Semantics

### Guard

Guard is a held defender state. A valid Block Press enters guard even when no attack is currently parryable. Guard remains active until Block Release, death, an owning paired sequence, or another explicit state transition ends it.

Holding Block does not automatically parry an attack whose parry window opens later. Perfect parry is evaluated only on the Block Press edge. Releasing and pressing again creates a new attempt.

### Normal Block

Normal block is a contact-stage outcome. It occurs when an attack physically connects while the defender is guarding, the attack is blockable, and the defender satisfies the final alignment requirement. It suppresses damage and selects defender impact presentation. It may interrupt the attacker only when attack semantics allow that response.

### Perfect Parry

Perfect parry is an input-intent outcome committed before physical contact. It requires an active attacker-owned parry window, explicit parry capability, valid defender state, hostile team relationship, and reachable perfect-parry alignment. It consumes the identified attack instance, prevents its later hit from resolving, and may open a Chain Counter sequence.

### Counter Start

Counter start is not another classification of the incoming hit. It is a later Chain state transition caused by attack input after a committed perfect parry and completed parry bridge. The perfect parry captures the instigating attack context; the counter input selects the defender's `UAttackData` and therefore its `CounterData` and `FinisherData`.

## Architecture And Ownership

| Owner | Responsibility |
|---|---|
| `UCombatComponent` | Own guard state, gather candidate snapshots, lock one threat, call the resolver, commit defense gameplay, and expose the active attack generation. |
| `FDefenseResolver` | Pure synchronous candidate scoring and outcome calculation. It performs no world query, mutation, montage playback, damage, or delegate broadcast. |
| `ABaseCombatCharacter` | Adapt weapon contact into one defense query, apply the returned damage policy once, and give the same resolution back to impact presentation. |
| `UWeaponComponent` | Produce physical contacts and hit metadata. It does not decide block or parry outcomes. |
| `UTargetingComponent` | Execute an approved alignment request and maintain warp targets. It does not decide defense eligibility. |
| `UHitReactionComponent` | Apply hit, block-impact, recoil, stagger, and death presentation selected by the committed result. |
| `UPairedAnimationComponent` | Own retained parry/counter/finisher sequence state, partner state, paired warp lifetime, input blocking, and terminal cleanup. |
| `USamuraiAnimInstance` | Present guard enter/loop/exit state and consume animation-facing state. It does not resolve gameplay. |
| `UDefenseConfiguration` | Own defense kinematics, threat-lock policy, and sparse presentation rows for a character or stance. |

Shared cross-component enums, structs, and delegates belong in `CombatTypes.h`. The pure resolver belongs in a focused non-component runtime file. This does not create a sixth gameplay component.

## Typed Contracts

### Attack Identity

`FAttackInstanceId` contains a weak attacker reference and the attacker's existing `AttackGeneration`. It is valid only while the actor exists and its `UCombatComponent` still reports the same generation. Perfect parry consumes this whole attack instance.

An active parry, counter, or hit window additionally carries a window generation, kind, start time, and end time. Notify End may close only the matching window instance. A late callback from an old montage or section cannot close or resolve a newer attack window. `FContactInstanceId` combines the attack instance and hit-window generation so a deliberately authored multi-hit attack can create separate contacts while repeated traces from one hit window remain duplicates.

`FDefenseInteractionId` combines the attack or contact instance, weak defender reference, query stage, and an epoch allocated when that interaction is first registered. Repeated traces look up the existing ID by contact instance and defender rather than allocating a new epoch. It is the duplicate-suppression and telemetry key; it is not a globally replicated GUID.

### `FAttackExecutionSnapshot`

The immutable snapshot passed into resolution contains:

- `FAttackInstanceId`, source `UAttackData`, active montage, section, section time, and attack phase.
- Active parry/counter window identity and timing.
- Attack tags and attack type.
- Authored body target and nominal lane.
- `ESwingDirection` as swing shape, not movement direction.
- Intended target when known, predicted attack path, predicted contact point, source socket, defender target bone, time to alignment deadline, and prediction confidence.
- Attacker transform, velocity, alive state, and team identity at query construction.

The snapshot is gathered once per query. Resolver code must not look up mutable component state a second time.

Null `AttackData` remains a contact-path compatibility case: it carries no authored defense tags, may be normally blocked using defender state and geometry, and cannot qualify perfect parry. This preserves existing generic hit behavior without fabricating parry capability.

### `FDefenseQuery`

The query contains the immutable attack snapshot plus:

- `EDefenseQueryStage`: `InputIntent` or `Contact`.
- Weak defender reference, defender transform, state, tags, team, and guard state.
- Block Press timestamp for intent queries.
- Actual `FHitReactionInfo` contact data for contact queries.
- Relative yaw, time to deadline, current threat-lock state, and effective defense policy.

### `FDefenseResolution`

The immutable result contains:

- Interaction ID, stage, explicit outcome, and reason code.
- The selected attack instance and locked threat.
- Resolved body height, defender-relative lane, swing shape, contact point, source socket, and target bone.
- Measured yaw, available turn, required final tolerance, and prediction confidence.
- Damage disposition, attacker response, alignment request, and selected presentation payload.
- Whether a perfect parry may open a Chain sequence.

The initial closed outcome set is:

- `Rejected`
- `GuardEntered`
- `PerfectParry`
- `NormalBlock`
- `Hit`
- `UnblockableHit`
- `IgnoredFriendly`
- `IgnoredConsumed`
- `IgnoredInvalid`

Reason codes explain why an outcome occurred without multiplying gameplay outcomes. Examples include invalid state, stale attack, no parry window, missing parry capability, outside hard cone, unreachable alignment, not guarding, outside contact tolerance, unblockable, duplicate, and friendly-fire disabled.

### `FDefenseSequenceContext`

Only `UPairedAnimationComponent` retains this context. It contains the originating interaction and attack snapshot, weak actors, selected counter `UAttackData`, resolved `CounterData`, resolved `FinisherData`, active Chain state, stage generation, timeout handles, active presentation data, and cleanup ownership flags.

Actor references are weak and revalidated at every stage. Referenced data assets are reflected strong references while the sequence is active. Cleanup is idempotent and keyed to the stage generation so stale callbacks cannot end a newer stage.

## Resolution Stages

### Input Intent: Block Press

1. `UCombatComponent` validates that the defender can enter guard. The initial slice preserves the current Idle/Blocking restriction and does not add attack-to-block cancellation.
2. It gathers hostile, alive, currently attacking candidates once and records whether each attack credibly targets the defender. Explicit target intent or a high-confidence predicted path through the defender's threat volume is required for perfect parry. Lower-confidence attacks may guide normal guard facing but cannot qualify perfect parry.
3. `FDefenseResolver` ranks the snapshots and returns one candidate. The same candidate is used for parry evaluation and guard alignment.
4. The resolver evaluates the input-intent matrix below.
5. `UCombatComponent` commits `GuardEntered` or `PerfectParry` once.

If no candidate qualifies, guard still starts without an auto-turn target. If perfect timing exists but only normal-block alignment is reachable, the result is `GuardEntered`; this is the canonical parry-to-block downgrade.

### Physical Contact

1. The attacker constructs `FHitReactionInfo` and an attack snapshot from the active attack instance.
2. The defender's `UCombatComponent` resolves the contact once.
3. The defender applies the returned damage disposition exactly once.
4. The attacker uses that same result for VFX, audio, hitstop, recoil, and delegates. It must not call `IsBlocking` or `CanBlockHit` again.

Generic `IDamageableInterface` targets retain the legacy best-effort path until they expose a hit-aware defense contract. Rich defense claims apply to `ABaseCombatCharacter` targets only.

## Outcome Matrix

### Input Intent

| Conditions | Outcome | Required behavior |
|---|---|---|
| Defender dead, paired-input blocked, attacking, or otherwise unable to guard | `Rejected` | Do not change guard, target, attack, or sequence state. |
| No valid hostile candidate | `GuardEntered` | Enter guard facing the current direction; do not snap or fabricate attack timing. |
| Candidate exists but has no active parry window or no `Attack.Defense.Parryable` tag | `GuardEntered` | Lock only if it is an active incoming threat; begin capped guard facing. |
| Parryable window is active but attack identity is stale | `GuardEntered` | Discard the stale candidate and do not consume any attack. |
| Parry timing is valid but the candidate is outside the hard guard cone | `GuardEntered` | Enter guard with no auto-turn to that candidate. |
| Parry timing is valid and normal alignment is reachable, but perfect tolerance is not | `GuardEntered` | Lock the candidate and align at the normal capped rate. |
| Parry timing, capability, team, state, and perfect alignment are valid | `PerfectParry` | Consume the attack instance, suppress later contact, apply parry attacker response, and attempt the parry bridge. |
| Attack is both unblockable and parryable | Matrix above | Unblockable bypasses normal block only; it does not override explicit parry capability. |

### Contact

| Conditions | Outcome | Damage and presentation |
|---|---|---|
| Same team and friendly fire is disabled | `IgnoredFriendly` | No damage, reaction, hitstop, impact FX/audio, paired transition, or hostile-target consumption. |
| Attacker or defender is invalid/dead before commit | `IgnoredInvalid` | No combat side effects. |
| Attack instance was already consumed by perfect parry | `IgnoredConsumed` | No side effects. |
| Duplicate interaction or stale attack identity | `IgnoredConsumed` | No side effects; emit diagnostic telemetry. |
| `Attack.Property.Unblockable` is present | `UnblockableHit` | Apply normal hit damage and hit presentation even while guard is held. |
| Defender is not guarding | `Hit` | Apply normal hit damage and hit presentation. |
| Defender is guarding but actual contact yaw exceeds normal-block tolerance | `Hit` | Apply normal hit damage; guard may remain held unless hit reaction cancels it. |
| Defender is guarding, attack is blockable, and contact alignment is valid | `NormalBlock` | Apply zero damage, block presentation, and the configured attacker response. |

Height and lane select presentation; they do not silently change block eligibility in the initial slice. Future stance-specific coverage must be an explicit policy and matrix extension.

## Threat Selection And Locking

Candidate ranking is deterministic:

1. Hostile, alive, active attack with valid attack identity; credible intent or trajectory toward this defender outranks low-confidence guard-only threats.
2. Earliest non-negative predicted contact or parry-window deadline.
3. Reachable alignment at that deadline.
4. Highest prediction confidence.
5. Smallest absolute defender-relative yaw.
6. Shortest distance.
7. Stable actor identity as the final tie-break.

The locked threat remains stable for `ThreatLockMinSeconds`. A new threat may replace it only when the current threat becomes invalid or the new threat reaches contact at least `ThreatSwitchLeadSeconds` earlier. Failed perfect-parry evaluation and fallback guard entry use the same selected candidate; they never perform a second world scan.

Missing or low-confidence timing may guide normal guard facing but cannot qualify a perfect parry. The resolver must downgrade rather than invent timing confidence.

For input-intent resolution, the canonical alignment deadline is the earliest available high-confidence predicted contact, active parry-window end, or selected parry bridge's authored deflection marker. Parry-window authoring therefore places the window end at or before the intended incoming contact. A window or bridge with no reliable deadline cannot qualify perfect parry.

## Alignment Policy

For a candidate with yaw error `Y`, time to deadline `T`, maximum turn rate `R`, final tolerance `F`, hard cone `H`, and maximum automatic turn `M`:

```text
AvailableTurn = min(M, max(T, 0) * R)
Reachable = abs(Y) <= H && abs(Y) <= F + AvailableTurn
```

Perfect parry uses a tighter `F` than normal block. Actual normal-block contact is accepted only when the measured contact-frame yaw is within normal-block tolerance; a future warp cannot retroactively make an invalid block valid.

Initial proof defaults are data-driven but start from current project behavior:

| Setting | Initial value |
|---|---:|
| Hard guard cone half-angle | 70 degrees |
| Maximum automatic turn | 70 degrees |
| Defense turn rate | 180 degrees/second |
| Normal-block final tolerance | 35 degrees |
| Perfect-parry final tolerance | 10 degrees |
| Center-lane half-angle | 12 degrees |
| Threat lock minimum | 0.15 seconds |
| Threat-switch lead | 0.10 seconds |
| Normal-block translation allowance | 0 cm |
| Perfect-parry translation allowance | 75 cm maximum |

`UDefenseConfiguration` is the authority for the defense turn rate. A presentation or attack may lower the effective rate but may not raise it above the character or stance capability.

Guard facing uses capped smooth rotation. A block-contact montage may continue rotation with Motion Warping. Perfect-parry and paired stages may use bounded translation and rotation warping. Every path uses the same effective rate and the engine's clamped rotation method. Direct `SetActorRotation`, teleporting, or an uncapped warp is prohibited in defense alignment.

Sustained guard facing uses an opt-in `UTargetingComponent` alignment request updated only while guard has a valid locked threat. Character rotation is applied through the Character Movement rotation path with a clamped per-frame delta; no permanently enabled combat scan is introduced. Threat validity and switch cadence are owned by `UCombatComponent`, while `UTargetingComponent` only executes the current request.

Warp targets are prepared before montage playback. Runtime code configures a copied root-motion modifier and must not mutate a shared notify template. Warp failure never changes an already committed gameplay outcome.

All reachability time is expressed in simulation seconds after montage play rate and time dilation are accounted for. Smooth facing and Motion Warping must remain under the same cap during slow motion and non-default montage play rates.

## Direction, Height, And Contact Data

The following axes remain independent:

- `EAttackHeight`: `High`, `Middle`, or `Low`; authored nominal body target.
- `EIncomingAttackLane`: `Left`, `Center`, or `Right`; resolved from the defender's perspective.
- `ESwingDirection`: `Horizontal`, `Vertical`, `Thrust`, `Sweep`, or `Grab`; swing shape.
- Source socket and defender target bone: exact attachment and IK/VFX contact identifiers.

`EAttackDirection` remains movement/targeting direction and must not be reused for incoming lane. Left and right are always defender-relative.

Authored nominal lane is a fallback and validation hint. Runtime predicted lane is used before contact when confidence is sufficient. Lane is calculated from the signed incoming direction in defender local space, using the configured center dead zone. Actual `FHitReactionInfo` point, normal, and bone become authoritative for contact VFX and IK. Actual bone mapping may refine body height for contact presentation, while pre-contact presentation retains the authored height. Predicted and actual data are stored separately so telemetry can measure prediction error.

## Authoring Model

### Attack Data

`UAttackData` gains a compact defense profile containing attack height, nominal incoming lane, swing shape, an optional source-contact override, and defender target-bone fallback. The source socket normally comes from the active weapon trace and attack hand. Existing `DefaultContactBone` is migrated or adapted rather than duplicated silently.

Tags retain open-ended attack semantics:

- `Attack.Property.Unblockable`: normal block cannot stop the attack.
- `Attack.Defense.Parryable`: the attack may be perfectly parried while its parry window is active.
- `Attack.Defense.BlockInterruptible`: a normal block may interrupt the attacker and request recoil.

Parry capability and parry timing are separate requirements. The tag supplies capability; `AnimNotifyState_ParryWindow` supplies timing. Neither alone is sufficient. An unblockable attack may deliberately also be parryable. `BlockInterruptible` has no effect when the outcome is not `NormalBlock`.

No tag implies that an animation, VFX, or sound exists. Those remain data references and are validated separately.

### Defense Configuration

`UDefenseConfiguration` contains:

- Kinematic and threat-lock policy.
- Optional guard enter/exit montages. The held guard pose remains the existing AnimBP state driven by `bIsBlocking`.
- Sparse defense presentation rows.
- Generic block and perfect-parry fallbacks.
- Default impact VFX/audio and attacker-response fallbacks.

A presentation row key may use outcome, attack height, incoming lane, swing shape, weapon/stance tags, and explicit priority. Its payload may contain a defender montage/section or paired parry-bridge data, blend settings, warp settings, VFX/audio overrides, hitstop, and contact overrides. Attacker recoil is selected from the attacker's configuration using the committed response key, not from the defender's animation set.

When paired parry-bridge data is selected, that paired asset owns both actors' bridge montages. Otherwise, defender presentation and attacker response are selected independently from their respective configurations.

Selection fallback is deterministic:

1. Exact outcome, height, lane, swing, and context match.
2. Outcome, height, and lane.
3. Outcome and height.
4. Generic outcome fallback.
5. No montage, while preserving the committed gameplay outcome.

Validation rejects ambiguous rows with equal specificity and priority. Missing specialized animation is not a gameplay failure, but proof assets require a valid visible fallback. A selector interface keeps the policy independent from storage. The first implementation uses the sparse deterministic table; a Chooser-backed selector may be added later without changing resolver contracts.

Impact payload precedence preserves existing authored data: exact defense-row override, attack-specific blocked impact config, defender generic defense fallback, then existing weapon/global fallback. A null defense configuration uses the C++ migration defaults above and no-montage presentation, logs once, and never crashes. The proof character and proof enemies require explicit configurations before visible acceptance.

## Attacker Response

The closed attacker response set is `None`, `Continue`, `BlendOut`, `Recoil`, and `ParryStagger`.

- Normal block uses `Continue` unless `Attack.Defense.BlockInterruptible` requests `Recoil`.
- Recoil uses the best matching presentation row, then a generic recoil, then a safe montage blend-out.
- Perfect parry uses `ParryStagger` and consumes the source attack generation. Stagger duration and playback are data-driven; the current hardcoded duration is not canonical.
- Presentation failure cannot restore a consumed attack or turn a block into damage.

Repeated weapon traces from the same attack generation and defender must not replay recoil, block FX, damage, or hitstop.

## Chain Sequence Contract

The Chain state machine becomes:

```text
None
  -> ParryActive
  -> CounterWindow
  -> CounterActive
  -> FinisherReady
  -> FinisherActive
  -> None
```

`FinisherActive` removes the current ambiguity between a ready finisher and one already playing.

On `PerfectParry`, `UPairedAnimationComponent` captures `FDefenseSequenceContext`, configures partner and warp state, and starts the parry bridge. The state reaches `CounterWindow` only after the bridge's configured completion event or successful fallback completion. It does not transition immediately in the same call.

Attack input in `CounterWindow` resolves defender `UAttackData` through `UCombatComponent` and calls `TryAdvanceChainCounter(UAttackData*)`. The input is consumed only when the next stage preflights and starts successfully.

Counter completion that should auto-continue updates the retained stage, paired data, and warp targets in place. It must not call global paired teardown, clear the action queue, expose `ChainState::None`, or restore collision/input between counter and finisher. Terminal cleanup occurs only after final completion, cancel, timeout, owner death, partner death, invalid target, or unrecoverable start failure.

Auto-continuing paired stages use an authored `AnimNotify_ChainStageTransition` before the outgoing montage ends. The paired-data owner designates one driver role; only that role's marker, matching the active interaction and stage generation, may advance the sequence. Partner markers and montage callbacks are telemetry or duplicate fallbacks. The next stage starts with explicit blend settings while the outgoing pose is still owned; `OnMontageEnded` is cleanup/failure fallback, not the normal counter-to-finisher trigger. Validation rejects auto-continuing proof data without one unambiguous driver marker.

All two-actor starts use preflight followed by rollback-safe execution:

1. Validate actors, state, animation instances, montages, sections, and required warp targets.
2. Establish context, partner collision, input ownership, and warp targets.
3. Start both montages with explicit blend settings.
4. If either start fails, stop any montage that started and restore only state owned by that stage.

If the parry bridge fails after perfect parry commit, the attack remains parried, the Chain sequence closes safely, and the defender remains in or returns to guard. If counter start fails, its input is not consumed and the existing CounterWindow may remain until timeout. If automatic finisher start fails while the partner remains valid, enter `FinisherReady` with input restored; otherwise perform terminal cleanup.

## Commit And Event Ordering

The required order is:

1. Validate attack and interaction identity.
2. Resolve without mutation.
3. Mark the interaction committed and, for perfect parry, consume the attack instance.
4. Apply gameplay state and damage disposition once.
5. Broadcast immutable `OnDefenseResolved` after gameplay commit.
6. Start selected presentation and broadcast component-local presentation started/failed telemetry.

No delegate broadcasts during pure resolution. Presentation listeners cannot mutate the committed outcome. VFX, audio, hitstop, and attacker response consume the same result that damage consumed.

Friendly contacts with friendly fire disabled produce no combat side effects and do not consume the attack against later hostile targets. Enemy-versus-enemy damage is disabled by default for the current proof configuration.

Equal non-neutral teams are friendly. Explicitly hostile teams may interact. Neutral or unknown-team actors retain the existing damageable contact path but are not auto-selected as parry or guard-facing threats without an explicit hostility result.

## Cancellation And Failure Rules

Every retained or asynchronous path validates the interaction ID and stage generation before acting. Terminal cleanup is idempotent and restores time dilation, collision, movement, input ownership, guard ownership, partner registration, and warp targets exactly once.

Required failure behavior:

- Attacker generation changes before commit: downgrade intent to guard or ignore stale contact.
- Block is released during normal guard: stop guard alignment and clear its threat lock.
- Block is released after perfect parry commit: the committed parry and owned Chain sequence continue.
- Owner or partner dies: cancel the sequence and remove both actors from paired participation.
- A dead, dying, or already paired actor cannot be selected for another paired sequence; terminal death invalidates outstanding sequence IDs.
- Selected threat becomes invalid: clear the lock and rescan only on the next normal selection opportunity.
- Prediction is missing or stale: disallow perfect parry qualification; normal contact remains authoritative.
- Presentation asset is missing: use deterministic fallback or no montage; never change damage after commit.
- Paired montage start partially succeeds: stop the started side and roll back stage-owned state.
- Duplicate hit or montage callback: ignore it using interaction and stage identity.
- A later hit kills an actor during another defense callback: subsequent interactions observe death and resolve as ignored/rejected.

## Debugging And Validation

Defense debug output must expose:

- Interaction and attack generation IDs.
- Candidate list, selected threat, and switch reason.
- Query stage, outcome, and reason code.
- Relative yaw, hard cone, available turn, and final tolerance.
- Parry-window start/end/progress and prediction confidence.
- Height, lane, swing shape, source socket, and target bone.
- Selected presentation row and fallback level.
- Chain state/stage generation and terminal cleanup reason.

Asset validation and migration reports must verify:

- Parryable tag and parry-window agreement.
- Defense-profile completeness.
- Counter/finisher boolean and reference agreement.
- Montage and section validity for every selected payload.
- Sparse presentation ambiguity and fallback coverage.
- Paired sync/collision readiness.
- Branch-critical target coverage, not merely successful loading of an incomplete manifest.

## Verification Requirements

### Automation

- Table-driven tests for every outcome-matrix row.
- Perfect-parry downgrade tests for timing, hard cone, and both alignment tolerances.
- Stable multi-attacker selection and hysteresis tests.
- Same candidate identity across failed parry and guard fallback.
- Stale attack/window/stage generation tests.
- One damage/effect/recoil commit per interaction.
- Friendly-fire suppression without hostile-target consumption.
- Presentation fallback and ambiguity validation tests.
- Equal turn-rate enforcement for smooth facing and Motion Warping.
- Turn-rate parity under time dilation and non-default montage play rates.
- No defense-path direct rotation test or static source gate.
- Parry bridge, counter, finisher, rollback, timeout, interruption, and death cleanup tests.
- No `ChainState::None`, input restoration, collision restoration, or queue clear between successful paired stages.

### Asset And PIE Proof

The first content rollout is intentionally narrow: `/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1` gains a defense profile, `Attack.Defense.Parryable`, and an explicitly reviewed parry window in `AM_Light_Combo_1` section `Attack_1`. The proof enemy configuration in `Lvl_ThirdPerson1` must select this attack. It must use its validated counter/finisher relationship or a reviewed replacement if visual inspection rejects the current template-derived pairing. No global parry-window seeding is allowed.

The branch-critical content manifest must be expanded to include every proof dependency used by this rollout. Existing HeavyAttack_1 through HeavyAttack_4 notify debt remains a separate readiness lane and does not block the single light-attack proof, but it must be resolved before broad defense rollout claims.

`Lvl_ThirdPerson1` must visibly prove:

- Held guard and a normal block with block montage, VFX, and audio.
- High, middle, and low metadata plus left, center, and right selection across representative attacks; all nine query combinations must resolve through exact or documented fallback rows.
- A block-interruptible attack causing attacker recoil and a non-interruptible attack continuing.
- An unblockable attack bypassing normal block, plus an explicitly parryable unblockable attack if authored.
- Out-of-cone contact causing a hit.
- Valid parry timing with insufficient perfect alignment downgrading to guard.
- Perfect parry playing its bridge and entering CounterWindow only after bridge completion.
- Attack input starting a paired counter and transitioning into finisher without an idle pose, transform snap, input gap, or repeated damage.
- Four surrounding enemies retaining one stable selected threat and respecting combat-token policy.
- Enemy attacks producing no enemy-versus-enemy damage or paired/death side effects.

Runtime telemetry must prove per-frame yaw change does not exceed `EffectiveTurnRate * DeltaSeconds` beyond floating-point tolerance, normal block adds no defender translation, perfect-parry translation remains within policy, and each paired stage applies damage at most once. Successful stage transitions must also record outgoing/incoming montage ownership and root/pelvis discontinuity; initial proof limits are 10 cm unexpected root displacement and 15 cm pelvis discontinuity, with yaw still governed by the rate cap.

The standard build and full `KatanaCombat` automation suite remain required. Headless success cannot establish animation quality; the final visible acceptance requires Editor/PIE evidence and captured defense telemetry.

## Project-Friendly Framework Decisions

- Use UE 5.6 Motion Warping with capped rotation rather than a custom root-motion solver.
- Use explicit montage blend settings and retained sequence state for pose continuity; Motion Warping alone cannot blend poses.
- Keep the first presentation selector as a deterministic sparse table. Chooser is an optional backend once content volume justifies its module and asset dependency.
- Do not adopt experimental Contextual Animation for this slice. Existing paired ownership, damage, input, and lifecycle rules would still be required after migration.
- Do not use animation sync groups as a substitute for cross-actor paired synchronization.

## Implementation Slices

The later implementation plan must preserve these independently verifiable slices:

1. Shared contracts, pure resolver, threat selector, and table tests with behavior-preserving adapters.
2. One-pass contact resolution, duplicate suppression, and explicit team policy.
3. Guard threat lock, capped alignment, defense configuration, and normal-block presentation.
4. Perfect-parry window identity, downgrade rules, parry bridge, and attacker response.
5. Retained counter-to-finisher stage transitions and rollback-safe paired playback.
6. Commandlet validation, one-attack asset rollout, PIE telemetry, and visible proof.

No slice may claim later-slice behavior from source structure or headless tests alone.
