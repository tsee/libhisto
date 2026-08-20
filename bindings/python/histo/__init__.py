"""
libhisto: Fast, portable C histogramming and statistical computing library for Python.
"""

from histo.constants import (
    VERSION, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,
    FLAG_NONE, FLAG_TRACK_SUMW2, FLAG_EXACT_MOMENTS,
    FIT_GAUSSIAN, FIT_EXPONENTIAL, FIT_POLYNOMIAL, FIT_BREIT_WIGNER, FIT_POWER_LAW,
    FIT_LOSS_CHI2, FIT_LOSS_POISSON_MLE
)
from histo.histogram import Histogram
from histo.histogram2d import Histogram2D
from histo.sketch import Sketch
from histo.fit import FitResult
import histo.cli as cli

# Aliases for convenience
Histo = Histogram
Histo2D = Histogram2D
DDSketch = Sketch

__version__ = VERSION
__all__ = [
    "Histogram",
    "Histo",
    "Histogram2D",
    "Histo2D",
    "Sketch",
    "DDSketch",
    "FitResult",
    "cli",
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
]
