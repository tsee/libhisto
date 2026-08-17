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

/* Test peak, mode, FWHM, and RMS estimation */
void test_mode_fwhm_rms(void) {
    /* 1. Standard symmetric Gaussian-like peak on [0, 100] (10 bins: binsize 10) */
    /* Bin 4 [40, 50): 10, Bin 5 [50, 60): 30 (peak), Bin 6 [60, 70): 10 */
    histo_t *h = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);
    histo_fill_bin(h, 4, 10.0);
    histo_fill_bin(h, 5, 30.0);
    histo_fill_bin(h, 6, 10.0);

    uint32_t mode_idx = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mode_bin(h, &mode_idx));
    TEST_ASSERT_EQUAL_UINT32(5, mode_idx);

    double mode_cont = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mode_continuous(h, &mode_cont));
    /* Symmetric neighbors (10 and 10) -> vertex is exactly bin 5 center: 55.0 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 55.0, mode_cont);

    double fwhm = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fwhm(h, &fwhm));
    /* Peak = 30 -> Half max = 15.
     * Left side: between bin 4 (10) and bin 5 (30): frac = (15-10)/(30-10) = 5/20 = 0.25 -> x_left = 45 + 0.25*(55-45) = 47.5
     * Right side: between bin 5 (30) and bin 6 (10): frac = (30-15)/(30-10) = 15/20 = 0.75 -> x_right = 55 + 0.75*(65-55) = 62.5
     * FWHM = 62.5 - 47.5 = 15.0
     */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 15.0, fwhm);

    double rms = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_rms(h, &rms));
    TEST_ASSERT_TRUE(rms > 50.0 && rms < 60.0);

    histo_destroy(h);

    /* 2. Asymmetric peak: Bin 4 (20), Bin 5 (30), Bin 6 (10) */
    /* Vertex shifted left towards bin 4 */
    histo_t *h_asym = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);
    histo_fill_bin(h_asym, 4, 20.0);
    histo_fill_bin(h_asym, 5, 30.0);
    histo_fill_bin(h_asym, 6, 10.0);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_mode_continuous(h_asym, &mode_cont));
    /* y_prev=20, y_curr=30, y_next=10 -> denom = 60-20-10 = 30
     * delta = 0.5 * (10 - 20) / 30 = -5/30 = -1/6
     * mode_cont = 55.0 + (-1/6)*10.0 = 55.0 - 1.66666... = 53.333333333333336
     */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 55.0 - 10.0 / 6.0, mode_cont);
    histo_destroy(h_asym);

    /* 3. Boundary mode bin (Bin 0 is peak) */
    histo_t *h_bound = histo_create_uniform(5, 0.0, 50.0, HISTO_FLAG_NONE);
    histo_fill_bin(h_bound, 0, 100.0);
    histo_fill_bin(h_bound, 1, 10.0);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mode_bin(h_bound, &mode_idx));
    TEST_ASSERT_EQUAL_UINT32(0, mode_idx);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mode_continuous(h_bound, &mode_cont));
    TEST_ASSERT_EQUAL_DOUBLE(5.0, mode_cont); /* returns bin 0 center */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fwhm(h_bound, &fwhm));
    TEST_ASSERT_TRUE(fwhm > 0.0);
    histo_destroy(h_bound);

    /* 4. Empty and single-spike edge cases */
    histo_t *h_empty = histo_create_uniform(5, 0.0, 10.0, HISTO_FLAG_NONE);
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_mode_bin(h_empty, &mode_idx));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_mode_continuous(h_empty, &mode_cont));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_fwhm(h_empty, &fwhm));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_rms(h_empty, &rms));
    histo_destroy(h_empty);

    /* 5. NULL pointer robustness */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_mode_bin(NULL, &mode_idx));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_mode_continuous(NULL, &mode_cont));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_fwhm(NULL, &fwhm));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_rms(NULL, &rms));
}

/* Test robust dispersion: IQR, MAD, trimmed mean, Winsorized mean */
void test_robust_dispersion(void) {
    /* 1. Uniform histogram on [0, 100] (4 bins: [0,25), [25,50), [50,75), [75,100), width=25) */
    /* Equal weight 10.0 in each bin -> Q25 = 25.0, Q75 = 75.0, Median = 50.0, Mean = 50.0 */
    histo_t *h = histo_create_uniform(4, 0.0, 100.0, HISTO_FLAG_NONE);
    for (uint32_t i = 0; i < 4; ++i) {
        histo_fill_bin(h, i, 10.0);
    }

    double iqr = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_iqr(h, &iqr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 50.0, iqr); /* 75 - 25 = 50 */

    double mad = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mad(h, &mad));
    /* Centers: 12.5, 37.5, 62.5, 87.5. Distances from 50: 37.5, 12.5, 12.5, 37.5. Sorted: 12.5, 12.5, 37.5, 37.5.
     * Median of distances = 12.5 or 37.5 -> 12.5 reached at cumulative weight 20 (>= target 20)
     */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 12.5, mad);

    double t_mean = 0.0;
    /* Trim outer 25% on both sides -> only middle 50% retained (bins 1 and 2: [25, 75)) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_trimmed_mean(h, 0.25, 0.75, &t_mean));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 50.0, t_mean);

    double w_mean = 0.0;
    /* Winsorized mean at 25% and 75% -> clamps to [25, 75] */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_winsorized_mean(h, 0.25, 0.75, &w_mean));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 50.0, w_mean);

    histo_destroy(h);

    /* 2. Asymmetric distribution with heavy outlier */
    histo_t *h_outlier = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);
    /* Ingest 90 items in bin 0 ([0, 10)), 10 items in bin 9 ([90, 100)) */
    histo_fill_bin(h_outlier, 0, 90.0);
    histo_fill_bin(h_outlier, 9, 10.0);

    double raw_mean = 0.0;
    histo_mean(h_outlier, &raw_mean);
    /* raw mean = (90*5 + 10*95)/100 = (450 + 950)/100 = 14.0 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 14.0, raw_mean);

    /* Trim upper 10% (p in [0.0, 0.90]) -> cuts off bin 9 completely, trimmed mean = 5.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_trimmed_mean(h_outlier, 0.0, 0.90, &t_mean));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, t_mean);

    histo_destroy(h_outlier);

    /* 3. Parameter bounds & Edge cases */
    histo_t *h_empty = histo_create_uniform(4, 0.0, 10.0, HISTO_FLAG_NONE);
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_iqr(h_empty, &iqr));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_mad(h_empty, &mad));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_trimmed_mean(h_empty, 0.1, 0.9, &t_mean));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_winsorized_mean(h_empty, 0.1, 0.9, &w_mean));

    /* Inverted or out-of-range percentiles */
    histo_t *h_valid = histo_create_uniform(4, 0.0, 10.0, HISTO_FLAG_NONE);
    histo_fill_bin(h_valid, 0, 1.0);
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo_trimmed_mean(h_valid, 0.8, 0.2, &t_mean));
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo_trimmed_mean(h_valid, -0.1, 0.5, &t_mean));
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo_trimmed_mean(h_valid, 0.2, 1.5, &t_mean));
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo_winsorized_mean(h_valid, 0.7, 0.3, &w_mean));
    histo_destroy(h_valid);
    histo_destroy(h_empty);

    /* 4. NULL pointers */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_iqr(NULL, &iqr));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_mad(NULL, &mad));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_trimmed_mean(NULL, 0.1, 0.9, &t_mean));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_winsorized_mean(NULL, 0.1, 0.9, &w_mean));
}

/* Test higher-order moments k=5, k=6 */
void test_moments_higher_order_extended(void) {
    histo_t *h = histo_create_uniform(5, -10.0, 10.0, HISTO_FLAG_NONE);
    /* Symmetric distribution */
    for (uint32_t i = 0; i < 5; ++i) {
        histo_fill_bin(h, i, 10.0);
    }

    double m5 = 0.0, m6 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_central_moment(h, 5, &m5));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, m5); /* Odd central moment of symmetric is 0 */

    TEST_ASSERT_EQUAL(HISTO_OK, histo_central_moment(h, 6, &m6));
    TEST_ASSERT_TRUE(m6 > 0.0); /* Even central moment is positive */

    histo_destroy(h);
}

/* Test trimmed and Winsorized mean edge cases: full range and single bin */
void test_trimmed_and_winsorized_mean_edge_cases(void) {
    histo_t *h = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_NONE);
    histo_fill_bin(h, 0, 10.0);
    histo_fill_bin(h, 1, 20.0);
    histo_fill_bin(h, 2, 30.0);
    histo_fill_bin(h, 3, 40.0);

    double t_mean = 0.0, w_mean = 0.0, std_mean = 0.0;
    histo_mean(h, &std_mean);

    /* [0.0, 1.0] full range trim and winsorize should equal standard mean */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_trimmed_mean(h, 0.0, 1.0, &t_mean));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, std_mean, t_mean);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_winsorized_mean(h, 0.0, 1.0, &w_mean));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, std_mean, w_mean);

    /* Single bin filled */
    histo_t *h_single = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);
    histo_fill_bin(h_single, 5, 100.0); /* center 55.0 */

    TEST_ASSERT_EQUAL(HISTO_OK, histo_trimmed_mean(h_single, 0.1, 0.9, &t_mean));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 55.0, t_mean);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_winsorized_mean(h_single, 0.1, 0.9, &w_mean));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 55.0, w_mean);

    histo_destroy(h);
    histo_destroy(h_single);
}

/* Test mode continuous on variable bin histogram returns bin center */
void test_mode_continuous_variable_bins(void) {
    const double edges[] = {0.0, 2.0, 10.0, 50.0};
    histo_t *h = histo_create_variable(3, edges, HISTO_FLAG_NONE);
    histo_fill_bin(h, 1, 100.0); /* Bin 1 [2.0, 10.0) center 6.0 is dominant mode */

    double mode = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mode_continuous(h, &mode));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 6.0, mode);

    histo_destroy(h);
}

/* Test MAD with large bin count (> 256) exercising dynamic buffer allocation */
void test_mad_large_bins(void) {
    const uint32_t nbins = 500;
    histo_t *h = histo_create_uniform(nbins, 0.0, 500.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    /* Symmetrically fill bins around center 250.0 */
    for (uint32_t i = 0; i < nbins; ++i) {
        histo_fill_bin(h, i, 1.0);
    }

    double mad = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mad(h, &mad));
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 125.0, mad);

    histo_destroy(h);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bin_queries);
    RUN_TEST(test_integral);
    RUN_TEST(test_moments);
    RUN_TEST(test_higher_order_moments);
    RUN_TEST(test_mode_fwhm_rms);
    RUN_TEST(test_robust_dispersion);
    RUN_TEST(test_quantile_and_median);
    RUN_TEST(test_get_stats);
    RUN_TEST(test_moments_higher_order_extended);
    RUN_TEST(test_trimmed_and_winsorized_mean_edge_cases);
    RUN_TEST(test_mode_continuous_variable_bins);
    RUN_TEST(test_mad_large_bins);
    return UNITY_END();
}
