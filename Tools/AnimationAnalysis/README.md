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
and copies its native source into ignored `Plugins/AnimationAnalysis`. The project
descriptor already enables that plugin. Run setup before building a fresh Katana
checkout. The public clone URL is tracked; no developer-specific path is required.
Use `--repository <checkout-or-URL>` to explicitly override the source for local work.

Edit shared Python/native code in the standalone repository, verify and commit it,
then update the full revision in the lock and run setup again. The installer checks
committed source bytes, rejects dirty/unowned copies, and preserves the previous
generated plugin under `Saved/AnalysisDependencies` when changing revisions.
Rebuild `KatanaCombatEditor` after changing the native dependency. Generated copies
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
