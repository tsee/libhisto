"""
1-Dimensional Histogram implementation for libhisto Python bindings.
"""

import math
from typing import Sequence, Optional, Union, Tuple, Dict, Any, Callable
import _libhisto
from histo.constants import FLAG_NONE, FLAG_TRACK_SUMW2, FLAG_EXACT_MOMENTS
from histo.fit import FitResult, MODEL_MAP, LOSS_MAP
from histo.compat import HAS_NUMPY, require_numpy, as_float64_buffer
from histo.axis import Axis, HistogramAxis
from histo.uhi import loc, rebin, underflow as uhi_underflow, overflow as uhi_overflow, Kind


class Histogram:
    """
    1-Dimensional High-Performance Histogram.
    """
    def __init__(
        self,
        bins: Optional[int] = None,
        range: Optional[Tuple[float, float]] = None,
        edges: Optional[Sequence[float]] = None,
        track_sumw2: bool = False,
        exact_moments: bool = False,
        flags: int = FLAG_NONE,
        _raw: Optional[_libhisto.Histo1D] = None,
    ):
        if _raw is not None:
            self._raw = _raw
            return

        active_flags = flags
        if track_sumw2:
            active_flags |= FLAG_TRACK_SUMW2
        if exact_moments:
            active_flags |= FLAG_EXACT_MOMENTS

        if edges is not None:
            self._raw = _libhisto.Histo1D.create_variable(edges=edges, flags=active_flags)
        elif bins is not None and range is not None:
            min_val, max_val = range
            self._raw = _libhisto.Histo1D.create_uniform(nbins=int(bins), min=float(min_val), max=float(max_val), flags=active_flags)
        else:
            raise ValueError("Must specify either (bins, range) for uniform binning or edges for variable binning")

    @classmethod
    def auto(
        cls,
        samples: Sequence[float],
        rule: Union[str, int] = "auto",
        track_sumw2: bool = False,
        exact_moments: bool = False,
        flags: int = FLAG_NONE,
    ) -> "Histogram":
        """Construct and fill an automatically sized uniform histogram from sample data."""
        active_flags = flags
        if track_sumw2:
            active_flags |= FLAG_TRACK_SUMW2
        if exact_moments:
            active_flags |= FLAG_EXACT_MOMENTS

        rule_map = {
            "auto": _libhisto.BIN_RULE_AUTO,
            "fd": _libhisto.BIN_RULE_FD,
            "scott": _libhisto.BIN_RULE_SCOTT,
            "sturges": _libhisto.BIN_RULE_STURGES,
            "doane": _libhisto.BIN_RULE_DOANE,
            "knuth": _libhisto.BIN_RULE_KNUTH,
        }
        r_code = rule_map.get(str(rule).lower(), rule) if isinstance(rule, str) else int(rule)
        raw = _libhisto.create_auto(samples=samples, rule=r_code, flags=active_flags)
        return cls(_raw=raw)

    # -------------------------------------------------------------------------
    # Deserialization & File I/O
    # -------------------------------------------------------------------------
    @classmethod
    def from_binary(cls, blob: bytes) -> "Histogram":
        """Deserialize histogram from canonical binary wire format."""
        raw = _libhisto.Histo1D.deserialize_binary(blob)
        return cls(_raw=raw)

    @classmethod
    def from_json(cls, json_str: str) -> "Histogram":
        """Deserialize histogram from JSON string."""
        raw = _libhisto.Histo1D.deserialize_json(json_str)
        return cls(_raw=raw)

    @classmethod
    def from_file(cls, path: str, format: str = "binary") -> "Histogram":
        """Read and deserialize histogram from file."""
        mode = "rb" if format.lower() == "binary" else "r"
        with open(path, mode) as f:
            content = f.read()
        if format.lower() == "binary":
            return cls.from_binary(content)
        return cls.from_json(content)

    @staticmethod
    def migrate_binary(blob: bytes) -> bytes:
        """Migrate older binary format to the latest version."""
        return _libhisto.Histo1D.migrate_binary(blob)

    def to_binary(self) -> bytes:
        """Serialize histogram to Little-Endian binary bytes."""
        return self._raw.serialize_binary()

    def serialize_binary(self) -> bytes:
        """Alias for to_binary."""
        return self.to_binary()

    def to_json(self) -> str:
        """Serialize histogram to JSON string."""
        return self._raw.serialize_json()

    def serialize_json(self) -> str:
        """Alias for to_json."""
        return self.to_json()

    def to_file(self, path: str, format: str = "binary") -> None:
        """Write serialized histogram to file."""
        mode = "wb" if format.lower() == "binary" else "w"
        content = self.to_binary() if format.lower() == "binary" else self.to_json()
        with open(path, mode) as f:
            f.write(content)

    # -------------------------------------------------------------------------
    # NumPy & Universal Histogram Interface (UHI) Interoperability
    # -------------------------------------------------------------------------
    def to_numpy(self, flow: bool = False) -> Tuple[Any, Any]:
        """
        Convert histogram to (counts, bin_edges) matching numpy.histogram format.

        Parameters
        ----------
        flow : bool, default=False
            If True, includes underflow at index 0 and overflow at index -1.
        """
        np = require_numpy("Histogram.to_numpy")
        edges = self.axes[0].edges
        if not flow:
            counts = np.array([self.bin_content(i) for i in range(self.nbins)], dtype=np.float64)
        else:
            counts = np.array(
                [self.underflow] + [self.bin_content(i) for i in range(self.nbins)] + [self.overflow],
                dtype=np.float64
            )
        return counts, edges

    @classmethod
    def from_numpy(cls, counts: Any, bin_edges: Any, track_sumw2: bool = False) -> "Histogram":
        """
        Construct a Histogram from existing NumPy counts and bin_edges arrays.
        """
        counts_list = [float(x) for x in counts]
        edges_list = [float(x) for x in bin_edges]
        if len(edges_list) != len(counts_list) + 1:
            raise ValueError(
                f"bin_edges length ({len(edges_list)}) must be len(counts) + 1 ({len(counts_list) + 1})"
            )

        n = len(counts_list)
        # Detect uniformity within floating point tolerance
        dx0 = edges_list[1] - edges_list[0]
        is_uniform = True
        for i in range(1, n):
            if abs((edges_list[i + 1] - edges_list[i]) - dx0) > 1e-10 * max(1.0, abs(dx0)):
                is_uniform = False
                break

        if is_uniform:
            h = cls(bins=n, range=(edges_list[0], edges_list[-1]), track_sumw2=track_sumw2)
        else:
            h = cls(edges=edges_list, track_sumw2=track_sumw2)

        for i, val in enumerate(counts_list):
            if val != 0.0:
                h.fill_bin(i, val)
        return h

    def __array__(self, dtype=None, copy=None) -> Any:
        """NumPy array interface returning bin contents as a 1D ndarray."""
        np = require_numpy("Histogram.__array__")
        arr = np.array([self.bin_content(i) for i in range(self.nbins)], dtype=np.float64)
        if dtype is not None and arr.dtype != dtype:
            return arr.astype(dtype, copy=copy)
        if copy:
            return arr.copy()
        return arr

    @property
    def axes(self) -> Tuple[HistogramAxis]:
        """UHI axes tuple."""
        return (HistogramAxis(self.nbins, self.min, self.max, raw_histo=self._raw),)

    def values(self, flow: bool = False) -> Any:
        """
        UHI values protocol returning NumPy array of bin counts.
        
        Parameters
        ----------
        flow : bool, default=False
            If True, returns array of length N+2 [underflow, *bins, overflow].
            If False, returns array of length N.
        """
        np = require_numpy("Histogram.values")
        if not flow:
            return np.array([self.bin_content(i) for i in range(self.nbins)], dtype=np.float64)
        return np.array(
            [self.underflow] + [self.bin_content(i) for i in range(self.nbins)] + [self.overflow],
            dtype=np.float64
        )

    def variances(self, flow: bool = False) -> Optional[Any]:
        """
        UHI variances protocol returning array of sum_w2 variances.
        
        Parameters
        ----------
        flow : bool, default=False
            If True, returns array of length N+2 [underflow_w2, *bins_w2, overflow_w2].
            If False, returns array of length N.
        """
        np = require_numpy("Histogram.variances")
        if not flow:
            return np.array([self.bin_sum_w2(i) for i in range(self.nbins)], dtype=np.float64)
        u_w2 = getattr(self._raw, "underflow_sum_w2", self.underflow)
        o_w2 = getattr(self._raw, "overflow_sum_w2", self.overflow)
        return np.array(
            [u_w2] + [self.bin_sum_w2(i) for i in range(self.nbins)] + [o_w2],
            dtype=np.float64
        )

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

    def to_scipy_dist(self) -> Any:
        """
        Convert histogram to a scipy.stats.rv_histogram continuous distribution object.

        Returns
        -------
        scipy.stats.rv_histogram
            Continuous random variable distribution interpolated from histogram bins.
        """
        try:
            import scipy.stats
        except ImportError:
            raise ImportError(
                "to_scipy_dist() requires scipy. "
                "Please install scipy using 'pip install scipy' or 'pip install histo[scipy]'."
            )

        np = require_numpy("Histogram.to_scipy_dist")
        counts = np.array([self.bin_content(i) for i in range(self.nbins)], dtype=np.float64)
        edges = self.axes[0].edges
        return scipy.stats.rv_histogram((counts, edges))

    def to_boost(self) -> Any:
        """
        Convert Histogram to a boost_histogram.Histogram object.

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

        ax = self.axes[0]
        edges = [float(x) for x in ax.edges]
        is_uniform = True
        dx0 = edges[1] - edges[0]
        for i in range(1, len(edges) - 1):
            if abs((edges[i + 1] - edges[i]) - dx0) > 1e-10 * max(1.0, abs(dx0)):
                is_uniform = False
                break

        if is_uniform:
            b_ax = bh.axis.Regular(self.nbins, self.min, self.max, underflow=True, overflow=True)
        else:
            b_ax = bh.axis.Variable(edges, underflow=True, overflow=True)

        has_w2 = (self.flags & FLAG_TRACK_SUMW2) != 0
        storage = bh.storage.Weight() if has_w2 else bh.storage.Double()
        bh_h = bh.Histogram(b_ax, storage=storage)

        vals = self.values(flow=True)
        if has_w2:
            vars_arr = self.variances(flow=True)
            bh_h.view(flow=True).value = vals
            bh_h.view(flow=True).variance = vars_arr
        else:
            bh_h.view(flow=True)[:] = vals

        return bh_h

    @classmethod
    def from_boost(cls, bh_histo: Any) -> "Histogram":
        """
        Construct a libhisto Histogram from a 1D boost-histogram Histogram.
        """
        if len(bh_histo.axes) != 1:
            raise ValueError(f"Expected 1D boost-histogram, got {len(bh_histo.axes)}D")

        b_ax = bh_histo.axes[0]
        edges = [float(x) for x in b_ax.edges]
        has_sumw2 = hasattr(bh_histo.view(), "variance")

        h = cls.from_numpy(bh_histo.values(), edges, track_sumw2=has_sumw2)
        if hasattr(bh_histo, "view"):
            view = bh_histo.view(flow=True)
            if hasattr(view, "value"):
                underflow_val = float(view.value[0])
                overflow_val = float(view.value[-1])
            else:
                underflow_val = float(view[0])
                overflow_val = float(view[-1])
            if underflow_val > 0:
                h._raw.fill(float(edges[0]) - 1.0, weight=underflow_val)
            if overflow_val > 0:
                h._raw.fill(float(edges[-1]) + 1.0, weight=overflow_val)
        return h

    # -------------------------------------------------------------------------
    # Ingestion Methods
    # -------------------------------------------------------------------------
    def fill(self, x: float, weight: float = 1.0) -> bool:
        """Fill single sample with optional weight. Returns True on success."""
        return self._raw.fill(float(x), weight=float(weight))

    def fill_n(self, x: Sequence[float], weights: Optional[Sequence[float]] = None) -> bool:
        """Batch fill samples from Python sequences."""
        return self._raw.fill_n(x, weights=weights)

    def fill_buffer(self, x_buf: Any, weights_buf: Any = None) -> bool:
        """
        SIMD batch fill from array-like or buffer objects (numpy array, memoryview, array.array, list).
        Transparently converts and aligns dtypes if needed.
        """
        x_buf_c = as_float64_buffer(x_buf)
        w_buf_c = as_float64_buffer(weights_buf) if weights_buf is not None else None
        return self._raw.fill_buffer(x_buf_c, weights=w_buf_c)

    def fill_packed(self, x_buf: Any, weights_buf: Any = None) -> bool:
        """Alias for fill_buffer."""
        return self.fill_buffer(x_buf, weights_buf)

    def fill_bin(self, bin_index: int, weight: float = 1.0) -> bool:
        """Directly accumulate weight into specific bin index."""
        return self._raw.fill_bin(int(bin_index), float(weight))

    def reset(self) -> None:
        """Reset all bin contents, moments, and guard counters to zero."""
        self._raw.reset()

    def clone(self, empty: bool = False) -> "Histogram":
        """Create exact clone or empty copy with identical binning."""
        raw_clone = self._raw.clone(empty=empty)
        return Histogram(_raw=raw_clone)

    # -------------------------------------------------------------------------
    # Properties & Geometry
    # -------------------------------------------------------------------------
    @property
    def nbins(self) -> int:
        """Number of bins."""
        return self._raw.nbins

    def __len__(self) -> int:
        return self.nbins

    @property
    def min(self) -> float:
        """Lower range boundary."""
        return self._raw.min

    @property
    def max(self) -> float:
        """Upper range boundary."""
        return self._raw.max

    @property
    def total_weight(self) -> float:
        """Total in-range accumulated weight."""
        return self._raw.total_weight

    @property
    def num_entries(self) -> int:
        """Total fill operations performed."""
        return self._raw.num_entries

    @property
    def underflow(self) -> float:
        """Accumulated underflow weight."""
        return self._raw.underflow

    @property
    def overflow(self) -> float:
        """Accumulated overflow weight."""
        return self._raw.overflow

    @property
    def nan_count(self) -> int:
        """Count of non-finite (NaN/Inf) samples skipped."""
        return self._raw.nan_count

    # -------------------------------------------------------------------------
    # Statistical Moments & Summaries
    # -------------------------------------------------------------------------
    @property
    def mean(self) -> float:
        """Sample mean."""
        return self._raw.mean

    @property
    def variance(self) -> float:
        """Sample variance."""
        return self._raw.variance

    @property
    def std_dev(self) -> float:
        """Sample standard deviation."""
        return self._raw.std_dev

    @property
    def skewness(self) -> float:
        """Sample skewness."""
        return self._raw.skewness

    @property
    def kurtosis(self) -> float:
        """Sample kurtosis."""
        return self._raw.kurtosis

    @property
    def excess_kurtosis(self) -> float:
        """Sample excess kurtosis (kurtosis - 3.0)."""
        return self._raw.excess_kurtosis

    @property
    def median(self) -> float:
        """Estimated median coordinate (50th percentile)."""
        return self._raw.median

    @property
    def iqr(self) -> float:
        """Interquartile range (Q75 - Q25)."""
        return self._raw.iqr

    @property
    def mad(self) -> float:
        """Median Absolute Deviation."""
        return self._raw.mad

    @property
    def mode(self) -> float:
        """Sub-bin continuous peak estimate (parabolic interpolation)."""
        return self._raw.mode

    @property
    def fwhm(self) -> float:
        """Full Width at Half Maximum."""
        return self._raw.fwhm

    @property
    def rms(self) -> float:
        """Root Mean Square."""
        return self._raw.rms

    @property
    def stats(self) -> Dict[str, Any]:
        """Comprehensive dictionary of statistical moments and properties."""
        return self._raw.get_stats()

    # -------------------------------------------------------------------------
    # Bin Accessors
    # -------------------------------------------------------------------------
    def find_bin(self, x: float) -> int:
        """Locate bin index for coordinate x (-1 underflow, nbins overflow)."""
        return self._raw.find_bin(float(x))

    def bin_content(self, bin_index: int) -> float:
        """Get accumulated weight in bin_index."""
        return self._raw.bin_content(int(bin_index))

    def bin_error(self, bin_index: int) -> float:
        """Get statistical standard error in bin_index."""
        return self._raw.bin_error(int(bin_index))

    def bin_sum_w2(self, bin_index: int) -> float:
        """Get sum of squared weights sum(w^2) in bin_index."""
        return self._raw.bin_sum_w2(int(bin_index))

    def bin_center(self, bin_index: int) -> float:
        """Get midpoint coordinate of bin_index."""
        return self._raw.bin_center(int(bin_index))

    def bin_bounds(self, bin_index: int) -> Tuple[float, float]:
        """Get (lower, upper) boundaries of bin_index."""
        return self._raw.bin_bounds(int(bin_index))

    def __getitem__(self, key: Any) -> Union[float, "Histogram"]:
        """
        Indexing and slicing with UHI locators and rebin support:
        - h[i]: returns bin content at integer index i.
        - h[loc(x)]: returns bin content at coordinate x.
        - h[start:stop]: returns sliced Histogram across [start, stop).
        - h[start:stop:rebin(n)] or h[start:stop:nj]: slices and rebins.
        """
        if isinstance(key, int):
            if key < 0:
                key += self.nbins
            if key < 0 or key >= self.nbins:
                raise IndexError(f"Bin index {key} out of range [0, {self.nbins - 1}]")
            return self.bin_content(key)

        if isinstance(key, loc):
            idx = self.axes[0].index(key.value) + key.offset
            if idx < 0 or idx >= self.nbins:
                raise IndexError(f"loc({key.value}) resolves to out-of-bounds bin {idx}")
            return self.bin_content(idx)

        if key is uhi_underflow or type(key).__name__ == "_UnderflowTag":
            return self.underflow

        if key is uhi_overflow or type(key).__name__ == "_OverflowTag":
            return self.overflow

        if isinstance(key, slice):
            # 1. Parse start
            if key.start is None:
                start_idx = 0
            elif isinstance(key.start, loc):
                start_idx = self.axes[0].index(key.start.value) + key.start.offset
            elif key.start is uhi_underflow or type(key.start).__name__ == "_UnderflowTag":
                start_idx = 0
            elif isinstance(key.start, int):
                start_idx = key.start
                if start_idx < 0:
                    start_idx += self.nbins
            else:
                start_idx = int(key.start)
            start_idx = max(0, min(self.nbins - 1, start_idx))

            # 2. Parse stop
            if key.stop is None:
                stop_idx = self.nbins - 1
            elif isinstance(key.stop, loc):
                idx = self.axes[0].index(key.stop.value) + key.stop.offset
                stop_idx = idx - 1 if idx > 0 else 0
            elif key.stop is uhi_overflow or type(key.stop).__name__ == "_OverflowTag":
                stop_idx = self.nbins - 1
            elif isinstance(key.stop, int):
                idx = key.stop
                if idx < 0:
                    idx += self.nbins
                stop_idx = idx
            else:
                idx = int(key.stop)
                stop_idx = idx
            stop_idx = max(start_idx, min(self.nbins - 1, stop_idx))

            # 3. Parse step (rebin)
            rebin_factor = 1
            if key.step is not None:
                if isinstance(key.step, rebin):
                    rebin_factor = key.step.factor
                elif isinstance(key.step, complex):
                    if key.step.real == 0 and key.step.imag > 0:
                        rebin_factor = int(round(key.step.imag))
                    else:
                        raise ValueError(f"Invalid complex slice step: {key.step}")
                elif isinstance(key.step, int) and key.step > 0:
                    rebin_factor = key.step
                else:
                    raise TypeError(f"Invalid slice step: {key.step}")

            if key.start is None and key.stop is None:
                res = self.clone()
            else:
                res = self.slice(start_idx, stop_idx)

            if rebin_factor > 1:
                res = res.rebin(rebin_factor)

            return res

        raise TypeError(f"Invalid index type: {type(key)}")

    def __iter__(self):
        for i in range(self.nbins):
            yield self.bin_content(i)

    # -------------------------------------------------------------------------
    # Analytical Functions
    # -------------------------------------------------------------------------
    def central_moment(self, k: int) -> float:
        """Compute k-th central statistical moment."""
        return self._raw.central_moment(int(k))

    def quantile(self, p: float) -> float:
        """Compute quantile coordinate for p in [0.0, 1.0]."""
        return self._raw.quantile(float(p))

    def trimmed_mean(self, lower_p: float, upper_p: float) -> float:
        """Compute trimmed mean excluding tails."""
        return self._raw.trimmed_mean(float(lower_p), float(upper_p))

    def winsorized_mean(self, lower_p: float, upper_p: float) -> float:
        """Compute Winsorized mean replacing tails with quantile thresholds."""
        return self._raw.winsorized_mean(float(lower_p), float(upper_p))

    def integral(self, start: int = 0, end: Optional[int] = None) -> float:
        """Compute integral across bin range [start, end]."""
        if end is None:
            end = self.nbins - 1
        return self._raw.integral(int(start), int(end))

    # -------------------------------------------------------------------------
    # Two-Sample Comparison Metrics
    # -------------------------------------------------------------------------
    def chi2_test(self, other: "Histogram") -> Tuple[float, int]:
        """Chi-Square test of compatibility returning (chi2, ndf)."""
        if not isinstance(other, Histogram):
            raise TypeError("other must be a Histogram instance")
        return self._raw.chi2_test(other._raw)

    def kolmogorov_smirnov(self, other: "Histogram") -> float:
        """Kolmogorov-Smirnov test statistic D in [0.0, 1.0]."""
        if not isinstance(other, Histogram):
            raise TypeError("other must be a Histogram instance")
        return self._raw.ks_test(other._raw)

    def wasserstein_distance(self, other: "Histogram") -> float:
        """1D Wasserstein distance (Earth Mover's Distance)."""
        if not isinstance(other, Histogram):
            raise TypeError("other must be a Histogram instance")
        return self._raw.wasserstein_distance(other._raw)

    def kl_divergence(self, other: "Histogram") -> float:
        """Kullback-Leibler divergence D_KL(self || other)."""
        if not isinstance(other, Histogram):
            raise TypeError("other must be a Histogram instance")
        return self._raw.kl_divergence(other._raw)

    def bhattacharyya_distance(self, other: "Histogram") -> float:
        """Bhattacharyya distance between two histograms."""
        if not isinstance(other, Histogram):
            raise TypeError("other must be a Histogram instance")
        return self._raw.bhattacharyya_distance(other._raw)

    # -------------------------------------------------------------------------
    # Transformations & Arithmetic Operators
    # -------------------------------------------------------------------------
    def add(self, other: "Histogram") -> "Histogram":
        """In-place addition: self += other."""
        if not isinstance(other, Histogram):
            raise TypeError("other must be a Histogram instance")
        self._raw.add(other._raw)
        return self

    def subtract(self, other: "Histogram") -> "Histogram":
        """In-place subtraction: self -= other."""
        if not isinstance(other, Histogram):
            raise TypeError("other must be a Histogram instance")
        self._raw.subtract(other._raw)
        return self

    def multiply(self, other: "Histogram") -> "Histogram":
        """In-place elementwise multiplication: self *= other."""
        if not isinstance(other, Histogram):
            raise TypeError("other must be a Histogram instance")
        self._raw.multiply(other._raw)
        return self

    def divide(self, other: "Histogram") -> "Histogram":
        """In-place elementwise division: self /= other."""
        if not isinstance(other, Histogram):
            raise TypeError("other must be a Histogram instance")
        self._raw.divide(other._raw)
        return self

    def scale(self, factor: float) -> "Histogram":
        """In-place scaling by scalar factor."""
        self._raw.scale(float(factor))
        return self

    def normalize(self, target_area: float = 1.0) -> "Histogram":
        """In-place area normalization."""
        self._raw.normalize(float(target_area))
        return self

    def rebin(self, factor: int) -> "Histogram":
        """Rebin by integer grouping factor, returning a new Histogram."""
        raw_rebinned = self._raw.rebin(int(factor))
        return Histogram(_raw=raw_rebinned)

    def slice(self, start: int, end: int) -> "Histogram":
        """Slice subset of bins [start, end], returning a new Histogram."""
        raw_sliced = self._raw.slice(int(start), int(end))
        return Histogram(_raw=raw_sliced)

    def cdf(self, normalized: bool = True) -> "Histogram":
        """Compute Cumulative Distribution Function (CDF) histogram."""
        raw_cdf = self._raw.cdf(bool(normalized))
        return Histogram(_raw=raw_cdf)

    def __add__(self, other: "Histogram") -> "Histogram":
        res = self.clone()
        return res.add(other)

    def __sub__(self, other: "Histogram") -> "Histogram":
        res = self.clone()
        return res.subtract(other)

    def __mul__(self, other: Union["Histogram", float, int]) -> "Histogram":
        res = self.clone()
        if isinstance(other, (int, float)):
            return res.scale(float(other))
        return res.multiply(other)

    def __rmul__(self, other: Union[float, int]) -> "Histogram":
        return self.__mul__(other)

    def __truediv__(self, other: Union["Histogram", float, int]) -> "Histogram":
        res = self.clone()
        if isinstance(other, (int, float)):
            if float(other) == 0.0:
                raise ZeroDivisionError("division by zero")
            return res.scale(1.0 / float(other))
        return res.divide(other)

    # -------------------------------------------------------------------------
    # Curve Fitting
    # -------------------------------------------------------------------------
    def fit(
        self,
        model: str = "gaussian",
        initial: Optional[Sequence[float]] = None,
        lower: Optional[Sequence[float]] = None,
        upper: Optional[Sequence[float]] = None,
        fixed: Optional[Sequence[bool]] = None,
        degree: int = 1,
        max_iter: int = 200,
        tol: float = 1e-8,
        loss: str = "chi2",
    ) -> FitResult:
        """
        Fit built-in model to histogram.

        Supported models: 'gaussian', 'exponential', 'polynomial', 'breit_wigner', 'power_law',
        'lognormal', 'gauss+linear', 'weibull', 'gamma', 'poisson', 'laplace'.
        """
        m_code = MODEL_MAP.get(model.lower())
        if m_code is None:
            raise ValueError(f"Unknown model '{model}'. Valid: {list(MODEL_MAP.keys())}")
        l_code = LOSS_MAP.get(loss.lower(), 0)

        raw_res = self._raw.fit_builtin(
            model=m_code,
            initial=initial,
            lower=lower,
            upper=upper,
            fixed=fixed,
            degree=degree,
            max_iter=max_iter,
            tol=tol,
            loss=l_code,
        )
        return FitResult(raw_res)

    def fit_custom(
        self,
        model_fn: Callable[[float, Sequence[float]], float],
        n_params: int,
        initial: Optional[Sequence[float]] = None,
        lower: Optional[Sequence[float]] = None,
        upper: Optional[Sequence[float]] = None,
        fixed: Optional[Sequence[bool]] = None,
        max_iter: int = 200,
        tol: float = 1e-8,
        loss: str = "chi2",
    ) -> FitResult:
        """
        Fit custom Python callable model_fn(x, params) -> y.
        """
        l_code = LOSS_MAP.get(loss.lower(), 0)
        raw_res = self._raw.fit_custom(
            model_fn=model_fn,
            n_params=int(n_params),
            initial=initial,
            lower=lower,
            upper=upper,
            fixed=fixed,
            max_iter=max_iter,
            tol=tol,
            loss=l_code,
        )
        return FitResult(raw_res)

    def plot(
        self,
        style: str = "unicode",
        color: Optional[Union[bool, str]] = None,
        palette: str = "viridis",
        sparkline: bool = False,
        width: Optional[int] = None,
        height: Optional[int] = None,
        log: bool = False,
        errors: bool = False,
        fit: Optional[str] = None,
        kde: bool = False,
        cdf: bool = False,
        show: bool = True,
    ) -> str:
        """
        Render terminal visualization of the histogram.
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
        if sparkline:
            args.append("-S")
        if width is not None:
            args.append(f"-w={width}")
        if height is not None:
            args.append(f"-H={height}")
        if log:
            args.append("-l")
        if errors:
            args.append("-e")
        if fit:
            args.append(f"--fit={fit}")
        if kde:
            args.append("--kde")
        if cdf:
            args.append("--cdf")

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

    def sparkline(self, color: Optional[Union[bool, str]] = None, show: bool = True) -> str:
        """Render a single-line Unicode sparkline summary."""
        return self.plot(sparkline=True, color=color, show=show)

    def __repr__(self) -> str:
        return f"Histogram({self.nbins} bins, range=({self.min:.4g}, {self.max:.4g}), entries={self.num_entries})"
