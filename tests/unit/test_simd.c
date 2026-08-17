#include "unity.h"
#include "histo/histo.h"
#include "../../src/simd.h"
#include "../../src/internal.h"
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
    /* Test bin edges, boundary transitions, epsilons */
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
    const size_t n = 67;
    double data[67];
    double weights[67];

    for (size_t i = 0; i < n; ++i) {
        data[i] = (double)i * 1.5 - 5.0; // spans underflow, in-range, overflow
        weights[i] = (double)(i % 5) * 0.5 + 0.25;
    }
    /* Inject non-finite specials */
    data[10] = NAN;
    weights[10] = 1.0;
    data[20] = INFINITY;
    weights[20] = 2.0;
    data[30] = 45.0;
    weights[30] = NAN;

    histo_t *h_scalar = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo_t *h_neon = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

    for (size_t i = 0; i < n; ++i) {
        histo_fill_w(h_scalar, data[i], weights[i]);
    }

    bool non_finite = histo_fill_uniform_w2_neon(h_neon, data, weights, n);
    TEST_ASSERT_TRUE(non_finite);

    assert_histograms_equal(h_scalar, h_neon);

    histo_destroy(h_scalar);
    histo_destroy(h_neon);
#else
    TEST_IGNORE_MESSAGE("NEON not enabled");
#endif
}

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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_neon_uniform_in_range);
    RUN_TEST(test_neon_exact_edges);
    RUN_TEST(test_neon_out_of_range_and_specials);
    RUN_TEST(test_neon_weighted_w2);
    RUN_TEST(test_simd_dispatch_equivalence);
    return UNITY_END();
}
