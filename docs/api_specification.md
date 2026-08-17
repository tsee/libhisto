# Public C API Specification: `libhisto`

This document defines the complete public C API for `libhisto` (ISO C99 compliant).

---

## 1. Header Organization

- **`include/histo/version.h`**: Version constants (`HISTO_VERSION_MAJOR`, `HISTO_VERSION_MINOR`, `HISTO_VERSION_PATCH`, `HISTO_VERSION_STRING`).
- **`include/histo/types.h`**: Status codes (`histo_status_t`), enums (`histo_bin_type_t`), feature flags (`histo_flags_t`), and summary structs (`histo_stats_t`).
- **`include/histo/histo.h`**: Master umbrella header declaring all public function prototypes.

All public headers include `extern "C"` wrappers for seamless C++ integration.

---

## 2. Status & Error Reporting: `histo_status_t`

```c
typedef enum histo_status {
    HISTO_WARN_NON_FINITE       =  1,  /* Operation succeeded, but non-finite sample(s) were skipped */
    HISTO_OK                    =  0,  /* Success */
    HISTO_ERR_INVALID_ARG       = -1,  /* NULL pointer or invalid argument passed */
    HISTO_ERR_NOMEM             = -2,  /* Memory allocation failed */
    HISTO_ERR_INCOMPATIBLE      = -3,  /* Histograms have mismatched geometry/binning */
    HISTO_ERR_OUT_OF_RANGE      = -4,  /* Bin index or percentile out of bounds */
    HISTO_ERR_NON_FINITE        = -5,  /* Unexpected NaN or Infinity input rejected */
    HISTO_ERR_EMPTY             = -6,  /* Operation requires at least one entry / non-zero weight */
    HISTO_ERR_DIV_BY_ZERO       = -7,  /* Division by zero (e.g. zero variance in kurtosis) */
    HISTO_ERR_SERIALIZATION     = -8,  /* Serialization encoding error / buffer capacity too small */
    HISTO_ERR_DESERIALIZATION   = -9   /* Corrupted or invalid serialized data */
} histo_status_t;

/* Human-readable error message lookup */
const char* histo_status_str(histo_status_t status);
```

---

## 3. Function Categories

### 3.1 Lifecycle & Allocation
```c
/* Create a uniform-bin histogram. Returns NULL on allocation or parameter failure. */
histo_t* histo_create_uniform(uint32_t nbins, double min, double max, uint32_t flags);

/* Create a variable-bin histogram from (nbins + 1) strictly monotonic edges. */
histo_t* histo_create_variable(uint32_t nbins, const double *edges, uint32_t flags);

/* Destroy histogram and release all allocated memory. Safe to call on NULL. */
void histo_destroy(histo_t *h);

/* Deep copy of a histogram. If 'empty' is true, copies binning structure with zeroed data. */
histo_t* histo_clone(const histo_t *src, bool empty);

/* Reset all accumulated counts, weights, moments, and out-of-range counters to zero. */
histo_status_t histo_reset(histo_t *h);
```

### 3.2 Data Ingestion (Fill)
```c
/* Fill a single sample with unit weight (1.0). */
histo_status_t histo_fill(histo_t *h, double x);

/* Fill a single sample with explicit weight. */
histo_status_t histo_fill_w(histo_t *h, double x, double weight);

/* Bulk fill N contiguous samples with optional weights array. */
histo_status_t histo_fill_n(histo_t *h, size_t n, const double *x, const double *weights);

/* Strided batch fill for Array-of-Structs (AoS) datasets. */
histo_status_t histo_fill_strided(histo_t *h, size_t n,
                                  const double *x, size_t x_stride_bytes,
                                  const double *weights, size_t w_stride_bytes);

/* Direct fill into a specific bin index with explicit weight. */
histo_status_t histo_fill_bin(histo_t *h, uint32_t bin_index, double weight);
```

### 3.3 Geometry & Bin Query
```c
/* Return number of in-range bins. */
uint32_t histo_nbins(const histo_t *h);

/* Return binning type (HISTO_BIN_UNIFORM or HISTO_BIN_VARIABLE). */
histo_bin_type_t histo_bin_type(const histo_t *h);

/* Query overall range [min, max]. */
histo_status_t histo_range(const histo_t *h, double *out_min, double *out_max);

/* Locate bin index for coordinate x. Sets *out_bin to index in [0, nbins-1],
 * or -1 for underflow, or (int64_t)nbins for overflow. */
histo_status_t histo_find_bin(const histo_t *h, double x, int64_t *out_bin);

/* Query lower and upper boundary coordinates for bin_index. */
histo_status_t histo_bin_bounds(const histo_t *h, uint32_t bin_index, double *out_lower, double *out_upper);

/* Query bin center coordinate for bin_index. */
histo_status_t histo_bin_center(const histo_t *h, uint32_t bin_index, double *out_center);

/* Query bin accumulated content/weight. */
histo_status_t histo_bin_content(const histo_t *h, uint32_t bin_index, double *out_content);

/* Query statistical error / standard deviation of a bin (sqrt(sum_w2) or sqrt(N)). */
histo_status_t histo_bin_error(const histo_t *h, uint32_t bin_index, double *out_error);

/* Query accumulated sum_w2 of a bin. */
histo_status_t histo_bin_sum_w2(const histo_t *h, uint32_t bin_index, double *out_sum_w2);

/* Global counters & accumulators */
double   histo_underflow(const histo_t *h);
double   histo_overflow(const histo_t *h);
uint64_t histo_nan_count(const histo_t *h);
double   histo_total_weight(const histo_t *h);
uint64_t histo_num_entries(const histo_t *h);
```

### 3.4 Summary Statistics & Quantiles
```c
typedef struct histo_stats {
    uint64_t n_entries;
    double   total_weight;
    double   mean;
    double   variance;
    double   std_dev;
    double   min;
    double   max;
    double   median;
} histo_stats_t;

/* Parametric moments */
histo_status_t histo_mean(const histo_t *h, double *out_mean);
histo_status_t histo_variance(const histo_t *h, double *out_variance);
histo_status_t histo_std_dev(const histo_t *h, double *out_std_dev);
histo_status_t histo_central_moment(const histo_t *h, uint32_t k, double *out_moment);
histo_status_t histo_skewness(const histo_t *h, double *out_skewness);
histo_status_t histo_kurtosis(const histo_t *h, double *out_kurtosis);
histo_status_t histo_excess_kurtosis(const histo_t *h, double *out_exc_kurtosis);

/* Peak & shape metrics */
histo_status_t histo_mode_bin(const histo_t *h, uint32_t *out_bin_idx);
histo_status_t histo_mode_continuous(const histo_t *h, double *out_mode);
histo_status_t histo_fwhm(const histo_t *h, double *out_fwhm);
histo_status_t histo_rms(const histo_t *h, double *out_rms);

/* Quantiles and robust dispersion */
histo_status_t histo_quantile(const histo_t *h, double p, double *out_quantile);
histo_status_t histo_median(const histo_t *h, double *out_median);
histo_status_t histo_iqr(const histo_t *h, double *out_iqr);
histo_status_t histo_mad(const histo_t *h, double *out_mad);
histo_status_t histo_trimmed_mean(const histo_t *h, double lower_p, double upper_p, double *out_mean);
histo_status_t histo_winsorized_mean(const histo_t *h, double lower_p, double upper_p, double *out_mean);

/* Integral & composite stats */
histo_status_t histo_integral(const histo_t *h, uint32_t start_bin, uint32_t end_bin, double *out_integral);
histo_status_t histo_get_stats(const histo_t *h, histo_stats_t *out_stats);
```

### 3.5 Two-Distribution Comparison & Distance Metrics
```c
/* Chi-Square test of compatibility between two histograms */
histo_status_t histo_cmp_chi2(const histo_t *h1, const histo_t *h2, double *out_chi2, uint32_t *out_ndf);

/* Kolmogorov-Smirnov test statistic D between two histograms */
histo_status_t histo_cmp_ks(const histo_t *h1, const histo_t *h2, double *out_ks_stat);

/* 1D Wasserstein distance / Earth Mover's Distance between two histograms */
histo_status_t histo_cmp_wasserstein_1d(const histo_t *h1, const histo_t *h2, double *out_distance);

/* Kullback-Leibler divergence D_KL(h1 || h2) in nats */
histo_status_t histo_cmp_kl_divergence(const histo_t *h1, const histo_t *h2, double *out_divergence);

/* Bhattacharyya distance between two histograms */
histo_status_t histo_cmp_bhattacharyya(const histo_t *h1, const histo_t *h2, double *out_distance);
```

### 3.6 Arithmetic & Transformations
```c
/* Element-wise addition: target += other */
histo_status_t histo_add(histo_t *target, const histo_t *other);

/* Element-wise subtraction: target -= other */
histo_status_t histo_subtract(histo_t *target, const histo_t *other);

/* Element-wise multiplication: target *= other */
histo_status_t histo_multiply(histo_t *target, const histo_t *other);

/* Element-wise division: target /= other */
histo_status_t histo_divide(histo_t *target, const histo_t *other);

/* Scale all bin contents and total weights by scalar factor */
histo_status_t histo_scale(histo_t *h, double factor);

/* Normalize total in-range weight to target_area */
histo_status_t histo_normalize(histo_t *h, double target_area);

/* Rebin by integer factor K (nbins must be divisible by factor). Returns new histogram. */
histo_t* histo_rebin(const histo_t *src, uint32_t factor);

/* Crop a slice of bins [start_bin, end_bin]. Returns new histogram. */
histo_t* histo_slice(const histo_t *src, uint32_t start_bin, uint32_t end_bin, bool empty);

/* Compute cumulative distribution function (CDF). Returns new normalized histogram. */
histo_t* histo_cdf(const histo_t *src, double prenormalization);
```

### 3.7 Serialization & Deserialization
```c
/* Determine required binary buffer size */
size_t histo_serialize_binary_size(const histo_t *h);

/* Serialize into caller-provided binary buffer */
histo_status_t histo_serialize_binary_into(const histo_t *h, void *buf, size_t capacity);

/* Serialize to newly allocated binary buffer (caller frees via histo_free_buffer) */
histo_status_t histo_serialize_binary(const histo_t *h, void **out_buf, size_t *out_size);

/* Deserialize from binary buffer */
histo_status_t histo_deserialize_binary(const void *buf, size_t size, histo_t **out_h);

/* Migrate older binary format buffers (e.g. v1) to current format version (v2) */
histo_status_t histo_migrate_binary(const void *in_buf, size_t in_size, void **out_buf, size_t *out_size);

/* Free library-allocated serialization buffers. Safe on NULL. */
void histo_free_buffer(void *buf);
```
