"""Consumer pinning must survive upstream edits and reject altered installed sources."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from animation_analysis_dependency import checkout, dependency_source_manifest, git, read_lock
from setup_dependency import install


class DependencyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="animation-dependency-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.upstream = self.root / "upstream"
        self.upstream.mkdir()
        subprocess.run(["git", "init", str(self.upstream)], check=True, capture_output=True)
        git(self.upstream, "config", "core.autocrlf", "false")
        for name, data in {
            ".gitignore": "__pycache__/\n*.pyc\n",
            "README.md": "Neutral fixture\n",
            "AnimationAnalysis.uplugin": "{}\n",
            "Source/AnimationCapture/Fixture.cpp": "// source revision one\n",
            "Shaders/Private/Fixture.usf": '#include "Fixture.ush"\n',
            "Shaders/Private/Fixture.ush": "#define FIXTURE_VALUE 1\n",
            "Python/pyproject.toml": '[project]\nname="fixture"\n',
            "Python/src/animation_analysis/__init__.py": "VALUE = 1\r\n",
        }.items():
            path = self.upstream / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data.encode())
        git(self.upstream, "add", "--", ".gitignore", "README.md", "AnimationAnalysis.uplugin", "Source", "Shaders", "Python")
        self.commit()
        self.revision = git(self.upstream, "rev-parse", "HEAD").decode().strip()
        self.project = self.root / "consumer"
        self.lock = self.project / "Tools/AnimationAnalysis/dependency.json"
        self.lock.parent.mkdir(parents=True)
        self.write_lock(self.revision)

    def commit(self):
        git(self.upstream, "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "commit", "-m", "Fixture revision")

    def write_lock(self, revision):
        self.lock.write_text(json.dumps(dict(schema_version=1, name="AnimationAnalysis", revision=revision)))

    def test_install_uses_pin_and_preserves_bytes_despite_newer_upstream_revision(self):
        path = self.upstream / "Python/src/animation_analysis/__init__.py"
        path.write_bytes(b"VALUE = 2\r\n")
        git(self.upstream, "add", "--", str(path))
        self.commit()
        install(self.upstream, self.project)
        root = checkout(self.project)
        self.assertEqual((root / "Python/src/animation_analysis/__init__.py").read_bytes(), b"VALUE = 1\r\n")
        self.assertEqual(install(project=self.project)["revision"], self.revision)
        manifest = dependency_source_manifest(self.project)
        self.assertIn("Dependencies/AnimationAnalysis/Python/src/animation_analysis/__init__.py", manifest)
        self.assertIn("Dependencies/AnimationAnalysis/Source/AnimationCapture/Fixture.cpp", manifest)

    def test_shader_resources_are_installed_and_identified(self):
        install(self.upstream, self.project)
        manifest = dependency_source_manifest(self.project)
        for name in ("Shaders/Private/Fixture.usf", "Shaders/Private/Fixture.ush"):
            self.assertEqual((self.project / "Plugins/AnimationAnalysis" / name).read_bytes(),
                             (self.upstream / name).read_bytes())
            self.assertIn("Dependencies/AnimationAnalysis/" + name, manifest)

    def test_modified_installed_shader_is_preserved_and_rejected(self):
        install(self.upstream, self.project)
        path = self.project / "Plugins/AnimationAnalysis/Shaders/Private/Fixture.usf"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("// local shader edit\n")
        with self.assertRaisesRegex(ValueError, "differs from the pinned"):
            dependency_source_manifest(self.project)
        with self.assertRaisesRegex(ValueError, "modified source"):
            install(project=self.project)
        self.assertEqual(path.read_text(), "// local shader edit\n")

    def test_untracked_installed_shader_is_rejected(self):
        install(self.upstream, self.project)
        path = self.project / "Plugins/AnimationAnalysis/Shaders/Private/Unexpected.ush"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("// unowned shader include\n")
        with self.assertRaisesRegex(ValueError, "differs from the pinned"):
            dependency_source_manifest(self.project)

    def test_ignored_cached_shader_cannot_shadow_the_pin(self):
        install(self.upstream, self.project)
        root = checkout(self.project)
        (root / ".git/info/exclude").write_text("Shaders/Private/Unexpected.ush\n")
        (root / "Shaders/Private/Unexpected.ush").write_text("// ignored shader include\n")
        with self.assertRaisesRegex(ValueError, "Unexpected dependency source"):
            checkout(self.project)

    def test_modified_cached_package_is_rejected(self):
        install(self.upstream, self.project)
        root = checkout(self.project)
        (root / "Python/src/animation_analysis/__init__.py").write_bytes(b"VALUE = 9\r\n")
        with self.assertRaisesRegex(ValueError, "Modified dependency source"):
            checkout(self.project)

    def test_fresh_install_resolves_repository_from_lock(self):
        lock = json.loads(self.lock.read_text())
        lock["repository"] = str(self.upstream)
        self.lock.write_text(json.dumps(lock))
        self.assertEqual(install(project=self.project)["revision"], self.revision)
        dependency_source_manifest(self.project)

    def test_modified_plugin_is_preserved_and_rejected(self):
        install(self.upstream, self.project)
        path = self.project / "Plugins/AnimationAnalysis/Source/AnimationCapture/Fixture.cpp"
        path.write_text("// local edit\n")
        with self.assertRaisesRegex(ValueError, "differs from the pinned"):
            dependency_source_manifest(self.project)
        with self.assertRaisesRegex(ValueError, "modified source"):
            install(project=self.project)
        self.assertEqual(path.read_text(), "// local edit\n")

    def test_revision_update_preserves_previous_plugin(self):
        install(self.upstream, self.project)
        path = self.upstream / "Source/AnimationCapture/Fixture.cpp"
        path.write_text("// source revision two\n")
        git(self.upstream, "add", "--", str(path))
        self.commit()
        self.write_lock(git(self.upstream, "rev-parse", "HEAD").decode().strip())
        install(self.upstream, self.project)
        previous = list((self.project / "Saved/AnalysisDependencies").glob("retired-plugin-*"))
        self.assertEqual(len(previous), 1)
        self.assertIn("revision one", (previous[0] / "Source/AnimationCapture/Fixture.cpp").read_text())
        dependency_source_manifest(self.project)

    def test_missing_or_symbolic_pin_fails_with_actionable_error(self):
        with self.assertRaisesRegex(ValueError, "setup_dependency.py"):
            checkout(self.project)
        self.write_lock("main")
        with self.assertRaisesRegex(ValueError, "full, lowercase"):
            read_lock(self.project)

    def test_untracked_python_module_is_rejected(self):
        install(self.upstream, self.project)
        root = checkout(self.project)
        (root / "Python/src/animation_analysis/unexpected.py").write_text("VALUE = 8\n")
        with self.assertRaisesRegex(ValueError, "Unexpected untracked"):
            checkout(self.project)
