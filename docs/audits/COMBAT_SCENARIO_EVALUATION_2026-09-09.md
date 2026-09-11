# Reusable scenario evaluation verification — 2026-09-09

This extends the [shared recorder](COMBAT_CAPTURE_AND_ANALYSIS_2026-09-09.md) with repeatable gameplay scenarios, explicit pose/frame eligibility, run identity and selected mechanical references. It remains project tooling in the existing repository/modules. The [usage guide](../guides/COMBAT_CAPTURE_AND_ANALYSIS.md) documents the complete command and extension boundary.

The bounded scenario-evaluation slice is implemented and verified. It is ready for gameplay regression work and the selected coarse displacement checks. Remaining measurement limits are explicit below.

## Implementation and proof boundary

- `CombatCaptureSession.*` now exports schema 2: finalized-pose identities, bone rotations, actor-relative points, contributing montage weights, ownership/movement state, and frame/sample association. The wall watchdog runs independently of world ticks; data volume and metadata are bounded.
- `CombatCaptureScenarioTests.cpp` drives completed/interrupted finishers through Enhanced Input on both maps, with active bystander StateTrees and attacks. It checks input suppression, victim/attacker ownership, token/partner cleanup, release/repress attack recovery and actual movement acceleration. It adds no test friends and changes no gameplay source.
- `run_scenario.py` builds, executes, captures and evaluates a selected scenario. It records source/config/scenario identity, engine and editor DLL identity, and AssetRegistry project dependencies plus external actors. This run hashed 480 project asset files for ThirdPerson and 364 for DefenseMatrix; no absent soft dependencies were reported.
- `evaluate_capture.py` separates integrity, eligibility and outcomes. Each assertion has a reason and time interval. Missing/stale/under-sampled evidence cannot satisfy the corresponding pose/visual assertion. Paused/dilated intervals are explicitly unsupported by the initial transition criterion.
- `summarize_runs.py` reports repeatability/overhead and creates explicitly selected references. References preserve observed variability and a stated margin/acceptance basis; they are never promoted from the latest recording automatically.

The initial detector measures a sampled actor-relative pelvis displacement envelope. It does not certify skin contact, weapon penetration, foot support, occlusion, visual readability, artistic motion quality, physical device response, audio or haptics. Montage contribution, observed damage state and control restoration are distinct sampled events, not a single input-to-visible-response number.

## Findings that changed the implementation

1. A first takeover assertion incorrectly expected the victim to use the attacker's paired-input flag. Source, existing unit tests and recorded behavior showed that the victim is owned by `HitReactionComponent`. The scenario now checks that state and actual input/movement restrictions. Gameplay code was unchanged.
2. The authored finisher commits lethal damage at its initial sync notify. Interruption after 0.45 seconds therefore checks cancellation after damage and preservation of that damage. Survival from cancellation before damage remains outside this scenario's evidence.
3. Resizing the outer PIE window produced 567×322 images despite requesting 960×540. Setting a fixed scene viewport size corrected it. The evaluator now rejects a mismatch between actual PNG dimensions and scenario configuration. All reference captures below are 960×540.
4. Failed Unreal tests repeat captured log messages. The first runner treated the replay as a second artifact; it now normalizes repeated output paths while retaining failure status and the failing scenario's evidence.

Exploratory runs are retained in `Saved/CombatScenarioRuns/20260909T210313-96285a5e/` and `20260909T211157-061bf9ea/`. They are not the calibrated reference set.

## Repeatability and reference basis

`Saved/CombatScenarioRuns/20260909T211520-46a2865a/batch.json` records **12/12 passing rendered scenarios**, three repetitions of each map/outcome combination. Every gameplay assertion and pose/frame eligibility check passed. The captures contain 907 motion samples and 887 validated PNGs. Maximum measured sample and frame gap in the evaluated windows was 62.18 ms; requested 60/30 Hz did not establish that actual rate.

| Map/outcome | Observed maximum pelvis steps, three runs (cm) | Reference envelope (cm) |
|---|---|---|
| ThirdPerson / Completed | 8.263, 8.341, 8.381 | 18.381 |
| ThirdPerson / Interrupted | 3.782, 3.589, 3.751 | 13.782 |
| DefenseMatrix / Completed | 7.785, 8.989, 10.213 | 20.213 |
| DefenseMatrix / Interrupted | 3.121, 3.217, 3.158 | 13.217 |

Each envelope is the largest observed step plus a declared 10 cm margin. These are conservative mechanical controls, not artist-approved motion standards. Representative side-view frames were inspected on both maps and outcomes for participant framing. The intended first sensitivity check is a large, known 200 cm mesh displacement. Smaller defects and faster transitions need their own calibrated criterion and denser evidence.

Small reference metadata lives in `docs/references/combat-capture/2026-09-09/`. Original timestamps, source/asset identities and bundle hashes are preserved. Raw recordings remain under generated `Saved/` paths; archive them separately for long-term pixel review.

## Robustness and reporting

The deliberate-control batch `Saved/CombatScenarioRuns/20260909T213045-5512976e/` produced **4/4 expected displacement failures**, while every gameplay assertion and evidence-eligibility check passed and each Unreal process exited 0. The external runner correctly exited 1 for the quality failures. Measured steps were 201.04–201.18 cm against the 13.22–20.21 cm reference envelopes. The nearest-frame links were checked in the browser; the recorded image visibly shows the injected mesh offset. Details are in `defect-control-summary.json` under the verification evidence directory.

Fresh processes with the injection removed then passed **4/4** against the same selected references in `Saved/CombatScenarioRuns/20260909T213332-e4d9d386/`, exit 0. Maximum steps returned to 9.045 cm (ThirdPerson completed), 3.744 cm (ThirdPerson interrupted), 9.178 cm (DefenseMatrix completed) and 3.112 cm (DefenseMatrix interrupted). The test changes a transient PIE mesh offset; it does not alter an animation asset. The separate fixed-viewport correction was also verified by the final 960×540 captures and resolution eligibility checks.

The same-backend overhead experiment in `Saved/CombatScenarioRuns/20260909T212224-fc428b49/` passed **9/9** scenarios, with three repetitions per capture mode and rendering enabled throughout:

| Mode | Median scenario wall time (s) | Median paired observation duration (simulation s) |
|---|---|---|
| Capture disabled | 4.9624 | 2.41667 |
| Motion/telemetry | 4.9601 | 2.41672 |
| Rendered PNG capture | 5.0990 | 2.39962 |

PNG capture added about 2.8% to median scenario wall duration in this sample. Paired-duration observations varied within one coarse captured frame, and all gameplay assertions passed. Motion/telemetry overhead was below this experiment's noise; the slightly lower median is not evidence of a speedup. These short replays do not establish general performance or sub-frame visual timing. Given the measured cost and the current detector's 75 ms sampling requirement, no asynchronous readback rewrite was needed for this slice.

The focused Unreal failure-path test passes for no-world-tick wall timeout, data exhaustion, participant destruction, nominated-mesh destruction/replacement, telemetry reset/overflow and a deliberately obstructed final manifest write. It verifies diagnostic output and restoration without adding gameplay access seams. The final rendered console/API integrations also pass **2/2**, including automatic stop at exactly one captured frame and the explicit frame-limit stop reason.

Offline regression coverage includes known numerical motion, known defect/removal, stale/missing/non-finite poses, frame readiness/association/resolution, incomplete scenarios, legacy schema limits, incompatible references, modified/duplicate reference recordings, and rejection of a rerun without leaving prior success current.

Browser inspection of the evaluation table and frame timeline passed: links returned HTTP 200, scrubbing decoded the selected 960×540 image, and neither page had horizontal overflow. Evidence is in `Saved/Logs/CombatEvaluation-20260909-164309/browser-review.json`.

## Final verification and disposition

The final editor build succeeded (`build5.out.log`, exit 0). The full headless suite then passed **749/749**, with no automation errors, 419 warnings, discovery/completion agreement and an explicit exit-0 marker. Evidence: `Saved/Logs/CombatEvaluation-20260909-164309/full-headless-summary.json` and matching logs. Offline regression tests pass **26/26** in the same evidence directory. The warnings remain diagnostic output; this is not a warning-free run.

`rendered-limits-summary.json` verifies the two final rendered recorder integrations. The twelve rendered repeatability runs, nine overhead controls, four expected defect failures and four fresh passing removals are recorded in the batch paths above. `browser-result-cases.json` additionally verifies failure-frame links and capture-disabled report links. `static-review.json` records valid affected links, no trailing whitespace, and no opaque local-process names in the new implementation.

`content-preservation.json` confirms that all **54 protected Content paths** retain their original hashes or absence, with no additional/missing changed Content paths. This slice changes editor/test tooling, offline tools, references and documentation; gameplay source and assets were preserved. Nothing was committed or staged.

Resume the outstanding combat acceptance work using this shared runner and recorder. Add a contact/support/perceptual evaluator when a concrete gameplay question supplies its intended region, timing and acceptance basis. Before-damage cancellation, physical device latency, precise sub-frame response, paused/hitstop-aware transition criteria, foot support and artistic acceptance remain unmeasured. A separate plugin or general learned feel platform is not required for this completed slice.
