# Paired facing policy and finisher comparison - 2026-09-11

The reusable paired-facing policy is implemented and verified after the
[AnimationAnalysis integration](ANIMATION_ANALYSIS_INTEGRATION_2026-09-11.md).
Existing assets retain `FacePartner`; explicit `FaceAwayFromPartner` and
`MatchPartnerHeading` are available on each role's `FPairedWarpConfig`.
No saved asset or contact threshold changed.

During this work the independent plugin workspace advanced to documentation-only
commit `2453ad8` (skeletal-sampling research). Its source, shaders and Python
implementation still match Katana's verified `3fd91eb` pin; the shared working tree
is clean. The consumer pin remains frozen to the revision used by these captures.

## Implementation and verification

- `CombatTypes.h`, `PairedAnimationTypes.h` and `PairedAnimationUtilityLibrary`
  define the policy, retained-request field and actor-independent yaw resolver.
- `TargetingComponent` uses it for direct paired setup/pre-update and retained
  request acquisition/update. Disabled rotation, coincidence fallback, target
  ownership and the existing antipodal turn convention are preserved.
- `PairedAnimationComponent` carries each role's policy into retained stages and
  checks its required turn against existing budgets. Default bridge defender
  checks retain the resolved defense-contact yaw. Invalid policies fail validation.
- The transient `paired-warp-tuning` runner accepts `--victim-facing-policy`.
  An explicit policy requires a sixth before/after override, validated by both
  Python and native evaluators. The complete role configuration is restored.

Both editor builds passed. The final full `KatanaCombat` baseline completed
**784 tests with zero failures**, process exit 0. This includes four new public-API
facing tests, policy-aware retained-stage transition/budget checks, and native
facing-provenance validation. Capture Python automation passed **81 tests**.
GPU-only checks defer under NullRHI; rendered plugin checks are recorded in the
separate integration audit. No new friend declarations were added.

The first focused run retained two failed test expectations. Direct tracking
stops on partner destruction but its endpoint remains until explicit named
cleanup; retained targets refresh through owner updates, not an automatic
pre-update subscription. Tests now exercise those existing contracts. No lifecycle
behavior was changed merely to satisfy those initial expectations.

## Rendered comparison

Eight D3D11 recordings passed gameplay, restoration and capture checks:
Completed/Interrupted on ThirdPerson/DefenseMatrix with the existing policy and
with transient `MatchPartnerHeading`. Both batches used the same source identity
`c6b24a9850eb69702d77dc131225250e3b1be11a2ab3e882243b2437e40c089e`.
The fixed diagnostic controls permit paired root motion, disable attacker
translation warping, retain victim translation/rotation, use the original effective
victim window `0.0001..0.872983634` seconds and retain the 50 cm runtime offset.
These are experimental recordings, not untouched saved-asset behavior.

| Map / policy | Heading difference near 0.3 s | Near 0.6 s | Near 0.9 s | Minimum sampled torso-proxy gap, 0.3..1.5 s |
|---|---:|---:|---:|---:|
| ThirdPerson / default | 171.01 deg | 173.06 deg | 180.00 deg | 39.10 cm |
| DefenseMatrix / default | 171.21 deg | 173.46 deg | 180.00 deg | 38.95 cm |
| ThirdPerson / matching | 130.96 deg | 62.90 deg | 0.00 deg | 35.06 cm |
| DefenseMatrix / matching | 130.03 deg | 63.39 deg | 0.00 deg | 34.90 cm |

Rows use actual nearest samples, not interpolated poses; exact clocks are in
`comparison.json`. Contact-interval observation gaps range up to 16.96-23.31 ms
across the recordings. A sampled separation does not establish continuous absence.

Four native evaluations completed with valid runtime provenance. In each,
`BladeToUpperTorso` fails and `EntryRootSeparation` passes. `ReleaseLeftHand` fails
with the default policy and passes with matching headings. Relative-alignment
checks still fail against the unchanged opposed-heading authored profile;
matching runtime headings are deliberately different from that reference. No
aggregate geometric or animation-quality pass is claimed.

The first DefenseMatrix native attempt correctly rejected the default profile:
the capture uses `SKM_Manny_Simple`, while that profile names the mercenary mesh.
The rejection is retained. Generated map-specific profiles use the actual captured
role meshes, changing no contact/alignment criteria or saved assets. Comparisons
are within each map; the two meshes are not treated as interchangeable calibration.

## Independent visual and timing findings

All 48 selected raw frames (12 per completed recording, approximately 0.25..1.5 s)
were inspected in fixed central crops. Full originals remain embedded in portable
raw/projected reviews. Separate reports use the shared visual-analysis contract
and explicitly identify assistant image review, not an automated contact detector.

- Default facing produces the frontal restraint mismatch from the source review.
  Matching headings puts the attacker behind the victim in later restraint poses.
  The large turn during entry remains a concern: matching is not reached near 0.6 s.
- Hands, blade and bodies obscure the likely contact region. Exact visible
  blade-to-skin contact and penetration remain **indeterminate**. The existing
  `spine_03` torso sphere is still a provisional and anatomically questionable proxy.
- In all four completed recordings, health drops from 100 to 0 at the first paired
  observation, at attacker montage time 0.01667-0.01806 s. Later reviewed frames
  show the blade raised before its downward motion. This confirms the early health
  transition in gameplay; it does not timestamp the exact damage callback or select
  a new impact event.
- Foot support, full recovery presentation, audio and haptics were not visually
  evaluated by this selected-frame review.

The next project-specific work is bounded entry alignment and impact/contact
authoring review before saving a finisher correction. Source initial spacing is
not a runtime warp endpoint. Keep the current control, define the intended head/neck
contact region, and relate the primary sync event to the intended strike interval.

## Evidence and reproduction

Evidence root: `Saved/Logs/PairedFacing-20260911-144506/`.
Default batch: `Saved/CombatScenarioRuns/20260911T163919-bd6554e9/`.
Matching batch: `Saved/CombatScenarioRuns/20260911T164211-cbf337ba/`.

| Map | Default visual analysis | Matching visual analysis |
|---|---|---|
| ThirdPerson | [Default](../../Saved/Logs/PairedFacing-20260911-144506/ThirdPerson-default-visual-analysis.html) | [Matching](../../Saved/Logs/PairedFacing-20260911-144506/ThirdPerson-matching-visual-analysis.html) |
| DefenseMatrix | [Default](../../Saved/Logs/PairedFacing-20260911-144506/DefenseMatrix-default-visual-analysis.html) | [Matching](../../Saved/Logs/PairedFacing-20260911-144506/DefenseMatrix-matching-visual-analysis.html) |

```powershell
python Tools/CombatCapture/run_scenario.py --map all --variant all --mode rendered --finisher-experiment paired-warp-tuning --victim-warp-window .0001 .872983634 --victim-warp-offset 50 0 0
python Tools/CombatCapture/run_scenario.py --map all --variant all --mode rendered --finisher-experiment paired-warp-tuning --victim-warp-window .0001 .872983634 --victim-warp-offset 50 0 0 --victim-facing-policy match-partner-heading
```

See the [evaluation guide](../guides/PAIRED_ANIMATION_EVALUATION.md) for the native
evaluator and portable review commands. Generated profiles, commands, parsed
results, rejected attempts and source identity accompany the recordings.
`verification.json` and `retention.json` record preservation checks, archive hashes
and exact image cleanup. Restore archived frame paths for image-dependent replay;
portable visual reports work without loose PNGs.

Preservation checks passed for all 7,948 Content size/time records, 54 protected
asset hash/absence records and both prior source-review documents. The verified
archive contains 1,437 entries (592,530,024 bytes), SHA256
`7b05d6196f8aa024f02e8c2e6878f3c4aaed5e70eb9febc5a7db742789ab7189`.
All **901 generated PNG files** from this phase were archived, hash-verified and
removed; zero remain in the scoped outputs. The integration phase separately
archived and removed 630 PNGs. Changes remain local and uncommitted.
