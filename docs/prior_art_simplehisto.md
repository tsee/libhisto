# Prior Art Analysis: Math::SimpleHisto::XS

This document provides a comprehensive technical analysis of `Math::SimpleHisto::XS` (CPAN distribution by Steffen Müller), evaluating its design, algorithms, strengths, and areas for improvement in `libhisto`.

---

## 1. Overview & Architecture

`Math::SimpleHisto::XS` is a Perl XS module with a C core (`histogram.h`, `histogram.c`, `histogram_agg.h`, `histogram_agg.c`) designed for fast 1D histogramming.

### Core Data Structure
The C structure (`struct simple_histo_1d_struct`) tracks:
- **Geometry**: `min`, `max`, `nbins`, `width`, `binsize`.
- **Bin Representation**:
  - `bins == NULL`: Fixed-width bins (uniform width calculated as `(max - min) / nbins`).
  - `bins != NULL`: Variable-width bins (array of $N+1$ monotonic bin boundary coordinates).
- **Contents**:
  - `data`: Array of $N$ double-precision bin weights/counts.
  - `underflow`, `overflow`: Accumulated weights outside $[min, max)$.
  - `total`: Running sum of all filled weights inside $[min, max)$.
  - `nfills`: Total fill event count.
- **Lazy Cumulative Cache**:
  - `cumulative_hist`: Cached cumulative histogram pointer, lazily built and invalidated on mutations via `HS_INVALIDATE_CUMULATIVE`.

---

## 2. Key Capabilities & Patterns to Adopt

1. **Dual Binning Model (Uniform vs. Variable)**:
   - Storing uniform histograms compactly with `bins == NULL` allows $O(1)$ bin calculation without extra memory allocation for bin edges.
   - Storing variable bin boundaries as an $(N+1)$-element monotonic coordinate array enables general non-uniform binning with $O(\log N)$ binary search lookup.
2. **Vector / Batch Ingestion**:
   - `histo_fill(self, n, x_array, w_array)` supports bulk insertion, reducing function call overhead for high-throughput pipelines.
   - Support for filling by coordinate or directly by bin index (`histo_fill_by_bin`).
3. **Histogram Arithmetic**:
   - Compatible histogram addition, subtraction, multiplication, division, and scalar scaling.
   - Fast bin-by-bin verification before operations.
4. **Transformations**:
   - Rebinning by an integer factor $K$ (merging adjacent bins).
   - Range cloning (`clone_from_bin_range`) to crop a sub-range while collapsing out-of-range bins into underflow/overflow.
5. **Empirical Distribution Functions**:
   - Generation of normalized cumulative distribution functions (CDF).
   - Linear-interpolation-based median/quantile estimation.

---

## 3. Critical Weaknesses & Bugs to Fix in `libhisto`

### 3.1 Numerical Safety & IEEE-754 Edge Cases
- **`NaN` Vulnerability**:
  - In `Math::SimpleHisto::XS`, overflow/underflow checks are implemented as:
    ```c
    if (x >= max) { self->overflow += w; }
    else if (x < min) { self->underflow += w; }
    else { data[(int)((x - min) / binsize)] += w; }
    ```
  - For `NaN`, both `x >= max` and `x < min` evaluate to `false`. The calculation `(int)((NaN - min) / binsize)` casts `NaN` to `0` (or undefined integer conversion in C), silently corrupting bin 0!
  - **`libhisto` Fix**: Explicitly check `isnan(x)`. Record `NaN` inputs into a designated counter or return an error code; never allow `NaN` to reach array indexing.
- **Floating-Point Boundary Precision**:
  - Direct division `(x - min) / binsize` can experience floating-point roundoff near upper bounds. Explicit bounds clamping `[0, nbins - 1]` is necessary after index computation.

### 3.2 Statistical Accuracy & Moments
- In `Math::SimpleHisto::XS`, mean and standard deviation are calculated from **bin centers** after the fact, which is lossy:
  $$\mu \approx \frac{1}{\sum w_i} \sum w_i \cdot x_{\text{center}, i}$$
- **`libhisto` Enhancement**:
  - Provide both bin-approximated statistics and optional online exact statistical accumulators (Welford's algorithm for online mean, variance, min, and max) updated during `fill`.
  - Add support for per-bin sum-of-weights squared ($\sum w^2$) to enable Poisson and weighted error propagation ($\sigma_{\text{bin}} = \sqrt{\sum w^2}$).

### 3.3 Portability & Library Independence
- `Math::SimpleHisto::XS` relies on Perl internal macros (`pTHX_`, `Newx`, `Safefree`, `croak`).
- **`libhisto` Fix**: Pure standard ISO C99, zero dependencies, explicit memory allocators (`malloc`/`free` or custom allocator interface), clean error returns without process abortion.

### 3.4 Thread-Safety & Mutation via Caching
- Storing a mutable `cumulative_hist` pointer inside the primary struct causes const query functions (e.g. `median()`) to mutate the struct, preventing concurrent read access across threads.
- **`libhisto` Fix**: Keep histogram handles const-pure during query/analytical operations. Cumulative histograms and CDFs should be explicitly computed as separate objects or caller-managed buffers.

### 3.5 Cross-Platform Serialization
- `Math::SimpleHisto::XS` native dump relied on host `pack()` without endian neutrality or explicit format versions.
- **`libhisto` Fix**: Define an explicit, portable binary schema (canonical little-endian integer/IEEE-754 encoding with header magic `HIST`, versioning, and CRC) and human-readable text/JSON formats.

---

## 4. Summary Matrix

| Feature | `Math::SimpleHisto::XS` | `libhisto` Target |
| :--- | :--- | :--- |
| **Language & Deps** | Perl XS + Perl-dependent C | Pure ISO C99 (`libc` only) |
| **Error Handling** | Perl `croak()` / `die` | Structured `histo_status_t` return codes |
| **NaN / Inf Handling** | Flawed (`NaN` corrupts bin 0) | Explicit, deterministic IEEE-754 handling |
| **Binning Types** | Fixed & Variable | Fixed & Variable with validation |
| **Statistical Moments** | Lossy bin-center summation | Both exact online accumulators & bin estimators |
| **Error / Weights** | Sum of weights only | Sum of weights + Sum of weights squared ($\sum w^2$) |
| **Thread Safety** | Non-thread-safe (internal mutation) | Const-safe query APIs |
| **Serialization** | Host-dependent Perl pack / JSON / text | Cross-platform canonical Little-Endian binary & JSON |
