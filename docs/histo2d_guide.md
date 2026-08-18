# 2-Dimensional Histograms Guide (`histo2d_t`) {#histo2d_guide}

`libhisto` provides high-performance, cache-aligned, and mathematically rigorous 2-Dimensional Histograms (`histo2d_t`) tailored for spatial telemetry, particle physics scattering, image processing, Monte Carlo simulation, and bivariate statistical analysis.

---

## 1. Architectural Architecture & Memory Layout

### 1.1 Four Binning Combinations
`libhisto` seamlessly supports both uniform and variable binning along either axis:

| Combination | Function | Description | Bin Lookup Complexity |
| :--- | :--- | :--- | :--- |
| **Uniform-Uniform** | `histo2d_create_uniform` | Uniform \f$N_x\f$ and uniform \f$N_y\f$ | \f$\mathcal{O}(1)\f$ |
| **Variable-Variable** | `histo2d_create_variable` | Arbitrary custom bin edges along both X and Y | \f$\mathcal{O}(\log N_x + \log N_y)\f$ |
| **Uniform-Variable** | `histo2d_create_uniform_variable` | Uniform X and variable Y edges | \f$\mathcal{O}(1 + \log N_y)\f$ |
| **Variable-Uniform** | `histo2d_create_variable_uniform` | Variable X edges and uniform Y | \f$\mathcal{O}(\log N_x + 1)\f$ |

### 1.2 Cacheline-Aligned Contiguous Memory Layout
2D cells are organized in a contiguous **X-Major (Row-Major)** Structure-of-Arrays (SoA) layout:
\f[
\text{Index}(i_x, i_y) = i_x \cdot N_y + i_y
\f]
- Allocated with 64-byte alignment (`posix_memalign` / `_aligned_malloc`) to eliminate false sharing and maximize AVX2/AVX-512 SIMD vector streaming.
- Optional per-bin sum of squared weights (`sum_w2`) is stored in a separate contiguous array allocated adjacent in memory.

---

## 2. 9-Region Guard Partitioning & Boundary Mathematics

To avoid losing out-of-range spatial events, the 2D bounding rectangle \f$[x_{\min}, x_{\max}) \times [y_{\min}, y_{\max})\f$ is partitioned into **9 discrete spatial regions**:

```text
        Y
        ▲
        │  North-West (7) │    North (2)    │ North-East (8)
   ymax ┼─────────────────┼─────────────────┼────────────────
        │                 │                 │
        │    West (4)     │   Center (0)    │    East (1)
        │                 │   (in-range)    │
   ymin ┼─────────────────┼─────────────────┼────────────────
        │  South-West (5) │    South (3)    │ South-East (6)
        └─────────────────┴─────────────────┴────────────────► X
                         xmin              xmax
```

Each guard region independently tracks:
1. `weight`: Accumulated sum of sample weights \f$\sum w\f$.
2. `sum_w2`: Accumulated sum of squared weights \f$\sum w^2\f$.
3. `count`: Number of fills \f$N\f$.

### Boundary Inclusion Semantics
- Intervals are half-open \f$[x_i, x_{i+1})\f$ along each axis.
- Exact boundary transitions:
  - \f$x = x_{\min}, y = y_{\min} \implies\f$ Bin \f$(0, 0)\f$ (Region `CENTER`).
  - \f$x = x_{\max}\f$ or \f$y = y_{\max} \implies\f$ Respective `EAST`, `NORTH`, or `NORTH_EAST` guard regions.
- Non-finite samples (`NaN`, `+Inf`, `-Inf`) are rejected with `HISTO_ERR_NON_FINITE` and tracked in `histo2d_nan_count(h)`.

---

## 3. Online Exact Bivariate Moments & Covariance

When created with `HISTO_FLAG_EXACT_MOMENTS`, `histo2d_t` updates bivariate running accumulators using **Pébay's 2D Weighted Welford Algorithm**:

\f[
\bar{x}_{n} = \bar{x}_{n-1} + \frac{w_n}{W_n}(x_n - \bar{x}_{n-1})
\f]
\f[
\bar{y}_{n} = \bar{y}_{n-1} + \frac{w_n}{W_n}(y_n - \bar{y}_{n-1})
\f]
\f[
M_{2,x}^{(n)} = M_{2,x}^{(n-1)} + w_n (x_n - \bar{x}_{n-1})(x_n - \bar{x}_n)
\f]
\f[
M_{2,y}^{(n)} = M_{2,y}^{(n-1)} + w_n (y_n - \bar{y}_{n-1})(y_n - \bar{y}_n)
\f]
\f[
C_{xy}^{(n)} = C_{xy}^{(n-1)} + w_n (x_n - \bar{x}_{n-1})(y_n - \bar{y}_n)
\f]

### Statistical Queries:
- **Sample Covariance**:
  \f[
  \text{Cov}(X, Y) = \frac{C_{xy}}{W}
  \f]
- **Pearson Product-Moment Correlation Coefficient**:
  \f[
  \rho_{xy} = \frac{\text{Cov}(X, Y)}{\sigma_x \cdot \sigma_y} = \frac{C_{xy}}{\sqrt{M_{2,x} \cdot M_{2,y}}}, \quad \rho_{xy} \in [-1.0, +1.0]
  \f]

---

## 4. Projections, Slices & Profiles

```text
                +-------------------+
                |   histo2d_t       |
                +-------------------+
                  /        |        \
                 /         |         \
                ▼          ▼          ▼
         Projection      Slice      Profile
        (Integral)    (Sub-range)  (Conditional Mean)
```

### 4.1 Marginal Projections (`histo2d_project_x`, `histo2d_project_y`)
Collapses one axis by integrating over all bins in the orthogonal dimension:
\f[
\text{Proj}_X(i_x) = \sum_{i_y=0}^{N_y-1} \text{bin}(i_x, i_y), \quad \sigma^2_{\text{Proj}_X}(i_x) = \sum_{i_y=0}^{N_y-1} \text{sum\_w2}(i_x, i_y)
\f]

### 4.2 Slices (`histo2d_slice_x`, `histo2d_slice_y`)
Integrates over a user-selected sub-interval of orthogonal bins \f$[i_{\min}, i_{\max}]\f$.

### 4.3 Profile Histograms (`histo2d_profile_x`, `histo2d_profile_y`)
Evaluates the conditional expectation and standard error of the mean for each bin along an axis (equivalent to ROOT `TProfile`):
\f[
\bar{y}(i_x) = \frac{\sum_{i_y} y_{\text{center}}(i_y) \cdot w(i_x, i_y)}{\sum_{i_y} w(i_x, i_y)}
\f]
\f[
\sigma_{\bar{y}}(i_x) = \frac{\sigma_y(i_x)}{\sqrt{N_{\text{eff}}(i_x)}}
\f]

---

## 5. Complete C99 Quickstart Example

```c
#include <histo/histo2d.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    /* 1. Create a 50x50 2D histogram */
    histo2d_t *h = histo2d_create_uniform(50, -5.0, 5.0, 50, -5.0, 5.0,
                                          HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);

    /* 2. Ingest bivariate Gaussian samples */
    for (int i = 0; i < 100000; ++i) {
        double x = ((double)rand() / RAND_MAX) * 6.0 - 3.0;
        double y = 0.8 * x + (((double)rand() / RAND_MAX) * 2.0 - 1.0);
        histo2d_fill(h, x, y);
    }

    /* 3. Compute summary statistics */
    histo2d_stats_t stats;
    histo2d_get_stats(h, &stats);

    printf("Entries:      %llu\n", (unsigned long long)stats.n_entries);
    printf("Mean (X, Y):  (%.4f, %.4f)\n", stats.mean_x, stats.mean_y);
    printf("StdDev (X,Y): (%.4f, %.4f)\n", stats.std_dev_x, stats.std_dev_y);
    printf("Covariance:   %.4f\n", stats.covariance);
    printf("Correlation:  %.4f\n", stats.correlation);

    /* 4. Extract 1D projection along X */
    histo_t *proj_x = NULL;
    histo2d_project_x(h, &proj_x);

    /* 5. Clean up */
    histo_destroy(proj_x);
    histo2d_destroy(h);
    return 0;
}
```

---

## 6. Algorithmic Complexity Reference Table

| Operation | Function | Time Complexity (Uniform) | Time Complexity (Variable) | Space Complexity |
| :--- | :--- | :--- | :--- | :--- |
| **Creation** | `histo2d_create_*` | \f$\mathcal{O}(N_x \cdot N_y)\f$ | \f$\mathcal{O}(N_x \cdot N_y + N_x + N_y)\f$ | \f$\mathcal{O}(N_x \cdot N_y)\f$ |
| **Destruction** | `histo2d_destroy` | \f$\mathcal{O}(1)\f$ | \f$\mathcal{O}(1)\f$ | \f$\mathcal{O}(1)\f$ |
| **Single Ingestion** | `histo2d_fill` / `fill_w` | \f$\mathcal{O}(1)\f$ | \f$\mathcal{O}(\log N_x + \log N_y)\f$ | \f$\mathcal{O}(1)\f$ |
| **Batch Ingestion** | `histo2d_fill_n` | \f$\mathcal{O}(M)\f$ | \f$\mathcal{O}(M(\log N_x + \log N_y))\f$ | \f$\mathcal{O}(1)\f$ |
| **Bin Lookup** | `histo2d_find_bin` | \f$\mathcal{O}(1)\f$ | \f$\mathcal{O}(\log N_x + \log N_y)\f$ | \f$\mathcal{O}(1)\f$ |
| **Projection X / Y** | `histo2d_project_x/y` | \f$\mathcal{O}(N_x \cdot N_y)\f$ | \f$\mathcal{O}(N_x \cdot N_y)\f$ | \f$\mathcal{O}(N_x)\f$ or \f$\mathcal{O}(N_y)\f$ |
| **Slice X / Y** | `histo2d_slice_x/y` | \f$\mathcal{O}(K \cdot N)\f$ | \f$\mathcal{O}(K \cdot N)\f$ | \f$\mathcal{O}(N)\f$ |
| **Profile X / Y** | `histo2d_profile_x/y` | \f$\mathcal{O}(N_x \cdot N_y)\f$ | \f$\mathcal{O}(N_x \cdot N_y)\f$ | \f$\mathcal{O}(N_x)\f$ or \f$\mathcal{O}(N_y)\f$ |
| **Rebinning** | `histo2d_rebin` | \f$\mathcal{O}(N_x \cdot N_y)\f$ | \f$\mathcal{O}(N_x \cdot N_y)\f$ | \f$\mathcal{O}(\frac{N_x N_y}{f_x f_y})\f$ |
| **Serialization** | `histo2d_serialize_binary` | \f$\mathcal{O}(N_x \cdot N_y)\f$ | \f$\mathcal{O}(N_x \cdot N_y + N_x + N_y)\f$ | \f$\mathcal{O}(1)\f$ |
