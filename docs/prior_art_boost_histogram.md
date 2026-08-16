# Prior Art Analysis: Boost.Histogram

**Boost.Histogram** (authored by Hans Dembinski) is a modern, high-performance C++14 header-only histogramming library distributed under the permissive **Boost Software License 1.0 (BSL-1.0)**.

---

## 1. Core Architecture & Design Highlights

### 1.1 Separation of Axis and Storage
Boost.Histogram cleanly decouples two primary abstractions:
1. **Axis Types**: Define the coordinate transformation from continuous sample space to discrete bin index.
   - **Regular Axis**: Uniform bin width $w = (x_{\max} - x_{\min}) / N$, transformed via fast arithmetic $O(1)$.
   - **Variable Axis**: Arbitrary bin edges, transformed via binary search ($O(\log N)$).
   - **Integer / Category Axis**: Discrete integer or enum mappings.
   - **Logarithmic / Power / Circular Axes**: Specialized mathematical transformations.
2. **Storage / Accumulator Types**: Define what is accumulated per bin.
   - **Count Storage (`uint64_t` or `double`)**: Simple frequency counter.
   - **Weighted Storage (`weight_storage`)**: Simultaneously tracks sum of weights $\sum w$ and sum of squared weights $\sum w^2$.
   - **Profile / Mean Storage**: Tracks sample count $N$, mean $\mu$, and variance $M_2$ per bin (using online Welford updates).

### 1.2 Underflow, Overflow, and NaN Handling
- **Underflow & Overflow**: Treated as dedicated auxiliary bins (index $-1$ and index $N$, or integrated within an internal extended indexing scheme $[0, N+1]$).
- **Non-Finite (`NaN`) Handling**: `NaN` is explicitly trapped and routed to a dedicated category or the overflow/invalid bin, rather than allowing undefined behavior or memory corruption.

### 1.3 Statistical Error & Variance
- In weighted histograms, the variance of the bin content is rigorously given by:
  $$\sigma_i^2 = \sum_{j \in \text{bin } i} w_j^2$$
- Storing $\sum w_i$ and $\sum w_i^2$ separately allows exact Poisson error calculation for unweighted fills ($w=1 \implies \sigma_i = \sqrt{N_i}$) and proper variance estimation for weighted samples.

---

## 2. Key Techniques & Lessons for `libhisto`

### 2.1 Storage Modes in C
In a pure C library, templated storage is not available, but we can support:
1. **Standard Storage (`HISTO_STORAGE_DOUBLE`)**: A single array of `double` tracking weights/counts per bin.
2. **Weighted/Error Storage (`HISTO_STORAGE_WEIGHTED`)**: Two parallel `double` arrays (or an array of `struct { double weight; double sum_w2; }`) tracking $\sum w$ and $\sum w^2$ per bin.

### 2.2 Boundary Invariants & Search Optimizations
- For variable bins of size $N$ (requiring $N+1$ boundaries $x_0 < x_1 < \dots < x_N$), binary search should use upper/lower bounds with branchless or linear fallback for small $N$ ($N \le 8$).
- In uniform binning, bounds clipping prevents float precision rounding errors at $x \approx x_{\max}$.

### 2.3 Binary Serialization Standards
- Serializing both axis configuration metadata and accumulator arrays in a compact, endian-deterministic layout.

---

## 3. Comparison with `libhisto` Goals

| Aspect | Boost.Histogram (C++) | `libhisto` (Plain C) |
| :--- | :--- | :--- |
| **Language** | C++14 (Templates / Metaprogramming) | Strict ISO C99 |
| **Dependencies** | Boost / C++ Standard Library | `libc` / `libm` only |
| **ABI / Linkage** | Header-only C++ | Stable C ABI (`libhisto.so` / `libhisto.a`) |
| **Binning Strategy** | Modular compile-time & runtime axes | Runtime polymorphic `histo_t` (Fixed / Variable) |
| **Uncertainties** | Optional `weight_storage` ($\sum w, \sum w^2$) | Built-in $\sum w^2$ error tracking support |
