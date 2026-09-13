# Initiator finisher approach qualification

The straight rear approach now has usable movement presentation: the attacker
walks toward an anchored victim before paired playback. Ownership, obstruction,
cancellation and recovery are verified. This is an unsaved authoring prototype;
transition phase, oblique choreography, floor support and contact acceptance
remain open. Shared AnimationAnalysis review does not block this RGB/gameplay work.

## Identity and scope

- Runtime/API slice: `6fe35835046cbfe60b63a6dfa0d21b7558055523`.
- Starting Katana commit: `9e176381c3e9fcd241b65ea9ff3136c13e2f72f9` on
  `investigate/finisher-source-pair`.
- AnimationAnalysis remains `2fb0dc980dbdba1348b02e3a93b4512606aa5d16`.
- Tested working source identity:
  `145dab585f6b80a7acaef144ca5c9a8afbe0a541038646f5732fc3d6ea086924`.
- Evidence root: `Saved/Logs/InitiatorApproach-20260913-135050/`.
- UE `5.6.1-44394996+++UE5+Release-5.6`, Win64 Development editor, Windows 11
  `10.0.26200`, Intel Core Ultra 9 275HX, RTX 5090 Laptop GPU, D3D11.
- Existing 15 WIP files were preserved. Two were extended:
  `CombatCaptureScenarioTests.cpp` resolves readiness for either moving role and
  hashes the selected animation; `evaluate_capture.py` validates presentation.
  The other 13 retained their original bytes. All 7,948 Content metadata records
  remained unchanged. No Content package was saved.

The runtime slice includes entry types/execution, ownership cleanup, native tests,
typed entry tuning, focused Python validation/tests and a read-only inventory tool.
Only the new entry-validation functions were staged from the mixed Python file.
Existing named-placement/sidecar capture WIP, including the fixture extensions,
remains uncommitted. Therefore the runtime commit alone is not the complete
rendered fixture state; replay includes the tested working files and diff.
No commits from this turn were pushed.

## Selected assets and declared criteria

The legacy paired asset is
`/Game/ProjectFiles/Data/PDA/Defense/GateA/DA_Finisher_GateA`.
Its production attacker/victim montages and actual AnimBlueprint evaluation remain
in use. Their recorded `authored_playback_layout` preserves source clips, segments,
slots, rates and 0.25-second blend times. The actor and effective mesh identities
are in each capture's `session.json` and `samples.jsonl`; the transient movement
clip is included in `asset-identity.json` dependency hashes.

Selected movement clip:
`/Game/Assets/Animations/KatanaAnimset/InPlace/WalkForward_InPlace.WalkForward_InPlace`.
Fresh RAW pose inventory found a matching skeleton, 1.3-second duration, disabled
root motion, and stationary root translation. The native control confirms zero
source notifies, closing the Python protected-property reflection gap. The paired
root-motion walk travels 158.588 cm in 1.3 seconds, motivating 122 cm/s at rate 1.
Four compatible in-place 90/180-degree turn clips were inventoried; none was
selected automatically or combined into a turning choreography.

`criteria.json` was written before rendered measurement. Straight rear placements
start 150 cm apart with equal heading, and request a victim-relative entry of
100 cm, yaw 0. The initiator moves; the victim anchors. Limits are 0.65 seconds,
122 cm/s, 150 cm travel, 540 degrees/s, 180 degrees turn, 2 cm position tolerance
and 3 degrees yaw tolerance. Presentation uses DefaultSlot, rate 1 and 0.1-second
blend-in. The initial 0.1-second blend-out and later 0.25-second comparison are
retained separately. The latter is the preferred next authoring candidate.

Paired controls retain victim warp 0.15–0.45 seconds, offset `[42,0,0]` cm,
matching heading, primary sync 0.466667 seconds and disabled sync nudge. Existing
transient movement-notify overrides permit source motion. No asset defaults change.

## Verification and measured results

| Check | Result |
|---|---|
| Editor build, `-MaxParallelActions=2 -NoUBA -NoHotReload` | Pass |
| `KatanaCombat.PairedAnimation` + `KatanaCombat.Targeting.BoundedAlignment` | 71/71 pass |
| `python -m unittest discover -s Tools/CombatCapture -p test_*.py` | 93/93 pass |
| Rendered public-input capture scenarios | 8/8 gameplay/evaluator passes |
| Shared visual review provenance validation | 22 original frames; review recorded |
| Full Katana automation suite | Not run; scoped paired/alignment ladder used |

The initial build caught a const qualification error in the montage-instance
lookup; the corrected and final builds passed. The first native run was 14/15:
the wall blocked public target acquisition before the intended executor test.
The corrected control introduces its wall after acceptance, then verifies Blocked,
no paired montage/damage, anchored victim, and restored input/movement. The final
71-test regression includes that control and all ten entry tests. Initial logs
are retained, not substituted with the successful results.

| Rendered case | Entry result | Executor time | Initiator travel |
|---|---|---:|---:|
| ThirdPerson rear distant, completed | Reached | 0.400 s | 48.800 cm |
| ThirdPerson rear distant, interrupted | Reached, then interrupted | 0.400 s | 48.800 cm |
| ThirdPerson slower approach, interrupted | Cancelled before paired playback | 0.4713 s observed | 28.276 cm sampled |
| ThirdPerson already at rear entry | Reached, movement clip skipped | 0.017 s | 0 cm |
| ThirdPerson oblique, completed | Reached; choreography concern | 0.367 s | 44.734 cm / 27 degrees |
| DefenseMatrix rear distant, completed | Reached; floor transition concern | 0.400 s | 48.801 cm |
| ThirdPerson wide view, completed | Reached | 0.405 s | 49.420 cm |
| ThirdPerson wide view, matched blend | Reached | 0.406 s | 49.585 cm |

The cancellation control uses 60 cm/s, playback rate `60/122`, and a 1.1-second
deadline so the existing 0.45-second interruption occurs during preparation.
Rendered checks verify no premature paired playback/damage, ownership cleanup,
movement/input recovery and execution of a fresh attack. Native controls also
verify replacement-montage preservation and cancellation/retry.

Executor logs report zero victim travel/turn for every reached entry. All sampled
pre-playback victim positions remain at their request placement. DefenseMatrix
then drops both capsules from Z=96 to Z=90.1501 when walking movement is restored
at the first paired sample: a **5.8499 cm vertical transition**, not entry travel.
The existing gameplay evaluator does not reject this visual/floor defect. The
analysis retains the boundary displacement and pre-playback interval separately.
Warmup-to-request placement resets are fixture setup and are excluded from entry
travel; timestamps and sampled positions were not rewritten.

Across eight captures: 2,181 samples and 1,085 original frames, achieved sampling
59.50–59.98 Hz and images 29.93–29.99 Hz. Maximum gaps were 0.0388 seconds for
samples and 0.0398 seconds for frames. No image rejection/failure or telemetry loss
was recorded. Offline diagnostic parsing/measurement took about 0.7 seconds on
this machine; captures themselves add overhead and are not performance benchmarks.

During the first straight capture's fully weighted walking interval
0.629955–0.913289 simulation seconds, the left foot bone displaced 2.007 cm and
the right 102.929 cm. These are observed bone trajectories, not planted-foot slip
or support measurements. No ground-contact labels or sole geometry were supplied.

The wide 0.1-second blend-out control has minimum summed approach/finisher montage
weight about 0.4 during the transition; the 0.25-second comparison stays near 1.0.
Both retain approximately 514.5 cm/s peak sampled actor speed during the finisher
opening, versus the 122 cm/s approach. Matching blend times addresses one visible
transition concern but does not resolve gait phase or the source velocity change.
The montage-weight sum is not a full AnimGraph contribution measurement.

## Replay and follow-up

Exact build/native/capture/publication commands, exit codes, initial failures,
frozen source/binary/asset identities, profiles, inventories, full raw samples,
frames, runtime overrides and measurements are retained under the evidence root
and its `initiator-approach-evidence.zip`. `capture-runs.json` maps each case to
its original run/capture directory; `archive-manifest.json` inside the ZIP records
every replay member hash. `visual-review.html` embeds the 22 reviewed original
frames and remains usable after loose-image cleanup.

Archive SHA-256:
`ac18386bcdf7da0c7e7068b245c8a792c1992eb4d7042b45434d1058db0ffa66`.
Size: 674,024,189 bytes; 1,414 payload members. Every member was read back and
hash-verified before removing the 1,085 owned PNGs. Zero loose PNGs remain in the
eight owned capture directories. `archive-verification.json` records the result.
The archive contains the audit before this archive-hash paragraph was appended;
the tracked audit records the final archive identity.

Next work is a deliberate approach-to-finisher transition: choose an ending gait
phase/clip compatible with the finisher opening, retain the matched blend control,
and address the rapid source advance. Add staged turning or directional clips for
oblique entry; resolve DefenseMatrix's fixture/support height before using it for
transition approval. This does not require changing the shared mesh acquisition
boundary. Live mesh-contact qualification remains blocked as recorded in the
[AnimationAnalysis handoff](ANIMATION_ANALYSIS_CONSUMER_HANDOFF_2026-09-13.md).

The [entry presentation guide](../guides/PAIRED_ENTRY_PRESENTATION.md) describes
the reusable API and transient configuration. The [implementation plan](../plans/INITIATOR_FINISHER_APPROACH.md)
defines this prototype's scope. No sampled outcome establishes continuous
collision, physical contact, containment or artistic acceptance.
