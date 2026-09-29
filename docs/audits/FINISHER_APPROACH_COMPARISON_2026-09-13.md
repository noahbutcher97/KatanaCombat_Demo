# Finisher approach and impact comparison

The existing source animation and saved 0.25-second montage blend provide a
coherent **rear starting-context reference**. A new turn clip is not justified
for that already aligned context. Six rendered lifecycle comparisons pass with
transient rear placement, entry, warp and impact settings. Distant/oblique
preparation still moves the victim without established supporting footwork, and
exact skin contact remains indeterminate. No production asset settings were saved.

This continues the [authoring plan](../plans/FINISHER_AUTHORING.md) and
[source reference](../specs/FINISHER_AUTHORING_REFERENCE.md).

## What changed

- Registered scenarios now support named participant placements. Each role has a
  bounded offset and upright yaw. The driver applies these during fixture setup
  and immediately before public input; the report records the actual transforms.
  The evaluator checks requested/observed poses and metadata identity. This is
  reusable scenario setup, not runtime alignment or a teleport-based approach.
- Read-only montage inspection records sections, source segments, rates, loop
  count and saved blend times. The captured pair has one `DefaultSlot` segment
  per montage: track and source start zero, rate scales and segment rates one,
  one loop. Both `Finisher` sections start at zero. Source and montage time are
  therefore directly comparable for this specific playback mapping.
- Full transient override provenance is retained in `runtime-overrides.json`.
  Recorder metadata carries a SHA-1 digest of the exact UTF-8 bytes; evaluation
  requires both the digest and parsed rows to agree with the scenario. This fixes
  the recorder metadata-size failure without increasing its bounds or dropping
  override details. Existing inline records remain readable.
- Entry assertions distinguish preparation that was observed from a request
  already satisfying readiness. Deferral is asserted only where preparation was
  sampled. An unobserved interval is not reported as verified deferral.
- Corrected a header comment: legacy finisher playback uses montage-asset blend
  settings. The paired-data blend overrides are used by the defense-chain path.
  No gameplay implementation, shared plugin code or dependency pin changed.

Implementation: `CombatScenarioPlacement.h`, `CombatCaptureScenarioTests.cpp`,
`MontagePlaybackInspection.h`, `scenario_placement.py`, the runner/evaluator,
focused tests, the registered scenario and the
[evaluation guide](../guides/PAIRED_ANIMATION_EVALUATION.md).

## Animation inventory and selected treatment

An isolated Unreal Python inspection loaded 13 sequences. All share the current
reference skeleton. This proves skeleton identity and sampled root motion, not
that every clip provides a suitable transition pose.

| Candidate family | Duration | Root behavior |
| --- | ---: | --- |
| KatanaAnimset root-motion 180-degree turns, left/right | 1.967 s | 180-degree yaw; negligible translation |
| KatanaAnimset root-motion 90-degree turns, left/right | 1.300 s | 90-degree yaw; negligible translation |
| KatanaAnimset WalkForward_Root | 1.300 s | About 158.6 cm forward travel |
| KatanaAnimset in-place 180-degree turns and forward walk | Same matching durations | Root extraction disabled; stationary root |
| GhostSamurai forward walk start | 1.233 s | About 142.3 cm forward travel |
| GhostSamurai forward walk loop | 1.167 s | About 200 cm forward travel |
| GhostSamurai forward walk end | 0.833 s | About 4.7 cm net root travel |
| Current Ambush01 / Ambushed01 pair | 2.433 / 4.200 s | Authored rear approach, restraint and strike |

Both finisher montages save **0.25-second linear blend-in and blend-out**, standard
blend mode, no blend profile. Unreal's `Montage_Play` consumes those settings.
The data asset's attacker/victim blend-in values of 0.10/0.05 seconds do not
override this legacy play path.

The captured attacker advances during blend-in; the hypothesized missing root
advance was not demonstrated. The source already supplies the advance into the
restraint. Preserve it and the current blend as the rear comparison treatment.
Do not insert a roughly two-second turn animation merely because a frontal test
fixture previously required the victim to turn around.

## Rendered comparisons

Fresh D3D11 runs used the actual player input/paired/recovery path and active
bystanders. Transient settings were:

- Entry goal: victim 100 cm ahead of initiator, matching yaw; 0.5-second deadline,
  300 cm/s translation limit, 150 cm travel budget, 540 degrees/s turn limit,
  180-degree turn budget, 2 cm / 3-degree readiness tolerances.
- Attacker source translation permitted; victim warp window 0.15-0.45 montage
  seconds, relative warp offset `(42,0,0)` cm, matching partner heading.
- Movement-blocking paired notifies overridden for root motion; input ownership
  retained. Primary damage sync requested at **0.466667 montage seconds** on both
  roles, with legacy position nudge disabled.

The 42 cm warp endpoint is an experimental approximation to the moving source
relationship near strike time. It is not a new default or a constant source
separation requirement.

| Map / case | Starting victim pose | Readiness/start observation after request | First lethal observation, montage time | Result |
| --- | --- | ---: | ---: | --- |
| ThirdPerson completed | 100 cm, yaw 0 | 0.0175 s | 0.4700 s | Pass |
| ThirdPerson interrupted | 100 cm, yaw 0 | 0.0167 s | None; interruption 0.4526 s | Pass |
| ThirdPerson distant completed | 150 cm, yaw 0 | 0.1667 s | 0.4672 s | Pass |
| ThirdPerson oblique completed | (120,20,0) cm, yaw 30 | 0.1000 s | 0.4682 s | Pass |
| DefenseMatrix completed | 100 cm, yaw 0 | 0.0167 s | 0.4667 s | Pass |
| DefenseMatrix interrupted | 100 cm, yaw 0 | 0.0167 s | None; interruption 0.4542 s | Pass |

Times are observations bounded by fixture cadence. Both interrupted cases retain
the victim's request-time health before the candidate impact. Completion,
ownership/collision cleanup, public-input suppression and input recovery pass.
The aligned cases begin within roughly one observation frame; they do not require
a measurable waiting interval. The distant and oblique cases exercise actual
preparation and its playback/damage deferral assertions.

The DefenseMatrix victim uses a mannequin mesh rather than the ThirdPerson
mercenary mesh. Passing both maps is useful lifecycle/context evidence; it is not
automatic approval of skin contact on either mesh.

## Visual findings and remaining work

The separate image reviews inspected 36 of 72 comparison frames through four
uncropped contact sheets, plus four of 22 close contact frames at full resolution.
The broad arrangement reads as rear restraint and strike, and the separate
180-degree prelude is absent in the aligned reference.

The distant case visibly translates the victim backward toward the waiting
attacker before paired playback. The oblique case also adjusts the victim while
its pose settles. No authored step was selected to account for that movement.
Short preparation duration and bounded displacement do not resolve this visual
concern. Fixture resets at/before the request are excluded from the finding.

Close views show blade travel near the head, partly obscured by hands, head and
the victim's own weapon. The first lethal observations lie near the independently
selected source strike interval. This narrows timing uncertainty; it does not
prove exact skin entry/exit, penetration or artistic timing. The displayed head
overlay has radius zero and denotes a bone origin. No torso threshold was relaxed
or replaced, and no contact pass is claimed.

The next production work is **how the initiator reaches a suitable rear start**:
choose the moving/anchored role and a supported approach animation or locomotion
transition, then verify foot support and interruption during that approach.
Preserve the victim's presentation rather than treating its idle translation as
finished choreography. Other starting contexts may require different paired
animation selection. This policy is not implemented by the new fixture poses.

Follow-up: the same day, the [initiator approach](INITIATOR_FINISHER_APPROACH_2026-09-13.md)
prototyped the straight rear approach with an anchored victim, and the
[entry transition](PAIRED_ENTRY_TRANSITION_2026-09-13.md) qualification addressed
floor continuity and source-phase entry. Current status is in the
[authoring plan](../plans/FINISHER_AUTHORING.md).

The 0.466667-second timing candidate still needs exposed contact review before
promotion. Obstruction, cancellation during preparation, wider heading/range
coverage and continuous full recovery presentation are not newly demonstrated by
this six-case matrix. Earlier unit/entry evidence remains separate.

## Verification and retention

- `KatanaCombatEditor Win64 Development` build passed. The initial parallel build
  exhausted Windows commit/pagefile capacity; retrying with
  `-MaxParallelActions=2 -NoUBA` passed without changing system settings.
- Python capture suite: **90/90 passed**.
- Native `KatanaCombat.Capture.ScenarioPlacement.InputValidation`: **passed**.
- Rendered transient comparisons: **6/6 passed**.
- Unmodified default finisher and hold/release scenarios: **2/2 passed** with
  motion recording under NullRHI, covering existing setup and the sidecar path.
- Visual evidence/review schemas and viewer JavaScript syntax passed. Full
  project automation was not rerun because production behavior was unchanged;
  verification targets the capture fixture, provenance and exercised scenarios.

Final executable/source/config/tool/dependency identity:
`4e5115f9d7a6a2aa2ed90b15197f5cb1622f3dde452872f157db13433dc3aa4b`.

Two earlier attempts are retained as instrument evidence: the metadata bound
prevented capture in the first; the second captured and completed gameplay but
failed the overly restrictive preparation-observation assertion. Neither is
silently counted as a passing run.

Local evidence: `Saved/Logs/FinisherApproach-20260913-102813/`.
Open the [comparison viewer](../../Saved/Logs/FinisherApproach-20260913-102813/comparison-frames.html),
[comparison findings](../../Saved/Logs/FinisherApproach-20260913-102813/comparison-visual-analysis.html),
or [close contact findings](../../Saved/Logs/FinisherApproach-20260913-102813/rear-contact-visual-analysis.html).
These generated artifacts remain local. `final-checks.json` records source and
Content preservation; `retention.json` records archive hashes, entry verification
and exact PNG pruning. Portable viewers retain embedded original pixels after
cleanup. The fixture and provenance code was committed in `16acdec2` and merged
to `main` through PR #124.
