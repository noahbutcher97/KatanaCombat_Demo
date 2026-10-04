"""The capture worktree script only moves worktrees it created, only when clean, and never deletes."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from capture_worktree import (MARKER, WorktreeError, build_command, default_path, ensure_worktree, git, parse_arguments,
                              setup_commands)


class CaptureWorktreeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="capture-worktree-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.repo = self.root / "Project"
        self.repo.mkdir()
        subprocess.run(["git", "init", str(self.repo)], check=True, capture_output=True)
        git(self.repo, "config", "core.autocrlf", "false")
        (self.repo / ".gitignore").write_text("Saved/\nPlugins/*\n")
        (self.repo / "Source.cpp").write_text("// one\n")
        self.first = self.commit("First")
        (self.repo / "Source.cpp").write_text("// two\n")
        self.second = self.commit("Second")
        self.target = self.root / "Project-capture-run"

    def commit(self, message):
        git(self.repo, "add", "--", ".")
        git(self.repo, "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "commit", "-m", message)
        return git(self.repo, "rev-parse", "HEAD")

    def test_creates_a_detached_worktree_at_the_commit_and_records_ownership(self):
        record = ensure_worktree(self.repo, self.first, self.target)
        self.assertEqual(record["action"], "created")
        self.assertEqual(git(self.target, "rev-parse", "HEAD"), self.first)
        self.assertEqual(git(self.target, "branch", "--show-current"), "", "a capture worktree never holds a branch")
        marker = json.loads((self.target / MARKER).read_text(encoding="utf-8"))
        self.assertEqual(marker["created_by"], "Tools/CombatCapture/capture_worktree.py")
        self.assertEqual(git(self.target, "status", "--porcelain"), "", "the ownership record is ignored by Git")

    def test_updates_its_own_clean_worktree_to_another_ref(self):
        ensure_worktree(self.repo, self.first, self.target)
        record = ensure_worktree(self.repo, "HEAD", self.target)
        self.assertEqual((record["action"], record["commit"]), ("updated", self.second))
        self.assertEqual((self.target / "Source.cpp").read_text(), "// two\n")
        history = json.loads((self.target / MARKER).read_text(encoding="utf-8"))["history"]
        self.assertEqual([entry["commit"] for entry in history], [self.first, self.second])
        self.assertEqual(ensure_worktree(self.repo, self.second, self.target)["action"], "unchanged")

    def test_refuses_tracked_changes_and_keeps_them(self):
        ensure_worktree(self.repo, self.first, self.target)
        (self.target / "Source.cpp").write_text("// in progress\n")
        with self.assertRaisesRegex(WorktreeError, "uncommitted changes"):
            ensure_worktree(self.repo, self.second, self.target)
        self.assertEqual(git(self.target, "rev-parse", "HEAD"), self.first)
        self.assertEqual((self.target / "Source.cpp").read_text(), "// in progress\n")

    def test_refuses_untracked_files(self):
        ensure_worktree(self.repo, self.first, self.target)
        (self.target / "Notes.txt").write_text("unsaved work\n")
        with self.assertRaisesRegex(WorktreeError, "uncommitted changes"):
            ensure_worktree(self.repo, self.second, self.target)
        self.assertTrue((self.target / "Notes.txt").is_file())

    def test_refuses_to_orphan_commits_made_in_the_capture_worktree(self):
        ensure_worktree(self.repo, self.first, self.target)
        (self.target / "Source.cpp").write_text("// quick fix during a capture\n")
        git(self.target, "add", "--", "Source.cpp")
        git(self.target, "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "commit", "-m", "Local")
        local = git(self.target, "rev-parse", "HEAD")
        with self.assertRaisesRegex(WorktreeError, "no branch or tag contains it"):
            ensure_worktree(self.repo, self.second, self.target)
        self.assertEqual(git(self.target, "rev-parse", "HEAD"), local)
        git(self.repo, "branch", "kept-capture-fix", local)
        self.assertEqual(ensure_worktree(self.repo, self.second, self.target)["action"], "updated",
                         "once a branch holds the commit, moving away loses nothing")

    def test_refuses_a_worktree_it_did_not_create(self):
        git(self.repo, "worktree", "add", "--detach", str(self.target), self.first)
        with self.assertRaisesRegex(WorktreeError, "not created by capture_worktree.py"):
            ensure_worktree(self.repo, self.second, self.target)
        self.assertEqual(git(self.target, "rev-parse", "HEAD"), self.first)

    def test_refuses_an_ordinary_directory_and_the_main_checkout(self):
        self.target.mkdir()
        (self.target / "keep.txt").write_text("not ours\n")
        with self.assertRaisesRegex(WorktreeError, "not a worktree"):
            ensure_worktree(self.repo, self.first, self.target)
        self.assertTrue((self.target / "keep.txt").is_file())
        with self.assertRaisesRegex(WorktreeError, "working checkout"):
            ensure_worktree(self.repo, self.first, self.repo)

    def test_default_path_sits_beside_the_main_checkout(self):
        self.assertEqual(default_path(self.repo), self.repo.parent / "Project-capture-run")

    def test_setup_runs_only_dependencies_present_at_the_target_and_reuses_a_local_pin(self):
        ensure_worktree(self.repo, self.first, self.target)
        self.assertEqual(setup_commands(self.repo, self.target), [], "a revision without pins needs no setup")
        for name in ("AnimationAnalysis", "PresentationCapture"):
            (self.target / "Tools" / name).mkdir(parents=True)
            (self.target / "Tools" / name / "setup_dependency.py").write_text("")
        revision = "a" * 40
        (self.target / "Tools/AnimationAnalysis/dependency.json").write_text(json.dumps(dict(revision=revision)))
        local = self.repo / "Saved/AnalysisDependencies" / revision / ".git"
        local.mkdir(parents=True)
        commands = setup_commands(self.repo, self.target)
        self.assertEqual(len(commands), 2)
        self.assertEqual(commands[0][0], sys.executable)
        self.assertEqual(commands[0][-2:], ["--repository", str(local.parent)])
        self.assertTrue(commands[1][1].endswith("setup_dependency.py") and "PresentationCapture" in commands[1][1])

    def test_repository_overrides_keep_clone_urls_intact_and_anchor_local_paths(self):
        ensure_worktree(self.repo, self.first, self.target)
        for name in ("AnimationAnalysis", "PresentationCapture"):
            (self.target / "Tools" / name).mkdir(parents=True)
            (self.target / "Tools" / name / "setup_dependency.py").write_text("")
        for remote in ("https://github.com/owner/PresentationCapture.git", "git@github.com:owner/PresentationCapture.git",
                       "ssh://git@github.com/owner/PresentationCapture.git", "file:///D:/repositories/PresentationCapture"):
            args = parse_arguments(["--analysis-repository", remote, "--recorder-repository", remote])
            commands = setup_commands(self.repo, self.target, args.analysis_repository, args.recorder_repository)
            self.assertEqual([command[-2:] for command in commands], [["--repository", remote]] * 2,
                             "a clone URL reaches setup byte for byte")
        # Setup runs with the worktree as its directory, so a relative checkout is anchored where it was named.
        local = parse_arguments(["--recorder-repository", "relative/PresentationCapture"]).recorder_repository
        self.assertEqual(local, str(Path("relative/PresentationCapture").resolve()))

    def test_build_targets_the_worktree_project_and_waits_for_the_shared_mutex(self):
        command = build_command(self.target, parallel=2)
        self.assertIn(f"-Project={self.target / 'KatanaCombat.uproject'}", command)
        self.assertIn("-WaitMutex", command)
        self.assertIn("-MaxParallelActions=2", command)


if __name__ == "__main__":
    unittest.main()
