# Combat capture sampling reliability

## Scope

This follow-up addresses capture overhead before paired-contact reports drive authoring changes. It preserves the existing hold scenario's 75 ms pose-gap criterion, 15 Hz PNG request, 60 Hz pose request and 960 by 540 viewport. No gameplay behavior, project assets or acceptance references are changed. Work remains in the existing editor/test modules.

## Diagnosis

The earlier eight-case rendered hold matrix passed gameplay but had five pose-inconclusive captures. Its largest pose gap was 86.408 ms. That gap spans exactly one engine frame, follows a PNG capture and has an 86.587 ms wall-clock gap. The other seven largest gaps in that capture also follow PNGs and span one engine frame. This implicates a game-frame stall, rather than a sampler skipping multiple completed frames.

Before changing export, three repeated ThirdPerson Forward runs in each mode kept rendering enabled. All nine passed. Pose-only runs recorded 339–342 poses, with maximum gaps of 35.602–40.352 ms. Synchronous PNG runs recorded 148–165 poses, with maximum gaps of 65.171–72.564 ms. The earlier failure above 75 ms did not recur in this baseline; the reduced density and increased tail remained.

Timing the synchronous path on both maps isolated PNG compression/write from viewport readback. ThirdPerson's median was 50.209 ms for encoding/writing versus 5.622 ms for readback; its maxima were 54.340 and 10.026 ms respectively. The largest sampled gaps correlate directly with these preceding image costs. Local UE 5.6 source confirms that viewport `ReadPixels` flushes rendering commands, while `SaveImageByExtension` compresses and writes synchronously.

DefenseMatrix's median encode/write and readback times were 46.173 ms and 4.785 ms. Its initial `scenario_ready` period had an 82.927 ms gap, including a 41.472 ms readback. The evaluated hold interval began later and had a 66.697 ms maximum, so that run correctly passed the scoped criterion. Synchronous readback can still stall; PNG work is the repeated dominant cost addressed here.

Evidence:

- Historical affected matrix: `Saved/CombatScenarioRuns/20260910T230304-9bbcd237`.
- Rendering-enabled baseline: `Saved/CombatScenarioRuns/20260910T232056-02a37c79`.
- Instrumented synchronous captures: `Saved/CombatScenarioRuns/20260910T233024-302b2a7b`.
- Build/test logs and derived measurements: `Saved/Logs/CaptureSampling-20260910-192055`.

## Change and integrity contracts

`FCombatCaptureImageWriter` owns bounded background lossless PNG encoding/writing. Up to four images may be submitted, with a 64 MiB reservation for raw plus encoded buffers; encoder workspace is additional. Conservative file reservations are checked against actual compressed sizes and counted against the session byte budget. Worker jobs hold pixels and paths, never world/actor/session pointers.

`FCombatCaptureSession` stamps viewport observations before readback, retains original simulation/frame/sample/pose identities and collects completed results in submission order. Readback, encoding and file-write durations are separate. Stop and teardown drain submitted work before frame streams close and the final manifest is published. Capture duration and drain duration are distinct. Outstanding work is bounded in count/size; filesystem responsiveness can still delay finalization.

Queue rejection and write failures produce explicit incomplete/error evidence. Final manifests include submitted/pending/failed/rejected counts and peak depth. A completed capture has no pending jobs. No synthetic intermediate poses, timestamp rescaling, lossy compression or threshold relaxation is used.

Changed implementation files:

- `Source/KatanaCombatEditor/Public/Analysis/CombatCaptureImageWriter.h`
- `Source/KatanaCombatEditor/Private/Analysis/CombatCaptureImageWriter.cpp`
- `Source/KatanaCombatEditor/Private/Analysis/CombatCaptureSession.cpp`
- `Source/KatanaCombatEditor/Private/Analysis/PairedContactProfileEvaluation.cpp` (observed-only report wording and declared gap)
- `Source/KatanaCombatEditor/KatanaCombatEditor.Build.cs`
- `Source/KatanaCombatTest/Private/CombatCaptureImageWriterTests.cpp`
- `Source/KatanaCombatTest/Private/CombatCaptureTests.cpp`
- `Source/KatanaCombatTest/Private/PairedContactEvaluationTests.cpp`

## Sampling resolution controls

The new `BriefContactSamplingPhases` control uses a point crossing a 10 cm sphere at 400 cm/s, giving a known 50 ms contact. Sixty sample phases at 60 Hz detect the contact under a 25 ms maximum-gap requirement. The same phases at 70 ms spacing are inconclusive under that requirement. Contact outside the declared interval fails. With the broader 75 ms criterion, a constructed phase misses the real 50 ms contact and returns a sampled failure.

This demonstrates why a valid capture and a sufficient contact resolution are separate requirements. A sampled failure does not establish continuous absence. Production contact duration has not been inferred from these controls. Authoring review must choose intent/geometry and the shortest required contact duration, then require sufficiently small observed gaps or add a separately validated motion/sweep model. The paired report now states this limit directly.

## Verification

The current editor build succeeds. Focused headless automation passes 28/28, including image fidelity/bounds/failure/teardown, recorder integration, hold/finisher scenarios and paired evaluation. Rendered recorder plus paired evaluation passes 10/10, including both maps' deliberate PNG-write obstruction, image ordering and one-frame automatic stop. Offline analyzer/evaluator tests pass 28/28.

The affected rendered hold matrix passes 8/8 with unchanged criteria in `Saved/CombatScenarioRuns/20260910T234141-3a041c9e`. The comparison below uses each full recording's largest gap; eligibility is evaluated over the narrower hold/recovery interval. This is a diagnostic replay comparison, not a performance benchmark or an animation-quality approval.

| Map | Direction | Poses before / after | Maximum gap before / after (ms) | Pose eligibility before / after |
| --- | --- | --- | --- | --- |
| DefenseMatrix | Backward | 177 / 325 | 61.171 / 42.959 | pass / pass |
| DefenseMatrix | Forward | 179 / 335 | 84.293 / 36.747 | inconclusive / pass |
| DefenseMatrix | Left | 200 / 362 | 85.471 / 37.445 | inconclusive / pass |
| DefenseMatrix | Right | 165 / 313 | 68.001 / 41.659 | pass / pass |
| ThirdPerson | Backward | 158 / 329 | 83.741 / 39.925 | inconclusive / pass |
| ThirdPerson | Forward | 163 / 340 | 86.408 / 37.815 | inconclusive / pass |
| ThirdPerson | Left | 174 / 364 | 66.893 / 37.006 | pass / pass |
| ThirdPerson | Right | 150 / 314 | 81.172 / 33.288 | inconclusive / pass |

All eight captures finish with zero pending, rejected or failed images. Peak depth is one or two images; the longest final drain is 34.062 ms. One ThirdPerson hold PNG was also inspected directly: it contains the player and the expected scene, without apparent image corruption. This is a frame-integrity spot check, not an assessment of animation quality.

The repeated rendering-enabled comparison also passes 9/9 in `Saved/CombatScenarioRuns/20260910T234608-0bf00818`. New pose-only runs each record 341 poses, with maximum gaps of 33.943–40.239 ms. PNG runs record 339–341 poses and 85 frames each, with maximum gaps of 36.036–43.429 ms. This restores pose density close to pose-only capture without reducing image resolution or rate. All queued PNGs finish successfully.

Two additional DefenseMatrix Forward recordings pass in `Saved/CombatScenarioRuns/20260910T235114-ec55e37a`, recording 337 and 338 poses with maximum gaps of 41.458 and 43.781 ms. Together with the matrix run, this provides three rendered Forward recordings on that map. No pending, rejected or failed image remains in any of the thirteen new rendered hold captures.

Fresh completed-finisher gameplay/capture checks pass 2/2 in `Saved/CombatScenarioRuns/20260910T235233-2bbcd802`. The same paired evaluator consumes both captures successfully (native commandlets exit 0). Both diagnostic evaluations return `fail` (Python exits 1) for the existing contact/alignment criteria, with no inconclusive cases. Runtime blade/torso minimum signed gap is 44.747 cm on each map against the unchanged provisional 8 cm maximum. The contact intervals' maximum observed pose gaps are 16.786 ms on ThirdPerson and 16.676 ms on DefenseMatrix. Runtime entry and release contact rules pass; authored blade contact and entry, and relative alignment budgets, still fail.

- ThirdPerson capture: `Saved/CombatCaptures/20260910T235257-09327B574FDF18169E5CE9BD9DAAFD60`.
- ThirdPerson contact report: `Saved/PairedAnimationEvaluations/20260910T235411-0E71F4FB4C43E191957B7A85B9F84EAA/report.html`.
- DefenseMatrix capture: `Saved/CombatCaptures/20260910T235329-6DF85B1443231073E497A2BADF7B0F2D`.
- DefenseMatrix contact report: `Saved/PairedAnimationEvaluations/20260910T235419-67C3C76A4720E56C4A1D978896F68855/report.html`.

The thirteen new rendered hold captures successfully finish all 1,095 submitted PNGs, with no pending/rejected/failed images and a peak queue depth of two. The source comparison against the pre-fix baseline identifies only the eight editor/test files listed above; all 223 runtime files match. All 54 protected Content paths retain their hashes or prior absence, and no additional Content change appears. No files are staged or committed by this work.

This bounded reliability pass is complete. No new full-project baseline is claimed; the prior full 765-test baseline predates this editor-only change. Remaining synchronous readback and other game-frame stalls remain observable and subject to interval-specific eligibility. Review the finisher's intended contact geometry, duration and relative placement next, using these fresh reports before editing assets. Do not promote the provisional profile or a sampled miss into artistic approval or proof of continuous absence.

## Reproduction

After building the current `KatanaCombatEditor` target:

```powershell
python Tools/CombatCapture/run_scenario.py --scenario Tools/CombatCapture/scenarios/hold-release-recovery.json --map all --variant all --mode rendered --skip-build
python Tools/CombatCapture/run_scenario.py --scenario Tools/CombatCapture/scenarios/hold-release-recovery.json --map ThirdPerson --variant Forward --mode all --render-world --repeat 3 --skip-build
python Tools/CombatCapture/run_scenario.py --scenario Tools/CombatCapture/scenarios/hold-release-recovery.json --map DefenseMatrix --variant Forward --mode rendered --repeat 2 --skip-build
python Tools/CombatCapture/run_scenario.py --map all --variant Completed --mode rendered --skip-build
```

The evidence directory retains `sampling_probe.py`, `compare_sampling_matrix.py`, summaries and scope checks for this investigation. The supported recorder/analysis commands and timing fields are described in the [capture guide](../guides/COMBAT_CAPTURE_AND_ANALYSIS.md); contact resolution guidance is in the [paired evaluation guide](../guides/PAIRED_ANIMATION_EVALUATION.md).
