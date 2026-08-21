#include "histo/histo.h"
#include "histo/fit.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 10) return 0;
    
    uint8_t fit_type = data[0] % 11;
    
    histo_t *h = histo_create_uniform(100, -50.0, 50.0, HISTO_FLAG_TRACK_SUMW2);
    if (!h) return 0;
    
    size_t n_doubles = (size - 1) / sizeof(double);
    const double *dvals = (const double *)(data + 1);
    for (size_t i = 0; i < n_doubles; i++) {
        histo_fill(h, dvals[i]);
    }
    
    histo_fit_result_t *res = NULL;
    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    
    switch (fit_type) {
        case 0:
            histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, NULL, &opts, &res);
            break;
        case 1:
            histo_fit_model(h, HISTO_FIT_MODEL_EXPONENTIAL, NULL, &opts, &res);
            break;
        case 2:
            opts.poly_degree = 2;
            histo_fit_model(h, HISTO_FIT_MODEL_POLYNOMIAL, NULL, &opts, &res);
            break;
        case 3:
            histo_fit_model(h, HISTO_FIT_MODEL_BREIT_WIGNER, NULL, &opts, &res);
            break;
        case 4:
            histo_fit_model(h, HISTO_FIT_MODEL_POWER_LAW, NULL, &opts, &res);
            break;
        case 5:
            histo_fit_model(h, HISTO_FIT_MODEL_LOG_NORMAL, NULL, &opts, &res);
            break;
        case 6:
            histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN_PLUS_LINEAR, NULL, &opts, &res);
            break;
        case 7:
            histo_fit_model(h, HISTO_FIT_MODEL_WEIBULL, NULL, &opts, &res);
            break;
        case 8:
            histo_fit_model(h, HISTO_FIT_MODEL_GAMMA, NULL, &opts, &res);
            break;
        case 9:
            histo_fit_model(h, HISTO_FIT_MODEL_POISSON, NULL, &opts, &res);
            break;
        case 10:
            histo_fit_model(h, HISTO_FIT_MODEL_LAPLACE, NULL, &opts, &res);
            break;
    }
    
    if (res) histo_fit_result_destroy(res);
    histo_destroy(h);
    return 0;
}
