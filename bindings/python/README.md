# histo: Fast C99 Histogramming for Python

`histo` provides high-performance, cache-conscious 1D/2D histogramming, statistical moments, non-linear curve fitting, and DDSketch quantile streaming with runtime SIMD acceleration (AVX-512, AVX2, NEON) and zero-copy NumPy / Buffer Protocol integration.

---

## Key Features

- **Blazing Fast Ingestion**: Over **450+ Million ops/sec** throughput with zero-copy Buffer Protocol (`memoryview`, `numpy.ndarray`).
- **1D & 2D Histograms**: Uniform equidistant and variable-width arbitrary binning.
- **Statistical Moments**: Running Welford mean, variance, skewness, kurtosis, continuous mode, FWHM, and RMS.
- **Robust Dispersion**: Quantiles, median, IQR, MAD, trimmed and Winsorized means.
- **Hypothesis Testing & Distances**: $\chi^2$ test, Kolmogorov-Smirnov, 1D Wasserstein (Earth Mover's Distance), Kullback-Leibler divergence, and Bhattacharyya distance.
- **Non-Linear Curve Fitting**: Levenberg-Marquardt optimizer for Gaussian, Exponential, Polynomial, Breit-Wigner, Power Law, Log-Normal, Gauss+Linear, Weibull, Gamma, Poisson, Laplace, and custom Python callback models.
- **Online Quantile Sketches**: DDSketch logarithmic dynamic sketches with provable relative error guarantees ($\epsilon \le 1\%$).
- **CLI Integration**: Built-in in-process execution and standalone `pyhisto` command.

---

## Quickstart

```python
import histo
import numpy as np

# Create 1D uniform histogram
h = histo.Histogram(bins=50, range=(0.0, 100.0), track_sumw2=True)

# Single and batch ingestion
h.fill(42.5)
h.fill(84.0, weight=2.0)

# Zero-copy SIMD ingestion from NumPy or buffer
data = np.random.normal(50.0, 10.0, 1_000_000)
h.fill_buffer(data)

# Statistical moments & quantiles
print(f"Mean: {h.mean:.3f} ± {h.std_dev:.3f}")
print(f"Median: {h.median:.3f}, IQR: {h.iqr:.3f}")
print(f"p95: {h.quantile(0.95):.3f}")

# Curve fitting
res = h.fit(model="gaussian")
print(f"Fitted mu: {res.params[1]:.3f}, sigma: {res.params[2]:.3f}, Chi2/NDF: {res.reduced_chi2:.3f}")

# Operator overloading
h_scaled = (h + h) * 0.5

# NumPy Interoperability
counts, edges = h.to_numpy()             # Match np.histogram return format
h_copy = histo.Histogram.from_numpy(counts, edges)  # Construct from NumPy
arr = np.asarray(h)                      # __array__ protocol: use with np.sum, ufuncs

# Universal Histogram Interface (UHI)
ax = h.axes[0]
edges = ax.edges
centers = ax.centers
vals = h.values()
vars_ = h.variances()

# 2D Histogram
h2d = histo.Histogram2D(xbins=20, xrange=(0, 10), ybins=20, yrange=(0, 10))
h2d.fill(3.5, 4.5)
H, xedges, yedges = h2d.to_numpy()        # Match np.histogram2d
print(f"Covariance: {h2d.covariance:.4f}")
proj_x = h2d.project_x()

# DDSketch Dynamic Streaming Quantile Sketch
sketch = histo.Sketch(alpha=0.01)
sketch.insert_buffer(data)
print(f"p99 quantile: {sketch.quantile(0.99):.3f}")
```

---

## NumPy & Universal Histogram Interface (UHI)

`histo` provides native zero-copy integration with NumPy and the Scikit-HEP Universal Histogram Interface:

- **`to_numpy()` & `from_numpy()`**: Direct roundtrip parity with `numpy.histogram` and `numpy.histogram2d`.
- **`__array__()`**: Enables passing `histo.Histogram` and `histo.Histogram2D` instances directly into `np.asarray(h)`, `np.sum(h)`, and NumPy ufuncs (`np.sqrt`, `np.log`).
- **UHI Protocols**: Compatible with `mplhep`, `plothist`, and `histoprint` via `.axes`, `.values()`, `.variances()`, `.edges`, and `.centers`.
- **Zero Mandatory Dependencies**: All NumPy functionality gracefully degrades without errors when NumPy is not installed, preserving a 100% lightweight, dependency-free installation.

---

## In-Process CLI Toolkit


```python
import histo.cli

code, stdout, stderr = histo.cli.run("plot", "--sparkline", "data.histo")
print(stdout)
```

Or via the command line:
```bash
pyhisto plot --style=ascii data.histo
```

---

## License

MIT License. Copyright (c) 2026 Steffen Mueller.
