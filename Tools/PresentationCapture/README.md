# PresentationCapture dependency

PresentationCapture records silent H.264 MP4 of the real PIE backbuffer with variable-frame-rate
timestamps, plus timing and transform telemetry. Its source is owned by the separate
PresentationCapture repository; this directory holds only Katana's revision lock, installer and tests.
`FCombatCaptureSession` uses it for opt-in video; see the
[capture guide](../../docs/guides/COMBAT_CAPTURE_AND_ANALYSIS.md#video-capture).

The repository has no remote. [dependency.json](dependency.json) pins a local repository path and an
exact commit, the way AnimationAnalysis setup runs with `--repository`:

```powershell
python Tools/PresentationCapture/setup_dependency.py
python Tools/PresentationCapture/setup_dependency.py --repository <checkout-containing-the-pin>
```

Setup clones the pinned commit into ignored `Saved/PresentationCaptureDependencies/<revision>`,
verifies committed bytes, and copies only the core plugin directory (`Plugin/PresentationCapture`) into
ignored `Plugins/PresentationCapture`. The analysis bridge plugin is not installed: it requires
AnimationAnalysis native 0.4.1 exactly and suspends itself while Katana's own session records.

The plugin repository ignores its worker binaries, so setup then compiles
`PresentationCaptureEncoder.exe` and `PresentationCapturePNG.exe` from the pinned sources with the
plugin's own `BuildWorkers.ps1` (Visual Studio C++ x64 tools and a Windows SDK; PowerShell 7 is
preferred). Build evidence goes to `Saved/PresentationCaptureDependencies/worker-builds/`. The
workers' SHA-256 hashes are kept in `plugin-install.json`; unchanged recorded workers are reused and
missing or changed ones are rebuilt. Every setup ends with the encoder's own `--check` self-test (a
short H.264 encode), recorded as `encoder_check`. The workers are reproducible (`/Brepro`): the
2026-10-03 build matched the plugin repository's own binaries byte for byte.

The editor build refuses a stale copy with `[PresentationCapture pin mismatch]`, the same guard as
AnimationAnalysis (`Source/KatanaCombatEditor/KatanaCombatEditor.Build.cs`), and also when the worker
executables are missing. Run setup again after any pull that moves the pin. Like AnimationAnalysis,
setup refuses to overwrite a generated copy holding unowned or modified source, and keeps a replaced
copy, build outputs included, under `Saved/PresentationCaptureDependencies/retired-plugin-*`.

The pin, verification and install mechanics are shared with AnimationAnalysis in
[`Tools/PluginDependencies/pinned_plugin.py`](../PluginDependencies/pinned_plugin.py).

```powershell
python -m unittest discover -s Tools/PresentationCapture -p "test_*.py"
python -m unittest discover -s Tools/AnimationAnalysis -p "test_*.py"
```

Recording constraints that come from the plugin: editor and PIE only, a rendering RHI
(`-RenderOffScreen` works, `-NullRHI` is refused), `framegrabber.framelatency 0`, clips of 1 to 30 s
and fewer than 3,600 frames per seat, output boxes 640x360, 1280x720 or 1920x1080. Its own release
lane qualifies D3D12; Katana's spike recorded complete clips on both D3D11 and D3D12.
