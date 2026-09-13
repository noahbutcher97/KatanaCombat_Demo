# AnimationAnalysis consumer integration - 2026-09-11

Katana now consumes published revision
`3fd91eb70be340db63778a697b12465ab895cd8a`, containing asynchronous readback
implementation `89d3f27c451dee746f0ae39514b4c5ae079ac26d`, in place of
`ba13149d3318f80d3098958cd1d2cd52bba3e5d1`. The standalone workspace is clean and
its remote main resolves to the selected revision. Fresh consumer checks cover the
rebuilt module, synchronous compatibility, opt-in async RGB and finisher recovery.
One initial synchronous PNG queue overflow remains a recorded reliability concern;
the unchanged failing scenario passed both targeted repetitions.

## Changes

1. Extended `Tools/AnimationAnalysis/animation_analysis_dependency.py` and its
   dependency tests to install, protect and hash shader resources alongside source.
   Included `.usf`/`.ush` in `run_scenario.py` source identity; shader edits invalidate
   that identity. The installer preserves the prior generated plugin for rollback.
2. Updated the lock and installed all 17 native source/resource files from the tracked
   public repository. Exposed the shared
   opt-in readback and diagnostic-resolution settings through `FCombatCaptureSettings`;
   preserved synchronous defaults and the existing surface API.
3. Added a rendered Katana API observation control for async capture, including native
   completion identity, ordinary telemetry, bounded frame completion and PNG-failure
   handling. Existing synchronous console/API controls remain in place.
4. Added readback mode and diagnostic-resolution policy to offline comparison
   compatibility. Legacy captures default to synchronous; invalid policies fail.
   Async reports distinguish enqueue cost from completion latency and explicitly
   state that the motion analyzer does not validate the terminal readback stream.

No standalone implementation was edited. The calibration-sensitive Python decoder
and pixel detector bytes are unchanged between pins. This is not authorization to
reuse a surface calibration across the new asynchronous native decoder identity.

## Fresh verification

| Check | Result |
|---|---|
| Dependency Python checks | 11 passed, including installed/modified/ignored shader controls |
| Capture Python checks | 79 passed, including shader provenance and readback-policy compatibility |
| KatanaCombatEditor build | Passed with two compiler jobs and UBA disabled |
| `KatanaCombat.Capture` under NullRHI | 23 successful test results; GPU-only async/surface checks defer here |
| `KatanaCombat.Editor.PairedEvaluation` | 11 passed |
| Rendered `KatanaCombat.Capture.PIE` | Three passed: ThirdPerson console, DefenseMatrix API, ThirdPerson async API |
| Rendered `KatanaCombat.Capture.Surfaces` | Two passed; five surface observations also replayed offline |
| Rendered finisher scenarios | ThirdPerson Completed/Interrupted and DefenseMatrix Interrupted passed initially; DefenseMatrix Completed passed both targeted repeats after the initial recording failure |

The async API capture contains **167 motion samples and 14 frames**, with valid
offline motion/PNG integrity and no reported issues. The observed combined pipeline
peaked at one frame and 1,766,536 reserved bytes. This small observation is a
compatibility control, not a throughput or performance benchmark.

The first build exhausted Windows' memory commit capacity during parallel PCH
allocation (C3859/C1076, OS error 1455). The bounded retry passed without changing
system settings. Both build logs remain in the evidence.

The initial DefenseMatrix Completed run reached all gameplay outcomes but stopped
recording at the four-frame PNG queue bound: 58 completed images, one rejected
request, zero PNG write failures. Its recording and derived visual checks correctly
failed or remained inconclusive. The underlying PNG writer bytes and synchronous
queue bound are unchanged from the previous pin. Two fresh repetitions with the
same source, cadence and limits passed; this does not prove the intermittent
overflow fixed or attribute its cause to the dependency update. No limits or
acceptance thresholds were relaxed.

Initial batch: `Saved/CombatScenarioRuns/20260911T142408-30185a41/`.
Targeted repeats: `Saved/CombatScenarioRuns/20260911T143351-223ba2c9/`.
The existing scenario runner analyzed and evaluated each recording, including the
unsuccessful one; its original result remains retained.

## Reproduction and limits

```powershell
python Tools/AnimationAnalysis/setup_dependency.py
python -m unittest discover -s Tools/AnimationAnalysis -p "test_*.py" -v
python -m unittest discover -s Tools/CombatCapture -p "test_*.py" -v
python Tools/CombatCapture/run_scenario.py --map all --variant all --mode rendered
```

For native checks, use the repository build/test commands with the scoped filters
above. Rendered checks used D3D11, offscreen rendering and a 960x540 launch viewport.
The async fixture explicitly disables AA and opts into diagnostic full resolution,
then restores its viewport flag. Ordinary scenario capture remains synchronous.
Exact commands, exit codes, source/binary identities, logs and parsed test paths are
retained under the evidence root.

The full combat baseline and the standalone host/performance campaign were not
repeated: this change affects the dependency installer, editor adapter, provenance
and observation controls. Fresh Katana checks establish consumer compatibility;
the upstream report supplies separately recorded standalone evidence.

## Disposition and preservation

The paired-facing correction remains the next project-system task after this
integration. This delivery does not make moving skeletal surface or penetration
analysis available, and it does not establish a project performance improvement.

`verification.json` and `retention.json` record asset/WIP preservation, replay
results, source identity, archive entry hashes and exact image cleanup. The failed
queue-overflow capture is retained alongside the passing captures. Restore archived
frame paths before repeating image-dependent analysis; the portable surface report
remains usable without loose PNG files.

All 7,948 Content size/time records and 54 protected asset hash/absence records
match their pre-integration snapshots. Both earlier source-review documents and
the separate plugin workspace remain unchanged. All **630 generated PNGs** were
archived and hash-verified before removal; zero remain in the scoped output trees.
The archive contains 1,206 verified file entries, is 370,450,632 bytes, and has
SHA-256 `dd0daec4d5037ce56252c8137993912d3c1c02c889aabda320d4a5b4ff70942e`.

Evidence root: `Saved/Logs/AnimationAnalysisIntegration-20260911-140421/`.
