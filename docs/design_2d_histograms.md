# Architecture & Design Specification: 2-Dimensional Histograms (`histo2d_t`)

This document defines the architectural blueprint, data structures, ingestion pipelines, statistical algorithms, transformations, and serialization formats for 2-Dimensional Histograms (`histo2d_t`) in `libhisto`.

---

## 1. Executive Summary & Purpose

2-Dimensional histograms map bivariate continuous or discrete data $(X, Y) \in \mathbb{R}^2$ into a grid of discrete cells (bins). They extend `libhisto`'s high-performance 1D histogramming core into multi-dimensional analysis, enabling joint probability density estimation, spatial correlation discovery, and phase-space characterization.

### 1.1 Key Use Cases Across Domains

| Domain | Application & Use Case | Key Requirements |
| :--- | :--- | :--- |
| **High-Energy Physics (HEP)** | Dalitz plots, invariant mass vs. transverse momentum ($p_T$ vs. $\eta$), detector hit occupancies | Unweighted and weighted events ($\sum w, \sum w^2$), variable bin edges at kinematic boundaries, fast 1D projections and slices. |
| **Data Science & Statistics** | Scatter plot density estimation, bivariate joint distributions, Pearson & Spearman correlation | Streaming 2D Welford covariance calculation, kernel-ready grid representations, fast marginal projections ($P(X), P(Y)$). |
| **Image Processing & Computer Vision** | 2D color histograms (Hue vs. Saturation), Optical Flow velocity fields $(\Delta u, \Delta v)$, joint spatial-intensity distributions | High-throughput batch AoS ingestion ($[x, y, x, y, \dots]$), AVX2/AVX-512 SIMD vectorization, fast normalization. |
| **Spatial & Geospatial Telemetry** | Heatmaps of GPS coordinates (Lat/Lon), robot trajectory occupancy grids, sensor array heatmaps | Real-time streaming ingestion, uniform/variable mixed grids, terminal ANSI TrueColor heatmaps. |
| **Financial Engineering & Risk** | Asset return correlations ($r_1$ vs. $r_2$), volatility vs. price return, joint tail-risk distributions | Exact online comoments, guard region tracking for extreme outlier joint events. |

---

## 2. Core Architectural Decisions

```
                           +-----------------------------------------------+
                           |                 histo2d_t                     |
                           +-----------------------------------------------+
                           | - x_axis: histo2d_axis_t                      |
                           | - y_axis: histo2d_axis_t                      |
                           | - flags: uint32_t (SUMW2, EXACT_MOMENTS)      |
                           | - storage: Row-Major Aligned Contiguous Buffs |
                           +-----------------------------------------------+
                                  |                                 |
           +----------------------+                                 +------------------------+
           |                                                                                 |
           v                                                                                 v
+-----------------------+                                                         +--------------------+
| 2D Ingestion Pipeline |                                                         | Statistical Engine |
+-----------------------+                                                         +--------------------+
| - histo2d_fill(x,y)   |                                                         | - 2D Welford       |
| - histo2d_fill_w()    |                                                         |   Means, Cov(X,Y)  |
| - histo2d_fill_n()    |                                                         |   Pearson rho_xy   |
| - histo2d_fill_strided|                                                         | - Marginal Proj.   |
| - SIMD AVX2/512 Kernel|                                                         | - Profile X/Y      |
+-----------------------+                                                         +--------------------+
```

### 2.1 Binning Models: The 4 Grid Combinations

`libhisto` 2D histograms decouple the X and Y axes, supporting all 4 combinations through the unified `histo2d_axis_t` abstraction:

1. **Uniform X – Uniform Y (`HISTO2D_MODEL_UU`)**:
   - Both axes use fixed-width equidistant intervals.
   - Lookup time: **$\mathcal{O}(1)$ direct arithmetic** on both axes.
   - Ideal for image color spaces, coordinate grids, and fixed-range telemetry.
2. **Uniform X – Variable Y (`HISTO2D_MODEL_UV`)**:
   - Uniform bins along X, arbitrary non-uniform bin boundaries along Y.
   - Lookup time: **$\mathcal{O}(\log N_y)$** (arithmetic on X, binary search on Y).
   - Useful when Y exhibits exponential or power-law distribution while X is uniform (e.g. time vs. logarithmic energy).
3. **Variable X – Uniform Y (`HISTO2D_MODEL_VU`)**:
   - Arbitrary bin boundaries along X, uniform bins along Y.
   - Lookup time: **$\mathcal{O}(\log N_x)$** (binary search on X, arithmetic on Y).
4. **Variable X – Variable Y (`HISTO2D_MODEL_VV`)**:
   - Arbitrary non-uniform bin boundaries on both axes.
   - Lookup time: **$\mathcal{O}(\log N_x + \log N_y)$** binary search.
   - Standard for complex physics phase space analysis where binning follows resonance widths.

### 2.2 Memory Layout & Cache Locality

#### Contiguous Row-Major 1D Flattened Array
Bins are stored in a single, contiguous 1D array of length $N_x \times N_y$.
The mapping from 2D coordinate bin indices $(i_x, i_y)$ to linear storage index $k$ follows **X-Major (Row-Major)** layout:

$$\text{linear\_index}(i_x, i_y) = i_x \times N_y + i_y \quad \text{where } 0 \le i_x < N_x, \; 0 \le i_y < N_y$$

*Alternative Y-Major layout:*
$$\text{linear\_index\_ymajor}(i_x, i_y) = i_y \times N_x + i_x$$

> **Design Choice: X-Major Mapping ($i_x \times N_y + i_y$)**:
> Storing consecutive $Y$ bins contiguously within an $X$ row ensures sequential memory streaming when integrating or projecting along $Y$, and allows SIMD vectorization across column blocks.

#### Structure of Arrays (SoA) vs. Array of Structures (AoS)
`libhisto` adopts **Structure of Arrays (SoA)**:
```c
double *bins;   /* [nx * ny] contiguous array of accumulated weights */
double *sum_w2; /* [nx * ny] contiguous array of sum(w^2), or NULL if disabled */
```

**Rationale**:
- **Cache Footprint**: Unweighted histograms and count queries do not pull `sum_w2` into L1/L2 cache lines, doubling effective memory bandwidth.
- **Vectorization**: Modern SIMD load/store operations (`_mm256_load_pd`, `_mm512_add_pd`) operate directly on contiguous double arrays without strided unpack instructions.
- **64-Byte Cacheline Alignment**: Buffers are aligned to 64-byte boundaries (`posix_memalign` / `_aligned_malloc` or C11 `aligned_alloc`) to prevent cacheline crossing penalties and enable aligned AVX-512 streaming instructions.

---

## 3. Out-of-Bounds & Guard Regions: 9-Region Partitioning

Any 2D sample $(x, y)$ falls into one of 9 mutually exclusive geometric regions:

```
                  Y ^
                    |
      North-West    |       North       |    North-East
       (NW: 7)      |      (N: 2)       |     (NE: 8)
  ------------------+-------------------+------------------ y_max
                    |                   |
        West        |      CENTER       |      East
       (W: 4)       |   (In-Range: 0)   |     (E: 1)
                    |                   |
  ------------------+-------------------+------------------ y_min
      South-West    |       South       |    South-East
       (SW: 5)      |      (S: 3)       |     (SE: 6)
                    |                   |
  ------------------+-------------------+------------------> X
                  x_min               x_max
```

### 3.1 Region Definitions & Enumeration

```c
typedef enum histo2d_region {
    HISTO2D_REGION_CENTER     = 0,  /**< In-range: [xmin, xmax) x [ymin, ymax) */
    HISTO2D_REGION_EAST       = 1,  /**< x >= xmax, ymin <= y < ymax */
    HISTO2D_REGION_NORTH      = 2,  /**< xmin <= x < xmax, y >= ymax */
    HISTO2D_REGION_SOUTH      = 3,  /**< xmin <= x < xmax, y < ymin */
    HISTO2D_REGION_WEST       = 4,  /**< x < xmin, ymin <= y < ymax */
    HISTO2D_REGION_SOUTH_WEST = 5,  /**< x < xmin, y < ymin */
    HISTO2D_REGION_SOUTH_EAST = 6,  /**< x >= xmax, y < ymin */
    HISTO2D_REGION_NORTH_WEST = 7,  /**< x < xmin, y >= ymax */
    HISTO2D_REGION_NORTH_EAST = 8,  /**< x >= xmax, y >= ymax */
    HISTO2D_REGION_COUNT      = 9
} histo2d_region_t;
```

### 3.2 Guard Accumulators Storage
Instead of scattered variables, guard regions are stored in a compact, indexable array inside `struct histo2d`:

```c
typedef struct histo2d_guard {
    double   weight[9];   /**< Accumulated weight per region (index 0 is unused or mirrors in-range) */
    double   sum_w2[9];   /**< Accumulated sum(w^2) per region */
    uint64_t count[9];    /**< Fill event count per region */
} histo2d_guard_t;
```

Additionally, rejected non-finite (`NaN` or `Inf`) samples are tracked via `uint64_t n_nan`.

---

## 4. Ingestion Pipeline & Vectorization Architecture

### 4.1 Ingestion Signatures
- **Scalar Unweighted**: `histo_status_t histo2d_fill(histo2d_t *h, double x, double y);`
- **Scalar Weighted**: `histo_status_t histo2d_fill_w(histo2d_t *h, double x, double y, double weight);`
- **Batch SoA Ingestion**: `histo_status_t histo2d_fill_n(histo2d_t *h, size_t n, const double *x, const double *y, const double *weights);`
- **Strided AoS Ingestion**: `histo_status_t histo2d_fill_strided(histo2d_t *h, size_t n, const double *x, size_t x_stride_bytes, const double *y, size_t y_stride_bytes, const double *weights, size_t w_stride_bytes);`
- **Direct Cell Ingestion**: `histo_status_t histo2d_fill_bin(histo2d_t *h, uint32_t ix, uint32_t iy, double weight);`

### 4.2 Uniform Bin Lookup Formulation
For uniform axes, the continuous coordinate is mapped to a discrete bin index using fast reciprocal multiplication:

$$i_x = \left\lfloor (x - x_{\min}) \cdot \text{inv\_dx} \right\rfloor, \quad i_y = \left\lfloor (y - y_{\min}) \cdot \text{inv\_dy} \right\rfloor$$

To prevent rounding error at the upper boundary ($x = x_{\max}$ or $y = y_{\max}$), `libhisto` applies upper-boundary clamping:

$$\text{if } x == x_{\max} \implies i_x = N_x - 1 \quad (\text{or overflow if strict half-open interval is configured})$$

Standard `libhisto` half-open boundary convention: $[x_i, x_{i+1})$, with the topmost bin optionally closed $[x_{N-1}, x_N]$ to catch exact maximums.

### 4.3 SIMD Vectorization Strategy for 2D Batch Fill

For uniform-uniform grids, batch ingestion in `histo2d_fill_n` leverages AVX2 / AVX-512 vector pipelines:

```
[x0, x1, x2, x3] ---> Subtract xmin, Multiply inv_dx ---> Floor & Convert to i32 [ix0, ix1, ix2, ix3]
[y0, y1, y2, y3] ---> Subtract ymin, Multiply inv_dy ---> Floor & Convert to i32 [iy0, iy1, iy2, iy3]
                                                                        |
                                    Compute linear indices: k = ix * Ny + iy
                                                                        |
                                    Detect conflicts across vector lanes (_mm256_conflict_epi32)
                                                                        |
                                    Scatter / Gather-Add into aligned double *bins array
```

For memory workloads with high duplicate bin hits in a single vector chunk, conflict detection serializes conflicting lane accumulations into temporary registers before issuing writebacks, eliminating data race hazards in vectorized kernels.

---

## 5. Exact Online 2D Statistical Moments & Correlation

When `HISTO_FLAG_EXACT_MOMENTS` is set, `histo2d_t` maintains exact online bivariate running statistics during every in-range fill using the **Weighted 2D Welford-Bennett Algorithm**:

### 5.1 Accumulators
- Total in-range weight: $W = \sum w_i$
- Running weighted sample means: $\bar{x}, \bar{y}$
- Running sum of squared deviations: $M_{2,xx}, M_{2,yy}$
- Running comoment (sum of cross deviations): $M_{2,xy}$
- Extrema: $x_{\min}^{\text{obs}}, x_{\max}^{\text{obs}}, y_{\min}^{\text{obs}}, y_{\max}^{\text{obs}}$

### 5.2 Online Update Equations (For new sample $(x, y)$ with weight $w$):

1. Update total weight:
   $$W_{\text{new}} = W_{\text{old}} + w$$
2. Compute deviations from previous means:
   $$\Delta_x = x - \bar{x}_{\text{old}}, \quad \Delta_y = y - \bar{y}_{\text{old}}$$
3. Update running means:
   $$\bar{x}_{\text{new}} = \bar{x}_{\text{old}} + \frac{w}{W_{\text{new}}} \Delta_x, \quad \bar{y}_{\text{new}} = \bar{y}_{\text{old}} + \frac{w}{W_{\text{new}}} \Delta_y$$
4. Update second moments and comoment:
   $$M_{2,xx}^{\text{new}} = M_{2,xx}^{\text{old}} + w \cdot \Delta_x \cdot (x - \bar{x}_{\text{new}})$$
   $$M_{2,yy}^{\text{new}} = M_{2,yy}^{\text{old}} + w \cdot \Delta_y \cdot (y - \bar{y}_{\text{new}})$$
   $$M_{2,xy}^{\text{new}} = M_{2,xy}^{\text{old}} + w \cdot \Delta_x \cdot (y - \bar{y}_{\text{new}})$$

### 5.3 Derived Statistical Measures

- **Sample Variances**:
  $$\sigma_x^2 = \frac{M_{2,xx}}{W}, \quad \sigma_y^2 = \frac{M_{2,yy}}{W}$$
- **Sample Covariance**:
  $$\text{Cov}(X, Y) = \frac{M_{2,xy}}{W}$$
- **Pearson Correlation Coefficient ($\rho_{xy}$)**:
  $$\rho_{xy} = \frac{\text{Cov}(X, Y)}{\sigma_x \sigma_y} = \frac{M_{2,xy}}{\sqrt{M_{2,xx} \cdot M_{2,yy}}}$$
  *Guard against $\sigma_x = 0$ or $\sigma_y = 0$: returns 0.0 or `HISTO_ERR_EMPTY`.*

---

## 6. Projections, Slices & Profile Histograms

2D histograms provide seamless reduction into 1D histograms (`histo_t`):

```
+-----------------------------------------------------------------------------------+
|                                  histo2d_t (Nx x Ny)                              |
+-----------------------------------------------------------------------------------+
       |                        |                         |                      |
       | Project All            | Range Slice             | Profile (Mean Y)     | Profile (Mean X)
       v                        v                         v                      v
+------------------+   +-------------------+    +--------------------+  +--------------------+
| histo2d_project_x|   | histo2d_slice_x   |    | histo2d_profile_x  |  | histo2d_profile_y  |
| 1D histo (Nx)    |   | 1D histo (Nx)     |    | 1D Profile (Nx)    |  | 1D Profile (Ny)    |
| Sum across all Y |   | Sum across Y range|    | Bin = Mean(Y)      |  | Bin = Mean(X)      |
|                  |   | [iy_min, iy_max]  |    | Err = StdErr(Y)    |  | Err = StdErr(X)    |
+------------------+   +-------------------+    +--------------------+  +--------------------+
```

### 6.1 1D Projections
- **Projection along X (`histo2d_project_x`)**:
  Creates a new 1D histogram matching the X-axis geometry ($N_x$, $x_{\min}, x_{\max}$, or $x\_\text{edges}$). For each $i_x \in [0, N_x-1]$:
  $$\text{bin}_{1D}[i_x] = \sum_{i_y=0}^{N_y-1} \text{bins}[i_x \cdot N_y + i_y], \quad \text{sum\_w2}_{1D}[i_x] = \sum_{i_y=0}^{N_y-1} \text{sum\_w2}[i_x \cdot N_y + i_y]$$
- **Projection along Y (`histo2d_project_y`)**:
  Creates a new 1D histogram matching the Y-axis geometry ($N_y$, $y_{\min}, y_{\max}$, or $y\_\text{edges}$). For each $i_y \in [0, N_y-1]$:
  $$\text{bin}_{1D}[i_y] = \sum_{i_x=0}^{N_x-1} \text{bins}[i_x \cdot N_y + i_y], \quad \text{sum\_w2}_{1D}[i_y] = \sum_{i_x=0}^{N_x-1} \text{sum\_w2}[i_x \cdot N_y + i_y]$$

### 6.2 1D Slices
- **X-Slice across Y-interval (`histo2d_slice_x`)**:
  Integrates over a subset of Y bins $[i_{y,\min}, i_{y,\max}]$ to produce a 1D histogram over X.
- **Y-Slice across X-interval (`histo2d_slice_y`)**:
  Integrates over a subset of X bins $[i_{x,\min}, i_{x,\max}]$ to produce a 1D histogram over Y.

### 6.3 1D Profile Histograms
Profile histograms display the conditional mean and standard error of one variable as a function of the other (equivalent to ROOT `TProfile`):

- **Profile along X (`histo2d_profile_x`)**:
  For each X-bin $i_x$, the value stored is the weighted mean of Y values in that bin:
  $$\bar{y}(i_x) = \frac{\sum_{i_y} y_{\text{center}}(i_y) \cdot w(i_x, i_y)}{\sum_{i_y} w(i_x, i_y)}$$
  Uncertainty (Standard Error of the Mean):
  $$\sigma_{\bar{y}}(i_x) = \frac{\sigma_y(i_x)}{\sqrt{N_{\text{eff}}(i_x)}}$$

---

## 7. 2D Transformations & Mathematical Operations

### 7.1 2D Rebinning
Combines adjacent cells by integer factors $(f_x, f_y)$ such that $N_x \pmod{f_x} == 0$ and $N_y \pmod{f_y} == 0$:
$$N_x' = \frac{N_x}{f_x}, \quad N_y' = \frac{N_y}{f_y}$$
$$\text{bin}'(I_x, I_y) = \sum_{u=0}^{f_x-1} \sum_{v=0}^{f_y-1} \text{bin}(I_x \cdot f_x + u, \; I_y \cdot f_y + v)$$

### 7.2 2D Normalization
Scales all bin contents such that the total 2D discrete volume (integral) or 2D continuous area integral $\iint f(x,y) \, dx \, dy$ equals `target_integral`:
$$\text{Scale Factor } S = \frac{\text{target\_integral}}{\sum_{i_x} \sum_{i_y} \text{bin}(i_x, i_y) \cdot \Delta x(i_x) \cdot \Delta y(i_y)}$$

### 7.3 2D Arithmetic Operations
- **Addition**: $H_1(x,y) \leftarrow H_1(x,y) + c \cdot H_2(x,y)$ with error propagation: $\text{sum\_w2}_1 \leftarrow \text{sum\_w2}_1 + c^2 \cdot \text{sum\_w2}_2$.
- **Subtraction**: $H_1(x,y) \leftarrow H_1(x,y) - H_2(x,y)$.
- **Multiplication / Division**: Cell-by-cell multiplication and division with standard Gaussian error propagation.
- **Scaling**: $H(x,y) \leftarrow \alpha \cdot H(x,y)$.

---

## 8. Serialization & Wire Format V3

2D histograms serialize to Canonical Little-Endian binary blobs using the **Version 3** format.

### 8.1 V3 Binary Header Specification (256 Bytes Fixed)

```
Offset (Hex)    Length    Type          Field Name       Description
---------------------------------------------------------------------------------------------------
0x00 - 0x07     8 bytes   uint8_t[8]    magic            \x89 L H I S T \n \0
0x08 - 0x09     2 bytes   uint16_t      version          0x0003 (Format V3)
0x0A - 0x0B     2 bytes   uint16_t      header_size      0x0100 (256 bytes)
0x0C - 0x0F     4 bytes   uint32_t      flags            Bitflags (HISTO2D_FLAG_*)
0x10 - 0x13     4 bytes   uint32_t      nx               Number of X bins
0x14 - 0x17     4 bytes   uint32_t      ny               Number of Y bins
0x18 - 0x1B     4 bytes   uint32_t      x_bin_type       0=Uniform, 1=Variable
0x1C - 0x1F     4 bytes   uint32_t      y_bin_type       0=Uniform, 1=Variable
0x20 - 0x27     8 bytes   double (LE)   xmin             Lower boundary along X
0x28 - 0x2F     8 bytes   double (LE)   xmax             Upper boundary along X
0x30 - 0x37     8 bytes   double (LE)   ymin             Lower boundary along Y
0x38 - 0x3F     8 bytes   double (LE)   ymax             Upper boundary along Y
0x40 - 0x47     8 bytes   double (LE)   total_weight     Total in-range accumulated weight
0x48 - 0x4F     8 bytes   double (LE)   total_sum_w2     Total in-range sum(w^2)
0x50 - 0x57     8 bytes   uint64_t(LE)  n_fills          Total in-range fill count
0x58 - 0x5F     8 bytes   uint64_t(LE)  n_nan            Rejected non-finite samples count
0x60 - 0x67     8 bytes   double (LE)   stats_mean_x     Exact online running mean X
0x68 - 0x6F     8 bytes   double (LE)   stats_mean_y     Exact online running mean Y
0x70 - 0x77     8 bytes   double (LE)   stats_M2_xx      Exact online M2_xx
0x78 - 0x7F     8 bytes   double (LE)   stats_M2_yy      Exact online M2_yy
0x80 - 0x87     8 bytes   double (LE)   stats_M2_xy      Exact online comoment M2_xy
0x88 - 0x8F     8 bytes   double (LE)   stats_min_x      Observed minimum X sample
0x90 - 0x97     8 bytes   double (LE)   stats_max_x      Observed maximum X sample
0x98 - 0x9F     8 bytes   double (LE)   stats_min_y      Observed minimum Y sample
0xA0 - 0xA7     8 bytes   double (LE)   stats_max_y      Observed maximum Y sample
0xA8 - 0xFF     88 bytes  uint8_t[88]   reserved         Zero-padded reserved future extension
```

### 8.2 Payload Ordering (Contiguous doubles, Little-Endian)
Immediately follows header at byte offset `256`:
1. `x_edges`: $(N_x + 1) \times 8$ bytes *(Only if `x_bin_type == 1`)*
2. `y_edges`: $(N_y + 1) \times 8$ bytes *(Only if `y_bin_type == 1`)*
3. `bins`: $(N_x \times N_y) \times 8$ bytes *(Always present)*
4. `sum_w2`: $(N_x \times N_y) \times 8$ bytes *(Only if `flags & HISTO_FLAG_TRACK_SUMW2`)*
5. `guard_weights`: $9 \times 8$ bytes (Weights for 9 regions)
6. `guard_sum_w2`: $9 \times 8$ bytes *(Only if `flags & HISTO_FLAG_TRACK_SUMW2`)*
7. `guard_counts`: $9 \times 8$ bytes (Fill counts for 9 regions)

---

## 9. Terminal & CLI Visualization

The CLI toolkit (`histo-plot`) is extended to support 2D rendering:

### 9.1 Visualization Modes
1. **ANSI TrueColor Heatmap (`--heatmap`)**:
   Uses 24-bit ANSI background escape codes (`\x1b[48;2;R;G;Bm  \x1b[0m`) to render smooth color gradients (e.g. Viridis, Plasma, Inferno, CoolWarm). Two vertical bins are packed into a single character cell using the Unicode half-block `▀` (foreground=top cell, background=bottom cell), doubling terminal resolution.
2. **ASCII Density Character Grid (`--ascii`)**:
   Maps normalized bin density to 10 intensity characters:
   `"  .:-=+*#%@"`
3. **Contour Map (`--contour`)**:
   Computes isolines at user-specified percentage levels (e.g. 10%, 25%, 50%, 75%, 90% of maximum height) rendered using Unicode box-drawing characters (`┼`, `─`, `│`, `┌`, `┐`, `└`, `┘`).

---

## 10. Algorithmic Complexity & Memory Reference Table

| Operation | Function Signature | Time Complexity (Uniform) | Time Complexity (Variable) | Auxiliary Space |
| :--- | :--- | :--- | :--- | :--- |
| **Creation** | `histo2d_create_*` | $\mathcal{O}(N_x \cdot N_y)$ | $\mathcal{O}(N_x \cdot N_y + N_x + N_y)$ | $\mathcal{O}(N_x \cdot N_y)$ |
| **Destruction** | `histo2d_destroy` | $\mathcal{O}(1)$ | $\mathcal{O}(1)$ | $\mathcal{O}(1)$ |
| **Single Fill** | `histo2d_fill` / `fill_w` | $\mathcal{O}(1)$ | $\mathcal{O}(\log N_x + \log N_y)$ | $\mathcal{O}(1)$ |
| **Batch Fill** | `histo2d_fill_n` | $\mathcal{O}(M)$ (AVX2 SIMD) | $\mathcal{O}(M (\log N_x + \log N_y))$ | $\mathcal{O}(1)$ |
| **Bin Lookup** | `histo2d_find_bin` | $\mathcal{O}(1)$ | $\mathcal{O}(\log N_x + \log N_y)$ | $\mathcal{O}(1)$ |
| **Projection X** | `histo2d_project_x` | $\mathcal{O}(N_x \cdot N_y)$ | $\mathcal{O}(N_x \cdot N_y)$ | $\mathcal{O}(N_x)$ |
| **Projection Y** | `histo2d_project_y` | $\mathcal{O}(N_x \cdot N_y)$ | $\mathcal{O}(N_x \cdot N_y)$ | $\mathcal{O}(N_y)$ |
| **Slice X / Y** | `histo2d_slice_*` | $\mathcal{O}(N_{\text{slice}} \cdot N_{\text{proj}})$ | $\mathcal{O}(N_{\text{slice}} \cdot N_{\text{proj}})$ | $\mathcal{O}(N_{\text{proj}})$ |
| **Profile X** | `histo2d_profile_x` | $\mathcal{O}(N_x \cdot N_y)$ | $\mathcal{O}(N_x \cdot N_y)$ | $\mathcal{O}(N_x)$ |
| **2D Rebin** | `histo2d_rebin` | $\mathcal{O}(N_x \cdot N_y)$ | $\mathcal{O}(N_x \cdot N_y)$ | $\mathcal{O}(\frac{N_x}{f_x} \cdot \frac{N_y}{f_y})$ |
| **2D Arithmetic** | `histo2d_add` / `sub` | $\mathcal{O}(N_x \cdot N_y)$ | $\mathcal{O}(N_x \cdot N_y)$ | $\mathcal{O}(1)$ |
| **Serialization** | `histo2d_serialize_binary` | $\mathcal{O}(N_x \cdot N_y)$ | $\mathcal{O}(N_x \cdot N_y + N_x + N_y)$ | $\mathcal{O}(1)$ |
| **Deserialization** | `histo2d_deserialize_binary`| $\mathcal{O}(N_x \cdot N_y)$ | $\mathcal{O}(N_x \cdot N_y + N_x + N_y)$ | $\mathcal{O}(N_x \cdot N_y)$ |
