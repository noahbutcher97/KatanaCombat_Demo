"""Offline checks for clip validation, the engine-frame join and the clip-quality gate."""
import csv
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import video_capture
from video_capture import QUALITY_THRESHOLDS, VideoError, analyze, load_clip

CAPTURE = "6454895A4427F9535EA4C1B20D63397F"
WORLD = "AC388FF84F98003CC01E8EAB9FC7C241"


class VideoBundle:
    """A finalized bundle: samples at engine frames 100..109 and a 10-frame clip drawn on the same frames."""

    def __init__(self, root, frames=10, fps=60.0):
        self.root, self.frames, self.fps = Path(root), frames, fps
        self.video = self.root / "video" / CAPTURE
        self.video.mkdir(parents=True)
        self.draws = [100 + i for i in range(frames)]
        self.pts = [round(0.014 + i / fps, 6) for i in range(frames)]
        self.samples = [dict(index=i + 1, engine_frame=frame, simulation_time_s=i / fps, wall_elapsed_s=0.2 + i / fps, actors=[])
                        for i, frame in enumerate(self.draws)]
        self.markers = [dict(index=1, marker="video_started", engine_frame=100, simulation_time_s=0, wall_elapsed_s=0.2,
                             payload=dict(capture_id=CAPTURE, platform_seconds=1000.2, game_frame=100)),
                        dict(index=2, marker="contact", engine_frame=103, simulation_time_s=0.05, wall_elapsed_s=0.25,
                             payload=dict(stage="contact", hit="committed-1", outcome="Hit", source="committed_contact",
                                          attacker="Attacker", victim="Victim"))]
        self.link = dict(schema_version=1, kind="katana_combat_capture_link", status="stopped",
                         analysis=dict(directory=".", start=dict(engine_frame=100, platform_seconds=1000.0, world_time_s=0.0)),
                         video=dict(started=True, capture_id=CAPTURE, world_id=WORLD, directory=f"video/{CAPTURE}",
                                    requested=dict(fps=60, resolution=720), viewport_widget_px=[1280, 720]))
        self.window = dict(seat="single", worldId=WORLD, output="single.mp4", timestamps="single-frames.csv", complete=True,
                           requests=frames, encodedFrames=frames, submittedFrames=frames, droppedFrames=0, pressureSkippedDraws=0,
                           maxAcquisitionGapSeconds=1 / fps, achievedAcquisitionFPS=fps, width=1280, height=720,
                           viewportWidth=1280, viewportHeight=720, cadenceAssessment="average-within-2-percent")
        self.manifest = dict(schemaVersion=2, captureId=CAPTURE, requestedFPS=60, complete=True, failureReason="",
                             captureEpochPlatformSeconds=1000.21, finalizeSeconds=0.1, windows=[self.window])
        self.telemetry = dict(clockId="C", worldFrames=[dict(gameFrame=frame, platformSeconds=1000.0 + 0.2 + i / fps + 0.00005)
                                                       for i, frame in enumerate(self.draws)])
        self.stream = dict(codec_name="h264", profile="Constrained Baseline", pix_fmt="yuv420p", width=1280, height=720,
                           nb_read_frames=str(frames), avg_frame_rate="60/1")

    def write(self):
        (self.root / "session.json").write_text(json.dumps(dict(schema_version=2, status="complete", world="/Game/Map")))
        (self.root / "capture-link.json").write_text(json.dumps(self.link))
        for name, rows in (("samples.jsonl", self.samples), ("markers.jsonl", self.markers)):
            (self.root / name).write_text("".join(json.dumps(row) + "\n" for row in rows))
        (self.video / "video-manifest.json").write_text(json.dumps(self.manifest))
        (self.video / "manifest.json").write_text(json.dumps(self.telemetry))
        (self.video / "single.mp4").write_bytes(b"not decoded by the fake probe")
        with open(self.video / "single-frames.csv", "w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(["videoSample", "requestedGameFrame", "drawGameFrame", "acquisitionPlatformSeconds", "expectedPTSSeconds"])
            for i, (draw, pts) in enumerate(zip(self.draws, self.pts)):
                writer.writerow([i, draw, draw, f"{1000.21 + pts:.9f}", f"{pts:.6f}"])
        return self

    def probe(self, pts=None):
        return dict(stream=dict(self.stream), format=dict(duration=str(self.pts[-1]), size="1000"), pts=list(pts or self.pts))


class VideoCaptureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="video-capture-test-")
        self.addCleanup(self.temporary.cleanup)
        self.bundle = VideoBundle(self.temporary.name)

    def run_analysis(self, pts=None):
        self.bundle.write()
        with patch("video_capture.probe", return_value=self.bundle.probe(pts)):
            return analyze(self.bundle.root, extract=False)

    def test_clean_clip_is_ok_native_and_joins_every_frame(self):
        result = self.run_analysis()
        self.assertEqual(result["video_quality"], "ok", result["quality"])
        self.assertTrue(result["validation"]["valid"])
        self.assertTrue(result["resolution"]["native"])
        self.assertEqual(result["join"]["exact_engine_frame_matches"], 10)
        self.assertEqual(result["join"]["exact_match_rate"], 1.0)
        self.assertEqual(result["frames"][3]["sample_index"], 4, "video frame 3 joins sample 4, both engine frame 103")
        self.assertAlmostEqual(result["join"]["clock_cross_check_ms"]["median"], 0.05, places=3)
        contact = next(m for m in result["markers"] if m["marker"] == "contact")
        self.assertEqual((contact["video_sample"], contact["frames_after_marker"], contact["outcome"]), (3, 0, "Hit"))
        self.assertTrue((self.bundle.root / "video-analysis.json").is_file())
        self.assertIn("Clip quality: <b>ok</b>", (self.bundle.root / "video-review.html").read_text(encoding="utf-8"))

    def test_pressure_skips_above_the_threshold_degrade_the_clip(self):
        self.bundle.window.update(pressureSkippedDraws=2, requests=8, encodedFrames=10)
        result = self.run_analysis()
        self.assertEqual(result["video_quality"], "degraded")
        self.assertTrue(any("max_pressure_skip_fraction" in reason for reason in result["quality"]["reasons"]))

    def test_one_skip_in_a_long_clip_stays_within_the_threshold(self):
        self.bundle.window.update(pressureSkippedDraws=1, requests=270)
        self.assertEqual(self.run_analysis()["video_quality"], "ok")

    def test_a_long_frame_gap_degrades_the_clip(self):
        self.bundle.window["maxAcquisitionGapSeconds"] = 0.215
        result = self.run_analysis()
        self.assertEqual(result["video_quality"], "degraded")
        self.assertTrue(any("max_frame_gap_s" in reason for reason in result["quality"]["reasons"]))

    def test_dropped_frames_degrade_even_a_complete_clip(self):
        self.bundle.window["droppedFrames"] = 1
        self.assertEqual(self.run_analysis()["video_quality"], "degraded")

    def test_thresholds_are_named_and_recorded(self):
        result = self.run_analysis()
        self.assertEqual(set(result["quality"]["thresholds"]), {"max_dropped_frames", "max_pressure_skip_fraction", "max_frame_gap_s"})
        self.assertEqual(result["quality"]["thresholds"], QUALITY_THRESHOLDS)

    def test_incomplete_recording_is_invalid(self):
        self.bundle.manifest.update(complete=False, failureReason="encoder failed")
        result = self.run_analysis()
        self.assertEqual(result["video_quality"], "invalid")
        self.assertTrue(any("encoder failed" in reason for reason in result["quality"]["reasons"]))

    def test_decoded_pts_must_match_the_frames_csv(self):
        shifted = list(self.bundle.pts)
        shifted[5] += 10e-6
        result = self.run_analysis(pts=shifted)
        self.assertEqual(result["video_quality"], "invalid")
        self.assertAlmostEqual(result["validation"]["max_pts_deviation_us"], 10.0, places=1)

    def test_decoded_frame_count_must_match_the_recorder(self):
        result = self.run_analysis(pts=self.bundle.pts[:-1])
        self.assertEqual(result["video_quality"], "invalid")
        self.assertTrue(any("Frame counts disagree" in issue for issue in result["validation"]["issues"]))

    def test_unsampled_frames_report_the_nearest_earlier_sample(self):
        del self.bundle.samples[3]  # engine frame 103
        result = self.run_analysis()
        self.assertEqual(result["join"]["exact_engine_frame_matches"], 9)
        self.assertEqual(result["join"]["nearest_earlier_lag_frames"], {"0": 9, "1": 1})
        self.assertIsNone(result["frames"][3]["sample_index"])
        self.assertEqual(result["frames"][3]["nearest_earlier_sample_index"], 3)

    def test_a_scaled_viewport_is_reported_as_not_native(self):
        self.bundle.link["video"]["viewport_widget_px"] = [759, 378]
        self.bundle.window.update(viewportWidth=759, viewportHeight=378, width=1280, height=636)
        self.bundle.stream.update(width=1280, height=636)
        result = self.run_analysis()
        self.assertFalse(result["resolution"]["native"])
        self.assertEqual(result["video_quality"], "ok", "scaling is reported separately from the quality gate")

    def test_an_unfinalized_clip_is_explained(self):
        self.bundle.write()
        (self.bundle.video / "video-manifest.json").unlink()
        with self.assertRaisesRegex(VideoError, "not finalized"):
            load_clip(self.bundle.root)

    def test_a_bundle_without_video_is_explained(self):
        with self.assertRaisesRegex(VideoError, "not recorded with video"):
            load_clip(self.bundle.root)

    def test_missing_ffmpeg_tools_fail_with_an_actionable_message(self):
        with patch("video_capture.shutil.which", return_value=None):
            with self.assertRaisesRegex(VideoError, "ffprobe was not found on PATH. Install FFmpeg"):
                video_capture.find_tool("ffprobe")

    def test_runner_turns_an_unreadable_clip_into_an_invalid_verdict(self):
        from run_scenario import video_report
        report = video_report(self.bundle.root)
        self.assertEqual(report["video_quality"], "invalid")
        self.assertIn("capture-link.json", report["video"]["error"])


@unittest.skipUnless(shutil.which("ffmpeg") and shutil.which("ffprobe"), "ffmpeg and ffprobe are not on PATH")
class RealDecoderTests(unittest.TestCase):
    """A real H.264 file through ffprobe and ffmpeg, as produced for a recorder clip."""

    def test_probe_extraction_and_contact_sheet(self):
        with tempfile.TemporaryDirectory(prefix="video-capture-decoder-") as directory:
            bundle = VideoBundle(directory, frames=10, fps=30.0)
            bundle.pts = [round(i / 30.0, 6) for i in range(10)]
            bundle.window.update(width=320, height=180, viewportWidth=320, viewportHeight=180)
            bundle.link["video"]["viewport_widget_px"] = [320, 180]
            bundle.write()
            encoded = subprocess.run([shutil.which("ffmpeg"), "-v", "error", "-y", "-f", "lavfi", "-i", "testsrc=size=320x180:rate=30",
                                      "-frames:v", "10", "-c:v", "libx264", "-bf", "0", "-pix_fmt", "yuv420p",
                                      str(bundle.video / "single.mp4")], capture_output=True, text=True)
            if encoded.returncode:
                self.skipTest("this ffmpeg build cannot encode H.264: " + encoded.stderr.strip()[-200:])
            result = analyze(bundle.root, extract=True, picks={"first": 0, "last": 9})
            self.assertTrue(result["validation"]["valid"], result["validation"]["issues"])
            self.assertEqual(result["validation"]["decoded_frames"], 10)
            self.assertEqual((result["validation"]["ffprobe"]["width"], result["validation"]["ffprobe"]["height"]), (320, 180))
            frames = result["review_frames"]["frames"]
            self.assertEqual([f["frame_index"] for f in frames], [0, 9])
            self.assertTrue(all(f["distinct_colors"] > 16 for f in frames), "test pattern frames are not blank")
            self.assertEqual(result["review_frames"]["contact_sheet"], "contact-sheet.png")
            self.assertTrue((bundle.root / "video-review" / "contact-sheet.png").is_file())


if __name__ == "__main__":
    unittest.main()
