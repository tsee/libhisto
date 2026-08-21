"""
2-Dimensional Histogram implementation for libhisto Python bindings.
"""

from typing import Sequence, Optional, Union, Tuple, Dict, Any
import _libhisto
from histo.constants import FLAG_NONE, FLAG_TRACK_SUMW2
from histo.histogram import Histogram
from histo.compat import HAS_NUMPY, require_numpy, as_float64_buffer
from histo.axis import Axis


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
    def axes(self) -> Tuple[Axis, Axis]:
        """UHI axes tuple (XAxis, YAxis)."""
        x_ax = Axis(self.nx, self.xmin, self.xmax, axis_idx=0)
        y_ax = Axis(self.ny, self.ymin, self.ymax, axis_idx=1)
        return (x_ax, y_ax)

    def values(self, flow: bool = False) -> Any:
        """UHI values protocol."""
        return self.__array__()

    def counts(self, flow: bool = False) -> Any:
        """UHI counts protocol."""
        return self.values(flow=flow)

    @property
    def kind(self) -> str:
        """UHI kind property."""
        return "COUNT"

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

    def __getitem__(self, idx: Tuple[int, int]) -> float:
        """Index access: h2[ix, iy]."""
        if not isinstance(idx, tuple) or len(idx) != 2:
            raise TypeError("Histogram2D index must be a (ix, iy) tuple")
        return self.bin_content(idx[0], idx[1])

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
