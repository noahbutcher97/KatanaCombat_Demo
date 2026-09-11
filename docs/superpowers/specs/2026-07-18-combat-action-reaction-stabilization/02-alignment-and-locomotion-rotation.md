# Micro-Spec 02: Alignment And Locomotion Rotation

## Purpose

Give attacks and locomotion independent, configurable rotation capabilities while retaining the existing alignment owner, generation, Motion Warping windows, and simulation-time rate normalization.

## Evidence And Precedent

Current attack setup computes `min(DefenseTurnRate, AttackRotationSpeed)` and uses the defense turn budget. This reduces an authored 720-degree/second attack to the 180-degree/second defense default and commonly stops at the defense 70-degree budget.

The 2026-07-19 `Combat.Debug.All` capture in `Saved/Logs/KatanaCombat-backup-2026.07.19-15.23.35.log` confirms the runtime path rather than only the static defect. Seven player attacks acquired `RotationTarget` without a target (lines 53058-56925), and `UAnimNotifyState_CombatWarp` selected rotation-only mode each time. Requested turns included 45, 90, and 135 degrees, but observed character-yaw changes were commonly only 12-24 degrees before a replacement/interruption or the next input sample. Examples include 131.2 to 143.2 toward 176.2 (lines 53059-53119), -88.5 to -100.9 toward -178.5 (lines 55620-55686), and -100.9 to -124.9 toward 124.1 before/after an interrupted 0.134-second attack (lines 55827-55880). Input capture and target publication work; completion does not.

The current queue also loses intent provenance. `OnInputEventAuto()` derives a direction at the physical edge, but `FActionQueueEntry` does not retain it and `SetupAttackWarp()` later rereads mutable `LastMovementInput`. A delayed attack can therefore face a newer or stale sample. Existing tests prove returned target rotation, request configuration, and modifier ownership, but none proves that a real no-target attack reaches the requested actor yaw.

Epic's [Motion Warping documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-warping-in-unreal-engine) defines authored warp windows as the place to adjust root motion toward runtime targets. UE 5.6 `RootMotionModifier.cpp` applies constant-rate limits in animation time; the project's existing `UTargetingComponent` compensates for montage play rate to preserve simulation-time yaw.

A read-only comparison with OperationPhoenix/OnSight found four useful pure direction policies (`LinearLerp`, `Magnetic`, `WeightedAngle`, and `ConeClamp`) plus a per-frame warp-target refresh. It also exposed reasons not to transplant that implementation: ordinary refresh is currently disabled, live targets bypass input and hard-face the target, target selection and rotation steering share settings, raw input is polled from several fallbacks, exact antipodal math remains ambiguous, and most regression coverage is source-shape rather than actor-yaw behavior. KatanaCombat adopts bounded angular policies and runtime target refresh, but keeps them separate from acquisition and routes them through the existing owner-aware alignment executor.

Installed UE 5.6 source confirms the safe update order. `UMotionWarpingComponent` broadcasts `OnPreUpdate` before modifier updates; an active modifier's `OnUpdateDelegate` runs after current animation positions/play rate are captured and before `URootMotionModifier_Warp` samples the warp target. Regular attack steering therefore updates the exact registered modifier owner from that callback, not from an unrelated actor tick or a GAS/notify lookup.

`UCharacterMovementComponent` uses `RotationRate` for desired/orient-to-movement rotation. That locomotion setting is not an attack-alignment substitute.

## Attack Alignment Contract

`FAttackWarpConfig` owns:

- `RotationSpeed`: maximum simulation-time yaw rate;
- `MaximumAutomaticTurn`: cumulative yaw budget for the attack, initially 180 degrees and clamped to 0-360; reviewed steering-enabled attacks must budget their base turn and steering envelope;
- `FinalFacingTolerance`: accepted residual yaw, initially 10 degrees and validated in the range 0.1-45;
- a closed failure policy: `BestEffort` for compatibility or `RequireReachable` for accepted production attacks;
- one nested `FAttackRotationSteeringConfig` for live player influence;
- existing target/no-input cones and translation limits.

Defense settings never cap a regular attack request. `UTargetingComponent` remains the only executor and continues to use `EAlignmentExecutor::MotionWarping`, owner generations, one-writer arbitration, and play-rate normalization.

`UCombatComponent` builds one immutable `FAttackAlignmentStartContext` after montage start and checkpoint discovery. It carries an `FAttackInstanceId`, exact montage instance and section, weak selected target, target/fallback provenance, immutable base facing intent, a by-value `FAttackWarpConfig`, effective damage deadline in montage time, and whether this locally controlled regular attack may consume live steering. `UTargetingComponent` validates and copies that context into private state bound to the acquired alignment handle; it never retains mutable AttackData as runtime authority. The current Blueprint-callable loose `SetupAttackWarp(Target, Rotation, Config)` surface remains only as a deprecated `BestEffort`/`Disabled` compatibility adapter until asset-reference audit proves removal is safe. It cannot satisfy strict production validation.

Every player attack edge carries one immutable base facing intent. Normalized world-space movement direction is authoritative; desired yaw and the eight-way branch direction are derived from it, while capture time and provenance remain diagnostic. The eight-way value remains appropriate for branch selection; startup and queued execution do not reread `LastMovementInput` or rebuild base yaw from the character's later rotation. Queued input retains the original edge's intent. A hold-owned follow-up captures the same context at physical release under Micro-Spec 04A. Programmatic/AI attacks carry an explicit target or desired-facing context instead of fabricating player input.

Facing-source precedence is closed and reason-coded: a valid explicit/paired/AI target, then a valid player soft target selected by canonical targeting policy, then the attack edge's captured nonzero world direction, then the facing yaw captured on that edge when movement is inside the dead zone. A targetless attack with input must therefore rotate toward that exact world direction before contact; a targetless attack without input preserves its captured facing and cannot reuse a stale movement vector. If a selected target becomes invalid before execution, the request falls back to the already captured player intent. Any later soft-target acquisition or retarget must be an explicit targeting decision in telemetry and cannot mutate the stored fallback intent.

An exactly antiparallel input must not depend on floating-point cross-product sign. Resolve signed yaw with `FMath::FindDeltaAngleDegrees`; when the absolute delta is within 0.1 degrees of 180, force the positive-yaw branch. Publish a target at `180 - BiasDegrees`, where `BiasDegrees` is the smaller of 0.5 degrees and half `FinalFacingTolerance`. This keeps the requested result within tolerance while preventing UE constant-rate Motion Warping from receiving a zero-sign antipodal target. Telemetry records whether the tie-break was used. No direct transform write is permitted.

## Live Player Steering Contract

The immutable attack-edge intent and live steering are different signals. The edge determines target acquisition, branch selection, startup preflight, fallback, and queue identity. While the exact regular-attack warp modifier is active, a terminal-aware live movement sample may offset only its rotation target. `UCombatComponent` owns the one canonical sample, carrying finite magnitude clamped to 0-1, normalized world direction, input serial, and observation time. `APlayerCharacter::Move` forwards its raw camera-relative value and current control yaw before movement application; CombatComponent rejects non-finite components, clamps diagonal/digital magnitude without discarding direction, derives world direction, and lets the physical attack edge snapshot that same record. `Completed` and `Canceled` clear it. Input-component teardown, unpossession, component EndPlay, and owner EndPlay also clear it idempotently. `UTargetingComponent` reads a value copy for the exact owner and never polls raw Enhanced Input, CMC acceleration, or stale `LastMovementInput` as alternate steering authorities.

`EAttackRotationSteeringMode` is closed to:

- `Disabled`: retain the base reference;
- `Weighted`: apply `SignedInputAngle * InputWeight`;
- `DeadZoneCurve`: ignore angular input through `DeadZoneAngle`, then raise normalized progress toward `FullInfluenceAngle` by `CurveExponent` and scale to the maximum deviation;
- `ConeClamp`: follow input directly inside the authored deviation cone and clamp at its edge.

Serialized compatibility/default values are:

| Field | Default | Valid range |
|---|---:|---:|
| `Mode` | `Disabled` | closed enum |
| `InputMagnitudeDeadZone` | `0.20` | 0-0.95 |
| `MaximumInputDeviation` | 25 degrees | 0-180 degrees |
| `TargetResponseRate` | 360 degrees/second | 1-1800 when enabled |
| `InputWeight` | `0.35` | 0-1 |
| `DeadZoneAngle` | 10 degrees | 0-90 degrees |
| `FullInfluenceAngle` | 90 degrees | greater than `DeadZoneAngle`, at most 180 degrees |
| `CurveExponent` | `2.0` | 0.1-5 |

All modes first treat magnitude at or below `InputMagnitudeDeadZone` as zero. Above it, `InputStrength = Clamp((Magnitude - InputMagnitudeDeadZone) / (1 - InputMagnitudeDeadZone), 0, 1)` scales the selected angular result, preserving analog authority while keyboard input remains full strength. Modes then use signed yaw, the same antipodal tie-break, and a final clamp to `MaximumInputDeviation`. `Weighted` owns `InputWeight`. `DeadZoneCurve` computes `Alpha = Pow(Clamp((AbsAngle - DeadZoneAngle) / (FullInfluenceAngle - DeadZoneAngle), 0, 1), CurveExponent)` and returns `Sign * MaximumInputDeviation * Alpha * InputStrength`. `Weighted` and `ConeClamp` likewise multiply their offset by `InputStrength`. Zero/terminal input requests zero offset and returns toward the current base reference at `TargetResponseRate`. Target response is simulation-time based and independent of montage play rate. A zero or non-finite simulation delta leaves the published target unchanged, so hitstop/pause cannot accumulate hidden steering. Target-relative reference yaw may track a surviving target; targetless reference yaw remains the immutable edge intent.

The pure steering resolver receives base yaw, actor yaw, prior published yaw, live world input, simulation delta, time to the effective damage deadline, remaining turn budget, and config. It rate-limits target motion, clamps the result to the attack's reference cone, and then clamps again to what the actor can reach with its remaining time/rate/budget plus `FinalFacingTolerance`. It never spends the budget reserve needed to return from the accepted offset to the base reference. Strict static validation therefore requires `MaximumAutomaticTurn >= MaximumSupportedBaseTurn + 2 * MaximumInputDeviation`; runtime uses the accepted current offset rather than always reserving the maximum. Input arriving too late or after budget pressure produces a partial, reason-coded steering contribution rather than an unreachable target or snap. At the effective damage deadline, the last reachable rotation target freezes through warp end.

Live steering never changes selected target identity, target-selection eligibility, translation destination, attack branch, attack generation, or montage. It is disabled for AI, defense alignment, guard/parry response, paired animation, counter sync, finishers, and any other non-regular alignment priority. A normal player attack may use a moving target as its base reference, but meaningful-delta hysteresis of 0.1 degrees for rotation and 1 cm for tracked translation prevents identical target rewrites and input noise from resetting the warp calculation each frame.

Before attack start, target-based callers evaluate an immutable alignment preflight:

- target and desired yaw are finite and current;
- required yaw is within the attack's budget plus tolerance;
- required authored warp target/window exists and has enough simulation time before the earliest gameplay alignment deadline for the configured rate when warping is mandatory;
- a steering-enabled strict player attack can cover its reviewed base turn plus authored steering envelope when input is held from warp start;
- translation is within the configured range.

The alignment deadline is the earliest event in the selected montage section that can enable damage: canonical Active/Hit-window begin or an earlier surviving `UAnimNotify_ToggleHitDetection` enable, capped by warp end. An earlier legacy toggle is both the effective deadline and a migration defect. A damaging `RequireReachable` attack with no discoverable damage boundary or warp window fails validation; non-damaging presentation is outside this attack path.

AI does not start or spend a `RequireReachable` attack action when alignment is unreachable. Player soft targeting excludes unreachable candidates and first falls back to the captured rotation-only input intent. If explicit player intent is still unreachable, preflight rejects before montage start and records disposition `Rejected` with reason `UnreachableAlignment`; it never silently consumes the edge, snaps, or starts `BestEffort`. Production validation therefore requires every strict player attack to reach all supported explicit angles before the effective damage deadline and every non-paired normal player attack to have a reviewed non-`Disabled` steering mode. Explicit player direction may require a 180-degree base turn; steering-enabled assets must author enough cumulative budget for that turn and their deviation envelope. `BestEffort` exists only as a migration/legacy mode and cannot satisfy alignment acceptance. No path uses `SetActorRotation` to repair a failed attack.

If an external reaction terminates the attack, its alignment owner terminates with it; finishing the abandoned turn is not required. If the attack remains active when its warp window ends outside facing tolerance, that is a contract violation: `RequireReachable` should have rejected startup, and telemetry must report the residual error rather than silently accepting the attack.

## Locomotion Contract

- Player locomotion rotation rate is an exposed defaults property, not a constructor magic number.
- The initial tuning default is 540 degrees/second and must be validated in PIE.
- AI approach/circle speed is owned by AI state in Micro-Spec 03.
- Movement intent routing and whether it cancels an active action are owned by Micro-Spec 04.
- Raising locomotion yaw may improve navigation facing, but it cannot be accepted as proof of attack alignment.

## Required Cases

| Case | Required result |
|---|---|
| attack rate exceeds defense rate | attack uses its own rate |
| attack requires more than its turn budget | preflight rejects; no partial misaligned start |
| montage play rate changes | final simulation-time yaw remains equivalent within tolerance |
| stale alignment callback | cannot release a newer request |
| no valid warp notify/target | deterministic fallback or rejection according to config; never direct snap |
| legacy loose setup adapter | remains non-strict with steering disabled and emits a compatibility reason |
| no target with 45/90/135/180-degree input | uses the captured world intent and reaches tolerance before contact when authored as reachable |
| no target after movement Completed/Canceled | preserves attack-edge facing; does not reuse the prior movement vector |
| exact antiparallel input | selects positive yaw, applies the bounded bias, and reaches tolerance without a zero-sign stall |
| strict player intent is unreachable | records `Rejected/UnreachableAlignment` before montage start; no silent consume, snap, or partial damaging start |
| legacy hit-enable precedes Active | uses the earlier deadline and reports a migration defect |
| queued attack after input/camera/character changes | retains the original attack edge's facing intent |
| targeted attack with live left/right input | keeps the same target and translation while rotation follows the configured bounded policy |
| targetless attack with live input change | retains the edge reference but smoothly offsets the active rotation target |
| movement input Completed/Canceled during warp | clears live influence and returns toward the base reference without stale steering |
| exact antiparallel live steering input | uses the positive-yaw tie-break and never stalls at a zero cross product |
| input changes immediately before damage | clamps to the reachable contribution and records the limiting reason |
| oscillating/noisy input | respects magnitude dead zone, response rate, hysteresis, and return-budget reservation |
| held stick while camera yaw changes | new Move samples update live world steering; immutable edge reference and branch do not change |
| partial analog magnitude | remaps smoothly from zero at the authored dead zone to full configured angular influence at magnitude one |
| diagonal digital input or non-finite sample | diagonal magnitude clamps to one; non-finite input is rejected and cleared with a reason |
| hitstop or zero simulation delta | steering target does not advance and resumes without a catch-up jump |
| paired/defense/AI alignment while player input changes | receives no player steering contribution |
| attack is externally interrupted | releases its exact alignment owner; no abandoned turn continues |
| locomotion input while idle | character reaches configured yaw rate without changing attack warp policy |

## Data Compatibility

Existing attacks receive the defaults `MaximumAutomaticTurn = 180.0f`, `FinalFacingTolerance = 10.0f`, `FailurePolicy = BestEffort`, and `Steering.Mode = Disabled` until audited. The migration reports required warp targets/windows, warp start/end, effective damage deadline, usable pre-contact duration, steering envelope, current budget, post-modifier Move/IMC dead-zone configuration, and every attack whose rate/deadline product cannot satisfy its configured base turn plus reviewed steering envelope. It then assigns a reviewed non-`Disabled` mode to active unpaired player attacks and sets only validated attacks to `RequireReachable`. Paired/sync content remains explicitly `Disabled`. Content migration may tune individual attacks only through an allowlisted audit/plan/apply operation. Initial values are playtest baselines, not final balance claims.

## Acceptance

- Automation proves defense settings no longer affect attack rotation, turn-budget/window rejection, immutable queued intent, all steering algorithms, terminal input, late-input reachability, owner isolation, and play-rate invariance.
- Automation proves stale montage/attack contexts and the loose compatibility adapter cannot acquire strict or steerable alignment.
- Source checks prohibit direct attack `SetActorRotation` repair.
- PIE telemetry records facing source, target selection/fallback reason, captured base intent, live input serial/world direction, steering mode/raw/clamped offset, response/reachability clamp reason, published yaw, chosen antipodal sign, applied yaw, rate, budget, alignment deadline, preflight result, interruption/release reason, and final error at contact and warp end.
- Representative player and AI attacks finish within the accepted facing tolerance without snapping.
