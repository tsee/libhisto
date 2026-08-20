import unittest
import array
import histo
from histo.compat import HAS_NUMPY

try:
    import numpy as np
except ImportError:
    np = None


@unittest.skipUnless(HAS_NUMPY, "NumPy not installed in test environment")
class TestNumPyIntegration(unittest.TestCase):
    def test_1d_to_numpy_and_from_numpy(self):
        # 1. Create and fill
        h = histo.Histogram(bins=20, range=(0.0, 100.0), track_sumw2=True)
        data = np.linspace(5.0, 95.0, 100)
        h.fill_buffer(data)

        # 2. to_numpy()
        counts, edges = h.to_numpy()
        self.assertIsInstance(counts, np.ndarray)
        self.assertIsInstance(edges, np.ndarray)
        self.assertEqual(counts.shape, (20,))
        self.assertEqual(edges.shape, (21,))
        self.assertAlmostEqual(counts.sum(), 100.0)

        # Compare directly with np.histogram
        np_counts, np_edges = np.histogram(data, bins=20, range=(0.0, 100.0))
        np.testing.assert_allclose(counts, np_counts)
        np.testing.assert_allclose(edges, np_edges)

        # 3. from_numpy() roundtrip (uniform)
        h_restored = histo.Histogram.from_numpy(counts, edges)
        self.assertEqual(h_restored.nbins, 20)
        self.assertAlmostEqual(h_restored.total_weight, 100.0)

        # 4. from_numpy() variable edges
        var_edges = np.array([0.0, 10.0, 50.0, 100.0])
        var_counts = np.array([5.0, 15.0, 25.0])
        h_var = histo.Histogram.from_numpy(var_counts, var_edges)
        self.assertEqual(h_var.nbins, 3)
        self.assertAlmostEqual(h_var.total_weight, 45.0)

    def test_2d_to_numpy_and_from_numpy(self):
        h2 = histo.Histogram2D(xbins=10, xrange=(0.0, 10.0), ybins=10, yrange=(0.0, 10.0))
        x_data = np.array([1.5, 2.5, 3.5])
        y_data = np.array([4.5, 5.5, 6.5])
        h2.fill_buffer(x_data, y_data)

        # to_numpy()
        H, xedges, yedges = h2.to_numpy()
        self.assertIsInstance(H, np.ndarray)
        self.assertEqual(H.shape, (10, 10))
        self.assertEqual(xedges.shape, (11,))
        self.assertEqual(yedges.shape, (11,))
        self.assertAlmostEqual(H.sum(), 3.0)

        # from_numpy() roundtrip
        h2_restored = histo.Histogram2D.from_numpy(H, xedges, yedges)
        self.assertEqual(h2_restored.nx, 10)
        self.assertEqual(h2_restored.ny, 10)
        self.assertAlmostEqual(h2_restored.total_weight, 3.0)

    def test_array_interface(self):
        h = histo.Histogram(bins=10, range=(0.0, 10.0))
        h.fill(2.5, weight=3.0)
        h.fill(7.5, weight=4.0)

        # np.asarray()
        arr = np.asarray(h)
        self.assertEqual(arr.shape, (10,))
        self.assertAlmostEqual(arr[2], 3.0)
        self.assertAlmostEqual(arr[7], 4.0)

        # np.sum(), np.mean(), ufuncs
        self.assertAlmostEqual(np.sum(h), 7.0)
        sqrt_arr = np.sqrt(h)
        self.assertAlmostEqual(sqrt_arr[2], np.sqrt(3.0))

        # 2D array interface
        h2 = histo.Histogram2D(xbins=4, xrange=(0.0, 4.0), ybins=4, yrange=(0.0, 4.0))
        h2.fill(1.5, 2.5, weight=5.0)
        arr2d = np.asarray(h2)
        self.assertEqual(arr2d.shape, (4, 4))
        self.assertAlmostEqual(arr2d[1, 2], 5.0)
        self.assertAlmostEqual(np.sum(h2), 5.0)

    def test_dtype_coercion_and_shapes(self):
        h = histo.Histogram(bins=10, range=(0.0, 100.0))

        # int64 array
        int64_data = np.array([10, 20, 30, 40], dtype=np.int64)
        self.assertTrue(h.fill_buffer(int64_data))
        self.assertEqual(h.num_entries, 4)

        # float32 array
        float32_data = np.array([15.5, 25.5], dtype=np.float32)
        self.assertTrue(h.fill_buffer(float32_data))
        self.assertEqual(h.num_entries, 6)

        # Non-contiguous / strided array
        full_arr = np.linspace(0.0, 90.0, 20)
        strided_arr = full_arr[::2]  # Step 2 -> non-contiguous
        self.assertTrue(h.fill_buffer(strided_arr))
        self.assertEqual(h.num_entries, 16)

        # 2D (N, 2) points array
        h2 = histo.Histogram2D(xbins=10, xrange=(0.0, 10.0), ybins=10, yrange=(0.0, 10.0))
        pts_Nx2 = np.array([[1.0, 2.0], [3.0, 4.0], [5.0, 6.0]], dtype=np.float64)
        self.assertTrue(h2.fill_buffer(pts_Nx2))
        self.assertEqual(h2.num_entries, 3)

    def test_uhi_protocol(self):
        h = histo.Histogram(bins=10, range=(0.0, 100.0), track_sumw2=True)
        h.fill(50.0, weight=2.0)

        # Axes
        self.assertEqual(len(h.axes), 1)
        ax = h.axes[0]
        self.assertEqual(len(ax), 10)
        self.assertAlmostEqual(ax.edges[0], 0.0)
        self.assertAlmostEqual(ax.edges[-1], 100.0)
        self.assertEqual(len(ax.centers), 10)
        self.assertEqual(len(ax.widths), 10)
        self.assertFalse(ax.traits.circular)
        self.assertFalse(ax.traits.discrete)

        # Values & variances
        vals = h.values()
        self.assertEqual(vals.shape, (10,))
        self.assertAlmostEqual(vals[5], 2.0)

        vars_arr = h.variances()
        self.assertEqual(vars_arr.shape, (10,))
        self.assertAlmostEqual(vars_arr[5], 4.0)

class TestFallbackWithoutNumPy(unittest.TestCase):
    def test_import_error_raised_without_numpy(self):
        if not HAS_NUMPY:
            h = histo.Histogram(bins=10, range=(0.0, 100.0))
            with self.assertRaises(ImportError) as ctx:
                h.to_numpy()
            self.assertIn("NumPy", str(ctx.exception))

            with self.assertRaises(ImportError) as ctx:
                h.__array__()
            self.assertIn("NumPy", str(ctx.exception))

    def test_axis_and_intervals_stdlib(self):
        from histo.axis import Axis, Interval
        iv = Interval(10.0, 20.0)
        self.assertEqual(iv.lower, 10.0)
        self.assertEqual(iv.upper, 20.0)
        self.assertEqual(list(iv), [10.0, 20.0])
        self.assertEqual(iv, (10.0, 20.0))

        h = histo.Histogram(bins=10, range=(0.0, 100.0))
        self.assertEqual(len(h.axes), 1)
        ax = h.axes[0]
        self.assertEqual(len(ax), 10)
        self.assertEqual(ax[0], (0.0, 10.0))
        self.assertEqual(ax[9], (90.0, 100.0))
        self.assertFalse(ax.traits.circular)


class MockNDArray(list):

    def __init__(self, data, dtype=float, shape=None):
        super().__init__(data)
        self.dtype = dtype
        self.shape = shape if shape is not None else (len(data),)
        self.flags = type('Flags', (), {'c_contiguous': True})()

    def astype(self, dtype, copy=None):
        return MockNDArray([dtype(x) for x in self], dtype=dtype, shape=self.shape)

    def sum(self):
        return sum(self)


class MockNumPyModule:
    float64 = float
    ndarray = MockNDArray

    @staticmethod
    def array(data, dtype=float):
        return MockNDArray(list(data), dtype=dtype)

    @staticmethod
    def asarray(data, dtype=float):
        if hasattr(data, '__array__'):
            return data.__array__(dtype=dtype)
        return MockNDArray(list(data), dtype=dtype)

    @staticmethod
    def ascontiguousarray(data, dtype=float):
        return MockNDArray(list(data), dtype=dtype)

    @staticmethod
    def zeros(shape, dtype=float):
        if isinstance(shape, tuple):
            rows, cols = shape
            matrix = [[0.0 for _ in range(cols)] for _ in range(rows)]
            m = MockNDArray(matrix, dtype=dtype, shape=shape)
            # Add 2D indexer support
            class MatrixIndexer:
                def __init__(self, m): self.m = m
                def __setitem__(self, idx, val): self.m[idx[0]][idx[1]] = val
                def __getitem__(self, idx): return self.m[idx[0]][idx[1]]
            m.__class__.__setitem__ = lambda self, idx, val: self[idx[0]].__setitem__(idx[1], val) if isinstance(idx, tuple) else list.__setitem__(self, idx, val)
            m.__class__.__getitem__ = lambda self, idx: self[idx[0]][idx[1]] if isinstance(idx, tuple) else list.__getitem__(self, idx)
            return m
        return MockNDArray([0.0] * shape, dtype=dtype, shape=(shape,))


class TestMockedNumPy(unittest.TestCase):
    def setUp(self):
        import histo.compat
        import histo.axis
        self._orig_numpy = histo.compat.np
        self._orig_has_numpy = histo.compat.HAS_NUMPY
        histo.compat.np = MockNumPyModule()
        histo.compat.HAS_NUMPY = True

    def tearDown(self):
        import histo.compat
        histo.compat.np = self._orig_numpy
        histo.compat.HAS_NUMPY = self._orig_has_numpy

    def test_mocked_1d_numpy_and_uhi(self):
        h = histo.Histogram(bins=10, range=(0.0, 100.0), track_sumw2=True)
        h.fill(25.0, weight=4.0)

        counts, edges = h.to_numpy()
        self.assertEqual(len(counts), 10)
        self.assertEqual(len(edges), 11)
        self.assertEqual(counts[2], 4.0)
        self.assertEqual(edges[0], 0.0)
        self.assertEqual(edges[-1], 100.0)

        # from_numpy
        h2 = histo.Histogram.from_numpy([1.0, 2.0, 3.0], [0.0, 10.0, 20.0, 30.0])
        self.assertEqual(h2.nbins, 3)
        self.assertEqual(h2.total_weight, 6.0)

        # __array__
        arr = h.__array__()
        self.assertEqual(len(arr), 10)
        self.assertEqual(arr[2], 4.0)

        # UHI values, variances, axes
        vals = h.values()
        self.assertEqual(vals[2], 4.0)
        vars_arr = h.variances()
        self.assertEqual(vars_arr[2], 16.0)

        ax = h.axes[0]
        self.assertEqual(len(ax.edges), 11)
        self.assertEqual(len(ax.centers), 10)
        self.assertEqual(len(ax.widths), 10)

    def test_mocked_2d_numpy(self):
        h2 = histo.Histogram2D(xbins=5, xrange=(0.0, 5.0), ybins=5, yrange=(0.0, 5.0))
        h2.fill(1.5, 2.5, weight=3.0)

        H, xedges, yedges = h2.to_numpy()
        self.assertEqual(len(xedges), 6)
        self.assertEqual(len(yedges), 6)
        self.assertEqual(H[1, 2], 3.0)

        arr2d = h2.__array__()
        self.assertEqual(arr2d[1, 2], 3.0)


if __name__ == "__main__":
    unittest.main()


