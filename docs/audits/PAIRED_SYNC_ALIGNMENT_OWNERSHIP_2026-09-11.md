# Paired sync alignment ownership - 2026-09-11

Direct sync correction now belongs to the active legacy paired sequence owner and
its accepted target. The correction cannot select the attacker or a bystander
through partner-list order. Retained defense keeps its existing scoped alignment.
The editor builds, all 790 KatanaCombat automation tests pass, and all eight
rendered scenario combinations have passing runs. Entry motion and exact contact
remain unresolved; no assets were saved.

## Runtime change and coverage

The [preceding entry/sync audit](FINISHER_ENTRY_SYNC_2026-09-11.md) recorded an
attacker being moved by the victim's notify. The notify had treated its first
tracked partner as the victim, without establishing sequence ownership.

`UPairedAnimationComponent::GetOwnedSyncCorrectionTarget` now resolves the current
accepted target from the active legacy generation. It requires current partner
tracking and excludes retained defense contexts. The sync notify queries it after
damage/presentation callbacks, which can end participation. Participant notifies,
unowned tracking, removed targets and ended generations cannot authorize a nudge.
The existing primary-sync, validation, nudge-enable and distance gates remain.
Damage timing, presentation and saved authoring defaults are unchanged.

[PairedSyncOwnershipTests.cpp](../../Source/KatanaCombatTest/Private/PairedSyncOwnershipTests.cpp)
exercises both notify orders, reordered/unrelated partners, removed targets,
inactive/ended participation and authored gates through public APIs.
`KatanaCombat.Defense.Chain.SyncPreservesAlignmentOwnership` commits a retained
parry through public Block input and checks that either notify order leaves both
actors untouched. No new test friends were added. The contract is documented in
[the paired-animation specification](../specs/PAIRED_ANIMATION_SPEC.md#35-sync-correction-ownership).

| Verification | Result |
|---|---|
| Pre-fix ownership regression | Three of four tests failed; authored gates passed |
| Final editor build | Passed |
| Focused ownership tests after fix | 4/4 passed |
| Final full `KatanaCombat` suite | 790/790 passed; process exit 0, 176.36 seconds |
| Rendered scenarios | Eight combinations passed: two maps, two nudge settings, completion/interruption |
| Native paired evaluations | Four completed with measured runtime provenance; geometric failures retained |
| Visual review | Four portable reports, 12 selected frames each |

An initial build reused a makefile and did not discover the new test file; its
zero-test attempt is retained and is not verification. Rebuilding with
`-NoUBTMakefiles` discovered it. The first full post-fix suite was 789/790: the new
bystander fixture requested an overlapping spawn position and asserted that
unreliable initial location. Explicit nonoverlapping placement fixed the fixture;
the subsequent focused and full runs above passed. The earlier red target test
also included those placement assertions; the notify-order and inactive cases
independently reproduced the ownership defect.

The full suite uses NullRHI and explicitly defers async RGB integration. The
separate scenario matrix uses rendered D3D11. One initial no-nudge
ThirdPerson/Interrupted attempt is inconclusive because a pose sampling gap of
83.037 ms exceeded its criterion. All its gameplay checks passed. Its isolated
repeat passed; the initial attempt remains in the evidence, and sampling
reliability is not declared solved.

## Fresh motion and contact findings

Every correction-enabled case logs exactly one correction of the manifest's
actual victim, with the manifest's actual attacker identified as owner. All four
reduce actor separation from 141.4 to 80.0 cm. No no-nudge case logs a correction.
These rendered cases corroborate the explicit notify-order tests; they do not
claim to force every possible runtime notify order.

The completed cases retain a second, distinct movement issue:

| Map and setting | Largest victim step after first paired sample, through 0.15 s | Peak early SkewWarp input -> output | Peak modifier interval |
|---|---:|---:|---|
| ThirdPerson, correction enabled | 3.99 cm | 2.68 -> 4.24 cm | 0.116667..0.133334 s |
| DefenseMatrix, correction enabled | 3.98 cm | 2.68 -> 4.24 cm | 0.116774..0.133441 s |
| ThirdPerson, no nudge | 31.79 cm | 2.68 -> 31.81 cm | 0.101016..0.117683 s |
| DefenseMatrix, no nudge | 37.24 cm | 2.68 -> 37.26 cm | 0.100000..0.116667 s |

Each listed modifier interval takes approximately 16.67 ms. The input/output
measurements come from UE 5.6's existing log-only `a.MotionWarping.Debug 1` path,
enabled in the recorded command for each case. `RootMotionModifier_SkewWarp.cpp`
passes `InRootMotion` and `FinalRootMotion` to `PrintLog`. Output is measured before
final CharacterMovement; the independent actor samples closely agree with the
large no-nudge output. This locates the remaining abrupt movement in the
motion-warp result rather than an unlogged sync nudge. It does not isolate every
contribution of target geometry, remaining root translation, shear and timing.

The first cross-boundary sample contains the fixture's request-time repositioning
and is excluded from the step maximum. The correction-enabled cases still apply
an instantaneous 61.4 cm separation correction. Their smaller later warp steps
must not be reported as smooth entry. The no-nudge interrupted repeat peaks at
22.58 cm, while its original inconclusive attempt has a 36.20 cm modifier peak;
the exact magnitude varies with the sampled montage interval.

The 48 reviewed central crops show a wider early reach without nudging, followed
by close restraint and a raised/lowering blade. Completed cases still have
approximately 58..63 degrees of actor-heading difference near montage 0.6 s.
Hands, forearms, weapons and the victim's upper body overlap in projection and
obscure the intended surface. Exact blade-to-skin contact and penetration remain
indeterminate from these views. All four native evaluations fail the unchanged
provisional upper-torso contact criterion and relative-alignment criteria. Bone
points and an 18 cm torso sphere are not a mesh-surface measurement.

Request-time victim health is 1; lethal state is first observed near montage
0.016667 s in every final scenario. Ownership correction has not moved the early
authored damage time. Foot support, full recovery presentation, audio, haptics and
overall artistic quality were not reviewed by the selected entry/strike pages.

## Next concrete work

Define and implement bounded entry translation/rotation using the existing paired
generation and alignment ownership, with an explicit failure policy when the
target cannot be reached within its budget. Verify varying starting geometry,
frame cadence, interruption and moving targets. Preserve intended contact and
alignment as separate criteria. UE's existing SkewWarp speed-ratio clamp is a
candidate mechanism to evaluate, not a complete acceptance policy: reducing speed
can leave the endpoint unreachable. Do not substitute retimed damage or a smaller
reported step for a coherent entry trajectory.

Once entry is coherent, evaluate the intended head/neck contact surface and select
sync timing against that evidence. Shared surface-analysis implementation remains
owned by the standalone AnimationAnalysis workstream.

## Evidence and preservation

Evidence is under `Saved/Logs/PairedSyncOwnership-20260911-202640/`:

- `verification.json` records builds, both full runs, focused regressions,
  scenarios, native evaluations and preservation checks.
- `comparison.json` and `warp-diagnostics.json` retain samples, actor identities,
  correction logs and all nine rendered attempts, including the sampling failure.
- `*-contact.html` and `*-visual-analysis.html` are self-contained review pages.
- `scoped-changes.patch` separates this slice from preexisting source changes.
- `retention.json` records the hash-verified archive, removed PNG paths and replay
  instructions. Raw image-dependent reports require restoration; portable reviews
  embed their frames.

The tested source/config/tool/dependency identity is
`9f0fcd45fe9d48aa5828d1e994203aadf10510bca44a1fd4f8fa334a792ecc93`
over repository HEAD `50685e86dc2a284408f9f8fa6fbcedd37efed470` plus the current WIP.
Katana's AnimationAnalysis implementation pin remains
`3fd91eb70be340db63778a697b12465ab895cd8a`; standalone HEAD
`dbb523442605f19eff57b8c7d65538027c86c9a3` has documentation-only changes since that
pin. A concurrent edit to its `docs/SURFACE_CAPABILITIES.md` was observed and left
untouched. Neither the shared workspace nor generated plugin was edited here.

Preservation passed for all 7,948 Content size/time records, 54 protected asset
hash/absence records and 35 untouched WIP files. The earlier facing changes within
the two intentionally edited source files remain intact in the scoped diff.
Changes remain local and uncommitted.

Cleanup completed: **994 PNG files** were archived, verified and removed, leaving
zero PNG files within this slice's capture/review/writer directories. The archive
contains 1,845 verified file entries, is 660,508,963 bytes, and has SHA-256
`573c17c4a5beb0de3bd87dc0c07358807c41117dcdeeda9b605659010ad5c0b3`.
All four portable contact pages loaded successfully after image removal.
