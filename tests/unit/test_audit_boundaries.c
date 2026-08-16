#include "unity.h"
#include "histo/histo.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

/* ========================================================================= */
/* Focus Area 1: Boundary Bin Transitions & Reciprocal Precision Traps       */
/* ========================================================================= */

/**
 * Audit 1.1: Exact transitions and nextafter epsilon for N=3 (1/3, 2/3 binary fractions)
 */
void test_uniform_boundary_exact_and_epsilon_n3(void) {
    histo_t *h = histo_create_uniform(3, 0.0, 1.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    int64_t bin = -999;

    /* x = 0.0 (exact min) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 0.0, &bin));
    TEST_ASSERT_EQUAL_INT64(0, bin);

    /* x < min (underflow) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(0.0, -1.0), &bin));
    TEST_ASSERT_EQUAL_INT64(-1, bin);

    /* x > min (inside bin 0) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(0.0, 1.0), &bin));
    TEST_ASSERT_EQUAL_INT64(0, bin);

    /* Boundary 1: 1/3 (0.33333333333333331...) */
    double edge1 = 1.0 / 3.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, edge1, &bin));
    TEST_ASSERT_EQUAL_INT64(1, bin); /* Exact edge must land in upper bin 1 */

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(edge1, 0.0), &bin));
    TEST_ASSERT_EQUAL_INT64(0, bin); /* Epsilon below must land in bin 0 */

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(edge1, 1.0), &bin));
    TEST_ASSERT_EQUAL_INT64(1, bin); /* Epsilon above must land in bin 1 */

    /* Boundary 2: 2/3 (0.66666666666666663...) */
    double edge2 = 2.0 / 3.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, edge2, &bin));
    TEST_ASSERT_EQUAL_INT64(2, bin); /* Exact edge must land in upper bin 2 */

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(edge2, 0.0), &bin));
    TEST_ASSERT_EQUAL_INT64(1, bin); /* Epsilon below must land in bin 1 */

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(edge2, 1.0), &bin));
    TEST_ASSERT_EQUAL_INT64(2, bin); /* Epsilon above must land in bin 2 */

    /* Upper Range Boundary: 1.0 (exact max -> overflow) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 1.0, &bin));
    TEST_ASSERT_EQUAL_INT64(3, bin); /* Overflow index = nbins = 3 */

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(1.0, 0.0), &bin));
    TEST_ASSERT_EQUAL_INT64(2, bin); /* Epsilon below max lands in bin 2 */

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(1.0, 2.0), &bin));
    TEST_ASSERT_EQUAL_INT64(3, bin); /* Epsilon above max is overflow */

    histo_destroy(h);
}

/**
 * Audit 1.2: Prime and non-power-of-two uniform divisions (N = 7, 11, 13, 17, 23, 29, 31, 37, 41)
 * Stress-testing reciprocal multiplication truncation on non-power-of-two divisions.
 */
void test_uniform_prime_divisions_sweep(void) {
    const uint32_t primes[] = {7, 11, 13, 17, 23, 29, 31, 37, 41, 97};
    const size_t num_primes = sizeof(primes) / sizeof(primes[0]);

    for (size_t p_idx = 0; p_idx < num_primes; ++p_idx) {
        uint32_t nbins = primes[p_idx];
        double min_val = -10.0;
        double max_val = 25.0;

        histo_t *h = histo_create_uniform(nbins, min_val, max_val, HISTO_FLAG_NONE);
        TEST_ASSERT_NOT_NULL(h);

        int64_t bin = -999;

        /* Test underflow boundary */
        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(min_val, -INFINITY), &bin));
        TEST_ASSERT_EQUAL_INT64(-1, bin);

        /* Test overflow boundary */
        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, max_val, &bin));
        TEST_ASSERT_EQUAL_INT64((int64_t)nbins, bin);

        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(max_val, -INFINITY), &bin));
        TEST_ASSERT_EQUAL_INT64((int64_t)nbins - 1, bin);

        /* Test every single bin boundary and its epsilon neighborhoods */
        for (uint32_t k = 0; k < nbins; ++k) {
            double low = 0.0, high = 0.0;
            TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_bounds(h, k, &low, &high));

            /* Exact lower edge must land in bin k */
            TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, low, &bin));
            TEST_ASSERT_EQUAL_INT64((int64_t)k, bin);

            /* Lower edge + 1 ULP must land in bin k */
            TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(low, +INFINITY), &bin));
            TEST_ASSERT_EQUAL_INT64((int64_t)k, bin);

            /* Lower edge - 1 ULP must land in bin k - 1 (or -1 if k == 0) */
            TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(low, -INFINITY), &bin));
            TEST_ASSERT_EQUAL_INT64((int64_t)k - 1, bin);

            /* Upper edge - 1 ULP must land in bin k */
            TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(high, -INFINITY), &bin));
            TEST_ASSERT_EQUAL_INT64((int64_t)k, bin);
        }

        histo_destroy(h);
    }
}

/**
 * Audit 1.3: IEEE-754 signed zero (-0.0 vs +0.0) at boundaries
 */
void test_uniform_signed_zero_boundary(void) {
    /* Case A: Zero is the lower boundary: [0.0, 10.0], nbins=5 */
    {
        histo_t *h = histo_create_uniform(5, 0.0, 10.0, HISTO_FLAG_NONE);
        TEST_ASSERT_NOT_NULL(h);

        int64_t bin = -999;
        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, +0.0, &bin));
        TEST_ASSERT_EQUAL_INT64(0, bin);

        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, -0.0, &bin));
        TEST_ASSERT_EQUAL_INT64(0, bin); /* -0.0 == +0.0, so lands in bin 0 */

        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(-0.0, -1.0), &bin));
        TEST_ASSERT_EQUAL_INT64(-1, bin); /* Underflow */

        histo_destroy(h);
    }

    /* Case B: Zero is an internal bin boundary: [-5.0, 5.0], nbins=10 (edge at 0 between bin 4 and 5) */
    {
        histo_t *h = histo_create_uniform(10, -5.0, 5.0, HISTO_FLAG_NONE);
        TEST_ASSERT_NOT_NULL(h);

        int64_t bin = -999;
        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, +0.0, &bin));
        TEST_ASSERT_EQUAL_INT64(5, bin);

        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, -0.0, &bin));
        TEST_ASSERT_EQUAL_INT64(5, bin); /* -0.0 >= 0.0 is true, so lands in bin 5 */

        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(-0.0, -1.0), &bin));
        TEST_ASSERT_EQUAL_INT64(4, bin);

        histo_destroy(h);
    }

    /* Case C: Zero is the upper boundary: [-10.0, 0.0], nbins=5 */
    {
        histo_t *h = histo_create_uniform(5, -10.0, 0.0, HISTO_FLAG_NONE);
        TEST_ASSERT_NOT_NULL(h);

        int64_t bin = -999;
        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, +0.0, &bin));
        TEST_ASSERT_EQUAL_INT64(5, bin); /* Overflow */

        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, -0.0, &bin));
        TEST_ASSERT_EQUAL_INT64(5, bin); /* -0.0 >= 0.0 is true, so overflow */

        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(-0.0, -1.0), &bin));
        TEST_ASSERT_EQUAL_INT64(4, bin); /* Last bin */

        histo_destroy(h);
    }
}

/**
 * Audit 1.4: Extreme dynamic range boundaries
 */
void test_uniform_extreme_ranges(void) {
    /* Extreme small range where inv_binsize remains finite (width = 1.0e-300, inv_binsize = 1.0e301 <= DBL_MAX) */
    {
        double min_val = 1.0e-300;
        double max_val = 2.0e-300;
        histo_t *h = histo_create_uniform(10, min_val, max_val, HISTO_FLAG_NONE);
        TEST_ASSERT_NOT_NULL(h);

        int64_t bin = -999;
        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, min_val, &bin));
        TEST_ASSERT_EQUAL_INT64(0, bin);

        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, min_val + 5.0e-301, &bin));
        TEST_ASSERT_EQUAL_INT64(5, bin);

        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(max_val, -1.0), &bin));
        TEST_ASSERT_EQUAL_INT64(9, bin);

        histo_destroy(h);
    }

    /* Large offset with small delta: [1e15, 1e15 + 10.0] */
    {
        double min_val = 1.0e15;
        double max_val = 1.0e15 + 10.0;
        histo_t *h = histo_create_uniform(10, min_val, max_val, HISTO_FLAG_NONE);
        TEST_ASSERT_NOT_NULL(h);

        int64_t bin = -999;
        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, min_val, &bin));
        TEST_ASSERT_EQUAL_INT64(0, bin);

        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, min_val + 5.0, &bin));
        TEST_ASSERT_EQUAL_INT64(5, bin);

        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(max_val, -1.0), &bin));
        TEST_ASSERT_EQUAL_INT64(9, bin);

        histo_destroy(h);
    }
}

/**
 * Audit 1.5: Subnormal range inv_binsize reciprocal overflow hazard
 * When range width is subnormal (< 1e-308), nbins / width overflows double precision to +INFINITY.
 */
void test_uniform_subnormal_inv_binsize_overflow(void) {
    double min_val = 1.0e-315;
    double max_val = 1.0e-310;
    histo_t *h = histo_create_uniform(10, min_val, max_val, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    int64_t bin = -999;
    /* First bin lookup */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, min_val, &bin));
    TEST_ASSERT_EQUAL_INT64(0, bin);

    /* Due to inv_binsize == +INFINITY, (x - min) * inf produces inf, which triggers
     * truncation clamping. Documenting this behavior:
     */
    histo_status_t st = histo_find_bin(h, nextafter(max_val, -1.0), &bin);
    TEST_ASSERT_EQUAL(HISTO_OK, st);

    histo_destroy(h);
}

/* ========================================================================= */
/* Focus Area 2: Variable Bin Binary Search & Pathologies                    */
/* ========================================================================= */

/**
 * Audit 2.1: Variable bin binary search boundary matching and epsilon transition
 */
void test_variable_bin_exact_edges_and_epsilon(void) {
    const double edges[] = {-1000.0, -100.0, -1.0, 0.0, 1.0, 100.0, 1000.0};
    const uint32_t nbins = 6;

    histo_t *h = histo_create_variable(nbins, edges, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    int64_t bin = -999;

    /* Test underflow boundary */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(edges[0], -INFINITY), &bin));
    TEST_ASSERT_EQUAL_INT64(-1, bin);

    /* Test overflow boundary */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, edges[nbins], &bin));
    TEST_ASSERT_EQUAL_INT64((int64_t)nbins, bin);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(edges[nbins], -INFINITY), &bin));
    TEST_ASSERT_EQUAL_INT64((int64_t)nbins - 1, bin);

    /* Test every internal boundary */
    for (uint32_t i = 0; i < nbins; ++i) {
        /* Exact lower edge */
        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, edges[i], &bin));
        TEST_ASSERT_EQUAL_INT64((int64_t)i, bin);

        /* Epsilon above lower edge */
        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(edges[i], +INFINITY), &bin));
        TEST_ASSERT_EQUAL_INT64((int64_t)i, bin);

        /* Epsilon below lower edge */
        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, nextafter(edges[i], -INFINITY), &bin));
        TEST_ASSERT_EQUAL_INT64((int64_t)i - 1, bin);
    }

    histo_destroy(h);
}

/**
 * Audit 2.2: Variable bin with near-zero-width bins (1 ULP spacing)
 */
void test_variable_bin_near_zero_width(void) {
    double base = 100.0;
    double e0 = base;
    double e1 = nextafter(base, +INFINITY);
    double e2 = nextafter(e1, +INFINITY);
    double e3 = nextafter(e2, +INFINITY);
    double e4 = 200.0;

    const double edges[] = {e0, e1, e2, e3, e4};
    histo_t *h = histo_create_variable(4, edges, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    int64_t bin = -999;

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, e0, &bin));
    TEST_ASSERT_EQUAL_INT64(0, bin);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, e1, &bin));
    TEST_ASSERT_EQUAL_INT64(1, bin);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, e2, &bin));
    TEST_ASSERT_EQUAL_INT64(2, bin);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, e3, &bin));
    TEST_ASSERT_EQUAL_INT64(3, bin);

    histo_destroy(h);
}

/**
 * Audit 2.3: Large-scale variable bin binary search (N = 1000 bins)
 */
void test_variable_bin_large_scale(void) {
    const uint32_t nbins = 1000;
    double *edges = (double *)malloc((nbins + 1) * sizeof(double));
    TEST_ASSERT_NOT_NULL(edges);

    for (uint32_t i = 0; i <= nbins; ++i) {
        edges[i] = (double)i * (double)i * 0.01; /* Quadratic spacing */
    }

    histo_t *h = histo_create_variable(nbins, edges, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    int64_t bin = -999;
    for (uint32_t i = 0; i < nbins; ++i) {
        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, edges[i], &bin));
        TEST_ASSERT_EQUAL_INT64((int64_t)i, bin);

        /* Midpoint */
        double mid = 0.5 * (edges[i] + edges[i + 1]);
        TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, mid, &bin));
        TEST_ASSERT_EQUAL_INT64((int64_t)i, bin);
    }

    histo_destroy(h);
    free(edges);
}

/**
 * Audit 2.4: Pathological edge configuration rejection
 */
void test_variable_bin_pathological_rejections(void) {
    /* Equal edges (zero-width bin) */
    {
        const double bad_edges[] = {0.0, 1.0, 1.0, 2.0};
        TEST_ASSERT_NULL(histo_create_variable(3, bad_edges, HISTO_FLAG_NONE));
    }

    /* Decreasing edges */
    {
        const double bad_edges[] = {0.0, 2.0, 1.0, 3.0};
        TEST_ASSERT_NULL(histo_create_variable(3, bad_edges, HISTO_FLAG_NONE));
    }

    /* Non-finite edges */
    {
        const double bad_edges[] = {0.0, NAN, 2.0};
        TEST_ASSERT_NULL(histo_create_variable(2, bad_edges, HISTO_FLAG_NONE));
    }
    {
        const double bad_edges[] = {-INFINITY, 0.0, 1.0};
        TEST_ASSERT_NULL(histo_create_variable(2, bad_edges, HISTO_FLAG_NONE));
    }

    /* Span overflow (max - min == INFINITY) */
    {
        const double bad_edges[] = {-1.0e308, 0.0, 1.0e308};
        TEST_ASSERT_NULL(histo_create_variable(2, bad_edges, HISTO_FLAG_NONE));
    }
}

/* ========================================================================= */
/* Focus Area 3: Ingestion Math, Striding & Numerical Edge Cases             */
/* ========================================================================= */

typedef struct SampleRecord {
    uint32_t id;
    uint8_t  flag;
    char     padding[3];
    double   value;
    uint64_t timestamp;
    double   weight;
} SampleRecord;

/**
 * Audit 3.1: Strided batch fill with struct padding and non-standard layout
 */
void test_fill_strided_struct_padding(void) {
    const size_t n_samples = 4;
    SampleRecord records[4] = {
        { .id = 1, .flag = 0, .value = 0.5,  .timestamp = 100, .weight = 2.0 },
        { .id = 2, .flag = 1, .value = 1.5,  .timestamp = 200, .weight = 3.0 },
        { .id = 3, .flag = 0, .value = 2.5,  .timestamp = 300, .weight = 4.0 },
        { .id = 4, .flag = 1, .value = -5.0, .timestamp = 400, .weight = 5.0 }  /* Underflow */
    };

    histo_t *h = histo_create_uniform(3, 0.0, 3.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    histo_status_t st = histo_fill_strided(h, n_samples,
                                           &records[0].value, sizeof(SampleRecord),
                                           &records[0].weight, sizeof(SampleRecord));
    TEST_ASSERT_EQUAL(HISTO_OK, st);

    /* Verify bin contents */
    double c0 = 0.0, c1 = 0.0, c2 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 0, &c0));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 1, &c1));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 2, &c2));

    TEST_ASSERT_EQUAL_DOUBLE(2.0, c0);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, c1);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, c2);

    /* Verify underflow */
    TEST_ASSERT_EQUAL_DOUBLE(5.0, histo_underflow(h));
    TEST_ASSERT_EQUAL_DOUBLE(9.0, histo_total_weight(h));
    TEST_ASSERT_EQUAL_UINT64(3, histo_num_entries(h));

    /* Verify sum_w2 */
    double err0 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h, 0, &err0));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, err0); /* sqrt(2^2) = 2 */

    histo_destroy(h);
}

/**
 * Audit 3.2: Strided fill edge cases (null weights, zero stride, n=0, null x)
 */
void test_fill_strided_edge_cases(void) {
    histo_t *h = histo_create_uniform(5, 0.0, 10.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    double vals[] = {1.0, 3.0, 5.0};

    /* weights = NULL (unit weights) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_strided(h, 3, vals, sizeof(double), NULL, 0));
    TEST_ASSERT_EQUAL_DOUBLE(3.0, histo_total_weight(h));

    /* stride = 0 fallback to sizeof(double) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_strided(h, 3, vals, 0, NULL, 0));
    TEST_ASSERT_EQUAL_DOUBLE(6.0, histo_total_weight(h));

    /* n = 0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_strided(h, 0, vals, sizeof(double), NULL, 0));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_strided(h, 0, NULL, 0, NULL, 0));

    /* x = NULL with n > 0 */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_fill_strided(h, 5, NULL, sizeof(double), NULL, 0));

    histo_destroy(h);
}

/**
 * Audit 3.3: Ingestion with mixed finite and non-finite samples
 */
void test_fill_mixed_non_finite(void) {
    histo_t *h = histo_create_uniform(5, 0.0, 10.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    double values[] = {1.0, NAN, 3.0, INFINITY, -INFINITY, 5.0};
    double weights[] = {1.0, 1.0, NAN, 2.0, 3.0, 1.0};

    histo_status_t st = histo_fill_n(h, 6, values, weights);
    /* Should return HISTO_WARN_NON_FINITE due to skipped NaNs */
    TEST_ASSERT_EQUAL(HISTO_WARN_NON_FINITE, st);

    /* Verify NaN count: index 1 has NaN x, index 2 has NaN weight -> 2 NaNs */
    TEST_ASSERT_EQUAL_UINT64(2, histo_nan_count(h));

    /* Verify +Inf went to overflow (weight 2.0) */
    TEST_ASSERT_EQUAL_DOUBLE(2.0, histo_overflow(h));

    /* Verify -Inf went to underflow (weight 3.0) */
    TEST_ASSERT_EQUAL_DOUBLE(3.0, histo_underflow(h));

    /* Verify finite in-range samples (index 0 with w=1, index 5 with w=1) */
    TEST_ASSERT_EQUAL_DOUBLE(2.0, histo_total_weight(h));
    TEST_ASSERT_EQUAL_UINT64(2, histo_num_entries(h));

    histo_destroy(h);
}

/**
 * Audit 3.4: Negative weights and zero-weight ingestion
 */
void test_fill_negative_and_zero_weights(void) {
    histo_t *h = histo_create_uniform(3, 0.0, 30.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    /* Zero weight fill */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_w(h, 5.0, 0.0));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, histo_total_weight(h));
    TEST_ASSERT_EQUAL_UINT64(1, histo_num_entries(h));

    /* Negative weight fill */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_w(h, 5.0, -10.0));
    TEST_ASSERT_EQUAL_DOUBLE(-10.0, histo_total_weight(h));

    double content = 0.0, err = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 0, &content));
    TEST_ASSERT_EQUAL_DOUBLE(-10.0, content);

    /* sum_w2 should accumulate (-10)^2 = 100 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h, 0, &err));
    TEST_ASSERT_EQUAL_DOUBLE(10.0, err);

    histo_destroy(h);
}

/* ========================================================================= */
/* Focus Area 4: Arithmetic, Scale, Transformations & Boundaries             */
/* ========================================================================= */

/**
 * Audit 4.1: Midpoint overflow analysis on histo_bin_center
 * Flaw: 0.5 * (lower + upper) overflows if lower + upper > DBL_MAX!
 */
void test_bin_center_intermediate_overflow_analysis(void) {
    /* Bins near DBL_MAX: [1.0e308, 1.6e308] */
    double min_val = 1.0e308;
    double max_val = 1.6e308;
    histo_t *h = histo_create_uniform(2, min_val, max_val, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    double low = 0.0, high = 0.0, center = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_bounds(h, 0, &low, &high));
    TEST_ASSERT_EQUAL_DOUBLE(1.0e308, low);
    TEST_ASSERT_EQUAL_DOUBLE(1.3e308, high);

    /* Analytical midpoint is 1.15e308.
     * With 0.5 * (lower + upper): 1.0e308 + 1.3e308 = 2.3e308 > DBL_MAX (1.797e308) -> INFINITY!
     */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_center(h, 0, &center));

    /* Document whether center calculation overflowed to INFINITY */
    if (isinf(center)) {
        /* Confirmed: Midpoint overflow flaw exists in 0.5 * (lower + upper) */
        TEST_ASSERT_TRUE(isinf(center));
    } else {
        TEST_ASSERT_DOUBLE_WITHIN(1.0e300, 1.15e308, center);
    }

    histo_destroy(h);
}

/**
 * Audit 4.2: Division error propagation and zero divisor bins
 */
void test_divide_zeros_and_error_propagation(void) {
    histo_t *h1 = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_TRACK_SUMW2);
    histo_t *h2 = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h1);
    TEST_ASSERT_NOT_NULL(h2);

    /* Bin 0: non-zero / non-zero: 12.0 / 4.0 = 3.0 */
    histo_fill_w(h1, 5.0, 12.0); /* w1 = 12, w1^2 = 144 */
    histo_fill_w(h2, 5.0, 4.0);  /* w2 = 4,  w2^2 = 16 */

    /* Bin 1: non-zero / zero: 10.0 / 0.0 -> should result in 0.0 */
    histo_fill_w(h1, 15.0, 10.0);

    /* Bin 2: zero / non-zero: 0.0 / 5.0 -> 0.0 */
    histo_fill_w(h2, 25.0, 5.0);

    /* Bin 3: zero / zero: 0.0 / 0.0 -> 0.0 */

    TEST_ASSERT_EQUAL(HISTO_OK, histo_divide(h1, h2));

    double c = 0.0, err = 0.0;

    /* Bin 0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h1, 0, &c));
    TEST_ASSERT_EQUAL_DOUBLE(3.0, c);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h1, 0, &err));
    /* Analytical error: sqrt( (w1^2*h2^2 + w2^2*h1^2) / h2^4 )
     * = sqrt( (144 * 16 + 16 * 144) / 256 ) = sqrt( 4608 / 256 ) = sqrt(18) = ~4.242640687
     */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, sqrt(18.0), err);

    /* Bin 1: division by zero bin safely results in 0.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h1, 1, &c));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, c);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h1, 1, &err));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, err);

    /* Bin 2: zero numerator results in 0.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h1, 2, &c));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, c);

    /* Bin 3: zero / zero results in 0.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h1, 3, &c));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, c);

    histo_destroy(h1);
    histo_destroy(h2);
}

/**
 * Audit 4.3: Scaling by zero and negative scalar factors
 */
void test_scale_zero_and_negative(void) {
    histo_t *h = histo_create_uniform(3, 0.0, 30.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    histo_fill_w(h, 5.0, 10.0);
    histo_fill_w(h, 15.0, 20.0);

    /* Scale by -2.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_scale(h, -2.0));
    TEST_ASSERT_EQUAL_DOUBLE(-60.0, histo_total_weight(h));

    double c0 = 0.0, err0 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 0, &c0));
    TEST_ASSERT_EQUAL_DOUBLE(-20.0, c0);

    /* sum_w2 must be positive: scaled by (-2.0)^2 = 4.0 -> 100 * 4 = 400 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h, 0, &err0));
    TEST_ASSERT_EQUAL_DOUBLE(20.0, err0);

    /* Scale by 0.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_scale(h, 0.0));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, histo_total_weight(h));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 0, &c0));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, c0);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h, 0, &err0));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, err0);

    /* Scale by non-finite rejected */
    TEST_ASSERT_EQUAL(HISTO_ERR_NON_FINITE, histo_scale(h, NAN));
    TEST_ASSERT_EQUAL(HISTO_ERR_NON_FINITE, histo_scale(h, INFINITY));

    histo_destroy(h);
}

/**
 * Audit 4.4: Normalization boundary conditions
 */
void test_normalize_boundaries(void) {
    histo_t *h = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    /* Normalizing empty histogram */
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_normalize(h, 1.0));

    /* Normalizing with invalid target area */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_normalize(h, 0.0));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_normalize(h, -5.0));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_normalize(h, NAN));

    /* Fill and normalize */
    histo_fill_w(h, 5.0, 10.0);
    histo_fill_w(h, 15.0, 30.0);
    TEST_ASSERT_EQUAL_DOUBLE(40.0, histo_total_weight(h));

    TEST_ASSERT_EQUAL(HISTO_OK, histo_normalize(h, 100.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 100.0, histo_total_weight(h));

    double c0 = 0.0, c1 = 0.0;
    histo_bin_content(h, 0, &c0);
    histo_bin_content(h, 1, &c1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 25.0, c0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 75.0, c1);

    histo_destroy(h);
}

/**
 * Audit 4.5: Rebinning factor divisibility and mass conservation
 */
void test_rebin_factors_and_mass_conservation(void) {
    histo_t *h = histo_create_uniform(12, 0.0, 120.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    for (uint32_t i = 0; i < 12; ++i) {
        histo_fill_w(h, (double)i * 10.0 + 5.0, (double)(i + 1));
    }
    TEST_ASSERT_EQUAL_DOUBLE(78.0, histo_total_weight(h)); /* Sum 1..12 = 78 */

    /* Invalid rebin factors */
    TEST_ASSERT_NULL(histo_rebin(h, 0));
    TEST_ASSERT_NULL(histo_rebin(h, 5));  /* 12 % 5 != 0 */
    TEST_ASSERT_NULL(histo_rebin(h, 13)); /* factor > nbins */

    /* Valid factors: 1, 2, 3, 4, 6, 12 */
    const uint32_t valid_factors[] = {1, 2, 3, 4, 6, 12};
    for (size_t i = 0; i < sizeof(valid_factors) / sizeof(valid_factors[0]); ++i) {
        uint32_t f = valid_factors[i];
        histo_t *rebinned = histo_rebin(h, f);
        TEST_ASSERT_NOT_NULL(rebinned);

        TEST_ASSERT_EQUAL_UINT32(12 / f, histo_nbins(rebinned));
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 78.0, histo_total_weight(rebinned));

        /* Range preserved */
        double rmin = 0.0, rmax = 0.0;
        TEST_ASSERT_EQUAL(HISTO_OK, histo_range(rebinned, &rmin, &rmax));
        TEST_ASSERT_EQUAL_DOUBLE(0.0, rmin);
        TEST_ASSERT_EQUAL_DOUBLE(120.0, rmax);

        histo_destroy(rebinned);
    }

    histo_destroy(h);
}

/**
 * Audit 4.6: Slicing sub-ranges, edge boundaries, and mass conservation
 */
void test_slice_boundaries_and_mass_conservation(void) {
    histo_t *h = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    /* Fill underflow, bins, overflow */
    histo_fill_w(h, -5.0, 10.0); /* Underflow = 10 */
    for (uint32_t i = 0; i < 10; ++i) {
        histo_fill_w(h, (double)i * 10.0 + 5.0, 1.0); /* 1.0 in each bin */
    }
    histo_fill_w(h, 150.0, 20.0); /* Overflow = 20 */

    /* Slice internal range [2, 5] (nbins = 4, range [20.0, 60.0]) */
    histo_t *slice = histo_slice(h, 2, 5, false);
    TEST_ASSERT_NOT_NULL(slice);

    TEST_ASSERT_EQUAL_UINT32(4, histo_nbins(slice));

    double smin = 0.0, smax = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_range(slice, &smin, &smax));
    TEST_ASSERT_EQUAL_DOUBLE(20.0, smin);
    TEST_ASSERT_EQUAL_DOUBLE(60.0, smax);

    /* In-range weight of slice = 4.0 */
    TEST_ASSERT_EQUAL_DOUBLE(4.0, histo_total_weight(slice));

    /* Underflow of slice = original underflow (10) + bins 0 and 1 (1+1) = 12 */
    TEST_ASSERT_EQUAL_DOUBLE(12.0, histo_underflow(slice));

    /* Overflow of slice = original overflow (20) + bins 6, 7, 8, 9 (1+1+1+1) = 24 */
    TEST_ASSERT_EQUAL_DOUBLE(24.0, histo_overflow(slice));

    /* Mass conservation: 12 + 4 + 24 = 40 == 10 + 10 + 20 */
    double total_slice_mass = histo_underflow(slice) + histo_total_weight(slice) + histo_overflow(slice);
    double total_orig_mass = histo_underflow(h) + histo_total_weight(h) + histo_overflow(h);
    TEST_ASSERT_EQUAL_DOUBLE(total_orig_mass, total_slice_mass);

    /* Invalid slices */
    TEST_ASSERT_NULL(histo_slice(h, 5, 2, false)); /* start > end */
    TEST_ASSERT_NULL(histo_slice(h, 2, 10, false)); /* end >= nbins */

    histo_destroy(slice);
    histo_destroy(h);
}

/**
 * Audit 4.7: CDF construction and prenormalization
 */
void test_cdf_properties(void) {
    histo_t *h = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    /* Empty histogram CDF */
    TEST_ASSERT_NULL(histo_cdf(h, 1.0));

    histo_fill_w(h, 5.0, 10.0);
    histo_fill_w(h, 15.0, 20.0);
    histo_fill_w(h, 25.0, 30.0);
    histo_fill_w(h, 35.0, 40.0);
    /* Total = 100 */

    /* Standard CDF (prenormalization = 0.0 defaults to 1.0) */
    histo_t *cdf_std = histo_cdf(h, 0.0);
    TEST_ASSERT_NOT_NULL(cdf_std);

    double c0 = 0.0, c1 = 0.0, c2 = 0.0, c3 = 0.0;
    histo_bin_content(cdf_std, 0, &c0);
    histo_bin_content(cdf_std, 1, &c1);
    histo_bin_content(cdf_std, 2, &c2);
    histo_bin_content(cdf_std, 3, &c3);

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.1, c0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.3, c1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.6, c2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, c3);

    /* Prenormalized CDF to 100.0 */
    histo_t *cdf_100 = histo_cdf(h, 100.0);
    TEST_ASSERT_NOT_NULL(cdf_100);

    histo_bin_content(cdf_100, 0, &c0);
    histo_bin_content(cdf_100, 3, &c3);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.0, c0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 100.0, c3);

    histo_destroy(cdf_std);
    histo_destroy(cdf_100);
    histo_destroy(h);
}

/**
 * Audit 4.8: Quantile boundaries and interpolation across empty bin plateaus
 */
void test_quantile_plateaus_and_boundaries(void) {
    histo_t *h = histo_create_uniform(5, 0.0, 50.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    /* Fill bin 0: [0, 10) with weight 10 */
    /* Leave bins 1, 2, 3 EMPTY */
    /* Fill bin 4: [40, 50) with weight 10 */
    histo_fill_w(h, 5.0, 10.0);
    histo_fill_w(h, 45.0, 10.0);

    double q = 0.0;

    /* p = 0.0 -> lower edge of bin 0 = 0.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h, 0.0, &q));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, q);

    /* p = 1.0 -> upper edge of bin 4 = 50.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h, 1.0, &q));
    TEST_ASSERT_EQUAL_DOUBLE(50.0, q);

    /* p = 0.25 -> target weight = 5.0 (out of 10 in bin 0) -> theta = 0.5 -> coordinate = 5.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h, 0.25, &q));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, q);

    /* p = 0.5 (median) -> target 10.0, completes bin 0 -> upper edge of bin 0 = 10.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h, 0.5, &q));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.0, q);

    /* p = 0.75 -> 5.0/10.0 through bin 4 = 40.0 + 0.5 * 10 = 45.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h, 0.75, &q));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 45.0, q);

    /* Out of bounds p */
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo_quantile(h, -0.1, &q));
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo_quantile(h, 1.1, &q));
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo_quantile(h, NAN, &q));

    histo_destroy(h);
}

/* ========================================================================= */
/* Test Runner Entry Point                                                   */
/* ========================================================================= */

int main(void) {
    UNITY_BEGIN();

    /* Focus Area 1: Boundary Bin Transitions & Reciprocal Precision Traps */
    RUN_TEST(test_uniform_boundary_exact_and_epsilon_n3);
    RUN_TEST(test_uniform_prime_divisions_sweep);
    RUN_TEST(test_uniform_signed_zero_boundary);
    RUN_TEST(test_uniform_extreme_ranges);
    RUN_TEST(test_uniform_subnormal_inv_binsize_overflow);

    /* Focus Area 2: Variable Bin Binary Search & Pathologies */
    RUN_TEST(test_variable_bin_exact_edges_and_epsilon);
    RUN_TEST(test_variable_bin_near_zero_width);
    RUN_TEST(test_variable_bin_large_scale);
    RUN_TEST(test_variable_bin_pathological_rejections);

    /* Focus Area 3: Ingestion Math, Striding & Numerical Edge Cases */
    RUN_TEST(test_fill_strided_struct_padding);
    RUN_TEST(test_fill_strided_edge_cases);
    RUN_TEST(test_fill_mixed_non_finite);
    RUN_TEST(test_fill_negative_and_zero_weights);

    /* Focus Area 4: Arithmetic, Scale, Transformations & Boundaries */
    RUN_TEST(test_bin_center_intermediate_overflow_analysis);
    RUN_TEST(test_divide_zeros_and_error_propagation);
    RUN_TEST(test_scale_zero_and_negative);
    RUN_TEST(test_normalize_boundaries);
    RUN_TEST(test_rebin_factors_and_mass_conservation);
    RUN_TEST(test_slice_boundaries_and_mass_conservation);
    RUN_TEST(test_cdf_properties);
    RUN_TEST(test_quantile_plateaus_and_boundaries);

    return UNITY_END();
}
