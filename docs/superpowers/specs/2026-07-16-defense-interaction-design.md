# Defense Interaction Design

Date: 2026-07-16
Status: Accepted for implementation planning; runtime implementation has not started

## Purpose

This document defines the accepted canonical target contract for guard entry, normal block, perfect parry, attacker recoil, defensive alignment, and the transition from parry into the existing Chain Counter system. It is the implementation authority for this work; current source remains the runtime baseline until each implementation slice lands and passes its proof gates. It elaborates the ownership rules in `2026-07-02-combat-semantics-ownership-design.md` and the Chain Counter contract in `docs/specs/PAIRED_ANIMATION_SPEC.md`.

The design is deliberately narrower than a universal combat transaction. It adds a typed defense boundary while preserving the five existing runtime components and `FHitReactionInfo` compatibility.

## Goals

- Resolve and commit each `(stage, attack/contact, defender)` interaction exactly once.
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

## Target Semantics

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
| `UCombatComponent` | Own guard state, active attack identity and prediction, interaction cache, threat lock, attack consumption, context-tag leases, and defense gameplay commit. |
| `FDefenseResolver` | Pure synchronous candidate scoring and outcome calculation. It returns `FDefenseDecision` and performs no world query, mutation, presentation selection, damage, or delegate broadcast. |
| `ABaseCombatCharacter` | On the target, expose the native rich-contact entry point and atomically commit defender gameplay/damage. On the source, finalize receipt-based weapon accounting, presentation, and deferred events without re-resolution. |
| `UWeaponComponent` | Produce contact candidates and hit metadata. On rich targets it updates trace dedupe and `MaxHitCount` only after the returned receipt accepts the contact. It does not decide defense outcomes. |
| `UTargetingComponent` | Arbitrate and execute owned alignment requests and maintain owned warp targets. It does not decide defense eligibility or clear another owner's request. |
| `UHitReactionComponent` | Apply hit, block-impact, recoil, stagger, and death presentation selected by the committed result. |
| `UPairedAnimationComponent` | Own retained parry/counter/finisher sequence state, partner state, paired warp lifetime, input blocking, and terminal cleanup. |
| `USamuraiAnimInstance` | Present guard enter/loop/exit state and consume animation-facing state. It does not resolve gameplay. |
| `UDefenseConfiguration` | Own defense kinematics, prediction thresholds, threat-lock policy, bone-height mapping, and sparse presentation rows referenced by `UCombatSettings` or an explicit component/stance override. |
| `UCombatEffectsWorldSubsystem` | Own non-ticking world and actor time-dilation lease registries. Existing cinematic utility calls delegate to it; it owns no combat outcomes. |

Shared cross-component enums, structs, and delegates belong in `CombatTypes.h`. The pure resolver belongs in a focused non-component runtime file. This does not create a sixth gameplay component.

## Typed Contracts

### Attack Identity

`FAttackInstanceId` contains a weak attacker reference and the attacker's existing `AttackGeneration`. It is valid only while the actor exists and its `UCombatComponent` still reports the same generation. Perfect parry consumes this whole attack instance.

An active parry, counter, or hit window additionally carries a window generation, kind, start time, and end time. Notify End may close only the matching window instance. Phase-driven hit detection increments the same hit-window generation when weapon tracing is enabled. A late callback from an old montage or section cannot close or resolve a newer attack window. `FContactInstanceId` combines the attack instance and hit-window generation so a deliberately authored multi-hit attack can create separate contacts while repeated traces from one hit window remain duplicates.

When a compatibility contact has null `AttackData` or no valid combat attack generation, `UWeaponComponent` supplies `FWeaponTraceInstanceId` from its weak component identity and monotonic trace-enable generation. This identity supports dedupe only; it cannot qualify parry or fabricate authored attack semantics.

`FDefenseInteractionId` combines the attack or contact instance, weak defender reference, query stage, and an epoch allocated when that interaction is first registered. Repeated traces look up the existing ID by `(stage, contact instance, defender)` rather than allocating a new epoch. Input intent and physical contact are separate interactions by design. The ID is the duplicate-suppression and telemetry key; it is not a globally replicated GUID.

The defender's `UCombatComponent` is canonical for rich-contact dedupe. It retains committed records until the source attack/window or compatibility weapon-trace generation reports terminal, then retains a tombstone for `InteractionTombstoneSeconds` so late traces and callbacks return the prior result. Source native lifecycle notifications start tombstone aging; cache access and owner teardown perform a lazy validity sweep as a fallback, with no permanent tick. The initial safety defaults are a 1.0-second tombstone and 128 terminal records per defender. Active records are never evicted; when the cap is reached, only the oldest terminal tombstone may be removed. `UWeaponComponent::HitActors` remains a trace optimization for rich targets and is updated only after a newly committed receipt accepts the contact. Generic targets retain the legacy weapon-owned dedupe path.

Threat tie-breaking uses an immutable 64-bit `FCombatantStableId`, assigned once at combatant registration and stored on `UCombatComponent`. The non-replicated slice uses a monotonic local serial and allows explicit IDs in tests. "Deterministic" means ordering cannot change during one authoritative world session; cross-run replay determinism is not claimed in this slice. A future replicated implementation must assign and replicate the serial from the server. Pointer address, `GetUniqueID`, transient actor GUID availability, and per-query array order are prohibited tie-breakers.

### Prediction And Contact Records

`FAttackThreatPrediction` is published by the attacker's `UCombatComponent` as part of the active attack execution record. It contains intended target, path origin and direction, predicted contact point, source socket, defender target bone, predicted contact time, lane, height, confidence, and a prediction timestamp. The producer uses the explicit action target captured when the attack starts, active weapon/source socket, authored target fallback, and the next reviewed hit-window or contact marker. A high-confidence path is the source-to-predicted-contact segment intersecting the intended defender's configured threat capsule. AI target state, player lock-on, attack target changes, window changes, and montage-rate changes invalidate or republish the record. Resolver code never infers high confidence from proximity alone.

Confidence is closed for the first slice:

- `None`: no intended target or usable path; guard facing only.
- `Low`: geometric proximity or facing only; guard facing only.
- `High`: explicit intended target plus a reviewed contact/window deadline and path intersecting the defender threat volume; may qualify perfect parry.

`FPredictedDefenseContact` stores the prediction fields and validity independently from actual impact. `FActualDefenseContact` stores the complete immutable `FHitReactionInfo`, source bearing, derived incoming trajectory, actual lane and height, and validity. Source bearing is the defender-to-attack-source direction used for cone eligibility. Incoming trajectory is weapon/path movement used for left/center/right classification. They must never share one overloaded direction field.

### `FAttackExecutionSnapshot`

The immutable snapshot passed into resolution contains:

- `FAttackInstanceId`, source `UAttackData`, active montage, section, section time, and attack phase.
- Active parry/counter window identity and timing.
- Attack tags and attack type.
- Authored body target and nominal lane.
- `ESwingDirection` as swing shape, not movement direction.
- Intended target and `FPredictedDefenseContact` copied from the published active attack record, plus time to each available alignment deadline.
- Attacker transform, velocity, alive/death state, paired participation, consumed-generation state, and team identity at query construction.

The snapshot is gathered once per query. Resolver code must not look up mutable component state a second time.

Null `AttackData` remains a contact-path compatibility case: it carries no authored defense tags, may be normally blocked using defender state and geometry, and cannot qualify perfect parry. This preserves existing generic hit behavior without fabricating parry capability.

### `FDefenseQuery`

The query contains the immutable attack snapshot plus:

- `EDefenseQueryStage`: `InputIntent` or `Contact`.
- Weak defender reference, defender transform, state, tags, team, and guard state.
- Block Press simulation and unscaled timestamps for intent queries; animation eligibility uses simulation time.
- Optional `FActualDefenseContact` for contact queries. Input queries never fabricate actual contact.
- Relative yaw, time to deadline, current threat-lock state, and effective defense policy.

### `FDefenseDecision`, `FDefenseResolution`, And Contact Receipt

`FDefenseDecision` is the pure resolver output. It contains outcome, reason, selected attack, derived geometry, damage disposition, attacker response, alignment policy, and Chain eligibility. It contains no selected montage, VFX, audio, or mutable object state.

The resolver never reads presentation assets. After it returns a decision, the selector chooses the final payload for that exact outcome. For `PerfectParry`, the selected bridge is then preflighted against remaining alignment time, per-role translation budgets, collision, and required markers. An unusable bridge falls through to the next row or the no-montage timer and cannot change the committed decision.

`FDefenseResolution` is the immutable committed envelope. It contains:

- Interaction ID, stage, explicit outcome, and reason code.
- The selected attack instance and locked threat.
- The original predicted record, optional actual record, resolved body height, defender-relative lane, swing shape, contact point, source socket, and target bone.
- Measured yaw, available turn, required final tolerance, and prediction confidence.
- Damage disposition, attacker response, alignment request specification/owner ID, selected presentation payload, and presentation fallback level. Runtime ownership handles remain in the owning component or sequence context, not in the immutable result.
- Whether a perfect parry may open a Chain sequence.

`FDefenseContactReceipt` wraps the resolution for the weapon caller and adds `EDefenseCommitStatus` (`NewCommit`, `Cached`, `InProgress`, or `RejectedBeforeRegistration`), actual post-resistance `AppliedDamage`, `bAcceptsWeaponHit`, and `bConsumesHitBudget`. These receipt fields describe commit/transport, not gameplay classification. The cache installs an `InProgress` sentinel before health or hit-reaction calls; synchronous reentry for the same key returns that status with no side effects. Completion atomically stores the final receipt, including actual damage. Only a `NewCommit` with `bAcceptsWeaponHit` updates rich-target weapon dedupe. `Hit`, `UnblockableHit`, and `NormalBlock` consume one hit budget; friendly, invalid, invulnerable, consumed, duplicate, and in-progress contacts do not.

`EDefenseDamageDisposition` is closed: `ApplyRequestedDamage`, `SuppressDamage`, or `NoContactSideEffects`. `Hit` and `UnblockableHit` apply; `NormalBlock` suppresses; ignored contact outcomes use no side effects. Input-intent outcomes never invoke contact damage.

The initial closed outcome set is:

- `Rejected`
- `GuardEntered`
- `PerfectParry`
- `NormalBlock`
- `Hit`
- `UnblockableHit`
- `IgnoredFriendly`
- `IgnoredInvulnerable`
- `IgnoredConsumed`
- `IgnoredInvalid`

Reason codes explain why an outcome occurred without multiplying gameplay outcomes. Examples include invalid state, stale attack, no parry window, missing parry capability, outside hard cone, unreachable alignment, not guarding, outside contact tolerance, unblockable, duplicate, and friendly-fire disabled.

### `FDefenseSequenceContext`

Only `UPairedAnimationComponent` retains this context. It contains the originating interaction and attack snapshot, weak actors, optional selected counter `UAttackData`, optional resolved `CounterData` and `FinisherData`, active Chain state, stage generation, timeout handles, active presentation data, and cleanup ownership handles. Counter/finisher fields remain null until attack input selects counter data in `CounterWindow`.

Actor references are weak and revalidated at every stage. Referenced data assets are reflected strong references while the sequence is active. Cleanup is idempotent and keyed to the stage generation so stale callbacks cannot end a newer stage.

## Resolution Stages

### Input Capture And Routing

Every input edge is recorded in the centralized combat-input history before routing. "Input is always buffered" means capture is never gated by a combo, parry, or counter window; it does not mean every recorded edge becomes a deferred attack.

- Block Press and Release are stateful controls evaluated synchronously from their captured records. A rejected Block Press is terminal for that edge and cannot execute later or retroactively become a parry.
- Light/Heavy input during `CounterWindow` is tagged `ChainOnly` and offered once to `TryAdvanceChainCounter`. Success consumes the record. Preflight failure rejects and expires that record without placing it in the normal attack queue; another physical press may retry while the window remains open.
- Outside an owning Chain route, attack inputs retain the existing last-input-wins buffering behavior.

### Input Intent: Block Press

1. `UCombatComponent` validates that the defender can enter guard. The initial slice preserves the current Idle/Blocking restriction and does not add attack-to-block cancellation.
2. It gathers hostile, alive, unpaired, unconsumed, currently attacking candidates once from their published active attack records and records whether each attack credibly targets the defender. Explicit target intent plus a high-confidence predicted path through the defender's threat volume is required for perfect parry. Lower-confidence attacks may guide normal guard facing but cannot qualify perfect parry.
3. `FDefenseResolver` ranks the snapshots and returns one candidate. The same candidate is used for parry evaluation and guard alignment.
4. The resolver evaluates the input-intent matrix below.
5. `UCombatComponent` commits `GuardEntered` or `PerfectParry` once.

If no candidate qualifies, guard still starts without an auto-turn target. If perfect timing exists but only normal-block alignment is reachable, the result is `GuardEntered`; this is the canonical parry-to-block downgrade.

### Physical Contact

Rich targets expose one native entry point on `ABaseCombatCharacter`:

```cpp
FDefenseContactReceipt ResolveAndCommitCombatContact(
    const FDefenseContactRequest& Request);
```

`FDefenseContactRequest` contains the attack snapshot and complete `FHitReactionInfo`. The method is deliberately native in the first slice so a Blueprint override cannot split resolution from commit. It does not replace `IDamageableInterface::ApplyDamage`; that interface remains the generic compatibility path.

The rich path never calls the current `ApplyDamage` implementation after resolving, because that path contains legacy block evaluation. `ABaseCombatCharacter` gains a native internal `CommitResolvedDefenseDamage(const FDefenseResolution&)` that applies only the closed damage disposition and downstream health/hit-reaction work. Legacy `ApplyDamage(FHitReactionInfo)` retains best-effort behavior for generic callers and may adapt into the resolver only when it can construct a valid contact request; it must never be called recursively by the rich adapter.

The rich-contact resolution/commit call is synchronous. Source-side finalization uses only its returned receipt:

1. `UWeaponComponent` produces a candidate without adding the target to `HitActors` or consuming `MaxHitCount`.
2. The target validates key structure and checks its interaction cache before testing current generation. A cached request returns its prior immutable resolution without side effects even when the source generation has since advanced.
3. For a new interaction, the target validates live identity, then snapshots team, alive/death, consumed-attack, and `CanBeDamaged` state before pure resolution. Team rejection and invulnerability are explicit outcomes.
4. The target selects final presentation, installs the in-progress cache record, applies defender gameplay/damage exactly once, atomically finalizes the receipt with `AppliedDamage`, and returns without public defense/presentation delegates.
5. The source weapon updates `HitActors` and `MaxHitCount` only from a newly committed receipt that accepts and consumes the contact. It revalidates its owner after the cross-actor call before touching source state.
6. The source `ABaseCombatCharacter` adapter invokes defender presentation and attacker response/effects directly on their owning components using that same receipt. No participant recomputes block, team, damage, or geometry.
7. After direct presentation attempts and participant revalidation, the source adapter flushes deferred immutable resolution, attack-consumed, and presentation telemetry. If the source becomes invalid, the target's committed interaction schedules the same one-shot flush for end of frame.

Generic `IDamageableInterface` targets retain the legacy best-effort path until they expose a hit-aware defense contract. Rich defense claims apply to `ABaseCombatCharacter` targets only.

### Attack Consumption

Perfect parry intentionally consumes the entire `FAttackInstanceId`, including all remaining targets for that generation. Damage already committed against another target is not rolled back. `UCombatComponent::ConsumeActiveAttack(FAttackInstanceId, EAttackConsumeReason)` is the single atomic source-side operation and must:

1. Mark the generation consumed before any callback or delegate.
2. Disable hit detection and close only matching hit/parry/counter windows.
3. Cancel the matching attack-alignment request and prevent queued combo continuation from that generation.
4. Mark the source montage as consumed-pending-presentation. The commit coordinator transfers it to a successfully started paired bridge or blends it out through the configured parry-stagger fallback if bridge start fails.
5. Record one source-side attack-consumed notification carrying the generation and reason for deferred public broadcast.

The source `UCombatComponent` invokes one internal native attack-termination notification after mutation. `UEnemyCombatAIComponent` handles it by aborting only the matching StateTree attack task and releasing its combat token exactly once before public delegates. Legacy `OnAttackParried` adapters may trigger presentation but must not release the token or terminate the generation a second time. `OnAttackConsumed` is broadcast with the other deferred interaction events after presentation ownership is coherent. Normal montage-end cleanup remains idempotent and sees the token already released. If contact and Block Press occur on the game thread in the same frame, first committed interaction wins: a committed contact prevents a retroactive perfect parry for that defender, while a committed parry makes a later contact `IgnoredConsumed`.

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
| Duplicate registered interaction | Prior cached outcome | Return `EDefenseCommitStatus::Cached`; no side effects or hit accounting; emit diagnostic telemetry. |
| Reentry while the same interaction is committing | Pending outcome | Return `EDefenseCommitStatus::InProgress`; no side effects or hit accounting. |
| Stale attack identity with no committed record | `IgnoredConsumed` | No side effects; emit diagnostic telemetry. |
| Defender cannot currently be damaged | `IgnoredInvulnerable` | No damage or hostile impact presentation and no weapon hit-budget consumption. |
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
7. `FCombatantStableId` as the final tie-break.

Each selection opportunity performs exactly one candidate enumeration through `UTargetingComponent`, using the configured defense threat range capped by the effective targeting range and the existing targetable/line-of-sight policy. `UCombatComponent` filters that immutable array to hostile active attack records before ranking. Losing line of sight disqualifies input-intent parry/auto-facing but never suppresses an actual physical contact query.

The locked threat remains stable for `ThreatLockMinSeconds`. A new threat may replace it only when the current threat becomes invalid or the new threat reaches contact at least `ThreatSwitchLeadSeconds` earlier. Failed perfect-parry evaluation and fallback guard entry use the same selected candidate; they never perform a second world scan.

A "normal selection opportunity" is explicit: Block Press, attack/window/target prediction publication, locked-threat invalidation, or the guarded refresh timer. `UCombatComponent` runs a 0.05-second guarded refresh timer only while guard is active and at least one candidate exists. Events request an immediate refresh but coalesce within the same frame. Releasing guard or entering an owning paired sequence cancels the timer. `UTargetingComponent` remains non-ticking when it has no alignment request.

Missing or low-confidence timing may guide normal guard facing but cannot qualify a perfect parry. A prediction older than `MaximumHighConfidencePredictionAge` is downgraded before ranking even if it was originally published as `High`. The resolver must downgrade rather than invent timing confidence.

For input-intent resolution, the canonical gameplay alignment deadline is the earlier of high-confidence predicted contact and active parry-window end when both exist, or the one reliable value when only one exists. Parry-window authoring places the window end at or before intended contact. Presentation markers never determine gameplay eligibility or create timing confidence.

## Alignment Policy

For a candidate with yaw error `Y`, time to deadline `T`, maximum turn rate `R`, final tolerance `F`, hard cone `H`, and maximum automatic turn `M`:

```text
AvailableTurn = min(M, max(T, 0) * R)
Reachable = abs(Y) <= H && abs(Y) <= F + AvailableTurn
```

Perfect parry uses a tighter `F` than normal block. Actual normal-block contact is accepted only when the measured contact-frame yaw is within normal-block tolerance; a future warp cannot retroactively make an invalid block valid.

`M` is the maximum cumulative correction for one selected threat from lock acquisition to that interaction's deadline; guarded refreshes do not reset it. A genuine threat switch creates a new interaction budget from the defender's then-current facing. After the deadline, sustained guard may continue tracking a still-valid threat at rate `R`, but that later rotation cannot retroactively upgrade the prior parry decision.

Initial proof defaults are provisional acceptance values, not a claim about uniform current behavior. The current player movement rate is 180 degrees/second, enemy movement uses 360 degrees/second, and legacy attack-warp settings use 720 degrees/second. Defense configuration deliberately normalizes proof actors to one capability and validation reports those migration differences.

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
| Guarded threat refresh interval | 0.05 seconds |
| Interaction tombstone lifetime | 1.0 seconds |
| Terminal interaction cache cap | 128 records |
| Maximum high-confidence prediction age | 0.10 seconds |
| Defense threat query range | 1000 cm, capped by targeting settings |
| Guard manual-override threshold | 0.25 normalized yaw input |
| Guard auto-facing resume delay | 0.10 unscaled seconds |
| No-montage parry-bridge duration | 0.15 seconds |
| Chain CounterWindow duration | 2.0 seconds |
| Chain FinisherReady duration | 2.0 seconds |
| Time-dilation lease watchdog | 10.0 unscaled seconds |

Configuration precedence follows existing project patterns: active scoped stance override, `UCombatComponent::DefenseConfigurationOverride`, `UCombatSettings::DefenseConfiguration`, then C++ safety defaults. `UDefenseConfiguration` is authoritative for defense turn rate. Existing `CharacterMovement::RotationRate` and attack `UMotionWarpingSettings::WarpRotationSpeed` are migration inputs only and cannot raise defense capability. A presentation or attack may lower the effective rate but may not raise it above the resolved character or stance capability.

Guard facing uses capped smooth rotation. A block-contact montage may continue rotation with Motion Warping. Perfect-parry and paired stages may use bounded translation and rotation warping. Every path uses the same effective rate and the engine's clamped rotation method. Direct `SetActorRotation`, teleporting, or an uncapped warp is prohibited in defense alignment.

Sustained guard facing uses an opt-in `UTargetingComponent` alignment request updated only while guard has a valid locked threat. Character rotation is applied through the Character Movement rotation path with a clamped per-frame delta; no permanently enabled combat scan is introduced. Threat validity and switch cadence are owned by `UCombatComponent`, while `UTargetingComponent` only executes the current request.

Every alignment operation acquires `FAlignmentRequestHandle` with an owner interaction/stage ID, priority, target, rotation cap, translation budget, and prior movement-rotation settings. Priority is terminal/death cancellation, paired sequence or parry bridge, block contact or attacker response, active attack warp, then guard facing. A higher request suspends a lower request; the lower request resumes only if its owner and target remain valid. Owners release only their own handles and warp-target names. Broad clear-all behavior is permitted only for death or component teardown. Releasing the last request restores exactly the movement/controller rotation settings captured by the first active request.

One request names one executor: Character Movement smooth rotation or Motion Warping. They may hand off at a recorded frame, but both may not rotate the same actor in one frame. The handoff carries the remaining yaw error and capability; it never resets the budget or applies a catch-up snap.

Sustained `GuardFacing` yields while player yaw input magnitude is at or above the configured manual-override threshold. The threat lock remains for hysteresis, but automatic correction is suspended until input stays below threshold for the unscaled resume delay; resumption revalidates the threat and uses current facing without resetting an active interaction budget. `BlockContact`, `PerfectParry` bridge, and paired-sequence requests own rotation over player steering for their bounded presentation, because gameplay has already committed. Total actor yaw while guarding still obeys the resolved defense capability regardless of manual or automatic source.

Warp targets are prepared before montage playback. Runtime code configures a copied root-motion modifier and must not mutate a shared notify template. Warp failure never changes an already committed gameplay outcome.

Defense capability is measured in degrees per simulation second. `SimulationDeltaSeconds` already reflects world/actor time dilation; montage play rate must not multiply the capability. UE 5.6's clamped Motion Warping method multiplies its angular step by effective montage play rate, so runtime must configure:

```text
EngineWarpMaxRotationRate = EffectiveDefenseTurnRate
    / max(abs(EffectiveMontagePlayRate), SmallRate)
AllowedFrameYaw = EffectiveDefenseTurnRate * SimulationDeltaSeconds
```

`EffectiveMontagePlayRate` includes montage instance rate and asset rate scale. The owned runtime modifier recomputes its effective engine rate whenever montage rate changes; a value captured only at montage start is insufficient. Runtime telemetry measures final actor yaw after authored root rotation and warp correction; engine modifier settings alone are not proof of the cap.

An effective montage rate at or below `SmallRate` cannot provide a finite bridge-marker deadline or execute Motion Warping. The resolver uses another reliable contact/window deadline or downgrades to guard, and presentation uses the no-warp fallback. Division by a clamped tiny rate must never create an effectively uncapped engine setting.

All guard/block/parry proof montages must pass sampled root-motion validation. Normal-block montages must be in-place in horizontal translation and must not contain authored yaw that can make final actor yaw exceed the cap. Numerical horizontal drift up to 1 cm over the montage is tolerated but the authored allowance remains 0 cm. Perfect-parry translation is measured as each actor's total horizontal actor displacement from pre-bridge commit to bridge completion and may not exceed 75 cm; paired relative-spacing error is reported separately. Assets that exceed rotation or translation policy are invalid rather than repaired with a transform snap.

## Clock Domains

- Montage sections, action windows, prediction timestamps/deadlines, alignment budgets, guarded refresh, threat hysteresis, and no-montage bridge completion use simulation time because they must track animation progress.
- `CounterWindow`, `FinisherReady`, and guard manual-override resume are player-response timings and use unscaled real time. Their configured durations therefore do not stretch during slow motion.
- Hitstop restoration, interaction tombstone aging, and terminal safety watchdogs use unscaled real time so paused or highly dilated worlds cannot strand ownership.
- Time-dilation leases end from owning sequence events and also have an unscaled emergency watchdog, initially 10.0 seconds with validated per-payload override; they never restore from a dilated one-shot timer.
- Telemetry records both simulation and unscaled timestamps. Tests advance the intended clock explicitly and never rely on frame count.

## Direction, Height, And Contact Data

The following axes remain independent:

- `EAttackHeight`: `High`, `Middle`, or `Low`; authored nominal body target.
- `EIncomingAttackLane`: `Left`, `Center`, or `Right`; resolved from the defender's perspective.
- `ESwingDirection`: `Horizontal`, `Vertical`, `Thrust`, `Sweep`, or `Grab`; swing shape.
- Source socket and defender target bone: exact attachment and IK/VFX contact identifiers.

`EAttackDirection` remains movement/targeting direction and must not be reused for incoming lane. Left and right are always defender-relative.

Authored nominal lane is a fallback and validation hint. Runtime predicted lane is used before contact when confidence is sufficient. Lane is calculated from signed incoming trajectory in defender local space using the configured center dead zone; defensive-cone yaw is calculated separately from source bearing. Actual incoming trajectory uses nonzero weapon velocity first, the accepted trace segment second, and authored nominal lane only as a flagged low-confidence fallback. It never substitutes source bearing. Actual `FHitReactionInfo` point, normal, surface, confidence, and bone become authoritative for contact VFX and IK. Actual bone mapping may refine body height for contact presentation, while pre-contact presentation retains authored height. Predicted and actual records are retained together in the committed resolution so telemetry can measure prediction error without overwriting either source.

## Authoring Model

### Attack Data

`UAttackData` gains a compact defense profile containing attack height, nominal incoming lane, swing shape, an optional source-contact override, defender target-bone fallback, and optional blocked-impact override. The source socket normally comes from the active weapon trace and attack hand. Existing `DefaultContactBone` is migrated or adapted rather than duplicated silently.

Tags retain open-ended attack semantics:

- `Attack.Property.Unblockable`: normal block cannot stop the attack.
- `Attack.Defense.Parryable`: the attack may be perfectly parried while its parry window is active.
- `Attack.Defense.BlockInterruptible`: a normal block may interrupt the attacker and request recoil.

Parry capability and parry timing are separate requirements. The tag supplies capability; `AnimNotifyState_ParryWindow` supplies timing. Neither alone is sufficient. An unblockable attack may deliberately also be parryable. `BlockInterruptible` has no effect when the outcome is not `NormalBlock`.

No tag implies that an animation, VFX, or sound exists. Those remain data references and are validated separately.

### Defense Configuration

`UDefenseConfiguration` contains:

- Kinematic, prediction, interaction-cache, and threat-lock policy.
- Parry-bridge fallback, CounterWindow, and FinisherReady timeout defaults.
- Defender skeleton bone-to-height rows.
- Optional guard enter/exit montages. The held guard pose remains the existing AnimBP state driven by `bIsBlocking`.
- Sparse defender-outcome and attacker-response presentation rows.
- Generic block and perfect-parry fallbacks.
- Default impact VFX/audio and attacker-response fallbacks.

A defender presentation row key has one exact outcome, optional exact-or-wildcard height/lane/swing fields, required tags, excluded tags, and integer priority. An attacker-response row uses the same schema but replaces outcome with exact `EAttackerResponse`. A row matches only when its closed key equals the committed value, every non-wildcard field equals context, all required tags are present, and no excluded tag is present. Matching rows sort by number of exact height/lane/swing fields, then required-tag count, then higher priority. A tie after all three is an authoring error; runtime logs once and uses lexical row name only as a deterministic safety fallback.

Its payload may contain a defender montage/section or paired parry-bridge data, blend settings, warp settings, VFX/audio overrides, hitstop, a reviewed deflection marker, and contact overrides. Attacker recoil is selected from the attacker's resolved configuration using the committed response key, not from the defender's animation set.

When paired parry-bridge data is selected, that paired asset owns both actors' bridge montages. Existing `UPairedAnimationData` role names map the initiating defender to `AttackerMontage` and the consumed source attacker to `VictimMontage`; validation and debug output display the semantic roles to avoid author confusion. Otherwise, defender presentation and attacker response are selected independently from their respective configurations.

Fallback uses that same ranking algorithm rather than a second hardcoded tree. An explicit generic row has the exact outcome/response, wildcard height/lane/swing, and no required tags. When no row matches, selection returns no montage while preserving the committed gameplay outcome.

Validation rejects ambiguous rows with equal specificity, required-tag count, and priority. Missing specialized animation is not a gameplay failure. After perfect-parry commit, a bridge row is usable only when both role transforms can reach its reviewed marker under their rate/translation caps and swept-capsule paths are clear. Unusable rows continue deterministic fallback. A no-montage perfect-parry bridge completes through a stage-generation-keyed fallback timer from configuration, initially 0.15 seconds; the visible proof configuration must instead provide valid bridge montages and a reviewed marker. A selector interface keeps policy independent from storage. The first implementation uses the sparse deterministic table; a Chooser-backed selector may be added later without changing resolver contracts.

Bone-height resolution first checks the exact hit bone, then walks mapped skeleton parents, then falls back to authored attack height. Missing mappings are validation warnings for normal content and errors for proof contacts that claim bone-refined selection.

Blocked-impact precedence is exact defense-row override, `UAttackData` defense-profile blocked-impact override, defender generic defense fallback, existing `UCombatFXData` blocked pools, then weapon/global fallback. Actual impact point, normal, surface type, and target bone remain authoritative regardless of which payload supplies assets. A null defense configuration uses C++ safety defaults and no-montage presentation, logs once, and never crashes. The proof character and proof enemies require explicit configurations before visible acceptance.

## Attacker Response

The closed attacker response set is `None`, `Continue`, `Recoil`, and `ParryStagger`.

- Normal block uses `Continue` unless `Attack.Defense.BlockInterruptible` requests `Recoil`.
- Recoil uses the best matching presentation row, then a generic recoil, then a safe montage blend-out. Blend-out is a presentation fallback for `Recoil`, not a separate gameplay response.
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

On `PerfectParry`, `UPairedAnimationComponent` captures `FDefenseSequenceContext`, acquires a scoped `Context.ParryCounter` lease from `UCombatComponent`, configures partner and warp state, and starts the parry bridge. The context lease remains active through `ParryActive`, `CounterWindow`, `CounterActive`, `FinisherReady`, and `FinisherActive`, and terminal cleanup releases it exactly once. No other system may add or remove this context on behalf of the sequence.

The state reaches `CounterWindow` only after the bridge's driver completion marker or generation-keyed no-montage fallback timer. It does not transition immediately in the same call. For paired bridge assets, the defender is the initiating/`AttackerMontage` role and the source attacker is the partner/`VictimMontage` role.

Bridge completion is a gameplay marker, not permission to drop animation ownership. Proof bridge data provides compatible defender and attacker `CounterReady` hold/loop sections, or explicitly validated terminal poses, that remain owned through `CounterWindow`. Counter start blends from those poses; timeout/cancel blends them out during terminal cleanup. An unexpected bridge montage end before counter input is a failure fallback, not the normal path. The no-montage fallback uses the existing guard/parry-stagger AnimBP states and does not claim pose-continuity proof.

`CounterWindow` duration starts at that transition, not at Block Press. The bridge payload may override the duration with a validated positive value; otherwise `UDefenseConfiguration` supplies the 2.0-second initial default. `FinisherReady` uses its own configured timeout. Light/Heavy input there is also `ChainOnly` and attempts the retained `FinisherData`; success enters `FinisherActive`, while failure expires the edge and leaves the state retryable until timeout.

Attack input in `CounterWindow` resolves defender `UAttackData` through `UCombatComponent` and calls `TryAdvanceChainCounter(UAttackData*)`. The captured input is routed `ChainOnly`: it is consumed when the next stage starts, and it expires without entering the normal queue when preflight/start fails. The window remains open until its original timeout so a later physical press can retry.

Counter completion that should auto-continue updates the retained stage, paired data, and warp targets in place. It must not call global paired teardown, clear the action queue, expose `ChainState::None`, or restore collision/input between counter and finisher. Terminal cleanup occurs only after final completion, cancel, timeout, owner death, partner death, invalid target, or unrecoverable start failure.

Auto-continuing paired stages use an authored `AnimNotify_ChainStageTransition` before the outgoing montage ends. The paired-data owner designates one driver role; only that role's marker, matching the active interaction and stage generation, may advance the sequence. Partner markers and montage callbacks are telemetry or duplicate fallbacks. The next stage starts with explicit blend settings while the outgoing pose is still owned; `OnMontageEnded` is cleanup/failure fallback, not the normal counter-to-finisher trigger. Validation rejects auto-continuing proof data without one unambiguous driver marker.

Before starting a successor montage, the component allocates the successor generation and retires the outgoing generation. Any interruption callback caused synchronously by the new start therefore observes the retired generation and cannot clean up the successor. A stage becomes active only after both montage starts succeed. Rollback restores stage-owned collision, input, context, time, and alignment leases; it never claims that an already interrupted outgoing pose can be restored.

All two-actor starts use preflight followed by rollback-safe execution:

1. Validate actors, state, animation instances, montages, sections, and required warp targets.
2. Allocate the stage generation and establish context, partner collision, input ownership, alignment handles, and warp targets.
3. Start both montages with explicit blend settings.
4. If either start fails, stop any montage that started and restore only state owned by that stage.

If the parry bridge fails after perfect parry commit, the attack remains parried and the Chain sequence closes safely. The defender returns to guard only when Block remains held and guard state is valid; otherwise it returns to the appropriate non-guard state. If counter start fails, the Chain-only input expires and the existing `CounterWindow` remains until timeout. If automatic finisher start fails while the partner and retained `FinisherData` remain valid and retryable, enter `FinisherReady` with input restored; missing/non-retryable finisher data or an invalid partner performs terminal cleanup.

`UCombatEffectsWorldSubsystem` owns `FTimeDilationLease`, not paired cleanup that writes `1.0`. The first world lease captures prior dilation; effective dilation is the minimum of that baseline and all active absolute requests; releasing a lease recomputes the remaining value; the last release restores the captured baseline. `FDefenseSequenceContext` stores only its lease handle. The subsystem provides equivalent per-actor leases that preserve prior custom dilation and combine overlapping freezes; a raw save/restore pair is insufficient for overlap. Existing `UCinematicEffectsUtilityLibrary` entry points delegate to the subsystem and direct external dilation changes while leases are active are diagnosed as ownership violations. Overlapping owners and world teardown are required tests.

## Commit And Event Ordering

The required order is:

1. Validate attack and interaction identity.
2. Return the cached receipt, or resolve without mutation and select the final payload.
3. Install the in-progress cache record and, for perfect parry, consume the attack instance.
4. Apply gameplay state and damage disposition once, then atomically finalize the receipt.
5. Complete transport accounting, then acquire presentation/alignment ownership and directly attempt defender and attacker presentation.
6. Revalidate weak participants after each direct cross-actor call; an invalidated participant suppresses remaining optional effects but cannot undo gameplay.
7. Broadcast deferred `OnAttackConsumed`, immutable `OnDefenseResolved`, and presentation started/failed telemetry only after dedupe, gameplay, receipt-based weapon accounting, AI-token state, and presentation ownership are coherent. A generation-keyed end-of-frame fallback flushes once if the normal source coordinator becomes invalid.

No delegate broadcasts during pure resolution or before interaction-cache finalization. Structured telemetry writes an in-progress record at registration and finalizes it with actual damage, so participant destruction cannot erase proof of the outcome. Presentation listeners cannot mutate the committed outcome. VFX, audio, hitstop, and attacker response consume the same resolution/receipt that damage produced. A cached receipt never rebroadcasts presentation or resolution delegates.

Friendly contacts with friendly fire disabled produce no combat side effects and do not consume the attack against later hostile targets. Enemy-versus-enemy damage is disabled by default for the current proof configuration.

Equal non-neutral teams are friendly. Explicitly hostile teams may interact. Neutral or unknown-team physical contacts retain the existing damageable policy through the rich adapter when the target is `ABaseCombatCharacter`, or the generic path otherwise. They are not auto-selected as parry or guard-facing threats without an explicit hostility result.

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
- Invulnerability changes before commit: resolve `IgnoredInvulnerable`; changes after commit do not rewrite the result.
- Presentation asset is missing: use deterministic fallback or no montage; never change damage after commit.
- Paired montage start partially succeeds: stop the started side and roll back stage-owned state.
- Duplicate hit or montage callback: ignore it using interaction and stage identity.
- A later hit kills an actor during another defense callback: subsequent interactions observe death and resolve as ignored/rejected.
- Any alignment, context-tag, time-dilation, AI-token, collision, or input lease release: release by owning handle and tolerate duplicate cleanup.

## Debugging And Validation

Defense debug output must expose:

- Interaction and attack generation IDs.
- Candidate list, selected threat, and switch reason.
- Query stage, outcome, and reason code.
- Relative yaw, hard cone, available turn, and final tolerance.
- Parry-window start/end/progress and prediction confidence.
- Predicted and actual source bearing, trajectory, contact, height, lane, swing shape, source socket, target bone, and prediction age.
- Selected presentation row and fallback level.
- Interaction-cache status, weapon acceptance/hit-budget disposition, and attack-consumption reason.
- Alignment owner, priority, effective montage rate, configured engine warp rate, and measured final actor yaw.
- Chain state/stage generation and terminal cleanup reason.

Asset validation and migration reports must verify:

- Parryable tag and parry-window agreement.
- Defense-profile completeness.
- Counter/finisher boolean and reference agreement.
- Montage and section validity for every selected payload.
- Parry-bridge driver marker and both-role `CounterReady` hold/terminal-pose compatibility.
- Sparse presentation ambiguity and fallback coverage.
- Bone-height map coverage and blocked-impact fallback provenance.
- Sampled authored root yaw/translation against defense capability and translation policy.
- Paired sync/collision readiness.
- Branch-critical target coverage, not merely successful loading of an incomplete manifest.

## Verification Requirements

### Automation

- Table-driven tests for every outcome-matrix row.
- Native rich-contact adapter tests proving one resolver call, one damage/effect commit, post-accept weapon dedupe, and exact `MaxHitCount` behavior.
- Rich-contact source/static gate proving it cannot call legacy `ApplyDamage`, `IsBlocking`, or `CanBlockHit` after resolution.
- Friendly, invalid, invulnerable, consumed, and duplicate contacts do not consume rich-target weapon hit budget.
- Perfect-parry downgrade tests for timing, hard cone, and both alignment tolerances.
- Stable multi-attacker selection and hysteresis tests with at least two simultaneously active attacks, scripted deadlines, and deterministic stable IDs.
- Same candidate identity across failed parry and guard fallback.
- Stale attack/window/stage generation tests.
- One damage/effect/recoil commit per interaction.
- Same-key synchronous reentry observes `InProgress`, performs no work, and later receives the finalized cached receipt with original `AppliedDamage`.
- Friendly-fire suppression without hostile-target consumption.
- Presentation fallback and ambiguity validation tests.
- Perfect-parry decisions remain identical when bridge rows are missing, blocked, over budget, or otherwise unusable; only presentation fallback changes.
- Alignment-handle priority, suspension, owner-only release, and exact movement-setting restoration tests.
- Sustained-guard manual override/resume tests, including total yaw-cap enforcement for manual and automatic sources.
- Equal final actor turn-rate enforcement for smooth facing and Motion Warping after authored root rotation is composed.
- Turn-rate parity under time dilation and 0.5x, 1.0x, and 2.0x effective montage play rates, including expected inverse engine-rate configuration.
- Root-motion validation tests for normal-block translation/yaw and perfect-parry translation budget.
- No defense-path direct rotation test or static source gate.
- Parry bridge, counter, finisher, rollback, timeout, interruption, and death cleanup tests.
- Bridge completion retains both actors' pose ownership through `CounterWindow`; unexpected montage end takes the failure path.
- Scoped `Context.ParryCounter`, AI attack-token release, and world time-dilation lease overlap tests.
- Same-frame contact-versus-parry ordering and whole-generation multi-target consumption tests.
- Public delegate reentrancy tests where a listener destroys a participant or starts another action.
- Input-history tests proving Block is captured but not deferred and failed `ChainOnly` input cannot fall through to a normal attack.
- No `ChainState::None`, input restoration, collision restoration, or queue clear between successful paired stages.
- Replace `KatanaCombat.CounterSystem.Input.BlockStartsChainParry` and retire/replace `KatanaCombat.CounterSystem.Internal.ChainParryTransition`; no acceptance test may assert that `ParryActive` and `CounterWindow` occur in the same call or use a protected helper as feature proof.

Counter test migration is classified before implementation:

- Rewrite `TryCounter.ChainUsesParryWindow`, `ChainStoresParriedTarget`, `ChainPairedCancelClearsContext`, `Input.BlockStartsChainParry`, `ChainAttackInputAdvancesCounter`, `ChainAdvanceRejectsNullAttackData`, and nonlethal counter policy coverage through the public resolver/bridge/stage-generation path.
- Replace `Internal.ChainParryStaggersEnemy` with data-driven `ParryStagger` response coverage, and rewrite `Internal.ChainCancelResetsState` to cancel real `ParryActive`, `CounterWindow`, and later generations.
- Retire `Internal.ChainParryTransition`; its expected behavior is explicitly invalid.
- `Internal.CancelChainCounterNoopWhenNone`, `Internal.ExecuteChainCounterAttackRequiresCounterWindow`, and `Internal.ChainNullAttackerFails` may remain narrow primitive/null-safety tests, but cannot satisfy any feature acceptance requirement.
- AC3-mode tests remain separate legacy-mode coverage unless a later approved design sunsets AC3; they do not prove Chain behavior.

### Asset And PIE Proof

Visible proof has two gates. Passing the vertical slice does not claim matrix coverage.

#### Gate A: One-Attack Vertical Slice

`/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1` gains a defense profile, `Attack.Defense.Parryable`, and an explicitly reviewed parry window in `AM_Light_Combo_1` section `Attack_1`. The proof enemy configuration in `Lvl_ThirdPerson1` selects this attack. Its existing counter/finisher relationship must be visually reviewed because current commandlet evidence identifies the counter as template-derived; replace it if the pair does not align. No global parry-window seeding is allowed.

Gate A visibly proves held guard, one normal block with montage/VFX/audio, one perfect-parry bridge, delayed entry to `CounterWindow`, paired counter-to-finisher continuity, attack consumption, AI token release/recovery, and no repeated damage. It also proves out-of-cone hit and valid-timing/insufficient-alignment downgrade using controlled defender orientation.

#### Gate B: Defense Matrix Fixture

Before broad claims, a reviewed proof fixture must include at least three attacks covering High, Middle, and Low. Each is exercised from Left, Center, and Right approach trajectories, producing all nine height/lane queries through exact or explicitly documented fallback rows. The fixture must also include at least one `BlockInterruptible` attack, one continuing blockable attack, one unblockable attack, and an unblockable-plus-parryable attack only if that combination is deliberately authored. One attack may cover multiple requirements, but the manifest must name every asset and expected row.

Automation or a dedicated functional map must create at least two simultaneously active threats with scripted deadlines to prove hysteresis and switching. The four-enemy `Lvl_ThirdPerson1` setup proves default combat-token policy and stable guard behavior, but it does not prove multi-active-threat ranking when policy permits only one attacker. A temporary proof configuration may allow two active tokens; it must be restored after the capture.

Before either gate is scheduled for visible acceptance, an asset preflight must inventory and validate defender block, parry bridge, attacker recoil/parry-stagger, counter, finisher, VFX, audio, configuration assignment, source/target sockets, and required markers. Missing visible fallback assets block that gate rather than silently reducing it to headless proof. The branch-critical manifest must include every selected dependency.

Current pre-implementation audit evidence is a baseline, not acceptance: the 20 inspected attack assets have no parry windows; 11 have counter windows/data and finishers; `LightAttack_1` has counter/finisher data but no parry timing; HeavyAttack_1 through HeavyAttack_4 retain separate notify debt. These facts are expected migration work and must remain visible in the implementation plan.

Telemetry samples transforms immediately before each presentation/stage and after every evaluated frame. Final per-frame actor yaw must not exceed `EffectiveTurnRate * SimulationDeltaSeconds` beyond 0.1-degree numerical tolerance. Normal-block horizontal actor displacement from commit to montage completion must remain at or below the 1 cm numerical tolerance. Perfect-parry total horizontal displacement is measured per actor from pre-bridge commit to bridge completion and must remain at or below 75 cm.

At a stage handoff, unexpected root displacement is the horizontal actor-transform delta between adjacent evaluated frames minus authored/approved warp translation; its initial limit is 10 cm. Pelvis discontinuity is the world-space pelvis socket distance from the last evaluated outgoing pose to the first evaluated incoming pose at the same handoff; its initial limit is 15 cm. Measurements record simulation delta, montage rates, expected root/warp contribution, and both actor identities so results are comparable across frame rates.

The standard build and full `KatanaCombat` automation suite remain required. Headless success cannot establish animation quality; final acceptance for each gate requires Editor/PIE capture, telemetry, and named asset evidence.

## Review Closure Matrix

Closure here means the written contract is explicit; it does not claim implementation.

| Audit finding | Contract closure |
|---|---|
| Split attacker/defender resolution and undefined hit accounting | Native rich-contact request/receipt, target-side atomic commit, source-side post-receipt accounting, and canonical defender cache. |
| Motion-Warping cap invalid under play rate/root motion | Inverse effective-play-rate configuration, one alignment executor, owned request arbitration, sampled root-motion validation, and final-yaw telemetry. |
| Outcome-dependent bridge circularity | Resolver reads no presentation data; bridge selection and reachability preflight occur only after decision and cannot rewrite it. |
| Undefined perfect-parry attack consumption | Whole-generation atomic consume contract, same-frame precedence, montage disposition, internal AI termination, and deferred public event. |
| Incomplete predicted/actual attack context | Published attack prediction plus separate immutable predicted and actual contact records with distinct source-bearing and trajectory axes. |
| Buffered input and Chain lifecycle ambiguity | Unconditional capture with explicit stateful/`ChainOnly` routing, scoped context, stage generations, retry windows, role mapping, and fallback completion. |
| One-asset proof could not satisfy matrix claims | Gate A one-attack vertical slice separated from Gate B multi-attack, nine-combination, and two-active-threat proof. |
| Undefined selector/config/cache determinism | Exact wildcard/tag/priority rules, explicit `UCombatSettings` precedence, stable session IDs, bounded tombstones, and bone/FX fallback ownership. |
| Delegate reentrancy and shared time-state clobbering | Commit/accounting before delegates, weak-participant revalidation, one-shot fallback flush, and world/actor dilation leases. |
| Draft/current/canonical documentation conflict | Read-first docs label current versus target behavior; posture-era deep references are legacy snapshots and stale source comments are corrected. |

## Project-Friendly Framework Decisions

- Use UE 5.6 Motion Warping with capped rotation rather than a custom root-motion solver.
- Compensate UE 5.6 clamped-warp rate for effective montage play rate and validate final composed root motion; setting `WarpMaxRotationRate` alone is insufficient.
- Use explicit montage blend settings and retained sequence state for pose continuity; Motion Warping alone cannot blend poses.
- Keep the first presentation selector as a deterministic sparse table. Chooser is an optional backend once content volume justifies its module and asset dependency.
- Do not adopt experimental Contextual Animation for this slice. Existing paired ownership, damage, input, and lifecycle rules would still be required after migration.
- Do not use animation sync groups as a substitute for cross-actor paired synchronization.

## Implementation Slices

The later implementation plan must preserve these independently verifiable slices:

1. Shared identity, prediction/contact records, pure decision resolver, selector, and table tests with behavior-preserving adapters.
2. Rich-contact request/receipt, interaction cache, duplicate suppression, team/invulnerability policy, and post-accept weapon hit accounting.
3. Captured input routing, threat publication/lock, alignment-handle arbitration, play-rate-normalized cap, defense configuration, and normal-block presentation.
4. Perfect-parry window identity, post-decision bridge preflight, attack-generation consumption, AI token release, downgrade rules, and attacker response.
5. Scoped Chain context, retained counter-to-finisher generations, time-dilation leases, reentrancy hardening, and rollback-safe paired playback.
6. Commandlet validation, asset preflight, `LightAttack_1` Gate A rollout, PIE telemetry, and visible vertical-slice proof.
7. Multi-attack Gate B fixture, nine height/lane combinations, two-active-threat proof, and broad visible acceptance.

No slice may claim later-slice behavior from source structure or headless tests alone.
