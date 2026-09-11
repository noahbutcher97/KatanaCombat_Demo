# Finisher sync reporting and movement experiment

Date: 2026-09-10 (America/New_York; evidence timestamps are September 11 UTC).

The opening-sync reporting defect is fixed. A controlled runtime experiment confirms that the paired-collision movement flags prevent root-motion travel: permitting movement restores actor travel and warp evaluation while preserving the tested input and cleanup behavior on both maps. It also exposes a large victim movement step near the warp-window end. The blade/torso criterion still fails. No animation or gameplay asset correction has been saved.

This completes the reporting repair and first movement experiment proposed in the [contact diagnosis](FINISHER_CONTACT_DIAGNOSIS_2026-09-10.md). The next correction should isolate the warp discontinuity and attacker translation-policy mismatch before choosing contact/damage timing or applying asset edits.

## Changes

- `CollectMontageSyncEvents` follows Unreal's forward notify-state overlap rule. It retains nominal time, actual trigger time/offset and end trigger; states active at fresh section entry receive effective pair time zero. States ending at or before entry are excluded. The HTML/JSON report distinguishes primary/configured-damage flags from an observed runtime commit.
- The reusable recorder now includes `movement_mode`, `paired_state_lease_count` and active motion-warp modifier class/state/window/translation/rotation settings. These describe observed component state, not a direct measurement of root-motion consumption or causal warp work.
- `run_scenario.py --finisher-experiment permit-root-motion` replaces only the two montages' paired-collision notify objects with transient copies whose `bDisableMovement` is false. Other notify properties, clocks, input ownership and assets are preserved. Original notify objects are restored before scenario completion, including interruption, and checked for unchanged package dirty state. No packages are saved.
- Capture metadata, scenario JSON and runner context carry the experiment identity; exact original notify/property changes are retained in `runtime_asset_overrides`. Native contact reports identify the experiment and continue to evaluate the actual captured world points against the unchanged authored reference. Inconsistent or unsupported override metadata is rejected. Experimental runs have distinct reference compatibility, so unchanged disk hashes cannot masquerade as unchanged effective runtime configuration.

The scope is nine editor/test/tool files: `CombatCaptureSession.cpp`, `PairedContactProfileEvaluation.cpp`, `PairedContactEvaluation.h`, `PairedAnimationAnalysisSubsystem.h`, `CombatCaptureScenarioTests.cpp`, `PairedContactEvaluationTests.cpp`, `run_scenario.py`, `evaluate_capture.py`, and `test_evaluate_capture.py`. The paired evaluation guide, plan and handoff document the resulting workflow and next action. Runtime source is unchanged.

## Verification

Evidence root: `Saved/Logs/FinisherMovementExperiment-20260910-203630/`.

| Check | Result |
| --- | --- |
| `KatanaCombatEditor Win64 Development` build | Passed, exit 0 |
| `KatanaCombat.Editor.PairedEvaluation` | 9/9 passed, exit 0 |
| Offline capture/evaluation tests | 30/30 passed |
| Unmodified completed/interrupted rendered scenarios, both maps | 4/4 passed |
| Movement-permitted completed/interrupted rendered scenarios, both maps | 4/4 passed |
| PNG export across the eight captures | 904 frames; zero rejected, failed or pending |
| Native contact evaluations of the four completed captures | All runtime lanes measured; all four blade criteria failed |
| Native mismatched-override control | Runtime cases inconclusive with `Inconsistent runtime experiment provenance` |
| Protected Content and runtime source | 54 Content paths preserved; all 223 runtime files unchanged |

The new boundary test compares the collector with Unreal's own `GetAnimNotifiesFromDeltaPositions`, covering opening trigger offsets, an interior state, a state already active at section entry, expired states, a later section, rate scaling, and primary/secondary damage flags. A fresh unchanged-finisher report includes attacker `FinisherImpact` at trigger -0.0001 s / effective pair time 0 and victim `Impact` at +0.0001 s. All 73 authored blade-gap observations are identical to the preceding report. `sync-report-verification.json` records that check.

The scenarios assert input takeover/suppression, cleanup, victim outcome/token release, fresh attack and movement recovery, and active bystanders. Experimental runs additionally assert successful application/restoration of the temporary notify changes. Recorded attacker movement acceleration remains zero while paired ownership is active in all eight captures, despite movement input injection. Actor travel in the experiment therefore does not represent ordinary movement-input acceleration leaking through the tested suppression path.

No full-project suite rerun is claimed; the prior 765-test full baseline predates these bounded editor/test changes. No new exact damage-commit-count assertion or full artistic acceptance is implied by the scenario passes. Native commandlet exit 0 means evaluation completed; the contact runners correctly return 1 when configured criteria fail.

## Results

The completed-capture motion comparison covers 137 observations per role, from approximately montage time 0.0167 to 2.28 s. Distances are maximum displacement from the first paired actor position, after the initial sync nudge; they are not path length or desired warp distance.

| Map / control | Attacker travel | Victim travel | Movement mode during interval | Warp modifier observations |
| --- | --- | --- | --- | --- |
| ThirdPerson / unchanged | 0 cm | 0 cm | Both `MOVE_None` | None |
| ThirdPerson / movement permitted | 38.21 cm | 182.97 cm | Both `MOVE_Walking` | Active attacker and victim modifiers |
| DefenseMatrix / unchanged | 0 cm | 0 cm | Both `MOVE_None` | None |
| DefenseMatrix / movement permitted | 38.10 cm | 186.59 cm | Both `MOVE_Walking` | Active attacker and victim modifiers |

Both controls retain a paired-state lease: the experiment permits movement without discarding collision/input ownership wholesale. The unchanged control reproduces the previous freeze; changing only the notify movement flags restores motion on both maps. Terminal victim behavior does not independently prevent movement during these paired observations.

The [comparison figure](../../Saved/Logs/FinisherMovementExperiment-20260910-203630/movement-and-contact-comparison.png) and `movement-results.json` retain the motion and contact comparison.

### Movement restoration exposes a discontinuity

The victim moves **46.22 cm in 16.67 ms** at montage time **0.8670–0.8837 s** on ThirdPerson. DefenseMatrix shows **31.18 cm in 16.67 ms** at **0.8543–0.8710 s**. Its `RootMotionModifier_SkewWarp` is active in these observations and ends at approximately **0.872984 s**. The corresponding nearby 60 Hz unwarped source-root samples from the preceding diagnosis have a maximum step of only **0.3685 cm** across 0.8333–0.9167 s.

This identifies the warp-window end as the next interval to isolate. It is not a direct measurement of the modifier's contribution, nor proof that a particular target-update formula is solely responsible. The movement-permitted result must not be promoted to a smooth-motion reference. The current generic transition envelope measures actor-relative pose steps and was not selected for these runs; it would not substitute for a world-space actor/warp discontinuity assertion.

### Contact remains unresolved

All geometry, timing and thresholds remain unchanged, including the provisional 18 cm torso sphere, 0.3–1.5 s blade interval and 8 cm maximum signed gap.

| Map | Unchanged minimum blade gap | Movement-permitted minimum blade gap | Result |
| --- | --- | --- | --- |
| ThirdPerson | 44.7466 cm | 47.3947 cm | Fail in both |
| DefenseMatrix | 44.7465 cm | 47.1456 cm | Fail in both |

Runtime entry-root separation remains within its provisional criterion. The release-left-hand criterion changes from pass to fail: the movement-permitted victim's outward travel puts even the minimum distance at **202.61 cm ThirdPerson / 204.85 cm DefenseMatrix**, beyond the existing 200 cm maximum. This is a failed provisional criterion, not justification to shorten the victim travel or relax the bound without reviewing the intended release.

The reviewed ThirdPerson frames around 0.6 and 1.25 s show a coordinated close interaction followed by outward victim movement. They do not establish the intended blade target or approve the grip, impact time, surface contact, release or ragdoll handoff. Observed contact misses still do not prove continuous absence between sampled poses.

## Exact artifacts

- Unmodified batch: `Saved/CombatScenarioRuns/20260911T004557-18b12a00/batch.json`.
- Movement-permitted batch: `Saved/CombatScenarioRuns/20260911T004801-e2d790e5/batch.json`.
- ThirdPerson movement-permitted capture: `Saved/CombatCaptures/20260911T004825-F2A69A5B47940859C0CB2FA211FE1425`; reviewed frames `frame_000027.png` and `frame_000047.png` under `frames/`.
- DefenseMatrix movement-permitted capture: `Saved/CombatCaptures/20260911T004928-DA121DE54C6FE2B2C2905898FE14F782`.
- ThirdPerson movement-permitted contact report: `Saved/PairedAnimationEvaluations/20260911T005748-D210202B4CDE8B9CB8C169A22A097AAC/report.html`.
- DefenseMatrix movement-permitted contact report: `Saved/PairedAnimationEvaluations/20260911T005740-C31C91014D6162CC783948AD5061BA46/report.html`.
- Provenance-refusal report: `Saved/PairedAnimationEvaluations/20260911T005949-AB994A364E4E65462116F8A0DB14C484/evaluation.json`.

The evidence root also contains build/test logs, exact commands, `actor-step-diagnosis.json`, `contact-results.json`, `capture-verification.json`, `input-acceleration-verification.json`, source snapshots and `scope-verification.json`. A scratch orchestration script initially failed while encoding a console message; its completed native evaluation was not treated as a process failure. The corrected script completed the four recorded contact comparisons. No Unreal crashes occurred in this pass.

## Reproduction and next action

```powershell
python Tools/CombatCapture/run_scenario.py --map all --variant all --mode rendered --finisher-experiment none
python Tools/CombatCapture/run_scenario.py --map all --variant all --mode rendered --finisher-experiment permit-root-motion --skip-build
python Tools/CombatCapture/evaluate_pair.py --profile Tools/CombatCapture/pairs/finisher-contact.json --capture Saved/CombatCaptures/<ThirdPerson-completed-capture> --skip-build
```

Use `finisher-contact-mannequin.json` for DefenseMatrix. Keep source/config/tools fixed while a batch runs. The same reusable capture and contact evaluator handle both controls; no alternate contact formula is introduced.

Next, isolate the victim's approximately 0.87 s world-space movement spike and the attacker's translation mismatch with separately declared overrides. Hold input and collision ownership fixed, compare moving versus fixed warp targets and modifier translation policy one variable at a time, and retain actor displacement plus original montage clocks. Then review actual contact/impact/release intent, freeze defensible criteria and verify any selected asset correction on both maps, including interruption. The current disk assets are deliberately unchanged; no reference was promoted, files staged or commits created.
