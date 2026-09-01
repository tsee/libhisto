/*
 * Unit tests verifying statistical moments, Welford stability, and quantiles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "unity.h"
#include "histo/histo.h"
#include "histo/histo2d.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void setUp(void) {}
void tearDown(void) {}

void test_welford_negative_weights(void) {
    histo_t *h = histo_create_uniform(10, 0.0, 30.0, HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);

    // 1. Fill x=10 with weight 1
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_w(h, 10.0, 1.0));
    
    // 2. Fill x=20 with weight 0.5
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_w(h, 20.0, 0.5));
    
    // 3. Fill x=10 with weight -0.5 (cancels out half of the first fill)
    // The resulting distribution should be x=10 w=0.5, x=20 w=0.5
    // Mean should be 15.0.
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_w(h, 10.0, -0.5));

    double mean = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h, &mean));

    // Wait, the mean from Welford will actually be 13.333!
    // But two-pass mean will be 15.0!
    // Let's assert the bug exists by checking if it's 15.0, which will fail if buggy.
    // We will assert true mathematical mean.
    TEST_ASSERT_FLOAT_WITHIN(1e-5, 15.0, mean);

    histo_destroy(h);
}

void test_subtract_moments_consistency(void) {
    histo_t *h1 = histo_create_uniform(10, 0.0, 30.0, HISTO_FLAG_EXACT_MOMENTS);
    histo_t *h2 = histo_create_uniform(10, 0.0, 30.0, HISTO_FLAG_EXACT_MOMENTS);

    histo_fill(h1, 10.0); // w=1, mean=10
    histo_fill(h1, 20.0); // w=2, mean=15

    histo_fill(h2, 20.0); // w=1, mean=20

    TEST_ASSERT_EQUAL(HISTO_OK, histo_subtract(h1, h2));

    double mean = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h1, &mean));

    // After subtracting x=20 w=1, h1 should only have x=10 w=1 left.
    // Mean should be 10.0.
    TEST_ASSERT_FLOAT_WITHIN(1e-5, 10.0, mean);

    histo_destroy(h1);
    histo_destroy(h2);
}

void test_quantile_discontinuity(void) {
    histo_t *h = histo_create_uniform(3, 0.0, 30.0, 0); /* bins: [0,10), [10,20), [20,30) */
    TEST_ASSERT_NOT_NULL(h);

    /* fill bin 0 (center 5) and bin 2 (center 25) with weight 1 each */
    histo_fill(h, 5.0);
    histo_fill(h, 25.0);

    /* p = 0.5 (target weight = 1.0, infimum inverse CDF lands on upper edge of bin 0 = 10.0) */
    double q1 = 0.0, q2 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h, 0.5, &q1));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.0, q1);

    /* p = 0.5 + eps (crosses empty plateau [10, 20) and enters lower edge of bin 2 = 20.0) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h, 0.5000001, &q2));
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 20.0, q2);

    histo_destroy(h);
}

/* ========================================================================= */
/* Welford Accumulator Stability on Ill-Conditioned Streams                  */
/* ========================================================================= */

void test_welford_cancellation_large_mean_variance_parity(void) {
    /* Test 1: Mean = 1e9, Variance = 1.0 */
    {
        const size_t n = 100000;
        const double mean_offset = 1.0e9;
        histo_t *h = histo_create_uniform(100, mean_offset - 50.0, mean_offset + 50.0, HISTO_FLAG_EXACT_MOMENTS);
        TEST_ASSERT_NOT_NULL(h);

        double delta_sum = 0.0;
        double delta_sq_sum = 0.0;
        uint32_t rng_state = 1234567;

        for (size_t i = 0; i < n; ++i) {
            rng_state = rng_state * 1664525u + 1013904223u;
            double u1 = (double)(rng_state & 0xFFFFFF) / 16777216.0;
            if (u1 <= 1e-15) u1 = 1e-15;

            rng_state = rng_state * 1664525u + 1013904223u;
            double u2 = (double)(rng_state & 0xFFFFFF) / 16777216.0;

            /* Box-Muller transform for standard normal sample */
            double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            double x = mean_offset + z;

            TEST_ASSERT_EQUAL(HISTO_OK, histo_fill(h, x));

            delta_sum += z;
            delta_sq_sum += z * z;
        }

        double expected_mean_delta = delta_sum / (double)n;
        double expected_var = (delta_sq_sum - (double)n * expected_mean_delta * expected_mean_delta) / (double)n;

        double computed_mean = 0.0;
        double computed_var = 0.0;
        TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h, &computed_mean));
        TEST_ASSERT_EQUAL(HISTO_OK, histo_variance(h, &computed_var));

        TEST_ASSERT_DOUBLE_WITHIN(1e-4, mean_offset + expected_mean_delta, computed_mean);
        /* Rel error on variance must be <= 1e-6 without catastrophic cancellation */
        double rel_var_err = fabs(computed_var - expected_var) / expected_var;
        TEST_ASSERT_TRUE(rel_var_err <= 1.0e-6);

        histo_destroy(h);
    }

    /* Test 2: Extreme mean = 1e12, tiny variance = 0.01 (sigma = 0.1) */
    {
        const size_t n = 100000;
        const double mean_offset = 1.0e12;
        const double sigma = 0.1;
        histo_t *h = histo_create_uniform(100, mean_offset - 5.0, mean_offset + 5.0, HISTO_FLAG_EXACT_MOMENTS);
        TEST_ASSERT_NOT_NULL(h);

        double delta_sum = 0.0;
        double delta_sq_sum = 0.0;
        uint32_t rng_state = 9876543;

        for (size_t i = 0; i < n; ++i) {
            rng_state = rng_state * 1664525u + 1013904223u;
            double u1 = (double)(rng_state & 0xFFFFFF) / 16777216.0;
            if (u1 <= 1e-15) u1 = 1e-15;

            rng_state = rng_state * 1664525u + 1013904223u;
            double u2 = (double)(rng_state & 0xFFFFFF) / 16777216.0;

            double z = sigma * sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            double x = mean_offset + z;

            TEST_ASSERT_EQUAL(HISTO_OK, histo_fill(h, x));

            delta_sum += z;
            delta_sq_sum += z * z;
        }

        double expected_mean_delta = delta_sum / (double)n;
        double expected_var = (delta_sq_sum - (double)n * expected_mean_delta * expected_mean_delta) / (double)n;

        double computed_mean = 0.0;
        double computed_var = 0.0;
        TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h, &computed_mean));
        TEST_ASSERT_EQUAL(HISTO_OK, histo_variance(h, &computed_var));

        TEST_ASSERT_DOUBLE_WITHIN(1e-2, mean_offset + expected_mean_delta, computed_mean);
        double rel_var_err = fabs(computed_var - expected_var) / expected_var;
        TEST_ASSERT_TRUE(rel_var_err <= 1.0e-3);

        histo_destroy(h);
    }
}

/* ========================================================================= */
/* Conservation Laws: 1D & 2D Weight and Marginal Projections                */
/* ========================================================================= */

void test_conservation_of_weight_1d_and_2d(void) {
    /* 1D Conservation */
    {
        const uint32_t nbins = 20;
        histo_t *h = histo_create_uniform(nbins, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
        TEST_ASSERT_NOT_NULL(h);

        uint32_t rng = 54321;
        const size_t num_samples = 500000;
        double total_injected_weight = 0.0;

        for (size_t i = 0; i < num_samples; ++i) {
            rng = rng * 1664525u + 1013904223u;
            double val = -20.0 + (double)(rng % 14000) / 100.0; /* [-20.0, 120.0] */
            double w = 0.5 + (double)(rng % 10) * 0.25;

            histo_fill_w(h, val, w);
            total_injected_weight += w;
        }

        double sum_bins = 0.0;
        for (uint32_t i = 0; i < nbins; ++i) {
            double c = 0.0;
            histo_bin_content(h, i, &c);
            sum_bins += c;
        }
        double total_w = histo_total_weight(h);
        double underflow_w = histo_underflow(h);
        double overflow_w = histo_overflow(h);

        TEST_ASSERT_DOUBLE_WITHIN(1e-7, total_w, sum_bins);
        TEST_ASSERT_DOUBLE_WITHIN(1e-7, total_injected_weight, sum_bins + underflow_w + overflow_w);

        histo_destroy(h);
    }

    /* 2D Conservation & Marginal Projections */
    {
        const uint32_t nx = 10, ny = 15;
        histo2d_t *h2 = histo2d_create_uniform(nx, 0.0, 100.0, ny, 0.0, 150.0, HISTO_FLAG_TRACK_SUMW2);
        TEST_ASSERT_NOT_NULL(h2);

        uint32_t rng = 998877;
        const size_t num_samples = 200000;

        for (size_t i = 0; i < num_samples; ++i) {
            rng = rng * 1664525u + 1013904223u;
            double x = -10.0 + (double)(rng % 12000) / 100.0;
            rng = rng * 1664525u + 1013904223u;
            double y = -15.0 + (double)(rng % 18000) / 100.0;
            double w = 1.0 + (double)(rng % 4) * 0.5;

            histo2d_fill_w(h2, x, y, w);
        }

        double sum_cells = 0.0;
        for (uint32_t ix = 0; ix < nx; ++ix) {
            for (uint32_t iy = 0; iy < ny; ++iy) {
                double c = 0.0;
                histo2d_bin_content(h2, ix, iy, &c);
                sum_cells += c;
            }
        }
        double total_2d_w = histo2d_total_weight(h2);
        TEST_ASSERT_DOUBLE_WITHIN(1e-7, total_2d_w, sum_cells);

        /* Marginal Projections along X and Y */
        histo_t *proj_x = NULL;
        histo_t *proj_y = NULL;
        TEST_ASSERT_EQUAL(HISTO_OK, histo2d_project_x(h2, &proj_x));
        TEST_ASSERT_EQUAL(HISTO_OK, histo2d_project_y(h2, &proj_y));
        TEST_ASSERT_NOT_NULL(proj_x);
        TEST_ASSERT_NOT_NULL(proj_y);

        double proj_x_tot = histo_total_weight(proj_x);
        double proj_y_tot = histo_total_weight(proj_y);

        TEST_ASSERT_DOUBLE_WITHIN(1e-7, total_2d_w, proj_x_tot);
        TEST_ASSERT_DOUBLE_WITHIN(1e-7, total_2d_w, proj_y_tot);

        histo_destroy(proj_x);
        histo_destroy(proj_y);
        histo2d_destroy(h2);
    }
}

/* ========================================================================= */
/* Quantile & Median Accuracy: Monotonicity, Scaling, and Translation        */
/* ========================================================================= */

void test_quantile_monotonicity_scaling_and_translation(void) {
    const uint32_t nbins = 100;
    histo_t *h_base  = histo_create_uniform(nbins, 0.0, 100.0, 0);
    histo_t *h_trans = histo_create_uniform(nbins, 25.0, 125.0, 0);
    histo_t *h_scale = histo_create_uniform(nbins, 0.0, 200.0, 0);

    const double translation = 25.0;
    const double scale = 2.0;

    /* Fill bimodal distribution */
    for (int i = 0; i < 1000; ++i) {
        double x1 = 30.0 + (double)(i % 20);
        double x2 = 70.0 + (double)(i % 20);
        histo_fill(h_base, x1);
        histo_fill(h_base, x2);

        histo_fill(h_trans, x1 + translation);
        histo_fill(h_trans, x2 + translation);

        histo_fill(h_scale, x1 * scale);
        histo_fill(h_scale, x2 * scale);
    }

    double prev_q = -1.0;
    for (int step = 0; step <= 200; ++step) {
        double p = (double)step / 200.0;
        double q_val = 0.0, q_trans = 0.0, q_scale = 0.0;

        TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h_base, p, &q_val));
        TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h_trans, p, &q_trans));
        TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h_scale, p, &q_scale));

        /* 1. Monotonicity: Q(p2) >= Q(p1) */
        TEST_ASSERT_TRUE(q_val >= prev_q);
        prev_q = q_val;

        /* 2. Translation Invariance: Q(X + c) == Q(X) + c */
        TEST_ASSERT_DOUBLE_WITHIN(1.0, q_val + translation, q_trans);

        /* 3. Scale Invariance: Q(s * X) == s * Q(X) */
        TEST_ASSERT_DOUBLE_WITHIN(2.0, q_val * scale, q_scale);
    }

    histo_destroy(h_base);
    histo_destroy(h_trans);
    histo_destroy(h_scale);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_welford_negative_weights);
    RUN_TEST(test_subtract_moments_consistency);
    RUN_TEST(test_quantile_discontinuity);
    RUN_TEST(test_welford_cancellation_large_mean_variance_parity);
    RUN_TEST(test_conservation_of_weight_1d_and_2d);
    RUN_TEST(test_quantile_monotonicity_scaling_and_translation);
    return UNITY_END();
}
