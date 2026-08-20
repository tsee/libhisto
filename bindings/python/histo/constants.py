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

# Curve Fitting Loss Objectives
FIT_LOSS_CHI2 = _libhisto.FIT_LOSS_CHI2
FIT_LOSS_POISSON_MLE = _libhisto.FIT_LOSS_POISSON_MLE
