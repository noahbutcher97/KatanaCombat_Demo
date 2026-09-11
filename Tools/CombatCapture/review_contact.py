"""Build a portable raw-pixel / projected-contact review from retained capture frames.

Uses only the standard library. This is a review aid, not a visible-contact verdict:
projection does not establish occlusion, mesh-surface intersection or artistic intent.
"""
import argparse
import base64
import hashlib
import json
import math
from pathlib import Path


from visual_analysis import linked_actor, project, segment_gap


def read_lines(path):
    return [json.loads(line) for line in path.read_text(encoding="utf-8-sig").splitlines() if line.strip()]


def build(args):
    if args.output.suffix.lower() != ".html":
        raise ValueError("Review output must be an HTML path")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("<!doctype html><title>Contact review pending</title><p>Review incomplete. Do not reuse a previous verdict.</p>", encoding="utf-8")
    args.output.with_suffix(".json").write_text(json.dumps({"status": "inconclusive", "reason": "Review generation incomplete"}), encoding="utf-8")
    capture = args.capture.resolve()
    samples = {s["index"]: s for s in read_lines(capture / "samples.jsonl")}
    candidates = []
    for frame in read_lines(capture / "frames.jsonl"):
        sample = samples[frame["sample_index"]]
        attacker = next((a for a in sample["actors"] if a["role"] == args.actor_role), None)
        montages = [m for m in attacker.get("montages", []) if args.montage in m["asset"] and m["weight"] > 0] if attacker else []
        if len(montages) != 1 or not args.start <= montages[0]["position_s"] <= args.end:
            continue
        candidates.append((montages[0]["position_s"], frame, sample))
    if not candidates:
        raise ValueError("No frames in the requested contributing montage interval")
    count = min(args.max_frames, len(candidates))
    selected = sorted({round(i * (len(candidates)-1) / max(1, count-1)) for i in range(count)})
    records = []
    for index in selected:
        time, frame, sample = candidates[index]
        attacker = linked_actor(sample, frame, args.actor_role)
        victim = linked_actor(sample, frame, args.target_role)
        def point(actor, name):
            return actor["points"][name]["world_cm"]
        start, end = point(attacker, args.segment_start), point(attacker, args.segment_end)
        target = point(victim, args.target_point)
        markers = [{"name": name, "px": project(point(victim, name), frame)}
                   for name in dict.fromkeys([args.target_point, *args.observe_point])]
        circles = []
        for axis_a, axis_b in ((0, 1), (0, 2), (1, 2)):
            circle = []
            for step in range(65):
                p = list(target)
                p[axis_a] += args.radius_cm * math.cos(step * math.tau / 64)
                p[axis_b] += args.radius_cm * math.sin(step * math.tau / 64)
                circle.append(project(p, frame))
            circles.append(circle)
        path = (capture / frame["file"]).resolve()
        if not path.is_relative_to(capture):
            raise ValueError("Frame path escapes the capture")
        data = path.read_bytes()
        records.append(dict(file=frame["file"], image_sha256=hashlib.sha256(data).hexdigest(),
                            image="data:image/png;base64," + base64.b64encode(data).decode("ascii"),
                            montage_time_s=time, sample_index=sample["index"],
                            sample_time_lag_s=frame["sample_time_lag_s"],
                            pose_links=frame["pose_links"], projection_source=frame["projection_source"],
                            width=frame["width"], height=frame["height"],
                            segment=[project(start, frame), project(end, frame)], markers=markers,
                            sphere_circles=circles, signed_gap_cm=segment_gap(start, end, target, args.radius_cm)))
    report = dict(status="review_required", capture=str(capture), montage=args.montage,
                  requested_interval_s=[args.start, args.end], available_frames=len(candidates),
                  target_point=args.target_point, radius_cm=args.radius_cm,
                  limits="Selected frames only. Projection is reconstructed by LocalPlayer after draw, without renderer temporal jitter or a depth/occlusion mask. Pose linkage is checked; it does not prove exact skinned-surface contact or artistic intent. A projected overlap is not a 3D contact verdict.",
                  frames=records,
                  input_hashes={name: hashlib.sha256((capture / name).read_bytes()).hexdigest()
                                for name in ("frames.jsonl", "samples.jsonl", "session.json", "scenario.json")})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(report).replace("<", "\\u003c")
    args.output.write_text(HTML.replace("__DATA__", payload), encoding="utf-8")
    for record in records:
        record.pop("image")
    args.output.with_suffix(".json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"Contact review: {args.output} ({len(records)} embedded frames; review required)")


HTML = '''<!doctype html><meta charset="utf-8"><title>Visual contact review</title>
<style>body{font:16px system-ui;background:#171a20;color:#eee;margin:24px}main{display:flex;gap:16px}section{width:50%}canvas{width:100%;background:#000}input{width:75%}p{max-width:1100px}label{margin-right:24px}</style>
<h1>Visual contact review</h1><p id="limits"></p><p id="identity"></p>
<input id="frame" type="range" min="0" step="1"><span id="time"></span>
<main><section><h2>Raw pixels</h2><canvas id="raw"></canvas></section><section><h2>Projected observations</h2><canvas id="overlay"></canvas></section></main>
<p id="detail"></p><p>Yellow: weapon segment. Magenta: declared target sphere. White: additional anatomical probes. Review raw pixels first; the overlay can be visible through occluding geometry.</p>
<script id="visual-evidence" type="application/json">__DATA__</script>
<script>const data=JSON.parse(document.getElementById("visual-evidence").textContent);const slider=document.getElementById('frame');slider.max=data.frames.length-1;slider.value=Math.min(data.frames.length-1,Math.max(0,Number(new URLSearchParams(location.hash.slice(1)).get("frame"))||0));
document.getElementById('limits').textContent=data.limits;document.getElementById('identity').textContent=data.capture+' | '+data.montage;
let request=0;function draw(){const serial=++request;const f=data.frames[Number(slider.value)];const img=new Image();img.onload=()=>{if(serial!==request)return;
for(const id of ['raw','overlay']){const c=document.getElementById(id);c.width=f.width;c.height=f.height;c.getContext('2d').drawImage(img,0,0)}
const ctx=document.getElementById('overlay').getContext('2d');function line(points,color){ctx.strokeStyle=color;ctx.lineWidth=2;ctx.beginPath();points.forEach((p,i)=>i?ctx.lineTo(...p):ctx.moveTo(...p));ctx.stroke()}
line(f.segment,'#ffd400');f.sphere_circles.forEach(c=>line(c,'#ff48d7'));ctx.font='14px system-ui';
f.markers.forEach((p,i)=>{ctx.fillStyle=i?'white':'#ff48d7';ctx.beginPath();ctx.arc(...p.px,3,0,2*Math.PI);ctx.fill();ctx.fillText(p.name,p.px[0]+8,p.px[1]-4)});
document.getElementById('time').textContent=f.montage_time_s.toFixed(3)+' s';document.getElementById('detail').textContent=f.file+' | 3D proxy gap '+f.signed_gap_cm.toFixed(2)+' cm | frame/sample lag '+f.sample_time_lag_s.toFixed(5)+' s | selected '+(Number(slider.value)+1)+'/'+data.frames.length;
};img.src=f.image;}slider.oninput=draw;draw();</script>'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--montage", required=True, help="Unique contributing montage path or substring")
    parser.add_argument("--start", type=float, required=True)
    parser.add_argument("--end", type=float, required=True)
    parser.add_argument("--actor-role", default="Attacker")
    parser.add_argument("--target-role", default="Victim")
    parser.add_argument("--segment-start", default="weapon_start")
    parser.add_argument("--segment-end", default="weapon_end")
    parser.add_argument("--target-point", required=True)
    parser.add_argument("--radius-cm", type=float, required=True)
    parser.add_argument("--observe-point", action="append", default=[])
    parser.add_argument("--max-frames", type=int, default=12)
    args = parser.parse_args()
    if not all(math.isfinite(x) for x in (args.start, args.end, args.radius_cm)) or args.end <= args.start or args.radius_cm < 0 or not 2 <= args.max_frames <= 60:
        parser.error("Use a finite increasing interval, nonnegative radius and 2–60 review frames")
    try:
        build(args)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f"Contact review unavailable: {error}\n")


if __name__ == "__main__":
    main()
