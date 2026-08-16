#ifndef LIBHISTO_TYPES_H
#define LIBHISTO_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum allowable bins to guard against integer overflow */
#define HISTO_MAX_NBINS 100000000u

/* Opaque histogram handle */
typedef struct histo histo_t;

/* Status and warning/error codes */
typedef enum histo_status {
    HISTO_WARN_NON_FINITE     =  1,  /* Operation succeeded, but non-finite sample(s) were skipped */
    HISTO_OK                  =  0,  /* Operation succeeded */
    HISTO_ERR_INVALID_ARG     = -1,  /* NULL pointer or invalid parameter */
    HISTO_ERR_NOMEM           = -2,  /* Out of memory */
    HISTO_ERR_INCOMPATIBLE    = -3,  /* Histograms have incompatible geometry */
    HISTO_ERR_OUT_OF_RANGE    = -4,  /* Bin index or range out of bounds */
    HISTO_ERR_NON_FINITE      = -5,  /* Non-finite (NaN or Inf) input rejected */
    HISTO_ERR_EMPTY           = -6,  /* Histogram has zero entries / weight */
    HISTO_ERR_DIV_BY_ZERO     = -7,  /* Division by zero */
    HISTO_ERR_SERIALIZATION   = -8,  /* Serialization encoding failed / buffer too small */
    HISTO_ERR_DESERIALIZATION = -9   /* Deserialization format corrupted or unsupported */
} histo_status_t;

/* Binning models */
typedef enum histo_bin_type {
    HISTO_BIN_UNIFORM  = 0,  /* Uniform / fixed-width equidistant bins */
    HISTO_BIN_VARIABLE = 1   /* Variable / non-uniform arbitrary bins */
} histo_bin_type_t;

/* Feature & storage flags */
typedef enum histo_flags {
    HISTO_FLAG_NONE          = 0,
    HISTO_FLAG_TRACK_SUMW2   = (1u << 0),  /* Track sum of weights squared (sum w^2) per bin */
    HISTO_FLAG_EXACT_MOMENTS = (1u << 1)   /* Track exact running mean/variance during fill */
} histo_flags_t;

/* Summary statistics container */
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

#ifdef __cplusplus
}
#endif

#endif /* LIBHISTO_TYPES_H */
