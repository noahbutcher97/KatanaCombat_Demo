# Visible segment alignment and finisher contact review

The first pixel-based instrument is implemented in the shared visual-analysis library. It measures a visible coloured segment inside an explicitly reviewed region, then compares that pixel fit with projected telemetry. New elevated finisher images also support apparent blade/body separation in sampled strike poses. These are separate findings: the detector abstains on the partly occluded contact frames.

## Implementation

- `Tools/CombatCapture/visual_analysis/pixel_alignment.py`: colour components, thin-line fitting, ambiguity/visibility rejection, transverse error and longitudinal overlap.
- `visual_analysis/images.py` and `requirements-vision.txt`: optional in-memory Pillow decoding with profile/recorded image-size checks. The core evidence/review APIs remain standard-library code.
- `calibrate_segment_alignment.py`: repeatable reviewed-image cohorts and fixed metadata/visibility controls, with profile, input and implementation identity.
- `detect_segment_alignment.py`: calibration-bound measurement and separate evidence-linked visual findings, including embedded raw/fit previews without derivative PNGs.
- `visual_analysis/profiles/cyan-segment-alignment.json`: experimental 960x540 cyan-segment profile. It is not a general material/view calibration or a contact criterion.
- `test_pixel_alignment.py` and `test_segment_calibration.py`: twelve additional offline tests, including rejected evidence, stale implementation, mismatched resolution, duplicate images and output/input collision protection.
- `scenarios/finisher-recovery.json` version 4: `contact_oblique` and `contact_elevated` views. No gameplay, asset or contact-threshold changes.

See the [library API and replay instructions](../../Tools/CombatCapture/visual_analysis/README.md) and [implementation plan](../plans/VISUAL_ANALYSIS_LIBRARY.md).

## Calibration and fresh validation

Evidence is under `Saved/Logs/VisualAlignment-20260911-000110/`. `segment-calibration-manifest.json` references the prior portable default/opposite pages and reviewed regions. Regions were seeded from the earlier projected segment bounds, visually checked and frozen during perturbations. The fitted line is measured from pixels; subject localization and visibility remain reviewed inputs.

The default cohort contains six images (five measurable, one abstention); the opposite cohort contains six (four measurable, two abstentions). Opposite images were inspected during feasibility, so this is cross-view validation, not a blind holdout. Ambiguous colour cases include fragmented blade pixels and competing background colour; they are not all additional weapons.

| Control | Cases | Result |
|---|---:|---|
| Unaltered reviewed baselines | 12 | Nine consistent, three indeterminate, all match labels |
| Perpendicular offsets of -16, -8, +8, +16 pixels | 36 | All concerns |
| Longitudinal offset of 100 pixels | 9 | All concerns |
| Explicitly labelled different-pose telemetry | 9 | All concerns |
| Unknown or occluded visibility inputs | 24 | All indeterminate |
| Total | 90 | Zero mismatches |

No source pixels or telemetry were modified to create controls. Successful replay binds the exact profile, measurement/decoder files, Pillow version and input hashes. It does not establish general detector accuracy or detect stale timestamps when the image pose is unchanged.

The numeric profile was then used unchanged on four fresh camera recordings. Of twelve reviewed opening-blade images, nine were consistent, with maximum perpendicular error at most **1.84 pixels**. Three abstained because their fitted line width exceeded the 2-pixel RMS limit (approximately 2.10, 2.09 and 2.03 pixels). All four partly occluded contact images abstained. No thresholds were relaxed to admit them. `fresh-detector-runs.json` identifies all four reports.

## Separate visual contact finding

The oblique views still foreshorten the blade and hide its base/grip near the strike. The elevated views expose the distal blade extending across/past the attacker's side, away from the victim torso, in sampled held poses around 0.58, 0.64-0.68 and 0.88-0.91 seconds of the attacker montage. This supports apparent separation in those observations, consistent with the existing spine-sphere geometric concern. It does not prove continuous absence of contact, the exact intended strike, or the cause of the placement/orientation issue.

The provisional `spine_03` target still projects around the abdomen, below `spine_05` and `neck_01`. Its older upper-torso wording does not establish anatomical intent. Feet, audio and overall feel were not evaluated here.

Each new portable page embeds 24 selected original frames; each separate visual review declares the eight frames actually inspected for its findings. Camera views are separate runs with their own observed montage clocks, not simultaneous stereo evidence.

- [ThirdPerson elevated visual findings](../../Saved/Logs/VisualAlignment-20260911-000110/ThirdPerson-elevated-contact-visual-analysis.html)
- [DefenseMatrix elevated visual findings](../../Saved/Logs/VisualAlignment-20260911-000110/DefenseMatrix-elevated-contact-visual-analysis.html)
- [ThirdPerson elevated pixel detector](../../Saved/Logs/VisualAlignment-20260911-000110/ThirdPerson-elevated-contact-detector.html)
- [Calibration controls](../../Saved/Logs/VisualAlignment-20260911-000110/segment-calibration.json)

Saved evidence is local and ignored by Git; these links require the retained bundle. The source, profile, schemas and replay commands are repository tooling.

## Verification and retention

- `KatanaCombatEditor Win64 Development`: succeeded; target up to date. Build log: `Saved/CombatScenarioRuns/20260911T040749-23ab90bf/build.log`.
- `python -m unittest discover -s Tools/CombatCapture -p 'test_*.py' -v`: **61/61 passed**; `offline-tests.log` records the run.
- Four rendered `Completed` scenarios: **4/4 passed**, both maps in each new camera view. Batch records: `20260911T040749-23ab90bf` and `20260911T041057-f70b1e1d` under `Saved/CombatScenarioRuns`.
- Each used the same transient diagnostic control: `--finisher-experiment paired-warp-tuning --victim-warp-window .0001 .872983634 --victim-warp-offset 50 0 0`. Runtime overrides are recorded in each session; this is not a claim about untouched default gameplay.
- Before pruning, verified all 96 embedded frame hashes against source PNGs, review/detector bindings, complete exports and current calibration identity. All **7,948 Content files** matched the starting size/mtime snapshot; all **54 protected asset entries** matched their prior SHA256/missing state. Runtime/editor/test C++, Config and project file hashes were unchanged this turn.
- Recycled **557 of 561** bulk frame PNGs, retaining one keyframe per capture and the 96 embedded frames. Retained four compact contact sheets; recycled two temporary ROI grids. Each capture has `retention.json`, `RETENTION.md` and an HTML notice. Full export results are historical after pruning; restore recycled frames or recapture before a full pixel re-analysis.

No assets were saved. The full combat suite and native paired evaluator were not rerun for these Python/camera changes; earlier results are not represented as fresh verification. A passing rendered scenario establishes its configured integration assertions, not visible contact or animation quality.

## Next work

Inspect the unwarped source pair, relative facing and weapon attachment/grip orientation before choosing a warp or placement correction. In parallel with that investigation, the plan now includes a **coarse mesh segmentation/contact layer**: aligned participant/weapon masks and depth, visible gap/occlusion measurements, independent RGB checks and known separation/contact/intersection controls. That layer is planned, not implemented in this change. It must distinguish screen overlap, depth-bounded proximity and unobservable surfaces rather than treating silhouette overlap as contact proof.
