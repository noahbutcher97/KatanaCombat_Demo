# Animation Analysis dependency

The shared implementation is owned by the public [AnimationAnalysis repository](https://github.com/noahbutcher97/AnimationAnalysis).
This directory contains Katana integration only: a revision lock,
installer, resolver and compatibility verification commands.

From the Katana project root, install the exact commit and repository recorded in
[dependency.json](dependency.json):

```powershell
python Tools/AnimationAnalysis/setup_dependency.py
```

The installer creates a detached checkout under ignored `Saved/AnalysisDependencies`
and copies its native source and shader resources into ignored `Plugins/AnimationAnalysis`. The project
descriptor already enables that plugin. Run setup before building a fresh Katana
checkout. The public clone URL is tracked; no developer-specific path is required.
Use `--repository <checkout-or-URL>` to explicitly override the source for local work.

Edit shared Python/native code in the standalone repository, verify and commit it,
then update the full revision in the lock and run setup again. The installer checks
committed source bytes, rejects dirty/unowned copies, and preserves the previous
generated plugin under `Saved/AnalysisDependencies` when changing revisions.
Rebuild and restart `KatanaCombatEditor` after changing the native dependency.
The readback module loads at `PostConfigInit` to register its shaders. Shader sources
and includes participate in installation integrity and scenario provenance. Generated copies
are consumption artifacts, not editable source. Offline project commands validate
the pin; scenario provenance also validates and hashes the native plugin.

Existing verification commands delegate to the pinned checkout:

```powershell
python Tools/AnimationAnalysis/verify_distribution.py --output Saved/Logs/AnalysisPythonVerification
python Tools/AnimationAnalysis/verify_unreal_host.py --engine "C:/Program Files/Epic Games/UE_5.6" --output Saved/Logs/AnalysisNativeVerification
python -m unittest discover -s Tools/AnimationAnalysis -p "test_*.py"
python -m unittest discover -s Tools/CombatCapture -p "test_*.py"
```

The standalone repository owns `Python/src/animation_analysis`, portable tests,
the neutral Unreal host, `Source/AnimationCapture` and the remaining migration
inventory. Katana retains its profiles, gameplay adapters, scenarios and assertions.
See [suite ownership](../../docs/architecture/ANIMATION_ANALYSIS_SUITE.md).

## Bounded async capture in Katana

The current dependency supplies bounded asynchronous readback as an explicit opt-in.
`FCombatCaptureSettings` forwards `bUseAsyncReadback` and
`bUseAsyncDiagnosticResolution` to the shared recorder. Both default to false;
ordinary console and scenario commands retain their synchronous acquisition policy.

```cpp
FCombatCaptureSettings Settings;
Settings.bUseAsyncReadback = true;
Settings.bUseAsyncDiagnosticResolution = true;
// Caller supplies the PIE participants and configures a supported no-AA view.
Session.Start(World, Settings, Participants, Error);
```

Supported acquisition is D3D11, one perspective view, full resolution and no AA.
The diagnostic-resolution opt-in enforces full resolution but does not disable AA.
Delayed frames retain acquisition identity and add completion/renderer metadata;
`readback_wall_s` measures enqueue cost in this mode. The offline motion analyzer
keeps readback mode/view policy in comparison compatibility and does not certify
the terminal readback stream. The native control validates the producer outcome.

`KatanaCombat.Capture.PIE.ThirdPersonAsyncAPI` exercises the Katana adapter, telemetry,
frame bounds and failed PNG handling in a rendered session. Its NullRHI result
explicitly defers the GPU check. The existing surface adapter remains synchronous;
new delayed surface consumers use the plugin's `FViewportAsyncCapture` API.

See the [integration evidence](../../docs/audits/ANIMATION_ANALYSIS_INTEGRATION_2026-09-11.md)
and the dependency's `docs/ASYNC_READBACK.md` for limits. This update does not add
moving skeletal surfaces or continuous mesh penetration measurements.
