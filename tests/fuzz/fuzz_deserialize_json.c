/*
 * LibFuzzer target for 1D histogram JSON deserialization robustness.
 */

#include "histo/histo.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size == 0) {
        return 0;
    }

    /* Create null-terminated string for JSON parser */
    char *json_str = (char *)malloc(size + 1);
    if (!json_str) {
        return 0;
    }
    memcpy(json_str, data, size);
    json_str[size] = '\0';

    histo_t *h = NULL;
    histo_status_t st = histo_deserialize_json(json_str, &h);
    free(json_str);

    if (st == HISTO_OK && h != NULL) {
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

        /* Clone */
        histo_t *clone = histo_clone(h, false);
        if (clone) {
            histo_destroy(clone);
        }

        /* Serialize back to JSON */
        char *out_json = NULL;
        if (histo_serialize_json(h, &out_json) == HISTO_OK && out_json) {
            /* Roundtrip deserialize */
            histo_t *h_rt = NULL;
            if (histo_deserialize_json(out_json, &h_rt) == HISTO_OK && h_rt) {
                histo_destroy(h_rt);
            }
            histo_free_buffer(out_json);
        }

        histo_destroy(h);
    }

    return 0;
}
