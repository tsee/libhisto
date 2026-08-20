"""
DDSketch Online Dynamic Quantile Sketch wrapper.
"""

from typing import Sequence, Optional, Union
import _libhisto


class Sketch:
    """
    DDSketch logarithmic bounded relative-error online quantile sketch.
    """
    def __init__(self, alpha: float = 0.01, max_bins: int = 2048, _raw: Optional[_libhisto.Sketch] = None):
        if _raw is not None:
            self._raw = _raw
        else:
            self._raw = _libhisto.Sketch.create(alpha=alpha, max_bins=max_bins)

    @classmethod
    def from_binary(cls, blob: bytes) -> "Sketch":
        """Deserialize sketch from binary format."""
        raw = _libhisto.Sketch.deserialize_binary(blob)
        return cls(_raw=raw)

    def insert(self, value: float, weight: float = 1.0) -> bool:
        """Stream a single value with optional weight."""
        return self._raw.insert(float(value), float(weight))

    def insert_n(self, values: Sequence[float], weights: Optional[Sequence[float]] = None) -> bool:
        """Stream a sequence of values with optional weights."""
        return self._raw.insert_n(values, weights)

    def insert_buffer(self, values_buf, weights_buf=None) -> bool:
        """Zero-copy stream from Python buffer (memoryview, numpy array, array.array)."""
        return self._raw.insert_buffer(values_buf, weights_buf)

    def quantile(self, q: float) -> float:
        """Query quantile for q in [0, 1] with alpha relative error guarantee."""
        return self._raw.quantile(float(q))

    def merge(self, other: "Sketch") -> bool:
        """Merge another Sketch into this sketch in-place."""
        if not isinstance(other, Sketch):
            raise TypeError("other must be a Sketch instance")
        return self._raw.merge(other._raw)

    def reset(self) -> None:
        """Reset sketch state to empty."""
        self._raw.reset()

    @property
    def min(self) -> float:
        """Minimum value observed."""
        return self._raw.min

    @property
    def max(self) -> float:
        """Maximum value observed."""
        return self._raw.max

    @property
    def total_weight(self) -> float:
        """Total accumulated weight."""
        return self._raw.total_weight

    @property
    def num_entries(self) -> int:
        """Total number of sample insertions."""
        return self._raw.num_entries

    def __len__(self) -> int:
        return self.num_entries

    def serialize_binary(self) -> bytes:
        """Serialize sketch to Little-Endian binary bytes."""
        return self._raw.serialize_binary()

    def to_binary(self) -> bytes:
        """Alias for serialize_binary."""
        return self.serialize_binary()

    def __repr__(self) -> str:
        return f"<Sketch entries={self.num_entries} weight={self.total_weight:.2f} min={self.min:.4f} max={self.max:.4f}>"
