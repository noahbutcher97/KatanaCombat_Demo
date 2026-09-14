# Paired animation contact and alignment evaluation

The Katana editor adapter evaluates declared contact intent on existing paired animation assets. It shares measurements and authored pose sampling between the paired preview and a commandlet. Install the pinned AnimationAnalysis dependency before building or running the tools; project-specific interpretation remains in `KatanaCombatEditor`.

The [suite architecture contract](../architecture/ANIMATION_ANALYSIS_SUITE.md) includes this evaluator, commandlet, preview integration and profiles. Shared capture, geometry and review code is consumed through the pinned [AnimationAnalysis dependency](../../Tools/AnimationAnalysis/README.md). Katana owns its paired-data, weapon and sync interpretation. The project commands and APIs below remain the consumer entry points.

## Run an existing pair

From the repository root:

```powershell
python Tools/CombatCapture/evaluate_pair.py --profile Tools/CombatCapture/pairs/finisher-contact.json
python Tools/CombatCapture/evaluate_pair.py --profile Tools/CombatCapture/pairs/counter-contact.json
```

The runner builds the editor, invokes the native evaluator, and prints the unique artifact directory. Use `--skip-build` only after building the current sources. Outputs are `Saved/PairedAnimationEvaluations/<UTC-GUID>/evaluation.json` and `report.html`; command, process exit, source identity and build evidence are under `Saved/PairedEvaluationRuns/`. Exit 0 means the configured criteria passed; exit 1 means failure or inconclusive evidence. The native commandlet's exit 0 means evaluation completed, so callers must inspect the report to determine the criterion outcome.

The tracked finisher and counter profiles are **diagnostic examples**, with provisional dimensions and tolerances. Their failures identify intervals for review; they are not approved artistic-quality judgments. Neither profile is automatically promoted to a reference. Existing asset paths retain legacy names because renaming assets is separate work.

## Compare gameplay with authored playback

```powershell
python Tools/CombatCapture/run_scenario.py --map all --variant Completed --mode rendered
python Tools/CombatCapture/evaluate_pair.py --profile Tools/CombatCapture/pairs/finisher-contact.json --capture Saved/CombatCaptures/<capture-directory> --skip-build
```

Use the capture path printed by the first command. Run the second command separately for each map. The current finisher scenario nominates the attached katana mesh for `weapon_start` and `weapon_end`, and captures the victim's `spine_03`, `spine_05` and `neck_01`. The latter two are anatomical observation points, not additional approved contact criteria. The generic recorder also accepts explicit point-source components through `FCombatCaptureParticipant::PointSources`. Point transforms include source component, source asset when static, attachment parent and socket; component-relative coordinates remain relative to the nominated skeletal mesh. Destroyed sources stay missing, even if a replacement component exists.

For DefenseMatrix, use `--profile Tools/CombatCapture/pairs/finisher-contact-mannequin.json`: that map uses `SKM_Manny_Simple` as its victim. The profile changes only the victim mesh and its explanatory basis. The default finisher profile matches ThirdPerson's mercenary. The tool rejects a mesh/profile mismatch instead of inferring compatibility.

Runtime comparison requires a complete schema-2 scenario capture with matching current project asset hashes and the required point-source metadata. Older captures remain readable by the general analyzer but cannot establish these new contact assertions. The finisher scenario is version 6; this version adds bounded-entry observations and checks. Prior versions' displacement references must not silently be reused.

Authored, runtime and relative-alignment cases are reported separately. No capture means runtime is `not_run`. Missing or stale poses, changed component identities, incompatible assets, unsupported time dilation/play rate, repeated paired instances, a reversed montage clock, or gaps exceeding the declared bound produce inconclusive evidence. Runtime contact uses the actual final evaluated world points, including gameplay blending. It does not substitute preview poses for gameplay.

## Evaluate bounded entry preparation

Entry is opt-in and is a separate placement policy from the montage warp endpoint. Supply a complete JSON object to `run_scenario.py --entry-config <path>` together with `--finisher-experiment paired-warp-tuning` and its usual warp arguments. For example:

```json
{
  "enabled": true,
  "victim_offset_cm": [80, 0, 0],
  "victim_yaw_deg": 0,
  "duration_s": 0.5,
  "translation_speed_cm_s": 300,
  "travel_budget_cm": 150,
  "turn_rate_deg_s": 540,
  "turn_budget_deg": 180,
  "position_tolerance_cm": 2,
  "yaw_tolerance_deg": 3
}
```

These are diagnostic settings, not approved authoring. Overrides are transient, recorded with complete before/after provenance and restored without saving assets. Both Python and native evaluators reject missing, malformed or mismatched entry settings. Preparation defers both montages and paired damage until the live goal is reached. Limits use world simulation seconds even when the roles have different positive actor time dilation; zero actor time dilation is unsupported and aborts preparation.

The scenario checks preparation was observed, neither montage nor paired damage started early, and the final entry outcome is consistent with completion or interruption. The interruption clock begins at reservation, so slow entry can exercise cancellation before montage playback. Such captures legitimately have no paired-montage interval and must be reviewed on their simulation clock. A passing scenario establishes lifecycle behavior; preparation has no authored approach animation and its limits do not constrain later montage root motion. Review both intervals and visible contact separately.

## Cross-check visible contact before tuning

Review raw pixels before interpreting a contact score or changing placement. Inspect the approach, closest observed contact and separation; then compare the projected weapon endpoints and anatomical target with the same captured poses. Check the actual weapon component, body region, timing, camera framing and occlusion. A health change, notify, sphere overlap or a passing scenario cannot establish visible contact.

The finisher scenario provides `default`, `opposite`, `contact_oblique` and `contact_elevated` camera views. The elevated view exposes the distal blade during the finisher's strike poses; the grip can still be occluded:

```powershell
python Tools/CombatCapture/run_scenario.py --map all --variant Completed --mode rendered --camera-view opposite
python Tools/CombatCapture/review_contact.py --capture Saved/CombatCaptures/<capture-directory> --output Saved/Logs/<review-directory>/contact.html --montage AM_Finisher_Attacker. --start 0.25 --end 1.5 --target-point spine_03 --radius-cm 18 --observe-point spine_05 --observe-point neck_01
```

Use identical explicit experiment arguments when comparing diagnostic controls across cameras. Each camera is a separate run, with its own observed montage clock; it is not a simultaneous stereo capture. Named views must be declared in the registered scenario and the returned view must match the request. Other scenarios can declare their own `camera_views` entries with `offset_cm`, `focus_cm` and `fov_deg`.

`review_contact.py` uses the standard library and produces a portable HTML slider with raw pixels beside projected observations, plus a JSON manifest with input/frame hashes. It embeds at most 12 selected frames by default (`--max-frames 2..60`); bulk PNGs can be pruned after required checks without breaking this review subset. Roles, montage interval, segment endpoints, target point/radius and additional probes are arguments, so the same tool supports other paired interactions. The radius is a displayed diagnostic proxy, not a newly accepted threshold.

Publish a separate visual analysis with `review_visual.py --evidence <portable-contact.html> --findings <reviewed-findings.json> --output <visual-analysis.html>`. The [shared visual-analysis library](../../Tools/CombatCapture/visual_analysis/README.md) defines reviewed frame references, observation versus interpretation, uncertainty, reviewer provenance and reusable report generation. The pixel/overlay viewer is evidence; the visual-analysis report records the interpretation. Neither converts a successful capture or geometric test into animation approval.

New frames record camera FOV, a row-major world-to-clip matrix and the constrained view rectangle from the engine's local player after draw. The review rejects missing projection data and mismatched frame/sample pose serials or engine frames. This reconstruction does not include renderer temporal jitter, depth/occlusion masks or a skinned-surface intersection test. Overlay markers can appear through bodies. Older captures need a fresh recording for this tool.

Record visual contact as observed, apparently separated or obscured/indeterminate, with frame and montage time. Record pixel/telemetry agreement separately. If the declared anatomical target does not match the rendered body region, resolve that interpretation before optimizing warp placement or relaxing tolerances. The current finisher's provisional `spine_03` sphere projects around the abdomen in reviewed poses despite its legacy upper-torso label; see the [visual contact review](../audits/FINISHER_VISUAL_CONTACT_2026-09-10.md).

For a bounded automated pixel/telemetry check, the shared library includes `calibrate_segment_alignment.py` and `detect_segment_alignment.py`. They fit visible coloured segments inside reviewed regions and require matching instrument controls. Pillow is an optional dependency for these image-decoding commands. The [segment alignment audit](../audits/VISUAL_SEGMENT_ALIGNMENT_2026-09-11.md) records the first calibration and fresh oblique/elevated results, including abstentions and the separate visual contact finding. This check does not infer blade/body contact from a matching projected line.

## Preview workflow

Open **Window > Paired Animation Preview**. In **Contact evaluation**, enter the profile path, choose **Load pair and criteria**, scrub or play the pair, adjust placement or weapon configuration, and choose **Evaluate and open report**. Reports preserve effective transforms and configured weapon sources so an adjustment is reviewable. Each evaluation creates a separate directory; compare observations at the same pair times before and after the adjustment.

Loading a contact profile selects its meshes, montages, sections, runtime timing convention and weapon attachment. Legacy optimization and analysis controls are disabled while this mode is active, because their broad proximity heuristics do not implement declared contact intent. Editing the profile path leaves this mode; reload the profile to reapply its configuration. Asset/timing changes that disagree with the profile are rejected. Evaluation does not save assets or write adjusted settings back into the profile.

Additional preview grip-socket compensation is unsupported in this mode; keep the grip socket set to None and use the weapon-data attachment transform. Unsupported compensation is rejected during evaluation.

For durable changes, edit the JSON placement/criteria and rerun the command, then verify the gameplay capture against the same criteria. Changing an asset requires a fresh capture; equal asset paths with different bytes are rejected. Do not treat a relaxed tolerance as a fix.

## Profile format

Schema 1 requires a paired asset, both skeletal meshes, initial mesh positions in centimetres, mesh rotations as **pitch, yaw, roll** degrees, a sample rate (10–240 Hz), a written `criteria_basis`, and 1–32 named contacts. Optional `attacker_weapon_data` and `victim_weapon_data` reuse the equipped mesh, attachment socket/transform and trace endpoints. Other points are explicit skeletal bones/sockets; no nearest-bone fallback is used.

Each contact supplies:

| Field | Meaning |
| --- | --- |
| `source`, optional `source_end` | `Attacker:point` or `Victim:point`; one point or a line segment |
| `target`, `target_radius_cm` | Named target origin and explicit spherical region radius |
| `start_s`, `end_s` | Intended interval on the paired clock |
| optional `sync_name`, `sync_role` | Offset the interval from exactly one matching notify on the named role |
| `minimum_gap_cm`, `maximum_gap_cm` | Allowed signed distance from source point/segment to target-region boundary; negative means proxy overlap |
| `maximum_sample_gap_s` | Maximum allowed spacing of observations bracketing the interval |
| `sustained` | True requires all observed poses to satisfy intent; false requires at least one contact inside the interval |
| optional `measure_orientation` | Enables source axis versus target normal measurement |
| `target_local_normal`, `expected_angle_deg`, `angle_tolerance_deg` | Required when orientation is measured; a point uses its local X axis, a segment uses start-to-end direction |

Optional `alignment_budgets` declare named intervals with `maximum_translation_cm`, `maximum_rotation_deg`, `maximum_victim_timing_error_s` and `maximum_sample_gap_s`. They compare victim-root relative to attacker-root in authored versus gameplay poses at the **original attacker montage clock**. Translation/rotation express the geometric correction needed to match authored relative placement. These are not measurements of actual motion-warp correction: blending, initial placement and locomotion can also contribute. Victim timing error is measured independently and is never fitted away. Entry, contact and exit can have separate budgets.

## Timing, provenance and limits

Sync-state export follows Unreal's forward interval-overlap rule. A state already active when fresh playback enters the selected section is included at effective `pair_time_s=0`; its original `nominal_montage_time_s`, `montage_time_s` (actual trigger), `trigger_offset_s` and end trigger remain visible. For example, the finisher's opening `FinisherImpact` retains its -0.0001 s trigger while anchoring to entry. A state that ended at or before entry is excluded. This also handles a section jump into the middle of a state, without claiming continuity from an earlier playback instance.

`is_primary`, `damage_configured` and `requests_damage_on_owner` distinguish configuration from the request passed to the owning paired component. The compatibility field `applies_damage` remains a configured flag. Successful runtime damage ownership/commit is **not measured** by these fields. The exporter reads montage-level sync states; source-segment notifies, branching traversal and a complete effective runtime damage timeline remain unsupported. The HTML report presents these distinctions alongside the original clocks.

The shared preview sampler uses absolute montage positions, so backward scrubbing does not accumulate extracted root motion. Section start plus elapsed time times montage `RateScale` defines the authored clock. The victim follows current runtime precedence: a negative start offset advances its start position; a named section jump overrides that position. A positive offset does not currently delay its world start. The data asset's legacy `SyncPointTime` is not a runtime authority.

Only a single montage slot and one continuous, non-looping shared section interval up to 30 seconds are supported. Runtime montage instance play rate must be 1; asset `RateScale` is accounted for. Section transitions, loops, reverse playback, multiple paired instances in one capture and full AnimGraph reproduction require additional support. The sampler evaluates the authored montage without gameplay notifies, with its extracted root delta applied once to the initial mesh transform.

The native report includes profile/editor-binary SHA-1, project package SHA-1 identities, original capture pose-ledger identity, effective preview settings, criteria and observations. The scenario runner retains SHA-256 identities and adds SHA-1 asset hashes for compatibility with Unreal's native hash implementation. Relevant missing soft dependencies are recorded as absent. Unsaved evaluated assets are rejected; preview transform overrides are serialized separately. These hashes detect changes in local evidence; they are not authenticity signatures.

The finisher runner supports bounded transient controls through `--finisher-experiment`:

| Control | Effective changes |
|---|---|
| `none` (default) | Original authored notifies |
| `permit-root-motion` | Paired-collision notify `bDisableMovement=false` on both montages |
| `attacker-source-translation` | Movement permitted on both; attacker warp notify's `RootMotionModifier.bWarpTranslation=false` |
| `victim-source-translation` | Movement permitted on both; victim warp notify's `RootMotionModifier.bWarpTranslation=false` |
| `victim-source-rotation` | Movement permitted on both; victim warp notify's `RootMotionModifier.bWarpRotation=false` |
| `paired-warp-tuning` | Movement permitted on both; attacker translation warping disabled to match pair configuration; requested victim warp window, horizontal relative offset and optional facing policy |

The controls substitute transient copies of notify states and their instanced modifiers, preserving other properties, montage clocks, input ownership and original objects. Translation controls retain rotation warping; the rotation control retains translation warping. They require exactly one matching warp notify for the selected role. Original notify objects and flags are checked on restoration; no packages are saved. Capture metadata, scenario results and runner context identify the experiment; exact notify/property changes are retained in `runtime_asset_overrides`. Both evaluators reject missing, duplicate, wrong-role or unsupported overrides. Native runtime evaluation reports the control explicitly while using unchanged disk assets for authored playback. Experimental captures cannot reuse an unmodified scenario reference as if their effective configuration were identical. These are diagnostic controls, not approved animation corrections.

For a bounded placement/window comparison:

```powershell
python Tools/CombatCapture/run_scenario.py --map ThirdPerson --variant Completed --mode motion --render-world --finisher-experiment paired-warp-tuning --victim-warp-window 0.20 0.43 --victim-warp-offset 50 0 0
```

The window uses effective montage trigger seconds, including the authored trigger offsets. Its duration must be at least 0.02 seconds, lie within 0–5 seconds and fit the loaded victim montage. Horizontal offsets are bounded to +/-200 cm; vertical offset must be zero. Both arguments are required and are rejected for other experiments. The fixture records and restores the complete notify events and original victim warp configuration. The request, scenario and capture must agree on all five changes. Victim translation/rotation and attacker rotation stay enabled.

Optionally add `--victim-facing-policy face-partner`, `face-away-from-partner`, or `match-partner-heading`. Omission preserves the asset's current policy. An explicit policy requires a sixth override recording `VictimWarpConfig.FacingPolicy` before/after; both evaluators reject missing, inconsistent or unrequested policy changes. The runtime setting is transient and restored with the rest of the role config. Authored evaluation still reads unchanged disk assets, so this control is not saved-authoring parity. Initial source-pair spacing and the moving runtime warp endpoint are separate quantities; do not copy a source placement into `--victim-warp-offset` without evaluating the resulting motion.

For primary-sync diagnostics, add `--primary-sync-time 0.6` and/or `--sync-nudge disabled` (or `enabled`). These independently set the primary damage notify's effective start time and position-nudge flag on both participants. Omitted fields retain their original values. The 0.6-second value is an example, not an approved impact time. Timing must be finite within 0..5 seconds and the retained notify duration must fit each montage. Exactly one primary damage notify per role is required; missing or ambiguous notifies fail setup. Two `PrimarySyncSettings` override rows record each role's effective start, end and nudge before/after. Native and Python validation require matching provenance, preserved durations and unchanged omitted fields. The fixture uses transient notify copies and restores original events and objects without saving assets.

The finisher scenario records its request-time victim health separately from sampled health. Optional timing experiments also record the first observed lethal montage position and interruption position, check for premature lethal state, and verify unchanged health when interruption precedes the requested sync. These are automation observations, not exact callback timestamps. Scenario version 5 adds the victim's `head` capture point for anatomical observations; contact criteria remain unchanged. Moving a sync event does not establish visible blade contact or solve entry alignment. Review rendered evidence alongside the geometric report before selecting authoring changes.

The [entry/sync comparison](../audits/FINISHER_ENTRY_SYNC_2026-09-11.md) records the three-control experiment, notify-order ownership defect, remaining abrupt motion, pre-sync cancellation, visual/contact limits and image retention.

Use `--mode motion --render-world` for numerical screening without PNG export. Use `--mode rendered` for selected frame reviews and image eligibility checks. A recorded movement lease is not proof of collision isolation: the finisher scenario also checks reciprocal movement-ignore relationships while both collision windows are active. Actor displacement includes collision corrections as well as root motion; native `-LogCmds="LogMovement Verbose"` logging identifies depenetration when those measurements disagree.

Capture samples include modifier state, translation/rotation flags, cached modifier target and current component target position/quaternion. These are observations at world-tick end. They help relate actor steps to target changes but do not measure the root-motion delta actually processed by CharacterMovement, an instantaneous pre-update target, or collision response.

Authored samples are interpolated only to obtain a bracketing reference for relative-root alignment; capture samples retain their original clocks. Contact tests use observed poses without claiming continuous collision detection between samples. Each runtime observation links its nearest captured frame and states the time offset; a nearby image does not establish exact-time contact or visibility. Orientation is null/not measured when no orientation criterion is declared.

Segment/sphere overlap is a declared geometry proxy. Skin penetration, mesh surface contact, foot support/sliding, input-to-visible-response latency, artistic readability, audio/haptics and actual motion-warp work remain unmeasured. Passing these checks does not establish overall animation quality or feel.

## Choosing contact sampling resolution

Set `maximum_sample_gap_s` from the shortest event the criterion must observe, then check actual gaps across its bracketing interval. The existing 75 ms diagnostic allowance is not a validated brief-strike requirement. A known 50 ms contact can fit entirely between 70 ms samples while satisfying that allowance. A sampled `fail` means the observed poses did not satisfy contact; it does not establish continuous absence. Sustained-contact passes likewise apply to observed poses.

`BriefContactSamplingPhases` exercises a controlled 50 ms segment/sphere contact at 60 different sample phases. At 60 Hz, every phase detects it under a 25 ms maximum-gap requirement; 70 ms sampling is inconclusive under that same requirement. The control also demonstrates the coarse-sampling miss under 75 ms eligibility and rejects contact outside the intended interval. These are instrument controls with known geometry and motion, not production contact-duration assumptions.

For a contact guaranteed to last at least a declared duration, a strictly smaller maximum observation gap with interval coverage is needed to ensure at least one observation, with margin for clock and pose uncertainty. A particular recording is not eligible merely because its configured rate is 60 Hz. Choose a tighter requirement when authoring intent needs it; do not widen a requirement to make a recording pass. Without a minimum duration or an independently validated motion/sweep bound, dense sampling still cannot prove continuous absence between poses.

## Verification

`KatanaCombat.Editor.PairedEvaluation.*` covers explicit geometry, orientation, timing, sparse/stale evidence, relative alignment budgets, real-montage absolute scrubbing, identical preview/report measurements and a constructed real-pair contact followed by displacement and restoration. `KatanaCombat.Capture.NominatedWeaponPointSource` covers ownership, source identity and destruction. The same evaluator is exercised on the tracked counter without changing the finisher contact tolerance.

See the [capture guide](COMBAT_CAPTURE_AND_ANALYSIS.md) for reusable recording and the [implementation plan](../plans/COMBAT_CAPTURE_AND_ANALYSIS.md) for remaining measurement boundaries.
