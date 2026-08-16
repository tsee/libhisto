# libhisto User Manual & Architecture Reference {#mainpage}

## 1. Overview & Architectural Design

`libhisto` is a high-performance, portable, memory-safe 1D histogramming library implemented in ISO C99. It is designed for high-throughput scientific analysis, physical simulations, telemetry ingestion, and general statistical computing.

### Key Architectural Pillars
- **Zero-Allocation Ingestion**: Ingestion routines (`histo_fill`, `histo_fill_w`, `histo_fill_n`, `histo_fill_strided`) perform zero heap allocations, achieving >30 Million fills/second.
- **Strict ISO C99 & Memory Safety**: Defensive pointer checks on all public APIs, clear destructor ownership semantics (`histo_destroy`), and 100% leak-free ASan/UBSan verification.
- **Deterministic Endian-Safe Wire Format**: Canonical 256-byte header with Little-Endian IEEE-754 data payloads for cross-platform serialization.
- **Exact & Numerically Stable Statistics**: Online weighted Welford accumulation, division-free error propagation, and machine-epsilon boundary guards.

---

## 2. Quickstart & Usage Guide

```c
#include <stdio.h>
#include <histo/histo.h>

int main(void) {
    // 1. Create a uniform histogram: 100 bins from 0.0 to 100.0 with sum_w2 tracking
    histo_t *h = histo_create_uniform(100, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    if (!h) {
        fprintf(stderr, "Failed to create histogram\n");
        return 1;
    }

    // 2. Ingest samples (unit weight and weighted)
    histo_fill(h, 25.4);
    histo_fill_w(h, 50.0, 2.5);

    // 3. Batch ingestion from array
    double samples[3] = {12.0, 45.0, 88.0};
    histo_fill_n(h, 3, samples, NULL);

    // 4. Query statistics
    double mean = 0.0, std_dev = 0.0, median = 0.0;
    histo_mean(h, &mean);
    histo_std_dev(h, &std_dev);
    histo_median(h, &median);

    printf("Mean: %.3f, Std Dev: %.3f, Median: %.3f\n", mean, std_dev, median);

    // 5. Clean teardown
    histo_destroy(h);
    return 0;
}
```

---

## 3. Numerical Algorithms & Statistical Formulations

### 3.1 Boundary-Guarded Uniform Bin Lookup (O(1))
For uniform binning over [min, max), naive floating-point division `(x - min) / width` is susceptible to floating-point truncation errors at exact bin boundaries. `libhisto` computes the provisional index via reciprocal multiplication:
\f[
i_{\text{prov}} = \lfloor (x - x_{\min}) \cdot \Delta^{-1} \rfloor
\f]
Followed by dual neighbor verification against machine-precision boundary limits:
- **Upper Guard**: If \f$ x \ge x_{\min} + (i_{\text{prov}} + 1) \Delta \f$, \f$ i = i_{\text{prov}} + 1 \f$.
- **Lower Guard**: If \f$ x < x_{\min} + i_{\text{prov}} \Delta \f$, \f$ i = i_{\text{prov}} - 1 \f$.
If \f$ \Delta^{-1} \f$ overflows to infinity on subnormal ranges, lookup falls back cleanly to division.

### 3.2 Variable-Width Bisection Binary Search (O(log N))
For variable binning defined by monotonic edges \f$ e_0 < e_1 < \dots < e_N \f$, bin lookup uses bisection search on \f$ [0, N) \f$:
- Invariant: \f$ x \in [e_i, e_{i+1}) \f$ with exact matching at \f$ e_0 = x_{\min} \f$ and \f$ e_N = x_{\max} \f$.
- Guaranteed \f$ O(\log N) \f$ time and \f$ O(1) \f$ auxiliary space.

### 3.3 Online Weighted Welford Statistics (O(1))
When `HISTO_FLAG_EXACT_MOMENTS` is enabled, running moments are accumulated without storing historical samples:
\f[
W_{\text{new}} = W_{\text{old}} + w_i
\f]
\f[
\delta = x_i - \mu_{\text{old}}
\f]
\f[
\mu_{\text{new}} = \mu_{\text{old}} + \frac{w_i}{W_{\text{new}}} \delta
\f]
\f[
M_{2, \text{new}} = M_{2, \text{old}} + w_i \delta (x_i - \mu_{\text{new}})
\f]
Variance is extracted as \f$ \sigma^2 = M_2 / W \f$. The algorithm natively supports negative event weights and avoids variance underflow via \f$ M_2 \ge 0 \f$ clamping.

### 3.4 Non-Empty Support Linear Quantile Interpolation (O(N))
Quantile coordinates \f$ Q(p) = F^{-1}(p) \f$ for \f$ p \in [0.0, 1.0] \f$ are evaluated using continuous inverse CDF interpolation:
- Targets cumulative mass \f$ T = p \cdot W_{\text{total}} \f$.
- Locates bin \f$ i \f$ where \f$ \sum_{j=0}^{i-1} w_j < T \le \sum_{j=0}^i w_j \f$.
- Computes within-bin linear coordinate:
\f[
Q(p) = \text{low}_i + \frac{T - \sum_{j=0}^{i-1} w_j}{w_i} (\text{high}_i - \text{low}_i)
\f]

### 3.5 Division-Free Error Propagation (O(N))
When combining histograms via multiplication (\f$ H = A \cdot B \f$) or division (\f$ H = A / B \f$), statistical uncertainties are propagated without intermediate divisions:
- **Product Uncertainty**:
\f[
\sigma_{A \cdot B}^2 = B^2 \sigma_A^2 + A^2 \sigma_B^2
\f]
- **Quotient Uncertainty**:
\f[
\sigma_{A / B}^2 = \frac{B^2 \sigma_A^2 + A^2 \sigma_B^2}{B^4}
\f]

---

## 4. Algorithmic Complexity Reference Table

| Function | Time Complexity | Space Complexity | Description |
| :--- | :--- | :--- | :--- |
| `histo_create_uniform` | O(N) | O(N) | Allocates and zeroes N bins |
| `histo_create_variable` | O(N) | O(N) | Validates edges and allocates N bins |
| `histo_destroy` | O(1) | O(1) | Deallocates histogram and buffers |
| `histo_clone` | O(N) | O(N) | Deep copies geometry, bins, and moments |
| `histo_reset` | O(N) | O(1) | Zeroes all bins and resets accumulators |
| `histo_find_bin` (Uniform) | O(1) | O(1) | Boundary-guarded reciprocal lookup |
| `histo_find_bin` (Variable) | O(log N) | O(1) | Binary search over monotonic edges |
| `histo_fill` / `histo_fill_w` (Uniform) | O(1) | O(1) | Constant-time bin increment |
| `histo_fill` / `histo_fill_w` (Variable) | O(log N) | O(1) | Binary search + bin increment |
| `histo_fill_n` / `histo_fill_strided` | O(K * L) | O(1) | Batch ingest K samples (L is lookup cost) |
| `histo_fill_bin` | O(1) | O(1) | Direct indexed bin accumulation |
| `histo_nbins` / `histo_bin_type` / `histo_range` | O(1) | O(1) | Header field accessor |
| `histo_bin_bounds` / `histo_bin_center` | O(1) | O(1) | Boundary interval / midpoint query |
| `histo_bin_content` / `histo_bin_error` / `histo_bin_sum_w2` | O(1) | O(1) | Bin value and uncertainty query |
| `histo_total_weight` / `histo_n_fills` / counters | O(1) | O(1) | Summary accumulator accessors |
| `histo_mean` / `histo_variance` (Exact Moments) | O(1) | O(1) | Direct return from Welford accumulator |
| `histo_mean` / `histo_variance` (Two-Pass) | O(N) | O(1) | Two-pass bin-center moments calculation |
| `histo_std_dev` | O(1) or O(N) | O(1) | Square root of variance |
| `histo_quantile` / `histo_median` | O(N) | O(1) | Single-pass cumulative CDF scan |
| `histo_integral` | O(M) | O(1) | Sum over M <= N bins |
| `histo_get_stats` | O(N) | O(1) | Aggregates moments, min/max, and median |
| `histo_add` / `histo_subtract` | O(N) | O(1) | Element-wise vector arithmetic and moment update |
| `histo_multiply` / `histo_divide` | O(N) | O(1) | Element-wise arithmetic + sum_w2 error propagation |
| `histo_scale` | O(N) | O(1) | Linear scaling of all bins and moments |
| `histo_normalize` | O(N) | O(1) | Scales total weight to target area |
| `histo_rebin` | O(N) | O(N / k) | Allocates and merges adjacent uniform bins |
| `histo_slice` | O(M) | O(M) | Allocates sub-range histogram of M bins |
| `histo_cdf` | O(N) | O(N) | Allocates prefix-sum CDF histogram |
| `histo_serialize_binary_size` | O(1) | O(1) | Header (256B) + bin payload size calculation |
| `histo_serialize_binary_into` | O(N) | O(1) | Encodes Little-Endian wire format into caller buffer |
| `histo_serialize_binary` | O(N) | O(N) | Allocates buffer and encodes wire format |
| `histo_deserialize_binary` | O(N) | O(N) | Decodes wire format into newly allocated histogram |
| `histo_free_buffer` | O(1) | O(1) | Deallocates serialization buffer |
