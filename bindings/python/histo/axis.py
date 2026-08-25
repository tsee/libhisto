"""
Universal Histogram Interface (UHI) compliant Axis implementation for libhisto.
"""

import math
import bisect
from typing import Tuple, Any, Optional, Sequence, Iterator
from histo.compat import HAS_NUMPY, require_numpy
from histo.uhi import AxisTraits, PlottableTraits


class Interval:
    """Interval representing a 1D bin range [lower, upper)."""
    def __init__(self, lower: float, upper: float):
        self.lower = float(lower)
        self.upper = float(upper)

    def __iter__(self) -> Iterator[float]:
        yield self.lower
        yield self.upper

    def __getitem__(self, idx: int) -> float:
        if idx == 0 or idx == -2:
            return self.lower
        if idx == 1 or idx == -1:
            return self.upper
        raise IndexError("Interval index out of range (0 or 1)")

    def __len__(self) -> int:
        return 2

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
    def __init__(
        self,
        nbins: int,
        min_val: float,
        max_val: float,
        raw_histo: Any = None,
        axis_idx: int = 0,
        edges: Optional[Sequence[float]] = None,
    ):
        self._nbins = int(nbins)
        self._min = float(min_val)
        self._max = float(max_val)
        self._raw = raw_histo
        self._axis_idx = int(axis_idx)
        self._explicit_edges = [float(x) for x in edges] if edges is not None else None
        self.traits = AxisTraits(circular=False, discrete=False, growth=False)

    def __len__(self) -> int:
        return self._nbins

    @property
    def min(self) -> float:
        """Lower coordinate boundary of the axis."""
        return self._min

    @property
    def max(self) -> float:
        """Upper coordinate boundary of the axis."""
        return self._max

    def __getitem__(self, idx: int) -> Interval:
        if idx < 0:
            idx += self._nbins
        if idx < 0 or idx >= self._nbins:
            raise IndexError(f"Axis index {idx} out of range [0, {self._nbins - 1}]")

        if self._explicit_edges is not None:
            return Interval(self._explicit_edges[idx], self._explicit_edges[idx + 1])

        if self._raw is not None and hasattr(self._raw, "bin_bounds"):
            if self._axis_idx == 0 and not hasattr(self._raw, "nx"):
                lo, hi = self._raw.bin_bounds(idx)
                return Interval(lo, hi)
            elif hasattr(self._raw, "nx"):
                # 2D histogram
                if self._axis_idx == 0:
                    xmin, xmax, _, _ = self._raw.bin_bounds(idx, 0)
                    return Interval(xmin, xmax)
                else:
                    _, _, ymin, ymax = self._raw.bin_bounds(0, idx)
                    return Interval(ymin, ymax)

        # Uniform fallback
        dx = (self._max - self._min) / self._nbins
        return Interval(self._min + idx * dx, self._min + (idx + 1) * dx)

    def bin(self, idx: int) -> Interval:
        """Alias for self[idx] returning bin Interval [lower, upper)."""
        return self[idx]

    def index(self, value: float) -> int:
        """
        Locate integer bin index for continuous coordinate value.

        Returns
        -------
        int
            Index in [0, len(self) - 1] for in-range coordinate,
            -1 for underflow (value < min), or len(self) for overflow (value >= max).
        """
        val = float(value)
        if math.isnan(val):
            raise ValueError("Cannot locate bin for NaN coordinate")

        if val < self._min:
            return -1
        if val >= self._max:
            return self._nbins

        if self._explicit_edges is not None:
            pos = bisect.bisect_right(self._explicit_edges, val) - 1
            return max(0, min(self._nbins - 1, pos))

        if self._raw is not None:
            if self._axis_idx == 0 and not hasattr(self._raw, "nx") and hasattr(self._raw, "find_bin"):
                return self._raw.find_bin(val)
            elif hasattr(self._raw, "nx") and hasattr(self._raw, "find_bin"):
                ix, iy = self._raw.find_bin(val if self._axis_idx == 0 else self._min,
                                           val if self._axis_idx == 1 else self._min)
                return ix if self._axis_idx == 0 else iy

        # Uniform calculation
        dx = (self._max - self._min) / self._nbins
        idx = int(math.floor((val - self._min) / dx))
        return max(0, min(self._nbins - 1, idx))

    def __iter__(self) -> Iterator[Interval]:
        for i in range(self._nbins):
            yield self[i]

    @property
    def edges(self) -> Any:
        """NumPy array (or list) of bin edges [e_0, e_1, ..., e_N]."""
        if self._explicit_edges is not None:
            edges_list = self._explicit_edges
        else:
            edges_list = [self[i].lower for i in range(self._nbins)]
            edges_list.append(self[self._nbins - 1].upper)

        if HAS_NUMPY:
            np = require_numpy("Axis.edges")
            return np.array(edges_list, dtype=np.float64)
        return edges_list

    @property
    def centers(self) -> Any:
        """NumPy array (or list) of bin midpoints."""
        if self._raw is not None and hasattr(self._raw, "bin_center"):
            if self._axis_idx == 0 and not hasattr(self._raw, "nx"):
                centers_list = [self._raw.bin_center(i) for i in range(self._nbins)]
            elif hasattr(self._raw, "nx"):
                if self._axis_idx == 0:
                    centers_list = [self._raw.bin_center(i, 0)[0] for i in range(self._nbins)]
                else:
                    centers_list = [self._raw.bin_center(0, i)[1] for i in range(self._nbins)]
            else:
                centers_list = [(self[i].lower + self[i].upper) * 0.5 for i in range(self._nbins)]
        else:
            centers_list = [(self[i].lower + self[i].upper) * 0.5 for i in range(self._nbins)]

        if HAS_NUMPY:
            np = require_numpy("Axis.centers")
            return np.array(centers_list, dtype=np.float64)
        return centers_list

    @property
    def widths(self) -> Any:
        """NumPy array (or list) of bin widths."""
        widths_list = [(self[i].upper - self[i].lower) for i in range(self._nbins)]
        if HAS_NUMPY:
            np = require_numpy("Axis.widths")
            return np.array(widths_list, dtype=np.float64)
        return widths_list

    def __repr__(self) -> str:
        return f"HistogramAxis({self._nbins} bins, range=({self._min}, {self._max}))"


HistogramAxis = Axis
