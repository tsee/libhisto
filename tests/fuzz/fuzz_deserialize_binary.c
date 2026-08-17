#include "histo/histo.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size == 0) {
        return 0;
    }

    /* ===================================================================== */
    /* 1. Direct Binary Deserialization Fuzzing                              */
    /* ===================================================================== */
    histo_t *h = NULL;
    histo_status_t st = histo_deserialize_binary(data, size, &h);
    if (st == HISTO_OK && h != NULL) {
        /* Exercise query and inspection APIs */
        uint32_t nbins = histo_nbins(h);
        double total_w = histo_total_weight(h);
        uint64_t n_fills = histo_num_entries(h);
        double min_v = 0.0, max_v = 0.0;
        histo_range(h, &min_v, &max_v);

        double mean_v = 0.0, var_v = 0.0, stdev_v = 0.0;
        double skew_v = 0.0, kurt_v = 0.0;
        histo_mean(h, &mean_v);
        histo_variance(h, &var_v);
        histo_std_dev(h, &stdev_v);
        histo_skewness(h, &skew_v);
        histo_kurtosis(h, &kurt_v);

        (void)nbins; (void)total_w; (void)n_fills;
        (void)min_v; (void)max_v; (void)mean_v; (void)var_v;
        (void)stdev_v; (void)skew_v; (void)kurt_v;

        if (nbins > 0) {
            double c = 0.0, err = 0.0, s2 = 0.0, center = 0.0, lower = 0.0, upper = 0.0;
            histo_bin_content(h, 0, &c);
            histo_bin_error(h, 0, &err);
            histo_bin_sum_w2(h, 0, &s2);
            histo_bin_center(h, 0, &center);
            histo_bin_bounds(h, 0, &lower, &upper);

            histo_bin_content(h, nbins - 1, &c);
            histo_bin_error(h, nbins - 1, &err);

            double integ = 0.0;
            histo_integral(h, 0, nbins - 1, &integ);
        }

        double q_val = 0.0;
        histo_quantile(h, 0.5, &q_val);

        /* Clone & Reset */
        histo_t *clone = histo_clone(h, false);
        if (clone) {
            histo_destroy(clone);
        }
        histo_t *empty_clone = histo_clone(h, true);
        if (empty_clone) {
            histo_destroy(empty_clone);
        }

        /* Re-serialize to binary and verify roundtrip */
        void *ser_buf = NULL;
        size_t ser_sz = 0;
        if (histo_serialize_binary(h, &ser_buf, &ser_sz) == HISTO_OK && ser_buf) {
            histo_t *h2 = NULL;
            if (histo_deserialize_binary(ser_buf, ser_sz, &h2) == HISTO_OK && h2) {
                histo_destroy(h2);
            }
            histo_free_buffer(ser_buf);
        }

        /* Test static buffer serialization */
        size_t needed_sz = histo_serialize_binary_size(h);
        if (needed_sz > 0 && needed_sz <= (1024 * 1024)) {
            void *static_buf = malloc(needed_sz);
            if (static_buf) {
                if (histo_serialize_binary_into(h, static_buf, needed_sz) == HISTO_OK) {
                    histo_t *h3 = NULL;
                    if (histo_deserialize_binary(static_buf, needed_sz, &h3) == HISTO_OK && h3) {
                        histo_destroy(h3);
                    }
                }
                free(static_buf);
            }
        }

        histo_destroy(h);
    }

    /* ===================================================================== */
    /* 2. Format Migration Fuzzing                                           */
    /* ===================================================================== */
    void *migrated_buf = NULL;
    size_t migrated_size = 0;
    st = histo_migrate_binary(data, size, &migrated_buf, &migrated_size);
    if (st == HISTO_OK && migrated_buf != NULL) {
        histo_t *h_migrated = NULL;
        if (histo_deserialize_binary(migrated_buf, migrated_size, &h_migrated) == HISTO_OK && h_migrated) {
            histo_destroy(h_migrated);
        }
        histo_free_buffer(migrated_buf);
    }

    return 0;
}
