"""
Universal Histogram Interface (UHI) protocol types, locators, and traits for libhisto.
"""

from typing import Protocol, runtime_checkable, Sequence, Tuple, Optional, Any, Union


class Kind:
    """UHI Kind enumeration constants."""
    COUNT = "COUNT"
    MEAN = "MEAN"


class AxisTraits:
    """
    UHI Axis Traits descriptor.
    Declares axis properties (discrete, circular, growth).
    """
    def __init__(self, circular: bool = False, discrete: bool = False, growth: bool = False):
        self.circular = bool(circular)
        self.discrete = bool(discrete)
        self.growth = bool(growth)

    def __repr__(self) -> str:
        return f"Traits(circular={self.circular}, discrete={self.discrete}, growth={self.growth})"

    def __eq__(self, other: Any) -> bool:
        if hasattr(other, "circular") and hasattr(other, "discrete"):
            return (
                self.circular == other.circular and
                self.discrete == other.discrete and
                getattr(self, "growth", False) == getattr(other, "growth", False)
            )
        return False


PlottableTraits = AxisTraits


class loc:
    """
    UHI Coordinate Locator for slicing and indexing by physical value.
    
    Examples
    --------
    >>> h[uhi.loc(2.5)]
    >>> h[uhi.loc(10.0):uhi.loc(50.0)]
    >>> h[uhi.loc(10.0) + 1]
    """
    def __init__(self, value: float, offset: int = 0):
        self.value = float(value)
        self.offset = int(offset)

    def __repr__(self) -> str:
        if self.offset == 0:
            return f"loc({self.value})"
        return f"loc({self.value}, offset={self.offset})"

    def __add__(self, offset: int) -> "loc":
        return loc(self.value, self.offset + int(offset))

    def __sub__(self, offset: int) -> "loc":
        return loc(self.value, self.offset - int(offset))

    def __eq__(self, other: Any) -> bool:
        if isinstance(other, loc):
            return self.value == other.value and self.offset == other.offset
        return False


class rebin:
    """
    UHI Rebinning step modifier.
    
    Examples
    --------
    >>> h[::uhi.rebin(2)]
    >>> h[uhi.loc(0):uhi.loc(100):uhi.rebin(4)]
    """
    def __init__(self, factor: int = 1):
        if factor <= 0:
            raise ValueError("Rebin factor must be a positive integer > 0")
        self.factor = int(factor)

    def __repr__(self) -> str:
        return f"rebin({self.factor})"

    def __eq__(self, other: Any) -> bool:
        if isinstance(other, rebin):
            return self.factor == other.factor
        if isinstance(other, int):
            return self.factor == other
        return False


class _UnderflowTag:
    """Tag representing the underflow bin in indexing expressions."""
    def __repr__(self) -> str:
        return "underflow"


class _OverflowTag:
    """Tag representing the overflow bin in indexing expressions."""
    def __repr__(self) -> str:
        return "overflow"


class _SumTag:
    """Tag representing summation / projection along an axis in 2D indexing."""
    def __repr__(self) -> str:
        return "sum"


underflow = _UnderflowTag()
overflow = _OverflowTag()
sum = _SumTag()


@runtime_checkable
class PlottableAxis(Protocol):
    """
    UHI PlottableAxis protocol definition.
    """
    traits: AxisTraits

    def __len__(self) -> int: ...
    def __getitem__(self, idx: int) -> Any: ...
    def bin(self, idx: int) -> Any: ...
    def index(self, value: float) -> int: ...


@runtime_checkable
class PlottableHistogram(Protocol):
    """
    UHI PlottableHistogram protocol definition for 1D and multi-dimensional histograms.
    """
    axes: Sequence[PlottableAxis]
    kind: str

    def values(self, flow: bool = False) -> Any: ...
    def variances(self, flow: bool = False) -> Optional[Any]: ...
    def counts(self, flow: bool = False) -> Any: ...
