# Combat capture and analysis

The [suite architecture contract](../architecture/ANIMATION_ANALYSIS_SUITE.md) governs this existing recorder, runners, analyzers, reports and fixtures as well as new capabilities. Commands below describe current Katana integration. The [portable Python package](../../Tools/AnimationAnalysis/README.md) owns geometry/raster/pixel/review services, shared integrity/identity/metric/interval/publication services and four command services; existing entry points delegate to it. `capture_format.py` owns legacy file selection and simulation-field translation. Recorder discovery/telemetry, remaining stream/report orchestration, preview integration and local launch defaults still require migration behind explicit adapters.

Run the [dependency setup](../../Tools/AnimationAnalysis/README.md) before building
a fresh checkout. Shared sources are edited in the separate AnimationAnalysis
repository; `Plugins/AnimationAnalysis` is a generated copy of the pinned commit.
`python Tools/AnimationAnalysis/setup_dependency.py` fetches that exact revision
from the public repository recorded in the dependency lock.

The [native capture plugin](../../Tools/AnimationAnalysis/README.md) owns recorder
lifetime, engine pose/view observations, image writing and surface readback.
`FCombatCaptureSession` preserves the project API and owns discovery/default points,
combat/warp telemetry and its global switches. Capture commands and schema-2
records remain compatible. The runner now records plugin source and its DLL in
producer provenance, excluding generated plugin build files from source identity.

Analyzer/evaluator identity now includes the portable package and project adapter
dependencies. Old references using only a script hash are incompatible and must be
explicitly regenerated from eligible runs. Runner source snapshots include the
locked package and `pyproject.toml` under `Dependencies/AnimationAnalysis/` keys.
The runner rejects a dirty dependency checkout or generated native source mismatch.
Captured execution identity excludes offline analysis
code, while the full source identity retains it; analysis changes are distinguished
from gameplay-source changes. See the [service migration audit](../audits/SHARED_ANALYSIS_SERVICES_2026-09-11.md).

For intended weapon/body contacts and authored-versus-gameplay root alignment, see [Paired animation evaluation](PAIRED_ANIMATION_EVALUATION.md). The generic distances below remain descriptive; the paired evaluator applies explicit intent and eligibility rules.

For bounded RGB/mesh-label/depth observations and coarse visible separation, see [Viewport surface analysis](VIEWPORT_SURFACE_ANALYSIS.md). Its optional renderer adapter is currently validated on known engine geometry and remains separate from this continuous recorder until skeletal pose linkage and readback overhead are established.

Current captures also record character `movement_mode`, `paired_state_lease_count` and `motion_warp_modifiers` (class, state, window, translation/rotation flags, target name, cached modifier target and current component target transforms). Targets are observed at world-tick end; actor displacement is not the applied root-motion delta. These fields distinguish an animated pose from movement/warp participation; `anim_root_motion_active` alone does not establish applied movement. Older schema-2 bundles may omit these additive fields. The [paired evaluation guide](PAIRED_ANIMATION_EVALUATION.md) describes transient movement and per-role translation controls; the [finisher movement experiment](../audits/FINISHER_MOVEMENT_EXPERIMENT_2026-09-10.md) records the initial results.

Use this tool to record ordinary PIE play or a scripted scenario, inspect synchronized images and motion, and compare measurements. It belongs to `KatanaCombatEditor`; it is available without loading an automation test. It does not depend on proof gate names, a local AI setup, a Python package installation, or a network service.

## Record an interaction

Build `KatanaCombatEditor`, start PIE on the map you want, and enter these commands in its console (or the editor Output Log when exactly one PIE world is running):

```text
Combat.Capture.Start LightAttackRecovery 30 5 60
Combat.Capture.Mark attack_pressed
Combat.Capture.Mark recovery_finished
Combat.Capture.Stop
```

Start arguments are `[Scenario] [MaxWallSeconds=60] [FrameHz=5] [SampleHz=60]`, positional or as `Name=Value`, followed by optional `Video=1 VideoFPS=60 VideoResolution=720 VideoSeconds=0` (see [Video capture](#video-capture)). The example records for at most 30 wall-clock seconds, requests five PNGs and 60 motion samples per simulation second, and labels the session `LightAttackRecovery`. Perform the interaction normally between commands. Set frame rate to `0` for telemetry/motion only. A manual marker records when its command executes; input telemetry carries the actual combat input timestamps. The console recorder also writes `contact` markers for every hit, block and parry between two recorded characters.

Each recording gets a unique `Saved/CombatCaptures/<UTC>-<GUID>/` directory, printed in the log. Stop, a configured limit, PIE teardown, or editor-module shutdown finalizes it. Existing recordings are preserved. One recording can be active at a time. Capture temporarily enables the existing action/reaction and defense telemetry switches, then restores their previous values. It does not clear existing component telemetry or change actors, cameras, animation ticking, combat decisions, or assets.

Console discovery enrolls the characters present at start, sorted by actor path. Roles are `Player1`, `Character1`, etc.; inspect `session.json` to see the actual mapping. New spawns are not enrolled automatically. For specific role names, non-character actors, explicit meshes, or custom bone/socket points, use the C++ API below. With multiple PIE worlds, use the intended world's console or pass that world explicitly to the API.

## Analyze and review

Python 3.10+ with its standard library is sufficient:

```powershell
python Tools/CombatCapture/analyze_capture.py 'Saved/CombatCaptures/<recording>'
python Tools/CombatCapture/analyze_capture.py 'Saved/CombatCaptures/<recording>' --distance Player1:hand_r Character1:pelvis
python Tools/CombatCapture/analyze_capture.py 'Saved/CombatCaptures/<candidate>' --baseline 'Saved/CombatCaptures/<reference>'
```

Open the generated `report.html`. It provides a scrubber and playback following the captured simulation timestamps, motion tables, links to nearby frames, point-distance results, and detailed data-quality findings. `analysis.json` contains the same numerical results for scripts. Multiple `--distance Role:Point Role:Point` options are supported. These are straight-line distances between sampled points, with coverage counts and the closest sampled timestamp/frame.

Exit codes: `0` means the capture's data-integrity checks passed; `1` means an incomplete recording or evidence-integrity finding; `2` means invalid input, unsupported schema, or incompatible comparison. A data-integrity pass does **not** mean animation quality passed. Read the coverage notes even when exit code is zero.

Baseline comparison requires matching map, scenario, roles/points, sampling settings, rendering mode, viewport resolutions, and distance requests. It reports metric deltas without imposing arbitrary quality thresholds. Use the same actions, duration, starting state, camera, and content revision for a meaningful experiment; those gameplay conditions are not enforced by the recorder. Different durations especially affect path length and event counts. Archive the source revision/diff and asset hashes alongside captures when using them as regression baselines.

## Reuse from automation or editor tooling

Depend on `KatanaCombatEditor` and include `Analysis/CombatCaptureSession.h`. Keep the session alive for the observation interval:

```cpp
FCombatCaptureSettings Settings;
Settings.Scenario = TEXT("AttackRecovery");
Settings.FrameHz = 10.0;

TArray<FCombatCaptureParticipant> Participants;
FCombatCaptureParticipant& Player = Participants.AddDefaulted_GetRef();
Player.Role = TEXT("Attacker");
Player.Actor = Attacker;
Player.Points = {TEXT("pelvis"), TEXT("hand_r"), TEXT("foot_l"), TEXT("foot_r")};

FString Error;
if (!CaptureSession.Start(PIEWorld, Settings, Participants, Error))
{
    // Surface Error to the test or tool user.
    return;
}
CaptureSession.Mark(TEXT("attack_requested"));
// Drive actions separately, then wait across world ticks in the caller.
// CaptureSession.Stop(TEXT("scenario_finished"), Error);
```

The recorder observes world ticks and matching viewport draws itself. Scenario code owns actions and assertions. The sample API accepts any actor in the selected PIE world and an optional skeletal mesh owned by that actor. Points resolve against that mesh; missing bones/sockets are `null`, never substituted with the actor origin. Names of roles must be unique ignoring case, at most 80 characters, and contain only letters, digits, `_`, or `-`.

The two `KatanaCombat.Capture.PIE` tests demonstrate console and API use on existing maps. They submit one scripted light-attack press/release through the combat input interface while leaving normal actors and AI running. Their purpose is recorder integration coverage; they do not assert all combat behavior, physical device latency, or visual quality. Existing defense proof fixtures retain their specialized assertions and older capture format. New scenarios should use this shared API; migrating those older outputs is not a prerequisite for recording another interaction.

## Image export and sampling overhead

Viewport readback still runs synchronously on the game thread. Lossless PNG encoding and file writing run on a bounded background queue: at most four submitted images and 64 MiB of reserved raw/encoded buffers. Compressor workspace is additional. Queue exhaustion, an oversized viewport or a failed image write makes the capture incomplete; frames are never silently dropped to preserve a pass. Pending file reservations also count against the session data budget.

Stop, world cleanup and recorder destruction drain submitted work before closing frame metadata and publishing completion. The number and size of outstanding jobs are bounded; drain latency still depends on compression and filesystem responsiveness. The final manifest records submitted, pending, rejected and failed images, peak queue depth, `capture_wall_duration_s` before draining, and `image_flush_wall_s`. `wall_duration_s` includes finalization.

Frame `wall_elapsed_s`, simulation time, engine frame, camera and pose links describe the original viewport observation. `readback_wall_s`, `image_encode_wall_s` and `image_write_wall_s` separate the costs; `image_collected_wall_elapsed_s` records when the game thread collected the finished export. The latter is not a replacement animation timestamp. Older schema-2 recordings may lack these additive diagnostics.

Requested sample/frame rates are upper bounds constrained by actual world ticks and viewport draws. Inspect the observed gaps in each relevant interval. Background export removes one source of stalls; it cannot generate a pose during a stalled game frame. Repeated rendering-enabled disabled/motion/rendered runs are the supported overhead comparison, with original clocks and eligibility criteria preserved.

## Repeatable scenario execution

From the repository root, this command builds the editor, runs the tracked scenario, hashes its source/configuration and project asset dependencies, then writes descriptive analysis and assertion results:

```powershell
python Tools/CombatCapture/run_scenario.py --map ThirdPerson --variant Completed --mode rendered
```

Use `--map all --variant all --repeat 3` for repeated completed/interrupted finishers on both maps. The default definition is `Tools/CombatCapture/scenarios/finisher-recovery.json`; the driver is `CombatCaptureScenarioTests.cpp`. The runner also registers directional hold/release recovery:

```powershell
python Tools/CombatCapture/run_scenario.py --scenario Tools/CombatCapture/scenarios/hold-release-recovery.json --map all --variant all --mode rendered
```

That scenario tests the real authored hold notify, competing attack input, directional follow-up, movement recovery and fresh attack input. A new family adds its own driver, definition and evaluator registration; ordinary console/API recording remains available for any PIE interaction.

`--mode motion` uses NullRHI and collects poses/telemetry without PNGs. `--mode disabled` executes the same gameplay assertions with recording disabled. For a meaningful recorder-overhead comparison, keep the render backend identical:

```powershell
python Tools/CombatCapture/run_scenario.py --map ThirdPerson --variant Completed --mode all --render-world --repeat 3
python Tools/CombatCapture/summarize_runs.py Saved/CombatScenarioRuns/<batch> --output Saved/CombatScenarioRuns/<batch>/summary.json
```

`--render-world` retains rendering in disabled and motion modes. Compare scenario wall/simulation durations, gameplay outcomes and event timings. Process startup and asset hashing are reported separately. Three runs are a small diagnostic sample, not a performance benchmark. `--skip-build` explicitly records use of an existing build; use it only after a successful build of the current source.

Each run has a unique directory under `Saved/CombatScenarioRuns/` containing the exact command, logs, initial/final status and artifact path. Exit 0 means the requested automation test and applicable evaluations passed; inspect individual `not_run` results for unmeasured assertions. Exit 1 means failed/inconclusive execution; exit 2 means runner setup failed. A timeout, missing artifact, mismatched run identity, changed source/asset during execution, or unsuccessful automation cannot reuse an older pass. The engine path and timeout can be set explicitly.

The driver injects Enhanced Input actions through the local player subsystem. It checks paired takeover, attack/movement suppression while inputs continue to be sampled, partner/token cleanup, release/repress attack recovery, movement acceleration, and active bystander StateTrees/attacks. The chosen player has extra health for the observation; enemy attacks, starting transforms, seed, camera and pose policy are declared fixture settings. This does not measure physical input-device latency.

The current authored finisher commits lethal damage at its initial sync notify. The interrupted variant cancels the real montage after 0.45 seconds and asserts preservation of already committed damage. It establishes cleanup after damage; it does not establish survival from cancellation before damage. The scenario records the victim's state at interruption. AI remains active, so a fixed seed reduces variability without promising deterministic navigation or frame timing.

The scenario forces pose evaluation for its two required participants and restores their previous policies. It also restores its camera and fixed viewport size. The recorder itself remains observational. Participant and nominated-mesh enrollment are fixed for a session: destruction produces missing evidence; replacements/new actors require a new session. Asset replacement on the same mesh is recorded and invalidates the current transition criterion.

## Contact markers and reaction review

`FCombatCaptureSession::ObserveContacts(Attacker, "Attacker", Victim, "Victim", Error)` writes a
`contact` marker each time the attacker strikes the victim: a committed defense contact on the victim
(`UCombatComponent::OnDefenseResolvedNative`), a weapon-trace hit on a non-character victim
(`UWeaponComponent::OnWeaponHit`), or a paired-animation sync point
(`UPairedAnimationComponent::OnPairedAnimationSyncPoint`). `ObserveParticipantContacts(Error)` marks
committed contacts between any two recorded combat characters, in either direction; the console
recorder uses it. Call either after `Start`; `Stop` releases the observer. The marker row carries a
`payload` the AnimationAnalysis recorder (native 0.4.0) stores verbatim:

| Key | Committed contact | Weapon trace | Paired sync point |
|---|---|---|---|
| `stage` | `contact` | `contact` | `contact` |
| `hit` | `committed-<n>` | `weapon-<n>` | `paired-<n>` |
| `attacker`, `victim` | the recorded roles | the roles given to `ObserveContacts` | same |
| `source` | `committed_contact` | `weapon_trace` | `paired_sync` |
| `outcome` | `Hit`, `UnblockableHit`, `NormalBlock` (`EDefenseOutcome`) | absent | absent |
| `region` | resolved target bone | hit bone name | empty |
| `direction_cm` | strike travel (unit); `direction_source` names the source | negated impact normal (unit) | victim minus attacker position (unit) |
| `impact_cm` / `sync_point` | impact point | impact point | sync point name |

Committed contacts also record `query_stage`, `attacker_response` and `damage_disposition`. Character
targets take the weapon's defense-contact path, which never reaches `OnWeaponHit`; until 2026-10 those
hits wrote no marker at all. Two committed resolutions keep their own labels so contact consumers never
read them as strikes: an input-stage perfect parry is a `defense` marker (`stage` `defense`), and a
resolution the resolver ignored (friendly, invulnerable, consumed or invalid, such as a swing through a
finisher victim) is a `contact_ignored` marker (`stage` `ignored`).

A finisher's damage is applied at its paired sync point, not by the weapon trace, so finisher
captures carry `paired_sync` contacts; every sync point of the pair writes one marker, named in
`sync_point`. A swing that touches nothing writes none. `FCombatCaptureSession::Mark(Label, Payload)`
writes any other payload marker; the recorder limits it to 4 KiB and rejects non-finite numbers.

The AnimationAnalysis Python package (0.5.0) measures the reaction from the bundle:

```powershell
animation-reaction-review --session 'Saved/CombatCaptures/<recording>' --criteria criteria.json --output report.json
```

with criteria naming the roles and bones, for example `{"victim": "Victim", "attacker": "Attacker",
"victim_bones": ["spine_03", "head"], "attacker_bone": "weapon_end"}`. The report gives per-bone onset
after contact, displacement direction relative to the strike, rotation over the window and the
stage chain, each stating whether the contact and direction came from the marker or were estimated
from pose. Its guide is `docs/guides/reaction-review.md` in the AnimationAnalysis repository. These
are sampled bone measurements relative to a stated contact; they are not contact, penetration or
quality verdicts.

## Video capture

One capture can hold both the data session and an MP4 of what the player saw. The video comes from
[PresentationCapture](../../Tools/PresentationCapture/README.md), pinned like AnimationAnalysis: run
`python Tools/PresentationCapture/setup_dependency.py` before building a fresh checkout. It reads the real
PIE backbuffer, encodes H.264 out of process and stamps each frame with its acquisition time
(variable frame rate). It needs a rendering editor: `-RenderOffScreen` works, `-NullRHI` cannot record,
so the baseline never exercises it.

**Record.** In PIE: `Combat.Capture.Start <Name> 30 0 60 Video=1`, play, `Combat.Capture.Stop`. Options:
`VideoFPS` (1-120, default 60), `VideoResolution` (360, 720 or 1080: output boxes 640x360, 1280x720,
1920x1080), `VideoSeconds` (1-30; 0 follows `MaxWallSeconds`, capped at the recorder's 30 s). With
`Video=1`, `FrameHz` defaults to 0: synchronous PNG readback stalls the game thread, and the stalls would
show in the clip. From C++, set `FCombatCaptureSettings::bRecordVideo` and the `Video*` fields. A video
request without a rendering editor, or while the recorder is busy, is refused before any bundle is
created. Starting the encoder blocks the game thread for 0.1-0.2 s, after the data session starts.

**Outputs.** The clip lands in the bundle: `video/<captureId>/single.mp4`, with the recorder's
`video-manifest.json` (completion, cadence, gaps, skips), `single-frames.csv` (per frame `drawGameFrame`,
acquisition time, `expectedPTSSeconds`), its telemetry `manifest.json` (per game frame platform seconds)
and the encoder log. `Stop` stops the clip first and returns; the encoder then finalizes on later ticks,
so wait for `FCombatCaptureSession::IsVideoFinalizing()` to clear (or for `video-manifest.json`) before
reading it. The scenario driver waits before publishing `scenario.json`.

**Link.** `capture-link.json` holds the clip's relative directory, capture, clock and world identities,
and both start instants as engine frame, platform time and world time. AnimationAnalysis's
`session.json` has no absolute start, so the link recovers it from the recorder's own status (platform
time minus capture wall seconds, within a microsecond) instead of changing the plugin. It also records
the PIE viewport widget and scene viewport sizes, and the stop instant. `video_started` and
`video_stop_requested` markers carry `platform_seconds` and `game_frame` anchors, and the recorder's
`manifest.json` names this bundle in `optionalChannels`.

**Join.** A video frame's `drawGameFrame` and a sample's `engine_frame` are both `GFrameCounter` on the
game thread, so frame k shows the world state of the sample with the same engine frame. This is a
game-frame to game-frame match, not the render-frame equality the recorder's schema warns against. The
spike and the acceptance runs joined 100% of frames. Exact joins need the game frame rate at or below
`SampleHz` in dilated world time; otherwise the analysis reports the nearest earlier sample and its lag.
Map events to frames by engine frame, never by seconds: a frame's PTS is its backbuffer acquisition,
8-37 ms after its game frame. Telemetry `unscaled_timestamp` values are platform seconds.

**Resolution.** The recorder reads the viewport widget's area of the window backbuffer and scales it into
the output box. In the level editor viewport that widget is sized by the editor layout, 759x378 under
`-RenderOffScreen` regardless of `-ResX`/`-ResY`, so console clips are scaled (to 1280x636 at 720). Scenario
video runs start PIE in their own window and grow it until the viewport widget equals the box (Slate's
title bar otherwise takes 32 px of a 1280x720 window), then fix the scene viewport to the same size. The
definition's 960x540 has the same 16:9 framing. `video-analysis.json` reports `resolution.native` only
when widget, scene viewport and output all equal the requested box.

**Analyze.** `python Tools/CombatCapture/video_capture.py <bundle>` (ffprobe and ffmpeg from PATH;
Pillow for the contact sheet) checks the MP4 against the recorder (codec, size, decoded frame count, every
decoded PTS within 2 us of the CSV), joins every frame to its sample, cross-checks the platform clocks,
maps every marker to the first frame that shows it, extracts review frames and writes
`video-analysis.json`, `video-review/contact-sheet.png` and `video-review.html`. `--frames 12,40` picks
frames; `--no-extract` skips them. Exit 0 means `ok`, 1 `degraded` or `invalid`, 2 unreadable input.

**Clip-quality gate.** `video_quality` is `ok`, `degraded` above a named threshold, or `invalid` when the
clip is incomplete or inconsistent. The thresholds are evidence settings in `QUALITY_THRESHOLDS`:

| Threshold | Value | Why |
|---|---|---|
| `max_dropped_frames` | 0 | An admitted frame lost after acquisition; the recorder also marks the clip incomplete |
| `max_pressure_skip_fraction` | 0.01 | Due acquisitions skipped under encoder pressure are missing frames. Clean spike runs skipped 0-1 of about 270 (under 0.4%); CPU-contended runs skipped 45-59% |
| `max_frame_gap_s` | 0.1 | Above six 60 FPS frames a stall reads as a hitch and can pass for hitstop (authored hitstops are 0.04-0.1 s). Clean D3D11 spike runs peaked at 71-72 ms, D3D12 runs at 117-537 ms |

The gate grades the recording, never the combat: it does not change the mechanical evaluation. A gap below
0.1 s can still hide the shortest hitstop, so check `local_max_gap_ms` on the markers around an event.
Encoding is software Media Foundation H.264, so CPU load from other processes, not the RHI, drives most
pressure; record on an idle machine.

**Scenario runs.** `run_scenario.py --mode rendered --video` records the registered scenario with a clip
(`--video-resolution`, `--video-fps`), sizes `-ResX`/`-ResY` to the box and writes `video_quality`, the
reasons and the join into `run.json` beside the unchanged mechanical `status`. `--rhi dx12` or `--rhi d3d11`
forces an RHI for that launch; `run.json` records the RHI the editor log shows. Runner provenance includes
the recorder pin, its generated source, DLL and worker executables. The motion `report.html` links the
video review.

**Capture worktrees.** `python Tools/CombatCapture/capture_worktree.py --ref <branch-or-commit>` creates or
updates a detached sibling worktree (`<main checkout>-capture-run` by default) at that commit, runs the
plugin setups present at that revision (reusing this checkout's AnimationAnalysis pin clone), builds with
`-WaitMutex` and prints the `run_scenario.py` command, with `--skip-build`. Captures then measure a
committed revision, unaffected by edits in progress elsewhere. It refuses a target with tracked or
untracked changes, an ordinary directory, a worktree it did not create and the main checkout. It never
deletes, cleans, resets or force-checks-out; ownership and history live in the ignored
`Saved/capture-worktree.json`. A fresh worktree checks out its LFS content and builds from scratch, and the
runner's isolated `Saved/CombatCaptureCache` derived-data cache starts cold.

**Review and retention.** Agents follow the `katana-capture` skill: read `run.json` and the link, map
markers to frames, extract frames with ffmpeg and read them as images. Keep the MP4 with its manifest,
frames CSV, telemetry manifest and encoder log, the link, samples, markers and telemetry. Prune extracted
review PNGs after review as described under [Eligibility and selected references](#eligibility-and-selected-references).

**Automation.** `KatanaCombat.Capture.Video.ConsolePIE` drives ordinary PIE through the console with
`Video=1`, lands two light attacks on a training dummy, and checks the link, the finalized clip, a 100%
engine-frame join and a `Hit` contact marker per landed hit; headless it checks the refusal. `KatanaCombat.Capture.CommittedContactMarkers` checks hit and block markers headless.

## Bundle schema (version 2)

| File | Contents |
|---|---|
| `session.json` | Status (`recording`, `complete`, `error`), stop reason, engine/map/world identity, UTC start, settings, roles, source actor/mesh paths, counts, telemetry loss/reset counts, export errors |
| `samples.jsonl` | Simulation/wall/engine-frame clocks, time dilation/pause state, actor transforms/velocity, world/component/actor-relative points and bone quaternions, explicit pose-finalization serial/frame/time, mesh identity, contributing montage instances/weights/rates, combat/paired/victim/AI ownership, input suppression and movement acceleration |
| `frames.jsonl` | PNG file, actual resolution, simulation/wall timestamps, last preceding sample index, exact PIE world and draw index, pixel-variation check, compilation readiness, active camera transform/view target |
| `markers.jsonl` | Timestamped user/scenario labels plus capture start/stop; `contact` markers carry a `payload` object (see below) |
| `<Role>.actions.csv` | Existing action/reaction telemetry schema, including input serial, queue identity, action start, montage and movement/AI events |
| `<Role>.defense.csv` | Existing defense telemetry schema, including stage/alignment/damage/cleanup events |
| `frames/*.png` | Images from the exact selected PIE game viewport after drawing |
| `scenario.json` | Optional scenario definition, events, individual gameplay assertions, actual interruption outcome and project package dependencies |
| `run-context.json`, `asset-identity.json` | Runner identity, source/config/scenario hashes, engine/build and editor DLL hashes, declared candidate changes, transitive project package hashes and absent soft dependencies |
| `evaluation.json`, `evaluation.html` | Per-assertion result, evidence interval, eligibility, threshold/reference basis, evidence links and bundle/evaluator identity |
| `capture-link.json` | Video only: clip directory and identities, both start instants, viewport sizes, stop instant and join rule (see [Video capture](#video-capture)) |
| `video/<captureId>/` | Video only: PresentationCapture's `single.mp4`, `video-manifest.json`, `single-frames.csv`, telemetry `manifest.json` and encoder log |
| `video-analysis.json`, `video-review.html`, `video-review/` | Derived by `video_capture.py`: validation, quality grade, frame-to-sample join, marker-to-frame map, review PNGs and contact sheet |

Positions and distances are Unreal centimetres; Euler rotations are degrees; bone quaternions use XYZW; timestamps/intervals are seconds. Simulation timestamps share the selected world's clock. The recorder's wall elapsed time starts at session creation. Telemetry retains its existing `unscaled_timestamp` convention; correlate to samples using `simulation_timestamp`, not by equating the two wall clock fields. Frame records include pose identities and measured lag to the preceding sample. The analyzer still reads schema 1; the transition evaluator requires schema 2 to establish pose freshness.

Samples and frame ledgers are streamed incrementally. Telemetry is drained each world tick into bounded per-session memory and written as CSV at stop; a process crash can lose that telemetry and buffered writes. The initial manifest remains `recording` after an unfinalized crash. Analyze only finalized bundles for comparisons. Telemetry events present before start are excluded; loss/reset counters identify discontinuities observed while recording.

Default limits: 7,200 samples, 600 PNGs, 60 wall seconds, 512 MiB of data files, 20,000 records per actor **per telemetry stream**, 10,000 markers. API limits are 32 participants, 64 points each, 240 sample Hz, 60 frame Hz, 600 seconds, 144,000 samples, 3,600 PNGs, 2 GiB of data and 100,000 records per actor per stream. Metadata permits 32 entries with keys up to 80 and values up to 4,096 characters. The final diagnostic manifest is exempt from the data budget so an exhausted recording can explain its failure. A conservative raw-image estimate bounds each PNG write. Reaching sample/frame/time/data bounds stops recording with an explicit reason. A core ticker enforces wall timeout even when world ticks stop. Telemetry/marker loss is reported as incomplete evidence. Limits prevent unbounded recording; they are not a target for routine use.

## What the measurements establish

- Actor step, sampled speed, and accumulated path length describe movement between captured positions. Component-space point steps help isolate pose motion from actor translation. Large steps are review candidates, not automatically a discontinuity defect.
- Point distances measure the named bone/socket origins. They do not establish weapon contact, skin penetration, collision, or foot support. Foot sliding requires a support interval and ground/contact model before a useful threshold can be applied.
- Input-to-action latency correlates nonzero input serials within a participant's telemetry. It measures combat input capture to action execution start in simulation time, not physical input to first visible response. Unmatched inputs/actions are reported; absent or discontinuous evidence does not become zero latency.
- Sampling gaps and frame offsets are explicit. There is no synthetic interpolation and no claim of fixed-rate video. Bone poses can be stale for off-screen actors depending on visibility-based animation ticking; the recorder observes this setting without overriding it. Finalized bone callbacks identify evaluated poses, and contributing montage instances include their actual weights. AnimGraph node/curve contribution is not fully traced.
- The initial view waits for compilation and two ready draws. Subsequent frames continue through background asset/shader compilation and record `render_resources_ready=false` when applicable, so a late compilation cannot silently remove an attack or recovery from the sequence. The analyzer reports those frames and gaps at the beginning/end of the motion interval.
- PNG CRC/scanline checks, dimensions, pixel variation, timestamps and viewport provenance establish image integrity. They do not detect occlusion, framing quality, mesh penetration, contact plausibility, or artistic appeal. The camera is recorded as-is, with FOV and the engine's post-draw projection matrix/view rectangle when available. Choose a useful view before recording. The [paired evaluation guide](PAIRED_ANIMATION_EVALUATION.md#cross-check-visible-contact-before-tuning) describes named scenario views and a portable raw-pixel/projected-contact review.
- Screenshot readback is synchronous and affects execution time. Use simulation-clock motion measurements and treat wall-clock timings as diagnostic. This is not a performance benchmark or a replacement for Unreal profiling.

This bundle is also an input for human or vision-assisted review: image paths, roles, event labels, pose samples and clocks are explicit. No model-based perception scorer, universal feel score, foot-support detector, or learned quality threshold is included.

## Eligibility and selected references

`evaluation.html` separates recording integrity, measurement eligibility and assertion outcomes. Each case returns `pass`, `fail`, `inconclusive` or `not_run`, with a reason and simulation interval. Headless visual evidence is `not_run`. Missing actors/bones, stale evaluations, changed meshes, insufficient cadence, missing pre/post observations, early capture limits, unfinished rendering or mismatched frame/pose/resolution make the associated measurement inconclusive. A valid bundle alone is not an animation-quality pass.

The initial mechanical criterion measures the maximum sampled actor-relative pelvis displacement from just before the finisher request through ownership release and its immediate aftermath. It also reports component-space angular steps. A selected reference supplies a displacement envelope; exceeding it flags a regression for inspection. It cannot distinguish all authored movement from defects and does not certify foot support, contact-region correctness or artistic quality. Paused/dilated intervals currently remain inconclusive for this criterion; their clocks are recorded for a future evaluator with explicit hitstop semantics.

Request, observed paired start, first evaluated montage contribution, observed victim health decrease, input-to-action telemetry and movement-ownership restoration remain distinct observations. Sampled transitions retain their original bounding timestamps and uncertainty. Montage contribution is not first visible pixel response; health change is not proof of intended geometric contact.

References require at least three distinct unchanged, eligible rendered examples and an explicit acceptance basis/margin. They are never selected or replaced automatically:

```powershell
python Tools/CombatCapture/summarize_runs.py --reference-captures <capture1> <capture2> <capture3> --margin-cm 10 --basis "Describe reviewed mechanical controls and intended detection range" --output <new-reference.json>
python Tools/CombatCapture/run_scenario.py --map ThirdPerson --variant Completed --references <reference-directory>
```

The first reviewed mechanical reference set can be selected with:

```powershell
python Tools/CombatCapture/run_scenario.py --map all --variant all --references docs/references/combat-capture/2026-09-09
```

See [the scenario verification report](../audits/COMBAT_SCENARIO_EVALUATION_2026-09-09.md) for its observed variability, actual timing resolution, defect/removal results and overhead measurements.

Reference filenames are `<Map>-<Variant>.json`. Execution-source, configuration, scenario and asset differences require an explicit `--declare-change "..."`; scenario/criteria/evaluator incompatibility still rejects comparison. The complete source/tooling snapshot is retained separately from the execution-source identity, so stored captures can be re-evaluated after reporting changes. References retain run/bundle identities and observed variability. Keep small reviewed reference metadata in `docs/references/combat-capture/`; raw recordings remain generated artifacts and must be archived separately if long-term pixel review is required.

After completing capture comparisons, retain a small, explicitly identified review-image subset and consolidate reproduction scripts with the evidence. Prune bulk generated PNGs once their required checks are recorded; use reversible removal where available. Preserve telemetry, timestamps, measurements, criteria, source/asset identities and original completion records. Record the cleanup in a separate `retention.json` beside each affected capture and label generated reports so historical successful exports cannot be mistaken for a currently complete image bundle. Re-running image eligibility or reviewing an unretained interval requires restoring the images or making a fresh capture. Never apply capture cleanup to `Content/`, user recordings or an active capture.

`--control-offset-cm 200 --declare-change "Inject transient 200 cm mesh displacement"` introduces a fixture-only defect to exercise detection. A fresh ordinary run verifies its removal. No asset or runtime source is changed by this control.

Analysis and evaluation replace previous success with an explicit in-progress state before reading inputs. Rejected reruns publish a fresh rejection; completed reports record input and evaluator hashes. An interrupted process leaves an inconclusive state. Re-evaluate a modified bundle instead of treating an old HTML page as current proof.

## Verification

```powershell
python -m unittest discover -s Tools/CombatCapture -p 'test_*.py' -v
python -m unittest discover -s Tools/PresentationCapture -p 'test_*.py' -v
```

After an editor build, run `Automation RunTests KatanaCombat.Capture;Quit` once with rendering and once with `-NullRHI`. Only the rendered run records video. The suite covers world/viewport identity, invalid roles, concurrent capture, lifecycle/limits, teardown restoration, and two real-map integrations. The finisher scenario additionally asserts that at least one `contact` marker with the reaction payload was written (`contact_markers_present`); the hold-release scenario records the count without requiring one. See [the implementation plan](../plans/COMBAT_CAPTURE_AND_ANALYSIS.md) for scope and acceptance.
