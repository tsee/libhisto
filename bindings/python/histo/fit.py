"""
Curve fitting and non-linear regression support for libhisto.
"""

from typing import List, Optional, Tuple, Dict, Callable
import _libhisto
from histo.constants import (
    FIT_GAUSSIAN, FIT_EXPONENTIAL, FIT_POLYNOMIAL,
    FIT_BREIT_WIGNER, FIT_POWER_LAW, FIT_LOG_NORMAL,
    FIT_GAUSSIAN_PLUS_LINEAR, FIT_WEIBULL, FIT_GAMMA,
    FIT_POISSON, FIT_LAPLACE, FIT_LOSS_CHI2, FIT_LOSS_POISSON_MLE
)

MODEL_MAP = {
    "gaussian": FIT_GAUSSIAN,
    "gauss": FIT_GAUSSIAN,
    "exponential": FIT_EXPONENTIAL,
    "exp": FIT_EXPONENTIAL,
    "polynomial": FIT_POLYNOMIAL,
    "poly": FIT_POLYNOMIAL,
    "breit_wigner": FIT_BREIT_WIGNER,
    "cauchy": FIT_BREIT_WIGNER,
    "power_law": FIT_POWER_LAW,
    "lognormal": FIT_LOG_NORMAL,
    "log_normal": FIT_LOG_NORMAL,
    "gauss_linear": FIT_GAUSSIAN_PLUS_LINEAR,
    "gauss+linear": FIT_GAUSSIAN_PLUS_LINEAR,
    "gauss+poly1": FIT_GAUSSIAN_PLUS_LINEAR,
    "weibull": FIT_WEIBULL,
    "gamma": FIT_GAMMA,
    "erlang": FIT_GAMMA,
    "poisson": FIT_POISSON,
    "laplace": FIT_LAPLACE,
}

LOSS_MAP = {
    "chi2": FIT_LOSS_CHI2,
    "poisson_mle": FIT_LOSS_POISSON_MLE,
    "mle": FIT_LOSS_POISSON_MLE,
}


class FitResult:
    """
    Result container for non-linear regression / curve fitting.
    """
    def __init__(self, raw: _libhisto.FitResult):
        self._raw = raw

    @property
    def params(self) -> List[float]:
        """Fitted parameter values."""
        return self._raw.params

    @property
    def errors(self) -> List[float]:
        """Parameter standard errors sqrt(Cov_ii)."""
        return self._raw.errors

    @property
    def chi2(self) -> float:
        """Chi-Square statistic value."""
        return self._raw.chi2

    @property
    def ndf(self) -> int:
        """Degrees of Freedom."""
        return self._raw.ndf

    @property
    def reduced_chi2(self) -> float:
        """Reduced Chi2 (Chi2 / NDF)."""
        return self._raw.reduced_chi2

    @property
    def p_value(self) -> float:
        """Goodness-of-fit p-value (upper tail of Chi2 distribution)."""
        return self._raw.p_value

    @property
    def log_likelihood(self) -> float:
        """Log-likelihood at optimal parameter point."""
        return self._raw.log_likelihood

    @property
    def aic(self) -> float:
        """Akaike Information Criterion."""
        return self._raw.aic

    @property
    def bic(self) -> float:
        """Bayesian Information Criterion."""
        return self._raw.bic

    @property
    def iterations(self) -> int:
        """Number of Levenberg-Marquardt iterations performed."""
        return self._raw.iterations

    @property
    def status(self) -> int:
        """Convergence status code."""
        return self._raw.status

    @property
    def converged(self) -> bool:
        """True if fitting converged successfully within tolerances."""
        return self._raw.converged

    @property
    def reason(self) -> str:
        """Human readable convergence reason string."""
        return self._raw.reason

    def __repr__(self) -> str:
        return (f"<FitResult converged={self.converged} chi2/ndf={self.reduced_chi2:.3f} "
                f"p_value={self.p_value:.4f} params={self.params}>")
