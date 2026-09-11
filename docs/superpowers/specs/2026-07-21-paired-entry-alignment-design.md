# Paired Entry Alignment — Design Spec (2026-07-21, rev 2)

**Status**: Revised after adversarial verification (2 independent code-verification passes against the current working tree, HEAD 6e170503). Pending user re-review.
**Scope decision**: Standalone **finisher** path only in v1 (`ReactionType == Finisher` — see §5.0). Defense-chain stages, parries, and AC3 counters unchanged.
**Prereqs confirmed**: Root motion enabled on finisher source sequences. Root-cause context: `docs/audits/FINISHER_WARP_AUDIT_2026-07-21.md`.

**Rev 2 changes (verification findings)**: the alignment arbiter's `CharacterMovement` executor is **rotation-only** (`MoveUpdatedComponent(FVector::ZeroVector, …)`, TargetingComponent.cpp:187-191) — the translation drive is **net-new work**, now specified in §5A; priority reuses `PairedOrParryBridge` instead of a new enum value; warp handoff is an effective-config override (per-role local frames), not a raw transform; the entry step is explicitly gated to finishers and explicitly async after commit; deprecation coupling and files-touched corrected (7 files added).

## 1. Problem

Standalone finishers start both montages from wherever the actors happen to stand. There is no entry-pose concept: rotation is hardcoded to "face partner," `FPairedWarpConfig.RelativeOffset` only steers mid-montage warp convergence, and the fields meant to author facing (`VictimFacingMode`/`VictimRelativeRotation`) were never wired. Designers cannot author start alignment (arrangement, distance, facing), and out-of-position finishers play visibly misaligned.

## 2. Goals / Non-Goals

**Goals**
- Author start alignment as **intent**: an arrangement enum + one distance value.
- Runtime achieves the pose **before** montages play: short kinematic approach, snap the residual, reject beyond range (finisher declines → input falls through to a normal attack — verified current behavior, CombatComponent.cpp:4002→4032).
- Entry pose and warp destination derive from the same computed values.
- Derivation math as pure statics shared by runtime, preview, and the future selector.

**Non-Goals (v1)**: orientation-based finisher *selection* (§9); defense-chain adoption; approach montages; preview round-trip; player-victim polish beyond §5.5.

## 3. Data Model

### 3.1 New enum — `Data/PairedAnimationTypes.h`

```cpp
UENUM(BlueprintType)
enum class EPairedEntryArrangement : uint8
{
    FaceToFace,       // attacker in front, both facing each other (default)
    BehindVictim,     // attacker behind; victim keeps facing away
    VictimLeftSide,   // attacker on victim's left, facing the victim
    VictimRightSide,
    Custom            // explicit offsets/yaws (advanced)
};
```

Preview-tool mapping (`ESpatialRelationship`, PairedAnimationEditorTypes.h:78-87) is **5-of-6, not 1:1**: Facing↔FaceToFace, Behind↔BehindVictim, LeftSide↔VictimLeftSide, RightSide↔VictimRightSide, Custom↔Custom; the editor-only `Inferred` value maps to "run `MeasureCurrentArrangement`" and has deliberately no runtime authoring peer. The §4 derivation MUST stay numerically consistent with the preview's authored constraints (`FSpatialRotationConstraint::CreateForRelationship`, editor types 1248-1267: Facing 180°, Behind 0°, LeftSide 90°, RightSide −90°, tolerance 30°).

### 3.2 `FPairedEntryPose` — one property on `UPairedAnimationData`

| Field | Type / default | Meaning |
|---|---|---|
| `Arrangement` | enum = FaceToFace | Authored intent |
| `EntryDistance` | float = 100 (cm, `Units="cm"`) | Attacker distance from victim at start |
| `ApproachSpeed` | float = 1400 (cm/s, AdvancedDisplay) | Kinematic approach speed; **0 = pure snap** |
| `MaxApproachTime` | float = 0.35 (s, AdvancedDisplay) | Hard cap; on expiry → snap-or-abort per §5A.4 |
| `SnapTolerance` | float = 15 (cm, AdvancedDisplay) | Residual snapped silently |
| `SnapYawTolerance` | float = 10 (deg, AdvancedDisplay) | Rotational residual snapped silently |
| `MaxApproachRange` | float = 350 (cm, AdvancedDisplay) | Attacker→**entry-transform** distance beyond which the finisher rejects |
| `CustomAttackerYawOffset` / `CustomVictimYawOffset` / `CustomAttackerLocalOffset` | (EditCondition Arrangement==Custom) | Advanced overrides |

Note the rejection interplay: the existing finisher gate is attacker→**victim** ≤ `SoftAimRange` (500 default; PairedAnimationComponent.cpp:2781-2795), the new gate is attacker→**entry point** ≤ `MaxApproachRange`. Effective rejection begins around `MaxApproachRange + EntryDistance` (~450cm default), geometry-dependent.

### 3.3 Removals — full coupling list (verified)

`VictimRelativePosition` / `VictimFacingMode` / `VictimRelativeRotation` are dead for transform application but still **read by two live validators and several tools**. The deprecation (`_DEPRECATED` + CoreRedirects + PostLoad + resave) must touch ALL of:
- `PairedAnimationData.cpp:56-101` `HasValidNumericConfiguration` (validator 1)
- `PairedAnimationComponent.cpp:63-109` `HasValidPairedRuntimeNumerics` (validator 2)
- `PairedAnimationUtilityLibrary.h/.cpp` (`CalculateVictimTransform` family — delete with the fields; test-only callers)
- `KatanaCombatEditor` commandlets: `DefenseProofAuthoringOperation.cpp:702-704, 772-773`; `CounterChainProofMigrationOperation.cpp:100-102, 149-151`
- `KatanaCombatTest/PairedAnimationTests.cpp:1141-1142` (default-value asserts)

`FPairedWarpConfig.RelativeOffset` remains authoritative for defense-chain paths; for standalone finishers it is superseded per §5B.

## 4. Derivation Math — `UPairedAlignmentLibrary` (pure statics)

```
ComputeEntryTransforms(Arrangement, EntryDistance, Custom…, VictimTransform)
    -> { AttackerLocation, AttackerYaw, VictimYaw, bVictimYawChanges }
MeasureCurrentArrangement(AttackerTransform, VictimTransform)
    -> { Best, DeviationDegrees, Distance }
```

- **Victim anchors.** Victim keeps location; victim yaw changes only for `FaceToFace` (turn toward the attacker's **entry** location — not the attacker's live location, to avoid chasing during the approach).
- Attacker location = `VictimLocation + VictimYawRot ⋅ Dir(Arrangement) × EntryDistance`; Dir: +X front / −X behind / −Y left / +Y right / normalized custom offset.
- Attacker yaw = look-at victim (+`CustomAttackerYawOffset` under Custom). Resulting relative yaws must equal the preview constraint table (§3.1).
- Ground-adjust Z as the warp setups do.
- **Recomputed per-frame during the approach** from the victim's current transform (victim drift robustness), mirroring warp tracking's per-frame recompute.

## 5. Runtime Flow — standalone finisher path

### 5.0 Gating (new, required)
The legacy body `TryStartPairedAnimationWithTarget` is also reached by AC3 counters (comp:4392) and parry (comp:1801). The entry step runs **only when `ReactionType == EPairedReactionType::Finisher`**; other types keep today's behavior untouched.

### 5.1 Synchronous commit point
Current body is synchronous (gates → state → both montages → return). With an approach phase the flow becomes: **all pure gates + `ComputeEntryTransforms` + `MaxApproachRange` check run synchronously before any state mutation** (verified insertion point: after the pure gates, before `EnterPairedAnimationState` — currently comp:3846→3857). Rejection there returns false → caller falls through to a normal attack (verified, CombatComponent.cpp:4002-4032). On pass, the function **commits** (enters paired state, returns true) and the approach continues **async**; montages start on arrival (§5A). Interruptions during the async window roll back via the existing cleanup path plus §5A.5. **Consequence**: an abort during the async window (≤`MaxApproachTime`) has already consumed the input — no finisher plays and no normal attack substitutes; the actor returns to idle. This is rare (post-gate failures only: preemption, blocked path) and bounded at ~0.35s; accepted for v1.

### 5.2 Ordered steps after commit
1. `EnterPairedAnimationState` + partner registration (existing).
2. `BeginPairedAnimation` (existing — allocates the legacy generation, input lease comp:4719).
3. **Early collision lease** — after step 2 (needs the allocated generation; taking it earlier gets `StageGeneration==0`), via `AcquirePairedStateLease` with `bDisableMovement=false` (a movement-disabled attacker cannot be driven) and a **dedicated entry-phase handle** (the existing lease is notify-keyed mid-montage; this one has its own acquire/release lifecycle wired into rollback and EndPlay).
4. **Cancel the attacker's stale motion intents**: release any `ActiveAttackWarp` alignment request/warp from a prior combo swing (`ReleaseActiveAttackWarp`) and stop the attacker's current attack montage with a short blend — otherwise the old warp contends with the entry drive and the character slides while mid-swing.
5. **Victim stabilization**: `EnterPairedAnimationState` calls `StopMovement` only for AI controllers (HitReactionComponent.cpp:1608) and never disables movement — a player-controlled victim can drift. Entry phase zeroes victim velocity and relies on the paired movement-input suppression for the rest.
6. Entry drive (§5A) → on completion, montage play + warp handoff (§5B) — the remainder of the existing flow.

### 5A. The entry drive — extending the alignment arbiter (NET-NEW translation support)

Verification refuted "reuse guard-facing": the `CharacterMovement` executor is rotation-only. v1 builds translation into it:

1. **Spec extension** (`FAlignmentRequestSpec`, CombatTypes.h): add `bDriveTranslation`, `DesiredLocation`, `ArrivalTolerance`, `MaximumDriveSpeed`. Existing consumers unaffected (default `bDriveTranslation=false`).
2. **Tick extension** (`UTargetingComponent::TickComponent`, currently :129-234): when the active request has `bDriveTranslation`, apply a **kinematic sweep step** — `MoveUpdatedComponent(ClampedDelta, NewRotation, true)` with `|ClampedDelta| ≤ MaximumDriveSpeed × Dt` — plus **arrival detection** (distance ≤ ArrivalTolerance → notify owner). Kinematic, not velocity: required speeds (≥ ~1400 cm/s) exceed locomotion, and `MoveUpdatedComponent` works under `MOVE_None` (verified — this is how rotation already behaves on stunned actors). Existing rotation-only requests are untouched.
3. **Priority**: **reuse `EDefenseAlignmentPriority::PairedOrParryBridge`** — no enum renumbering; outranks `ActiveAttackWarp`/`BlockContact`; on the victim's component it ties with a possible hit-reaction request and wins via the newest-`AcquisitionOrder` tiebreak (verified arbitration, TargetingComponent.cpp:1131-1134). Only `Terminal` preempts — correct (death/cleanup).
4. **Stall & timeout resolution** (the arbiter deactivates silently on preemption and stops in place on release — verified): the entry controller (PairedAnimationComponent timer at `MaxApproachTime`, wall-clock per project convention) resolves every outcome identically: **if residual ≤ Snap tolerances → snap and proceed; else → abort with full rollback** (pre-montage rollback is cheap; the finisher simply doesn't happen — no half-aligned playback). This covers preemption, wall-blocked sweeps, and slow arrivals with one rule.
5. **Both roles acquire on their own components** (verified topology): attacker = rotation+translation drive; victim = rotation-only request (only for `FaceToFace`), targeting the attacker's entry location.
6. `ApproachSpeed = 0` degenerates to immediate snap; the snap uses the `SetActorLocation*`/TeleportPhysics family (precedent: sync nudge, AnimNotifyState_PairedAnimationSync.cpp:118) — always **after** the collision lease is active so capsule overlap at close arrangements cannot eject the partner.

### 5B. Warp handoff — effective config, per-role local frames

Verified: the warp `OnPreUpdate` handlers **recompute every frame from the stored `FPairedWarpConfig.RelativeOffset`** (TargetingComponent.cpp:1785, 1986) — a one-shot transform would be overwritten next tick. Mechanism: the entry step builds **effective configs** for the existing `SetupAttackerPairedWarp`/`SetupVictimWarp` calls whose `RelativeOffset` equals the derived entry offset converted to each role's frame — **attacker warp offset is victim-local, victim warp offset is attacker-local** (verified handedness) — leaving the functions' continuous-tracking behavior and all defense-chain users untouched.

## 6. Validation (`UPairedAnimationData::IsDataValid` additions)

- Error: non-finite/≤0 entry fields; `EntryDistance ≤ 0`.
- Error: montage lacks a stock `AnimNotifyState_MotionWarping` whose modifier name matches the warp config (lift the `HasNamedRotationWarp` pattern — touches `DefenseAssetValidationService` for reuse or duplicates the 20-line check locally; local copy preferred to avoid editor↔runtime dependency).
- Warning: warp translation expected but a source sequence has `bEnableRootMotion == false`.
- Warning: `CombatWarp` notify on a paired montage.

## 7. Testing

- Unit: `ComputeEntryTransforms` all arrangements × victim yaws; constraint parity with preview values (§3.1); `MeasureCurrentArrangement` sector boundaries.
- Integration (test world; BeginPlay does not run — access components directly; the approach is tick-driven, so tests must **pump `TickComponent` manually** on the targeting component): snap-within-tolerance; reject-beyond-range returns false and normal attack proceeds; `BehindVictim` leaves victim yaw untouched; abort-on-timeout rolls back fully (leases, state, no montage); effective-config warp offsets match entry pose; entry step does not run for Parry/AC3-Counter reaction types.

## 8. Telemetry & Debug

`ActionReactionTelemetry` (record-append model, `FActionReactionTelemetryRecord`) gains: `EntryArrangement`, `EntryDeviationDegrees`, `EntryApproachSeconds`, `EntrySnapDistance` fields and reason values `EntryRangeRejected`, `EntryApproachTimedOut`, `EntryAborted`. Reuse existing events (`PairedStageStartSucceeded/Failed`, `AlignmentChanged`). Debug: `Combat.Debug.PairedAnim.Warp` draws the computed entry transforms and (audit R4) the real published warp target.

## 9. Future Work (designed-for, not built)

1. **Orientation-based finisher selection (v2)**: `AttackData.FinisherData` (verified still a single `TObjectPtr`, AttackData.h:328) evolves to an arrangement-keyed collection; selection = `MeasureCurrentArrangement` → best candidate → v1 alignment absorbs the residual. Both hinge functions ship in v1.
2. **Preview round-trip (P1/P2)**: seed preview from `FPairedEntryPose`; preset buttons ↔ arrangement enum; transacted "Apply to Asset".
3. Defense-chain adoption of approach-then-snap for CounterActive.

## 10. Files Touched (v1, corrected)

Runtime: `Public/Data/PairedAnimationTypes.h` (enum + struct) · `Public/Data/PairedAnimationData.h/.cpp` (property, validators, deprecation) · `Public/Utilities/PairedAlignmentLibrary.h` + `Private/Utilities/PairedAlignmentLibrary.cpp` (new) · `Private/Utilities/PairedAnimationUtilityLibrary.h/.cpp` (remove dead transform family with the deprecated fields) · `Private/Core/PairedAnimationComponent.cpp/.h` (gating, commit flow, entry controller, validator, rollback/EndPlay) · `Private/Core/TargetingComponent.cpp` + `Public/Core/TargetingComponent.h` (translation drive, arrival, effective-config plumbing) · `Public/CombatTypes.h` (`FAlignmentRequestSpec` extension — no priority enum change) · `Public/Debug/ActionReactionTelemetry.h` + `.cpp` (fields/reasons).
Editor: `KatanaCombatEditor` commandlets `DefenseProofAuthoringOperation.cpp`, `CounterChainProofMigrationOperation.cpp` (deprecated-field reads).
Tests: `KatanaCombatTest/PairedEntryAlignmentTests.cpp` (new) · `PairedAnimationTests.cpp` (update trio asserts).
Docs: CLAUDE.md sync, audit cross-references.
