# Initiator finisher approach

Prototype an attacker moving into the reviewed rear entry pose while the victim
holds its accepted position. Keep assets unsaved and AnimationAnalysis pinned to
`2fb0dc980dbdba1348b02e3a93b4512606aa5d16` during shared capability review.

1. Inventory compatible in-place walking/turning clips, root motion, notifies and
   source timing. Select the straight rear approach first.
2. Extend `PairedAnimationTypes.h` and `PairedAnimationComponent_Entry.cpp` with an
   optional moving role and explicit single-cycle in-place movement presentation.
   Preserve the existing victim-moving default and bounded swept executor.
   Supervise animation instance ownership; cancellation must preserve a replacement.
3. Extend `PairedEntryTuning.h`, capture validation and the transient PIE fixture to
   record these settings. No gameplay defaults or Content packages change.
4. Extend `PairedEntryTests.cpp` for initiator goals, anchored victim, obstruction,
   cancellation, replacement and early readiness. Build the editor; run entry,
   bounded alignment and paired regression tests plus capture Python tests.
5. Capture completed and interrupted rear approaches and a ready-at-request
   control. Inspect feet and transition frames, report movement/cadence evidence,
   and retain oblique direction mismatch as a separate limitation if observed.
6. Write a durable audit with exact commands, source/asset identities and outcomes.
   Archive and verify replay evidence before removing owned generated PNGs.

Acceptance is mechanical safety plus a reviewable presentation experiment.
Obstruction/cancellation are native controls; rendered runs establish the actual
AnimBlueprint presentation. Neither sampled movement nor images establish mesh
contact, continuous collision, planted feet or artistic acceptance. Navigation,
floor following, root-motion approach execution, and staged turning remain outside
this bounded prototype.

Completed 2026-09-13: runtime slice `6fe35835`, 71 native regressions, 93 Python
tests and eight rendered gameplay scenarios pass. The [qualification audit](../audits/INITIATOR_FINISHER_APPROACH_2026-09-13.md)
retains visual findings, exact source identity and verified replay hashes.
Next authoring work is gait-phase/velocity continuity into the finisher, deliberate
oblique choreography and DefenseMatrix floor height at movement restoration.
