# Viewport surface analysis implementation and controls

The first coarse mesh measurement layer is implemented as separate renderer, data-adapter and pure-analysis modules. It accepts nominated primitives and arbitrary subject IDs; it has no finisher, bone, map or test-name branches. Five known-geometry controls now have aligned RGB, labels and depth, and a portable review.

## What changed

- `Source/KatanaCombatEditor/Public/Analysis/ViewportSurfaceCapture.h` and its private implementation: bounded viewport observations plus separately scoped primitive labels. They contain no combat-class imports or asset paths.
- `KatanaCombatEditor.Build.cs`: public renderer interfaces are consumed through private `Renderer`, `RenderCore` and `RHI` dependencies; no engine-private include paths or custom shaders.
- `Source/KatanaCombatTest/Private/ViewportSurfaceCaptureTests.cpp`: label ownership/restoration and rendered geometry controls using engine primitives. Expected layouts belong to this fixture, not the capture or measurement implementation.
- `Tools/CombatCapture/visual_analysis/surfaces.py`: pure visible pixel-centre separation and observed-label visibility, with an exact distance transform rather than an exhaustive boundary-pair scan.
- `visual_analysis/surface_evidence.py`: bounded binary/manifest validation and input hashes.
- `review_surfaces.py`: reusable portable RGB/label review and measurement report, compatible with the existing visual-evidence reader.
- `test_surface_analysis.py` and `test_surface_evidence.py`: eleven new offline tests, including unrelated IDs/resolutions, brute-force distance references, occlusion/missing evidence, invalid depth/clocks, stale frames and overwrite protection.

See the [usage guide](../guides/VIEWPORT_SURFACE_ANALYSIS.md) and [ongoing plan](../plans/VISUAL_ANALYSIS_LIBRARY.md).

## Known geometry and independent RGB checks

Final bundle: `Saved/SurfaceObservations/A0BF0DA94E7152A32CD7FE966BB069B5/`. Evidence root: `Saved/Logs/SurfaceContact-20260911-003734/`.

| Authored control | Measurement | Interpretation |
|---|---|---|
| Separated solids | 66 px between nearest visible pixel centres | Visible separation in this view |
| Touching solids | 1 px | Adjacent silhouettes; no automatic contact approval |
| Intersecting solids | 1 px | Adjacency does not distinguish intersection from touching |
| Second solid behind the first labelled solid | Second label absent; indeterminate distance | Frontmost custom depth cannot recover the hidden labelled surface |
| Second solid behind an ordinary scene occluder | Label present, zero visible pixels; indeterminate distance | Scene depth identifies occlusion of the observed label |

The native fixture checks a known front plane at 350 cm and rejects mismatched renderer/RGB frame identity, a different viewport and duplicate pending requests. All five saved observations retain their originating renderer frame and simulation time. Controls do not rely on Katana animations or contact tolerances.

An independent RGB foreground check uses the explicitly declared black background and visible unlit cube material. For the four controls without the ordinary scene occluder, combined RGB/label silhouette IoU is **1.0**. The fixed acceptance floor remained 0.99. This checks the combined visible outline, not independent identity of touching same-material subjects. The fifth control is inspected visually and classified through depth. All five raw/overlay pairs were visually inspected.

An earlier lit control had IoU 0.971893 because the second cube's side was entirely black against the black background. Thresholds from 1 through 48 all missed the same 2,614 pixels. This was insufficient RGB contrast, not demonstrated telemetry desynchronization. The fixture now explicitly selects unlit view mode; changing only the lighting show flag was overridden by the viewport mode. `rgb-control-initial.json` preserves the diagnosis. No measurement tolerance was relaxed.

The controlled script and results are `check_surface_controls.py` and `surface-controls.json` under the evidence root. Their foreground assumption is specific to this calibration fixture; it does not enter the reusable analysis API.

- [Portable RGB and label review](../../Saved/Logs/SurfaceContact-20260911-003734/surface-review.html)
- [Independent visual findings](../../Saved/Logs/SurfaceContact-20260911-003734/surface-visual-analysis.html)
- [Control results](../../Saved/Logs/SurfaceContact-20260911-003734/surface-controls.json)

These Saved links require the retained local evidence bundle. Repository source and the guide contain the reusable implementation and replay contract.

## Verification and scope

- Fresh editor build succeeded: `build.log`.
- **72/72 Python tests passed**: `offline-tests.log`. The eleven surface tests were also rerun after the final viewer-only navigation adjustment: `surface-tests-final.log`.
- **Two native surface automation tests passed in the rendered run**, including the five known-geometry observations: `automation-unlit.log`; command and process arguments in `final-command.json`.
- The `KatanaCombat.Capture` NullRHI regression run completed 22 results: **21 exercised checks passed**, while the rendered-only surface test reported its explicit headless skip. This run preceded the final fixture-only view-mode adjustment; the final rendered rerun covers that adjustment. Log: `automation-capture.log`.
- The full combat suite was not rerun: no combat behavior or continuous recorder code changed. All 7,948 Content files retain their starting size and modification time; the 54 previously protected asset paths also retain their SHA256 hashes or missing-file state. `preservation.json` records these checks and the nine changed source/tool files.

The adapter currently supports D3D11 D32F/S8, a single perspective view with no AA, and bounded full-resolution observations. The explicit diagnostic option supplies a view-family resolution driver; it does not silently reinterpret scaled images. UE's logical depth byte count differs from physical staging layout, so raw decoding is guarded by the verified backend/format. Other paths reject.

Measured depth/label readback cost was **12.38-13.47 ms** per 640x480 observation. This excludes total rendering, RGB screenshot and export cost. It is a synchronous diagnostic implementation, separate from the continuous capture recorder. It does not establish real-time capture cadence, moving skeletal pose alignment, arbitrary materials, temporal persistence, full hidden silhouettes, penetration volume or artistic quality.

No assets were saved. Reviews embed images; one compact PNG sheet is retained. Fifteen superseded raw observations were archived and verified byte-for-byte before recycling, with manifests and retention notices preserved. `scratch-retention.json` records the exact paths and hashes; the archive is approximately 1.49 MB. The final five binary observations remain available for numeric replay.

## Next integration

Connect explicitly nominated body/weapon primitives to the existing participant/pose contract. Preserve renderer settings in capture identity, add bounded asynchronous collection and test stale-pose/resolution/material changes before continuous use. Validate on a moving pair and another interaction. The finisher's source choreography, facing and grip investigation remains open; these generic controls do not establish its intended contact.
