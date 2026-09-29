# Finisher authoring

Build a coherent gameplay finisher from a reviewed source reference. Keep the
already-published runtime/capture checkpoint separate from subsequent authoring.
PR #124 (merged 2026-09-14) contains that checkpoint; entry and new facing
policies remain opt-in.

Status, 2026-09-29: steps 1-2 are complete; step 3 is partly complete; steps 4-5
remain. Later 2026-09-13 slices built on the comparison below:

- [Initiator approach](../audits/INITIATOR_FINISHER_APPROACH_2026-09-13.md): the
  attacker walks a straight rear approach toward an anchored victim (unsaved
  prototype; ownership, obstruction, cancellation and recovery verified).
- [Entry transition](../audits/PAIRED_ENTRY_TRANSITION_2026-09-13.md): fixture
  floor grounding and explicit source-phase entry. The evaluated 0.375 s phase is
  not an accepted improvement.
- [Finalized-pose trial](../audits/FINALIZED_POSE_CONSUMER_TRIAL_2026-09-13.md):
  live mesh-pair observation works for partial geometry diagnosis; visible-contact
  qualification of the impact interval remains insufficient.

Next: the finisher's rapid forward advance (gait-phase/velocity continuity into
paired playback), deliberate oblique choreography, contact approval of the impact
timing, then step 5.

History, 2026-09-13 morning: steps 1-2 have a refreshed
[source reference and visual review](../specs/FINISHER_AUTHORING_REFERENCE.md).
The candidate strike interval is 0.45-0.483333 source seconds; exact skin contact
remains indeterminate. The subsequent
[approach comparison](../audits/FINISHER_APPROACH_COMPARISON_2026-09-13.md) retains
the existing source advance and 0.25-second blend for an aligned rear start.
Six rendered transient cases pass, including a 0.466667-second impact candidate
and interruption before impact. Production approach for distant/other contexts,
contact approval and saved asset authoring remain pending.

1. Refresh the source pair and attachment properties in an isolated unsaved editor
   process. Reuse the existing 60 Hz source-pose exporter and validated renderer.
   Record source asset hashes and check the 100 cm, matching-heading reference.
2. Inspect closer side, opposite-oblique and overhead views around the source
   strike. Record the intended region and candidate impact interval with exact
   frame references. Bone proximity and projected overlap cannot certify skin
   contact. Preserve the earlier failed torso criterion.
3. Use that reference to select a supported approach/turn or blend treatment,
   keeping starting pose separate from the moving montage warp endpoint. Inventory
   available animations before selecting one. Implement only a demonstrated need.
4. Test transient gameplay settings for facing, preparation, warp and impact time.
   Exercise multiple starting distances/headings, obstruction, cancellation before
   playback, interruption before impact and normal completion. Check movement,
   visible contact, input recovery and ownership cleanup independently.
5. Promote the reviewed settings to the exact paired-data/montage assets, verify
   them through normal gameplay, and commit that asset work separately with visual
   evidence. Do not save intermediate diagnostic configurations.

The source reference pass changed documentation and generated artifacts; the
approach comparison also updates capture fixture/provenance tooling. Neither
changes production behavior, dependency pins or saved assets. Preserve Content
and current source hashes, and archive/hash-verify exact generated PNG files before
removing them. Full rendered-surface/penetration tooling is a separate dependency
integration task and is not assumed available for this reference pass.

## Approach and transition comparison

The next implementation slice inventories candidate approach/turn sequences and
the saved montage blend settings through an isolated Unreal inspection. It adds
named participant placements to the Katana capture driver so rear source placement
can be compared through the same public finisher input and recovery path.

Planned files are the scenario placement parser, `CombatCaptureScenarioTests.cpp`,
the Python runner/evaluator and focused parser tests, the registered finisher
scenario, and the evaluation guide. Default placement behavior stays explicit.
Record the requested placement and the actual transforms at the request boundary;
reject malformed or mismatched placement evidence rather than inferring setup.

Acceptance: build the editor; exercise valid/invalid placement controls; capture
the rear 100 cm reference with transient matching facing, permitted root motion
and candidate impact time; compare preparation and early montage movement against
the earlier frontal fixture. Inspect rendered entry/contact and capture the full
recovery. Select an approach animation or blend change only if this evidence
demonstrates that it is needed. Keep experimental settings unsaved.
