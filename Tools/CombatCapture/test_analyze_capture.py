import csv
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

from analyze_capture import CaptureError, analyze, compare, write_report


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


class CaptureAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.manifest = dict(schema_version=1, status="complete", scenario="Movement", map="/Test/Map",
            world="/Test/UEDPIE_0_Map", render_available=True, sample_hz=2, frame_hz=2,
            sample_count=3, frame_count=1, marker_count=0, errors=[], participants=[dict(
                role="Player", points=["pelvis"], action_records=2, defense_records=0,
                telemetry_lost_records=0, telemetry_resets=0)])
        self.samples = [dict(index=i+1, simulation_time_s=i*.5, wall_elapsed_s=i*.6, marker="moving", actors=[dict(
            role="Player", valid=True, position_cm=[i*3, 0, 0], recently_rendered=True,
            points=dict(pelvis=dict(world_cm=[i*3, 0, 100], component_cm=[0, 0, 100+i])))]) for i in range(3)]
        self.frames = [dict(index=1, simulation_time_s=0.5, wall_elapsed_s=0.6, marker="moving",
            sample_index=2, pie_draw_index=5, file="frame.png", capture_world=self.manifest["world"],
            source="PIEGameViewportAfterDraw", width=2, height=2, nontrivial_pixels=True)]
        png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 8, 2, 0, 0, 0))
        png += chunk(b"IDAT", zlib.compress(b"\0\xff\0\0\0\xff\0" * 2)) + chunk(b"IEND", b"")
        (self.root / "frame.png").write_bytes(png)
        self.write()
        with (self.root / "Player.actions.csv").open("w", newline="") as file:
            writer = csv.writer(file)
            writer.writerow(["schema_version", "sequence", "event", "input_serial", "simulation_timestamp"])
            writer.writerow([1, 1, "InputCaptured", 4, 0.2])
            writer.writerow([1, 2, "ActionExecutionStarted", 4, 0.35])
        (self.root / "Player.defense.csv").write_text("schema_version,event\n")

    def write(self):
        (self.root / "session.json").write_text(json.dumps(self.manifest))
        for name, rows in (("samples", self.samples), ("frames", self.frames), ("markers", [])):
            (self.root / f"{name}.jsonl").write_text("".join(json.dumps(row)+"\n" for row in rows))

    def test_known_motion_distance_latency_and_frame_link(self):
        result, frames, markers = analyze(self.root, [["Player:pelvis", "Player:pelvis"]])
        self.assertEqual(result["data_integrity"], "valid")
        self.assertEqual(result["motion"]["Player"]["sampled_path_length_cm"], 6)
        self.assertEqual(result["motion"]["Player"]["actor_speed_cm_s"]["max"], 6)
        self.assertEqual(result["motion"]["Player"]["point_component_step_cm"]["pelvis"]["max"], 1)
        self.assertAlmostEqual(result["input_timing"]["Player"]["input_to_action_ms"]["max"], 150)
        self.assertEqual(result["point_distances"]["Player:pelvis -> Player:pelvis"]["distance_cm"]["min"], 0)
        self.assertEqual(result["motion"]["Player"]["largest_actor_step_frame"]["file"], "frame.png")
        write_report(self.root, result, frames, markers)
        self.assertTrue((self.root / "report.html").exists())

    def test_missing_and_headless_data_never_become_zero_measurements(self):
        self.manifest.update(render_available=False, frame_count=0)
        self.frames = []
        for row in self.samples:
            row["actors"][0]["points"]["pelvis"] = None
        self.write()
        result, _, _ = analyze(self.root, [["Player:pelvis", "Player:pelvis"]])
        distance = result["point_distances"]["Player:pelvis -> Player:pelvis"]
        self.assertIsNone(distance["distance_cm"]["min"])
        self.assertEqual(distance["missing_samples"], 3)
        self.assertTrue(any("Headless" in note for note in result["notes"]))

    def test_corrupt_image_and_wrong_world_fail_integrity(self):
        (self.root / "frame.png").write_bytes(b"not a PNG")
        self.frames[0]["capture_world"] = "EditorWorld"
        self.write()
        result, _, _ = analyze(self.root)
        self.assertEqual(result["data_integrity"], "incomplete")
        self.assertTrue(any("provenance" in issue for issue in result["issues"]))
        self.assertTrue(any("Invalid PNG" in issue for issue in result["issues"]))

    def test_incomplete_session_duplicate_time_and_missing_csv_fail(self):
        self.manifest["status"] = "recording"
        self.samples[1]["simulation_time_s"] = 0
        self.write()
        (self.root / "Player.actions.csv").unlink()
        result, _, _ = analyze(self.root)
        self.assertEqual(result["data_integrity"], "incomplete")
        self.assertGreaterEqual(len(result["issues"]), 3)

    def test_path_escape_and_nonfinite_positions_rejected(self):
        self.frames[0]["file"] = "../outside.png"
        self.write()
        result, _, _ = analyze(self.root)
        self.assertTrue(any("escapes" in issue for issue in result["issues"]))
        self.samples[0]["actors"][0]["position_cm"][0] = float("nan")
        self.write()
        with self.assertRaises(CaptureError):
            analyze(self.root)

    def test_baseline_compatibility_and_known_delta(self):
        baseline, _, _ = analyze(self.root)
        self.samples[2]["actors"][0]["position_cm"][0] = 10
        self.write()
        current, _, _ = analyze(self.root)
        delta = compare(current, baseline)
        self.assertEqual(delta["metrics"]["motion.Player.sampled_path_length_cm"]["delta"], 4)
        self.manifest["map"] = "/Another/Map"
        self.write()
        incompatible, _, _ = analyze(self.root)
        with self.assertRaises(CaptureError):
            compare(incompatible, baseline)

    def test_telemetry_loss_rejects_comparison(self):
        baseline, _, _ = analyze(self.root)
        self.manifest["participants"][0]["telemetry_lost_records"] = 2
        self.write()
        current, _, _ = analyze(self.root)
        with self.assertRaises(CaptureError):
            compare(current, baseline)

    def test_readback_mode_and_diagnostic_policy_are_comparison_inputs(self):
        baseline, _, _ = analyze(self.root)
        self.assertEqual(baseline["compatibility"]["readback_mode"], "synchronous")
        self.manifest.update(readback_mode="asynchronous", readback_diagnostic_resolution=False)
        self.write()
        asynchronous, _, _ = analyze(self.root)
        self.assertTrue(any("enqueue cost" in note for note in asynchronous["notes"]))
        with self.assertRaisesRegex(CaptureError, "incompatible"):
            compare(asynchronous, baseline)
        self.manifest["readback_diagnostic_resolution"] = True
        self.write()
        diagnostic, _, _ = analyze(self.root)
        with self.assertRaisesRegex(CaptureError, "incompatible"):
            compare(diagnostic, asynchronous)

    def test_unknown_readback_policy_is_rejected(self):
        self.manifest["readback_mode"] = "automatic"
        self.write()
        with self.assertRaisesRegex(CaptureError, "readback mode"):
            analyze(self.root)
        self.manifest.update(readback_mode="synchronous", readback_diagnostic_resolution=True)
        self.write()
        with self.assertRaisesRegex(CaptureError, "diagnostic-resolution"):
            analyze(self.root)

    def test_report_escapes_marker_script(self):
        self.frames[0]["marker"] = "</script><script>alert(1)</script>"
        self.write()
        result, frames, markers = analyze(self.root)
        write_report(self.root, result, frames, markers)
        self.assertNotIn("</script><script>alert", (self.root / "report.html").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
