# Public C API Specification: `libhisto`

This document defines the complete public C API for `libhisto` (ISO C99 compliant).

---

## 1. Header Organization

- **`include/histo/version.h`**: Version constants (`HISTO_VERSION_MAJOR`, etc.).
- **`include/histo/types.h`**: Status codes, enums, flag definitions, and summary structs.
- **`include/histo/histo.h`**: Master umbrella header declaring all public function prototypes.

All public headers include `extern "C"` wrappers for seamless C++ integration.

---

## 2. Status & Error Reporting: `histo_status_t`

```c
typedef enum histo_status {
    HISTO_OK                    =  0,  /* Success */
    HISTO_ERR_INVALID_ARG       = -1,  /* NULL pointer or invalid argument passed */
    HISTO_ERR_NOMEM             = -2,  /* Memory allocation failed */
    HISTO_ERR_INCOMPATIBLE      = -3,  /* Histograms have mismatched geometry/binning */
    HISTO_ERR_OUT_OF_RANGE      = -4,  /* Bin index out of bounds */
    HISTO_ERR_NON_FINITE        = -5,  /* Unexpected NaN or Infinity */
    HISTO_ERR_EMPTY             = -6,  /* Operation requires at least one entry/non-zero weight */
    HISTO_ERR_DIV_BY_ZERO       = -7,  /* Division by zero encountered */
    HISTO_ERR_SERIALIZATION     = -8,  /* Serialization encoding error */
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

/* Bulk fill N samples with optional weights array (pass weights=NULL for unit weights). */
histo_status_t histo_fill_n(histo_t *h, size_t n, const double *x, const double *weights);

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

/* Global counters & accumulators */
double   histo_underflow(const histo_t *h);
double   histo_overflow(const histo_t *h);
uint64_t histo_nan_count(const histo_t *h);
double   histo_total_weight(const histo_t *h);
uint64_t histo_num_entries(const histo_t *h);
```

### 3.4 Statistical Analysis
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

histo_status_t histo_mean(const histo_t *h, double *out_mean);
histo_status_t histo_variance(const histo_t *h, double *out_variance);
histo_status_t histo_std_dev(const histo_t *h, double *out_std_dev);
histo_status_t histo_quantile(const histo_t *h, double p, double *out_quantile);
histo_status_t histo_median(const histo_t *h, double *out_median);
histo_status_t histo_integral(const histo_t *h, uint32_t start_bin, uint32_t end_bin, double *out_integral);
histo_status_t histo_get_stats(const histo_t *h, histo_stats_t *out_stats);
```

### 3.5 Arithmetic & Transformations
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

### 3.6 Serialization & Deserialization
```c
/* Serialize to canonical Little-Endian binary byte buffer. */
histo_status_t histo_serialize_binary(const histo_t *h, void **out_buf, size_t *out_size);

/* Deserialize from binary byte buffer. */
histo_status_t histo_deserialize_binary(const void *buf, size_t size, histo_t **out_h);

/* Serialize to human-readable JSON string. */
histo_status_t histo_serialize_json(const histo_t *h, char **out_json_str);

/* Deserialize from JSON string. */
histo_status_t histo_deserialize_json(const char *json_str, histo_t **out_h);

/* Free buffers returned by serialization functions. */
void histo_free_buffer(void *buf);
```
