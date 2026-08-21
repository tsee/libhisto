/*
 * Unit tests for SIMD vector vs. scalar parity across alignments and buffers.
 */

#include "unity.h"
#include "histo/histo.h"
#include "../../src/simd.h"
#include "../../src/internal.h"
#include "histo/histo2d.h"
#include "../../src/internal_2d.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void assert_histograms_equal(const histo_t *h1, const histo_t *h2) {
    TEST_ASSERT_EQUAL_UINT32(h1->nbins, h2->nbins);
    TEST_ASSERT_EQUAL_UINT64(h1->n_fills, h2->n_fills);
    TEST_ASSERT_EQUAL_UINT64(h1->n_underflow, h2->n_underflow);
    TEST_ASSERT_EQUAL_UINT64(h1->n_overflow, h2->n_overflow);
    TEST_ASSERT_EQUAL_UINT64(h1->n_nan, h2->n_nan);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->total_weight, h2->total_weight);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->underflow_weight, h2->underflow_weight);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->overflow_weight, h2->overflow_weight);

    for (uint32_t i = 0; i < h1->nbins; ++i) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->bins[i], h2->bins[i]);
    }

    if (h1->sum_w2 && h2->sum_w2) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->total_sum_w2, h2->total_sum_w2);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->underflow_sum_w2, h2->underflow_sum_w2);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->overflow_sum_w2, h2->overflow_sum_w2);
        for (uint32_t i = 0; i < h1->nbins; ++i) {
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->sum_w2[i], h2->sum_w2[i]);
        }
    }
}

static void assert_histograms_2d_equal(const histo2d_t *h1, const histo2d_t *h2) {
    TEST_ASSERT_EQUAL_UINT32(h1->x_axis.nbins, h2->x_axis.nbins);
    TEST_ASSERT_EQUAL_UINT32(h1->y_axis.nbins, h2->y_axis.nbins);
    TEST_ASSERT_EQUAL_UINT64(h1->n_fills, h2->n_fills);
    TEST_ASSERT_EQUAL_UINT64(h1->n_nan, h2->n_nan);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->total_weight, h2->total_weight);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->total_sum_w2, h2->total_sum_w2);

    uint32_t nx = h1->x_axis.nbins;
    uint32_t ny = h1->y_axis.nbins;
    for (uint32_t i = 0; i < nx * ny; ++i) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->bins[i], h2->bins[i]);
        if (h1->sum_w2 && h2->sum_w2) {
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->sum_w2[i], h2->sum_w2[i]);
        }
    }

    for (int r = 0; r < 9; ++r) {
        TEST_ASSERT_EQUAL_UINT64(h1->guards[r].count, h2->guards[r].count);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->guards[r].weight, h2->guards[r].weight);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, h1->guards[r].sum_w2, h2->guards[r].sum_w2);
    }
}

/* ========================================================================= */
/* NEON Tests                                                                */
/* ========================================================================= */

void test_neon_uniform_in_range(void) {
#ifdef LIBHISTO_ENABLE_NEON
    const size_t sizes[] = {1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 31, 32, 63, 64, 127, 128, 513};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t s = 0; s < num_sizes; ++s) {
        size_t n = sizes[s];
        double *data = malloc(n * sizeof(double));
        TEST_ASSERT_NOT_NULL(data);

        for (size_t i = 0; i < n; ++i) {
            data[i] = 10.0 + (double)(i % 80) + ((double)i * 0.013);
        }

        histo_t *h_scalar = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);
        histo_t *h_neon = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);

        for (size_t i = 0; i < n; ++i) {
            histo_fill(h_scalar, data[i]);
        }

        bool non_finite = histo_fill_uniform_neon(h_neon, data, n);
        TEST_ASSERT_FALSE(non_finite);

        assert_histograms_equal(h_scalar, h_neon);

        histo_destroy(h_scalar);
        histo_destroy(h_neon);
        free(data);
    }
#else
    TEST_IGNORE_MESSAGE("NEON not enabled");
#endif
}

void test_neon_exact_edges(void) {
#ifdef LIBHISTO_ENABLE_NEON
    double edges[] = {
        0.0, 0.0 + 1e-15,
        9.99999999999999, 10.0, 10.00000000000001,
        19.9999999999999, 20.0, 20.00000000000001,
        29.9999999999999, 30.0, 30.00000000000001,
        49.9999999999999, 50.0, 50.00000000000001,
        89.9999999999999, 90.0, 90.00000000000001,
        99.9999999999999, 100.0, 100.0 + 1e-15,
        -0.0, +0.0
    };
    size_t n = sizeof(edges) / sizeof(edges[0]);

    histo_t *h_scalar = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);
    histo_t *h_neon = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);

    for (size_t i = 0; i < n; ++i) {
        histo_fill(h_scalar, edges[i]);
    }

    bool non_finite = histo_fill_uniform_neon(h_neon, edges, n);
    TEST_ASSERT_FALSE(non_finite);

    assert_histograms_equal(h_scalar, h_neon);

    histo_destroy(h_scalar);
    histo_destroy(h_neon);
#else
    TEST_IGNORE_MESSAGE("NEON not enabled");
#endif
}

void test_neon_out_of_range_and_specials(void) {
#ifdef LIBHISTO_ENABLE_NEON
    double specials[] = {
        -INFINITY, -1e308, -500.0, -0.000001,
        10.0, 25.0, 50.0, 75.0,
        100.0, 100.00001, 500.0, 1e308, INFINITY,
        NAN, -NAN,
        DBL_MIN, DBL_MIN / 2.0, 1e-310, -1e-310
    };
    size_t n = sizeof(specials) / sizeof(specials[0]);

    histo_t *h_scalar = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);
    histo_t *h_neon = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);

    for (size_t i = 0; i < n; ++i) {
        histo_fill(h_scalar, specials[i]);
    }

    bool non_finite = histo_fill_uniform_neon(h_neon, specials, n);
    TEST_ASSERT_TRUE(non_finite);

    assert_histograms_equal(h_scalar, h_neon);

    histo_destroy(h_scalar);
    histo_destroy(h_neon);
#else
    TEST_IGNORE_MESSAGE("NEON not enabled");
#endif
}

void test_neon_weighted_w2(void) {
#ifdef LIBHISTO_ENABLE_NEON
    const size_t sizes[] = {1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 31, 32, 63, 64, 127, 128, 513};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t s = 0; s < num_sizes; ++s) {
        size_t n = sizes[s];
        double *data = malloc(n * sizeof(double));
        double *weights = malloc(n * sizeof(double));
        TEST_ASSERT_NOT_NULL(data);
        TEST_ASSERT_NOT_NULL(weights);

        for (size_t i = 0; i < n; ++i) {
            data[i] = (double)i * 1.5 - 5.0;
            weights[i] = (double)(i % 5) * 0.5 + 0.25;
        }
        if (n > 30) {
            data[10] = NAN; weights[10] = 1.0;
            data[20] = INFINITY; weights[20] = 2.0;
            data[30] = 45.0; weights[30] = NAN;
        }

        histo_t *h_scalar = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
        histo_t *h_neon = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

        for (size_t i = 0; i < n; ++i) {
            histo_fill_w(h_scalar, data[i], weights[i]);
        }

        bool non_finite = histo_fill_uniform_w2_neon(h_neon, data, weights, n);
        if (n > 30) {
            TEST_ASSERT_TRUE(non_finite);
        }

        assert_histograms_equal(h_scalar, h_neon);

        histo_destroy(h_scalar);
        histo_destroy(h_neon);
        free(data);
        free(weights);
    }
#else
    TEST_IGNORE_MESSAGE("NEON not enabled");
#endif
}

void test_neon_2d_uniform_in_range(void) {
#ifdef LIBHISTO_ENABLE_NEON
    const size_t n = 64;
    double x[64], y[64];
    for (size_t i = 0; i < n; ++i) {
        x[i] = 2.0 + (double)i * 1.5;
        y[i] = 1.0 + (double)i * 1.2;
    }

    histo2d_t *h_scalar = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, 0);
    histo2d_t *h_neon = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, 0);

    for (size_t i = 0; i < n; ++i) {
        histo2d_fill(h_scalar, x[i], y[i]);
    }

    bool non_finite = histo2d_fill_uniform_neon(h_neon, x, y, n);
    TEST_ASSERT_FALSE(non_finite);

    assert_histograms_2d_equal(h_scalar, h_neon);

    histo2d_destroy(h_scalar);
    histo2d_destroy(h_neon);
#else
    TEST_IGNORE_MESSAGE("NEON not enabled");
#endif
}

void test_neon_2d_weighted_w2(void) {
#ifdef LIBHISTO_ENABLE_NEON
    const size_t sizes[] = {1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 31, 32, 63, 64, 127, 128, 513};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t s = 0; s < num_sizes; ++s) {
        size_t n = sizes[s];
        double *x = malloc(n * sizeof(double));
        double *y = malloc(n * sizeof(double));
        double *weights = malloc(n * sizeof(double));
        TEST_ASSERT_NOT_NULL(x);
        TEST_ASSERT_NOT_NULL(y);
        TEST_ASSERT_NOT_NULL(weights);

        for (size_t i = 0; i < n; ++i) {
            x[i] = (double)(i % 90) + 5.0;
            y[i] = (double)(i % 80) + 10.0;
            weights[i] = 1.0 + (double)(i % 4) * 0.5;
        }
        if (n > 30) {
            x[10] = -5.0; y[10] = 50.0; weights[10] = 2.0;
            x[20] = 105.0; y[20] = 50.0; weights[20] = 3.0;
            x[30] = NAN; y[30] = 50.0; weights[30] = 1.0;
        }

        histo2d_t *h_scalar = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
        histo2d_t *h_neon = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

        for (size_t i = 0; i < n; ++i) {
            histo2d_fill_w(h_scalar, x[i], y[i], weights[i]);
        }

        bool non_finite = histo2d_fill_uniform_w2_neon(h_neon, x, y, weights, n);
        if (n > 30) {
            TEST_ASSERT_TRUE(non_finite);
        }

        assert_histograms_2d_equal(h_scalar, h_neon);

        histo2d_destroy(h_scalar);
        histo2d_destroy(h_neon);
        free(x);
        free(y);
        free(weights);
    }
#else
    TEST_IGNORE_MESSAGE("NEON not enabled");
#endif
}

void test_neon_2d_all_regions_and_specials(void) {
#ifdef LIBHISTO_ENABLE_NEON
    /* Test samples hitting all 9 regions in 2D plus IEEE specials */
    double x_vals[] = {
        50.0,   /* center */
        150.0,  /* east */
        50.0,   /* north */
        50.0,   /* south */
        -10.0,  /* west */
        -10.0,  /* south-west */
        150.0,  /* south-east */
        -10.0,  /* north-west */
        150.0,  /* north-east */
        NAN,    /* NaN x */
        50.0,   /* NaN y */
        INFINITY, /* Inf x */
        -INFINITY /* -Inf y */
    };
    double y_vals[] = {
        50.0,   /* center */
        50.0,   /* east */
        150.0,  /* north */
        -10.0,  /* south */
        50.0,   /* west */
        -10.0,  /* south-west */
        -10.0,  /* south-east */
        150.0,  /* north-west */
        150.0,  /* north-east */
        50.0,   /* NaN x */
        NAN,    /* NaN y */
        50.0,   /* Inf x */
        -INFINITY /* -Inf y */
    };
    double weights[] = {
        1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 1.0, 1.0, 1.0, 1.0
    };
    size_t n = sizeof(x_vals) / sizeof(x_vals[0]);

    histo2d_t *h_scalar = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo2d_t *h_neon = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

    for (size_t i = 0; i < n; ++i) {
        histo2d_fill_w(h_scalar, x_vals[i], y_vals[i], weights[i]);
    }

    bool non_finite = histo2d_fill_uniform_w2_neon(h_neon, x_vals, y_vals, weights, n);
    TEST_ASSERT_TRUE(non_finite);

    assert_histograms_2d_equal(h_scalar, h_neon);

    histo2d_destroy(h_scalar);
    histo2d_destroy(h_neon);
#else
    TEST_IGNORE_MESSAGE("NEON not enabled");
#endif
}

/* ========================================================================= */
/* AVX2 Tests                                                                */
/* ========================================================================= */

void test_avx2_uniform_in_range(void) {
#ifdef LIBHISTO_ENABLE_AVX2
    if (!histo_simd_has_avx2()) {
        TEST_IGNORE_MESSAGE("AVX2 CPU instructions not supported on host");
    }
    const size_t sizes[] = {1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 31, 32, 63, 64, 127, 128, 513};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t s = 0; s < num_sizes; ++s) {
        size_t n = sizes[s];
        double *data = malloc(n * sizeof(double));
        TEST_ASSERT_NOT_NULL(data);

        for (size_t i = 0; i < n; ++i) {
            data[i] = 10.0 + (double)(i % 80) + ((double)i * 0.013);
        }

        histo_t *h_scalar = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);
        histo_t *h_avx2 = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);

        for (size_t i = 0; i < n; ++i) {
            histo_fill(h_scalar, data[i]);
        }

        bool non_finite = histo_fill_uniform_avx2(h_avx2, data, n);
        TEST_ASSERT_FALSE(non_finite);

        assert_histograms_equal(h_scalar, h_avx2);

        histo_destroy(h_scalar);
        histo_destroy(h_avx2);
        free(data);
    }
#else
    TEST_IGNORE_MESSAGE("AVX2 not enabled");
#endif
}

void test_avx2_weighted_w2(void) {
#ifdef LIBHISTO_ENABLE_AVX2
    if (!histo_simd_has_avx2()) {
        TEST_IGNORE_MESSAGE("AVX2 CPU instructions not supported on host");
    }
    const size_t sizes[] = {1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 31, 32, 63, 64, 127, 128, 513};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t s = 0; s < num_sizes; ++s) {
        size_t n = sizes[s];
        double *data = malloc(n * sizeof(double));
        double *weights = malloc(n * sizeof(double));
        TEST_ASSERT_NOT_NULL(data);
        TEST_ASSERT_NOT_NULL(weights);

        for (size_t i = 0; i < n; ++i) {
            data[i] = (double)i * 1.5 - 5.0;
            weights[i] = (double)(i % 5) * 0.5 + 0.25;
        }
        if (n > 30) {
            data[10] = NAN; weights[10] = 1.0;
            data[20] = INFINITY; weights[20] = 2.0;
            data[30] = 45.0; weights[30] = NAN;
        }

        histo_t *h_scalar = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
        histo_t *h_avx2 = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

        for (size_t i = 0; i < n; ++i) {
            histo_fill_w(h_scalar, data[i], weights[i]);
        }

        bool non_finite = histo_fill_uniform_w2_avx2(h_avx2, data, weights, n);
        if (n > 30) {
            TEST_ASSERT_TRUE(non_finite);
        }

        assert_histograms_equal(h_scalar, h_avx2);

        histo_destroy(h_scalar);
        histo_destroy(h_avx2);
        free(data);
        free(weights);
    }
#else
    TEST_IGNORE_MESSAGE("AVX2 not enabled");
#endif
}

void test_avx2_2d_weighted_w2(void) {
#ifdef LIBHISTO_ENABLE_AVX2
    if (!histo_simd_has_avx2()) {
        TEST_IGNORE_MESSAGE("AVX2 CPU instructions not supported on host");
    }
    const size_t sizes[] = {1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 31, 32, 63, 64, 127, 128, 513};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t s = 0; s < num_sizes; ++s) {
        size_t n = sizes[s];
        double *x = malloc(n * sizeof(double));
        double *y = malloc(n * sizeof(double));
        double *weights = malloc(n * sizeof(double));
        TEST_ASSERT_NOT_NULL(x);
        TEST_ASSERT_NOT_NULL(y);
        TEST_ASSERT_NOT_NULL(weights);

        for (size_t i = 0; i < n; ++i) {
            x[i] = (double)(i % 90) + 5.0;
            y[i] = (double)(i % 80) + 10.0;
            weights[i] = 1.0 + (double)(i % 4) * 0.5;
        }
        if (n > 30) {
            x[10] = -5.0; y[10] = 50.0; weights[10] = 2.0;
            x[20] = 105.0; y[20] = 50.0; weights[20] = 3.0;
            x[30] = NAN; y[30] = 50.0; weights[30] = 1.0;
        }

        histo2d_t *h_scalar = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
        histo2d_t *h_avx2 = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

        for (size_t i = 0; i < n; ++i) {
            histo2d_fill_w(h_scalar, x[i], y[i], weights[i]);
        }

        bool non_finite = histo2d_fill_uniform_w2_avx2(h_avx2, x, y, weights, n);
        if (n > 30) {
            TEST_ASSERT_TRUE(non_finite);
        }

        assert_histograms_2d_equal(h_scalar, h_avx2);

        histo2d_destroy(h_scalar);
        histo2d_destroy(h_avx2);
        free(x);
        free(y);
        free(weights);
    }
#else
    TEST_IGNORE_MESSAGE("AVX2 not enabled");
#endif
}

void test_avx2_2d_all_regions_and_specials(void) {
#ifdef LIBHISTO_ENABLE_AVX2
    if (!histo_simd_has_avx2()) {
        TEST_IGNORE_MESSAGE("AVX2 CPU instructions not supported on host");
    }
    double x_vals[] = {
        50.0, 150.0, 50.0, 50.0, -10.0, -10.0, 150.0, -10.0, 150.0, NAN, 50.0, INFINITY, -INFINITY,
        DBL_MIN, 1e308, -1e308, -0.0, +0.0
    };
    double y_vals[] = {
        50.0, 50.0, 150.0, -10.0, 50.0, -10.0, -10.0, 150.0, 150.0, 50.0, NAN, 50.0, -INFINITY,
        DBL_MIN, 1e308, -1e308, +0.0, -0.0
    };
    double weights[] = {
        1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 1.0, 1.0, 1.0, 1.0,
        2.2, 0.5, 0.75, 1.25, 3.33
    };
    size_t n = sizeof(x_vals) / sizeof(x_vals[0]);

    histo2d_t *h_scalar = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo2d_t *h_avx2 = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

    for (size_t i = 0; i < n; ++i) {
        histo2d_fill_w(h_scalar, x_vals[i], y_vals[i], weights[i]);
    }

    bool non_finite = histo2d_fill_uniform_w2_avx2(h_avx2, x_vals, y_vals, weights, n);
    TEST_ASSERT_TRUE(non_finite);

    assert_histograms_2d_equal(h_scalar, h_avx2);

    histo2d_destroy(h_scalar);
    histo2d_destroy(h_avx2);
#else
    TEST_IGNORE_MESSAGE("AVX2 not enabled");
#endif
}

/* ========================================================================= */
/* AVX-512 Tests                                                             */
/* ========================================================================= */

void test_avx512_uniform_in_range(void) {
#ifdef LIBHISTO_ENABLE_AVX512
    if (!histo_simd_has_avx512()) {
        TEST_IGNORE_MESSAGE("AVX-512 CPU instructions not supported on host");
    }
    const size_t n = 64;
    double data[64];
    for (size_t i = 0; i < n; ++i) data[i] = 10.0 + (double)i;

    histo_t *h_scalar = histo_create_uniform(10, 0.0, 100.0, 0);
    histo_t *h_avx512 = histo_create_uniform(10, 0.0, 100.0, 0);
    for (size_t i = 0; i < n; ++i) histo_fill(h_scalar, data[i]);

    bool non_finite = histo_fill_uniform_avx512(h_avx512, data, n);
    TEST_ASSERT_FALSE(non_finite);
    assert_histograms_equal(h_scalar, h_avx512);

    histo_destroy(h_scalar);
    histo_destroy(h_avx512);
#else
    TEST_IGNORE_MESSAGE("AVX-512 not enabled");
#endif
}

void test_avx512_weighted_w2(void) {
#ifdef LIBHISTO_ENABLE_AVX512
    if (!histo_simd_has_avx512()) {
        TEST_IGNORE_MESSAGE("AVX-512 CPU instructions not supported on host");
    }
    const size_t n = 64;
    double data[64], weights[64];
    for (size_t i = 0; i < n; ++i) {
        data[i] = 10.0 + (double)i;
        weights[i] = 1.5 + (double)(i % 3);
    }
    histo_t *h_scalar = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo_t *h_avx512 = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    for (size_t i = 0; i < n; ++i) histo_fill_w(h_scalar, data[i], weights[i]);

    bool non_finite = histo_fill_uniform_w2_avx512(h_avx512, data, weights, n);
    TEST_ASSERT_FALSE(non_finite);
    assert_histograms_equal(h_scalar, h_avx512);

    histo_destroy(h_scalar);
    histo_destroy(h_avx512);
#else
    TEST_IGNORE_MESSAGE("AVX-512 not enabled");
#endif
}

void test_avx512_2d_weighted_w2(void) {
#ifdef LIBHISTO_ENABLE_AVX512
    if (!histo_simd_has_avx512()) {
        TEST_IGNORE_MESSAGE("AVX-512 CPU instructions not supported on host");
    }
    const size_t n = 64;
    double x[64], y[64], weights[64];
    for (size_t i = 0; i < n; ++i) {
        x[i] = 10.0 + (double)i;
        y[i] = 15.0 + (double)i;
        weights[i] = 1.5 + (double)(i % 3);
    }
    histo2d_t *h_scalar = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo2d_t *h_avx512 = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    for (size_t i = 0; i < n; ++i) histo2d_fill_w(h_scalar, x[i], y[i], weights[i]);

    bool non_finite = histo2d_fill_uniform_w2_avx512(h_avx512, x, y, weights, n);
    TEST_ASSERT_FALSE(non_finite);
    assert_histograms_2d_equal(h_scalar, h_avx512);

    histo2d_destroy(h_scalar);
    histo2d_destroy(h_avx512);
#else
    TEST_IGNORE_MESSAGE("AVX-512 not enabled");
#endif
}

/* ========================================================================= */
/* High-Level Dispatch Tests                                                 */
/* ========================================================================= */

void test_simd_dispatch_equivalence(void) {
    const size_t n = 200;
    double data[200];
    double weights[200];

    for (size_t i = 0; i < n; ++i) {
        data[i] = (double)(i % 120) - 10.0;
        weights[i] = 1.25 + (double)(i % 3);
    }

    histo_t *h_scalar = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo_t *h_batch = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

    for (size_t i = 0; i < n; ++i) {
        histo_fill_w(h_scalar, data[i], weights[i]);
    }

    histo_status_t st = histo_fill_n(h_batch, n, data, weights);
    TEST_ASSERT_EQUAL(HISTO_OK, st);

    assert_histograms_equal(h_scalar, h_batch);

    histo_destroy(h_scalar);
    histo_destroy(h_batch);
}

void test_simd_dispatch_equivalence_2d(void) {
    const size_t n = 200;
    double x[200], y[200], weights[200];

    for (size_t i = 0; i < n; ++i) {
        x[i] = (double)(i % 120) - 10.0;
        y[i] = (double)(i % 110) - 5.0;
        weights[i] = 1.25 + (double)(i % 3);
    }

    histo2d_t *h_scalar = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo2d_t *h_batch = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

    for (size_t i = 0; i < n; ++i) {
        histo2d_fill_w(h_scalar, x[i], y[i], weights[i]);
    }

    histo_status_t st = histo2d_fill_n(h_batch, n, x, y, weights);
    TEST_ASSERT_EQUAL(HISTO_OK, st);

    assert_histograms_2d_equal(h_scalar, h_batch);

    histo2d_destroy(h_scalar);
    histo2d_destroy(h_batch);
}

/* ========================================================================= */
/* Comprehensive Parity: Arbitrary Buffer Sizes & Pointer Alignments         */
/* ========================================================================= */

void test_simd_arbitrary_buffer_sizes_and_alignments(void) {
    const size_t sizes[] = {0, 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 100, 1000, 10007};
    const size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    const size_t offsets[] = {0, 1, 2, 3, 4, 7};
    const size_t num_offsets = sizeof(offsets) / sizeof(offsets[0]);

    const size_t max_n = 10007;
    char *raw_x = (char *)malloc((max_n + 4) * sizeof(double) + 64);
    char *raw_y = (char *)malloc((max_n + 4) * sizeof(double) + 64);
    char *raw_w = (char *)malloc((max_n + 4) * sizeof(double) + 64);
    TEST_ASSERT_NOT_NULL(raw_x);
    TEST_ASSERT_NOT_NULL(raw_y);
    TEST_ASSERT_NOT_NULL(raw_w);

    for (size_t s = 0; s < num_sizes; ++s) {
        size_t n = sizes[s];

        for (size_t off_idx = 0; off_idx < num_offsets; ++off_idx) {
            size_t off = offsets[off_idx];
            double *x = (double *)(void *)(raw_x + off);
            double *y = (double *)(void *)(raw_y + off);
            double *w = (double *)(void *)(raw_w + off);

            for (size_t i = 0; i < n; ++i) {
                x[i] = -10.0 + (double)(i % 120) * 1.05;
                y[i] = -5.0 + (double)(i % 110) * 0.95;
                w[i] = 0.25 + (double)(i % 7) * 0.3;
            }

            /* 1D Unweighted */
            histo_t *h1_scalar = histo_create_uniform(12, 0.0, 100.0, HISTO_FLAG_NONE);
            histo_t *h1_simd   = histo_create_uniform(12, 0.0, 100.0, HISTO_FLAG_NONE);
            for (size_t i = 0; i < n; ++i) {
                histo_fill(h1_scalar, x[i]);
            }
            TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_n(h1_simd, n, x, NULL));
            assert_histograms_equal(h1_scalar, h1_simd);
            histo_destroy(h1_scalar);
            histo_destroy(h1_simd);

            /* 1D Weighted with SumW2 */
            histo_t *h1_w_scalar = histo_create_uniform(12, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
            histo_t *h1_w_simd   = histo_create_uniform(12, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
            for (size_t i = 0; i < n; ++i) {
                histo_fill_w(h1_w_scalar, x[i], w[i]);
            }
            TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_n(h1_w_simd, n, x, w));
            assert_histograms_equal(h1_w_scalar, h1_w_simd);
            histo_destroy(h1_w_scalar);
            histo_destroy(h1_w_simd);

            /* 2D Unweighted */
            histo2d_t *h2_scalar = histo2d_create_uniform(8, 0.0, 100.0, 8, 0.0, 100.0, 0);
            histo2d_t *h2_simd   = histo2d_create_uniform(8, 0.0, 100.0, 8, 0.0, 100.0, 0);
            for (size_t i = 0; i < n; ++i) {
                histo2d_fill(h2_scalar, x[i], y[i]);
            }
            TEST_ASSERT_EQUAL(HISTO_OK, histo2d_fill_n(h2_simd, n, x, y, NULL));
            assert_histograms_2d_equal(h2_scalar, h2_simd);
            histo2d_destroy(h2_scalar);
            histo2d_destroy(h2_simd);

            /* 2D Weighted with SumW2 */
            histo2d_t *h2_w_scalar = histo2d_create_uniform(8, 0.0, 100.0, 8, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
            histo2d_t *h2_w_simd   = histo2d_create_uniform(8, 0.0, 100.0, 8, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
            for (size_t i = 0; i < n; ++i) {
                histo2d_fill_w(h2_w_scalar, x[i], y[i], w[i]);
            }
            TEST_ASSERT_EQUAL(HISTO_OK, histo2d_fill_n(h2_w_simd, n, x, y, w));
            assert_histograms_2d_equal(h2_w_scalar, h2_w_simd);
            histo2d_destroy(h2_w_scalar);
            histo2d_destroy(h2_w_simd);
        }
    }

    free(raw_x);
    free(raw_y);
    free(raw_w);
}

/* ========================================================================= */
/* Boundary Data & Epsilon Transitions Sweep                                 */
/* ========================================================================= */

void test_simd_boundary_data_epsilon_sweep(void) {
    const uint32_t nbins = 10;
    const double min_val = 0.0;
    const double max_val = 100.0;
    const double dx = 10.0;

    double boundary_vals[256];
    size_t count = 0;

    for (uint32_t k = 0; k <= nbins; ++k) {
        double edge = min_val + (double)k * dx;
        boundary_vals[count++] = edge;
        boundary_vals[count++] = edge - 1e-14;
        boundary_vals[count++] = edge + 1e-14;
        boundary_vals[count++] = edge - 1e-10;
        boundary_vals[count++] = edge + 1e-10;
        boundary_vals[count++] = nextafter(edge, -INFINITY);
        boundary_vals[count++] = nextafter(edge, +INFINITY);
    }
    boundary_vals[count++] = -1e-15;
    boundary_vals[count++] = +1e-15;
    boundary_vals[count++] = -0.0;
    boundary_vals[count++] = +0.0;

    histo_t *h1_scalar = histo_create_uniform(nbins, min_val, max_val, HISTO_FLAG_TRACK_SUMW2);
    histo_t *h1_simd   = histo_create_uniform(nbins, min_val, max_val, HISTO_FLAG_TRACK_SUMW2);

    for (size_t i = 0; i < count; ++i) {
        histo_fill(h1_scalar, boundary_vals[i]);
    }
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_n(h1_simd, count, boundary_vals, NULL));
    assert_histograms_equal(h1_scalar, h1_simd);

    histo_destroy(h1_scalar);
    histo_destroy(h1_simd);

    /* 2D boundary check */
    histo2d_t *h2_scalar = histo2d_create_uniform(nbins, min_val, max_val, nbins, min_val, max_val, HISTO_FLAG_TRACK_SUMW2);
    histo2d_t *h2_simd   = histo2d_create_uniform(nbins, min_val, max_val, nbins, min_val, max_val, HISTO_FLAG_TRACK_SUMW2);

    for (size_t i = 0; i < count; ++i) {
        size_t y_idx = (count - 1 - i);
        histo2d_fill(h2_scalar, boundary_vals[i], boundary_vals[y_idx]);
    }
    double *y_rev = (double *)malloc(count * sizeof(double));
    TEST_ASSERT_NOT_NULL(y_rev);
    for (size_t i = 0; i < count; ++i) {
        y_rev[i] = boundary_vals[count - 1 - i];
    }
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_fill_n(h2_simd, count, boundary_vals, y_rev, NULL));
    assert_histograms_2d_equal(h2_scalar, h2_simd);

    histo2d_destroy(h2_scalar);
    histo2d_destroy(h2_simd);
    free(y_rev);
}

/* ========================================================================= */
/* Special Floats Matrix (NaN, Inf, Denormals, Negative Zero)                */
/* ========================================================================= */

void test_simd_special_floats_matrix(void) {
    const double specials[] = {
        NAN, -NAN,
        INFINITY, -INFINITY,
        +0.0, -0.0,
        DBL_MIN, DBL_MIN / 2.0, 1.0e-315,
        DBL_MAX, -DBL_MAX, 1.0e308, -1.0e308,
        50.0, 25.0, -50.0, 150.0,
        nextafter(0.0, 1.0), nextafter(100.0, 0.0),
        nextafter(0.0, -1.0), nextafter(100.0, 200.0)
    };
    const size_t num_specials = sizeof(specials) / sizeof(specials[0]);

    const size_t total_n = 500;
    double *data = (double *)malloc(total_n * sizeof(double));
    double *weights = (double *)malloc(total_n * sizeof(double));
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_NOT_NULL(weights);

    for (size_t i = 0; i < total_n; ++i) {
        if (i % 5 == 0) {
            data[i] = specials[(i / 5) % num_specials];
        } else {
            data[i] = (double)(i % 120) - 10.0;
        }
        weights[i] = (i % 7 == 0) ? specials[(i / 7) % num_specials] : 1.25;
    }

    histo_t *h1_scalar = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo_t *h1_simd   = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

    for (size_t i = 0; i < total_n; ++i) {
        histo_fill_w(h1_scalar, data[i], weights[i]);
    }
    histo_status_t st = histo_fill_n(h1_simd, total_n, data, weights);
    TEST_ASSERT_EQUAL(HISTO_WARN_NON_FINITE, st);

    assert_histograms_equal(h1_scalar, h1_simd);

    histo_destroy(h1_scalar);
    histo_destroy(h1_simd);
    free(data);
    free(weights);
}

/* ========================================================================= */
/* Strided & Packed (x, w) Ingestion Tests                                   */
/* ========================================================================= */

typedef struct {
    double x;
    double weight;
    char   metadata[24];
} test_packed_1d_event_t;

typedef struct {
    double x;
    double y;
    double weight;
    char   metadata[16];
} test_packed_2d_event_t;

void test_simd_strided_and_packed_ingestion(void) {
    const size_t n = 300;
    test_packed_1d_event_t *ev1 = (test_packed_1d_event_t *)malloc(n * sizeof(test_packed_1d_event_t));
    test_packed_2d_event_t *ev2 = (test_packed_2d_event_t *)malloc(n * sizeof(test_packed_2d_event_t));
    TEST_ASSERT_NOT_NULL(ev1);
    TEST_ASSERT_NOT_NULL(ev2);

    for (size_t i = 0; i < n; ++i) {
        ev1[i].x = (double)(i % 120) - 10.0;
        ev1[i].weight = 0.5 + (double)(i % 4) * 0.25;

        ev2[i].x = (double)(i % 120) - 10.0;
        ev2[i].y = (double)(i % 110) - 5.0;
        ev2[i].weight = 1.0 + (double)(i % 3) * 0.5;
    }
    /* Inject non-finite */
    ev1[10].x = NAN;
    ev1[20].weight = INFINITY;
    ev2[15].y = NAN;
    ev2[25].weight = -INFINITY;

    /* 1D Strided vs Scalar */
    histo_t *h1_scalar = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo_t *h1_strided = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

    for (size_t i = 0; i < n; ++i) {
        histo_fill_w(h1_scalar, ev1[i].x, ev1[i].weight);
    }
    histo_status_t st1 = histo_fill_strided(
        h1_strided, n,
        &ev1[0].x, sizeof(test_packed_1d_event_t),
        &ev1[0].weight, sizeof(test_packed_1d_event_t)
    );
    TEST_ASSERT_EQUAL(HISTO_WARN_NON_FINITE, st1);
    assert_histograms_equal(h1_scalar, h1_strided);

    histo_destroy(h1_scalar);
    histo_destroy(h1_strided);

    /* 2D Strided vs Scalar */
    histo2d_t *h2_scalar = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo2d_t *h2_strided = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

    for (size_t i = 0; i < n; ++i) {
        histo2d_fill_w(h2_scalar, ev2[i].x, ev2[i].y, ev2[i].weight);
    }
    histo_status_t st2 = histo2d_fill_strided(
        h2_strided, n,
        &ev2[0].x, sizeof(test_packed_2d_event_t),
        &ev2[0].y, sizeof(test_packed_2d_event_t),
        &ev2[0].weight, sizeof(test_packed_2d_event_t)
    );
    TEST_ASSERT_EQUAL(HISTO_WARN_NON_FINITE, st2);
    assert_histograms_2d_equal(h2_scalar, h2_strided);

    histo2d_destroy(h2_scalar);
    histo2d_destroy(h2_strided);

    free(ev1);
    free(ev2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_neon_uniform_in_range);
    RUN_TEST(test_neon_exact_edges);
    RUN_TEST(test_neon_out_of_range_and_specials);
    RUN_TEST(test_neon_weighted_w2);
    RUN_TEST(test_neon_2d_uniform_in_range);
    RUN_TEST(test_neon_2d_weighted_w2);
    RUN_TEST(test_neon_2d_all_regions_and_specials);

    RUN_TEST(test_avx2_uniform_in_range);
    RUN_TEST(test_avx2_weighted_w2);
    RUN_TEST(test_avx2_2d_weighted_w2);
    RUN_TEST(test_avx2_2d_all_regions_and_specials);

    RUN_TEST(test_avx512_uniform_in_range);
    RUN_TEST(test_avx512_weighted_w2);
    RUN_TEST(test_avx512_2d_weighted_w2);

    RUN_TEST(test_simd_dispatch_equivalence);
    RUN_TEST(test_simd_dispatch_equivalence_2d);

    RUN_TEST(test_simd_arbitrary_buffer_sizes_and_alignments);
    RUN_TEST(test_simd_boundary_data_epsilon_sweep);
    RUN_TEST(test_simd_special_floats_matrix);
    RUN_TEST(test_simd_strided_and_packed_ingestion);

    return UNITY_END();
}
