"""Resolve Katana's exact shared-suite revision without a machine-specific path."""
from __future__ import annotations

from pathlib import Path
import sys

PROJECT = Path(__file__).resolve().parents[2]
# The pin/verify/install mechanics are shared with other pinned plugins; this module binds the
# AnimationAnalysis paths and messages and keeps its historical public API.
_shared = PROJECT / "Tools/PluginDependencies"
if str(_shared) not in sys.path:
    sys.path.insert(0, str(_shared))
import pinned_plugin  # noqa: E402
from pinned_plugin import SOURCE_SUFFIXES, git  # noqa: E402,F401

NATIVE_RESOURCE_DIRECTORIES = ("Source", "Shaders")
SPEC = pinned_plugin.PinnedPlugin(
    name="AnimationAnalysis",
    lock="Tools/AnimationAnalysis/dependency.json",
    cache="Saved/AnalysisDependencies",
    setup_script="Tools/AnimationAnalysis/setup_dependency.py",
    native_files=("AnimationAnalysis.uplugin", "README.md"),
    native_directories=NATIVE_RESOURCE_DIRECTORIES,
    required=frozenset({"AnimationAnalysis.uplugin", "README.md", "Python/src/animation_analysis/__init__.py", "Python/pyproject.toml"}),
    missing_required_message="Dependency revision is missing its plugin or Python package",
    shadow_directories=("Python/src", *NATIVE_RESOURCE_DIRECTORIES),
)


def read_lock(project=PROJECT):
    return pinned_plugin.read_lock(SPEC, project)


def verified_files(root, revision):
    """Compare actual source bytes to committed blobs, independent of Git stat caches."""
    return pinned_plugin.verified_files(SPEC, root, revision)


def checkout(project=PROJECT):
    return pinned_plugin.checkout(SPEC, project)


def package_source(project=PROJECT):
    return checkout(project) / "Python/src"


def native_files(files):
    return pinned_plugin.native_files(SPEC, files)


def installed_plugin_files(plugin):
    return pinned_plugin.installed_plugin_files(SPEC, plugin)


def validate_plugin(project, expected):
    pinned_plugin.validate_plugin(SPEC, project, expected)


def dependency_source_manifest(project=PROJECT):
    return pinned_plugin.dependency_source_manifest(SPEC, project)
