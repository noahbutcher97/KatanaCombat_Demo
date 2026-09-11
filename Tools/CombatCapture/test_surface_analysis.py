import math
import random
import unittest
from visual_analysis.surfaces import measure_surface_relation, minimum_pixel_distance


class SurfaceRelationTests(unittest.TestCase):
    def test_distance_matches_brute_force_across_unrelated_rasters(self):
        rng=random.Random(987)
        for width,height in ((7,9),(21,5),(4,3)):
            for _ in range(10):
                a,b=rng.sample(range(width*height),3),rng.sample(range(width*height),4)
                first=[i in a for i in range(width*height)]
                second=[i in b for i in range(width*height)]
                expected=min(math.hypot(i%width-j%width,i//width-j//width) for i in a for j in b)
                self.assertAlmostEqual(minimum_pixel_distance(first,second,width,height),expected)

    def test_separation_uses_declared_ids_without_domain_names(self):
        labels=bytes([0,41,0,0,201,0])
        result=measure_surface_relation(labels,[50]*6,[50]*6,6,1,41,201,.25)
        self.assertEqual(result['status'],'measured')
        self.assertEqual(result['minimum_visible_pixel_center_distance_px'],3)

    def test_adjacent_pixels_do_not_become_contact_approval(self):
        result=measure_surface_relation(bytes([4,9]),[60,60],[60,60],2,1,4,9,.1)
        self.assertEqual(result['minimum_visible_pixel_center_distance_px'],1)
        self.assertNotIn('contact',result)

    def test_scene_occlusion_is_separate_from_missing_label(self):
        occluded=measure_surface_relation(bytes([4,9]),[60,30],[60,80],2,1,4,9,.1)
        missing=measure_surface_relation(bytes([4,0]),[60,math.inf],[60,math.inf],2,1,4,9,.1)
        self.assertEqual(occluded['status'],'indeterminate')
        self.assertEqual(occluded['subjects'][1]['scene_occluded_pixels'],1)
        self.assertEqual(missing['subjects'][1]['observed_label_pixels'],0)
        self.assertIsNone(missing['subjects'][1]['observed_label_visible_fraction'])

    def test_wrong_depth_and_invalid_inputs_cannot_measure(self):
        self.assertEqual(measure_surface_relation(bytes([4,9]),[60,80],[60,20],2,1,4,9,.1)['status'],'indeterminate')
        for depths,tolerance in (([60,math.nan],.1),([60,-2],.1),([60,80],math.nan),([60,80],0)):
            with self.assertRaises(ValueError):
                measure_surface_relation(bytes([4,9]),depths,[60,80],2,1,4,9,tolerance)


if __name__=='__main__': unittest.main()
