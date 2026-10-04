"""Analyze a versioned combat capture without Unreal, network access, or third-party packages."""
from __future__ import annotations

import argparse
import csv
import html
import json
import math
from pathlib import Path

import visual_analysis  # Repository bootstrap; portable modules never modify sys.path.
from animation_analysis.errors import EvidenceError as CaptureError
from animation_analysis.integrity import read_json, read_lines, bundle_path, number, vector, png_dimensions
from animation_analysis.artifacts import atomic_text as replace_text, file_manifest
from animation_analysis.metrics import distribution, numeric_deltas
from capture_format import implementation_identity, input_names


def analyze(root: Path, distances=()):
    manifest = read_json(root / "session.json")
    if manifest.get("schema_version") not in (1, 2):
        raise CaptureError("Unsupported session schema; expected version 1 or 2")
    samples, frames, markers = (read_lines(root / f"{name}.jsonl") for name in ("samples", "frames", "markers"))
    issues = list(manifest.get("errors", []))
    readback_mode = manifest.get("readback_mode", "synchronous")
    if readback_mode not in ("synchronous", "asynchronous"):
        raise CaptureError("Unsupported image readback mode")
    diagnostic_resolution = manifest.get("readback_diagnostic_resolution", False)
    if type(diagnostic_resolution) is not bool or (diagnostic_resolution and readback_mode != "asynchronous"):
        raise CaptureError("Invalid async diagnostic-resolution policy")
    notes = ["Measurements describe sampled motion; they do not score feel, foot support, occlusion, or collision correctness.",
             "Capture adds overhead. Wall-time measurements are not performance benchmarks."]
    if readback_mode == "asynchronous":
        notes.append("Async frames retain acquisition time separately from completion. Readback API time measures enqueue cost, not completion latency. This motion analysis does not validate the terminal readback stream.")
    if manifest.get("status") != "complete":
        issues.append(f"Session is {manifest.get('status')!r}, not finalized successfully")
    for name, rows in (("sample", samples), ("frame", frames), ("marker", markers)):
        if manifest.get(f"{name}_count") != len(rows):
            issues.append(f"{name} count disagrees with manifest")
        previous = -math.inf
        for index, row in enumerate(rows, 1):
            timestamp = number(row["simulation_time_s"])
            number(row["wall_elapsed_s"])
            if timestamp < previous or (name != "marker" and timestamp == previous):
                issues.append(f"Non-increasing {name} timestamps at {index}")
            if row.get("index") != index:
                issues.append(f"Non-contiguous {name} index at {index}")
            previous = timestamp
    if len(samples) < 2:
        issues.append("Fewer than two motion samples")
    frame_hz, sample_hz = number(manifest["frame_hz"]), number(manifest["sample_hz"])
    if sample_hz <= 0 or frame_hz < 0:
        raise CaptureError("Invalid sampling configuration")
    if frame_hz == 0:
        notes.append("Images were disabled for this session.")
    elif not manifest.get("render_available"):
        notes.append("Headless capture: visual evidence is unavailable.")
    elif not frames:
        issues.append("Rendering requested and available, but no frames captured")
    previous_draw = 0
    resolutions = set()
    for frame in frames:
        if frame.get("source") != "PIEGameViewportAfterDraw" or frame.get("capture_world") != manifest.get("world"):
            issues.append(f"Wrong viewport/world provenance in frame {frame['index']}")
        if number(frame["pie_draw_index"]) <= previous_draw:
            issues.append("Non-increasing PIE draw indices")
        previous_draw = frame["pie_draw_index"]
        sample_index = frame.get("sample_index", 0)
        if not isinstance(sample_index, int) or not 1 <= sample_index <= len(samples):
            issues.append(f"Frame {frame['index']} has no preceding motion sample")
        elif samples[sample_index - 1]["simulation_time_s"] > frame["simulation_time_s"]:
            issues.append(f"Frame {frame['index']} links to a future motion sample")
        try:
            size = png_dimensions(bundle_path(root, frame["file"]))
            if size != (frame["width"], frame["height"]):
                issues.append(f"Image dimensions disagree in frame {frame['index']}")
            resolutions.add(size)
        except (OSError, ValueError) as error:
            issues.append(str(error))
        if not frame.get("nontrivial_pixels"):
            issues.append(f"Uniform/empty-looking image at frame {frame['index']}")
    if len(resolutions) > 1:
        notes.append("Viewport resolution changed during capture.")
    warming_frames = sum(frame.get("render_resources_ready") is False for frame in frames)
    if warming_frames:
        notes.append(f"{warming_frames} frames were captured while asset/shader compilation was pending; inspect their materials before visual comparison.")
    frame_coverage = {"first_frame_delay_s": None, "last_frame_to_last_sample_s": None}
    if frames and samples:
        frame_coverage = {"first_frame_delay_s": max(0, frames[0]["simulation_time_s"] - samples[0]["simulation_time_s"]),
                          "last_frame_to_last_sample_s": max(0, samples[-1]["simulation_time_s"] - frames[-1]["simulation_time_s"])}
        if frame_hz > 0 and max(frame_coverage.values()) > 2 / frame_hz:
            notes.append("Rendered frames do not cover the full motion interval; inspect first/last frame coverage gaps.")

    def nearest_frame(time):
        if not frames:
            return None
        frame = min(frames, key=lambda f: abs(f["simulation_time_s"] - time))
        return {"file": frame["file"], "offset_s": frame["simulation_time_s"] - time}

    roles = {row["role"]: row for row in manifest["participants"]}
    if len(roles) != len(manifest["participants"]):
        raise CaptureError("Duplicate participant roles")
    tracks = {role: [] for role in roles}
    for row in samples:
        seen = set()
        for actor in row["actors"]:
            role = actor["role"]
            if role not in roles or role in seen:
                raise CaptureError("Unknown or duplicate sample role")
            seen.add(role)
            if actor.get("valid"):
                vector(actor["position_cm"])
                for point in actor["points"].values():
                    if point is not None:
                        vector(point["world_cm"])
                        vector(point["component_cm"])
            tracks[role].append((row["simulation_time_s"], actor))
        if seen != roles.keys():
            issues.append(f"Missing participant in sample {row['index']}")

    metrics = {}
    for role, track in tracks.items():
        steps, speeds, missing = [], [], 0
        point_steps = {name: [] for name in roles[role]["points"]}
        missing_points = {name: 0 for name in point_steps}
        previous = None
        largest_time, largest_step = None, -1
        not_rendered = 0
        for time, actor in track:
            if not actor.get("valid"):
                missing += 1
                previous = None
                continue
            not_rendered += not actor.get("recently_rendered", False)
            for name in point_steps:
                missing_points[name] += actor.get("points", {}).get(name) is None
            if previous and time > previous[0]:
                old_time, old = previous
                step = math.dist(actor["position_cm"], old["position_cm"])
                steps.append(step)
                speeds.append(step / (time - old_time))
                if step > largest_step:
                    largest_time, largest_step = time, step
                for name in point_steps:
                    a, b = actor["points"].get(name), old["points"].get(name)
                    if a is not None and b is not None:
                        point_steps[name].append(math.dist(a["component_cm"], b["component_cm"]))
            previous = time, actor
        metrics[role] = {"actor_step_cm": distribution(steps), "actor_speed_cm_s": distribution(speeds),
                         "sampled_path_length_cm": sum(steps), "missing_actor_samples": missing,
                         "not_recently_rendered_samples": not_rendered,
                         "point_component_step_cm": {name: distribution(values) for name, values in point_steps.items()},
                         "missing_point_samples": missing_points,
                         "largest_actor_step_frame": nearest_frame(largest_time) if largest_time is not None else None}
        if missing or any(missing_points.values()):
            notes.append(f"{role}: missing actor/point observations; inspect coverage before interpreting metrics.")
        if not_rendered:
            notes.append(f"{role}: {not_rendered} samples were not recently rendered; visibility-based animation ticking may leave stale poses.")
        if roles[role].get("telemetry_lost_records", 0) or roles[role].get("telemetry_resets", 0):
            issues.append(f"{role}: telemetry has loss or resets; timing correlations may be incomplete")

    distance_results = {}
    for first, second in distances:
        try:
            first_role, first_point = first.split(":", 1)
            second_role, second_point = second.split(":", 1)
            if first_point not in roles[first_role]["points"] or second_point not in roles[second_role]["points"]:
                raise KeyError("point")
        except (ValueError, KeyError) as error:
            raise CaptureError(f"Unknown point pair {first} {second}; use Role:Point from session.json") from error
        values = []
        for row in samples:
            actors = {actor["role"]: actor for actor in row["actors"] if actor.get("valid")}
            a = actors.get(first_role, {}).get("points", {}).get(first_point)
            b = actors.get(second_role, {}).get("points", {}).get(second_point)
            if a is not None and b is not None:
                values.append((math.dist(a["world_cm"], b["world_cm"]), row["simulation_time_s"]))
        closest = min(values) if values else None
        distance_results[f"{first} -> {second}"] = {"distance_cm": distribution([x[0] for x in values]),
            "missing_samples": len(samples) - len(values), "closest_time_s": closest[1] if closest else None,
            "closest_frame": nearest_frame(closest[1]) if closest else None}

    latencies = {}
    for role, participant in roles.items():
        path = bundle_path(root, role + ".actions.csv")
        try:
            with path.open(encoding="utf-8-sig", newline="") as stream:
                rows = list(csv.DictReader(stream))
        except OSError as error:
            issues.append(str(error))
            rows = []
        if len(rows) != participant.get("action_records"):
            issues.append(f"{role}: action CSV count mismatch")
        # Correlate within each component/role. A zero serial is not a correlation.
        pending, delays, unmatched, consumed = {}, [], 0, set()
        for row in rows:
            if row.get("schema_version") not in ("1", "2"):
                raise CaptureError("Unsupported action telemetry schema")
            serial = row.get("input_serial", "0")
            time = number(float(row["simulation_timestamp"]))
            if row["event"] == "InputCaptured" and serial != "0":
                pending[serial] = time
            elif row["event"] == "ActionExecutionStarted":
                if serial != "0" and serial in pending and serial not in consumed and time >= pending[serial]:
                    delays.append((time - pending[serial]) * 1000)
                    consumed.add(serial)
                else:
                    unmatched += 1
        if participant.get("telemetry_resets", 0) or participant.get("telemetry_lost_records", 0):
            delays = []
        latencies[role] = {"input_to_action_ms": distribution(delays), "unmatched_action_starts": unmatched,
                           "inputs_without_correlated_action": len(pending.keys() - consumed)}
        if not delays:
            notes.append(f"{role}: no correlated input/action latency measurement (absence is not zero latency).")
        defense_path = bundle_path(root, role + ".defense.csv")
        try:
            with defense_path.open(encoding="utf-8-sig", newline="") as stream:
                defense_count = sum(1 for _ in csv.DictReader(stream))
            if defense_count != participant.get("defense_records"):
                issues.append(f"{role}: defense CSV count mismatch")
        except OSError as error:
            issues.append(str(error))

    gaps = [b["simulation_time_s"] - a["simulation_time_s"] for a, b in zip(samples, samples[1:])]
    frame_gaps = [b["simulation_time_s"] - a["simulation_time_s"] for a, b in zip(frames, frames[1:])]
    if gaps and max(gaps) > 2 / sample_hz:
        notes.append("Motion sampling contains gaps exceeding twice the requested interval; no missing poses were interpolated.")
    result = {"analysis_schema_version": 1, "scenario": manifest["scenario"], "map": manifest["map"],
              "data_integrity": "valid" if not issues else "incomplete", "issues": sorted(set(issues)), "notes": notes,
              "sample_count": len(samples), "frame_count": len(frames),
              "sample_gap_s": distribution(gaps), "frame_gap_s": distribution(frame_gaps),
              "frame_coverage": frame_coverage,
              "motion": metrics, "point_distances": distance_results, "input_timing": latencies,
              "compatibility": {"scenario": manifest["scenario"], "map": manifest["map"],
                  "sample_hz": sample_hz, "frame_hz": frame_hz, "render_available": manifest["render_available"],
                  "readback_mode": readback_mode, "readback_diagnostic_resolution": diagnostic_resolution,
                  "participants": sorted((role, sorted(data["points"])) for role, data in roles.items()),
                  "resolutions": sorted(resolutions), "distances": list(distances)}}
    return result, frames, markers


def compare(current, baseline):
    if current["data_integrity"] != "valid" or baseline["data_integrity"] != "valid":
        raise CaptureError("Baseline comparison requires intact, finalized captures")
    if current["compatibility"] != baseline["compatibility"]:
        raise CaptureError("Baseline configuration is incompatible (map/scenario/participants/cadence/rendering/points)")
    changes = {}
    for category in ("motion", "point_distances", "input_timing", "sample_gap_s", "frame_gap_s"):
        changes.update(numeric_deltas(current[category], baseline[category], category))
    return {"interpretation": "Metric deltas; no quality thresholds or universal pass/fail are implied.", "metrics": changes}


def invalidate_report(root, reason):
    replace_text(root / "analysis.json", json.dumps(dict(analysis_state="inconclusive", reason=reason)))
    replace_text(root / "report.html", "<!doctype html><meta charset=utf-8><p>No current analysis: " + html.escape(reason) + "</p>")


def write_report(root, report, frames, markers):
    report["analysis_state"] = "complete"
    report["analyzer_identity"] = implementation_identity(__file__)
    report["input_files"] = file_manifest(root, input_names(root))
    # JSON is inert data, but escape '<' so user markers cannot terminate the script element.
    payload = json.dumps({"frames": frames, "markers": markers}).replace("<", "\\u003c")
    def display(value):
        return "unavailable" if value is None else f"{value:.3f}"

    def frame_link(frame):
        return ('<a href="' + html.escape(frame["file"], quote=True) + '">View frame</a> (offset '
                + display(frame["offset_s"]) + ' s)') if frame else 'No frame'

    motion_rows = ''.join('<tr><td>' + html.escape(role) + '</td><td>' + display(data['actor_step_cm']['max'])
                          + '</td><td>' + display(data['actor_speed_cm_s']['max']) + '</td><td>'
                          + str(data['missing_actor_samples']) + '</td><td>' + frame_link(data['largest_actor_step_frame']) + '</td></tr>'
                          for role, data in report['motion'].items())
    distance_rows = ''.join('<tr><td>' + html.escape(pair) + '</td><td>' + display(data['distance_cm']['min'])
                            + '</td><td>' + str(data['missing_samples']) + '</td><td>' + frame_link(data['closest_frame']) + '</td></tr>'
                            for pair, data in report['point_distances'].items())
    tables = '<h2>Motion observations</h2><table><tr><th>Participant</th><th>Largest sampled step (cm)</th><th>Max sampled speed (cm/s)</th><th>Missing samples</th><th>Nearest frame</th></tr>' + motion_rows + '</table>'
    if distance_rows:
        tables += '<h2>Configured point distances</h2><table><tr><th>Pair</th><th>Closest sampled distance (cm)</th><th>Missing samples</th><th>Nearest frame</th></tr>' + distance_rows + '</table>'
    page = """<!doctype html><meta charset="utf-8"><title>Combat capture analysis</title>
<style>body{font:16px system-ui;background:#111820;color:#e8eef4;max-width:1100px;margin:32px auto;padding:0 20px}a{color:#86d4ff}img{max-width:100%;max-height:65vh}pre{white-space:pre-wrap;background:#1a2531;padding:16px}input{width:100%}button{padding:8px 18px}small{color:#aebfcd}table{border-collapse:collapse;width:100%}td,th{text-align:left;padding:10px;border-bottom:1px solid #344657}</style>
<h1>Combat capture analysis</h1><p>SCENARIO</p><p>STATUS</p>
<p><a href="session.json">Session metadata</a> Â· <a href="analysis.json">Analysis JSON</a> Â· <a href="samples.jsonl">Motion samples</a></p>
VIDEOLINK<div id="viewer"><button id="play">Play sampled frames</button><input id="scrub" type="range" min="0" value="0"><p id="caption"></p><img id="frame" alt="Captured PIE viewport"></div>
<p><small>Playback follows captured simulation timestamps. Sparse samples cannot show motion between frames.</small></p>
TABLES
<h2>Measurements and evidence limits</h2><pre>REPORT</pre>
<script type="application/json" id="data">PAYLOAD</script>
<script>const data=JSON.parse(document.getElementById('data').textContent),frames=data.frames;
const scrub=document.getElementById('scrub'),picture=document.getElementById('frame'),caption=document.getElementById('caption');
let timer=null; scrub.max=Math.max(0,frames.length-1);
function show(){if(!frames.length){document.getElementById('viewer').textContent='No rendered frames in this recording.';return;}
const f=frames[Number(scrub.value)];picture.src=f.file;caption.textContent=`Frame ${f.index} Â· simulation ${f.simulation_time_s.toFixed(3)} s Â· ${f.marker} Â· ${f.width}Ã—${f.height}`;}
scrub.oninput=()=>{clearTimeout(timer);timer=null;show();};
function advance(){const i=Number(scrub.value);if(i>=frames.length-1){timer=null;return;}
timer=setTimeout(()=>{scrub.value=i+1;show();advance();},Math.max(1,(frames[i+1].simulation_time_s-frames[i].simulation_time_s)*1000));}
document.getElementById('play').onclick=()=>{if(timer){clearTimeout(timer);timer=null;}else{if(Number(scrub.value)>=frames.length-1)scrub.value=0;show();advance();}};show();</script>"""
    video_link = ('<p><a href="video-review.html">Video review</a>: the clip recorded with this capture, joined by '
                  '<a href="capture-link.json">capture-link.json</a>. Run video_capture.py if the page is missing.</p>\n'
                  if (root / "capture-link.json").is_file() else "")
    page = page.replace("VIDEOLINK", video_link)
    page = page.replace("SCENARIO", html.escape(report["scenario"] + " â€” " + report["map"]))
    page = page.replace("STATUS", html.escape("Data integrity: " + report["data_integrity"] + ". This is not an animation-quality verdict."))
    page = page.replace("TABLES", tables).replace("REPORT", html.escape(json.dumps(report, indent=2))).replace("PAYLOAD", payload)
    replace_text(root / "report.html", page)
    replace_text(root / "analysis.json", json.dumps(report, indent=2, allow_nan=False) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--baseline", type=Path, help="Compatible capture directory to compare")
    parser.add_argument("--distance", nargs=2, action="append", default=[], metavar=("ROLE:POINT", "ROLE:POINT"))
    args = parser.parse_args()
    try:
        invalidate_report(args.capture, "Analysis in progress")
        result, frames, markers = analyze(args.capture, args.distance)
        if args.baseline:
            baseline, _, _ = analyze(args.baseline, args.distance)
            result["comparison"] = compare(result, baseline)
        write_report(args.capture, result, frames, markers)
        print(f"{result['data_integrity']}: {args.capture / 'report.html'}")
        return 0 if result["data_integrity"] == "valid" else 1
    except (CaptureError, KeyError, TypeError, OSError, ValueError) as error:
        if args.capture.is_dir():
            invalidate_report(args.capture, str(error))
        print(f"Capture analysis failed: {error}")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
