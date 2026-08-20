import unittest
import histo


class TestSketch(unittest.TestCase):
    def test_sketch_lifecycle_and_quantiles(self):
        s = histo.Sketch(alpha=0.01, max_bins=1024)
        self.assertEqual(len(s), 0)
        self.assertEqual(s.num_entries, 0)
        self.assertAlmostEqual(s.total_weight, 0.0)

        # Ingest 1..1000
        vals = [float(x) for x in range(1, 1001)]
        self.assertTrue(s.insert_n(vals))
        self.assertEqual(s.num_entries, 1000)
        self.assertAlmostEqual(s.total_weight, 1000.0)
        self.assertAlmostEqual(s.min, 1.0)
        self.assertAlmostEqual(s.max, 1000.0)

        # Quantile error bounds <= 2%
        p50 = s.quantile(0.50)
        self.assertAlmostEqual(p50, 500.0, delta=10.0)

        p90 = s.quantile(0.90)
        self.assertAlmostEqual(p90, 900.0, delta=18.0)

        p99 = s.quantile(0.99)
        self.assertAlmostEqual(p99, 990.0, delta=20.0)

        # Merge
        s2 = histo.Sketch(alpha=0.01)
        vals2 = [float(x) for x in range(1001, 2001)]
        s2.insert_n(vals2)

        self.assertTrue(s.merge(s2))
        self.assertEqual(s.num_entries, 2000)
        self.assertAlmostEqual(s.max, 2000.0)

        # Serialization
        blob = s.to_binary()
        self.assertGreater(len(blob), 0)
        restored = histo.Sketch.from_binary(blob)
        self.assertEqual(restored.num_entries, 2000)


if __name__ == "__main__":
    unittest.main()
