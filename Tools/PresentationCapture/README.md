# PresentationCapture dependency

PresentationCapture records silent H.264 MP4 of the real PIE backbuffer with variable-frame-rate
timestamps, plus timing and transform telemetry. Its source is owned by the separate
PresentationCapture repository; this directory holds only Katana's revision lock, installer and tests.
`FCombatCaptureSession` uses it for opt-in video; see the
[capture guide](../../docs/guides/COMBAT_CAPTURE_AND_ANALYSIS.md#video-capture).

[dependency.json](dependency.json) pins the private repository
`https://github.com/noahbutcher97/PresentationCapture.git` and an exact commit, exactly as AnimationAnalysis
pins its public one. Cloning needs GitHub credentials with read access to that repository. For local plugin
work, `--repository` points setup at a checkout instead, such as the owner's development repository
`D:/UnrealProjects/Plugins/PresentationCapture`:

```powershell
python Tools/PresentationCapture/setup_dependency.py
python Tools/PresentationCapture/setup_dependency.py --repository <checkout-or-URL-containing-the-pin>
```

CI installs the recorder the same way, in the self-hosted job (`.github/workflows/ue5-ci.yml`, "Install pinned
PresentationCapture recorder"), with the runner machine's Git credentials. GitHub-hosted validation does not
build the editor. A failed fetch reports Git's own error; a missing `--repository` path is named. Over SSH,
`--repository` takes `ssh://git@github.com/noahbutcher97/PresentationCapture.git` or the scp-style
`git@github.com:noahbutcher97/PresentationCapture.git`; a drive path such as `D:/...` is always a local path.

The repository's deepest paths (its analysis bridge, which is not installed) exceed Windows' 260 characters
under a project's `Saved/` cache, so setup checks out with `core.longpaths`; Windows' long-path policy must also
be enabled for the verification reads. A fresh install from the remote at a project root as long as the CI
runner's (`D:/actions-runner/_work/KatanaCombat/KatanaCombat`) failed without it and succeeds with it.

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
