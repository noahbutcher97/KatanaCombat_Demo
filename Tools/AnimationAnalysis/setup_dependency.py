"""Install the pinned shared suite from an explicit repository; never follow its branch tip."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import uuid

from animation_analysis_dependency import (PROJECT, git, installed_plugin_files, native_files,
                                         read_lock, validate_plugin, verified_files)


def install(repository=None, project=PROJECT):
    project = Path(project).resolve()
    lock = read_lock(project)
    revision = lock["revision"]
    repository = repository or lock.get("repository")
    cache = project / "Saved/AnalysisDependencies"
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
            verified_files(staged, revision)
            staged.rename(target)
    files = verified_files(target, revision)
    expected = native_files(files)
    plugin = project / "Plugins/AnimationAnalysis"
    marker = cache / "plugin-install.json"
    if plugin.exists() and installed_plugin_files(plugin) == expected:
        validate_plugin(project, files)
    else:
        if plugin.exists():
            previous = json.loads(marker.read_text()) if marker.is_file() else {}
            if plugin.is_symlink() or not previous.get("files") or installed_plugin_files(plugin) != previous["files"]:
                raise ValueError("Existing plugin has unowned or modified source; preserve it before installing")
        with tempfile.TemporaryDirectory(prefix="plugin-stage-", dir=cache) as directory:
            scratch = Path(directory).resolve()
            assert scratch.parent == cache and scratch.name.startswith("plugin-stage-")
            staged = scratch / "AnimationAnalysis"
            for name in expected:
                destination = staged / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(target / name, destination)
            assert installed_plugin_files(staged) == expected
            plugin.parent.mkdir(parents=True, exist_ok=True)
            if plugin.exists():
                backup = cache / ("retired-plugin-" + uuid.uuid4().hex)
                assert plugin.resolve().parent == project / "Plugins" and backup.parent == cache
                plugin.rename(backup)  # Preserve the prior generated plugin, including build outputs.
            staged.rename(plugin)
        validate_plugin(project, files)
    result = dict(schema_version=1, revision=revision, files=expected)
    temporary = marker.with_suffix(".tmp")
    temporary.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    temporary.replace(marker)
    return dict(revision=revision, checkout=str(target), plugin=str(plugin), native_source_files=len(expected))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", help="Override the lock's repository with a checkout or clone URL containing the pinned commit")
    args = parser.parse_args()
    try:
        print(json.dumps(install(args.repository), indent=2))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Dependency setup failed: {error}\n")
