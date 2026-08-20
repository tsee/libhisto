"""
2-Dimensional Histogram implementation for libhisto Python bindings.
"""

from typing import Sequence, Optional, Union, Tuple, Dict, Any
import _libhisto
from histo.constants import FLAG_NONE, FLAG_TRACK_SUMW2
from histo.histogram import Histogram


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
    # Ingestion Methods
    # -------------------------------------------------------------------------
    def fill(self, x: float, y: float, weight: float = 1.0) -> bool:
        """Fill single 2D sample (x, y) with optional weight."""
        return self._raw.fill(float(x), float(y), weight=float(weight))

    def fill_n(self, x: Sequence[float], y: Sequence[float], weights: Optional[Sequence[float]] = None) -> bool:
        """Batch fill 2D samples from sequences."""
        return self._raw.fill_n(x, y, weights=weights)

    def fill_buffer(self, x_buf, y_buf, weights_buf=None) -> bool:
        """Zero-copy SIMD batch fill from float64 buffers."""
        return self._raw.fill_buffer(x_buf, y_buf, weights=weights_buf)

    def fill_packed(self, x_buf, y_buf, weights_buf=None) -> bool:
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

    def bin_content(self, ix: int, iy: int) -> float:
        """Accumulated weight in cell (ix, iy)."""
        return self._raw.bin_content(int(ix), int(iy))

    # -------------------------------------------------------------------------
    # Projections & Profiles
    # -------------------------------------------------------------------------
    def project_x(self, y0: int = 0, y1: Optional[int] = None) -> Histogram:
        """Project along X axis to 1D Histogram."""
        if y1 is None:
            y1 = self.ny - 1
        raw_p = self._raw.project_x(int(y0), int(y1))
        return Histogram(_raw=raw_p)

    def project_y(self, x0: int = 0, x1: Optional[int] = None) -> Histogram:
        """Project along Y axis to 1D Histogram."""
        if x1 is None:
            x1 = self.nx - 1
        raw_p = self._raw.project_y(int(x0), int(x1))
        return Histogram(_raw=raw_p)

    def profile_x(self) -> Histogram:
        """Profile histogram along X (mean of Y in each X bin with standard error)."""
        raw_p = self._raw.profile_x()
        return Histogram(_raw=raw_p)

    def profile_y(self) -> Histogram:
        """Profile histogram along Y (mean of X in each Y bin with standard error)."""
        raw_p = self._raw.profile_y()
        return Histogram(_raw=raw_p)

    def __repr__(self) -> str:
        return (f"<Histogram2D grid=({self.nx}x{self.ny}) "
                f"xrange=({self.xmin:.2f}, {self.xmax:.2f}) yrange=({self.ymin:.2f}, {self.ymax:.2f}) "
                f"entries={self.num_entries} weight={self.total_weight:.2f}>")
