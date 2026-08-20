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
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->total_weight, h2->total_weight);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->underflow_weight, h2->underflow_weight);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->overflow_weight, h2->overflow_weight);

    for (uint32_t i = 0; i < h1->nbins; ++i) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->bins[i], h2->bins[i]);
    }

    if (h1->sum_w2 && h2->sum_w2) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->total_sum_w2, h2->total_sum_w2);
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->underflow_sum_w2, h2->underflow_sum_w2);
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->overflow_sum_w2, h2->overflow_sum_w2);
        for (uint32_t i = 0; i < h1->nbins; ++i) {
            TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->sum_w2[i], h2->sum_w2[i]);
        }
    }
}

static void assert_histograms_2d_equal(const histo2d_t *h1, const histo2d_t *h2) {
    TEST_ASSERT_EQUAL_UINT32(h1->x_axis.nbins, h2->x_axis.nbins);
    TEST_ASSERT_EQUAL_UINT32(h1->y_axis.nbins, h2->y_axis.nbins);
    TEST_ASSERT_EQUAL_UINT64(h1->n_fills, h2->n_fills);
    TEST_ASSERT_EQUAL_UINT64(h1->n_nan, h2->n_nan);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->total_weight, h2->total_weight);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->total_sum_w2, h2->total_sum_w2);

    uint32_t nx = h1->x_axis.nbins;
    uint32_t ny = h1->y_axis.nbins;
    for (uint32_t i = 0; i < nx * ny; ++i) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->bins[i], h2->bins[i]);
        if (h1->sum_w2 && h2->sum_w2) {
            TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->sum_w2[i], h2->sum_w2[i]);
        }
    }

    for (int r = 0; r < 9; ++r) {
        TEST_ASSERT_EQUAL_UINT64(h1->guards[r].count, h2->guards[r].count);
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->guards[r].weight, h2->guards[r].weight);
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, h1->guards[r].sum_w2, h2->guards[r].sum_w2);
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
    return UNITY_END();
}
