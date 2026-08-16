# Design Specification: Numerical Behavior, Precision & Error Handling

This document specifies the exact numerical semantics, IEEE-754 edge case policies, boundary conventions, and error-handling strategies for `libhisto`.

---

## 1. IEEE-754 Floating-Point Policies

All floating-point coordinates, weights, and accumulators in `libhisto` utilize 64-bit double precision (`double`).

### 1.1 Non-Finite & Special Values
| Value | Fill Ingestion Policy | Bin Lookup / Range Query |
| :--- | :--- | :--- |
| **`NaN`** (Quiet or Signaling) | Rejects indexing. Increments `n_nan` counter. Returns `HISTO_ERR_NON_FINITE`. Never mutates bins. | Returns `HISTO_ERR_NON_FINITE` and `*out_bin = -1`. |
| **`+Infinity`** | Categorized as **overflow**. Increments `overflow_weight`, `overflow_sum_w2`, and `n_overflow`. | Returns `HISTO_OK` and `*out_bin = (int64_t)nbins`. |
| **`-Infinity`** | Categorized as **underflow**. Increments `underflow_weight`, `underflow_sum_w2`, and `n_underflow`. | Returns `HISTO_OK` and `*out_bin = -1`. |
| **`-0.0` (Negative Zero)** | Treated identically to `+0.0` per IEEE-754 comparison rules. | Mapped to matching bin interval containing $0.0$. |
| **Subnormals (Denormals)** | Handled standardly without flushing to zero unless platform hardware forces DAZ. | Standard interval lookup. |

---

## 2. Bin Interval & Boundary Semantics

### 2.1 Standard Half-Open Interval Rule
`libhisto` adheres strictly to the standard **half-open interval convention** $[x_{\text{lower}}, x_{\text{upper}})$:
- A sample $x$ belongs to bin $i$ if and only if:
  $$x_{\text{lower}, i} \le x < x_{\text{upper}, i}$$
- **Lower Edge Inclusivity**: If $x = x_{\min}$, $x$ belongs to bin $0$.
- **Upper Edge Exclusivity**: If $x = x_{\max}$, $x$ is categorized as **overflow** ($x \ge x_{\max}$).

### 2.2 Uniform Binning Lookup Algorithm & Clamping
For a uniform histogram with range $[x_{\min}, x_{\max}]$ and $N$ bins with bin size $w = (x_{\max} - x_{\min}) / N$:

```c
if (isnan(x)) {
    return HISTO_ERR_NON_FINITE;
}
if (x < min) {
    /* Underflow */
    return HANDLE_UNDERFLOW;
}
if (x >= max) {
    /* Overflow */
    return HANDLE_OVERFLOW;
}

/* Fast O(1) index computation */
int64_t idx = (int64_t)((x - min) / binsize);

/* Floating-point rounding guard */
if (idx < 0) {
    idx = 0;
} else if ((uint32_t)idx >= nbins) {
    idx = (int64_t)nbins - 1;
}
```

### 2.3 Variable Binning Binary Search Algorithm
For variable bins defined by monotonic edges $E = [x_0, x_1, \dots, x_N]$:
1. If $x < E[0] \implies$ Underflow.
2. If $x \ge E[N] \implies$ Overflow.
3. Binary search via bisection finds index $i \in [0, N-1]$ such that $E[i] \le x < E[i+1]$ in $\lceil \log_2 N \rceil$ comparisons:

```c
uint32_t low = 0;
uint32_t high = nbins; /* size of edges array is nbins + 1 */

while (low < high) {
    uint32_t mid = low + (high - low) / 2;
    if (x >= edges[mid + 1]) {
        low = mid + 1;
    } else {
        high = mid;
    }
}
return low;
```

---

## 3. Error Handling Architecture

### 3.1 Return Value Conventions
1. **Functions Returning Results via Pointers**: Return `histo_status_t`. Output parameters are only populated when the status is `HISTO_OK`.
2. **Factory / Constructor Functions**: Return valid pointer `histo_t*` on success, or `NULL` on error.
3. **Defensive Pointer Auditing**: Every public function immediately validates input pointers:
   ```c
   if (h == NULL) {
       return HISTO_ERR_INVALID_ARG;
   }
   ```

### 3.2 Thread-Safety Guarantee
- All read-only query and statistical functions (`histo_mean`, `histo_quantile`, `histo_bin_content`, etc.) accept `const histo_t *h` and perform **zero internal memory mutations**.
- Distinct threads may safely read and query the same `histo_t` instance concurrently without synchronization.
