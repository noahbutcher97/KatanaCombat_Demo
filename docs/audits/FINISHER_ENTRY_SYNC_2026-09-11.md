# Finisher entry and primary-sync evaluation - 2026-09-11

Independent transient sync timing and position-nudge controls are implemented and
verified. The comparison identifies an alignment-ownership defect and abrupt entry
motion; it does not establish an acceptable finisher correction. No assets were
saved and no runtime combat implementation was changed in this slice.

## Findings that determine the next work

1. **The sync nudge has no participant-ownership guard.** In
   [the sync notify](../../Source/KatanaCombat/Private/Animation/AnimNotifyState_PairedAnimationSync.cpp),
   primary sync calls the paired damage owner, then independently takes the first
   partner and can teleport it to 80% of the distance threshold. Both participant
   notifies can reach this alignment code. Fresh matching-control logs reduce
   separation from 141.4 to 80.0 cm. ThirdPerson Completed specifically logs
   `Nudged victim BP_Player_C_0`: it moved the attacker. Other cases move the actual
   victim. Source and logs support an evaluation-order dependency in which actor
   is corrected; the log's hardcoded word "victim" does not establish role.
2. **Disabling that nudge does not produce smooth entry.** The first paired
   separation becomes approximately 141.43 cm, but a subsequent sampled horizontal
   victim step reaches 35.27 cm in 21.44 ms on ThirdPerson and 37.22 cm in 16.67 ms
   on DefenseMatrix, both near montage 0.116 s. The early rendered reach spans a
   wider gap before restraint. This is remaining entry-motion evidence, not proof
   of the exact motion-warping cause. Matching facing still differs by roughly
   58..67 degrees near montage 0.6 s across the controls.
3. **Sync timing is independently controllable.** With nudging disabled and sync
   moved to 0.6 s, lethal state is first observed at 0.611169 s on ThirdPerson and
   0.606661 s on DefenseMatrix. Their interrupted variants cancel at observed
   montage positions 0.480036 and 0.469508 s respectively, retain request-time
   health, and never observe lethal state during the pair. Completion, ownership
   cleanup, input suppression and fresh recovery input pass. This validates the
   diagnostic control and pre-sync cancellation, not an approved impact time.
4. **Contact remains unresolved.** All six native runtime comparisons fail the
   unchanged provisional torso-contact criterion. Selected pixels obscure the
   exact head/neck surface with hands, weapons and bodies. Exact visible contact
   and penetration remain indeterminate. Neither a later sync nor a heading
   target should be saved as a complete correction on this evidence.

The fixture sets health to **1 immediately before requesting the finisher**. The
new `victim_health_at_request` field separates this setup from damage observations.
The earlier facing audit's sampled 100-to-0 transition is not evidence of a
100-point damage event: the request-time reset occurs between those observations.

## Reusable controls and verification

`PairedSyncTuning` defines optional primary-sync time/nudge settings and exact
before/after validation. The existing paired-warp experiment accepts
`--primary-sync-time` and `--sync-nudge enabled|disabled` independently. It selects
exactly one primary damage notify per role by semantics, duplicates notify objects
transiently, preserves effective duration/trigger offsets and unchanged fields,
and restores original events/objects. Native and Python evaluators require both
role records and reject malformed or inconsistent provenance. See the
[evaluation guide](../guides/PAIRED_ANIMATION_EVALUATION.md).

Scenario version 5 adds the victim's `head` observation point; acceptance criteria
are unchanged. Timing experiments also record observed lethal/interruption montage
positions and check request-time health when interrupted before sync. These are
automation observations, not exact callback timestamps.

| Verification | Result |
|---|---|
| Final KatanaCombatEditor build | Passed |
| Capture Python suite | 83 passed |
| `KatanaCombat.Editor.PairedEvaluation` | 13 passed, including independent sync fields/provenance |
| Broad rendered `KatanaCombat.Capture` | 21 passed, two recorder failures retained |
| Isolated repeat of the two ThirdPerson finisher cases | Both passed |
| Delayed-sync motion setup control | Passed; first lethal observation 0.600029 s |
| Final rendered matrix | All 12 cases passed: three controls, two maps, completion/interruption |
| Native contact evaluations | Six completed with measured runtime provenance; geometric failures retained |

The two broad-suite failures exhausted the synchronous-readback recorder's bounded
PNG encoding queue: `image_queue_limit_reached`, peak four pending frames, one
rejected frame per capture, zero encoder failures. They occurred in ThirdPerson
Completed/Interrupted. This repeats the integration audit's reliability concern;
isolated passes do not erase it. The broad suite was not rerun as an aggregate
after the later metadata-serialization-only fixture change.

The first expanded nudge batch failed all four setups because pretty-printed
override JSON exceeded the recorder's 4096-character metadata bound. Compact JSON
fixed this without dropping fields or raising the bound. The combined delayed-sync
motion control records 3766 characters. Initial failures, the successful original
matching batch, the compact-serialization build and fresh final controls remain
retained. Longer metadata still fails closed at the existing recorder bound.

## Measured and visual comparison

All final controls use permitted paired movement, attacker translation warping
disabled, victim warp window `0.0001..0.872983634` s, offset `[50,0,0]` cm and
`match-partner-heading`. They are transient experiments, not unchanged saved-asset
behavior. "Matching" retains original sync settings; "No nudge" changes only the
nudge flags; "Delayed" additionally sets primary sync to 0.6 s.

| Map / control | First paired separation (cm) | Largest later entry step (cm) | Heading difference near 0.6 s | First lethal observation (s) |
|---|---:|---:|---:|---:|
| ThirdPerson / Matching | 80.00 | 5.36 | 66.62 deg | 0.016667 |
| DefenseMatrix / Matching | 80.00 | 3.99 | 62.92 deg | 0.016667 |
| ThirdPerson / No nudge | 141.43 | 35.27 | 59.71 deg | 0.016667 |
| DefenseMatrix / No nudge | 141.43 | 37.22 | 58.89 deg | 0.016667 |
| ThirdPerson / Delayed | 140.46 | 23.44 | 60.22 deg | 0.611169 |
| DefenseMatrix / Delayed | 141.43 | 32.21 | 57.84 deg | 0.606661 |

These are completed-case observations. The step metric excludes the first paired
sample's cross-boundary step, which also contains the fixture's request-time reset;
it covers later samples through montage 0.15 s. It must not be used to deny the
matching control's separate approximately 61 cm logged sync correction. Actual
nearest sample clocks and all interrupted-case measurements are in `comparison.json`.

The unchanged 18 cm torso sphere has minimum sampled gaps of 34.84..35.14 cm over
0.3..1.5 s. Native runtime `BladeToUpperTorso` fails, while `EntryRootSeparation`
and `ReleaseLeftHand` pass in all six cases. Relative-alignment checks fail against
the unchanged opposed-heading authored profile. Per-map profiles name each actual
captured mesh and change no criteria. Cross-mesh equivalence is not assumed.

Diagnostic blade-segment-to-head-bone distances over 0.45..0.75 s have sampled
minima of 17.24..17.48 cm for matching, 36.50..36.58 cm without nudging, and
37.38..43.25 cm with delayed sync. These points are not calibrated skin/contact
regions. Maximum contact-interval sample gaps are 16.78..26.47 ms; no continuous
contact or absence claim follows from the sampled minima.

All 72 selected final raw frames were inspected as fixed central crops, 12 per
completed capture over approximately 0..1.5 s. Full originals remain embedded in
portable contact reviews. Separate visual reports identify assistant image review,
keep geometric evidence separate, and record entry concerns and indeterminate
contact. Foot support, full recovery presentation, audio and haptics were not
reviewed. The sampled frames do not independently timestamp the sync teleport.

## Evidence, preservation and next implementation

Evidence root: `Saved/Logs/FinisherEntryTiming-20260911-194647/`.
Final batches: `20260911T200838-c26a39b1` (matching),
`20260911T201103-c1c96071` (no nudge), `20260911T201325-f56f265b` (delayed), all under
`Saved/CombatScenarioRuns/`. Commands, source identity, scenario checks, native
results, and initial failures are retained in that evidence root.

| Map | Matching | No nudge | Delayed |
|---|---|---|---|
| ThirdPerson | [Review](../../Saved/Logs/FinisherEntryTiming-20260911-194647/ThirdPerson-matching-Completed-visual-analysis.html) | [Review](../../Saved/Logs/FinisherEntryTiming-20260911-194647/ThirdPerson-no-nudge-Completed-visual-analysis.html) | [Review](../../Saved/Logs/FinisherEntryTiming-20260911-194647/ThirdPerson-delayed-Completed-visual-analysis.html) |
| DefenseMatrix | [Review](../../Saved/Logs/FinisherEntryTiming-20260911-194647/DefenseMatrix-matching-Completed-visual-analysis.html) | [Review](../../Saved/Logs/FinisherEntryTiming-20260911-194647/DefenseMatrix-no-nudge-Completed-visual-analysis.html) | [Review](../../Saved/Logs/FinisherEntryTiming-20260911-194647/DefenseMatrix-delayed-Completed-visual-analysis.html) |

The next project implementation should establish a single alignment owner and
prove that attacker-first/victim-first notify evaluation cannot reposition different
participants. Then address the measured abrupt entry trajectory with bounded
motion before committing impact timing or assets. Simply disabling nudging,
shortening the entire warp window, or moving damage is insufficient. The historical
entry-approach design remains pending re-review and was not adopted here.

AnimationAnalysis remains pinned to `3fd91eb70be340db63778a697b12465ab895cd8a`.
Later standalone commits through `dbb523442605f19eff57b8c7d65538027c86c9a3` change
documentation only; the shared implementation still matches the pin. Its broader
surface/fidelity research has not delivered a production skeletal-surface API.
The next Katana alignment work can proceed independently of that plugin work.

Final preservation checks passed for all 7,948 Content size/time records, 54
protected asset hash/absence records and 27 prior WIP files outside the intentional
edits. The shared workspace was clean and its implementation matched the pin.
The final source identity is
`0a43c0eeb4dd92cde55adeacb20bcdf1827e8c099980c88955a48da2d594fee4`.

The verified archive contains 4,042 entries (1,866,333,419 bytes), SHA256
`39c3efe17603288798a858098b2e33d9f00172fa3063ee7630abd8b1ab2c4af0`.
All **3,079 generated PNG files** were archived, hash-verified and removed; zero
remain in the scoped outputs. All six portable contact reviews load after removal.
See `verification.json` and `retention.json` in the evidence root. Restore selected
archived paths for image-dependent replay. Changes remain local and uncommitted.
