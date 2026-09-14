"""Resolve Katana's exact shared-suite revision without a machine-specific path."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import subprocess

PROJECT = Path(__file__).resolve().parents[2]
SOURCE_SUFFIXES = {".cpp", ".h", ".cs", ".ini", ".py", ".json", ".toml", ".uplugin", ".uproject", ".usf", ".ush"}
NATIVE_RESOURCE_DIRECTORIES = ("Source", "Shaders")


def git(root, *arguments):
    return subprocess.check_output(["git", "-C", str(root), *arguments], stderr=subprocess.PIPE)


def read_lock(project=PROJECT):
    lock = json.loads((project / "Tools/AnimationAnalysis/dependency.json").read_text(encoding="utf-8"))
    if (lock.get("schema_version") != 1 or lock.get("name") != "AnimationAnalysis"
            or not re.fullmatch(r"[0-9a-f]{40}", str(lock.get("revision", "")))):
        raise ValueError("AnimationAnalysis dependency must name a full, lowercase Git commit SHA")
    return lock


def verified_files(root, revision):
    """Compare actual source bytes to committed blobs, independent of Git stat caches."""
    if git(root, "rev-parse", "HEAD").decode().strip() != revision:
        raise ValueError("AnimationAnalysis checkout does not match the dependency pin")
    files = {}
    for entry in git(root, "ls-tree", "-rz", "--full-tree", "HEAD").split(b"\0"):
        if not entry:
            continue
        metadata, encoded = entry.split(b"\t", 1)
        mode, kind, expected = metadata.decode().split()
        name = encoded.decode("utf-8")
        path = root / name
        if mode != "100644" or kind != "blob" or path.is_symlink() or not path.resolve().is_relative_to(root.resolve()):
            raise ValueError(f"Unsupported dependency entry: {name}")
        data = path.read_bytes()
        actual = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        if actual != expected:
            raise ValueError(f"Modified dependency source: {name}; edit the standalone repository and update the pin")
        files[name] = hashlib.sha256(data).hexdigest()
    if git(root, "ls-files", "--others", "--exclude-standard").strip():
        raise ValueError("Unexpected untracked files in the pinned dependency checkout")
    # Ignored bytecode/build outputs are allowed, but ignored source cannot shadow a tracked module.
    for directory in (root / "Python/src", *(root / name for name in NATIVE_RESOURCE_DIRECTORIES)):
        for path in directory.rglob("*"):
            if path.is_file() and path.suffix in SOURCE_SUFFIXES and path.relative_to(root).as_posix() not in files:
                raise ValueError(f"Unexpected dependency source: {path.relative_to(root)}")
    required = {"AnimationAnalysis.uplugin", "README.md", "Python/src/animation_analysis/__init__.py", "Python/pyproject.toml"}
    if not required <= files.keys():
        raise ValueError("Dependency revision is missing its plugin or Python package")
    return files


def checkout(project=PROJECT):
    revision = read_lock(project)["revision"]
    root = project / "Saved/AnalysisDependencies" / revision
    if not root.is_dir():
        raise ValueError("AnimationAnalysis is not installed. Run python Tools/AnimationAnalysis/setup_dependency.py --repository <AnimationAnalysis-checkout>")
    verified_files(root, revision)
    return root


def package_source(project=PROJECT):
    return checkout(project) / "Python/src"


def native_files(files):
    return {name: sha for name, sha in files.items()
            if name in ("AnimationAnalysis.uplugin", "README.md")
            or any(name.startswith(directory + "/") for directory in NATIVE_RESOURCE_DIRECTORIES)}


def installed_plugin_files(plugin):
    paths = [path for name in NATIVE_RESOURCE_DIRECTORIES for path in (plugin / name).rglob("*")]
    paths += [plugin / "AnimationAnalysis.uplugin", plugin / "README.md"]
    return {path.relative_to(plugin).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in paths if path.is_file()}


def validate_plugin(project, expected):
    plugin = project / "Plugins/AnimationAnalysis"
    if plugin.is_symlink() or installed_plugin_files(plugin) != native_files(expected):
        raise ValueError("Generated AnimationAnalysis plugin differs from the pinned source; run setup_dependency.py after preserving any local edits")


def dependency_source_manifest(project=PROJECT):
    if not (project / "Tools/AnimationAnalysis/dependency.json").is_file():
        return {}  # Source-identity fixtures may have no suite dependency.
    root = checkout(project)
    files = verified_files(root, read_lock(project)["revision"])
    validate_plugin(project, files)
    return {"Dependencies/AnimationAnalysis/" + name: sha for name, sha in files.items()
            if Path(name).suffix in SOURCE_SUFFIXES}
