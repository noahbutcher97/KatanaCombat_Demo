---
name: katana-capture
description: Record and review a rendered KatanaCombat capture with video, the PresentationCapture MP4 joined frame by frame to AnimationAnalysis samples, combat telemetry and contact markers. Use when a change affects anything a player sees or feels (attacks, reactions, knockback, hitstop, finishers, parries, timing, camera, effects), or when asked to capture, record, film or review combat feel.
---

# Katana Capture

## When A Capture Is Required

Any change a player sees or feels needs a rendered capture with video before it is called done: animation and
montage timing, hit reactions, knockback, hitstop, stagger, finishers and paired entries, blocks and parries,
input lockout, camera, and effect timing. The baseline runs `-NullRHI`, so it proves mechanics only; it never
shows a frame. Docs, tooling and data plumbing with no runtime effect need no capture.

A capture is evidence for review, not a verdict. Report what the frames show, with frame indices.

## Run A Scenario Rendered With Video

1. Commit the change. Captures measure a committed revision, never a working tree with edits in progress.
2. Build a capture worktree at that commit:

   ```powershell
   python Tools/CombatCapture/capture_worktree.py --ref <branch-or-commit>
   ```

   It creates or updates the detached sibling `<main checkout>-capture-run`, runs the AnimationAnalysis and
   PresentationCapture setups (both clone from GitHub; PresentationCapture's repository is private), builds with
   `-WaitMutex`, and prints `CAPTURE_WORKTREE=<path>` and the run command. It refuses a target with uncommitted
   changes, one it did not create, or one holding commits no branch contains, and never deletes anything.
3. Run the scenario from that worktree, on an idle machine (contention degrades the clip):

   ```powershell
   python "<worktree>/Tools/CombatCapture/run_scenario.py" --map ThirdPerson --variant Completed --mode rendered --video --skip-build
   ```

   `--rhi dx12` forces D3D12 for that launch; D3D11 stays the default. `--video-resolution 360|720|1080`,
   `--video-fps` and `--video-seconds` (1-30, default 30) set the clip; the PIE viewport is sized to the box so
   the clip is not scaled.
   Registered scenarios: `finisher-recovery.json` (default) and `hold-release-recovery.json`
   (`--scenario Tools/CombatCapture/scenarios/hold-release-recovery.json`).

Ordinary PIE (a person playing, or an ad hoc check): in the PIE console run
`Combat.Capture.Start <Name> 30 0 60 Video=1`, play, then `Combat.Capture.Stop` (ending PIE also finalizes it).
The encoder finishes after Stop: wait until `video/<captureId>/video-manifest.json` exists in the bundle, then run
`python Tools/CombatCapture/video_capture.py Saved/CombatCaptures/<bundle>`. That clip is
the level viewport scaled to the output box; scenario clips are native.

## Where Outputs Land

- `Saved/CombatScenarioRuns/<batch>/<run>/run.json`: mechanical `status`, `video_quality` and its reasons,
  `video` (resolution, join, contacts), `rhi_used`. Exit 0 only means the mechanical evaluation passed.
- The bundle `Saved/CombatCaptures/<UTC>-<GUID>/`: `samples.jsonl`, `markers.jsonl`, `<Role>.actions.csv`,
  `<Role>.defense.csv`, `scenario.json`, `evaluation.html`, `report.html`, `capture-link.json`,
  `video/<captureId>/single.mp4` with `video-manifest.json`, `single-frames.csv` and the recorder's
  `manifest.json`, plus `video-analysis.json`, `video-review.html` and `video-review/*.png`.

## Review

1. Read `run.json`: `video_quality`, `video.resolution.native`, `video.join.exact_match_rate` (expect 1.0)
   and `video.contacts`.
2. Read `capture-link.json` and `markers.jsonl`. The link's `status` and `video.stop_reason` say how the clip
   ended. `stopped_by_recorder_limit` means the clip stopped at its own bound and covers only the start of the
   run; `stopped_by_recorder_error` carries the recorder's failure. `contact` markers carry `outcome` (`Hit`, `NormalBlock`,
   `UnblockableHit`), `attacker`, `victim`, `hit` and `direction_cm`; `defense` marks an input-stage parry;
   `contact_ignored` marks a contact the defense resolver ignored; `paired_sync` contacts come from finishers.
3. Map events to frames by engine frame, never by seconds. `video-analysis.json` lists every marker with the
   first `video_sample` that shows it, and every video frame with its `sample_index`.
4. Look at frames with the image reader: start with `video-review/contact-sheet.png`, then extract what you need:

   ```powershell
   ffmpeg -v error -i <bundle>/video/<id>/single.mp4 -vf "select=eq(n\,K)" -fps_mode passthrough -frames:v 1 frame_K.png
   ffmpeg -v error -i <bundle>/video/<id>/single.mp4 -vf "select=between(n\,K-3\,K+6)" -fps_mode passthrough window_%03d.png
   python Tools/CombatCapture/video_capture.py <bundle> --frames K1,K2,K3
   ```

   Read the samples and telemetry rows for the same engine frames before concluding what moved and why.
5. Report the run, the clip verdict, the frames you looked at and what they show. Say what a frame cannot show.

## Clip-Quality Gate

`video_capture.QUALITY_THRESHOLDS` grades each clip; the guide explains the values.

- `ok`: within every threshold.
- `degraded`: dropped frames, encoder-pressure skips above 1% of due draws, or a frame gap above 0.1 s.
  Re-run on an idle machine before drawing timing conclusions. Never read a degraded clip's hitch as hitstop.
- `invalid`: no complete, consistent clip (unfinalized, counts or PTS disagree). Read the reasons.

The gate never changes the mechanical status, and the mechanical status never vouches for the clip.
Video runs cannot serve as mechanical references (no PNG frames); pick references from ordinary rendered runs.

## Retention

Keep the MP4 with its `video-manifest.json`, frames CSV, recorder `manifest.json` and encoder log, plus
`capture-link.json`, samples, markers, telemetry, `scenario.json`, evaluations and `video-analysis.json`.
Prune extracted PNGs (`video-review/*.png`) and any PNG frames after review, recording the cleanup in
`retention.json` as the capture guide describes. Never prune an active capture or anything under `Content/`.

## Rules

- Never use live, editor-routed UEMCP calls while the owner's editor may be open; use files and offline tools.
- Never kill processes you did not start. Builds use `-WaitMutex` because other agents build in other worktrees.
- Never edit `Plugins/AnimationAnalysis` or `Plugins/PresentationCapture`; they are generated by setup scripts.
- Read `docs/guides/COMBAT_CAPTURE_AND_ANALYSIS.md#video-capture` for the full contract.
