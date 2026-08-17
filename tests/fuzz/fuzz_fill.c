#include "histo/histo.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_FUZZ_BINS 1024
#define MAX_BATCH_SAMPLES 512

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size < 16) {
        return 0;
    }

    /* Configuration header extracted from first bytes */
    uint8_t mode_byte = data[0];
    uint8_t flags_byte = data[1];
    uint16_t raw_nbins;
    memcpy(&raw_nbins, data + 2, sizeof(uint16_t));

    uint32_t nbins = (raw_nbins % MAX_FUZZ_BINS) + 1;
    uint32_t flags = HISTO_FLAG_NONE;
    if (flags_byte & 0x01) flags |= HISTO_FLAG_TRACK_SUMW2;
    if (flags_byte & 0x02) flags |= HISTO_FLAG_EXACT_MOMENTS;

    bool is_variable = (mode_byte & 0x01) != 0;

    histo_t *h = NULL;
    if (!is_variable) {
        double range_min = -100.0;
        double range_max = 100.0;
        if (size >= 32) {
            memcpy(&range_min, data + 4, sizeof(double));
            memcpy(&range_max, data + 12, sizeof(double));
        }
        if (!isfinite(range_min) || !isfinite(range_max) || range_min >= range_max) {
            range_min = 0.0;
            range_max = 100.0;
        }
        h = histo_create_uniform(nbins, range_min, range_max, flags);
    } else {
        /* Construct strictly monotonic variable edges */
        double *edges = (double *)malloc((nbins + 1) * sizeof(double));
        if (!edges) return 0;
        edges[0] = 0.0;
        for (uint32_t i = 1; i <= nbins; i++) {
            edges[i] = edges[i - 1] + (double)i * 1.5;
        }
        h = histo_create_variable(nbins, edges, flags);
        free(edges);
    }

    if (!h) {
        return 0;
    }

    /* Extract double values from stream starting at offset 20 */
    size_t offset = 20;
    if (offset > size) offset = size;
    size_t remaining = size - offset;
    size_t num_doubles = remaining / sizeof(double);

    if (num_doubles > 0) {
        double *samples = (double *)malloc(num_doubles * sizeof(double));
        if (samples) {
            memcpy(samples, data + offset, num_doubles * sizeof(double));

            /* 1. Scalar fills with weights */
            for (size_t i = 0; i < num_doubles; i++) {
                double x = samples[i];
                double w = (i + 1 < num_doubles) ? samples[i + 1] : 1.0;
                
                /* Ingestion with IEEE-754 special values (NaN, sNaN, Inf, subnormals) */
                histo_fill(h, x);
                histo_fill_w(h, x, w);

                int64_t bin_idx = 0;
                histo_find_bin(h, x, &bin_idx);
            }

            /* 2. Direct bin fill */
            if (num_doubles >= 1) {
                histo_fill_bin(h, 0, samples[0]);
                histo_fill_bin(h, nbins - 1, samples[0]);
            }

            /* 3. Batch fills */
            size_t batch_size = num_doubles > MAX_BATCH_SAMPLES ? MAX_BATCH_SAMPLES : num_doubles;
            histo_fill_n(h, batch_size, samples, NULL);
            if (batch_size >= 2) {
                histo_fill_n(h, batch_size / 2, samples, samples + (batch_size / 2));
            }

            /* 4. Strided fills */
            if (num_doubles >= 4) {
                histo_fill_strided(h, num_doubles / 2, samples, 2 * sizeof(double), NULL, 0);
            }

            free(samples);
        }
    }

    /* 5. Exercise moment calculations and statistics post-ingestion */
    double mean = 0.0, var = 0.0, stdev = 0.0, skew = 0.0, kurt = 0.0;
    histo_mean(h, &mean);
    histo_variance(h, &var);
    histo_std_dev(h, &stdev);
    histo_skewness(h, &skew);
    histo_kurtosis(h, &kurt);
    double total_w = histo_total_weight(h);
    uint64_t n_entries = histo_num_entries(h);
    (void)mean; (void)var; (void)stdev; (void)skew; (void)kurt;
    (void)total_w; (void)n_entries;

    double q_val = 0.0;
    histo_quantile(h, 0.5, &q_val);

    /* 6. Clone & Reset lifecycle */
    histo_t *clone = histo_clone(h, false);
    if (clone) {
        histo_destroy(clone);
    }
    histo_reset(h);

    histo_destroy(h);
    return 0;
}
