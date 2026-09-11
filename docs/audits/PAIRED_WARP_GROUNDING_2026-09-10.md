# Paired warp grounding and upright facing

Date: September 10, 2026 (local); capture directories use September 11 UTC timestamps.

The paired ground query was accepting character collision as ground. Both maps reproduce a player skeletal-mesh hit that raises the victim target by about 155 cm. Paired targets now select walkable environment support and use yaw-only facing and relative offsets. Fresh rendered movement-permitted captures retain translation and rotation warping, hold victim target height constant and keep actor pitch at zero. The late victim displacement drops below 1 cm. A separate early translation spike remains.

## Cause and correction

The original helper calls `LineTraceSingleByChannel` using `ECC_WorldStatic`. That selects collision responses to a channel; it does not restrict hits to static environment objects. It ignores only the querying owner and accepts a blocking hit without requiring walkability. The before-fix logs identify character capsules and the player's `CharacterMesh0` among accepted hits. On ThirdPerson, the victim query at `(58.492, -6.421, 300.150)` hits player mesh Z `365.376`. Adding the 88 cm victim capsule half-height gives a `453.376` cm target instead of floor target Z `298`: a **155.376 cm rise**. That mesh hit passes the slope check, so slope filtering alone cannot fix it.

`UDebugUtils::SampleWalkableGroundAtLocation` explicitly queries static/dynamic environment object types, rejects pawns and their owned/attached objects, and requires character-specific walkability, step-up permission and a blocking response to the capsule channel. Rejected components are ignored on a bounded retry so they cannot hide support below. Both paired setup and continuous pre-update use this helper. Missing support preserves the requested position. This does not prove capsule clearance, reachability, foot contact or support on arbitrary gravity axes.

Paired offset rotation and facing now use yaw only. Ground elevation no longer supplies pitch, and coincident XY retains the owner's heading. This prevents pitch-driven world translation from the offset skeletal mesh while retaining rotation warping. The older general ground helper retains its behavior; unrelated attack alignment and ground-snap callers are outside this correction.

The disabled-by-default `Combat.Debug.GroundSampling` CVar records query/hit identity and eligibility without drawing. After-fix paired queries accept only each map's level floor: 277 accepted hits on ThirdPerson and 279 on DefenseMatrix in the diagnostic replays. General legacy ground-query logs are also present; they must not be counted as paired-query eligibility results.

## Rendered results and remaining translation

| Movement-permitted replay | Late victim step, ThirdPerson / DefenseMatrix | Maximum pitch magnitude | Target-height range, ThirdPerson / DefenseMatrix |
|---|---:|---:|---:|
| Before correction, ground logging | 45.80 / 27.34 cm | 55.04 / 55.38 degrees | 156.26 / 156.21 cm |
| Corrected, ground logging | **0.85 / 0.81 cm** | **0 / 0 degrees** | **0 / 0 cm** |
| Corrected, ordinary repeat | **0.81 / 0.90 cm** | **0 / 0 degrees** | **0 / 0 cm** |

Late step uses consecutive actor positions whose ending victim montage time is inside 0.84-0.90 s. Original timestamps and sample intervals are retained. These are observed runs, not a statistical effect estimate; capture phase changes the peaks. The broader 0.80-0.96 s interval still reaches 2.14 / 3.96 cm in the corrected diagnostic replays. The opening alignment snap is excluded. See [before/after plots](../../Saved/Logs/PairedWarpGrounding-20260910-214854/grounding-comparison.png).

The early step is a separate translation-warp effect. All following controls use the corrected runtime, retain rotation warping and permit movement:

| Control | Maximum victim step ending inside 0.05-0.30 s, ThirdPerson / DefenseMatrix |
|---|---:|
| Both original translation modifiers | 15.45 / 16.68 cm |
| Attacker translation warp disabled | 17.53 / 17.77 cm |
| Victim translation warp disabled | **2.68 / 2.68 cm** |

Two additional native logging replays prove local translation amplification: ThirdPerson changes a **2.683 cm input into 19.026 cm processed local translation**; DefenseMatrix changes **2.467 cm into 16.101 cm**. Converted world-translation predictions agree with those processed magnitudes. These are modifier outputs before final swept CharacterMovement, distinct from the sampled actor steps above. In a corrected ground replay, the victim moves 14.76 cm while its cached target moves only 0.10 cm; target height remains constant and actor pitch remains zero.

The preserved authored-pose samples show a roughly 2.68 cm victim root step at 0.117-0.133 s. The root first moves away and then returns near its starting position by the warp-window end. Unreal's `URootMotionModifier_SkewWarp::WarpTranslation` scales/skews each delta against remaining root translation and target distance. That path/window relationship is a supported explanation to investigate next; no engine-algorithm change or final replacement window is established here. The source-translation control also retains a later 13.77 cm actor step near 1.25 s, outside the active warp window, so reducing the early peak does not approve all motion.

All six native contact comparisons are measured, with no inconclusive runtime result. Native commandlets exit 0; the evaluator correctly exits 1 because criteria fail. The corrected movement-permitted blade minimum signed gap is **44.84 / 44.84 cm** against the unchanged 8 cm allowance. Attacker translation-off remains 44.78 / 44.91 cm; victim translation-off becomes 48.18 / 48.18 cm. Runtime entry-root separation passes, while release-hand separation fails in all six comparisons (205.53-212.61 cm minimum gap against 200 cm maximum). Criteria remain provisional and unreviewed. A mechanically smoother control is not automatically a better contact solution.

## Verification

The editor build passes. All four new `KatanaCombat.Targeting.PairedGrounding.*` tests fail on the old behavior and pass after correction. They use public targeting and motion-warp APIs for setup and refresh, including steep ground, moving support, character-owned geometry, missing ground, upright offsets and coincident heading. No test friends were added.

Full headless automation passes **773/773**, with no failed/error results and native exit 0. The log records 481 automation warnings. Fresh post-fix rendered verification passes **16/16**: four movement-permitted completed/interrupted cases, four disk-authored controls, four completed role-specific translation controls, two ground-logging replays and two native root-motion logging replays. All **2,032 PNGs** finish with zero pending, rejected or failed images. Two before-fix diagnostic runs are separate from that count.

Scenario assertions verify input suppression, bystander activity, interruption/completion cleanup, movement restoration and fresh input after recovery. Experimental active modifiers retain their requested flags, and attacker acceleration remains zero while paired input is suppressed. The disk-authored victim has no active modifier observation while movement is disabled; that control does not establish active warp behavior. No capture/tool source changed, so the offline tooling suite was not rerun.

## Evidence and reproduction

Evidence root: `Saved/Logs/PairedWarpGrounding-20260910-214854/`.

- `source-before.json`, `source-after.json`, `task-source.diff`, `scope-verification.json`: isolated source change and protected Content identity.
- `red-tests.log`, `green-tests.log`, `green-build-final.log`, `full-tests.log`, `full-tests-summary.json`: regression and build evidence.
- `before-ground-trace.log`, `after-ground-trace.log`, `ground-hit-results.json`: native hit actor/component evidence.
- `verification.json`, `capture-verification.json`, `contact-results.json`, `native-root-motion-trace.json`, `source-root-early-steps.json`: aggregate checks, contact artifacts and early translation evidence.
- `movement-results.json`, `warp-target-results.json`, `rotation-diagnosis.json`: original clocks, actor transforms and warp target observations.
- `review-frames.json`, `grounding-comparison.png`: reviewed frame paths/offsets and before/after plots. Near 0.75 s, the before frame shows the victim tilted with lifted legs; the corrected frame is closer to an upright kneeling pose. This is not artistic acceptance.

After building the editor, run the focused regression with `Automation RunTests KatanaCombat.Targeting.PairedGrounding;Quit`. The standard runner reproduces the rendered movement-permitted cases:

```powershell
python Tools/CombatCapture/run_scenario.py --map all --variant all --mode rendered --skip-build --finisher-experiment permit-root-motion
```

Use `--finisher-experiment none` for disk-authored behavior, or the existing `attacker-source-translation` / `victim-source-translation` controls to retain source translation for one role. Each experimental capture records exact transient overrides and verifies restoration. Exact native logging launch arguments are in each run's `command.json`; the Saved helper scripts are retained with the evidence.

## Scope and limits

Changed runtime files: `TargetingComponent.cpp`, `DebugUtils.cpp`, `DebugUtils.h`. Added `PairedWarpGroundingTests.cpp`; updated the paired specification, capture plan, execution handoff and capture-retention guide. No capture-tool source changed. All 54 protected Content paths are unchanged, including existing modifications, deletions and untracked assets. No assets were resaved or committed, and no criteria or reference was promoted.

The existing montage collision notifies still disable movement. Root-motion evidence therefore uses explicit transient movement-permitting overrides. The actual attacker warp modifier still enables translation despite the pair config disabling it. These asset/behavior decisions, opening alignment and damage timing remain separate from the ground and upright-target correction.

## Next concrete step

Run a bounded transient authoring comparison of victim translation-warp timing and target placement against the source root trajectory, while reconciling the attacker's pair-config/modifier translation mismatch. Require both the early-step and contact reports, retain input/cleanup checks, and keep the same provisional criteria visible. Select a concrete movement/warp configuration before any saved asset change; then review contact intent and damage timing. Disabling victim translation alone improves the early step but worsens measured contact, so it is not the final correction.

## Post-verification cleanup

At the user's request, bulk generated PNGs were pruned from 122 completed capture bundles across this workstream after checks finished. **10,697 PNGs (6.09 GB) are no longer in the workspace; 11 explicitly reviewed images remain at their original paths.** They were moved to the Windows Recycle Bin, so disk capacity is not reclaimed until the bin is emptied. The automatic approval review rejected the original permanent-deletion command; reversible recycling succeeded. An incomplete manual-capture bundle was excluded.

Telemetry, measurements, logs, source snapshots and original capture records remain. Each affected bundle now has `retention.json` / `RETENTION.md`, and generated HTML reports carry a retention notice. Earlier exported-image counts describe verification before cleanup, not current image availability. Full image eligibility or an unretained interval needs restored frames or a new capture. `cleanup-plan.json` and `cleanup-result.json` record the inventory and outcome. Twelve temporary Python scripts were consolidated into this evidence root, with hashes verified before their redundant Saved-root copies were recycled. The [capture guide](../guides/COMBAT_CAPTURE_AND_ANALYSIS.md) now includes this cleanup practice.
