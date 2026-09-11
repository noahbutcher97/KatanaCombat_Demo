# Animation analysis suite: portability and ownership

Status: suite-wide architecture and migration plan, recorded 2026-09-11. Portable Python services and the independent Unreal capture module now belong to the separate AnimationAnalysis repository. Katana consumes a [pinned revision](../../Tools/AnimationAnalysis/README.md). Remaining suite families are still migrating; see [repository extraction](../audits/ANIMATION_ANALYSIS_REPOSITORY_2026-09-11.md).

## Scope and objective

Develop general capture, geometry, temporal evaluation and visual-analysis systems first, then specialize them through explicit engine and project adapters. Develop shared code in the separate AnimationAnalysis repository; keep Katana adapters and integration verification here. The entire existing suite remains subject to the ownership inventory below, including implementations still awaiting extraction.

This applies retroactively to recording, pose/image/depth/mesh observation, offline analysis, calibration, paired-animation evaluation, preview/commandlet integration, comparison reports, profiles, automation fixtures, launch orchestration and artifact retention. Existing implementations receive the same dependency review as new work. Ordinary combat gameplay, asset authoring/migration operations and unrelated editor features remain project systems; they consume the suite where useful.

Reusable code must execute with data supplied by another project. A renamed class, configurable filename or wrapper around a Katana-dependent implementation does not establish that property. Create abstractions around shared data and actual ownership boundaries; add extension points when a consumer needs them.

## Dependency direction

| Layer | Owns | Allowed dependencies |
|---|---|---|
| Portable contracts | Subject/observation identity, geometry/raster/clock conventions, provenance, capability descriptions and result schemas | No game classes, Unreal objects, project filesystem discovery or local tools |
| Analysis and evidence core | Geometry, raster and temporal measurements, criterion evaluation, integrity/compatibility checks, calibration and review records | Portable contracts; explicit optional image/numeric backends |
| General orchestration and presentation | Analysis jobs, bounded output, comparison, reports and verified retention | Contracts/core plus explicit process, storage and presentation adapters |
| Unreal adapter | View/pose/mesh sampling, GPU readback, component lifetime, rendering policy, engine asset decoding and editor UI bridges | Engine APIs and portable contracts; no Katana gameplay module |
| Katana integration | Participant discovery, combat telemetry, paired-data and weapon interpretation, gameplay assertions, asset paths, anatomy mappings, project build/launch configuration | Public suite and Unreal adapter APIs, Katana systems |

Project integration may compose the other layers. Core and engine adapters never import project integration. Analysis can run on recorded input without an editor or launcher. Optional decoding/geometry dependencies must not become requirements for unrelated measurements. Engine adapters may use UE types internally; portable records must not require UE objects to interpret them.

The separate repository contains an engine-independent Python package and an Unreal plugin/module. Build and test them independently. Katana entry points, example assets and scenario configuration stay in this repository and consume their public APIs.

## Contract requirements

These are semantic requirements for the next contract implementation, not a claim that the current formats already provide them:

- Subjects have opaque stable identities, explicit component ownership and optional caller-authored groups/regions. A body, weapon, foot, support surface or prop is a configured use of a surface; it is not a mandatory class or role enum in the core. Replaced/destroyed components cannot silently inherit observation identity.
- Observations declare a stream/frame identity, acquisition time, clock domain and pose revision where applicable. Delayed readback records acquisition identity separately from completion time. Simulation, animation, wall and render clocks require explicit relationships; neither ordering nor equal numeric timestamps proves synchronization.
- Geometry declares units, coordinate space, transform/projection convention, topology/LOD identity, deformation support and source hashes. The adapter normalizes supported conventions or reports them unsupported. Raster data additionally declares size, channel format, depth convention and visibility semantics. Missing or hidden surfaces remain unknown.
- Results distinguish successful measurement, insufficient evidence and unsupported capabilities, with reasons and affected intervals. Surface crossing, containment, signed distance, intersection volume and swept intersection are separate capabilities. Proxy geometry is identified as such. Expected versus unwanted contact/penetration belongs to a supplied criterion profile.
- Profiles provide thresholds, selected pairs/regions, intended intervals, support reference frames and calibration provenance. Core modules never choose a skeleton, gameplay event, asset, test name or reference profile on the caller's behalf. Project examples are optional integration data.
- Artifacts bind input, schema, implementation, producer and profile identities. Portable references resolve from an explicit bundle root; absolute local paths may be diagnostic metadata but cannot be required for replay. Source/binary/asset identity collection belongs to its adapter.
- Resource policy declares duration, sample/queue/byte bounds and how exhaustion, cancellation and partial results are reported. Retention operates on an explicit verified artifact manifest, preserves required replay evidence and cannot discover deletion targets from a project folder convention.

Use versioned schemas and explicit compatibility adapters. Preserve prior captures and CLI entry points during migration; do not reinterpret older evidence as having capabilities it never recorded. Shared hashes, atomic publication and validation should have one implementation used by their consumers.

## Existing-suite migration inventory

The following records the initial dependency/ownership review and required dispositions. It is not a full correctness audit or a list of completed refactors. Python inventory and direct imports were recorded in `Saved/Logs/AnalysisArchitecture-20260911/suite-inventory.json`; the tracked paths below provide the reviewable migration scope without local tooling. Current progress is recorded after the table.

| Existing family | Current coupling or boundary | Required disposition |
|---|---|---|
| `CombatCaptureSession.h/.cpp`, `CombatCaptureCommands.cpp` | Project adapter delegates lifetime/engine observation to `FAnimationCaptureSession`; discovery, humanoid defaults, combat/warp fields and telemetry switch ownership remain in Katana | Preserve compatibility while further normalizing streams and extension contracts; migrate remaining paired consumers separately |
| `AnimationCaptureImageWriter`, `ViewportSurfaceCapture`, old compatibility headers | Implementations compile in the independent `AnimationCapture` plugin module; original headers delegate without duplicate implementations | Keep verified lifecycle/identity semantics while adding bounded asynchronous readback and moving-surface capture |
| `Tools/CombatCapture/analyze_capture.py` | Strict input/PNG integrity, summaries/deltas and atomic publication delegate to the portable package; legacy stream decoding, motion metrics and combat report layout remain here | Move remaining reusable stream analysis/presentation behind explicit records and supplied context |
| `evaluate_capture.py`, `summarize_runs.py`, `capture_format.py` | Shared identity, status and clock/interval services are portable; file selection and simulation-field translation are in the format adapter; gameplay checks and reference criteria remain project-side | Continue separating reusable evaluation/report assembly from combat assertions, experiment validation and reference selection |
| `run_scenario.py`, `evaluate_pair.py` | Process handling and identity collection assume the checkout layout, editor target, project file, DLL names, automation namespace and engine install default | Separate reusable execution/evidence services from an explicit project launch/identity descriptor and Katana entry points |
| `PairedContactProfileEvaluation.cpp`, `PairedAlignmentEvaluation.cpp`, `PairedContactEvaluation.h`, `PairedWarpTuning.h` | Geometry/clock evaluation is reached through project subsystem/library APIs; authored sampling resolves Katana paired data, weapon data and sync notifies | Extract common measurements/contracts; keep Unreal sampling separate from Katana asset/notify and warp-experiment adapters |
| `PairedAnimationAnalysisLibrary`, `PairedAnimationAnalysisSubsystem`, `PairedAnimationEditorTypes`, `PairedAnimationPreview`, `PairedAnimationPreviewConfig`, evaluation commandlet | Existing library/subsystem/UI split contains shared calculations, engine state and project authoring assumptions; some calculations still live in the widget | Inventory each calculation/type during migration; shared math and results enter the suite, engine state stays in adapters, project authoring/selection stays in Katana. UI and commandlet consume the same evaluation API |
| `visual_analysis/geometry.py`, package exports | Pure segment math shares a module with UE projection conventions and legacy actor/pose linkage; a generic error still refers to a weapon | Move format/engine interpretation behind compatibility adapters; keep math and subject-neutral results portable; update consumers and exports together |
| `visual_analysis/reviews.py`, `surface_evidence.py`, `images.py` | Useful independent review/decoding services, with current montage/simulation clock and engine-frame field conventions | Normalize through versioned adapters; retain current formats as supported legacy readers and keep decoding optional |
| `visual_analysis/surfaces.py`, `pixel_alignment.py` | Reusable explicit-input measurements; detector identity/profile/file concerns share the pixel module | Preserve algorithms; separate measurement from profile loading and implementation identity when packaging |
| `review_contact.py`, `review_visual.py`, `review_surfaces.py`, `calibrate_segment_alignment.py`, `detect_segment_alignment.py` | Reusable command behavior imports sibling scripts/package paths; contact review interprets project capture records | Expose callable package services; leave thin CLIs and compatibility translation; use shared evidence/publication rather than duplicating it |
| `scenarios/*.json`, `pairs/*.json`, `visual_analysis/profiles/cyan-segment-alignment.json` | Katana assets, role/bone names, provisional criteria and a project-calibrated profile | Keep as optional Katana integration data, outside generic package defaults; migrate profile paths and all consumers together without resaving assets |
| Python `test_*.py`, native capture/contact/warp tests and scenario fixtures | Pure controls, engine controls and gameplay integration currently share local module/test placement | Separate portable, Unreal-only and Katana integration suites; preserve meaningful regression coverage and repository test naming conventions |
| Generated review/cleanup scripts and retention records under `Saved/` | Previous runs retain verified evidence but some retention orchestration is session-specific | Promote reusable manifest-driven retention into the suite; preserve historical artifacts as evidence, not required executable infrastructure |
| Guides, examples, build/test commands and repository entry points | Current usage depends on project paths and local Unreal setup | Document both standalone usage and Katana integration; provide runnable neutral examples and isolated dependency checks |

The portable Python package lives under `Python/src/animation_analysis` in AnimationAnalysis, with explicit clock, pose and projection contracts, canonical evidence, producer-format adapters and four command services. Katana's `Tools/AnimationAnalysis` contains only dependency integration and verification wrappers. Existing `visual_analysis` imports and review/calibration commands resolve the locked checkout. Katana profiles remain outside its distribution. A wheel has been installed and exercised outside the checkout, with core operations verified without Pillow and an additional image test with the optional extra.

The shared offline services own strict structured-input/PNG integrity, explicit artifact manifests, identity, atomic file publication, numeric summaries/deltas, status aggregation and clock-declared interval bracketing. Existing project analyzers/runners and all four packaged publication jobs consume those services. Runner source identity includes the dependency lock, integration code and committed dependency sources under portable `Dependencies/AnimationAnalysis/` keys; offline analyzer/evaluator identity includes the resolved package and project adapters. See [shared-service verification](../audits/SHARED_ANALYSIS_SERVICES_2026-09-11.md).

The [Unreal capture dependency](../../Tools/AnimationAnalysis/README.md) owns native
session lifetime, engine observation, PNG writing and viewport surface readback.
It accepts explicit subjects/output and producer extensions. Katana's adapter owns
discovery, default points, combat/warp telemetry and exclusive telemetry resources.
The minimal host under the shared repository's `Python/UnrealHost` builds without
Katana modules or assets. Runner provenance verifies the generated native plugin
against the pin and includes its sources and binaries.

Legacy stream/motion analysis, report assembly, process launching and reference
orchestration still have mixed responsibilities in the project scripts. Paired
preview/evaluation still depend on Katana editor/gameplay modules. Native streams
retain legacy schema names, and reusable retention still requires migration. The
foundation has moved; whole-suite migration remains incomplete. Existing legacy asset names stay
in project integration until a scoped asset migration updates their consumers.

## Implementation order and migration acceptance

1. **Contracts and isolation first.** Implement the portable package boundary and compatibility readers with neutral datasets. Exercise from a temporary directory with no project on the import path. Declare optional dependencies and replace sibling-script imports in reusable services. Keep working project commands as thin consumers.
2. **Retrofit existing producers and consumers.** Separate capture lifecycle/engine observation from Katana telemetry/discovery, then separate common evaluation/reporting from scenario assertions. Give the native adapter its own engine-only build boundary. Migrate existing preview, commandlet, CLIs, profiles and tests through the same public services. Each slice updates its callers/docs and demonstrates behavior parity or explicitly documented corrections.
3. **Extend through the common APIs.** Add asynchronous RGB/depth readback, moving skeletal observations, surface/support/region measurements and temporal evaluation. Profile capture-on versus capture-off under the same renderer; report frame-time distributions, completion latency, queue/memory bounds and dropped/gapped observations. Preserve diagnostic rendering policy and verify geometry/RGB agreement.
4. **Add geometric penetration capabilities.** Start with offline intersection on explicitly supported deformed meshes and known controls. Declare geometry validity, deformation coverage and between-sample limits. Add containment, defined depth/volume metrics and swept intersection as separately verified capabilities. Unsupported data must not become a clean-contact result.
5. **Validate extraction and project value.** Run a neutral non-combat consumer and a minimal Unreal host without Katana gameplay dependencies. Then validate existing finisher and counter integrations and apply the results to the open choreography/facing/grip issue. Two Katana cases establish integration coverage; independent hosts establish portability.

Migration is complete only when every inventory family has a disposition, reusable code no longer depends on project implementation, current consumers use the extracted services, and compatibility, isolated execution/build, performance, evidence and retention checks pass. Track remaining mixed responsibilities explicitly; do not close the workstream after making only the newest module portable.

The initial repository extraction carries the shared implementation, neutral fixtures, schema/version documentation, dependency declarations and remaining inventory without Katana assets or local AI configuration. Further migrations must preserve independent host checks and Katana consumer verification. Katana-specific scenarios and integrations remain here.

## Verification policy

Documentation-only updates require relative-link and scoped-diff checks. Implementation verification follows the individual migration steps above and the repository verification ladder. A Python wheel or offline replay does not establish a native build, runtime pass, capture performance improvement or completed suite extraction.

Related: [active implementation plan](../plans/VISUAL_ANALYSIS_LIBRARY.md), [current surface adapter](../guides/VIEWPORT_SURFACE_ANALYSIS.md), [capture guide](../guides/COMBAT_CAPTURE_AND_ANALYSIS.md), [paired evaluation guide](../guides/PAIRED_ANIMATION_EVALUATION.md), [visual library API](../../Tools/CombatCapture/visual_analysis/README.md).
