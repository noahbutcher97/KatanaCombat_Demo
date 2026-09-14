# AnimationAnalysis mesh delivery: Katana consumer qualification

## Assessment

The exact merged dependency is integrated and its consumer compatibility checks
pass. **The selected live katana/victim region-pair workflow is not yet usable for
surface distance or intersection.** Both live skeletal participants use an
unsupported AnimBlueprint pose path. A separate eligible authored reference also
exposes incompatible native cross-component acquisition stamps. No timestamps,
live animation modes, acceptance criteria or assets were changed to obtain a pass.

The immediate shared requirements are finalized ordinary AnimGraph/montage poses
and a qualified relationship between acquisitions of two components. Effective
masked/PDO facial-hair materials additionally block a visible-surface assertion.
The [shared-worker handoff](ANIMATION_ANALYSIS_CONSUMER_HANDOFF_2026-09-13.md)
records these bounded requests. It has been prepared in Katana for the worker;
no shared source or subsequent shared development commit was edited here.

## Versions and preservation

| Item | Verified identity |
|---|---|
| Katana base/branch | `ae5994b70e850a00505ef527d4278f1ef6ea0ef9`, `investigate/finisher-source-pair` |
| Previous dependency | `3fd91eb70be340db63778a697b12465ab895cd8a` |
| Installed dependency | `2fb0dc980dbdba1348b02e3a93b4512606aa5d16`, merged [shared PR 1](https://github.com/noahbutcher97/AnimationAnalysis/pull/1) |
| Python | `animation_analysis` 0.4.0; CPython 3.11.9 |
| Fresh wheel SHA-256 | `4db2151222cb9516c9242d2642462f9921df32aa884a12d2155568082ab59526` |
| Final rendered capture source/config/tool/dependency identity | `9c1316eea9841b547155eb6c68427608045a92e3e58ff12016f3b55797fb9e9d` (536 files) |
| Engine | UE 5.6.1, changelist 44394996, Windows build 26200, D3D11 rendered consumer checks |

The installer used the explicit local shared repository as its source, checked out
the requested commit detached under `Saved/AnalysisDependencies`, verified source
bytes and refreshed 24 installed plugin source/resource files. It preserved the
previous generated plugin copy. The project Python resolver now selects this same
pin. Full affected editor modules were rebuilt, including AnimationCapture and
both Katana editor/test adapters. There was no Python-only integration.

The dependency consumer slice is commit
`38e17443adaafe2c5339f68f88e284be71486678`. The qualification slice is committed
separately on the same branch; `commit-receipt.json` in the evidence root records
both exact consumer commits after publication to local Git.

All **15 pre-existing WIP files remain byte-identical**, and all **7,948 Content
files retain their initial sizes and modification times**. This is not a full
Content byte-hash comparison. Twenty-two selected asset/material package SHA-256s
are recorded separately. Existing WIP is preserved outside the new consumer
commits. The selected live scenario includes that pre-existing finisher-placement
WIP; the archive contains its exact source bytes and tracked-file patch. Do not
interpret the live result as evidence from a clean base checkout alone.

## Predeclared workflow and actual inventory

Criteria are in [the scoped plan](../plans/MESH_REGION_PAIR_INTEGRATION.md):
ThirdPerson completed rear-reference paired finisher, attacker montage time
**0.40–0.55 s**, katana surface versus a geometric victim head/upper-neck neighborhood,
centimetres, **0.5 cm** proximity tolerance, target **30 Hz**, maximum acquisition
gap **0.05 s**, exact interval endpoints and no missing required coverage.
Proximity, triangle intersection and containment are separate questions.

The paired data is `/Game/ProjectFiles/Data/PDA/Defense/GateA/DA_Finisher_GateA`.
The montages are `AM_Finisher_Attacker` and `AM_Finisher_Victim` under
`/Game/ProjectFiles/Animation/Montages/Defense/GateA/`. These are existing asset
identities; the new tools/tests use descriptive names.

The final opt-in live inventory ran at montage time **0.403363 s** in
`/Game/ProjectFiles/Levels/Lvl_ThirdPerson1`, during the actual paired scenario.
The first inventory at 0.400092 s is retained separately.

| Effective component | LOD 0 geometry | Actual live path |
|---|---|---|
| Attacker `SKM_CyberpunkRunnerr_B` | 32,451 vertices / 58,512 triangles | `ABP_SamuraiCharacter_C`; predicted and render-object LOD 0 |
| Victim `SKM_FuturisticMercenary_FullBodyC` | 38,374 vertices / 66,291 triangles | Same AnimBlueprint; predicted and render-object LOD 0 |
| Equipped `SKM_Katana` | 3,424 vertices / 6,272 triangles | Ordinary static component, direct attacker attachment at `weapon_r` |
| Sheathe `SKM_Katana_Holster` | Inventoried separately | Direct attacker attachment at `weapon_back`; outside selected region |

Both characters had 89 required bones, animation mode 0, no post-process
class/instance, leader, reference-pose override, running parallel evaluation at
the callback, simulated physics or physics blending. Mesh-to-capsule transforms
retained yaw -90 degrees and Z -90 cm. Their effective morph weight arrays were
empty, no external morph sets or LOD-0 deformer instances were found, and all
resident LOD-0 sections had no cloth mapping. These are callback-time observations,
not whole-interval absence proofs. The inspection did not change live pose
evaluation. The existing scenario retains its documented always-refresh pose policy.

The effective victim facial-hair slots 6 and 8 are masked and use pixel-depth
offset; reported WPO usage is false. Other inventoried effective materials reported
no WPO/PDO usage. The attacker's used null material slot remains explicitly unknown.
Full material paths, section ranges, attachment transforms and topology hashes are
in `verified/live-inventory.json`. Static render LOD is **not observed**; requested
reference analysis LOD is explicitly 0. No final rendered surface was acquired.

CPU enrollment rejected all four live components; GPU enrollment rejected both
skeletal components with this exact error:

```text
unavailable: pose ordering requires finalized single-node animation without leader, post-process, physics blending or reference-pose override
```

Thus **zero eligible live acquisitions** cover the required interval. There were
no GPU requests admitted or live geometry results to complete. We stopped live
measurement at eligibility and continued independent integration/reference checks.
The inventory includes synchronous topology inspection overhead and is not a live
capture-performance benchmark.

## Separate authored reference and shared API results

The explicit paused single-node fixture uses those meshes, source sequences
`GhostSamurai_Ambush01` / `GhostSamurai_Ambushed01` under
`/Game/Assets/Animations/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Execution/`,
same-heading initial roots 100 cm apart and source root motion. It samples source
times 0.400000, 0.433333, 0.466667, 0.500000 and 0.533333 s at analysis LOD 0.
Its intended surface is expressly a pre-material CPU bone/rigid reference, accepting
the documented exclusions for morph, cloth, mesh deformer, material displacement
and raster visibility. It is not a live or final-rendered substitute.

All **10/10 native observations** exported successfully and pass their individual
requirements. All also replayed using the newly built wheel in a separate isolated
environment. Completion/acquisition stamps, engine frames, observer-local pose
revisions and component/configuration generations survive replay unchanged.

The victim region contains **13,599 explicit global triangle ordinals**, selected
by triangle centroids inside a first-pose head-centered world-axis box with
half-extents **12/12/15 cm**. The katana region contains all **6,272 triangles**.
The selections are fixed across the sequence, not inferred from skin weights or
material labels. The geometric projection was inspected; it is a head/neck
neighborhood, not certified anatomical segmentation. A subsequent section audit
finds 2,282 selected triangles mapped to the masked/PDO facial-hair slots.

| Identity | SHA-256 |
|---|---|
| Victim topology, also matching live inventory | `d9fa2a84731e446f5713242ff09d6ca4baa54de15b7698bb86e142f322984748` |
| Katana reference topology | `c20a79101003bfc5b920f876f75d6fa15a48772a147884e3832eadc4aa347399` |
| Victim region | `2a6121ca7f30ec89cdb4f24d7e84bc53edf03404f4eadfc238aa3d05b1c0c087` |
| Katana region | `e2991c931fb72e396c033dfef50ad64765e4057a61dabf7d1aa9b0b6f76723e8` |

`measure_mesh_pair` returns **insufficient for all five pairs**, specifically
`second:acquisition_mismatch`. `summarize_mesh_interval` retains those failures and
returns insufficient, with no observed pair minimum. Capturing both components
within one engine frame does not satisfy equality of independent monotonic stamps.
The adapter never changes either stamp. This is a shared acquisition/analysis
contract limitation, not a measured miss, separation or intersection.

A final Python-only participant guard additionally binds replay request/role,
asset and LOD to the declared native reference inventory. Its fresh replay is in
`verified/analysis-final/`; the original acquisitions and decisions are unchanged.
A manifest naming a different victim asset was rejected before measurement.
`delivery-source.json` records this final adapter revision separately from the
rendered capture identity. Timing values below refer to `verified/analysis/`.

| Final authored sweep timing | Observed range |
|---|---|
| Victim acquisition call, excluding export | 10.31–11.00 ms |
| Rigid katana acquisition call, excluding export | 0.425–0.443 ms |
| Victim export | 13.64–15.62 ms |
| Katana export | 4.31–5.60 ms |
| Original two-component acquisition skew | 24.16–26.04 ms |
| Consecutive victim acquisition gaps | 30.63–33.30 ms |
| Consecutive katana acquisition gaps | 30.96–33.36 ms |
| Shared/local peak retained snapshot reservation | 6,669,860 bytes; snapshots released after each export |
| Offline rejected pair call | 0.423–0.554 ms; median 0.456 ms |

This is an **unpaced synchronous authored sweep in one engine frame**, with poses
explicitly finalized for the fixture. Source-time spacing is not acquisition
cadence. Roughly 31 wall-clock acquisitions per second in this short sweep does not
establish sustained 30 Hz live capture. Sequential export contributes to skew and
gaps; capture latency and export cost are reported separately. Desktop workload was
uncontrolled. The initial sweep had a genuine 51.54 ms gap exceeding the declared
bound; that insufficient result remains archived rather than relabeled.

The body/weapon algorithm stops before geometry search, so those sub-millisecond
numbers measure **eligibility rejection only**. Full-region search time, live GPU
mesh cost and real-time performance remain unmeasured. No requests/exports were
lost in the authored sweep; all five requested pair measurements are ineligible.

Retained controls on original native records passed:

- One selected katana triangle against itself: measured distance 0 cm, intersection
  true, one pair test/node visit, 0.922 ms service time. This is a same-surface
  identity control, not two-body contact.
- Wrong region topology and wrong component generation: insufficient, exact
  mismatch reasons, no distance.
- Required cloth against explicitly excluded reference coverage: insufficient.
- Whole katana against a one-triangle analysis budget: insufficient, triangle-limit
  reasons and no partial distance.
- Actual first/last stamps with an intentionally smaller gap limit: insufficient;
  intermediate observations were omitted explicitly, never retimed.
- One-byte shared native capture budget: no geometry, exact error
  `unavailable: combined capture byte or reservation admission exhausted`.

Containment remains `not_evaluated`. Sampled intersection, proximity and even a
complete sampled interval would not establish continuous collision, penetration
depth/volume, physical contact or artistic acceptance.

## Fresh verification and reproduction

| Check | Result |
|---|---|
| Katana editor build including changed native dependency | Pass, final `build-verified.log` |
| Isolated Python wheel, without image extra | 104 pass / 1 optional-image skip |
| Same wheel with image extra | 105/105 pass |
| Shared tooling, invoked from the pinned consumer checkout | 26/26 pass |
| Katana dependency installer/resolver controls | 11/11 pass |
| Existing Katana capture Python regression | 90/90 pass |
| Isolated native host via consumer pin | 8/8 NullRHI controls; separate host, not Katana gameplay evidence |
| Katana rendered compatibility | 5/5: DefenseMatrixAPI, ThirdPersonAsyncAPI, ThirdPersonConsole, LabelOwnership, RenderedGeometry |
| Katana authored finisher mesh test | Pass; final 10/10 exports and native exhausted-budget rejection |
| Selected completed rendered finisher | Pass twice; final capture `20260913T165727-37B74D6D4693DC4ADA2F798C294EF9BC` |
| Shared pair/interval qualification plus retained controls | Controls pass; selected pair/interval correctly insufficient |
| Isolated installed-wheel replay of final consumer records | 10/10 pass |

The shared delivery's previously recorded 24 rendered/native controls are not
claimed as fresh Katana evidence. Its new GPU mesh backend cannot acquire the
chosen live pose. The complete Katana baseline, interruption matrix, other region
pairs and broader deformation qualification were not run for this scoped update.

From the repository root, the principal commands were:

```powershell
python Tools/AnimationAnalysis/setup_dependency.py --repository D:/UnrealProjects/Plugins/AnimationAnalysis
& 'C:/Program Files/Epic Games/UE_5.6/Engine/Build/BatchFiles/Build.bat' KatanaCombatEditor Win64 Development -Project='D:/UnrealProjects/5.6/KatanaCombat/KatanaCombat.uproject' -NoHotReload -MaxParallelActions=2 -NoUBA
python Tools/AnimationAnalysis/verify_distribution.py --output <evidence>/PythonDistribution
python Tools/AnimationAnalysis/verify_unreal_host.py --engine 'C:/Program Files/Epic Games/UE_5.6' --output <evidence>/NativeHost
python -m unittest discover -s Tools/AnimationAnalysis -p 'test_*.py'
python -m unittest discover -s Tools/CombatCapture -p 'test_*.py'
```

Rendered native runs used `UnrealEditor-Cmd.exe`, the absolute project path,
`-D3D11 -RenderOffScreen -unattended -nopause -nosplash -stdout
-FullStdOutLogOutput -DDC=InstalledNoZenLocalFallback`, unique `-abslog`, and either:

```text
-ExecCmds=Automation RunTests KatanaCombat.Capture.PIE+KatanaCombat.Capture.Surfaces;Quit
-ExecCmds=Automation RunTests KatanaCombat.Capture.Mesh.AuthoredFinisherReference;Quit
```

The authored run and selected live run set `KATANA_MESH_OUTPUT` to
`<evidence>/verified`. This root is an explicit output argument, not a required
developer-local input. The authored test also works without that environment
variable and logs its generated output path.

The selected scenario command, using the retained placement-authoring WIP, was:

```powershell
python Tools/CombatCapture/run_scenario.py --map ThirdPerson --variant Completed --mode rendered --skip-build --placement rear-reference --camera-view contact_oblique --finisher-experiment paired-warp-tuning --victim-warp-window .15 .45 --victim-warp-offset 42 0 0 --victim-facing-policy match-partner-heading --primary-sync-time .466667 --sync-nudge disabled --entry-config <evidence>/entry-reference.json
python Tools/CombatCapture/measure_finisher_regions.py <evidence>/verified/authored-reference --output <new-analysis-directory>
```

Native command lines are retained in their logs; scenario commands, source and DLL
hashes are in the corresponding run contexts. Python distribution/host command
arrays are retained. Initial compile errors in the new consumer fixture were fixed.
The `final/` attempt used an older binary after a failed intermediate build and is
explicitly excluded; only `verified/` supplies the final authored/live results.

## Replay and retention

Evidence root: `Saved/Logs/MeshIntegration-20260913-123732/`. Key files:
`verified/live-inventory.json`, `verified/authored-reference/reference.json`, ten
schema-1 native bundle directories, `verified/analysis/regions.json`,
`verified/analysis/measurements.json`, `source-final.json`, `binaries-final.json`,
`selected-assets-sha256.json`, `preservation.json`, and `installed-replay.json`.
The original geometric region projection is retained as `analysis/region-selection.svg`.
Native record and buffer hashes are embedded in the measurement report and replay
markers. The separately verified native-host archive has SHA-256
`5c778a829e0381d6ad678e616a53bcc3deb8cdf761fc05bc7af94bfa46b3ac8d`.

Combined archive: `mesh-integration-evidence.zip`, **201,770,082 bytes**,
SHA-256 **`c27c83854fe1d5b49a6b6a83eb09ba015a8e7b85849c7e6e5408bc52b8ffd9fa`**.
All **751 payload entries** passed size/SHA-256 readback; the archive also contains
its explicit inventory. **330 generated PNG files** were archived, rechecked and
removed. The retention receipt is `archive-retention.json` and the complete
inventory is `archive-inventory.json`. Ten native bundles were then extracted from
the archive into a disposable directory and replayed successfully; see
`archive-replay-verification.json`. No owned PNG files remain loose. Three empty
PNG-named directories created by existing write-failure controls were also removed
individually after their control evidence was retained.

Evidence includes the pre-existing source WIP, all integration attempts and exact
selected capture directories. Replay restores the original repository-relative
paths; images are retained in the archive rather than left loose. New consumer
commits preserve the prior WIP separately. The delivered source identity after the
last Python participant guard is
`c32a5760e91f49f402d79ac32a54012092f4d1e0dad2360d46bba8456d9180c8`.
