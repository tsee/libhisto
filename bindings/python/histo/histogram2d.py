"""
2-Dimensional Histogram implementation for libhisto Python bindings.
"""

from typing import Sequence, Optional, Union, Tuple, Dict, Any
import _libhisto
from histo.constants import (
    FLAG_NONE, FLAG_TRACK_SUMW2,
    REGION_CENTER, REGION_EAST, REGION_NORTH, REGION_SOUTH, REGION_WEST,
    REGION_SOUTH_WEST, REGION_SOUTH_EAST, REGION_NORTH_WEST, REGION_NORTH_EAST
)
from histo.histogram import Histogram
from histo.compat import HAS_NUMPY, require_numpy, as_float64_buffer
from histo.axis import Axis, HistogramAxis
from histo.uhi import loc, rebin, underflow as uhi_underflow, overflow as uhi_overflow, sum as uhi_sum, Kind


class Histogram2D:
    """
    2-Dimensional High-Performance Histogram.
    """
    def __init__(
        self,
        xbins: Optional[int] = None,
        xrange: Optional[Tuple[float, float]] = None,
        ybins: Optional[int] = None,
        yrange: Optional[Tuple[float, float]] = None,
        xedges: Optional[Sequence[float]] = None,
        yedges: Optional[Sequence[float]] = None,
        track_sumw2: bool = False,
        flags: int = FLAG_NONE,
        _raw: Optional[_libhisto.Histo2D] = None,
    ):
        if _raw is not None:
            self._raw = _raw
            return

        active_flags = flags
        if track_sumw2:
            active_flags |= FLAG_TRACK_SUMW2

        if xedges is not None and yedges is not None:
            self._raw = _libhisto.Histo2D.create_variable(xedges=xedges, yedges=yedges, flags=active_flags)
        elif xbins is not None and xrange is not None and ybins is not None and yrange is not None:
            xmin, xmax = xrange
            ymin, ymax = yrange
            self._raw = _libhisto.Histo2D.create_uniform(
                nx=int(xbins), xmin=float(xmin), xmax=float(xmax),
                ny=int(ybins), ymin=float(ymin), ymax=float(ymax),
                flags=active_flags
            )
        else:
            raise ValueError("Must specify either (xbins, xrange, ybins, yrange) or (xedges, yedges)")

    # -------------------------------------------------------------------------
    # Deserialization & File I/O
    # -------------------------------------------------------------------------
    @classmethod
    def from_binary(cls, blob: bytes) -> "Histogram2D":
        """Deserialize 2D histogram from binary wire format."""
        raw = _libhisto.Histo2D.deserialize_binary(blob)
        return cls(_raw=raw)

    @classmethod
    def from_json(cls, json_str: str) -> "Histogram2D":
        """Deserialize 2D histogram from JSON string."""
        raw = _libhisto.Histo2D.deserialize_json(json_str)
        return cls(_raw=raw)

    def to_binary(self) -> bytes:
        """Serialize 2D histogram to binary bytes."""
        return self._raw.serialize_binary()

    def serialize_binary(self) -> bytes:
        """Alias for to_binary."""
        return self.to_binary()

    def to_json(self) -> str:
        """Serialize 2D histogram to JSON string."""
        return self._raw.serialize_json()

    def serialize_json(self) -> str:
        """Alias for to_json."""
        return self.to_json()

    # -------------------------------------------------------------------------
    # NumPy & Universal Histogram Interface (UHI) Interoperability
    # -------------------------------------------------------------------------
    def to_numpy(self, flow: bool = False) -> Tuple[Any, Any, Any]:
        """
        Convert 2D histogram to (H, xedges, yedges) matching numpy.histogram2d format.
        """
        np = require_numpy("Histogram2D.to_numpy")
        H = np.zeros((self.nx, self.ny), dtype=np.float64)
        for ix in range(self.nx):
            for iy in range(self.ny):
                H[ix, iy] = self.bin_content(ix, iy)
        xedges = self.axes[0].edges
        yedges = self.axes[1].edges
        return H, xedges, yedges

    @classmethod
    def from_numpy(
        cls,
        H: Any,
        xedges: Any,
        yedges: Any,
        track_sumw2: bool = False
    ) -> "Histogram2D":
        """
        Construct a Histogram2D from existing NumPy 2D array H and edge vectors.
        """
        np = require_numpy("Histogram2D.from_numpy")
        H_arr = np.asarray(H, dtype=np.float64)
        x_list = [float(x) for x in xedges]
        y_list = [float(y) for y in yedges]

        if H_arr.shape != (len(x_list) - 1, len(y_list) - 1):
            raise ValueError(
                f"H shape {H_arr.shape} does not match edges ({len(x_list)-1}, {len(y_list)-1})"
            )

        nx = len(x_list) - 1
        ny = len(y_list) - 1

        dx0 = x_list[1] - x_list[0]
        is_x_uniform = all(abs((x_list[i + 1] - x_list[i]) - dx0) <= 1e-10 * max(1.0, abs(dx0)) for i in range(1, nx))

        dy0 = y_list[1] - y_list[0]
        is_y_uniform = all(abs((y_list[i + 1] - y_list[i]) - dy0) <= 1e-10 * max(1.0, abs(dy0)) for i in range(1, ny))

        if is_x_uniform and is_y_uniform:
            h2 = cls(xbins=nx, xrange=(x_list[0], x_list[-1]), ybins=ny, yrange=(y_list[0], y_list[-1]), track_sumw2=track_sumw2)
        else:
            h2 = cls(xedges=x_list, yedges=y_list, track_sumw2=track_sumw2)

        for ix in range(nx):
            for iy in range(ny):
                val = float(H_arr[ix, iy])
                if val != 0.0:
                    # Ingest via fill at bin center
                    cx = (x_list[ix] + x_list[ix + 1]) * 0.5
                    cy = (y_list[iy] + y_list[iy + 1]) * 0.5
                    h2.fill(cx, cy, weight=val)
        return h2

    def __array__(self, dtype=None, copy=None) -> Any:
        """NumPy array interface returning 2D array of bin contents."""
        np = require_numpy("Histogram2D.__array__")
        H = np.zeros((self.nx, self.ny), dtype=np.float64)
        for ix in range(self.nx):
            for iy in range(self.ny):
                H[ix, iy] = self.bin_content(ix, iy)
        if dtype is not None and H.dtype != dtype:
            return H.astype(dtype, copy=copy)
        if copy:
            return H.copy()
        return H

    @property
    def axes(self) -> Tuple[HistogramAxis, HistogramAxis]:
        """UHI axes tuple (XAxis, YAxis)."""
        x_ax = HistogramAxis(self.nx, self.xmin, self.xmax, raw_histo=self._raw, axis_idx=0)
        y_ax = HistogramAxis(self.ny, self.ymin, self.ymax, raw_histo=self._raw, axis_idx=1)
        return (x_ax, y_ax)

    def values(self, flow: bool = False) -> Any:
        """
        UHI values protocol returning 2D NumPy array of bin contents.

        Parameters
        ----------
        flow : bool, default=False
            If False, returns (nx, ny) array.
            If True, returns (nx+2, ny+2) array incorporating the 9 boundary guard regions:
              * center: bins[ix, iy] at [1:nx+1, 1:ny+1]
              * left guard: region_left[iy] at [0, 1:ny+1]
              * right guard: region_right[iy] at [nx+1, 1:ny+1]
              * bottom guard: region_bottom[ix] at [1:nx+1, 0]
              * top guard: region_top[ix] at [1:nx+1, ny+1]
              * corners: bottom-left [0, 0], bottom-right [nx+1, 0],
                         top-left [0, ny+1], top-right [nx+1, ny+1]
        """
        np = require_numpy("Histogram2D.values")
        if not flow:
            return self.__array__()

        arr = np.zeros((self.nx + 2, self.ny + 2), dtype=np.float64)
        for ix in range(self.nx):
            for iy in range(self.ny):
                arr[ix + 1, iy + 1] = self.bin_content(ix, iy)

        # 4 Corner guard regions
        arr[0, 0] = self._raw.region_content(REGION_SOUTH_WEST)
        arr[self.nx + 1, 0] = self._raw.region_content(REGION_SOUTH_EAST)
        arr[0, self.ny + 1] = self._raw.region_content(REGION_NORTH_WEST)
        arr[self.nx + 1, self.ny + 1] = self._raw.region_content(REGION_NORTH_EAST)

        # 4 Edge guard regions
        w_west = self._raw.region_content(REGION_WEST)
        w_east = self._raw.region_content(REGION_EAST)
        w_south = self._raw.region_content(REGION_SOUTH)
        w_north = self._raw.region_content(REGION_NORTH)

        arr[0, 1 : self.ny + 1] = w_west / self.ny
        arr[self.nx + 1, 1 : self.ny + 1] = w_east / self.ny
        arr[1 : self.nx + 1, 0] = w_south / self.nx
        arr[1 : self.nx + 1, self.ny + 1] = w_north / self.nx

        return arr

    def variances(self, flow: bool = False) -> Optional[Any]:
        """
        UHI variances protocol returning 2D NumPy array of bin uncertainties squared.

        Parameters
        ----------
        flow : bool, default=False
            If False, returns (nx, ny) array.
            If True, returns (nx+2, ny+2) array incorporating the 9 boundary guard regions.
        """
        np = require_numpy("Histogram2D.variances")
        if not flow:
            H = np.zeros((self.nx, self.ny), dtype=np.float64)
            for ix in range(self.nx):
                for iy in range(self.ny):
                    H[ix, iy] = self.bin_sum_w2(ix, iy)
            return H

        arr = np.zeros((self.nx + 2, self.ny + 2), dtype=np.float64)
        for ix in range(self.nx):
            for iy in range(self.ny):
                arr[ix + 1, iy + 1] = self.bin_sum_w2(ix, iy)

        # 4 Corner guard regions
        arr[0, 0] = self._raw.region_sum_w2(REGION_SOUTH_WEST)
        arr[self.nx + 1, 0] = self._raw.region_sum_w2(REGION_SOUTH_EAST)
        arr[0, self.ny + 1] = self._raw.region_sum_w2(REGION_NORTH_WEST)
        arr[self.nx + 1, self.ny + 1] = self._raw.region_sum_w2(REGION_NORTH_EAST)

        # 4 Edge guard regions
        w2_west = self._raw.region_sum_w2(REGION_WEST)
        w2_east = self._raw.region_sum_w2(REGION_EAST)
        w2_south = self._raw.region_sum_w2(REGION_SOUTH)
        w2_north = self._raw.region_sum_w2(REGION_NORTH)

        arr[0, 1 : self.ny + 1] = w2_west / self.ny
        arr[self.nx + 1, 1 : self.ny + 1] = w2_east / self.ny
        arr[1 : self.nx + 1, 0] = w2_south / self.nx
        arr[1 : self.nx + 1, self.ny + 1] = w2_north / self.nx

        return arr

    def counts(self, flow: bool = False) -> Any:
        """UHI counts protocol (alias for values)."""
        return self.values(flow=flow)

    @property
    def kind(self) -> str:
        """UHI kind property."""
        return Kind.COUNT

    @property
    def flags(self) -> int:
        """Active feature flags."""
        return getattr(self._raw, "flags", FLAG_NONE)

    def to_boost(self) -> Any:
        """
        Convert Histogram2D to a boost_histogram.Histogram 2D object.

        Returns
        -------
        boost_histogram.Histogram
        """
        try:
            import boost_histogram as bh
        except ImportError:
            raise ImportError(
                "to_boost() requires boost-histogram. "
                "Please install boost-histogram using 'pip install boost-histogram' or 'pip install histo[uhi]'."
            )

        axes = []
        for i, ax in enumerate(self.axes):
            edges = [float(x) for x in ax.edges]
            is_uniform = True
            dx0 = edges[1] - edges[0]
            for j in range(1, len(edges) - 1):
                if abs((edges[j + 1] - edges[j]) - dx0) > 1e-10 * max(1.0, abs(dx0)):
                    is_uniform = False
                    break
            if is_uniform:
                axes.append(bh.axis.Regular(len(ax), ax.min, ax.max, underflow=True, overflow=True))
            else:
                axes.append(bh.axis.Variable(edges, underflow=True, overflow=True))

        has_w2 = (self.flags & FLAG_TRACK_SUMW2) != 0
        storage = bh.storage.Weight() if has_w2 else bh.storage.Double()
        bh_h = bh.Histogram(*axes, storage=storage)

        vals = self.values(flow=True)
        if has_w2:
            vars_arr = self.variances(flow=True)
            bh_h.view(flow=True).value = vals
            bh_h.view(flow=True).variance = vars_arr
        else:
            bh_h.view(flow=True)[:] = vals

        return bh_h

    @classmethod
    def from_boost(cls, bh_histo: Any) -> "Histogram2D":
        """
        Construct a libhisto Histogram2D from a 2D boost-histogram Histogram.
        """
        if len(bh_histo.axes) != 2:
            raise ValueError(f"Expected 2D boost-histogram, got {len(bh_histo.axes)}D")

        xedges = [float(x) for x in bh_histo.axes[0].edges]
        yedges = [float(x) for x in bh_histo.axes[1].edges]
        has_sumw2 = hasattr(bh_histo.view(), "variance")

        return cls.from_numpy(bh_histo.values(), xedges, yedges, track_sumw2=has_sumw2)

    # -------------------------------------------------------------------------
    # Ingestion Methods
    # -------------------------------------------------------------------------
    def fill(self, x: float, y: float, weight: float = 1.0) -> bool:
        """Fill single 2D sample (x, y) with optional weight."""
        return self._raw.fill(float(x), float(y), weight=float(weight))

    def fill_n(self, x: Sequence[float], y: Sequence[float], weights: Optional[Sequence[float]] = None) -> bool:
        """Batch fill 2D samples from sequences."""
        return self._raw.fill_n(x, y, weights=weights)

    def fill_buffer(self, x_buf: Any, y_buf: Any = None, weights_buf: Any = None) -> bool:
        """
        SIMD batch fill from float64 buffers or array-like objects.
        Supports either:
          - fill_buffer(x_array, y_array, weights=None)
          - fill_buffer(points_Nx2, weights=None)
        """
        if y_buf is None and HAS_NUMPY:
            np = require_numpy()
            arr = np.asarray(x_buf)
            if arr.ndim == 2 and arr.shape[1] == 2:
                x_buf = arr[:, 0]
                y_buf = arr[:, 1]
            else:
                raise ValueError("Single argument to fill_buffer must have shape (N, 2)")
        elif y_buf is None:
            raise ValueError("Must provide both x_buf and y_buf")

        x_buf_c = as_float64_buffer(x_buf)
        y_buf_c = as_float64_buffer(y_buf)
        w_buf_c = as_float64_buffer(weights_buf) if weights_buf is not None else None
        return self._raw.fill_buffer(x_buf_c, y_buf_c, weights=w_buf_c)

    def fill_packed(self, x_buf: Any, y_buf: Any = None, weights_buf: Any = None) -> bool:
        """Alias for fill_buffer."""
        return self.fill_buffer(x_buf, y_buf, weights_buf)

    # -------------------------------------------------------------------------
    # Properties & Geometry
    # -------------------------------------------------------------------------
    @property
    def nx(self) -> int:
        """Number of bins along X axis."""
        return self._raw.nx

    @property
    def ny(self) -> int:
        """Number of bins along Y axis."""
        return self._raw.ny

    @property
    def xmin(self) -> float:
        """Lower bound along X."""
        return self._raw.xmin

    @property
    def xmax(self) -> float:
        """Upper bound along X."""
        return self._raw.xmax

    @property
    def ymin(self) -> float:
        """Lower bound along Y."""
        return self._raw.ymin

    @property
    def ymax(self) -> float:
        """Upper bound along Y."""
        return self._raw.ymax

    @property
    def total_weight(self) -> float:
        """Total in-range accumulated weight."""
        return self._raw.total_weight

    @property
    def num_entries(self) -> int:
        """Total fill operations performed."""
        return self._raw.num_entries

    # -------------------------------------------------------------------------
    # Moments & Covariance
    # -------------------------------------------------------------------------
    @property
    def mean_x(self) -> float:
        """Sample mean along X."""
        return self._raw.mean_x

    @property
    def mean_y(self) -> float:
        """Sample mean along Y."""
        return self._raw.mean_y

    @property
    def std_dev_x(self) -> float:
        """Sample standard deviation along X."""
        return self._raw.std_dev_x

    @property
    def std_dev_y(self) -> float:
        """Sample standard deviation along Y."""
        return self._raw.std_dev_y

    @property
    def variance_x(self) -> float:
        """Sample variance along X."""
        return self._raw.variance_x

    @property
    def variance_y(self) -> float:
        """Sample variance along Y."""
        return self._raw.variance_y

    @property
    def covariance(self) -> float:
        """Sample covariance Cov(X, Y)."""
        return self._raw.covariance

    @property
    def correlation(self) -> float:
        """Pearson correlation coefficient rho_xy in [-1.0, 1.0]."""
        return self._raw.correlation

    # -------------------------------------------------------------------------
    # Bin Accessors
    # -------------------------------------------------------------------------
    def bin_content(self, ix: int, iy: int) -> float:
        """Get accumulated weight in 2D cell (ix, iy)."""
        return self._raw.bin_content(int(ix), int(iy))

    def bin_error(self, ix: int, iy: int) -> float:
        """Get statistical uncertainty (standard error) of 2D cell (ix, iy)."""
        return self._raw.bin_error(int(ix), int(iy))

    def bin_sum_w2(self, ix: int, iy: int) -> float:
        """Get sum of squared weights sum(w^2) of 2D cell (ix, iy)."""
        return self._raw.bin_sum_w2(int(ix), int(iy))

    def bin_bounds(self, ix: int, iy: int) -> Tuple[float, float, float, float]:
        """Get bounding box (xmin, xmax, ymin, ymax) of 2D cell (ix, iy)."""
        return self._raw.bin_bounds(int(ix), int(iy))

    def bin_center(self, ix: int, iy: int) -> Tuple[float, float]:
        """Get midpoint coordinate (cx, cy) of 2D cell (ix, iy)."""
        return self._raw.bin_center(int(ix), int(iy))

    def find_bin(self, x: float, y: float) -> Tuple[int, int]:
        """Locate (ix, iy) bin indices for coordinate pair (x, y)."""
        return self._raw.find_bin(float(x), float(y))

    def find_region(self, x: float, y: float) -> int:
        """Identify which of the 9 geometric regions (x, y) falls into."""
        return self._raw.find_region(float(x), float(y))

    def integral(
        self,
        ix_min: Optional[int] = None,
        ix_max: Optional[int] = None,
        iy_min: Optional[int] = None,
        iy_max: Optional[int] = None,
    ) -> float:
        """
        Compute total in-range 2D volume or sub-grid integral over [ix_min, ix_max] x [iy_min, iy_max].
        """
        if ix_min is not None and ix_max is not None and iy_min is not None and iy_max is not None:
            return self._raw.integral(int(ix_min), int(ix_max), int(iy_min), int(iy_max))
        return self._raw.integral()

    def __getitem__(self, key: Any) -> Union[float, Histogram, "Histogram2D"]:
        """
        2D Indexing, slicing, projection, and rebinning:
        - h2[ix, iy]: returns bin content (scalar float).
        - h2[loc(x), loc(y)]: returns bin content at coordinates (x, y).
        - h2[ix, :]: returns 1D Histogram slice along Y at fixed X bin ix.
        - h2[:, iy]: returns 1D Histogram slice along X at fixed Y bin iy.
        - h2[:, uhi.sum] or h2[:, sum]: projects along X axis (returns 1D Histogram).
        - h2[uhi.sum, :] or h2[sum, :]: projects along Y axis (returns 1D Histogram).
        - h2[slice_x, slice_y]: returns sliced / rebinned Histogram2D.
        """
        if not isinstance(key, tuple):
            key = (key, slice(None, None))
        if len(key) != 2:
            raise TypeError("Histogram2D index must have 2 dimensions (x, y)")

        key_x, key_y = key

        # Check for sum / projection tags or built-in sum function
        is_x_sum = key_x is uhi_sum or type(key_x).__name__ == "_SumTag" or key_x is sum
        is_y_sum = key_y is uhi_sum or type(key_y).__name__ == "_SumTag" or key_y is sum

        if is_x_sum and is_y_sum:
            return self.total_weight

        if is_y_sum:
            # Project onto X axis
            proj = self.project_x()
            if isinstance(key_x, slice) and (key_x.start is not None or key_x.stop is not None or key_x.step is not None):
                return proj[key_x]
            return proj

        if is_x_sum:
            # Project onto Y axis
            proj = self.project_y()
            if isinstance(key_y, slice) and (key_y.start is not None or key_y.stop is not None or key_y.step is not None):
                return proj[key_y]
            return proj

        # Check for coordinate / integer single cell access
        is_x_scalar = isinstance(key_x, (int, loc))
        is_y_scalar = isinstance(key_y, (int, loc))

        if is_x_scalar and is_y_scalar:
            if isinstance(key_x, loc):
                ix = self.axes[0].index(key_x.value) + key_x.offset
            else:
                ix = int(key_x)
                if ix < 0:
                    ix += self.nx
            if ix < 0 or ix >= self.nx:
                raise IndexError(f"X bin index {ix} out of range [0, {self.nx - 1}]")

            if isinstance(key_y, loc):
                iy = self.axes[1].index(key_y.value) + key_y.offset
            else:
                iy = int(key_y)
                if iy < 0:
                    iy += self.ny
            if iy < 0 or iy >= self.ny:
                raise IndexError(f"Y bin index {iy} out of range [0, {self.ny - 1}]")

            return self.bin_content(ix, iy)

        # 1D slice along Y at fixed X
        if is_x_scalar and isinstance(key_y, slice):
            if isinstance(key_x, loc):
                ix = self.axes[0].index(key_x.value) + key_x.offset
            else:
                ix = int(key_x)
                if ix < 0:
                    ix += self.nx
            if ix < 0 or ix >= self.nx:
                raise IndexError(f"X bin index {ix} out of range [0, {self.nx - 1}]")
            h1_y = self.slice_y(ix, ix)
            if key_y.start is not None or key_y.stop is not None or key_y.step is not None:
                return h1_y[key_y]
            return h1_y

        # 1D slice along X at fixed Y
        if isinstance(key_x, slice) and is_y_scalar:
            if isinstance(key_y, loc):
                iy = self.axes[1].index(key_y.value) + key_y.offset
            else:
                iy = int(key_y)
                if iy < 0:
                    iy += self.ny
            if iy < 0 or iy >= self.ny:
                raise IndexError(f"Y bin index {iy} out of range [0, {self.ny - 1}]")
            h1_x = self.slice_x(iy, iy)
            if key_x.start is not None or key_x.stop is not None or key_x.step is not None:
                return h1_x[key_x]
            return h1_x

        # 2D slice / rebin
        if isinstance(key_x, slice) and isinstance(key_y, slice):
            rebin_x = 1
            if key_x.step is not None:
                if isinstance(key_x.step, rebin):
                    rebin_x = key_x.step.factor
                elif isinstance(key_x.step, complex) and key_x.step.real == 0 and key_x.step.imag > 0:
                    rebin_x = int(round(key_x.step.imag))
                elif isinstance(key_x.step, int) and key_x.step > 0:
                    rebin_x = key_x.step

            rebin_y = 1
            if key_y.step is not None:
                if isinstance(key_y.step, rebin):
                    rebin_y = key_y.step.factor
                elif isinstance(key_y.step, complex) and key_y.step.real == 0 and key_y.step.imag > 0:
                    rebin_y = int(round(key_y.step.imag))
                elif isinstance(key_y.step, int) and key_y.step > 0:
                    rebin_y = key_y.step

            res = self.clone()
            if rebin_x > 1 or rebin_y > 1:
                res = res.rebin(rebin_x, rebin_y)
            return res

        raise TypeError(f"Invalid 2D indexing types: {type(key_x)}, {type(key_y)}")

    # -------------------------------------------------------------------------
    # Projections, Slices & Profiles
    # -------------------------------------------------------------------------
    def project_x(self) -> Histogram:
        """Project 2D histogram onto X-axis, integrating over Y."""
        raw_1d = self._raw.project_x()
        return Histogram(_raw=raw_1d)

    def project_y(self) -> Histogram:
        """Project 2D histogram onto Y-axis, integrating over X."""
        raw_1d = self._raw.project_y()
        return Histogram(_raw=raw_1d)

    def slice_x(self, iy_min: int, iy_max: int) -> Histogram:
        """Slice 2D histogram along X across Y-bin interval [iy_min, iy_max]."""
        raw_1d = self._raw.slice_x(int(iy_min), int(iy_max))
        return Histogram(_raw=raw_1d)

    def slice_y(self, ix_min: int, ix_max: int) -> Histogram:
        """Slice 2D histogram along Y across X-bin interval [ix_min, ix_max]."""
        raw_1d = self._raw.slice_y(int(ix_min), int(ix_max))
        return Histogram(_raw=raw_1d)

    def profile_x(self) -> Histogram:
        """Compute 1D Profile along X: mean of Y in each X bin."""
        raw_1d = self._raw.profile_x()
        return Histogram(_raw=raw_1d)

    def profile_y(self) -> Histogram:
        """Compute 1D Profile along Y: mean of X in each Y bin."""
        raw_1d = self._raw.profile_y()
        return Histogram(_raw=raw_1d)

    # -------------------------------------------------------------------------
    # Transformations & Arithmetic
    # -------------------------------------------------------------------------
    def scale(self, factor: float) -> "Histogram2D":
        """Scale all bin contents and weights in-place by factor."""
        self._raw.scale(float(factor))
        return self

    def normalize(self, target_integral: float = 1.0) -> "Histogram2D":
        """Normalize 2D histogram in-place such that total volume equals target_integral."""
        self._raw.normalize(float(target_integral))
        return self

    def rebin(self, factor_x: int, factor_y: int) -> "Histogram2D":
        """Return new Histogram2D rebinned by integer factors (factor_x, factor_y)."""
        raw = self._raw.rebin(int(factor_x), int(factor_y))
        return Histogram2D(_raw=raw)

    def reset(self) -> None:
        """Reset all bin contents, moments, and counters to zero."""
        self._raw.reset()

    def clone(self, empty: bool = False) -> "Histogram2D":
        """Create exact clone or empty schema copy."""
        raw = self._raw.clone(empty=empty)
        return Histogram2D(_raw=raw)

    def add(self, other: "Histogram2D", scale: float = 1.0) -> "Histogram2D":
        """In-place addition: self += scale * other."""
        if not isinstance(other, Histogram2D):
            raise TypeError("other must be a Histogram2D instance")
        self._raw.add(other._raw, float(scale))
        return self

    def subtract(self, other: "Histogram2D") -> "Histogram2D":
        """In-place subtraction: self -= other."""
        if not isinstance(other, Histogram2D):
            raise TypeError("other must be a Histogram2D instance")
        self._raw.subtract(other._raw)
        return self

    def multiply(self, other: "Histogram2D") -> "Histogram2D":
        """In-place multiplication: self *= other."""
        if not isinstance(other, Histogram2D):
            raise TypeError("other must be a Histogram2D instance")
        self._raw.multiply(other._raw)
        return self

    def divide(self, other: "Histogram2D") -> "Histogram2D":
        """In-place division: self /= other."""
        if not isinstance(other, Histogram2D):
            raise TypeError("other must be a Histogram2D instance")
        self._raw.divide(other._raw)
        return self

    def __add__(self, other: "Histogram2D") -> "Histogram2D":
        res = self.clone()
        return res.add(other)

    def __sub__(self, other: "Histogram2D") -> "Histogram2D":
        res = self.clone()
        return res.subtract(other)

    def __mul__(self, other: Union["Histogram2D", float, int]) -> "Histogram2D":
        res = self.clone()
        if isinstance(other, (int, float)):
            return res.scale(float(other))
        return res.multiply(other)

    def __rmul__(self, other: Union[float, int]) -> "Histogram2D":
        return self.__mul__(other)

    def __truediv__(self, other: Union["Histogram2D", float, int]) -> "Histogram2D":
        res = self.clone()
        if isinstance(other, (int, float)):
            if float(other) == 0.0:
                raise ZeroDivisionError("division by zero")
            return res.scale(1.0 / float(other))
        return res.divide(other)

    # -------------------------------------------------------------------------
    # Visualization
    # -------------------------------------------------------------------------
    def plot(
        self,
        style: str = "unicode",
        color: Optional[Union[bool, str]] = None,
        palette: str = "viridis",
        width: Optional[int] = None,
        height: Optional[int] = None,
        log: bool = False,
        show: bool = True,
    ) -> str:
        """
        Render terminal visualization (2D heatmap) of the bivariate histogram.
        """
        import tempfile
        import os
        import sys
        import histo.cli as cli

        args = ["plot"]
        if style:
            args.append(f"--style={style}")
        if color is True:
            args.append("--color=always")
        elif color is False:
            args.append("--color=never")
        elif isinstance(color, str):
            args.append(f"--color={color}")
        if palette:
            args.append(f"--palette={palette}")
        if width is not None:
            args.append(f"-w={width}")
        if height is not None:
            args.append(f"-H={height}")
        if log:
            args.append("-l")

        with tempfile.NamedTemporaryFile(suffix=".json", delete=False, mode="w") as tf:
            tf.write(self.to_json())
            tf_path = tf.name

        try:
            args.append(tf_path)
            code, out, err = cli.run(*args)
            if code != 0 and err:
                raise RuntimeError(f"histo plot failed: {err}")
            if show and out:
                sys.stdout.write(out)
                sys.stdout.flush()
            return out
        finally:
            if os.path.exists(tf_path):
                os.remove(tf_path)

    def __repr__(self) -> str:
        return (
            f"Histogram2D({self.nx}x{self.ny} bins, "
            f"xrange=({self.xmin:.4g}, {self.xmax:.4g}), "
            f"yrange=({self.ymin:.4g}, {self.ymax:.4g}), "
            f"entries={self.num_entries})"
        )
