"""Create or update a sibling worktree at a committed revision, install its pinned plugins and build it.

Captures then measure exactly that revision, unaffected by in-progress edits in any other checkout,
and run_scenario.py's own source-identity checks stay meaningful. The worktree is detached at the
resolved commit so it never holds a branch.

Safety: the script refuses a target with uncommitted changes (tracked or untracked), refuses a
directory it did not create, and never deletes anything: no worktree removal, clean, reset or forced
checkout. It records what it created in <worktree>/Saved/capture-worktree.json (ignored by Git).
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import sys

MARKER = Path("Saved/capture-worktree.json")
ENGINE = Path("C:/Program Files/Epic Games/UE_5.6")


class WorktreeError(Exception):
    """The target cannot be created or updated safely."""


def git(root, *arguments):
    completed = subprocess.run(["git", "-C", str(root), *arguments], capture_output=True, text=True)
    if completed.returncode:
        raise WorktreeError(f"git {' '.join(arguments)} failed: {completed.stderr.strip()}")
    return completed.stdout.strip()


def same(a, b):
    return os.path.normcase(str(Path(a).resolve())) == os.path.normcase(str(Path(b).resolve()))


def registered_worktrees(repo):
    return [Path(line[len("worktree "):]) for line in git(repo, "worktree", "list", "--porcelain").splitlines()
            if line.startswith("worktree ")]


def default_path(repo):
    """<main checkout>-capture-run, beside the main checkout."""
    main = registered_worktrees(repo)[0]
    return main.parent / f"{main.name}-capture-run"


def ensure_worktree(repo, ref, path):
    """Create the worktree at ref, or move an existing one this script created to ref. Returns a record."""
    repo, path = Path(repo).resolve(), Path(path).resolve()
    commit = git(repo, "rev-parse", "--verify", f"{ref}^{{commit}}")
    worktrees = registered_worktrees(repo)
    if any(same(path, tree) for tree in worktrees[:1]) or same(path, repo):
        raise WorktreeError(f"{path} is a working checkout, not a capture worktree")
    marker = path / MARKER
    now = datetime.now(timezone.utc).isoformat(timespec="seconds")
    if path.exists():
        if not any(same(path, tree) for tree in worktrees):
            raise WorktreeError(f"{path} exists but is not a worktree of {repo}; refusing to touch it")
        if not marker.is_file():
            raise WorktreeError(f"{path} was not created by capture_worktree.py (no {MARKER.as_posix()}); refusing to touch it")
        changes = git(path, "status", "--porcelain", "--untracked-files=normal")
        if changes:
            raise WorktreeError(f"{path} has uncommitted changes; commit or move them before updating:\n{changes}")
        record = json.loads(marker.read_text(encoding="utf-8"))
        previous = git(path, "rev-parse", "HEAD")
        if previous != commit:
            git(path, "checkout", "--detach", commit)
        record.setdefault("history", []).append(dict(commit=commit, ref=ref, at=now, previous=previous))
        action = "updated" if previous != commit else "unchanged"
    else:
        git(repo, "worktree", "add", "--detach", str(path), commit)
        record = dict(created_by="Tools/CombatCapture/capture_worktree.py", created=now, source_repository=str(repo),
                      history=[dict(commit=commit, ref=ref, at=now, previous=None)])
        action = "created"
    if git(path, "rev-parse", "HEAD") != commit:
        raise WorktreeError(f"{path} is not at {commit} after checkout")
    marker.parent.mkdir(parents=True, exist_ok=True)
    marker.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return dict(path=str(path), commit=commit, ref=ref, action=action)


def local_analysis_repository(repo, worktree):
    """Reuse this checkout's verified AnimationAnalysis clone of the target's pin, avoiding a network fetch."""
    lock = Path(worktree) / "Tools/AnimationAnalysis/dependency.json"
    if not lock.is_file():
        return None
    revision = json.loads(lock.read_text(encoding="utf-8")).get("revision", "")
    candidate = Path(repo) / "Saved/AnalysisDependencies" / revision
    return candidate if revision and (candidate / ".git").exists() else None


def setup_commands(repo, worktree, analysis_repository=None, recorder_repository=None):
    """Plugin setup commands present at the target revision, in dependency order."""
    commands = []
    analysis = Path(worktree) / "Tools/AnimationAnalysis/setup_dependency.py"
    if analysis.is_file():
        source = analysis_repository or local_analysis_repository(repo, worktree)
        commands.append([sys.executable, str(analysis)] + (["--repository", str(source)] if source else []))
    recorder = Path(worktree) / "Tools/PresentationCapture/setup_dependency.py"
    if recorder.is_file():
        commands.append([sys.executable, str(recorder)] + (["--repository", str(recorder_repository)] if recorder_repository else []))
    return commands


def build_command(worktree, engine=ENGINE, parallel=4):
    worktree = Path(worktree)
    return [str(Path(engine) / "Engine/Build/BatchFiles/Build.bat"), "KatanaCombatEditor", "Win64", "Development",
            f"-Project={worktree / 'KatanaCombat.uproject'}", "-NoHotReload", "-WaitMutex", "-NoUBA",
            f"-MaxParallelActions={parallel}", f"-Log={worktree / 'Saved/Logs/capture-worktree-ubt.txt'}"]


def run_logged(command, log, cwd):
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8") as stream:
        code = subprocess.run(command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT).returncode
    if code:
        raise WorktreeError(f"{Path(command[1] if command[0] == sys.executable else command[0]).name} failed with exit {code}; see {log}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--ref", default="HEAD", help="Commit or branch to capture; resolved to a commit and checked out detached")
    parser.add_argument("--path", type=Path, help="Worktree directory; defaults to <main checkout>-capture-run beside it")
    parser.add_argument("--analysis-repository", type=Path, help="AnimationAnalysis checkout containing the target's pin")
    parser.add_argument("--recorder-repository", type=Path, help="PresentationCapture checkout containing the target's pin")
    parser.add_argument("--engine", type=Path, default=ENGINE)
    parser.add_argument("--max-parallel-actions", type=int, default=4, help="Build parallelism; lower it under memory pressure")
    parser.add_argument("--skip-setup", action="store_true")
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    try:
        target = args.path or default_path(repo)
        record = ensure_worktree(repo, args.ref, target)
        worktree = Path(record["path"])
        logs = worktree / "Saved/Logs"
        if not args.skip_setup:
            for index, command in enumerate(setup_commands(repo, worktree, args.analysis_repository, args.recorder_repository)):
                run_logged(command, logs / f"capture-worktree-setup-{index + 1}.log", worktree)
        if not args.skip_build:
            run_logged(build_command(worktree, args.engine, args.max_parallel_actions), logs / "capture-worktree-build.log", worktree)
    except WorktreeError as error:
        parser.exit(2, f"capture_worktree: {error}\n")
    record["built"] = not args.skip_build
    record["run_scenario"] = (f'python "{worktree / "Tools/CombatCapture/run_scenario.py"}" --map ThirdPerson --variant Completed '
                              f'--mode rendered --video{" --skip-build" if not args.skip_build else ""}')
    print(json.dumps(record, indent=2))
    print(f"CAPTURE_WORKTREE={worktree}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
