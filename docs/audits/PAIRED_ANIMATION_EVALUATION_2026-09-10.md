# Hold recovery and paired animation evaluation — 2026-09-10

This work verifies the real-montage hold/recovery path and extends the existing capture tooling with intended-contact and relative-alignment analysis. No runtime implementation or Content asset was changed in this slice. Existing runtime/editor/asset WIP remains in the workspace.

## Implemented behavior

- `HoldReleaseRecovery` injects public Enhanced Input actions, observes real hold-window notifies, holds through competing heavy input, selects the authored directional follow-up, waits for natural recovery, observes movement acceleration and starts a fresh attack. Four camera-relative directions run on each existing map. The fixture moves enemies out of range in disposable PIE and records that isolation explicitly.
- Contact profiles declare point/segment-to-region geometry, intended intervals, overlap/separation limits, optional orientation, strikes and sustained contacts. Required weapon endpoints come from the nominated attached mesh, not a skeletal fallback or whichever bone happens to be nearest.
- The pure library evaluates both authored and captured observations. The subsystem supplies assets, absolute montage sampling and runtime evidence. The paired preview and commandlet share this path. The CLI creates unique JSON/HTML reports, checks source/profile identity, and returns a useful status.
- Entry/contact/exit alignment budgets compare the two roots relative to each other and preserve original victim timing. They measure the geometric correction needed to match authored placement, not actual motion-warp work. Missing required evidence remains inconclusive.
- Profiles and reports preserve criteria basis, effective preview transforms, asset and binary hashes, source-component identities, original times and nearest-frame offsets. Unsaved or mismatched assets, stale poses, sparse intervals and unsupported clocks cannot silently pass.

See [usage and profile format](../guides/PAIRED_ANIMATION_EVALUATION.md) and the [implementation plan](../plans/COMBAT_CAPTURE_AND_ANALYSIS.md).

## Verification evidence

Evidence root: `Saved/Logs/PairedEvaluation-20260910-180025/`.

| Check | Evidence and outcome |
| --- | --- |
| Final editor build | `effective-input-build.log`: succeeded |
| Final focused native evaluator and recorder tests | `effective-input-tests.log`, `effective-input-summary.json`: 8/8 succeeded, process exit 0 |
| Offline capture/evaluator tests | `offline-final-tests.log`, command `python -m unittest discover -s Tools/CombatCapture -p 'test_*.py' -q`: 28/28 passed |
| Full headless regression | `full-regression.log`, `full-regression-summary.json`: 765 unique tests succeeded, no failures, process exit 0; includes all eight hold scenarios |
| Protected Content | `content-verification.json`: all 54 initial modified/deleted/untracked paths retain their starting hashes or absence |

The focused checks include known geometry, wrong orientation, mistiming, unrelated-bone proximity, sparse/stale/missing evidence, bounded alignment, actual-montage backward scrubbing and a real-pair control. The control places the victim's torso at the attacker's hand at a declared time, passes, displaces the victim by 500 cm and fails, then restores the placement and passes with the same criterion. Preview and headless observations agree numerically for identical settings. This constructed contact is an instrument control, not an approved animation-quality reference.

After the full run, final review added explicit preview-clock ownership across world ticks, preserved centimetre units when root transform scale metadata differs, rejected collapsed contact segments, suppressed legacy proximity overlays/analytics in contact-profile mode, and included preview-selected weapon meshes in effective asset identity. The final focused suite passes 8/8 for these editor-only changes. The full suite was not repeated after this focused hardening; gameplay and recorder implementation were unchanged. The full suite's passing result is independent of the provisional asset-contact failures below: automation validates the instrument and gameplay contracts, while the profile runner evaluates the selected asset against its diagnostic criteria.

An initial native hash call hit Unreal's unimplemented Windows generic SHA-256 function. The subsequent passing run uses native SHA-1; the Python runner retains SHA-256 and supplies matching SHA-1 asset identities. The earlier failed run is retained in `paired-validation-tests.log` and is not counted as a pass.

Desktop automation initialization returned `Windows Computer Use Sky runtime is unavailable`. Shared preview sampling/evaluation was tested, but mouse interaction, button layout and the live Slate viewport were not visually verified. No asset-save or publishing action was performed.

## Hold/recovery observations

The first rendered matrix, `Saved/CombatScenarioRuns/20260910T220655-28cfcf3e/`, completed all eight gameplay scenarios successfully. Five pose evaluations passed eligibility; three exceeded the 75 ms pose-gap limit (ThirdPerson Forward, DefenseMatrix Backward and Left). DefenseMatrix Left also exposed an overstrict runner requirement for the shutdown banner despite exact test success and process exit 0. The runner now requires exactly one successful result for the requested test plus exit 0 and fresh artifact identity; wrong scope, duplicate results and failed exits are covered by offline tests.

Frame inspection exposed the fixed camera losing the player during a long follow-up. The hold camera now follows player translation, while keeping its initial angle and distance. Final rendered verification is recorded below. No sampling tolerance was relaxed to obtain a passing result.

Final rendered matrix: `Saved/CombatScenarioRuns/20260910T230304-9bbcd237/batch.json`. All eight automation processes exit 0 and all gameplay assertions pass. Three pose evaluations meet the criterion; five remain inconclusive. `rendered-hold-summary.json` in the evidence root records each capture path and result.

| Map | Direction | Pose eligibility |
| --- | --- | --- |
| ThirdPerson | Forward | Inconclusive: 86.408 ms maximum gap |
| ThirdPerson | Backward | Inconclusive: 83.741 ms maximum gap |
| ThirdPerson | Left | Pass |
| ThirdPerson | Right | Inconclusive: 81.172 ms maximum gap |
| DefenseMatrix | Forward | Inconclusive: 81.015 ms maximum gap |
| DefenseMatrix | Backward | Pass |
| DefenseMatrix | Left | Inconclusive: 78.487 ms maximum gap |
| DefenseMatrix | Right | Pass |

The batch exits 1 because the combined evidence result is inconclusive, despite passed gameplay. The threshold remains 75 ms. This verifies hold/recovery mechanics but does not establish a repeatable pose-quality envelope for every direction. Recovery/repress frames were reviewed in ThirdPerson Forward (`20260910T230334-BCE5B7B146D17906FA8CE9A6635FC255`, frame 80) and DefenseMatrix Backward (`20260910T230646-0BDC8617471DAEA192F958B9D4892CA6`, frame 78); the follow camera keeps the player in view in both.

## Finisher evaluation on both maps

Fresh completed-finisher captures pass gameplay and capture eligibility on both maps:

| Map | Capture directory under `Saved/CombatCaptures/` | Paired report under `Saved/PairedAnimationEvaluations/` |
| --- | --- | --- |
| ThirdPerson | `20260910T225531-5D9F3D454F6100106594D898384F37DD` | `20260910T231053-E92583814C68535F15559F994E4FEDEC` |
| DefenseMatrix | `20260910T225608-E93088204F8FA68BD1FEDD8D4BD6D97F` | `20260910T231105-4E401C7F4C78C663A172F0B30524A541` |

Runner batch: `Saved/CombatScenarioRuns/20260910T225501-9c8d8454/batch.json`, 2/2 pass. Each paired report's `report.html` links observations to frames; full measurements and identities are in `evaluation.json`.

These reports were regenerated after the preview-clock/geometry hardening and preserve the earlier case outcomes and approximately 44.91/44.93 cm runtime contact gaps. The subsequent change only adds preview-selected weapon meshes to asset identity; the focused suite verifies that report path on the final build.

DefenseMatrix uses `SKM_Manny_Simple`; ThirdPerson uses the mercenary mesh. The first attempt to use the mercenary profile on DefenseMatrix was correctly inconclusive because the captured mesh identity did not match. `finisher-contact-mannequin.json` declares the actual mannequin while retaining the same geometry and tolerances. No map or mesh asset was changed.

Both maps produce the same case outcomes:

| Case | Authored playback | Gameplay |
| --- | --- | --- |
| Blade to upper torso, 0.3–1.5 s | Fail | Fail |
| Entry root separation, 0.1–0.3 s | Fail | Pass |
| Release left hand, 1.8–2.3 s | Pass | Pass |
| Entry/contact/exit relative alignment | Requires gameplay comparison | Fail against provisional translation budget |

The minimum sampled signed blade-to-region gap is 34.64 cm in authored playback, 44.91 cm in ThirdPerson gameplay and 44.93 cm in DefenseMatrix gameplay. The diagnostic region has an 18 cm radius and allows a maximum 8 cm gap. These are explicit proxy measurements, not mesh-surface penetration measurements. The criteria were not loosened to make the existing pair pass.

ThirdPerson's relative-root translation deviations peak at 82.27 cm at entry, 89.15 cm during contact and 135.05 cm at exit, against a provisional 25 cm budget. Sampled victim timing error is zero; relative rotation error is negligible. This isolates a spatial discrepancy for review without attributing its cause to a particular warp, blend or asset setting.

ThirdPerson frame `frame_000020.png` is linked at the nearest sampled blade approach (pair time 0.6928 s, original simulation time 1.2243 s, frame offset zero). It shows both participants in the interaction and supports reviewing that interval. A screenshot alone does not establish the contact diagnosis.

## Counter reuse and remaining boundaries

The counter uses the same segment/18 cm torso proxy and 8 cm gap allowance, anchored to the attacker's authored `CounterImpact` notify. Report: `Saved/PairedAnimationEvaluations/20260910T231116-2DA8EEFD4B0F87E39E0B8185D3843413/`. Authored contact fails over 0.45–0.60 s, with minimum gap 165.76 cm. This demonstrates reuse without changing the contact tolerance or duplicating evaluator logic. Counter gameplay capture and full chain-stage evaluation were not part of this execution.

The criteria remain provisional review triggers. Artist-approved contact annotations, continuous surface collision, foot support/sliding, full AnimGraph parity, actual motion-warp correction, input-to-visible-response latency and artistic/audio/haptic feel are not established. A plugin split, candidate ranking and learned perception scoring remain deferred.

The next gameplay/authoring step is to inspect the finisher's declared blade-contact interval and placement together with its montage sync events, choose the intended contact model, and verify any asset/runtime adjustment with fresh captures against the same justified criteria. Do not promote a proxy failure to an asset fix without that review.

## Changed files and final review

Task changes are in editor analysis/capture and preview APIs, `PairedAnimationEvaluationCommandlet`, the capture scenario/recorder tests, `PairedContactEvaluationTests.cpp`, `Tools/CombatCapture` runners/evaluators/profiles/tests, and the linked guide, plan, test README and handoff. The runtime changes and other editor/asset changes already present in Git status belong to earlier work. Nothing was staged or committed.

Scoped `git diff --check` passes. New C++ files were checked for trailing whitespace; all protected Content hashes/absence match and there are no additional changed Content paths. Existing LF/CRLF warnings remain outside this slice. Generated evidence stays under `Saved/`.
