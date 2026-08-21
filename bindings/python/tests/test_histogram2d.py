import unittest
import histo


class TestHistogram2D(unittest.TestCase):
    def test_2d_uniform_creation_and_fill(self):
        h2 = histo.Histogram2D(xbins=10, xrange=(0.0, 10.0), ybins=5, yrange=(0.0, 5.0), track_sumw2=True)
        self.assertEqual(h2.nx, 10)
        self.assertEqual(h2.ny, 5)
        self.assertAlmostEqual(h2.xmin, 0.0)
        self.assertAlmostEqual(h2.xmax, 10.0)
        self.assertAlmostEqual(h2.ymin, 0.0)
        self.assertAlmostEqual(h2.ymax, 5.0)

        # Ingest
        self.assertTrue(h2.fill(2.5, 1.5))
        self.assertTrue(h2.fill(7.5, 3.5, weight=2.0))
        self.assertEqual(h2.num_entries, 2)
        self.assertAlmostEqual(h2.total_weight, 3.0)

        # Batch fill
        self.assertTrue(h2.fill_n([1.0, 2.0, 3.0], [1.0, 2.0, 3.0]))
        self.assertEqual(h2.num_entries, 5)

        # Covariance & correlation
        self.assertIsNotNone(h2.covariance)
        self.assertIsNotNone(h2.correlation)

        # Projections
        px = h2.project_x()
        self.assertIsInstance(px, histo.Histogram)
        self.assertEqual(px.nbins, 10)
        self.assertAlmostEqual(px.total_weight, h2.total_weight)

        py = h2.project_y()
        self.assertIsInstance(py, histo.Histogram)
        self.assertEqual(py.nbins, 5)

        # Profiles
        prof_x = h2.profile_x()
        self.assertIsInstance(prof_x, histo.Histogram)
        self.assertEqual(prof_x.nbins, 10)

        # Serialization
        blob = h2.to_binary()
        self.assertGreater(len(blob), 0)
        restored = histo.Histogram2D.from_binary(blob)
        self.assertEqual(restored.nx, 10)
        self.assertEqual(restored.ny, 5)
        self.assertEqual(restored.num_entries, 5)

    def test_2d_variable_creation(self):
        h2 = histo.Histogram2D(xedges=[0.0, 5.0, 10.0], yedges=[0.0, 2.0, 4.0, 6.0])
        self.assertEqual(h2.nx, 2)
        self.assertEqual(h2.ny, 3)

    def test_2d_slicing_and_queries(self):
        h2 = histo.Histogram2D(xbins=4, xrange=(0.0, 4.0), ybins=4, yrange=(0.0, 4.0), track_sumw2=True)
        h2.fill(0.5, 0.5, weight=1.0)
        h2.fill(1.5, 1.5, weight=2.0)
        h2.fill(2.5, 2.5, weight=3.0)
        h2.fill(3.5, 3.5, weight=4.0)

        # Bounds and center
        xmin, xmax, ymin, ymax = h2.bin_bounds(0, 0)
        self.assertAlmostEqual(xmin, 0.0)
        self.assertAlmostEqual(xmax, 1.0)
        self.assertAlmostEqual(ymin, 0.0)
        self.assertAlmostEqual(ymax, 1.0)

        cx, cy = h2.bin_center(0, 0)
        self.assertAlmostEqual(cx, 0.5)
        self.assertAlmostEqual(cy, 0.5)

        # find_bin & find_region
        ix, iy = h2.find_bin(2.5, 2.5)
        self.assertEqual((ix, iy), (2, 2))
        reg = h2.find_region(2.5, 2.5)
        self.assertEqual(reg, 0)  # HISTO2D_REGION_IN_RANGE

        # bin_error & sum_w2
        self.assertAlmostEqual(h2.bin_content(1, 1), 2.0)
        self.assertAlmostEqual(h2.bin_sum_w2(1, 1), 4.0)
        self.assertAlmostEqual(h2.bin_error(1, 1), 2.0)

        # Integral
        self.assertAlmostEqual(h2.integral(), 10.0)
        self.assertAlmostEqual(h2.integral(0, 1, 0, 1), 3.0)

        # Slices
        sx = h2.slice_x(0, 1)
        self.assertIsInstance(sx, histo.Histogram)
        self.assertEqual(sx.nbins, 4)
        self.assertAlmostEqual(sx.total_weight, 3.0)

        sy = h2.slice_y(2, 3)
        self.assertIsInstance(sy, histo.Histogram)
        self.assertEqual(sy.nbins, 4)
        self.assertAlmostEqual(sy.total_weight, 7.0)

        # std_dev_x, std_dev_y
        self.assertGreater(h2.std_dev_x, 0.0)
        self.assertGreater(h2.std_dev_y, 0.0)

    def test_2d_arithmetic_and_rebin(self):
        h2a = histo.Histogram2D(xbins=4, xrange=(0.0, 4.0), ybins=4, yrange=(0.0, 4.0))
        h2b = histo.Histogram2D(xbins=4, xrange=(0.0, 4.0), ybins=4, yrange=(0.0, 4.0))

        h2a.fill(1.5, 1.5, weight=2.0)
        h2b.fill(1.5, 1.5, weight=3.0)

        # Addition
        h2_sum = h2a + h2b
        self.assertAlmostEqual(h2_sum.bin_content(1, 1), 5.0)

        # Subtraction
        h2_diff = h2b - h2a
        self.assertAlmostEqual(h2_diff.bin_content(1, 1), 1.0)

        # Scalar multiplication & division
        h2_scaled = h2a * 3.0
        self.assertAlmostEqual(h2_scaled.bin_content(1, 1), 6.0)

        h2_div = h2a / 2.0
        self.assertAlmostEqual(h2_div.bin_content(1, 1), 1.0)

        # Rebin
        h2_reb = h2a.rebin(2, 2)
        self.assertEqual(h2_reb.nx, 2)
        self.assertEqual(h2_reb.ny, 2)
        self.assertAlmostEqual(h2_reb.total_weight, 2.0)

        # Normalize
        h2a.normalize(1.0)
        self.assertAlmostEqual(h2a.integral(), 1.0)

        # Reset
        h2a.reset()
        self.assertEqual(h2a.num_entries, 0)
        self.assertAlmostEqual(h2a.total_weight, 0.0)

    def test_2d_plot(self):
        h2 = histo.Histogram2D(xbins=5, xrange=(0.0, 5.0), ybins=5, yrange=(0.0, 5.0))
        h2.fill(1.0, 1.0)
        h2.fill(2.0, 2.0, weight=2.0)
        h2.fill(3.0, 3.0, weight=3.0)

        out = h2.plot(palette="turbo", color=False, show=False)
        self.assertTrue(len(out) > 0)


if __name__ == "__main__":
    unittest.main()
