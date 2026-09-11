# Animation Analysis repository extraction

Publication update: [AnimationAnalysis](https://github.com/noahbutcher97/AnimationAnalysis)
is now public. Katana's dependency lock records its clone URL and keeps the verified
implementation revision below. The remaining sections describe the extraction-time
state and evidence; the separate developer owns subsequent shared feature work.

The shared foundation now has its own local Git repository at
`D:\UnrealProjects\Plugins\AnimationAnalysis`, with 51 tracked source/documentation
files and a clean working tree. Katana pins commit
`ba13149d3318f80d3098958cd1d2cd52bba3e5d1`. No remote has been created or published.

## Ownership and consumption

AnimationAnalysis owns the portable package under `Python/`, the neutral Unreal
host under `Python/UnrealHost`, and the native `AnimationCapture` plugin at its
root. Its README, repository instructions and `docs/MIGRATION.md` carry standalone
usage, semantic constraints and the whole existing-suite migration inventory.

Katana retains project discovery, telemetry, skeleton/weapon interpretation,
profiles, scenarios and gameplay assertions. [Dependency setup](../../Tools/AnimationAnalysis/README.md)
documents its full commit lock, detached dependency checkout and generated plugin.
The shared implementation is no longer authored in either Katana source location.
The original directories were preserved in the evidence folder as
`previous-python` and `previous-plugin`, including prior generated build outputs.

For this workstation, setup was executed as:

```powershell
python Tools/AnimationAnalysis/setup_dependency.py --repository D:/UnrealProjects/Plugins/AnimationAnalysis
```

Tracked configuration contains the commit pin, not a workstation path. Setup
checks committed source bytes, rejects dirty/unowned copies and preserves a prior
installed plugin on revision changes. Katana source provenance includes the lock,
resolved shared sources and generated native sources; native DLL hashes remain
separate. Offline analysis sources remain excluded from execution-source identity.
Katana integration changes remain in its existing working tree; unrelated WIP was
not staged or committed.

The first import commit is `4f5577a681e1472511559471454debd28e9935e7`. Checkout
verification caught Git index normalization of two mixed-ending detector files.
The pinned follow-up commit preserves their original bytes with scoped Git
attributes. The final cloned package and built wheel both preserve detector
identity `3e7f2f23feed97d026da365586a0d7a2369a88cbc266166fe5992e926f0b4691`.
The Python implementation and native producer source bytes otherwise match the
pre-extraction copies; only packaging/host routing and integration changed.

## Verification

Evidence root: `Saved/Logs/AnalysisRepository-20260911-113408`.

| Check | Result |
|---|---|
| Isolated installed Python distribution from the final pin | 24 core tests pass, optional image test skips without Pillow; all 25 pass with the declared image extra; four CLI entry points run |
| Independent Unreal host outside Katana | Build passes and two `AnimationAnalysis.Capture.Portability.*` tests pass; all 16 tested plugin/host source files match the final pin exactly |
| Pinned dependency integration | Six tests pass: exact revision despite upstream advancement, byte preservation/idempotent setup, dirty source rejection, native copy rejection, safe revision update and actionable missing/invalid pin behavior |
| Existing project Python suite | 77 tests pass using the final pinned package |
| Katana editor target | `KatanaCombatEditor Win64 Development` rebuild succeeds, including the installed plugin and both consuming modules |
| Rendered native integration | Four D3D11 tests pass: PIE API/console on both maps and surface label ownership/rendered geometry |
| Rendered finisher scenario | ThirdPerson/Completed automation and offline evaluation pass; current source and binary identities match the recorded run |
| Preservation | All 7,948 Content file size/time records and 54 protected asset hash/missing records unchanged; project C++ source, config and descriptor unchanged during extraction |

The final wheel SHA-256 is
`1cf4fad5532ca558b42a61533f37a0614dc23c7ff5efe3a57297086ef084e38f`.
`verification.json`, `dependency-files.json`, `pinned-source-check.json`, retained
test/build logs and before/after source manifests provide detailed evidence.
The scenario batch is `Saved/CombatScenarioRuns/20260911T114729-befcf22d`.

The capture bundles were archived and each archive entry hash verified before all
176 newly generated loose PNGs were removed. `capture-evidence.zip` is 96,855,685
bytes, SHA-256 `ea0ef372c1b12761534e6b9a673448694a72a28cdc2204c737c0eb2e145297c8`.
Restore its project-relative entries before replaying image-dependent analysis;
the remaining loose capture folders are intentionally image-incomplete.
Temporary package environments and the neutral host were removed by their verifiers.

## Remaining work

This completes repository extraction of the verified foundation. It does not
complete migration of all paired preview/evaluation, stream/report processing,
process orchestration or reusable retention. The full [ownership inventory](../architecture/ANIMATION_ANALYSIS_SUITE.md)
remains active and is also carried in the shared repository.

Next, implement bounded asynchronous RGB/depth readback in AnimationAnalysis and
verify acquisition identity, teardown, queue/memory bounds and measured overhead
in both the neutral host and Katana. Moving skeletal sampling follows. No animation
tuning, contact-quality verdict, full penetration detection or broad combat
baseline claim is part of this extraction.
