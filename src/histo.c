#include "internal.h"

/* ========================================================================= */
/* Status & Error Strings                                                    */
/* ========================================================================= */

const char* histo_status_str(histo_status_t status) {
    switch (status) {
        case HISTO_WARN_NON_FINITE:
            return "Non-finite sample(s) skipped";
        case HISTO_OK:
            return "Success";
        case HISTO_ERR_INVALID_ARG:
            return "Invalid argument";
        case HISTO_ERR_NOMEM:
            return "Out of memory";
        case HISTO_ERR_INCOMPATIBLE:
            return "Incompatible histogram geometry";
        case HISTO_ERR_OUT_OF_RANGE:
            return "Index or range out of bounds";
        case HISTO_ERR_NON_FINITE:
            return "Non-finite floating-point value";
        case HISTO_ERR_EMPTY:
            return "Histogram is empty";
        case HISTO_ERR_DIV_BY_ZERO:
            return "Division by zero";
        case HISTO_ERR_SERIALIZATION:
            return "Serialization failed";
        case HISTO_ERR_DESERIALIZATION:
            return "Deserialization failed";
        default:
            return "Unknown status";
    }
}

/* ========================================================================= */
/* Lifecycle & Memory Management                                             */
/* ========================================================================= */

histo_t* histo_create_uniform(uint32_t nbins, double min, double max, uint32_t flags) {
    if (nbins == 0 || nbins > HISTO_MAX_NBINS) {
        return NULL;
    }
    if (!isfinite(min) || !isfinite(max) || min >= max || !isfinite(max - min)) {
        return NULL;
    }

    histo_t *h = (histo_t *)calloc(1, sizeof(histo_t));
    if (!h) {
        return NULL;
    }

    h->bin_type = HISTO_BIN_UNIFORM;
    h->flags = flags;
    h->nbins = nbins;
    h->reserved_pad = 0;
    h->min = min;
    h->max = max;
    h->width = max - min;
    h->binsize = h->width / (double)nbins;
    h->inv_binsize = (double)nbins / h->width;
    h->bin_edges = NULL;

    h->bins = (double *)calloc(nbins, sizeof(double));
    if (!h->bins) {
        free(h);
        return NULL;
    }

    if (flags & HISTO_FLAG_TRACK_SUMW2) {
        h->sum_w2 = (double *)calloc(nbins, sizeof(double));
        if (!h->sum_w2) {
            free(h->bins);
            free(h);
            return NULL;
        }
    } else {
        h->sum_w2 = NULL;
    }

    h->stats_min = INFINITY;
    h->stats_max = -INFINITY;

    return h;
}

histo_t* histo_create_variable(uint32_t nbins, const double *edges, uint32_t flags) {
    if (nbins == 0 || nbins > HISTO_MAX_NBINS || !edges) {
        return NULL;
    }

    /* Verify strict monotonicity of edges: edges[0] < edges[1] < ... < edges[nbins] */
    for (uint32_t i = 0; i < nbins; ++i) {
        if (!isfinite(edges[i]) || !isfinite(edges[i + 1]) || edges[i] >= edges[i + 1]) {
            return NULL;
        }
    }
    if (!isfinite(edges[nbins] - edges[0])) {
        return NULL;
    }

    histo_t *h = (histo_t *)calloc(1, sizeof(histo_t));
    if (!h) {
        return NULL;
    }

    h->bin_type = HISTO_BIN_VARIABLE;
    h->flags = flags;
    h->nbins = nbins;
    h->reserved_pad = 0;
    h->min = edges[0];
    h->max = edges[nbins];
    h->width = h->max - h->min;
    h->binsize = 0.0;
    h->inv_binsize = 0.0;

    h->bin_edges = (double *)malloc((nbins + 1) * sizeof(double));
    if (!h->bin_edges) {
        free(h);
        return NULL;
    }
    memcpy(h->bin_edges, edges, (nbins + 1) * sizeof(double));

    h->bins = (double *)calloc(nbins, sizeof(double));
    if (!h->bins) {
        free(h->bin_edges);
        free(h);
        return NULL;
    }

    if (flags & HISTO_FLAG_TRACK_SUMW2) {
        h->sum_w2 = (double *)calloc(nbins, sizeof(double));
        if (!h->sum_w2) {
            free(h->bins);
            free(h->bin_edges);
            free(h);
            return NULL;
        }
    } else {
        h->sum_w2 = NULL;
    }

    h->stats_min = INFINITY;
    h->stats_max = -INFINITY;

    return h;
}

void histo_destroy(histo_t *h) {
    if (!h) {
        return;
    }
    if (h->bins) {
        free(h->bins);
    }
    if (h->sum_w2) {
        free(h->sum_w2);
    }
    if (h->bin_edges) {
        free(h->bin_edges);
    }
    free(h);
}

histo_t* histo_clone(const histo_t *src, bool empty) {
    if (!src) {
        return NULL;
    }

    histo_t *dst = NULL;
    if (src->bin_type == HISTO_BIN_UNIFORM) {
        dst = histo_create_uniform(src->nbins, src->min, src->max, src->flags);
    } else {
        dst = histo_create_variable(src->nbins, src->bin_edges, src->flags);
    }

    if (!dst) {
        return NULL;
    }

    if (!empty) {
        memcpy(dst->bins, src->bins, src->nbins * sizeof(double));
        if (src->sum_w2 && dst->sum_w2) {
            memcpy(dst->sum_w2, src->sum_w2, src->nbins * sizeof(double));
        }
        dst->underflow_weight = src->underflow_weight;
        dst->overflow_weight = src->overflow_weight;
        dst->underflow_sum_w2 = src->underflow_sum_w2;
        dst->overflow_sum_w2 = src->overflow_sum_w2;
        dst->n_underflow = src->n_underflow;
        dst->n_overflow = src->n_overflow;
        dst->n_nan = src->n_nan;
        dst->n_fills = src->n_fills;
        dst->total_weight = src->total_weight;
        dst->total_sum_w2 = src->total_sum_w2;
        dst->stats_min = src->stats_min;
        dst->stats_max = src->stats_max;
        dst->stats_mean = src->stats_mean;
        dst->stats_M2 = src->stats_M2;
    }

    return dst;
}

histo_status_t histo_reset(histo_t *h) {
    if (!h) {
        return HISTO_ERR_INVALID_ARG;
    }

    if (h->bins) {
        memset(h->bins, 0, h->nbins * sizeof(double));
    }
    if (h->sum_w2) {
        memset(h->sum_w2, 0, h->nbins * sizeof(double));
    }

    h->underflow_weight = 0.0;
    h->overflow_weight = 0.0;
    h->underflow_sum_w2 = 0.0;
    h->overflow_sum_w2 = 0.0;
    h->n_underflow = 0;
    h->n_overflow = 0;
    h->n_nan = 0;
    h->n_fills = 0;
    h->total_weight = 0.0;
    h->total_sum_w2 = 0.0;
    h->stats_min = INFINITY;
    h->stats_max = -INFINITY;
    h->stats_mean = 0.0;
    h->stats_M2 = 0.0;

    return HISTO_OK;
}

/* ========================================================================= */
/* Geometry & Property Queries                                               */
/* ========================================================================= */

uint32_t histo_nbins(const histo_t *h) {
    return h ? h->nbins : 0;
}

histo_bin_type_t histo_bin_type(const histo_t *h) {
    return h ? h->bin_type : HISTO_BIN_UNIFORM;
}

histo_status_t histo_range(const histo_t *h, double *out_min, double *out_max) {
    if (!h || !out_min || !out_max) {
        return HISTO_ERR_INVALID_ARG;
    }
    *out_min = h->min;
    *out_max = h->max;
    return HISTO_OK;
}

double histo_underflow(const histo_t *h) {
    return h ? h->underflow_weight : 0.0;
}

double histo_overflow(const histo_t *h) {
    return h ? h->overflow_weight : 0.0;
}

uint64_t histo_nan_count(const histo_t *h) {
    return h ? h->n_nan : 0;
}

double histo_total_weight(const histo_t *h) {
    return h ? h->total_weight : 0.0;
}

uint64_t histo_num_entries(const histo_t *h) {
    return h ? h->n_fills : 0;
}

void histo_free_buffer(void *buf) {
    if (buf) {
        free(buf);
    }
}

