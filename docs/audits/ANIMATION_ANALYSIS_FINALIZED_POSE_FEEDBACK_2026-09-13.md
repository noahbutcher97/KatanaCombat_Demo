# AnimationAnalysis feedback: finalized live Katana poses

Consumer trial of exact candidate `ea890e38dced51e21503c5ff637b26ac0b6170b5` is
complete. Katana rebuilt the native modules and used opt-in `FinalizedAnimation`
with mixed skeletal/rigid `CaptureBatch`. The ordinary live AnimBlueprint path
supplied all seven pairs without pose-mode substitution, acquisition retiming or
partial batch publication. This resolves the selected workflow's earlier live-pose
eligibility blocker. The primary Katana dependency has not been promoted.

The [full consumer report](FINALIZED_POSE_CONSUMER_TRIAL_2026-09-13.md) records exact
commands, source/pin identities, tests, inventory, replay and archive verification.
Implementation is consumer commit `c2cec986b4ce0320bbcd9b310d8f7e874c9f2236` on
`integrate/finalized-pose-trial`, following pin commit
`e9246996dd8912f9027cbae87513a43ed7dc4826`. No shared source was edited from Katana.

## Review requested

1. **Full-region search cost:** the unchanged 13,599-triangle victim region versus
   6,272-triangle weapon query exhausted 200,000 node visits after 303.986 seconds,
   having tested 86,395 triangle pairs. No measurement was returned. Review this
   workload before making any practical throughput claim. It is a performance
   finding, not evidence that the returned predicates are incorrect.
2. **Coverage:** current bone/rigid pre-material observations still exclude
   material displacement and raster visibility. The effective victim selection
   contains 2,282 triangles in masked/PDO sections; required visible-surface coverage
   remains insufficient. This is evidence for considering that specific deferral.
   It does not call for arbitrary AnimGraph/physics support or GPU grouping as part
   of this delivery.

At an explicit diagnostic ceiling of 1,000 pair tests / 10,000 nodes, the final
completed interval yields four measured queries and three work-limit failures.
Measured results: 39.823116 cm separation at montage 0.425706 s and intersections
at 0.520146, 0.549335 and 0.589642 s. The last is a guard sample outside the requested
0.40–0.55 interval. Full interval qualification is insufficient. Individual query
cost is approximately 4.54–7.64 seconds, far above small-fixture timings.

Native capture cost is 10.411–10.896 ms per pair; Katana inventory adds 6.570–7.424 ms.
Seven observations average 29.580 Hz, with 25.095–46.001 ms gaps. These are a short
diagnostic interval, not sustained 30 Hz. Mesh export originally stalled the later
RGB stream; Katana fixed that by retaining bounded snapshots until world cleanup.
Final Completed and Interrupted rendered scenarios pass their registered checks.
The interrupted mesh interval remains insufficient with three observations and
four missing attempts, including a 50.534 ms actual acquisition gap.

## Reproduction inputs

Primary evidence root:
`D:\UnrealProjects\5.6\KatanaCombat\Saved\Logs\FinalizedPoseConsumer-20260913-164015`.

- `first-pair-probe.json`: expensive full-budget query result and timings.
- `live-first/live-batch/victim-0` and `katana-0`: exact original failing query inputs.
- `live-final/live-batch`: final completed captures and effective component inventory.
- `live-final-analysis/measurements.json`: full pair/interval identities, limits,
  measurements, coverage failures and controls.
- `live-interrupted/live-batch`: incomplete interval control.
- `consumer-source/Tools/CombatCapture/regions/finisher-head-katana.json`: explicit
  triangle IDs, topology and region identities. This is a geometric neighborhood
  and complete weapon, not an anatomical neck/blade profile.
- `finalized-pose-consumer-evidence.zip`: verified portable evidence; see the report
  for hash, member paths and cleanup state.

Load these mesh records with the public replay API and feed the unchanged explicit
regions into `measure_mesh_pair`. Use centimetres, tolerance 0.5, coordinate-bit limit
256, triangle limit 50,000 and the stated query work limits. Preserve original pose,
component, acquisition and completion identities. The query's first failing input
is the original `live-first` pair, not the differently timed final run.

Native negative controls reject exhausted snapshot admission, duplicate components
and missing subsequent finalization without partial outputs. Python rejects wrong
topology/generation/pose/acquisition, unsupported required cloth, exhausted analysis
and actual gap controls. Historical captures still reproduce all five independent
acquisition mismatches. No shared correctness defect was established by this trial.

Katana owns explicit anatomical/weapon-region authoring and gameplay interpretation.
Containment, continuous collision, physical contact and artistic acceptance remain
unevaluated. Upstream `main` now points to the exact tested `ea890e3` revision
(verified 2026-09-13 through `git ls-remote`); publication is no longer outstanding.
Review the performance and coverage findings independently. Katana's primary pin
promotion remains a separate consumer action.
