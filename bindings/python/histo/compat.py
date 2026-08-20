"""
Compatibility and runtime dependency detection helpers for libhisto.
"""

import sys
import array
from typing import Any, Tuple, Optional

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    np = None  # type: ignore
    HAS_NUMPY = False


def require_numpy(feature: str = "This feature") -> Any:
    """Ensure NumPy is installed, otherwise raise informative ImportError."""
    if not HAS_NUMPY:
        raise ImportError(
            f"{feature} requires NumPy. "
            "Please install NumPy using 'pip install numpy' or 'pip install histo[numpy]'."
        )
    return np


def as_float64_buffer(obj: Any) -> Any:
    """
    Coerce an array-like or sequence into a contiguous float64 buffer object
    compatible with the Python Buffer Protocol (e.g. for SIMD kernels).
    """
    # 1. Already a contiguous float64 array.array or bytes/memoryview
    if isinstance(obj, (bytes, bytearray, memoryview)):
        return obj
    if isinstance(obj, array.array) and obj.typecode == 'd':
        return obj

    # 2. If NumPy is available, handle ndarray / array-like
    if HAS_NUMPY:
        if isinstance(obj, np.ndarray):
            if obj.dtype == np.float64 and obj.flags.c_contiguous:
                return obj
            return np.ascontiguousarray(obj, dtype=np.float64)
        if hasattr(obj, "__array__"):
            return np.ascontiguousarray(obj, dtype=np.float64)

    # 3. Fallback for sequences (list, tuple, etc.)
    if isinstance(obj, (list, tuple)):
        return array.array('d', (float(x) for x in obj))

    # 4. Try standard array.array conversion or buffer protocol directly
    try:
        return array.array('d', obj)
    except Exception:
        return obj
