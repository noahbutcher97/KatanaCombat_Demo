# Paired entry transition and support

Refine the unsaved initiator approach after the 2026-09-13 qualification. Preserve
the existing working files and AnimationAnalysis pin. No Content package saves.

1. Inspect authored walk, directional walk, and finisher starting poses. Add an
   explicit source-time offset to entry presentation in `PairedAnimationTypes.h`,
   `PairedAnimationComponent_Entry.cpp` and native/Python entry tuning. Preserve
   old defaults, instance ownership and single-cycle duration limits. Verify source
   seconds versus montage-track seconds with a non-unit asset rate control.
2. Correct named fixture setup using a reusable test-side floor preparation helper.
   Query the actual CMC/capsule, bound the adjustment, reject unsupported floors,
   and retain requested/achieved poses and floor evidence in the capture. Extend
   `CombatCaptureScenarioTests.cpp`, placement validation and native/Python controls.
   Grounding runs before public input only, never during entry or playback.
3. Run the editor build and paired/alignment/placement regressions plus capture
   Python tests. Capture old-phase and selected-phase controls on both maps,
   interruption, and a directional oblique candidate. Compare actual transition
   poses, source rates, actor/foot motion, floor continuity and sampling gaps.
4. Record which effects improved and which still require choreography. Do not
   promote the shared live-mesh coverage or interpret pose distance as artistic
   acceptance. Archive/hash-verify replay before removing generated PNGs; commit
   verified, reviewable consumer slices without mixing unrelated WIP.

Acceptance: no setup-induced vertical step at movement restoration on the selected
flat floors; settings reach the requested source phase without retiming evidence;
unchanged ownership/no-premature-damage controls; reviewable animation comparisons.
Generic navigation, terrain-following entry and automatic turn choreography remain
separate work. A directional candidate is not arbitrary-heading qualification.

Executed on 2026-09-13. See the [qualification report](../audits/PAIRED_ENTRY_TRANSITION_2026-09-13.md):
floor continuity and phase selection passed; the selected phase and directional
clip remain comparison candidates. Root-motion velocity and artistic acceptance
remain follow-up work.
