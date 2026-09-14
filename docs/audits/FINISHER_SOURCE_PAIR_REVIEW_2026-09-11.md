# Finisher source-pair review - 2026-09-11

The source pair reads as a **rear approach, head restraint and downward strike**.
Matching participant headings with **100 cm initial root spacing** produces a
coherent source control. The current victim warp instead turns toward the attacker.
That is a concrete configuration mismatch to address before further placement tuning.

The strike passes near the head, while the provisional `BladeToUpperTorso` criterion
uses an 18 cm sphere at the lower `spine_03` origin. These are separate findings:
correcting facing does not make that criterion an anatomical contact test.
**Exact visible surface contact remains indeterminate** because the blade is partly
occluded. No runtime or asset correction is saved by this review.

## Scope and evidence

This completes the source-review step from the
[baseline disposition](COMBAT_BASELINE_2026-09-11.md#disposition) and the
[earlier visual contact investigation](FINISHER_VISUAL_CONTACT_2026-09-10.md).
Project HEAD is `50685e86dc2a284408f9f8fa6fbcedd37efed470`, on the local
`investigate/finisher-source-pair` branch. AnimationAnalysis remains pinned to
`ba13149d3318f80d3098958cd1d2cd52bba3e5d1`; its separate development repository was
not modified.

Generated evidence is under `Saved/Logs/FinisherSourceReview-20260911-124728/`:

- [Portable four-control comparison](../../Saved/Logs/FinisherSourceReview-20260911-124728/source-controls.html)
  retains all 64 original images, source clocks, camera/pose observations and hashes.
- [Separate visual analysis](../../Saved/Logs/FinisherSourceReview-20260911-124728/visual-analysis.html)
  records ten actually inspected frames through the existing shared review library:
  three concerns, two bounded consistent findings, one indeterminate finding and two
  unreviewed aspects. `review_recorded` is not an automatic quality pass.
- `source-assets.json` retains fresh asset properties and 147 poses per role at
  60 Hz over the shared attacker duration. `source-control-measurements.json` retains
  finite blade-segment distances to named bone origins, without contact radii or
  pass thresholds.
- `render-pose-validation.json` compares 512 rendered root/head/hand observations
  with independently exported source poses. Maximum disagreement is **0.006203 cm**.
- `same-facing-contact.json` and `native-evaluation/` retain the candidate profile,
  native report and runner evidence. Original criteria are unchanged.

These local generated links are not checked into Git. The evidence archive retains
the inspection/render scripts and exact command lines as well as images and reports.
The tracked paired-evaluation guide remains the reusable command reference.

## Source identity and capture method

The inspected pair is
`/Game/ProjectFiles/Data/PDA/Defense/GateA/DA_Finisher_GateA`.
Its attacker/victim montages are `AM_Finisher_Attacker` and `AM_Finisher_Victim`
under `/Game/ProjectFiles/Animation/Montages/Defense/GateA/`, both using section
`Finisher`, rate scale 1 and zero victim start offset. These are existing asset
paths, not names introduced by this work.

AssetRegistry reports exactly one hard AnimSequence package dependency for each
montage: `GhostSamurai_Ambush01` and `GhostSamurai_Ambushed01` under
`/Game/Assets/Animations/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Execution/`.
Python reflection does not expose montage slot tracks here; the dependency lookup
is recorded explicitly rather than represented as a reflected track read.
Both sequences enable root motion and use reference-pose root lock. Attacker duration
is approximately 2.433 s; the victim lasts 4.2 s. Full victim recovery was not reviewed.

The source views use the profile's CyberpunkRunner attacker mesh and ThirdPerson
FuturisticMercenary victim mesh, sharing `SK_Mannequin`. The attacker carries
`SKM_Katana` using `DA_Weapon_Katana`, its existing `weapon_r` socket and identity
mesh attachment offset. The victim weapon is omitted to reduce occlusion. These
views therefore do not establish full gameplay presentation parity or DefenseMatrix
mesh compatibility.

An isolated UE 5.6 Katana process renders a transient unsaved blank map through
SceneCapture2D, D3D11, 1024x768, with two fixed diagnostic cameras. Single-node
playback supplies paused source poses. Its root translation was locked, so the
renderer restores each source root's translation relative to its entry pose once
on the actor. The sampled roots have fixed identity orientation and unit scale;
those assumptions are checked. This bounded source inspector is not a general
montage, blend or motion-warp sampler.

The initial wrong-rotation and root-locked image sets were rejected. Only the final
64-frame set passes the independent pose comparison above. Failed reflection/API
attempts and rejected image sets remain identified in the retained evidence;
none contribute to the visual findings. The live UEMCP editor belonged to another
project, so it was not used for these operations.

## Facing and spacing controls

All controls use attacker mesh yaw -90 degrees. Matching victim yaw is -90 degrees;
opposed victim yaw is +90 degrees. Each control has side and elevated views at
0, 0.3, 0.45, 0.6, 0.9, 1.3, 1.8 and 2.3 seconds of source animation time.
The four side views at 0.6 s and six additional views of the matching-heading
100 cm control were inspected. The retained page exposes every other frame for
further review without treating it as already reviewed.

| Initial spacing | Relative heading | Finding in the inspected controls |
|---|---|---|
| 50 cm | Opposed, as in the current diagnostic profile | Bodies cross; the restraint and weapon action do not align with a rear approach |
| 100 cm | Opposed | Extra distance does not resolve the frontal-facing mismatch |
| 50 cm | Matching | The attacker's source advance carries the bodies across each other |
| 100 cm | Matching | Rear restraint, downward action and release form a coherent source sequence |

At 0.6 s the attacker has advanced approximately **56.98 cm** relative to its entry
root, while the victim has advanced approximately **0.63 cm**. Consequently,
100 cm initial spacing is **not** a proposal to set the runtime warp offset to
100 cm. The moving partner-relative endpoint and the initial source placement
must be specified separately.

The current pair config disables attacker translation warping but enables attacker
rotation. Victim translation and rotation are enabled, with `RelativeOffset=(50,0,0)`.
In [TargetingComponent.cpp](../../Source/KatanaCombat/Private/Core/TargetingComponent.cpp),
`SetupVictimWarp` and `OnVictimMotionWarpingPreUpdate` both resolve enabled rotation
with `UprightPairedFacing(Owner, WarpLocation, AttackerLocation)` (lines 1735 and 1812
at the recorded HEAD). This explicitly faces the attacker.

[FPairedWarpConfig](../../Source/KatanaCombat/Public/Data/PairedAnimationTypes.h)
contains a rotation enable flag but no heading policy. The older
`VictimFacingMode`/`VictimRelativePosition` fields in
[PairedAnimationData.h](../../Source/KatanaCombat/Public/Data/PairedAnimationData.h)
are marked unwired. Setting those fields cannot express the required behavior.
Disabling victim rotation only retains an existing heading; it does not reliably
establish the aligned entry shown by the source control.

## Grip, anatomical target and timing

The visible wind-up and downward action show the handle in the attacker's right
hand without an obvious reversed attachment in the inspected views. The weapon
origin is not its grip, and the skeletal socket already supplies a substantial
offset. Identity `MeshAttachOffset` is therefore not evidence of a grip defect.
The transformed authored `weapon_grip` remains **9.182 cm** from the right wrist
bone in the sampled source poses. A wrist-to-grip distance is not a palm-fit metric;
fingers and the exact handle seating remain partly hidden.

For matching headings at 100 cm, the minimum finite blade-segment distances over
0.3..1.5 s are **2.401 cm to `head`** at 0.5667 s, **11.507 cm to `neck_01`**
at 0.6 s, and **49.486 cm to `spine_03`** at 1.2667 s. These are distances to bone
origins, not skinned surfaces. The visual action supports head/upper-neck intent;
it does not establish an exact anatomical entry point, penetration depth or
continuous contact. Do not promote a head sphere solely because it yields a pass.

The fresh native authored evaluator, with the original profile criteria preserved,
reports:

| Criterion | Result | Observed signed gap range |
|---|---|---|
| `BladeToUpperTorso` | Fail | 31.486..125.083 cm from the existing 18 cm `spine_03` sphere |
| `EntryRootSeparation` | Pass | 47.639..62.027 cm |
| `ReleaseLeftHand` | Fail | 107.898..210.819 cm; the victim separates beyond the configured 200 cm maximum |
| Runtime relative-alignment checks | Not run | No gameplay capture supplied |

The release failure is not evidence that the hand failed to release. Its current
criterion also imposes an upper separation bound. Define the desired recovery
behavior before changing that bound.

Both montages have primary damage-configured sync notifies at nominal montage time
zero. The attacker event is active at entry; the victim trigger offset is +0.0001 s.
Their authored region is `spine_03`. **Runtime damage commit was not measured.**
The source strike develops later, so event timing deserves a synchronized gameplay
comparison; a nominal notify timestamp alone does not prove early health loss.

## Concrete correction plan

1. Add an explicit facing policy to paired warp configuration, preserving existing
   face-partner behavior by default. Support matching partner heading for rear
   choreography and use one resolver for setup and pre-update in both participant
   roles. Cover default behavior, enabled/disabled rotation, changing partner heading,
   upright rotation and invalid partner handling with focused tests. Do not revive
   the unwired legacy field or add a finisher-name special case.
2. Compare a transient matching-heading finisher control with the source reference.
   Keep the existing grip initially. Express entry placement separately from the
   moving contact-time warp endpoint; account for source root motion and the saved
   movement-disabling notify flags. Retain the existing opposed control for comparison.
3. Record damage commit alongside source/montage time and exposed weapon views.
   Specify the anatomical target and strike interval from that evidence, then revise
   the diagnostic criteria explicitly. Preserve today's failed torso/release results
   and keep visual/surface judgments separate from geometric pass states.
4. Run fresh Completed and Interrupted captures on both ThirdPerson and DefenseMatrix
   using the existing raw/projected review pipeline. Verify entry, strike, interruption
   cleanup, release and terminal recovery before promoting saved authoring or a
   visual reference. Inspect visible contact and record occlusion/penetration limits.

The immediate implementation is the reusable facing-policy seam in Katana's paired
runtime configuration. It does not depend on the shared agent's asynchronous
readback or moving-surface work. This review adds no shared-suite implementation.

## Verification and retention

The explicit source inspection and final rendered source process exited 0. Native
evaluation built the editor successfully (up to date) and exited 0; its Python
runner exited 1 because the preserved provisional criteria fail. This is a measured
diagnostic failure, not an editor or capture-instrument failure.

All 64 embedded image hashes and the ten reviewed-frame references pass the pinned
shared evidence/review contract. The portable viewer's JavaScript parses. This
docs/evidence task makes no runtime, test, dependency or asset edits; the earlier
778-test baseline is not represented as a new run here. No fresh gameplay scenarios
were run.

`retention.json` records the evidence archive hash and entry verification. All
**128 generated PNGs**, including rejected controls, were archived and verified
before removal; **zero loose PNGs remain**. The portable page retains all 64 valid
source frames. `evidence.zip` is 136,012,070 bytes, with 175 verified file entries;
SHA-256: `7f2cbf1bac161a5b4a8f47f6646a07d10e67ec1f04b916557da75a6ebde54de9`.

`final-checks.json` confirms that all **7,948 Content metadata records**, **54
protected asset hash/absence records**, and **459 source/configuration/dependency
hashes** are unchanged. Both documentation files pass whitespace and local-link
checks. Nothing is staged or committed.
