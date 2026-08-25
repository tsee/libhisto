"""
Constants, flags, and error codes for libhisto Python bindings.
"""

import _libhisto

VERSION = _libhisto.VERSION_STRING
VERSION_MAJOR = _libhisto.VERSION_MAJOR
VERSION_MINOR = _libhisto.VERSION_MINOR
VERSION_PATCH = _libhisto.VERSION_PATCH

# Feature Flags
FLAG_NONE = _libhisto.FLAG_NONE
FLAG_TRACK_SUMW2 = _libhisto.FLAG_TRACK_SUMW2
FLAG_EXACT_MOMENTS = _libhisto.FLAG_EXACT_MOMENTS

# Curve Fitting Models
FIT_GAUSSIAN = _libhisto.FIT_GAUSSIAN
FIT_EXPONENTIAL = _libhisto.FIT_EXPONENTIAL
FIT_POLYNOMIAL = _libhisto.FIT_POLYNOMIAL
FIT_BREIT_WIGNER = _libhisto.FIT_BREIT_WIGNER
FIT_POWER_LAW = _libhisto.FIT_POWER_LAW
FIT_LOG_NORMAL = _libhisto.FIT_LOG_NORMAL
FIT_GAUSSIAN_PLUS_LINEAR = _libhisto.FIT_GAUSSIAN_PLUS_LINEAR
FIT_WEIBULL = _libhisto.FIT_WEIBULL
FIT_GAMMA = _libhisto.FIT_GAMMA
FIT_POISSON = _libhisto.FIT_POISSON
FIT_LAPLACE = _libhisto.FIT_LAPLACE

# Curve Fitting Loss Objectives
FIT_LOSS_CHI2 = _libhisto.FIT_LOSS_CHI2
FIT_LOSS_POISSON_MLE = _libhisto.FIT_LOSS_POISSON_MLE

# Auto-Binning Rules
BIN_RULE_AUTO = _libhisto.BIN_RULE_AUTO
BIN_RULE_FD = _libhisto.BIN_RULE_FD
BIN_RULE_SCOTT = _libhisto.BIN_RULE_SCOTT
BIN_RULE_STURGES = _libhisto.BIN_RULE_STURGES
BIN_RULE_DOANE = _libhisto.BIN_RULE_DOANE
BIN_RULE_KNUTH = _libhisto.BIN_RULE_KNUTH

# KDE Kernels
KDE_KERNEL_GAUSSIAN = _libhisto.KDE_KERNEL_GAUSSIAN
KDE_KERNEL_EPANECHNIKOV = _libhisto.KDE_KERNEL_EPANECHNIKOV
KDE_KERNEL_UNIFORM = _libhisto.KDE_KERNEL_UNIFORM
KDE_KERNEL_TRIANGULAR = _libhisto.KDE_KERNEL_TRIANGULAR
KDE_KERNEL_BIWEIGHT = _libhisto.KDE_KERNEL_BIWEIGHT
KDE_KERNEL_COSINE = _libhisto.KDE_KERNEL_COSINE

# KDE Bandwidth Methods
KDE_BW_SILVERMAN = _libhisto.KDE_BW_SILVERMAN
KDE_BW_SCOTT = _libhisto.KDE_BW_SCOTT
KDE_BW_MANUAL = _libhisto.KDE_BW_MANUAL

# Colormap Palettes
PALETTES = (
    "viridis",
    "plasma",
    "inferno",
    "magma",
    "turbo",
    "cividis",
    "grayscale",
    "rainbow",
)

# 2D Guard Regions
REGION_CENTER = getattr(_libhisto, "REGION_CENTER", 0)
REGION_EAST = getattr(_libhisto, "REGION_EAST", 1)
REGION_NORTH = getattr(_libhisto, "REGION_NORTH", 2)
REGION_SOUTH = getattr(_libhisto, "REGION_SOUTH", 3)
REGION_WEST = getattr(_libhisto, "REGION_WEST", 4)
REGION_SOUTH_WEST = getattr(_libhisto, "REGION_SOUTH_WEST", 5)
REGION_SOUTH_EAST = getattr(_libhisto, "REGION_SOUTH_EAST", 6)
REGION_NORTH_WEST = getattr(_libhisto, "REGION_NORTH_WEST", 7)
REGION_NORTH_EAST = getattr(_libhisto, "REGION_NORTH_EAST", 8)

