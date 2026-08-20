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


if __name__ == "__main__":
    unittest.main()
