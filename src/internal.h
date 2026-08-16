#ifndef LIBHISTO_INTERNAL_H
#define LIBHISTO_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "histo/histo.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Core histogram structure */
struct histo {
    /* --- Cache Line 0: Hot-Path Ingestion Variables (64 bytes) --- */
    double          *bins;           /* Array of (nbins) doubles: accumulated bin weights */
    double          *sum_w2;         /* Array of (nbins) doubles: accumulated sum of squared weights (NULL if disabled) */
    double           min;            /* Lower bound of bin 0 (finite) */
    double           max;            /* Upper bound of bin N-1 (max > min, finite) */
    double           inv_binsize;    /* Fast reciprocal (nbins / width) for 3-cycle FMUL lookup */
    double           total_weight;   /* Total in-range accumulated weight */
    uint64_t         n_fills;        /* Total in-range fill events */
    uint32_t         nbins;          /* Number of in-range bins (N >= 1) */
    histo_bin_type_t bin_type;       /* Uniform or Variable */

    /* --- Cache Line 1+: Remaining Geometry & Data --- */
    uint32_t         flags;          /* Configuration flags (HISTO_FLAG_*) */
    uint32_t         reserved_pad;   /* Explicit padding for strict 8-byte alignment */

    double           width;          /* Range width: (max - min) */
    double           binsize;        /* Width per bin: (width / nbins) for UNIFORM; 0.0 for VARIABLE */
    double          *bin_edges;      /* Pointer to (nbins + 1) strictly monotonic doubles for VARIABLE; NULL for UNIFORM */

    /* --- Out-of-Range & Exception Accumulators --- */
    double           underflow_weight;   /* Accumulated weight for x < min */
    double           overflow_weight;    /* Accumulated weight for x >= max */
    double           underflow_sum_w2;   /* Accumulated sum(w^2) for underflow */
    double           overflow_sum_w2;    /* Accumulated sum(w^2) for overflow */
    uint64_t         n_underflow;        /* Fill count for underflow */
    uint64_t         n_overflow;         /* Fill count for overflow */
    uint64_t         n_nan;              /* Count of rejected NaN / non-finite samples */

    /* --- In-Range Global Aggregates --- */
    double           total_sum_w2;       /* Total in-range accumulated sum of squared weights */

    /* --- Exact Online Sample Statistics (Updated if HISTO_FLAG_EXACT_MOMENTS is set) --- */
    double           stats_min;          /* Exact minimum sample value observed */
    double           stats_max;          /* Exact maximum sample value observed */
    double           stats_mean;         /* Online running mean (Welford) */
    double           stats_M2;           /* Online running sum of squared differences (Welford) */
};

/* Endian conversion helpers for canonical Little-Endian serialization */
static inline uint16_t histo_htole16(uint16_t val) {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return (uint16_t)((val << 8) | (val >> 8));
#else
    return val;
#endif
}

static inline uint32_t histo_htole32(uint32_t val) {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return (uint32_t)(((val & 0x000000FFu) << 24) |
                      ((val & 0x0000FF00u) << 8)  |
                      ((val & 0x00FF0000u) >> 8)  |
                      ((val & 0xFF000000u) >> 24));
#else
    return val;
#endif
}

static inline uint64_t histo_htole64(uint64_t val) {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return (((val & 0x00000000000000FFULL) << 56) |
            ((val & 0x000000000000FF00ULL) << 40) |
            ((val & 0x0000000000FF0000ULL) << 24) |
            ((val & 0x00000000FF000000ULL) << 8)  |
            ((val & 0x000000FF00000000ULL) >> 8)  |
            ((val & 0x0000FF0000000000ULL) >> 24) |
            ((val & 0x00FF000000000000ULL) >> 40) |
            ((val & 0xFF00000000000000ULL) >> 56));
#else
    return val;
#endif
}

static inline uint16_t histo_le16toh(uint16_t val) { return histo_htole16(val); }
static inline uint32_t histo_le32toh(uint32_t val) { return histo_htole32(val); }
static inline uint64_t histo_le64toh(uint64_t val) { return histo_htole64(val); }

static inline uint64_t histo_double_to_bits(double d) {
    uint64_t bits;
    memcpy(&bits, &d, sizeof(bits));
    return bits;
}

static inline double histo_bits_to_double(uint64_t bits) {
    double d;
    memcpy(&d, &bits, sizeof(d));
    return d;
}

static inline double histo_dtole(double d) {
    uint64_t bits = histo_htole64(histo_double_to_bits(d));
    return histo_bits_to_double(bits);
}

static inline double histo_letoh_d(double d) {
    uint64_t bits = histo_le64toh(histo_double_to_bits(d));
    return histo_bits_to_double(bits);
}

#ifdef __cplusplus
}
#endif

#endif /* LIBHISTO_INTERNAL_H */
