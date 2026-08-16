# libhisto: High-Level Goals and Scope

`libhisto` is a lightweight, high-performance, plain C library dedicated to 1-dimensional in-memory histogramming.

---

## 1. Core Objectives & Design Principles

- **In-Memory & 1-Dimensional**: Focused on fast, single-dimensional histogram representation and manipulation residing entirely in memory.
- **Performance**: Optimized bin-lookup and sample accumulation routines with minimal per-sample overhead.
- **Low Dependencies**: Written in standard C (ISO C99/C11) with no external runtime dependencies beyond the standard C library (`libc` / `libm`).
- **Portability**: Highly portable across architectures (32-bit/64-bit, endian-agnostic) and operating systems (Linux, macOS, Windows, BSD, embedded environments).
- **Correctness**: Rigorously defined numerical behavior, robust handling of floating-point edge cases (NaN, $\pm\infty$, precision limits), explicit overflow/underflow handling, and deterministic calculations.
- **Memory Safety & Robustness**: Explicit memory ownership, strict bounds checking, defensive API design to avoid buffer overflows and memory leaks, and clear error reporting.
- **Well-Tested**: Comprehensive automated testing, including unit tests, edge-case validation, property-based tests, and memory sanitizer validation (ASan/UBSan/Valgrind).

---

## 2. Target Feature Set

### 2.1 Binning Models
- **Fixed-Width (Uniform) Bins**:
  - Defined by range $[x_{\min}, x_{\max}]$ and number of bins $N$.
  - Constant time $O(1)$ bin index computation.
- **Variable-Width (Non-Uniform) Bins**:
  - Defined by an arbitrary, strictly monotonic array of bin boundaries/edges: $x_0 < x_1 < \dots < x_N$.
  - Fast $O(\log N)$ bin lookup via binary search.
- **Out-of-Range Handling**:
  - Dedicated tracking for underflow ($x < x_{\min}$) and overflow ($x \ge x_{\max}$) samples and weights.
  - Robust handling of non-finite inputs (`NaN`, `+Inf`, `-Inf`).

### 2.2 Core APIs
- **Lifecycle Management**:
  - Creation, initialization, copying/cloning, resetting, and destruction.
- **Filling / Ingestion**:
  - Single-value insertion (`fill(h, value)`).
  - Weighted sample insertion (`fill_weighted(h, value, weight)`).
  - Batch / vector filling for performance optimization.
- **Serialization & Deserialization**:
  - Export/import to portable representations (e.g., structured binary format and human-readable text/JSON).
  - Cross-platform byte-order and schema compatibility.
- **Statistical Analysis & Querying**:
  - Bin count/weight retrieval and bin edge inspection.
  - Summary statistics: total counts/weights, sample mean, variance, standard deviation, skewness, and kurtosis (exact or bin-approximated).
  - Range and quantile/percentile estimations.
  - Peak and mode detection.

### 2.3 Advanced & Higher-Level Capabilities (Roadmap)
- **Histogram Arithmetic**:
  - Addition / merging of compatible histograms ($H_1 + H_2$).
  - Subtraction ($H_1 - H_2$) and scalar multiplication/scaling ($\alpha \cdot H$).
- **Distribution Functions**:
  - Empirical Probability Density Function (PDF) generation / normalization.
  - Cumulative Distribution Function (CDF) computation.
- **Rebinning & Transformation**:
  - Coarsening/merging adjacent bins.
  - Range slicing and re-centering.
- **Error Propagation**:
  - Support for sum-of-weights squared ($\sum w^2$) to track Poisson/weighted statistical uncertainties per bin.

---

## 3. Project Phases

1. **Phase a: Research and High-Level Goals** *(Current)*
2. **Phase b: Design and Documentation** (API specification, data structures, error-handling strategy, architecture)
3. **Phase c: Implementation** (Core algorithms, test suites, build configuration, benchmarks)
