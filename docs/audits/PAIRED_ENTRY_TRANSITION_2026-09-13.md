# Paired entry transition qualification

The named capture fixture now settles its live walking capsules before input,
removing DefenseMatrix's 5.85 cm movement-restoration drop. Entry presentation can
start at an explicit source time, including non-unit asset rates. The evaluated
0.375 s phase is **not an accepted animation improvement**: it trades reduced
right-foot motion for increased left-foot motion. The finisher's rapid forward
advance remains the next authoring problem.

## Identity and changes

- Starting Katana commit: `973158b190eec237a592352d9ebad7e17bb177c0`.
- Source-phase API: `b53f17306716cd87fbf44ddaf1f055b7fad7db5f`.
- Capture placement/provenance: `16acdec250a41df0da3f828c6a17e75304ce0fbd`.
- Branch: `investigate/finisher-source-pair`; these commits are local, not pushed.
- AnimationAnalysis pin remains `2fb0dc980dbdba1348b02e3a93b4512606aa5d16`.
- Qualified working-source identity:
  `e5dd12fb3c26a9fa6a4dbc37dcce2faee3ee5c845ff5bde6c2e65c8f5e8c282c`.
- Evidence root: `Saved/Logs/EntryTransition-20260913-160006/`.
- UE `5.6.1-44394996+++UE5+Release-5.6`, Win64 Development editor; Windows 11
  `10.0.26200.9445`, Core Ultra 9 275HX, RTX 5090 Laptop GPU, D3D11, 960x540.
  Other Unreal project builds ran concurrently; timings are not benchmarks.

`MovementStartTime` defaults to zero and uses source animation seconds. Remaining
single-cycle coverage subtracts the offset before dividing by asset and requested
rates. Playback starts at source time divided by asset `RateScale`; UE 5.6's dynamic
montage factory ignores its own start-time argument. Instance/generation ownership
and replacement/cancellation behavior remain intact. Ten- and sixteen-field tuning
documents remain readable; seventeen-field documents retain explicit phase.

The reusable test-side floor helper requires an upright walking capsule, ordinary
gravity, a walkable nonpenetrating capsule-sweep result and bounded correction.
It queries before moving and again afterward; unsuccessful settling restores the
requested position. It never runs during entry or paired playback. Named placement
evidence retains requested/achieved coordinates, live capsule dimensions, floor
component/impact, distances and correction. Python validates that contract without
weakening the original position tolerance.

The capture commit also includes the existing, now-qualified named-placement,
montage-layout and override-sidecar work required to reproduce these experiments.
Six of the fifteen pre-existing WIP files were extended; nine retained their
original bytes. The unrelated header comment and four authoring documents remain
WIP. The only uncommitted source difference at qualification is that header comment;
the complete working bytes and prior versions are archived. All 7,948 Content
size/mtime records remained unchanged. No Content package was saved.

## Cases and criteria

The production paired montages are `AM_Finisher_Attacker` and `AM_Finisher_Victim`
under `/Game/ProjectFiles/Animation/Montages/Defense/GateA/`, section `Finisher`.
Their sources are `GhostSamurai_Ambush01` and `GhostSamurai_Ambushed01` under
`/Game/Assets/Animations/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Execution/`.
Full object paths, sections, segments, rates and asset hashes are in each capture.
The existing legacy asset directory was not renamed or saved.

Entry candidates use `WalkForward_InPlace` and `WalkLeft_InPlace` under
`/Game/Assets/Animations/KatanaAnimset/InPlace/`, with matching root-motion clips
inspected separately. All sampled walk clips have length about 1.3 s and asset rate
1. Raw root-track counterparts travel about 158.588 cm per cycle, approximately
122 cm/s. The live path retains its AnimBlueprint and transient slot montage.

Before capture, `criteria.json` declared centimetres, simulation seconds, source
seconds, a 20 cm fixture correction budget, and at most 0.1 cm vertical discontinuity
through entry/restoration. The registered limits remain 0.075 s between telemetry
samples and 0.12 s between images. No artistic or planted-foot acceptance threshold
was invented. Raw root-relative pelvis/foot positions at 60 Hz screened the phase
candidate; the actual 0.375 s phase was evaluated in live capture separately.

Straight entry starts 150 cm behind the victim and approaches a 100 cm relative
pose at 122 cm/s, deadline 0.65 s, travel budget 150 cm, turn rate 540 degrees/s,
turn budget 180 degrees, position/yaw tolerances 2 cm/3 degrees. Movement blends
are 0.1 s in and 0.25 s out, matching the paired montage's 0.25 s blend-in.
The cancellation control uses 60 cm/s and play rate `60/122`, with a 1.1 s deadline.
The oblique placement puts the victim at `(120,20,0)` relative to the fixture and
yaw 30 degrees. All settings are transient and recorded.

| Case | Entry interval (s) | Initiator travel (cm) | Samples / images | Result |
| --- | ---: | ---: | ---: | --- |
| ThirdPerson, phase 0 | 0.4051 | 49.419 | 301 / 150 | Pass |
| ThirdPerson, phase 0.375 | 0.4004 | 48.848 | 307 / 153 | Pass |
| DefenseMatrix, phase 0 | 0.4002 | 48.825 | 306 / 152 | Pass |
| DefenseMatrix, phase 0.375 | 0.4005 | 48.863 | 308 / 153 | Pass |
| Slower entry cancellation | 0.4676 | 28.054 | 168 / 83 | Pass; no paired playback |
| Oblique, forward walk | 0.3677 | 44.857 | 309 / 154 | Pass |
| Oblique, left walk | 0.3667 | 44.734 | 309 / 154 | Pass |

Intervals use request and first paired-playback sample, or interruption marker;
they retain actual cadence rather than substituting executor ticks. Victim drift
before paired playback is zero in every completed case. Both actor heights remain
unchanged through sampled entry/restoration. DefenseMatrix records requested Z 96,
88 cm capsule half-height, floor distance about 8 cm and correction -5.849902 cm,
yielding Z 90.150098 and 2.150001 cm floor clearance before input. ThirdPerson already
has appropriate clearance and receives no correction.

The first entry sample records the requested phase plus its actual elapsed time;
phase residuals are below 0.000001 s. Native control also verifies asset rate 2.
All paired transitions retain summed montage weights near 1, but actor speed still
peaks at approximately 514.506 cm/s. That sum is not a full AnimGraph blend proof.

Over the sampled interval from 0.1 s before first paired playback to 0.25 s after,
ThirdPerson's left-foot peak world speed rises from 653.48 to 916.40 cm/s with the
candidate, while the right foot drops from 487.02 to 251.81 cm/s. DefenseMatrix
reproduces that tradeoff (650.18 to 916.65; 495.19 to 251.84). The wider entry stance
is visible in the reviewed frames. A nearer raw pose is insufficient to choose the
live transition. These bone speeds do not identify planted-foot sliding.

The oblique left-walk candidate reduces left-foot peak speed from 905.19 to
667.31 cm/s, while increasing right-foot speed from 241.76 to 529.36 cm/s. Directional
presentation is selectable and useful for comparison; this does not qualify an
arbitrary-heading policy or replace deliberate turn/approach choreography.

## Verification, failures and replay

The editor build passed. The first native run passed 73/74: an adversarial floor
case exposed partial movement after a rejected settling sweep. After adding
rollback, the rebuilt run passed **74/74**, with discovery/completion counts and
exit 0 verified. It retains 40 automation warning lines and no automation errors.
Python passed **95/95**. Exact commands, logs and durations are archived.

```powershell
& 'C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat' KatanaCombatEditor Win64 Development -Project='D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject' -Progress -NoHotReload -MaxParallelActions=2 -NoUBA
python -m unittest discover -s Tools/CombatCapture -p 'test_*.py'
```

Native selection was
`Automation RunTests KatanaCombat.PairedAnimation+KatanaCombat.Targeting.BoundedAlignment+KatanaCombat.Capture.ScenarioPlacement;Quit`
with NullRHI, unattended execution and `-TestExit=Automation Test Queue Empty`.
The correction build took 18.63 s after waiting for another project build; total
elapsed time was 325.08 s. Native regression took 18.02 s and Python 5.56 s.

Every rendered command is in `capture-runs.json`. Common options were:

```text
python Tools/CombatCapture/run_scenario.py --mode rendered --skip-build
  --camera-view opposite --finisher-experiment paired-warp-tuning
  --victim-warp-window .15 .45 --victim-warp-offset 42 0 0
  --victim-facing-policy match-partner-heading --primary-sync-time .466667
  --sync-nudge disabled --map <map> --variant <variant>
  --placement <named placement> --entry-config <archived JSON>
```

Seven complete runs retain 2,008 samples and 999 images. Achieved telemetry cadence
is 58.77–59.96 Hz and image cadence 29.48–29.98 Hz; maximum gaps are 0.0719985 s in
both streams. The complete runs report zero image rejection/failure and telemetry
record loss. Actual gaps remain visible. End-to-end runs take 32.30–39.69 s including
editor startup and analysis. The two interval diagnostic passes take about 1.25 s
combined offline; this is not a real-time implementation claim.

Two additional attempts stopped with `image_queue_limit_reached`, one rejected
image each: ThirdPerson phase zero retained 94 samples/46 images, and oblique left
retained 245 samples/121 images. Their gameplay checks passed, but capture export
failed and they are excluded from complete-run claims. Each was retried once with
unchanged sampling/budgets. All nine attempts are retained. The four-frame PNG queue
and concurrent build load make capture reliability a follow-up concern; this
qualification does not establish reliable 30 fps PNG throughput under contention.

`visual-review.html` embeds 21 reviewed original frames with original timestamps
and hashes, published through the shared visual-review API. It records three
consistent findings, one concern and one indeterminate finding, not visual-quality
acceptance. `approach-measurements.json`, `transition-measurements.json`, the raw
inventory, exact profiles, failed attempts and replay files retain the measurements.

Next: address the approach-to-finisher root-motion velocity handoff and choose an
authored ending that fits both feet. Preserve the explicit phase control and floor
fixture; keep phase zero as the default. Staged turns must preserve total travel,
turn and deadline budgets. The existing [shared mesh handoff](ANIMATION_ANALYSIS_CONSUMER_HANDOFF_2026-09-13.md)
still governs live AnimGraph/coherent-pair acquisition and omitted material effects.
No shared repository was edited, and no live mesh/contact coverage was upgraded.
Sampled RGB/bones establish neither continuous collision, physical contact,
containment nor artistic acceptance. See the [entry guide](../guides/PAIRED_ENTRY_PRESENTATION.md)
and [plan](../plans/PAIRED_ENTRY_TRANSITION.md).

Replay archive: `paired-entry-transition-evidence.zip` in the evidence root,
SHA-256 `b94ad11c917469fc155c898954ae0c37e5d2db66a0a3260966ba7b9323122acf`.
It contains 1,518 payload members and is 725,701,699 bytes. Every member was read
back and hash-verified before removing all 1,166 owned PNGs across the nine capture
directories. Zero loose PNGs remain there. `archive-manifest.json` inside the ZIP
maps replay members to hashes; `archive-verification.json` records verification
and cleanup. The embedded visual review remains readable without loose images.
The archive contains this audit before the final archive-identity paragraph.
