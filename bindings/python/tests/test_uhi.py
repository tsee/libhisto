"""
Comprehensive Unit and Integration Tests for Universal Histogram Interface (UHI) Compliance.
"""

import unittest
import math
import histo
from histo.uhi import (
    loc, rebin, underflow, overflow, sum as uhi_sum,
    Kind, AxisTraits, PlottableTraits, PlottableAxis, PlottableHistogram
)
from histo.axis import Axis, HistogramAxis, Interval
from histo.compat import HAS_NUMPY


class MockNDArray(list):
    """Mock numpy ndarray for environments where numpy is not installed."""
    def __init__(self, data, dtype=float, shape=None):
        super().__init__(data)
        self.dtype = dtype
        self.shape = shape if shape is not None else (len(data),)
        self.flags = type("Flags", (), {"c_contiguous": True})()

    def astype(self, dtype, copy=None):
        return MockNDArray([dtype(x) for x in self], dtype=dtype, shape=self.shape)

    def sum(self):
        if self.shape and len(self.shape) == 2:
            return sum(sum(row) for row in self)
        return sum(self)


class MockNumPyModule:
    float64 = float
    ndarray = MockNDArray

    @staticmethod
    def array(data, dtype=float):
        data_list = list(data)
        return MockNDArray(data_list, dtype=dtype, shape=(len(data_list),))

    @staticmethod
    def asarray(data, dtype=float):
        if hasattr(data, "__array__"):
            return data.__array__(dtype=dtype)
        if isinstance(data, (list, tuple)) and data and isinstance(data[0], (list, tuple, MockNDArray)):
            shape = (len(data), len(data[0]))
            return MockNDArray(data, dtype=dtype, shape=shape)
        data_list = list(data)
        return MockNDArray(data_list, dtype=dtype, shape=(len(data_list),))

    @staticmethod
    def ascontiguousarray(data, dtype=float):
        return MockNumPyModule.asarray(data, dtype=dtype)

    @staticmethod
    def zeros(shape, dtype=float):
        if isinstance(shape, tuple):
            rows, cols = shape
            matrix = [[0.0 for _ in range(cols)] for _ in range(rows)]
            m = MockNDArray(matrix, dtype=dtype, shape=shape)

            def getitem(s, idx):
                if isinstance(idx, tuple):
                    r, c = idx
                    if isinstance(r, slice) and isinstance(c, slice):
                        sub = [row[c] for row in s[r]]
                        return MockNDArray(sub, dtype=dtype, shape=(len(sub), len(sub[0]) if sub else 0))
                    elif isinstance(r, slice):
                        sub = [row[c] for row in s[r]]
                        return MockNDArray(sub, dtype=dtype, shape=(len(sub),))
                    elif isinstance(c, slice):
                        sub = s[r][c]
                        return MockNDArray(sub, dtype=dtype, shape=(len(sub),))
                    return s[r][c]
                return list.__getitem__(s, idx)

            def setitem(s, idx, val):
                if isinstance(idx, tuple):
                    r, c = idx
                    if isinstance(r, slice) and isinstance(c, int):
                        # Column slice assignment
                        rows_selected = list(range(len(s)))[r]
                        if isinstance(val, (list, tuple, MockNDArray)):
                            for idx_i, row_idx in enumerate(rows_selected):
                                s[row_idx][c] = float(val[idx_i])
                        else:
                            for row_idx in rows_selected:
                                s[row_idx][c] = float(val)
                    elif isinstance(r, int) and isinstance(c, slice):
                        # Row slice assignment
                        cols_selected = list(range(len(s[r])))[c]
                        if isinstance(val, (list, tuple, MockNDArray)):
                            for idx_i, col_idx in enumerate(cols_selected):
                                s[r][col_idx] = float(val[idx_i])
                        else:
                            for col_idx in cols_selected:
                                s[r][col_idx] = float(val)
                    elif isinstance(r, slice) and isinstance(c, slice):
                        # 2D block assignment
                        rows_selected = list(range(len(s)))[r]
                        for i_r, row_idx in enumerate(rows_selected):
                            cols_selected = list(range(len(s[row_idx])))[c]
                            for i_c, col_idx in enumerate(cols_selected):
                                s[row_idx][col_idx] = float(val[i_r][i_c] if hasattr(val, "__getitem__") else val)
                    else:
                        s[r][c] = float(val)
                else:
                    list.__setitem__(s, idx, val)

            m.__class__.__getitem__ = getitem
            m.__class__.__setitem__ = setitem
            return m
        return MockNDArray([0.0] * shape, dtype=dtype, shape=(shape,))


class TestUHITraitsAndLocators(unittest.TestCase):
    """Test traits, locators, tags and Interval structures."""

    def test_traits(self):
        t1 = AxisTraits()
        self.assertFalse(t1.discrete)
        self.assertFalse(t1.circular)
        self.assertFalse(t1.growth)
        self.assertIn("circular=False", repr(t1))

        t2 = PlottableTraits(circular=True, discrete=True, growth=True)
        self.assertTrue(t2.circular)
        self.assertTrue(t2.discrete)
        self.assertTrue(t2.growth)
        self.assertEqual(t1, AxisTraits(False, False, False))
        self.assertNotEqual(t1, t2)

    def test_interval(self):
        iv = Interval(5.0, 10.0)
        self.assertEqual(iv.lower, 5.0)
        self.assertEqual(iv.upper, 10.0)
        self.assertEqual(iv[0], 5.0)
        self.assertEqual(iv[1], 10.0)
        self.assertEqual(iv[-2], 5.0)
        self.assertEqual(iv[-1], 10.0)
        self.assertEqual(len(iv), 2)
        self.assertEqual(list(iv), [5.0, 10.0])
        self.assertEqual(iv, (5.0, 10.0))
        self.assertEqual(iv, [5.0, 10.0])
        self.assertIn("Interval(5.0, 10.0)", repr(iv))

        with self.assertRaises(IndexError):
            _ = iv[2]

    def test_locators(self):
        l1 = loc(25.0)
        self.assertEqual(l1.value, 25.0)
        self.assertEqual(l1.offset, 0)
        self.assertEqual(repr(l1), "loc(25.0)")

        l2 = l1 + 2
        self.assertEqual(l2.value, 25.0)
        self.assertEqual(l2.offset, 2)
        self.assertEqual(repr(l2), "loc(25.0, offset=2)")

        l3 = l2 - 3
        self.assertEqual(l3.offset, -1)

        self.assertEqual(l1, loc(25.0))
        self.assertNotEqual(l1, loc(26.0))

    def test_rebin_tag(self):
        r1 = rebin(2)
        self.assertEqual(r1.factor, 2)
        self.assertEqual(repr(r1), "rebin(2)")
        self.assertEqual(r1, 2)
        self.assertEqual(r1, rebin(2))

        with self.assertRaises(ValueError):
            rebin(0)
        with self.assertRaises(ValueError):
            rebin(-1)

    def test_tags(self):
        self.assertEqual(repr(underflow), "underflow")
        self.assertEqual(repr(overflow), "overflow")
        self.assertEqual(repr(uhi_sum), "sum")
        self.assertEqual(Kind.COUNT, "COUNT")
        self.assertEqual(Kind.MEAN, "MEAN")


class TestUHIAxis(unittest.TestCase):
    """Test PlottableAxis / HistogramAxis implementation."""

    def test_uniform_axis(self):
        ax = HistogramAxis(10, 0.0, 100.0)
        self.assertEqual(len(ax), 10)
        self.assertEqual(ax.min, 0.0)
        self.assertEqual(ax.max, 100.0)

        # Indexing & bin method
        self.assertEqual(ax[0], (0.0, 10.0))
        self.assertEqual(ax.bin(0), (0.0, 10.0))
        self.assertEqual(ax[9], (90.0, 100.0))
        self.assertEqual(ax[-1], (90.0, 100.0))

        # Iteration
        bins = list(ax)
        self.assertEqual(len(bins), 10)
        self.assertEqual(bins[0], (0.0, 10.0))
        self.assertEqual(bins[-1], (90.0, 100.0))

        # index() coordinate lookups
        self.assertEqual(ax.index(-5.0), -1)  # underflow
        self.assertEqual(ax.index(0.0), 0)
        self.assertEqual(ax.index(5.0), 0)
        self.assertEqual(ax.index(9.99), 0)
        self.assertEqual(ax.index(10.0), 1)
        self.assertEqual(ax.index(55.0), 5)
        self.assertEqual(ax.index(99.9), 9)
        self.assertEqual(ax.index(100.0), 10)  # overflow
        self.assertEqual(ax.index(150.0), 10)  # overflow

        # Geometry properties
        widths = ax.widths
        self.assertEqual(len(widths), 10)
        for w in widths:
            self.assertAlmostEqual(w, 10.0)

        centers = ax.centers
        self.assertEqual(len(centers), 10)
        self.assertAlmostEqual(centers[0], 5.0)
        self.assertAlmostEqual(centers[9], 95.0)

        edges = ax.edges
        self.assertEqual(len(edges), 11)
        self.assertAlmostEqual(edges[0], 0.0)
        self.assertAlmostEqual(edges[-1], 100.0)

        # Protocol check
        self.assertIsInstance(ax, PlottableAxis)

    def test_variable_axis(self):
        edges = [0.0, 10.0, 30.0, 100.0]
        ax = HistogramAxis(3, 0.0, 100.0, edges=edges)
        self.assertEqual(len(ax), 3)
        self.assertEqual(ax[0], (0.0, 10.0))
        self.assertEqual(ax[1], (10.0, 30.0))
        self.assertEqual(ax[2], (30.0, 100.0))

        self.assertEqual(ax.index(-1.0), -1)
        self.assertEqual(ax.index(5.0), 0)
        self.assertEqual(ax.index(20.0), 1)
        self.assertEqual(ax.index(50.0), 2)
        self.assertEqual(ax.index(100.0), 3)

        self.assertEqual(list(ax.edges), edges)
        self.assertEqual(list(ax.widths), [10.0, 20.0, 70.0])
        self.assertEqual(list(ax.centers), [5.0, 20.0, 65.0])


class TestUHIHistogram1D(unittest.TestCase):
    """Test 1D Histogram UHI compliance, flow arrays, slicing, locators, and converters."""

    def setUp(self):
        import histo.compat
        self._orig_numpy = histo.compat.np
        self._orig_has_numpy = histo.compat.HAS_NUMPY
        histo.compat.np = MockNumPyModule()
        histo.compat.HAS_NUMPY = True

    def tearDown(self):
        import histo.compat
        histo.compat.np = self._orig_numpy
        histo.compat.HAS_NUMPY = self._orig_has_numpy

    def test_uhi_1d_properties_and_protocol(self):
        h = histo.Histogram(bins=10, range=(0.0, 100.0), track_sumw2=True)
        self.assertIsInstance(h, PlottableHistogram)
        self.assertEqual(h.kind, "COUNT")
        self.assertEqual(len(h.axes), 1)
        self.assertIsInstance(h.axes[0], PlottableAxis)

    def test_uhi_1d_flow_arrays(self):
        h = histo.Histogram(bins=5, range=(0.0, 50.0), track_sumw2=True)
        # Fill in-range
        h.fill(5.0, weight=2.0)   # bin 0
        h.fill(25.0, weight=3.0)  # bin 2
        # Fill flow
        h.fill(-10.0, weight=1.5) # underflow
        h.fill(75.0, weight=4.0)  # overflow

        # values(flow=False) vs values(flow=True)
        v_no_flow = h.values(flow=False)
        self.assertEqual(len(v_no_flow), 5)
        self.assertEqual(v_no_flow[0], 2.0)
        self.assertEqual(v_no_flow[1], 0.0)
        self.assertEqual(v_no_flow[2], 3.0)

        v_flow = h.values(flow=True)
        self.assertEqual(len(v_flow), 7)
        self.assertEqual(v_flow[0], 1.5)  # underflow
        self.assertEqual(v_flow[1], 2.0)  # bin 0
        self.assertEqual(v_flow[3], 3.0)  # bin 2
        self.assertEqual(v_flow[6], 4.0)  # overflow

        # counts protocol
        self.assertEqual(list(h.counts(flow=True)), list(v_flow))

        # variances(flow=False) vs variances(flow=True)
        var_no_flow = h.variances(flow=False)
        self.assertEqual(len(var_no_flow), 5)
        self.assertAlmostEqual(var_no_flow[0], 4.0)   # 2.0^2
        self.assertAlmostEqual(var_no_flow[2], 9.0)   # 3.0^2

        var_flow = h.variances(flow=True)
        self.assertEqual(len(var_flow), 7)
        self.assertAlmostEqual(var_flow[0], 2.25)  # 1.5^2
        self.assertAlmostEqual(var_flow[1], 4.0)   # 2.0^2
        self.assertAlmostEqual(var_flow[3], 9.0)   # 3.0^2
        self.assertAlmostEqual(var_flow[6], 16.0)  # 4.0^2

    def test_uhi_1d_slicing_and_locators(self):
        h = histo.Histogram(bins=10, range=(0.0, 100.0))
        for i in range(10):
            h.fill(i * 10.0 + 5.0, weight=i + 1)
        h.fill(-5.0, weight=99.0)
        h.fill(150.0, weight=100.0)

        # Single loc
        self.assertEqual(h[loc(25.0)], 3.0)
        self.assertEqual(h[loc(25.0) + 1], 4.0)

        # Tags
        self.assertEqual(h[underflow], 99.0)
        self.assertEqual(h[overflow], 100.0)

        # Slicing with locators
        sub = h[loc(20.0):loc(60.0)]
        self.assertIsInstance(sub, histo.Histogram)
        self.assertEqual(sub.nbins, 4)
        self.assertEqual(sub[0], 3.0)  # was bin 2 [20, 30)
        self.assertEqual(sub[3], 6.0)  # was bin 5 [50, 60)

        # Slicing with rebin modifier
        rebinned = h[::rebin(2)]
        self.assertEqual(rebinned.nbins, 5)
        self.assertEqual(rebinned[0], 1.0 + 2.0)
        self.assertEqual(rebinned[1], 3.0 + 4.0)

        # Complex step (boost-histogram style nj)
        rebinned_c = h[::2j]
        self.assertEqual(rebinned_c.nbins, 5)
        self.assertEqual(rebinned_c[0], 3.0)

    def test_uhi_converters_import_errors(self):
        h = histo.Histogram(bins=10, range=(0.0, 100.0))
        with self.assertRaises(ImportError) as ctx:
            h.to_scipy_dist()
        self.assertIn("scipy", str(ctx.exception).lower())

        with self.assertRaises(ImportError) as ctx:
            h.to_boost()
        self.assertIn("boost-histogram", str(ctx.exception).lower())

    def test_mocked_to_scipy_dist(self):
        import sys
        h = histo.Histogram(bins=5, range=(0.0, 50.0))
        h.fill(15.0, weight=2.0)

        # Mock scipy.stats
        class MockScipyStats:
            @staticmethod
            def rv_histogram(histogram_tuple):
                counts, edges = histogram_tuple
                return {"counts": list(counts), "edges": list(edges), "type": "rv_histogram"}

        class MockScipy:
            stats = MockScipyStats

        orig_scipy = sys.modules.get("scipy")
        orig_scipy_stats = sys.modules.get("scipy.stats")
        try:
            sys.modules["scipy"] = MockScipy()
            sys.modules["scipy.stats"] = MockScipyStats()

            dist = h.to_scipy_dist()
            self.assertEqual(dist["type"], "rv_histogram")
            self.assertEqual(len(dist["counts"]), 5)
            self.assertEqual(len(dist["edges"]), 6)
            self.assertEqual(dist["counts"][1], 2.0)
        finally:
            if orig_scipy is not None:
                sys.modules["scipy"] = orig_scipy
            else:
                sys.modules.pop("scipy", None)
            if orig_scipy_stats is not None:
                sys.modules["scipy.stats"] = orig_scipy_stats
            else:
                sys.modules.pop("scipy.stats", None)

    def test_mocked_boost_histogram_1d(self):
        import sys
        h = histo.Histogram(bins=4, range=(0.0, 40.0), track_sumw2=True)
        h.fill(15.0, weight=3.0)

        class MockBHView:
            def __init__(self, n):
                self.value = [0.0] * n
                self.variance = [0.0] * n
            def __getitem__(self, idx):
                return self.value[idx]
            def __setitem__(self, idx, val):
                self.value[idx] = val

        class MockBHAxis:
            def __init__(self, edges):
                self.edges = edges
            def __len__(self):
                return len(self.edges) - 1

        class MockBHHistogram:
            def __init__(self, *axes, storage=None):
                self.axes = axes
                self.storage = storage
                total_bins = 1
                for ax in axes:
                    total_bins *= (len(ax) + 2)
                self._view = MockBHView(total_bins)

            def values(self):
                # Return in-range values
                n = len(self.axes[0])
                return self._view.value[1:n+1]

            def view(self, flow=False):
                return self._view

        class MockBHModule:
            class axis:
                @staticmethod
                def Regular(nbins, min_val, max_val, underflow=True, overflow=True):
                    dx = (max_val - min_val) / nbins
                    edges = [min_val + i * dx for i in range(nbins + 1)]
                    return MockBHAxis(edges)
                @staticmethod
                def Variable(edges, underflow=True, overflow=True):
                    return MockBHAxis(list(edges))

            class storage:
                class Weight: pass
                class Double: pass

            Histogram = MockBHHistogram

        orig_bh = sys.modules.get("boost_histogram")
        try:
            sys.modules["boost_histogram"] = MockBHModule()

            bh_obj = h.to_boost()
            self.assertIsInstance(bh_obj, MockBHHistogram)
            self.assertEqual(len(bh_obj.axes), 1)
            self.assertEqual(len(bh_obj.axes[0]), 4)
            self.assertEqual(bh_obj.view(flow=True).value[2], 3.0)
            self.assertEqual(bh_obj.view(flow=True).variance[2], 9.0)

            # from_boost roundtrip
            h_back = histo.Histogram.from_boost(bh_obj)
            self.assertEqual(h_back.nbins, 4)
            self.assertEqual(h_back.bin_content(1), 3.0)
        finally:
            if orig_bh is not None:
                sys.modules["boost_histogram"] = orig_bh
            else:
                sys.modules.pop("boost_histogram", None)


class TestUHIHistogram2D(unittest.TestCase):
    """Test 2D Histogram UHI compliance, 9-guard flow arrays, 2D slicing, projections, and sum."""

    def setUp(self):
        import histo.compat
        self._orig_numpy = histo.compat.np
        self._orig_has_numpy = histo.compat.HAS_NUMPY
        histo.compat.np = MockNumPyModule()
        histo.compat.HAS_NUMPY = True

    def tearDown(self):
        import histo.compat
        histo.compat.np = self._orig_numpy
        histo.compat.HAS_NUMPY = self._orig_has_numpy

    def test_uhi_2d_properties_and_protocol(self):
        h2 = histo.Histogram2D(xbins=5, xrange=(0.0, 50.0), ybins=4, yrange=(0.0, 40.0))
        self.assertIsInstance(h2, PlottableHistogram)
        self.assertEqual(h2.kind, "COUNT")
        self.assertEqual(len(h2.axes), 2)
        self.assertIsInstance(h2.axes[0], PlottableAxis)
        self.assertIsInstance(h2.axes[1], PlottableAxis)
        self.assertEqual(len(h2.axes[0]), 5)
        self.assertEqual(len(h2.axes[1]), 4)

    def test_uhi_2d_9_region_flow_matrix(self):
        h2 = histo.Histogram2D(xbins=2, xrange=(0.0, 20.0), ybins=2, yrange=(0.0, 20.0), track_sumw2=True)
        # Center cells
        h2.fill(5.0, 5.0, weight=1.0)    # (0, 0)
        h2.fill(15.0, 5.0, weight=2.0)   # (1, 0)
        h2.fill(5.0, 15.0, weight=3.0)   # (0, 1)
        h2.fill(15.0, 15.0, weight=4.0)  # (1, 1)

        # 4 Corners
        h2.fill(-5.0, -5.0, weight=10.0)   # SOUTH_WEST
        h2.fill(25.0, -5.0, weight=20.0)   # SOUTH_EAST
        h2.fill(-5.0, 25.0, weight=30.0)   # NORTH_WEST
        h2.fill(25.0, 25.0, weight=40.0)   # NORTH_EAST

        # 4 Edge Guards
        h2.fill(-5.0, 10.0, weight=50.0)   # WEST (left)
        h2.fill(25.0, 10.0, weight=60.0)   # EAST (right)
        h2.fill(10.0, -5.0, weight=70.0)   # SOUTH (bottom)
        h2.fill(10.0, 25.0, weight=80.0)   # NORTH (top)

        # Flow matrix should have shape (2+2, 2+2) = (4, 4)
        flow_vals = h2.values(flow=True)
        self.assertEqual(flow_vals.shape, (4, 4))

        # Check Center: (ix+1, iy+1)
        self.assertEqual(flow_vals[1, 1], 1.0)
        self.assertEqual(flow_vals[2, 1], 2.0)
        self.assertEqual(flow_vals[1, 2], 3.0)
        self.assertEqual(flow_vals[2, 2], 4.0)

        # Check Corners:
        self.assertEqual(flow_vals[0, 0], 10.0)  # Bottom-Left
        self.assertEqual(flow_vals[3, 0], 20.0)  # Bottom-Right
        self.assertEqual(flow_vals[0, 3], 30.0)  # Top-Left
        self.assertEqual(flow_vals[3, 3], 40.0)  # Top-Right

        # Check Guards:
        # Left (West) along Y (ny=2 -> distributed as 50.0 / 2 = 25.0)
        self.assertEqual(flow_vals[0, 1], 25.0)
        self.assertEqual(flow_vals[0, 2], 25.0)
        # Right (East) along Y (60.0 / 2 = 30.0)
        self.assertEqual(flow_vals[3, 1], 30.0)
        self.assertEqual(flow_vals[3, 2], 30.0)
        # Bottom (South) along X (70.0 / 2 = 35.0)
        self.assertEqual(flow_vals[1, 0], 35.0)
        self.assertEqual(flow_vals[2, 0], 35.0)
        # Top (North) along X (80.0 / 2 = 40.0)
        self.assertEqual(flow_vals[1, 3], 40.0)
        self.assertEqual(flow_vals[2, 3], 40.0)

        # Check Variances flow matrix
        flow_vars = h2.variances(flow=True)
        self.assertEqual(flow_vars.shape, (4, 4))
        self.assertAlmostEqual(flow_vars[1, 1], 1.0)
        self.assertAlmostEqual(flow_vars[2, 2], 16.0)
        self.assertAlmostEqual(flow_vars[0, 0], 100.0)
        self.assertAlmostEqual(flow_vars[3, 3], 1600.0)

    def test_uhi_2d_slicing_and_projections(self):
        h2 = histo.Histogram2D(xbins=4, xrange=(0.0, 40.0), ybins=4, yrange=(0.0, 40.0))
        for ix in range(4):
            for iy in range(4):
                h2.fill(ix * 10.0 + 5.0, iy * 10.0 + 5.0, weight=(ix + 1) * 10 + (iy + 1))

        # Scalar access
        self.assertEqual(h2[0, 0], 11.0)
        self.assertEqual(h2[loc(15.0), loc(25.0)], 23.0)

        # Projection along Y using uhi.sum / built-in sum -> 1D Histogram along X
        proj_x = h2[:, uhi_sum]
        self.assertIsInstance(proj_x, histo.Histogram)
        self.assertEqual(proj_x.nbins, 4)
        # Row 0 sum across Y: (11 + 12 + 13 + 14) = 50.0
        self.assertAlmostEqual(proj_x[0], 50.0)

        # Projection along X using uhi.sum -> 1D Histogram along Y
        proj_y = h2[sum, :]
        self.assertIsInstance(proj_y, histo.Histogram)
        self.assertEqual(proj_y.nbins, 4)
        # Col 0 sum across X: (11 + 21 + 31 + 41) = 104.0
        self.assertAlmostEqual(proj_y[0], 104.0)

        # Fixed X slice along Y
        slice_y = h2[1, :]
        self.assertIsInstance(slice_y, histo.Histogram)
        self.assertEqual(slice_y.nbins, 4)
        self.assertEqual(slice_y[0], 21.0)
        self.assertEqual(slice_y[3], 24.0)

        # Fixed Y slice along X
        slice_x = h2[:, 2]
        self.assertIsInstance(slice_x, histo.Histogram)
        self.assertEqual(slice_x.nbins, 4)
        self.assertEqual(slice_x[0], 13.0)
        self.assertEqual(slice_x[3], 43.0)

        # 2D Rebinning
        rebinned_2d = h2[::rebin(2), ::rebin(2)]
        self.assertIsInstance(rebinned_2d, histo.Histogram2D)
        self.assertEqual(rebinned_2d.nx, 2)
        self.assertEqual(rebinned_2d.ny, 2)
        # Block (0..1, 0..1): 11 + 12 + 21 + 22 = 66.0
        self.assertAlmostEqual(rebinned_2d[0, 0], 66.0)

    def test_mocked_boost_histogram_2d(self):
        import sys
        h2 = histo.Histogram2D(xbins=3, xrange=(0.0, 30.0), ybins=2, yrange=(0.0, 20.0))
        h2.fill(15.0, 15.0, weight=5.0)

        class MockBHView2D:
            def __init__(self, nx, ny):
                self.value = [[0.0 for _ in range(ny)] for _ in range(nx)]
                self.variance = [[0.0 for _ in range(ny)] for _ in range(nx)]
            def __getitem__(self, idx):
                if isinstance(idx, tuple):
                    return self.value[idx[0]][idx[1]]
                return self.value
            def __setitem__(self, idx, val):
                if isinstance(idx, tuple):
                    self.value[idx[0]][idx[1]] = val
                else:
                    for ix in range(len(self.value)):
                        for iy in range(len(self.value[0])):
                            self.value[ix][iy] = float(val[ix][iy])

        class MockBHAxis:
            def __init__(self, edges):
                self.edges = edges
            def __len__(self):
                return len(self.edges) - 1

        class MockBHHistogram2D:
            def __init__(self, *axes, storage=None):
                self.axes = axes
                self.storage = storage
                nx = len(axes[0]) + 2
                ny = len(axes[1]) + 2
                self._view = MockBHView2D(nx, ny)

            def values(self):
                nx = len(self.axes[0])
                ny = len(self.axes[1])
                return [[self._view.value[ix + 1][iy + 1] for iy in range(ny)] for ix in range(nx)]

            def view(self, flow=False):
                return self._view

        class MockBHModule:
            class axis:
                @staticmethod
                def Regular(nbins, min_val, max_val, underflow=True, overflow=True):
                    dx = (max_val - min_val) / nbins
                    edges = [min_val + i * dx for i in range(nbins + 1)]
                    return MockBHAxis(edges)
                @staticmethod
                def Variable(edges, underflow=True, overflow=True):
                    return MockBHAxis(list(edges))

            class storage:
                class Weight: pass
                class Double: pass

            Histogram = MockBHHistogram2D

        orig_bh = sys.modules.get("boost_histogram")
        try:
            sys.modules["boost_histogram"] = MockBHModule()

            bh_obj = h2.to_boost()
            self.assertIsInstance(bh_obj, MockBHHistogram2D)
            self.assertEqual(len(bh_obj.axes), 2)
            self.assertEqual(len(bh_obj.axes[0]), 3)
            self.assertEqual(len(bh_obj.axes[1]), 2)

            # from_boost roundtrip
            h2_back = histo.Histogram2D.from_boost(bh_obj)
            self.assertEqual(h2_back.nx, 3)
            self.assertEqual(h2_back.ny, 2)
            self.assertEqual(h2_back.bin_content(1, 1), 5.0)
        finally:
            if orig_bh is not None:
                sys.modules["boost_histogram"] = orig_bh
            else:
                sys.modules.pop("boost_histogram", None)


if __name__ == "__main__":
    unittest.main()

