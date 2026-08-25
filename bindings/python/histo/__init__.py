"""
libhisto: Fast, portable C histogramming and statistical computing library for Python.
"""

from typing import Tuple, Any
from histo.constants import (
    VERSION, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,
    FLAG_NONE, FLAG_TRACK_SUMW2, FLAG_EXACT_MOMENTS,
    FIT_GAUSSIAN, FIT_EXPONENTIAL, FIT_POLYNOMIAL, FIT_BREIT_WIGNER, FIT_POWER_LAW,
    FIT_LOSS_CHI2, FIT_LOSS_POISSON_MLE,
    BIN_RULE_AUTO, BIN_RULE_FD, BIN_RULE_SCOTT, BIN_RULE_STURGES, BIN_RULE_DOANE, BIN_RULE_KNUTH,
    KDE_KERNEL_GAUSSIAN, KDE_KERNEL_EPANECHNIKOV, KDE_KERNEL_UNIFORM, KDE_KERNEL_TRIANGULAR,
    KDE_KERNEL_BIWEIGHT, KDE_KERNEL_COSINE,
    KDE_BW_SILVERMAN, KDE_BW_SCOTT, KDE_BW_MANUAL,
    PALETTES,
    REGION_CENTER, REGION_EAST, REGION_NORTH, REGION_SOUTH, REGION_WEST,
    REGION_SOUTH_WEST, REGION_SOUTH_EAST, REGION_NORTH_WEST, REGION_NORTH_EAST
)
from histo.histogram import Histogram
from histo.histogram2d import Histogram2D
from histo.axis import Axis, HistogramAxis, Interval
from histo.uhi import loc, rebin, underflow, overflow, sum, Kind, AxisTraits, PlottableTraits, PlottableAxis, PlottableHistogram
from histo.sketch import Sketch
from histo.fit import FitResult
from histo.kde import KDE
import histo.cli as cli

# Aliases for convenience
Histo = Histogram
Histo2D = Histogram2D
DDSketch = Sketch


def plot(obj, *args, **kwargs) -> str:
    """Render terminal visualization of a Histogram or Histogram2D."""
    if hasattr(obj, "plot"):
        return obj.plot(*args, **kwargs)
    raise TypeError(f"Object of type '{type(obj).__name__}' does not support plotting")


def top(*args) -> Tuple[int, str, str]:
    """Execute real-time interactive terminal monitor (histo top)."""
    return cli.run("top", *args)


__version__ = VERSION
__all__ = [
    "Histogram",
    "Histo",
    "Histogram2D",
    "Histo2D",
    "Axis",
    "HistogramAxis",
    "Interval",
    "loc",
    "rebin",
    "underflow",
    "overflow",
    "sum",
    "Kind",
    "AxisTraits",
    "PlottableTraits",
    "PlottableAxis",
    "PlottableHistogram",
    "Sketch",
    "DDSketch",
    "FitResult",
    "KDE",
    "cli",
    "plot",
    "top",
    "PALETTES",
    "REGION_CENTER",
    "REGION_EAST",
    "REGION_NORTH",
    "REGION_SOUTH",
    "REGION_WEST",
    "REGION_SOUTH_WEST",
    "REGION_SOUTH_EAST",
    "REGION_NORTH_WEST",
    "REGION_NORTH_EAST",
    "VERSION",
    "FLAG_NONE",
    "FLAG_TRACK_SUMW2",
    "FLAG_EXACT_MOMENTS",
    "FIT_GAUSSIAN",
    "FIT_EXPONENTIAL",
    "FIT_POLYNOMIAL",
    "FIT_BREIT_WIGNER",
    "FIT_POWER_LAW",
    "FIT_LOSS_CHI2",
    "FIT_LOSS_POISSON_MLE",
    "BIN_RULE_AUTO",
    "BIN_RULE_FD",
    "BIN_RULE_SCOTT",
    "BIN_RULE_STURGES",
    "BIN_RULE_DOANE",
    "BIN_RULE_KNUTH",
    "KDE_KERNEL_GAUSSIAN",
    "KDE_KERNEL_EPANECHNIKOV",
    "KDE_KERNEL_UNIFORM",
    "KDE_KERNEL_TRIANGULAR",
    "KDE_KERNEL_BIWEIGHT",
    "KDE_KERNEL_COSINE",
    "KDE_BW_SILVERMAN",
    "KDE_BW_SCOTT",
    "KDE_BW_MANUAL",
]
