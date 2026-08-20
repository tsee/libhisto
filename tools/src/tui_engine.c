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

static void reservoir_init(tui_reservoir_t *r, size_t cap, bool has_weights) {
    if (!r) return;
    if (cap < 1000) cap = TUI_RESERVOIR_DEFAULT_CAP;
    r->samples = (double *)malloc(cap * sizeof(double));
    r->weights = has_weights ? (double *)malloc(cap * sizeof(double)) : NULL;
    r->count = 0;
    r->cap = cap;
    r->head = 0;
    r->has_weights = has_weights;
}

static void reservoir_push(tui_reservoir_t *r, double x, double w) {
    if (!r || !r->samples) return;
    if (r->count < r->cap) {
        r->samples[r->count] = x;
        if (r->weights) r->weights[r->count] = w;
        r->count++;
    } else {
        /* Ring buffer overwrite */
        r->samples[r->head] = x;
        if (r->weights) r->weights[r->head] = w;
        r->head = (r->head + 1) % r->cap;
    }
}

static void reservoir_free(tui_reservoir_t *r) {
    if (!r) return;
    if (r->samples) free(r->samples);
    if (r->weights) free(r->weights);
    r->samples = NULL;
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
    const size_t batch_max = 512;
    double batch_x[512];
    double batch_w[512];
    size_t batch_len = 0;

    double last_calc_time = get_time_now_sec();
    uint64_t last_calc_samples = 0;

    while (is_engine_running(eng)) {
        if (!fgets(line, sizeof(line), eng->in_stream)) {
            histo_mutex_lock(&eng->mutex);
            if (batch_len > 0) {
                if (eng->live_1d) {
                    if (eng->has_weights) {
                        histo_fill_n(eng->live_1d, batch_len, batch_x, batch_w);
                    } else {
                        histo_fill_n(eng->live_1d, batch_len, batch_x, NULL);
                    }
                }
                for (size_t i = 0; i < batch_len; ++i) {
                    reservoir_push(&eng->reservoir, batch_x[i], batch_w[i]);
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
        double val = strtod(p, &endp);
        if (endp == p) continue;

        double weight = 1.0;
        if (eng->has_weights && *endp != '\0') {
            while (*endp && (isspace((unsigned char)*endp) || *endp == ',' || *endp == '\t' || *endp == ';')) endp++;
            if (*endp != '\0') {
                char *w_end = NULL;
                double w_val = strtod(endp, &w_end);
                if (w_end != endp) weight = w_val;
            }
        }

        batch_x[batch_len] = val;
        batch_w[batch_len] = weight;
        batch_len++;

        if (batch_len >= batch_max) {
            histo_mutex_lock(&eng->mutex);
            if (eng->live_1d) {
                if (eng->has_weights) {
                    histo_fill_n(eng->live_1d, batch_len, batch_x, batch_w);
                } else {
                    histo_fill_n(eng->live_1d, batch_len, batch_x, NULL);
                }
            }
            for (size_t i = 0; i < batch_len; ++i) {
                reservoir_push(&eng->reservoir, batch_x[i], batch_w[i]);
            }
            eng->total_samples += batch_len;
            histo_mutex_unlock(&eng->mutex);
            batch_len = 0;
        }

        /* Update rate calculation every 250ms */
        double now = get_time_now_sec();
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
        if (eng->live_1d) {
            if (eng->has_weights) {
                histo_fill_n(eng->live_1d, batch_len, batch_x, batch_w);
            } else {
                histo_fill_n(eng->live_1d, batch_len, batch_x, NULL);
            }
        }
        for (size_t i = 0; i < batch_len; ++i) {
            reservoir_push(&eng->reservoir, batch_x[i], batch_w[i]);
        }
        eng->total_samples += batch_len;
        histo_mutex_unlock(&eng->mutex);
        batch_len = 0;
    }

    return NULL;
}

bool tui_engine_init(tui_engine_t *eng, FILE *in_stream, bool is_2d, uint32_t nbins, double rmin, double rmax, uint32_t flags) {
    if (!eng) return false;
    memset(eng, 0, sizeof(*eng));

    eng->in_stream = in_stream;
    eng->is_2d = is_2d;
    eng->val_col = 1;
    eng->x_col = 1;
    eng->y_col = 2;
    eng->w_col = 2;
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

    reservoir_init(&eng->reservoir, TUI_RESERVOIR_DEFAULT_CAP, eng->has_weights);
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

histo_t *tui_engine_get_snapshot_1d(tui_engine_t *eng) {
    if (!eng) return NULL;
    histo_mutex_lock(&eng->mutex);
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

    if (out_h) {
        *out_h = new_h;
    } else {
        if (eng->live_1d) histo_destroy(eng->live_1d);
        eng->live_1d = new_h;
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

    if (out_h) {
        *out_h = new_h;
    } else {
        if (eng->live_1d) histo_destroy(eng->live_1d);
        eng->live_1d = new_h;
    }

    histo_mutex_unlock(&eng->mutex);
    return true;
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
