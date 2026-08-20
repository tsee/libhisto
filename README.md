# libhisto

**libhisto** is a high-performance, portable, memory-safe ISO C99 1D histogramming and quantile sketch library designed for high-throughput scientific computing, physics simulations, telemetry ingestion, and general statistical analysis. It offers exact online Welford moments, robust non-parametric statistics, DDSketch dynamic quantile sketches, SIMD vector acceleration (AVX-512 / AVX2 / NEON-ready), curve fitting (Levenberg-Marquardt), two-distribution distance metrics, deterministic binary serialization, and zero-allocation ingestion loops capable of processing hundreds of millions of samples per second.

---

## Visualizations at a Glance

`libhisto` ships with a high-performance Unix CLI toolkit (`histo`) designed for stream processing and terminal visualization.

### Real-Time Interactive Streaming Monitor (`histo top`)
Monitor live streaming data in an interactive 60 FPS terminal dashboard with analytical curve overlays, statistical moments, and 24-bit TrueColor bivariate heatmaps. See the [Real-Time Interactive Monitor Guide (`histo top`)](docs/top_manual.md).

```bash
# 1D live distribution monitor with real-time Gaussian fit, KDE, and error whiskers
python3 -u -c "import random, time; [print(f'{random.gauss(50, 12):.2f}', flush=True) or time.sleep(0.001) for _ in range(100000)]" | \
    histo top --bins=60

# 2D real-time bivariate TrueColor heatmap
python3 -u -c "import random, time; [print(f'{random.gauss(50, 15):.2f} {random.gauss(50, 10) + 0.3 * (random.gauss(50, 15) - 50):.2f}', flush=True) or time.sleep(0.001) for _ in range(50000)]" | \
    histo top --2d --bins=35
```

### TrueColor Terminal Plots
Render distributions directly in your terminal using 1/8th sub-character Unicode blocks and TrueColor gradients.

![libhisto TrueColor Terminal Plot](docs/assets/demo_terminal_1d.svg)

```bash
python3 -c "import random, sys; sys.stdout.write(''.join(f'{random.gauss(50, 15):.4f}\n' for _ in range(100000)))" | \
    histo fill --bins=25 --min=0 --max=100 -o binary | \
    histo plot --color=always --title="Gaussian Monte Carlo (N=100k, μ=50, σ=15)"
```
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

### Inline Single-Line Sparklines

For compact in-terminal dashboards, shell prompts, or streaming logs, render distributions as 1-line sparklines:

```bash
python3 -c "import random, sys; sys.stdout.write(''.join(f'{random.gauss(50, 15):.4f}\n' for _ in range(10000)))" | \
    histo fill --bins=20 --auto-range | histo plot -S
```

```text
    ▂▃▄▆▇██▇▆▄▂▂      [N=9987, range=[0, 1e+02), μ=50, σ=15]
```

---

## Level 0: 30-Second Quickstart

Get up and running with a basic fixed-grid histogram and statistical moment extraction.

### CLI Example
```bash
echo -e "10\n20\n30\n20\n20" | histo fill --min=0 --max=50 --bins=5 | histo plot --style=ascii
```

### C99 Example
```c
#include <stdio.h>
#include <histo/histo.h>

int main(void) {
    // 1. Create a uniform histogram: 10 bins over [0.0, 100.0]
    histo_t *h = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);
    if (!h) return 1;

    // 2. Ingest samples
    histo_fill(h, 25.4);
    histo_fill_w(h, 50.0, 2.5); // Fill coordinate 50.0 with weight 2.5

    // 3. Print basic metrics
    printf("Entries: %lu\n", (unsigned long)histo_num_entries(h));

    // 4. Clean teardown
    histo_destroy(h);
    return 0;
}
```

---

## Level 1: EDA & Summary Statistics

Enable Welford's exact online moments and statistical error tracking (`sum_w2`) to extract summary statistics without discretisation error.

### CLI Example
```bash
python3 -c "import random; print('\n'.join(f'{random.gauss(100, 10):.2f}\t{random.uniform(0.5, 2.0):.2f}' for _ in range(1000)))" | \
    histo fill -w --auto-range | histo stats -f json
```


### C99 Example
```c
#include <stdio.h>
#include <histo/histo.h>

int main(void) {
    // Enable sum_w2 error tracking and exact online Welford statistics
    histo_t *h = histo_create_uniform(100, 0.0, 100.0, 
                                      HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);

    double samples[] = {12.0, 45.0, 88.0, 50.0, 45.0, 48.0};
    for (int i = 0; i < 6; i++) {
        histo_fill(h, samples[i]);
    }

    // Query statistics, robust dispersion, and shape metrics
    double mean = 0.0, std_dev = 0.0, median = 0.0, iqr = 0.0, fwhm = 0.0;
    histo_mean(h, &mean);
    histo_std_dev(h, &std_dev);
    histo_median(h, &median);
    histo_iqr(h, &iqr);
    histo_fwhm(h, &fwhm);
    
    printf("Mean: %.3f | StdDev: %.3f | Median: %.3f | IQR: %.3f | FWHM: %.3f\n",
           mean, std_dev, median, iqr, fwhm);

    histo_destroy(h);
    return 0;
}
```

---

## Level 2: Parametric Curve Fitting

Perform high-performance non-linear regression using the built-in Levenberg-Marquardt optimizer. Supports models like Gaussian, Exponential, Polynomials, Breit-Wigner, and Power Law.

### C99 Example
```c
#include <stdio.h>
#include <histo/histo.h>
#include <histo/fit.h>

int main(void) {
    histo_t *h = histo_create_uniform(50, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    // Ingest sample data centered around 50.0
    for (int i = 0; i < 1000; ++i) {
        double x = 50.0 + ((i % 20) - 10) * 1.5;
        histo_fill(h, x);
    }

    // 1. Configure the fit (optional: you can pass NULL for defaults)
    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.loss_type = HISTO_FIT_LOSS_CHI2;


    // 2. Perform a Gaussian fit (initial parameters guessed automatically)
    histo_fit_result_t *res = NULL;
    histo_status_t status = histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, NULL, &opts, &res);

    if (status == HISTO_OK && res->converged) {
        printf("Fit Converged! Chi2 / NDF: %.3f / %d (p-value: %.4f)\n", 
               res->chi2, res->ndf, res->p_value);
        printf("Amplitude: %.3f +/- %.3f\n", res->params[0], res->param_errors[0]);
        printf("Mean (mu): %.3f +/- %.3f\n", res->params[1], res->param_errors[1]);
        printf("Sigma:     %.3f +/- %.3f\n", res->params[2], res->param_errors[2]);
    }

    histo_fit_result_destroy(res);
    histo_destroy(h);
    return 0;
}
```

---

## Level 3: 2D Histograms

Full support for bivariate distributions, allowing you to compute covariance, correlation, marginal projections, and slices.

### C99 Example
```c
#include <stdio.h>
#include <histo/histo2d.h>

int main(void) {
    // 1. Create a 50x50 2D histogram over [-5, 5] x [-5, 5]
    histo2d_t *h2d = histo2d_create_uniform(50, -5.0, 5.0, 50, -5.0, 5.0,
                                            HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);

    // 2. Ingest bivariate samples
    histo2d_fill(h2d, 1.2, 2.4);
    histo2d_fill_w(h2d, -0.5, 0.5, 3.0); // Weighted entry

    // 3. Compute bivariate statistics
    histo2d_stats_t stats;
    histo2d_get_stats(h2d, &stats);
    printf("2D Entries: %lu | Covariance: %.4f | Pearson Rho: %.4f\n",
           (unsigned long)stats.n_entries, stats.covariance, stats.correlation);

    // 4. Extract 1D marginal projection along X
    histo_t *proj_x = NULL;
    histo2d_project_x(h2d, &proj_x);

    histo_destroy(proj_x);
    histo2d_destroy(h2d);
    return 0;
}
```

---

## Level 4: Streaming Quantile Sketches (DDSketch)

For streaming quantile estimation over unbounded numeric ranges with guaranteed relative error, `libhisto` provides a highly optimized DDSketch implementation.

### C99 Example
```c
#include <stdio.h>
#include <histo/sketch.h>

int main(void) {
    // 1. Create a DDSketch: alpha = 0.01 (+/- 1% error guarantee), max 1024 bins
    histo_sketch_t *sketch = histo_sketch_create(0.01, 1024);
    if (!sketch) return 1;

    // 2. Stream latency values across multiple orders of magnitude (e.g., microseconds to seconds)
    histo_sketch_insert(sketch, 0.042);
    histo_sketch_insert(sketch, 1.250);
    histo_sketch_insert_w(sketch, 45.100, 3.0); // Weighted entry

    // 3. Query arbitrary quantiles with bounded relative error
    double p50 = 0.0, p90 = 0.0, p99 = 0.0;
    histo_sketch_quantile(sketch, 0.50, &p50);
    histo_sketch_quantile(sketch, 0.90, &p90);
    histo_sketch_quantile(sketch, 0.99, &p99);
    printf("Quantiles -> P50: %.3f | P90: %.3f | P99: %.3f\n", p50, p90, p99);

    // 4. Destroy sketch
    histo_sketch_destroy(sketch);
    return 0;
}
```

---

## Level 5: High Performance

Push throughput limits with SIMD vector acceleration (AVX-512 / AVX2). `histo_fill_n` dynamically selects vectorized execution paths for branchless boundary guards and parallel memory increments.

### C99 Example
```c
#include <stdio.h>
#include <stdlib.h>
#include <histo/histo.h>

#define NUM_SAMPLES 1000000

int main(void) {
    histo_t *h = histo_create_uniform(100, 0.0, 100.0, HISTO_FLAG_NONE);
    
    // Allocate contiguous cache-aligned sample array
    double *samples = malloc(NUM_SAMPLES * sizeof(double));
    for (size_t i = 0; i < NUM_SAMPLES; i++) {
        samples[i] = (double)(i % 100); 
    }

    // High-throughput SIMD batch ingestion (~441 Mops/s on modern CPUs)
    // Providing NULL for weights assumes unit weight (1.0) for all entries.
    histo_fill_n(h, NUM_SAMPLES, samples, NULL);

    printf("Ingested %lu samples using vector intrinsics.\n", 
           (unsigned long)histo_num_entries(h));

    free(samples);
    histo_destroy(h);
    return 0;
}
```

---

## Documentation & Guides

Comprehensive technical documentation, algorithmic formulas, and manuals are available in [`docs/`](docs/):

- **[User Manual & Architecture Guide (Levels 0–5)](docs/manual.md)**: Full walkthrough of C99, Python, Perl, SIMD vectorization, and serialization.
- **[Real-Time Interactive Monitor Guide (`histo top`)](docs/top_manual.md)**: Dedicated user guide for 1D/2D live streaming dashboards, analytical curve overlays, and keybindings.
- **[Kernel Density Estimation & Automated Binning Guide](docs/kde_guide.md)**: 1D continuous KDE engine, bandwidth selectors, and optimal bin heuristics.
- **[Curve Fitting & Non-Linear Regression Guide](docs/curve_fitting_guide.md)**: Levenberg-Marquardt optimizer, Gaussian/Lorentzian/Exponential models, and ASCII overlays.
- **[2D Histograms, Projections & Heatmaps Guide](docs/histo2d_guide.md)**: 2D bivariate distributions, marginal projections, slices, profile histograms, and TrueColor heatmaps.
- **[Statistical & Analytical Formulae Reference](docs/statistical_formulae.md)**: Mathematical reference for Welford moments, robust dispersion, and distance metrics.
- **[Serialization Wire Formats (V1, V2, V3, JSON)](docs/serialization_format.md)**: Deterministic binary serialization schemas and portable wire formats.

---

## Dependencies & Building

- **Core Library:** Zero external dependencies (strict ISO C99 standard library only).
- **Build System:** CMake 3.15+ (Required).

### CMake Integration
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

```bash
# Build and test
cmake -B build -S . && cmake --build build
ctest --test-dir build --output-on-failure
```

## License
[MIT License](LICENSE) - Copyright (c) Steffen Mueller
