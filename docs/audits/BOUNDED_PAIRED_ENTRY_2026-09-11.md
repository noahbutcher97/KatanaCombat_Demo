# Bounded paired entry verification — 2026-09-11

Opt-in preparation now reserves a pair and moves it toward an explicit starting
pose before either montage or paired damage can begin. The movement executor has
speed, travel, turn and time limits, environment sweeps and scoped ownership.
Current assets remain opted out. This establishes a usable mechanical boundary;
the rendered preparation still needs animation treatment and contact authoring.

The final editor build passed, the full headless suite passed **799/799**, and
**12/12 rendered lifecycle scenarios passed**. Six native contact evaluations
completed with `runtime_status=measured`; all six still fail the provisional
contact/alignment criteria. Those failures are preserved as animation findings,
not folded into the successful lifecycle result.

## Implementation and ownership

- `AlignmentMotionLibrary.*` supplies actor-independent validation, reachability,
  bounded step calculation and simulation-clock conversion. Limits and results
  are declared in `CombatTypes.h`.
- `TargetingComponent_BoundedAlignment.cpp` executes scoped bounded requests with
  swept character movement. It measures actual displacement after movement,
  detects obstruction and stale targets, and cannot extend accepted limits via
  a request update. One request executes at most once per engine frame.
- `FPairedEntryConfig` and `UPairedAnimationData::Entry` declare a victim pose
  relative to the initiating actor. This is a **preparation pose**, separate from
  the montage warp target. Entry defaults to disabled.
- `PairedAnimationComponent_Entry.cpp` supervises reservation, alignment/state
  leases, live readiness and cleanup. The initiator holds its initial world
  pose; the victim approaches. Only the accepted partners ignore one another's
  collision. Locomotion is suspended; environment collision remains active.
- The existing legacy montage start is extracted into
  `StartLegacyPairedMontages`. Preparation defers it until both live poses are
  ready. A readiness report is rechecked against current transforms before
  playback, including movement after the alignment executor ran.
- Cancellation, preemption, participant loss, obstruction or an exhausted limit
  releases this generation's handles without teleporting back or applying paired
  damage. Completion and sync callbacks cannot advance pending preparation.
  Entry-enabled pairs suppress the subsequent legacy sync nudge. Retained defense
  continues through its existing path.
- Clock limits use world simulation seconds. Unreal scales component ticks by
  actor `CustomTimeDilation`; the executor and supervisor undo that actor scale.
  Positive unequal actor rates therefore share the same budgets. Zero/invalid
  actor rates fail closed. Global pause and slowdown retain world-clock behavior.

The capture adapter accepts a complete `--entry-config` alongside
`paired-warp-tuning`, records before/after settings and restores transient data
without saving. Python and native provenance checks reject malformed, missing or
mismatched settings. The finisher scenario is now version 6 and records entry
outcome, observed preparation and deferred playback/damage checks. See the
[specification](../specs/PAIRED_ANIMATION_SPEC.md#36-bounded-legacy-paired-entry),
[usage guide](../guides/PAIRED_ANIMATION_EVALUATION.md#evaluate-bounded-entry-preparation)
and [implementation plan](../plans/BOUNDED_PAIRED_ENTRY.md).

## Verification

Evidence root: `Saved/Logs/PairedEntry-20260911-225256/`.
The [portable index](../../Saved/Logs/PairedEntry-20260911-225256/index.html)
links simulation-clock preparation frames, montage contact/overlay reviews and
separate image assessments. JSON, exact commands and logs remain beside it.

Final source identity:
`282df9f180dbf8dd0bf06ae40d97ae2f86c1f7aba06d1b0f7bbc30f193585551`.
This covers the tested dirty source/config/tool state and pinned dependency, not
just the unchanged Git HEAD. Documentation and Saved artifacts are not executable
source-identity inputs.

| Check | Result | Evidence |
|---|---|---|
| `KatanaCombatEditor Win64 Development` build | Passed; final incremental build 16.8 s | `entry-clock-build/` |
| Full `Automation RunTests KatanaCombat;Quit`, NullRHI | 799 completed, 0 failed, exit 0; 180.7 s | `combat-baseline-final/` |
| Capture Python unittest discovery | 86 passed, exit 0 | `capture-python-final/` |
| Rendered D3D11 matrix, two maps × two variants × three entry controls | 12/12 passed, all process exits 0 | `matrix.json` and referenced batches |
| Native pair evaluations of six completed captures | Six measured results, all criterion failures; commandlet exits 0 | `native-comparison.json` |
| Visual review | Six montage contact reviews; six completed preparations and two slow cancellations reviewed separately | `*-visual-analysis.html`, `preparation-image-review.json` |

The nine new native tests use public component APIs: five under
`KatanaCombat.Targeting.BoundedAlignment` and four under
`KatanaCombat.PairedAnimation.Entry`. They cover 20/60/120 Hz execution, cumulative
budgets, moving targets, obstacle sweeps, target loss, invalid and paused input,
duplicate-frame suppression, immutable limits, actor time dilation, deferred
montages/damage, cancellation/retry, unreachable geometry, competing ownership,
live-goal revalidation and exhaustion cleanup. Existing full-suite coverage also
exercises retained defense and entry-disabled behavior. No test friends were added.

The earlier 798-test baseline, focused checks and first rendered smoke are retained
but superseded by the final baseline/matrix. The smoke batch is
`20260911T231300-e9a406cf`; it predates the final clock and live-goal refinements.
The generic asynchronous RGB integration test explicitly defers under NullRHI;
this turn's rendered evidence is the scenario matrix, not an assertion that this
separate generic RGB test ran in rendered mode.

## Rendered movement comparison

All controls use victim warp window `[0.0001, 0.872983634]`, endpoint offset
`[50, 0, 0]` cm, and `match-partner-heading`. Entry pose yaw is 0 degrees. These
are transient experiments, not accepted asset values. Normal controls use a
0.5 s deadline, 300 cm/s translation, 150 cm travel, 540 deg/s turn, 180 degree
turn budget and 2 cm / 3 degree readiness tolerances. The slow control uses
1.5 s, 90 cm/s and 120 deg/s, with the same other limits.

| Control | Preparation result | Maximum sampled victim step after first paired sample, through montage 0.15 s |
|---|---|---|
| 80 cm pose | Ready in 0.333–0.338 s; 70 cm travel and 180 degree turn | 7.22–7.35 cm |
| 50 cm pose | Ready in 0.333 s; 100 cm travel and 180 degree turn | 9.10–9.34 cm |
| Slow 80 cm, completed | Ready in 1.483 s | 6.60–6.74 cm |
| Slow 80 cm, interrupted | Cancel requested about 0.467 s after reservation; no paired montage, no lethal observation | Not applicable |

The previous [ownership-corrected, no-nudge control](PAIRED_SYNC_ALIGNMENT_OWNERSHIP_2026-09-11.md)
showed roughly 32–37 cm early steps. Preparation markedly reduces that observed
problem, but these are separate runtime captures, not identical-pose replay.
Do not infer a universal percentage improvement or promote 80 cm as final
authoring. The 50 cm preparation actually produces a larger early montage step
than 80 cm despite matching the endpoint's numeric offset: start pose and warp
endpoint are different concepts.

`comparison.json` measures final actor samples. `warp-diagnostics.json` independently
parses engine SkewWarp logs: victim input near 2.683 cm becomes approximately
6.71–9.42 cm output around montage 0.117 s. Modifier output precedes final
CharacterMovement, so the values need not equal the final actor step. No sync
nudge appears in any of the 12 logs.

Preparation samples stay within the declared 300/90 cm/s and 540/120 deg/s
limits. The first request-boundary sample includes fixture relocation and is
retained as raw data but excluded from movement-only maxima. Cancellation uses
the actual `interruption_requested` marker/frame as the cutoff, not the next
`entry_failure_observed` marker: normal movement resumes after release. The
earlier analysis that included that extra frame is retained in
`comparison-before-cancellation-boundary-fix.json`; it was an interval-selection
error, not a bounded-movement failure. Interrupted slow captures are evaluated on
the simulation clock and never assigned an invented montage time.

## Image findings and remaining work

Preparation visibly moves and turns the victim in a standing/locomotion pose;
there is no coordinated approach animation. The slow setting lengthens this
visible prelude. Speed limits establish bounded placement, not natural motion.
No acceleration/jerk policy, pathfinding or foot-support solution is supplied.

The completed montage frames show the close restraint, raised blade and lowering
action. Arms, head and bodies obscure the relevant contact surface. Exact
blade-to-skin contact and penetration remain **indeterminate**. The provisional
torso sphere and anatomical bone points remain diagnostic proxies, not body
surfaces. Six native evaluations fail both authored/runtime contact criteria and
relative-alignment criteria; their measurement paths remain valid.

All completed runs still first observe lethal state around montage 0.016667 s,
before the reviewed strike poses. Preparation correctly prevents this during its
own interval; it does not repair existing in-montage impact timing. The fast
interrupted controls reach playback before cancellation and are already lethal;
the slow cancellations preserve the victim alive.

Next authoring work should establish the intended preparation pose from the
authored pair, provide an approach/turn or blend treatment, and then select the
actual contact region and impact frame. Re-evaluate from a camera exposing that
surface before saving settings. Later root motion remains outside the preparation
budgets. Full recovery visuals, quantitative foot support, limb penetration,
audio/haptics and broad starting-geometry navigation are not certified here.

## Preservation and dependency boundary

At this verification checkpoint, changes were local and uncommitted. The later
[publication checkpoint](PAIRED_WORK_PUBLICATION_2026-09-13.md) records the Git handoff.
No asset was saved. Preservation checks compare
all 7,948 Content file sizes/mtimes and 54 previously protected asset hashes or
absences. Unrelated prior WIP is byte-checked. `scoped-changes.patch` and
`after-source/` isolate this turn from earlier WIP.

Katana still consumes AnimationAnalysis revision
`3fd91eb70be340db63778a697b12465ab895cd8a`; its detached checkout and generated
plugin are byte-verified by source identity. The independently developed shared
workspace advanced to `5b716a07cf1c170e15cad9a50598e333e84ee2a4`, with new mesh
record/replay/reference implementation and a clean worktree at inspection. Those
new commits were not integrated during this verification. Review their delivery
contract and qualify a separate pin update before using them for Katana contact
claims. This turn did not modify that workspace or the generated plugin.

Retention targets comprise 59 newly created capture directories, four image-writer
test directories, the current evidence root, four scenario batches including the
earlier smoke, and six native evaluation runs/results. They contain **1,665 PNGs**.
`finalize.py` verifies the archive entry hashes before removing those exact PNG
files. `retention.json` records the completed archive digest, verified count,
removed paths and replay instructions; portable reviews retain embedded frames.
No broad directory cleanup or unrelated-image removal is used.
