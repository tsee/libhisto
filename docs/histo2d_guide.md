# 2-Dimensional Histograms & Spatial Telemetry Guide {#histo2d_guide}

This guide details the 2-Dimensional Histogramming capabilities of `libhisto` (`include/histo/histo2d.h` and the CLI `histo fill --2d`). `libhisto` supports all 4 binning grid combinations (Uniform-Uniform, Variable-Variable, Uniform-Variable, Variable-Uniform), 9-region out-of-bounds guard partitioning, online bivariate Welford covariance \f$\mathrm{Cov}(X, Y)\f$, Pearson correlation \f$\rho_{xy}\f$, 1D marginal projections, sub-interval slices, profile histograms, and 24-bit TrueColor terminal heatmaps.


---

## 1. CLI: 2D Workflows & Heatmaps

### 1.1 Ingestion with Delimiter Auto-Detection

The `histo fill --2d` tool automatically detects common delimiters (commas `,`, tabs `\t`, semicolons `;`, and whitespace):

```bash
# Generate 20,000 bivariate Gaussian samples and render TrueColor heatmap
python3 -c "import random; print('\n'.join(f'{random.gauss(-1.5, 1.0)},{random.gauss(-1.5, 1.0)}' if random.random() < 0.55 else f'{random.gauss(2.0, 0.8)},{random.gauss(2.0, 0.8)}' for _ in range(20000)))" \
  | histo fill --2d --xbins 30 --ybins 18 \
  | histo plot
```

**Terminal Output:**
```text
── 2D Histogram Heatmap (30 x 18 bins, Entries: 20000, Weight: 2e+04) ──
   4.88 │                                                              │
   4.35 │                             . . : - - . .                    │
   3.82 │                           . : + # # # + : .                  │
   3.28 │                           . - + % @ # + : .                  │
   2.75 │                             . - = = = - .                    │
   2.22 │               . . . .           . . .                        │
   1.69 │           . : - = - - : .                                    │
   1.16 │         . - = * * * + = : .                                  │
   0.63 │         . - = * # # + = : .                                  │
   0.09 │         . : - = + = = : .                                    │
  -0.44 │             . . : . . .                                      │
  -0.97 │                                                              │
        └─────────────────────────────────────────────────────────────┘
         -5.61                         -0.33                            4.96

  Density scale: [ .:-=+*#%@ ] (0.00 -> 5.60e+02)
```

---

## 2. Programmatic C99 API (`histo/histo2d.h`)

### 2.1 Initialization, Ingestion & Bivariate Statistics

```c
#include <stdio.h>
#include <histo/histo2d.h>

int main(void) {
    // 1. Create a 50x50 2D histogram over [-5.0, 5.0] x [-5.0, 5.0]
    histo2d_t *h2d = histo2d_create_uniform(50, -5.0, 5.0, 50, -5.0, 5.0,
                                            HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    if (!h2d) return 1;

    // 2. Ingest coordinate pairs
    histo2d_fill(h2d, 1.2, 2.4);
    histo2d_fill_w(h2d, -0.5, 0.5, 3.0); // Weighted entry

    // 3. Batch ingestion (SIMD accelerated on AVX2 / AVX-512 / NEON)
    double x[4] = {0.1, 1.2, -2.1, 3.0};
    double y[4] = {0.2, 1.4, -1.9, 2.8};
    histo2d_fill_n(h2d, 4, x, y, NULL);

    // 4. Query exact running moments
    histo2d_stats_t stats;
    histo2d_get_stats(h2d, &stats);
    printf("Total Entries: %lu\n", (unsigned long)stats.n_entries);
    printf("Mean X: %.3f +/- %.3f | Mean Y: %.3f +/- %.3f\n",
           stats.mean_x, stats.std_dev_x, stats.mean_y, stats.std_dev_y);
    printf("Covariance Cov(X,Y): %.4f | Pearson Correlation (rho): %.4f\n",
           stats.covariance, stats.correlation);

    // 5. Clean teardown
    histo2d_destroy(h2d);
    return 0;
}
```

### 2.2 Projections, Slices & Profile Histograms

Transform 2D distributions into 1D histograms (`histo_t*`):

```c
// 1. Marginal projection along X (integrates across all Y bins)
histo_t *proj_x = NULL;
histo2d_project_x(h2d, &proj_x);

// 2. Sub-interval slice along X (integrates Y over bin index range [y_start, y_end])
histo_t *slice_x = NULL;
histo2d_slice_x(h2d, 10, 25, &slice_x);

// 3. Profile histogram along X (computes conditional mean of Y and error on mean for each X bin)
histo_t *prof_x = NULL;
histo2d_profile_x(h2d, &prof_x);

// Inspect results with any 1D function
double mean = 0.0;
histo_mean(proj_x, &mean);
printf("Projected X Mean: %.3f\n", mean);

// Destroy generated 1D histograms
histo_destroy(proj_x);
histo_destroy(slice_x);
histo_destroy(prof_x);
```

### 2.3 2D Serialization (Format V3 & JSON)

```c
// Format V3 Canonical Little-Endian binary
void *buf = NULL;
size_t size = 0;
if (histo2d_serialize_binary_alloc(h2d, &buf, &size) == HISTO_OK) {
    histo2d_t *reloaded = NULL;
    histo2d_deserialize_binary(buf, size, &reloaded);
    printf("Reloaded entries: %lu\n", (unsigned long)histo2d_num_entries(reloaded));
    histo2d_destroy(reloaded);
    histo_free_buffer(buf);
}
```
