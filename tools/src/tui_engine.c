#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700

#include "tui_engine.h"
#include "cli_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

static double get_time_now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void reservoir_init(tui_reservoir_t *r, size_t cap, bool is_2d, bool has_weights) {
    if (!r) return;
    if (cap < 1000) cap = TUI_RESERVOIR_DEFAULT_CAP;
    r->samples = (double *)malloc(cap * sizeof(double));
    r->samples_y = is_2d ? (double *)malloc(cap * sizeof(double)) : NULL;
    r->weights = has_weights ? (double *)malloc(cap * sizeof(double)) : NULL;
    r->count = 0;
    r->cap = cap;
    r->head = 0;
    r->is_2d = is_2d;
    r->has_weights = has_weights;
}

static void reservoir_push(tui_reservoir_t *r, double x, double y, double w) {
    if (!r || !r->samples) return;
    if (r->count < r->cap) {
        r->samples[r->count] = x;
        if (r->samples_y) r->samples_y[r->count] = y;
        if (r->weights) r->weights[r->count] = w;
        r->count++;
    } else {
        /* Ring buffer overwrite */
        r->samples[r->head] = x;
        if (r->samples_y) r->samples_y[r->head] = y;
        if (r->weights) r->weights[r->head] = w;
        r->head = (r->head + 1) % r->cap;
    }
}

static void reservoir_free(tui_reservoir_t *r) {
    if (!r) return;
    if (r->samples) free(r->samples);
    if (r->samples_y) free(r->samples_y);
    if (r->weights) free(r->weights);
    r->samples = NULL;
    r->samples_y = NULL;
    r->weights = NULL;
    r->count = 0;
    r->cap = 0;
}

static bool is_engine_running(tui_engine_t *eng) {
    if (!eng) return false;
    histo_mutex_lock(&eng->mutex);
    bool r = eng->running;
    histo_mutex_unlock(&eng->mutex);
    return r;
}

static void *ingest_worker_thread(void *arg) {
    tui_engine_t *eng = (tui_engine_t *)arg;
    if (!eng || !eng->in_stream) return NULL;

    char line[4096];
    const size_t batch_max = 64;
    double batch_x[64];
    double batch_y[64];
    double batch_w[64];
    size_t batch_len = 0;

    double last_calc_time = get_time_now_sec();
    double last_flush_time = last_calc_time;
    uint64_t last_calc_samples = 0;

    while (is_engine_running(eng)) {
        if (!fgets(line, sizeof(line), eng->in_stream)) {
            histo_mutex_lock(&eng->mutex);
            if (batch_len > 0) {
                if (eng->is_2d && eng->live_2d) {
                    for (size_t i = 0; i < batch_len; ++i) {
                        histo2d_fill_w(eng->live_2d, batch_x[i], batch_y[i], batch_w[i]);
                    }
                } else if (eng->live_1d) {
                    if (eng->has_weights) {
                        histo_fill_n(eng->live_1d, batch_len, batch_x, batch_w);
                    } else {
                        histo_fill_n(eng->live_1d, batch_len, batch_x, NULL);
                    }
                }
                for (size_t i = 0; i < batch_len; ++i) {
                    reservoir_push(&eng->reservoir, batch_x[i], batch_y[i], batch_w[i]);
                }
                eng->total_samples += batch_len;
                batch_len = 0;
            }
            eng->finished_reading = true;
            histo_mutex_unlock(&eng->mutex);
            /* End of file / stream closed, idle sleep until app quits */
            usleep(20000);
            continue;
        }

        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '#') continue;

        char *endp = NULL;
        double val_x = strtod(p, &endp);
        if (endp == p) continue;

        double val_y = 0.0;
        double weight = 1.0;

        if (eng->is_2d) {
            /* 2D mode: parse second column as Y */
            while (*endp && (isspace((unsigned char)*endp) || *endp == ',' || *endp == '\t' || *endp == ';')) endp++;
            if (*endp == '\0') continue; /* need at least x y */
            char *y_end = NULL;
            val_y = strtod(endp, &y_end);
            if (y_end == endp) continue;
            endp = y_end;
        }

        if (eng->has_weights && *endp != '\0') {
            while (*endp && (isspace((unsigned char)*endp) || *endp == ',' || *endp == '\t' || *endp == ';')) endp++;
            if (*endp != '\0') {
                char *w_end = NULL;
                double w_val = strtod(endp, &w_end);
                if (w_end != endp) weight = w_val;
            }
        }

        batch_x[batch_len] = val_x;
        batch_y[batch_len] = val_y;
        batch_w[batch_len] = weight;
        batch_len++;

        double now = get_time_now_sec();
        if (batch_len >= batch_max || (batch_len > 0 && now - last_flush_time >= 0.05)) {
            histo_mutex_lock(&eng->mutex);
            if (eng->is_2d && eng->live_2d) {
                for (size_t i = 0; i < batch_len; ++i) {
                    histo2d_fill_w(eng->live_2d, batch_x[i], batch_y[i], batch_w[i]);
                }
            } else if (eng->live_1d) {
                if (eng->has_weights) {
                    histo_fill_n(eng->live_1d, batch_len, batch_x, batch_w);
                } else {
                    histo_fill_n(eng->live_1d, batch_len, batch_x, NULL);
                }
            }
            for (size_t i = 0; i < batch_len; ++i) {
                reservoir_push(&eng->reservoir, batch_x[i], batch_y[i], batch_w[i]);
            }
            eng->total_samples += batch_len;
            histo_mutex_unlock(&eng->mutex);
            batch_len = 0;
            last_flush_time = now;
        }

        /* Update rate calculation every 250ms */
        if (now - last_calc_time >= 0.25) {
            histo_mutex_lock(&eng->mutex);
            uint64_t diff = eng->total_samples - last_calc_samples;
            eng->current_rate_ops = (double)diff / (now - last_calc_time);
            last_calc_time = now;
            last_calc_samples = eng->total_samples;
            histo_mutex_unlock(&eng->mutex);
        }
    }

    if (batch_len > 0) {
        histo_mutex_lock(&eng->mutex);
        if (eng->is_2d && eng->live_2d) {
            for (size_t i = 0; i < batch_len; ++i) {
                histo2d_fill_w(eng->live_2d, batch_x[i], batch_y[i], batch_w[i]);
            }
        } else if (eng->live_1d) {
            if (eng->has_weights) {
                histo_fill_n(eng->live_1d, batch_len, batch_x, batch_w);
            } else {
                histo_fill_n(eng->live_1d, batch_len, batch_x, NULL);
            }
        }
        for (size_t i = 0; i < batch_len; ++i) {
            reservoir_push(&eng->reservoir, batch_x[i], batch_y[i], batch_w[i]);
        }
        eng->total_samples += batch_len;
        histo_mutex_unlock(&eng->mutex);
        batch_len = 0;
    }

    return NULL;
}

bool tui_engine_init(tui_engine_t *eng, FILE *in_stream, bool is_2d, uint32_t nbins, double rmin, double rmax, uint32_t flags, bool has_weights) {
    if (!eng) return false;
    memset(eng, 0, sizeof(*eng));

    eng->in_stream = in_stream;
    eng->is_2d = is_2d;
    eng->has_weights = has_weights;
    eng->val_col = 1;
    eng->x_col = 1;
    eng->y_col = 2;
    eng->w_col = has_weights ? 2 : 0;
    eng->flags = flags;
    eng->running = false;
    eng->finished_reading = false;
    eng->paused = false;

    if (rmin >= rmax) {
        rmin = 0.0;
        rmax = 100.0;
    }
    if (nbins == 0) nbins = 50;

    histo_mutex_init(&eng->mutex);

    if (is_2d) {
        eng->live_2d = histo2d_create_uniform(nbins, rmin, rmax, nbins, rmin, rmax, flags);
    } else {
        eng->live_1d = histo_create_uniform(nbins, rmin, rmax, flags);
    }

    reservoir_init(&eng->reservoir, TUI_RESERVOIR_DEFAULT_CAP, is_2d, eng->has_weights);
    eng->auto_range = true;
    eng->auto_range_threshold = 0.05;
    eng->last_autorange_time_sec = get_time_now_sec();
    return true;
}

bool tui_engine_start(tui_engine_t *eng) {
    if (!eng) return false;
    eng->running = true;
    eng->last_time_sec = get_time_now_sec();
    if (histo_thread_create(&eng->thread, ingest_worker_thread, eng) != 0) {
        eng->running = false;
        return false;
    }
    return true;
}

void tui_engine_stop(tui_engine_t *eng) {
    if (!eng) return;
    histo_mutex_lock(&eng->mutex);
    if (!eng->running) {
        histo_mutex_unlock(&eng->mutex);
        return;
    }
    eng->running = false;
    histo_mutex_unlock(&eng->mutex);
    histo_thread_join(eng->thread);
}

void tui_engine_free(tui_engine_t *eng) {
    if (!eng) return;
    tui_engine_stop(eng);
    histo_mutex_lock(&eng->mutex);
    if (eng->live_1d) {
        histo_destroy(eng->live_1d);
        eng->live_1d = NULL;
    }
    if (eng->live_2d) {
        histo2d_destroy(eng->live_2d);
        eng->live_2d = NULL;
    }
    reservoir_free(&eng->reservoir);
    histo_mutex_unlock(&eng->mutex);
    histo_mutex_destroy(&eng->mutex);
}

static int double_cmp(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (isnan(da)) return isnan(db) ? 0 : 1;
    if (isnan(db)) return -1;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

static bool tui_engine_check_and_autorange_locked(tui_engine_t *eng) {
    if (!eng || !eng->auto_range || !eng->live_1d || eng->reservoir.count < 50) {
        return false;
    }

    double now = get_time_now_sec();
    if (now - eng->last_autorange_time_sec < 1.0) {
        return false;
    }

    double in_range_w = histo_total_weight(eng->live_1d);
    double underflow_w = histo_underflow(eng->live_1d);
    double overflow_w = histo_overflow(eng->live_1d);
    double total_w = in_range_w + underflow_w + overflow_w;
    if (total_w <= 0.0) return false;

    double bad_ratio = (underflow_w + overflow_w) / total_w;
    if (bad_ratio <= eng->auto_range_threshold) {
        return false;
    }

    /* Out of bounds ratio exceeded threshold: compute robust p1-p99 range from reservoir */
    eng->last_autorange_time_sec = now;

    size_t count = eng->reservoir.count;
    double *sorted = (double *)malloc(count * sizeof(double));
    if (!sorted) return false;

    memcpy(sorted, eng->reservoir.samples, count * sizeof(double));
    qsort(sorted, count, sizeof(double), double_cmp);

    /* Filter out non-finite entries at the end if any */
    size_t valid_n = count;
    while (valid_n > 0 && !isfinite(sorted[valid_n - 1])) {
        valid_n--;
    }
    if (valid_n < 10) {
        free(sorted);
        return false;
    }

    /* Compute 1st and 99th percentiles */
    size_t idx_p1 = (size_t)(0.01 * (double)(valid_n - 1));
    size_t idx_p99 = (size_t)(0.99 * (double)(valid_n - 1) + 0.5);
    if (idx_p99 >= valid_n) idx_p99 = valid_n - 1;

    double p1 = sorted[idx_p1];
    double p99 = sorted[idx_p99];
    free(sorted);

    double span = p99 - p1;
    double rmin, rmax;
    if (span <= 1e-9) {
        rmin = p1 - 1.0;
        rmax = p99 + 1.0;
    } else {
        /* Add 5% margin to p1-p99 span */
        double margin = span * 0.05;
        rmin = p1 - margin;
        rmax = p99 + margin;
    }

    uint32_t nbins = histo_nbins(eng->live_1d);
    if (nbins < 5) nbins = 50;

    histo_t *new_h = histo_create_uniform(nbins, rmin, rmax, eng->flags);
    if (!new_h) return false;

    if (eng->reservoir.has_weights && eng->reservoir.weights) {
        histo_fill_n(new_h, eng->reservoir.count, eng->reservoir.samples, eng->reservoir.weights);
    } else {
        histo_fill_n(new_h, eng->reservoir.count, eng->reservoir.samples, NULL);
    }

    if (eng->live_1d) histo_destroy(eng->live_1d);
    eng->live_1d = new_h;
    return true;
}

void tui_engine_set_autorange(tui_engine_t *eng, bool enable, double threshold) {
    if (!eng) return;
    histo_mutex_lock(&eng->mutex);
    eng->auto_range = enable;
    if (threshold > 0.0 && threshold < 1.0) {
        eng->auto_range_threshold = threshold;
    }
    histo_mutex_unlock(&eng->mutex);
}

bool tui_engine_check_and_autorange(tui_engine_t *eng) {
    if (!eng) return false;
    histo_mutex_lock(&eng->mutex);
    bool r = tui_engine_check_and_autorange_locked(eng);
    histo_mutex_unlock(&eng->mutex);
    return r;
}

histo_t *tui_engine_get_snapshot_1d(tui_engine_t *eng) {
    if (!eng) return NULL;
    histo_mutex_lock(&eng->mutex);
    tui_engine_check_and_autorange_locked(eng);
    histo_t *snapshot = NULL;
    if (eng->live_1d) {
        snapshot = histo_clone(eng->live_1d, false);
    }
    histo_mutex_unlock(&eng->mutex);
    return snapshot;
}

histo2d_t *tui_engine_get_snapshot_2d(tui_engine_t *eng) {
    if (!eng) return NULL;
    histo_mutex_lock(&eng->mutex);
    histo2d_t *snapshot = NULL;
    if (eng->live_2d) {
        snapshot = histo2d_clone(eng->live_2d, false);
    }
    histo_mutex_unlock(&eng->mutex);
    return snapshot;
}

bool tui_engine_rebuild_1d(tui_engine_t *eng, uint32_t nbins, double rmin, double rmax, histo_t **out_h) {
    if (!eng) return false;
    histo_mutex_lock(&eng->mutex);
    if (eng->reservoir.count == 0) {
        histo_mutex_unlock(&eng->mutex);
        return false;
    }

    if (rmin >= rmax) {
        /* Auto-range over reservoir */
        rmin = eng->reservoir.samples[0];
        rmax = eng->reservoir.samples[0];
        for (size_t i = 1; i < eng->reservoir.count; ++i) {
            if (eng->reservoir.samples[i] < rmin) rmin = eng->reservoir.samples[i];
            if (eng->reservoir.samples[i] > rmax) rmax = eng->reservoir.samples[i];
        }
        if (fabs(rmax - rmin) < 1e-12) {
            rmin -= 1.0;
            rmax += 1.0;
        } else {
            rmax += (rmax - rmin) * 1e-6;
        }
    }

    histo_t *new_h = histo_create_uniform(nbins, rmin, rmax, eng->flags);
    if (!new_h) {
        histo_mutex_unlock(&eng->mutex);
        return false;
    }

    if (eng->reservoir.has_weights && eng->reservoir.weights) {
        histo_fill_n(new_h, eng->reservoir.count, eng->reservoir.samples, eng->reservoir.weights);
    } else {
        histo_fill_n(new_h, eng->reservoir.count, eng->reservoir.samples, NULL);
    }

    if (eng->live_1d) histo_destroy(eng->live_1d);
    eng->live_1d = new_h;
    if (out_h) {
        *out_h = histo_clone(new_h, false);
    }

    histo_mutex_unlock(&eng->mutex);
    return true;
}

bool tui_engine_rebuild_1d_log(tui_engine_t *eng, uint32_t nbins, histo_t **out_h) {
    if (!eng) return false;
    if (nbins < 2) nbins = 2;

    histo_mutex_lock(&eng->mutex);
    if (eng->reservoir.count == 0) {
        histo_mutex_unlock(&eng->mutex);
        return false;
    }

    double pos_min = 1e300;
    double pos_max = -1e300;
    for (size_t i = 0; i < eng->reservoir.count; ++i) {
        double val = eng->reservoir.samples[i];
        if (val > 0.0) {
            if (val < pos_min) pos_min = val;
            if (val > pos_max) pos_max = val;
        }
    }

    if (pos_min >= pos_max || pos_min <= 0.0 || pos_min > 1e200) {
        pos_min = 1.0;
        pos_max = 100.0;
    }

    double *edges = (double *)malloc((nbins + 1) * sizeof(double));
    if (!edges) {
        histo_mutex_unlock(&eng->mutex);
        return false;
    }

    double log_min = log10(pos_min);
    double log_max = log10(pos_max * 1.000001);
    double step = (log_max - log_min) / (double)nbins;
    for (uint32_t i = 0; i <= nbins; ++i) {
        edges[i] = pow(10.0, log_min + (double)i * step);
    }

    histo_t *new_h = histo_create_variable(nbins, edges, eng->flags);
    free(edges);
    if (!new_h) {
        histo_mutex_unlock(&eng->mutex);
        return false;
    }

    if (eng->reservoir.has_weights && eng->reservoir.weights) {
        histo_fill_n(new_h, eng->reservoir.count, eng->reservoir.samples, eng->reservoir.weights);
    } else {
        histo_fill_n(new_h, eng->reservoir.count, eng->reservoir.samples, NULL);
    }

    if (eng->live_1d) histo_destroy(eng->live_1d);
    eng->live_1d = new_h;
    if (out_h) {
        *out_h = histo_clone(new_h, false);
    }

    histo_mutex_unlock(&eng->mutex);
    return true;
}

bool tui_engine_zoom_1d(tui_engine_t *eng, double factor, histo_t **out_h) {
    if (!eng) return false;
    histo_mutex_lock(&eng->mutex);
    histo_t *source = eng->live_1d;
    if (!source) {
        histo_mutex_unlock(&eng->mutex);
        return false;
    }
    double rmin = 0.0, rmax = 0.0;
    histo_range(source, &rmin, &rmax);
    uint32_t nbins = histo_nbins(source);
    if (nbins < 2) nbins = 50;

    double center = 0.5 * (rmin + rmax);
    double half_span = 0.5 * (rmax - rmin) * factor;
    if (half_span < 1e-12) half_span = 1.0;

    double new_min = center - half_span;
    double new_max = center + half_span;

    eng->auto_range = false; /* Manual zoom overrides auto-range */
    histo_mutex_unlock(&eng->mutex);

    return tui_engine_rebuild_1d(eng, nbins, new_min, new_max, out_h);
}

bool tui_engine_pan_1d(tui_engine_t *eng, double fraction, histo_t **out_h) {
    if (!eng) return false;
    histo_mutex_lock(&eng->mutex);
    histo_t *source = eng->live_1d;
    if (!source) {
        histo_mutex_unlock(&eng->mutex);
        return false;
    }
    double rmin = 0.0, rmax = 0.0;
    histo_range(source, &rmin, &rmax);
    uint32_t nbins = histo_nbins(source);
    if (nbins < 2) nbins = 50;

    double shift = (rmax - rmin) * fraction;
    double new_min = rmin + shift;
    double new_max = rmax + shift;

    eng->auto_range = false; /* Manual pan overrides auto-range */
    histo_mutex_unlock(&eng->mutex);

    return tui_engine_rebuild_1d(eng, nbins, new_min, new_max, out_h);
}

bool tui_engine_rebuild_2d(tui_engine_t *eng, uint32_t xbins, double xmin, double xmax,
                           uint32_t ybins, double ymin, double ymax, histo2d_t **out_h) {
    if (!eng) return false;
    histo_mutex_lock(&eng->mutex);
    if (eng->reservoir.count == 0 || !eng->reservoir.samples_y) {
        histo_mutex_unlock(&eng->mutex);
        return false;
    }

    if (xbins < 2) xbins = 50;
    if (ybins < 2) ybins = 50;
    if (xmin >= xmax) { xmin = 0.0; xmax = 100.0; }
    if (ymin >= ymax) { ymin = 0.0; ymax = 100.0; }

    histo2d_t *new_h = histo2d_create_uniform(xbins, xmin, xmax, ybins, ymin, ymax, eng->flags);
    if (!new_h) {
        histo_mutex_unlock(&eng->mutex);
        return false;
    }

    if (eng->reservoir.has_weights && eng->reservoir.weights) {
        histo2d_fill_n(new_h, eng->reservoir.count, eng->reservoir.samples, eng->reservoir.samples_y, eng->reservoir.weights);
    } else {
        histo2d_fill_n(new_h, eng->reservoir.count, eng->reservoir.samples, eng->reservoir.samples_y, NULL);
    }

    if (eng->live_2d) histo2d_destroy(eng->live_2d);
    eng->live_2d = new_h;
    if (out_h) {
        *out_h = histo2d_clone(new_h, false);
    }

    histo_mutex_unlock(&eng->mutex);
    return true;
}

bool tui_engine_zoom_2d(tui_engine_t *eng, double factor, histo2d_t **out_h) {
    if (!eng) return false;
    histo_mutex_lock(&eng->mutex);
    histo2d_t *source = eng->live_2d;
    if (!source) {
        histo_mutex_unlock(&eng->mutex);
        return false;
    }
    histo2d_axis_t ax, ay;
    histo2d_axis_x(source, &ax);
    histo2d_axis_y(source, &ay);
    uint32_t nx = histo2d_nbins_x(source);
    uint32_t ny = histo2d_nbins_y(source);

    double cx = 0.5 * (ax.min + ax.max);
    double cy = 0.5 * (ay.min + ay.max);
    double hx = 0.5 * (ax.max - ax.min) * factor;
    double hy = 0.5 * (ay.max - ay.min) * factor;
    if (hx < 1e-12) hx = 1.0;
    if (hy < 1e-12) hy = 1.0;

    double n_xmin = cx - hx, n_xmax = cx + hx;
    double n_ymin = cy - hy, n_ymax = cy + hy;

    eng->auto_range = false;
    histo_mutex_unlock(&eng->mutex);

    return tui_engine_rebuild_2d(eng, nx, n_xmin, n_xmax, ny, n_ymin, n_ymax, out_h);
}

bool tui_engine_pan_2d(tui_engine_t *eng, double frac_x, double frac_y, histo2d_t **out_h) {
    if (!eng) return false;
    histo_mutex_lock(&eng->mutex);
    histo2d_t *source = eng->live_2d;
    if (!source) {
        histo_mutex_unlock(&eng->mutex);
        return false;
    }
    histo2d_axis_t ax, ay;
    histo2d_axis_x(source, &ax);
    histo2d_axis_y(source, &ay);
    uint32_t nx = histo2d_nbins_x(source);
    uint32_t ny = histo2d_nbins_y(source);

    double shift_x = (ax.max - ax.min) * frac_x;
    double shift_y = (ay.max - ay.min) * frac_y;

    double n_xmin = ax.min + shift_x, n_xmax = ax.max + shift_x;
    double n_ymin = ay.min + shift_y, n_ymax = ay.max + shift_y;

    eng->auto_range = false;
    histo_mutex_unlock(&eng->mutex);

    return tui_engine_rebuild_2d(eng, nx, n_xmin, n_xmax, ny, n_ymin, n_ymax, out_h);
}

void tui_engine_clear(tui_engine_t *eng) {
    if (!eng) return;
    histo_mutex_lock(&eng->mutex);
    if (eng->live_1d) histo_reset(eng->live_1d);
    if (eng->live_2d) histo2d_reset(eng->live_2d);
    eng->reservoir.count = 0;
    eng->reservoir.head = 0;
    eng->total_samples = 0;
    eng->current_rate_ops = 0.0;
    histo_mutex_unlock(&eng->mutex);
}

bool tui_engine_is_finished(tui_engine_t *eng) {
    if (!eng) return true;
    histo_mutex_lock(&eng->mutex);
    bool fin = eng->finished_reading;
    histo_mutex_unlock(&eng->mutex);
    return fin;
}
