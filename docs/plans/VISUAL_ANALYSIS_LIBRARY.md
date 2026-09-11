# Reusable visual analysis library

## Active suite-wide direction

The [animation analysis suite architecture](../architecture/ANIMATION_ANALYSIS_SUITE.md) governs the whole existing suite and all extensions. The shared Python package, native capture plugin and neutral host now belong to the separate AnimationAnalysis repository; Katana consumes a [pinned dependency](../../Tools/AnimationAnalysis/README.md). This covers capture/session management, image/depth/mesh observation, offline evaluation, paired preview/commandlet integration, visual review, profiles, runners, tests and artifact retention. Existing project coupling must still migrate; moving the foundation alone does not complete the suite.

Next implementation order:

Steps 1 and 2 established the independently verified foundation and repository boundary. The next implementation is step 3 in AnimationAnalysis, with Katana integration checks. See [extraction evidence](../audits/ANIMATION_ANALYSIS_REPOSITORY_2026-09-11.md).

1. Establish portable contracts, package/import boundaries, compatibility readers and isolated execution checks. Use neutral fixtures and caller-provided subjects, clocks, coordinates and criteria. Move existing UE-format interpretation and Katana configuration to their owning adapters. The architecture document inventories the existing mixed responsibilities.
2. Separate the existing native recorder's engine observation/lifecycle from Katana discovery and combat telemetry. Give generic Unreal capture/evaluation code a build boundary without Katana gameplay dependencies; migrate the existing surface adapter and image writer into it. Preserve existing console/CLI behavior through thin project entry points.
3. Implement bounded asynchronous RGB/depth collection and moving skeletal observation through these contracts. Measure frame-time overhead, readback latency, resource use and sample gaps; verify frame/pose identity through delayed completion and teardown.
4. Add registered rigid/skinned surfaces, region/support and temporal measurements, then offline deformed-mesh intersection analysis with explicit geometry validity and supported-deformation conditions. Keep containment, depth/volume and between-sample intersection claims distinct; full penetration coverage requires their separate validation.
5. Exercise the common APIs with a non-combat fixture and existing finisher/counter consumers. Apply the results to the unresolved source-pair facing/grip/contact investigation. Move reusable calculations out of current project runners, evaluator and preview code as each consumer migrates; retain Katana behavior criteria and asset interpretation in project adapters.

Completion requires the suite-wide migration and extraction checks in the architecture contract. Two Katana scenarios alone do not prove independence. The dated sections below record earlier work and evidence; their historical test counts and initial scope are not current extraction claims.

### Completed slice: shared offline services

Move strict structured-input/PNG integrity, explicitly selected file manifests,
canonical identities, atomic file publication, numeric summaries/comparison and
clock-declared interval bracketing into `animation_analysis`. Keep legacy capture
file selection, simulation-field translation, gameplay assertions and installation
defaults in `Tools/CombatCapture`. Preserve existing command/API consumers through
imports and adapters. Include the portable package in runner source snapshots and
offline implementation identities; single-script references must become incompatible.

Verify malformed evidence, failed publication, unrelated clocks, missing interval
coverage and changed dependency identities with neutral fixtures. Run the existing
project regressions and an independently installed wheel, replay retained captures
without modifying their evidence, and check source/asset preservation. This slice
does not move the native recorder or implement new surface measurements.

Implemented on 2026-09-11 as package version 0.2.0. All 76 project Python tests
pass. An isolated wheel passes 24 core tests with one image-only skip, then all
25 tests with the image extra. Retained calibration, segment/surface measurements
and four captures' telemetry/scenario results replay unchanged; offline producer
identities intentionally change. The four captures retain their existing incomplete
image status after bulk PNG retention cleanup. See the [service migration audit](../audits/SHARED_ANALYSIS_SERVICES_2026-09-11.md).

### Completed slice: portable Python package and compatibility boundary

Create `Tools/AnimationAnalysis` as an independently installable `animation_analysis` package. Move existing raster, pixel, geometry, evidence and review services into it, with explicit adapters for current Unreal capture/surface formats. Introduce producer-neutral clock, pose identity and projection contracts. Keep `Tools/CombatCapture/visual_analysis` and the four review/calibration entry points as compatibility delegates; retain project profiles outside the distribution. No C++ or assets change in this slice.

Verification: existing Python regression suite, new contract/compatibility tests, wheel installation and execution in a clean temporary environment outside the repository, and numeric replay of retained surface/segment evidence. Confirm package imports do not require Unreal, Katana, Pillow for core operations, project paths or sibling scripts. Preserve old evidence bytes and profile thresholds; changed implementation identity must not silently reuse calibration. Document the remaining suite inventory after this slice.

Implemented on 2026-09-11: standalone package, explicit clock/pose/projection contracts, canonical evidence schema 2 with historical adapters, and migration of existing geometry/raster/pixel/review services plus four command services. The 72 project Python regressions pass through compatibility delegates. Installed-package tests pass outside the checkout: ten core tests with the image-only test explicitly skipped, then all eleven with the optional image extra. Ninety segment controls, sixteen reviewed segment observations and five surface observations replay unchanged. See [evidence and remaining migration](../audits/PORTABLE_ANIMATION_ANALYSIS_2026-09-11.md).

The shared-service migration is recorded above. The independent native recorder
boundary is also complete: [native capture implementation and checks](../audits/NATIVE_ANIMATION_CAPTURE_2026-09-11.md).
Next, implement bounded asynchronous readback with acquisition/completion identity,
resource/cancellation/teardown checks and measured capture overhead, then integrate
moving skeletal surface observations. Remaining legacy stream/motion analysis,
report assembly, process orchestration, preview/commandlet and retention migration
remain required parts of the whole-suite plan.

## First implementation

Keep this in the current repository under `Tools/CombatCapture/visual_analysis`. The package serves captured animation tasks; paired contact is its first consumer. Reuse verified engine capture data rather than creating another recorder or a separate plugin.

1. Extract projection, frame/pose linkage and geometry helpers from the contact CLI.
2. Define a portable evidence reader that verifies embedded image hashes and unique frame identity.
3. Define structured visual findings with capture/input identity, reviewed frame references, reviewer method, observation, interpretation, uncertainty and next action. Keep visual findings separate from automation and geometric outcomes.
4. Add a generic visual-review command and report renderer; use the finisher recordings as the first real analysis. No additional captures or PNG exports are needed for this step.
5. Test swapped evidence, stale hashes, missing references and invalid verdicts, plus a second non-contact example to establish that the schema is reusable. Document the API and extension boundary.

## Outcomes and limits

The first implementation is complete: both CLI consumers use the package, four finisher visual analyses are published against their 96 retained frames, and all 49 offline tests pass. Verification includes a non-contact review, changed/swapped evidence, missing references, malformed reviews, detector provenance and replacement of a previous result after rejection. Current C++ capture verification remains the editor build, 20 capture automation tests and four rendered camera scenarios recorded in the finisher audit; the library extraction did not change C++ or assets.

The first library records `consistent`, `concern`, `indeterminate` and `not_reviewed` assessments. It validates evidence references; it does not automatically detect visual defects or certify animation quality. Reviewer provenance distinguishes human review, assistant image review and future detector outputs. Contact, alignment, clipping, timing, framing, foot support and telemetry agreement are independent categories, not a weighted feel score.

Subsequent task-specific detectors must use the same evidence contract and be calibrated against known visible defects and reviewed controls. Add a detector when a real task needs it. Keep capture adapters and interpretation separate so finisher assumptions, bone names and private workflow names do not enter the shared API.

## Completed slice: visible segment alignment and contact visibility

The first pixel detector will compare an independently detected coloured segment in a reviewed image region with its projected telemetry segment. It addresses the current risk of visible weapon/telemetry disagreement. Region identity and unobscured visibility remain explicit review inputs; the detector must abstain for unknown/occluded or ambiguous evidence. This is not a detector of skinned blade/body contact.

- Add image-based segment fitting and alignment measurements in `visual_analysis/pixel_alignment.py`, with no weapon names or skeleton assumptions. Keep image decoding as an optional CLI dependency.
- Add a reusable command consuming portable evidence plus reviewed regions and a versioned detector profile. Publish measurements and evidence-linked visual findings separately from capture/geometric status.
- Calibrate on the existing default-camera visible-weapon frames; use opposite-camera frames for cross-view validation. Those opposite views were inspected during feasibility, so they are not a blind holdout. Exercise known perpendicular telemetry offsets, stale segment data, empty/ambiguous colour regions and occlusion. Retain raw pixels and control definitions; never alter source captures to create a control.
- Add an oblique camera preset to the finisher scenario, build the current editor and capture both maps using the same transient warp control. Use raw pixels to determine whether contact is now visible. Add an elevated view only if the oblique view remains insufficient.
- Verify focused Python controls, fresh rendered scenarios, provenance and asset preservation. Recycle bulk PNGs after portable review export and required integrity checks; preserve compact findings and detector calibration evidence.

Acceptance: independent pixel measurements work on the declared material/view conditions, known misalignments are flagged, unmeasurable cases abstain, and the final report separates visible contact from segment/projection agreement. A failed calibration remains an explicit experimental result rather than being promoted to a detector pass.

Completed on 2026-09-11: reusable calibration/detection commands, an experimental cyan-segment profile and 61 passing offline tests. The real-image calibration replay passed 90 controls. Four fresh rendered scenarios passed after a successful editor build. On the unchanged detector settings, nine fresh opening frames agreed within 1.84 pixels, three abstained on line-fit width and four contact frames abstained for occlusion. The elevated raw images support apparent separation in sampled strike poses; this is a separate visual concern, not a detector-derived contact verdict. See the [audit and evidence](../audits/VISUAL_SEGMENT_ALIGNMENT_2026-09-11.md).

## Next investigation

Inspect the unwarped authored pair, relative facing and weapon attachment/grip orientation at the reviewed strike times. Establish the intended contact region from the choreography before choosing a placement correction. Reuse the existing paired evaluator and portable visual reports, making one explicit diagnostic change at a time. Keep the detector thresholds and provisional contact tolerances unchanged while identifying the cause. Automatic subject/occlusion recognition and additional visual metrics remain separate extensions justified by concrete tasks.

## Next measurement layer: coarse mesh contact and segmentation

Added from the 2026-09-11 review. Whole-body and weapon silhouettes are needed alongside thin-segment fitting. This is reusable project tooling for body/body and weapon/body interactions, including finishers, counters, grapples and paired transitions. It is planned work; the current pixel detector does not implement it.

1. **Prove capture alignment first.** Investigate a bounded extension of the existing recorder for participant/weapon masks and depth at the same camera, viewport, engine frame and observed poses as RGB. Preserve component identity, resolution, projection, depth convention/units, renderer settings and pass hashes. Reject stale or mismatched passes. Validate the actual UE 5.6 rendering path before choosing an implementation; a separately timed capture is not automatically equivalent. Restore temporary render flags and avoid asset saves.
2. **Separate visible and occluded silhouettes.** A visible object-ID image contains only the frontmost object at each pixel, so its disjoint labels cannot measure overlap behind an occluder. If projected overlap is needed, establish separately rendered participant masks with verified matching poses/projection, clearly labelled as full projected silhouettes. Ordinary single-layer scene/custom depth does not expose both hidden surfaces. Missing layers remain unknown. Include nominated weapon meshes as separate identities and account for bystanders and environment occlusion.
3. **Add reusable coarse measurements.** Start with visible contour gap in pixels, projected silhouette overlap fraction where the required masks exist, visible-area fraction and persistence over a declared simulation interval. Use available depth to reject obvious foreground/background false contact and to bound proximity at sampled visible surfaces. Report uncertainty from mask edges, thin objects, view direction and sampling. Screen-space overlap or a small depth difference is a contact candidate, not proof of touching or penetration; depth samples cannot certify an entire skinned mesh intersection.
4. **Cross-check masks against RGB.** Render labels are engine-derived evidence, not independent visual truth. Review overlays against raw images and use a small independently reviewed segmentation corpus to measure boundary disagreement. Test swapped actor labels, missing weapons, stale masks, clipped silhouettes and occluders. Never generate validation masks from the same projected bones or collision proxies whose correctness is being checked. Consider an RGB segmentation model only if reviewed/engine masks leave a demonstrated gap; record its version, conditions and uncertainty if introduced.
5. **Calibrate before judging animations.** Begin with simple known geometry exhibiting clear separation, touching surfaces, deliberate intersection, apparent 2D overlap with depth separation, and partial/full occlusion. Measure false positives and abstentions, not only expected successes. Then use distinct reviewed paired-animation recordings across the two maps and multiple views. Thresholds are profile-specific; no general contact or artistic pass is inferred from a small calibration set. Penetration controls test whether the tool flags a concern, not whether it can recover exact intersection volume.
6. **Use the existing reports and retention policy.** Keep mask/depth/RGB provenance, metric results and independent visual findings together through adapters into the shared evidence contract. Retain compact selected evidence and numeric masks/depth needed for replay; recycle bulk exports after verification. Start with one interaction on both maps before expanding coverage.

First deliverable: a capture feasibility result plus one calibrated coarse gap/occlusion measurement with known-defect controls. Run it alongside the source-pair facing/grip investigation before treating placement tuning as visually validated. Do not delay inspection of already captured apparent separation while building this layer.

Engine references: Epic documents [Custom Depth and Stencil through post-process materials](https://dev.epicgames.com/documentation/unreal-engine/post-process-materials-in-unreal-engine) and [object-ID/world-depth cinematic passes](https://dev.epicgames.com/documentation/unreal-engine/cinematic-render-passes-in-unreal-engine). These establish candidate rendering capabilities, not compatibility with this recorder; local UE 5.6 source and a synchronized capture control must determine that. A Movie Render Queue migration is not part of this plan.

### Bounded implementation: viewport surface observations

Keep this implementation modular inside KatanaCombat. `ViewportSurfaceCapture` will own one explicitly requested viewport observation and return pure arrays plus frame/projection identity. A separate scoped label helper will manage nominated primitive components and restore their render settings. Neither may import combat classes or infer subjects from a test name. Existing continuous RGB recording remains unchanged while readback cost and correctness are established.

`visual_analysis/surfaces.py` will consume labelled raster/depth data with explicit validity and measure visible contour gaps and occlusion. A separate adapter/CLI will load the capture bundle and write evidence; criteria stay in profiles or reviewed controls. The first producer will be a rendered automation fixture using engine primitives with known spacing, depth and occlusion, independent of Katana assets. Use exact RGB/render frame identity, reject unsupported projections/render modes, enforce a bounded observation size and expose readback cost. The initial raw depth implementation may support only the locally verified D3D11 format; other backends must reject rather than reinterpret bytes.

Files: `Public/Analysis/ViewportSurfaceCapture.h`, `Private/Analysis/ViewportSurfaceCapture.cpp`, `KatanaCombatEditor.Build.cs`, `Private/ViewportSurfaceCaptureTests.cpp`, Python surface measurement/adapter tests and documentation. Acceptance: fresh editor build; label ownership/restoration and failure tests; rendered known-geometry controls with matching RGB/depth identity; portable compact review; pure measurements tested on unrelated labels, resolutions and defective inputs. This establishes the adapter and measurement instrument before connecting it to continuous paired-animation scenarios.

Completed on 2026-09-11: the bounded adapter, scoped labels, surface bundle reader, pure visible-separation/occlusion measurements and portable review command. Both rendered automation tests pass, including stale-frame and duplicate-request rejection. All 72 offline tests pass. Known controls show a 66-pixel separated gap, 1-pixel adjacency for touching/intersection, and indeterminate hidden surfaces. Four eligible independent RGB foreground comparisons match the combined mask exactly under the declared unlit calibration conditions. Readback alone costs about 12.4-13.5 ms per observed 640x480 frame; this is not yet a continuous animation recorder. See [implementation and evidence](../audits/VIEWPORT_SURFACE_ANALYSIS_2026-09-11.md).

Next: connect explicitly nominated body/weapon primitives to the existing capture participant/pose contract, preserve the actual rendering policy in capture identity, and establish a bounded asynchronous readback path before continuous sampling. Validate first on a moving skeletal pair and a second interaction, retaining independent RGB findings and known stale/occluded controls. The source-pair facing/grip investigation remains open; these generic controls do not settle the finisher's contact intent.
