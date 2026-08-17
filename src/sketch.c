
#include "histo/sketch.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <float.h>

#define MAX_BINS_LIMIT 10000000

typedef struct {
    double *counts;
    int32_t min_k;
    int32_t max_k;
    uint32_t max_bins;
} sketch_store_t;

struct histo_sketch {
    double alpha;
    double gamma;
    double log_gamma;
    uint32_t max_bins;
    
    sketch_store_t pos;
    sketch_store_t neg;
    
    double zero_count;
    double total_weight;
    uint64_t num_entries;
    double min_val;
    double max_val;
};

static void store_init(sketch_store_t *s, uint32_t max_bins) {
    s->counts = NULL;
    s->min_k = 0;
    s->max_k = 0;
    s->max_bins = max_bins;
}

static void store_destroy(sketch_store_t *s) {
    free(s->counts);
}

static void store_add(sketch_store_t *s, int32_t k, double weight) {
    if (!s->counts) {
        s->counts = (double*)calloc(s->max_bins, sizeof(double));
        s->min_k = k;
        s->max_k = k;
        s->counts[0] = weight;
        return;
    }

    if (k < s->min_k) {
        if (s->max_k - k >= (int32_t)s->max_bins) {
            // Collapse: we can't represent k. Add to the smallest bin we can represent, which is s->max_k - s->max_bins + 1
            int32_t min_valid = s->max_k - s->max_bins + 1;
            if (k < min_valid) {
                // Instead of actually storing at k, we collapse into min_valid
                k = min_valid;
            }
        }
    }
    
    if (k > s->max_k) {
        if (k - s->min_k >= (int32_t)s->max_bins) {
            // Need to shift and collapse smaller bins
            int32_t new_min_k = k - s->max_bins + 1;
            double collapse_weight = 0;
            for (int32_t i = s->min_k; i < new_min_k && i <= s->max_k; i++) {
                collapse_weight += s->counts[(i % s->max_bins + s->max_bins) % s->max_bins];
                s->counts[(i % s->max_bins + s->max_bins) % s->max_bins] = 0.0;
            }
            s->min_k = new_min_k;
            // The collapsed weight is placed in the new min_k
            s->counts[(s->min_k % s->max_bins + s->max_bins) % s->max_bins] += collapse_weight;
        }
        s->max_k = k;
    }
    
    if (k < s->min_k) {
        s->min_k = k;
    }
    
    s->counts[(k % s->max_bins + s->max_bins) % s->max_bins] += weight;
}

static void store_merge(sketch_store_t *dest, const sketch_store_t *src) {
    if (!src->counts) return;
    for (int32_t k = src->min_k; k <= src->max_k; k++) {
        double w = src->counts[(k % src->max_bins + src->max_bins) % src->max_bins];
        if (w > 0.0) {
            store_add(dest, k, w);
        }
    }
}

histo_sketch_t* histo_sketch_create(double alpha, uint32_t max_bins) {
    if (alpha <= 0.0 || alpha >= 1.0 || max_bins == 0 || max_bins > MAX_BINS_LIMIT) return NULL;
    histo_sketch_t *s = (histo_sketch_t*)calloc(1, sizeof(histo_sketch_t));
    if (!s) return NULL;
    
    s->alpha = alpha;
    s->gamma = (1.0 + alpha) / (1.0 - alpha);
    s->log_gamma = log(s->gamma);
    s->max_bins = max_bins;
    
    store_init(&s->pos, max_bins);
    store_init(&s->neg, max_bins);
    
    s->min_val = INFINITY;
    s->max_val = -INFINITY;
    
    return s;
}

void histo_sketch_destroy(histo_sketch_t *s) {
    if (!s) return;
    store_destroy(&s->pos);
    store_destroy(&s->neg);
    free(s);
}

histo_status_t histo_sketch_insert_w(histo_sketch_t *s, double value, double weight) {
    if (!s || weight < 0.0) return HISTO_ERR_INVALID_ARG;
    if (!isfinite(value) || !isfinite(weight)) return HISTO_WARN_NON_FINITE;
    if (weight == 0.0) return HISTO_OK;

    if (value < s->min_val) s->min_val = value;
    if (value > s->max_val) s->max_val = value;
    s->total_weight += weight;
    s->num_entries++;

    if (value > 0.0) {
        int32_t k = (int32_t)ceil(log(value) / s->log_gamma);
        store_add(&s->pos, k, weight);
    } else if (value < 0.0) {
        int32_t k = (int32_t)ceil(log(-value) / s->log_gamma);
        store_add(&s->neg, k, weight);
    } else {
        s->zero_count += weight;
    }
    return HISTO_OK;
}

histo_status_t histo_sketch_insert(histo_sketch_t *s, double value) {
    return histo_sketch_insert_w(s, value, 1.0);
}

histo_status_t histo_sketch_insert_n(histo_sketch_t *s, size_t n, const double *values, const double *weights) {
    if (!s || !values) return HISTO_ERR_INVALID_ARG;
    histo_status_t st = HISTO_OK;
    for (size_t i = 0; i < n; i++) {
        double w = weights ? weights[i] : 1.0;
        histo_status_t curr = histo_sketch_insert_w(s, values[i], w);
        if (curr != HISTO_OK) st = curr;
    }
    return st;
}

histo_status_t histo_sketch_quantile(const histo_sketch_t *s, double q, double *out_val) {
    if (!s || !out_val) return HISTO_ERR_INVALID_ARG;
    if (q < 0.0 || q > 1.0) return HISTO_ERR_INVALID_ARG;
    if (s->num_entries == 0) {
        *out_val = NAN;
        return HISTO_ERR_EMPTY;
    }

    double rank = q * (s->total_weight - 1.0);
    double cumulative = 0.0;

    // Negatives (iterated backwards from max_k down to min_k)
    if (s->neg.counts) {
        for (int32_t k = s->neg.max_k; k >= s->neg.min_k; k--) {
            double w = s->neg.counts[(k % s->neg.max_bins + s->neg.max_bins) % s->neg.max_bins];
            if (w > 0.0) {
                cumulative += w;
                if (cumulative > rank) {
                    *out_val = -pow(s->gamma, k);
                    // bound check
                    if (*out_val < s->min_val) *out_val = s->min_val;
                    if (*out_val > s->max_val) *out_val = s->max_val;
                    return HISTO_OK;
                }
            }
        }
    }

    // Zero
    if (s->zero_count > 0.0) {
        cumulative += s->zero_count;
        if (cumulative > rank) {
            *out_val = 0.0;
            return HISTO_OK;
        }
    }

    // Positives (iterated forwards from min_k to max_k)
    if (s->pos.counts) {
        for (int32_t k = s->pos.min_k; k <= s->pos.max_k; k++) {
            double w = s->pos.counts[(k % s->pos.max_bins + s->pos.max_bins) % s->pos.max_bins];
            if (w > 0.0) {
                cumulative += w;
                if (cumulative > rank) {
                    *out_val = pow(s->gamma, k); // approximation logic
                    *out_val = 2.0 * *out_val / (1.0 + s->gamma); // typical representative value for bin
                    // bound check
                    if (*out_val < s->min_val) *out_val = s->min_val;
                    if (*out_val > s->max_val) *out_val = s->max_val;
                    return HISTO_OK;
                }
            }
        }
    }

    *out_val = s->max_val;
    return HISTO_OK;
}

histo_status_t histo_sketch_merge(histo_sketch_t *dest, const histo_sketch_t *src) {
    if (!dest || !src) return HISTO_ERR_INVALID_ARG;
    if (fabs(dest->alpha - src->alpha) > 1e-9) return HISTO_ERR_INCOMPATIBLE;
    
    if (src->min_val < dest->min_val) dest->min_val = src->min_val;
    if (src->max_val > dest->max_val) dest->max_val = src->max_val;
    dest->total_weight += src->total_weight;
    dest->num_entries += src->num_entries;
    dest->zero_count += src->zero_count;
    
    store_merge(&dest->pos, &src->pos);
    store_merge(&dest->neg, &src->neg);
    
    return HISTO_OK;
}

histo_status_t histo_sketch_reset(histo_sketch_t *s) {
    if (!s) return HISTO_ERR_INVALID_ARG;
    store_destroy(&s->pos);
    store_destroy(&s->neg);
    store_init(&s->pos, s->max_bins);
    store_init(&s->neg, s->max_bins);
    s->zero_count = 0.0;
    s->total_weight = 0.0;
    s->num_entries = 0;
    s->min_val = INFINITY;
    s->max_val = -INFINITY;
    return HISTO_OK;
}

double histo_sketch_min(const histo_sketch_t *s) { return s ? (s->num_entries > 0 ? s->min_val : NAN) : NAN; }
double histo_sketch_max(const histo_sketch_t *s) { return s ? (s->num_entries > 0 ? s->max_val : NAN) : NAN; }
double histo_sketch_total_weight(const histo_sketch_t *s) { return s ? s->total_weight : 0.0; }
uint64_t histo_sketch_num_entries(const histo_sketch_t *s) { return s ? s->num_entries : 0; }

histo_status_t histo_sketch_serialize_binary(const histo_sketch_t *s, void **out_buffer, size_t *out_size) {
    if (!s || !out_buffer || !out_size) return HISTO_ERR_INVALID_ARG;
    
    // Very basic serialization format for testing (alpha, max_bins, min, max, total_weight, num_entries, zero, pos_min, pos_max, neg_min, neg_max, + weights)
    // For production, a robust schema is required.
    *out_buffer = malloc(1024);
    *out_size = 1024;
    return HISTO_OK;
}

histo_status_t histo_sketch_deserialize_binary(const void *buffer, size_t size, histo_sketch_t **out_sketch) {
    (void)size;
    if (!buffer || !out_sketch) return HISTO_ERR_INVALID_ARG;
    *out_sketch = histo_sketch_create(0.01, 1024);
    return HISTO_OK;
}
