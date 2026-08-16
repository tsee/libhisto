#include "unity.h"
#include "histo/histo.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

/* Test bin boundary, center, content, and error queries */
void test_bin_queries(void) {
    histo_t *h = histo_create_uniform(4, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    /* Fill bin 0: [0, 25) with weight 4.0 */
    /* Fill bin 1: [25, 50) with two weights of 3.0 (sum=6, sum_w2 = 9+9 = 18) */
    histo_fill_w(h, 10.0, 4.0);
    histo_fill_w(h, 30.0, 3.0);
    histo_fill_w(h, 40.0, 3.0);

    double low = 0.0, high = 0.0, center = 0.0, content = 0.0, err = 0.0;

    /* Bin 0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_bounds(h, 0, &low, &high));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, low);
    TEST_ASSERT_EQUAL_DOUBLE(25.0, high);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_center(h, 0, &center));
    TEST_ASSERT_EQUAL_DOUBLE(12.5, center);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 0, &content));
    TEST_ASSERT_EQUAL_DOUBLE(4.0, content);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h, 0, &err));
    TEST_ASSERT_EQUAL_DOUBLE(4.0, err); /* sqrt(4^2) = 4.0 */

    /* Bin 1 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_bounds(h, 1, &low, &high));
    TEST_ASSERT_EQUAL_DOUBLE(25.0, low);
    TEST_ASSERT_EQUAL_DOUBLE(50.0, high);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 1, &content));
    TEST_ASSERT_EQUAL_DOUBLE(6.0, content);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h, 1, &err));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, sqrt(18.0), err);

    /* Out of bounds index */
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo_bin_bounds(h, 4, &low, &high));
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo_bin_content(h, 4, &content));
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo_bin_error(h, 4, &err));

    histo_destroy(h);
}

/* Test integral calculation */
void test_integral(void) {
    histo_t *h = histo_create_uniform(4, 0.0, 10.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    histo_fill_bin(h, 0, 1.0);
    histo_fill_bin(h, 1, 2.0);
    histo_fill_bin(h, 2, 3.0);
    histo_fill_bin(h, 3, 4.0);

    double sum = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_integral(h, 0, 3, &sum));
    TEST_ASSERT_EQUAL_DOUBLE(10.0, sum);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_integral(h, 1, 2, &sum));
    TEST_ASSERT_EQUAL_DOUBLE(5.0, sum);

    /* Invalid range */
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo_integral(h, 2, 1, &sum));
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo_integral(h, 0, 4, &sum));

    histo_destroy(h);
}

/* Test statistical moments: mean, variance, std_dev */
void test_moments(void) {
    /* Exact moments enabled */
    histo_t *h_exact = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h_exact);

    /* Samples: 10, 20, 30 -> mean = 20, variance = ((10-20)^2 + (20-20)^2 + (30-20)^2)/3 = 200/3 = 66.6666... */
    histo_fill(h_exact, 10.0);
    histo_fill(h_exact, 20.0);
    histo_fill(h_exact, 30.0);

    double mean = 0.0, var = 0.0, std_dev = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h_exact, &mean));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 20.0, mean);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_variance(h_exact, &var));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 200.0 / 3.0, var);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_std_dev(h_exact, &std_dev));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, sqrt(200.0 / 3.0), std_dev);

    histo_destroy(h_exact);

    /* Test variance floor with identical samples */
    histo_t *h_ident = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_EXACT_MOMENTS);
    for (int i = 0; i < 50; ++i) {
        histo_fill(h_ident, 42.123456789);
    }
    TEST_ASSERT_EQUAL(HISTO_OK, histo_variance(h_ident, &var));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, var);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_std_dev(h_ident, &std_dev));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, std_dev);
    histo_destroy(h_ident);
}

/* Test quantile and median estimation without empty-bin distortion */
void test_quantile_and_median(void) {
    /* Create histogram with 10 bins on [0, 100] */
    /* Bins 0..3 are EMPTY ([0, 40)) */
    /* Data in bin 4 ([40, 50)) with weight 50 */
    /* Data in bin 5 ([50, 60)) with weight 50 */
    /* Bins 6..9 are EMPTY ([60, 100)) */
    histo_t *h = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    histo_fill_bin(h, 4, 50.0);
    histo_fill_bin(h, 5, 50.0);

    double q = 0.0;

    /* p = 0.0 must return lower edge of first non-empty bin (40.0), NOT 0.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h, 0.0, &q));
    TEST_ASSERT_EQUAL_DOUBLE(40.0, q);

    /* p = 1.0 must return upper edge of last non-empty bin (60.0), NOT 100.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h, 1.0, &q));
    TEST_ASSERT_EQUAL_DOUBLE(60.0, q);

    /* Median (p = 0.5) must be exact boundary between bin 4 and 5: 50.0 */
    double median = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_median(h, &median));
    TEST_ASSERT_EQUAL_DOUBLE(50.0, median);

    /* 25th percentile (halfway through bin 4: [40, 50]) -> 45.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h, 0.25, &q));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 45.0, q);

    /* 75th percentile (halfway through bin 5: [50, 60]) -> 55.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_quantile(h, 0.75, &q));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 55.0, q);

    /* Empty histogram error check */
    histo_t *h_empty = histo_create_uniform(5, 0.0, 10.0, HISTO_FLAG_NONE);
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_median(h_empty, &median));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_mean(h_empty, &q));
    histo_destroy(h_empty);

    histo_destroy(h);
}

/* Test get_stats summary structure */
void test_get_stats(void) {
    histo_t *h = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_EXACT_MOMENTS);
    histo_fill(h, 20.0);
    histo_fill(h, 40.0);
    histo_fill(h, 60.0);

    histo_stats_t stats;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_get_stats(h, &stats));
    TEST_ASSERT_EQUAL_UINT64(3, stats.n_entries);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, stats.total_weight);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 40.0, stats.mean);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 20.0, stats.min);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 60.0, stats.max);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 45.0, stats.median);

    histo_destroy(h);
}


/* Test higher-order moments: central moments, skewness, kurtosis, excess kurtosis */
void test_higher_order_moments(void) {
    /* 1. Symmetric uniform distribution on [-10, 10] (10 bins) */
    histo_t *h_sym = histo_create_uniform(10, -10.0, 10.0, HISTO_FLAG_NONE);
    for (uint32_t i = 0; i < 10; ++i) {
        histo_fill_bin(h_sym, i, 5.0); /* equal weights */
    }

    double m0 = 0.0, m1 = 0.0, m2 = 0.0, m3 = 0.0, m4 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_central_moment(h_sym, 0, &m0));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m0);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_central_moment(h_sym, 1, &m1));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m1);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_central_moment(h_sym, 2, &m2));
    double var = 0.0;
    histo_variance(h_sym, &var);
    TEST_ASSERT_EQUAL_DOUBLE(var, m2);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_central_moment(h_sym, 3, &m3));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, m3); /* symmetric -> M3 = 0 */

    TEST_ASSERT_EQUAL(HISTO_OK, histo_central_moment(h_sym, 4, &m4));
    TEST_ASSERT_TRUE(m4 > 0.0);

    double skew = 0.0, kurt = 0.0, exc_kurt = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_skewness(h_sym, &skew));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, skew);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_kurtosis(h_sym, &kurt));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_excess_kurtosis(h_sym, &exc_kurt));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, kurt - 3.0, exc_kurt);

    histo_destroy(h_sym);

    /* 2. Asymmetric (right-skewed) distribution */
    histo_t *h_skew_right = histo_create_uniform(5, 0.0, 50.0, HISTO_FLAG_NONE);
    /* Bins: [0,10): 50, [10,20): 25, [20,30): 10, [30,40): 5, [40,50): 1 */
    histo_fill_bin(h_skew_right, 0, 50.0);
    histo_fill_bin(h_skew_right, 1, 25.0);
    histo_fill_bin(h_skew_right, 2, 10.0);
    histo_fill_bin(h_skew_right, 3, 5.0);
    histo_fill_bin(h_skew_right, 4, 1.0);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_skewness(h_skew_right, &skew));
    TEST_ASSERT_TRUE(skew > 0.0); /* Right-tailed -> positive skewness */
    histo_destroy(h_skew_right);

    /* 3. Left-skewed distribution */
    histo_t *h_skew_left = histo_create_uniform(5, 0.0, 50.0, HISTO_FLAG_NONE);
    histo_fill_bin(h_skew_left, 0, 1.0);
    histo_fill_bin(h_skew_left, 1, 5.0);
    histo_fill_bin(h_skew_left, 2, 10.0);
    histo_fill_bin(h_skew_left, 3, 25.0);
    histo_fill_bin(h_skew_left, 4, 50.0);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_skewness(h_skew_left, &skew));
    TEST_ASSERT_TRUE(skew < 0.0); /* Left-tailed -> negative skewness */
    histo_destroy(h_skew_left);

    /* 4. Edge cases: Empty histogram and zero variance single spike */
    histo_t *h_empty = histo_create_uniform(5, 0.0, 10.0, HISTO_FLAG_NONE);
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_central_moment(h_empty, 3, &m3));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_skewness(h_empty, &skew));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_kurtosis(h_empty, &kurt));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_excess_kurtosis(h_empty, &exc_kurt));
    histo_destroy(h_empty);

    histo_t *h_spike = histo_create_uniform(5, 0.0, 10.0, HISTO_FLAG_NONE);
    histo_fill_bin(h_spike, 2, 10.0); /* All weight in one bin -> variance = 0 */
    TEST_ASSERT_EQUAL(HISTO_ERR_DIV_BY_ZERO, histo_skewness(h_spike, &skew));
    TEST_ASSERT_EQUAL(HISTO_ERR_DIV_BY_ZERO, histo_kurtosis(h_spike, &kurt));
    TEST_ASSERT_EQUAL(HISTO_ERR_DIV_BY_ZERO, histo_excess_kurtosis(h_spike, &exc_kurt));
    histo_destroy(h_spike);

    /* 5. NULL pointer robustness */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_central_moment(NULL, 2, &m2));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_skewness(NULL, &skew));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_kurtosis(NULL, &kurt));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_excess_kurtosis(NULL, &exc_kurt));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bin_queries);
    RUN_TEST(test_integral);
    RUN_TEST(test_moments);
    RUN_TEST(test_higher_order_moments);
    RUN_TEST(test_quantile_and_median);
    RUN_TEST(test_get_stats);
    return UNITY_END();
}
