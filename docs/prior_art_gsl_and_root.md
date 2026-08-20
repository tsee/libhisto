# Prior Art Analysis: GSL, ROOT, and HdrHistogram

This document analyzes three prominent histogramming implementations in scientific and systems computing: **GSL Histogram** (C), **ROOT TH1** (C++ / High Energy Physics standard), and **HdrHistogram** (C/Java latency telemetry).

---

## 1. GNU Scientific Library: `gsl_histogram`

**GSL** provides a pure C histogram implementation (`gsl_histogram`, `gsl_histogram_pdf`).

### 1.1 Architectural Characteristics
- **Data Structure**:
  ```c
  typedef struct {
      size_t n;         /* number of bins */
      double *range;    /* n + 1 boundary values */
      double *bin;      /* n bin contents */
  } gsl_histogram;
  ```
- **Uniform vs. Variable Representation**: GSL allocates an $(N+1)$-element `range` array for *all* histograms, even uniform ones.
  - *Trade-off*: Simplifies the API by treating all histograms uniformly, but wastes memory and forces $O(\log N)$ binary search (`gsl_histogram_find`) even for uniform grids where $O(1)$ arithmetic lookup is possible.
- **Underflow & Overflow**: GSL functions return `GSL_EDOM` (domain error) on out-of-range inputs; they do **not** accumulate underflow/overflow totals.
- **Analytical Features**:
  - `gsl_histogram_mean`, `gsl_histogram_sigma` (computed from bin centers and weights).
  - `gsl_histogram_pdf`: Dedicated PDF sampling struct precalculating cumulative sums for inversion sampling.

---

## 2. CERN ROOT: `TH1` / `TH1D`

**ROOT** is the de-facto standard data analysis framework in High-Energy Physics (HEP).

### 2.1 Architectural Characteristics
- **Bin Indexing Convention**:
  - Bin `0`: **Underflow** bin.
  - Bins `1` to `N`: **In-range** bins.
  - Bin `N + 1`: **Overflow** bin.
  - *Advantage*: Uniform indexing where underflow/overflow are contiguous with main data, simplifying loops and serialization.
- **Statistical Moments Tracking**:
  - Tracks running statistics during fill:
    - $\sum w$, $\sum w \cdot x$, $\sum w \cdot x^2$.
  - Enables exact sample mean and RMS (standard deviation) without bin-center discretization error.
- **Statistical Uncertainties (`Sumw2`)**:
  - Parallel array `fSumw2` storing $\sum w_i^2$ per bin, created on demand or during weighted fills.
- **Comparison & Hypothesis Testing**:
  - Built-in Kolmogorov-Smirnov test (`TH1::KolmogorovTest`) and Chi-Square test (`TH1::Chi2Test`) for statistical comparison between two histograms.

---

## 3. HdrHistogram (High Dynamic Range Histogram)

**HdrHistogram** (by Gil Tene) is optimized for recording high-dynamic-range integer values (e.g. latency in nanoseconds) with a fixed number of significant digits.

### 3.1 Architectural Characteristics
- **Logarithmic Compaction**: Uses sub-bucket logarithmically spaced bins to cover orders of magnitude (e.g., $1\,\mu\text{s}$ to $1\,\text{hour}$) with bounded relative error ($\le 1\%$).
- **Fixed Memory Footprint**: Allocates all buckets upfront based on min/max trackable values and precision digits.
- **Lessons for `libhisto`**: While `libhisto` focuses on general floating-point 1D histogramming, HdrHistogram demonstrates the utility of logarithmic binning modes and zero-allocation continuous ingestion.

---

## 4. Synthesis of Techniques for `libhisto`

| Design Dimension | GSL | ROOT `TH1D` | `Math::SimpleHisto::XS` | `libhisto` Decision |
| :--- | :--- | :--- | :--- | :--- |
| **Uniform Bin Lookup** | $O(\log N)$ (wastes array) | $O(1)$ arithmetic | $O(1)$ arithmetic | **$O(1)$ arithmetic** with bounds check |
| **Variable Bin Lookup** | $O(\log N)$ bisection | $O(\log N)$ bisection | $O(\log N)$ bisection | **$O(\log N)$ optimized binary search** |
| **Out-of-range Handling** | Domain Error (`EDOM`) | Bins `0` & `N+1` | `underflow` / `overflow` fields | **Dedicated underflow / overflow accumulators + non-finite (`NaN`) counter** |
| **Uncertainty ($\sum w^2$)** | None | Optional (`Sumw2`) | None | **First-class $\sum w^2$ support** |
| **Moments Calculation** | Bin-center approx | Exact online stats | Bin-center approx | **Both exact online stats + bin-center estimators** |
| **Error Reporting** | Return code | Logging / Crash | Perl `croak()` | **Explicit `histo_status_t` return codes** |
| **Dependencies** | GSL library | Massive C++ | Perl C headers | **Zero dependencies (Pure ISO C99)** |
