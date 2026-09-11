# Independent Unreal animation capture

Completed on 2026-09-11. The project editor and copied engine-only host build;
native host controls, focused project automation, D3D11 capture/surface checks and
a fresh rendered finisher evaluation pass. The 77 Python regressions also pass.
See [implementation, evidence and limits](../audits/NATIVE_ANIMATION_CAPTURE_2026-09-11.md).

Extract the existing recorder lifecycle, engine observations, bounded PNG writer
and viewport surface adapter into `Plugins/AnimationAnalysis`, using an editor-only
`AnimationCapture` module with no Katana dependency. Keep discovery, humanoid point
defaults, combat/warp telemetry and its global switch ownership in the project
`FCombatCaptureSession` adapter. Existing commands and schema-2 output remain usable.

The native API takes explicit subjects, optional meshes/points, an output root and
an optional observation extension. Extensions contribute additional fields and
bounded text artifacts; they cannot replace the recorder's identity fields or
overwrite its streams. Every successful extension start has a matching end on
manual stop, limits, teardown and startup export failure. Independent native
sessions do not share Katana's telemetry singleton.

Implementation files: plugin descriptor/module/types/session/image writer/surface
adapter; compatibility headers and session adapter in `KatanaCombatEditor`; editor
module/plugin registration; runner source and binary provenance; a minimal host
under `Tools/AnimationAnalysis/UnrealHost` and isolated build verifier.

Acceptance: project editor build; focused capture lifecycle/telemetry/image/surface
automation; fresh D3D11 rendered capture controls; neutral host build and tests with
only the copied plugin, Engine and host sources. Verify extension ownership,
metadata protection, output isolation, arbitrary non-combat actors, and cleanup.
Keep retained evidence compact and remove generated test PNGs after verification.
Check preservation of all pre-existing Content and unrelated source WIP.

This step establishes the native dependency boundary. Async GPU readback, moving
skeletal surface integration and additional surface/penetration capabilities remain
subsequent implementations, with their own performance and geometry validation.
