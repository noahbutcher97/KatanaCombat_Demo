# Combat stabilization publication checkpoint

The combat stabilization branch is ready for draft review. The existing WIP is
recorded in five commits, with each code or asset slice built independently in an
isolated checkout. The complete committed tree passes the full headless baseline,
focused rendered checks, and Python tests. Merge remains a separate review step.

## Commit boundaries

Branch: `codex/combat-action-reaction-stabilization`.
Previous local HEAD: `6e170503b5958d84d4c065cfaf6e18704546df7a`.

| Commit | Scope | Isolated editor build |
|---|---|---|
| `a35de50a` | Pinned AnimationAnalysis dependency, installer and CI setup | Pass |
| `7858f90f` | Combat input ownership, hold behavior and paired lifecycle | Pass |
| `0cb98264` | Existing authored content and proof dependency repairs | Pass |
| `d3c507a0` | Project capture adapters and paired animation evaluation | Pass |
| `1dd26215` | Verification, usage and ownership documentation | Same code as previous slice |

This publication task preserves the existing gameplay and asset changes. Its new
implementation changes make the dependency URL available through the lock file,
default installation command and Unreal CI job. The installer has a focused test
for installation from the lock's repository URL. Repository guidance also records
the requested commit-message convention.

## Public dependency

[AnimationAnalysis](https://github.com/noahbutcher97/AnimationAnalysis) is public.
Its published `main` is `0dda6bd37ecb520fa70c46f30ec5d0075269df99` at this
checkpoint. Katana continues to pin
`ba13149d3318f80d3098958cd1d2cd52bba3e5d1`; ongoing shared-library development
does not change the installed project dependency automatically.

Fresh project setup now uses:

```powershell
git lfs pull
python Tools/AnimationAnalysis/setup_dependency.py
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Codex/run-agent-baseline.ps1
```

The isolated checkout fetched the pinned dependency from its public GitHub URL.
The project checkout was a detached worktree sharing the local Git/LFS object store;
its full Content tree was checked out without junctions to the primary workspace.
This establishes checkout/build reproducibility, not a complete project download
from an empty remote clone. Remote LFS availability is checked separately when the
branch is pushed.

## Committed-tree verification

Tested revision: `1dd2621590c1cf59124263a7a200d28e32458174`.
The publication record and final guidance changes affect documentation only.
Source/configuration/dependency identity across 459 files:
`ce2c4834dda6f9248353a8ed0b0c50d6b94933f813be07f84fb5e5c5eb9957dd`.
Four editor DLL hashes are retained separately.

| Check | Result |
|---|---|
| Four successive `KatanaCombatEditor Win64 Development` slice builds | All exit 0 |
| Standard build and full `KatanaCombat` automation baseline | 778 discovered, 778 unique successful results; exit 0 |
| Automation failure/error records | 0 |
| Automation warning records | 481; retained for inspection |
| `KatanaCombat.Capture.PIE` and `.Surfaces`, offscreen D3D11 at 960x540 | 4 successful results; exit 0 |
| Dependency installer Python suite | 7 pass |
| Project capture Python suite | 77 pass |
| Git LFS object and pointer integrity | Pass |
| Changed workflow YAML and commit whitespace | Pass |

Headless automation explicitly defers one GPU-only surface control; the separate
D3D11 run exercises it. Neither run establishes finisher choreography, intended
grip/contact placement, continuous penetration detection, or production performance.
The 481 warnings are not assertion failures and are not all classified as harmless.
The next gameplay task remains the source-pair review described in
[the finisher contact investigation](FINISHER_VISUAL_CONTACT_2026-09-10.md).

## Preservation and evidence

All 54 protected Content paths retain their original hash or absence, including
the seven added assets and five already-deleted assets. The primary workspace's
7,948 Content size/time records are unchanged. The isolated checkout's Content
metadata also remains unchanged after testing. Existing asset bytes were committed
without saving or authoring new asset changes during publication.

Evidence root: `Saved/Logs/GitPublication-20260911-121905`.
It contains the pre-commit file archive and hashes, commit/path ledger, four build
logs, full baseline logs, complete result and warning records, source/binary
identities, and preservation checks. The runner log prefix is
`Codex-Agent-Baseline-20260911-082950`; those logs were copied into
`committed-baseline` beneath the evidence root.

The new capture bundles were archived and every entry hash checked before all 36
generated PNG files were removed. `capture-evidence.zip` is 19,936,079 bytes,
SHA-256 `673ea96252a128ac7bd7f7aa0940c3f72563618e7ece8bf01cbfd399ff9b388e`.
Separate `test-artifacts.zip` retention includes raw surface observations,
authoring/evaluation outputs and engine logs. Restore the corresponding archive
entries before replaying image- or surface-dependent analysis. Generated evidence
and build outputs remain outside Git.
