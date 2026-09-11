# Finisher warp isolation

Date: September 10, 2026 (local); capture directories use September 11 UTC timestamps.

The late victim displacement is strongly driven by **rotation warping**, including world translation induced by rotating the offset skeletal mesh. Disabling victim translation alone does not remove it. With victim rotation warping disabled, the largest observed late-window step falls below 1 cm on both maps and actor pitch stays at zero. This is diagnostic evidence, not an approved finisher correction: an early approximately 15 cm step and failed contact/release criteria remain.

## Controlled results

All controls permit CharacterMovement through transient paired-collision notify copies. Each additional control changes one boolean on a duplicated instanced warp modifier. Input ownership, other notify properties, montage timing and disk assets are preserved. The fixture restores original objects and checks their flags and package state before reporting completion.

| Control | Late victim step, ThirdPerson / DefenseMatrix | Maximum victim pitch magnitude | Blade minimum signed gap, ThirdPerson / DefenseMatrix |
|---|---:|---:|---:|
| Movement permitted | 46.07 / 38.47 cm | 55.09 / 52.85 degrees | 47.39 / 47.82 cm |
| Attacker translation warp off | 26.57 / 24.72 cm | 55.50 / 52.20 degrees | 47.31 / 46.91 cm |
| Victim translation warp off | 33.34 / 31.81 cm | 55.04 / 54.79 degrees | 47.26 / 47.42 cm |
| Victim rotation warp off | **0.78 / 0.81 cm** | **0 / 0 degrees** | 44.70 / 44.74 cm |

Late step means the largest consecutive actor-position distance whose ending victim montage time is inside 0.84–0.90 s. Original simulation intervals are retained; the listed peak intervals are approximately 16.67 ms. Sampling phase differs between runs, so relative magnitudes are observations rather than a statistical effect estimate. Over the wider 0.80–0.96 s ending-time interval, the rotation control reaches 5.24 / 5.22 cm. Across 0.05–2.3 s it still reaches **15.45 / 15.35 cm near 0.13 s**. The opening alignment snap is outside these comparisons.

The blade criterion remains an unreviewed segment/sphere proxy allowing at most 8 cm signed gap. All eight completed-capture comparisons fail it without any inconclusive runtime cases. Entry-root separation passes. Release-left-hand fails in every control, including approximately 206.45 / 206.47 cm minimum gap with victim rotation disabled against the existing 200 cm allowance. No tolerance or reference was changed.

Attacker translation settings are a separate mismatch: its pair config disables translation, while the original montage modifier enables it. The targeting code supplies the owner's current location in that configuration; this does not disable modifier translation. Changing the actual modifier increases observed attacker travel from about 38 cm to 51 cm, but leaves a substantial victim spike. That flag alone is insufficient as a correction.

## Why translation-off still moves the actor

1. Recorded victim warp-target height varies by approximately **156 cm** on both maps. In the ThirdPerson translation-off capture, target Z changes from 298 cm to 453.38 cm around 0.567 s; target pitch changes from +2.46 to −71.93 degrees. Actor pitch later reaches approximately −55 degrees. The late actor spike can occur after the target has returned close to ground height, so checking only the target's displacement during the peak interval misses the preceding rotation.
2. `TargetingComponent.cpp` computes the victim target from attacker position plus rotated relative offset, adjusts Z to the sampled ground, then uses the full three-dimensional attacker-to-target direction for rotation. Ground height therefore affects pitch. `DebugUtils.cpp` uses `LineTraceSingleByChannel` with `ECC_WorldStatic`, ignores only the owning actor, and accepts a found hit without requiring the sampled walkability flag. A channel trace is not an object-type filter. The exact actor/component responsible for the elevated hits has **not** been recorded; nearby blocking character/weapon geometry remains a hypothesis.
3. Two additional native logging replays retain victim translation-off. In one ThirdPerson interval, the original and processed local translation are both **0.38 cm**, while the engine's converted world-translation prediction is **31.94 cm**. Unreal's `USkeletalMeshComponent::ConvertLocalRootMotionToWorld` composes the root-motion rotation with the mesh-to-actor offset. Its returned world translation can therefore be large even when the modifier preserves local translation. This prediction is distinct from final swept CharacterMovement displacement.
4. The victim rotation-off control retains translation warping and the unstable target heights. Actor pitch nevertheless stays zero on both maps and the late spike disappears. This isolates a substantial rotational contribution without claiming that every remaining movement defect shares that cause.

Local engine references: `MotionWarping/Private/RootMotionModifier_SkewWarp.cpp`, `RootMotionModifier.cpp`, and `Engine/Private/Components/SkeletalMeshComponent.cpp`. Project references: [victim target construction](../../Source/KatanaCombat/Private/Core/TargetingComponent.cpp), [ground query](../../Source/KatanaCombat/Private/Debug/DebugUtils.cpp).

Reviewed ThirdPerson PNGs around 0.75 s show the movement-permitted victim tilted forward with its legs lifted, versus the rotation-off victim closer to the kneeling pose. These frames support the transform diagnosis; they do not approve grip, contact, foot support or artistic quality. Exact frame paths and sample offsets are preserved in `review-frames.json`.

## Implementation and verification

Changed six source/tool files:

- `CombatCaptureSession.cpp`: additive cached/component target position and quaternion observations at world-tick end.
- `CombatCaptureScenarioTests.cpp`: transient per-role translation and victim rotation controls, instanced-object checks and restoration.
- `PairedContactProfileEvaluation.cpp` and `evaluate_capture.py`: exact accepted role/property combinations; reject missing, duplicate or unsupported edits.
- `run_scenario.py`: documented control selection and run provenance.
- `test_evaluate_capture.py`: accepted controls and wrong-role/property/index/missing/duplicate refusal checks.

Editor builds pass before each control expansion. Final-build paired evaluation passes **9/9**; offline tooling passes **31/31**. The matrix passes **16/16** completed/interrupted rendered scenarios across four controls and two maps. Two additional rendered logging replays pass. All **2,087 PNGs** finish with zero pending, rejected or failed images. Matrix samples confirm the requested modifier flags and zero attacker movement acceleration during paired input suppression. Gameplay checks cover takeover, completion/interruption cleanup, token release, movement recovery, fresh attack input and active bystanders.

The first twelve matrix runs used the translation-control build; four rotation-control runs and final paired tests used the subsequent build. Both builds preserve existing control behavior, and each run retains source/binary identity. All eight completed captures were evaluated by the native evaluator after adding the rotation control. A final text-encoding correction restored report punctuation and was rebuilt and checked without changing measurement code. Two deliberately malformed scratch bundles are refused with runtime `inconclusive`, including internally consistent wrong-role and missing-warp metadata. A commandlet exit of zero means the report completed; contact failure makes the Python driver return one, as expected.

No full automation rerun was needed for this editor/test-tooling diagnostic slice; the earlier full-suite result is not a current full-suite claim. Hash verification confirms **all 223 runtime files unchanged**, **all 54 protected Content paths unchanged**, and no additional changed Content paths. No assets were saved, no branch commit was made and no quality reference was promoted.

## Evidence and reproduction

Evidence root: `Saved/Logs/FinisherWarpIsolation-20260910-211451/`; pointer: `Saved/Logs/finisher-warp-isolation-current.txt`.

The root contains source snapshots/diff, build and automation logs, `movement-results.json`, `warp-target-results.json`, `rotation-diagnosis.json`, `native-root-motion-trace.json`, `contact-results.json`, `capture-verification.json`, `provenance-refusal-verification.json`, `scope-verification.json`, and reproducible analysis scripts. `warp-isolation-comparison.png` plots actor steps, target steps and target pitch. Raw images and native reports remain in their uniquely identified `Saved/CombatCaptures/` and `Saved/PairedAnimationEvaluations/` directories; these generated artifacts require separate archiving for long-term pixel review.

```powershell
python Tools/CombatCapture/run_scenario.py --map all --variant all --mode rendered --finisher-experiment permit-root-motion
python Tools/CombatCapture/run_scenario.py --map all --variant all --mode rendered --finisher-experiment attacker-source-translation --skip-build
python Tools/CombatCapture/run_scenario.py --map all --variant all --mode rendered --finisher-experiment victim-source-translation --skip-build
python Tools/CombatCapture/run_scenario.py --map all --variant all --mode rendered --finisher-experiment victim-source-rotation --skip-build
```

Evaluate completed captures with `evaluate_pair.py`, selecting `finisher-contact.json` for ThirdPerson and `finisher-contact-mannequin.json` for DefenseMatrix. See the [paired evaluation guide](../guides/PAIRED_ANIMATION_EVALUATION.md).

## Next corrective slice

Record the actual ground-query hit actor/component and walkability, reproduce the elevated-target case, then correct ground eligibility and upright paired-target rotation with focused regressions. Preserve movement and input ownership, and verify turning toward the partner still works; globally disabling rotation is only the control. Re-run this matrix and inspect the remaining early translation step before contact/damage-timing authoring. Reconcile the attacker's config/modifier policy as part of that correction. The remaining contact and release failures must retain their original criteria until authored intent is reviewed.
