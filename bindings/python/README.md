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

## Universal Histogram Interface (UHI) & Scikit-HEP Compliance

`histo` implements full compliance with the [Universal Histogram Interface (UHI)](https://uhi.readthedocs.io/) protocols (`PlottableHistogram`, `PlottableAxis`), enabling seamless interoperability with the entire Python HEP visualization ecosystem (`mplhep`, `hist`, `plothist`, `histoprint`, `boost-histogram`).

### 1D & 2D UHI Protocols & Properties

- **`axes`**: Tuple of `HistogramAxis` (`PlottableAxis`) objects with `len(ax)`, `ax.edges`, `ax.centers`, `ax.widths`, `ax.traits`, `ax.bin(i)`, and `ax.index(val)`.
- **`kind`**: Returns `"COUNT"` (`Kind.COUNT`).
- **`values(flow: bool = False)`**:
  - 1D: Returns 1D array of shape `(N,)` (`flow=False`) or `(N+2,)` incorporating `[underflow, *bins, overflow]` (`flow=True`).
  - 2D: Returns 2D array of shape `(Nx, Ny)` (`flow=False`) or `(Nx+2, Ny+2)` (`flow=True`) incorporating all 9 geometric boundary regions:
    * Center grid: `[1:Nx+1, 1:Ny+1]`
    * 4 Corners: Bottom-Left `[0, 0]`, Bottom-Right `[Nx+1, 0]`, Top-Left `[0, Ny+1]`, Top-Right `[Nx+1, Ny+1]`
    * 4 Boundary Guards: West `[0, 1:Ny+1]`, East `[Nx+1, 1:Ny+1]`, South `[1:Nx+1, 0]`, North `[1:Nx+1, Ny+1]`
- **`variances(flow: bool = False)`**: Returns $\sum w^2$ variances array with matching shapes.
- **`counts(flow: bool = False)`**: Alias for `values(flow=flow)`.

### UHI Slicing, Locators, and Rebinning

```python
from histo import Histogram, Histogram2D, loc, rebin, underflow, overflow, sum as uhi_sum

h = Histogram(bins=50, range=(0.0, 100.0), track_sumw2=True)
h.fill(25.0, weight=2.0)

# Coordinate locators & boundary tags
content = h[loc(25.0)]         # Direct coordinate lookup
underflow_w = h[underflow]     # Underflow weight
overflow_w = h[overflow]       # Overflow weight

# Slicing ranges with locators
sub_h = h[loc(20.0):loc(60.0)] # Sub-histogram from 20.0 to 60.0

# Rebinning in slice expressions
rebinned = h[::rebin(2)]       # Rebin by factor of 2
rebinned_c = h[::2j]           # Complex step rebinning (boost-histogram convention)

# 2D Slicing, Projection & Locators
h2 = Histogram2D(xbins=20, xrange=(0, 10), ybins=20, yrange=(0, 10))
h2.fill(2.5, 7.5, weight=3.0)

val = h2[loc(2.5), loc(7.5)]   # 2D coordinate access
proj_x = h2[:, uhi_sum]        # Project onto X-axis (integrating over Y)
proj_y = h2[uhi_sum, :]        # Project onto Y-axis (integrating over X)
slice_1d = h2[loc(2.5), :]     # 1D slice along Y at fixed X
h2_sub = h2[::rebin(2), ::rebin(2)] # 2D rebinning
```

### High-Level Interoperability & Converters

```python
# Convert to continuous SciPy probability distribution
dist = h.to_scipy_dist()       # Returns scipy.stats.rv_histogram
pdf_val = dist.pdf(25.0)

# Bidirectional boost-histogram conversion
bh_histo = h.to_boost()        # Convert 1D/2D to boost_histogram.Histogram
h_restored = Histogram.from_boost(bh_histo) # Restore to libhisto Histogram
```

- **Zero Mandatory Dependencies**: All NumPy, SciPy, and boost-histogram integrations gracefully degrade without errors when those optional packages are absent.

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
