"""
Universal Histogram Interface (UHI) compliant Axis implementation for libhisto.
"""

from typing import Tuple, Any, Optional
from histo.compat import HAS_NUMPY, require_numpy


class AxisTraits:
    """UHI traits descriptor."""
    def __init__(self, circular: bool = False, discrete: bool = False, growth: bool = False):
        self.circular = circular
        self.discrete = discrete
        self.growth = growth

    def __repr__(self) -> str:
        return f"Traits(circular={self.circular}, discrete={self.discrete}, growth={self.growth})"


class Interval:
    """Interval representing a 1D bin range [lower, upper)."""
    def __init__(self, lower: float, upper: float):
        self.lower = float(lower)
        self.upper = float(upper)

    def __iter__(self):
        yield self.lower
        yield self.upper

    def __getitem__(self, idx: int) -> float:
        if idx == 0:
            return self.lower
        if idx == 1:
            return self.upper
        raise IndexError("Interval index out of range (0 or 1)")

    def __repr__(self) -> str:
        return f"Interval({self.lower}, {self.upper})"

    def __eq__(self, other: Any) -> bool:
        if isinstance(other, Interval):
            return self.lower == other.lower and self.upper == other.upper
        if isinstance(other, (tuple, list)) and len(other) == 2:
            return self.lower == other[0] and self.upper == other[1]
        return False


class Axis:
    """
    Universal Histogram Interface (UHI) Axis object.
    Represents an equidistant (regular) or variable 1D histogram axis.
    """
    def __init__(self, nbins: int, min_val: float, max_val: float, raw_histo: Any = None, axis_idx: int = 0):
        self._nbins = int(nbins)
        self._min = float(min_val)
        self._max = float(max_val)
        self._raw = raw_histo
        self._axis_idx = axis_idx
        self.traits = AxisTraits()

    def __len__(self) -> int:
        return self._nbins

    def __getitem__(self, idx: int) -> Interval:
        if idx < 0:
            idx += self._nbins
        if idx < 0 or idx >= self._nbins:
            raise IndexError(f"Axis index {idx} out of range [0, {self._nbins - 1}]")
        
        if self._raw and hasattr(self._raw, "bin_bounds"):
            lo, hi = self._raw.bin_bounds(idx)
            return Interval(lo, hi)
        
        # Uniform fallback
        dx = (self._max - self._min) / self._nbins
        return Interval(self._min + idx * dx, self._min + (idx + 1) * dx)

    @property
    def edges(self) -> Any:
        """NumPy array of bin edges [e_0, e_1, ..., e_N]."""
        np = require_numpy("Axis.edges")
        edges_list = [self[i].lower for i in range(self._nbins)]
        edges_list.append(self[self._nbins - 1].upper)
        return np.array(edges_list, dtype=np.float64)

    @property
    def centers(self) -> Any:
        """NumPy array of bin midpoints."""
        np = require_numpy("Axis.centers")
        if self._raw and hasattr(self._raw, "bin_center"):
            return np.array([self._raw.bin_center(i) for i in range(self._nbins)], dtype=np.float64)
        return np.array([(self[i].lower + self[i].upper) * 0.5 for i in range(self._nbins)], dtype=np.float64)

    @property
    def widths(self) -> Any:
        """NumPy array of bin widths."""
        np = require_numpy("Axis.widths")
        return np.array([(self[i].upper - self[i].lower) for i in range(self._nbins)], dtype=np.float64)

    def __repr__(self) -> str:
        return f"Axis({self._nbins} bins, range=({self._min}, {self._max}))"
