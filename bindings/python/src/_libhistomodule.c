/*
 * CPython C extension module bridging libhisto C APIs to Python.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>
#include <math.h>
#include <stdbool.h>

#include "histo/histo.h"
#include "histo/histo2d.h"
#include "histo/fit.h"
#include "histo/kde.h"
#include "histo/sketch.h"
#include "histo/cli.h"
#include "histo/version.h"
#include "histo/types.h"
#include "internal.h"
#include "internal_2d.h"

/* ------------------------------------------------------------------------- */
/* Exception types                                                           */
/* ------------------------------------------------------------------------- */
static PyObject *HistoError = NULL;

static void set_histo_error(histo_status_t st, const char *msg) {
    if (!msg) msg = histo_status_str(st);
    PyErr_Format(HistoError, "libhisto error (%d): %s", (int)st, msg);
}

/* ------------------------------------------------------------------------- */
/* Forward declarations of PyTypeObjects                                     */
/* ------------------------------------------------------------------------- */
static PyTypeObject Histo1DType;
static PyTypeObject Histo2DType;
static PyTypeObject SketchType;
static PyTypeObject FitResultType;
static PyTypeObject KDEType;

/* ------------------------------------------------------------------------- */
/* Histo1D Python Object                                                     */
/* ------------------------------------------------------------------------- */
typedef struct {
    PyObject_HEAD
    histo_t *h;
} Histo1DObject;

static PyObject *Histo1D_new_from_ptr(histo_t *h) {
    if (!h) {
        Py_RETURN_NONE;
    }
    Histo1DObject *self = (Histo1DObject *)Histo1DType.tp_alloc(&Histo1DType, 0);
    if (!self) {
        histo_destroy(h);
        return NULL;
    }
    self->h = h;
    return (PyObject *)self;
}

static void Histo1D_dealloc(Histo1DObject *self) {
    if (self->h) {
        histo_destroy(self->h);
        self->h = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Histo1D_create_uniform(PyObject *cls, PyObject *args, PyObject *kwargs) {
    (void)cls;
    static char *kwlist[] = {"nbins", "min", "max", "flags", NULL};
    unsigned int nbins = 0;
    double min_val = 0.0, max_val = 0.0;
    unsigned int flags = HISTO_FLAG_NONE;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Idd|I", kwlist, &nbins, &min_val, &max_val, &flags)) {
        return NULL;
    }

    histo_t *h = histo_create_uniform(nbins, min_val, max_val, flags);
    if (!h) {
        PyErr_SetString(HistoError, "Failed to create uniform histogram (invalid parameters)");
        return NULL;
    }
    return Histo1D_new_from_ptr(h);
}

static PyObject *Histo1D_create_variable(PyObject *cls, PyObject *args, PyObject *kwargs) {
    (void)cls;
    static char *kwlist[] = {"edges", "flags", NULL};
    PyObject *edges_obj = NULL;
    unsigned int flags = HISTO_FLAG_NONE;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|I", kwlist, &edges_obj, &flags)) {
        return NULL;
    }

    PyObject *seq = PySequence_Fast(edges_obj, "edges must be a sequence of numbers");
    if (!seq) return NULL;

    Py_ssize_t len = PySequence_Fast_GET_SIZE(seq);
    if (len < 2) {
        Py_DECREF(seq);
        PyErr_SetString(PyExc_ValueError, "edges sequence must contain at least 2 boundaries");
        return NULL;
    }

    uint32_t nbins = (uint32_t)(len - 1);
    double *edges = (double *)malloc((size_t)len * sizeof(double));
    if (!edges) {
        Py_DECREF(seq);
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < len; i++) {
        PyObject *item = PySequence_Fast_GET_ITEM(seq, i);
        edges[i] = PyFloat_AsDouble(item);
        if (PyErr_Occurred()) {
            free(edges);
            Py_DECREF(seq);
            return NULL;
        }
    }
    Py_DECREF(seq);

    histo_t *h = histo_create_variable(nbins, edges, flags);
    free(edges);

    if (!h) {
        PyErr_SetString(HistoError, "Failed to create variable histogram (edges must be strictly increasing)");
        return NULL;
    }
    return Histo1D_new_from_ptr(h);
}

static PyObject *Histo1D_deserialize_binary(PyObject *cls, PyObject *args) {
    (void)cls;
    Py_buffer view;
    if (!PyArg_ParseTuple(args, "y*", &view)) return NULL;

    histo_t *out_h = NULL;
    histo_status_t st = histo_deserialize_binary(view.buf, (size_t)view.len, &out_h);
    PyBuffer_Release(&view);

    if (st != HISTO_OK || !out_h) {
        set_histo_error(st, "Failed to deserialize binary histogram");
        return NULL;
    }
    return Histo1D_new_from_ptr(out_h);
}

static PyObject *Histo1D_deserialize_json(PyObject *cls, PyObject *args) {
    (void)cls;
    const char *json_str = NULL;
    if (!PyArg_ParseTuple(args, "s", &json_str)) return NULL;

    histo_t *out_h = NULL;
    histo_status_t st = histo_deserialize_json(json_str, &out_h);
    if (st != HISTO_OK || !out_h) {
        set_histo_error(st, "Failed to deserialize JSON histogram");
        return NULL;
    }
    return Histo1D_new_from_ptr(out_h);
}

static PyObject *Histo1D_migrate_binary(PyObject *cls, PyObject *args) {
    (void)cls;
    Py_buffer view;
    if (!PyArg_ParseTuple(args, "y*", &view)) return NULL;

    void *out_buf = NULL;
    size_t out_size = 0;
    histo_status_t st = histo_migrate_binary(view.buf, (size_t)view.len, &out_buf, &out_size);
    PyBuffer_Release(&view);

    if (st != HISTO_OK || !out_buf) {
        set_histo_error(st, "Failed to migrate binary histogram");
        return NULL;
    }

    PyObject *res = PyBytes_FromStringAndSize((const char *)out_buf, (Py_ssize_t)out_size);
    histo_free_buffer(out_buf);
    return res;
}

static PyObject *Histo1D_fill(Histo1DObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"x", "weight", NULL};
    double x = 0.0, weight = 1.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "d|d", kwlist, &x, &weight)) {
        return NULL;
    }

    histo_status_t st = (weight == 1.0) ? histo_fill(self->h, x) : histo_fill_w(self->h, x, weight);
    return PyBool_FromLong(st == HISTO_OK);
}

static PyObject *Histo1D_fill_n(Histo1DObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"x", "weights", NULL};
    PyObject *x_obj = NULL, *w_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O", kwlist, &x_obj, &w_obj)) {
        return NULL;
    }

    PyObject *x_seq = PySequence_Fast(x_obj, "x must be a sequence");
    if (!x_seq) return NULL;
    Py_ssize_t n = PySequence_Fast_GET_SIZE(x_seq);
    if (n == 0) {
        Py_DECREF(x_seq);
        Py_RETURN_TRUE;
    }

    PyObject *w_seq = NULL;
    if (w_obj && w_obj != Py_None) {
        w_seq = PySequence_Fast(w_obj, "weights must be a sequence");
        if (!w_seq) {
            Py_DECREF(x_seq);
            return NULL;
        }
        if (PySequence_Fast_GET_SIZE(w_seq) != n) {
            Py_DECREF(x_seq);
            Py_DECREF(w_seq);
            PyErr_SetString(PyExc_ValueError, "x and weights sequences must have identical length");
            return NULL;
        }
    }

    double *x_arr = (double *)malloc((size_t)n * sizeof(double));
    double *w_arr = w_seq ? (double *)malloc((size_t)n * sizeof(double)) : NULL;
    if (!x_arr || (w_seq && !w_arr)) {
        free(x_arr); free(w_arr);
        Py_DECREF(x_seq); Py_XDECREF(w_seq);
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < n; i++) {
        x_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(x_seq, i));
        if (w_seq) {
            w_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(w_seq, i));
        }
    }
    Py_DECREF(x_seq);
    Py_XDECREF(w_seq);

    if (PyErr_Occurred()) {
        free(x_arr); free(w_arr);
        return NULL;
    }

    histo_status_t st = histo_fill_n(self->h, (size_t)n, x_arr, w_arr);
    free(x_arr); free(w_arr);
    return PyBool_FromLong(st == HISTO_OK || st == HISTO_WARN_NON_FINITE);
}

static PyObject *Histo1D_fill_buffer(Histo1DObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"x", "weights", NULL};
    Py_buffer x_buf, w_buf;
    w_buf.buf = NULL;
    PyObject *w_obj = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "y*|O", kwlist, &x_buf, &w_obj)) {
        return NULL;
    }

    if (x_buf.len % sizeof(double) != 0) {
        PyBuffer_Release(&x_buf);
        PyErr_SetString(PyExc_ValueError, "x buffer byte length must be a multiple of sizeof(double) (8 bytes)");
        return NULL;
    }

    size_t n = (size_t)x_buf.len / sizeof(double);
    const double *x_arr = (const double *)x_buf.buf;
    const double *w_arr = NULL;

    if (w_obj && w_obj != Py_None) {
        if (PyObject_GetBuffer(w_obj, &w_buf, PyBUF_CONTIG_RO) != 0) {
            PyBuffer_Release(&x_buf);
            return NULL;
        }
        if ((size_t)w_buf.len != (size_t)x_buf.len) {
            PyBuffer_Release(&x_buf);
            PyBuffer_Release(&w_buf);
            PyErr_SetString(PyExc_ValueError, "weights buffer byte length must match x buffer length");
            return NULL;
        }
        w_arr = (const double *)w_buf.buf;
    }

    histo_status_t st = histo_fill_n(self->h, n, x_arr, w_arr);
    PyBuffer_Release(&x_buf);
    if (w_buf.buf) PyBuffer_Release(&w_buf);

    return PyBool_FromLong(st == HISTO_OK || st == HISTO_WARN_NON_FINITE);
}

static PyObject *Histo1D_fill_bin(Histo1DObject *self, PyObject *args) {
    unsigned int bin_idx = 0;
    double weight = 1.0;
    if (!PyArg_ParseTuple(args, "I|d", &bin_idx, &weight)) return NULL;

    histo_status_t st = histo_fill_bin(self->h, bin_idx, weight);
    if (st != HISTO_OK) {
        set_histo_error(st, "fill_bin index out of range");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject *Histo1D_reset(Histo1DObject *self, PyObject *Py_UNUSED(ignored)) {
    histo_reset(self->h);
    Py_RETURN_NONE;
}

static PyObject *Histo1D_clone(Histo1DObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"empty", NULL};
    int empty = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|p", kwlist, &empty)) return NULL;

    histo_t *c = histo_clone(self->h, empty ? true : false);
    if (!c) {
        PyErr_SetString(HistoError, "Failed to clone histogram");
        return NULL;
    }
    return Histo1D_new_from_ptr(c);
}

/* Getters & properties */
static PyObject *Histo1D_nbins(Histo1DObject *self, void *closure) {
    (void)closure;
    return PyLong_FromUnsignedLong(histo_nbins(self->h));
}

static PyObject *Histo1D_min(Histo1DObject *self, void *closure) {
    (void)closure;
    double mn = 0.0, mx = 0.0;
    histo_range(self->h, &mn, &mx);
    return PyFloat_FromDouble(mn);
}

static PyObject *Histo1D_max(Histo1DObject *self, void *closure) {
    (void)closure;
    double mn = 0.0, mx = 0.0;
    histo_range(self->h, &mn, &mx);
    return PyFloat_FromDouble(mx);
}

static PyObject *Histo1D_total_weight(Histo1DObject *self, void *closure) {
    (void)closure;
    return PyFloat_FromDouble(histo_total_weight(self->h));
}

static PyObject *Histo1D_num_entries(Histo1DObject *self, void *closure) {
    (void)closure;
    return PyLong_FromUnsignedLongLong(histo_num_entries(self->h));
}

static PyObject *Histo1D_underflow(Histo1DObject *self, void *closure) {
    (void)closure;
    return PyFloat_FromDouble(histo_underflow(self->h));
}

static PyObject *Histo1D_underflow_sum_w2(Histo1DObject *self, void *closure) {
    (void)closure;
    if (!self->h) return PyFloat_FromDouble(0.0);
    return PyFloat_FromDouble(self->h->underflow_sum_w2);
}

static PyObject *Histo1D_overflow(Histo1DObject *self, void *closure) {
    (void)closure;
    return PyFloat_FromDouble(histo_overflow(self->h));
}

static PyObject *Histo1D_overflow_sum_w2(Histo1DObject *self, void *closure) {
    (void)closure;
    if (!self->h) return PyFloat_FromDouble(0.0);
    return PyFloat_FromDouble(self->h->overflow_sum_w2);
}

static PyObject *Histo1D_nan_count(Histo1DObject *self, void *closure) {
    (void)closure;
    return PyLong_FromUnsignedLongLong(histo_nan_count(self->h));
}

static PyObject *Histo1D_mean(Histo1DObject *self, void *closure) {
    (void)closure;
    double val = 0.0;
    histo_status_t st = histo_mean(self->h, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_variance(Histo1DObject *self, void *closure) {
    (void)closure;
    double val = 0.0;
    histo_status_t st = histo_variance(self->h, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_std_dev(Histo1DObject *self, void *closure) {
    (void)closure;
    double val = 0.0;
    histo_status_t st = histo_std_dev(self->h, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_skewness(Histo1DObject *self, void *closure) {
    (void)closure;
    double val = 0.0;
    histo_status_t st = histo_skewness(self->h, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_kurtosis(Histo1DObject *self, void *closure) {
    (void)closure;
    double val = 0.0;
    histo_status_t st = histo_kurtosis(self->h, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_excess_kurtosis(Histo1DObject *self, void *closure) {
    (void)closure;
    double val = 0.0;
    histo_status_t st = histo_excess_kurtosis(self->h, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_median(Histo1DObject *self, void *closure) {
    (void)closure;
    double val = 0.0;
    histo_status_t st = histo_median(self->h, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_iqr(Histo1DObject *self, void *closure) {
    (void)closure;
    double val = 0.0;
    histo_status_t st = histo_iqr(self->h, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_mad(Histo1DObject *self, void *closure) {
    (void)closure;
    double val = 0.0;
    histo_status_t st = histo_mad(self->h, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_mode_bin(Histo1DObject *self, void *closure) {
    (void)closure;
    uint32_t val = 0;
    histo_status_t st = histo_mode_bin(self->h, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyLong_FromUnsignedLong(val);
}

static PyObject *Histo1D_mode(Histo1DObject *self, void *closure) {
    (void)closure;
    double val = 0.0;
    histo_status_t st = histo_mode_continuous(self->h, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_fwhm(Histo1DObject *self, void *closure) {
    (void)closure;
    double val = 0.0;
    histo_status_t st = histo_fwhm(self->h, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_rms(Histo1DObject *self, void *closure) {
    (void)closure;
    double val = 0.0;
    histo_status_t st = histo_rms(self->h, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_flags(Histo1DObject *self, void *closure) {
    (void)closure;
    if (!self->h) return PyLong_FromUnsignedLong(0);
    return PyLong_FromUnsignedLong(self->h->flags);
}

static PyGetSetDef Histo1D_getsetters[] = {
    {"flags", (getter)Histo1D_flags, NULL, "Histogram feature flags", NULL},
    {"nbins", (getter)Histo1D_nbins, NULL, "Number of bins", NULL},
    {"min", (getter)Histo1D_min, NULL, "Lower range limit", NULL},
    {"max", (getter)Histo1D_max, NULL, "Upper range limit", NULL},
    {"total_weight", (getter)Histo1D_total_weight, NULL, "Total in-range accumulated weight", NULL},
    {"num_entries", (getter)Histo1D_num_entries, NULL, "Total fill operations", NULL},
    {"underflow", (getter)Histo1D_underflow, NULL, "Underflow accumulated weight", NULL},
    {"underflow_sum_w2", (getter)Histo1D_underflow_sum_w2, NULL, "Underflow sum of squared weights", NULL},
    {"overflow", (getter)Histo1D_overflow, NULL, "Overflow accumulated weight", NULL},
    {"overflow_sum_w2", (getter)Histo1D_overflow_sum_w2, NULL, "Overflow sum of squared weights", NULL},
    {"nan_count", (getter)Histo1D_nan_count, NULL, "Non-finite sample count", NULL},
    {"mean", (getter)Histo1D_mean, NULL, "Distribution mean", NULL},
    {"variance", (getter)Histo1D_variance, NULL, "Distribution variance", NULL},
    {"std_dev", (getter)Histo1D_std_dev, NULL, "Distribution standard deviation", NULL},
    {"skewness", (getter)Histo1D_skewness, NULL, "Distribution skewness", NULL},
    {"kurtosis", (getter)Histo1D_kurtosis, NULL, "Distribution kurtosis", NULL},
    {"excess_kurtosis", (getter)Histo1D_excess_kurtosis, NULL, "Distribution excess kurtosis", NULL},
    {"median", (getter)Histo1D_median, NULL, "Distribution median (50th percentile)", NULL},
    {"iqr", (getter)Histo1D_iqr, NULL, "Interquartile Range (IQR)", NULL},
    {"mad", (getter)Histo1D_mad, NULL, "Median Absolute Deviation (MAD)", NULL},
    {"mode_bin", (getter)Histo1D_mode_bin, NULL, "Index of bin with highest weight", NULL},
    {"mode", (getter)Histo1D_mode, NULL, "Continuous mode peak estimate", NULL},
    {"fwhm", (getter)Histo1D_fwhm, NULL, "Full Width at Half Maximum", NULL},
    {"rms", (getter)Histo1D_rms, NULL, "Root Mean Square", NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

/* Methods */
static PyObject *Histo1D_find_bin(Histo1DObject *self, PyObject *args) {
    double x = 0.0;
    if (!PyArg_ParseTuple(args, "d", &x)) return NULL;
    int64_t b = 0;
    histo_status_t st = histo_find_bin(self->h, x, &b);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyLong_FromLongLong(b);
}

static PyObject *Histo1D_bin_content(Histo1DObject *self, PyObject *args) {
    unsigned int idx = 0;
    if (!PyArg_ParseTuple(args, "I", &idx)) return NULL;
    double val = 0.0;
    histo_status_t st = histo_bin_content(self->h, idx, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_bin_error(Histo1DObject *self, PyObject *args) {
    unsigned int idx = 0;
    if (!PyArg_ParseTuple(args, "I", &idx)) return NULL;
    double val = 0.0;
    histo_status_t st = histo_bin_error(self->h, idx, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_bin_sum_w2(Histo1DObject *self, PyObject *args) {
    unsigned int idx = 0;
    if (!PyArg_ParseTuple(args, "I", &idx)) return NULL;
    double val = 0.0;
    histo_status_t st = histo_bin_sum_w2(self->h, idx, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_bin_center(Histo1DObject *self, PyObject *args) {
    unsigned int idx = 0;
    if (!PyArg_ParseTuple(args, "I", &idx)) return NULL;
    double val = 0.0;
    histo_status_t st = histo_bin_center(self->h, idx, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_bin_bounds(Histo1DObject *self, PyObject *args) {
    unsigned int idx = 0;
    if (!PyArg_ParseTuple(args, "I", &idx)) return NULL;
    double lower = 0.0, upper = 0.0;
    histo_status_t st = histo_bin_bounds(self->h, idx, &lower, &upper);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return Py_BuildValue("(dd)", lower, upper);
}

static PyObject *Histo1D_central_moment(Histo1DObject *self, PyObject *args) {
    unsigned int k = 0;
    if (!PyArg_ParseTuple(args, "I", &k)) return NULL;
    double val = 0.0;
    histo_status_t st = histo_central_moment(self->h, k, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_quantile(Histo1DObject *self, PyObject *args) {
    double p = 0.0;
    if (!PyArg_ParseTuple(args, "d", &p)) return NULL;
    double val = 0.0;
    histo_status_t st = histo_quantile(self->h, p, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_trimmed_mean(Histo1DObject *self, PyObject *args) {
    double p_low = 0.0, p_high = 1.0;
    if (!PyArg_ParseTuple(args, "dd", &p_low, &p_high)) return NULL;
    double val = 0.0;
    histo_status_t st = histo_trimmed_mean(self->h, p_low, p_high, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_winsorized_mean(Histo1DObject *self, PyObject *args) {
    double p_low = 0.0, p_high = 1.0;
    if (!PyArg_ParseTuple(args, "dd", &p_low, &p_high)) return NULL;
    double val = 0.0;
    histo_status_t st = histo_winsorized_mean(self->h, p_low, p_high, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_integral(Histo1DObject *self, PyObject *args) {
    unsigned int start = 0, end = 0;
    uint32_t nb = histo_nbins(self->h);
    if (nb == 0) return PyFloat_FromDouble(0.0);
    end = nb - 1;

    if (!PyArg_ParseTuple(args, "|II", &start, &end)) return NULL;
    double val = 0.0;
    histo_status_t st = histo_integral(self->h, start, end, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo1D_get_stats(Histo1DObject *self, PyObject *Py_UNUSED(ignored)) {
    histo_stats_t st_data;
    histo_status_t st = histo_get_stats(self->h, &st_data);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }

    double skew = 0.0, kurt = 0.0, xkurt = 0.0, iqr = 0.0, mad = 0.0;
    double mode = 0.0, fwhm = 0.0, rms = 0.0;
    uint32_t mode_bin = 0;

    histo_skewness(self->h, &skew);
    histo_kurtosis(self->h, &kurt);
    histo_excess_kurtosis(self->h, &xkurt);
    histo_iqr(self->h, &iqr);
    histo_mad(self->h, &mad);
    histo_mode_bin(self->h, &mode_bin);
    histo_mode_continuous(self->h, &mode);
    histo_fwhm(self->h, &fwhm);
    histo_rms(self->h, &rms);

    return Py_BuildValue("{s:K,s:d,s:d,s:d,s:d,s:d,s:d,s:d,s:d,s:d,s:d,s:d,s:d,s:d,s:d,s:d,s:d,s:d,s:d}",
        "n_entries", (unsigned long long)st_data.n_entries,
        "total_weight", st_data.total_weight,
        "mean", st_data.mean,
        "variance", st_data.variance,
        "std_dev", st_data.std_dev,
        "skewness", skew,
        "kurtosis", kurt,
        "excess_kurtosis", xkurt,
        "median", st_data.median,
        "iqr", iqr,
        "mad", mad,
        "mode_bin", (double)mode_bin,
        "mode", mode,
        "fwhm", fwhm,
        "rms", rms,
        "underflow", histo_underflow(self->h),
        "overflow", histo_overflow(self->h),
        "min", st_data.min,
        "max", st_data.max
    );
}


/* Comparison metrics */
static PyObject *Histo1D_chi2_test(Histo1DObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    if (!PyArg_ParseTuple(args, "O!", &Histo1DType, &other_obj)) return NULL;
    Histo1DObject *other = (Histo1DObject *)other_obj;

    double chi2 = 0.0;
    uint32_t ndf = 0;
    histo_status_t st = histo_cmp_chi2(self->h, other->h, &chi2, &ndf);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return Py_BuildValue("(dI)", chi2, ndf);
}

static PyObject *Histo1D_ks_test(Histo1DObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    if (!PyArg_ParseTuple(args, "O!", &Histo1DType, &other_obj)) return NULL;
    Histo1DObject *other = (Histo1DObject *)other_obj;

    double ks = 0.0;
    histo_status_t st = histo_cmp_ks(self->h, other->h, &ks);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(ks);
}

static PyObject *Histo1D_wasserstein_distance(Histo1DObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    if (!PyArg_ParseTuple(args, "O!", &Histo1DType, &other_obj)) return NULL;
    Histo1DObject *other = (Histo1DObject *)other_obj;

    double dist = 0.0;
    histo_status_t st = histo_cmp_wasserstein_1d(self->h, other->h, &dist);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(dist);
}

static PyObject *Histo1D_kl_divergence(Histo1DObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    if (!PyArg_ParseTuple(args, "O!", &Histo1DType, &other_obj)) return NULL;
    Histo1DObject *other = (Histo1DObject *)other_obj;

    double div = 0.0;
    histo_status_t st = histo_cmp_kl_divergence(self->h, other->h, &div);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(div);
}

static PyObject *Histo1D_bhattacharyya_distance(Histo1DObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    if (!PyArg_ParseTuple(args, "O!", &Histo1DType, &other_obj)) return NULL;
    Histo1DObject *other = (Histo1DObject *)other_obj;

    double dist = 0.0;
    histo_status_t st = histo_cmp_bhattacharyya(self->h, other->h, &dist);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(dist);
}

/* Arithmetic transformations */
static PyObject *Histo1D_add(Histo1DObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    if (!PyArg_ParseTuple(args, "O!", &Histo1DType, &other_obj)) return NULL;
    Histo1DObject *other = (Histo1DObject *)other_obj;

    histo_status_t st = histo_add(self->h, other->h);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Histo1D_subtract(Histo1DObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    if (!PyArg_ParseTuple(args, "O!", &Histo1DType, &other_obj)) return NULL;
    Histo1DObject *other = (Histo1DObject *)other_obj;

    histo_status_t st = histo_subtract(self->h, other->h);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Histo1D_multiply(Histo1DObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    if (!PyArg_ParseTuple(args, "O!", &Histo1DType, &other_obj)) return NULL;
    Histo1DObject *other = (Histo1DObject *)other_obj;

    histo_status_t st = histo_multiply(self->h, other->h);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Histo1D_divide(Histo1DObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    if (!PyArg_ParseTuple(args, "O!", &Histo1DType, &other_obj)) return NULL;
    Histo1DObject *other = (Histo1DObject *)other_obj;

    histo_status_t st = histo_divide(self->h, other->h);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Histo1D_scale(Histo1DObject *self, PyObject *args) {
    double factor = 1.0;
    if (!PyArg_ParseTuple(args, "d", &factor)) return NULL;

    histo_status_t st = histo_scale(self->h, factor);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Histo1D_normalize(Histo1DObject *self, PyObject *args) {
    double area = 1.0;
    if (!PyArg_ParseTuple(args, "|d", &area)) return NULL;

    histo_status_t st = histo_normalize(self->h, area);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Histo1D_rebin(Histo1DObject *self, PyObject *args) {
    unsigned int factor = 1;
    if (!PyArg_ParseTuple(args, "I", &factor)) return NULL;

    histo_t *res = histo_rebin(self->h, factor);
    if (!res) {
        PyErr_SetString(HistoError, "rebin failed (nbins must be divisible by factor)");
        return NULL;
    }
    return Histo1D_new_from_ptr(res);
}

static PyObject *Histo1D_slice(Histo1DObject *self, PyObject *args) {
    unsigned int start = 0, end = 0;
    int empty = 0;
    if (!PyArg_ParseTuple(args, "II|p", &start, &end, &empty)) return NULL;

    histo_t *res = histo_slice(self->h, start, end, empty ? true : false);
    if (!res) {
        PyErr_SetString(HistoError, "slice failed (invalid bin range)");
        return NULL;
    }
    return Histo1D_new_from_ptr(res);
}

static PyObject *Histo1D_cdf(Histo1DObject *self, PyObject *args) {
    double prenorm = 1.0;
    if (!PyArg_ParseTuple(args, "|d", &prenorm)) return NULL;

    histo_t *res = histo_cdf(self->h, prenorm);
    if (!res) {
        PyErr_SetString(HistoError, "cdf generation failed");
        return NULL;
    }
    return Histo1D_new_from_ptr(res);
}

static PyObject *Histo1D_serialize_binary(Histo1DObject *self, PyObject *Py_UNUSED(ignored)) {
    void *buf = NULL;
    size_t sz = 0;
    histo_status_t st = histo_serialize_binary(self->h, &buf, &sz);
    if (st != HISTO_OK || !buf) {
        set_histo_error(st, "Binary serialization failed");
        return NULL;
    }
    PyObject *bytes = PyBytes_FromStringAndSize((const char *)buf, (Py_ssize_t)sz);
    histo_free_buffer(buf);
    return bytes;
}

static PyObject *Histo1D_serialize_json(Histo1DObject *self, PyObject *Py_UNUSED(ignored)) {
    char *json = NULL;
    histo_status_t st = histo_serialize_json(self->h, &json);
    if (st != HISTO_OK || !json) {
        set_histo_error(st, "JSON serialization failed");
        return NULL;
    }
    PyObject *str = PyUnicode_FromString(json);
    histo_free_buffer(json);
    return str;
}

/* Curve Fitting */
typedef struct {
    PyObject_HEAD
    histo_fit_result_t *res;
} FitResultObject;


static PyObject *FitResult_new_from_ptr(histo_fit_result_t *res) {
    if (!res) Py_RETURN_NONE;
    FitResultObject *self = (FitResultObject *)FitResultType.tp_alloc(&FitResultType, 0);
    if (!self) {
        histo_fit_result_destroy(res);
        return NULL;
    }
    self->res = res;
    return (PyObject *)self;
}

static void FitResult_dealloc(FitResultObject *self) {
    if (self->res) {
        histo_fit_result_destroy(self->res);
        self->res = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *FitResult_params(FitResultObject *self, void *closure) {
    (void)closure;
    if (!self->res) Py_RETURN_NONE;
    size_t np = self->res->num_params;
    const double *p = self->res->params;
    PyObject *list = PyList_New((Py_ssize_t)np);
    for (size_t i = 0; i < np; i++) {
        PyList_SET_ITEM(list, (Py_ssize_t)i, PyFloat_FromDouble(p ? p[i] : 0.0));
    }
    return list;
}

static PyObject *FitResult_errors(FitResultObject *self, void *closure) {
    (void)closure;
    if (!self->res) Py_RETURN_NONE;
    size_t np = self->res->num_params;
    const double *e = self->res->param_errors;
    PyObject *list = PyList_New((Py_ssize_t)np);
    for (size_t i = 0; i < np; i++) {
        PyList_SET_ITEM(list, (Py_ssize_t)i, PyFloat_FromDouble(e ? e[i] : 0.0));
    }
    return list;
}

static PyObject *FitResult_chi2(FitResultObject *self, void *closure) {
    (void)closure;
    return PyFloat_FromDouble(self->res ? self->res->chi2 : 0.0);
}

static PyObject *FitResult_ndf(FitResultObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(self->res ? self->res->ndf : 0);
}

static PyObject *FitResult_reduced_chi2(FitResultObject *self, void *closure) {
    (void)closure;
    return PyFloat_FromDouble(self->res ? self->res->reduced_chi2 : 0.0);
}

static PyObject *FitResult_p_value(FitResultObject *self, void *closure) {
    (void)closure;
    return PyFloat_FromDouble(self->res ? self->res->p_value : 0.0);
}

static PyObject *FitResult_log_likelihood(FitResultObject *self, void *closure) {
    (void)closure;
    return PyFloat_FromDouble(self->res ? self->res->log_likelihood : 0.0);
}

static PyObject *FitResult_aic(FitResultObject *self, void *closure) {
    (void)closure;
    return PyFloat_FromDouble(self->res ? self->res->aic : 0.0);
}

static PyObject *FitResult_bic(FitResultObject *self, void *closure) {
    (void)closure;
    return PyFloat_FromDouble(self->res ? self->res->bic : 0.0);
}

static PyObject *FitResult_iterations(FitResultObject *self, void *closure) {
    (void)closure;
    return PyLong_FromUnsignedLong(self->res ? self->res->iterations : 0);
}

static PyObject *FitResult_status(FitResultObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(self->res ? (long)self->res->status : 0);
}

static PyObject *FitResult_converged(FitResultObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->res ? self->res->converged : 0);
}

static PyObject *FitResult_reason(FitResultObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(self->res && self->res->stop_reason ? self->res->stop_reason : "");
}


static PyGetSetDef FitResult_getsetters[] = {
    {"params", (getter)FitResult_params, NULL, "Fitted parameter values", NULL},
    {"errors", (getter)FitResult_errors, NULL, "Fitted parameter standard errors", NULL},
    {"chi2", (getter)FitResult_chi2, NULL, "Chi2 statistic", NULL},
    {"ndf", (getter)FitResult_ndf, NULL, "Degrees of freedom", NULL},
    {"reduced_chi2", (getter)FitResult_reduced_chi2, NULL, "Reduced Chi2 (Chi2/NDF)", NULL},
    {"p_value", (getter)FitResult_p_value, NULL, "p-value", NULL},
    {"log_likelihood", (getter)FitResult_log_likelihood, NULL, "Log likelihood", NULL},
    {"aic", (getter)FitResult_aic, NULL, "Akaike Information Criterion", NULL},
    {"bic", (getter)FitResult_bic, NULL, "Bayesian Information Criterion", NULL},
    {"iterations", (getter)FitResult_iterations, NULL, "Optimization iterations", NULL},
    {"status", (getter)FitResult_status, NULL, "Convergence status code", NULL},
    {"converged", (getter)FitResult_converged, NULL, "True if optimization converged", NULL},
    {"reason", (getter)FitResult_reason, NULL, "Stopping reason description", NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static PyTypeObject FitResultType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_libhisto.FitResult",
    .tp_doc = "Non-linear regression and curve fitting result",
    .tp_basicsize = sizeof(FitResultObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)FitResult_dealloc,
    .tp_getset = FitResult_getsetters,
};

static PyObject *Histo1D_fit_builtin(Histo1DObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"model", "initial", "lower", "upper", "fixed", "degree", "max_iter", "tol", "loss", NULL};
    int model_type = 0;
    PyObject *init_obj = NULL, *lower_obj = NULL, *upper_obj = NULL, *fixed_obj = NULL;
    int degree = 1;
    int max_iter = 200;
    double tol = 1e-8;
    int loss = HISTO_FIT_LOSS_CHI2;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i|OOOOiidi", kwlist,
            &model_type, &init_obj, &lower_obj, &upper_obj, &fixed_obj, &degree, &max_iter, &tol, &loss)) {
        return NULL;
    }

    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.max_iterations = (uint32_t)max_iter;
    opts.ftol = tol;
    opts.xtol = tol;
    opts.gtol = tol;
    opts.loss_type = (histo_fit_loss_t)loss;
    opts.poly_degree = (uint32_t)degree;

    double initial_arr[16] = {0};
    double lower_arr[16] = {0};
    double upper_arr[16] = {0};
    bool fixed_arr[16] = {false};
    const double *p_init = NULL;

    if (init_obj && init_obj != Py_None) {
        PyObject *seq = PySequence_Fast(init_obj, "initial must be a sequence");
        if (!seq) return NULL;
        Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
        for (Py_ssize_t i = 0; i < n && i < 16; i++) {
            initial_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(seq, i));
        }
        Py_DECREF(seq);
        p_init = initial_arr;
    }
    if (lower_obj && lower_obj != Py_None) {
        PyObject *seq = PySequence_Fast(lower_obj, "lower must be a sequence");
        if (!seq) return NULL;
        Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
        for (Py_ssize_t i = 0; i < n && i < 16; i++) {
            lower_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(seq, i));
        }
        Py_DECREF(seq);
        opts.lower_bounds = lower_arr;
    }
    if (upper_obj && upper_obj != Py_None) {
        PyObject *seq = PySequence_Fast(upper_obj, "upper must be a sequence");
        if (!seq) return NULL;
        Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
        for (Py_ssize_t i = 0; i < n && i < 16; i++) {
            upper_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(seq, i));
        }
        Py_DECREF(seq);
        opts.upper_bounds = upper_arr;
    }
    if (fixed_obj && fixed_obj != Py_None) {
        PyObject *seq = PySequence_Fast(fixed_obj, "fixed must be a sequence");
        if (!seq) return NULL;
        Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
        for (Py_ssize_t i = 0; i < n && i < 16; i++) {
            fixed_arr[i] = PyObject_IsTrue(PySequence_Fast_GET_ITEM(seq, i)) ? true : false;
        }
        Py_DECREF(seq);
        opts.fixed_params = fixed_arr;
    }

    histo_fit_result_t *res = NULL;
    histo_status_t st = histo_fit_model(self->h, (histo_fit_model_t)model_type, p_init, &opts, &res);
    if (st != HISTO_OK || !res) {
        set_histo_error(st, "Curve fitting failed");
        return NULL;
    }
    return FitResult_new_from_ptr(res);
}


typedef struct {
    PyObject *callable;
    size_t n_params;
} py_custom_fit_ctx_t;

static double py_fit_model_callback(double x, const double *params, void *userdata) {
    py_custom_fit_ctx_t *ctx = (py_custom_fit_ctx_t *)userdata;
    PyGILState_STATE gstate = PyGILState_Ensure();
    PyObject *params_list = PyList_New((Py_ssize_t)ctx->n_params);
    for (size_t i = 0; i < ctx->n_params; i++) {
        PyList_SET_ITEM(params_list, (Py_ssize_t)i, PyFloat_FromDouble(params[i]));
    }
    PyObject *res = PyObject_CallFunction(ctx->callable, "dO", x, params_list);
    Py_DECREF(params_list);
    double out = 0.0;
    if (res) {
        out = PyFloat_AsDouble(res);
        Py_DECREF(res);
    }
    PyGILState_Release(gstate);
    return out;
}

static PyObject *Histo1D_fit_custom(Histo1DObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"model_fn", "n_params", "initial", "lower", "upper", "fixed", "max_iter", "tol", "loss", NULL};
    PyObject *fn_obj = NULL;
    unsigned int n_params = 0;
    PyObject *init_obj = NULL, *lower_obj = NULL, *upper_obj = NULL, *fixed_obj = NULL;
    int max_iter = 200;
    double tol = 1e-8;
    int loss = HISTO_FIT_LOSS_CHI2;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OI|OOOOidi", kwlist,
            &fn_obj, &n_params, &init_obj, &lower_obj, &upper_obj, &fixed_obj, &max_iter, &tol, &loss)) {
        return NULL;
    }

    if (!PyCallable_Check(fn_obj)) {
        PyErr_SetString(PyExc_TypeError, "model_fn must be callable");
        return NULL;
    }

    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.max_iterations = (uint32_t)max_iter;
    opts.ftol = tol;
    opts.xtol = tol;
    opts.gtol = tol;
    opts.loss_type = (histo_fit_loss_t)loss;

    py_custom_fit_ctx_t ctx = { .callable = fn_obj, .n_params = (size_t)n_params };
    opts.userdata = &ctx;


    double initial_arr[16] = {0};
    double lower_arr[16] = {0};
    double upper_arr[16] = {0};
    bool fixed_arr[16] = {false};

    if (init_obj && init_obj != Py_None) {
        PyObject *seq = PySequence_Fast(init_obj, "initial must be a sequence");
        if (!seq) return NULL;
        Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
        for (Py_ssize_t i = 0; i < n && i < 16; i++) {
            initial_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(seq, i));
        }
        Py_DECREF(seq);
    }
    if (lower_obj && lower_obj != Py_None) {
        PyObject *seq = PySequence_Fast(lower_obj, "lower must be a sequence");
        if (!seq) return NULL;
        Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
        for (Py_ssize_t i = 0; i < n && i < 16; i++) {
            lower_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(seq, i));
        }
        Py_DECREF(seq);
        opts.lower_bounds = lower_arr;
    }
    if (upper_obj && upper_obj != Py_None) {
        PyObject *seq = PySequence_Fast(upper_obj, "upper must be a sequence");
        if (!seq) return NULL;
        Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
        for (Py_ssize_t i = 0; i < n && i < 16; i++) {
            upper_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(seq, i));
        }
        Py_DECREF(seq);
        opts.upper_bounds = upper_arr;
    }
    if (fixed_obj && fixed_obj != Py_None) {
        PyObject *seq = PySequence_Fast(fixed_obj, "fixed must be a sequence");
        if (!seq) return NULL;
        Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
        for (Py_ssize_t i = 0; i < n && i < 16; i++) {
            fixed_arr[i] = PyObject_IsTrue(PySequence_Fast_GET_ITEM(seq, i)) ? true : false;
        }
        Py_DECREF(seq);
        opts.fixed_params = fixed_arr;
    }

    histo_fit_result_t *res = NULL;
    histo_status_t st = histo_fit_custom(self->h, py_fit_model_callback, (size_t)n_params, initial_arr, &opts, &res);
    if (st != HISTO_OK || !res) {
        set_histo_error(st, "Custom curve fitting failed");
        return NULL;
    }
    return FitResult_new_from_ptr(res);
}

static PyMethodDef Histo1D_methods[] = {
    {"create_uniform", (PyCFunction)(void(*)(void))Histo1D_create_uniform, METH_VARARGS | METH_KEYWORDS | METH_CLASS, "Create uniform 1D histogram"},
    {"create_variable", (PyCFunction)(void(*)(void))Histo1D_create_variable, METH_VARARGS | METH_KEYWORDS | METH_CLASS, "Create variable 1D histogram"},
    {"deserialize_binary", (PyCFunction)(void(*)(void))Histo1D_deserialize_binary, METH_VARARGS | METH_CLASS, "Deserialize from binary buffer"},
    {"deserialize_json", (PyCFunction)(void(*)(void))Histo1D_deserialize_json, METH_VARARGS | METH_CLASS, "Deserialize from JSON string"},
    {"migrate_binary", (PyCFunction)(void(*)(void))Histo1D_migrate_binary, METH_VARARGS | METH_CLASS, "Migrate binary buffer to latest version"},
    {"fill", (PyCFunction)(void(*)(void))Histo1D_fill, METH_VARARGS | METH_KEYWORDS, "Fill single sample"},
    {"fill_n", (PyCFunction)(void(*)(void))Histo1D_fill_n, METH_VARARGS | METH_KEYWORDS, "Batch fill from sequences"},
    {"fill_buffer", (PyCFunction)(void(*)(void))Histo1D_fill_buffer, METH_VARARGS | METH_KEYWORDS, "Zero-copy SIMD batch fill from float64 buffer"},
    {"fill_bin", (PyCFunction)(void(*)(void))Histo1D_fill_bin, METH_VARARGS, "Directly fill bin index"},
    {"reset", (PyCFunction)(void(*)(void))Histo1D_reset, METH_NOARGS, "Reset histogram contents"},
    {"clone", (PyCFunction)(void(*)(void))Histo1D_clone, METH_VARARGS | METH_KEYWORDS, "Clone histogram"},
    {"find_bin", (PyCFunction)(void(*)(void))Histo1D_find_bin, METH_VARARGS, "Locate bin for coordinate x"},
    {"bin_content", (PyCFunction)(void(*)(void))Histo1D_bin_content, METH_VARARGS, "Get bin content"},
    {"bin_error", (PyCFunction)(void(*)(void))Histo1D_bin_error, METH_VARARGS, "Get bin standard error"},
    {"bin_sum_w2", (PyCFunction)(void(*)(void))Histo1D_bin_sum_w2, METH_VARARGS, "Get bin sum of squared weights"},
    {"bin_center", (PyCFunction)(void(*)(void))Histo1D_bin_center, METH_VARARGS, "Get bin midpoint coordinate"},
    {"bin_bounds", (PyCFunction)(void(*)(void))Histo1D_bin_bounds, METH_VARARGS, "Get bin (lower, upper) boundaries"},
    {"central_moment", (PyCFunction)(void(*)(void))Histo1D_central_moment, METH_VARARGS, "Calculate k-th central moment"},
    {"quantile", (PyCFunction)(void(*)(void))Histo1D_quantile, METH_VARARGS, "Calculate quantile coordinate"},
    {"trimmed_mean", (PyCFunction)(void(*)(void))Histo1D_trimmed_mean, METH_VARARGS, "Calculate trimmed mean"},
    {"winsorized_mean", (PyCFunction)(void(*)(void))Histo1D_winsorized_mean, METH_VARARGS, "Calculate Winsorized mean"},
    {"integral", (PyCFunction)(void(*)(void))Histo1D_integral, METH_VARARGS, "Calculate integrated bin content"},
    {"get_stats", (PyCFunction)(void(*)(void))Histo1D_get_stats, METH_NOARGS, "Get complete statistics dictionary"},
    {"chi2_test", (PyCFunction)(void(*)(void))Histo1D_chi2_test, METH_VARARGS, "Two-sample Chi-Square test"},
    {"ks_test", (PyCFunction)(void(*)(void))Histo1D_ks_test, METH_VARARGS, "Two-sample Kolmogorov-Smirnov test"},
    {"wasserstein_distance", (PyCFunction)(void(*)(void))Histo1D_wasserstein_distance, METH_VARARGS, "1D Wasserstein (Earth Mover's) distance"},
    {"kl_divergence", (PyCFunction)(void(*)(void))Histo1D_kl_divergence, METH_VARARGS, "Kullback-Leibler divergence"},
    {"bhattacharyya_distance", (PyCFunction)(void(*)(void))Histo1D_bhattacharyya_distance, METH_VARARGS, "Bhattacharyya distance"},
    {"add", (PyCFunction)(void(*)(void))Histo1D_add, METH_VARARGS, "In-place addition"},
    {"subtract", (PyCFunction)(void(*)(void))Histo1D_subtract, METH_VARARGS, "In-place subtraction"},
    {"multiply", (PyCFunction)(void(*)(void))Histo1D_multiply, METH_VARARGS, "In-place elementwise multiplication"},
    {"divide", (PyCFunction)(void(*)(void))Histo1D_divide, METH_VARARGS, "In-place elementwise division"},
    {"scale", (PyCFunction)(void(*)(void))Histo1D_scale, METH_VARARGS, "Scale by scalar factor"},
    {"normalize", (PyCFunction)(void(*)(void))Histo1D_normalize, METH_VARARGS, "Normalize to target area"},
    {"rebin", (PyCFunction)(void(*)(void))Histo1D_rebin, METH_VARARGS, "Rebin by integer factor"},
    {"slice", (PyCFunction)(void(*)(void))Histo1D_slice, METH_VARARGS, "Slice subset of bins"},
    {"cdf", (PyCFunction)(void(*)(void))Histo1D_cdf, METH_VARARGS, "Compute CDF histogram"},
    {"serialize_binary", (PyCFunction)(void(*)(void))Histo1D_serialize_binary, METH_NOARGS, "Serialize to binary bytes"},
    {"serialize_json", (PyCFunction)(void(*)(void))Histo1D_serialize_json, METH_NOARGS, "Serialize to JSON string"},
    {"fit_builtin", (PyCFunction)(void(*)(void))Histo1D_fit_builtin, METH_VARARGS | METH_KEYWORDS, "Fit built-in model"},
    {"fit_custom", (PyCFunction)(void(*)(void))Histo1D_fit_custom, METH_VARARGS | METH_KEYWORDS, "Fit custom python callback model"},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject Histo1DType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_libhisto.Histo1D",
    .tp_doc = "1-Dimensional high performance histogram",
    .tp_basicsize = sizeof(Histo1DObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)Histo1D_dealloc,
    .tp_getset = Histo1D_getsetters,
    .tp_methods = Histo1D_methods,
};

/* ------------------------------------------------------------------------- */
/* Histo2D Python Object                                                     */
/* ------------------------------------------------------------------------- */
typedef struct {
    PyObject_HEAD
    histo2d_t *h2d;
} Histo2DObject;

static PyObject *Histo2D_new_from_ptr(histo2d_t *h2d) {
    if (!h2d) Py_RETURN_NONE;
    Histo2DObject *self = (Histo2DObject *)Histo2DType.tp_alloc(&Histo2DType, 0);
    if (!self) {
        histo2d_destroy(h2d);
        return NULL;
    }
    self->h2d = h2d;
    return (PyObject *)self;
}

static void Histo2D_dealloc(Histo2DObject *self) {
    if (self->h2d) {
        histo2d_destroy(self->h2d);
        self->h2d = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Histo2D_create_uniform(PyObject *cls, PyObject *args, PyObject *kwargs) {
    (void)cls;
    static char *kwlist[] = {"nx", "xmin", "xmax", "ny", "ymin", "ymax", "flags", NULL};
    unsigned int nx = 0, ny = 0;
    double xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0;
    unsigned int flags = HISTO_FLAG_NONE;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "IddIdd|I", kwlist,
            &nx, &xmin, &xmax, &ny, &ymin, &ymax, &flags)) {
        return NULL;
    }

    histo2d_t *h2d = histo2d_create_uniform(nx, xmin, xmax, ny, ymin, ymax, flags);
    if (!h2d) {
        PyErr_SetString(HistoError, "Failed to create uniform 2D histogram");
        return NULL;
    }
    return Histo2D_new_from_ptr(h2d);
}

static PyObject *Histo2D_create_variable(PyObject *cls, PyObject *args, PyObject *kwargs) {
    (void)cls;
    static char *kwlist[] = {"xedges", "yedges", "flags", NULL};
    PyObject *x_obj = NULL, *y_obj = NULL;
    unsigned int flags = HISTO_FLAG_NONE;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|I", kwlist, &x_obj, &y_obj, &flags)) {
        return NULL;
    }

    PyObject *x_seq = PySequence_Fast(x_obj, "xedges must be a sequence");
    PyObject *y_seq = PySequence_Fast(y_obj, "yedges must be a sequence");
    if (!x_seq || !y_seq) {
        Py_XDECREF(x_seq); Py_XDECREF(y_seq);
        return NULL;
    }

    Py_ssize_t nx_edges = PySequence_Fast_GET_SIZE(x_seq);
    Py_ssize_t ny_edges = PySequence_Fast_GET_SIZE(y_seq);
    if (nx_edges < 2 || ny_edges < 2) {
        Py_DECREF(x_seq); Py_DECREF(y_seq);
        PyErr_SetString(PyExc_ValueError, "edge sequences must contain at least 2 elements");
        return NULL;
    }

    double *x_arr = (double *)malloc((size_t)nx_edges * sizeof(double));
    double *y_arr = (double *)malloc((size_t)ny_edges * sizeof(double));
    if (!x_arr || !y_arr) {
        free(x_arr); free(y_arr);
        Py_DECREF(x_seq); Py_DECREF(y_seq);
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < nx_edges; i++) x_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(x_seq, i));
    for (Py_ssize_t i = 0; i < ny_edges; i++) y_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(y_seq, i));
    Py_DECREF(x_seq); Py_DECREF(y_seq);

    histo2d_t *h2d = histo2d_create_variable((uint32_t)(nx_edges - 1), x_arr, (uint32_t)(ny_edges - 1), y_arr, flags);
    free(x_arr); free(y_arr);

    if (!h2d) {
        PyErr_SetString(HistoError, "Failed to create variable 2D histogram");
        return NULL;
    }
    return Histo2D_new_from_ptr(h2d);
}

static PyObject *Histo2D_deserialize_binary(PyObject *cls, PyObject *args) {
    (void)cls;
    Py_buffer view;
    if (!PyArg_ParseTuple(args, "y*", &view)) return NULL;

    histo2d_t *out_h = NULL;
    histo_status_t st = histo2d_deserialize_binary(view.buf, (size_t)view.len, &out_h);
    PyBuffer_Release(&view);

    if (st != HISTO_OK || !out_h) {
        set_histo_error(st, "Failed to deserialize 2D binary histogram");
        return NULL;
    }
    return Histo2D_new_from_ptr(out_h);
}

static PyObject *Histo2D_deserialize_json(PyObject *cls, PyObject *args) {
    (void)cls;
    const char *json_str = NULL;
    if (!PyArg_ParseTuple(args, "s", &json_str)) return NULL;

    histo2d_t *out_h = NULL;
    histo_status_t st = histo2d_deserialize_json(json_str, &out_h);
    if (st != HISTO_OK || !out_h) {
        set_histo_error(st, "Failed to deserialize 2D JSON histogram");
        return NULL;
    }
    return Histo2D_new_from_ptr(out_h);
}

static PyObject *Histo2D_fill(Histo2DObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"x", "y", "weight", NULL};
    double x = 0.0, y = 0.0, weight = 1.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "dd|d", kwlist, &x, &y, &weight)) return NULL;

    histo_status_t st = (weight == 1.0) ? histo2d_fill(self->h2d, x, y) : histo2d_fill_w(self->h2d, x, y, weight);
    return PyBool_FromLong(st == HISTO_OK);
}

static PyObject *Histo2D_fill_n(Histo2DObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"x", "y", "weights", NULL};
    PyObject *x_obj = NULL, *y_obj = NULL, *w_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|O", kwlist, &x_obj, &y_obj, &w_obj)) return NULL;

    PyObject *x_seq = PySequence_Fast(x_obj, "x must be a sequence");
    PyObject *y_seq = PySequence_Fast(y_obj, "y must be a sequence");
    if (!x_seq || !y_seq) {
        Py_XDECREF(x_seq); Py_XDECREF(y_seq);
        return NULL;
    }

    Py_ssize_t n = PySequence_Fast_GET_SIZE(x_seq);
    if (PySequence_Fast_GET_SIZE(y_seq) != n) {
        Py_DECREF(x_seq); Py_DECREF(y_seq);
        PyErr_SetString(PyExc_ValueError, "x and y sequences must have identical length");
        return NULL;
    }

    PyObject *w_seq = NULL;
    if (w_obj && w_obj != Py_None) {
        w_seq = PySequence_Fast(w_obj, "weights must be a sequence");
        if (!w_seq || PySequence_Fast_GET_SIZE(w_seq) != n) {
            Py_DECREF(x_seq); Py_DECREF(y_seq); Py_XDECREF(w_seq);
            PyErr_SetString(PyExc_ValueError, "weights sequence must match x length");
            return NULL;
        }
    }

    double *x_arr = (double *)malloc((size_t)n * sizeof(double));
    double *y_arr = (double *)malloc((size_t)n * sizeof(double));
    double *w_arr = w_seq ? (double *)malloc((size_t)n * sizeof(double)) : NULL;
    if (!x_arr || !y_arr || (w_seq && !w_arr)) {
        free(x_arr); free(y_arr); free(w_arr);
        Py_DECREF(x_seq); Py_DECREF(y_seq); Py_XDECREF(w_seq);
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < n; i++) {
        x_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(x_seq, i));
        y_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(y_seq, i));
        if (w_seq) w_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(w_seq, i));
    }
    Py_DECREF(x_seq); Py_DECREF(y_seq); Py_XDECREF(w_seq);

    histo_status_t st = histo2d_fill_n(self->h2d, (size_t)n, x_arr, y_arr, w_arr);
    free(x_arr); free(y_arr); free(w_arr);
    return PyBool_FromLong(st == HISTO_OK || st == HISTO_WARN_NON_FINITE);
}

static PyObject *Histo2D_fill_buffer(Histo2DObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"x", "y", "weights", NULL};
    Py_buffer x_buf, y_buf, w_buf;
    w_buf.buf = NULL;
    PyObject *w_obj = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "y*y*|O", kwlist, &x_buf, &y_buf, &w_obj)) return NULL;

    if (x_buf.len % sizeof(double) != 0 || x_buf.len != y_buf.len) {
        PyBuffer_Release(&x_buf); PyBuffer_Release(&y_buf);
        PyErr_SetString(PyExc_ValueError, "x and y buffers must match in byte length and be multiple of 8 bytes");
        return NULL;
    }

    size_t n = (size_t)x_buf.len / sizeof(double);
    const double *x_arr = (const double *)x_buf.buf;
    const double *y_arr = (const double *)y_buf.buf;
    const double *w_arr = NULL;

    if (w_obj && w_obj != Py_None) {
        if (PyObject_GetBuffer(w_obj, &w_buf, PyBUF_CONTIG_RO) != 0) {
            PyBuffer_Release(&x_buf); PyBuffer_Release(&y_buf);
            return NULL;
        }
        if ((size_t)w_buf.len != (size_t)x_buf.len) {
            PyBuffer_Release(&x_buf); PyBuffer_Release(&y_buf); PyBuffer_Release(&w_buf);
            PyErr_SetString(PyExc_ValueError, "weights buffer length mismatch");
            return NULL;
        }
        w_arr = (const double *)w_buf.buf;
    }

    histo_status_t st = histo2d_fill_n(self->h2d, n, x_arr, y_arr, w_arr);
    PyBuffer_Release(&x_buf); PyBuffer_Release(&y_buf);
    if (w_buf.buf) PyBuffer_Release(&w_buf);

    return PyBool_FromLong(st == HISTO_OK || st == HISTO_WARN_NON_FINITE);
}

/* 2D Getters */
static PyObject *Histo2D_nx(Histo2DObject *self, void *closure) {
    (void)closure;
    return PyLong_FromUnsignedLong(histo2d_nbins_x(self->h2d));
}

static PyObject *Histo2D_ny(Histo2DObject *self, void *closure) {
    (void)closure;
    return PyLong_FromUnsignedLong(histo2d_nbins_y(self->h2d));
}

static PyObject *Histo2D_xmin(Histo2DObject *self, void *closure) {
    (void)closure;
    histo2d_axis_t ax;
    histo2d_axis_x(self->h2d, &ax);
    return PyFloat_FromDouble(ax.min);
}

static PyObject *Histo2D_xmax(Histo2DObject *self, void *closure) {
    (void)closure;
    histo2d_axis_t ax;
    histo2d_axis_x(self->h2d, &ax);
    return PyFloat_FromDouble(ax.max);
}

static PyObject *Histo2D_ymin(Histo2DObject *self, void *closure) {
    (void)closure;
    histo2d_axis_t ax;
    histo2d_axis_y(self->h2d, &ax);
    return PyFloat_FromDouble(ax.min);
}

static PyObject *Histo2D_ymax(Histo2DObject *self, void *closure) {
    (void)closure;
    histo2d_axis_t ax;
    histo2d_axis_y(self->h2d, &ax);
    return PyFloat_FromDouble(ax.max);
}

static PyObject *Histo2D_total_weight(Histo2DObject *self, void *closure) { (void)closure; return PyFloat_FromDouble(histo2d_total_weight(self->h2d)); }
static PyObject *Histo2D_num_entries(Histo2DObject *self, void *closure) { (void)closure; return PyLong_FromUnsignedLongLong(histo2d_num_entries(self->h2d)); }
static PyObject *Histo2D_mean_x(Histo2DObject *self, void *closure) { (void)closure; double v = 0.0; histo2d_mean_x(self->h2d, &v); return PyFloat_FromDouble(v); }
static PyObject *Histo2D_mean_y(Histo2DObject *self, void *closure) { (void)closure; double v = 0.0; histo2d_mean_y(self->h2d, &v); return PyFloat_FromDouble(v); }
static PyObject *Histo2D_variance_x(Histo2DObject *self, void *closure) { (void)closure; double v = 0.0; histo2d_variance_x(self->h2d, &v); return PyFloat_FromDouble(v); }
static PyObject *Histo2D_variance_y(Histo2DObject *self, void *closure) { (void)closure; double v = 0.0; histo2d_variance_y(self->h2d, &v); return PyFloat_FromDouble(v); }
static PyObject *Histo2D_std_dev_x(Histo2DObject *self, void *closure) { (void)closure; double v = 0.0; histo2d_std_dev_x(self->h2d, &v); return PyFloat_FromDouble(v); }
static PyObject *Histo2D_std_dev_y(Histo2DObject *self, void *closure) { (void)closure; double v = 0.0; histo2d_std_dev_y(self->h2d, &v); return PyFloat_FromDouble(v); }
static PyObject *Histo2D_covariance(Histo2DObject *self, void *closure) { (void)closure; double v = 0.0; histo2d_covariance(self->h2d, &v); return PyFloat_FromDouble(v); }
static PyObject *Histo2D_correlation(Histo2DObject *self, void *closure) { (void)closure; double v = 0.0; histo2d_correlation(self->h2d, &v); return PyFloat_FromDouble(v); }
static PyObject *Histo2D_flags(Histo2DObject *self, void *closure) {
    (void)closure;
    if (!self->h2d) return PyLong_FromUnsignedLong(0);
    return PyLong_FromUnsignedLong(self->h2d->flags);
}

static PyGetSetDef Histo2D_getsetters[] = {
    {"flags", (getter)Histo2D_flags, NULL, "2D Histogram feature flags", NULL},
    {"nx", (getter)Histo2D_nx, NULL, "Bins along X", NULL},
    {"ny", (getter)Histo2D_ny, NULL, "Bins along Y", NULL},
    {"xmin", (getter)Histo2D_xmin, NULL, "X minimum", NULL},
    {"xmax", (getter)Histo2D_xmax, NULL, "X maximum", NULL},
    {"ymin", (getter)Histo2D_ymin, NULL, "Y minimum", NULL},
    {"ymax", (getter)Histo2D_ymax, NULL, "Y maximum", NULL},
    {"total_weight", (getter)Histo2D_total_weight, NULL, "Total weight", NULL},
    {"num_entries", (getter)Histo2D_num_entries, NULL, "Total entries", NULL},
    {"mean_x", (getter)Histo2D_mean_x, NULL, "Mean X", NULL},
    {"mean_y", (getter)Histo2D_mean_y, NULL, "Mean Y", NULL},
    {"variance_x", (getter)Histo2D_variance_x, NULL, "Variance X", NULL},
    {"variance_y", (getter)Histo2D_variance_y, NULL, "Variance Y", NULL},
    {"std_dev_x", (getter)Histo2D_std_dev_x, NULL, "Standard Deviation X", NULL},
    {"std_dev_y", (getter)Histo2D_std_dev_y, NULL, "Standard Deviation Y", NULL},
    {"covariance", (getter)Histo2D_covariance, NULL, "Covariance", NULL},
    {"correlation", (getter)Histo2D_correlation, NULL, "Pearson correlation", NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

/* 2D Methods */
static PyObject *Histo2D_bin_content(Histo2DObject *self, PyObject *args) {
    unsigned int ix = 0, iy = 0;
    if (!PyArg_ParseTuple(args, "II", &ix, &iy)) return NULL;
    double val = 0.0;
    histo_status_t st = histo2d_bin_content(self->h2d, ix, iy, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo2D_bin_error(Histo2DObject *self, PyObject *args) {
    unsigned int ix = 0, iy = 0;
    if (!PyArg_ParseTuple(args, "II", &ix, &iy)) return NULL;
    double val = 0.0;
    histo_status_t st = histo2d_bin_error(self->h2d, ix, iy, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo2D_bin_sum_w2(Histo2DObject *self, PyObject *args) {
    unsigned int ix = 0, iy = 0;
    if (!PyArg_ParseTuple(args, "II", &ix, &iy)) return NULL;
    double val = 0.0;
    histo_status_t st = histo2d_bin_sum_w2(self->h2d, ix, iy, &val);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo2D_bin_bounds(Histo2DObject *self, PyObject *args) {
    unsigned int ix = 0, iy = 0;
    if (!PyArg_ParseTuple(args, "II", &ix, &iy)) return NULL;
    double xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0;
    histo_status_t st = histo2d_bin_bounds(self->h2d, ix, iy, &xmin, &xmax, &ymin, &ymax);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return Py_BuildValue("(dddd)", xmin, xmax, ymin, ymax);
}

static PyObject *Histo2D_bin_center(Histo2DObject *self, PyObject *args) {
    unsigned int ix = 0, iy = 0;
    if (!PyArg_ParseTuple(args, "II", &ix, &iy)) return NULL;
    double cx = 0.0, cy = 0.0;
    histo_status_t st = histo2d_bin_center(self->h2d, ix, iy, &cx, &cy);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return Py_BuildValue("(dd)", cx, cy);
}

static PyObject *Histo2D_find_bin(Histo2DObject *self, PyObject *args) {
    double x = 0.0, y = 0.0;
    if (!PyArg_ParseTuple(args, "dd", &x, &y)) return NULL;
    int64_t ix = 0, iy = 0;
    histo_status_t st = histo2d_find_bin(self->h2d, x, y, &ix, &iy);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return Py_BuildValue("(LL)", (long long)ix, (long long)iy);
}

static PyObject *Histo2D_find_region(Histo2DObject *self, PyObject *args) {
    double x = 0.0, y = 0.0;
    if (!PyArg_ParseTuple(args, "dd", &x, &y)) return NULL;
    histo2d_region_t reg = HISTO2D_REGION_CENTER;
    histo_status_t st = histo2d_find_region(self->h2d, x, y, &reg);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyLong_FromLong((long)reg);
}

static PyObject *Histo2D_region_content(Histo2DObject *self, PyObject *args) {
    int region = 0;
    if (!PyArg_ParseTuple(args, "i", &region)) return NULL;
    if (region < 0 || region >= HISTO2D_REGION_COUNT || !self->h2d) {
        PyErr_SetString(PyExc_ValueError, "Invalid region index (must be 0..8)");
        return NULL;
    }
    double weight = 0.0;
    uint64_t count = 0;
    histo_status_t st = histo2d_region_content(self->h2d, (histo2d_region_t)region, &weight, &count);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(weight);
}

static PyObject *Histo2D_region_sum_w2(Histo2DObject *self, PyObject *args) {
    int region = 0;
    if (!PyArg_ParseTuple(args, "i", &region)) return NULL;
    if (region < 0 || region >= HISTO2D_REGION_COUNT || !self->h2d) {
        PyErr_SetString(PyExc_ValueError, "Invalid region index (must be 0..8)");
        return NULL;
    }
    return PyFloat_FromDouble(self->h2d->guards[region].sum_w2);
}

static PyObject *Histo2D_region_count(Histo2DObject *self, PyObject *args) {
    int region = 0;
    if (!PyArg_ParseTuple(args, "i", &region)) return NULL;
    if (region < 0 || region >= HISTO2D_REGION_COUNT || !self->h2d) {
        PyErr_SetString(PyExc_ValueError, "Invalid region index (must be 0..8)");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong(self->h2d->guards[region].count);
}

static PyObject *Histo2D_integral(Histo2DObject *self, PyObject *args) {
    unsigned int ix_min = 0, ix_max = 0, iy_min = 0, iy_max = 0;
    uint32_t nx = histo2d_nbins_x(self->h2d);
    uint32_t ny = histo2d_nbins_y(self->h2d);
    if (nx == 0 || ny == 0) return PyFloat_FromDouble(0.0);
    ix_max = nx - 1;
    iy_max = ny - 1;

    if (!PyArg_ParseTuple(args, "|IIII", &ix_min, &ix_max, &iy_min, &iy_max)) return NULL;

    double val = 0.0;
    histo_status_t st;
    if (PyTuple_Size(args) == 4) {
        st = histo2d_integral_range(self->h2d, ix_min, ix_max, iy_min, iy_max, &val);
    } else {
        st = histo2d_integral(self->h2d, &val);
    }
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    return PyFloat_FromDouble(val);
}

static PyObject *Histo2D_slice_x(Histo2DObject *self, PyObject *args) {
    unsigned int iy_min = 0, iy_max = 0;
    if (!PyArg_ParseTuple(args, "II", &iy_min, &iy_max)) return NULL;
    histo_t *p = NULL;
    histo_status_t st = histo2d_slice_x(self->h2d, iy_min, iy_max, &p);
    if (st != HISTO_OK || !p) { set_histo_error(st, "slice_x failed"); return NULL; }
    return Histo1D_new_from_ptr(p);
}

static PyObject *Histo2D_slice_y(Histo2DObject *self, PyObject *args) {
    unsigned int ix_min = 0, ix_max = 0;
    if (!PyArg_ParseTuple(args, "II", &ix_min, &ix_max)) return NULL;
    histo_t *p = NULL;
    histo_status_t st = histo2d_slice_y(self->h2d, ix_min, ix_max, &p);
    if (st != HISTO_OK || !p) { set_histo_error(st, "slice_y failed"); return NULL; }
    return Histo1D_new_from_ptr(p);
}

static PyObject *Histo2D_scale(Histo2DObject *self, PyObject *args) {
    double factor = 1.0;
    if (!PyArg_ParseTuple(args, "d", &factor)) return NULL;
    histo_status_t st = histo2d_scale(self->h2d, factor);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Histo2D_normalize(Histo2DObject *self, PyObject *args) {
    double target = 1.0;
    if (!PyArg_ParseTuple(args, "|d", &target)) return NULL;
    histo_status_t st = histo2d_normalize(self->h2d, target);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Histo2D_rebin(Histo2DObject *self, PyObject *args) {
    unsigned int fx = 1, fy = 1;
    if (!PyArg_ParseTuple(args, "II", &fx, &fy)) return NULL;
    histo2d_t *rebinned = NULL;
    histo_status_t st = histo2d_rebin(self->h2d, fx, fy, &rebinned);
    if (st != HISTO_OK || !rebinned) { set_histo_error(st, "rebin failed"); return NULL; }
    return Histo2D_new_from_ptr(rebinned);
}

static PyObject *Histo2D_add(Histo2DObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    double scale = 1.0;
    if (!PyArg_ParseTuple(args, "O!|d", &Histo2DType, &other_obj, &scale)) return NULL;
    Histo2DObject *other = (Histo2DObject *)other_obj;
    histo_status_t st = histo2d_add(self->h2d, other->h2d, scale);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Histo2D_subtract(Histo2DObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    if (!PyArg_ParseTuple(args, "O!", &Histo2DType, &other_obj)) return NULL;
    Histo2DObject *other = (Histo2DObject *)other_obj;
    histo_status_t st = histo2d_subtract(self->h2d, other->h2d);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Histo2D_multiply(Histo2DObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    if (!PyArg_ParseTuple(args, "O!", &Histo2DType, &other_obj)) return NULL;
    Histo2DObject *other = (Histo2DObject *)other_obj;
    histo_status_t st = histo2d_multiply(self->h2d, other->h2d);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Histo2D_divide(Histo2DObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    if (!PyArg_ParseTuple(args, "O!", &Histo2DType, &other_obj)) return NULL;
    Histo2DObject *other = (Histo2DObject *)other_obj;
    histo_status_t st = histo2d_divide(self->h2d, other->h2d);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Histo2D_reset(Histo2DObject *self, PyObject *Py_UNUSED(ignored)) {
    histo_status_t st = histo2d_reset(self->h2d);
    if (st != HISTO_OK) { set_histo_error(st, NULL); return NULL; }
    Py_RETURN_NONE;
}

static PyObject *Histo2D_clone(Histo2DObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"empty", NULL};
    int empty = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|p", kwlist, &empty)) return NULL;
    histo2d_t *cloned = histo2d_clone(self->h2d, empty ? true : false);
    if (!cloned) {
        PyErr_SetString(HistoError, "Failed to clone 2D histogram");
        return NULL;
    }
    return Histo2D_new_from_ptr(cloned);
}

static PyObject *Histo2D_project_x(Histo2DObject *self, PyObject *Py_UNUSED(ignored)) {
    histo_t *p = NULL;
    histo_status_t st = histo2d_project_x(self->h2d, &p);
    if (st != HISTO_OK || !p) { set_histo_error(st, "project_x failed"); return NULL; }
    return Histo1D_new_from_ptr(p);
}

static PyObject *Histo2D_project_y(Histo2DObject *self, PyObject *Py_UNUSED(ignored)) {
    histo_t *p = NULL;
    histo_status_t st = histo2d_project_y(self->h2d, &p);
    if (st != HISTO_OK || !p) { set_histo_error(st, "project_y failed"); return NULL; }
    return Histo1D_new_from_ptr(p);
}

static PyObject *Histo2D_profile_x(Histo2DObject *self, PyObject *Py_UNUSED(ignored)) {
    histo_t *p = NULL;
    histo_status_t st = histo2d_profile_x(self->h2d, &p);
    if (st != HISTO_OK || !p) { set_histo_error(st, "profile_x failed"); return NULL; }
    return Histo1D_new_from_ptr(p);
}

static PyObject *Histo2D_profile_y(Histo2DObject *self, PyObject *Py_UNUSED(ignored)) {
    histo_t *p = NULL;
    histo_status_t st = histo2d_profile_y(self->h2d, &p);
    if (st != HISTO_OK || !p) { set_histo_error(st, "profile_y failed"); return NULL; }
    return Histo1D_new_from_ptr(p);
}


static PyObject *Histo2D_serialize_binary(Histo2DObject *self, PyObject *Py_UNUSED(ignored)) {
    void *buf = NULL;
    size_t sz = 0;
    histo_status_t st = histo2d_serialize_binary_alloc(self->h2d, &buf, &sz);
    if (st != HISTO_OK || !buf) { set_histo_error(st, NULL); return NULL; }
    PyObject *res = PyBytes_FromStringAndSize((const char *)buf, (Py_ssize_t)sz);
    histo_free_buffer(buf);
    return res;
}

static PyObject *Histo2D_serialize_json(Histo2DObject *self, PyObject *Py_UNUSED(ignored)) {
    char *json = NULL;
    size_t sz = 0;
    histo_status_t st = histo2d_serialize_json_alloc(self->h2d, &json, &sz);
    if (st != HISTO_OK || !json) { set_histo_error(st, NULL); return NULL; }
    PyObject *res = PyUnicode_FromString(json);
    histo_free_buffer(json);
    return res;
}

static PyMethodDef Histo2D_methods[] = {
    {"create_uniform", (PyCFunction)(void(*)(void))Histo2D_create_uniform, METH_VARARGS | METH_KEYWORDS | METH_CLASS, "Create uniform 2D histogram"},
    {"create_variable", (PyCFunction)(void(*)(void))Histo2D_create_variable, METH_VARARGS | METH_KEYWORDS | METH_CLASS, "Create variable 2D histogram"},
    {"deserialize_binary", (PyCFunction)(void(*)(void))Histo2D_deserialize_binary, METH_VARARGS | METH_CLASS, "Deserialize 2D from binary"},
    {"deserialize_json", (PyCFunction)(void(*)(void))Histo2D_deserialize_json, METH_VARARGS | METH_CLASS, "Deserialize 2D from JSON"},
    {"fill", (PyCFunction)(void(*)(void))Histo2D_fill, METH_VARARGS | METH_KEYWORDS, "Fill single 2D sample"},
    {"fill_n", (PyCFunction)(void(*)(void))Histo2D_fill_n, METH_VARARGS | METH_KEYWORDS, "Batch fill 2D sequences"},
    {"fill_buffer", (PyCFunction)(void(*)(void))Histo2D_fill_buffer, METH_VARARGS | METH_KEYWORDS, "Zero-copy 2D SIMD fill from buffers"},
    {"bin_content", (PyCFunction)(void(*)(void))Histo2D_bin_content, METH_VARARGS, "Get 2D bin content"},
    {"bin_error", (PyCFunction)(void(*)(void))Histo2D_bin_error, METH_VARARGS, "Get 2D bin uncertainty"},
    {"bin_sum_w2", (PyCFunction)(void(*)(void))Histo2D_bin_sum_w2, METH_VARARGS, "Get 2D bin sum of squared weights"},
    {"bin_bounds", (PyCFunction)(void(*)(void))Histo2D_bin_bounds, METH_VARARGS, "Get 2D bin bounding box (xmin, xmax, ymin, ymax)"},
    {"bin_center", (PyCFunction)(void(*)(void))Histo2D_bin_center, METH_VARARGS, "Get 2D bin midpoint (cx, cy)"},
    {"find_bin", (PyCFunction)(void(*)(void))Histo2D_find_bin, METH_VARARGS, "Find (ix, iy) bin indices for coordinate (x, y)"},
    {"find_region", (PyCFunction)(void(*)(void))Histo2D_find_region, METH_VARARGS, "Identify 9-guard region for coordinate (x, y)"},
    {"region_content", (PyCFunction)(void(*)(void))Histo2D_region_content, METH_VARARGS, "Get 2D guard region accumulated weight"},
    {"region_sum_w2", (PyCFunction)(void(*)(void))Histo2D_region_sum_w2, METH_VARARGS, "Get 2D guard region sum of squared weights"},
    {"region_count", (PyCFunction)(void(*)(void))Histo2D_region_count, METH_VARARGS, "Get 2D guard region entry count"},
    {"integral", (PyCFunction)(void(*)(void))Histo2D_integral, METH_VARARGS, "Calculate 2D volume/integral"},
    {"project_x", (PyCFunction)(void(*)(void))Histo2D_project_x, METH_VARARGS, "Project along X to 1D"},
    {"project_y", (PyCFunction)(void(*)(void))Histo2D_project_y, METH_VARARGS, "Project along Y to 1D"},
    {"slice_x", (PyCFunction)(void(*)(void))Histo2D_slice_x, METH_VARARGS, "Slice along X across Y-bin interval"},
    {"slice_y", (PyCFunction)(void(*)(void))Histo2D_slice_y, METH_VARARGS, "Slice along Y across X-bin interval"},
    {"profile_x", (PyCFunction)(void(*)(void))Histo2D_profile_x, METH_NOARGS, "Profile histogram along X"},
    {"profile_y", (PyCFunction)(void(*)(void))Histo2D_profile_y, METH_NOARGS, "Profile histogram along Y"},
    {"scale", (PyCFunction)(void(*)(void))Histo2D_scale, METH_VARARGS, "Scale 2D histogram by factor"},
    {"normalize", (PyCFunction)(void(*)(void))Histo2D_normalize, METH_VARARGS, "Normalize 2D histogram to target area"},
    {"rebin", (PyCFunction)(void(*)(void))Histo2D_rebin, METH_VARARGS, "Rebin 2D histogram by (factor_x, factor_y)"},
    {"add", (PyCFunction)(void(*)(void))Histo2D_add, METH_VARARGS, "In-place add another 2D histogram"},
    {"subtract", (PyCFunction)(void(*)(void))Histo2D_subtract, METH_VARARGS, "In-place subtract another 2D histogram"},
    {"multiply", (PyCFunction)(void(*)(void))Histo2D_multiply, METH_VARARGS, "In-place multiply by another 2D histogram"},
    {"divide", (PyCFunction)(void(*)(void))Histo2D_divide, METH_VARARGS, "In-place divide by another 2D histogram"},
    {"reset", (PyCFunction)(void(*)(void))Histo2D_reset, METH_NOARGS, "Reset all 2D bins and moments to zero"},
    {"clone", (PyCFunction)(void(*)(void))Histo2D_clone, METH_VARARGS | METH_KEYWORDS, "Clone 2D histogram"},
    {"serialize_binary", (PyCFunction)(void(*)(void))Histo2D_serialize_binary, METH_NOARGS, "Serialize to binary bytes"},
    {"serialize_json", (PyCFunction)(void(*)(void))Histo2D_serialize_json, METH_NOARGS, "Serialize to JSON"},
    {NULL, NULL, 0, NULL}
};


static PyTypeObject Histo2DType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_libhisto.Histo2D",
    .tp_doc = "2-Dimensional high performance histogram",
    .tp_basicsize = sizeof(Histo2DObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)Histo2D_dealloc,
    .tp_getset = Histo2D_getsetters,
    .tp_methods = Histo2D_methods,
};

/* ------------------------------------------------------------------------- */
/* DDSketch Python Object                                                    */
/* ------------------------------------------------------------------------- */
typedef struct {
    PyObject_HEAD
    histo_sketch_t *s;
} SketchObject;

static PyObject *Sketch_new_from_ptr(histo_sketch_t *s) {
    if (!s) Py_RETURN_NONE;
    SketchObject *self = (SketchObject *)SketchType.tp_alloc(&SketchType, 0);
    if (!self) { histo_sketch_destroy(s); return NULL; }
    self->s = s;
    return (PyObject *)self;
}

static void Sketch_dealloc(SketchObject *self) {
    if (self->s) {
        histo_sketch_destroy(self->s);
        self->s = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Sketch_create(PyObject *cls, PyObject *args, PyObject *kwargs) {
    (void)cls;
    static char *kwlist[] = {"alpha", "max_bins", NULL};
    double alpha = 0.01;
    unsigned int max_bins = 2048;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|dI", kwlist, &alpha, &max_bins)) return NULL;

    histo_sketch_t *s = histo_sketch_create(alpha, max_bins);
    if (!s) { PyErr_SetString(HistoError, "Failed to create sketch"); return NULL; }
    return Sketch_new_from_ptr(s);
}

static PyObject *Sketch_deserialize_binary(PyObject *cls, PyObject *args) {
    (void)cls;
    Py_buffer view;
    if (!PyArg_ParseTuple(args, "y*", &view)) return NULL;

    histo_sketch_t *s = NULL;
    histo_status_t st = histo_sketch_deserialize_binary(view.buf, (size_t)view.len, &s);
    PyBuffer_Release(&view);
    if (st != HISTO_OK || !s) { set_histo_error(st, "Sketch deserialization failed"); return NULL; }
    return Sketch_new_from_ptr(s);
}

static PyObject *Sketch_insert(SketchObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"val", "weight", NULL};
    double val = 0.0, weight = 1.0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "d|d", kwlist, &val, &weight)) return NULL;

    histo_status_t st = (weight == 1.0) ? histo_sketch_insert(self->s, val) : histo_sketch_insert_w(self->s, val, weight);
    return PyBool_FromLong(st == HISTO_OK);
}

static PyObject *Sketch_insert_n(SketchObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"vals", "weights", NULL};
    PyObject *v_obj = NULL, *w_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O", kwlist, &v_obj, &w_obj)) return NULL;

    PyObject *v_seq = PySequence_Fast(v_obj, "vals must be a sequence");
    if (!v_seq) return NULL;
    Py_ssize_t n = PySequence_Fast_GET_SIZE(v_seq);
    if (n == 0) { Py_DECREF(v_seq); Py_RETURN_TRUE; }

    PyObject *w_seq = (w_obj && w_obj != Py_None) ? PySequence_Fast(w_obj, "weights must be sequence") : NULL;
    double *v_arr = (double *)malloc((size_t)n * sizeof(double));
    double *w_arr = w_seq ? (double *)malloc((size_t)n * sizeof(double)) : NULL;
    if (!v_arr || (w_seq && !w_arr)) {
        free(v_arr); free(w_arr); Py_DECREF(v_seq); Py_XDECREF(w_seq);
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < n; i++) {
        v_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(v_seq, i));
        if (w_seq) w_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(w_seq, i));
    }
    Py_DECREF(v_seq); Py_XDECREF(w_seq);

    histo_status_t st = histo_sketch_insert_n(self->s, (size_t)n, v_arr, w_arr);
    free(v_arr); free(w_arr);
    return PyBool_FromLong(st == HISTO_OK);
}

static PyObject *Sketch_insert_buffer(SketchObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"vals", "weights", NULL};
    Py_buffer v_buf, w_buf;
    w_buf.buf = NULL;
    PyObject *w_obj = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "y*|O", kwlist, &v_buf, &w_obj)) return NULL;
    if (v_buf.len % sizeof(double) != 0) {
        PyBuffer_Release(&v_buf);
        PyErr_SetString(PyExc_ValueError, "buffer must be multiple of 8 bytes");
        return NULL;
    }
    size_t n = (size_t)v_buf.len / sizeof(double);
    const double *v_arr = (const double *)v_buf.buf;
    const double *w_arr = NULL;

    if (w_obj && w_obj != Py_None) {
        if (PyObject_GetBuffer(w_obj, &w_buf, PyBUF_CONTIG_RO) != 0) {
            PyBuffer_Release(&v_buf); return NULL;
        }
        w_arr = (const double *)w_buf.buf;
    }

    histo_status_t st = histo_sketch_insert_n(self->s, n, v_arr, w_arr);
    PyBuffer_Release(&v_buf);
    if (w_buf.buf) PyBuffer_Release(&w_buf);

    return PyBool_FromLong(st == HISTO_OK);
}

static PyObject *Sketch_quantile(SketchObject *self, PyObject *args) {
    double q = 0.5;
    if (!PyArg_ParseTuple(args, "d", &q)) return NULL;
    double out = 0.0;
    histo_status_t st = histo_sketch_quantile(self->s, q, &out);
    if (st != HISTO_OK) { set_histo_error(st, "quantile query failed"); return NULL; }
    return PyFloat_FromDouble(out);
}

static PyObject *Sketch_merge(SketchObject *self, PyObject *args) {
    PyObject *other_obj = NULL;
    if (!PyArg_ParseTuple(args, "O!", &SketchType, &other_obj)) return NULL;
    SketchObject *other = (SketchObject *)other_obj;

    histo_status_t st = histo_sketch_merge(self->s, other->s);
    if (st != HISTO_OK) { set_histo_error(st, "merge failed"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject *Sketch_reset(SketchObject *self, PyObject *Py_UNUSED(ignored)) {
    histo_sketch_reset(self->s);
    Py_RETURN_NONE;
}

static PyObject *Sketch_min(SketchObject *self, void *closure) { (void)closure; return PyFloat_FromDouble(histo_sketch_min(self->s)); }
static PyObject *Sketch_max(SketchObject *self, void *closure) { (void)closure; return PyFloat_FromDouble(histo_sketch_max(self->s)); }
static PyObject *Sketch_total_weight(SketchObject *self, void *closure) { (void)closure; return PyFloat_FromDouble(histo_sketch_total_weight(self->s)); }
static PyObject *Sketch_num_entries(SketchObject *self, void *closure) { (void)closure; return PyLong_FromUnsignedLongLong(histo_sketch_num_entries(self->s)); }

static PyGetSetDef Sketch_getsetters[] = {
    {"min", (getter)Sketch_min, NULL, "Minimum observed value", NULL},
    {"max", (getter)Sketch_max, NULL, "Maximum observed value", NULL},
    {"total_weight", (getter)Sketch_total_weight, NULL, "Total weight", NULL},
    {"num_entries", (getter)Sketch_num_entries, NULL, "Total entries", NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static PyObject *Sketch_serialize_binary(SketchObject *self, PyObject *Py_UNUSED(ignored)) {
    void *buf = NULL;
    size_t sz = 0;
    histo_status_t st = histo_sketch_serialize_binary(self->s, &buf, &sz);
    if (st != HISTO_OK || !buf) { set_histo_error(st, NULL); return NULL; }
    PyObject *res = PyBytes_FromStringAndSize((const char *)buf, (Py_ssize_t)sz);
    histo_free_buffer(buf);
    return res;
}

static PyMethodDef Sketch_methods[] = {
    {"create", (PyCFunction)(void(*)(void))Sketch_create, METH_VARARGS | METH_KEYWORDS | METH_CLASS, "Create DDSketch"},
    {"deserialize_binary", (PyCFunction)(void(*)(void))Sketch_deserialize_binary, METH_VARARGS | METH_CLASS, "Deserialize sketch from binary"},
    {"insert", (PyCFunction)(void(*)(void))Sketch_insert, METH_VARARGS | METH_KEYWORDS, "Insert single value"},
    {"insert_n", (PyCFunction)(void(*)(void))Sketch_insert_n, METH_VARARGS | METH_KEYWORDS, "Batch insert values"},
    {"insert_buffer", (PyCFunction)(void(*)(void))Sketch_insert_buffer, METH_VARARGS | METH_KEYWORDS, "Zero-copy insert from buffer"},
    {"quantile", (PyCFunction)(void(*)(void))Sketch_quantile, METH_VARARGS, "Query quantile"},
    {"merge", (PyCFunction)(void(*)(void))Sketch_merge, METH_VARARGS, "Merge other sketch"},
    {"reset", (PyCFunction)(void(*)(void))Sketch_reset, METH_NOARGS, "Reset sketch"},
    {"serialize_binary", (PyCFunction)(void(*)(void))Sketch_serialize_binary, METH_NOARGS, "Serialize to binary bytes"},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject SketchType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_libhisto.Sketch",
    .tp_doc = "DDSketch bounded relative-error streaming quantile sketch",
    .tp_basicsize = sizeof(SketchObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)Sketch_dealloc,
    .tp_getset = Sketch_getsetters,
    .tp_methods = Sketch_methods,
};

/* ------------------------------------------------------------------------- */
/* KDE Python Object                                                         */
/* ------------------------------------------------------------------------- */
typedef struct {
    PyObject_HEAD
    histo_kde_t *kde;
} KDEObject;

static PyObject *KDE_new_from_ptr(histo_kde_t *kde) {
    if (!kde) Py_RETURN_NONE;
    KDEObject *self = (KDEObject *)KDEType.tp_alloc(&KDEType, 0);
    if (!self) { histo_kde_destroy(kde); return NULL; }
    self->kde = kde;
    return (PyObject *)self;
}

static void KDE_dealloc(KDEObject *self) {
    if (self->kde) {
        histo_kde_destroy(self->kde);
        self->kde = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *KDE_create(PyObject *cls, PyObject *args, PyObject *kwargs) {
    (void)cls;
    static char *kwlist[] = {"samples", "weights", "kernel", "bw_method", "bandwidth", "bw_adjust", NULL};
    PyObject *s_obj = NULL, *w_obj = NULL;
    int kernel = 0, bw_method = 0;
    double bandwidth = 0.0, bw_adjust = 1.0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|Oiidd", kwlist,
                                     &s_obj, &w_obj, &kernel, &bw_method, &bandwidth, &bw_adjust)) {
        return NULL;
    }

    PyObject *s_seq = PySequence_Fast(s_obj, "samples must be a sequence or array");
    if (!s_seq) return NULL;
    Py_ssize_t n = PySequence_Fast_GET_SIZE(s_seq);
    if (n == 0) {
        Py_DECREF(s_seq);
        PyErr_SetString(PyExc_ValueError, "samples sequence cannot be empty");
        return NULL;
    }

    PyObject *w_seq = (w_obj && w_obj != Py_None) ? PySequence_Fast(w_obj, "weights must be a sequence") : NULL;
    if (w_obj && w_obj != Py_None && !w_seq) {
        Py_DECREF(s_seq);
        return NULL;
    }

    double *s_arr = (double *)malloc((size_t)n * sizeof(double));
    double *w_arr = w_seq ? (double *)malloc((size_t)n * sizeof(double)) : NULL;
    if (!s_arr || (w_seq && !w_arr)) {
        free(s_arr); free(w_arr);
        Py_DECREF(s_seq); Py_XDECREF(w_seq);
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < n; i++) {
        s_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(s_seq, i));
        if (w_seq) {
            w_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(w_seq, i));
        }
    }
    Py_DECREF(s_seq); Py_XDECREF(w_seq);

    histo_kde_options_t opts;
    opts.kernel = (histo_kde_kernel_t)kernel;
    opts.bw_method = (histo_kde_bandwidth_method_t)bw_method;
    opts.bandwidth = bandwidth;
    opts.bw_adjust = bw_adjust;

    histo_kde_t *kde = histo_kde_create((size_t)n, s_arr, w_arr, &opts);
    free(s_arr); free(w_arr);

    if (!kde) {
        PyErr_SetString(HistoError, "Failed to construct KDE model");
        return NULL;
    }
    return KDE_new_from_ptr(kde);
}

static PyObject *KDE_create_from_histo(PyObject *cls, PyObject *args, PyObject *kwargs) {
    (void)cls;
    static char *kwlist[] = {"histo", "kernel", "bw_method", "bandwidth", "bw_adjust", NULL};
    PyObject *h_obj = NULL;
    int kernel = 0, bw_method = 0;
    double bandwidth = 0.0, bw_adjust = 1.0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|iidd", kwlist,
                                     &h_obj, &kernel, &bw_method, &bandwidth, &bw_adjust)) {
        return NULL;
    }

    if (!PyObject_IsInstance(h_obj, (PyObject *)&Histo1DType)) {
        PyErr_SetString(PyExc_TypeError, "Expected a Histo1D instance");
        return NULL;
    }

    Histo1DObject *ho = (Histo1DObject *)h_obj;
    histo_kde_options_t opts;
    opts.kernel = (histo_kde_kernel_t)kernel;
    opts.bw_method = (histo_kde_bandwidth_method_t)bw_method;
    opts.bandwidth = bandwidth;
    opts.bw_adjust = bw_adjust;

    histo_kde_t *kde = histo_kde_create_from_histo(ho->h, &opts);
    if (!kde) {
        PyErr_SetString(HistoError, "Failed to construct KDE model from histogram");
        return NULL;
    }
    return KDE_new_from_ptr(kde);
}

static PyObject *KDE_eval(KDEObject *self, PyObject *args) {
    PyObject *x_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &x_obj)) return NULL;

    if (PyFloat_Check(x_obj) || PyLong_Check(x_obj)) {
        double x = PyFloat_AsDouble(x_obj);
        double val = histo_kde_eval(self->kde, x);
        return PyFloat_FromDouble(val);
    }

    PyObject *seq = PySequence_Fast(x_obj, "x must be a number or sequence of numbers");
    if (!seq) return NULL;
    Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);

    PyObject *list = PyList_New(n);
    if (!list) { Py_DECREF(seq); return NULL; }

    for (Py_ssize_t i = 0; i < n; i++) {
        double x = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(seq, i));
        double val = histo_kde_eval(self->kde, x);
        PyList_SET_ITEM(list, i, PyFloat_FromDouble(val));
    }
    Py_DECREF(seq);
    return list;
}

static PyObject *KDE_cdf(KDEObject *self, PyObject *args) {
    double x = 0.0;
    if (!PyArg_ParseTuple(args, "d", &x)) return NULL;
    return PyFloat_FromDouble(histo_kde_cdf(self->kde, x));
}

static PyObject *KDE_quantile(KDEObject *self, PyObject *args) {
    double q = 0.0;
    if (!PyArg_ParseTuple(args, "d", &q)) return NULL;
    double out_val = 0.0;
    histo_status_t st = histo_kde_quantile(self->kde, q, &out_val);
    if (st != HISTO_OK) {
        set_histo_error(st, "KDE quantile calculation failed");
        return NULL;
    }
    return PyFloat_FromDouble(out_val);
}

static PyObject *KDE_sample(KDEObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"n", "seed", NULL};
    Py_ssize_t n = 1;
    unsigned long long seed = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "n|K", kwlist, &n, &seed)) return NULL;
    if (n <= 0) {
        return PyList_New(0);
    }

    double *buf = (double *)malloc((size_t)n * sizeof(double));
    if (!buf) return PyErr_NoMemory();

    histo_status_t st = histo_kde_sample(self->kde, (size_t)n, buf, (uint64_t)seed);
    if (st != HISTO_OK) {
        free(buf);
        set_histo_error(st, "KDE sampling failed");
        return NULL;
    }

    PyObject *list = PyList_New(n);
    if (!list) { free(buf); return NULL; }
    for (Py_ssize_t i = 0; i < n; i++) {
        PyList_SET_ITEM(list, i, PyFloat_FromDouble(buf[i]));
    }
    free(buf);
    return list;
}

static PyObject *KDE_get_bandwidth(KDEObject *self, void *closure) {
    (void)closure;
    return PyFloat_FromDouble(histo_kde_get_bandwidth(self->kde));
}

static PyObject *KDE_get_kernel(KDEObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong((long)histo_kde_get_kernel(self->kde));
}

static PyObject *KDE_get_n_points(KDEObject *self, void *closure) {
    (void)closure;
    return PyLong_FromSize_t(histo_kde_num_points(self->kde));
}

static PyGetSetDef KDE_getsetters[] = {
    {"bandwidth", (getter)KDE_get_bandwidth, NULL, "Bandwidth h", NULL},
    {"kernel", (getter)KDE_get_kernel, NULL, "Kernel function type", NULL},
    {"n_points", (getter)KDE_get_n_points, NULL, "Number of sample points", NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static PyMethodDef KDE_methods[] = {
    {"create", (PyCFunction)(void(*)(void))KDE_create, METH_VARARGS | METH_KEYWORDS | METH_CLASS, "Create KDE model from samples"},
    {"create_from_histo", (PyCFunction)(void(*)(void))KDE_create_from_histo, METH_VARARGS | METH_KEYWORDS | METH_CLASS, "Create KDE model from histogram"},
    {"eval", (PyCFunction)(void(*)(void))KDE_eval, METH_VARARGS, "Evaluate estimated PDF"},
    {"cdf", (PyCFunction)(void(*)(void))KDE_cdf, METH_VARARGS, "Evaluate estimated CDF"},
    {"quantile", (PyCFunction)(void(*)(void))KDE_quantile, METH_VARARGS, "Evaluate estimated quantile"},
    {"sample", (PyCFunction)(void(*)(void))KDE_sample, METH_VARARGS | METH_KEYWORDS, "Generate random samples"},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject KDEType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_libhisto.KDE",
    .tp_doc = "Kernel Density Estimation non-parametric continuous density estimator",
    .tp_basicsize = sizeof(KDEObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)KDE_dealloc,
    .tp_getset = KDE_getsetters,
    .tp_methods = KDE_methods,
};

/* ------------------------------------------------------------------------- */
/* Auto-binning and CLI runner functions                                     */
/* ------------------------------------------------------------------------- */
static PyObject *py_estimate_bins(PyObject *self, PyObject *args, PyObject *kwargs) {
    (void)self;
    static char *kwlist[] = {"samples", "rule", NULL};
    PyObject *s_obj = NULL;
    int rule = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|i", kwlist, &s_obj, &rule)) return NULL;

    PyObject *s_seq = PySequence_Fast(s_obj, "samples must be a sequence or array");
    if (!s_seq) return NULL;
    Py_ssize_t n = PySequence_Fast_GET_SIZE(s_seq);
    if (n == 0) {
        Py_DECREF(s_seq);
        PyErr_SetString(PyExc_ValueError, "samples sequence cannot be empty");
        return NULL;
    }

    double *s_arr = (double *)malloc((size_t)n * sizeof(double));
    if (!s_arr) { Py_DECREF(s_seq); return PyErr_NoMemory(); }

    for (Py_ssize_t i = 0; i < n; i++) {
        s_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(s_seq, i));
    }
    Py_DECREF(s_seq);

    uint32_t nbins = 0;
    double min_v = 0.0, max_v = 0.0;
    histo_status_t st = histo_estimate_bins((size_t)n, s_arr, (histo_bin_rule_t)rule, &nbins, &min_v, &max_v);
    free(s_arr);

    if (st != HISTO_OK) {
        set_histo_error(st, "Auto bin estimation failed");
        return NULL;
    }
    return Py_BuildValue("(Idd)", nbins, min_v, max_v);
}

static PyObject *py_create_auto(PyObject *self, PyObject *args, PyObject *kwargs) {
    (void)self;
    static char *kwlist[] = {"samples", "rule", "flags", NULL};
    PyObject *s_obj = NULL;
    int rule = 0;
    unsigned int flags = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|iI", kwlist, &s_obj, &rule, &flags)) return NULL;

    PyObject *s_seq = PySequence_Fast(s_obj, "samples must be a sequence or array");
    if (!s_seq) return NULL;
    Py_ssize_t n = PySequence_Fast_GET_SIZE(s_seq);
    if (n == 0) {
        Py_DECREF(s_seq);
        PyErr_SetString(PyExc_ValueError, "samples sequence cannot be empty");
        return NULL;
    }

    double *s_arr = (double *)malloc((size_t)n * sizeof(double));
    if (!s_arr) { Py_DECREF(s_seq); return PyErr_NoMemory(); }

    for (Py_ssize_t i = 0; i < n; i++) {
        s_arr[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(s_seq, i));
    }
    Py_DECREF(s_seq);

    histo_t *h = histo_create_auto((size_t)n, s_arr, (histo_bin_rule_t)rule, flags);
    free(s_arr);

    if (!h) {
        PyErr_SetString(HistoError, "Failed to create automatic histogram");
        return NULL;
    }
    return Histo1D_new_from_ptr(h);
}

static PyObject *py_cli_run(PyObject *self, PyObject *args) {
    (void)self;
    PyObject *seq = PySequence_Fast(args, "cli_run arguments");
    if (!seq) return NULL;
    Py_ssize_t len = PySequence_Fast_GET_SIZE(seq);

    int argc = (int)len + 1;
    char **argv = (char **)malloc((size_t)(argc + 1) * sizeof(char *));
    if (!argv) { Py_DECREF(seq); return PyErr_NoMemory(); }
    argv[0] = "histo";

    for (Py_ssize_t i = 0; i < len; i++) {
        PyObject *item = PySequence_Fast_GET_ITEM(seq, i);
        if (!PyUnicode_Check(item)) {
            free(argv); Py_DECREF(seq);
            PyErr_SetString(PyExc_TypeError, "All CLI arguments must be strings");
            return NULL;
        }
        argv[i + 1] = (char *)PyUnicode_AsUTF8(item);
    }
    argv[argc] = NULL;

    FILE *out_fp = tmpfile();
    FILE *err_fp = tmpfile();
    if (!out_fp || !err_fp) {
        if (out_fp) fclose(out_fp);
        if (err_fp) fclose(err_fp);
        free(argv); Py_DECREF(seq);
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    int ret = histo_cli_main(argc, argv, out_fp, err_fp);
    free(argv);
    Py_DECREF(seq);

    rewind(out_fp);
    fseek(out_fp, 0, SEEK_END);
    long out_len = ftell(out_fp);
    rewind(out_fp);
    char *out_buf = (char *)malloc((size_t)out_len + 1);
    if (out_len > 0 && out_buf) {
        size_t nread = fread(out_buf, 1, (size_t)out_len, out_fp);
        out_buf[nread] = '\0';
    } else if (out_buf) {
        out_buf[0] = '\0';
    }
    fclose(out_fp);

    rewind(err_fp);
    fseek(err_fp, 0, SEEK_END);
    long err_len = ftell(err_fp);
    rewind(err_fp);
    char *err_buf = (char *)malloc((size_t)err_len + 1);
    if (err_len > 0 && err_buf) {
        size_t nread = fread(err_buf, 1, (size_t)err_len, err_fp);
        err_buf[nread] = '\0';
    } else if (err_buf) {
        err_buf[0] = '\0';
    }
    fclose(err_fp);

    PyObject *out_str = PyUnicode_FromString(out_buf ? out_buf : "");
    PyObject *err_str = PyUnicode_FromString(err_buf ? err_buf : "");
    free(out_buf); free(err_buf);

    return Py_BuildValue("(iOO)", ret, out_str, err_str);
}

static PyObject *py_fit_eval(PyObject *self, PyObject *args, PyObject *kwargs) {
    (void)self;
    int model = 0;
    PyObject *params_obj = NULL;
    double x = 0.0;
    static char *kwlist[] = {"model", "params", "x", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "iOd", kwlist, &model, &params_obj, &x)) {
        return NULL;
    }
    Py_buffer view;
    double stack_buf[16];
    double *buf = stack_buf;
    size_t n = 0;
    if (PyObject_GetBuffer(params_obj, &view, PyBUF_SIMPLE) == 0) {
        n = (size_t)(view.len / sizeof(double));
        buf = (double *)view.buf;
    } else {
        PyErr_Clear();
        if (PySequence_Check(params_obj)) {
            Py_ssize_t seq_len = PySequence_Size(params_obj);
            if (seq_len < 0) return NULL;
            n = (size_t)seq_len;
            if (n > 16) {
                buf = (double *)malloc(n * sizeof(double));
                if (!buf) return PyErr_NoMemory();
            }
            for (size_t i = 0; i < n; ++i) {
                PyObject *item = PySequence_GetItem(params_obj, (Py_ssize_t)i);
                if (!item) {
                    if (buf != stack_buf) free(buf);
                    return NULL;
                }
                buf[i] = PyFloat_AsDouble(item);
                Py_DECREF(item);
                if (PyErr_Occurred()) {
                    if (buf != stack_buf) free(buf);
                    return NULL;
                }
            }
        } else {
            PyErr_SetString(PyExc_TypeError, "params must be a sequence of floats or buffer of doubles");
            return NULL;
        }
    }
    double result = histo_fit_eval((histo_fit_model_t)model, buf, n, x);
    if (buf == (double *)view.buf) {
        PyBuffer_Release(&view);
    } else if (buf != stack_buf) {
        free(buf);
    }
    return PyFloat_FromDouble(result);
}

static PyObject *py_fit_eval_gradient(PyObject *self, PyObject *args, PyObject *kwargs) {
    (void)self;
    int model = 0;
    PyObject *params_obj = NULL;
    double x = 0.0;
    static char *kwlist[] = {"model", "params", "x", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "iOd", kwlist, &model, &params_obj, &x)) {
        return NULL;
    }
    Py_buffer view;
    double stack_buf[16];
    double *buf = stack_buf;
    size_t n = 0;
    if (PyObject_GetBuffer(params_obj, &view, PyBUF_SIMPLE) == 0) {
        n = (size_t)(view.len / sizeof(double));
        buf = (double *)view.buf;
    } else {
        PyErr_Clear();
        if (PySequence_Check(params_obj)) {
            Py_ssize_t seq_len = PySequence_Size(params_obj);
            if (seq_len < 0) return NULL;
            n = (size_t)seq_len;
            if (n > 16) {
                buf = (double *)malloc(n * sizeof(double));
                if (!buf) return PyErr_NoMemory();
            }
            for (size_t i = 0; i < n; ++i) {
                PyObject *item = PySequence_GetItem(params_obj, (Py_ssize_t)i);
                if (!item) {
                    if (buf != stack_buf) free(buf);
                    return NULL;
                }
                buf[i] = PyFloat_AsDouble(item);
                Py_DECREF(item);
                if (PyErr_Occurred()) {
                    if (buf != stack_buf) free(buf);
                    return NULL;
                }
            }
        } else {
            PyErr_SetString(PyExc_TypeError, "params must be a sequence of floats or buffer of doubles");
            return NULL;
        }
    }
    double *grad = (double *)malloc(n * sizeof(double));
    if (!grad) {
        if (buf == (double *)view.buf) PyBuffer_Release(&view);
        else if (buf != stack_buf) free(buf);
        return PyErr_NoMemory();
    }
    histo_status_t st = histo_fit_eval_gradient((histo_fit_model_t)model, buf, n, x, grad);
    if (buf == (double *)view.buf) {
        PyBuffer_Release(&view);
    } else if (buf != stack_buf) {
        free(buf);
    }
    if (st != HISTO_OK) {
        free(grad);
        PyErr_SetString(HistoError, "Failed to evaluate gradient");
        return NULL;
    }
    PyObject *res_list = PyList_New((Py_ssize_t)n);
    if (!res_list) {
        free(grad);
        return NULL;
    }
    for (size_t i = 0; i < n; ++i) {
        PyList_SET_ITEM(res_list, (Py_ssize_t)i, PyFloat_FromDouble(grad[i]));
    }
    free(grad);
    return res_list;
}

/* ------------------------------------------------------------------------- */
/* Module initialization                                                     */
/* ------------------------------------------------------------------------- */
static PyMethodDef LibhistoModuleMethods[] = {
    {"estimate_bins", (PyCFunction)(void(*)(void))py_estimate_bins, METH_VARARGS | METH_KEYWORDS, "Estimate optimal uniform bins (nbins, min, max) for samples"},
    {"create_auto", (PyCFunction)(void(*)(void))py_create_auto, METH_VARARGS | METH_KEYWORDS, "Create automatically sized and filled 1D histogram"},
    {"cli_run", (PyCFunction)(void(*)(void))py_cli_run, METH_VARARGS, "Run libhistocli command in-process returning (exit_code, stdout, stderr)"},
    {"fit_eval", (PyCFunction)(void(*)(void))py_fit_eval, METH_VARARGS | METH_KEYWORDS, "Evaluate built-in fit model at x"},
    {"fit_eval_gradient", (PyCFunction)(void(*)(void))py_fit_eval_gradient, METH_VARARGS | METH_KEYWORDS, "Evaluate built-in fit model gradient df/dp at x"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef libhistomodule = {
    PyModuleDef_HEAD_INIT,
    "_libhisto",
    "Low-level C extension wrapping libhisto, libhistocli, and KDE",
    -1,
    LibhistoModuleMethods,
    NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC PyInit__libhisto(void) {
    if (PyType_Ready(&Histo1DType) < 0) return NULL;
    if (PyType_Ready(&Histo2DType) < 0) return NULL;
    if (PyType_Ready(&SketchType) < 0) return NULL;
    if (PyType_Ready(&FitResultType) < 0) return NULL;
    if (PyType_Ready(&KDEType) < 0) return NULL;

    PyObject *m = PyModule_Create(&libhistomodule);
    if (!m) return NULL;

    HistoError = PyErr_NewException("_libhisto.HistoError", NULL, NULL);
    Py_XINCREF(HistoError);
    PyModule_AddObject(m, "HistoError", HistoError);

    Py_INCREF(&Histo1DType);
    PyModule_AddObject(m, "Histo1D", (PyObject *)&Histo1DType);

    Py_INCREF(&Histo2DType);
    PyModule_AddObject(m, "Histo2D", (PyObject *)&Histo2DType);

    Py_INCREF(&SketchType);
    PyModule_AddObject(m, "Sketch", (PyObject *)&SketchType);

    Py_INCREF(&FitResultType);
    PyModule_AddObject(m, "FitResult", (PyObject *)&FitResultType);

    Py_INCREF(&KDEType);
    PyModule_AddObject(m, "KDE", (PyObject *)&KDEType);

    /* Version constants */
    PyModule_AddStringConstant(m, "VERSION_STRING", HISTO_VERSION_STRING);
    PyModule_AddIntConstant(m, "VERSION_MAJOR", HISTO_VERSION_MAJOR);
    PyModule_AddIntConstant(m, "VERSION_MINOR", HISTO_VERSION_MINOR);
    PyModule_AddIntConstant(m, "VERSION_PATCH", HISTO_VERSION_PATCH);

    /* Feature Flags */
    PyModule_AddIntConstant(m, "FLAG_NONE", HISTO_FLAG_NONE);
    PyModule_AddIntConstant(m, "FLAG_TRACK_SUMW2", HISTO_FLAG_TRACK_SUMW2);
    PyModule_AddIntConstant(m, "FLAG_EXACT_MOMENTS", HISTO_FLAG_EXACT_MOMENTS);

    /* Fit Loss */
    PyModule_AddIntConstant(m, "FIT_LOSS_CHI2", HISTO_FIT_LOSS_CHI2);
    PyModule_AddIntConstant(m, "FIT_LOSS_POISSON_MLE", HISTO_FIT_LOSS_POISSON_MLE);

    /* Fit Models */
    PyModule_AddIntConstant(m, "FIT_GAUSSIAN", HISTO_FIT_MODEL_GAUSSIAN);
    PyModule_AddIntConstant(m, "FIT_EXPONENTIAL", HISTO_FIT_MODEL_EXPONENTIAL);
    PyModule_AddIntConstant(m, "FIT_POLYNOMIAL", HISTO_FIT_MODEL_POLYNOMIAL);
    PyModule_AddIntConstant(m, "FIT_BREIT_WIGNER", HISTO_FIT_MODEL_BREIT_WIGNER);
    PyModule_AddIntConstant(m, "FIT_POWER_LAW", HISTO_FIT_MODEL_POWER_LAW);
    PyModule_AddIntConstant(m, "FIT_LOG_NORMAL", HISTO_FIT_MODEL_LOG_NORMAL);
    PyModule_AddIntConstant(m, "FIT_GAUSSIAN_PLUS_LINEAR", HISTO_FIT_MODEL_GAUSSIAN_PLUS_LINEAR);
    PyModule_AddIntConstant(m, "FIT_WEIBULL", HISTO_FIT_MODEL_WEIBULL);
    PyModule_AddIntConstant(m, "FIT_GAMMA", HISTO_FIT_MODEL_GAMMA);
    PyModule_AddIntConstant(m, "FIT_POISSON", HISTO_FIT_MODEL_POISSON);
    PyModule_AddIntConstant(m, "FIT_LAPLACE", HISTO_FIT_MODEL_LAPLACE);

    /* Auto Binning Rules */
    PyModule_AddIntConstant(m, "BIN_RULE_AUTO", HISTO_BIN_RULE_AUTO);
    PyModule_AddIntConstant(m, "BIN_RULE_FD", HISTO_BIN_RULE_FD);
    PyModule_AddIntConstant(m, "BIN_RULE_SCOTT", HISTO_BIN_RULE_SCOTT);
    PyModule_AddIntConstant(m, "BIN_RULE_STURGES", HISTO_BIN_RULE_STURGES);
    PyModule_AddIntConstant(m, "BIN_RULE_DOANE", HISTO_BIN_RULE_DOANE);
    PyModule_AddIntConstant(m, "BIN_RULE_KNUTH", HISTO_BIN_RULE_KNUTH);

    /* KDE Kernels */
    PyModule_AddIntConstant(m, "KDE_KERNEL_GAUSSIAN", HISTO_KDE_KERNEL_GAUSSIAN);
    PyModule_AddIntConstant(m, "KDE_KERNEL_EPANECHNIKOV", HISTO_KDE_KERNEL_EPANECHNIKOV);
    PyModule_AddIntConstant(m, "KDE_KERNEL_UNIFORM", HISTO_KDE_KERNEL_UNIFORM);
    PyModule_AddIntConstant(m, "KDE_KERNEL_TRIANGULAR", HISTO_KDE_KERNEL_TRIANGULAR);
    PyModule_AddIntConstant(m, "KDE_KERNEL_BIWEIGHT", HISTO_KDE_KERNEL_BIWEIGHT);
    PyModule_AddIntConstant(m, "KDE_KERNEL_COSINE", HISTO_KDE_KERNEL_COSINE);

    /* KDE Bandwidth Methods */
    PyModule_AddIntConstant(m, "KDE_BW_SILVERMAN", HISTO_KDE_BANDWIDTH_SILVERMAN);
    PyModule_AddIntConstant(m, "KDE_BW_SCOTT", HISTO_KDE_BANDWIDTH_SCOTT);
    PyModule_AddIntConstant(m, "KDE_BW_MANUAL", HISTO_KDE_BANDWIDTH_MANUAL);

    /* 2D Guard Regions */
    PyModule_AddIntConstant(m, "REGION_CENTER", HISTO2D_REGION_CENTER);
    PyModule_AddIntConstant(m, "REGION_EAST", HISTO2D_REGION_EAST);
    PyModule_AddIntConstant(m, "REGION_NORTH", HISTO2D_REGION_NORTH);
    PyModule_AddIntConstant(m, "REGION_SOUTH", HISTO2D_REGION_SOUTH);
    PyModule_AddIntConstant(m, "REGION_WEST", HISTO2D_REGION_WEST);
    PyModule_AddIntConstant(m, "REGION_SOUTH_WEST", HISTO2D_REGION_SOUTH_WEST);
    PyModule_AddIntConstant(m, "REGION_SOUTH_EAST", HISTO2D_REGION_SOUTH_EAST);
    PyModule_AddIntConstant(m, "REGION_NORTH_WEST", HISTO2D_REGION_NORTH_WEST);
    PyModule_AddIntConstant(m, "REGION_NORTH_EAST", HISTO2D_REGION_NORTH_EAST);

    return m;
}


