"""
Kernel Density Estimation (KDE) Engine.
"""

from typing import Sequence, Optional, Union, List
import _libhisto


KERNEL_MAP = {
    "gaussian": _libhisto.KDE_KERNEL_GAUSSIAN,
    "epanechnikov": _libhisto.KDE_KERNEL_EPANECHNIKOV,
    "uniform": _libhisto.KDE_KERNEL_UNIFORM,
    "boxcar": _libhisto.KDE_KERNEL_UNIFORM,
    "triangular": _libhisto.KDE_KERNEL_TRIANGULAR,
    "biweight": _libhisto.KDE_KERNEL_BIWEIGHT,
    "quartic": _libhisto.KDE_KERNEL_BIWEIGHT,
    "cosine": _libhisto.KDE_KERNEL_COSINE,
}

BW_MAP = {
    "silverman": _libhisto.KDE_BW_SILVERMAN,
    "scott": _libhisto.KDE_BW_SCOTT,
    "manual": _libhisto.KDE_BW_MANUAL,
}


class KDE:
    """
    1-Dimensional Non-Parametric Kernel Density Estimator.
    """
    def __init__(
        self,
        samples: Optional[Sequence[float]] = None,
        weights: Optional[Sequence[float]] = None,
        kernel: Union[str, int] = "gaussian",
        bw_method: Union[str, int] = "silverman",
        bandwidth: float = 0.0,
        bw_adjust: float = 1.0,
        _raw: Optional[_libhisto.KDE] = None,
    ):
        if _raw is not None:
            self._raw = _raw
        else:
            if samples is None:
                raise ValueError("samples must be provided")

            k_code = KERNEL_MAP.get(str(kernel).lower(), kernel) if isinstance(kernel, str) else int(kernel)
            bw_code = BW_MAP.get(str(bw_method).lower(), bw_method) if isinstance(bw_method, str) else int(bw_method)

            self._raw = _libhisto.KDE.create(
                samples=samples,
                weights=weights,
                kernel=k_code,
                bw_method=bw_code,
                bandwidth=float(bandwidth),
                bw_adjust=float(bw_adjust),
            )

    @classmethod
    def from_histogram(
        cls,
        histogram,
        kernel: Union[str, int] = "gaussian",
        bw_method: Union[str, int] = "silverman",
        bandwidth: float = 0.0,
        bw_adjust: float = 1.0,
    ) -> "KDE":
        """Construct KDE directly from a Histogram instance."""
        raw_h = getattr(histogram, "_raw", histogram)
        k_code = KERNEL_MAP.get(str(kernel).lower(), kernel) if isinstance(kernel, str) else int(kernel)
        bw_code = BW_MAP.get(str(bw_method).lower(), bw_method) if isinstance(bw_method, str) else int(bw_method)

        raw = _libhisto.KDE.create_from_histo(
            histo=raw_h,
            kernel=k_code,
            bw_method=bw_code,
            bandwidth=float(bandwidth),
            bw_adjust=float(bw_adjust),
        )
        return cls(_raw=raw)

    def eval(self, x: Union[float, Sequence[float]]) -> Union[float, List[float]]:
        """Evaluate estimated probability density function (PDF) at coordinate(s) x."""
        return self._raw.eval(x)

    def pdf(self, x: Union[float, Sequence[float]]) -> Union[float, List[float]]:
        """Alias for eval()."""
        return self.eval(x)

    def cdf(self, x: float) -> float:
        """Evaluate cumulative distribution function (CDF) at coordinate x."""
        return self._raw.cdf(float(x))

    def quantile(self, q: float) -> float:
        """Evaluate quantile for probability q in [0, 1]."""
        return self._raw.quantile(float(q))

    def sample(self, n: int = 1, seed: int = 0) -> List[float]:
        """Generate n pseudo-random synthetic samples from the estimated distribution."""
        return self._raw.sample(n=int(n), seed=int(seed))

    @property
    def bandwidth(self) -> float:
        """Effective smoothing bandwidth parameter h."""
        return self._raw.bandwidth

    @property
    def kernel(self) -> int:
        """Kernel function code."""
        return self._raw.kernel

    @property
    def n_points(self) -> int:
        """Number of points in model."""
        return self._raw.n_points

    def __repr__(self) -> str:
        return f"<KDE points={self.n_points} bandwidth={self.bandwidth:.6g}>"
