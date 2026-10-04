"""Build, run, capture and evaluate a tracked combat scenario with one command."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import hashlib
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import uuid

from analyze_capture import CaptureError, analyze, read_json, write_report
from animation_analysis.artifacts import atomic_json, digest, identity, file_manifest
from capture_format import implementation_identity
from evaluate_capture import evaluate_and_write
from scenario_placement import resolve_placement
from animation_analysis_dependency import SOURCE_SUFFIXES, dependency_source_manifest
from video_capture import RESOLUTION_BOXES, VideoError, analyze as analyze_video

REPO = Path(__file__).resolve().parents[2]
TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(REPO / "Tools/PresentationCapture"))
from presentation_capture_dependency import dependency_source_manifest as recorder_source_manifest  # noqa: E402


def source_state(repo=REPO):
    directories = ("Source", "Config", "Tools/CombatCapture", "Tools/AnimationAnalysis", "Plugins/AnimationAnalysis",
                   "Tools/PresentationCapture", "Tools/PluginDependencies", "Plugins/PresentationCapture")
    paths = [p for directory in directories for p in (repo / directory).rglob("*")
             if p.is_file() and p.suffix in SOURCE_SUFFIXES
             and not any(part in ("Binaries", "Intermediate", "Saved") for part in p.relative_to(repo).parts)]
    paths += list(repo.glob("*.uproject"))
    files = file_manifest(repo, (p.relative_to(repo) for p in paths))
    files.update(dependency_source_manifest(repo))
    files.update(recorder_source_manifest(repo))
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip()
    return dict(revision=revision, files=files, identity=identity(files))


def editor_binary_state(repo=REPO):
    """Capture project and independent native producer binaries by relative path."""
    paths = list((repo / "Binaries/Win64").glob("UnrealEditor-KatanaCombat*.dll"))
    paths += list((repo / "Plugins/AnimationAnalysis/Binaries/Win64").glob("UnrealEditor-AnimationCapture.dll"))
    recorder = repo / "Plugins/PresentationCapture/Binaries/Win64"
    paths += list(recorder.glob("UnrealEditor-PresentationCapture.dll")) + list(recorder.glob("PresentationCapture*.exe"))
    return file_manifest(repo, (p.relative_to(repo) for p in paths))


def content_snapshot():
    return {p.relative_to(REPO).as_posix(): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in (REPO / "Content").rglob("*") if p.is_file()}


def asset_state(scenario, before):
    paths, missing = set(), []
    for package in scenario["project_package_dependencies"]:
        if not package.startswith("/Game/") or ".." in package.split("/"):
            raise CaptureError("Invalid project package identity")
        base = REPO / "Content" / package[len("/Game/"):]
        matches = [Path(str(base) + ext) for ext in (".uasset", ".umap") if Path(str(base) + ext).is_file()]
        if not matches:
            # Stale soft references are meaningful identity information, not hashed as present.
            missing.append(package)
        paths.update(matches)
    map_package = scenario["definition"]["maps"][scenario["map_key"]]
    external = REPO / "Content/__ExternalActors__" / map_package[len("/Game/"):]
    paths.update(external.rglob("*.uasset"))
    files, files_sha1 = {}, {}
    for path in sorted(paths):
        name = path.relative_to(REPO).as_posix()
        if before.get(name) != (path.stat().st_size, path.stat().st_mtime_ns):
            raise CaptureError(f"Asset changed during execution: {name}")
        files[name] = digest(path)
        files_sha1[name] = hashlib.sha1(path.read_bytes()).hexdigest()
    return dict(scope="AssetRegistry transitive Game package dependencies plus selected map external actors; engine identity recorded separately",
                files=files, files_sha1=files_sha1, absent_soft_dependencies=sorted(missing), identity=identity(files))


def checked_process(command, log, timeout, env=None):
    with log.open("w", encoding="utf-8") as stream:
        try:
            process = subprocess.Popen(command, cwd=REPO, stdout=stream, stderr=subprocess.STDOUT, env=env,
                                       creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            try:
                return process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=15)
                raise CaptureError(f"Process timed out after {timeout}s; see {log}")
        except OSError as error:
            raise CaptureError(f"Could not launch process: {error}") from error


def scenario_outputs(text):
    outputs = {value.strip() for value in re.findall(r"COMBAT_SCENARIO_OUTPUT=([^\r\n]+)", text)}
    return {re.sub(r"\s+\[log\]\s*$", "", value) for value in outputs}


def rhi_identity(text):
    """The RHI the editor actually used, read from its own log; --rhi is only a request."""
    used = re.findall(r"LogRHI: Using (Default|Forced) RHI: (\S+)", text)
    level = re.findall(r"LogRHI: Using Highest Feature Level of \S+: (\S+)", text)
    return dict(rhi=used[-1][1] if used else None, selection=used[-1][0].lower() if used else None,
                feature_level=level[-1] if level else None)


def automation_succeeded(text, exit_code, scope):
    # ;Quit may exit before the command-line shutdown banner is flushed. The
    # exact requested result plus process exit and fresh artifact identity are
    # decisive; a different test, duplicate result or failed exit cannot pass.
    results = re.findall(r"Test Completed\. Result=\{([^}]+)\}.*Path=\{" + re.escape(scope) + r"\}", text)
    return exit_code == 0 and results == ["Success"]


def run_one(args, batch_dir, source, map_key, variant, mode, iteration):
    run_id = uuid.uuid4().hex
    run_dir = batch_dir / f"{map_key}-{variant}-{mode}-{iteration}-{run_id[:8]}"
    run_dir.mkdir()
    result = dict(run_id=run_id, status="inconclusive", map_key=map_key, variant=variant, mode=mode,
                  camera_view=args.camera_view,
                  iteration=iteration, reason="Run in progress", run_directory=str(run_dir),
                  render_backend="rendered" if mode == "rendered" or args.render_world else "NullRHI",
                  rhi=args.rhi, video=args.video_request)
    atomic_json(run_dir / "run.json", result)
    started = time.monotonic()
    try:
        if source_state()["identity"] != source["identity"]:
            raise CaptureError("Source/config/scenario/evaluator changed since this batch started")
        context = dict(run_id=run_id, source_identity=source["identity"], source=source,
                       scenario_hash=digest(args.scenario), evaluator_identity=implementation_identity(TOOLS / "evaluate_capture.py"),
                       engine_build=read_json(args.engine / "Engine/Build/Build.version"),
                       editor_binaries=editor_binary_state(),
                       build_this_batch=not args.skip_build, declared_changes=args.declare_change,
                       runtime_experiment=args.finisher_experiment, warp_tuning=args.warp_tuning, camera_view=args.camera_view,
                       placement=args.placement, rhi=args.rhi, video=args.video_request,
                       scenario_path=args.scenario.relative_to(REPO).as_posix())
        atomic_json(run_dir / "run-context.json", context)
        before = content_snapshot()
        log = run_dir / "automation.log"
        scope = f"KatanaCombat.Capture.Scenarios.{args.definition['scenario']}.{map_key}.{variant}"
        command = [str(args.engine / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"), str(REPO / "KatanaCombat.uproject"),
                   f"-ExecCmds=Automation RunTests {scope};Quit", "-unattended", "-nopause", "-nosplash", "-stdout",
                   "-FullStdOutLogOutput", "-DDC=InstalledNoZenLocalFallback", f"-abslog={log}",
                   f"-ShaderWorkingDir={batch_dir / 'shaders'}", f"-CombatCaptureMode={mode}",
                   f"-CombatCaptureRunContext={run_dir / 'run-context.json'}", f"-CombatCaptureControlOffset={args.control_offset_cm}"]
        command.append(f"-CombatFinisherExperiment={args.finisher_experiment}")
        rendered = mode == "rendered" or args.render_world
        size = RESOLUTION_BOXES[args.video_resolution] if args.video else (960, 540)
        command += ["-RenderOffScreen", "-windowed", f"-ResX={size[0]}", f"-ResY={size[1]}"] if rendered else ["-NullRHI"]
        if rendered and args.rhi != "default":
            command.append("-dx12" if args.rhi == "dx12" else "-d3d11")
        if args.video:
            command += ["-CombatCaptureVideo=1", f"-CombatCaptureVideoFPS={args.video_fps}",
                        f"-CombatCaptureVideoResolution={args.video_resolution}", f"-CombatCaptureVideoSeconds={args.video_seconds}"]
        env = dict(os.environ)
        env["UE-LocalDataCachePath"] = str(REPO / "Saved/CombatCaptureCache")
        atomic_json(run_dir / "command.json", command)
        exit_code = checked_process(command, run_dir / "stdout.log", args.timeout, env)
        result["process_exit_code"] = exit_code
        text = log.read_text(encoding="utf-8-sig", errors="replace")
        result["rhi_used"] = rhi_identity(text)
        outputs = scenario_outputs(text)
        if len(outputs) != 1:
            raise CaptureError(f"Expected one fresh scenario artifact, got {len(outputs)}")
        capture = Path(outputs.pop().strip()).resolve()
        if not capture.is_relative_to((REPO / "Saved/CombatCaptures").resolve()):
            raise CaptureError("Scenario output is outside the capture directory")
        scenario = read_json(capture / "scenario.json")
        if scenario["run_id"] != run_id:
            raise CaptureError("Scenario returned an artifact from a different run")
        if scenario.get("runtime_experiment", "none") != args.finisher_experiment:
            raise CaptureError("Runtime experiment did not match the requested control")
        if scenario.get("camera_view", "default") != args.camera_view:
            raise CaptureError("Captured camera view did not match the request; rebuild the editor")
        result["capture"] = str(capture)
        if source_state()["identity"] != source["identity"]:
            raise CaptureError("Source/config changed during the run")
        assets = asset_state(scenario, before)
        atomic_json(capture / "asset-identity.json", assets)
        atomic_json(capture / "run-context.json", context)
        if mode != "disabled":
            report, frames, markers = analyze(capture)
            write_report(capture, report, frames, markers)
        reference_path = args.references / f"{map_key}-{variant}.json" if args.references else None
        reference = read_json(reference_path) if reference_path else None
        evaluation = evaluate_and_write(capture, reference)
        result.update(status=evaluation["status"], reason="Scenario and evaluator results recorded",
                      evaluation=str(capture / "evaluation.html"), wall_duration_s=scenario["wall_duration_s"],
                      simulation_duration_s=scenario["simulation_duration_s"], timing=evaluation["timing"],
                      measurements=evaluation["measurements"], assets_identity=assets["identity"])
        if args.video:
            # Clip quality is evidence about the recording; it never changes the mechanical status.
            result.update(video_report(capture))
        if not automation_succeeded(text, exit_code, scope):
            result.update(status="fail", reason="Automation process or exact requested scenario did not pass")
    except (CaptureError, OSError, ValueError, KeyError, TypeError) as error:
        result.update(status="inconclusive", reason=str(error))
    result["total_process_and_analysis_wall_s"] = time.monotonic() - started
    atomic_json(run_dir / "run.json", result)
    print(f"{result['status']}: {map_key}/{variant}/{mode}/{iteration} â€” {result.get('evaluation', run_dir / 'run.json')}", flush=True)
    if args.video:
        print(f"video_quality: {result.get('video_quality', 'invalid')} - {result.get('video_review') or result.get('video', {}).get('error')}", flush=True)
    return result


def video_report(capture):
    """Validate, join and grade the run's clip; any failure becomes an `invalid` verdict, never an exception.

    The clip is evidence about the recording. Malformed recorder or link output (a null field, a wrong
    type) must not crash the batch or replace the mechanical status, so every exception is caught here.
    """
    try:
        video = analyze_video(capture)
        return dict(video_quality=video["video_quality"], video_review=str(capture / "video-review.html"),
                    video=video_summary(capture, video))
    except Exception as error:  # noqa: BLE001 - deliberately total; see the docstring.
        return dict(video_quality="invalid", video=dict(error=f"{type(error).__name__}: {error}"))


def video_summary(capture, video):
    return dict(path=str(capture / video["video"]), link_status=video.get("link_status"), stop=video.get("link_stop"),
                reasons=video["quality"]["reasons"],
                measured=video["quality"].get("measured"), thresholds=video["quality"]["thresholds"],
                validation_issues=video["validation"]["issues"], ffprobe=video["validation"]["ffprobe"],
                max_pts_deviation_us=video["validation"]["max_pts_deviation_us"],
                resolution=video["resolution"], join=video["join"],
                contacts=[m for m in video["markers"] if m["marker"] in ("contact", "defense")])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", type=Path, default=TOOLS / "scenarios/finisher-recovery.json")
    parser.add_argument("--map", choices=("ThirdPerson", "DefenseMatrix", "all"), default="ThirdPerson")
    parser.add_argument("--variant", default=None, help="Registered scenario variant, or all; defaults to the first variant")
    parser.add_argument("--mode", choices=("disabled", "motion", "rendered", "all"), default="rendered")
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--engine", type=Path, default=Path("C:/Program Files/Epic Games/UE_5.6"))
    parser.add_argument("--skip-build", action="store_true", help="Record use of an existing build; caller must ensure it matches source")
    parser.add_argument("--render-world", action="store_true", help="Keep rendering enabled for disabled/motion modes to isolate recorder overhead")
    parser.add_argument("--camera-view", default="default", help="Named camera view from the registered scenario; default preserves its standard camera")
    parser.add_argument("--placement", default="default", help="Named participant placement from the registered scenario; applied as fixture setup before public input")
    parser.add_argument("--timeout", type=int, default=240, help="Per-process wall deadline including editor startup")
    parser.add_argument("--control-offset-cm", type=float, default=0, help="Deliberate transient PIE mesh displacement for detector validation")
    parser.add_argument("--finisher-experiment", choices=("none", "permit-root-motion", "attacker-source-translation", "victim-source-translation", "victim-source-rotation", "paired-warp-tuning"), default="none",
                        help="Transient movement and per-role warp controls; restored without saving and recorded in capture provenance")
    parser.add_argument("--victim-warp-window", type=float, nargs=2, metavar=("START_S", "END_S"), help="Effective victim warp window for paired-warp-tuning")
    parser.add_argument("--victim-warp-offset", type=float, nargs=3, metavar=("X", "Y", "Z"), help="Victim relative offset in cm for paired-warp-tuning")
    parser.add_argument("--victim-facing-policy", choices=("face-partner", "face-away-from-partner", "match-partner-heading"),
                        help="Transient victim facing for paired-warp-tuning; omitted preserves the asset policy")
    parser.add_argument("--primary-sync-time", type=float, help="Transient primary sync start on both roles; preserves notify durations")
    parser.add_argument("--sync-nudge", choices=("enabled", "disabled"), help="Transient primary-sync position nudge on both roles")
    parser.add_argument("--entry-config", type=Path, help="Complete transient paired-entry configuration JSON for paired-warp-tuning")
    parser.add_argument("--references", type=Path, help="Explicitly selected reference directory; never automatically promotes current results")
    parser.add_argument("--declare-change", action="append", default=[], help="Document each intended source/asset difference from a reference")
    parser.add_argument("--video", action="store_true", help="Also record an MP4 of the scenario with PresentationCapture (rendered mode only)")
    parser.add_argument("--video-fps", type=int, default=60, help="Requested video rate, 1-120; the achieved cadence is graded, not promised")
    parser.add_argument("--video-resolution", type=int, choices=sorted(RESOLUTION_BOXES), default=720,
                        help="Video output box height; the PIE viewport is sized to match so the clip is not scaled")
    parser.add_argument("--video-seconds", type=int, default=30,
                        help="Clip bound, 1-30 s (the recorder's limit). A bound shorter than the scenario ends the clip "
                             "first; capture-link.json then records stopped_by_recorder_limit")
    parser.add_argument("--rhi", choices=("default", "d3d11", "dx12"), default="default",
                        help="Rendered runs only: default keeps the project RHI (D3D11); dx12 or d3d11 forces one for this launch")
    args = parser.parse_args()
    if args.video and args.mode != "rendered":
        parser.error("--video records only rendered runs; use --mode rendered")
    if not 1 <= args.video_fps <= 120:
        parser.error("--video-fps must be 1-120")
    if not 1 <= args.video_seconds <= 30:
        parser.error("--video-seconds must be 1-30")
    args.video_request = dict(fps=args.video_fps, resolution=args.video_resolution, seconds=args.video_seconds) if args.video else None
    if not 1 <= args.repeat <= 20 or not 30 <= args.timeout <= 1800:
        parser.error("Repeat must be 1..20 and timeout 30..1800 seconds")
    if not math.isfinite(args.control_offset_cm) or abs(args.control_offset_cm) > 1000:
        parser.error("Control displacement must be finite and within +/-1000 cm")
    args.warp_tuning = None
    if args.finisher_experiment == "paired-warp-tuning":
        from evaluate_capture import validate_warp_tuning_settings
        try:
            settings = dict(victim_window_s=args.victim_warp_window, victim_offset_cm=args.victim_warp_offset)
            if args.entry_config is not None:
                settings["entry"] = read_json(args.entry_config)
            if args.victim_facing_policy is not None:
                settings["victim_facing_policy"] = args.victim_facing_policy
            sync = {}
            if args.primary_sync_time is not None:
                sync["time_s"] = args.primary_sync_time
            if args.sync_nudge is not None:
                sync["nudge_enabled"] = args.sync_nudge == "enabled"
            if sync:
                settings["primary_sync"] = sync
            args.warp_tuning = validate_warp_tuning_settings(settings)
        except CaptureError as error:
            parser.error(str(error))
    elif any(value is not None for value in (args.victim_warp_window, args.victim_warp_offset, args.victim_facing_policy, args.primary_sync_time, args.sync_nudge, args.entry_config)):
        parser.error("Victim warp settings require paired-warp-tuning")
    args.scenario = args.scenario.resolve()
    registered = {"finisher-recovery.json": "FinisherRecovery", "hold-release-recovery.json": "HoldReleaseRecovery"}
    if args.scenario.parent != (TOOLS / "scenarios").resolve() or args.scenario.name not in registered:
        parser.error("Scenario must be a registered repository definition")
    args.definition = read_json(args.scenario)
    try:
        resolve_placement(args.definition, args.placement)
    except CaptureError as error:
        parser.error(str(error))
    if args.camera_view != "default" and args.camera_view not in args.definition.get("camera_views", {}):
        parser.error("Camera view must be declared in the registered scenario")
    if args.definition["scenario"] != registered[args.scenario.name]:
        parser.error("Scenario definition does not match its registered driver")
    args.variant = args.variant or next(iter(args.definition["variants"]))
    if args.variant != "all" and args.variant not in args.definition["variants"]:
        parser.error("Unknown variant for the selected scenario")
    if args.definition["scenario"] != "FinisherRecovery" and args.control_offset_cm:
        parser.error("The displacement control is registered only for FinisherRecovery")
    if args.definition["scenario"] != "FinisherRecovery" and args.finisher_experiment != "none":
        parser.error("The movement experiment is registered only for FinisherRecovery")
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S")
    batch = REPO / "Saved/CombatScenarioRuns" / (stamp + "-" + uuid.uuid4().hex[:8])
    batch.mkdir(parents=True)
    print(f"Scenario batch: {batch}", flush=True)
    atomic_json(batch / "batch.json", dict(status="inconclusive", reason="Batch in progress"))
    try:
        if not args.skip_build:
            build = [str(args.engine / "Engine/Build/BatchFiles/Build.bat"), "KatanaCombatEditor", "Win64", "Development",
                     f"-Project={REPO / 'KatanaCombat.uproject'}", "-NoHotReload", "-NoUBA", "-MaxParallelActions=1",
                     f"-Log={batch / 'unreal-build-tool.log'}"]
            if checked_process(build, batch / "build.log", 1800) != 0:
                raise CaptureError(f"Editor build failed; see {batch / 'build.log'}")
        source = source_state()
        maps = ("ThirdPerson", "DefenseMatrix") if args.map == "all" else (args.map,)
        variants = tuple(args.definition["variants"]) if args.variant == "all" else (args.variant,)
        modes = ("disabled", "motion", "rendered") if args.mode == "all" else (args.mode,)
        results = []
        # Alternate modes per repetition to expose ordering effects instead of hiding them.
        for iteration in range(1, args.repeat + 1):
            for map_key in maps:
                for variant in variants:
                    for mode in modes:
                        results.append(run_one(args, batch, source, map_key, variant, mode, iteration))
        status = "fail" if any(r["status"] == "fail" for r in results) else "inconclusive" if any(r["status"] != "pass" for r in results) else "pass"
        atomic_json(batch / "batch.json", dict(status=status, source_identity=source["identity"], results=results))
        print(f"{status}: {batch / 'batch.json'}", flush=True)
        return 0 if status == "pass" else 1
    except (CaptureError, OSError, ValueError) as error:
        atomic_json(batch / "batch.json", dict(status="inconclusive", reason=str(error)))
        print(str(error), flush=True)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
