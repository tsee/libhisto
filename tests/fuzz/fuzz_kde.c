/*
 * LibFuzzer target for 1D Kernel Density Estimation and bandwidth heuristics.
 */

#include "histo/histo.h"
#include "histo/kde.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_KDE_SAMPLES 256

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size < 16) {
        return 0;
    }

    /* Extract options from header bytes */
    histo_kde_options_t opts = histo_kde_default_options();
    opts.kernel = (histo_kde_kernel_t)(data[0] % 6);
    opts.bw_method = (histo_kde_bandwidth_method_t)(data[1] % 3);

    double bw_param = 1.0;
    memcpy(&bw_param, data + 2, sizeof(double));
    if (isfinite(bw_param) && bw_param > 1e-6 && bw_param < 1e6) {
        opts.bandwidth = bw_param;
    } else {
        opts.bandwidth = 1.0;
    }

    const uint8_t *payload = data + 10;
    size_t payload_len = size - 10;
    size_t n_samples = payload_len / sizeof(double);
    if (n_samples > MAX_KDE_SAMPLES) {
        n_samples = MAX_KDE_SAMPLES;
    }
    if (n_samples < 2) {
        return 0;
    }

    const double *samples = (const double *)payload;

    /* 1. Direct sample KDE construction */
    histo_kde_t *kde = histo_kde_create(n_samples, samples, NULL, &opts);
    if (kde) {
        double eval_pt = samples[0];
        double d = histo_kde_eval(kde, eval_pt);
        double c = histo_kde_cdf(kde, eval_pt);
        (void)d;
        (void)c;

        /* Evaluate small grid */
        double grid_x[8];
        double grid_y[8];
        for (int i = 0; i < 8; i++) {
            grid_x[i] = eval_pt + (double)i * 0.25;
        }
        (void)histo_kde_eval_n(kde, 8, grid_x, grid_y);

        histo_kde_destroy(kde);
    }

    /* 2. Histogram-backed KDE */
    histo_t *h = histo_create_uniform(20, -50.0, 50.0, HISTO_FLAG_NONE);
    if (h) {
        for (size_t i = 0; i < n_samples; i++) {
            if (isfinite(samples[i])) {
                histo_fill(h, samples[i]);
            }
        }
        histo_kde_t *kde_h = histo_kde_create_from_histo(h, &opts);
        if (kde_h) {
            double dens = histo_kde_eval(kde_h, 0.0);
            (void)dens;
            histo_kde_destroy(kde_h);
        }
        histo_destroy(h);
    }

    return 0;
}
