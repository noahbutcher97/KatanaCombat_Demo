"""Install and resolve project plugins pinned to an exact Git commit; never follow a branch tip.

A ``PinnedPlugin`` describes one dependency: its lock, cache, generated plugin directory, which
committed files form the native plugin and the consumer's wording. Consumers keep thin modules
(``Tools/AnimationAnalysis/animation_analysis_dependency.py``,
``Tools/PresentationCapture/presentation_capture_dependency.py``) that bind a description to these
functions, so each dependency keeps its own paths, messages and command-line entry points.

The checkout's committed bytes are verified independently of Git stat caches. A generated plugin
copy is a consumption artifact: install refuses to overwrite unowned or modified copies and keeps
the previous generated copy, including its build outputs, when the revision changes.
"""
from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import uuid

SOURCE_SUFFIXES = {".cpp", ".h", ".cs", ".ini", ".py", ".json", ".toml", ".uplugin", ".uproject", ".usf", ".ush"}
# Unreal and the worker builds write here inside a generated plugin; they are never pinned source.
BUILD_OUTPUT_DIRECTORIES = ("Binaries", "Intermediate")


@dataclass(frozen=True)
class PinnedPlugin:
    """Where one pinned dependency lives in the consuming project and what it installs."""
    name: str
    """Lock ``name``, generated ``Plugins/<name>`` directory and ``Dependencies/<name>/`` identity prefix."""
    lock: str
    """Project-relative lock file."""
    cache: str
    """Project-relative directory holding pinned checkouts, the install record and retired copies."""
    setup_script: str
    """Project-relative installer named in actionable errors."""
    plugin_root: str = ""
    """Repository-relative directory that holds the plugin; empty when the repository root is the plugin."""
    native_files: tuple = ()
    """Plugin-relative files copied into the generated plugin."""
    native_directories: tuple = ()
    """Plugin-relative directories copied recursively into the generated plugin."""
    copy_all: bool = False
    """Copy every committed file below ``plugin_root`` instead of the selections above."""
    required: frozenset = frozenset()
    """Repository-relative files every pinned revision must contain."""
    missing_required_message: str = "Dependency revision is missing its plugin"
    shadow_directories: tuple = ()
    """Repository-relative directories where ignored source files must not hide behind tracked ones."""

    @property
    def plugin(self):
        return f"Plugins/{self.name}"

    @property
    def marker(self):
        return f"{self.cache}/plugin-install.json"


def git(root, *arguments):
    return subprocess.check_output(["git", "-C", str(root), *arguments], stderr=subprocess.PIPE)


def read_lock(spec, project):
    lock = json.loads((Path(project) / spec.lock).read_text(encoding="utf-8"))
    if (lock.get("schema_version") != 1 or lock.get("name") != spec.name
            or not re.fullmatch(r"[0-9a-f]{40}", str(lock.get("revision", "")))):
        raise ValueError(f"{spec.name} dependency must name a full, lowercase Git commit SHA")
    return lock


def verified_files(spec, root, revision):
    """Compare actual source bytes to committed blobs, independent of Git stat caches."""
    if git(root, "rev-parse", "HEAD").decode().strip() != revision:
        raise ValueError(f"{spec.name} checkout does not match the dependency pin")
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
    for directory in (root / name for name in spec.shadow_directories):
        for path in directory.rglob("*"):
            if path.is_file() and path.suffix in SOURCE_SUFFIXES and path.relative_to(root).as_posix() not in files:
                raise ValueError(f"Unexpected dependency source: {path.relative_to(root)}")
    if not spec.required <= files.keys():
        raise ValueError(spec.missing_required_message)
    return files


def checkout(spec, project):
    revision = read_lock(spec, project)["revision"]
    root = Path(project) / spec.cache / revision
    if not root.is_dir():
        raise ValueError(f"{spec.name} is not installed. Run python {spec.setup_script} --repository <{spec.name}-checkout>")
    verified_files(spec, root, revision)
    return root


def _plugin_relative(spec, name):
    """Map a repository-relative name to its generated-plugin path, or None when not installed."""
    prefix = spec.plugin_root.strip("/")
    if prefix:
        if not name.startswith(prefix + "/"):
            return None
        name = name[len(prefix) + 1:]
    if spec.copy_all:
        return None if name.split("/", 1)[0] in BUILD_OUTPUT_DIRECTORIES else name
    if name in spec.native_files or any(name.startswith(directory + "/") for directory in spec.native_directories):
        return name
    return None


def native_files(spec, files):
    """Repository-relative verified files -> plugin-relative installed files with their SHA-256."""
    selected = {}
    for name, sha in files.items():
        relative = _plugin_relative(spec, name)
        if relative is not None:
            selected[relative] = sha
    return selected


def installed_plugin_files(spec, plugin):
    if spec.copy_all:
        paths = [path for path in plugin.rglob("*")
                 if path.relative_to(plugin).parts[0] not in BUILD_OUTPUT_DIRECTORIES]
    else:
        paths = [path for name in spec.native_directories for path in (plugin / name).rglob("*")]
        paths += [plugin / name for name in spec.native_files]
    return {path.relative_to(plugin).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in paths if path.is_file()}


def validate_plugin(spec, project, expected):
    plugin = Path(project) / spec.plugin
    if plugin.is_symlink() or installed_plugin_files(spec, plugin) != native_files(spec, expected):
        raise ValueError(f"Generated {spec.name} plugin differs from the pinned source; run setup_dependency.py after preserving any local edits")


def dependency_source_manifest(spec, project):
    if not (Path(project) / spec.lock).is_file():
        return {}  # Source-identity fixtures may have no such dependency.
    root = checkout(spec, project)
    files = verified_files(spec, root, read_lock(spec, project)["revision"])
    validate_plugin(spec, project, files)
    return {f"Dependencies/{spec.name}/" + name: sha for name, sha in files.items()
            if Path(name).suffix in SOURCE_SUFFIXES}


def read_install_record(spec, project):
    marker = Path(project) / spec.marker
    return json.loads(marker.read_text()) if marker.is_file() else {}


def install(spec, repository=None, project=None, finish=None):
    """Install the pinned revision; ``finish(plugin, record)`` may add build outputs to the record.

    ``record`` is the previous install record. ``finish`` returns extra record fields; it runs
    after the generated source is in place and validated, before the record is published.
    """
    project = Path(project).resolve()
    lock = read_lock(spec, project)
    revision = lock["revision"]
    repository = repository or lock.get("repository")
    cache = project / spec.cache
    cache.mkdir(parents=True, exist_ok=True)
    target = cache / revision
    if not target.exists():
        if not repository:
            raise ValueError("Supply --repository with a checkout or repository containing the pinned commit")
        with tempfile.TemporaryDirectory(prefix="dependency-stage-", dir=cache) as directory:
            scratch = Path(directory).resolve()
            assert scratch.parent == cache and scratch.name.startswith("dependency-stage-")
            staged = scratch / "checkout"
            subprocess.run(["git", "-c", "core.autocrlf=false", "clone", "--no-hardlinks", "--no-checkout",
                            "--", str(repository), str(staged)], check=True, capture_output=True)
            git(staged, "config", "core.autocrlf", "false")
            git(staged, "checkout", "--detach", revision)
            verified_files(spec, staged, revision)
            staged.rename(target)
    files = verified_files(spec, target, revision)
    expected = native_files(spec, files)
    plugin = project / spec.plugin
    marker = cache / "plugin-install.json"
    previous = json.loads(marker.read_text()) if marker.is_file() else {}
    if plugin.exists() and installed_plugin_files(spec, plugin) == expected:
        validate_plugin(spec, project, files)
    else:
        if plugin.exists():
            if plugin.is_symlink() or not previous.get("files") or installed_plugin_files(spec, plugin) != previous["files"]:
                raise ValueError("Existing plugin has unowned or modified source; preserve it before installing")
        with tempfile.TemporaryDirectory(prefix="plugin-stage-", dir=cache) as directory:
            scratch = Path(directory).resolve()
            assert scratch.parent == cache and scratch.name.startswith("plugin-stage-")
            staged = scratch / spec.name
            source = target / spec.plugin_root if spec.plugin_root else target
            for name in expected:
                destination = staged / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source / name, destination)
            assert installed_plugin_files(spec, staged) == expected
            plugin.parent.mkdir(parents=True, exist_ok=True)
            if plugin.exists():
                backup = cache / ("retired-plugin-" + uuid.uuid4().hex)
                assert plugin.resolve().parent == project / "Plugins" and backup.parent == cache
                plugin.rename(backup)  # Preserve the prior generated plugin, including build outputs.
            staged.rename(plugin)
        validate_plugin(spec, project, files)
    extra = finish(plugin, previous if previous.get("revision") == revision else {}) if finish else {}
    result = dict(schema_version=1, revision=revision, files=expected, **extra)
    temporary = marker.with_suffix(".tmp")
    temporary.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    temporary.replace(marker)
    return dict(revision=revision, checkout=str(target), plugin=str(plugin), native_source_files=len(expected), **extra)
