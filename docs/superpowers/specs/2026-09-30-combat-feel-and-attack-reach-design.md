# Combat Feel and Attack Reach Design

## Status

Design approved section by section (2026-09-29/30), revised after an independent review and
engine-source verification. Pending review of this written spec. This spec does not
authorize asset saves; every asset change goes through an approved plan.

## Delivery Order

Step 3 ships as five PRs, each with a green baseline:

1. **3a-knockback**: the procedural displacement executor and knockback.
2. **3a-charge**: charge damage, charge-scaled knockback, full-charge feedback.
3. **C1**: operation contract, registry, unified approval, headless adapters
   ([editor operation workflow spec](2026-09-30-editor-asset-operation-workflow-design.md)).
4. **3b**: attack reach bake (headless) and its runtime consumers.
5. **C2**: editor operation UI, button migrations, validator cleanup; gives the reach bake
   its details-panel and Content Browser entry points.

Later steps, recorded here so their dependencies are visible:

- **Paired entry migration** (its own step, after step 3): move paired entry from the
  bounded mover to the procedural displacement executor's movement channel, with full
  re-qualification. See "Paired Entry Follow-Up".
- **Step 4, hit reactions and trades**: implement the approved 2026-07-18 micro-specs 05/06
  (additive flinch vs full-body reaction), refreshed against current code. Its first,
  explicit decision is the stagger model: contextual stagger (a per-attack
  `InterruptPower` against per-phase `InterruptResistance` decides flinch vs interrupt;
  stagger and finisher windows come from gameplay rules) or a stagger gauge (per-attack
  stagger damage filling a gauge). The CHANGELOG records the project's deliberate pivot
  from Sekiro-style combat to AC/Arkham free-flow when posture was deprecated; a gauge would
  partly reverse it. Note: micro-spec 05 planned to reuse `StaggerPower`, which PR #130
  deleted; the refreshed spec reintroduces the value under the chosen model's name.

## Problem

Three authored data surfaces do nothing at runtime:

- **Knockback:** `FHitReactionEntry::KnockbackForce` (200 on all 8 `DA_HitReaction`
  entries, the untouched default) and `UHitReactionSettings::GlobalKnockbackMultiplier` are
  never read.
- **Charge:** `UAttackData::MaxChargeTime`, `ChargeTimeScale` and
  `MaxChargeDamageMultiplier` are never read. Heavies loop a charge section while held, but
  charging has no effect or feedback. `UMontageUtilityLibrary::CalculateChargeLevel` has no
  callers; `SamuraiAnimInstance::UpdateCharge` hard-codes zero.
- **Reach:** `UWeaponData::WeaponReach` (300 on `DA_Weapon_Katana`) is read only by a test.
  Attack warps drive toward the target's center, soft-aim uses a fixed 500 cm, and AI uses a
  fixed 150 cm approach range, none of which reflect how far an attack's blade travels.

## Goals

- Every field in scope either drives runtime behavior as its name and tooltip say, or is
  deleted.
- Knockback creates spacing on interrupting hits, moves correctly over real terrain, and
  pauses during hitstop.
- Charged heavies deal more damage (capped), optionally push farther, and give an event plus
  an audiovisual cue at full charge.
- Attack reach is a measured fact about each attack's animation; designers author intent as
  adjustments to that fact.
- Every behavior is pinned by automated tests where the test environment allows, and by
  capture scenarios or recorded PIE checks where it needs a real world.

## Non-Goals

- Block/parry pushback, ragdoll impulses, launchers.
- The additive-flinch reaction model and the stagger decision (step 4).
- Paired entry migration (its own step).
- Per-character reach tables.
- Network replication.

## Design Rules Applied

- **Rule 4 (hold = button state at window start):** whether a heavy charges is decided only
  by the button being held when its hold window opens. Charge adds a magnitude read once at
  release; duration never gates any transition.
- **Rule 6 (delegates):** `FOnFullyCharged` is declared in `CombatTypes.h` because UI and
  Blueprints consume it.
- **Derive facts, author intent:** measurable quantities are derived; feel decisions are
  authored as adjustments to derived facts, with explicit, validated overrides.

## Part A: Procedural Displacement Executor (PR 3a-knockback)

### Why a new executor

Verified in engine source: while a root-motion animation plays, character movement applies
only the animation's root motion and returns early
(`UCharacterMovementComponent::ApplyRootMotionToVelocity`: "Animation root motion ... takes
precedence"), so root-motion sources are ignored. Every hit reaction plays root-motion
animations (`UE5M_Root_HitReaction_*`, `bEnableRootMotion = true`). The existing bounded
mover moves the capsule directly in 3D with no floor handling, so it cannot push across
slopes or ledges. The new executor lets our code decide the displacement curve and lets the
engine apply it through whichever channel is live.

### Contract

`EAlignmentExecutor::ProceduralDisplacement` is appended after `BoundedMovement`. A request
carries an immutable `FProceduralDisplacement`:

| Field | Meaning |
| --- | --- |
| `Direction` | Horizontal unit vector (world). |
| `Distance` | Centimeters. |
| `Duration` | Seconds on the request's clock. |
| `SpeedProfile` | `Linear` or `EaseOut`. |
| `Clock` | `ActorTime` (pauses when the owner is frozen by hitstop) or `WorldTime`. Knockback uses `ActorTime`. |
| `AnimationBlend` | `AddToAnimation` or `ReplaceAnimation`: how the push combines with a playing root-motion animation. |

Step 3 implements this fixed-curve mode. The goal-seeking mode that paired entry needs is
added in the paired entry step.

### Pure math

`DisplacementMath` (runtime `Utilities`, pure): normalized progress
`s(u) = u` for `Linear` and `s(u) = 1 − (1 − u)²` for `EaseOut` (`u = τ / T`), and the
displacement between two request times. Deltas over any partition of `[0, T]` sum to
`Distance`.

### Channels

The executor re-evaluates the channel every tick:

1. **Animation channel**, used when the owner is playing a montage whose root motion is
   extracted and whose instance is advancing (`FAnimMontageInstance::IsPlaying`). A montage
   blending out after its last section, or paused, still counts as playing root motion but
   extracts none, so the push takes the movement channel instead of freezing with it; only
   hitstop pauses a push. A custom `URootMotionModifier_ProceduralDisplacement` (subclass of
   `URootMotionModifier`, named like the engine's `_Warp` and `_Scale` modifiers) is added at
   runtime through `UMotionWarpingComponent::AddModifier`, spanning the rest of the push in
   montage time. Its `ProcessRootMotion` converts this frame's curve delta into the
   plugin's root-motion space (as the built-in warp modifiers do) and adds it to the
   animation's root motion, or replaces the animation's translation, per `AnimationBlend`.
   The animation's rotation is kept. Its clock advances by the root-motion `DeltaSeconds`,
   which is dilated, so hitstop pauses the push without extra code. It also reports the
   kept animation travel along the push direction, so the blocked check does not mistake
   a reaction that steps sideways for a wall.
2. **Movement channel**, used when no root-motion animation plays. The project's
   `FRootMotionSource_ProceduralDisplacement`, a character-movement root-motion source in
   override mode with `IgnoreZAccumulate` (gravity still applies) and no timeout. It
   evaluates the curve in `PrepareRootMotion` from each movement step's actual simulation
   time, so it lands on the curve at any frame rate and contributes nothing past the
   curve's end. The executor owns termination. On removal the finish velocity clamps
   horizontal speed to zero (`ClampVelocity` 0), which keeps a fall's downward speed. The
   source's own time is the record of applied push time. Character movement's dilated
   delta pauses it under hitstop. On a step with animation root motion, character movement
   applies only the animation and ignores every source, so the source leaves its clock
   alone on that step (`HasAnimRootMotion()` in `PrepareRootMotion`). A root-motion montage
   that starts mid-push therefore takes over the curve where the movement channel left it.

If the animation channel ends before the curve completes (the montage stops advancing,
stops, or is replaced), the remainder continues through the movement channel. In both
channels character movement handles floors, slopes, steps, ledges (the victim falls) and
walls (it slides).

### Outcomes and lifecycle

- **Reached** when request time reaches `Duration`.
- **Blocked** when 0.05 s of request time in a row passes on steps whose actual movement
  along the push is below 10% of the expected progress (a head-on wall). It counts time, not
  ticks, so a graze ends a push the same way at any frame rate; 0.05 s is three 60 Hz ticks.
  Steps whose expected progress is under 0.1 cm (hitstop) neither count nor reset it.
- **Invalid** when no channel can be installed (no motion warping component and no
  character movement, or movement mode `None`).
- **Cancelled** when the request is removed while still running (release,
  `ReleaseAllAlignmentRequests`, a lost target), or when a suspension outlasts the push.
- **Suspension.** A higher-priority request suspends a running push: its channel is removed
  and its clock stops. When it is active again it resumes from its clock only if the
  suspension, measured on the owner's dilated time (hitstop does not count), was shorter
  than the push it had left (`Duration` minus elapsed). Otherwise it ends `Cancelled`, and
  releases itself if it carries `bReleaseWhenFinished`.
- The request carries `bReleaseWhenFinished`: the targeting component releases it on any
  terminal outcome and removes the active modifier or source. Release, preemption,
  `ReleaseAllAlignmentRequests` and death remove whichever channel is active.
- `CaptureAlignmentRotationSettings` is skipped for requests that never rotate, so a pushed
  player keeps orient-to-movement and controller yaw.

### Arbiter integration

The five places hard-wired to `BoundedMovement` are generalized: tick dispatch, motion-state
queries, the negative-generation rule, limit immutability, and `HasSmoothAlignmentRequest`
(which enables the targeting tick). A new priority `HitKnockback` is inserted between
`ActiveAttackWarp` and `BlockContact` (ordinal comparison, newest acquisition wins ties; no
asset serializes the enum).

### Tests (executor)

- `DisplacementMath`: both profiles; partitions sum to `Distance`; `EaseOut` covers more
  than half of `Distance` in the first half of `Duration`.
- Channel selection as a pure decision function (root-motion montage playing or not,
  motion warping present or not, montage instance advancing or not).
- `URootMotionModifier_ProceduralDisplacement::ProcessRootMotion` on a test character: add vs replace,
  direction conversion, zero delta at zero `DeltaSeconds`.
- Arbiter: acquire, priority ordering, suspension, self-release on each terminal outcome,
  release removes the channel, existing executors unchanged (the `DefenseAlignment` and
  `BoundedAlignment` suites re-run).
- Movement over terrain needs a real world: a new capture scenario (see Verification).

## Part A: Knockback (PR 3a-knockback)

### Model

Knockback is **reaction-coupled** and applies to **interrupting** reactions: a push starts
only when a directional hit reaction starts. Blocked and parried hits, super armor,
suppressed paired states and lethal hits never push. On the defense path the victim is
already dying before the reaction; on the `ApplyDamage` path the reaction runs before health
changes and death releases the push in the same frame. The legacy `PlayHitReaction` fallback
path (no settings) never pushes, and logs that once per component. Until step 4, every reaction is full-body, so every
reaction pushes; step 4 makes additive flinches not push through the same decision function.

### Data

| Change | Detail |
| --- | --- |
| Add `FKnockbackConfig` (`CombatTypes.h`) | Values only: `Distance` (cm, `ClampMin=0, ClampMax=500`), `Duration` (s, `ClampMin=0.05, ClampMax=1`), `DirectionMode` (`EKnockbackDirection`: `AwayFromAttacker`, `AlongSwing`), `SpeedProfile` (`Linear`, `EaseOut`), `AnimationBlend` (`AddToAnimation`, `ReplaceAnimation`; default `AddToAnimation`). The resolved result, and the type of the defaults map. The blend is data, so the per-type decision from the measurement is a data change. |
| Add `FKnockbackOverride` (`CombatTypes.h`) | The same five fields, each with an inline override toggle. It is a separate type so the defaults map shows plain, editable values rather than fields greyed out behind toggles that do not apply there. |
| Add `UAttackData::Knockback` | `FKnockbackOverride`; each field overridden independently, else the attacker's combat-settings default. |
| Add `UCombatSettings::DefaultKnockback` | `TMap<EAttackType, FKnockbackConfig>`: Light `{25 cm, 0.2 s, AwayFromAttacker, EaseOut}`, Heavy `{20 cm, 0.25 s, AwayFromAttacker, EaseOut}`. Missing types resolve to no push. The attacker's combat settings are the character's `ABaseCombatCharacter::CombatSettings`. |
| Rename `UHitReactionSettings::GlobalKnockbackMultiplier` → `KnockbackScale` | Default 1, `ClampMin=0, ClampMax=5`. Victim-side distance scale: 1 normal, 0 immune. No asset serializes the old name. |
| Delete `FHitReactionEntry::KnockbackForce` | Saved 200s ignored on load. |
| Add `UAttackData::MaxChargeKnockbackMultiplier` | Default 1 (off), `ClampMin=1`, `ClampMax=5`. Heavy category. |
| Add `FHitReactionInfo::ChargeLevel` | 0..1, default 0. No damage site writes it yet; the charge PR will. |

`ResolveKnockback(AttackData, AttackerCombatSettings)` is pure and returns the resolved
config. Then:

```
PushDistance = Distance × Lerp(1, MaxChargeKnockbackMultiplier, HitInfo.ChargeLevel)
                        × VictimHitReactionSettings.KnockbackScale
```

The authored distance is the uncharged push; charge only adds. Duration is never scaled.
A non-finite charge level counts as 0 (uncharged) and a non-finite multiplier as 1.
When the attacker is not an `ABaseCombatCharacter`, the victim's own combat settings are
used; if neither exists, there is no push.

### Direction

- `AwayFromAttacker` (default): `CombatMath::FlatDirection(AttackerLocation, VictimLocation)`.
- `AlongSwing`: the flattened negation of `DirectionToAttacker` (the blade's velocity at
  contact), continuous in the swing. Only the part that points toward the attacker is
  removed and replaced by the same length of `AwayFromAttacker`: a swing straight away stays
  straight away, a tangential swing stays tangential, and a pure back-swing becomes
  `AwayFromAttacker`. It falls back to `AwayFromAttacker` when the horizontal part is under
  half of that vector's length (overhead chops).
- Degenerate direction: no push.

### Request

`StartKnockback(const FHitReactionInfo&)` runs in `PlayHitReaction` right after
`PlayReactionFromEntry` succeeds (not inside it; death reactions share that function). It
acquires a `ProceduralDisplacement` request: `Clock = ActorTime`, `AnimationBlend` from the
resolved config (`AddToAnimation` for both types until the reaction measurement says
otherwise), priority `HitKnockback`, `bReleaseWhenFinished`. A new push releases the previous
one (`Replaced`). These also release it, each naming itself in the `Cancelled` row:
`EnterPairedAnimationState` (`PairedEntry`), `UCombatComponent::PrepareForPairedTakeover`
(`PairedTakeover`, so the character who starts a paired animation drops its own push too), the
defense chain taking over the defender (`ChainStart`: when the parry sequence begins, which covers
the no-montage parry bridge, and at each stage start), and `EndPlay` (`EndPlay`).

### Observability

- `Combat.Debug.Knockback` (and `Combat.Debug.All`): draws start, commanded end and channel;
  logs the resolved config, charge level, scale and outcome.
- Action-reaction telemetry records the start and finish as `AlignmentChanged` rows within the
  existing schema (no new event values, no `schema_version` bump; `analyze_capture.py`
  accepts only 1 or 2). `StartKnockback` writes `AlignmentOwner = HitKnockback` with
  disposition `Started` or `Rejected`. A `Rejected` row names its cause in `Detail`, and the
  debug line prints it: `no push distance: <cause>` (`no attack data`, `no combat settings`,
  `missing type default (<type>)`, `non-finite scale or distance`, `zero victim scale` or
  `zero authored distance`), `degenerate direction`, or `acquire rejected`. The targeting component then writes every
  displacement's later rows with `AlignmentOwner` = the request's owner, the measured travel,
  and the reason in `Detail`:
  - the terminal outcome: `Reached` (`DurationReached`), `Blocked` (`ProgressStalled`),
    `Invalid` (`NoDeliverableChannel`) or `Cancelled` (`StaleSuspension`);
  - `Cancelled` when a still-running push is removed by anything else: `Released` or the
    releasing caller's reason (such as `Replaced`), the `EAlignmentReleaseReason` name (such
    as `Death` or `ComponentTeardown`), or `TargetLost`;
  - `Suspended` when a higher-priority request takes over (`Detail`: its owner), and
    `Resumed` when the push continues afterwards.

  The `Combat.Debug.Knockback` log line for each row ends with the same reason.

### Tests (knockback)

`PlayHitReaction` cannot run in the test world (no AnimInstance before `BeginPlay`):

- a pure `ShouldApplyKnockback` decision function covers blocked, parried, super armor,
  suppressed, dying and legacy-fallback cases. Blocked and parried hits never start a
  directional reaction, and source-structure tests pin that path: only
  `ApplyRequestedDamage` reaches the reaction commit, super armor commits without a
  reaction, and `PlayHitReaction` has one caller, behind the commit's reaction gate;
- a source-order test asserts `PlayHitReaction` calls `StartKnockback` only after
  `PlayReactionFromEntry` succeeds (the `DefenseArchitectureSourceTests` pattern);
- `StartKnockback` is tested directly through a friend declaration: resolution (each field
  overridden independently, defaults, missing type), the distance formula (charge 0/0.5/1
  with multiplier 2; scale 0.5 and 0), direction modes and fallbacks, and the acquired
  request's parameters.

## Part A: Charge (PR 3a-charge)

### Data

| Change | Detail |
| --- | --- |
| `MaxChargeTime` | Now read. `ClampMin=0`. |
| `MaxChargeDamageMultiplier` | Now read. `ClampMin=1`. |
| Add `ChargeEasing` | `EEasingType`, default `Linear`. |
| Add `ChargeCurve` | Optional `UCurveFloat`; when set it replaces `ChargeEasing`. Output clamped to [0, 1] by our code (`CalculateChargeLevel` does not clamp curve output). |
| Delete `ChargeTimeScale` | `HeavyAttack_1`'s 0.05 ignored on load. |
| Add `FullChargeCue` | `FCombatCueConfig` (`CombatTypes.h`): `Sound`, `VFX`, `AttachSocket` (None = weapon trace tip socket), `VolumeMultiplier`, `VFXScale`. |
| Add `UCombatFXData::FullChargePool` | `FImpactFXPool`; surface-alignment fields ignored for attached cues. |
| Tooltips | The "[NOT WIRED]" tooltips on the charge fields are replaced. |

**Balance note:** the C++ defaults (`MaxChargeTime` 2.0 s, `MaxChargeDamageMultiplier` 2.5)
become live on all four heavies, none of which author these fields. This is an intentional
balance change, recorded in the PR.

### Measurement

- The clock starts in `OnHoldWindowStartWithContext`'s heavy branch once the hold activates,
  recording `UWorld::GetTimeSeconds()` and the hold id.
- **World time**: pauses with the game and follows global slow motion like the loop
  animation. A charging character is not hitstopped.
- Level = `CalculateChargeLevel(Held, MaxChargeTime, ChargeEasing, ChargeCurve)`, clamped to
  [0, 1]; `MaxChargeTime <= 0` gives 1.
- AI cannot hold input: AI heavies never charge.

### Latching (release)

`DeactivateHoldWithInputSerial`'s heavy branch tries, in order: jump to the release
section, dispatch a directional follow-up, or return to idle. Starting a follow-up calls
`ClearHoldState` and changes the attack generation. Therefore:

- The level is computed at the **top** of the heavy branch, before any dispatch.
- After the branch decides, the level is latched for the attack that delivers the strike:
  the current generation for a release section (a section jump keeps the generation), the
  new generation for a follow-up (follow-ups inherit the charge). Return-to-idle latches
  nothing.
- The latch lives outside `HoldState`, so `ClearHoldState` does not erase it; it is cleared
  in `ResetTerminalAttackState` and replaced by the next latch.
- `GetChargeDamageMultiplier(Generation)` returns `Lerp(1, MaxChargeDamageMultiplier,
  Level)` for a matching generation, else 1. Both damage sites (`WeaponComponent.cpp`
  primary, `BaseCombatCharacter.cpp` legacy) multiply by it and copy the level into
  `FHitReactionInfo::ChargeLevel`. Counter and finisher damage are unchanged.

### Full-charge feedback

- At clock start a world timer (`FTimerManager`, pauses with the game) is set for
  `MaxChargeTime` (next tick when `<= 0`), bound through a weak object and the hold id. This
  deliberately differs from the `FTSTicker` guideline, which is for effects that must hold
  real time.
- On firing: broadcast `OnFullyCharged(AActor* Attacker, UAttackData* AttackData)`
  (`FOnFullyCharged` in `CombatTypes.h`, `BlueprintAssignable` on `UCombatComponent`), log
  `[HOLD] Fully charged` under `CombatDebug::IsHoldDebugEnabled()`, and play the cue:
  `AttackData.FullChargeCue` → weapon `UCombatFXData.FullChargePool` → nothing, attached to
  the weapon mesh at `AttachSocket` or the trace tip socket (owner location without a weapon
  mesh) via a new `UCinematicEffectsUtilityLibrary::PlayAttachedCombatCue`.
- The timer is cleared on release, in `ClearHoldState` and in `EndPlay`.
- Telemetry rows `ChargeFull` and `ChargeLatched` (existing schema).

### Animation stub

`SamuraiAnimInstance::UpdateCharge`: `bIsCharging` from `ECombatState::ChargingHeavyAttack`;
`ChargePercent` from `UCombatComponent::GetCurrentChargeLevel()`, computed on read from the
clock. The AnimInstance caches the combat component in `NativeInitializeAnimation`.

### Validation

`UAttackData::IsDataValid` warns when a heavy has a `ChargeLoopSection` but
`MaxChargeTime <= 0`, and when it has a `ChargeLoopSection` but neither a
`ChargeReleaseSection` nor heavy directional follow-ups (charging leads nowhere).

### Tests (charge)

`BeginChargeForTesting(AttackData, Generation)` starts the clock and timer without a
montage. Time advances with `World->Tick`, which advances world time and timers together.

- level math, easing, curve (clamped), `MaxChargeTime <= 0`;
- multiplier capped; ×1 at level 0;
- latch: release-section keeps the generation; follow-up inherits; return-to-idle latches
  nothing; `ClearHoldState` keeps the latch; next attack gets ×1;
- timer fires once; cleared by release, `ClearHoldState` and owner destruction;
- `ChargeLevel` reaches `FHitReactionInfo`; cue resolution order; both validation warnings.

Pausing mid-charge is verified in PIE (the test world's pause behavior is not relied on).

## Part B: Attack Reach (PR 3b)

Depends on C1 (headless operation framework). The bake runs headless in 3b; C2 adds its
details-panel and Content Browser entry points.

### Reference rig

`UKatanaReachBakeSettings` (`UDeveloperSettings`, runtime module, `config=Editor`,
`defaultconfig`): `ReferenceCharacterClass` (`TSoftClassPtr<ABaseCombatCharacter>`) and
`ReferenceWeapon` (`TSoftObjectPtr<UWeaponData>`). The bake reads mesh, mesh-relative
transform and capsule radius from the class default object, and weapon mesh, attach socket
and offset, trace sockets and `TraceRadius` from the weapon data. `KatanaCombat.Build.cs`
adds `DeveloperSettings`.

### Data on UAttackData

`FAttackReachFacts` (baked, read-only under "Reach|Baked"):

| Field | Meaning |
| --- | --- |
| `bBaked` | Facts present. |
| `ReachFromStart` | Maximum horizontal distance, over the strike's Active phase, from the character's position at the start of the strike path to the blade tip (the reference weapon's effective trace end socket), plus `TraceRadius`. Root motion included; measured in any horizontal direction. |
| `ReachFromWarpEnd` | The same, from the character's position when the warp window ends. Equals `ReachFromStart` without a warp window. |
| `StrikeAxisYaw` | Direction of the `ReachFromStart` maximum relative to facing at the path start (0 = ahead, positive = right). |
| `bHasWarpWindow` | The strike path contains an `AnimNotifyState_CombatWarp` window. |
| `RigIdentity`, `Fingerprint`, `BakeVersion` | Provenance. |

**Strike paths.** A normal attack's path is `MontageSection` through its Active phase. An
attack with a `ChargeLoopSection` has two paths: uncharged (`MontageSection` Active) and
charged (`MontageSection` up to the loop, then `ChargeReleaseSection` through its Active
phase). Both are baked, and each stored fact is the per-field minimum, so a lunge never
stops out of range for either path.

`FAttackReachIntent` (authored, "Reach"): `ContactInset` (15 cm, `ClampMin=0`),
`AcquisitionBonus` (0 cm, may be negative), and `bOverrideReach` + `ReachOverride`.

`TryGetEffectiveReach` returns the override when set, else baked facts, else false (every
consumer keeps today's behavior). An override on an unbaked attack treats the warp term as
present when `WarpConfig.bEnableWarp` is set.

### Shared formulas (`AttackReachMath`, pure, runtime module)

- `WarpReach = (bEnableWarp && bHasWarpWindow) ? MaxWarpDistance : 0`
- `StopDistance = max(1, ReachFromWarpEnd + TargetCapsuleRadius − ContactInset)`; applied
  only when `bUseStopDistance` is set on the warp request (not "0 means off").
- `AcquisitionRange = clamp(ReachFromStart + WarpReach + TargetRadiusAllowance + AcquisitionBonus, 1, MaxTargetDistance)`,
  where `TargetRadiusAllowance` is the attacker's own capsule radius, a proxy for a typical
  target (candidates are not known when the range is chosen, and the reference-rig settings
  are editor-only config unavailable in cooked builds).
- `AIAttackRange = max(1, ReachFromStart + min(WarpReach, LungeAllowance) + TargetCapsuleRadius − ContactInset)`

Clamps keep every range above zero, so a negative bonus or large inset can never fall into
the `MaxRange <= 0 → SoftAimRange` sentinel. Validation warns when clamping changed a value.

### Consumers

1. **Warp stop distance.** `FAlignmentRequestSpec` gains `bUseStopDistance` and
   `StopDistance`. `ConfigureAlignmentWarpTarget` subtracts it before the `MaximumTranslation`
   clamp, only when the warp translates (distance at least `MinWarpDistance`). An attacker
   already inside turns without stepping back. It applies only when the attack has effective
   reach and a zero `TargetRelativeOffset` (authored placement wins). The defense threat
   prediction shares only the target aim-point helper; its predicted contact point stays on
   the defender.
2. **Soft-aim.** `CombatComponent` passes `AcquisitionRange` to `FindBestTargetForDirection`
   and `FindNearestTarget` when the attack has effective reach, else `-1`. Distance scoring
   (`1 − Distance / Range`) uses the same range.
3. **AI ranges.** `FEnemyAttackConfig::MaxRange` is derived as `AIAttackRange` when the attack
   has effective reach (`bUseDerivedMaxRange`, default true); `MinRange` stays authored.
   `FEnemyApproachConfig` gains `LungeAllowance` (default 100 cm). The approach task's
   acceptance radius uses the largest effective `MaxRange` in the AI's attack pool (×0.8 as
   today), so no attack needs pre-selecting; `IsInAttackRange` uses the selected attack when
   one exists, else the pool maximum. With no baked attack in the pool,
   `ApproachConfig.AttackRange` remains the fallback.

### Removals

`UWeaponData::WeaponReach`, `UWeaponComponent::GetWeaponReach` and its test. The katana's
saved 300 is ignored on load.

### Bake

- **Pose sampler** (editor adapter layer): a single-character sampler extracted from
  `UPairedAnimationAnalysisSubsystem::SampleContactPreviewPose`, shared with the paired
  contact evaluator. It attaches the weapon as runtime does and fails closed on a missing
  socket.
- **Library** (pure): reach facts from per-sample actor transforms, tip positions, phase and
  warp-window times.
- **Service**: resolves section-scoped phase times, samples each strike path at 60 Hz and
  returns facts plus fingerprint.
- **Fail closed** when: phase notifies are missing in a section (no `ManualTiming`
  fallback); a section is missing; a weapon socket is missing; the reference class or weapon
  is unset or fails to load; the montage's skeleton does not match the reference mesh's
  skeleton; headless, a source package is dirty (the editor binds in-memory state, per C2).
- **Fingerprint**: SHA-1 of sorted semantic inputs (section bounds and phase and warp notify
  times for each path, source animation data-model GUIDs and root-motion flags, rig class,
  mesh, mesh transform and capsule radius, weapon mesh, sockets, attach offset, trace radius,
  `BakeVersion`). Built by one runtime-module function under `WITH_EDITOR`, shared by the
  bake and validation.
- **Operation**: `AttackReachBake` (Preflight lists unbaked, stale and failing attacks; Plan
  lists facts to write; Apply writes them). Saving requires an approved plan.

### Validation

`IsDataValid` warns when the fingerprint is stale, the rig differs, an attack with a warp
window has no effective reach, or a clamp changed a range; it notes overrides, and
`|StrikeAxisYaw| > 45°` (the warp faces the target, so an off-axis strike may need a facing
offset or override). It computes the fingerprint from already-loaded assets and a
per-session cached rig description, so saving stays fast. `Combat.Debug.Reach` draws the stop
point, acquisition ring and AI range.

### Tests (reach)

- `AttackReachMath` formulas and clamps; negative bonus; large inset.
- Library: synthetic samples with and without warp windows, sideways and backward strikes,
  two strike paths take the minimum.
- Fingerprint deterministic and sensitive to each input; unrelated notify edits do not change
  it.
- Fail-closed cases, including skeleton mismatch.
- Warp: stop distance applied only when translating; already-inside turns only; authored
  offset wins; threat prediction keeps its defender contact point.
- Soft-aim and AI: derived ranges, pool maximum for approach, fallbacks.
- End-to-end editor test: bake `LightAttack_1` in memory and assert plausible, stable facts.

## Paired Entry Follow-Up (own step, after step 3)

Recorded so step 3 does not paint it into a corner. The entry mapping found:

- the entry lease calls `DisableMovement()` (`MOVE_None`), which clears root-motion sources;
  the mover needs input suppression in Walking instead, while the anchor may stay locked;
- `MoveToDynamicForce` ignores speed caps, travel budgets and turn limits, so our
  `AlignmentMotion` step math stays the authority (in a planar variant) and character
  movement is the actuator (the executor's movement channel in goal-seeking mode, with yaw
  on the existing zero-translation sweep);
- readiness becomes planar across preflight, the supervisor, the executor and the capture
  harness;
- six of eleven `PairedEntryTests` need a floor, a controller (or
  `bRunPhysicsWithNoController`) and a character-movement tick;
- re-qualification reproduces the rendered matrices of `BOUNDED_PAIRED_ENTRY_2026-09-11`,
  `INITIATOR_FINISHER_APPROACH_2026-09-13` and `PAIRED_ENTRY_TRANSITION_2026-09-13`.

The bounded executor and its tests remain as the kinematic reference until then.

## Serialized Data Impact

| Asset | Field | Effect |
| --- | --- | --- |
| `DA_HitReaction` | `KnockbackForce` ×8 | Ignored on load; removed on next save. |
| `HeavyAttack_1` | `ChargeTimeScale` 0.05 | Ignored on load; removed on next save. |
| `DA_Weapon_Katana` | `WeaponReach` 300 | Ignored on load; removed on next save. |
| All `UAttackData` | new fields | Defaults apply until authored or baked. |
| `DefaultEditor.ini` | `UKatanaReachBakeSettings` | New section (text, reviewable). |

## Verification

- Focused suites per commit; full baseline before each PR.
- **Knockback PIE measurement** (`KatanaCombat.Knockback.PIE.ReactionMeasurement`): real
  Light and Heavy hits on the ThirdPerson map, each measured with knockback disabled (the
  reaction animation's own root motion) and enabled; writes
  `Saved/Logs/KnockbackMeasurement.json`. It also checks that the reactions play root
  motion. They do in the assets: every `DA_HitReaction` montage uses the pack's `RootMotion`
  sequences, so the animation channel carries the push. The Heavy reactions are authored
  knockback animations (`UE5M_Root_knockback_*`) that travel about 89 cm on their own, so the
  push lands on travel the animation already has. The measurement sets the default
  `AnimationBlend` per attack type; the distances are decided: Light 25 cm, Heavy 20 cm. A
  focused PIE test replaces the planned capture-harness scenario, because the harness is one
  monolithic latent command. Flat ground, walls, ledges, hitstop and suspension are covered
  headless by the `KatanaCombat.Displacement.Executor.*` tests. Slopes ride on character
  movement's floor handling and are checked by hand in PIE.
- **Proofs the push can disturb**: `DefenseGateAPIEProofTests` (the parry bridge has a 75 cm
  per-role budget; its out-of-cone case lands a hit), `DefenseGateBSemanticPIEProofTests`
  (an unblockable hit on the player followed by a perfect parry without a position reset)
  and the `CombatCaptureScenarioTests` finisher and hold-release scenarios. They run in PR
  3a-knockback; a proof that fails only because of position disables knockback transiently
  in its fixture, and the knockback measurement and executor tests cover the behavior.
- PIE checks recorded in the PRs: both profiles, no push on block or super armor, a charged
  heavy's cue and stronger hit, pausing mid-charge pauses the charge.
- PR 3b: headless `AttackReachBake` Preflight across all 29 `UAttackData`, reviewed Plan,
  approved apply; the five directional attacks' `StrikeAxisYaw` reviewed.

## Documentation Updates

Each PR updates CLAUDE.md (defaults and status), `docs/guides/ATTACK_CREATION.md`,
`docs/architecture/API_REFERENCE.md`, and the data asset audit's execution log.

## Follow-Ups

- Block/parry pushback through the same executor.
- Capture-based runtime check of baked reach.
- Per-character reach scaling if characters diverge from the reference rig.
