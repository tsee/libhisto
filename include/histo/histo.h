#ifndef LIBHISTO_HISTO_H
#define LIBHISTO_HISTO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "types.h"
#include "version.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* Status & Error Strings                                                    */
/* ========================================================================= */

/**
 * Returns a human-readable description of a status code.
 */
const char* histo_status_str(histo_status_t status);

/* ========================================================================= */
/* Lifecycle & Memory Management                                             */
/* ========================================================================= */

/**
 * Creates a new uniform-bin histogram over the interval [min, max] with nbins.
 * Returns NULL on allocation failure or invalid arguments (e.g. min >= max, nbins == 0).
 */
histo_t* histo_create_uniform(uint32_t nbins, double min, double max, uint32_t flags);

/**
 * Creates a new variable-bin histogram from an array of (nbins + 1) strictly monotonic edges.
 * Returns NULL on allocation failure or invalid arguments.
 */
histo_t* histo_create_variable(uint32_t nbins, const double *edges, uint32_t flags);

/**
 * Destroys a histogram and releases all underlying memory. Safe to call with NULL.
 */
void histo_destroy(histo_t *h);

/**
 * Clones a histogram. If 'empty' is true, the cloned histogram retains the same
 * binning structure but with all bin counts and statistics reset to zero.
 */
histo_t* histo_clone(const histo_t *src, bool empty);

/**
 * Resets all accumulated data, weights, moments, and out-of-range counters to zero.
 */
histo_status_t histo_reset(histo_t *h);

/* ========================================================================= */
/* Ingestion / Filling                                                       */
/* ========================================================================= */

/**
 * Fills a single value into the histogram with unit weight (1.0).
 */
histo_status_t histo_fill(histo_t *h, double x);

/**
 * Fills a single value into the histogram with a specified weight.
 */
histo_status_t histo_fill_w(histo_t *h, double x, double weight);

/**
 * Batch fills N contiguous values into the histogram with an optional weights array.
 * If weights is NULL, unit weights (1.0) are used for all samples.
 * Returns HISTO_WARN_NON_FINITE if any NaN or non-finite values were skipped.
 */
histo_status_t histo_fill_n(histo_t *h, size_t n, const double *x, const double *weights);

/**
 * Strided batch fill for Array-of-Structs (AoS) data layouts.
 * Pass x_stride_bytes = sizeof(struct) and w_stride_bytes = sizeof(struct).
 * If weights is NULL, unit weights (1.0) are used for all samples.
 */
histo_status_t histo_fill_strided(histo_t *h, size_t n,
                                  const double *x, size_t x_stride_bytes,
                                  const double *weights, size_t w_stride_bytes);

/**
 * Directly fills weight into a specific bin index.
 */
histo_status_t histo_fill_bin(histo_t *h, uint32_t bin_index, double weight);

/* ========================================================================= */
/* Geometry & Bin Query                                                      */
/* ========================================================================= */

/**
 * Returns the number of in-range bins.
 */
uint32_t histo_nbins(const histo_t *h);

/**
 * Returns the binning model type.
 */
histo_bin_type_t histo_bin_type(const histo_t *h);

/**
 * Retrieves the overall range [min, max].
 */
histo_status_t histo_range(const histo_t *h, double *out_min, double *out_max);

/**
 * Locates the bin index for coordinate x using boundary-guarded lookup.
 * Sets *out_bin to index in [0, nbins - 1], or -1 for underflow, or (int64_t)nbins for overflow.
 */
histo_status_t histo_find_bin(const histo_t *h, double x, int64_t *out_bin);

/**
 * Retrieves lower and upper boundary coordinates for bin_index.
 */
histo_status_t histo_bin_bounds(const histo_t *h, uint32_t bin_index, double *out_lower, double *out_upper);

/**
 * Retrieves the center coordinate of bin_index.
 */
histo_status_t histo_bin_center(const histo_t *h, uint32_t bin_index, double *out_center);

/**
 * Retrieves the accumulated content/weight of bin_index.
 */
histo_status_t histo_bin_content(const histo_t *h, uint32_t bin_index, double *out_content);

/**
 * Retrieves the statistical uncertainty of bin_index (sqrt(sum_w2) or sqrt(N)).
 */
histo_status_t histo_bin_error(const histo_t *h, uint32_t bin_index, double *out_error);

/**
 * Returns the accumulated underflow weight (x < min).
 */
double histo_underflow(const histo_t *h);

/**
 * Returns the accumulated overflow weight (x >= max).
 */
double histo_overflow(const histo_t *h);

/**
 * Returns the number of rejected non-finite (NaN) samples.
 */
uint64_t histo_nan_count(const histo_t *h);

/**
 * Returns total accumulated in-range weight.
 */
double histo_total_weight(const histo_t *h);

/**
 * Returns total number of in-range fill operations.
 */
uint64_t histo_num_entries(const histo_t *h);

/* ========================================================================= */
/* Statistical Analysis                                                      */
/* ========================================================================= */

/**
 * Computes or retrieves the sample mean.
 */
histo_status_t histo_mean(const histo_t *h, double *out_mean);

/**
 * Computes or retrieves the sample variance.
 */
histo_status_t histo_variance(const histo_t *h, double *out_variance);

/**
 * Computes or retrieves the sample standard deviation.
 */
histo_status_t histo_std_dev(const histo_t *h, double *out_std_dev);

/**
 * Estimates the quantile value corresponding to cumulative probability p in [0.0, 1.0].
 * Uses continuous linear interpolation bracketed strictly on non-empty bins.
 */
histo_status_t histo_quantile(const histo_t *h, double p, double *out_quantile);

/**
 * Estimates the median (50th percentile).
 */
histo_status_t histo_median(const histo_t *h, double *out_median);

/**
 * Computes the integral (sum of bin contents) across bin range [start_bin, end_bin].
 */
histo_status_t histo_integral(const histo_t *h, uint32_t start_bin, uint32_t end_bin, double *out_integral);

/**
 * Computes and populates a summary statistics structure.
 */
histo_status_t histo_get_stats(const histo_t *h, histo_stats_t *out_stats);

/* ========================================================================= */
/* Arithmetic & Transformations                                              */
/* ========================================================================= */

/**
 * Performs element-wise addition: target += other. Both must have identical binning.
 */
histo_status_t histo_add(histo_t *target, const histo_t *other);

/**
 * Performs element-wise subtraction: target -= other. Both must have identical binning.
 */
histo_status_t histo_subtract(histo_t *target, const histo_t *other);

/**
 * Performs element-wise multiplication: target *= other (using division-free error propagation).
 */
histo_status_t histo_multiply(histo_t *target, const histo_t *other);

/**
 * Performs element-wise division: target /= other.
 */
histo_status_t histo_divide(histo_t *target, const histo_t *other);

/**
 * Scales all bin contents and weights by a scalar factor.
 */
histo_status_t histo_scale(histo_t *h, double factor);

/**
 * Normalizes the total in-range weight of the histogram to target_area.
 */
histo_status_t histo_normalize(histo_t *h, double target_area);

/**
 * Rebins the histogram by integer factor K (nbins must be evenly divisible by factor).
 * Returns a new histogram handle, or NULL on failure.
 */
histo_t* histo_rebin(const histo_t *src, uint32_t factor);

/**
 * Slices the histogram to a sub-range of bins [start_bin, end_bin].
 * Returns a new histogram handle, or NULL on failure.
 */
histo_t* histo_slice(const histo_t *src, uint32_t start_bin, uint32_t end_bin, bool empty);

/**
 * Generates a cumulative distribution function (CDF) histogram.
 * Returns a new histogram handle, or NULL on failure.
 */
histo_t* histo_cdf(const histo_t *src, double prenormalization);

/* ========================================================================= */
/* Serialization & Deserialization                                           */
/* ========================================================================= */

/**
 * Calculates the exact byte size required to serialize the histogram in binary format.
 */
size_t histo_serialize_binary_size(const histo_t *h);

/**
 * Serializes the histogram into a caller-provided binary buffer of given capacity.
 */
histo_status_t histo_serialize_binary_into(const histo_t *h, void *buf, size_t capacity);

/**
 * Serializes the histogram into a newly allocated binary byte buffer.
 * Caller must free *out_buf using histo_free_buffer().
 */
histo_status_t histo_serialize_binary(const histo_t *h, void **out_buf, size_t *out_size);

/**
 * Deserializes a histogram from a binary buffer.
 */
histo_status_t histo_deserialize_binary(const void *buf, size_t size, histo_t **out_h);

/**
 * Calculates the maximum buffer size required to serialize the histogram into JSON.
 */
size_t histo_serialize_json_size(const histo_t *h);

/**
 * Serializes the histogram into a caller-provided JSON string buffer of given capacity.
 */
histo_status_t histo_serialize_json_into(const histo_t *h, char *buf, size_t capacity);

/**
 * Serializes the histogram into a newly allocated JSON string.
 * Caller must free *out_json_str using histo_free_buffer().
 */
histo_status_t histo_serialize_json(const histo_t *h, char **out_json_str);

/**
 * Deserializes a histogram from a JSON string.
 */
histo_status_t histo_deserialize_json(const char *json_str, histo_t **out_h);

/**
 * Frees buffers allocated by serialization functions. Safe to call on NULL.
 */
void histo_free_buffer(void *buf);

#ifdef __cplusplus
}
#endif

#endif /* LIBHISTO_HISTO_H */
