# Independent native animation capture

Subsequent ownership update: the plugin is now consumed from a [separate pinned repository](ANIMATION_ANALYSIS_REPOSITORY_2026-09-11.md). The evidence below records the preceding native extraction.

The recorder's engine implementation lives in the [Animation Analysis dependency](../../Tools/AnimationAnalysis/README.md),
compiled as `AnimationCapture`. Katana's existing session and console interfaces
delegate to it. The plugin builds and runs in a minimal host without Katana modules
or assets; the project remains its first integration.

## Boundary

The plugin owns world/ticker/draw cleanup, engine pose/view observations, stream
budgets, background PNG writing and the existing surface-label/readback adapter.
It takes explicit subjects, optional meshes/points and an output root. There are no
humanoid point defaults, gameplay dependencies or project path discovery in its code.
Independent native sessions can coexist. Legacy schema-2 names remain for compatibility.

`IAnimationCaptureExtension` owns producer-specific collection and contributes
additional fields and text artifacts. Successful acquisition is paired with one
release on manual/automatic stop, teardown or initial manifest failure. Conflicting
fields invalidate evidence without replacing core identity; escaping/reserved names
and existing artifact files cannot be overwritten. Artifact bytes count against
the recording budget. File publication remains incremental, with manifest status
as the completion indicator.

The Katana adapter retains participant discovery, point defaults, combat/health/AI/
paired/warp observations, CSV telemetry and exclusive ownership/restoration of its
global telemetry switches. Existing image-writer and surface headers remain thin
compatibility delegates; their old implementation files were moved out of the
editor module. No duplicate algorithm implementation remains there.

Project registration/build dependencies now include the plugin. `.gitignore`
allows this plugin's source while keeping external plugins and generated binaries
ignored. Runner identity now includes plugin source/descriptor and the native DLL;
generated plugin build files are excluded. The isolated host and its verifier live
under `Tools/AnimationAnalysis` and require an explicit engine installation path.

## Verification

Evidence: `Saved/Logs/NativeCapture-20260911-110601/`.

| Check | Result |
|---|---|
| KatanaCombatEditor Win64 Development | Built successfully |
| Independent host | Copied 16 source/descriptor files; built Engine + plugin + host without Katana modules/assets |
| Native host controls | Two passed: concurrent sessions/moving prop/limits/teardown and extension rejection/identity/output protection |
| Project capture suite, NullRHI | 22 successful result records; 21 exercised checks plus the explicit rendered-surface deferral |
| D3D11 capture/surface suite | Four passed: both project PIE entry points, label ownership and rendered geometry |
| Fresh surface analysis | Five measurements exactly match pre-migration controls |
| Fresh rendered finisher runner | ThirdPerson/Completed passed automation and offline evaluation, with plugin source/binary provenance |
| Project Python regressions | 77 passed |

The first project link exposed a missing direct test-module dependency on
`AnimationCapture`. It was added, then the project rebuilt successfully before
automation. The final copied host source hashes are recorded separately from the
initial host run; its build/test temporary directory was removed.

The rendered scenario batch is `Saved/CombatScenarioRuns/20260911T112028-00176c02`.
Its capture was `Saved/CombatCaptures/20260911T112054-5ABFC20C4376B7866B1F15BE50413D22`.
The fresh surface bundle remains under `Saved/SurfaceObservations/D6BA1EAE49ADEE6117B6E697212490EA`,
with a portable RGB/label report in the evidence folder.

Complete new capture/image-writer evidence was archived and every entry SHA256
checked before removing 181 generated loose PNG files. `capture-evidence.zip` is
about 101 MiB; `retention.json` records entry hashes, exact cleanup scope and archive
identity. Existing recordings were preserved. Loose capture directories now lack
their PNGs; restore the complete archived bundle before re-running image-integrity
checks or following raw-image report links. The recorded pass applies to the intact
evidence before retention, and that intact evidence is retained in the archive.

All 7,948 pre-existing Content file sizes/timestamps and the 54 protected asset
hash/missing states were preserved. Runtime combat source and unrelated WIP were
unchanged. Configuration changes were limited to plugin registration and scoped
build/ignore rules; no assets were saved and no commits or external publications
were made.

## Limits and next implementation

This establishes an independent native capture boundary, not completion of the
[whole-suite migration](../architecture/ANIMATION_ANALYSIS_SUITE.md). Paired preview/
evaluation, remaining offline stream/report orchestration and generalized retention
still need migration. The native stream keeps its existing field/coordinate semantics.

RGB acquisition and surface depth readback remain synchronous; PNG encoding is the
existing bounded background operation. The next implementation is bounded async
readback with acquisition/completion identity and cancellation/teardown controls,
followed by moving skeletal surface integration. No performance improvement,
contact/artistic-quality verdict, support/region capability or mesh penetration
capability is established by this extraction.
