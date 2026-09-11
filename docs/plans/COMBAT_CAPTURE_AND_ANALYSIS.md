# Reusable combat capture and analysis

## Scope and acceptance

Capture is project tooling shared by manual PIE sessions and automation. Scenarios select a world, participants, labels, and actions; the recorder does not know any proof gate or manipulate combat. Keep this in the existing editor module and repository. Plugin packaging and a learned feel score are deferred.

1. Add `Source/KatanaCombatEditor/{Public,Private}/Analysis/CombatCaptureSession.*`: bounded sessions, explicit PIE-world ownership, sampled actor/bone/montage state, rendered frames after the matching viewport draw, event markers, and incremental export of existing action/reaction and defense telemetry. Record clocks, units, settings, actual sampling gaps, image integrity, errors, and missing data. Preserve recordings in unique directories.
2. Add `CombatCaptureCommands.cpp` and register lifecycle cleanup in the editor module. `Combat.Capture.Start`, `Mark`, and `Stop` make capture available during ordinary PIE on any map, without a test harness. Capture ends on world teardown or configured limits and restores telemetry settings.
3. Add `Tools/CombatCapture/analyze_capture.py`: portable offline JSON and HTML reports, linked frame timeline, sampled motion and point-distance measurements, correlated input-to-action timings, explicit data-quality findings, and compatible baseline comparisons. No new Python packages or network service.
4. Add `CombatCaptureTests.cpp` and analyzer regression tests. Exercise the same session format through the console and C++ API on two existing maps; test lifecycle, limits, missing/invalid evidence, and known numerical analysis results. Build the editor, run rendered and headless capture suites, inspect exported reports and representative frames, then run the existing regression suite.
5. Document commands, extension examples, schema, metric definitions, and limitations in `docs/guides/COMBAT_CAPTURE_AND_ANALYSIS.md` and link from the test README.

Acceptance: a contributor can record and analyze a new PIE interaction without editing a proof test. Both manual and automated sessions produce the same versioned bundle. Analysis works across scenarios and cannot present absent rendering, missing bones, sparse sampling, or uncorrelated telemetry as a quality pass. Existing assets and gameplay remain unchanged.

## Measurement boundaries

- Simulation time drives motion/timing measurements; wall time exposes capture overhead and stalls. PNG readback is synchronous and unsuitable for performance benchmarking.
- Sampled world/component bone positions support displacement and distance measurements. Distance between configured points is not collision/contact correctness; foot motion alone does not establish foot sliding without a support annotation.
- Images support visual review and future vision analysis. Pixel variation detects empty/uniform captures only. Projected centers are not occlusion or whole-body visibility tests.
- Comparisons require matching map, scenario, participants, sampling configuration, and analysis options. They show metric changes, not a universal animation-quality score.

## Status

Current next action (2026-09-10): [paired warp tuning and collision ownership](../audits/PAIRED_WARP_TUNING_2026-09-10.md). Opening lethal damage cleared the victim partner before its collision notify. Expected paired death now preserves that link until exact sequence cleanup; unrelated death still cancels. Build, 776/776 full automation tests, 35 offline tests, four rendered completed/interrupted cases and two native movement-logging replays pass. The four transient settings reduce early steps to 2.7–6.2 cm after the fix; no victim–attacker depenetration appears in the final native logs. Eight native contact comparisons still fail blade/release criteria. Review source-pair facing/placement and intended blade contact before saved asset or damage-timing changes. All 54 protected Content paths remain unchanged. Cleanup retained four new review frames and recycled 451 PNGs; telemetry and original export results remain available.

Earlier disposition (2026-09-10): [finisher warp isolation](../audits/FINISHER_WARP_ISOLATION_2026-09-10.md) identifies a substantial rotation contribution. Victim rotation-off keeps actor pitch at zero and reduces the late-window actor step below 1 cm on both maps; translation-off alone does not remove it. Ground-adjusted targets rise approximately 156 cm and request pitch near -72 degrees. Record the actual ground-query hit, correct ground eligibility and upright paired-target rotation, then inspect the remaining early approximately 15 cm translation step before contact/timing authoring. Final editor build, 9 paired tests, 31 offline tests, 16 rendered matrix cases and 2 logging replays pass; all eight completed contact comparisons still fail unchanged provisional blade/release criteria. Runtime source and Content remain unchanged; no correction or quality reference is promoted. The [preceding movement experiment](../audits/FINISHER_MOVEMENT_EXPERIMENT_2026-09-10.md) retains opening-sync repair and movement-ownership evidence.

The first reusable layer is implemented and verified. See the [usage guide](../guides/COMBAT_CAPTURE_AND_ANALYSIS.md) and [verification report](../audits/COMBAT_CAPTURE_AND_ANALYSIS_2026-09-09.md). This supersedes the narrower capture-only scope in the [earlier hardening report](../audits/COMBAT_CAPTURE_HARDENING_2026-09-09.md).

## Scenario evaluation contract

These requirements now have a first implementation through completed/interrupted real-montage finisher recovery with active bystanders on both maps. The [scenario verification report](../audits/COMBAT_SCENARIO_EVALUATION_2026-09-09.md) records the exact acceptance evidence. Requirements for additional contact, foot-support and perceptual validators remain extension contracts; passing the initial displacement criterion does not establish those capabilities.

### Measurement eligibility and results

Keep three outcomes separate: recording integrity, suitability of the evidence for a particular measurement, and the scenario/quality assertion result. Each evaluator must return `pass`, `fail`, `inconclusive`, or `not_run`, with its reason and evidence interval. A complete JSON/PNG bundle can still be unsuitable for a pose or visual assertion.

Define required participants/points, pose freshness, sample/frame coverage, maximum timing uncertainty, rendering readiness, and allowed headless operation per evaluator. Missing or stale observations, an uncovered transition, or an early capture limit must not satisfy the associated assertion. Warnings can remain useful for exploratory captures; assertion execution requires an explicit eligibility decision. `WasRecentlyRendered` is a hint, not proof of fresh bone evaluation or visibility.

`analyze_capture.py` retains descriptive observations and exploratory `compare()` deltas. Assertion execution now uses `evaluate_capture.py`, which independently establishes pose/frame eligibility and returns a per-case outcome. A descriptive report's `data_integrity=valid` is never an assertion result.

### Timing and pose evidence

Correlate input capture, action acceptance/start, first evaluated animation response, intended contact/damage, and control restoration as separate events. Do not label the existing input-to-action metric as input-to-visible-response latency. Record relevant interaction/attack generations and use explicit event windows with pre/post samples.

Strengthen the association between evaluated poses and rendered frames with frame/evaluation identity or a measured, bounded association error. Keep original timestamps when comparing event-aligned windows; alignment must not conceal a late response or altered duration. Hitstop, time dilation, pause and interruption need explicit clock semantics.

Schema 2 records bone rotations, contributing montage instances/weights and root-motion/ownership context alongside the existing alignment telemetry. A generic displacement peak alone cannot distinguish authored motion from a pose defect; the initial selected reference is a bounded regression envelope. Contact validators still need intended point/region, timing and acceptable exceptions; foot-slip validation additionally needs a defensible support interval.

### Repeatable scenarios and references

Track reusable scenario definitions with stable logical roles, starting transforms/state, actions and their timing, duration/end conditions, randomness controls, camera/resolution, and capture/evaluation settings. Specify how actor destruction, replacement and new participants are handled; console discovery currently enrolls only the initial character set. Keep orchestration and evaluators separate from the recorder.

Record source revision plus dirty-diff identity, relevant asset/config identities, scenario version, and evaluator/criteria version in the bundle. Compare expected invariants and explicitly declared candidate changes. A candidate asset differing from the reference is normal in a regression experiment; blindly requiring every content hash to match would defeat that experiment.

Run unchanged scenarios repeatedly to establish natural variability before choosing tolerances. Use reviewed acceptable examples and controlled defects to assess detection and false alarms. Document the reference's acceptance basis and replacement rationale; avoid silently promoting the latest recording. Preserve small durable reference fixtures/metadata separately from disposable `Saved/` output.

### Capture overhead and robustness

Compare the same short scenario with capture disabled, telemetry/motion only, and rendered capture. Measure wall-clock overhead and check gameplay outcomes/timing for capture-induced changes. Higher requested rates alone do not establish useful cadence. Use bounded dense bursts around transitions; change the readback implementation only if measured overhead requires it. Any diagnostic mode that forces pose evaluation must record and restore that change and be distinguishable from ordinary gameplay observation.

Focused lifecycle coverage now exercises no world ticks, time/data limits, participant destruction and nominated-mesh replacement, telemetry reset/overflow, final manifest write failure and rendered frame limits. The independent core ticker replaces the original world-tick-only timeout. Process interruption leaves an initial recording manifest or in-progress report, never a completed scenario result; the external runner also has a process deadline. Storage and metadata are bounded as well as in-memory records. These guarantees do not claim recovery of telemetry lost in an abrupt process crash.

### Reusable execution and reporting

Provide one repository-documented command that selects a scenario, runs the required capture mode, invokes analysis/evaluators, and returns a useful exit status and artifact path. Exercise it outside the original implementation session. Keep schema compatibility explicit and test representative stored bundles as the format evolves.

Tie generated analysis to its input bundle and evaluator identity so an interrupted or rejected rerun cannot leave an older successful report looking current. Present failures and inconclusive results with participant, event interval, threshold/reference basis, coverage and linked frames. Preserve the distinction between a mechanical defect assertion and an artistic/readability judgment requiring review.

## Acceptance for the implemented slice

Implementation sequence (2026-09-09): extend `CombatCaptureSession.*` with schema-2 pose/frame identity, animation context and bounded watchdog/storage behavior; add `CombatCaptureScenarioTests.cpp` plus tracked `Tools/CombatCapture/scenarios/*.json` for completed/interrupted finisher recovery on both maps; add `evaluate_capture.py` and `run_scenario.py` for eligibility, per-case results, immutable run identity and one-command execution; extend the recorder/analyzer tests with missing/stale evidence, lifecycle and export-failure controls. Run repeated unchanged scenarios and capture-disabled/telemetry/rendered variants, calibrate a limited transition envelope, and verify a controlled defect and its removal. Keep new gameplay assertions separate from visual eligibility. Do not mark any pending acceptance complete without its recorded result.

Complete the evidence/eligibility and scenario work together, then establish a small set of calibrated assertions:

1. A contributor can run the same tracked scenario through a documented command on the two project maps, with declared differences and complete run identity.
2. Existing input-during-finisher, release/repress-after-recovery and active-bystander questions have explicit scenario assertions and recorded outcomes.
3. The selected transition/contact measurements pass eligibility on usable recordings and become inconclusive on deliberately missing, stale or temporally insufficient evidence.
4. Repeated unchanged recordings establish variability; acceptable controls and deliberately defective controls demonstrate what the selected criteria detect. A real fix is verified against the same scenario and linked evidence.
5. Capture overhead is measured, relevant failure paths are covered, and reports cannot confuse incomplete or stale evidence with an evaluated pass.

After this slice, continue gameplay work and extend the shared tools when a concrete scenario needs it. A plugin split, broad learned perception platform, audio/haptic capture, and exhaustive visual scoring remain outside this slice. Record those modalities as unmeasured when discussing overall feel.

Disposition (2026-09-09): the bounded finisher-recovery/displacement slice is complete. The editor builds; 749/749 headless tests, 26/26 offline tests, 12/12 repeated rendered scenarios, 9/9 overhead controls and 2/2 rendered recorder integrations pass. Four deliberately displaced scenarios fail only the calibrated displacement assertion; four fresh runs without the displacement pass the same references. See the verification report for exact artifacts, protected Content hashes and remaining unmeasured capabilities. The original reusable console/API capture path remains available for new scenarios.

## Hold recovery and paired authoring evaluation (2026-09-10)

Execution order: verify hold/recovery with real montages on both existing maps; resolve failures affecting paired entry, playback or recovery; implement one finisher evaluation; exercise the same evaluator on a counter or chain stage. Record unrelated gameplay failures separately. This ordering closes existing verification debt before the next development slice; it is not a technical dependency between the tools.

### Hold/recovery verification

Add a purpose-named PIE scenario using public input and observation APIs in `Source/KatanaCombatTest/Private/`, a tracked scenario definition under `Tools/CombatCapture/scenarios/`, and registration in the shared runner. Observe real hold-window notifies, movement suppression while holding, release-directed follow-up ownership despite competing input, terminal cleanup, movement recovery and a fresh attack after release/repress. Preserve actual montage/data settings and report unsupported authored follow-ups explicitly. Build and run on both maps; retain telemetry, assertions and rendered evidence. Never substitute seeded protected state for the real-montage result.

### Paired authoring evaluation

Extend the existing `PairedAnimationAnalysisLibrary`, `PairedAnimationAnalysisSubsystem` and paired preview, with focused tests in `KatanaCombatTest`. Pure measurements belong in the library; asset/pose acquisition belongs in the subsystem; preview and automated callers consume the same evaluation. Existing broad proximity heuristics remain exploratory until reconciled with explicit intent.

- Declare intended contacts by role, bone/socket or configured geometry, interval, position/orientation tolerance and permitted separation/overlap. Support multiple contacts, including sustained grabs and strikes. Reuse existing authored fields where they are authoritative. Runtime timing comes from montage sync notifies; legacy asset `SyncPointTime` is not a runtime timing authority.
- Validate required meshes, skeleton/bone/socket availability, montage sections and timing mappings before measuring. Missing data must be inconclusive, never a zero-distance pass. Account for sections, offsets, play rates and root motion; retain original times alongside event-aligned comparisons.
- Evaluate entry, contact and exit for both roles. Compare authored playback and gameplay observations with the same contact definitions. Report residual error separately from measured translation/rotation correction; enforce declared correction limits only when the required evidence exists.
- Show each result with units, timing uncertainty, criteria basis and linked frames. Point/region proxies do not establish skin penetration, foot support or artistic quality. Those remain separate, explicitly unmeasured criteria unless implemented and validated.
- Test mistiming, misalignment and misleading proximity: unrelated bones being close must not satisfy the intended weapon contact. Include acceptable controls, missing/stale evidence and defect removal. Reserve an additional pair for validation after choosing tolerances, and demonstrate reuse on a counter or chain stage without duplicating evaluator logic.
- Complete the contributor workflow: select an existing pair, supply/reuse its evaluation definition, evaluate, inspect the failing interval, adjust settings and compare. Keep asset changes explicit and preserve user WIP. Preview and automated reports must agree numerically for identical inputs. Report any unsupported runtime/preview mapping rather than implying parity.

Completion requires one real finisher evaluated end to end, the same evaluator exercised on another paired interaction, controlled-defect evidence, a documented repeatable command and an actionable preview/report workflow. Candidate discovery/ranking, plugin extraction and broad perception scoring remain subsequent work. No new acceptance result is implied by this plan.

Disposition (2026-09-10): the bounded contact/alignment instrument is implemented in the existing editor module. The full headless regression passes 765/765; focused tests prove exact-geometry controls, missing/stale rejection, real-pair backward scrubbing, preview/report agreement and displacement/restoration. Completed-finisher gameplay captures pass on both maps, and the same evaluator processes finisher and counter profiles. The diagnostic finisher contact fails in both authored and gameplay observations; criteria remain provisional and are not promoted to quality references. Actual motion-warp correction, full AnimGraph parity and artistic/foot-support criteria remain unmeasured. Desktop automation was unavailable, so the live Slate layout was not visually verified. See the [dated evidence and remaining limits](../audits/PAIRED_ANIMATION_EVALUATION_2026-09-10.md) and [repeatable authoring workflow](../guides/PAIRED_ANIMATION_EVALUATION.md). Continue with review of the measured finisher contact/placement interval, rather than expanding to plugin packaging or learned feel scoring.

## Sampling reliability follow-up (2026-09-10)

Before using contact reports to drive authoring changes, diagnose the five inconclusive hold captures. The original pose-gap bound remains 75 ms. Compare disabled, pose-only and PNG modes with rendering enabled throughout, using repeated ThirdPerson Forward holds. Correlate the largest gaps with engine-frame deltas, scenario events and preceding image captures. Add bounded timing diagnostics if needed to distinguish GPU readback from image encoding/writing and pose/telemetry work. Fix the demonstrated bottleneck, then repeat the affected matrix on both maps and instrument failure controls. Preserve original pose/frame timestamps and explicit handling of incomplete work; do not fabricate intermediate poses or automatically widen criteria.

Determine what cadence can establish for brief contact separately from capture reliability. Validate a known brief contact at different sample phases and gaps, documenting the shortest supported observation requirement. A sampled gap/proximity result cannot prove continuous absence of contact between samples without an additional motion bound or sweep model. Asset authoring changes remain outside this reliability slice.

Disposition (2026-09-10): complete. Timing identified synchronous PNG compression/writing as the repeated capture stall. Bounded background export restores 339–341 poses in repeated rendered Forward holds versus 148–165 before, preserves original observation clocks, and exports all 1,095 PNGs across thirteen rendered hold runs without failure or rejection. The affected hold matrix passes 8/8 under unchanged criteria; repeated mode comparisons pass 9/9; two additional DefenseMatrix holds and both fresh completed-finisher captures pass. Focused verification passes 28/28 headless, 10/10 rendered and 28/28 offline tests. Brief-contact phase controls establish the limits of sampled results; fresh finisher contact intervals now have approximately 16.8 ms maximum gaps but still fail the provisional blade/torso proxy. See [sampling evidence and next action](../audits/COMBAT_CAPTURE_SAMPLING_2026-09-10.md). Continue with finisher contact-intent and relative-placement review; no further capture expansion is needed before that review.


## Paired ground and facing correction (2026-09-10)

Completed: actual ground-hit identity exposed character meshes/capsules as the source of the elevated paired target. Character-aware environment support and upright yaw now hold paired target height and actor pitch steady on both maps. Build, four red/green regressions, 773/773 full headless tests and 16/16 rendered post-fix scenarios pass; all 2,032 PNGs finish without loss. The early translation defect is separate: native logs show local delta amplification, and victim translation-off reduces the early step to 2.68 cm while contact worsens. Six fresh contact comparisons fail the unchanged provisional criteria; no quality reference or asset correction is promoted. See [grounding evidence and next action](../audits/PAIRED_WARP_GROUNDING_2026-09-10.md).

Completed comparison: the strict transient window/offset control also reconciles attacker translation to the pair configuration. Its screening exposed and helped fix victim collision ownership during opening lethal damage. See the current disposition above. Next: review authored relative facing/placement and intended blade contact, because the tested window/offset variants still fail contact and do not justify saved settings. Use the existing capture and paired-analysis tools; no plugin expansion is needed.
