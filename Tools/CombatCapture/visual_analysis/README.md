# Visual analysis library

Compatibility imports for existing project commands. Implementations belong to the separate [AnimationAnalysis dependency](../../AnimationAnalysis/README.md). Run its setup command first. This directory resolves the pinned package and delegates to it; it contains no independent measurement implementation. The profile under `profiles/` remains Katana integration data and is excluded from the portable distribution.

The [suite architecture contract](../../../docs/architecture/ANIMATION_ANALYSIS_SUITE.md) applies to all existing capture/evaluation commands, native tooling, reports, profiles and tests. UE projection/pose conventions and historical evidence clocks have explicit compatibility adapters inside `animation_analysis.adapters`; portable contracts and measurements do not import them. The four visual review/surface review/segment calibration/detection commands delegate to package services. Native capture is also extracted; project evaluation/launching, preview integration and generalized retention remain migration work.

## API

| API | Purpose |
|---|---|
| `project(point, frame)` | Project a world point with the captured UE row-vector matrix and constrained viewport; reject invalid or behind-camera geometry. |
| `linked_actor(sample, frame, role)` | Require matching observed pose serial and engine frame for a named participant. |
| `segment_gap(start, end, target, radius)` | Signed finite-segment/sphere distance. This is geometry, not a visible-contact detector. |
| `load_evidence(path)` | Read a portable page's `visual-evidence` JSON block without executing scripts; verify unique frame identifiers, timestamps and embedded PNG hashes. |
| `validate_review(review, evidence, evidence_sha256)` | Bind visual findings to exact evidence and reviewed frames; validate reviewer provenance and explicit uncertainty. |
| `render_review(review, evidence, summary, evidence_link)` | Generate a visual-findings page linking each observation to its retained pixels and observed clock. |
| `pixel_alignment.analyze_segment(rgb, width, height, region, expected_segment, visibility, settings)` | Fit a coloured line from packed RGB pixels within a reviewed region, then compare it with a projected segment; abstain when visibility or fit is insufficient. |
| `images.decode_frame(frame, expected_size)` | Decode an embedded PNG in memory, checking recorded and profile dimensions; requires Pillow. |
| `surface_evidence.load_surface_bundle(directory)` | Validate a bounded native raster/depth bundle and return channels plus input hashes. |
| `surfaces.measure_surface_relation(labels, scene_depth_cm, label_depth_cm, width, height, first_id, second_id, visibility_tolerance_cm)` | Measure visible pixel-centre separation and observed-label occlusion; no contact verdict. |

Consumers beside this package can use `from visual_analysis import ...`. Other repository Python tools can add `Tools/CombatCapture` to their import path. Capture adapters should populate the evidence contract; domain-specific detectors should consume it rather than assuming finisher bone names or scenario identifiers in the shared package.

Independent consumers install `Tools/AnimationAnalysis` and import `animation_analysis` directly, without this compatibility directory. Canonical evidence uses schema 2 with `time: {"domain": "...", "seconds": 1.25}`. Existing `visual_analysis` imports and CLI entry points continue to accept historical montage/simulation-clock records; normalization preserves the original input bytes and hash. Core import, geometry and review do not require Pillow or Unreal.

## Create a visual review

First produce a portable pixel/observation page, currently with `review_contact.py`. Review the raw images and then their overlays. Author a findings JSON bound to that page's SHA256:

```json
{
  "schema_version": 1,
  "title": "Animation transition visual review",
  "evidence_sha256": "SHA256 of the exact portable HTML page",
  "reviewer": {"kind": "human", "identity": "Reviewer name"},
  "summary": "Describe the reviewed outcome and its limits.",
  "reviewed_frames": ["frames/frame_000012.png"],
  "findings": [{
    "id": "transition_alignment",
    "category": "Transition alignment",
    "assessment": "indeterminate",
    "basis": "pixels",
    "observation": "Describe what is visible in the referenced frame.",
    "interpretation": "Explain what this does and does not establish.",
    "limits": "The character is partly occluded at the transition.",
    "next_action": "Capture a view exposing both sides of the transition.",
    "evidence": ["frames/frame_000012.png"]
  }]
}
```

Replace the hash/frame references with actual retained evidence. `reviewed_frames` records images actually inspected, not all available images by default. Then publish:

```powershell
python Tools/CombatCapture/review_visual.py --evidence Saved/Logs/<review>/contact.html --findings Saved/Logs/<review>/findings.json --output Saved/Logs/<review>/visual-analysis.html
```

The output is a separate visual-analysis HTML/JSON pair. It returns `review_recorded` when validation succeeds; concerns and indeterminate observations remain visible and are not converted to a quality pass. Bad inputs replace previous success with an incomplete result. Keep the portable evidence page beside the report: frame links use its slider and do not depend on bulk PNGs still existing.

## Contract and boundaries

- Assessments are `consistent`, `concern`, `indeterminate` and `not_reviewed`. There is no implicit aggregate score. A `consistent` finding applies only to its cited evidence and stated scope.
- Each finding distinguishes `pixels` from `pixels_and_telemetry`. Unreviewed aspects use `not_reviewed`, empty evidence and an explanation. Frame references must belong to the declared reviewed subset.
- Reviewer kinds are `human`, `assistant_image_review` and `automated_detector`. Detector provenance additionally requires a version and calibration reference. The library checks that provenance is present; it does not certify the referenced calibration or the reviewer's judgment.
- The portable adapter requires exactly one `<script id="visual-evidence" type="application/json">` block. Its `frames` contain unique `file`, finite `montage_time_s` or `simulation_time_s`, `image_sha256` and embedded PNG `image` fields. Additional pose, camera, source and criterion fields are preserved by consumers. Hash verification does not replace the capture analyzer's PNG integrity and timing checks.
- A 2D overlap is not depth contact. Occlusion, missing viewpoints, unknown anatomical intent, sampling gaps and unreviewed intervals must remain explicit. Geometry and automation results do not establish visible quality.

The first real consumer is the [finisher visual analysis](../../../docs/audits/FINISHER_VISUAL_CONTACT_2026-09-10.md). A non-contact locomotion/camera fixture tests the same schema. Future support/sliding, transitions and impact-readability detectors should enter through this contract with task-specific calibration and known-defect controls. The segment detector below is the first bounded pixel measurement; no model service or universal feel score is implemented.

## Measure visible segment alignment

Install the optional decoder in the Python environment used for these commands:

```powershell
python -m pip install -r Tools/CombatCapture/requirements-vision.txt
```

The experimental `profiles/cyan-segment-alignment.json` profile targets reviewed, unobscured cyan segments in 960x540 images. A reviewer identifies the subject, freezes its pixel region and records visibility. The detector selects colour components and fits the dominant line using pixel coordinates before comparing telemetry. It rejects fragmented/competing components, clipped regions and insufficient line fits. It checks perpendicular error and longitudinal overlap separately. It does not recognize the subject or determine occlusion automatically.

Create a regions JSON for each portable evidence page:

```json
{
  "schema_version": 1,
  "evidence_sha256": "SHA256 of the exact portable HTML page",
  "reviewer": "Reviewer identity",
  "frames": [{
    "file": "frames/frame_000012.png",
    "region_px": [480, 140, 550, 260],
    "visibility": "visible",
    "basis": "Describe how the intended segment and visibility were established.",
    "expected_assessment": "consistent",
    "stale_reference_frame": "frames/frame_000025.png"
  }]
}
```

Region bounds are integer `[left, top, right, bottom]`, with right/bottom excluded. Replace the example with reviewed coordinates and evidence references. Visibility is `visible`, `occluded` or `unknown`. Calibration additionally requires a reviewed baseline label (`consistent` or `indeterminate`) and, for consistent cases, a stale reference from a visibly different pose in the same page. Detection ignores these calibration-only fields. Do not move the region in response to injected metadata errors.

Create a calibration manifest with distinct images in both cohorts; paths resolve relative to the manifest:

```json
{
  "schema_version": 1,
  "basis": "Describe the reviewed conditions, labels and any prior inspection of validation images.",
  "datasets": [
    {"evidence": "calibration.html", "regions": "calibration-regions.json", "cohort": "calibration"},
    {"evidence": "validation.html", "regions": "validation-regions.json", "cohort": "validation"}
  ]
}
```

```powershell
python Tools/CombatCapture/calibrate_segment_alignment.py --manifest Saved/Logs/<review>/manifest.json --profile Tools/CombatCapture/visual_analysis/profiles/cyan-segment-alignment.json --output Saved/Logs/<review>/calibration.json
python Tools/CombatCapture/detect_segment_alignment.py --evidence Saved/Logs/<review>/contact.html --regions Saved/Logs/<review>/regions.json --profile Tools/CombatCapture/visual_analysis/profiles/cyan-segment-alignment.json --calibration Saved/Logs/<review>/calibration.json --output Saved/Logs/<review>/segment-analysis.html
```

Calibration replays labels, perpendicular offsets of +/-8 and +/-16 pixels, a 100-pixel longitudinal offset, a labelled stale-pose segment, and unknown/occluded visibility controls. These perturbations affect only the comparison argument in memory; source images, telemetry and regions remain intact. Identical image hashes cannot be counted twice or across cohorts. The output binds the profile, input hashes and measurement/decoder implementation (including Pillow version). A failed calibration cannot publish detector findings. Recalibrate after changing the profile or measurement implementation.

Detection produces measurements and a separate visual-findings HTML/JSON pair, with raw-pixel and fitted-line views embedded in HTML. No derivative PNG files are needed. `review_recorded` means findings were written, including any concerns or abstentions. Successful controls establish only the declared instrument conditions: they do not establish general accuracy, temporal synchronization of an unchanged pose, skinned-surface contact or artistic approval. For the initial real corpus and fresh camera validation, see the [segment alignment audit](../../../docs/audits/VISUAL_SEGMENT_ALIGNMENT_2026-09-11.md).

## Verification

The next consumer is [viewport surface analysis](../../../docs/guides/VIEWPORT_SURFACE_ANALYSIS.md). Its native adapter and pure raster measurements have separate ownership. The first calibration fixture uses engine primitives; its names and expected geometry do not enter the shared library. `review_surfaces.py` emits the same portable evidence contract and embeds images without a PNG export directory.

```powershell
python -m unittest discover -s Tools/CombatCapture -p 'test_*.py' -v
```
