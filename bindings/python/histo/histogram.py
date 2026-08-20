"""
1-Dimensional Histogram implementation for libhisto Python bindings.
"""

import math
from typing import Sequence, Optional, Union, Tuple, Dict, Any, Callable
import _libhisto
from histo.constants import FLAG_NONE, FLAG_TRACK_SUMW2, FLAG_EXACT_MOMENTS
from histo.fit import FitResult, MODEL_MAP, LOSS_MAP


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
    # Ingestion Methods
    # -------------------------------------------------------------------------
    def fill(self, x: float, weight: float = 1.0) -> bool:
        """Fill single sample with optional weight. Returns True on success."""
        return self._raw.fill(float(x), weight=float(weight))

    def fill_n(self, x: Sequence[float], weights: Optional[Sequence[float]] = None) -> bool:
        """Batch fill samples from Python sequences."""
        return self._raw.fill_n(x, weights=weights)

    def fill_buffer(self, x_buf, weights_buf=None) -> bool:
        """Zero-copy SIMD batch fill from float64 buffer (numpy array, memoryview, array.array)."""
        return self._raw.fill_buffer(x_buf, weights=weights_buf)

    def fill_packed(self, x_buf, weights_buf=None) -> bool:
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
        """Number of non-finite (NaN / Inf) rejected samples."""
        return self._raw.nan_count

    # -------------------------------------------------------------------------
    # Statistical Moments & Summaries
    # -------------------------------------------------------------------------
    @property
    def mean(self) -> float:
        """Distribution mean."""
        return self._raw.mean

    @property
    def variance(self) -> float:
        """Distribution variance."""
        return self._raw.variance

    @property
    def std_dev(self) -> float:
        """Distribution standard deviation."""
        return self._raw.std_dev

    @property
    def skewness(self) -> float:
        """Distribution skewness."""
        return self._raw.skewness

    @property
    def kurtosis(self) -> float:
        """Distribution kurtosis."""
        return self._raw.kurtosis

    @property
    def excess_kurtosis(self) -> float:
        """Distribution excess kurtosis (kurtosis - 3.0)."""
        return self._raw.excess_kurtosis

    @property
    def median(self) -> float:
        """Distribution median (50th percentile)."""
        return self._raw.median

    @property
    def iqr(self) -> float:
        """Interquartile Range (Q75 - Q25)."""
        return self._raw.iqr

    @property
    def mad(self) -> float:
        """Median Absolute Deviation (MAD)."""
        return self._raw.mad

    @property
    def mode_bin(self) -> int:
        """Index of the bin with the highest accumulated weight."""
        return self._raw.mode_bin

    @property
    def mode(self) -> float:
        """Continuous mode peak estimate via 3-point parabolic interpolation."""
        return self._raw.mode

    @property
    def fwhm(self) -> float:
        """Full Width at Half Maximum of peak."""
        return self._raw.fwhm

    @property
    def rms(self) -> float:
        """Root Mean Square."""
        return self._raw.rms

    @property
    def stats(self) -> Dict[str, Any]:
        """Complete statistical summary dictionary."""
        return self._raw.get_stats()

    # -------------------------------------------------------------------------
    # Bin Access & Indexing
    # -------------------------------------------------------------------------
    def find_bin(self, x: float) -> int:
        """Locate bin index for coordinate x (-1 for underflow, nbins for overflow)."""
        return self._raw.find_bin(float(x))

    def bin_content(self, idx: int) -> float:
        """Accumulated weight in bin idx."""
        return self._raw.bin_content(int(idx))

    def bin_error(self, idx: int) -> float:
        """Statistical uncertainty in bin idx."""
        return self._raw.bin_error(int(idx))

    def bin_sum_w2(self, idx: int) -> float:
        """Sum of squared weights in bin idx."""
        return self._raw.bin_sum_w2(int(idx))

    def bin_center(self, idx: int) -> float:
        """Midpoint coordinate of bin idx."""
        return self._raw.bin_center(int(idx))

    def bin_bounds(self, idx: int) -> Tuple[float, float]:
        """Interval (lower, upper) for bin idx."""
        return self._raw.bin_bounds(int(idx))

    def __getitem__(self, key: Union[int, slice]) -> Union[float, "Histogram"]:
        if isinstance(key, slice):
            start = 0 if key.start is None else int(key.start)
            stop = (self.nbins - 1) if key.stop is None else int(key.stop)
            return self.slice(start, stop)
        elif isinstance(key, int):
            if key < 0:
                key += self.nbins
            if key < 0 or key >= self.nbins:
                raise IndexError(f"Bin index {key} out of range [0, {self.nbins - 1}]")
            return self.bin_content(key)
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
        """Bhattacharyya distance."""
        if not isinstance(other, Histogram):
            raise TypeError("other must be a Histogram instance")
        return self._raw.bhattacharyya_distance(other._raw)

    # -------------------------------------------------------------------------
    # Arithmetic & Transformations
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
        """In-place element-wise multiplication: self *= other."""
        if not isinstance(other, Histogram):
            raise TypeError("other must be a Histogram instance")
        self._raw.multiply(other._raw)
        return self

    def divide(self, other: "Histogram") -> "Histogram":
        """In-place element-wise division: self /= other."""
        if not isinstance(other, Histogram):
            raise TypeError("other must be a Histogram instance")
        self._raw.divide(other._raw)
        return self

    def scale(self, factor: float) -> "Histogram":
        """Scale all bin contents by scalar factor in-place."""
        self._raw.scale(float(factor))
        return self

    def normalize(self, target_area: float = 1.0) -> "Histogram":
        """Normalize total in-range weight to target_area in-place."""
        self._raw.normalize(float(target_area))
        return self

    def rebin(self, factor: int) -> "Histogram":
        """Rebin by integer factor, returning newly allocated rebinned Histogram."""
        raw_rebinned = self._raw.rebin(int(factor))
        return Histogram(_raw=raw_rebinned)

    def slice(self, start: int, end: int, empty: bool = False) -> "Histogram":
        """Slice bin range [start, end], returning newly allocated sub-Histogram."""
        raw_slice = self._raw.slice(int(start), int(end), empty)
        return Histogram(_raw=raw_slice)

    def cdf(self, prenormalization: float = 1.0) -> "Histogram":
        """Generate Cumulative Distribution Function (CDF) histogram."""
        raw_cdf = self._raw.cdf(float(prenormalization))
        return Histogram(_raw=raw_cdf)

    # -------------------------------------------------------------------------
    # Operator Overloading
    # -------------------------------------------------------------------------
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
            if other == 0:
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

        Supported models: 'gaussian', 'exponential', 'polynomial', 'breit_wigner', 'power_law'.
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

    def __repr__(self) -> str:
        return (f"<Histogram nbins={self.nbins} range=({self.min:.2f}, {self.max:.2f}) "
                f"entries={self.num_entries} weight={self.total_weight:.2f}>")
