# Viewport surface observations

This bounded diagnostic adapter captures RGB, frontmost custom-depth labels, scene depth and label depth from one viewport draw. Pure Python functions measure visible pixel separation and observed-label occlusion. These measurements do not establish skinned-mesh contact or penetration.

Python implementation now lives in the standalone [animation-analysis package](../../Tools/AnimationAnalysis/README.md): `animation_analysis.surfaces`, `animation_analysis.adapters.unreal_surface` and `animation_analysis.jobs.surface_review`. Existing `visual_analysis` imports and `review_surfaces.py` remain compatibility delegates. Newly published portable HTML uses canonical evidence schema 2 with an explicit clock domain; the native `SURFACE1` bundle format and numeric measurements are unchanged.

## Modular boundaries

| Layer | Responsibility |
|---|---|
| `FScopedSurfaceCaptureLabels` | Apply caller-specified primitive IDs/names; reject ownership/ID collisions; restore custom-depth flags, stencil values and write masks. |
| `FViewportSurfaceCapture` | One requested renderer observation with strict viewport/frame/resolution checks, bounded raw readback and projection identity. No combat classes, asset lookup or file writes. |
| `visual_analysis.surface_evidence` | Validate binary/JSON identity, clocks, subjects, sizes and paths; hash every input. |
| `visual_analysis.surfaces` | Pure raster/depth measurements with explicit subject IDs and visibility tolerance. No maps, bones, filenames or control-case names. |
| `review_surfaces.py` | Publish measurements and portable RGB/label evidence. No artistic acceptance criteria. |
| Caller or calibration fixture | Choose subjects, rendering policy, observation times, criteria and export location. |

The first producer uses engine cubes in `/Engine/Maps/Entry`, without Katana asset references in the fixture. The library and adapter remain in this repository until a second project validates their extraction boundary.

## Native adapter

Declare unique `FSurfaceCaptureLabel` entries with nonzero byte IDs, names and registered primitives in one world. The scope validates all entries before mutation. Duplicate ownership and ID collisions with other custom-depth primitives reject. `Restore()` and destruction restore original component settings, including after a nominated component is destroyed. The caller owns any `r.CustomDepth` change and must restore it; the adapter requires value 3.

Construct `FViewportSurfaceCapture(World, Viewport, bUseDiagnosticResolution)`, call `Request()` before a draw and call `Collect()` from `UGameViewportClient::OnViewportRendered`, passing that draw's `GFrameCounter`. Only one request may be pending. Successful collection returns RGB and surface arrays with matching renderer frame identity and dimensions. Wrong viewports, stale frames, unsupported rendering and readback failures cannot produce success.

The initial backend is **D3D11 D32F/S8**, one full-frame perspective view, no AA and at most 1920x1080 pixels. Other RHIs, D24, split/stereo views and unsupported scaling reject. Raw staging data has an eight-byte layout although UE reports five logical format bytes. Depth conversion uses the captured view's inverse-device-Z parameters; generic screenshot depth normalization is unsuitable for contact distances.

The explicit diagnostic-resolution option installs a full-resolution driver on the requested view family. It affects that diagnostic RGB image too, without changing a CVar, viewport setting or asset. PIE otherwise overrides the client's screen-percentage flag. AA and view mode remain caller-owned and must be restored by the caller. The calibration fixture deliberately uses unlit view mode so a cube side cannot disappear into the black background.

Readback currently blocks for two bounded GPU copies. `ReadbackSeconds` measures those copies, excluding total rendering, screenshot and export cost. The adapter remains separate from the continuous RGB recorder. Queued readback, skeletal pose linkage and overhead need validation before enabling continuous animation observations.

## Bundle and replay

`surfaces.json` declares schema version 1, backend/render policy, `depth_convention: camera_axis_cm_clear_infinity`, `label_semantics: frontmost_custom_depth`, a name-to-byte-ID `subjects` registry and 1..60 ordered `frames`. Each frame records a direct `.surface` filename, engine frame, simulation time, width/height and 16 row-major world-to-clip values. Additional producer metadata is preserved.

Each little-endian binary has the eight-byte magic `SURFACE1`, a uint64 renderer frame, two uint32 dimensions, then row-major BGRA8 pixels, uint8 labels, float32 scene depth and float32 label depth. Positive infinity denotes clear depth; finite depths are positive camera-axis distances in cm. The loader rejects mismatched identity/dimensions, malformed lengths, unsafe paths, undeclared IDs, invalid clocks and duplicate render frames. It does not infer synchronization for independently assembled channels.

```powershell
python Tools/CombatCapture/review_surfaces.py --capture Saved/SurfaceObservations/<recording> --first FirstSolid --second SecondSolid --visibility-tolerance-cm 1 --output Saved/Logs/<review>/surface-review.html
python -m unittest discover -s Tools/CombatCapture -p 'test_surface*.py' -v
```

Output must be outside the input bundle. The HTML embeds original RGB, label overlays and the shared `visual-evidence` contract; use `review_visual.py` for separate reviewed findings. PNGs are generated in memory. Keep binary bundles for numeric replay. JSON reports bind input and measurement-implementation hashes. `measurement_recorded` means results were written, including indeterminate cases.

## Interpretation

- Pixel-centre distance is Euclidean screen-space distance. Adjacent pixels are one pixel apart; this is not a subpixel mesh boundary or world-space gap.
- Observed-label visibility describes only the frontmost labelled surface. A subject hidden by another labelled object is missing, so full-silhouette coverage and projected overlap cannot be inferred.
- Scene depth distinguishes a label behind an ordinary occluder from one visible in the main scene. Inconsistent depth or missing subjects makes the relation indeterminate.
- Touching and intersecting controls can both produce adjacent silhouettes. Contact, penetration, anatomical target, temporal persistence and artistic quality remain unasserted.
- Engine masks need independent RGB checks under suitable visibility/contrast. The initial black-background foreground control applies only to the declared calibration scene; it is not arbitrary-scene segmentation.

See the [surface analysis audit](../audits/VIEWPORT_SURFACE_ANALYSIS_2026-09-11.md) for controls, results and retained evidence.
