/*
 * Node-API C binding bridge for libhisto and libhistocli.
 */

#include <node_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "histo/types.h"
#include "histo/histo.h"
#include "histo/histo2d.h"
#include "histo/fit.h"
#include "histo/kde.h"
#include "histo/sketch.h"
#include "histo/version.h"
#include "histo/cli.h"

/* ========================================================================= */
/* Wrapper structures for safe lifecycle and double-free protection           */
/* ========================================================================= */

typedef struct {
    histo_t *handle;
    bool destroyed;
} HistoWrap;

typedef struct {
    histo2d_t *handle;
    bool destroyed;
} Histo2DWrap;

typedef struct {
    histo_kde_t *handle;
    bool destroyed;
} KDEWrap;

typedef struct {
    histo_sketch_t *handle;
    bool destroyed;
} SketchWrap;

/* ========================================================================= */
/* Helper Macros and Error Handlers                                          */
/* ========================================================================= */

#define NAPI_CALL_RET(env, call, ret_val)                                    \
    do {                                                                     \
        napi_status _st = (call);                                            \
        if (_st != napi_ok) {                                                \
            return (ret_val);                                                \
        }                                                                    \
    } while (0)

#define NAPI_CALL(env, call) NAPI_CALL_RET(env, call, NULL)

static void throw_error(napi_env env, const char *msg) {
    napi_throw_error(env, NULL, msg);
}

static void throw_type_error(napi_env env, const char *msg) {
    napi_throw_type_error(env, NULL, msg);
}

static void throw_range_error(napi_env env, const char *msg) {
    napi_throw_range_error(env, NULL, msg);
}

static void throw_histo_status(napi_env env, histo_status_t status, const char *context) {
    char buf[256];
    const char *err_str = histo_status_str(status);
    if (context && context[0] != '\0') {
        snprintf(buf, sizeof(buf), "%s: %s (code %d)", context, err_str, (int)status);
    } else {
        snprintf(buf, sizeof(buf), "libhisto error: %s (code %d)", err_str, (int)status);
    }
    if (status == HISTO_ERR_INVALID_ARG) {
        throw_type_error(env, buf);
    } else if (status == HISTO_ERR_OUT_OF_RANGE) {
        throw_range_error(env, buf);
    } else {
        throw_error(env, buf);
    }
}

/* ========================================================================= */
/* Finalizers                                                                */
/* ========================================================================= */

static void histo_finalizer(napi_env env, void *finalize_data, void *finalize_hint) {
    (void)env; (void)finalize_hint;
    HistoWrap *wrap = (HistoWrap *)finalize_data;
    if (wrap) {
        if (!wrap->destroyed && wrap->handle) {
            histo_destroy(wrap->handle);
            wrap->handle = NULL;
            wrap->destroyed = true;
        }
        free(wrap);
    }
}

static void histo2d_finalizer(napi_env env, void *finalize_data, void *finalize_hint) {
    (void)env; (void)finalize_hint;
    Histo2DWrap *wrap = (Histo2DWrap *)finalize_data;
    if (wrap) {
        if (!wrap->destroyed && wrap->handle) {
            histo2d_destroy(wrap->handle);
            wrap->handle = NULL;
            wrap->destroyed = true;
        }
        free(wrap);
    }
}

static void kde_finalizer(napi_env env, void *finalize_data, void *finalize_hint) {
    (void)env; (void)finalize_hint;
    KDEWrap *wrap = (KDEWrap *)finalize_data;
    if (wrap) {
        if (!wrap->destroyed && wrap->handle) {
            histo_kde_destroy(wrap->handle);
            wrap->handle = NULL;
            wrap->destroyed = true;
        }
        free(wrap);
    }
}

static void sketch_finalizer(napi_env env, void *finalize_data, void *finalize_hint) {
    (void)env; (void)finalize_hint;
    SketchWrap *wrap = (SketchWrap *)finalize_data;
    if (wrap) {
        if (!wrap->destroyed && wrap->handle) {
            histo_sketch_destroy(wrap->handle);
            wrap->handle = NULL;
            wrap->destroyed = true;
        }
        free(wrap);
    }
}

/* ========================================================================= */
/* Wrapping / Unwrapping Helpers                                             */
/* ========================================================================= */

static napi_value wrap_histo(napi_env env, histo_t *h) {
    if (!h) {
        throw_error(env, "Failed to create 1D histogram");
        return NULL;
    }
    HistoWrap *wrap = (HistoWrap *)malloc(sizeof(HistoWrap));
    if (!wrap) {
        histo_destroy(h);
        throw_error(env, "Out of memory allocating HistoWrap");
        return NULL;
    }
    wrap->handle = h;
    wrap->destroyed = false;

    napi_value obj;
    if (napi_create_object(env, &obj) != napi_ok) {
        histo_destroy(h);
        free(wrap);
        throw_error(env, "Failed to create JS object");
        return NULL;
    }

    if (napi_wrap(env, obj, wrap, histo_finalizer, NULL, NULL) != napi_ok) {
        histo_destroy(h);
        free(wrap);
        throw_error(env, "Failed to wrap native histogram");
        return NULL;
    }
    return obj;
}

static HistoWrap* unwrap_histo(napi_env env, napi_value obj) {
    HistoWrap *wrap = NULL;
    napi_status st = napi_unwrap(env, obj, (void **)&wrap);
    if (st != napi_ok || !wrap) {
        throw_type_error(env, "Invalid Histogram instance");
        return NULL;
    }
    if (wrap->destroyed || !wrap->handle) {
        throw_error(env, "Histogram instance has already been destroyed");
        return NULL;
    }
    return wrap;
}

static napi_value wrap_histo2d(napi_env env, histo2d_t *h2) {
    if (!h2) {
        throw_error(env, "Failed to create 2D histogram");
        return NULL;
    }
    Histo2DWrap *wrap = (Histo2DWrap *)malloc(sizeof(Histo2DWrap));
    if (!wrap) {
        histo2d_destroy(h2);
        throw_error(env, "Out of memory allocating Histo2DWrap");
        return NULL;
    }
    wrap->handle = h2;
    wrap->destroyed = false;

    napi_value obj;
    if (napi_create_object(env, &obj) != napi_ok) {
        histo2d_destroy(h2);
        free(wrap);
        throw_error(env, "Failed to create JS object");
        return NULL;
    }

    if (napi_wrap(env, obj, wrap, histo2d_finalizer, NULL, NULL) != napi_ok) {
        histo2d_destroy(h2);
        free(wrap);
        throw_error(env, "Failed to wrap native 2D histogram");
        return NULL;
    }
    return obj;
}

static Histo2DWrap* unwrap_histo2d(napi_env env, napi_value obj) {
    Histo2DWrap *wrap = NULL;
    napi_status st = napi_unwrap(env, obj, (void **)&wrap);
    if (st != napi_ok || !wrap) {
        throw_type_error(env, "Invalid Histogram2D instance");
        return NULL;
    }
    if (wrap->destroyed || !wrap->handle) {
        throw_error(env, "Histogram2D instance has already been destroyed");
        return NULL;
    }
    return wrap;
}

static napi_value wrap_kde(napi_env env, histo_kde_t *kde) {
    if (!kde) {
        throw_error(env, "Failed to create KDE model");
        return NULL;
    }
    KDEWrap *wrap = (KDEWrap *)malloc(sizeof(KDEWrap));
    if (!wrap) {
        histo_kde_destroy(kde);
        throw_error(env, "Out of memory allocating KDEWrap");
        return NULL;
    }
    wrap->handle = kde;
    wrap->destroyed = false;

    napi_value obj;
    if (napi_create_object(env, &obj) != napi_ok) {
        histo_kde_destroy(kde);
        free(wrap);
        throw_error(env, "Failed to create JS object");
        return NULL;
    }

    if (napi_wrap(env, obj, wrap, kde_finalizer, NULL, NULL) != napi_ok) {
        histo_kde_destroy(kde);
        free(wrap);
        throw_error(env, "Failed to wrap native KDE");
        return NULL;
    }
    return obj;
}

static KDEWrap* unwrap_kde(napi_env env, napi_value obj) {
    KDEWrap *wrap = NULL;
    napi_status st = napi_unwrap(env, obj, (void **)&wrap);
    if (st != napi_ok || !wrap) {
        throw_type_error(env, "Invalid KDE instance");
        return NULL;
    }
    if (wrap->destroyed || !wrap->handle) {
        throw_error(env, "KDE instance has already been destroyed");
        return NULL;
    }
    return wrap;
}

static napi_value wrap_sketch(napi_env env, histo_sketch_t *s) {
    if (!s) {
        throw_error(env, "Failed to create Sketch instance");
        return NULL;
    }
    SketchWrap *wrap = (SketchWrap *)malloc(sizeof(SketchWrap));
    if (!wrap) {
        histo_sketch_destroy(s);
        throw_error(env, "Out of memory allocating SketchWrap");
        return NULL;
    }
    wrap->handle = s;
    wrap->destroyed = false;

    napi_value obj;
    if (napi_create_object(env, &obj) != napi_ok) {
        histo_sketch_destroy(s);
        free(wrap);
        throw_error(env, "Failed to create JS object");
        return NULL;
    }

    if (napi_wrap(env, obj, wrap, sketch_finalizer, NULL, NULL) != napi_ok) {
        histo_sketch_destroy(s);
        free(wrap);
        throw_error(env, "Failed to wrap native Sketch");
        return NULL;
    }
    return obj;
}

static SketchWrap* unwrap_sketch(napi_env env, napi_value obj) {
    SketchWrap *wrap = NULL;
    napi_status st = napi_unwrap(env, obj, (void **)&wrap);
    if (st != napi_ok || !wrap) {
        throw_type_error(env, "Invalid Sketch instance");
        return NULL;
    }
    if (wrap->destroyed || !wrap->handle) {
        throw_error(env, "Sketch instance has already been destroyed");
        return NULL;
    }
    return wrap;
}

/* ========================================================================= */
/* Zero-Copy / Array Extraction Helpers                                      */
/* ========================================================================= */

static bool extract_double_array(napi_env env, napi_value val, const double **out_ptr, size_t *out_len, double **out_allocated) {
    *out_ptr = NULL;
    *out_len = 0;
    *out_allocated = NULL;

    bool is_typedarray = false;
    napi_is_typedarray(env, val, &is_typedarray);
    if (is_typedarray) {
        napi_typedarray_type type;
        size_t length = 0;
        void *data = NULL;
        if (napi_get_typedarray_info(env, val, &type, &length, &data, NULL, NULL) == napi_ok) {
            if (type == napi_float64_array) {
                *out_ptr = (const double *)data;
                *out_len = length;
                *out_allocated = NULL;
                return true;
            } else if (type == napi_float32_array) {
                float *f_data = (float *)data;
                double *buf = (double *)malloc(length * sizeof(double));
                if (!buf) {
                    throw_error(env, "Out of memory converting Float32Array");
                    return false;
                }
                for (size_t i = 0; i < length; i++) {
                    buf[i] = (double)f_data[i];
                }
                *out_ptr = buf;
                *out_len = length;
                *out_allocated = buf;
                return true;
            } else if (type == napi_int32_array) {
                int32_t *i_data = (int32_t *)data;
                double *buf = (double *)malloc(length * sizeof(double));
                if (!buf) {
                    throw_error(env, "Out of memory converting Int32Array");
                    return false;
                }
                for (size_t i = 0; i < length; i++) {
                    buf[i] = (double)i_data[i];
                }
                *out_ptr = buf;
                *out_len = length;
                *out_allocated = buf;
                return true;
            } else if (type == napi_uint32_array) {
                uint32_t *u_data = (uint32_t *)data;
                double *buf = (double *)malloc(length * sizeof(double));
                if (!buf) {
                    throw_error(env, "Out of memory converting Uint32Array");
                    return false;
                }
                for (size_t i = 0; i < length; i++) {
                    buf[i] = (double)u_data[i];
                }
                *out_ptr = buf;
                *out_len = length;
                *out_allocated = buf;
                return true;
            }
        }
    }

    bool is_array = false;
    napi_is_array(env, val, &is_array);
    if (is_array) {
        uint32_t length = 0;
        napi_get_array_length(env, val, &length);
        double *buf = (double *)malloc(length * sizeof(double));
        if (!buf && length > 0) {
            throw_error(env, "Out of memory converting Array to double[]");
            return false;
        }
        for (uint32_t i = 0; i < length; i++) {
            napi_value item;
            napi_get_element(env, val, i, &item);
            double d = 0.0;
            napi_get_value_double(env, item, &d);
            buf[i] = d;
        }
        *out_ptr = buf;
        *out_len = length;
        *out_allocated = buf;
        return true;
    }

    throw_type_error(env, "Expected Float64Array or Array of numbers");
    return false;
}

static void free_double_array(double *allocated) {
    if (allocated) {
        free(allocated);
    }
}

/* ========================================================================= */
/* 1D Histogram Native Functions                                             */
/* ========================================================================= */

static napi_value n_histo_create_uniform(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

    if (argc < 3) {
        throw_type_error(env, "histo_create_uniform requires at least 3 arguments: nbins, min, max, [flags]");
        return NULL;
    }

    uint32_t nbins = 0;
    double min = 0.0, max = 0.0;
    uint32_t flags = 0;

    NAPI_CALL(env, napi_get_value_uint32(env, args[0], &nbins));
    NAPI_CALL(env, napi_get_value_double(env, args[1], &min));
    NAPI_CALL(env, napi_get_value_double(env, args[2], &max));
    if (argc >= 4) {
        napi_get_value_uint32(env, args[3], &flags);
    }

    histo_t *h = histo_create_uniform(nbins, min, max, flags);
    if (!h) {
        throw_error(env, "Failed to create uniform histogram (check bounds: min < max, nbins >= 1)");
        return NULL;
    }
    return wrap_histo(env, h);
}

static napi_value n_histo_create_variable(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

    if (argc < 1) {
        throw_type_error(env, "histo_create_variable requires edges array and optional flags");
        return NULL;
    }

    const double *edges = NULL;
    size_t edges_len = 0;
    double *edges_alloc = NULL;
    if (!extract_double_array(env, args[0], &edges, &edges_len, &edges_alloc)) {
        return NULL;
    }

    if (edges_len < 2) {
        free_double_array(edges_alloc);
        throw_range_error(env, "Variable bin edges array must have at least 2 elements");
        return NULL;
    }

    uint32_t flags = 0;
    if (argc >= 2) {
        napi_get_value_uint32(env, args[1], &flags);
    }

    uint32_t nbins = (uint32_t)(edges_len - 1);
    histo_t *h = histo_create_variable(nbins, edges, flags);
    free_double_array(edges_alloc);

    if (!h) {
        throw_error(env, "Failed to create variable histogram (edges must be strictly monotonic)");
        return NULL;
    }
    return wrap_histo(env, h);
}

static napi_value n_histo_create_auto(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

    if (argc < 1) {
        throw_type_error(env, "histo_create_auto requires sample values, [rule], [flags]");
        return NULL;
    }

    const double *samples = NULL;
    size_t n = 0;
    double *samples_alloc = NULL;
    if (!extract_double_array(env, args[0], &samples, &n, &samples_alloc)) {
        return NULL;
    }

    if (n == 0) {
        free_double_array(samples_alloc);
        throw_range_error(env, "Cannot auto-estimate bins from empty sample array");
        return NULL;
    }

    uint32_t rule = HISTO_BIN_RULE_AUTO;
    uint32_t flags = 0;
    if (argc >= 2) {
        napi_get_value_uint32(env, args[1], &rule);
    }
    if (argc >= 3) {
        napi_get_value_uint32(env, args[2], &flags);
    }

    histo_t *h = histo_create_auto(n, samples, (histo_bin_rule_t)rule, flags);
    free_double_array(samples_alloc);

    if (!h) {
        throw_error(env, "Failed to auto-create histogram from samples");
        return NULL;
    }
    return wrap_histo(env, h);
}

static napi_value n_histo_estimate_bins(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

    if (argc < 1) {
        throw_type_error(env, "histo_estimate_bins requires sample array and optional rule");
        return NULL;
    }

    const double *samples = NULL;
    size_t n = 0;
    double *samples_alloc = NULL;
    if (!extract_double_array(env, args[0], &samples, &n, &samples_alloc)) {
        return NULL;
    }

    if (n == 0) {
        free_double_array(samples_alloc);
        throw_range_error(env, "Cannot estimate bins from empty sample array");
        return NULL;
    }

    uint32_t rule = HISTO_BIN_RULE_AUTO;
    if (argc >= 2) {
        napi_get_value_uint32(env, args[1], &rule);
    }

    uint32_t nbins = 0;
    double min = 0.0, max = 0.0;
    histo_status_t st = histo_estimate_bins(n, samples, (histo_bin_rule_t)rule, &nbins, &min, &max);
    free_double_array(samples_alloc);

    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_estimate_bins failed");
        return NULL;
    }

    napi_value res;
    napi_create_object(env, &res);
    napi_value v_nbins, v_min, v_max;
    napi_create_uint32(env, nbins, &v_nbins);
    napi_create_double(env, min, &v_min);
    napi_create_double(env, max, &v_max);

    napi_set_named_property(env, res, "nbins", v_nbins);
    napi_set_named_property(env, res, "min", v_min);
    napi_set_named_property(env, res, "max", v_max);
    return res;
}

static napi_value n_histo_destroy(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) return NULL;

    HistoWrap *wrap = NULL;
    if (napi_unwrap(env, args[0], (void **)&wrap) == napi_ok && wrap) {
        if (!wrap->destroyed && wrap->handle) {
            histo_destroy(wrap->handle);
            wrap->handle = NULL;
            wrap->destroyed = true;
        }
    }
    return NULL;
}

static napi_value n_histo_clone(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) {
        throw_type_error(env, "histo_clone requires histogram instance");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    bool empty = false;
    if (argc >= 2) {
        napi_get_value_bool(env, args[1], &empty);
    }

    histo_t *cloned = histo_clone(wrap->handle, empty);
    if (!cloned) {
        throw_error(env, "Failed to clone histogram");
        return NULL;
    }
    return wrap_histo(env, cloned);
}

static napi_value n_histo_reset(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) return NULL;

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    histo_status_t st = histo_reset(wrap->handle);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_reset failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo_fill(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_fill requires histogram instance and x coordinate");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double x = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &x));

    histo_status_t st;
    if (argc >= 3) {
        napi_valuetype vt;
        napi_typeof(env, args[2], &vt);
        if (vt == napi_number) {
            double weight = 1.0;
            napi_get_value_double(env, args[2], &weight);
            st = histo_fill_w(wrap->handle, x, weight);
        } else {
            st = histo_fill(wrap->handle, x);
        }
    } else {
        st = histo_fill(wrap->handle, x);
    }

    if (st != HISTO_OK && st != HISTO_WARN_NON_FINITE) {
        throw_histo_status(env, st, "histo_fill failed");
        return NULL;
    }
    napi_value ret;
    napi_create_int32(env, (int32_t)st, &ret);
    return ret;
}

static napi_value n_histo_fill_n(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_fill_n requires histogram and values array");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    const double *x = NULL;
    size_t n = 0;
    double *x_alloc = NULL;
    if (!extract_double_array(env, args[1], &x, &n, &x_alloc)) {
        return NULL;
    }

    const double *weights = NULL;
    size_t nw = 0;
    double *w_alloc = NULL;
    if (argc >= 3) {
        napi_valuetype vt;
        napi_typeof(env, args[2], &vt);
        if (vt != napi_undefined && vt != napi_null) {
            if (!extract_double_array(env, args[2], &weights, &nw, &w_alloc)) {
                free_double_array(x_alloc);
                return NULL;
            }
            if (nw != n) {
                free_double_array(x_alloc);
                free_double_array(w_alloc);
                throw_range_error(env, "Weights array length must match values array length");
                return NULL;
            }
        }
    }

    histo_status_t st = histo_fill_n(wrap->handle, n, x, weights);
    free_double_array(x_alloc);
    free_double_array(w_alloc);

    if (st != HISTO_OK && st != HISTO_WARN_NON_FINITE) {
        throw_histo_status(env, st, "histo_fill_n failed");
        return NULL;
    }
    napi_value ret;
    napi_create_int32(env, (int32_t)st, &ret);
    return ret;
}

static napi_value n_histo_fill_bin(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo_fill_bin requires histogram, binIndex, weight");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t bin = 0;
    double weight = 0.0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &bin));
    NAPI_CALL(env, napi_get_value_double(env, args[2], &weight));

    histo_status_t st = histo_fill_bin(wrap->handle, bin, weight);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_fill_bin failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo_nbins(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t nb = histo_nbins(wrap->handle);
    napi_value res;
    napi_create_uint32(env, nb, &res);
    return res;
}

static napi_value n_histo_bin_type(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    histo_bin_type_t bt = histo_bin_type(wrap->handle);
    napi_value res;
    napi_create_int32(env, (int32_t)bt, &res);
    return res;
}

static napi_value n_histo_range(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double min = 0.0, max = 0.0;
    histo_status_t st = histo_range(wrap->handle, &min, &max);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_range failed");
        return NULL;
    }
    napi_value res;
    napi_create_array_with_length(env, 2, &res);
    napi_value v0, v1;
    napi_create_double(env, min, &v0);
    napi_create_double(env, max, &v1);
    napi_set_element(env, res, 0, v0);
    napi_set_element(env, res, 1, v1);
    return res;
}

static napi_value n_histo_find_bin(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_find_bin requires histogram and x");
        return NULL;
    }
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double x = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &x));

    int64_t out_bin = 0;
    histo_status_t st = histo_find_bin(wrap->handle, x, &out_bin);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_find_bin failed");
        return NULL;
    }
    napi_value res;
    napi_create_int64(env, out_bin, &res);
    return res;
}

static napi_value n_histo_bin_bounds(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_bin_bounds requires histogram and binIndex");
        return NULL;
    }
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t bin = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &bin));

    double lower = 0.0, upper = 0.0;
    histo_status_t st = histo_bin_bounds(wrap->handle, bin, &lower, &upper);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_bin_bounds failed");
        return NULL;
    }
    napi_value res;
    napi_create_array_with_length(env, 2, &res);
    napi_value v0, v1;
    napi_create_double(env, lower, &v0);
    napi_create_double(env, upper, &v1);
    napi_set_element(env, res, 0, v0);
    napi_set_element(env, res, 1, v1);
    return res;
}

static napi_value n_histo_bin_center(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_bin_center requires histogram and binIndex");
        return NULL;
    }
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t bin = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &bin));

    double center = 0.0;
    histo_status_t st = histo_bin_center(wrap->handle, bin, &center);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_bin_center failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, center, &res);
    return res;
}

static napi_value n_histo_bin_content(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_bin_content requires histogram and binIndex");
        return NULL;
    }
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t bin = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &bin));

    double content = 0.0;
    histo_status_t st = histo_bin_content(wrap->handle, bin, &content);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_bin_content failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, content, &res);
    return res;
}

static napi_value n_histo_bin_error(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_bin_error requires histogram and binIndex");
        return NULL;
    }
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t bin = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &bin));

    double error = 0.0;
    histo_status_t st = histo_bin_error(wrap->handle, bin, &error);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_bin_error failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, error, &res);
    return res;
}

static napi_value n_histo_bin_sum_w2(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_bin_sum_w2 requires histogram and binIndex");
        return NULL;
    }
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t bin = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &bin));

    double sum_w2 = 0.0;
    histo_status_t st = histo_bin_sum_w2(wrap->handle, bin, &sum_w2);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_bin_sum_w2 failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, sum_w2, &res);
    return res;
}

static napi_value n_histo_total_weight(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double tw = histo_total_weight(wrap->handle);
    napi_value res;
    napi_create_double(env, tw, &res);
    return res;
}

static napi_value n_histo_num_entries(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint64_t n = histo_num_entries(wrap->handle);
    napi_value res;
    napi_create_int64(env, (int64_t)n, &res);
    return res;
}

static napi_value n_histo_underflow(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double u = histo_underflow(wrap->handle);
    napi_value res;
    napi_create_double(env, u, &res);
    return res;
}

static napi_value n_histo_overflow(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double o = histo_overflow(wrap->handle);
    napi_value res;
    napi_create_double(env, o, &res);
    return res;
}

static napi_value n_histo_nan_count(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint64_t nan_c = histo_nan_count(wrap->handle);
    napi_value res;
    napi_create_int64(env, (int64_t)nan_c, &res);
    return res;
}

/* ========================================================================= */
/* 1D Moments & Quantiles                                                    */
/* ========================================================================= */

static napi_value n_histo_mean(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo_mean(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_mean failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_variance(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo_variance(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_variance failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_std_dev(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo_std_dev(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_std_dev failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_central_moment(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_central_moment requires histogram and order k");
        return NULL;
    }
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t k = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &k));

    double val = 0.0;
    histo_status_t st = histo_central_moment(wrap->handle, k, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_central_moment failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_skewness(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo_skewness(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_skewness failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_kurtosis(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo_kurtosis(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_kurtosis failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_excess_kurtosis(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo_excess_kurtosis(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_excess_kurtosis failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_mode_bin(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t val = 0;
    histo_status_t st = histo_mode_bin(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_mode_bin failed");
        return NULL;
    }
    napi_value res;
    napi_create_uint32(env, val, &res);
    return res;
}

static napi_value n_histo_mode_continuous(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo_mode_continuous(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_mode_continuous failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_fwhm(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo_fwhm(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_fwhm failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_rms(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo_rms(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_rms failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_quantile(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_quantile requires histogram and probability p in [0, 1]");
        return NULL;
    }
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double p = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &p));

    double val = 0.0;
    histo_status_t st = histo_quantile(wrap->handle, p, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_quantile failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_median(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo_median(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_median failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_iqr(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo_iqr(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_iqr failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_mad(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo_mad(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_mad failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_trimmed_mean(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo_trimmed_mean requires histogram, lower_p, upper_p");
        return NULL;
    }
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double lower_p = 0.0, upper_p = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &lower_p));
    NAPI_CALL(env, napi_get_value_double(env, args[2], &upper_p));

    double val = 0.0;
    histo_status_t st = histo_trimmed_mean(wrap->handle, lower_p, upper_p, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_trimmed_mean failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_winsorized_mean(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo_winsorized_mean requires histogram, lower_p, upper_p");
        return NULL;
    }
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double lower_p = 0.0, upper_p = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &lower_p));
    NAPI_CALL(env, napi_get_value_double(env, args[2], &upper_p));

    double val = 0.0;
    histo_status_t st = histo_winsorized_mean(wrap->handle, lower_p, upper_p, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_winsorized_mean failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_integral(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t nb = histo_nbins(wrap->handle);
    uint32_t start_bin = 0;
    uint32_t end_bin = (nb > 0) ? nb - 1 : 0;

    if (argc >= 3) {
        napi_valuetype vt1, vt2;
        napi_typeof(env, args[1], &vt1);
        napi_typeof(env, args[2], &vt2);
        if (vt1 == napi_number && vt2 == napi_number) {
            napi_get_value_uint32(env, args[1], &start_bin);
            napi_get_value_uint32(env, args[2], &end_bin);
        }
    }

    double val = 0.0;
    histo_status_t st = histo_integral(wrap->handle, start_bin, end_bin, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_integral failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo_get_stats(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    histo_stats_t st_val;
    histo_status_t st = histo_get_stats(wrap->handle, &st_val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_get_stats failed");
        return NULL;
    }

    napi_value res;
    napi_create_object(env, &res);

    napi_value v_nentries, v_totw, v_mean, v_var, v_std, v_min, v_max, v_med;
    napi_create_int64(env, (int64_t)st_val.n_entries, &v_nentries);
    napi_create_double(env, st_val.total_weight, &v_totw);
    napi_create_double(env, st_val.mean, &v_mean);
    napi_create_double(env, st_val.variance, &v_var);
    napi_create_double(env, st_val.std_dev, &v_std);
    napi_create_double(env, st_val.min, &v_min);
    napi_create_double(env, st_val.max, &v_max);
    napi_create_double(env, st_val.median, &v_med);

    napi_set_named_property(env, res, "numEntries", v_nentries);
    napi_set_named_property(env, res, "totalWeight", v_totw);
    napi_set_named_property(env, res, "mean", v_mean);
    napi_set_named_property(env, res, "variance", v_var);
    napi_set_named_property(env, res, "stdDev", v_std);
    napi_set_named_property(env, res, "min", v_min);
    napi_set_named_property(env, res, "max", v_max);
    napi_set_named_property(env, res, "median", v_med);
    return res;
}

/* ========================================================================= */
/* 1D Comparison & Distance Metrics                                          */
/* ========================================================================= */

static napi_value n_histo_cmp_chi2(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_cmp_chi2 requires two histogram instances");
        return NULL;
    }

    HistoWrap *w1 = unwrap_histo(env, args[0]);
    HistoWrap *w2 = unwrap_histo(env, args[1]);
    if (!w1 || !w2) return NULL;

    double chi2 = 0.0;
    uint32_t ndf = 0;
    histo_status_t st = histo_cmp_chi2(w1->handle, w2->handle, &chi2, &ndf);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_cmp_chi2 failed");
        return NULL;
    }

    napi_value res, v_chi2, v_ndf;
    napi_create_object(env, &res);
    napi_create_double(env, chi2, &v_chi2);
    napi_create_uint32(env, ndf, &v_ndf);
    napi_set_named_property(env, res, "chi2", v_chi2);
    napi_set_named_property(env, res, "ndf", v_ndf);
    return res;
}

static napi_value n_histo_cmp_ks(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_cmp_ks requires two histogram instances");
        return NULL;
    }

    HistoWrap *w1 = unwrap_histo(env, args[0]);
    HistoWrap *w2 = unwrap_histo(env, args[1]);
    if (!w1 || !w2) return NULL;

    double ks_stat = 0.0;
    histo_status_t st = histo_cmp_ks(w1->handle, w2->handle, &ks_stat);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_cmp_ks failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, ks_stat, &res);
    return res;
}

static napi_value n_histo_cmp_wasserstein_1d(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_cmp_wasserstein_1d requires two histogram instances");
        return NULL;
    }

    HistoWrap *w1 = unwrap_histo(env, args[0]);
    HistoWrap *w2 = unwrap_histo(env, args[1]);
    if (!w1 || !w2) return NULL;

    double dist = 0.0;
    histo_status_t st = histo_cmp_wasserstein_1d(w1->handle, w2->handle, &dist);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_cmp_wasserstein_1d failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, dist, &res);
    return res;
}

static napi_value n_histo_cmp_kl_divergence(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_cmp_kl_divergence requires two histogram instances");
        return NULL;
    }

    HistoWrap *w1 = unwrap_histo(env, args[0]);
    HistoWrap *w2 = unwrap_histo(env, args[1]);
    if (!w1 || !w2) return NULL;

    double div = 0.0;
    histo_status_t st = histo_cmp_kl_divergence(w1->handle, w2->handle, &div);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_cmp_kl_divergence failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, div, &res);
    return res;
}

static napi_value n_histo_cmp_bhattacharyya(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_cmp_bhattacharyya requires two histogram instances");
        return NULL;
    }

    HistoWrap *w1 = unwrap_histo(env, args[0]);
    HistoWrap *w2 = unwrap_histo(env, args[1]);
    if (!w1 || !w2) return NULL;

    double dist = 0.0;
    histo_status_t st = histo_cmp_bhattacharyya(w1->handle, w2->handle, &dist);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_cmp_bhattacharyya failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, dist, &res);
    return res;
}

/* ========================================================================= */
/* 1D Arithmetic & Transformations                                           */
/* ========================================================================= */

static napi_value n_histo_add(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_add requires destination and source histogram");
        return NULL;
    }

    HistoWrap *dst = unwrap_histo(env, args[0]);
    HistoWrap *src = unwrap_histo(env, args[1]);
    if (!dst || !src) return NULL;

    histo_status_t st = histo_add(dst->handle, src->handle);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_add failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo_subtract(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_subtract requires destination and source histogram");
        return NULL;
    }

    HistoWrap *dst = unwrap_histo(env, args[0]);
    HistoWrap *src = unwrap_histo(env, args[1]);
    if (!dst || !src) return NULL;

    histo_status_t st = histo_subtract(dst->handle, src->handle);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_subtract failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo_multiply(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_multiply requires destination and source histogram");
        return NULL;
    }

    HistoWrap *dst = unwrap_histo(env, args[0]);
    HistoWrap *src = unwrap_histo(env, args[1]);
    if (!dst || !src) return NULL;

    histo_status_t st = histo_multiply(dst->handle, src->handle);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_multiply failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo_divide(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_divide requires destination and source histogram");
        return NULL;
    }

    HistoWrap *dst = unwrap_histo(env, args[0]);
    HistoWrap *src = unwrap_histo(env, args[1]);
    if (!dst || !src) return NULL;

    histo_status_t st = histo_divide(dst->handle, src->handle);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_divide failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo_scale(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_scale requires histogram and scaling factor");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double factor = 1.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &factor));

    histo_status_t st = histo_scale(wrap->handle, factor);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_scale failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo_normalize(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_normalize requires histogram and target area");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double target_area = 1.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &target_area));

    histo_status_t st = histo_normalize(wrap->handle, target_area);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_normalize failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo_rebin(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo_rebin requires histogram and integer factor");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t factor = 1;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &factor));

    histo_t *rebinned = histo_rebin(wrap->handle, factor);
    if (!rebinned) {
        throw_error(env, "Failed to rebin histogram (nbins must be divisible by factor)");
        return NULL;
    }
    return wrap_histo(env, rebinned);
}

static napi_value n_histo_slice(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo_slice requires histogram, start_bin, end_bin, [empty]");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t start_bin = 0, end_bin = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &start_bin));
    NAPI_CALL(env, napi_get_value_uint32(env, args[2], &end_bin));

    bool empty = false;
    if (argc >= 4) {
        napi_get_value_bool(env, args[3], &empty);
    }

    histo_t *sliced = histo_slice(wrap->handle, start_bin, end_bin, empty);
    if (!sliced) {
        throw_error(env, "Failed to slice histogram (check bin index range)");
        return NULL;
    }
    return wrap_histo(env, sliced);
}

static napi_value n_histo_cdf(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) {
        throw_type_error(env, "histo_cdf requires histogram and optional prenormalization target");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    double target = 1.0;
    if (argc >= 2) {
        napi_get_value_double(env, args[1], &target);
    }

    histo_t *cdf_h = histo_cdf(wrap->handle, target);
    if (!cdf_h) {
        throw_error(env, "Failed to construct CDF histogram");
        return NULL;
    }
    return wrap_histo(env, cdf_h);
}

/* ========================================================================= */
/* 1D Serialization & Deserialization                                        */
/* ========================================================================= */

static napi_value n_histo_serialize_binary(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) {
        throw_type_error(env, "histo_serialize_binary requires histogram instance");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    size_t sz = histo_serialize_binary_size(wrap->handle);
    if (sz == 0) {
        throw_error(env, "Failed to calculate serialization size");
        return NULL;
    }

    void *buf_data = NULL;
    napi_value buf;
    if (napi_create_buffer(env, sz, &buf_data, &buf) != napi_ok) {
        throw_error(env, "Failed to allocate Node.js Buffer for serialization");
        return NULL;
    }

    histo_status_t st = histo_serialize_binary_into(wrap->handle, buf_data, sz);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_serialize_binary_into failed");
        return NULL;
    }
    return buf;
}

static napi_value n_histo_deserialize_binary(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) {
        throw_type_error(env, "histo_deserialize_binary requires Buffer");
        return NULL;
    }

    void *data = NULL;
    size_t len = 0;
    bool is_buf = false;
    napi_is_buffer(env, args[0], &is_buf);
    if (is_buf) {
        napi_get_buffer_info(env, args[0], &data, &len);
    } else {
        bool is_typedarray = false;
        napi_is_typedarray(env, args[0], &is_typedarray);
        if (is_typedarray) {
            napi_typedarray_type type;
            napi_get_typedarray_info(env, args[0], &type, &len, &data, NULL, NULL);
        } else {
            throw_type_error(env, "Expected Buffer or Uint8Array for binary deserialization");
            return NULL;
        }
    }

    histo_t *h = NULL;
    histo_status_t st = histo_deserialize_binary(data, len, &h);
    if (st != HISTO_OK || !h) {
        throw_histo_status(env, st, "histo_deserialize_binary failed");
        return NULL;
    }
    return wrap_histo(env, h);
}

static napi_value n_histo_serialize_json(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) {
        throw_type_error(env, "histo_serialize_json requires histogram instance");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    char *json_str = NULL;
    histo_status_t st = histo_serialize_json(wrap->handle, &json_str);
    if (st != HISTO_OK || !json_str) {
        throw_histo_status(env, st, "histo_serialize_json failed");
        return NULL;
    }

    napi_value res;
    napi_status nst = napi_create_string_utf8(env, json_str, strlen(json_str), &res);
    histo_free_buffer(json_str);

    if (nst != napi_ok) {
        throw_error(env, "Failed to create JS string from JSON");
        return NULL;
    }
    return res;
}

static napi_value n_histo_deserialize_json(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) {
        throw_type_error(env, "histo_deserialize_json requires JSON string");
        return NULL;
    }

    size_t str_len = 0;
    NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], NULL, 0, &str_len));
    char *buf = (char *)malloc(str_len + 1);
    if (!buf) {
        throw_error(env, "Out of memory allocating JSON string buffer");
        return NULL;
    }
    NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], buf, str_len + 1, &str_len));

    histo_t *h = NULL;
    histo_status_t st = histo_deserialize_json(buf, &h);
    free(buf);

    if (st != HISTO_OK || !h) {
        throw_histo_status(env, st, "histo_deserialize_json failed");
        return NULL;
    }
    return wrap_histo(env, h);
}

static napi_value n_histo_migrate_binary(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) {
        throw_type_error(env, "histo_migrate_binary requires Buffer");
        return NULL;
    }

    void *data = NULL;
    size_t len = 0;
    bool is_buf = false;
    napi_is_buffer(env, args[0], &is_buf);
    if (is_buf) {
        napi_get_buffer_info(env, args[0], &data, &len);
    } else {
        throw_type_error(env, "Expected Buffer");
        return NULL;
    }

    void *out_buf = NULL;
    size_t out_sz = 0;
    histo_status_t st = histo_migrate_binary(data, len, &out_buf, &out_sz);
    if (st != HISTO_OK || !out_buf) {
        throw_histo_status(env, st, "histo_migrate_binary failed");
        return NULL;
    }

    void *js_buf_data = NULL;
    napi_value res;
    if (napi_create_buffer(env, out_sz, &js_buf_data, &res) != napi_ok) {
        histo_free_buffer(out_buf);
        throw_error(env, "Failed to allocate Node Buffer for migrated payload");
        return NULL;
    }
    memcpy(js_buf_data, out_buf, out_sz);
    histo_free_buffer(out_buf);
    return res;
}

/* ========================================================================= */
/* 2D Histogram Native Functions                                             */
/* ========================================================================= */

static napi_value n_histo2d_create_uniform(napi_env env, napi_callback_info info) {
    size_t argc = 7;
    napi_value args[7];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

    if (argc < 6) {
        throw_type_error(env, "histo2d_create_uniform requires nx, xmin, xmax, ny, ymin, ymax, [flags]");
        return NULL;
    }

    uint32_t nx = 0, ny = 0;
    double xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0;
    uint32_t flags = 0;

    NAPI_CALL(env, napi_get_value_uint32(env, args[0], &nx));
    NAPI_CALL(env, napi_get_value_double(env, args[1], &xmin));
    NAPI_CALL(env, napi_get_value_double(env, args[2], &xmax));
    NAPI_CALL(env, napi_get_value_uint32(env, args[3], &ny));
    NAPI_CALL(env, napi_get_value_double(env, args[4], &ymin));
    NAPI_CALL(env, napi_get_value_double(env, args[5], &ymax));
    if (argc >= 7) {
        napi_get_value_uint32(env, args[6], &flags);
    }

    histo2d_t *h2 = histo2d_create_uniform(nx, xmin, xmax, ny, ymin, ymax, flags);
    if (!h2) {
        throw_error(env, "Failed to create uniform 2D histogram");
        return NULL;
    }
    return wrap_histo2d(env, h2);
}

static napi_value n_histo2d_create_variable(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

    if (argc < 2) {
        throw_type_error(env, "histo2d_create_variable requires xedges, yedges, [flags]");
        return NULL;
    }

    const double *xedges = NULL, *yedges = NULL;
    size_t nx_edges = 0, ny_edges = 0;
    double *x_alloc = NULL, *y_alloc = NULL;

    if (!extract_double_array(env, args[0], &xedges, &nx_edges, &x_alloc)) {
        return NULL;
    }
    if (!extract_double_array(env, args[1], &yedges, &ny_edges, &y_alloc)) {
        free_double_array(x_alloc);
        return NULL;
    }

    if (nx_edges < 2 || ny_edges < 2) {
        free_double_array(x_alloc);
        free_double_array(y_alloc);
        throw_range_error(env, "Edge arrays must each have at least 2 elements");
        return NULL;
    }

    uint32_t flags = 0;
    if (argc >= 3) {
        napi_get_value_uint32(env, args[2], &flags);
    }

    histo2d_t *h2 = histo2d_create_variable((uint32_t)(nx_edges - 1), xedges,
                                           (uint32_t)(ny_edges - 1), yedges, flags);
    free_double_array(x_alloc);
    free_double_array(y_alloc);

    if (!h2) {
        throw_error(env, "Failed to create variable 2D histogram");
        return NULL;
    }
    return wrap_histo2d(env, h2);
}

static napi_value n_histo2d_create_uniform_variable(napi_env env, napi_callback_info info) {
    size_t argc = 5;
    napi_value args[5];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

    if (argc < 4) {
        throw_type_error(env, "histo2d_create_uniform_variable requires nx, xmin, xmax, yedges, [flags]");
        return NULL;
    }

    uint32_t nx = 0;
    double xmin = 0.0, xmax = 0.0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[0], &nx));
    NAPI_CALL(env, napi_get_value_double(env, args[1], &xmin));
    NAPI_CALL(env, napi_get_value_double(env, args[2], &xmax));

    const double *yedges = NULL;
    size_t ny_edges = 0;
    double *y_alloc = NULL;
    if (!extract_double_array(env, args[3], &yedges, &ny_edges, &y_alloc)) {
        return NULL;
    }

    if (ny_edges < 2) {
        free_double_array(y_alloc);
        throw_range_error(env, "yedges array must have at least 2 elements");
        return NULL;
    }

    uint32_t flags = 0;
    if (argc >= 5) {
        napi_get_value_uint32(env, args[4], &flags);
    }

    histo2d_t *h2 = histo2d_create_uniform_variable(nx, xmin, xmax, (uint32_t)(ny_edges - 1), yedges, flags);
    free_double_array(y_alloc);

    if (!h2) {
        throw_error(env, "Failed to create Uniform-Variable 2D histogram");
        return NULL;
    }
    return wrap_histo2d(env, h2);
}

static napi_value n_histo2d_create_variable_uniform(napi_env env, napi_callback_info info) {
    size_t argc = 5;
    napi_value args[5];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

    if (argc < 4) {
        throw_type_error(env, "histo2d_create_variable_uniform requires xedges, ny, ymin, ymax, [flags]");
        return NULL;
    }

    const double *xedges = NULL;
    size_t nx_edges = 0;
    double *x_alloc = NULL;
    if (!extract_double_array(env, args[0], &xedges, &nx_edges, &x_alloc)) {
        return NULL;
    }

    if (nx_edges < 2) {
        free_double_array(x_alloc);
        throw_range_error(env, "xedges array must have at least 2 elements");
        return NULL;
    }

    uint32_t ny = 0;
    double ymin = 0.0, ymax = 0.0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &ny));
    NAPI_CALL(env, napi_get_value_double(env, args[2], &ymin));
    NAPI_CALL(env, napi_get_value_double(env, args[3], &ymax));

    uint32_t flags = 0;
    if (argc >= 5) {
        napi_get_value_uint32(env, args[4], &flags);
    }

    histo2d_t *h2 = histo2d_create_variable_uniform((uint32_t)(nx_edges - 1), xedges, ny, ymin, ymax, flags);
    free_double_array(x_alloc);

    if (!h2) {
        throw_error(env, "Failed to create Variable-Uniform 2D histogram");
        return NULL;
    }
    return wrap_histo2d(env, h2);
}

static napi_value n_histo2d_destroy(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) return NULL;

    Histo2DWrap *wrap = NULL;
    if (napi_unwrap(env, args[0], (void **)&wrap) == napi_ok && wrap) {
        if (!wrap->destroyed && wrap->handle) {
            histo2d_destroy(wrap->handle);
            wrap->handle = NULL;
            wrap->destroyed = true;
        }
    }
    return NULL;
}

static napi_value n_histo2d_clone(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) return NULL;

    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    bool empty = false;
    if (argc >= 2) {
        napi_get_value_bool(env, args[1], &empty);
    }

    histo2d_t *cloned = histo2d_clone(wrap->handle, empty);
    if (!cloned) {
        throw_error(env, "Failed to clone 2D histogram");
        return NULL;
    }
    return wrap_histo2d(env, cloned);
}

static napi_value n_histo2d_reset(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    histo_status_t st = histo2d_reset(wrap->handle);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_reset failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo2d_fill(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo2d_fill requires histogram, x, y, [weight]");
        return NULL;
    }

    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double x = 0.0, y = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &x));
    NAPI_CALL(env, napi_get_value_double(env, args[2], &y));

    histo_status_t st;
    if (argc >= 4) {
        napi_valuetype vt;
        napi_typeof(env, args[3], &vt);
        if (vt == napi_number) {
            double weight = 1.0;
            napi_get_value_double(env, args[3], &weight);
            st = histo2d_fill_w(wrap->handle, x, y, weight);
        } else {
            st = histo2d_fill(wrap->handle, x, y);
        }
    } else {
        st = histo2d_fill(wrap->handle, x, y);
    }

    if (st != HISTO_OK && st != HISTO_WARN_NON_FINITE) {
        throw_histo_status(env, st, "histo2d_fill failed");
        return NULL;
    }
    napi_value ret;
    napi_create_int32(env, (int32_t)st, &ret);
    return ret;
}

static napi_value n_histo2d_fill_n(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo2d_fill_n requires histogram, x_array, y_array, [weights]");
        return NULL;
    }

    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    const double *x = NULL, *y = NULL;
    size_t nx = 0, ny = 0;
    double *x_alloc = NULL, *y_alloc = NULL;

    if (!extract_double_array(env, args[1], &x, &nx, &x_alloc)) {
        return NULL;
    }
    if (!extract_double_array(env, args[2], &y, &ny, &y_alloc)) {
        free_double_array(x_alloc);
        return NULL;
    }

    if (nx != ny) {
        free_double_array(x_alloc);
        free_double_array(y_alloc);
        throw_range_error(env, "X and Y coordinate arrays must have the same length");
        return NULL;
    }

    const double *weights = NULL;
    size_t nw = 0;
    double *w_alloc = NULL;
    if (argc >= 4) {
        napi_valuetype vt;
        napi_typeof(env, args[3], &vt);
        if (vt != napi_undefined && vt != napi_null) {
            if (!extract_double_array(env, args[3], &weights, &nw, &w_alloc)) {
                free_double_array(x_alloc);
                free_double_array(y_alloc);
                return NULL;
            }
            if (nw != nx) {
                free_double_array(x_alloc);
                free_double_array(y_alloc);
                free_double_array(w_alloc);
                throw_range_error(env, "Weights array length must match coordinate array length");
                return NULL;
            }
        }
    }

    histo_status_t st = histo2d_fill_n(wrap->handle, nx, x, y, weights);
    free_double_array(x_alloc);
    free_double_array(y_alloc);
    free_double_array(w_alloc);

    if (st != HISTO_OK && st != HISTO_WARN_NON_FINITE) {
        throw_histo_status(env, st, "histo2d_fill_n failed");
        return NULL;
    }
    napi_value ret;
    napi_create_int32(env, (int32_t)st, &ret);
    return ret;
}

static napi_value n_histo2d_fill_bin(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 4) {
        throw_type_error(env, "histo2d_fill_bin requires histogram, ix, iy, weight");
        return NULL;
    }

    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint32_t ix = 0, iy = 0;
    double weight = 0.0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &ix));
    NAPI_CALL(env, napi_get_value_uint32(env, args[2], &iy));
    NAPI_CALL(env, napi_get_value_double(env, args[3], &weight));

    histo_status_t st = histo2d_fill_bin(wrap->handle, ix, iy, weight);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_fill_bin failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo2d_nbins_x(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint32_t nx = histo2d_nbins_x(wrap->handle);
    napi_value res;
    napi_create_uint32(env, nx, &res);
    return res;
}

static napi_value n_histo2d_nbins_y(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint32_t ny = histo2d_nbins_y(wrap->handle);
    napi_value res;
    napi_create_uint32(env, ny, &res);
    return res;
}

static napi_value n_histo2d_find_bin(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo2d_find_bin requires histogram, x, y");
        return NULL;
    }
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double x = 0.0, y = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &x));
    NAPI_CALL(env, napi_get_value_double(env, args[2], &y));

    int64_t ix = 0, iy = 0;
    histo_status_t st = histo2d_find_bin(wrap->handle, x, y, &ix, &iy);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_find_bin failed");
        return NULL;
    }

    napi_value res, v_ix, v_iy;
    napi_create_object(env, &res);
    napi_create_int64(env, ix, &v_ix);
    napi_create_int64(env, iy, &v_iy);
    napi_set_named_property(env, res, "ix", v_ix);
    napi_set_named_property(env, res, "iy", v_iy);
    return res;
}

static napi_value n_histo2d_find_region(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo2d_find_region requires histogram, x, y");
        return NULL;
    }
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double x = 0.0, y = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &x));
    NAPI_CALL(env, napi_get_value_double(env, args[2], &y));

    histo2d_region_t reg;
    histo_status_t st = histo2d_find_region(wrap->handle, x, y, &reg);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_find_region failed");
        return NULL;
    }

    napi_value res;
    napi_create_int32(env, (int32_t)reg, &res);
    return res;
}

static napi_value n_histo2d_bin_content(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo2d_bin_content requires histogram, ix, iy");
        return NULL;
    }
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint32_t ix = 0, iy = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &ix));
    NAPI_CALL(env, napi_get_value_uint32(env, args[2], &iy));

    double content = 0.0;
    histo_status_t st = histo2d_bin_content(wrap->handle, ix, iy, &content);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_bin_content failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, content, &res);
    return res;
}

static napi_value n_histo2d_bin_error(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo2d_bin_error requires histogram, ix, iy");
        return NULL;
    }
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint32_t ix = 0, iy = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &ix));
    NAPI_CALL(env, napi_get_value_uint32(env, args[2], &iy));

    double error = 0.0;
    histo_status_t st = histo2d_bin_error(wrap->handle, ix, iy, &error);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_bin_error failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, error, &res);
    return res;
}

static napi_value n_histo2d_bin_sum_w2(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo2d_bin_sum_w2 requires histogram, ix, iy");
        return NULL;
    }
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint32_t ix = 0, iy = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &ix));
    NAPI_CALL(env, napi_get_value_uint32(env, args[2], &iy));

    double sum_w2 = 0.0;
    histo_status_t st = histo2d_bin_sum_w2(wrap->handle, ix, iy, &sum_w2);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_bin_sum_w2 failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, sum_w2, &res);
    return res;
}

static napi_value n_histo2d_bin_bounds(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo2d_bin_bounds requires histogram, ix, iy");
        return NULL;
    }
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint32_t ix = 0, iy = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &ix));
    NAPI_CALL(env, napi_get_value_uint32(env, args[2], &iy));

    double xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0;
    histo_status_t st = histo2d_bin_bounds(wrap->handle, ix, iy, &xmin, &xmax, &ymin, &ymax);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_bin_bounds failed");
        return NULL;
    }

    napi_value res, v_xmin, v_xmax, v_ymin, v_ymax;
    napi_create_object(env, &res);
    napi_create_double(env, xmin, &v_xmin);
    napi_create_double(env, xmax, &v_xmax);
    napi_create_double(env, ymin, &v_ymin);
    napi_create_double(env, ymax, &v_ymax);

    napi_set_named_property(env, res, "xmin", v_xmin);
    napi_set_named_property(env, res, "xmax", v_xmax);
    napi_set_named_property(env, res, "ymin", v_ymin);
    napi_set_named_property(env, res, "ymax", v_ymax);
    return res;
}

static napi_value n_histo2d_bin_center(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo2d_bin_center requires histogram, ix, iy");
        return NULL;
    }
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint32_t ix = 0, iy = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &ix));
    NAPI_CALL(env, napi_get_value_uint32(env, args[2], &iy));

    double cx = 0.0, cy = 0.0;
    histo_status_t st = histo2d_bin_center(wrap->handle, ix, iy, &cx, &cy);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_bin_center failed");
        return NULL;
    }

    napi_value res, v_cx, v_cy;
    napi_create_object(env, &res);
    napi_create_double(env, cx, &v_cx);
    napi_create_double(env, cy, &v_cy);
    napi_set_named_property(env, res, "x", v_cx);
    napi_set_named_property(env, res, "y", v_cy);
    return res;
}

static napi_value n_histo2d_region_content(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo2d_region_content requires histogram and region enum");
        return NULL;
    }
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint32_t reg = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &reg));

    double weight = 0.0;
    uint64_t count = 0;
    histo_status_t st = histo2d_region_content(wrap->handle, (histo2d_region_t)reg, &weight, &count);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_region_content failed");
        return NULL;
    }

    napi_value res, v_w, v_c;
    napi_create_object(env, &res);
    napi_create_double(env, weight, &v_w);
    napi_create_int64(env, (int64_t)count, &v_c);
    napi_set_named_property(env, res, "weight", v_w);
    napi_set_named_property(env, res, "count", v_c);
    return res;
}

static napi_value n_histo2d_total_weight(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double tw = histo2d_total_weight(wrap->handle);
    napi_value res;
    napi_create_double(env, tw, &res);
    return res;
}

static napi_value n_histo2d_num_entries(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint64_t n = histo2d_num_entries(wrap->handle);
    napi_value res;
    napi_create_int64(env, (int64_t)n, &res);
    return res;
}

static napi_value n_histo2d_nan_count(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint64_t n = histo2d_nan_count(wrap->handle);
    napi_value res;
    napi_create_int64(env, (int64_t)n, &res);
    return res;
}

static napi_value n_histo2d_mean_x(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo2d_mean_x(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_mean_x failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo2d_mean_y(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo2d_mean_y(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_mean_y failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo2d_variance_x(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo2d_variance_x(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_variance_x failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo2d_variance_y(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo2d_variance_y(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_variance_y failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo2d_std_dev_x(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo2d_std_dev_x(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_std_dev_x failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo2d_std_dev_y(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo2d_std_dev_y(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_std_dev_y failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo2d_covariance(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo2d_covariance(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_covariance failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo2d_correlation(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st = histo2d_correlation(wrap->handle, &val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_correlation failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo2d_integral(napi_env env, napi_callback_info info) {
    size_t argc = 5;
    napi_value args[5];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double val = 0.0;
    histo_status_t st;
    bool has_range = false;
    if (argc >= 5) {
        napi_valuetype vt1, vt2, vt3, vt4;
        napi_typeof(env, args[1], &vt1);
        napi_typeof(env, args[2], &vt2);
        napi_typeof(env, args[3], &vt3);
        napi_typeof(env, args[4], &vt4);
        if (vt1 == napi_number && vt2 == napi_number && vt3 == napi_number && vt4 == napi_number) {
            uint32_t ix_min = 0, ix_max = 0, iy_min = 0, iy_max = 0;
            napi_get_value_uint32(env, args[1], &ix_min);
            napi_get_value_uint32(env, args[2], &ix_max);
            napi_get_value_uint32(env, args[3], &iy_min);
            napi_get_value_uint32(env, args[4], &iy_max);
            st = histo2d_integral_range(wrap->handle, ix_min, ix_max, iy_min, iy_max, &val);
            has_range = true;
        }
    }
    if (!has_range) {
        st = histo2d_integral(wrap->handle, &val);
    }

    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_integral failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_histo2d_get_stats(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    histo2d_stats_t st_val;
    histo_status_t st = histo2d_get_stats(wrap->handle, &st_val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_get_stats failed");
        return NULL;
    }

    napi_value res;
    napi_create_object(env, &res);

    napi_value v;
    napi_create_int64(env, (int64_t)st_val.n_entries, &v);
    napi_set_named_property(env, res, "numEntries", v);
    napi_create_double(env, st_val.total_weight, &v);
    napi_set_named_property(env, res, "totalWeight", v);
    napi_create_double(env, st_val.mean_x, &v);
    napi_set_named_property(env, res, "meanX", v);
    napi_create_double(env, st_val.mean_y, &v);
    napi_set_named_property(env, res, "meanY", v);
    napi_create_double(env, st_val.variance_x, &v);
    napi_set_named_property(env, res, "varianceX", v);
    napi_create_double(env, st_val.variance_y, &v);
    napi_set_named_property(env, res, "varianceY", v);
    napi_create_double(env, st_val.std_dev_x, &v);
    napi_set_named_property(env, res, "stdDevX", v);
    napi_create_double(env, st_val.std_dev_y, &v);
    napi_set_named_property(env, res, "stdDevY", v);
    napi_create_double(env, st_val.covariance, &v);
    napi_set_named_property(env, res, "covariance", v);
    napi_create_double(env, st_val.correlation, &v);
    napi_set_named_property(env, res, "correlation", v);
    napi_create_double(env, st_val.min_x, &v);
    napi_set_named_property(env, res, "minX", v);
    napi_create_double(env, st_val.max_x, &v);
    napi_set_named_property(env, res, "maxX", v);
    napi_create_double(env, st_val.min_y, &v);
    napi_set_named_property(env, res, "minY", v);
    napi_create_double(env, st_val.max_y, &v);
    napi_set_named_property(env, res, "maxY", v);

    return res;
}

/* ========================================================================= */
/* 2D Projections, Slices & Profiles                                         */
/* ========================================================================= */

static napi_value n_histo2d_project_x(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    histo_t *h1d = NULL;
    histo_status_t st = histo2d_project_x(wrap->handle, &h1d);
    if (st != HISTO_OK || !h1d) {
        throw_histo_status(env, st, "histo2d_project_x failed");
        return NULL;
    }
    return wrap_histo(env, h1d);
}

static napi_value n_histo2d_project_y(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    histo_t *h1d = NULL;
    histo_status_t st = histo2d_project_y(wrap->handle, &h1d);
    if (st != HISTO_OK || !h1d) {
        throw_histo_status(env, st, "histo2d_project_y failed");
        return NULL;
    }
    return wrap_histo(env, h1d);
}

static napi_value n_histo2d_slice_x(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo2d_slice_x requires histogram, iy_min, iy_max");
        return NULL;
    }
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint32_t iy_min = 0, iy_max = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &iy_min));
    NAPI_CALL(env, napi_get_value_uint32(env, args[2], &iy_max));

    histo_t *h1d = NULL;
    histo_status_t st = histo2d_slice_x(wrap->handle, iy_min, iy_max, &h1d);
    if (st != HISTO_OK || !h1d) {
        throw_histo_status(env, st, "histo2d_slice_x failed");
        return NULL;
    }
    return wrap_histo(env, h1d);
}

static napi_value n_histo2d_slice_y(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo2d_slice_y requires histogram, ix_min, ix_max");
        return NULL;
    }
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint32_t ix_min = 0, ix_max = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &ix_min));
    NAPI_CALL(env, napi_get_value_uint32(env, args[2], &ix_max));

    histo_t *h1d = NULL;
    histo_status_t st = histo2d_slice_y(wrap->handle, ix_min, ix_max, &h1d);
    if (st != HISTO_OK || !h1d) {
        throw_histo_status(env, st, "histo2d_slice_y failed");
        return NULL;
    }
    return wrap_histo(env, h1d);
}

static napi_value n_histo2d_profile_x(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    histo_t *h1d = NULL;
    histo_status_t st = histo2d_profile_x(wrap->handle, &h1d);
    if (st != HISTO_OK || !h1d) {
        throw_histo_status(env, st, "histo2d_profile_x failed");
        return NULL;
    }
    return wrap_histo(env, h1d);
}

static napi_value n_histo2d_profile_y(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    histo_t *h1d = NULL;
    histo_status_t st = histo2d_profile_y(wrap->handle, &h1d);
    if (st != HISTO_OK || !h1d) {
        throw_histo_status(env, st, "histo2d_profile_y failed");
        return NULL;
    }
    return wrap_histo(env, h1d);
}

/* ========================================================================= */
/* 2D Arithmetic & Transformations                                           */
/* ========================================================================= */

static napi_value n_histo2d_scale(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo2d_scale requires histogram and factor");
        return NULL;
    }
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double factor = 1.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &factor));

    histo_status_t st = histo2d_scale(wrap->handle, factor);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_scale failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo2d_normalize(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo2d_normalize requires histogram and target integral");
        return NULL;
    }
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    double target = 1.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &target));

    histo_status_t st = histo2d_normalize(wrap->handle, target);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_normalize failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo2d_rebin(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "histo2d_rebin requires histogram, factor_x, factor_y");
        return NULL;
    }
    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    uint32_t fx = 1, fy = 1;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &fx));
    NAPI_CALL(env, napi_get_value_uint32(env, args[2], &fy));

    histo2d_t *rebinned = NULL;
    histo_status_t st = histo2d_rebin(wrap->handle, fx, fy, &rebinned);
    if (st != HISTO_OK || !rebinned) {
        throw_histo_status(env, st, "histo2d_rebin failed");
        return NULL;
    }
    return wrap_histo2d(env, rebinned);
}

static napi_value n_histo2d_add(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo2d_add requires target, source, [scale]");
        return NULL;
    }
    Histo2DWrap *dst = unwrap_histo2d(env, args[0]);
    Histo2DWrap *src = unwrap_histo2d(env, args[1]);
    if (!dst || !src) return NULL;

    double scale = 1.0;
    if (argc >= 3) {
        napi_get_value_double(env, args[2], &scale);
    }

    histo_status_t st = histo2d_add(dst->handle, src->handle, scale);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_add failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo2d_subtract(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo2d_subtract requires target, source");
        return NULL;
    }
    Histo2DWrap *dst = unwrap_histo2d(env, args[0]);
    Histo2DWrap *src = unwrap_histo2d(env, args[1]);
    if (!dst || !src) return NULL;

    histo_status_t st = histo2d_subtract(dst->handle, src->handle);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_subtract failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo2d_multiply(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo2d_multiply requires target, source");
        return NULL;
    }
    Histo2DWrap *dst = unwrap_histo2d(env, args[0]);
    Histo2DWrap *src = unwrap_histo2d(env, args[1]);
    if (!dst || !src) return NULL;

    histo_status_t st = histo2d_multiply(dst->handle, src->handle);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_multiply failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_histo2d_divide(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "histo2d_divide requires target, source");
        return NULL;
    }
    Histo2DWrap *dst = unwrap_histo2d(env, args[0]);
    Histo2DWrap *src = unwrap_histo2d(env, args[1]);
    if (!dst || !src) return NULL;

    histo_status_t st = histo2d_divide(dst->handle, src->handle);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo2d_divide failed");
        return NULL;
    }
    return NULL;
}

/* ========================================================================= */
/* 2D Serialization                                                          */
/* ========================================================================= */

static napi_value n_histo2d_serialize_binary(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) return NULL;

    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    void *out_buf = NULL;
    size_t out_sz = 0;
    histo_status_t st = histo2d_serialize_binary_alloc(wrap->handle, &out_buf, &out_sz);
    if (st != HISTO_OK || !out_buf) {
        throw_histo_status(env, st, "histo2d_serialize_binary failed");
        return NULL;
    }

    void *js_buf_data = NULL;
    napi_value res;
    if (napi_create_buffer(env, out_sz, &js_buf_data, &res) != napi_ok) {
        histo_free_buffer(out_buf);
        throw_error(env, "Failed to create Node.js Buffer for 2D serialization");
        return NULL;
    }
    memcpy(js_buf_data, out_buf, out_sz);
    histo_free_buffer(out_buf);
    return res;
}

static napi_value n_histo2d_deserialize_binary(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) return NULL;

    void *data = NULL;
    size_t len = 0;
    bool is_buf = false;
    napi_is_buffer(env, args[0], &is_buf);
    if (is_buf) {
        napi_get_buffer_info(env, args[0], &data, &len);
    } else {
        bool is_typedarray = false;
        napi_is_typedarray(env, args[0], &is_typedarray);
        if (is_typedarray) {
            napi_typedarray_type type;
            napi_get_typedarray_info(env, args[0], &type, &len, &data, NULL, NULL);
        } else {
            throw_type_error(env, "Expected Buffer or Uint8Array for 2D binary deserialization");
            return NULL;
        }
    }

    histo2d_t *h2 = NULL;
    histo_status_t st = histo2d_deserialize_binary(data, len, &h2);
    if (st != HISTO_OK || !h2) {
        throw_histo_status(env, st, "histo2d_deserialize_binary failed");
        return NULL;
    }
    return wrap_histo2d(env, h2);
}

static napi_value n_histo2d_serialize_json(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) return NULL;

    Histo2DWrap *wrap = unwrap_histo2d(env, args[0]);
    if (!wrap) return NULL;

    char *out_str = NULL;
    size_t out_sz = 0;
    histo_status_t st = histo2d_serialize_json_alloc(wrap->handle, &out_str, &out_sz);
    if (st != HISTO_OK || !out_str) {
        throw_histo_status(env, st, "histo2d_serialize_json failed");
        return NULL;
    }

    napi_value res;
    napi_status nst = napi_create_string_utf8(env, out_str, out_sz, &res);
    histo_free_buffer(out_str);

    if (nst != napi_ok) {
        throw_error(env, "Failed to create JS string from 2D JSON");
        return NULL;
    }
    return res;
}

static napi_value n_histo2d_deserialize_json(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) return NULL;

    size_t str_len = 0;
    NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], NULL, 0, &str_len));
    char *buf = (char *)malloc(str_len + 1);
    if (!buf) {
        throw_error(env, "Out of memory allocating 2D JSON string buffer");
        return NULL;
    }
    NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], buf, str_len + 1, &str_len));

    histo2d_t *h2 = NULL;
    histo_status_t st = histo2d_deserialize_json(buf, &h2);
    free(buf);

    if (st != HISTO_OK || !h2) {
        throw_histo_status(env, st, "histo2d_deserialize_json failed");
        return NULL;
    }
    return wrap_histo2d(env, h2);
}

/* ========================================================================= */
/* KDE (Kernel Density Estimation) Native Functions                          */
/* ========================================================================= */

static void parse_kde_options(napi_env env, napi_value opts_val, histo_kde_options_t *opts) {
    *opts = histo_kde_default_options();
    if (!opts_val) return;

    napi_valuetype vt;
    napi_typeof(env, opts_val, &vt);
    if (vt != napi_object) return;

    bool has_prop = false;
    napi_value val;

    if (napi_has_named_property(env, opts_val, "kernel", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "kernel", &val);
        uint32_t k = 0;
        if (napi_get_value_uint32(env, val, &k) == napi_ok) {
            opts->kernel = (histo_kde_kernel_t)k;
        }
    }
    if (napi_has_named_property(env, opts_val, "bwMethod", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "bwMethod", &val);
        uint32_t bwm = 0;
        if (napi_get_value_uint32(env, val, &bwm) == napi_ok) {
            opts->bw_method = (histo_kde_bandwidth_method_t)bwm;
        }
    }
    if (napi_has_named_property(env, opts_val, "bandwidth", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "bandwidth", &val);
        double bw = 0.0;
        if (napi_get_value_double(env, val, &bw) == napi_ok && bw > 0.0) {
            opts->bandwidth = bw;
            opts->bw_method = HISTO_KDE_BANDWIDTH_MANUAL;
        }
    }
    if (napi_has_named_property(env, opts_val, "bwAdjust", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "bwAdjust", &val);
        double bwa = 1.0;
        if (napi_get_value_double(env, val, &bwa) == napi_ok && bwa > 0.0) {
            opts->bw_adjust = bwa;
        }
    }
}

static napi_value n_kde_create(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) {
        throw_type_error(env, "kde_create requires samples array, [weights], [options]");
        return NULL;
    }

    const double *samples = NULL;
    size_t n = 0;
    double *s_alloc = NULL;
    if (!extract_double_array(env, args[0], &samples, &n, &s_alloc)) {
        return NULL;
    }

    const double *weights = NULL;
    size_t nw = 0;
    double *w_alloc = NULL;
    if (argc >= 2) {
        napi_valuetype vt;
        napi_typeof(env, args[1], &vt);
        if (vt != napi_undefined && vt != napi_null) {
            if (!extract_double_array(env, args[1], &weights, &nw, &w_alloc)) {
                free_double_array(s_alloc);
                return NULL;
            }
            if (nw != n) {
                free_double_array(s_alloc);
                free_double_array(w_alloc);
                throw_range_error(env, "Weights array length must match samples array length");
                return NULL;
            }
        }
    }

    histo_kde_options_t opts;
    parse_kde_options(env, (argc >= 3) ? args[2] : NULL, &opts);

    histo_kde_t *kde = histo_kde_create(n, samples, weights, &opts);
    free_double_array(s_alloc);
    free_double_array(w_alloc);

    if (!kde) {
        throw_error(env, "Failed to create KDE model");
        return NULL;
    }
    return wrap_kde(env, kde);
}

static napi_value n_kde_create_from_histo(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) {
        throw_type_error(env, "kde_create_from_histo requires histogram instance, [options]");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    histo_kde_options_t opts;
    parse_kde_options(env, (argc >= 2) ? args[1] : NULL, &opts);

    histo_kde_t *kde = histo_kde_create_from_histo(wrap->handle, &opts);
    if (!kde) {
        throw_error(env, "Failed to create KDE model from histogram");
        return NULL;
    }
    return wrap_kde(env, kde);
}

static napi_value n_kde_destroy(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) return NULL;

    KDEWrap *wrap = NULL;
    if (napi_unwrap(env, args[0], (void **)&wrap) == napi_ok && wrap) {
        if (!wrap->destroyed && wrap->handle) {
            histo_kde_destroy(wrap->handle);
            wrap->handle = NULL;
            wrap->destroyed = true;
        }
    }
    return NULL;
}

static napi_value n_kde_eval(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "kde_eval requires KDE model and x coordinate");
        return NULL;
    }

    KDEWrap *wrap = unwrap_kde(env, args[0]);
    if (!wrap) return NULL;

    double x = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &x));

    double pdf = histo_kde_eval(wrap->handle, x);
    napi_value res;
    napi_create_double(env, pdf, &res);
    return res;
}

static napi_value n_kde_eval_n(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "kde_eval_n requires KDE model and input coordinates array");
        return NULL;
    }

    KDEWrap *wrap = unwrap_kde(env, args[0]);
    if (!wrap) return NULL;

    const double *x_in = NULL;
    size_t n = 0;
    double *x_alloc = NULL;
    if (!extract_double_array(env, args[1], &x_in, &n, &x_alloc)) {
        return NULL;
    }

    napi_value arraybuffer;
    void *data_out = NULL;
    if (napi_create_arraybuffer(env, n * sizeof(double), &data_out, &arraybuffer) != napi_ok) {
        free_double_array(x_alloc);
        throw_error(env, "Failed to allocate ArrayBuffer for KDE eval_n results");
        return NULL;
    }

    napi_value typedarray;
    if (napi_create_typedarray(env, napi_float64_array, n, arraybuffer, 0, &typedarray) != napi_ok) {
        free_double_array(x_alloc);
        throw_error(env, "Failed to allocate Float64Array for KDE eval_n results");
        return NULL;
    }

    histo_status_t st = histo_kde_eval_n(wrap->handle, n, x_in, (double *)data_out);
    free_double_array(x_alloc);

    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_kde_eval_n failed");
        return NULL;
    }
    return typedarray;
}

static napi_value n_kde_cdf(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "kde_cdf requires KDE model and x coordinate");
        return NULL;
    }

    KDEWrap *wrap = unwrap_kde(env, args[0]);
    if (!wrap) return NULL;

    double x = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &x));

    double cdf = histo_kde_cdf(wrap->handle, x);
    napi_value res;
    napi_create_double(env, cdf, &res);
    return res;
}

static napi_value n_kde_quantile(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "kde_quantile requires KDE model and probability q in [0, 1]");
        return NULL;
    }

    KDEWrap *wrap = unwrap_kde(env, args[0]);
    if (!wrap) return NULL;

    double q = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &q));

    double out_val = 0.0;
    histo_status_t st = histo_kde_quantile(wrap->handle, q, &out_val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_kde_quantile failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, out_val, &res);
    return res;
}

static napi_value n_kde_sample(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "kde_sample requires KDE model and count n, [seed]");
        return NULL;
    }

    KDEWrap *wrap = unwrap_kde(env, args[0]);
    if (!wrap) return NULL;

    uint32_t n = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &n));

    int64_t seed = 0;
    if (argc >= 3) {
        napi_get_value_int64(env, args[2], &seed);
    }

    napi_value arraybuffer;
    void *data_out = NULL;
    if (napi_create_arraybuffer(env, n * sizeof(double), &data_out, &arraybuffer) != napi_ok) {
        throw_error(env, "Failed to allocate ArrayBuffer for KDE samples");
        return NULL;
    }

    napi_value typedarray;
    if (napi_create_typedarray(env, napi_float64_array, n, arraybuffer, 0, &typedarray) != napi_ok) {
        throw_error(env, "Failed to allocate Float64Array for KDE samples");
        return NULL;
    }

    histo_status_t st = histo_kde_sample(wrap->handle, n, (double *)data_out, (uint64_t)seed);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_kde_sample failed");
        return NULL;
    }
    return typedarray;
}

static napi_value n_kde_get_bandwidth(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    KDEWrap *wrap = unwrap_kde(env, args[0]);
    if (!wrap) return NULL;

    double bw = histo_kde_get_bandwidth(wrap->handle);
    napi_value res;
    napi_create_double(env, bw, &res);
    return res;
}

static napi_value n_kde_get_kernel(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    KDEWrap *wrap = unwrap_kde(env, args[0]);
    if (!wrap) return NULL;

    histo_kde_kernel_t k = histo_kde_get_kernel(wrap->handle);
    napi_value res;
    napi_create_int32(env, (int32_t)k, &res);
    return res;
}

static napi_value n_kde_num_points(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    KDEWrap *wrap = unwrap_kde(env, args[0]);
    if (!wrap) return NULL;

    size_t np = histo_kde_num_points(wrap->handle);
    napi_value res;
    napi_create_int64(env, (int64_t)np, &res);
    return res;
}

/* ========================================================================= */
/* DDSketch Native Functions                                                 */
/* ========================================================================= */

static napi_value n_sketch_create(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "sketch_create requires alpha (relative error, e.g. 0.01) and max_bins");
        return NULL;
    }

    double alpha = 0.01;
    uint32_t max_bins = 2048;
    NAPI_CALL(env, napi_get_value_double(env, args[0], &alpha));
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &max_bins));

    histo_sketch_t *s = histo_sketch_create(alpha, max_bins);
    if (!s) {
        throw_error(env, "Failed to create sketch (alpha must be in (0, 1) and max_bins >= 1)");
        return NULL;
    }
    return wrap_sketch(env, s);
}

static napi_value n_sketch_destroy(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) return NULL;

    SketchWrap *wrap = NULL;
    if (napi_unwrap(env, args[0], (void **)&wrap) == napi_ok && wrap) {
        if (!wrap->destroyed && wrap->handle) {
            histo_sketch_destroy(wrap->handle);
            wrap->handle = NULL;
            wrap->destroyed = true;
        }
    }
    return NULL;
}

static napi_value n_sketch_insert(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "sketch_insert requires sketch and value, [weight]");
        return NULL;
    }

    SketchWrap *wrap = unwrap_sketch(env, args[0]);
    if (!wrap) return NULL;

    double value = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &value));

    histo_status_t st;
    if (argc >= 3) {
        napi_valuetype vt;
        napi_typeof(env, args[2], &vt);
        if (vt == napi_number) {
            double weight = 1.0;
            napi_get_value_double(env, args[2], &weight);
            st = histo_sketch_insert_w(wrap->handle, value, weight);
        } else {
            st = histo_sketch_insert(wrap->handle, value);
        }
    } else {
        st = histo_sketch_insert(wrap->handle, value);
    }

    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_sketch_insert failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_sketch_insert_n(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "sketch_insert_n requires sketch and values array, [weights]");
        return NULL;
    }

    SketchWrap *wrap = unwrap_sketch(env, args[0]);
    if (!wrap) return NULL;

    const double *values = NULL;
    size_t n = 0;
    double *v_alloc = NULL;
    if (!extract_double_array(env, args[1], &values, &n, &v_alloc)) {
        return NULL;
    }

    const double *weights = NULL;
    size_t nw = 0;
    double *w_alloc = NULL;
    if (argc >= 3) {
        napi_valuetype vt;
        napi_typeof(env, args[2], &vt);
        if (vt != napi_undefined && vt != napi_null) {
            if (!extract_double_array(env, args[2], &weights, &nw, &w_alloc)) {
                free_double_array(v_alloc);
                return NULL;
            }
            if (nw != n) {
                free_double_array(v_alloc);
                free_double_array(w_alloc);
                throw_range_error(env, "Weights array length must match values array length");
                return NULL;
            }
        }
    }

    histo_status_t st = histo_sketch_insert_n(wrap->handle, n, values, weights);
    free_double_array(v_alloc);
    free_double_array(w_alloc);

    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_sketch_insert_n failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_sketch_quantile(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "sketch_quantile requires sketch and quantile rank q in [0, 1]");
        return NULL;
    }

    SketchWrap *wrap = unwrap_sketch(env, args[0]);
    if (!wrap) return NULL;

    double q = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &q));

    double out_val = 0.0;
    histo_status_t st = histo_sketch_quantile(wrap->handle, q, &out_val);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_sketch_quantile failed");
        return NULL;
    }
    napi_value res;
    napi_create_double(env, out_val, &res);
    return res;
}

static napi_value n_sketch_merge(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "sketch_merge requires dest and src sketch instances");
        return NULL;
    }

    SketchWrap *dest = unwrap_sketch(env, args[0]);
    SketchWrap *src = unwrap_sketch(env, args[1]);
    if (!dest || !src) return NULL;

    histo_status_t st = histo_sketch_merge(dest->handle, src->handle);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_sketch_merge failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_sketch_reset(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    SketchWrap *wrap = unwrap_sketch(env, args[0]);
    if (!wrap) return NULL;

    histo_status_t st = histo_sketch_reset(wrap->handle);
    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_sketch_reset failed");
        return NULL;
    }
    return NULL;
}

static napi_value n_sketch_min(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    SketchWrap *wrap = unwrap_sketch(env, args[0]);
    if (!wrap) return NULL;

    double m = histo_sketch_min(wrap->handle);
    napi_value res;
    napi_create_double(env, m, &res);
    return res;
}

static napi_value n_sketch_max(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    SketchWrap *wrap = unwrap_sketch(env, args[0]);
    if (!wrap) return NULL;

    double m = histo_sketch_max(wrap->handle);
    napi_value res;
    napi_create_double(env, m, &res);
    return res;
}

static napi_value n_sketch_total_weight(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    SketchWrap *wrap = unwrap_sketch(env, args[0]);
    if (!wrap) return NULL;

    double tw = histo_sketch_total_weight(wrap->handle);
    napi_value res;
    napi_create_double(env, tw, &res);
    return res;
}

static napi_value n_sketch_num_entries(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    SketchWrap *wrap = unwrap_sketch(env, args[0]);
    if (!wrap) return NULL;

    uint64_t n = histo_sketch_num_entries(wrap->handle);
    napi_value res;
    napi_create_int64(env, (int64_t)n, &res);
    return res;
}

static napi_value n_sketch_serialize_binary(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    SketchWrap *wrap = unwrap_sketch(env, args[0]);
    if (!wrap) return NULL;

    void *out_buf = NULL;
    size_t out_sz = 0;
    histo_status_t st = histo_sketch_serialize_binary(wrap->handle, &out_buf, &out_sz);
    if (st != HISTO_OK || !out_buf) {
        throw_histo_status(env, st, "histo_sketch_serialize_binary failed");
        return NULL;
    }

    void *js_buf_data = NULL;
    napi_value res;
    if (napi_create_buffer(env, out_sz, &js_buf_data, &res) != napi_ok) {
        free(out_buf);
        throw_error(env, "Failed to allocate Node.js Buffer for sketch serialization");
        return NULL;
    }
    memcpy(js_buf_data, out_buf, out_sz);
    free(out_buf);
    return res;
}

static napi_value n_sketch_deserialize_binary(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) return NULL;

    void *data = NULL;
    size_t len = 0;
    bool is_buf = false;
    napi_is_buffer(env, args[0], &is_buf);
    if (is_buf) {
        napi_get_buffer_info(env, args[0], &data, &len);
    } else {
        bool is_typedarray = false;
        napi_is_typedarray(env, args[0], &is_typedarray);
        if (is_typedarray) {
            napi_typedarray_type type;
            napi_get_typedarray_info(env, args[0], &type, &len, &data, NULL, NULL);
        } else {
            throw_type_error(env, "Expected Buffer or Uint8Array for sketch deserialization");
            return NULL;
        }
    }

    histo_sketch_t *s = NULL;
    histo_status_t st = histo_sketch_deserialize_binary(data, len, &s);
    if (st != HISTO_OK || !s) {
        throw_histo_status(env, st, "histo_sketch_deserialize_binary failed");
        return NULL;
    }
    return wrap_sketch(env, s);
}

/* ========================================================================= */
/* Curve Fitting & Regression Native Functions                               */
/* ========================================================================= */

static void parse_fit_options(napi_env env, napi_value opts_val, histo_fit_options_t *opts,
                              double **lower_alloc, double **upper_alloc, bool **fixed_alloc) {
    histo_fit_options_init(opts);
    *lower_alloc = NULL;
    *upper_alloc = NULL;
    *fixed_alloc = NULL;
    if (!opts_val) return;

    napi_valuetype vt;
    napi_typeof(env, opts_val, &vt);
    if (vt != napi_object) return;

    bool has_prop = false;
    napi_value val;

    if (napi_has_named_property(env, opts_val, "maxIterations", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "maxIterations", &val);
        uint32_t mi = 500;
        if (napi_get_value_uint32(env, val, &mi) == napi_ok) opts->max_iterations = mi;
    }
    if (napi_has_named_property(env, opts_val, "ftol", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "ftol", &val);
        double tol = 1e-8;
        if (napi_get_value_double(env, val, &tol) == napi_ok) opts->ftol = tol;
    }
    if (napi_has_named_property(env, opts_val, "xtol", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "xtol", &val);
        double tol = 1e-8;
        if (napi_get_value_double(env, val, &tol) == napi_ok) opts->xtol = tol;
    }
    if (napi_has_named_property(env, opts_val, "gtol", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "gtol", &val);
        double tol = 1e-8;
        if (napi_get_value_double(env, val, &tol) == napi_ok) opts->gtol = tol;
    }
    if (napi_has_named_property(env, opts_val, "lossType", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "lossType", &val);
        uint32_t lt = 0;
        if (napi_get_value_uint32(env, val, &lt) == napi_ok) opts->loss_type = (histo_fit_loss_t)lt;
    }
    if (napi_has_named_property(env, opts_val, "algo", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "algo", &val);
        uint32_t al = 0;
        if (napi_get_value_uint32(env, val, &al) == napi_ok) opts->algo = (histo_fit_algo_t)al;
    }
    if (napi_has_named_property(env, opts_val, "polyDegree", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "polyDegree", &val);
        uint32_t pd = 1;
        if (napi_get_value_uint32(env, val, &pd) == napi_ok) opts->poly_degree = pd;
    }
    if (napi_has_named_property(env, opts_val, "rangeMin", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "rangeMin", &val);
        double rmin = 0.0;
        if (napi_get_value_double(env, val, &rmin) == napi_ok) opts->range_min = rmin;
    }
    if (napi_has_named_property(env, opts_val, "rangeMax", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "rangeMax", &val);
        double rmax = 0.0;
        if (napi_get_value_double(env, val, &rmax) == napi_ok) opts->range_max = rmax;
    }
    if (napi_has_named_property(env, opts_val, "lowerBounds", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "lowerBounds", &val);
        const double *lb = NULL; size_t n_lb = 0;
        if (extract_double_array(env, val, &lb, &n_lb, lower_alloc)) {
            opts->lower_bounds = lb;
        }
    }
    if (napi_has_named_property(env, opts_val, "upperBounds", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "upperBounds", &val);
        const double *ub = NULL; size_t n_ub = 0;
        if (extract_double_array(env, val, &ub, &n_ub, upper_alloc)) {
            opts->upper_bounds = ub;
        }
    }
    if (napi_has_named_property(env, opts_val, "fixedParams", &has_prop) == napi_ok && has_prop) {
        napi_get_named_property(env, opts_val, "fixedParams", &val);
        bool is_arr = false;
        napi_is_array(env, val, &is_arr);
        if (is_arr) {
            uint32_t len = 0;
            napi_get_array_length(env, val, &len);
            bool *fbuf = (bool *)malloc(len * sizeof(bool));
            if (fbuf) {
                for (uint32_t i = 0; i < len; i++) {
                    napi_value elem;
                    napi_get_element(env, val, i, &elem);
                    bool b = false;
                    napi_get_value_bool(env, elem, &b);
                    fbuf[i] = b;
                }
                *fixed_alloc = fbuf;
                opts->fixed_params = fbuf;
            }
        }
    }
}

static napi_value create_fit_result_js_object(napi_env env, const histo_fit_result_t *res) {
    if (!res) return NULL;

    napi_value obj;
    napi_create_object(env, &obj);

    size_t np = res->num_params;
    napi_value v_np, v_chi2, v_ndf, v_redchi2, v_pval, v_ll, v_aic, v_bic, v_iter, v_conv, v_stat, v_reason;
    napi_create_uint32(env, (uint32_t)np, &v_np);
    napi_create_double(env, res->chi2, &v_chi2);
    napi_create_int32(env, res->ndf, &v_ndf);
    napi_create_double(env, res->reduced_chi2, &v_redchi2);
    napi_create_double(env, res->p_value, &v_pval);
    napi_create_double(env, res->log_likelihood, &v_ll);
    napi_create_double(env, res->aic, &v_aic);
    napi_create_double(env, res->bic, &v_bic);
    napi_create_uint32(env, res->iterations, &v_iter);
    napi_get_boolean(env, res->converged, &v_conv);
    napi_create_int32(env, (int32_t)res->status, &v_stat);
    napi_create_string_utf8(env, res->stop_reason ? res->stop_reason : "", NAPI_AUTO_LENGTH, &v_reason);

    napi_set_named_property(env, obj, "numParams", v_np);
    napi_set_named_property(env, obj, "chi2", v_chi2);
    napi_set_named_property(env, obj, "ndf", v_ndf);
    napi_set_named_property(env, obj, "reducedChi2", v_redchi2);
    napi_set_named_property(env, obj, "pValue", v_pval);
    napi_set_named_property(env, obj, "logLikelihood", v_ll);
    napi_set_named_property(env, obj, "aic", v_aic);
    napi_set_named_property(env, obj, "bic", v_bic);
    napi_set_named_property(env, obj, "iterations", v_iter);
    napi_set_named_property(env, obj, "converged", v_conv);
    napi_set_named_property(env, obj, "status", v_stat);
    napi_set_named_property(env, obj, "stopReason", v_reason);

    /* params Float64Array */
    if (res->params && np > 0) {
        napi_value ab, ta;
        void *data = NULL;
        napi_create_arraybuffer(env, np * sizeof(double), &data, &ab);
        memcpy(data, res->params, np * sizeof(double));
        napi_create_typedarray(env, napi_float64_array, np, ab, 0, &ta);
        napi_set_named_property(env, obj, "params", ta);
    }

    /* param_errors Float64Array */
    if (res->param_errors && np > 0) {
        napi_value ab, ta;
        void *data = NULL;
        napi_create_arraybuffer(env, np * sizeof(double), &data, &ab);
        memcpy(data, res->param_errors, np * sizeof(double));
        napi_create_typedarray(env, napi_float64_array, np, ab, 0, &ta);
        napi_set_named_property(env, obj, "paramErrors", ta);
    }

    /* cov_matrix Float64Array */
    if (res->cov_matrix && np > 0) {
        napi_value ab, ta;
        void *data = NULL;
        napi_create_arraybuffer(env, np * np * sizeof(double), &data, &ab);
        memcpy(data, res->cov_matrix, np * np * sizeof(double));
        napi_create_typedarray(env, napi_float64_array, np * np, ab, 0, &ta);
        napi_set_named_property(env, obj, "covMatrix", ta);
    }

    /* cor_matrix Float64Array */
    if (res->cor_matrix && np > 0) {
        napi_value ab, ta;
        void *data = NULL;
        napi_create_arraybuffer(env, np * np * sizeof(double), &data, &ab);
        memcpy(data, res->cor_matrix, np * np * sizeof(double));
        napi_create_typedarray(env, napi_float64_array, np * np, ab, 0, &ta);
        napi_set_named_property(env, obj, "corMatrix", ta);
    }

    return obj;
}

static napi_value n_fit_model_num_params(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) {
        throw_type_error(env, "fit_model_num_params requires model, [poly_degree]");
        return NULL;
    }

    uint32_t model = 0, poly_degree = 1;
    NAPI_CALL(env, napi_get_value_uint32(env, args[0], &model));
    if (argc >= 2) {
        napi_get_value_uint32(env, args[1], &poly_degree);
    }

    size_t np = histo_fit_model_num_params((histo_fit_model_t)model, poly_degree);
    napi_value res;
    napi_create_uint32(env, (uint32_t)np, &res);
    return res;
}

static napi_value n_fit_estimate_initial_params(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "fit_estimate_initial_params requires histogram, model, [options]");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t model = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &model));

    histo_fit_options_t opts;
    double *la = NULL, *ua = NULL; bool *fa = NULL;
    parse_fit_options(env, (argc >= 3) ? args[2] : NULL, &opts, &la, &ua, &fa);

    size_t np = histo_fit_model_num_params((histo_fit_model_t)model, opts.poly_degree);
    if (np == 0) {
        free_double_array(la); free_double_array(ua); free(fa);
        throw_range_error(env, "Invalid model or polynomial degree");
        return NULL;
    }

    napi_value ab, ta;
    void *data = NULL;
    if (napi_create_arraybuffer(env, np * sizeof(double), &data, &ab) != napi_ok) {
        free_double_array(la); free_double_array(ua); free(fa);
        throw_error(env, "Failed to allocate ArrayBuffer");
        return NULL;
    }
    if (napi_create_typedarray(env, napi_float64_array, np, ab, 0, &ta) != napi_ok) {
        free_double_array(la); free_double_array(ua); free(fa);
        throw_error(env, "Failed to allocate Float64Array");
        return NULL;
    }

    histo_status_t st = histo_fit_estimate_initial_params(wrap->handle, (histo_fit_model_t)model, &opts, (double *)data);
    free_double_array(la); free_double_array(ua); free(fa);

    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_fit_estimate_initial_params failed");
        return NULL;
    }
    return ta;
}

static napi_value n_fit_model(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "fit_model requires histogram, model, [initial_params], [options]");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    uint32_t model = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[1], &model));

    const double *initial_params = NULL;
    size_t n_init = 0;
    double *init_alloc = NULL;
    if (argc >= 3) {
        napi_valuetype vt;
        napi_typeof(env, args[2], &vt);
        if (vt != napi_undefined && vt != napi_null) {
            if (!extract_double_array(env, args[2], &initial_params, &n_init, &init_alloc)) {
                return NULL;
            }
        }
    }

    histo_fit_options_t opts;
    double *la = NULL, *ua = NULL; bool *fa = NULL;
    parse_fit_options(env, (argc >= 4) ? args[3] : NULL, &opts, &la, &ua, &fa);

    histo_fit_result_t *res = NULL;
    histo_status_t st = histo_fit_model(wrap->handle, (histo_fit_model_t)model, initial_params, &opts, &res);

    free_double_array(init_alloc);
    free_double_array(la);
    free_double_array(ua);
    free(fa);

    if (st != HISTO_OK || !res) {
        if (res) histo_fit_result_destroy(res);
        throw_histo_status(env, st, "histo_fit_model failed");
        return NULL;
    }

    napi_value js_res = create_fit_result_js_object(env, res);
    histo_fit_result_destroy(res);
    return js_res;
}

typedef struct {
    napi_env env;
    napi_value js_fn;
    size_t num_params;
} JSFitCustomContext;

static double js_fit_custom_bridge_fn(double x, const double *params, void *userdata) {
    JSFitCustomContext *ctx = (JSFitCustomContext *)userdata;
    napi_env env = ctx->env;

    napi_handle_scope scope;
    if (napi_open_handle_scope(env, &scope) != napi_ok) return 0.0;

    napi_value js_x;
    if (napi_create_double(env, x, &js_x) != napi_ok) {
        napi_close_handle_scope(env, scope);
        return 0.0;
    }

    napi_value js_params;
    if (napi_create_array_with_length(env, ctx->num_params, &js_params) != napi_ok) {
        napi_close_handle_scope(env, scope);
        return 0.0;
    }
    for (size_t i = 0; i < ctx->num_params; i++) {
        napi_value elem;
        napi_create_double(env, params[i], &elem);
        napi_set_element(env, js_params, (uint32_t)i, elem);
    }

    napi_value argv[2] = { js_x, js_params };
    napi_value js_ret;
    napi_value global;
    napi_get_global(env, &global);
    if (napi_call_function(env, global, ctx->js_fn, 2, argv, &js_ret) != napi_ok) {
        napi_close_handle_scope(env, scope);
        return 0.0;
    }

    double ret_val = 0.0;
    napi_get_value_double(env, js_ret, &ret_val);
    napi_close_handle_scope(env, scope);
    return ret_val;
}

static napi_value n_fit_custom(napi_env env, napi_callback_info info) {
    size_t argc = 5;
    napi_value args[5];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 4) {
        throw_type_error(env, "fit_custom requires histogram, js_fn, num_params, initial_params, [options]");
        return NULL;
    }

    HistoWrap *wrap = unwrap_histo(env, args[0]);
    if (!wrap) return NULL;

    napi_value js_fn = args[1];
    napi_valuetype vt;
    napi_typeof(env, js_fn, &vt);
    if (vt != napi_function) {
        throw_type_error(env, "model callback must be a JavaScript function (x, params) => number");
        return NULL;
    }

    uint32_t num_params = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[2], &num_params));
    if (num_params == 0) {
        throw_range_error(env, "num_params must be >= 1");
        return NULL;
    }

    const double *initial_params = NULL;
    size_t n_init = 0;
    double *init_alloc = NULL;
    if (!extract_double_array(env, args[3], &initial_params, &n_init, &init_alloc)) {
        return NULL;
    }
    if (n_init != num_params) {
        free_double_array(init_alloc);
        throw_range_error(env, "initial_params length must match num_params");
        return NULL;
    }

    histo_fit_options_t opts;
    double *la = NULL, *ua = NULL; bool *fa = NULL;
    parse_fit_options(env, (argc >= 5) ? args[4] : NULL, &opts, &la, &ua, &fa);

    JSFitCustomContext ctx = {
        .env = env,
        .js_fn = js_fn,
        .num_params = num_params
    };
    opts.userdata = &ctx;

    histo_fit_result_t *res = NULL;
    histo_status_t st = histo_fit_custom(wrap->handle, js_fit_custom_bridge_fn, (size_t)num_params,
                                         initial_params, &opts, &res);

    free_double_array(init_alloc);
    free_double_array(la);
    free_double_array(ua);
    free(fa);

    if (st != HISTO_OK || !res) {
        if (res) histo_fit_result_destroy(res);
        throw_histo_status(env, st, "histo_fit_custom failed");
        return NULL;
    }

    napi_value js_res = create_fit_result_js_object(env, res);
    histo_fit_result_destroy(res);
    return js_res;
}

static napi_value n_fit_eval(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "fit_eval requires model, params array, x coordinate");
        return NULL;
    }

    uint32_t model = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[0], &model));

    const double *params = NULL;
    size_t np = 0;
    double *p_alloc = NULL;
    if (!extract_double_array(env, args[1], &params, &np, &p_alloc)) {
        return NULL;
    }

    double x = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[2], &x));

    double val = histo_fit_eval((histo_fit_model_t)model, params, np, x);
    free_double_array(p_alloc);

    napi_value res;
    napi_create_double(env, val, &res);
    return res;
}

static napi_value n_fit_eval_gradient(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 3) {
        throw_type_error(env, "fit_eval_gradient requires model, params array, x coordinate");
        return NULL;
    }

    uint32_t model = 0;
    NAPI_CALL(env, napi_get_value_uint32(env, args[0], &model));

    const double *params = NULL;
    size_t np = 0;
    double *p_alloc = NULL;
    if (!extract_double_array(env, args[1], &params, &np, &p_alloc)) {
        return NULL;
    }

    double x = 0.0;
    NAPI_CALL(env, napi_get_value_double(env, args[2], &x));

    napi_value ab, ta;
    void *data = NULL;
    if (napi_create_arraybuffer(env, np * sizeof(double), &data, &ab) != napi_ok) {
        free_double_array(p_alloc);
        throw_error(env, "Failed to allocate ArrayBuffer for gradient");
        return NULL;
    }
    if (napi_create_typedarray(env, napi_float64_array, np, ab, 0, &ta) != napi_ok) {
        free_double_array(p_alloc);
        throw_error(env, "Failed to allocate Float64Array for gradient");
        return NULL;
    }

    histo_status_t st = histo_fit_eval_gradient((histo_fit_model_t)model, params, np, x, (double *)data);
    free_double_array(p_alloc);

    if (st != HISTO_OK) {
        throw_histo_status(env, st, "histo_fit_eval_gradient failed");
        return NULL;
    }
    return ta;
}

static napi_value n_fit_chi2_p_value(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 2) {
        throw_type_error(env, "fit_chi2_p_value requires chi2 and ndf");
        return NULL;
    }

    double chi2 = 0.0;
    int32_t ndf = 0;
    NAPI_CALL(env, napi_get_value_double(env, args[0], &chi2));
    NAPI_CALL(env, napi_get_value_int32(env, args[1], &ndf));

    double pval = histo_fit_chi2_p_value(chi2, (int)ndf);
    napi_value res;
    napi_create_double(env, pval, &res);
    return res;
}

/* ========================================================================= */
/* In-Process CLI Execution Native Function                                  */
/* ========================================================================= */

static napi_value n_cli_run(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
    if (argc < 1) {
        throw_type_error(env, "cli_run requires array of argument strings");
        return NULL;
    }

    bool is_arr = false;
    napi_is_array(env, args[0], &is_arr);
    if (!is_arr) {
        throw_type_error(env, "cli_run argument must be an array of strings");
        return NULL;
    }

    uint32_t len = 0;
    napi_get_array_length(env, args[0], &len);

    int cli_argc = (int)len + 1;
    char **cli_argv = (char **)malloc((size_t)(cli_argc + 1) * sizeof(char *));
    if (!cli_argv) {
        throw_error(env, "Out of memory allocating argv");
        return NULL;
    }
    cli_argv[0] = "histo";

    for (uint32_t i = 0; i < len; i++) {
        napi_value elem;
        napi_get_element(env, args[0], i, &elem);
        size_t s_len = 0;
        napi_get_value_string_utf8(env, elem, NULL, 0, &s_len);
        char *str = (char *)malloc(s_len + 1);
        if (!str) {
            for (uint32_t j = 1; j < i; j++) free(cli_argv[j]);
            free(cli_argv);
            throw_error(env, "Out of memory copying CLI string");
            return NULL;
        }
        napi_get_value_string_utf8(env, elem, str, s_len + 1, &s_len);
        cli_argv[i + 1] = str;
    }
    cli_argv[cli_argc] = NULL;

    FILE *out_fp = tmpfile();
    FILE *err_fp = tmpfile();
    if (!out_fp || !err_fp) {
        if (out_fp) fclose(out_fp);
        if (err_fp) fclose(err_fp);
        for (int i = 1; i < cli_argc; i++) free(cli_argv[i]);
        free(cli_argv);
        throw_error(env, "Failed to create temporary streams for CLI capture");
        return NULL;
    }

    int ret = histo_cli_main(cli_argc, cli_argv, out_fp, err_fp);

    for (int i = 1; i < cli_argc; i++) free(cli_argv[i]);
    free(cli_argv);

    /* Read stdout */
    rewind(out_fp);
    fseek(out_fp, 0, SEEK_END);
    long out_len = ftell(out_fp);
    rewind(out_fp);
    char *out_buf = (char *)malloc((size_t)out_len + 1);
    if (out_len > 0 && out_buf) {
        size_t nr = fread(out_buf, 1, (size_t)out_len, out_fp);
        out_buf[nr] = '\0';
    } else if (out_buf) {
        out_buf[0] = '\0';
    }
    fclose(out_fp);

    /* Read stderr */
    rewind(err_fp);
    fseek(err_fp, 0, SEEK_END);
    long err_len = ftell(err_fp);
    rewind(err_fp);
    char *err_buf = (char *)malloc((size_t)err_len + 1);
    if (err_len > 0 && err_buf) {
        size_t nr = fread(err_buf, 1, (size_t)err_len, err_fp);
        err_buf[nr] = '\0';
    } else if (err_buf) {
        err_buf[0] = '\0';
    }
    fclose(err_fp);

    napi_value res, v_code, v_out, v_err;
    napi_create_object(env, &res);
    napi_create_int32(env, ret, &v_code);
    napi_create_string_utf8(env, out_buf ? out_buf : "", NAPI_AUTO_LENGTH, &v_out);
    napi_create_string_utf8(env, err_buf ? err_buf : "", NAPI_AUTO_LENGTH, &v_err);

    free(out_buf);
    free(err_buf);

    napi_set_named_property(env, res, "exitCode", v_code);
    napi_set_named_property(env, res, "stdout", v_out);
    napi_set_named_property(env, res, "stderr", v_err);
    return res;
}

/* ========================================================================= */
/* Module Initialization & Export Setup                                      */
/* ========================================================================= */

#define EXPORT_FN(name, fn)                                                  \
    do {                                                                     \
        napi_value fn_val;                                                   \
        if (napi_create_function(env, (name), NAPI_AUTO_LENGTH, (fn), NULL,  \
                                 &fn_val) == napi_ok) {                      \
            napi_set_named_property(env, exports, (name), fn_val);           \
        }                                                                    \
    } while (0)

#define EXPORT_INT(name, val)                                                \
    do {                                                                     \
        napi_value num_val;                                                  \
        if (napi_create_int32(env, (int32_t)(val), &num_val) == napi_ok) {   \
            napi_set_named_property(env, exports, (name), num_val);         \
        }                                                                    \
    } while (0)

#define EXPORT_STR(name, str)                                                \
    do {                                                                     \
        napi_value str_val;                                                  \
        if (napi_create_string_utf8(env, (str), NAPI_AUTO_LENGTH,            \
                                    &str_val) == napi_ok) {                  \
            napi_set_named_property(env, exports, (name), str_val);         \
        }                                                                    \
    } while (0)

NAPI_MODULE_INIT() {
    /* Version info */
    EXPORT_STR("VERSION", HISTO_VERSION_STRING);
    EXPORT_INT("VERSION_MAJOR", HISTO_VERSION_MAJOR);
    EXPORT_INT("VERSION_MINOR", HISTO_VERSION_MINOR);
    EXPORT_INT("VERSION_PATCH", HISTO_VERSION_PATCH);

    /* Enums: Status */
    EXPORT_INT("OK", HISTO_OK);
    EXPORT_INT("WARN_NON_FINITE", HISTO_WARN_NON_FINITE);
    EXPORT_INT("ERR_INVALID_ARG", HISTO_ERR_INVALID_ARG);
    EXPORT_INT("ERR_NOMEM", HISTO_ERR_NOMEM);
    EXPORT_INT("ERR_INCOMPATIBLE", HISTO_ERR_INCOMPATIBLE);
    EXPORT_INT("ERR_OUT_OF_RANGE", HISTO_ERR_OUT_OF_RANGE);
    EXPORT_INT("ERR_NON_FINITE", HISTO_ERR_NON_FINITE);
    EXPORT_INT("ERR_EMPTY", HISTO_ERR_EMPTY);
    EXPORT_INT("ERR_DIV_BY_ZERO", HISTO_ERR_DIV_BY_ZERO);
    EXPORT_INT("ERR_SERIALIZATION", HISTO_ERR_SERIALIZATION);
    EXPORT_INT("ERR_DESERIALIZATION", HISTO_ERR_DESERIALIZATION);

    /* Enums: Bin Types */
    EXPORT_INT("BIN_UNIFORM", HISTO_BIN_UNIFORM);
    EXPORT_INT("BIN_VARIABLE", HISTO_BIN_VARIABLE);

    /* Enums: Flags */
    EXPORT_INT("FLAG_NONE", HISTO_FLAG_NONE);
    EXPORT_INT("FLAG_TRACK_SUMW2", HISTO_FLAG_TRACK_SUMW2);
    EXPORT_INT("FLAG_EXACT_MOMENTS", HISTO_FLAG_EXACT_MOMENTS);

    /* Enums: Auto-bin rules */
    EXPORT_INT("BIN_RULE_AUTO", HISTO_BIN_RULE_AUTO);
    EXPORT_INT("BIN_RULE_FD", HISTO_BIN_RULE_FD);
    EXPORT_INT("BIN_RULE_SCOTT", HISTO_BIN_RULE_SCOTT);
    EXPORT_INT("BIN_RULE_STURGES", HISTO_BIN_RULE_STURGES);
    EXPORT_INT("BIN_RULE_DOANE", HISTO_BIN_RULE_DOANE);
    EXPORT_INT("BIN_RULE_KNUTH", HISTO_BIN_RULE_KNUTH);

    /* Enums: 2D Regions */
    EXPORT_INT("REGION_CENTER", HISTO2D_REGION_CENTER);
    EXPORT_INT("REGION_EAST", HISTO2D_REGION_EAST);
    EXPORT_INT("REGION_NORTH", HISTO2D_REGION_NORTH);
    EXPORT_INT("REGION_SOUTH", HISTO2D_REGION_SOUTH);
    EXPORT_INT("REGION_WEST", HISTO2D_REGION_WEST);
    EXPORT_INT("REGION_SOUTH_WEST", HISTO2D_REGION_SOUTH_WEST);
    EXPORT_INT("REGION_SOUTH_EAST", HISTO2D_REGION_SOUTH_EAST);
    EXPORT_INT("REGION_NORTH_WEST", HISTO2D_REGION_NORTH_WEST);
    EXPORT_INT("REGION_NORTH_EAST", HISTO2D_REGION_NORTH_EAST);

    /* Enums: KDE Kernels */
    EXPORT_INT("KDE_KERNEL_GAUSSIAN", HISTO_KDE_KERNEL_GAUSSIAN);
    EXPORT_INT("KDE_KERNEL_EPANECHNIKOV", HISTO_KDE_KERNEL_EPANECHNIKOV);
    EXPORT_INT("KDE_KERNEL_UNIFORM", HISTO_KDE_KERNEL_UNIFORM);
    EXPORT_INT("KDE_KERNEL_TRIANGULAR", HISTO_KDE_KERNEL_TRIANGULAR);
    EXPORT_INT("KDE_KERNEL_BIWEIGHT", HISTO_KDE_KERNEL_BIWEIGHT);
    EXPORT_INT("KDE_KERNEL_COSINE", HISTO_KDE_KERNEL_COSINE);

    /* Enums: KDE Bandwidth */
    EXPORT_INT("KDE_BANDWIDTH_SILVERMAN", HISTO_KDE_BANDWIDTH_SILVERMAN);
    EXPORT_INT("KDE_BANDWIDTH_SCOTT", HISTO_KDE_BANDWIDTH_SCOTT);
    EXPORT_INT("KDE_BANDWIDTH_MANUAL", HISTO_KDE_BANDWIDTH_MANUAL);

    /* Enums: Fit Models */
    EXPORT_INT("FIT_MODEL_GAUSSIAN", HISTO_FIT_MODEL_GAUSSIAN);
    EXPORT_INT("FIT_MODEL_EXPONENTIAL", HISTO_FIT_MODEL_EXPONENTIAL);
    EXPORT_INT("FIT_MODEL_POLYNOMIAL", HISTO_FIT_MODEL_POLYNOMIAL);
    EXPORT_INT("FIT_MODEL_BREIT_WIGNER", HISTO_FIT_MODEL_BREIT_WIGNER);
    EXPORT_INT("FIT_MODEL_POWER_LAW", HISTO_FIT_MODEL_POWER_LAW);
    EXPORT_INT("FIT_MODEL_LOG_NORMAL", HISTO_FIT_MODEL_LOG_NORMAL);
    EXPORT_INT("FIT_MODEL_GAUSSIAN_PLUS_LINEAR", HISTO_FIT_MODEL_GAUSSIAN_PLUS_LINEAR);
    EXPORT_INT("FIT_MODEL_WEIBULL", HISTO_FIT_MODEL_WEIBULL);
    EXPORT_INT("FIT_MODEL_GAMMA", HISTO_FIT_MODEL_GAMMA);
    EXPORT_INT("FIT_MODEL_POISSON", HISTO_FIT_MODEL_POISSON);
    EXPORT_INT("FIT_MODEL_LAPLACE", HISTO_FIT_MODEL_LAPLACE);
    EXPORT_INT("FIT_MODEL_CUSTOM", HISTO_FIT_MODEL_CUSTOM);

    /* Enums: Fit Loss */
    EXPORT_INT("FIT_LOSS_CHI2", HISTO_FIT_LOSS_CHI2);
    EXPORT_INT("FIT_LOSS_POISSON_MLE", HISTO_FIT_LOSS_POISSON_MLE);
    EXPORT_INT("FIT_LOSS_UNWEIGHTED_LS", HISTO_FIT_LOSS_UNWEIGHTED_LS);

    /* Enums: Fit Algo */
    EXPORT_INT("FIT_ALGO_AUTO", HISTO_FIT_ALGO_AUTO);
    EXPORT_INT("FIT_ALGO_LEVENBERG_MARQUARDT", HISTO_FIT_ALGO_LEVENBERG_MARQUARDT);
    EXPORT_INT("FIT_ALGO_LINEAR_LS", HISTO_FIT_ALGO_LINEAR_LS);

    /* 1D Functions */
    EXPORT_FN("histo_create_uniform", n_histo_create_uniform);
    EXPORT_FN("histo_create_variable", n_histo_create_variable);
    EXPORT_FN("histo_create_auto", n_histo_create_auto);
    EXPORT_FN("histo_estimate_bins", n_histo_estimate_bins);
    EXPORT_FN("histo_destroy", n_histo_destroy);
    EXPORT_FN("histo_clone", n_histo_clone);
    EXPORT_FN("histo_reset", n_histo_reset);
    EXPORT_FN("histo_fill", n_histo_fill);
    EXPORT_FN("histo_fill_n", n_histo_fill_n);
    EXPORT_FN("histo_fill_bin", n_histo_fill_bin);
    EXPORT_FN("histo_nbins", n_histo_nbins);
    EXPORT_FN("histo_bin_type", n_histo_bin_type);
    EXPORT_FN("histo_range", n_histo_range);
    EXPORT_FN("histo_find_bin", n_histo_find_bin);
    EXPORT_FN("histo_bin_bounds", n_histo_bin_bounds);
    EXPORT_FN("histo_bin_center", n_histo_bin_center);
    EXPORT_FN("histo_bin_content", n_histo_bin_content);
    EXPORT_FN("histo_bin_error", n_histo_bin_error);
    EXPORT_FN("histo_bin_sum_w2", n_histo_bin_sum_w2);
    EXPORT_FN("histo_total_weight", n_histo_total_weight);
    EXPORT_FN("histo_num_entries", n_histo_num_entries);
    EXPORT_FN("histo_underflow", n_histo_underflow);
    EXPORT_FN("histo_overflow", n_histo_overflow);
    EXPORT_FN("histo_nan_count", n_histo_nan_count);

    EXPORT_FN("histo_mean", n_histo_mean);
    EXPORT_FN("histo_variance", n_histo_variance);
    EXPORT_FN("histo_std_dev", n_histo_std_dev);
    EXPORT_FN("histo_central_moment", n_histo_central_moment);
    EXPORT_FN("histo_skewness", n_histo_skewness);
    EXPORT_FN("histo_kurtosis", n_histo_kurtosis);
    EXPORT_FN("histo_excess_kurtosis", n_histo_excess_kurtosis);
    EXPORT_FN("histo_mode_bin", n_histo_mode_bin);
    EXPORT_FN("histo_mode_continuous", n_histo_mode_continuous);
    EXPORT_FN("histo_fwhm", n_histo_fwhm);
    EXPORT_FN("histo_rms", n_histo_rms);
    EXPORT_FN("histo_quantile", n_histo_quantile);
    EXPORT_FN("histo_median", n_histo_median);
    EXPORT_FN("histo_iqr", n_histo_iqr);
    EXPORT_FN("histo_mad", n_histo_mad);
    EXPORT_FN("histo_trimmed_mean", n_histo_trimmed_mean);
    EXPORT_FN("histo_winsorized_mean", n_histo_winsorized_mean);
    EXPORT_FN("histo_integral", n_histo_integral);
    EXPORT_FN("histo_get_stats", n_histo_get_stats);

    EXPORT_FN("histo_cmp_chi2", n_histo_cmp_chi2);
    EXPORT_FN("histo_cmp_ks", n_histo_cmp_ks);
    EXPORT_FN("histo_cmp_wasserstein_1d", n_histo_cmp_wasserstein_1d);
    EXPORT_FN("histo_cmp_kl_divergence", n_histo_cmp_kl_divergence);
    EXPORT_FN("histo_cmp_bhattacharyya", n_histo_cmp_bhattacharyya);

    EXPORT_FN("histo_add", n_histo_add);
    EXPORT_FN("histo_subtract", n_histo_subtract);
    EXPORT_FN("histo_multiply", n_histo_multiply);
    EXPORT_FN("histo_divide", n_histo_divide);
    EXPORT_FN("histo_scale", n_histo_scale);
    EXPORT_FN("histo_normalize", n_histo_normalize);
    EXPORT_FN("histo_rebin", n_histo_rebin);
    EXPORT_FN("histo_slice", n_histo_slice);
    EXPORT_FN("histo_cdf", n_histo_cdf);

    EXPORT_FN("histo_serialize_binary", n_histo_serialize_binary);
    EXPORT_FN("histo_deserialize_binary", n_histo_deserialize_binary);
    EXPORT_FN("histo_serialize_json", n_histo_serialize_json);
    EXPORT_FN("histo_deserialize_json", n_histo_deserialize_json);
    EXPORT_FN("histo_migrate_binary", n_histo_migrate_binary);

    /* 2D Functions */
    EXPORT_FN("histo2d_create_uniform", n_histo2d_create_uniform);
    EXPORT_FN("histo2d_create_variable", n_histo2d_create_variable);
    EXPORT_FN("histo2d_create_uniform_variable", n_histo2d_create_uniform_variable);
    EXPORT_FN("histo2d_create_variable_uniform", n_histo2d_create_variable_uniform);
    EXPORT_FN("histo2d_destroy", n_histo2d_destroy);
    EXPORT_FN("histo2d_clone", n_histo2d_clone);
    EXPORT_FN("histo2d_reset", n_histo2d_reset);
    EXPORT_FN("histo2d_fill", n_histo2d_fill);
    EXPORT_FN("histo2d_fill_n", n_histo2d_fill_n);
    EXPORT_FN("histo2d_fill_bin", n_histo2d_fill_bin);
    EXPORT_FN("histo2d_nbins_x", n_histo2d_nbins_x);
    EXPORT_FN("histo2d_nbins_y", n_histo2d_nbins_y);
    EXPORT_FN("histo2d_find_bin", n_histo2d_find_bin);
    EXPORT_FN("histo2d_find_region", n_histo2d_find_region);
    EXPORT_FN("histo2d_bin_content", n_histo2d_bin_content);
    EXPORT_FN("histo2d_bin_error", n_histo2d_bin_error);
    EXPORT_FN("histo2d_bin_sum_w2", n_histo2d_bin_sum_w2);
    EXPORT_FN("histo2d_bin_bounds", n_histo2d_bin_bounds);
    EXPORT_FN("histo2d_bin_center", n_histo2d_bin_center);
    EXPORT_FN("histo2d_region_content", n_histo2d_region_content);
    EXPORT_FN("histo2d_total_weight", n_histo2d_total_weight);
    EXPORT_FN("histo2d_num_entries", n_histo2d_num_entries);
    EXPORT_FN("histo2d_nan_count", n_histo2d_nan_count);

    EXPORT_FN("histo2d_mean_x", n_histo2d_mean_x);
    EXPORT_FN("histo2d_mean_y", n_histo2d_mean_y);
    EXPORT_FN("histo2d_variance_x", n_histo2d_variance_x);
    EXPORT_FN("histo2d_variance_y", n_histo2d_variance_y);
    EXPORT_FN("histo2d_std_dev_x", n_histo2d_std_dev_x);
    EXPORT_FN("histo2d_std_dev_y", n_histo2d_std_dev_y);
    EXPORT_FN("histo2d_covariance", n_histo2d_covariance);
    EXPORT_FN("histo2d_correlation", n_histo2d_correlation);
    EXPORT_FN("histo2d_integral", n_histo2d_integral);
    EXPORT_FN("histo2d_get_stats", n_histo2d_get_stats);

    EXPORT_FN("histo2d_project_x", n_histo2d_project_x);
    EXPORT_FN("histo2d_project_y", n_histo2d_project_y);
    EXPORT_FN("histo2d_slice_x", n_histo2d_slice_x);
    EXPORT_FN("histo2d_slice_y", n_histo2d_slice_y);
    EXPORT_FN("histo2d_profile_x", n_histo2d_profile_x);
    EXPORT_FN("histo2d_profile_y", n_histo2d_profile_y);

    EXPORT_FN("histo2d_scale", n_histo2d_scale);
    EXPORT_FN("histo2d_normalize", n_histo2d_normalize);
    EXPORT_FN("histo2d_rebin", n_histo2d_rebin);
    EXPORT_FN("histo2d_add", n_histo2d_add);
    EXPORT_FN("histo2d_subtract", n_histo2d_subtract);
    EXPORT_FN("histo2d_multiply", n_histo2d_multiply);
    EXPORT_FN("histo2d_divide", n_histo2d_divide);

    EXPORT_FN("histo2d_serialize_binary", n_histo2d_serialize_binary);
    EXPORT_FN("histo2d_deserialize_binary", n_histo2d_deserialize_binary);
    EXPORT_FN("histo2d_serialize_json", n_histo2d_serialize_json);
    EXPORT_FN("histo2d_deserialize_json", n_histo2d_deserialize_json);

    /* KDE Functions */
    EXPORT_FN("kde_create", n_kde_create);
    EXPORT_FN("kde_create_from_histo", n_kde_create_from_histo);
    EXPORT_FN("kde_destroy", n_kde_destroy);
    EXPORT_FN("kde_eval", n_kde_eval);
    EXPORT_FN("kde_eval_n", n_kde_eval_n);
    EXPORT_FN("kde_cdf", n_kde_cdf);
    EXPORT_FN("kde_quantile", n_kde_quantile);
    EXPORT_FN("kde_sample", n_kde_sample);
    EXPORT_FN("kde_get_bandwidth", n_kde_get_bandwidth);
    EXPORT_FN("kde_get_kernel", n_kde_get_kernel);
    EXPORT_FN("kde_num_points", n_kde_num_points);

    /* DDSketch Functions */
    EXPORT_FN("sketch_create", n_sketch_create);
    EXPORT_FN("sketch_destroy", n_sketch_destroy);
    EXPORT_FN("sketch_insert", n_sketch_insert);
    EXPORT_FN("sketch_insert_n", n_sketch_insert_n);
    EXPORT_FN("sketch_quantile", n_sketch_quantile);
    EXPORT_FN("sketch_merge", n_sketch_merge);
    EXPORT_FN("sketch_reset", n_sketch_reset);
    EXPORT_FN("sketch_min", n_sketch_min);
    EXPORT_FN("sketch_max", n_sketch_max);
    EXPORT_FN("sketch_total_weight", n_sketch_total_weight);
    EXPORT_FN("sketch_num_entries", n_sketch_num_entries);
    EXPORT_FN("sketch_serialize_binary", n_sketch_serialize_binary);
    EXPORT_FN("sketch_deserialize_binary", n_sketch_deserialize_binary);

    /* Fit Functions */
    EXPORT_FN("fit_model_num_params", n_fit_model_num_params);
    EXPORT_FN("fit_estimate_initial_params", n_fit_estimate_initial_params);
    EXPORT_FN("fit_model", n_fit_model);
    EXPORT_FN("fit_custom", n_fit_custom);
    EXPORT_FN("fit_eval", n_fit_eval);
    EXPORT_FN("fit_eval_gradient", n_fit_eval_gradient);
    EXPORT_FN("fit_chi2_p_value", n_fit_chi2_p_value);

    /* CLI */
    EXPORT_FN("cli_run", n_cli_run);

    return exports;
}
