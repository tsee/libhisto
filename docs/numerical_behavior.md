# Design Specification: Numerical Behavior, Precision & Error Handling

This document specifies the exact numerical semantics, IEEE-754 floating-point edge case policies, boundary conventions, and error-handling strategies implemented in `libhisto`.

---

## 1. IEEE-754 Floating-Point Policies

All coordinates, weights, and statistical accumulators in `libhisto` utilize standard IEEE-754 64-bit double-precision floating-point numbers (`double`).

### 1.1 Non-Finite & Special Values Matrix

| Value / Condition | Single Fill (`histo_fill`/`histo_fill_w`) | Batch Fill (`histo_fill_n`/`histo_fill_strided`) | Range / Bin Query (`histo_find_bin`) |
| :--- | :--- | :--- | :--- |
| **`NaN`** (Quiet or Signaling) | Rejects indexing. Increments `n_nan`. Returns `HISTO_ERR_NON_FINITE`. Never mutates bins. | Increments `n_nan`. Skips sample and continues loop. Returns `HISTO_WARN_NON_FINITE`. | Returns `HISTO_ERR_NON_FINITE` and `*out_bin = -1`. |
| **`+Infinity`** | Categorized as **overflow**. Increments `overflow_weight`, `overflow_sum_w2`, and `n_overflow`. | Ingests into overflow. | Returns `HISTO_OK` and `*out_bin = (int64_t)nbins`. |
| **`-Infinity`** | Categorized as **underflow**. Increments `underflow_weight`, `underflow_sum_w2`, and `n_underflow`. | Ingests into underflow. | Returns `HISTO_OK` and `*out_bin = -1`. |
| **`weight = NaN / ±Inf`** | Rejects fill; increments `n_nan`. Returns `HISTO_ERR_NON_FINITE`. | Increments `n_nan`. Skips sample; returns `HISTO_WARN_NON_FINITE`. | N/A |
| **`-0.0` (Negative Zero)** | Treated identically to `+0.0` per IEEE-754 rules. | Treated identically to `+0.0`. | Mapped to matching interval containing $0.0$. |
| **Subnormals (Denormals)** | Processed without flushing to zero. | Processed standardly. | Standard interval lookup. |

---

## 2. Bin Interval & Boundary Semantics

### 2.1 Standard Half-Open Interval Rule
`libhisto` adheres strictly to the standard **half-open interval convention** $[x_{\text{lower}}, x_{\text{upper}})$:
- A sample $x$ belongs to bin $i$ if and only if:
  $$x_{\text{lower}, i} \le x < x_{\text{upper}, i}$$
- **Lower Edge Inclusivity**: If $x = x_{\min}$, $x$ belongs to bin $0$.
- **Upper Edge Exclusivity**: If $x = x_{\max}$, $x$ is categorized as **overflow** ($x \ge x_{\max}$).

### 2.2 Uniform Binning: Reciprocal Multiplication with Boundary Guards
Direct floating-point division `(int64_t)((x - min) / binsize)` can suffer from truncation error on exact boundaries (e.g. $x = 2/3$ evaluating to $1.9999999999999997 \to 1$ instead of $2$).

`libhisto` utilizes the **boundary-guarded fast reciprocal algorithm**:

```c
if (isnan(x)) {
    h->n_nan++;
    return HISTO_ERR_NON_FINITE;
}
if (x < h->min) {
    /* Underflow */
    h->underflow_weight += weight;
    if (h->sum_w2) h->underflow_sum_w2 += weight * weight;
    h->n_underflow++;
    return HISTO_OK;
}
if (x >= h->max) {
    /* Overflow */
    h->overflow_weight += weight;
    if (h->sum_w2) h->overflow_sum_w2 += weight * weight;
    h->n_overflow++;
    return HISTO_OK;
}

/* Fast O(1) candidate index using precomputed reciprocal */
int64_t idx = (int64_t)((x - h->min) * h->inv_binsize);

/* Bounds clamping [0, nbins - 1] */
if (idx < 0) {
    idx = 0;
} else if ((uint32_t)idx >= h->nbins) {
    idx = (int64_t)h->nbins - 1;
}

/* Boundary Guard 1: Check if x reached or exceeded next bin's lower boundary */
if (idx + 1 < (int64_t)h->nbins) {
    double next_lower = h->min + (double)(idx + 1) * h->binsize;
    if (x >= next_lower) {
        idx++;
    }
}

/* Boundary Guard 2: Check if x is below current bin's lower boundary */
if (idx > 0) {
    double curr_lower = h->min + (double)idx * h->binsize;
    if (x < curr_lower) {
        idx--;
    }
}

/* Safe, verified index */
h->bins[idx] += weight;
if (h->sum_w2) h->sum_w2[idx] += weight * weight;
h->total_weight += weight;
if (h->sum_w2) h->total_sum_w2 += weight * weight;
h->n_fills++;
```

### 2.3 Variable Binning Binary Search Algorithm
For variable bins defined by monotonic edges $E = [x_0, x_1, \dots, x_N]$:
1. Validate `!isnan(x)`.
2. If $x < E[0] \implies$ Underflow.
3. If $x \ge E[N] \implies$ Overflow.
4. Binary search via bisection maintains invariant $E[\text{low}] \le x < E[\text{low}+1]$:

```c
uint32_t low = 0;
uint32_t high = h->nbins;

while (low < high) {
    uint32_t mid = low + (high - low) / 2;
    if (x >= h->bin_edges[mid + 1]) {
        low = mid + 1;
    } else {
        high = mid;
    }
}
return low;
```

---

## 3. Numerical Guarding in Analytical Routines

### 3.1 Skewness & Kurtosis Zero-Variance Protection
Higher-order moment calculations divide by $\sigma^3$ (skewness) and $\sigma^4$ (kurtosis). When all filled samples are identical or variance is zero ($\sigma^2 \le 0.0$), `libhisto` safely prevents division by zero and returns `HISTO_ERR_DIV_BY_ZERO`.

### 3.2 Parabolic Mode Peak Localization
When estimating continuous mode via 3-point parabolic interpolation, the denominator is $\text{denom} = 2 w_m - w_{m-1} - w_{m+1}$.
- If $\text{denom} \le 0.0$ (flat plateau or non-convex peak), the algorithm smoothly falls back to the mode bin center.
- The sub-bin offset $\delta$ is clamped to $[-0.5, 0.5]$ to prevent the estimated peak from migrating outside the mode bin boundaries.

### 3.3 FWHM Flank Search Boundary Fallbacks
When searching for the half-maximum crossings on the left and right flanks of the dominant peak:
- If the distribution does not drop below half-maximum on the left side before reaching the lower range boundary, $x_{\text{left}}$ defaults to $x_{\min}$.
- If the distribution does not drop below half-maximum on the right side before reaching the upper range boundary, $x_{\text{right}}$ defaults to $x_{\max}$.
- If the peak height is non-positive, $\text{FWHM} = 0.0$.

### 3.4 Kullback-Leibler Divergence Singularity Protection
In `histo_cmp_kl_divergence`, relative entropy is calculated as $\sum P_i \ln(P_i / Q_i)$.
If $P_i > 0$ and $Q_i = 0$, evaluating $\ln(0)$ would produce $-\infty$. `libhisto` clamps empty target bins to a numerical regularization floor $\epsilon = 10^{-12}$, maintaining deterministic numerical bounds.

### 3.5 Geometry Compatibility Verification
All two-histogram functions (`histo_add`, `histo_subtract`, `histo_multiply`, `histo_divide`, `histo_cmp_chi2`, `histo_cmp_ks`, `histo_cmp_wasserstein_1d`, `histo_cmp_kl_divergence`, `histo_cmp_bhattacharyya`) strictly verify:
1. `bin_type` match (`HISTO_BIN_UNIFORM` vs `HISTO_BIN_VARIABLE`).
2. `nbins` equality.
3. Matching domain range limits: $|a_{\min} - b_{\min}| \le 10^{-12}$ and $|a_{\max} - b_{\max}| \le 10^{-12}$.
4. For variable bins, equality across all boundary edges: $|a_{\text{edges}, i} - b_{\text{edges}, i}| \le 10^{-12}$.

If geometries mismatch, the functions immediately return `HISTO_ERR_INCOMPATIBLE` without modifying any destination buffers.

---

## 4. Error Handling Architecture & Thread Safety

### 4.1 Return Code Conventions
- `HISTO_OK` ($0$): Operation succeeded completely.
- `HISTO_WARN_NON_FINITE` ($+1$): Batch operation succeeded for all finite entries, but non-finite (`NaN` / `Inf`) samples were encountered and skipped.
- Negative return values (`HISTO_ERR_*`): Fatal error; operation aborted with zero state mutation.

### 4.2 Thread-Safety Guarantees
- All read-only query and analytical functions accept `const histo_t *h` and perform **zero internal memory mutations** or static state modifications.
- Multiple threads may concurrently read, query, and compute statistics on the same `histo_t` instance without locks or synchronization.
