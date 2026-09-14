# Finalized animation pose: Katana consumer trial

## Assessment

The candidate supplies eligible synchronized observations for this ordinary live
AnimBlueprint finisher. That specific live-pose blocker is resolved in the isolated
consumer trial. The workflow is useful for **partial pre-material geometry diagnosis**;
the requested region-pair interval remains **insufficient**. Full-region query cost,
excluded masked/PDO coverage and the coarse geometric region prevent a complete
visible-contact qualification. No gameplay/content correction or artistic acceptance
is inferred from these results.

The final completed run acquired seven pairs, with four measured queries and three
search-budget failures. Two intersections fall inside the requested montage interval;
a third belongs to the trailing guard sample. An interrupted control retained only
three pairs and correctly remained insufficient. Both final rendered scenario
evaluations passed their registered gameplay/pose/RGB checks; those checks do not
certify the separate mesh-contact question.

## Revisions and isolation

- Primary Katana base: `8411cb45bfab3cfcb3ddc0f7cbb0d0366c0bcfac`, branch
  `investigate/finisher-source-pair`.
- Primary accepted AnimationAnalysis pin remains
  `2fb0dc980dbdba1348b02e3a93b4512606aa5d16`.
- Trial: `D:\UnrealProjects\5.6\KatanaCombat\Saved\FinalizedPoseTrial`, branch
  `integrate/finalized-pose-trial`.
- Exact tested revision: `ea890e38dced51e21503c5ff637b26ac0b6170b5`, originally from
  `D:\UnrealProjects\Plugins\AnimationAnalysis\Saved\FinalizedPoseWorktree`.
  The revision was unpublished when the trial began. On 2026-09-13,
  `git ls-remote https://github.com/noahbutcher97/AnimationAnalysis.git refs/heads/main`
  confirmed that upstream `main` now points to this exact commit. No dependency
  bytes changed after qualification; this consumer did not publish shared source.
- Dependency commit: `e9246996dd8912f9027cbae87513a43ed7dc4826`.
- Consumer implementation commit: `c2cec986b4ce0320bbcd9b310d8f7e874c9f2236`.

The sparse trial checkout excludes tracked Content and uses a junction to primary
Content. No assets were saved. The five pre-existing primary WIP paths retained their
SHA-256 hashes, and all 7,948 Content file size/mtime records matched preflight. The
primary lock, installed plugin and editor binaries were not updated. Shared-library
source was read only; the installer created a separate detached candidate checkout
and generated plugin inside the trial.

## Environment and commands

Windows build 26200; Python 3.11.9; Intel Core Ultra 9 275HX, 24 cores/threads;
NVIDIA GeForce RTX 5090 Laptop GPU, driver 32.0.16.1074. Installed UE
`5.6.1-44394996+++UE5+Release-5.6`; D3D11 SM5; offscreen RGB at 960×540.
Target: `KatanaCombatEditor Win64 Development`.

From the trial root:

```powershell
python Tools/AnimationAnalysis/setup_dependency.py --repository D:/UnrealProjects/Plugins/AnimationAnalysis/Saved/FinalizedPoseWorktree
& 'C:/Program Files/Epic Games/UE_5.6/Engine/Build/BatchFiles/Build.bat' KatanaCombatEditor Win64 Development "-Project=$PWD/KatanaCombat.uproject" -Progress -NoHotReload -MaxParallelActions=2 -NoUBA
python -m unittest discover -s Tools/AnimationAnalysis -p test_*.py
python -m unittest discover -s Tools/CombatCapture -p test_*.py
python Tools/AnimationAnalysis/verify_distribution.py --output '<new evidence>/distribution'
```

Native filter: `KatanaCombat.PairedAnimation+KatanaCombat.Capture.ScenarioPlacement+KatanaCombat.Capture.Mesh.AuthoredFinisherReference`.
Rendered filter: `KatanaCombat.Capture.PIE.ThirdPersonAsyncAPI+KatanaCombat.Capture.Surfaces+KatanaCombat.Capture.ImageWriter`.
Both were issued through `UnrealEditor-Cmd.exe` with
`-ExecCmds="Automation RunTests <filter>;Quit"`, unattended/no-pause/no-splash,
stdout and `-DDC=InstalledNoZenLocalFallback`. Native used NullRHI; rendered used
`-RenderOffScreen -windowed -ResX=960 -ResY=540`. Exact expanded commands, durations,
logs, environment and source/binary hashes are retained with the evidence.

The [workflow guide](../guides/LIVE_FINISHER_MESH_EVALUATION.md) gives capture and
analysis commands. Final captures used `KATANA_MESH_BATCH=1`, separate output roots,
the ThirdPerson `rear-reference` placement and `contact_oblique` camera. Existing
transient paired-warp tuning was `.15–.45` seconds, offset `(42,0,0)` cm,
`match-partner-heading`, primary sync `.466667`, sync nudge disabled. Completed and
Interrupted were separate rendered processes; source, gameplay tuning and pose
evaluation were not changed to admit a mesh observation.

## Verification

| Check | Result |
|---|---|
| Exact installer / full affected-module build | Pass; full build 153.09 s |
| Final observer rebuild | Pass; engine build 3.96 s |
| Dependency tooling | 11/11 |
| Consumer Python after review fixes | 104/104, including 9 new binding/coverage controls |
| Pinned package with images | 105/105 |
| Isolated wheel distribution | Verified; 11 commands; optional images absent |
| Native Katana filters | 70/70 |
| Rendered API/surface/image-writer controls | 5/5 |
| Final Completed scenario | Pass; process/evaluation exit 0 |
| Final Interrupted scenario | Pass; process/evaluation exit 0 |
| Historical five-pair replay | Five expected acquisition mismatches retained; controls pass |
| Fresh default SingleNode authored reference | Ten records produced; five independent-acquisition mismatches retained; controls pass |

These are Katana integration checks. The delivery's 26 native, 53 tooling and 105
package results are upstream qualification, not additional Katana tests. Broad Katana
baseline qualification remains follow-up work.

The first rendered attempt was inconclusive: synchronous replay export introduced
a later 0.150563 s pose gap and inadequate RGB cadence. Its complete evidence remains
retained. The observer now holds bounded immutable snapshots and exports at world
cleanup. A subsequent trial and both final scenarios passed. Snapshot retention
costs 57,622,908 reserved bytes for seven pairs; final teardown reported zero retained
snapshots. Process termination before export can still lose unpublished evidence.

Review fixed non-finite montage inputs that could evade gap checks and prevented
silent enrollment of another player after retirement of the original participant.
NaN/type controls ran; actual participant destruction was not separately exercised.

## Live inventory and intended surfaces

- Montage: `/Game/ProjectFiles/Animation/Montages/Defense/GateA/AM_Finisher_Attacker.AM_Finisher_Attacker`
  (existing asset path retained as provenance).
- Victim: `SKM_FuturisticMercenary_FullBodyC`, ordinary skeletal component,
  `/Game/ProjectFiles/Animation/ABP_SamuraiCharacter.ABP_SamuraiCharacter_C`.
- Weapon: the attacker's actual `WeaponMesh`, static `SKM_Katana`, directly attached
  to its skeletal mesh's `weapon_r` socket. The victim's visible weapon is a different
  component and was not silently substituted.
- Finalized samplers enrolled early in real playback and witnessed later natural
  finalizations. Every capture ran outside the world tick with a common batch
  acquisition/request/frame and individual completion, configuration and pose keys.
- Recorded live inventories show no linked/post-process/leader instances, physics
  blend, forced reference pose, pending evaluation, active morph weights, mapped cloth
  sections or effective analysis-LOD deformer. Excluded features remain excluded;
  absence in this inventory is not a general deformation-coverage certificate.
- Victim analysis/render object/predicted LOD are 0: 38,374 vertices and 198,873 indices.
  Rigid analysis LOD 0 has 3,424 vertices and 18,816 indices; its actual rendered LOD
  remains unobserved. GPU Skin Cache grouping was not used.
- Victim slots 6 and 8 use masked `M_FacialHair` with PDO. The retained selection has
  2,282 triangles in those sections. This is material inventory, not anatomical inference.

The tracked region profile retains exactly 13,599 victim triangles from the earlier
first-pose centroid box and all 6,272 weapon triangles. Topologies:

```text
victim d9fa2a84731e446f5713242ff09d6ca4baa54de15b7698bb86e142f322984748
katana c20a79101003bfc5b920f876f75d6fa15a48772a147884e3832eadc4aa347399
```

This is a geometric neighborhood around an authored head/neck location, including
other nearby geometry, and the complete weapon. It is not an anatomical neck region
or a blade-only selection. Regions were not reselected from live poses, material
sections or weights. A more specific gameplay question needs reviewed explicit
triangle selections in Katana, not a shared-library anatomy heuristic.

## Measurements, coverage and cost

Predeclared question: selected victim region versus complete attacker weapon, montage
`0.40–0.55` s, centimetres, 0.5 cm proximity, requested 30 Hz, maximum 0.05 s gaps.
Seven guard requests at `0.375+n/30` bracket that interval. Neither exact montage
endpoint was observed; no endpoint interpolation or clock relabeling was performed.

| Final actual montage s | Result | Minimum distance cm |
|---:|---|---:|
| 0.386988 | Search budget exhausted; leading guard | — |
| 0.425706 | Measured separation | 39.823116 |
| 0.463190 | Search budget exhausted | — |
| 0.494912 | Search budget exhausted | — |
| 0.520146 | Sampled surface intersection | 0 |
| 0.549335 | Sampled surface intersection | 0 |
| 0.589642 | Sampled surface intersection; trailing guard | 0 |

All seven acquisitions completed without pair loss. Actual acquisition gaps were
25.095–46.001 ms; mean cadence 29.580 Hz across a 0.202842 s acquisition span. Per-pair
capture cost was 10.411–10.896 ms; inventory added 6.570–7.424 ms. These seven samples
do not establish sustained 30 Hz operation, and this instrumentation has material cost.

The first full-region query used 100,000 pair tests / 200,000 node visits and exhausted
the node bound after 303.986 s (86,395 triangle tests). It produced no distance. The
unchanged full regions were then evaluated with an explicit diagnostic resource
ceiling of 1,000 pair tests / 10,000 nodes. Final queries cost approximately 4.54–7.64 s
each, including three work-limit failures. Limits, individual stats and exact result
identities are preserved. This is offline reference analysis, not real-time performance.

`summarize_mesh_interval` retains the three insufficient inputs; its observed-span
summary remains insufficient despite three sampled intersections. Required visible
analysis separately requires material-displacement and raster coverage and rejects
all seven. Containment and between-sample behavior remain `not_evaluated`. Surface
intersection, proximity, physical contact and artistic acceptance are different claims.

The Interrupted run stopped with `playback_ended_before_interval`, three acquired
pairs and four missing attempts. Its requested interval is not bracketed; an actual
50.534 ms acquisition gap also exceeds the limit. It was not reinterpreted as a shorter
successful interval. All three diagnostic geometry queries exhausted their budgets.

Native controls retained exact reasons for exhausted snapshot admission, duplicate
components and absent subsequent finalization, with zero partial output and zero
budget leaks. Python controls rejected wrong topology, generation, pose and actual
acquisition, required cloth, exhausted analysis limits and an actual over-limit gap.
The gap assertion checks its specific reason, not merely an insufficient status.

## Visual cross-check and remaining work

Original RGB frames were inspected, with shared projection of measured nearest
points. At frame 784, the separated result projects to the victim head area and the
attacker's raised horizontal weapon; the apparent nearby vertical weapon belongs
to the victim. At frame 787, the intersection projects near pixel `(562,317)` into
overlapping arms/weapons. Occlusion prevents identifying exact contact from that view.
The last two intersection samples have no exact engine-frame RGB image; adjacent
images were not used as same-pose evidence. Details and original image hashes are
in `visual-inspection.json`.

Pass back to AnimationAnalysis: the selected ordinary finalized AnimBlueprint +
rigid batch path works in Katana. Review full-region search cost using the retained
13,599×6,272 query and exact work-limit evidence. Material/raster coverage remains a
separate unmet requirement; this trial justifies discussing that specific deferral,
not broadening all pose/deformation paths or GPU grouping automatically.

Katana owns the next region-authoring decision: select and review the intended
anatomical patch and weapon sub-surface explicitly before interpreting these generic
intersections as a finisher cut. Do not narrow regions merely to obtain a pass.
The tested upstream revision is now published on `main`. Adoption into the primary
checkout still requires review and integration of the consumer branch. The primary
combat work can continue independently.

## Durable evidence

Evidence root: `Saved/Logs/FinalizedPoseConsumer-20260913-164015` in the primary Katana
workspace. `live-final/live-batch` and `live-interrupted/live-batch` contain native
replay; corresponding `*-analysis/measurements.json` contain public API results.
`first-pair-probe.json` retains the original expensive failure. `historical-replay`
and `authored-current-analysis` retain legacy mismatch results. The original observer
source was recovered and verified against its frozen first-run source hash before
archival; final consumer sources are also retained.

Final Completed RGB: trial `Saved/CombatCaptures/20260913T211051-5AF7099345A2DDF9561C02B2BF2C8852`.
Final Interrupted RGB: trial `Saved/CombatCaptures/20260913T211126-209BF91247C292AAB848ADB1551B1EDE`.
Scenario batches, expanded commands, source/asset/binary hashes and logs are under
trial `Saved/CombatScenarioRuns` and included in the archive. Archive verification,
restored replay comparison and PNG cleanup results are recorded below.


Archive: `finalized-pose-consumer-evidence.zip`, 318,649,997 bytes.
SHA-256: `b18dfc2a3a87959c5d553c586e926e45e9e581c859917ad61343ba3a3a5a3a89`. All 993 payloads verified
against the archive manifest. Native bundles restore under
`restored/evidence/live-final/live-batch` and
`restored/evidence/live-interrupted/live-batch`. Both restored analyses reproduced
all geometry, identities, coverage, controls and gaps exactly; only wall-clock
analysis timing and the source-directory label were excluded from comparison.

Archive members use `evidence/` for the primary evidence root and
`trial/Saved/` for RGB, scenario and rendered-control records. Historical source
bundles remain in the separately preserved earlier integration archive
referenced by `MESH_REGION_PAIR_INTEGRATION_2026-09-13.md`; this archive retains
their current replay results and hashes.

After archive and replay verification, 511 generated PNGs
were individually hash-checked and removed from the owned trial capture directories.
Remaining loose PNGs in those directories: 0. Images remain
recoverable from the archive; local HTML image links require restoring the matching
`trial/Saved/CombatCaptures` members. `archive-verification.json`,
`restored-replay-verification.json` and `cleanup.json` are retained sidecars.
