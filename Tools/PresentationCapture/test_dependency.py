"""The PresentationCapture pin installs only the core plugin and owns the workers it builds."""
import json
import os
from pathlib import Path, PureWindowsPath
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from presentation_capture_dependency import (SPEC, WORKERS, checkout, dependency_source_manifest, git, install,
                                             read_lock, worker_hashes)
import pinned_plugin

PLUGIN = "Plugin/PresentationCapture"

# Stands in for ssh: git runs `<GIT_SSH_COMMAND> <host> "git-upload-pack '<path>'"` (GIT_SSH_VARIANT=simple).
# It records what git asked for and serves the fixture repository, so an scp-style remote clones offline.
FAKE_SSH = """import json, subprocess, sys
repository, record, host, command = sys.argv[1], sys.argv[2], sys.argv[-2], sys.argv[-1]
with open(record, "w") as stream:
    json.dump([host, command], stream)
raise SystemExit(subprocess.call(["git", command.split()[0].replace("git-", "", 1), repository]))
"""


class FakeWorkerBuilder:
    """Stands in for BuildWorkers.ps1: writes deterministic executables where the script would."""
    def __init__(self, payload=b"worker"):
        self.calls, self.payload = [], payload

    def __call__(self, plugin, evidence):
        self.calls.append(Path(evidence))
        output = Path(plugin) / "Binaries/Win64"
        output.mkdir(parents=True, exist_ok=True)
        for name in WORKERS:
            (output / name).write_bytes(self.payload + name.encode())


class PresentationCaptureDependencyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="presentation-dependency-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.upstream = self.root / "upstream"
        self.upstream.mkdir()
        subprocess.run(["git", "init", str(self.upstream)], check=True, capture_output=True)
        git(self.upstream, "config", "core.autocrlf", "false")
        for name, data in {
            ".gitignore": "Binaries/\nIntermediate/\n",
            f"{PLUGIN}/PresentationCapture.uplugin": "{}\n",
            f"{PLUGIN}/README.md": "Core recorder\n",
            f"{PLUGIN}/Config/FilterPlugin.ini": "[FilterPlugin]\n",
            f"{PLUGIN}/Source/PresentationCapture/Private/Recorder.cpp": "// recorder one\n",
            f"{PLUGIN}/Source/Programs/PresentationCaptureEncoder/Encoder.cpp": "// encoder one\r\n",
            f"{PLUGIN}/Source/Programs/PresentationCaptureEncoder/BuildWorkers.ps1": "param()\n",
            "Integration/PresentationCaptureAnalysisBridge/Bridge.uplugin": "{}\n",
            "Tests/Host/PresentationCaptureHost.uproject": "{}\n",
        }.items():
            path = self.upstream / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data.encode())
        git(self.upstream, "add", "--", ".")
        self.commit()
        self.revision = git(self.upstream, "rev-parse", "HEAD").decode().strip()
        self.project = self.root / "consumer"
        self.lock = self.project / SPEC.lock
        self.lock.parent.mkdir(parents=True)
        self.write_lock(self.revision)
        self.plugin = self.project / "Plugins/PresentationCapture"

    def install(self, builder, checker=None):
        self.checks = getattr(self, "checks", [])
        def check(plugin, scratch):
            self.checks.append(Path(plugin))
            return dict(status="passed")
        return install(project=self.project, builder=builder, checker=checker or check)

    def commit(self):
        git(self.upstream, "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "commit", "-m", "Fixture revision")

    def write_lock(self, revision, name="PresentationCapture"):
        self.lock.write_text(json.dumps(dict(schema_version=1, name=name, repository=str(self.upstream), revision=revision)))

    def advance_upstream(self):
        path = self.upstream / PLUGIN / "Source/PresentationCapture/Private/Recorder.cpp"
        path.write_text("// recorder two\n")
        git(self.upstream, "add", "--", str(path))
        self.commit()
        return git(self.upstream, "rev-parse", "HEAD").decode().strip()

    def test_installs_only_the_core_plugin_at_the_pin_and_builds_workers(self):
        self.advance_upstream()  # The upstream tip moves; the pin must not follow it.
        builder = FakeWorkerBuilder()
        result = self.install(builder=builder)
        self.assertEqual(result["revision"], self.revision)
        self.assertEqual((self.plugin / "Source/PresentationCapture/Private/Recorder.cpp").read_text(), "// recorder one\n")
        self.assertEqual((self.plugin / "Source/Programs/PresentationCaptureEncoder/Encoder.cpp").read_bytes(), b"// encoder one\r\n")
        self.assertFalse((self.plugin / "Integration").exists(), "the analysis bridge is never installed")
        self.assertFalse(any(self.plugin.rglob("*.uproject")), "repository test hosts are never installed")
        self.assertEqual(len(builder.calls), 1)
        self.assertTrue(builder.calls[0].is_relative_to(self.project / SPEC.cache), "worker evidence stays outside the plugin")
        record = json.loads((self.project / SPEC.marker).read_text())
        self.assertEqual(record["revision"], self.revision)
        self.assertEqual(record["workers"], worker_hashes(self.plugin))
        self.assertEqual(set(record["workers"]), set(WORKERS))
        self.assertEqual(record["encoder_check"], dict(status="passed"))
        manifest = dependency_source_manifest(self.project)
        self.assertIn(f"Dependencies/PresentationCapture/{PLUGIN}/Source/PresentationCapture/Private/Recorder.cpp", manifest)

    def test_recorded_workers_are_reused_and_missing_or_changed_workers_rebuilt(self):
        builder = FakeWorkerBuilder()
        self.install(builder=builder)
        self.install(builder=builder)
        self.assertEqual(len(builder.calls), 1, "unchanged recorded workers are reused")
        self.assertEqual(len(self.checks), 2, "the encoder self-check runs on every setup")
        (self.plugin / "Binaries/Win64" / WORKERS[0]).unlink()
        self.install(builder=builder)
        self.assertEqual(len(builder.calls), 2, "a missing worker is rebuilt")
        (self.plugin / "Binaries/Win64" / WORKERS[1]).write_bytes(b"replaced")
        self.install(builder=builder)
        self.assertEqual(len(builder.calls), 3, "a worker that differs from the record is rebuilt")

    def test_build_outputs_do_not_count_as_modified_source(self):
        self.install(builder=FakeWorkerBuilder())
        intermediate = self.plugin / "Intermediate/Build/Win64/generated.cpp"
        intermediate.parent.mkdir(parents=True)
        intermediate.write_text("// unreal build output\n")
        dependency_source_manifest(self.project)

    def test_modified_generated_source_is_preserved_and_rejected(self):
        self.install(builder=FakeWorkerBuilder())
        path = self.plugin / "Source/PresentationCapture/Private/Recorder.cpp"
        path.write_text("// local edit\n")
        with self.assertRaisesRegex(ValueError, "Generated PresentationCapture plugin differs from the pinned source"):
            dependency_source_manifest(self.project)
        with self.assertRaisesRegex(ValueError, "unowned or modified source"):
            self.install(builder=FakeWorkerBuilder())
        self.assertEqual(path.read_text(), "// local edit\n")

    def test_a_damaged_install_record_rebuilds_the_workers_after_a_missing_plugin_is_reinstalled(self):
        builder = FakeWorkerBuilder()
        self.install(builder=builder)
        shutil.rmtree(self.plugin)
        (self.project / SPEC.marker).write_text("{truncated")
        self.assertEqual(self.install(builder=builder)["revision"], self.revision)
        self.assertEqual(len(builder.calls), 2, "workers named by no readable record are rebuilt")
        record = json.loads((self.project / SPEC.marker).read_text())
        self.assertEqual(record["workers"], worker_hashes(self.plugin), "a valid record replaces the damaged one")
        dependency_source_manifest(self.project)

    def test_a_damaged_install_record_beside_an_unchanged_plugin_rebuilds_the_workers(self):
        builder = FakeWorkerBuilder()
        self.install(builder=builder)
        for damaged in ("{truncated", "[]", "\udcff"):
            with self.subTest(record=damaged):
                (self.project / SPEC.marker).write_text(damaged, errors="surrogateescape")
                self.install(builder=builder)
                self.assertEqual(json.loads((self.project / SPEC.marker).read_text())["workers"], worker_hashes(self.plugin))
        self.assertEqual(len(builder.calls), 4, "each unreadable record rebuilds rather than trusting unknown workers")

    def test_a_damaged_install_record_still_refuses_a_modified_plugin(self):
        self.install(builder=FakeWorkerBuilder())
        path = self.plugin / "Source/PresentationCapture/Private/Recorder.cpp"
        path.write_text("// local edit\n")
        (self.project / SPEC.marker).write_text("{truncated")
        with self.assertRaisesRegex(ValueError, "unowned or modified source"):
            self.install(builder=FakeWorkerBuilder())
        self.assertEqual(path.read_text(), "// local edit\n")

    def test_unowned_existing_copy_is_refused(self):
        stray = self.plugin / "PresentationCapture.uplugin"
        stray.parent.mkdir(parents=True)
        stray.write_text("{}\n")  # For example a hand-copied plugin with no install record.
        with self.assertRaisesRegex(ValueError, "unowned or modified source"):
            self.install(builder=FakeWorkerBuilder())
        self.assertTrue(stray.is_file())

    def test_revision_update_retires_previous_copy_and_rebuilds_workers(self):
        builder = FakeWorkerBuilder()
        self.install(builder=builder)
        self.write_lock(self.advance_upstream())
        self.install(builder=builder)
        retired = list((self.project / SPEC.cache).glob("retired-plugin-*"))
        self.assertEqual(len(retired), 1)
        self.assertTrue((retired[0] / "Binaries/Win64" / WORKERS[0]).is_file(), "build outputs move with the retired copy")
        self.assertIn("recorder one", (retired[0] / "Source/PresentationCapture/Private/Recorder.cpp").read_text())
        self.assertEqual(len(builder.calls), 2)

    def test_lock_must_name_this_plugin_and_a_full_revision(self):
        self.write_lock(self.revision, name="AnimationAnalysis")
        with self.assertRaisesRegex(ValueError, "PresentationCapture dependency must name a full, lowercase Git commit SHA"):
            read_lock(self.project)
        self.write_lock("main")
        with self.assertRaisesRegex(ValueError, "full, lowercase"):
            read_lock(self.project)

    def test_missing_checkout_names_this_setup_script(self):
        with self.assertRaisesRegex(ValueError, "Tools/PresentationCapture/setup_dependency.py"):
            checkout(self.project)

    def test_a_missing_repository_override_names_the_path_and_the_fix(self):
        missing = self.root / "not-on-this-machine"
        with self.assertRaises(ValueError) as raised:
            install(repository=missing, project=self.project, builder=FakeWorkerBuilder(), checker=lambda *_: {})
        message = str(raised.exception)
        for expected in (str(missing), "does not exist on this machine",
                         "Tools/PresentationCapture/setup_dependency.py --repository"):
            self.assertIn(expected, message)
        self.assertFalse((self.project / SPEC.cache / self.revision).exists())

    def test_an_scp_style_remote_is_cloned_rather_than_rejected_as_a_missing_path(self):
        transport, received = self.root / "fake_ssh.py", self.root / "ssh-request.json"
        transport.write_text(FAKE_SSH)
        remote = "git@fixture.invalid:owner/PresentationCapture.git"
        self.assertFalse(Path(remote).exists(), "premise: the remote is not a path on this machine")
        command = " ".join(f'"{Path(part).as_posix()}"' for part in (sys.executable, transport, self.upstream, received))
        with patch.dict(os.environ, {"GIT_SSH_COMMAND": command, "GIT_SSH_VARIANT": "simple"}):
            result = install(repository=remote, project=self.project, builder=FakeWorkerBuilder(), checker=lambda *_: {})
        self.assertEqual(result["revision"], self.revision)
        host, request = json.loads(received.read_text())
        self.assertEqual(host, "git@fixture.invalid", "git parsed the scp form and connected over ssh")
        self.assertIn("owner/PresentationCapture.git", request)
        self.assertEqual((self.plugin / "Source/PresentationCapture/Private/Recorder.cpp").read_text(), "// recorder one\n")

    def test_the_lock_pins_the_remote_like_animation_analysis(self):
        lock = read_lock()  # The project's own lock, not the fixture's.
        self.assertTrue(lock["repository"].startswith("https://"), "the source of truth is a clone URL")
        self.assertEqual(lock["revision"], "d7b8651e0baac29ff43ea4002edde76a3f2acef1")

    def test_a_repository_without_the_pin_reports_git_and_the_fix(self):
        self.write_lock("f" * 40)
        with self.assertRaises(ValueError) as raised:
            self.install(builder=FakeWorkerBuilder())
        message = str(raised.exception)
        self.assertIn("Could not fetch PresentationCapture", message)
        self.assertIn("fatal:", message, "git's own error is surfaced")
        self.assertIn("does not contain", message)

    def test_failed_encoder_check_publishes_no_install_record(self):
        def failing(plugin, scratch):
            raise ValueError("encoder self-check failed")
        with self.assertRaisesRegex(ValueError, "self-check failed"):
            self.install(builder=FakeWorkerBuilder(), checker=failing)
        self.assertFalse((self.project / SPEC.marker).exists())

    def test_failed_worker_build_publishes_no_install_record(self):
        def failing(plugin, evidence):
            raise ValueError("compiler unavailable")
        with self.assertRaisesRegex(ValueError, "compiler unavailable"):
            self.install(builder=failing)
        self.assertFalse((self.project / SPEC.marker).exists())


class RepositoryFormTests(unittest.TestCase):
    """Which --repository values git clones over a transport, and which name a path on this machine."""

    def test_clone_urls_and_scp_style_remotes_are_remote(self):
        for remote in ("git@github.com:owner/repo.git", "github.com:owner/repo.git", "ssh://git@github.com/owner/repo.git",
                       "https://github.com/owner/repo.git", "file:///D:/repositories/repo", "git+ssh://host/owner/repo.git"):
            self.assertTrue(pinned_plugin.is_remote(remote), remote)

    def test_drive_unc_and_relative_paths_are_local(self):
        # A drive letter, even drive-relative D:repo, is a path; so is anything with a separator before its
        # first colon, which git itself treats as a path (spell ./foo:bar to keep such a name local).
        for local in (r"C:\repositories\repo", "C:/repositories/repo", "D:repo", "./foo:bar", r".\foo:bar",
                      "relative/checkout", "checkout", r"\\server\share\repo", "//server/share/repo",
                      PureWindowsPath(r"C:\repositories\repo")):
            self.assertFalse(pinned_plugin.is_remote(local), local)


if __name__ == "__main__":
    unittest.main()
