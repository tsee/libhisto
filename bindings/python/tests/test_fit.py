import unittest
import math
import histo


class TestFit(unittest.TestCase):
    def test_gaussian_fit(self):
        h = histo.Histogram(bins=50, range=(0.0, 100.0), track_sumw2=True)
        # Synthetic Gaussian: A=100, mu=50, sigma=10
        for i in range(50):
            x = h.bin_center(i)
            val = 100.0 * math.exp(-0.5 * ((x - 50.0) / 10.0) ** 2)
            h.fill_bin(i, val)

        res = h.fit(model="gaussian", initial=[100.0, 50.0, 10.0])
        self.assertTrue(res.converged)
        self.assertAlmostEqual(res.params[0], 100.0, delta=5.0)  # Amplitude
        self.assertAlmostEqual(res.params[1], 50.0, delta=1.0)   # Mean
        self.assertAlmostEqual(res.params[2], 10.0, delta=1.0)   # Sigma
        self.assertGreater(res.p_value, 0.0)

    def test_polynomial_fit(self):
        h = histo.Histogram(bins=20, range=(0.0, 10.0))
        for i in range(20):
            x = h.bin_center(i)
            val = 2.0 * x + 5.0
            h.fill_bin(i, val)

        res = h.fit(model="polynomial", degree=1)
        self.assertTrue(res.converged)
        self.assertAlmostEqual(res.params[0], 5.0, delta=1.0)  # Intercept
        self.assertAlmostEqual(res.params[1], 2.0, delta=0.5)  # Slope

    def test_custom_callable_fit(self):
        h = histo.Histogram(bins=30, range=(0.0, 30.0))
        for i in range(30):
            x = h.bin_center(i)
            val = 3.0 * x + 1.0
            h.fill_bin(i, val)

        def line_model(x, p):
            return p[0] + p[1] * x

        res = h.fit_custom(line_model, n_params=2, initial=[1.0, 3.0])
        self.assertTrue(res.converged)
        self.assertAlmostEqual(res.params[0], 1.0, delta=1.0)
        self.assertAlmostEqual(res.params[1], 3.0, delta=0.5)


if __name__ == "__main__":
    unittest.main()
