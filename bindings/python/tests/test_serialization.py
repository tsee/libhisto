import unittest
import os
import tempfile
import histo


class TestSerialization(unittest.TestCase):
    def test_binary_roundtrip(self):
        h = histo.Histogram(bins=20, range=(0.0, 100.0), track_sumw2=True)
        h.fill(25.0)
        h.fill(75.0, weight=2.5)

        blob = h.to_binary()
        self.assertGreater(len(blob), 0)

        restored = histo.Histogram.from_binary(blob)
        self.assertEqual(restored.nbins, 20)
        self.assertAlmostEqual(restored.min, 0.0)
        self.assertAlmostEqual(restored.max, 100.0)
        self.assertEqual(restored.num_entries, 2)
        self.assertAlmostEqual(restored.total_weight, 3.5)

    def test_json_roundtrip(self):
        h = histo.Histogram(bins=10, range=(0.0, 10.0))
        h.fill(5.0)

        json_str = h.to_json()
        self.assertIn('"nbins":', json_str)

        restored = histo.Histogram.from_json(json_str)
        self.assertEqual(restored.nbins, 10)
        self.assertAlmostEqual(restored.total_weight, 1.0)

    def test_file_io(self):
        h = histo.Histogram(bins=10, range=(0.0, 10.0))
        h.fill(3.0)

        with tempfile.TemporaryDirectory() as tmpdir:
            path_bin = os.path.join(tmpdir, "test.histo")
            h.to_file(path_bin, format="binary")
            restored_bin = histo.Histogram.from_file(path_bin, format="binary")
            self.assertEqual(restored_bin.num_entries, 1)

            path_json = os.path.join(tmpdir, "test.json")
            h.to_file(path_json, format="json")
            restored_json = histo.Histogram.from_file(path_json, format="json")
            self.assertEqual(restored_json.num_entries, 1)


if __name__ == "__main__":
    unittest.main()
