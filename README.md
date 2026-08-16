# libhisto

**libhisto** is a high-performance, portable, memory-safe ISO C99 1D histogramming library designed for high-throughput scientific computing, physics simulations, telemetry ingestion, and general statistical analysis. It offers exact online Welford statistics, deterministic binary serialization, and zero-allocation ingestion loops capable of processing tens of millions of samples per second.

## Key Features

- **Flexible Binning:** Uniform (O(1)) and Variable-width (O(log N)) binning configurations.
- **Exact Statistics:** Online exact Welford moments for mean and variance, with support for negative weights and protection against precision loss.
- **Error Propagation:** Division-free `sum_w2` statistical uncertainty propagation across arithmetic operations.
- **Advanced Operations:** Continuous quantile interpolation, vector arithmetic (add, sub, mul, div, scale, normalize, rebin, slice, cdf).
- **Portable Serialization:** Canonical 256-byte Little-Endian binary wire format for reliable cross-platform data interchange.
- **Memory Safety:** 100% leak-free, defensive pointer checks, and clear ownership semantics.

## 5-Minute Quickstart

Here's how to create a histogram, ingest data, extract statistics, and serialize the results:

```c
#include <stdio.h>
#include <histo/histo.h>

int main(void) {
    // 1. Create a uniform histogram: 100 bins from 0.0 to 100.0
    // We enable sum_w2 tracking for statistical errors and exact online Welford moments.
    histo_t *h = histo_create_uniform(100, 0.0, 100.0, 
                                      HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    if (!h) return 1;

    // 2. Ingest samples: unit weight and weighted
    histo_fill(h, 25.4);
    histo_fill_w(h, 50.0, 2.5); // Fill value 50.0 with weight 2.5

    // 3. Batch ingestion from array
    double samples[3] = {12.0, 45.0, 88.0};
    histo_fill_n(h, 3, samples, NULL); // NULL weights implies unit weights

    // 4. Query statistics
    double mean = 0.0, std_dev = 0.0, median = 0.0;
    histo_mean(h, &mean);
    histo_std_dev(h, &std_dev);
    histo_median(h, &median);
    printf("Mean: %.3f, Std Dev: %.3f, Median: %.3f\n", mean, std_dev, median);

    // 5. Binary serialization roundtrip
    void *buffer = NULL;
    size_t size = 0;
    histo_serialize_binary(h, &buffer, &size);
    histo_t *h_loaded = histo_deserialize_binary(buffer, size);
    
    // 6. Clean teardown
    histo_free_buffer(buffer);
    histo_destroy(h_loaded);
    histo_destroy(h);
    
    return 0;
}
```

Compile with:
```bash
gcc -O3 -std=c99 -Wall -Wextra -Iinclude example.c -Lbuild -lhisto -lm -o example
```

## Integration with CMake

You can easily integrate `libhisto` into your CMake project using `FetchContent` or `add_subdirectory`:

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

## Core Concepts

- **Uniform vs. Variable Bins:** Uniform binning divides a range into equally sized segments, enabling blazing-fast O(1) bin lookups. Variable binning allows custom, monotonically increasing bin edges, utilizing a highly optimized O(log N) bisection binary search.
- **`sum_w2` Statistical Uncertainties:** `libhisto` accurately tracks the sum of squared weights (`sum_w2`) for each bin. This provides reliable statistical error bars on your data and enables robust, division-free error propagation during histogram arithmetic (like multiplication and division).
- **Exact Online Welford Moments:** When enabled, the library computes running mean and variance continuously as data streams in, without ever needing to store the individual samples or make a second pass. It natively supports negative weights and utilizes Welford's numerically stable algorithm to prevent precision loss and catastrophic cancellation.

## Performance Envelope

Real measured benchmarks demonstrating `libhisto`'s throughput:

- **Batch Ingestion:** ~93.1 Mops/s (10.7 ns/sample)
- **Single Fill:** ~45.6 Mops/s (21.9 ns/op)
- **Weighted Fill with sum_w2:** ~43.1 Mops/s (23.2 ns/op)
- **Variable Bin Binary Search (100 bins):** ~22.2 Mops/s (45.0 ns/op)
- **Binary Serialization:** ~13,500 serializations/sec (10k bins)

## Dependencies

- **Core Library:** Zero external dependencies (ISO C99 standard library only).
- **Build System:** CMake 3.15+ (Required).
- **Documentation:** Doxygen 1.9+ & Graphviz (Optional, for generating HTML manual).
- **Testing:** Unity (Bundled in `tests/vendor/unity`, for test runner).

## Build Instructions & Targets

The build system relies on CMake. You can use standard CMake commands or the provided Makefile wrappers.

- **Build library (`libhisto.a`):**
  ```bash
  make build
  # or
  cmake -B build -S . && cmake --build build
  ```
- **Run tests:**
  ```bash
  make test
  # or
  ctest --test-dir build --output-on-failure
  ```
- **Run tests with AddressSanitizer (ASan):**
  ```bash
  make test-asan
  # or
  cmake -B build-asan -S . -DLIBHISTO_ENABLE_ASAN=ON && cmake --build build-asan && ctest --test-dir build-asan --output-on-failure
  ```
- **Run tests with ThreadSanitizer (TSan):**
  ```bash
  make test-tsan
  # or
  cmake -B build-tsan -S . -DLIBHISTO_ENABLE_TSAN=ON && cmake --build build-tsan && ctest --test-dir build-tsan --output-on-failure
  ```
- **Run tests with MemorySanitizer (MSan) (Clang only):**
  ```bash
  make test-msan
  # or
  cmake -B build-msan -S . -DLIBHISTO_ENABLE_MSAN=ON && cmake --build build-msan && ctest --test-dir build-msan --output-on-failure
  ```
- **Run tests under Valgrind memcheck:**
  ```bash
  make memcheck
  # or
  make test-valgrind
  ```
- **Generate documentation:**
  ```bash
  make docs
  # or
  cmake --build build --target docs
  # Output: build/docs/html/index.html
  ```
- **Run benchmarks:**
  ```bash
  ./build/bench/bench_histo
  ```
- **Run quickstart example:**
  ```bash
  ./build/examples/quickstart
  ```

## License and Author

**License:** [MIT License](LICENSE)

**Author:** Steffen Mueller
