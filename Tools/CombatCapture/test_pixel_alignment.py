from dataclasses import replace
import unittest

from visual_analysis.pixel_alignment import analyze_segment, SegmentAlignmentSettings


class PixelAlignmentTests(unittest.TestCase):
    def setUp(self):
        self.width, self.height = 120, 100
        self.rgb = bytearray(self.width*self.height*3)
        self.settings = SegmentAlignmentSettings()
        self.region = [5, 5, 110, 95]
        for x in range(20, 91):
            for y in range(39, 42):
                index = (y*self.width+x)*3
                self.rgb[index:index+3] = bytes((10, 170, 180))

    def measure(self, segment=None, visibility="visible"):
        return analyze_segment(self.rgb, self.width, self.height, self.region,
                               segment or [[20, 40], [90, 40]], visibility, self.settings)

    def test_matching_pixels_are_measured_independently(self):
        result = self.measure()
        self.assertEqual(result["assessment"], "consistent")
        self.assertAlmostEqual(result["measurements"]["maximum_perpendicular_error_px"], 0)

    def test_transverse_and_longitudinal_telemetry_offsets_are_flagged(self):
        for segment in ([[20, 48], [90, 48]], [[75, 40], [145, 40]]):
            with self.subTest(segment=segment):
                self.assertEqual(self.measure(segment)["assessment"], "concern")

    def test_wrong_orientation_is_flagged(self):
        self.assertEqual(self.measure([[45, 15], [45, 85]])["assessment"], "concern")

    def test_unknown_and_occluded_visibility_abstain_despite_good_geometry(self):
        for visibility in ("unknown", "occluded"):
            self.assertEqual(self.measure(visibility=visibility)["assessment"], "indeterminate")

    def test_second_same_color_segment_abstains(self):
        for x in range(20, 91):
            for y in range(65, 68):
                index = (y*self.width+x)*3
                self.rgb[index:index+3] = bytes((10, 170, 180))
        self.assertIn("Competing", self.measure()["reason"])

    def test_blank_image_is_unmeasurable(self):
        self.rgb = bytes(len(self.rgb))
        self.assertEqual(self.measure()["assessment"], "indeterminate")

    def test_region_clipping_and_invalid_settings_are_rejected(self):
        self.region = [20, 5, 110, 95]
        self.assertIn("boundary", self.measure()["reason"])
        self.settings = replace(self.settings, maximum_alignment_error_px=float("nan"))
        with self.assertRaises(ValueError):
            self.measure()


if __name__ == "__main__":
    unittest.main()
