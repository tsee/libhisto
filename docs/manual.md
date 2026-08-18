# libhisto User Manual & Architecture Reference {#mainpage}

Welcome to **libhisto**! This guide covers the core concepts, user recipes, advanced statistical algorithms, SIMD acceleration architectures, DDSketch dynamic quantile sketches, and computational complexity characteristics of the library.

## 1. Introduction

`libhisto` is a high-performance, portable, memory-safe 1D histogramming and quantile sketch library implemented in strict ISO C99. Designed for high-throughput scientific analysis, physical simulations, telemetry ingestion, and general statistical computing, `libhisto` provides mathematically exact results, division-free error propagation, SIMD vector acceleration, and microsecond-level serialization.

### Key Architectural Pillars
- **Zero-Allocation Ingestion**: Ingestion routines (`histo_fill`, `histo_fill_w`, `histo_fill_n`, `histo_fill_strided`, `histo_sketch_insert`) perform zero heap allocations in their hot paths, processing >440 Million fills/second in SIMD batch mode.
- **SIMD Vector Acceleration**: Runtime-detected AVX-512 and AVX2/FMA vector kernels accelerate batch coordinate indexing, boundary guarding, and bin accumulation with clean scalar and NEON-ready fallbacks.
- **DDSketch Dynamic Quantile Sketches**: Fully dynamic, bounded relative-error online quantile sketches (`include/histo/sketch.h`) with logarithmic dynamic binning, circular collapsing buffers, and mergeability across distributed streams.
- **Strict ISO C99 & Memory Safety**: Defensive pointer checks on all public APIs, clear destructor ownership semantics (`histo_destroy`, `histo_sketch_destroy`, `histo_free_buffer`), and 100% leak-free ASan/UBSan/Valgrind verification.
- **Deterministic Endian-Safe Wire Format**: Canonical 256-byte header with Little-Endian IEEE-754 data payloads, lossless JSON serialization, and automatic version migration (`histo_migrate_binary`).
- **Comprehensive Statistical Analysis**: Online weighted Welford accumulation, two-pass higher-order central moments, sub-bin parabolic peak mode estimation, robust non-parametric metrics (IQR, MAD, trimmed/winsorized mean), and two-distribution comparison metrics (\f$\chi^2\f$, KS, Wasserstein-1D, KL divergence, Bhattacharyya).
- **Unix CLI & Terminal Visualization**: Rich command-line toolkit (`histo-fill`, `histo-plot`, `histo-stats`, `histo-cmp`) supporting ANSI TrueColor gradients, 1/8th Unicode fractional blocks, compact single-line sparklines (`-S, --sparkline`), and live streaming `--watch` mode.

---

## 2. Core Concepts: Binning Models Visualized

`libhisto` provides three fundamental models for distribution tracking:

### 2.1 Uniform Fixed-Width Binning (`HISTO_BIN_UNIFORM`)
Uniform bins divide the domain \f$[x_{\min}, x_{\max})\f$ into \f$N\f$ equidistant intervals of width \f$\Delta = (x_{\max} - x_{\min}) / N\f$. Lookups execute in \f$O(1)\f$ time via boundary-guarded reciprocal multiplication.

```text
       [ Bin 0 )   [ Bin 1 )   [ Bin 2 )   [ Bin 3 )
     |-----------|-----------|-----------|-----------|
   0.0          2.5         5.0         7.5        10.0
```

### 2.2 Variable-Width Binning (`HISTO_BIN_VARIABLE`)
Variable binning allows arbitrary, strictly monotonic boundary edges \f$e_0 < e_1 < \dots < e_N\f$. Lookups execute in \f$O(\log N)\f$ time using an optimized bisection binary search.

```text
       [ B0 ) [B1)       [   Bin 2   )  [  Bin 3  )
     |------|----|-------------------|------------|
   0.0     3.0  5.0                15.0         20.0
```

### 2.3 Dynamic Logarithmic Binning (DDSketch)
For streaming datasets spanning multiple orders of magnitude with no predefined lower or upper bounds, DDSketch maps values to exponentially growing logarithmic bins \f$k = \lceil \log_\gamma |x| \rceil\f$ where \f$\gamma = \frac{1+\alpha}{1-\alpha}\f$. Bins grow dynamically and collapse into circular stores when capacity limits are reached.

```text
      [ Bin -1 )  [  Bin 0  )    [    Bin 1    )      [      Bin 2      )
    |------------|-----------|------------------|-----------------------|
  1.000        1.020       1.041              1.062                   1.084
```

---

## 3. Quickstart Walkthrough

### 3.1 Uniform Histogramming, Moments, and Binary Roundtrip

```c
#include <stdio.h>
#include <histo/histo.h>

int main(void) {
    // 1. Create a uniform histogram: 100 bins over [0.0, 100.0]
    // Enable sum_w2 error tracking and exact online Welford statistics
    histo_t *h = histo_create_uniform(100, 0.0, 100.0, 
                                      HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    if (!h) {
        fprintf(stderr, "Failed to create histogram\n");
        return 1;
    }

    // 2. Ingest individual samples (unit weight and weighted)
    histo_fill(h, 25.4);
    histo_fill_w(h, 50.0, 2.5); // Fill coordinate 50.0 with weight 2.5

    // 3. Batch SIMD ingestion from contiguous array
    double samples[4] = {12.0, 45.0, 88.0, 50.0};
    histo_fill_n(h, 4, samples, NULL); // NULL weights implies unit weights

    // 4. Query statistics, robust dispersion, and peak shape
    double mean = 0.0, std_dev = 0.0, median = 0.0, iqr = 0.0, fwhm = 0.0;
    histo_mean(h, &mean);
    histo_std_dev(h, &std_dev);
    histo_median(h, &median);
    histo_iqr(h, &iqr);
    histo_fwhm(h, &fwhm);

    printf("Mean: %.3f | StdDev: %.3f | Median: %.3f | IQR: %.3f | FWHM: %.3f\n", 
           mean, std_dev, median, iqr, fwhm);

    // 5. Binary serialization roundtrip (Canonical Little-Endian)
    void *buffer = NULL;
    size_t size = 0;
    if (histo_serialize_binary(h, &buffer, &size) == HISTO_OK) {
        histo_t *h_loaded = NULL;
        if (histo_deserialize_binary(buffer, size, &h_loaded) == HISTO_OK) {
            printf("Reloaded histogram total weight: %.2f\n", histo_total_weight(h_loaded));
            histo_destroy(h_loaded);
        }
        histo_free_buffer(buffer);
    }
    
    // 6. Clean teardown (0 memory leaks)
    histo_destroy(h);
    return 0;
}
```

### 3.2 Streaming Dynamic Quantile Sketching (DDSketch)

```c
#include <stdio.h>
#include <histo/sketch.h>

int main(void) {
    // Create sketch: relative error alpha = 0.01 (+/- 1%), max 1024 bins
    histo_sketch_t *sketch = histo_sketch_create(0.01, 1024);
    if (!sketch) return 1;

    // Ingest unbounded streaming latencies (microseconds to seconds)
    histo_sketch_insert(sketch, 0.045);
    histo_sketch_insert(sketch, 1.250);
    histo_sketch_insert_w(sketch, 45.100, 3.0);

    // Query tail percentiles
    double p50 = 0.0, p90 = 0.0, p99 = 0.0;
    histo_sketch_quantile(sketch, 0.50, &p50);
    histo_sketch_quantile(sketch, 0.90, &p90);
    histo_sketch_quantile(sketch, 0.99, &p99);

    printf("Quantiles -> P50: %.3f | P90: %.3f | P99: %.3f\n", p50, p90, p99);

    histo_sketch_destroy(sketch);
    return 0;
}
```

---

## 4. Practical Recipes

### 4.1 Weighted Monte Carlo Events & Uncertainties
When processing Monte Carlo simulations, events typically have non-unit statistical weights. Enabling `HISTO_FLAG_TRACK_SUMW2` ensures that statistical errors are tracked per bin:

```c
histo_t *mc_hist = histo_create_uniform(50, -5.0, 5.0, HISTO_FLAG_TRACK_SUMW2);

for (size_t i = 0; i < num_events; ++i) {
    double value = get_event_value(i);
    double weight = get_event_weight(i);
    histo_fill_w(mc_hist, value, weight);
}

// Query bin content and error
double content = 0.0, error = 0.0;
histo_bin_content(mc_hist, 10, &content);
histo_bin_error(mc_hist, 10, &error);
printf("Bin 10: %.3f +/- %.3f\n", content, error);
```

### 4.2 High-Throughput Batch SIMD Ingestion
For maximum performance with vectorized telemetry feeds or simulation outputs, ingest contiguous or strided memory buffers:

```c
double values[1000];
double weights[1000];
// Populate values and weights ...

// Contiguous SIMD batch ingestion
histo_fill_n(h, 1000, values, weights);

// Strided ingestion for Array-of-Structs (AoS) data layouts
struct Particle {
    double x, y, z;
    double energy;
    double weight;
};
struct Particle particles[500];

histo_fill_strided(h, 500,
                   &particles[0].energy, sizeof(struct Particle),
                   &particles[0].weight, sizeof(struct Particle));
```

### 4.3 Dynamic Quantile Sketches (DDSketch) & Merging
DDSketch handles online quantile estimation across distributed workers and can merge sketches seamlessly:

```c
histo_sketch_t *worker1 = histo_sketch_create(0.01, 1024);
histo_sketch_t *worker2 = histo_sketch_create(0.01, 1024);

// Worker 1 processes partition A
for (size_t i = 0; i < n_a; ++i) histo_sketch_insert(worker1, samples_a[i]);

// Worker 2 processes partition B
for (size_t i = 0; i < n_b; ++i) histo_sketch_insert(worker2, samples_b[i]);

// Merge worker2 into worker1
histo_sketch_merge(worker1, worker2);

double global_p95 = 0.0;
histo_sketch_quantile(worker1, 0.95, &global_p95);
printf("Merged Global P95: %.4f\n", global_p95);

histo_sketch_destroy(worker1);
histo_sketch_destroy(worker2);
```

### 4.4 Binary Save, Load, JSON Serialization, and Version Migration
Save histograms to files with portable Canonical Little-Endian encoding or human-readable JSON. Old binary formats (e.g. v1) are automatically migrated:

```c
// Binary serialization
void *buf = NULL;
size_t size = 0;
if (histo_serialize_binary(h, &buf, &size) == HISTO_OK) {
    FILE *f = fopen("histogram.dat", "wb");
    if (f) {
        fwrite(buf, 1, size, f);
        fclose(f);
    }
    histo_free_buffer(buf);
}

// Lossless JSON serialization
char *json_str = NULL;
if (histo_serialize_json(h, &json_str) == HISTO_OK) {
    FILE *f = fopen("histogram.json", "w");
    if (f) {
        fputs(json_str, f);
        fclose(f);
    }
    histo_free_buffer(json_str);
}
```

### 4.5 Higher-Order Moments & Peak/Shape Analysis
Extract distribution asymmetry (skewness), tail heaviness (kurtosis), dominant peak width (FWHM), continuous peak coordinate, and Root Mean Square (RMS):

```c
double skewness = 0.0, excess_kurtosis = 0.0;
histo_skewness(h, &skewness);
histo_excess_kurtosis(h, &excess_kurtosis);

double peak_mode = 0.0, fwhm = 0.0, rms = 0.0;
histo_mode_continuous(h, &peak_mode); // 3-point parabolic peak interpolation
histo_fwhm(h, &fwhm);                 // Full Width at Half Maximum
histo_rms(h, &rms);                   // Root Mean Square

printf("Peak @ %.3f | FWHM: %.3f | Skewness: %.3f | Exc Kurtosis: %.3f | RMS: %.3f\n",
       peak_mode, fwhm, skewness, excess_kurtosis, rms);
```

### 4.6 Robust Non-Parametric & Dispersion Statistics
When dealing with heavy-tailed or outlier-prone distributions, robust location and dispersion metrics provide resistant estimators:

```c
double iqr = 0.0, mad = 0.0;
histo_iqr(h, &iqr); // Interquartile Range (Q75 - Q25)
histo_mad(h, &mad); // Median Absolute Deviation: median(|x - median|)

double trimmed = 0.0, winsorized = 0.0;
// 10% trimmed and Winsorized mean (between 10th and 90th percentiles)
histo_trimmed_mean(h, 0.10, 0.90, &trimmed);
histo_winsorized_mean(h, 0.10, 0.90, &winsorized);

printf("IQR: %.3f | MAD: %.3f | 10%% Trimmed Mean: %.3f | Winsorized Mean: %.3f\n",
       iqr, mad, trimmed, winsorized);
```

### 4.7 Comparing Distributions & Statistical Distances
Compare two histograms with matching binning geometries using hypothesis tests and distance metrics:

```c
// 1. Chi-Square goodness-of-fit / compatibility
double chi2 = 0.0;
uint32_t ndf = 0;
histo_cmp_chi2(h1, h2, &chi2, &ndf);

// 2. Kolmogorov-Smirnov supremum CDF distance
double ks_stat = 0.0;
histo_cmp_ks(h1, h2, &ks_stat);

// 3. 1D Wasserstein / Earth Mover's Distance (L1 distance between CDFs)
double emd = 0.0;
histo_cmp_wasserstein_1d(h1, h2, &emd);

// 4. Kullback-Leibler divergence (D_KL(h1 || h2)) in nats
double kl_div = 0.0;
histo_cmp_kl_divergence(h1, h2, &kl_div);

// 5. Bhattacharyya distance (-ln(BC))
double bhatt_dist = 0.0;
histo_cmp_bhattacharyya(h1, h2, &bhatt_dist);

printf("Chi2/NDF: %.2f/%u | KS: %.4f | EMD: %.4f | KL: %.4f | Bhatt: %.4f\n",
       chi2, ndf, ks_stat, emd, kl_div, bhatt_dist);
```

### 4.8 Unix CLI Toolkit

`libhisto` ships with a high-performance, modular Unix CLI toolkit built on the standard Unix filter philosophy (stdin \f$\to\f$ stdout). The tools can be invoked via the multi-call binary `histo <command>` or direct standalone symlinks (`histo-fill`, `histo-plot`, `histo-stats`, `histo-cmp`).

```text
               ┌────────────┐
Data Stream ──>│ histo-fill │──> Binary Stream / JSON / TSV
               └────────────┘        │
             ┌───────────────────────┼───────────────────────┐
             ▼                       ▼                       ▼
      ┌────────────┐          ┌─────────────┐         ┌───────────┐
      │ histo-plot │          │ histo-stats │         │ histo-cmp │
      └────────────┘          └─────────────┘         └───────────┘
   (Terminal Visualizer)      (Moment Report)       (A/B Comparison)
```

#### 4.8.1 Streaming Ingestion (`histo-fill`)
Ingests whitespace-delimited numbers, CSV columns, or raw IEEE-754 Little-Endian binary `double` streams (`--binary-f64`):

```bash
# Ingest 100k numbers with automatic min/max range detection
seq 1 100000 | awk '{print rand() * 100}' | histo-fill --bins=25 --auto-range -o binary > hist.bin

# Ingest CSV file extracting coordinate from column 2 and weight from column 4
cat physics_events.csv | histo-fill --delimiter=',' --value-col=2 --weights-col=4 --bins=50 --min=0 --max=200 -o json > hist.json

# Live streaming snapshot mode (emits binary histogram snapshot every 250ms)
tail -f access.log | grep -o 'time=[0-9.]*' | cut -d= -f2 | \
    histo-fill --bins=30 --auto-range --emit-interval=0.25 | histo-plot --watch
```

#### 4.8.2 Terminal Visualization & Sparklines (`histo-plot`)
Renders histograms directly in terminal emulators with automatic width detection, 1/8th sub-character fractional Unicode blocks (` ▂▃▄▅▆▇█`), ANSI TrueColor density gradients, error whiskers (\f$\pm \sigma\f$), sparklines, and continuous watch mode (`--watch`):

```bash
# Gaussian Monte Carlo piped through binary wire format into TrueColor plot
python3 -c "import random, sys; sys.stdout.write(''.join(f'{random.gauss(50, 15):.4f}\n' for _ in range(100000)))" | \
    histo-fill --bins=25 --min=0 --max=100 -o binary | \
    histo-plot --color=always --title="Gaussian Monte Carlo (N=100k, μ=50, σ=15)"
```

**Inline Single-Line Sparklines (`-S, --sparkline`):**
```bash
python3 -c "import random, sys; sys.stdout.write(''.join(f'{random.gauss(50, 15):.4f}\n' for _ in range(10000)))" | \
    histo-fill --bins=20 --min=0 --max=100 -o binary | \
    histo-plot -S
```
```text
    ▂▃▄▆▇██▇▆▄▂▂      [N=9987, range=[0, 1e+02), μ=50, σ=15]
```

**ASCII Sparkline (`--style=ascii --no-stats`):**
```bash
cat data.bin | histo-plot --sparkline --style=ascii --no-stats
```
```text
  ..::||##||::..  
```

---

## 5. Numerical Algorithms & Statistical Formulations

### 5.1 Boundary-Guarded Uniform Bin Lookup (\f$O(1)\f$)
For uniform binning over \f$[x_{\min}, x_{\max})\f$, naive floating-point division `(x - min) / width` is susceptible to floating-point truncation errors at exact bin boundaries. `libhisto` computes the candidate provisional index via reciprocal multiplication:
\f[
i_{\text{prov}} = \lfloor (x - x_{\min}) \cdot \Delta^{-1} \rfloor
\f]
Followed by dual neighbor verification against machine-precision boundary limits:
- **Upper Guard**: If \f$ x \ge x_{\min} + (i_{\text{prov}} + 1) \Delta \f$, then \f$ i = i_{\text{prov}} + 1 \f$.
- **Lower Guard**: If \f$ x < x_{\min} + i_{\text{prov}} \Delta \f$, then \f$ i = i_{\text{prov}} - 1 \f$.
If \f$ \Delta^{-1} \f$ overflows to infinity on subnormal ranges, lookup falls back cleanly to robust division.

### 5.2 Variable-Width Bisection Binary Search (\f$O(\log N)\f$)
For variable binning defined by monotonic edges \f$ e_0 < e_1 < \dots < e_N \f$, bin lookup uses bisection search on \f$ [0, N) \f$:
- Invariant: \f$ x \in [e_i, e_{i+1}) \f$ with exact matching at \f$ e_0 = x_{\min} \f$ and \f$ e_N = x_{\max} \f$.
- Guaranteed \f$ O(\log N) \f$ time and \f$ O(1) \f$ auxiliary space.

### 5.3 Online Weighted Welford Statistics (\f$O(1)\f$)
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
Variance is extracted as \f$ \sigma^2 = M_2 / W \f$. The algorithm natively supports negative event weights and avoids variance underflow via \f$ M_2 = \max(0.0, M_2) \f$ clamping.

### 5.4 Higher-Order Central Moments & Shape Formulations (\f$O(N)\f$)
For orders \f$ k \ge 2 \f$, central moments are computed using a two-pass algorithm over active bin centers:
\f[
M_k = \frac{1}{W_{\text{total}}} \sum_{i=0}^{N-1} w_i (x_{c, i} - \mu)^k
\f]
- **Skewness** (\f$ \gamma_1 \f$): \f$ \gamma_1 = \frac{M_3}{M_2^{3/2}} = \frac{M_3}{\sigma^3} \f$
- **Kurtosis** (\f$ \beta_2 \f$): \f$ \beta_2 = \frac{M_4}{M_2^2} = \frac{M_4}{\sigma^4} \f$
- **Excess Kurtosis** (\f$ \gamma_2 \f$): \f$ \gamma_2 = \beta_2 - 3.0 \f$ (equals 0.0 for normal distributions).

### 5.5 Continuous Parabolic Peak Mode & FWHM (\f$O(N)\f$)
- **Continuous Mode**: Identifies mode bin \f$ m \f$ having maximum weight \f$ w_m \f$. For uniform histograms, fits a parabola through \f$ (x_{m-1}, w_{m-1}), (x_m, w_m), (x_{m+1}, w_{m+1}) \f$:
\f[
\delta = \frac{1}{2} \frac{w_{m+1} - w_{m-1}}{2 w_m - w_{m-1} - w_{m+1}}, \quad x_{\text{mode}} = x_{c, m} + \delta \cdot \Delta
\f]
- **FWHM**: Scans outward from the mode bin to find the left and right crossings where \f$ w(x) = \frac{1}{2} w_{\text{max}} \f$, linearly interpolating between bin centers.

### 5.6 Non-Empty Support Linear Quantile & Dispersion (\f$O(N)\f$)
Quantile coordinates \f$ Q(p) = F^{-1}(p) \f$ for \f$ p \in [0.0, 1.0] \f$ are evaluated using continuous inverse CDF interpolation:
- Targets cumulative mass \f$ T = p \cdot W_{\text{total}} \f$.
- Locates non-empty bin \f$ i \f$ where \f$ \sum_{j=0}^{i-1} w_j < T <= \sum_{j=0}^i w_j \f$.
- Computes intra-bin linear coordinate:
\f[
Q(p) = \text{low}_i + \frac{T - \sum_{j=0}^{i-1} w_j}{w_i} (\text{high}_i - \text{low}_i)
\f]
- **MAD**: Evaluates median deviation \f$ d_i = |x_{c, i} - \text{median}| \f$, sorts weighted pairs in \f$ O(N \log N) \f$, and finds the 50th percentile of absolute deviations.

### 5.7 Division-Free Error Propagation (\f$O(N)\f$)
When combining histograms via multiplication (\f$ H = A \cdot B \f$) or division (\f$ H = A / B \f$), statistical uncertainties are propagated without intermediate divisions:
- **Product Uncertainty**:
\f[
\text{sum\_w2}_{A \cdot B, i} = B_i^2 \, \text{sum\_w2}_{A, i} + A_i^2 \, \text{sum\_w2}_{B, i}
\f]
- **Quotient Uncertainty**:
\f[
\text{sum\_w2}_{A / B, i} = \frac{\text{sum\_w2}_{A, i} \, B_i^2 + \text{sum\_w2}_{B, i} \, A_i^2}{B_i^4}
\f]

### 5.8 Two-Distribution Comparison Metrics (\f$O(N)\f$)
- **Weighted Chi-Square**: \f$ \chi^2 = \sum_{i} \frac{(w_{1,i} - w_{2,i})^2}{\sigma_{1,i}^2 + \sigma_{2,i}^2} \f$ with \f$ \text{NDF} \f$ matching the number of non-zero variance bins.
- **Kolmogorov-Smirnov**: \f$ D = \max_i |F_1(x_i) - F_2(x_i)| \f$ over normalized empirical CDFs.
- **1D Wasserstein (EMD)**: \f$ W_1 = \int |F_1(x) - F_2(x)| \, dx = \sum_i |F_1(x_i) - F_2(x_i)| \Delta x_i \f$.
- **KL Divergence**: \f$ D_{\text{KL}}(P \parallel Q) = \sum_{i} P_i \ln \frac{P_i}{Q_i} \f$ (with \f$ \epsilon = 10^{-12} \f$ floor for zero target bins).
- **Bhattacharyya Distance**: \f$ D_B(P, Q) = -\ln \sum_i \sqrt{P_i Q_i} \f$.

### 5.9 DDSketch Bounded Relative-Error Quantile Sketch (\f$O(1)\f$ insertion)
The `histo_sketch_t` provides a fully dynamic quantile sketch based on Masson et al. (VLDB 2019). It guarantees a relative error bound \f$\alpha\f$ for any quantile query:
- **Logarithmic Mapping**: \f$ k = \lceil \log_{\gamma}(|x|) \rceil = \lceil \frac{\ln |x|}{\ln \gamma} \rceil \f$ where \f$\gamma = \frac{1 + \alpha}{1 - \alpha}\f$.
- **Quantile Reconstruction**: For bin index \f$k\f$, the representative value is \f$\hat{q} = \frac{2 \gamma^k}{1 + \gamma}\f$, bounding the relative error by \f$|q - \hat{q}| <= \alpha \cdot q\f$.
- **Collapsing Circular Buffer**: Positive and negative values are tracked in independent circular stores. When \f$k_{\max} - k_{\min} \ge \text{max\_bins}\f$, lower bins are collapsed into a single boundary bin to enforce strict memory bounds \f$O(\text{max\_bins})\f$.
- **Mergeability**: Distributed sketches created with the same \f$\alpha\f$ can be merged exactly in \f$O(\text{max\_bins})\f$ time.

### 5.10 SIMD Vector Acceleration Architecture (\f$O(N)\f$)
`histo_fill_n` evaluates batch coordinate arrays using vectorized hardware pipelines:
- **Runtime CPUID Dispatch**: On x86_64 architectures, CPUID dynamically selects AVX-512 (8-wide double vectors) or AVX2/FMA (4-wide double vectors).
- **Vectorized Indexing**: Coordinates are loaded via unaligned SIMD loads (`_mm256_loadu_pd` / `_mm512_loadu_pd`), subtracted from \f$x_{\min}\f$, and multiplied by \f$\text{inv\_binsize}\f$ using FMA.
- **Branchless Masking**: Out-of-range (< min, >= max) and NaN/Inf coordinates are detected via SIMD comparison masks (`_mm256_cmp_pd`, `_mm512_cmp_pd_mask`). Valid blocks execute in direct vectorized pipelines; edge cases fall back to scalar handlers.
- **Cross-Platform Fallback**: On ARM and non-x86 architectures, the pipeline compiles clean, portable scalar/auto-vectorized loops ready for NEON optimization.

---

## 6. Performance Envelope & Mechanical Sympathy

Benchmark measurements conducted on **Intel(R) Core(TM) Ultra 7 255HX (5.3 GHz max, 20 cores, Linux x86_64)**:

| Operation / Benchmark Target | Throughput | Latency / Sample or Query | Complexity |
| :--- | :--- | :--- | :--- |
| **Batch Ingestion (`histo_fill_n` SIMD)** | **441.15 Mops/s** | **2.27 ns / sample** | O(N) vectorized |
| **Single Uniform Fill (`histo_fill`)** | **288.26 Mops/s** | **3.47 ns / sample** | O(1) |
| **Weighted Fill + `sum_w2` (`histo_fill_w`)** | **223.45 Mops/s** | **4.48 ns / sample** | O(1) |
| **Variable Bin Binary Search (100 bins)** | **33.65 Mops/s** | **29.72 ns / sample** | O(log N) |
| **DDSketch Streaming Ingestion** | **143.58 Mops/s** | **6.97 ns / sample** | O(1) amortized |
| **DDSketch Quantile Queries (P50/P90/P99)** | **231.17 k-queries/s** | **4.33 µs / query** | O(B) |
| **Two-Sample Comparison (Chi2/KS/EMD/Bhatt)** | **27.62 k-comparisons/s** | **36.20 µs / comparison** | O(N) |
| **Two-Pass Moments (Mean + Variance, 10k bins)** | **126.8 k-queries/s** | **7.89 µs / call** | O(N) |
| **Quantile & Median (Q25 + Q50 + Q75, 10k bins)** | **73.5 k-queries/s** | **13.6 µs / call** | O(N) |
| **Binary Serialization (10k bins)** | **38.4 k-ser/s** | **26.0 µs / save** | O(N) |

### Mechanical Sympathy Principles

1. **Zero-Allocation Hot Path**: Hot ingestion routines make zero `malloc` or system calls, avoiding heap lock contention and OS memory subsystem latency.
2. **Instruction Pipeline Optimization**: Uniform coordinate indexing evaluates reciprocal multiplication (`FMUL`, latency 3–4 cycles) rather than floating-point hardware division (`FDIV`, latency 14–20 cycles).
3. **Data Cache Locality**: Bin arrays and statistical moments are laid out contiguously with strict 8-byte alignment, ensuring maximum cache-line utilization in CPU L1d (32KB–48KB) and L2 caches.
4. **Vector Execution & FMA**: Ingestion batches utilize 256-bit and 512-bit vector registers to process 4 to 8 IEEE-754 double-precision floats concurrently per core.
5. **Division-Free Error Propagation**: Arithmetic routines evaluate algebraic formulations that propagate variances without intermediate division operations, preventing floating-point exception traps and zero-divisor stalls.

---

## 7. Algorithmic Complexity Reference Table

| Function | Time Complexity | Space Complexity | Description |
| :--- | :--- | :--- | :--- |
| `histo_create_uniform` | O(N) | O(N) | Allocates and zeroes N equidistant bins |
| `histo_create_variable` | O(N) | O(N) | Validates monotonic edges and allocates N bins |
| `histo_destroy` | O(1) | O(1) | Deallocates histogram structure and bin arrays |
| `histo_clone` | O(N) | O(N) | Deep copies geometry, bin contents, and moments |
| `histo_reset` | O(N) | O(1) | Zeroes all bins and resets statistical accumulators |
| `histo_find_bin` (Uniform) | O(1) | O(1) | Boundary-guarded fast reciprocal lookup |
| `histo_find_bin` (Variable) | O(log N) | O(1) | Monotonic bisection binary search |
| `histo_fill` / `histo_fill_w` (Uniform) | O(1) | O(1) | Constant-time bin and accumulator increment |
| `histo_fill` / `histo_fill_w` (Variable) | O(log N) | O(1) | Binary search + bin increment |
| `histo_fill_n` (SIMD) | O(K / V) | O(1) | Vectorized batch ingest of K samples (V = 4 or 8) |
| `histo_fill_strided` | O(K * L) | O(1) | Strided batch ingest for AoS datasets |
| `histo_fill_bin` | O(1) | O(1) | Direct indexed bin accumulation |
| `histo_nbins` / `histo_bin_type` / `histo_range` | O(1) | O(1) | Header field query |
| `histo_bin_bounds` / `histo_bin_center` | O(1) | O(1) | Boundary coordinate / midpoint calculation |
| `histo_bin_content` / `histo_bin_error` / `histo_bin_sum_w2` | O(1) | O(1) | Bin content and statistical uncertainty query |
| `histo_total_weight` / `histo_num_entries` / counters | O(1) | O(1) | Summary accumulator accessors |
| `histo_mean` / `histo_variance` (Exact Moments) | O(1) | O(1) | Direct query from online Welford accumulators |
| `histo_mean` / `histo_variance` (Two-Pass) | O(N) | O(1) | Two-pass bin-center moments calculation |
| `histo_std_dev` | O(1) or O(N) | O(1) | Square root of variance |
| `histo_central_moment` | O(N) | O(1) | Two-pass calculation of k-th central moment |
| `histo_skewness` / `histo_kurtosis` / `histo_excess_kurtosis` | O(N) | O(1) | Higher-order standardized moments |
| `histo_mode_bin` | O(N) | O(1) | Linear scan for maximum weight bin |
| `histo_mode_continuous` | O(N) | O(1) | 3-point parabolic peak vertex interpolation |
| `histo_fwhm` | O(N) | O(1) | Full Width at Half Maximum linear edge crossings |
| `histo_rms` | O(N) or O(1) | O(1) | Root Mean Square: sqrt(variance + mean^2) |
| `histo_quantile` / `histo_median` | O(N) | O(1) | Single-pass cumulative CDF scan |
| `histo_iqr` | O(N) | O(1) | Interquartile Range: Q75 - Q25 |
| `histo_mad` | O(N log N) | O(N) | Median Absolute Deviation from median |
| `histo_trimmed_mean` | O(N) | O(1) | Truncated mean across percentile interval |
| `histo_winsorized_mean` | O(N) | O(1) | Outlier-clamped Winsorized mean |
| `histo_integral` | O(M) | O(1) | Sum over M <= N bins |
| `histo_get_stats` | O(N) | O(1) | Aggregates moments, range limits, and median |
| `histo_cmp_chi2` | O(N) | O(1) | Weighted Chi-Square test statistic & NDF |
| `histo_cmp_ks` | O(N) | O(1) | Kolmogorov-Smirnov maximum vertical CDF metric |
| `histo_cmp_wasserstein_1d` | O(N) | O(1) | 1D Earth Mover's Distance / L1 CDF integral |
| `histo_cmp_kl_divergence` | O(N) | O(1) | Kullback-Leibler relative entropy in nats |
| `histo_cmp_bhattacharyya` | O(N) | O(1) | Bhattacharyya distance between distributions |
| `histo_add` / `histo_subtract` | O(N) | O(1) | Element-wise vector arithmetic and moment update |
| `histo_multiply` / `histo_divide` | O(N) | O(1) | Element-wise arithmetic + sum_w2 error propagation |
| `histo_scale` | O(N) | O(1) | Linear scaling of all bins and moments |
| `histo_normalize` | O(N) | O(1) | Scales total weight to target area |
| `histo_rebin` | O(N) | O(N / k) | Allocates and merges adjacent uniform bins |
| `histo_slice` | O(M) | O(M) | Allocates sub-range histogram of M bins |
| `histo_cdf` | O(N) | O(N) | Allocates prefix-sum CDF histogram |
| `histo_serialize_binary_size` | O(1) | O(1) | Header (256B) + payload size calculation |
| `histo_serialize_binary_into` | O(N) | O(1) | Encodes Little-Endian wire format into caller buffer |
| `histo_serialize_binary` | O(N) | O(N) | Allocates buffer and encodes wire format |
| `histo_deserialize_binary` | O(N) | O(N) | Decodes wire format into newly allocated histogram |
| `histo_serialize_json` | O(N) | O(N) | Serializes histogram to IEEE-754 lossless JSON string |
| `histo_deserialize_json` | O(N) | O(N) | Deserializes histogram from JSON string |
| `histo_migrate_binary` | O(N) | O(N) | Migrates older binary format buffers to current version |
| `histo_free_buffer` | O(1) | O(1) | Deallocates library-allocated serialization buffer |
| `histo_sketch_create` / `destroy` | O(B) | O(B) | Sketch allocation and circular buffer initialization |
| `histo_sketch_insert` / `_w` | O(1) | O(1) | Amortized constant time logarithmic mapping & insertion |
| `histo_sketch_insert_n` | O(K) | O(1) | Batch insertion of K elements into sketch |
| `histo_sketch_quantile` | O(B) | O(1) | Single-pass cumulative weight scan over B bins |
| `histo_sketch_merge` | O(B) | O(1) | Merges two sketches with identical alpha |
| `histo_sketch_reset` | O(B) | O(1) | Resets circular stores and summary counters |
| `histo_sketch_serialize_binary` | O(B) | O(B) | Encodes sketch to binary buffer |
| `histo_sketch_deserialize_binary` | O(B) | O(B) | Decodes sketch from binary buffer |

---

## 9. Curve Fitting & Non-Linear Regression Engine

`libhisto` includes a dedicated curve fitting engine in `histo/fit.h` for parameter estimation across 1D histograms.

### Key Capabilities
- **Built-in Parametric Models**: Gaussian peaks, Exponential decay with baseline, Polynomials ($d <= 10$), Breit-Wigner / Cauchy-Lorentz resonance peaks, and Power Law distributions.
- **Custom User Models**: Arbitrary function callbacks `histo_fit_fn` with user context pointers and optional analytical gradient callbacks `histo_fit_grad_fn` (or automatic finite-difference fallback).
- **Levenberg-Marquardt & Linear Least Squares**: Direct Cholesky solution for polynomials, and adaptive damping LM optimizer for non-linear models.
- **Poisson MLE**: Cash deviance ($-2\ln\lambda$) for sparse and low-count histograms.
- **Box Constraints & Freezing**: Parameter lower/upper bounds and fixed parameter masks.
- **Diagnostics**: Parameter errors ($\sqrt{\text{Cov}_{ii}}$), covariance matrix, correlation matrix, reduced $\chi^2$, $p$-values, AIC, and BIC.

For complete mathematical derivations and user recipes, see [`docs/curve_fitting_guide.md`](curve_fitting_guide.md).

### Computational Complexity: Curve Fitting

| Function | Time Complexity | Auxiliary Space | Notes |
| :--- | :--- | :--- | :--- |
| `histo_fit_options_init` | O(1) | O(1) | Initializes default optimizer options |
| `histo_fit_estimate_initial_params` | O(N) | O(1) | Statistical moment-based heuristic estimation |
| `histo_fit_model` (Linear LS) | O(N * P + P^3) | O(P^2) | Direct single-pass Cholesky polynomial solver |
| `histo_fit_model` / `histo_fit_custom` (LM) | O(I * (N * P + P^3)) | O(N * P + P^2) | Levenberg-Marquardt non-linear least squares |
| `histo_fit_eval` / `_custom` | O(P) | O(1) | Model evaluation at coordinate x |
| `histo_fit_result_destroy` | O(1) | O(1) | Frees result arrays and container |
| `histo_fit_chi2_p_value` | O(1) | O(1) | Regularized incomplete gamma evaluation |

