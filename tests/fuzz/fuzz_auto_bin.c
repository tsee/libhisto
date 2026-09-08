/*
 * LibFuzzer target for automated bin estimation rules on adversarial numerical data.
 */

#include "histo/histo.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_SAMPLES 512

typedef histo_status_t (*estimator_fn)(size_t, const double *, uint32_t *, double *, double *);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    size_t n = size / sizeof(double);
    if (n < 2) {
        return 0;
    }
    if (n > MAX_SAMPLES) {
        n = MAX_SAMPLES;
    }

    const double *values = (const double *)data;

    estimator_fn estimators[] = {
        histo_estimate_bins_fd,
        histo_estimate_bins_scott,
        histo_estimate_bins_sturges,
        histo_estimate_bins_doane,
        histo_estimate_bins_knuth
    };
    size_t num_est = sizeof(estimators) / sizeof(estimators[0]);

    for (size_t i = 0; i < num_est; i++) {
        uint32_t nbins = 0;
        double rmin = 0.0, rmax = 0.0;
        histo_status_t st = estimators[i](n, values, &nbins, &rmin, &rmax);

        if (st == HISTO_OK) {
            /* If estimation succeeded, invariants must hold */
            if (nbins == 0 || nbins > HISTO_MAX_NBINS || !isfinite(rmin) || !isfinite(rmax) || rmin >= rmax) {
                abort();
            }

            /* Exercise histogram creation with estimated parameters */
            histo_t *h = histo_create_uniform(nbins, rmin, rmax, HISTO_FLAG_NONE);
            if (h) {
                for (size_t k = 0; k < n; k++) {
                    if (isfinite(values[k])) {
                        histo_fill(h, values[k]);
                    }
                }
                histo_destroy(h);
            }
        }
    }

    return 0;
}
