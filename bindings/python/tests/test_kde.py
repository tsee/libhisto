import unittest
import math
from histo import KDE, Histogram


class TestKDE(unittest.TestCase):
    def test_kde_basic(self):
        samples = [float(i) for i in range(1, 11)]
        kde = KDE(samples, kernel="gaussian", bw_method="silverman")

        self.assertEqual(kde.n_points, 10)
        self.assertGreater(kde.bandwidth, 0.0)

        # Evaluate PDF
        val = kde.eval(5.5)
        self.assertIsInstance(val, float)
        self.assertGreater(val, 0.0)

        # Batch eval
        vals = kde.eval([2.0, 5.5, 9.0])
        self.assertIsInstance(vals, list)
        self.assertEqual(len(vals), 3)

        # CDF
        cdf_mid = kde.cdf(5.5)
        self.assertTrue(0.40 <= cdf_mid <= 0.60)

        # Quantile
        q50 = kde.quantile(0.50)
        self.assertTrue(4.5 <= q50 <= 6.5)

        # Sampling
        samples_out = kde.sample(n=50, seed=42)
        self.assertEqual(len(samples_out), 50)

    def test_kde_all_kernels(self):
        samples = [1.0, 2.0, 3.0, 4.0, 5.0]
        kernels = ["gaussian", "epanechnikov", "boxcar", "triangular", "biweight", "cosine"]

        for k in kernels:
            kde = KDE(samples, kernel=k, bandwidth=1.0, bw_method="manual")
            val = kde.eval(3.0)
            self.assertGreater(val, 0.0)

    def test_kde_from_histogram(self):
        h = Histogram(bins=20, range=(-5.0, 5.0))
        for i in range(200):
            h.fill(math.sin(i * 0.1) * 3.0)

        kde = KDE.from_histogram(h)
        self.assertGreater(kde.n_points, 0)
        self.assertGreater(kde.bandwidth, 0.0)
        val = kde.eval(0.0)
        self.assertGreater(val, 0.0)


if __name__ == "__main__":
    unittest.main()
