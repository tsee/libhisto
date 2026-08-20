import unittest
import math
import histo


class TestHistogram(unittest.TestCase):
    def test_uniform_creation_and_fill(self):
        h = histo.Histogram(bins=10, range=(0.0, 100.0), track_sumw2=True)
        self.assertEqual(len(h), 10)
        self.assertEqual(h.nbins, 10)
        self.assertAlmostEqual(h.min, 0.0)
        self.assertAlmostEqual(h.max, 100.0)
        self.assertEqual(h.num_entries, 0)
        self.assertAlmostEqual(h.total_weight, 0.0)

        # Ingestion
        self.assertTrue(h.fill(25.0))
        self.assertTrue(h.fill(75.0, weight=2.0))
        self.assertEqual(h.num_entries, 2)
        self.assertAlmostEqual(h.total_weight, 3.0)

        # Batch fill
        self.assertTrue(h.fill_n([10.0, 20.0, 30.0], weights=[1.0, 1.0, 1.0]))
        self.assertEqual(h.num_entries, 5)
        self.assertAlmostEqual(h.total_weight, 6.0)

        # Non-finite rejection
        self.assertFalse(h.fill(float('nan')))
        self.assertEqual(h.nan_count, 1)

    def test_variable_creation(self):
        edges = [0.0, 10.0, 50.0, 100.0]
        h = histo.Histogram(edges=edges)
        self.assertEqual(len(h), 3)
        self.assertAlmostEqual(h.min, 0.0)
        self.assertAlmostEqual(h.max, 100.0)

        h.fill(5.0)
        h.fill(25.0)
        h.fill(75.0)
        self.assertEqual(h.num_entries, 3)

    def test_statistical_moments(self):
        h = histo.Histogram(bins=100, range=(0.0, 100.0), track_sumw2=True)
        # Fill symmetric distribution centered at 50
        for x in [40.0, 45.0, 50.0, 50.0, 55.0, 60.0]:
            h.fill(x)

        self.assertAlmostEqual(h.mean, 50.0, delta=1.0)
        self.assertGreater(h.variance, 0.0)
        self.assertAlmostEqual(h.std_dev, math.sqrt(h.variance), places=6)
        self.assertAlmostEqual(h.median, 50.0, delta=2.0)
        self.assertGreater(h.iqr, 0.0)
        self.assertGreaterEqual(h.mad, 0.0)
        self.assertAlmostEqual(h.mode, 50.0, delta=2.0)
        self.assertGreater(h.fwhm, 0.0)
        self.assertGreater(h.rms, 0.0)

        stats = h.stats
        self.assertIn("mean", stats)
        self.assertIn("variance", stats)
        self.assertIn("total_weight", stats)

    def test_indexing_and_slicing(self):
        h = histo.Histogram(bins=10, range=(0.0, 100.0))
        h.fill(5.0)   # bin 0
        h.fill(15.0)  # bin 1
        h.fill(25.0)  # bin 2

        self.assertAlmostEqual(h[0], 1.0)
        self.assertAlmostEqual(h[1], 1.0)
        self.assertAlmostEqual(h[2], 1.0)
        self.assertAlmostEqual(h[3], 0.0)

        # Slice
        sub = h[1:3]
        self.assertIsInstance(sub, histo.Histogram)
        self.assertEqual(sub.nbins, 3)
        self.assertAlmostEqual(sub.total_weight, 2.0)

        # Iteration
        contents = list(h)
        self.assertEqual(len(contents), 10)
        self.assertAlmostEqual(contents[0], 1.0)

    def test_arithmetic_operators(self):
        h1 = histo.Histogram(bins=10, range=(0.0, 10.0))
        h2 = histo.Histogram(bins=10, range=(0.0, 10.0))

        h1.fill(2.5, weight=2.0)
        h2.fill(2.5, weight=3.0)

        # Addition
        h_add = h1 + h2
        self.assertAlmostEqual(h_add[2], 5.0)

        # Subtraction
        h_sub = h2 - h1
        self.assertAlmostEqual(h_sub[2], 1.0)

        # Scalar multiplication
        h_mul = h1 * 3.0
        self.assertAlmostEqual(h_mul[2], 6.0)

        # Scalar division
        h_div = h1 / 2.0
        self.assertAlmostEqual(h_div[2], 1.0)

    def test_comparison_metrics(self):
        h1 = histo.Histogram(bins=20, range=(0.0, 100.0))
        h2 = histo.Histogram(bins=20, range=(0.0, 100.0))

        for v in [20, 30, 40, 50, 60, 70, 80]:
            h1.fill(float(v))
            h2.fill(float(v))

        chi2, ndf = h1.chi2_test(h2)
        self.assertAlmostEqual(chi2, 0.0, places=4)
        self.assertGreater(ndf, 0)

        ks = h1.kolmogorov_smirnov(h2)
        self.assertAlmostEqual(ks, 0.0, places=4)

        emd = h1.wasserstein_distance(h2)
        self.assertAlmostEqual(emd, 0.0, places=4)

        kl = h1.kl_divergence(h2)
        self.assertAlmostEqual(kl, 0.0, places=4)

        bhatt = h1.bhattacharyya_distance(h2)
        self.assertAlmostEqual(bhatt, 0.0, places=4)


if __name__ == "__main__":
    unittest.main()
