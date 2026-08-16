# Design Specification: Numerical Behavior, Precision & Error Handling

This document specifies the exact numerical semantics, IEEE-754 edge case policies, boundary conventions, and error-handling strategies for `libhisto`.

---

## 1. IEEE-754 Floating-Point Policies

All floating-point coordinates, weights, and accumulators in `libhisto` utilize 64-bit double precision (`double`).

### 1.1 Non-Finite & Special Values
| Value | Single Fill (`histo_fill`/`histo_fill_w`) | Batch Fill (`histo_fill_n`/`histo_fill_strided`) | Range / Bin Query |
| :--- | :--- | :--- | :--- |
| **`NaN`** (Quiet or Signaling) | Rejects indexing. Increments `n_nan`. Returns `HISTO_ERR_NON_FINITE`. Never mutates bins. | Increments `n_nan`. Skips sample and continues loop. Returns `HISTO_WARN_NON_FINITE`. | Returns `HISTO_ERR_NON_FINITE` and `*out_bin = -1`. |
| **`+Infinity`** | Categorized as **overflow**. Increments `overflow_weight`, `overflow_sum_w2`, and `n_overflow`. | Ingests into overflow. | Returns `HISTO_OK` and `*out_bin = (int64_t)nbins`. |
| **`-Infinity`** | Categorized as **underflow**. Increments `underflow_weight`, `underflow_sum_w2`, and `n_underflow`. | Ingests into underflow. | Returns `HISTO_OK` and `*out_bin = -1`. |
| **`weight = NaN / ±Inf`** | Rejects fill; increments `n_nan`. Returns `HISTO_ERR_NON_FINITE`. | Increments `n_nan`. Skips sample; returns `HISTO_WARN_NON_FINITE`. | N/A |
| **`-0.0` (Negative Zero)** | Treated identically to `+0.0` per IEEE-754 rules. | Treated identically to `+0.0`. | Mapped to matching interval containing $0.0$. |
| **Subnormals (Denormals)** | Handled standardly without flushing to zero. | Handled standardly. | Standard interval lookup. |

---

## 2. Bin Interval & Boundary Semantics

### 2.1 Standard Half-Open Interval Rule
`libhisto` adheres strictly to the standard **half-open interval convention** $[x_{\text{lower}}, x_{\text{upper}})$:
- A sample $x$ belongs to bin $i$ if and only if:
  $$x_{\text{lower}, i} \le x < x_{\text{upper}, i}$$
- **Lower Edge Inclusivity**: If $x = x_{\min}$, $x$ belongs to bin $0$.
- **Upper Edge Exclusivity**: If $x = x_{\max}$, $x$ is categorized as **overflow** ($x \ge x_{\max}$).

### 2.2 Uniform Binning Lookup: Fast Reciprocal with Boundary Guard
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

/* Fast O(1) candidate index using precomputed reciprocal (nbins / width) */
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

## 3. Error Handling Architecture

### 3.1 Warning Codes for Batch Operations
- `HISTO_WARN_NON_FINITE` ($+1$): Returned by batch ingestion functions (`histo_fill_n`, `histo_fill_strided`) if one or more `NaN` or non-finite weights were skipped, while all valid finite samples were successfully ingested.

### 3.2 Thread-Safety Guarantee
- All read-only query and statistical functions (`histo_mean`, `histo_quantile`, `histo_bin_content`, etc.) accept `const histo_t *h` and perform **zero internal memory mutations**.
- Distinct threads may safely read and query the same `histo_t` instance concurrently without synchronization.
