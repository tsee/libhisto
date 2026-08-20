# libhisto User Manual & Progressive Architecture Guide {#mainpage}

Welcome to **libhisto**! This manual follows an incremental learning architecture (**Levels 0 through 5**). Whether you want a 30-second CLI pipeline, a 10-line C99 integration, or deep control over SIMD vector acceleration and DDSketch streaming math, you will find the relevant information below.

---

## 1. Feature Map & Progressive Learning Path

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │ Level 0: 30-Second Quickstart (1D Ingestion & Terminal Plots)           │
 ├────────────────────────────────────────────────────────────────────────┤
 │ Level 1: Summary Statistics & Moment Analysis (Welford, Quantiles, IQR) │
 ├────────────────────────────────────────────────────────────────────────┤
 │ Level 2: Parametric Curve Fitting & Regression (Gaussian, LM, Chi2/MLE)│
 ├────────────────────────────────────────────────────────────────────────┤
 │ Level 3: 2D Histograms & Heatmaps (Bivariate Welford, Projections)     │
 ├────────────────────────────────────────────────────────────────────────┤
 │ Level 4: Streaming Dynamic Quantile Sketches (DDSketch, Log-Bins)      │
 ├────────────────────────────────────────────────────────────────────────┤
 │ Level 5: High Performance, SIMD Vectorization & System Architecture    │
 └────────────────────────────────────────────────────────────────────────┘
```

---

## Level 0: 30-Second Quickstart

`libhisto` is a fast, zero-dependency ISO C99 library and Unix CLI toolkit for distribution tracking, statistical estimation, curve fitting, and terminal visualization.

### 0.1 Instant CLI Pipeline

Pipe any numeric stream into `histo fill` and visualize it immediately with `histo plot`:

```bash
# Generate 1,000 Gaussian random numbers and plot in terminal
python3 -c "import random; print('\n'.join(str(random.gauss(50, 10)) for _ in range(1000)))" \
  | histo fill --bins 20 --min 10 --max 90 \
  | histo plot
```

**Terminal Output:**
```text
── Histogram (20 bins, range: [10, 90), entries: 998, weight: 998.0) ──
 [10.00, 14.00)     0 │ 
 [14.00, 18.00)     1 │ ▏
 [18.00, 22.00)     4 │ ▌
 [22.00, 26.00)    10 │ █▎
 [26.00, 30.00)    22 │ ██▉
 [30.00, 34.00)    48 │ ██████▍
 [34.00, 38.00)    74 │ █████████▉
 [38.00, 42.00)   115 │ ███████████████▍
 [42.00, 46.00)   162 │ █████████████████████▋
 [46.00, 50.00)   190 │ █████████████████████████▍
 [50.00, 54.00)   158 │ █████████████████████▏
 [54.00, 58.00)   102 │ █████████████▋
 [58.00, 62.00)    61 │ ████████▏
 [62.00, 66.00)    31 │ ████▏
 [66.00, 70.00)    13 │ █▋
 [70.00, 74.00)     5 │ ▋
 [74.00, 78.00)     2 │ ▎
 [78.00, 82.00)     0 │ 
 [82.00, 86.00)     0 │ 
 [86.00, 90.00)     0 │ 
 ─────────────────────┴────────────────────────────────────────────────
```

### 0.2 Minimal C99 Program

```c
#include <stdio.h>
#include <histo/histo.h>

int main(void) {
    // 1. Create a uniform histogram: 10 bins over [0.0, 100.0]
    histo_t *h = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);
    if (!h) return 1;

    // 2. Ingest samples (unit weight and explicit weight)
    histo_fill(h, 25.4);
    histo_fill_w(h, 50.0, 2.5);

    // 3. Inspect summary
    printf("Total entries: %lu | Total weight: %.2f\n",
           (unsigned long)histo_num_entries(h), histo_total_weight(h));

    // 4. Clean teardown (0 memory leaks)
    histo_destroy(h);
    return 0;
}
```

---

## Level 1: Summary Statistics & Moment Analysis

Enable exact online Welford statistics (`HISTO_FLAG_EXACT_MOMENTS`) and per-bin uncertainty tracking (`HISTO_FLAG_TRACK_SUMW2`) to compute higher moments and robust dispersion metrics without discretisation error.

### 1.1 CLI Inspection (`histo stats` & Sparklines)

```bash
# Ingest CSV/TSV data and output comprehensive statistical report
python3 -c "import random; print('\n'.join(f'{random.gauss(100, 10):.2f}\t{random.uniform(0.5, 2.0):.2f}' for _ in range(1000)))" | \
    histo fill -w --auto-range | histo stats

# Compact single-line sparkline for dashboards or log summaries
python3 -c "import random, sys; sys.stdout.write(''.join(f'{random.gauss(50, 15):.4f}\n' for _ in range(10000)))" | \
    histo fill --bins=20 --auto-range | histo plot -S
```


**Sparkline Output:**
```text
    ▂▃▄▆▇██▇▆▄▂▂      [N=10000, range=[12.4, 88.6), μ=50.01, σ=9.98]
```

### 1.2 C99 Statistics Recipe

```c
#include <stdio.h>
#include <histo/histo.h>

int main(void) {
    // Enable sum_w2 error tracking and exact online Welford moments
    histo_t *h = histo_create_uniform(100, 0.0, 100.0,
                                      HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);

    // Ingest data stream
    for (int i = 1; i <= 100; ++i) {
        histo_fill_w(h, (double)i, 1.0);
    }

    // Query standard moments
    double mean = 0.0, std_dev = 0.0, skewness = 0.0, kurtosis = 0.0;
    histo_mean(h, &mean);
    histo_std_dev(h, &std_dev);
    histo_skewness(h, &skewness);
    histo_excess_kurtosis(h, &kurtosis);

    // Query robust non-parametric metrics
    double median = 0.0, iqr = 0.0, mad = 0.0;
    histo_median(h, &median);
    histo_iqr(h, &iqr); // Interquartile range (Q75 - Q25)
    histo_mad(h, &mad); // Median Absolute Deviation: median(|x - median|)

    // Query continuous peak mode & FWHM
    double peak_coord = 0.0, fwhm = 0.0;
    histo_mode_continuous(h, &peak_coord); // 3-point parabolic peak interpolation
    histo_fwhm(h, &fwhm);                 // Full Width at Half Maximum

    printf("Mean: %.2f | StdDev: %.2f | Median: %.2f | IQR: %.2f | FWHM: %.2f\n",
           mean, std_dev, median, iqr, fwhm);

    histo_destroy(h);
    return 0;
}
```

---

## Level 2: Parametric Curve Fitting & Regression

`libhisto` includes a non-linear least squares engine in `histo/fit.h` powered by the Levenberg-Marquardt algorithm and Direct Linear Least Squares for polynomials.

### 2.1 CLI Curve Fitting (`histo fit`)

Fit a Gaussian peak with standard errors, 95% confidence intervals, \f$\chi^2/\mathrm{NDF}\f$, and visual ASCII curve overlay:


```bash
python3 -c "import random; print('\n'.join(str(random.gauss(50.0, 5.0)) for _ in range(10000)))" \
  | histo fill -n 50 --min 20 --max 80 \
  | histo fit -p
```

**Terminal Output:**
```text
  Data Overlay Plot [ █ Data Bins, * Fitted Curve ]:
    962.0 ┤                                 ***                                 
    881.8 ┤                                *███**                               
    801.7 ┤                               *██████*                              
    721.5 ┤                              *████████*                             
    641.3 ┤                              █████████                              
    561.2 ┤                             *██████████*                            
    481.0 ┤                            *████████████*                           
    400.8 ┤                           *██████████████*                          
    320.7 ┤                          *████████████████*                         
    240.5 ┤                         *██████████████████*                        
    160.3 ┤                        *████████████████████*                       
     80.2 ┤                     ***██████████████████████***                    
      0.0 ┤ ********************████████████████████████████********************
          ┼────────────────────────────────────────────────────────────────────
          20                                                                 80

================================================================================
 MODEL: Gaussian Peak [ f(x) = A · exp(-(x - μ)² / (2σ²)) ]
================================================================================
  Param  Name                   Estimate      Std. Error       95% Conf. Interval
  [0]    Amplitude (A)            955.3531     ±   11.6695        [   932.4813,   978.2249 ]
  [1]    Mean (μ)                 49.9559     ±    0.0502        [    49.8575,    50.0543 ]
  [2]    Std Dev (σ)               5.0019     ±    0.0352        [     4.9330,     5.0708 ]
--------------------------------------------------------------------------------
 GOODNESS OF FIT:
  χ² / NDF       = 21.37 / 47 (0.455)
  p-value        = 0.9995 (Consistent with model)
  Log-Likelihood = -126.80  |  AIC = 259.60  |  BIC = 265.33
  Convergence    = Converged (3 iterations, Relative reduction in loss below tolerance (ftol))
================================================================================
```

### 2.2 C99 Curve Fitting Recipe

```c
#include <stdio.h>
#include <histo/histo.h>
#include <histo/fit.h>

int main(void) {
    histo_t *h = histo_create_uniform(50, 20.0, 80.0, HISTO_FLAG_TRACK_SUMW2);
    // (Populate histogram with experimental or simulated data...)

    // 1. Initialize options with defaults
    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.loss_type = HISTO_FIT_LOSS_CHI2; // Or HISTO_FIT_LOSS_POISSON_MLE for sparse counts

    // 2. Perform Gaussian fit (initial parameters estimated automatically)
    histo_fit_result_t *res = NULL;
    histo_status_t st = histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, NULL, &opts, &res);

    if (st == HISTO_OK && res->converged) {
        printf("Fit Converged in %u iterations!\n", res->iterations);
        printf("Amplitude: %.3f +/- %.3f\n", res->params[0], res->param_errors[0]);
        printf("Mean (mu): %.3f +/- %.3f\n", res->params[1], res->param_errors[1]);
        printf("Sigma:     %.3f +/- %.3f\n", res->params[2], res->param_errors[2]);
        printf("Reduced Chi2: %.3f (p-value: %.4f)\n", res->reduced_chi2, res->p_value);
    }

    histo_fit_result_destroy(res);
    histo_destroy(h);
    return 0;
}
```

---

## Level 3: 2-Dimensional Histograms & Spatial Heatmaps

Track bivariate distributions (X, Y) across all 4 binning combinations (Uniform-Uniform, Variable-Variable, Uniform-Variable, Variable-Uniform) with online running covariance \f$\mathrm{Cov}(X, Y)\f$, marginal projections, and terminal heatmaps.


### 3.1 CLI 2D Pipeline with Delimiter Auto-Detection

```bash
# Ingest CSV/TSV bivariate data and render 24-bit TrueColor terminal heatmap
python3 -c "import random; print('\n'.join(f'{random.gauss(-1.5, 1.0)},{random.gauss(-1.5, 1.0)}' if random.random() < 0.55 else f'{random.gauss(2.0, 0.8)},{random.gauss(2.0, 0.8)}' for _ in range(25000)))" \
  | histo fill --2d --xbins 30 --ybins 18 \
  | histo plot
```

### 3.2 C99 2D Histogram Recipe

```c
#include <stdio.h>
#include <histo/histo2d.h>

int main(void) {
    // 1. Create a 50x50 2D histogram over [-5, 5] x [-5, 5]
    histo2d_t *h2d = histo2d_create_uniform(50, -5.0, 5.0, 50, -5.0, 5.0,
                                            HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);

    // 2. Ingest bivariate samples
    histo2d_fill(h2d, 1.2, 2.4);
    histo2d_fill_w(h2d, -0.5, 0.5, 3.0); // Weighted coordinate pair

    // 3. Compute bivariate statistics & Pearson correlation coefficient
    histo2d_stats_t stats;
    histo2d_get_stats(h2d, &stats);
    printf("2D Entries: %lu | Cov(X,Y): %.4f | Pearson Rho: %.4f\n",
           (unsigned long)stats.n_entries, stats.covariance, stats.correlation);

    // 4. Extract 1D marginal projection along X
    histo_t *proj_x = NULL;
    histo2d_project_x(h2d, &proj_x);
    printf("Projected X total weight: %.2f\n", histo_total_weight(proj_x));

    histo_destroy(proj_x);
    histo2d_destroy(h2d);
    return 0;
}
```

---

## Level 4: Streaming Dynamic Quantile Sketches (DDSketch)

For streaming telemetry, response latencies, and distributed workloads spanning unbounded numeric ranges, DDSketch (`include/histo/sketch.h`) guarantees a relative error bound \f$\alpha\f$ (e.g. \f$\pm 1\%\f$) using dynamic logarithmic binning.


### 4.1 C99 DDSketch Recipe

```c
#include <stdio.h>
#include <histo/sketch.h>

int main(void) {
    // 1. Create DDSketch: alpha = 0.01 (+/- 1% relative error), max 1024 bins
    histo_sketch_t *sketch = histo_sketch_create(0.01, 1024);
    if (!sketch) return 1;

    // 2. Ingest unbounded streaming latencies (microseconds to seconds)
    histo_sketch_insert(sketch, 0.045);
    histo_sketch_insert(sketch, 1.250);
    histo_sketch_insert_w(sketch, 45.100, 3.0);

    // 3. Query percentiles
    double p50 = 0.0, p90 = 0.0, p99 = 0.0;
    histo_sketch_quantile(sketch, 0.50, &p50);
    histo_sketch_quantile(sketch, 0.90, &p90);
    histo_sketch_quantile(sketch, 0.99, &p99);

    printf("Latency Percentiles -> P50: %.3f | P90: %.3f | P99: %.3f\n", p50, p90, p99);

    histo_sketch_destroy(sketch);
    return 0;
}
```

---

## Level 5: High Performance, Vectorization & Systems Engineering

### 5.1 SIMD Vector Acceleration Architecture

`histo_fill_n` evaluates batch coordinate arrays using vectorized hardware pipelines:
- **Runtime CPU Detection**: On x86_64 architectures, CPU features dynamically select AVX-512 (8-wide `double` vectors) or AVX2/FMA (4-wide `double` vectors). On ARM64, NEON vector kernels accelerate batch operations.
- **Branchless Masking**: Out-of-bounds (< min, >= max) and non-finite (NaN / Inf) samples are filtered with vector comparison masks.


```c
#define N_SAMPLES 1000000
double values[N_SAMPLES];
// Contiguous SIMD batch ingestion (~441 Mops/s on AVX-512 / AVX2)
histo_fill_n(h, N_SAMPLES, values, NULL);
```

### 5.2 Algorithmic Complexity Reference Table

| Function | Time Complexity | Auxiliary Space | Description |
| :--- | :--- | :--- | :--- |
| `histo_create_uniform` | O(N) | O(N) | Allocates and zeroes N equidistant bins |
| `histo_create_variable` | O(N) | O(N) | Validates monotonic edges and allocates N bins |
| `histo_destroy` | O(1) | O(1) | Deallocates histogram structure and bin arrays |
| `histo_find_bin` (Uniform) | O(1) | O(1) | Boundary-guarded fast reciprocal lookup |
| `histo_find_bin` (Variable) | O(log N) | O(1) | Monotonic bisection binary search |
| `histo_fill` / `_w` (Uniform) | O(1) | O(1) | Constant-time bin and accumulator increment |
| `histo_fill_n` (SIMD) | O(K / V) | O(1) | Vectorized batch ingest of K samples (V=4 or 8) |
| `histo_mean` / `histo_variance` | O(1) | O(1) | Direct query from online Welford accumulators |
| `histo_quantile` / `histo_median` | O(N) | O(1) | Single-pass cumulative inverse CDF scan |
| `histo_fit_model` (LM) | O(I * (N * P + P^3)) | O(N * P + P^2) | Levenberg-Marquardt non-linear regression |
| `histo2d_fill` (Uniform) | O(1) | O(1) | 2D coordinate lookup & Welford comoments |
| `histo2d_project_x/y` | O(Nx * Ny) | O(Nx) or O(Ny) | Integrates across orthogonal axis |
| `histo_sketch_insert` | O(1) | O(1) | DDSketch dynamic logarithmic mapping |


---

## 6. Building & Installation

### CMake Integration (`FetchContent`)
```cmake
include(FetchContent)
FetchContent_Declare(
    libhisto
    GIT_REPOSITORY https://github.com/steffenm/libhisto.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(libhisto)
target_link_libraries(your_target PRIVATE libhisto::libhisto)
```

### Build from Source
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build
```
