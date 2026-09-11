"""Evaluate a paired-animation contact profile using the editor's shared native evaluator."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import os
from pathlib import Path
import re
import uuid

from analyze_capture import CaptureError, analyze, read_json, write_report
from animation_analysis.artifacts import atomic_json, digest
from run_scenario import REPO, checked_process, source_state


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--engine", type=Path, default=Path("C:/Program Files/Epic Games/UE_5.6"))
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S")
    directory = REPO / "Saved/PairedEvaluationRuns" / (stamp + "-" + uuid.uuid4().hex[:8])
    directory.mkdir(parents=True)
    result = dict(status="inconclusive", reason="Run in progress", profile=str(args.profile.resolve()))
    atomic_json(directory / "run.json", result)
    print(f"Paired evaluation run: {directory}", flush=True)
    try:
        if not args.skip_build:
            build = [str(args.engine / "Engine/Build/BatchFiles/Build.bat"), "KatanaCombatEditor", "Win64", "Development",
                     f"-Project={REPO / 'KatanaCombat.uproject'}", "-NoHotReload", "-NoUBA", "-MaxParallelActions=1", f"-Log={directory / 'ubt.log'}"]
            if checked_process(build, directory / "build.log", 1800) != 0:
                raise CaptureError("Editor build failed")
        if args.capture:
            report, frames, markers = analyze(args.capture)
            write_report(args.capture, report, frames, markers)
            if report["data_integrity"] != "valid":
                raise CaptureError("Runtime capture failed integrity validation")
        source = source_state()
        profile_identity = digest(args.profile)
        command = [str(args.engine / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"), str(REPO / "KatanaCombat.uproject"),
                   "-run=PairedAnimationEvaluation", f"-Profile={args.profile.resolve()}", "-NullRHI", "-unattended", "-nopause",
                   "-nosplash", "-stdout", "-DDC=InstalledNoZenLocalFallback", "-DisablePlugins=RiderLink",
                   f"-abslog={directory / 'evaluation.log'}"]
        if args.capture:
            command.append(f"-Capture={args.capture.resolve()}")
        atomic_json(directory / "command.json", command)
        env = dict(os.environ, **{"UE-LocalDataCachePath": str(REPO / "Saved/CombatCaptureCache")})
        exit_code = checked_process(command, directory / "stdout.log", 180, env)
        text = (directory / "stdout.log").read_text(encoding="utf-8-sig", errors="replace")
        outputs = set(re.findall(r"PAIRED_EVALUATION_OUTPUT=([^\r\n]+)", text))
        if len(outputs) != 1:
            raise CaptureError("Expected exactly one evaluation output")
        artifact = Path(outputs.pop().strip()).resolve()
        expected = (REPO / "Saved/PairedAnimationEvaluations").resolve()
        if artifact.parent != expected:
            raise CaptureError("Unexpected evaluation artifact location")
        evaluation = read_json(artifact / "evaluation.json")
        result.update(artifact=str(artifact), process_exit_code=exit_code)
        if source_state()["identity"] != source["identity"]:
            raise CaptureError("Source/config/profile/evaluator changed during execution")
        if digest(args.profile) != profile_identity:
            raise CaptureError("Profile bytes changed during execution")
        if exit_code != 0:
            raise CaptureError(evaluation.get("reason", "Evaluation process failed"))
        result.update(status=evaluation["status"], reason=evaluation["reason"], artifact=str(artifact),
                      process_exit_code=exit_code, source=source, build_this_run=not args.skip_build)
    except (CaptureError, OSError, ValueError, KeyError) as error:
        result.update(status="inconclusive", reason=str(error))
    atomic_json(directory / "run.json", result)
    print(f"{result['status']}: {result.get('artifact', directory)} â€” {result['reason']}", flush=True)
    return 0 if result["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
