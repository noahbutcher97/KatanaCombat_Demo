# Finisher visual contact cross-check — 2026-09-10

Visual contact must be checked before tuning paired placement against a geometric report. The fresh review exposes an anatomical interpretation problem: the diagnostic `BladeToUpperTorso` criterion uses an 18 cm sphere at `spine_03`, which projects around the abdomen in these poses. Chest (`spine_05`) and neck (`neck_01`) observations are visibly higher. This does not establish a faulty socket transform or authorize replacing the contact criterion.

The unobscured weapon poses show no obvious pixel/endpoint mismatch in the inspected frames. During approximately 0.56–0.92 seconds, the two bodies and arms hide portions of the attacking blade even from opposite sides. **Visible blade-to-victim contact remains indeterminate.** A projected overlay through a body is not visible contact, and a positive sampled 3D separation is not continuous proof of absence between poses.

## Evidence and interpretation

Four fresh rendered `Completed` scenarios passed: default and opposite cameras on ThirdPerson and DefenseMatrix. Both views used the same transient `paired-warp-tuning` control: movement allowed on both paired notifies, attacker translation warping disabled to match the pair configuration, original effective victim warp window `0.0001..0.872983634`, and victim relative offset `(50,0,0)` cm. Original notify objects and package dirty state were restored. These recordings show a diagnostic control, not newly saved asset behavior.

The portable reviews embed 24 selected original images each from montage interval `0.25..1.5` seconds. All 96 selected raw frames were inspected in sequence sheets. Six raw/projected frame pairs from each view/map were also inspected around 0.3, 0.43, 0.57, 0.67, 0.9 and 1.3 seconds. Requested times resolve to each run's actual observed montage times; the cameras are separate runs, not simultaneous stereo views.

| Map | Default view | Opposite view |
|---|---|---|
| ThirdPerson | [Portable review](../../Saved/Logs/FinisherPlacementReview-20260910-231757/ThirdPerson-default-contact.html) | [Portable review](../../Saved/Logs/FinisherPlacementReview-20260910-231757/ThirdPerson-opposite-contact.html) |
| DefenseMatrix | [Portable review](../../Saved/Logs/FinisherPlacementReview-20260910-231757/DefenseMatrix-default-contact.html) | [Portable review](../../Saved/Logs/FinisherPlacementReview-20260910-231757/DefenseMatrix-opposite-contact.html) |

These links point to generated local evidence. The tracked [guide](../guides/PAIRED_ANIMATION_EVALUATION.md#cross-check-visible-contact-before-tuning) contains reproduction commands for another workspace.

## Separate visual analysis

Dedicated findings are published through the [shared library](../../Tools/CombatCapture/visual_analysis/README.md), independently of capture and geometric outcomes:

- [ThirdPerson default visual analysis](../../Saved/Logs/FinisherPlacementReview-20260910-231757/ThirdPerson-default-contact-visual-analysis.html)
- [ThirdPerson opposite visual analysis](../../Saved/Logs/FinisherPlacementReview-20260910-231757/ThirdPerson-opposite-contact-visual-analysis.html)
- [DefenseMatrix default visual analysis](../../Saved/Logs/FinisherPlacementReview-20260910-231757/DefenseMatrix-default-contact-visual-analysis.html)
- [DefenseMatrix opposite visual analysis](../../Saved/Logs/FinisherPlacementReview-20260910-231757/DefenseMatrix-opposite-contact-visual-analysis.html)

Each analysis has eight evidence-linked dispositions: anatomical target and contact visibility are concerns; unobscured weapon/telemetry agreement is consistent within the inspected scope; blade contact, intended paired alignment, surface penetration and impact timing are indeterminate; foot support and full recovery are not reviewed. The reviewer method is explicitly `assistant_image_review`. These are authored visual judgments bound to the exact retained pixel page, not outputs of an automatic defect detector. The library rejects stale evidence, unreviewed frame references and attempts to use an automation `pass` as a visual verdict.

## Measurement cross-check

The new review requires exact frame/sample pose serial and engine-frame agreement for both roles. It uses the engine's recorded post-draw projection matrix and constrained view rectangle rather than assuming camera FOV/aspect. All 96 selected embedded frames passed those linkage checks. Camera provenance and point source identity aid a visual cross-check; they do not establish skinned-surface intersection or renderer depth visibility.

Native runtime contact evaluation completed as `measured` on both opposite-view captures. It still fails the original blade criterion: minimum signed segment/sphere gaps were **38.914 cm** on ThirdPerson and **39.035 cm** on DefenseMatrix. Independent review geometry agrees with the native observations within `1e-6` cm. Entry root separation passes; release-hand separation and relative-alignment budgets still fail. This is a measured failure of provisional criteria, not a visual-quality verdict.

As an anatomical diagnostic only, applying the same 18 cm radius to additional observed points gives minimum gaps across the four recordings of approximately 21.8 cm at `spine_05` and 16.9 cm at `neck_01`. Moving the proxy upward alone does not produce observed overlap. These are not calibrated chest/neck collision shapes and were not added as acceptance tests.

The preceding fresh authored-pose grid covered 120 yaw/XY placements. One quarter-turn placement passed all three geometric criteria, while other orientations could reduce blade distance but violate release separation. That result is retained in `placement-grid.json` as a diagnostic. No candidate was selected or promoted: optimizing against an anatomically misleading target could create a geometric pass with incorrect choreography. Native confirmation of that candidate was deferred for this reason.

## Reusable changes

- `run_scenario.py --camera-view` selects a named preset from the registered scenario. Unknown presets fail before launch; requested and returned view names must agree.
- Finisher scenario version 3 adds the opposite camera and chest/neck observation points. Existing contact thresholds are unchanged.
- `CombatCaptureScenarioTests.cpp` applies and records the selected view, including the camera tracking path used by hold recovery.
- `CombatCaptureSession.cpp` records FOV, world-to-clip matrix and constrained viewport after draw.
- `review_contact.py` provides a portable raw/projected frame slider and hashed JSON manifest with configurable roles, montage interval, weapon endpoints, target sphere and anatomical probes. It reports `review_required`, never automatic visible-contact success. Failed generation replaces prior output with an incomplete status.
- Capture and paired-evaluation guides now require visual contact/occlusion and pixel/telemetry agreement to be recorded separately before tuning.

## Verification and retention

- Editor build succeeded; no runtime combat implementation was changed by this follow-up.
- `Automation RunTests KatanaCombat.Capture;Quit` under NullRHI: **20/20 passed**, zero automation errors, process exit 0. The broader 776-test suite was not repeated because this follow-up changes observation tooling, not combat behavior.
- Four current rendered camera scenarios passed; two earlier single-view captures also passed before the camera/projection additions.
- Offline Python automation: **49/49 passed** after extracting the reusable visual-analysis library. Coverage includes projection/linkage, finite-segment distance, a non-contact review, changed/swapped evidence, invalid verdicts, malformed reviews, detector provenance and rejected re-publication replacing stale success.
- Unknown camera control rejected before Unreal launch. Four generated review scripts parsed successfully and all 96 embedded PNG hashes matched the selected originals.
- Both native contact runs exited 0 and produced measured reports; the Python runner exited 1 because the configured geometric criteria fail, as described above.
- **850 PNG exports were checked before cleanup; 846 were recycled and four raw key frames retained.** Four portable pages retain selected pixels, alongside compact contact sheets. Original telemetry, image-export records, source/asset identities and native reports remain. Each pruned capture has `retention.json` and a visible report notice; full image analysis requires restoring frames or recapturing.

Evidence root: `Saved/Logs/FinisherPlacementReview-20260910-231757/`. See `two-view-review.json`, `anatomical-probe-diagnostics.json`, `contact-results.json`, `capture-tests-summary.txt`, `review-pages-check.log` and `cleanup-result.json` for exact paths, clocks and results.

## Disposition

Keep visual checking ahead of placement optimization. The next authoring step is to review the unwarped source pair's choreography and intended relative facing, identify the intended blade contact region, then compare that configuration in gameplay using these same raw/projected views. Keep the provisional criteria unchanged until that interpretation is established. Do not save the quarter-turn grid candidate, infer torso contact from damage, or present an occluded frame as an artistic pass.

No animation assets, contact-profile thresholds, commits or staging were changed. The original protected Content snapshot is checked again in `final-checks.json`.
