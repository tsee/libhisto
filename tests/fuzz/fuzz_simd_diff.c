/*
 * LibFuzzer target for differential testing: SIMD vectorized vs scalar parity.
 */

#include "histo/histo.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_SAMPLES 512

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size < 16) {
        return 0;
    }

    uint8_t align_offset = (uint8_t)(data[0] % 8);
    uint32_t nbins = (uint32_t)((data[1] % 64) + 1);

    double rmin = -50.0;
    double rmax = 50.0;
    if (size >= 32) {
        memcpy(&rmin, data + 2, sizeof(double));
        memcpy(&rmax, data + 10, sizeof(double));
    }
    if (!isfinite(rmin) || !isfinite(rmax) || rmin >= rmax) {
        rmin = -50.0;
        rmax = 50.0;
    }

    const uint8_t *payload = data + 18;
    size_t payload_len = (size > 18) ? (size - 18) : 0;
    size_t n_samples = payload_len / sizeof(double);
    if (n_samples > MAX_SAMPLES) {
        n_samples = MAX_SAMPLES;
    }
    if (n_samples == 0) {
        return 0;
    }

    /* Allocate buffer with explicit unaligned byte offset */
    size_t alloc_sz = (n_samples * sizeof(double)) + 16;
    uint8_t *raw_buf = (uint8_t *)malloc(alloc_sz);
    if (!raw_buf) return 0;

    double *samples = (double *)(raw_buf + align_offset);
    memcpy(samples, payload, n_samples * sizeof(double));

    histo_t *h_simd = histo_create_uniform(nbins, rmin, rmax, HISTO_FLAG_NONE);
    histo_t *h_scalar = histo_create_uniform(nbins, rmin, rmax, HISTO_FLAG_NONE);

    if (!h_simd || !h_scalar) {
        if (h_simd) histo_destroy(h_simd);
        if (h_scalar) histo_destroy(h_scalar);
        free(raw_buf);
        return 0;
    }

    /* 1. Ingest via vectorized batch API */
    histo_fill_n(h_simd, n_samples, samples, NULL);

    /* 2. Ingest via strict scalar loop */
    for (size_t i = 0; i < n_samples; i++) {
        histo_fill(h_scalar, samples[i]);
    }

    /* 3. Differential assertion: results must be bit-for-bit identical */
    if (histo_num_entries(h_simd) != histo_num_entries(h_scalar) ||
        histo_nan_count(h_simd) != histo_nan_count(h_scalar) ||
        histo_underflow(h_simd) != histo_underflow(h_scalar) ||
        histo_overflow(h_simd) != histo_overflow(h_scalar) ||
        histo_total_weight(h_simd) != histo_total_weight(h_scalar)) {
        abort();
    }

    for (uint32_t b = 0; b < nbins; b++) {
        double c_simd = 0.0, c_scalar = 0.0;
        histo_bin_content(h_simd, b, &c_simd);
        histo_bin_content(h_scalar, b, &c_scalar);
        if (c_simd != c_scalar) {
            abort();
        }
    }

    histo_destroy(h_simd);
    histo_destroy(h_scalar);
    free(raw_buf);
    return 0;
}
