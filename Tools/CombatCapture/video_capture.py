"""Validate, join and grade the PresentationCapture clip recorded with a combat capture.

A bundle recorded with video (``Combat.Capture.Start ... Video=1`` or ``run_scenario.py --video``)
holds ``capture-link.json`` and ``video/<captureId>/``: PresentationCapture's ``video-manifest.json``,
``<seat>-frames.csv``, telemetry ``manifest.json`` and the MP4. This module

- checks the MP4 against the recorder's own accounting (ffprobe codec, size, decoded frame count and
  every decoded PTS against the frames CSV);
- joins every video frame to the AnimationAnalysis sample with the same engine frame (both are
  ``GFrameCounter`` on the game thread) and cross-checks the platform clocks;
- maps every marker, contacts included, to the first video frame that shows it;
- grades the clip ``ok``, ``degraded`` or ``invalid`` against the named ``QUALITY_THRESHOLDS``.

The grade is evidence about the clip, not about combat: it never changes a scenario evaluation.
ffprobe and ffmpeg come from PATH; contact sheets need Pillow. Ported from the scripts that validated the
test captures recorded on 2026-10-03, joined their clocks and built their contact sheets.
"""
from __future__ import annotations

import argparse
import bisect
import csv
import html
import json
from pathlib import Path
import shutil
import statistics
import subprocess

# Evidence settings for the clip-quality gate. A clip above any of them is `degraded`: still
# reviewable, but its timing cannot be trusted at the frame level. See the capture guide.
QUALITY_THRESHOLDS = {
    # Admitted frames lost after acquisition. The recorder already marks such a clip incomplete.
    "max_dropped_frames": 0,
    # Share of due acquisition opportunities the recorder skipped under encoder pressure. Each skip
    # is a missing frame. Clean D3D11 test recordings on 2026-10-03 skipped 0-1 of ~270 (<0.4%); CPU-contended runs
    # skipped 45-59% and showed 0.5-1.8 s holes.
    "max_pressure_skip_fraction": 0.01,
    # Largest interval between acquired frames. Above six 60 FPS frames a stall reads as a hitch on
    # playback and can be mistaken for hitstop (authored hitstops are 0.04-0.1 s). Those clean
    # D3D11 recordings peaked at 71-72 ms; that day's D3D12 recordings at 117-537 ms.
    "max_frame_gap_s": 0.1,
}
PTS_TOLERANCE_S = 2e-6  # PresentationCapture's own viewer accepts decoded PTS within 2 us of the CSV.
RESOLUTION_BOXES = {360: (640, 360), 720: (1280, 720), 1080: (1920, 1080)}


class VideoError(Exception):
    """The clip or its link cannot be read or interpreted."""


def find_tool(name):
    path = shutil.which(name)
    if not path:
        raise VideoError(f"{name} was not found on PATH. Install FFmpeg (it provides ffprobe and ffmpeg), "
                         "for example with `winget install Gyan.FFmpeg`, then open a new shell.")
    return path


def _json(path):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as error:
        raise VideoError(f"Cannot read {path}: {error}") from error


def _jsonl(path):
    try:
        with open(path, encoding="utf-8") as stream:
            return [json.loads(line) for line in stream if line.strip()]
    except (OSError, ValueError) as error:
        raise VideoError(f"Cannot read {path}: {error}") from error


def probe(mp4):
    """ffprobe stream facts plus every decoded frame's PTS, in presentation order."""
    ffprobe = find_tool("ffprobe")
    stream = subprocess.run([ffprobe, "-v", "error", "-select_streams", "v:0", "-count_frames", "-show_entries",
                             "stream=codec_name,profile,pix_fmt,width,height,nb_frames,nb_read_frames,avg_frame_rate,r_frame_rate,time_base"
                             ":format=duration,size,format_name", "-of", "json", str(mp4)],
                            capture_output=True, text=True)
    frames = subprocess.run([ffprobe, "-v", "error", "-select_streams", "v:0", "-show_entries", "frame=pts_time",
                             "-of", "json", str(mp4)], capture_output=True, text=True)
    if stream.returncode or frames.returncode:
        raise VideoError(f"ffprobe could not read {mp4}: {(stream.stderr + frames.stderr).strip()[-400:]}")
    facts = json.loads(stream.stdout)
    pts = [float(frame["pts_time"]) for frame in json.loads(frames.stdout).get("frames", []) if "pts_time" in frame]
    return dict(stream=(facts.get("streams") or [{}])[0], format=facts.get("format", {}), pts=pts)


def load_clip(bundle):
    """The link, the recorder outputs for this bundle's world and the bundle's own streams."""
    bundle = Path(bundle)
    link_path = bundle / "capture-link.json"
    if not link_path.is_file():
        raise VideoError(f"{bundle} has no capture-link.json; it was not recorded with video")
    link = _json(link_path)
    if link.get("schema_version") != 1 or link.get("kind") != "katana_combat_capture_link":
        raise VideoError("Unsupported capture-link.json")
    video = link.get("video", {})
    if not video.get("started"):
        raise VideoError(f"The clip never started: {video.get('error') or link.get('status')}")
    directory = bundle / video["directory"]
    manifest_path = directory / "video-manifest.json"
    if not manifest_path.is_file():
        raise VideoError(f"{manifest_path} is missing: the recorder has not finalized the clip")
    manifest = _json(manifest_path)
    windows = manifest.get("windows", [])
    window = next((w for w in windows if w.get("worldId") == video.get("world_id")), None)
    if window is None:
        if len(windows) != 1:
            raise VideoError("No recorded seat matches the captured world")
        window = windows[0]
    with open(directory / window["timestamps"], newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    telemetry = directory / "manifest.json"
    return dict(bundle=bundle, link=link, directory=directory, manifest=manifest, window=window, rows=rows,
                telemetry=_json(telemetry) if telemetry.is_file() else {},
                session=_json(bundle / "session.json"), samples=_jsonl(bundle / "samples.jsonl"),
                markers=_jsonl(bundle / "markers.jsonl"))


def validate_mp4(clip, probed):
    """Compare the decoded MP4 with the recorder's manifest and frames CSV."""
    window, rows, stream = clip["window"], clip["rows"], probed["stream"]
    expected = [float(row["expectedPTSSeconds"]) for row in rows]
    pts = probed["pts"]
    issues = []
    if not clip["manifest"].get("complete") or not window.get("complete"):
        issues.append(f"Recorder reports an incomplete clip: {clip['manifest'].get('failureReason') or window.get('failureReason') or 'no reason given'}")
    if stream.get("codec_name") != "h264":
        issues.append(f"Unexpected codec {stream.get('codec_name')!r}")
    if (stream.get("width"), stream.get("height")) != (window.get("width"), window.get("height")):
        issues.append("Decoded size differs from the manifest output size")
    if not (len(pts) == len(rows) == window.get("encodedFrames")):
        issues.append(f"Frame counts disagree: decoded {len(pts)}, CSV {len(rows)}, manifest {window.get('encodedFrames')}")
    deviation = max((abs(a - b) for a, b in zip(pts, expected)), default=None) if len(pts) == len(expected) else None
    if deviation is not None and deviation > PTS_TOLERANCE_S:
        issues.append(f"Decoded PTS differ from the frames CSV by up to {deviation * 1e6:.1f} us")
    draws = [int(row["drawGameFrame"]) for row in rows]
    if any(b <= a for a, b in zip(draws, draws[1:])):
        issues.append("drawGameFrame is not strictly increasing")
    intervals = [b - a for a, b in zip(expected, expected[1:])]
    return dict(
        valid=not issues, issues=issues,
        ffprobe=dict(codec=stream.get("codec_name"), profile=stream.get("profile"), pix_fmt=stream.get("pix_fmt"),
                     width=stream.get("width"), height=stream.get("height"), nb_read_frames=stream.get("nb_read_frames"),
                     avg_frame_rate=stream.get("avg_frame_rate"), duration_s=float(probed["format"].get("duration", 0) or 0),
                     size_bytes=int(probed["format"].get("size", 0) or 0)),
        decoded_frames=len(pts), csv_rows=len(rows), encoded_frames=window.get("encodedFrames"),
        max_pts_deviation_us=None if deviation is None else round(deviation * 1e6, 3),
        frame_interval_ms=dict(min=round(min(intervals) * 1e3, 2), median=round(statistics.median(intervals) * 1e3, 2),
                               max=round(max(intervals) * 1e3, 2)) if intervals else None)


def resolution_check(clip):
    """True only when the recorded viewport widget, the scene viewport and the output all equal the requested box."""
    window, video = clip["window"], clip["link"]["video"]
    box = RESOLUTION_BOXES.get(video.get("requested", {}).get("resolution"))
    output = (window.get("width"), window.get("height"))
    viewport = (window.get("viewportWidth"), window.get("viewportHeight"))
    widget = tuple(video.get("viewport_widget_px") or (0, 0))
    native = box is not None and output == box and viewport == box and widget == box
    return dict(native=native, requested_box=list(box) if box else None, output=list(output),
                scene_viewport=list(viewport), viewport_widget=list(widget),
                note="native" if native else "scaled: the recorder resized the viewport area to fit the output box")


def grade(clip, validation, thresholds=QUALITY_THRESHOLDS):
    """ok, degraded (above a named threshold) or invalid (not a complete, consistent clip)."""
    window = clip["window"]
    if not validation["valid"]:
        return dict(video_quality="invalid", reasons=validation["issues"], thresholds=dict(thresholds))
    # Thresholds are compared with the raw values; only the serialized `measured` fields are rounded, so
    # a value just above a threshold can never round down to it and pass.
    dropped = window.get("droppedFrames", 0)
    skips = window.get("pressureSkippedDraws", 0)
    due = skips + window.get("requests", window.get("encodedFrames", 0))
    skip_fraction = skips / due if due else 0.0
    gap = window.get("maxAcquisitionGapSeconds", 0)
    measured = dict(dropped_frames=dropped,
                    pressure_skipped_draws=skips,
                    pressure_skip_fraction=round(skip_fraction, 6),
                    max_frame_gap_s=round(gap, 6),
                    achieved_fps=round(window.get("achievedAcquisitionFPS", 0), 2),
                    requested_fps=clip["manifest"].get("requestedFPS"))
    reasons = []
    if dropped > thresholds["max_dropped_frames"]:
        reasons.append(f"{dropped} dropped frame(s) > max_dropped_frames {thresholds['max_dropped_frames']}")
    if skip_fraction > thresholds["max_pressure_skip_fraction"]:
        reasons.append(f"pressure skips {skips} of {due} due draws ({skip_fraction:.4%}) > max_pressure_skip_fraction "
                       f"{thresholds['max_pressure_skip_fraction']:.4%}")
    if gap > thresholds["max_frame_gap_s"]:
        reasons.append(f"largest frame gap {gap * 1e3:.3f} ms > max_frame_gap_s {thresholds['max_frame_gap_s'] * 1e3:.3f} ms")
    return dict(video_quality="degraded" if reasons else "ok", reasons=reasons, measured=measured, thresholds=dict(thresholds))


def join(clip, pts):
    """Video frame -> sample by engine frame, and the platform-clock cross-check."""
    rows, samples = clip["rows"], clip["samples"]
    by_frame = {}
    for sample in samples:
        by_frame.setdefault(int(sample["engine_frame"]), sample)
    sorted_frames = sorted(by_frame)
    frames, lags = [], []
    for k, row in enumerate(rows):
        draw = int(row["drawGameFrame"])
        exact = by_frame.get(draw)
        position = bisect.bisect_right(sorted_frames, draw) - 1
        earlier = by_frame[sorted_frames[position]] if position >= 0 else None
        lag = None if earlier is None else draw - int(earlier["engine_frame"])
        if lag is not None:
            lags.append(lag)
        frames.append(dict(video_sample=int(row["videoSample"]), pts_s=pts[k] if k < len(pts) else float(row["expectedPTSSeconds"]),
                           draw_game_frame=draw, sample_index=exact["index"] if exact else None,
                           nearest_earlier_sample_index=earlier["index"] if earlier else None, lag_frames=lag))
    draws = [f["draw_game_frame"] for f in frames]
    inside = [d for d in draws if sorted_frames and sorted_frames[0] <= d <= sorted_frames[-1]]
    exact_count = sum(1 for f in frames if f["sample_index"] is not None)
    summary = dict(video_frames=len(frames), inside_sampled_interval=len(inside), exact_engine_frame_matches=exact_count,
                   exact_match_rate=round(exact_count / len(frames), 4) if frames else None,
                   nearest_earlier_lag_frames={str(k): lags.count(k) for k in sorted(set(lags))},
                   sampled_engine_frames=[sorted_frames[0], sorted_frames[-1]] if sorted_frames else None,
                   video_draw_frames=[draws[0], draws[-1]] if draws else None)
    start = clip["link"]["analysis"].get("start", {}).get("platform_seconds")
    world_frames = clip["telemetry"].get("worldFrames", [])
    if start and world_frames:
        platform = {int(r["gameFrame"]): r["platformSeconds"] for r in world_frames}
        differences = [(platform[int(s["engine_frame"])] - (start + s["wall_elapsed_s"])) * 1e3
                       for s in samples if int(s["engine_frame"]) in platform]
        if differences:
            summary["clock_cross_check_ms"] = dict(
                matched_frames=len(differences), median=round(statistics.median(differences), 3),
                min=round(min(differences), 3), max=round(max(differences), 3),
                meaning="PresentationCapture post-actor-tick platform seconds minus the sample's platform time "
                        "(link start + wall_elapsed_s) for the same game frame; positive means the recorder ran later in the frame")
    return summary, frames


def map_markers(clip, frames):
    """First video frame drawn at or after each marker's engine frame, with the frame gap around it."""
    draws = [f["draw_game_frame"] for f in frames]
    mapped = []
    for marker in clip["markers"]:
        frame = int(marker["engine_frame"])
        k = bisect.bisect_left(draws, frame)
        row = dict(marker=marker["marker"], index=marker.get("index"), engine_frame=frame,
                   simulation_time_s=marker.get("simulation_time_s"))
        payload = marker.get("payload")
        if isinstance(payload, dict):
            row.update({key: payload[key] for key in ("hit", "stage", "outcome", "source", "attacker", "victim") if key in payload})
        if k < len(frames):
            near = frames[max(0, k - 3):k + 4]
            row.update(video_sample=frames[k]["video_sample"], pts_s=frames[k]["pts_s"], draw_game_frame=draws[k],
                       frames_after_marker=draws[k] - frame,
                       local_max_gap_ms=round(max((b["pts_s"] - a["pts_s"] for a, b in zip(near, near[1:])), default=0) * 1e3, 1))
        else:
            row.update(video_sample=None, reason="after the last video frame")
        mapped.append(row)
    return mapped


def image_statistics(path):
    from PIL import Image, ImageStat
    with Image.open(path) as image:
        rgb = image.convert("RGB")
        luma = ImageStat.Stat(rgb.convert("L"))
        return dict(size=list(rgb.size), luma_mean=round(luma.mean[0], 1), luma_stddev=round(luma.stddev[0], 1),
                    distinct_colors=len(rgb.getcolors(maxcolors=1 << 20) or []))


def extract_frames(mp4, picks, out_dir):
    """Decode the chosen frame indices to PNG with ffmpeg. picks: {label: frame index}."""
    ffmpeg = find_tool("ffmpeg")
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    extracted = []
    for label, index in picks.items():
        target = out_dir / f"{label}_n{index:05d}.png"
        completed = subprocess.run([ffmpeg, "-v", "error", "-y", "-i", str(mp4), "-vf", f"select=eq(n\\,{index})",
                                    "-fps_mode", "passthrough", "-frames:v", "1", str(target)], capture_output=True, text=True)
        if completed.returncode or not target.is_file():
            raise VideoError(f"ffmpeg could not extract frame {index}: {completed.stderr.strip()[-300:]}")
        entry = dict(label=label, frame_index=index, file=target.name)
        try:
            entry.update(image_statistics(target))
        except ImportError:
            entry["statistics"] = "Pillow unavailable"
        extracted.append(entry)
    return extracted


def contact_sheet(out_dir, extracted, target, columns=2, tile_width=640):
    from PIL import Image, ImageDraw
    tiles = []
    for entry in extracted:
        with Image.open(Path(out_dir) / entry["file"]) as image:
            tile = image.convert("RGB")
            tile = tile.resize((tile_width, max(1, round(tile.height * tile_width / tile.width))))
            ImageDraw.Draw(tile).text((8, 8), f"{entry['label']} n={entry['frame_index']}", fill=(255, 255, 0))
            tiles.append(tile)
    if not tiles:
        return None
    rows = (len(tiles) + columns - 1) // columns
    height = max(tile.height for tile in tiles)
    sheet = Image.new("RGB", (tile_width * min(columns, len(tiles)), height * rows))
    for i, tile in enumerate(tiles):
        sheet.paste(tile, ((i % columns) * tile_width, (i // columns) * height))
    sheet.save(target)
    return Path(target).name


def default_picks(frames, mapped):
    """Early, middle and late frames plus the first frame showing each contact, up to eight."""
    if not frames:
        return {}
    count = len(frames)
    picks = {"early": count // 10, "middle": count // 2, "late": (count * 9) // 10}
    for row in mapped:
        if row["marker"] in ("contact", "defense") and row.get("video_sample") is not None and len(picks) < 8:
            picks[f"{row['marker']}-{row.get('hit', row['index'])}"] = row["video_sample"]
    return picks


def analyze(bundle, extract=True, picks=None, thresholds=QUALITY_THRESHOLDS):
    """Write video-analysis.json (and review frames, a contact sheet and video-review.html) into the bundle."""
    clip = load_clip(bundle)
    mp4 = clip["directory"] / clip["window"]["output"]
    probed = probe(mp4)
    validation = validate_mp4(clip, probed)
    quality = grade(clip, validation, thresholds)
    summary, frames = join(clip, probed["pts"])
    mapped = map_markers(clip, frames)
    window = clip["window"]
    result = dict(
        schema_version=1, bundle=str(clip["bundle"]), video=str(mp4.relative_to(clip["bundle"]).as_posix()),
        capture_id=clip["link"]["video"].get("capture_id"), link_status=clip["link"].get("status"),
        # Who ended the clip and why: the session's stop request, or the recorder's own bound or error.
        link_stop=dict(stopped_by=clip["link"]["video"].get("stopped_by"), reason=clip["link"]["video"].get("stop_reason")),
        video_quality=quality["video_quality"], quality=quality, validation=validation,
        resolution=resolution_check(clip), join=summary, markers=mapped,
        recorder=dict(cadence=window.get("cadenceAssessment"), requests=window.get("requests"),
                      missed_target_slots=window.get("missedTargetSlots"), finalize_s=clip["manifest"].get("finalizeSeconds"),
                      capture_epoch_platform_seconds=clip["manifest"].get("captureEpochPlatformSeconds")),
        frames=frames)
    if extract and frames:
        review = clip["bundle"] / "video-review"
        extracted = extract_frames(mp4, picks if picks is not None else default_picks(frames, mapped), review)
        result["review_frames"] = dict(directory="video-review", frames=extracted,
                                       retention="Extracted PNGs are review copies; prune them after review and keep the MP4")
        try:
            result["review_frames"]["contact_sheet"] = contact_sheet(review, extracted, review / "contact-sheet.png")
        except ImportError:
            result["review_frames"]["contact_sheet"] = None
    (clip["bundle"] / "video-analysis.json").write_text(json.dumps(result, indent=1), encoding="utf-8")
    write_review_page(clip["bundle"], result)
    return result


def write_review_page(bundle, result):
    """A static page: the clip, its grade and the marker table with seek buttons."""
    def cell(value):
        return html.escape("" if value is None else str(value))
    rows = "".join(
        "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>".format(
            cell(m["marker"]), cell(m.get("outcome") or m.get("stage") or ""), cell(m["engine_frame"]), cell(m.get("video_sample")),
            f'<button onclick="seek({m["pts_s"]:.6f})">{m["pts_s"]:.3f} s</button>' if m.get("pts_s") is not None else "",
            cell(m.get("local_max_gap_ms")))
        for m in result["markers"])
    quality = result["quality"]
    reasons = "".join(f"<li>{cell(reason)}</li>" for reason in quality.get("reasons", [])) or "<li>within every threshold</li>"
    sheet = (result.get("review_frames") or {}).get("contact_sheet")
    page = f"""<!doctype html><meta charset="utf-8"><title>Combat capture video</title>
<style>body{{font:15px system-ui;background:#111820;color:#e8eef4;max-width:1300px;margin:24px auto;padding:0 16px}}video,img{{max-width:100%}}
table{{border-collapse:collapse;width:100%}}td,th{{text-align:left;padding:6px;border-bottom:1px solid #344657}}a{{color:#86d4ff}}button{{padding:2px 8px}}</style>
<h1>Combat capture video</h1>
<p>Clip quality: <b>{cell(result['video_quality'])}</b> · resolution: {cell(result['resolution']['note'])} {cell(result['resolution']['output'])}
· join: {cell(result['join']['exact_engine_frame_matches'])}/{cell(result['join']['video_frames'])} frames by engine frame</p>
<ul>{reasons}</ul>
<video id="clip" controls preload="auto" src="{cell(result['video'])}"></video>
<p><small>Seek buttons use the frame's PTS; browser seeking on variable-frame-rate video is approximate. The frame index in
video-analysis.json is exact. Extract a precise frame with ffmpeg.</small></p>
{f'<p><img src="video-review/{cell(sheet)}" alt="Contact sheet"></p>' if sheet else ''}
<h2>Markers</h2><table><tr><th>Marker</th><th>Outcome or stage</th><th>Engine frame</th><th>Video frame</th><th>PTS</th><th>Local max gap (ms)</th></tr>{rows}</table>
<p><a href="video-analysis.json">video-analysis.json</a> · <a href="capture-link.json">capture-link.json</a> · <a href="report.html">motion report</a></p>
<script>function seek(t){{const v=document.getElementById('clip');v.currentTime=t;v.pause();}}</script>
"""
    (Path(bundle) / "video-review.html").write_text(page, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("bundle", type=Path, help="Saved/CombatCaptures/<recording> with capture-link.json")
    parser.add_argument("--frames", help="Comma-separated video frame indices to extract instead of the defaults")
    parser.add_argument("--no-extract", action="store_true", help="Skip PNG extraction and the contact sheet")
    args = parser.parse_args()
    picks = None
    if args.frames:
        picks = {f"frame{i}": int(value) for i, value in enumerate(args.frames.split(","))}
    try:
        result = analyze(args.bundle, extract=not args.no_extract, picks=picks)
    except VideoError as error:
        parser.exit(2, f"{error}\n")
    print(json.dumps(dict(video_quality=result["video_quality"], reasons=result["quality"]["reasons"],
                          resolution=result["resolution"], join=result["join"], validation_issues=result["validation"]["issues"],
                          review=str(Path(args.bundle) / "video-review.html")), indent=1))
    return 0 if result["video_quality"] == "ok" else 1


if __name__ == "__main__":
    raise SystemExit(main())
