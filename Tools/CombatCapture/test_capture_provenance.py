"""Project selection policy must include the portable analysis implementation."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import capture_format
from run_scenario import editor_binary_state, rhi_identity, source_state


class CaptureProvenanceTests(unittest.TestCase):
    def test_native_plugin_sources_and_binary_are_identity_inputs(self):
        with tempfile.TemporaryDirectory() as directory, patch("run_scenario.subprocess.check_output", return_value="revision"):
            root = Path(directory)
            plugin = root / "Plugins/AnimationAnalysis"
            code = plugin / "Source/AnimationCapture/Private/AnimationCaptureSession.cpp"
            code.parent.mkdir(parents=True)
            code.write_text("// producer one\n")
            descriptor = plugin / "AnimationAnalysis.uplugin"
            descriptor.write_text('{"FileVersion":3}')
            binary = plugin / "Binaries/Win64/UnrealEditor-AnimationCapture.dll"
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b"first producer binary")
            generated = plugin / "Intermediate/generated.cpp"
            generated.parent.mkdir()
            generated.write_text("// generated cache\n")
            shader = plugin / "Shaders/Private/Readback.usf"
            shader.parent.mkdir(parents=True)
            shader.write_text("// shader one\n")
            before = source_state(root)
            self.assertIn(descriptor.relative_to(root).as_posix(), before["files"])
            self.assertIn(shader.relative_to(root).as_posix(), before["files"])
            self.assertNotIn(generated.relative_to(root).as_posix(), before["files"])
            code.write_text("// producer two\n")
            self.assertNotEqual(before["identity"], source_state(root)["identity"])
            before_shader = source_state(root)
            shader.write_text("// shader two\n")
            self.assertNotEqual(before_shader["identity"], source_state(root)["identity"])
            binaries = editor_binary_state(root)
            self.assertIn(binary.relative_to(root).as_posix(), binaries)
            binary.write_bytes(b"second producer binary")
            self.assertNotEqual(binaries, editor_binary_state(root))

    def test_source_identity_tracks_resolved_dependency_and_integration_changes(self):
        with tempfile.TemporaryDirectory() as directory, patch("run_scenario.subprocess.check_output", return_value="revision"):
            root = Path(directory)
            package = root / "Tools/AnimationAnalysis"
            module = package / "animation_analysis_dependency.py"
            module.parent.mkdir(parents=True)
            module.write_text("value = 1\n")
            metadata = package / "dependency.json"
            metadata.write_text('{"revision":"fixture"}')
            shared = "Dependencies/AnimationAnalysis/Python/src/animation_analysis/metrics.py"
            with patch("run_scenario.dependency_source_manifest", return_value={shared: "first"}):
                before = source_state(root)
            self.assertIn(module.relative_to(root).as_posix(), before["files"])
            self.assertIn(metadata.relative_to(root).as_posix(), before["files"])
            self.assertIn(shared, before["files"])
            with patch("run_scenario.dependency_source_manifest", return_value={shared: "second"}):
                changed_source = source_state(root)
                self.assertNotEqual(before["identity"], changed_source["identity"])
                metadata.write_text('{"revision":"updated-fixture"}')
                self.assertNotEqual(changed_source["identity"], source_state(root)["identity"])

    def test_dependency_change_invalidates_offline_identity_without_entrypoint_change(self):
        entrypoint = Path(capture_format.__file__).parent / "evaluate_capture.py"
        with patch("capture_format.implementation_manifest", return_value={"metrics.py": "first"}):
            before = capture_format.implementation_identity(entrypoint)
        with patch("capture_format.implementation_manifest", return_value={"metrics.py": "second"}):
            self.assertNotEqual(before, capture_format.implementation_identity(entrypoint))

    def test_rhi_identity_is_read_from_the_editor_log(self):
        log = "\n".join(["[0]LogRHI: Using Forced RHI: D3D12", "[0]LogRHI: Using Highest Feature Level of D3D12: SM6"])
        self.assertEqual(rhi_identity(log), dict(rhi="D3D12", selection="forced", feature_level="SM6"))
        self.assertEqual(rhi_identity("no renderer")["rhi"], None)

    def test_video_and_review_outputs_never_change_the_bundle_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "session.json").write_text('{"schema_version":2}')
            (root / "samples.jsonl").write_text('{"index":1}\n')
            before = capture_format.bundle_identity(root)
            clip = root / "video/CAPTURE"
            clip.mkdir(parents=True)
            (clip / "single-frames.csv").write_text("videoSample\n0\n")  # The recorder may still be finalizing.
            review = root / "video-review"
            review.mkdir()
            (review / "contact-sheet.png").write_bytes(b"written after evaluation")
            self.assertEqual(before, capture_format.bundle_identity(root))
            (root / "samples.jsonl").write_text('{"index":2}\n')
            self.assertNotEqual(before, capture_format.bundle_identity(root))

    def test_bundle_selection_excludes_derived_reports_but_includes_scenario_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "scenario.json").write_text('{"schema_version":1}')
            before = capture_format.bundle_identity(root)
            (root / "evaluation.json").write_text('{"status":"pass"}')
            self.assertEqual(before, capture_format.bundle_identity(root))
            (root / "scenario.json").write_text('{"schema_version":2}')
            self.assertNotEqual(before, capture_format.bundle_identity(root))
