# Combat Feel and Attack Reach Design

## Status

Approved design sections (2026-09-29/30), pending review of this written spec. It covers
step 3 of the combat cleanup: Part A (knockback, charge) ships as PR 3a; Part B (attack
reach) ships as PR 3b after the editor operation workflow in
[2026-09-30-editor-asset-operation-workflow-design.md](2026-09-30-editor-asset-operation-workflow-design.md)
lands. This spec does not authorize asset saves; every asset change goes through an
approved plan.

## Problem

Three authored data surfaces do nothing at runtime, so designers tune values that have no
effect:

- **Knockback:** `FHitReactionEntry::KnockbackForce` (200 on all 8 `DA_HitReaction` entries,
  the untouched default) and `UHitReactionSettings::GlobalKnockbackMultiplier` are never read.
- **Charge:** `UAttackData::MaxChargeTime`, `ChargeTimeScale` and `MaxChargeDamageMultiplier`
  are never read. Heavy attacks already loop a charge section while the button is held, but
  charging has no effect and gives no feedback. `UMontageUtilityLibrary::CalculateChargeLevel`
  has no callers. `SamuraiAnimInstance::UpdateCharge` hard-codes zero.
- **Reach:** `UWeaponData::WeaponReach` (300 on `DA_Weapon_Katana`) is read only by a test.
  Attack warps drive the attacker toward the target's center, soft-aim uses a fixed 500 cm
  range, and AI uses a fixed 150 cm attack range, none of which reflect how far an attack's
  blade actually travels.

## Goals

- Every field in scope either drives runtime behavior as its name and tooltip say, or is deleted.
- Knockback creates spacing on landed hits without sliding characters that are not reacting.
- Charged heavies deal more damage, capped, with an event and an audiovisual cue at full charge.
- Attack reach is a measured fact about each attack's animation; designers author intent as
  adjustments to that fact, not as free-floating numbers.
- Every behavior is pinned by automated tests; the combat baseline stays green.

## Non-Goals

- Block/parry pushback (guard slide), ragdoll impulses, launchers.
- Charge-driven animation changes beyond filling the existing AnimInstance stub.
- Per-character reach tables (one reference rig is used).
- Runtime verification that baked reach matches AnimGraph output (IK, procedural blending).
- Network replication.

## Design Rules Applied

- **Rule 4 (hold = button state at window start):** whether a heavy charges is still decided
  only by the button being held when its hold window opens. Charge adds a magnitude read once
  at release; duration never gates any transition.
- **Rule 6 (delegates):** `FOnFullyCharged` is declared in `CombatTypes.h` because UI and
  Blueprints consume it.
- **Derive facts, author intent:** measurable quantities (how far a blade travels) are
  derived; feel decisions (how deep the blade should land, aim forgiveness) are authored as
  adjustments to the derived fact, with an explicit, validated override for exceptions.

## Part A: Knockback (PR 3a)

### Model

Knockback is **reaction-coupled**: a push happens only when a directional hit reaction
actually starts. It inherits every existing eligibility rule for free. Blocked and parried
hits, super armor, suppressed paired states and lethal hits (the victim is already dying
before `PlayHitReaction`) never push.

### Data

| Change | Detail |
| --- | --- |
| Add `UAttackData::KnockbackDistance` | `float`, centimeters, `ClampMin=0, ClampMax=500`, with inline override toggle `bOverrideKnockbackDistance`. When not overridden, the distance comes from the attacker's combat settings. |
| Add `UCombatSettings::DefaultKnockbackDistance` | `TMap<EAttackType, float>`, defaults `{Light: 25, Heavy: 60}`. A type missing from the map (None, Special) resolves to 0. |
| Add `UCombatSettings::KnockbackDuration` | `float`, seconds, default `0.2`, `ClampMin=0.05, ClampMax=1`. Global. |
| Rename `UHitReactionSettings::GlobalKnockbackMultiplier` → `KnockbackScale` | `float`, default 1, `ClampMin=0, ClampMax=5`. Victim-side scale: 1 normal, 0 immune. Not serialized in any asset, so a plain rename is safe. |
| Delete `FHitReactionEntry::KnockbackForce` | Its saved values (200 on 8 entries) are ignored on load. |

Resolved push distance = `ResolveKnockbackDistance(AttackData, AttackerCombatSettings)` ×
`VictimHitReactionSettings.KnockbackScale`. The attacker's combat settings come from
`FHitReactionInfo::Attacker` when it is an `ABaseCombatCharacter`; otherwise the victim's own
combat settings are used; if neither exists the distance is 0. The resolution function is
pure and unit-tested.

### Direction

Straight away from the attacker, horizontal: `CombatMath::FlatDirection(AttackerLocation,
VictimLocation)`. It deliberately does not use `DirectionToAttacker`, which is usually the
negated blade velocity. A degenerate direction (actors stacked vertically) skips the push.

### Movement

Knockback is a bounded alignment request on the victim's `UTargetingComponent`, executed by
the existing swept bounded-movement executor. Two general capabilities are added to the
alignment system; both are opt-in per request so existing requests are unaffected:

1. **`bAdvanceOnActorTime`** (`FAlignmentRequestSpec`, immutable after acquire): the
   executor advances this request on the owner's actor-dilated component delta instead of
   `AlignmentMotion::SimulationDelta`, and measures its deadline on the same clock. A victim
   frozen by hitstop (dilation 0.0001) therefore does not move until the freeze ends.
2. **`bReleaseWhenFinished`** (`FAlignmentRequestSpec`, immutable): when the request's
   outcome becomes Reached, Blocked, Exhausted or Invalid, the targeting component releases
   it itself during the same evaluation. This removes the need for owners to poll, stops
   finished requests from outranking lower-priority ones, and lets the targeting tick
   switch off.

A new priority **`EDefenseAlignmentPriority::HitKnockback`** is inserted between
`ActiveAttackWarp` and `BlockContact`. A hit therefore overrides the victim's own attack warp
(the reaction montage has already interrupted the attack), while block contact, paired and
parry bridges and terminal requests always win. No asset serializes this enum.

Request parameters for a push of distance `D` over duration `T`:

- Null target; `BoundedGoal` = world transform at `VictimLocation + Direction × D`, yaw equal
  to the victim's current yaw.
- `MotionLimits`: `TranslationSpeed = D / T`, `TravelBudget = D + PositionTolerance`,
  `Duration = T × 1.5`, `TurnRate = 0`, `TurnBudget = 0`, `YawTolerance = 180`,
  `PositionTolerance = 2`.
- `bAdvanceOnActorTime = true`, `bReleaseWhenFinished = true`, priority `HitKnockback`,
  owner id `HitKnockback` with the component's own generation counter.

A blocking sweep ends the push (outcome Blocked) and the request releases.

### Ownership and lifecycle (UHitReactionComponent)

- `StartKnockback(const FHitReactionInfo&)` is called in `PlayHitReaction` immediately after
  `PlayReactionFromEntry` succeeds. It is not called from `PlayReactionFromEntry` itself,
  because death reactions also use that function.
- Holds `KnockbackAlignmentHandle` plus a generation counter. A new push releases the
  previous one first, so a second hit restarts from the victim's current position.
- Releases the handle on `EnterPairedAnimationState` and `EndPlay`. Death is covered by the
  existing `ReleaseAllAlignmentRequests(Death)`.
- A victim without a targeting component or character movement gets no push (the acquire
  returns an invalid handle, which is not an error).

### Tests (Part A knockback)

`PlayHitReaction` cannot run in the test world (no AnimInstance before `BeginPlay`), so tests
call `StartKnockback` through a friend declaration, and drive the targeting component with
the manual-tick pattern from `BoundedAlignmentTests.cpp`:

- distance resolution: override, per-type default, missing type → 0, attacker vs victim
  settings fallback, `KnockbackScale` 0 → no request;
- direction: away from the attacker regardless of `DirectionToAttacker`; degenerate → no push;
- movement reaches `D` in `T` at normal dilation; at dilation 0.0001 it does not advance;
- wall ahead → Blocked and released;
- `bReleaseWhenFinished` releases on each terminal outcome; requests without it keep today's
  behavior;
- a higher-priority request (BlockContact) suspends the push; paired entry releases it;
- priority ordering: HitKnockback beats ActiveAttackWarp, loses to BlockContact.

## Part A: Charge (PR 3a)

### Data

| Change | Detail |
| --- | --- |
| `UAttackData::MaxChargeTime` | Now read. `ClampMin=0`. Validation warns when a heavy with a `ChargeLoopSection` has `MaxChargeTime <= 0` (it charges instantly). |
| `UAttackData::MaxChargeDamageMultiplier` | Now read. `ClampMin=1`, so charging can never reduce damage. |
| Delete `UAttackData::ChargeTimeScale` | `HeavyAttack_1`'s saved 0.05 is ignored on load. |
| Add `UAttackData::FullChargeCue` | `FCombatCueConfig` (new struct in `CombatTypes.h`): `Sound`, `VFX`, `AttachSocket` (`FName`, None = the weapon's trace tip socket), `VolumeMultiplier`, `VFXScale`. Optional per-attack override. |
| Add `UCombatFXData::FullChargePool` | `FImpactFXPool`. Weapon-level default cue with random selection and pitch variation. Surface alignment fields are ignored for attached cues. |

### Measurement

- The charge clock starts in `OnHoldWindowStartWithContext`'s heavy branch once the hold is
  activated (the loop is playing). It records `FPlatformTime::Seconds()` and the hold id.
- Charge level = `UMontageUtilityLibrary::CalculateChargeLevel(HeldSeconds, MaxChargeTime,
  EEasingType::Linear, nullptr)`, which clamps to [0, 1] and returns 1 when
  `MaxChargeTime <= 0`. The function's default easing (`EaseInQuad`) is not used.
- Damage multiplier = `Lerp(1, MaxChargeDamageMultiplier, Level)`. Holding longer than
  `MaxChargeTime` never increases it.
- The clock is wall time, so world slow motion does not slow charging.

### Latching and application

- In `DeactivateHoldWithInputSerial`'s heavy branch, before `TerminateHoldIfMatches`, the
  component latches `{AttackGeneration, Level}`.
- `UCombatComponent::GetChargeDamageMultiplier(int32 AttackGeneration)` returns the latched
  multiplier only when the generation matches, else 1. Any new attack or combo step changes
  the generation, so a charge never leaks into the next swing. The latch is also cleared in
  `ResetTerminalAttackState`.
- Both damage sites multiply by it: `WeaponComponent.cpp` (primary, using the contact's
  attack generation) and the legacy `BaseCombatCharacter.cpp` non-character path.
- Releasing before the charge loop starts leaves no latch (×1). Counter and finisher damage
  paths are unchanged.

### Full-charge feedback

- At clock start, one `FTSTicker` callback is scheduled for `MaxChargeTime` seconds (fires on
  the next ticker update when `MaxChargeTime <= 0`). It captures weak pointers to the
  component and world and the hold id, and does nothing if the owner is gone or the hold is
  no longer current.
- On firing it broadcasts **`OnFullyCharged(AActor* Attacker, UAttackData* AttackData)`**
  (`FOnFullyCharged` in `CombatTypes.h`, `BlueprintAssignable` member on `UCombatComponent`),
  logs `[HOLD] Fully charged` under `CombatDebug::IsHoldDebugEnabled()`, and plays the cue.
- Cue resolution: `AttackData.FullChargeCue` (if it has a sound or VFX) → weapon
  `UCombatFXData.FullChargePool` → nothing. The cue attaches to the spawned weapon mesh at
  `AttachSocket` or the weapon's trace tip socket; without a weapon mesh it plays at the
  owner's location. A new helper
  `UCinematicEffectsUtilityLibrary::PlayAttachedCombatCue` provides attached playback beside
  the existing impact helpers.
- The ticker handle is removed on release, in `ClearHoldState` (which every interrupt,
  cancel, stun and death path already calls) and in `EndPlay`.

### Animation stub

`SamuraiAnimInstance::UpdateCharge` sets `bIsCharging` from
`ECombatState::ChargingHeavyAttack` and `ChargePercent` from
`UCombatComponent::GetCurrentChargeLevel()`, computed on read from the charge clock (no
tick). The AnimInstance caches the combat component in `NativeInitializeAnimation`, like the
existing hit reaction component cache.

### Tests (Part A charge)

A heavy's loop cannot start without a montage in the test world, so
`BeginChargeForTesting(AttackData, AttackGeneration)` starts the clock and timer directly.

- level math: 0, partial, clamped at 1 past `MaxChargeTime`, `MaxChargeTime <= 0` → 1;
- multiplier: ×1 at level 0, capped at `MaxChargeDamageMultiplier`;
- latch: matching generation applies; next generation gets ×1; release before the loop → ×1;
- timer: fires once at `MaxChargeTime` (driven with `FTSTicker::GetCoreTicker().Tick`);
  cancelled by release, by `ClearHoldState`, and by owner destruction;
- cue resolution order and the no-cue case;
- `IsDataValid` warning for `MaxChargeTime <= 0` on a charging heavy.

## Part B: Attack Reach (PR 3b)

Part B depends on the editor operation workflow spec. The bake is that workflow's first new
client.

### Reference rig

Reach is a fact about (attack, character, weapon). One reference rig is used for every
attack. It is **derived** rather than hand-entered:

- `UKatanaReachBakeSettings` (`UDeveloperSettings`, runtime module, `config=Editor`,
  `defaultconfig`, shown under Project Settings) has two fields: `ReferenceCharacterClass`
  (`TSoftClassPtr<ABaseCombatCharacter>`) and `ReferenceWeapon` (`TSoftObjectPtr<UWeaponData>`).
- The bake reads the skeletal mesh, mesh-relative transform and capsule radius from the
  character class default object, and the weapon mesh, attach socket, attach offset, trace
  sockets and `TraceRadius` from the weapon data. Nothing about geometry is typed by hand.

### Data on UAttackData

`FAttackReachFacts` (baked, cooked, shown read-only under "Reach|Baked"):

| Field | Meaning |
| --- | --- |
| `bBaked` | Facts present. |
| `ReachFromStart` | Maximum forward distance, over the Active phase, from the character's position at section start to the blade tip, plus `TraceRadius`. Root motion is included. The blade tip is the weapon's effective trace end socket (`UWeaponComponent::GetEffectiveEndSocketName` semantics, resolved from the reference weapon data). |
| `ReachFromWarpEnd` | The same, measured from the character's position when the attack's warp window ends. Equals `ReachFromStart` when the section has no warp window. |
| `RigIdentity` | Reference character class path and weapon path used. |
| `Fingerprint` | Semantic fingerprint (see Bake). |
| `BakeVersion` | Bake algorithm version. |

`FAttackReachIntent` (authored, under "Reach"):

| Field | Default | Meaning |
| --- | --- | --- |
| `ContactInset` | 15 cm | How deep into the target the blade should land when a warp stops. |
| `AcquisitionBonus` | 0 cm | Extra soft-aim forgiveness. |
| `bOverrideReach` + `ReachOverride` | off | Replaces both facts with one value when the measured fact is wrong for gameplay. |

`UAttackData::TryGetEffectiveReach(float& OutFromStart, float& OutFromWarpEnd)` returns the
override when set, else the baked facts when baked, else false. "False" means every consumer
keeps today's behavior.

### Shared formulas

A pure `AttackReachMath` namespace in the runtime module (used by runtime and editor):

- `StopDistance = max(0, ReachFromWarpEnd + TargetCapsuleRadius − ContactInset)`
- `AcquisitionRange = min(ReachFromStart + MaxWarpDistance + AcquisitionBonus, MaxTargetDistance)`
- `AIAttackRange = ReachFromStart + MaxWarpDistance + TargetCapsuleRadius − ContactInset`

`TargetCapsuleRadius` is the target's capsule radius when it is an `ACharacter`, else 0.

### Consumers

1. **Attack warp stop distance.** `FAlignmentRequestSpec` gains `StopDistance` (cm, 0 = off).
   The warp goal becomes `TargetLocation − Direction2D × StopDistance`, then the existing
   `MaximumTranslation` clamp applies. If the attacker is already within `StopDistance`, the
   request turns without translating (it never steps back). `SetupAttackWarp` sets
   `StopDistance` only when the attack has effective reach **and** its `TargetRelativeOffset`
   is zero; an authored offset keeps its explicit placement. The warp goal computation is one
   function shared with the defense threat prediction (`CombatComponent` threat endpoint),
   so prediction and execution agree.
2. **Soft-aim acquisition.** The `CombatComponent` callers of `FindBestTargetForDirection` and
   `FindNearestTarget` pass `AcquisitionRange` when the attack has effective reach, else `-1`
   (today's `SoftAimRange`).
3. **AI attack range.** `UEnemyCombatAIComponent::GetEffectiveAttackRange()` returns
   `AIAttackRange` for the selected attack when it has effective reach, else
   `ApproachConfig.AttackRange`. `IsInAttackRange` (its empty `SelectedAttack` branch) and the
   StateTree approach task's acceptance radius both use it.

### Removals

`UWeaponData::WeaponReach`, `UWeaponComponent::GetWeaponReach` and its test are deleted.
`DA_Weapon_Katana`'s saved 300 is ignored on load. No runtime blade-length value is added;
blade geometry enters through the bake.

### Bake

Layers follow the editor three-layer rule:

- **Pose sampler (adapter layer, editor module).** A single-character sampler extracted from
  `UPairedAnimationAnalysisSubsystem::SampleContactPreviewPose`. It places the reference
  character mesh in a preview scene with the mesh-relative transform, attaches the weapon
  exactly as runtime does (fail closed if the attach socket is missing), poses the montage at
  time `t` in single-node mode, and accumulates root motion with
  `UAnimMontage::ExtractRootMotionFromTrackRange`. The paired contact evaluator is refactored
  to use the same sampler.
- **Library (pure).** Given per-sample actor transforms, tip positions, phase times and the
  warp-window end time, computes `ReachFromStart` and `ReachFromWarpEnd`. Forward distance is
  measured along the character's facing at the reference time, horizontally.
- **Service.** For one `UAttackData`: resolves section-scoped phase times
  (`GetSectionTimeRange` plus the section's Active and Recovery phase-transition notifies),
  finds the section's `AnimNotifyState_CombatWarp` end if present, samples at 60 Hz from
  section start to the end of Active, and returns facts plus fingerprint.

Fail closed (no bake, explicit error) when: phase notifies are missing in the section (the
`ManualTiming` fallback is never used); the section is not found; the weapon attach or trace
sockets are missing; the reference class or weapon is unset or fails to load; a source
package is dirty.

**Fingerprint:** SHA-1 of sorted `key=value` lines over semantic inputs: section bounds and
phase and warp notify times; each source animation's data-model GUID and root-motion flag;
the reference character class path, skeletal mesh path, mesh-relative transform and capsule
radius; weapon mesh path, attach socket and offset, trace socket transforms and trace radius;
and `BakeVersion`. Package bytes are not hashed, so editing an unrelated notify on a shared
montage does not mark every attack stale. The fingerprint builder lives in the runtime module
under `WITH_EDITOR` so both the bake and `IsDataValid` use one implementation.

**Entry points** (through the editor operation workflow): an `AttackReachBake` operation
with Preflight (reports unbaked, stale and failing attacks), Plan (facts to write per
attack), and Apply. It is available as a details-panel button, a Content Browser action on
selected `UAttackData`, and headless via `KatanaAssetMigration`. Saving requires an approved
plan.

### Validation

`UAttackData::IsDataValid` recomputes the fingerprint and **warns** when it differs from the
stored one, when the rig identity differs from the current settings, or when an attack with a
warp window has no effective reach. It lists overrides as informational. Staleness never
blocks saving. Runtime cannot recompute fingerprints and uses stored values.

### Stated limitation

The bake measures the authored montage and root motion. It does not model the AnimGraph
(procedural blending, IK), so it is a fact about the animation, not a per-frame runtime
guarantee.

### Tests (Part B)

- `AttackReachMath`: each formula, clamping, zero radius;
- library: reach from synthetic samples, with and without a warp window, root motion
  included, backward tip ignored;
- fingerprint: deterministic; changes when each input changes; unchanged by an unrelated
  notify edit;
- fail-closed cases (missing phase notify, missing socket, unset rig);
- warp: stop distance applied, already-inside turns only, authored `TargetRelativeOffset`
  wins, threat prediction equals executed goal;
- soft-aim and AI: effective reach used when present, unbaked falls back to today's values;
- end-to-end: bake `LightAttack_1` against the reference rig in an editor test and assert
  plausible, stable facts (without saving).

## Serialized Data Impact

| Asset | Field | Effect |
| --- | --- | --- |
| `DA_HitReaction` | `KnockbackForce` ×8 | Ignored on load; removed on next save. |
| `HeavyAttack_1` | `ChargeTimeScale` 0.05 | Ignored on load; removed on next save. |
| `DA_Weapon_Katana` | `WeaponReach` 300 | Ignored on load; removed on next save. |
| All `UAttackData` | new fields | Defaults apply until authored or baked. |
| `DefaultEditor.ini` | `UKatanaReachBakeSettings` | New section with the reference class and weapon (text, reviewable). |

## Verification

- Focused suites per commit; full baseline before each PR.
- PIE checks recorded in the PR: a light and a heavy hit push the enemy visibly, no push on
  block or super armor, charged heavy logs `[HOLD] Fully charged` and hits harder.
- PR 3b: headless `AttackReachBake` Preflight across all 29 `UAttackData`, reviewed Plan,
  then an approved apply.

## Follow-Ups (out of scope)

- Block/parry pushback reusing `StartKnockback`'s movement path.
- Capture-based runtime check of baked reach.
- Per-character reach scaling if characters diverge from the reference rig.
