# Finisher authoring reference

Recorded 2026-09-13 at `ae5994b70e850a00505ef527d4278f1ef6ea0ef9`.

The current source pair supports a rear restraint with a strike near the head or
upper neck. Matching headings and 100 cm initial source-root separation form the
working reference. Visible blade arrival is around **0.45-0.483333 source seconds**;
this is a candidate interval for gameplay comparison, not a proven skin-contact
bound or an approved damage-event time.

The [authoring plan](../plans/FINISHER_AUTHORING.md) separates this reference from
approach/transition selection, gameplay verification and saved asset authoring.
[PR #124](https://github.com/noahbutcher97/KatanaCombat_Demo/pull/124) (merged
2026-09-14) contains the preceding runtime/capture checkpoint. This reference pass changes no runtime code,
dependency pins or saved assets.

## Exact source and placement

Existing asset names below are identifiers, including legacy directory names.
They are not names for new workflows or tests.

| Role | Asset |
| --- | --- |
| Pair | `/Game/ProjectFiles/Data/PDA/Defense/GateA/DA_Finisher_GateA` |
| Attacker montage | `/Game/ProjectFiles/Animation/Montages/Defense/GateA/AM_Finisher_Attacker` |
| Victim montage | `/Game/ProjectFiles/Animation/Montages/Defense/GateA/AM_Finisher_Victim` |
| Attacker sequence | `/Game/Assets/Animations/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Execution/GhostSamurai_Ambush01` |
| Victim sequence | `/Game/Assets/Animations/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Execution/GhostSamurai_Ambushed01` |
| Attacker mesh | `/Game/Assets/Characters/CyberpunkRunner/Meshes/SKM_CyberpunkRunnerr_B` |
| Victim mesh | `/Game/Assets/Characters/FuturisticMercenary/Meshes/SKM_FuturisticMercenary_FullBodyC` |
| Shared skeleton | `/Game/Assets/Characters/Mannequins/Meshes/SK_Mannequin` |
| Weapon data | `/Game/ProjectFiles/Data/PDA/Weapons/DA_Weapon_Katana` |
| Weapon mesh | `/Game/Assets/Characters/CyberpunkRunner/Meshes/SKM_Katana` |

Both configured montage sections are `Finisher`, rate scales are 1, and the victim
start offset is zero. Each montage has exactly one hard animation-sequence
dependency, which supplies this source reference. Python reflection did not expose
the montage slot-track layout: segment trimming, section mapping and effective
runtime playback rate still need explicit verification before converting a source
sample into a montage event time.

In the isolated source renderer, the attacker starts at `(0,0,0)` and the victim at
`(100,0,0)` cm, both with mesh yaw -90 degrees. This maps the mannequin's authored
forward direction into world +X. These are source mesh-root transforms, not
character capsule-center transforms. Production use must account for each
character's mesh-to-capsule transform.

The weapon attaches to `weapon_r` using the existing identity attachment offset
and unit scale. `weapon_start` and `weapon_end` define the measured finite blade
segment. The victim weapon is omitted from these diagnostic renders.

The fresh RAW source export contains 147 samples per role. The sequence identities
and every exported pose sample exactly match the
[September 11 source review](../audits/FINISHER_SOURCE_PAIR_REVIEW_2026-09-11.md).
Entry-relative root translation is restored once at the actor level while the
single-node mesh pose has root translation locked. Source root rotations are
identity and scales are one for these samples.

## Authored motion and strike landmarks

At time zero the attacker is behind the victim. At 0.3 seconds the attacker has
advanced into a left-hand mouth/jaw restraint while raising the blade. The source
animation itself therefore supplies much of the advance into the strike.

Distances below use exported source poses and the existing weapon sockets. Bone
origins are geometric reference points; these numbers are not skin clearances.

| Source time (s) | Root separation (cm) | Blade to head origin (cm) | Blade to spine_03 origin (cm) |
| --- | ---: | ---: | ---: |
| 0 | 100.000 | 162.798 | 137.313 |
| 0.150000 | 56.907 | 120.927 | 114.689 |
| 0.300000 | 47.639 | 113.150 | 135.802 |
| 0.433333 | 43.716 | 45.018 | 91.860 |
| 0.450000 | 42.748 | 22.035 | 71.275 |
| 0.466667 | 41.783 | 13.125 | 59.738 |
| 0.483333 | 42.124 | 9.546 | 56.657 |
| 0.566667 | 43.588 | 2.401 | 50.426 |
| 0.600000 | 43.721 | 3.166 | 49.904 |

Close side and front-oblique views show the blade above and clear of the head at
0.433333 seconds. At 0.45 seconds it reaches the head silhouette; at 0.466667 and
0.483333 seconds parts of the blade are occluded near the rear/side of the head
and visible beyond it. The overhead view supplies another projection, but shows
projected overlap even before the side views suggest arrival. Projected overlap
alone is insufficient evidence of physical contact.

The sampled blade-to-head-origin minimum is **2.401 cm at 0.566667 seconds**. It
occurs after visible arrival and should not be selected as first impact merely
because it minimizes the distance. The next transient gameplay comparison should
test source samples **27, 28 and 29 at 60 Hz** (0.45, 0.466667 and 0.483333 seconds),
with explicit conversion to the active montage clock. Sample 28 is a comparison
candidate, not a saved authoring decision.

The apparent head/upper-neck intent conflicts with the inherited
`BladeToUpperTorso` sphere at `spine_03`. Keep the original failed criterion and
its measurements. Define the intended anatomical contact region independently;
do not choose a head radius from the observed gap just to obtain a passing result.

Exact skin entry/exit, continuous contact and penetration depth remain
**indeterminate**. The head, hands and adjacent geometry obscure the contact
surface. No calibrated skin segmentation or surface reconstruction was run in
this pass. The plugin's newer CPU bone/rigid reference delivery is also not a
rendered skin-surface or full-penetration implementation.

## Consequences for gameplay authoring

Preparation should bring the pair to a suitable start for the authored motion.
It must not duplicate or consume the source's advance. The initial 100 cm source
separation is not a constant montage warp endpoint: it reduces to roughly 42 cm
during the candidate strike interval. Map both moving source roles and production
mesh transforms when constructing any endpoint.

The earlier frontal fixture required a visible victim turn to reach matching
headings. This source reference does not establish that such a prelude is good
choreography. First compare a rear starting context against the unwarped source;
then assess which approach/turn or blend treatment is supported for other
contexts. Existing animation filenames alone do not establish compatibility.

Next work (done 2026-09-13; see the
[approach comparison](../audits/FINISHER_APPROACH_COMPARISON_2026-09-13.md) and the
[authoring plan](../plans/FINISHER_AUTHORING.md) for current status) was to inspect
available approach/turn animations and the production blend path, then choose the
smallest demonstrated correction. Verify moving
entry, strike and full recovery in gameplay, including distance/heading variants,
obstruction, cancellation before playback, interruption before damage, ownership
cleanup and input recovery. Foot support and motion quality need their own
observations from continuous, stable-camera captures.

No saved facing, entry, warp or damage setting was changed by this reference
review. The previously observed early damage event and failed contact evaluations
remain open; see the
[bounded-entry audit](../audits/BOUNDED_PAIRED_ENTRY_2026-09-11.md).

## Evidence and verification

Local evidence root: `Saved/Logs/FinisherReference-20260913-134846/`.
The portable [source viewer](../../Saved/Logs/FinisherReference-20260913-134846/source-reference.html)
embeds all original images. The separate
[visual analysis](../../Saved/Logs/FinisherReference-20260913-134846/visual-analysis.html)
records observations, interpretations, limits and exact reviewed frame references.
These generated artifacts are local; they are not uploaded with the tracked doc.

- Fresh Unreal source-inspection process exited 0. Two isolated D3D11 render jobs
  exited 0 and produced 60 + 15 frames, at 1024x768, across four camera views.
- There are 20 sampled source times; the three close views cover every 60 Hz
  sample from 0.4 through 0.666667 seconds. Overview frames cover a smaller set.
- Visual review inspected 48 unique frames through six uncropped contact sheets
  and five full-size image views. The available 75 frames are not all claimed as
  visually reviewed. This is an evidence-linked review, not an automated quality
  pass.
- Independent RAW-to-world checks compared 1,350 pose points and 225 weapon
  points against the render observations. Maximum errors were 0.005996 cm and
  0.006113 cm, below the declared 0.01 cm limit. All 75 image hashes matched.
  This checks pose/observation agreement, not rendered-surface intersection.
- The portable evidence and visual-review schema validate; the viewer JavaScript
  passes syntax checking. No gameplay automation or build was rerun for this
  documentation and source-reference pass. Earlier mechanical test evidence is
  recorded separately in the
  [publication audit](../audits/PAIRED_WORK_PUBLICATION_2026-09-13.md).

Reproduction scripts, command arguments, process logs, exact source/asset hashes,
point measurements and frame manifests are retained beside the viewer and in the
archive. `final-checks.json` records post-run source and Content preservation;
`retention.json` records the archive hash, full entry verification and exact PNG
cleanup. Restore images from the archive if loose files are needed; the embedded
viewer remains usable after cleanup.
