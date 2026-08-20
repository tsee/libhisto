import unittest
import math
from histo import Histogram, BIN_RULE_FD, BIN_RULE_SCOTT, BIN_RULE_STURGES, BIN_RULE_DOANE, BIN_RULE_KNUTH


class TestAutoBin(unittest.TestCase):
    def test_histogram_auto_normal(self):
        samples = [math.sin(i * 0.1) * (i % 7) for i in range(500)]
        h = Histogram.auto(samples, rule="auto")
        self.assertGreaterEqual(h.nbins, 5)
        self.assertEqual(h.num_entries, 500)
        self.assertAlmostEqual(h.total_weight, 500.0, places=5)

    def test_histogram_auto_rules(self):
        samples = [float(i) for i in range(100)]
        for rule in ["fd", "scott", "sturges", "doane", "knuth"]:
            h = Histogram.auto(samples, rule=rule)
            self.assertGreaterEqual(h.nbins, 1)
            self.assertEqual(h.num_entries, 100)

    def test_histogram_auto_identical(self):
        samples = [42.0] * 20
        h = Histogram.auto(samples, rule="fd")
        self.assertEqual(h.nbins, 1)
        self.assertEqual(h.num_entries, 20)


if __name__ == "__main__":
    unittest.main()
