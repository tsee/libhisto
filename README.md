# libhisto

**libhisto** is a high-performance, portable, memory-safe ISO C99 1D histogramming library designed for high-throughput scientific computing, physics simulations, telemetry ingestion, and general statistical analysis. It offers exact online Welford moments, robust non-parametric statistics, two-distribution distance metrics, deterministic binary serialization, and zero-allocation ingestion loops capable of processing tens of millions of samples per second.

## Key Features

- **Flexible Binning:** Uniform ($O(1)$) and Variable-width ($O(\log N)$) binning configurations with boundary-guarded floating-point indexing.
- **Exact Online Moments:** Online weighted Welford moments for sample mean and variance with variance floor clamping and full support for negative weights.
- **Statistical Uncertainty:** Division-free `sum_w2` statistical error tracking and propagation across arithmetic operations (addition, subtraction, multiplication, division).
- **Comprehensive Summary Statistics:** Mean, variance, standard deviation, higher-order central moments ($M_k$), skewness, kurtosis, and excess kurtosis.
- **Peak & Shape Characterization:** Discrete mode bin lookup, continuous 3-point parabolic peak interpolation, Full Width at Half Maximum (FWHM), and Root Mean Square (RMS).
- **Robust Non-Parametric Measures:** Piecewise-linear quantile interpolation, Median, Interquartile Range (IQR), Median Absolute Deviation (MAD), Trimmed Mean, and Winsorized Mean.
- **Two-Distribution Comparisons:** Weighted Chi-Square ($\chi^2$) test with degrees of freedom (NDF), Kolmogorov-Smirnov ($D$), 1D Wasserstein / Earth Mover's Distance ($W_1$), Kullback-Leibler (KL) divergence, and Bhattacharyya distance.
- **Transformations & Arithmetic:** Element-wise vector arithmetic (`add`, `subtract`, `multiply`, `divide`), scalar scaling, normalization to target area, integer rebinning, slicing, and cumulative distribution functions (CDF).
- **Portable Serialization:** Canonical 256-byte Little-Endian binary wire format with automatic format migration (`histo_migrate_binary`) for cross-platform data interchange.
- **Memory Safety & Portability:** Strict ISO C99, 100% leak-free, thread-safe concurrent queries, zero compiler warnings, and clean ASan/UBSan/TSan/Valgrind validation.

---

## 5-Minute Quickstart

Here is a quick demonstration of creating a histogram, filling data, calculating statistics, and serializing to binary format:

```c
#include <stdio.h>
#include <histo/histo.h>

int main(void) {
    // 1. Create a uniform histogram: 100 bins over [0.0, 100.0]
    // Enable sum_w2 error tracking and exact online Welford statistics
    histo_t *h = histo_create_uniform(100, 0.0, 100.0, 
                                      HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    if (!h) {
        fprintf(stderr, "Failed to allocate histogram\n");
        return 1;
    }

    // 2. Ingest samples: unit weight and explicit weights
    histo_fill(h, 25.4);
    histo_fill_w(h, 50.0, 2.5); // Fill coordinate 50.0 with weight 2.5

    // 3. High-throughput batch ingestion from array
    double samples[3] = {12.0, 45.0, 88.0};
    histo_fill_n(h, 3, samples, NULL); // NULL weights implies unit weight (1.0)

    // 4. Query statistics and shape metrics
    double mean = 0.0, std_dev = 0.0, median = 0.0, fwhm = 0.0;
    histo_mean(h, &mean);
    histo_std_dev(h, &std_dev);
    histo_median(h, &median);
    histo_fwhm(h, &fwhm);
    printf("Mean: %.3f | Std Dev: %.3f | Median: %.3f | FWHM: %.3f\n",
           mean, std_dev, median, fwhm);

    // 5. Binary serialization roundtrip (Canonical Little-Endian)
    void *buffer = NULL;
    size_t size = 0;
    if (histo_serialize_binary(h, &buffer, &size) == HISTO_OK) {
        histo_t *h_loaded = NULL;
        if (histo_deserialize_binary(buffer, size, &h_loaded) == HISTO_OK) {
            printf("Successfully reloaded histogram with %lu entries!\n",
                   (unsigned long)histo_num_entries(h_loaded));
            histo_destroy(h_loaded);
        }
        histo_free_buffer(buffer);
    }
    
    // 6. Clean teardown (0 memory leaks)
    histo_destroy(h);
    return 0;
}
```

Compile with:
```bash
gcc -O3 -std=c99 -Wall -Wextra -Iinclude example.c -Lbuild -lhisto -lm -o example
```

---

## Integration with CMake

Integrate `libhisto` into your CMake project using `FetchContent` or `add_subdirectory`:

### Using `FetchContent`
```cmake
include(FetchContent)
FetchContent_Declare(
    libhisto
    GIT_REPOSITORY https://github.com/your-org/libhisto.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(libhisto)

target_link_libraries(your_target PRIVATE libhisto::libhisto)
```

### Using `add_subdirectory`
```cmake
add_subdirectory(path/to/libhisto)
target_link_libraries(your_target PRIVATE libhisto::libhisto)
```

---

## Core Architectural Concepts

- **Uniform vs. Variable Bins:**
  - *Uniform Binning*: Divides range $[x_{\min}, x_{\max})$ into equal widths, enabling $O(1)$ lookups via boundary-guarded reciprocal multiplication.
  - *Variable Binning*: Allows arbitrary monotonically increasing bin edges, utilizing an optimized $O(\log N)$ bisection binary search.
- **Statistical Uncertainty (`sum_w2`):**
  - Accurately tracks per-bin $\sum w_i^2$, providing statistical error bars ($\sigma_i = \sqrt{\sum w_i^2}$) and enabling division-free error propagation for histogram products and quotients.
- **Exact Online Welford Moments:**
  - When `HISTO_FLAG_EXACT_MOMENTS` is active, running mean and $M_2$ are updated in $O(1)$ on every fill without sample storage or bin discretization artifacts.
- **Robust & Distribution Comparison Metrics:**
  - Compute higher-order moments (skewness, kurtosis), robust non-parametric metrics (IQR, MAD, trimmed/winsorized mean), and measure statistical distances ($\chi^2$, Kolmogorov-Smirnov, 1D Wasserstein, KL divergence, Bhattacharyya) between histograms with matching geometry.

---

## Performance Envelope

Measured benchmarks on modern x86_64 architecture:

| Operation | Throughput | Latency |
| :--- | :--- | :--- |
| **Batch Ingestion (`histo_fill_n`)** | ~93.1 Mops/s | 10.7 ns/sample |
| **Single Fill (`histo_fill`)** | ~45.6 Mops/s | 21.9 ns/op |
| **Weighted Fill + `sum_w2` (`histo_fill_w`)** | ~43.1 Mops/s | 23.2 ns/op |
| **Variable Bin Lookup (100 bins)** | ~22.2 Mops/s | 45.0 ns/op |
| **Binary Serialization (10k bins)** | ~13,500 saves/s | 74.0 µs/op |
| **Quantile Scan / CDF Interpolation** | ~12.5 Mops/s | 80.0 ns/op |

---

## Unix CLI Toolkit

`libhisto` ships with a high-performance Unix CLI toolkit designed for stream processing and terminal visualization. The tools can be invoked via the multi-call dispatcher (`histo <cmd>`) or direct standalone symlinks (`histo-fill`, `histo-plot`, `histo-stats`, `histo-cmp`):

```text
               ┌────────────┐
Data Stream ──>│ histo-fill │──> Binary Stream / JSON / TSV
               └────────────┘        │
             ┌───────────────────────┼───────────────────────┐
             ▼                       ▼                       ▼
      ┌────────────┐          ┌─────────────┐         ┌───────────┐
      │ histo-plot │          │ histo-stats │         │ histo-cmp │
      └────────────┘          └─────────────┘         └───────────┘
   (ASCII / Unicode Plot)     (Moment Report)       (A/B Comparison)
```

### Quick CLI Example: Streaming Monte Carlo to Terminal Plot

Generate 100,000 Gaussian random samples, pipe through the binary wire format into `histo-fill`, and render with TrueColor gradients, 1/8th sub-character Unicode blocks (` ▂▃▄▅▆▇█`), and summary statistics:

```bash
python3 -c "import random, sys; sys.stdout.write(''.join(f'{random.gauss(50, 15):.4f}\n' for _ in range(100000)))" | \
    histo-fill --bins=25 --min=0 --max=100 -o binary | \
    histo-plot --color=always --title="Gaussian Monte Carlo (N=100k, μ=50, σ=15)"
```

**Terminal Output:**
```text
Gaussian Monte Carlo (N=100k, μ=50, σ=15) (Entries: 99919, Total Weight: 99919)
┌─ Statistics ──────────────────────────────────────────────────────────────┐
│ Mean: 49.97  │ StdDev: 14.96 │ Median: 49.97 │ IQR: 20.18  │ Mode: 49.91  │
└───────────────────────────────────────────────────────────────────────────┘
[  0.00,   4.00) │     63 │ ▎
[  4.00,   8.00) │    151 │ ▋
[  8.00,  12.00) │    314 │ █▍
[ 12.00,  16.00) │    593 │ ██▊
[ 16.00,  20.00) │   1111 │ █████▎
[ 20.00,  24.00) │   1901 │ █████████
[ 24.00,  28.00) │   2929 │ █████████████▉
[ 28.00,  32.00) │   4410 │ ████████████████████▉
[ 32.00,  36.00) │   6002 │ ████████████████████████████▍
[ 36.00,  40.00) │   7787 │ ████████████████████████████████████▉
[ 40.00,  44.00) │   9213 │ ███████████████████████████████████████████▋
[ 44.00,  48.00) │  10301 │ ████████████████████████████████████████████████▊
[ 48.00,  52.00) │  10546 │ ██████████████████████████████████████████████████
[ 52.00,  56.00) │  10278 │ ████████████████████████████████████████████████▋
[ 56.00,  60.00) │   9277 │ ███████████████████████████████████████████▉
[ 60.00,  64.00) │   7656 │ ████████████████████████████████████▎
[ 64.00,  68.00) │   6007 │ ████████████████████████████▍
[ 68.00,  72.00) │   4313 │ ████████████████████▍
[ 72.00,  76.00) │   2943 │ █████████████▉
[ 76.00,  80.00) │   1892 │ ████████▉
[ 80.00,  84.00) │   1059 │ █████
[ 84.00,  88.00) │    655 │ ███
[ 88.00,  92.00) │    300 │ █▍
[ 92.00,  96.00) │    149 │ ▋
[ 96.00, 100.00) │     69 │ ▎
 Underflow: 36 │ In-Range: 99919 │ Overflow: 45 │ Non-Finite/NaN: 0
```

### CLI Tool Overview

1. **`histo-fill`**:
   - Ingests streaming text, CSV columns, or raw IEEE-754 Little-Endian binary `double` streams (`--binary-f64`).
   - Supports auto-ranging (`--auto-range`), custom variable bin edges (`--edges=...`), weighted samples (`-w`), in-flight rebinning (`--rebin`), and cumulative CDF conversion (`--cdf`).
   - Emits periodic intermediate snapshot blobs for live streaming pipelines (`--emit-interval=SEC` or `--emit-every=N`).

2. **`histo-plot`**:
   - Renders 1D histograms as terminal charts with automatic width adaptation.
   - Supports Unicode 1/8th fractional blocks, shaded blocks, ASCII bars (`--style=ascii`), ANSI 24-bit TrueColor density gradients (`--color=always`), logarithmic scaling (`--log`), error whiskers ($\pm \sigma$), and live continuous watch mode (`--watch`).

3. **`histo-stats`**:
   - Computes comprehensive summary reports including exact central moments (skewness, kurtosis), robust dispersion (IQR, MAD, trimmed/Winsorized means), and sub-bin continuous peak mode. Emits human-readable tables, TSV, or machine-readable JSON (`-f json`).

4. **`histo-cmp`**:
   - Compares two distributions and reports weighted Chi-Square ($\chi^2$/NDF), Kolmogorov-Smirnov supremum distance ($D$), 1D Wasserstein Earth Mover's Distance ($W_1$), Kullback-Leibler divergence ($D_{\text{KL}}$), and Bhattacharyya distance.

5. **`histo` (Multi-call Dispatcher)**:
   - Unified entry point: `histo fill ...`, `histo plot ...`, `histo stats ...`, `histo cmp ...`.

---

## Dependencies & Requirements

- **Core Library:** Zero external dependencies (strict ISO C99 standard library only: `math.h`, `stdint.h`, `stdbool.h`, `stdlib.h`, `string.h`).
- **Build System:** CMake 3.15+ (Required).
- **Testing:** Unity (Bundled in `tests/vendor/unity`).
- **Documentation:** Doxygen 1.9+ & MathJax (Optional, for generating HTML manual).

---

## Build Targets & Development Guide

Build and test using standard CMake or the provided `Makefile`:

```bash
# Build static library (libhisto.a)
make build
# or: cmake -B build -S . && cmake --build build

# Run unit and integration test suite
make test
# or: ctest --test-dir build --output-on-failure

# Run tests under AddressSanitizer (ASan) & UBSan
make test-asan

# Run tests under ThreadSanitizer (TSan)
make test-tsan

# Run tests under MemorySanitizer (MSan) (Clang only)
make test-msan

# Run tests under Valgrind Memcheck
make memcheck

# Run benchmark suite
./build/bench/bench_histo

# Run quickstart example
./build/examples/quickstart

# Generate HTML documentation (outputs to docs/html/index.html)
make docs
```

---

## License and Author

- **License:** [MIT License](LICENSE)
- **Author:** Steffen Mueller

