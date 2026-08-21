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


    def test_lognormal_fit(self):
        h = histo.Histogram(bins=50, range=(0.1, 15.0), track_sumw2=True)
        for i in range(50):
            x = h.bin_center(i)
            lx = math.log(x)
            val = (500.0 / (x * 0.4 * math.sqrt(2 * math.pi))) * math.exp(-0.5 * ((lx - 1.5) / 0.4) ** 2)
            h.fill_bin(i, val)

        res = h.fit(model="lognormal")
        self.assertTrue(res.converged)
        self.assertAlmostEqual(res.params[0], 500.0, delta=10.0)
        self.assertAlmostEqual(res.params[1], 1.5, delta=0.1)
        self.assertAlmostEqual(res.params[2], 0.4, delta=0.1)

    def test_poisson_fit(self):
        h = histo.Histogram(bins=20, range=(0.0, 15.0), track_sumw2=True)
        for i in range(20):
            x = h.bin_center(i)
            val = 100.0 * (4.5 ** x * math.exp(-4.5)) / math.gamma(x + 1.0)
            h.fill_bin(i, val)

        res = h.fit(model="poisson")
        self.assertTrue(res.converged)
        self.assertAlmostEqual(res.params[0], 100.0, delta=5.0)
        self.assertAlmostEqual(res.params[1], 4.5, delta=0.2)

    def test_laplace_fit(self):
        h = histo.Histogram(bins=40, range=(0.0, 10.0), track_sumw2=True)
        for i in range(40):
            x = h.bin_center(i)
            val = (150.0 / (2 * 1.2)) * math.exp(-abs(x - 5.0) / 1.2)
            h.fill_bin(i, val)

        res = h.fit(model="laplace")
        self.assertTrue(res.converged)
        self.assertAlmostEqual(res.params[0], 150.0, delta=10.0)
        self.assertAlmostEqual(res.params[1], 5.0, delta=0.2)
        self.assertAlmostEqual(res.params[2], 1.2, delta=0.2)


    def test_eval_model_and_gradient(self):
        from histo.fit import eval_model, eval_gradient
        # Gaussian: f(x) = A * exp(-(x - mu)^2 / (2 * sigma^2))
        params = [100.0, 5.0, 2.0]
        val = eval_model("gaussian", params, 5.0)
        self.assertAlmostEqual(val, 100.0, places=6)

        grad = eval_gradient("gaussian", params, 5.0)
        self.assertEqual(len(grad), 3)
        self.assertAlmostEqual(grad[0], 1.0, places=6)  # df/dA = exp(0) = 1
        self.assertAlmostEqual(grad[1], 0.0, places=6)  # df/dmu = 0 at x = mu


if __name__ == "__main__":
    unittest.main()
