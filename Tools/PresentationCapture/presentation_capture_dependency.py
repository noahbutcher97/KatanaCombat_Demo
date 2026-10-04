"""Resolve Katana's exact PresentationCapture revision and its locally built worker executables.

The core recorder plugin lives at ``Plugin/PresentationCapture`` in its repository. Only that
directory is installed into the ignored ``Plugins/PresentationCapture``. The repository ignores
its worker binaries, so setup compiles them from the pinned sources with the plugin's own
``BuildWorkers.ps1`` and records their SHA-256 in the install record. The optional analysis
bridge plugin is deliberately not installed.
"""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

PROJECT = Path(__file__).resolve().parents[2]
_shared = PROJECT / "Tools/PluginDependencies"
if str(_shared) not in sys.path:
    sys.path.insert(0, str(_shared))
import pinned_plugin  # noqa: E402
from pinned_plugin import SOURCE_SUFFIXES, git  # noqa: E402,F401

PLUGIN_ROOT = "Plugin/PresentationCapture"
WORKER_SCRIPT = "Source/Programs/PresentationCaptureEncoder/BuildWorkers.ps1"
WORKERS = ("PresentationCaptureEncoder.exe", "PresentationCapturePNG.exe")
SPEC = pinned_plugin.PinnedPlugin(
    name="PresentationCapture",
    lock="Tools/PresentationCapture/dependency.json",
    cache="Saved/PresentationCaptureDependencies",
    setup_script="Tools/PresentationCapture/setup_dependency.py",
    plugin_root=PLUGIN_ROOT,
    copy_all=True,
    required=frozenset({f"{PLUGIN_ROOT}/PresentationCapture.uplugin", f"{PLUGIN_ROOT}/{WORKER_SCRIPT}"}),
    missing_required_message="Dependency revision is missing the PresentationCapture plugin or its worker build script",
    shadow_directories=(f"{PLUGIN_ROOT}/Source",),
    local_source_note=("PresentationCapture has no remote: its source of truth is the owner's local repository on the "
                       "build machine, which the self-hosted CI runner shares."),
)


def read_lock(project=PROJECT):
    return pinned_plugin.read_lock(SPEC, project)


def checkout(project=PROJECT):
    return pinned_plugin.checkout(SPEC, project)


def dependency_source_manifest(project=PROJECT):
    return pinned_plugin.dependency_source_manifest(SPEC, project)


def worker_hashes(plugin):
    directory = Path(plugin) / "Binaries/Win64"
    return {name: hashlib.sha256((directory / name).read_bytes()).hexdigest()
            for name in WORKERS if (directory / name).is_file()}


def powershell_command():
    """PowerShell 7 when present; otherwise Windows PowerShell with its own module path.

    A Windows PowerShell child inherits PowerShell 7's PSModulePath when launched from it and
    then fails to load built-in cmdlets such as Get-FileHash, which the worker script uses.
    """
    environment = dict(os.environ)
    shell = shutil.which("pwsh")
    if not shell:
        shell = shutil.which("powershell")
        environment.pop("PSModulePath", None)
    if not shell:
        raise ValueError("PresentationCapture worker build needs PowerShell (pwsh or powershell) on PATH")
    return [shell, "-NoProfile", "-ExecutionPolicy", "Bypass"], environment


def build_workers(plugin, evidence):
    """Compile both workers in the generated copy with the plugin's script (MSVC x64 and a Windows SDK)."""
    script = Path(plugin) / WORKER_SCRIPT
    shell, environment = powershell_command()
    completed = subprocess.run([*shell, "-File", str(script), "-EvidenceDirectory", str(evidence)],
                               capture_output=True, text=True, env=environment)
    if completed.returncode != 0:
        output = (completed.stdout + completed.stderr).strip().splitlines()[-12:]
        raise ValueError("PresentationCapture worker build failed (exit "
                         f"{completed.returncode}); it needs Visual Studio C++ x64 tools and a Windows SDK. "
                         f"Evidence: {evidence}\n" + "\n".join(output))


def check_encoder(plugin, scratch):
    """Run the encoder's own ``--check`` self-test: a short H.264 encode the recorder also runs before capture."""
    probe = Path(scratch) / f"encoder-check-{uuid.uuid4().hex}.mp4"
    try:
        completed = subprocess.run([str(Path(plugin) / "Binaries/Win64" / WORKERS[0]), "--check", str(probe)],
                                   capture_output=True, text=True, timeout=30)
        data = probe.read_bytes() if probe.is_file() else b""
    except subprocess.TimeoutExpired as error:
        raise ValueError("PresentationCapture encoder self-check did not finish within 30 s") from error
    finally:
        for leftover in (probe, Path(str(probe) + ".native-partial"), Path(str(probe) + ".publishing")):
            if leftover.is_file():
                leftover.unlink()  # Created by this check only.
    output = completed.stdout + completed.stderr
    if completed.returncode != 0 or "BACKEND=PCE_VIDEO_V1" not in output or "DONE=1" not in output or data[4:8] != b"ftyp":
        raise ValueError("PresentationCapture encoder self-check failed (exit "
                         f"{completed.returncode}); check Windows media components. Output: {output.strip()[-400:]}")
    return dict(status="passed", backend="PCE_VIDEO_V1", probe_bytes=len(data))


def ensure_workers(project, plugin, record, builder=build_workers, checker=check_encoder):
    """Reuse workers this setup recorded for the installed revision, otherwise rebuild them; then self-check."""
    project = Path(project).resolve()
    current = worker_hashes(plugin)
    if len(current) == len(WORKERS) and current == record.get("workers"):
        build = record.get("worker_build", "reused")
    else:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        evidence = project / SPEC.cache / "worker-builds" / f"{stamp}-{uuid.uuid4().hex[:8]}"
        evidence.parent.mkdir(parents=True, exist_ok=True)
        builder(plugin, evidence)
        current = worker_hashes(plugin)
        if len(current) != len(WORKERS):
            raise ValueError(f"PresentationCapture worker build did not produce {', '.join(WORKERS)}; see {evidence}")
        build = evidence.relative_to(project).as_posix()
    return dict(workers=current, worker_build=build, encoder_check=checker(plugin, project / SPEC.cache))


def install(repository=None, project=PROJECT, builder=build_workers, checker=check_encoder):
    project = Path(project).resolve()
    return pinned_plugin.install(SPEC, repository, project,
                                 finish=lambda plugin, record: ensure_workers(project, plugin, record, builder, checker))
