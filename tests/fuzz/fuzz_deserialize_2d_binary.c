#include "histo/histo2d.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size == 0) {
        return 0;
    }

    histo2d_t *h = NULL;
    histo_status_t st = histo2d_deserialize_binary(data, size, &h);
    if (st == HISTO_OK && h != NULL) {
        uint32_t nbins_x = histo2d_nbins_x(h);
        uint32_t nbins_y = histo2d_nbins_y(h);
        double total_w = histo2d_total_weight(h);
        uint64_t n_fills = histo2d_num_entries(h);

        double mean_x = 0.0, var_x = 0.0, mean_y = 0.0, var_y = 0.0, cov = 0.0, rho = 0.0;
        histo2d_mean_x(h, &mean_x);
        histo2d_variance_x(h, &var_x);
        histo2d_mean_y(h, &mean_y);
        histo2d_variance_y(h, &var_y);
        histo2d_covariance(h, &cov);
        histo2d_correlation(h, &rho);

        (void)nbins_x; (void)nbins_y; (void)total_w; (void)n_fills;
        (void)mean_x; (void)var_x; (void)mean_y; (void)var_y; (void)cov; (void)rho;

        if (nbins_x > 0 && nbins_y > 0) {
            double c = 0.0, err = 0.0, s2 = 0.0;
            histo2d_bin_content(h, 0, 0, &c);
            histo2d_bin_error(h, 0, 0, &err);
            histo2d_bin_sum_w2(h, 0, 0, &s2);
            histo2d_bin_content(h, nbins_x - 1, nbins_y - 1, &c);
        }

        histo2d_t *clone = histo2d_clone(h, false);
        if (clone) histo2d_destroy(clone);

        histo2d_t *empty_clone = histo2d_clone(h, true);
        if (empty_clone) histo2d_destroy(empty_clone);

        void *ser_buf = NULL;
        size_t ser_sz = 0;
        if (histo2d_serialize_binary_alloc(h, &ser_buf, &ser_sz) == HISTO_OK && ser_buf) {
            histo2d_t *h2 = NULL;
            if (histo2d_deserialize_binary(ser_buf, ser_sz, &h2) == HISTO_OK && h2) {
                histo2d_destroy(h2);
            }
            histo_free_buffer(ser_buf);
        }

        histo_t *proj_x = NULL;
        if (histo2d_project_x(h, &proj_x) == HISTO_OK && proj_x) histo_destroy(proj_x);

        histo_t *proj_y = NULL;
        if (histo2d_project_y(h, &proj_y) == HISTO_OK && proj_y) histo_destroy(proj_y);

        histo2d_destroy(h);
    }
    return 0;
}
