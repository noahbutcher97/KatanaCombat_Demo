# Reusable combat capture and analysis verification

Follow-up: [scenario evaluation and verification](COMBAT_SCENARIO_EVALUATION_2026-09-09.md) adds schema-2 pose/frame evidence, repeatable input/recovery scenarios, explicit eligibility and selected references. The results below describe the initial reusable layer.

## Result

The project now has a capture/analysis layer usable from ordinary PIE and C++ automation, independent of the existing proof fixtures. A contributor can record another interaction with `Combat.Capture.Start`, `Mark`, and `Stop`, then run `Tools/CombatCapture/analyze_capture.py` to review frames and measurements. See [the usage guide](../guides/COMBAT_CAPTURE_AND_ANALYSIS.md) for commands, schema, extension examples, and metric boundaries.

This is a reusable first implementation, not every future visual-quality validator. It measures sampled movement, component-space point motion, configurable point distances, input-to-action timing and evidence quality. It supports compatible baseline deltas and synchronized frame playback. It does not implement foot-support detection, skin/weapon penetration, occlusion scoring, learned perception, or a universal feel score. Existing proof fixtures keep their older specialized outputs; the shared recorder and console do not require them.

## Changed files

- `Source/KatanaCombatEditor/Public/Analysis/CombatCaptureSession.h` and `Private/Analysis/CombatCaptureSession.cpp`: explicit world/participants, unique recording directories, bounded sampling/export, event labels, frame provenance and compilation readiness, telemetry retention/loss reporting, teardown cleanup.
- `Source/KatanaCombatEditor/Private/Analysis/CombatCaptureCommands.cpp` and `Private/KatanaCombatEditor.cpp`: start/mark/stop commands and editor-module lifecycle.
- `Source/KatanaCombatEditor/KatanaCombatEditor.Build.cs`: ImageCore dependency and independent compilation of editor translation units. Adding tooling changed unity grouping and exposed pre-existing private helper collisions (`FloatTolerance`, `IsApplyMode`, `ManifestPath`) between authoring files. Disabling unity for this editor module preserves each file's private scope without changing authoring behavior. No runtime module dependency changed.
- `Source/KatanaCombatTest/Private/CombatCaptureTests.cpp` and test README: capture lifecycle/limits/world identity and two real-map integrations using the console and API.
- `Tools/CombatCapture/analyze_capture.py` and `test_analyze_capture.py`: standard-library-only analyzer, data-quality checks, JSON/HTML output, comparison logic, and numerical/error regressions.
- Matching guide, plan and this report; link from the earlier capture-hardening audit.

No gameplay source or asset was changed by this slice. Existing user WIP remains. No commit was created.

## Evidence

All verification logs and recording references are under `Saved/Logs/CombatCapture-20260909-151145/`.

| Check | Result | Evidence |
|---|---|---|
| Editor build | Success, exit 0 | `build5.out.log`, `build5-ubt.log`, `build5.exitcode.txt` |
| Rendered `KatanaCombat.Capture` | 4/4, no automation errors, 5 warnings, exit 0 | `rendered-verified.log`, `rendered-summary.json` |
| Full headless `KatanaCombat` | 744/744, no automation errors, 378 warnings, exit 0 | `full-headless.log`, `full-headless-summary.json` |
| Analyzer regressions | 8/8 | `analyzer-tests.err.log`, `analyzer-tests.exitcode.txt` |
| Live rendered bundle analysis | Both bundles intact; all 27 PNGs passed PNG/provenance checks | `rendered-captures.json` and each recording's `analysis.json` |
| Headless bundle analysis | Both bundles intact; visual evidence explicitly unavailable | `headless-captures.json` and each recording's `analysis.json` |
| Browser report | Frame loaded, scrubber selected a frame, timed playback advanced; no console errors in isolated HTTP review | `browser-check.json`; inline browser screenshot reviewed |
| Existing asset WIP | All 54 pre-existing modified/untracked/deleted Content paths retained their hash/absence state | `content-before.json`, `content-preservation.json` |

Both automation summaries have matching discovery/completion counts and the explicit success exit marker. Warnings are retained in the logs; the suite is not warning-free.

The final rendered recordings are:

| Scenario / entry point | Samples | Frames | Recording under `Saved/CombatCaptures/` |
|---|---:|---:|---|
| Third-person map / ordinary PIE console | 152 | 14 | `20260909T193326-E7B35C76414308D3ADC6028E3058F7A5` |
| Defense matrix map / C++ API | 138 | 13 | `20260909T193320-26EC1F04476E9245909EAAA5C2434159` |

Each submits a light-attack press/release through the combat input interface, preserving normal AI and actors. Each export contains one correlated input/action start, occurring in the same simulation timestamp (0 ms by this metric), plus the release input without a separate action start. This is not a physical-input or input-to-visible-response latency measurement. Actual image size was 759 x 483, taken from the viewport rather than inferred from command-line resolution arguments.

The final frames end within 0.146 simulation seconds of the last motion sample in the third-person recording and 0.191 seconds in the matrix recording. Reports retain sampling gaps and off-camera pose-staleness caveats. Inspection showed actual gameplay frames and an awkward close-up during movement; the recorder preserves the chosen camera and does not claim that valid images are well framed. That is useful debugging evidence, not a capture success criterion being substituted for animation quality.

## Corrections found during verification

- Initial builds exposed existing authoring-file unity collisions; the final editor build compiles those translation units independently.
- The first rendered run captured both maps, then hit a container-alias assertion in the new duplicate-participant test fixture. The duplicate is now copied before insertion.
- The next lifecycle run exposed TextureShare cleanup side effects from starting gameplay in a synthetic recorder-only world. That fixture now creates a world without gameplay BeginPlay; actual PIE coverage remains in both map tests. The final lifecycle test covers sample limits, repeat stop, unique restart, world teardown and telemetry restoration.
- Background shader compilation initially suppressed later images during an attack. The recorder now waits for the initial view, then continues through subsequent compilation while recording readiness per frame. The analyzer reports readiness and frame-coverage gaps.

Those intermediate runs remain recorded as failures; the final passing results use the corrected source. Browser inspection used inline tool results because the browser connector's configured file-output roots rejected this workspace; the normal workspace build/export path remained writable.

## Next use

Use the shared recorder for the pending real-montage input/recovery and active-bystander scenarios. Define scenario-specific expectations separately from the recording schema. Add support/contact/visibility validators only with meaningful annotations, known defect examples, and calibrated criteria. Plugin extraction or a perception model is not required to use this layer now.
