import base64
from dataclasses import asdict
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest

from calibrate_segment_alignment import calibrate
from detect_segment_alignment import publish
from visual_analysis.pixel_alignment import SegmentAlignmentSettings


class SegmentCalibrationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        profile = dict(schema_version=1, detector="colored_segment_alignment", detector_version="1",
                       image_size=[120, 100], settings=asdict(SegmentAlignmentSettings()))
        self.profile = self.root / "profile.json"
        self.profile.write_text(json.dumps(profile), encoding="utf-8")
        datasets = []
        from PIL import Image
        for cohort, background in (("calibration", 0), ("validation", 15)):
            frames = []
            for index, segment in enumerate(([[20, 40], [90, 40]], [[45, 15], [45, 85]])):
                pixels = bytearray([background] * (120*100*3))
                for t in range(71):
                    x, y = (20+t, 40) if index == 0 else (45, 15+t)
                    for offset in (-1, 0, 1):
                        px, py = (x, y+offset) if index == 0 else (x+offset, y)
                        start = (py*120+px)*3
                        pixels[start:start+3] = bytes((10, 170, 180))
                stream = io.BytesIO()
                Image.frombytes("RGB", (120, 100), bytes(pixels)).save(stream, format="PNG")
                png = stream.getvalue()
                frames.append(dict(file=f"frame_{index}.png", simulation_time_s=index, width=120, height=100, segment=segment,
                                   image_sha256=hashlib.sha256(png).hexdigest(), image="data:image/png;base64,"+base64.b64encode(png).decode()))
            page = self.root / (cohort+".html")
            page.write_text('<script id="visual-evidence" type="application/json">'+json.dumps(dict(frames=frames))+'</script>', encoding="utf-8")
            regions = dict(schema_version=1, evidence_sha256=hashlib.sha256(page.read_bytes()).hexdigest(), reviewer="Synthetic control",
                           frames=[dict(file="frame_0.png", region_px=[5, 5, 110, 95], visibility="visible", expected_assessment="consistent",
                                        stale_reference_frame="frame_1.png", basis="Known rasterized segment and distinct later orientation")])
            region_path = self.root / (cohort+"-regions.json")
            region_path.write_text(json.dumps(regions), encoding="utf-8")
            datasets.append(dict(evidence=page.name, regions=region_path.name, cohort=cohort))
        self.manifest = self.root / "manifest.json"
        self.manifest.write_text(json.dumps(dict(schema_version=1, basis="Synthetic instrument controls", datasets=datasets)), encoding="utf-8")
        self.calibration = self.root / "calibration.json"

    def test_calibration_and_detector_publish_with_exact_profile(self):
        report = calibrate(self.manifest, self.profile, self.calibration)
        self.assertEqual(report["status"], "controls_passed")
        self.assertEqual(report["cases"], 18)
        result = publish(self.root/"validation.html", self.root/"validation-regions.json", self.profile, self.calibration, self.root/"result.html")
        self.assertEqual(result["assessments"]["consistent"], 1)

    def test_changed_detector_identity_and_resolution_reject(self):
        report = calibrate(self.manifest, self.profile, self.calibration)
        report["detector_identity"] = "stale implementation"
        self.calibration.write_text(json.dumps(report), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "instrument controls"):
            publish(self.root/"validation.html", self.root/"validation-regions.json", self.profile, self.calibration, self.root/"result.html")
        profile = json.loads(self.profile.read_text())
        profile["image_size"] = [960, 540]
        self.profile.write_text(json.dumps(profile), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "outside this detector profile"):
            calibrate(self.manifest, self.profile, self.calibration)

    def test_duplicate_evidence_cannot_count_as_validation(self):
        manifest = json.loads(self.manifest.read_text())
        manifest["datasets"][1].update(evidence="calibration.html", regions="calibration-regions.json")
        self.manifest.write_text(json.dumps(manifest), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "counted twice"):
            calibrate(self.manifest, self.profile, self.calibration)

    def test_output_input_collision_preserves_reviewed_regions(self):
        regions = self.root/"validation-regions.json"
        before = regions.read_bytes()
        with self.assertRaisesRegex(ValueError, "overwrite dataset inputs"):
            calibrate(self.manifest, self.profile, regions)
        self.assertEqual(regions.read_bytes(), before)

    def test_wrong_label_prevents_calibration_promotion(self):
        regions = self.root/"validation-regions.json"
        data = json.loads(regions.read_text())
        data["frames"][0]["expected_assessment"] = "indeterminate"
        regions.write_text(json.dumps(data), encoding="utf-8")
        report = calibrate(self.manifest, self.profile, self.calibration)
        self.assertEqual(report["status"], "controls_failed")
        self.assertGreater(report["mismatches"], 0)


if __name__ == "__main__":
    unittest.main()
