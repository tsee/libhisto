/*
 * Unit tests for histogram arithmetic (+, -, *, /) and scalar linear scaling.
 */

#include "unity.h"
#include "histo/histo.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

/* Test addition and subtraction with uncertainty propagation */
void test_add_subtract(void) {
    histo_t *h1 = histo_create_uniform(3, 0.0, 30.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    histo_t *h2 = histo_create_uniform(3, 0.0, 30.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h1);
    TEST_ASSERT_NOT_NULL(h2);

    /* Fill h1: bin 0 with weight 10 (sum_w2 = 100), bin 1 with 20 (sum_w2 = 400) */
    histo_fill_w(h1, 5.0, 10.0);
    histo_fill_w(h1, 15.0, 20.0);

    /* Fill h2: bin 0 with weight 5 (sum_w2 = 25), bin 1 with 10 (sum_w2 = 100) */
    histo_fill_w(h2, 5.0, 5.0);
    histo_fill_w(h2, 15.0, 10.0);

    /* Add h2 into h1 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_add(h1, h2));
    TEST_ASSERT_EQUAL_DOUBLE(45.0, histo_total_weight(h1));

    double content = 0.0, err = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h1, 0, &content));
    TEST_ASSERT_EQUAL_DOUBLE(15.0, content);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h1, 0, &err));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, sqrt(125.0), err); /* 100 + 25 = 125 */

    /* Subtract h2 from h1 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_subtract(h1, h2));
    TEST_ASSERT_EQUAL_DOUBLE(30.0, histo_total_weight(h1));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h1, 0, &content));
    TEST_ASSERT_EQUAL_DOUBLE(10.0, content);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h1, 0, &err));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, sqrt(150.0), err); /* 125 + 25 = 150 (quadrature) */

    /* Incompatible addition test */
    histo_t *h_incompat = histo_create_uniform(4, 0.0, 30.0, HISTO_FLAG_NONE);
    TEST_ASSERT_EQUAL(HISTO_ERR_INCOMPATIBLE, histo_add(h1, h_incompat));
    histo_destroy(h_incompat);

    histo_destroy(h1);
    histo_destroy(h2);
}

/* Test division-free multiplication and division error propagation with empty bins */
void test_multiply_divide(void) {
    histo_t *h1 = histo_create_uniform(3, 0.0, 30.0, HISTO_FLAG_TRACK_SUMW2);
    histo_t *h2 = histo_create_uniform(3, 0.0, 30.0, HISTO_FLAG_TRACK_SUMW2);

    /* Bin 0: non-empty in both */
    histo_fill_w(h1, 5.0, 4.0);  /* bin 0 content 4, sum_w2 16 */
    histo_fill_w(h2, 5.0, 2.0);  /* bin 0 content 2, sum_w2 4 */

    /* Bin 1: empty in h2 */
    histo_fill_w(h1, 15.0, 5.0);

    /* Bin 2: empty in both */

    /* Multiply h2 into h1 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_multiply(h1, h2));

    double content = 0.0, err = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h1, 0, &content));
    TEST_ASSERT_EQUAL_DOUBLE(8.0, content); /* 4 * 2 = 8 */

    /* Bin 0 error: sqrt( h2^2 * w1^2 + h1^2 * w2^2 ) = sqrt( 4*16 + 16*4 ) = sqrt(128) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h1, 0, &err));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, sqrt(128.0), err);

    /* Bin 1 content: 5 * 0 = 0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h1, 1, &content));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, content);
    /* Bin 1 error: 0.0 without NaN */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h1, 1, &err));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, err);

    /* Bin 2 error: 0.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h1, 2, &err));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, err);

    histo_destroy(h1);
    histo_destroy(h2);
}

/* Test scaling and normalization */
void test_scale_normalize(void) {
    histo_t *h = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_TRACK_SUMW2);
    histo_fill_w(h, 5.0, 10.0);
    histo_fill_w(h, 15.0, 30.0);

    TEST_ASSERT_EQUAL_DOUBLE(40.0, histo_total_weight(h));

    /* Scale by 2.5 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_scale(h, 2.5));
    TEST_ASSERT_EQUAL_DOUBLE(100.0, histo_total_weight(h));

    double content = 0.0, err = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 0, &content));
    TEST_ASSERT_EQUAL_DOUBLE(25.0, content);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h, 0, &err));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 25.0, err); /* 10 * 2.5 = 25 */

    /* Normalize to 1.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_normalize(h, 1.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, histo_total_weight(h));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 0, &content));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.25, content);

    /* Invalid normalization */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_normalize(h, -1.0));
    TEST_ASSERT_EQUAL(HISTO_ERR_NON_FINITE, histo_scale(h, NAN));

    histo_destroy(h);
}

/* Test rebinning */
void test_rebin(void) {
    histo_t *src = histo_create_uniform(6, 0.0, 60.0, HISTO_FLAG_TRACK_SUMW2);
    for (uint32_t i = 0; i < 6; ++i) {
        histo_fill_bin(src, i, 10.0);
    }

    /* Rebin by factor 2 -> 3 bins */
    histo_t *rebinned = histo_rebin(src, 2);
    TEST_ASSERT_NOT_NULL(rebinned);
    TEST_ASSERT_EQUAL_UINT32(3, histo_nbins(rebinned));
    TEST_ASSERT_EQUAL_DOUBLE(60.0, histo_total_weight(rebinned));

    double c0 = 0.0, c1 = 0.0, c2 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(rebinned, 0, &c0));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(rebinned, 1, &c1));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(rebinned, 2, &c2));
    TEST_ASSERT_EQUAL_DOUBLE(20.0, c0);
    TEST_ASSERT_EQUAL_DOUBLE(20.0, c1);
    TEST_ASSERT_EQUAL_DOUBLE(20.0, c2);

    /* Invalid rebin factor */
    TEST_ASSERT_NULL(histo_rebin(src, 4)); /* 6 not divisible by 4 */
    TEST_ASSERT_NULL(histo_rebin(src, 0));

    histo_destroy(src);
    histo_destroy(rebinned);
}

/* Test slicing */
void test_slice(void) {
    histo_t *src = histo_create_uniform(5, 0.0, 50.0, HISTO_FLAG_TRACK_SUMW2);
    for (uint32_t i = 0; i < 5; ++i) {
        histo_fill_bin(src, i, 10.0);
    }

    /* Slice bins [1, 3] -> range [10, 40] with 3 bins */
    histo_t *sliced = histo_slice(src, 1, 3, false);
    TEST_ASSERT_NOT_NULL(sliced);
    TEST_ASSERT_EQUAL_UINT32(3, histo_nbins(sliced));
    TEST_ASSERT_EQUAL_DOUBLE(30.0, histo_total_weight(sliced));
    TEST_ASSERT_EQUAL_DOUBLE(10.0, histo_underflow(sliced)); /* bin 0 became underflow */
    TEST_ASSERT_EQUAL_DOUBLE(10.0, histo_overflow(sliced));  /* bin 4 became overflow */

    double min_v = 0.0, max_v = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_range(sliced, &min_v, &max_v));
    TEST_ASSERT_EQUAL_DOUBLE(10.0, min_v);
    TEST_ASSERT_EQUAL_DOUBLE(40.0, max_v);

    histo_destroy(src);
    histo_destroy(sliced);
}

/* Test CDF generation */
void test_cdf(void) {
    histo_t *src = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_NONE);
    histo_fill_bin(src, 0, 10.0);
    histo_fill_bin(src, 1, 20.0);
    histo_fill_bin(src, 2, 30.0);
    histo_fill_bin(src, 3, 40.0);

    /* Default CDF normalized to 1.0 */
    histo_t *cdf = histo_cdf(src, 1.0);
    TEST_ASSERT_NOT_NULL(cdf);

    double c0 = 0.0, c1 = 0.0, c2 = 0.0, c3 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(cdf, 0, &c0));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(cdf, 1, &c1));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(cdf, 2, &c2));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(cdf, 3, &c3));

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.1, c0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.3, c1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.6, c2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, c3);

    histo_destroy(src);
    histo_destroy(cdf);
}

/* Test scale with exact moments flag maintains mean and variance */
void test_scale_exact_moments_invariance(void) {
    histo_t *h = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);

    histo_fill(h, 20.0);
    histo_fill(h, 40.0);
    histo_fill(h, 60.0);

    double orig_mean = 0.0, orig_var = 0.0, orig_std = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h, &orig_mean));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_variance(h, &orig_var));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_std_dev(h, &orig_std));

    /* Scale by 3.5 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_scale(h, 3.5));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.5, histo_total_weight(h));

    double scaled_mean = 0.0, scaled_var = 0.0, scaled_std = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h, &scaled_mean));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_variance(h, &scaled_var));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_std_dev(h, &scaled_std));

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, orig_mean, scaled_mean);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, orig_var, scaled_var);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, orig_std, scaled_std);

    /* Scale by 0.1 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_scale(h, 0.1));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h, &scaled_mean));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_variance(h, &scaled_var));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, orig_mean, scaled_mean);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, orig_var, scaled_var);

    histo_destroy(h);
}

/* Test multiply and divide with exact moments recalculation */
void test_multiply_divide_exact_moments(void) {
    histo_t *h1 = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_EXACT_MOMENTS);
    histo_t *h2 = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_EXACT_MOMENTS);

    /* h1: bin 0 (center 5) = 2, bin 1 (center 15) = 4 */
    /* h2: bin 0 (center 5) = 3, bin 1 (center 15) = 2 */
    histo_fill_bin(h1, 0, 2.0);
    histo_fill_bin(h1, 1, 4.0);
    histo_fill_bin(h2, 0, 3.0);
    histo_fill_bin(h2, 1, 2.0);

    /* Multiply h2 into h1 -> bin 0 = 6, bin 1 = 8 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_multiply(h1, h2));

    double mean = 0.0, var = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h1, &mean));
    /* mean = (6*5 + 8*15) / 14 = (30 + 120) / 14 = 150 / 14 = 10.7142857... */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 150.0 / 14.0, mean);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_variance(h1, &var));
    TEST_ASSERT_TRUE(var > 0.0);

    /* Divide h1 by h2 -> bin 0 = 6/3 = 2, bin 1 = 8/2 = 4 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_divide(h1, h2));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h1, &mean));
    /* mean = (2*5 + 4*15) / 6 = 70 / 6 = 11.66666... */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 70.0 / 6.0, mean);

    histo_destroy(h1);
    histo_destroy(h2);
}

/* Test variable bin rebinning */
void test_rebin_variable_bins(void) {
    const double edges[] = {0.0, 1.0, 3.0, 7.0, 15.0, 31.0, 63.0}; /* 6 bins */
    histo_t *src = histo_create_variable(6, edges, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(src);

    for (uint32_t i = 0; i < 6; ++i) {
        histo_fill_bin(src, i, 5.0);
    }

    /* Rebin factor 2 -> 3 variable bins: [0, 3], [3, 15], [15, 63] */
    histo_t *rebinned = histo_rebin(src, 2);
    TEST_ASSERT_NOT_NULL(rebinned);
    TEST_ASSERT_EQUAL_UINT32(3, histo_nbins(rebinned));
    TEST_ASSERT_EQUAL(HISTO_BIN_VARIABLE, histo_bin_type(rebinned));

    double low = 0.0, high = 0.0, content = 0.0, sumw2 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_bounds(rebinned, 0, &low, &high));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, low);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, high);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(rebinned, 0, &content));
    TEST_ASSERT_EQUAL_DOUBLE(10.0, content);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_sum_w2(rebinned, 0, &sumw2));
    TEST_ASSERT_EQUAL_DOUBLE(50.0, sumw2); /* 5^2 + 5^2 = 50 */

    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_bounds(rebinned, 1, &low, &high));
    TEST_ASSERT_EQUAL_DOUBLE(3.0, low);
    TEST_ASSERT_EQUAL_DOUBLE(15.0, high);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_bounds(rebinned, 2, &low, &high));
    TEST_ASSERT_EQUAL_DOUBLE(15.0, low);
    TEST_ASSERT_EQUAL_DOUBLE(63.0, high);

    histo_destroy(src);
    histo_destroy(rebinned);
}

/* Test variable bin slicing */
void test_slice_variable_bins(void) {
    const double edges[] = {0.0, 5.0, 15.0, 30.0, 50.0}; /* 4 bins */
    histo_t *src = histo_create_variable(4, edges, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(src);

    histo_fill_bin(src, 0, 10.0); /* [0, 5) -> will become underflow */
    histo_fill_bin(src, 1, 20.0); /* [5, 15) -> bin 0 of slice */
    histo_fill_bin(src, 2, 30.0); /* [15, 30) -> bin 1 of slice */
    histo_fill_bin(src, 3, 40.0); /* [30, 50) -> will become overflow */

    /* Slice bins [1, 2] -> range [5, 30] with 2 variable bins */
    histo_t *sliced = histo_slice(src, 1, 2, false);
    TEST_ASSERT_NOT_NULL(sliced);
    TEST_ASSERT_EQUAL_UINT32(2, histo_nbins(sliced));
    TEST_ASSERT_EQUAL(HISTO_BIN_VARIABLE, histo_bin_type(sliced));
    TEST_ASSERT_EQUAL_DOUBLE(50.0, histo_total_weight(sliced));
    TEST_ASSERT_EQUAL_DOUBLE(10.0, histo_underflow(sliced));
    TEST_ASSERT_EQUAL_DOUBLE(40.0, histo_overflow(sliced));

    double low = 0.0, high = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_bounds(sliced, 0, &low, &high));
    TEST_ASSERT_EQUAL_DOUBLE(5.0, low);
    TEST_ASSERT_EQUAL_DOUBLE(15.0, high);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_bounds(sliced, 1, &low, &high));
    TEST_ASSERT_EQUAL_DOUBLE(15.0, low);
    TEST_ASSERT_EQUAL_DOUBLE(30.0, high);

    histo_destroy(src);
    histo_destroy(sliced);
}

/* Test CDF on variable bins and prenormalization parameter */
void test_cdf_variable_and_prenormalization(void) {
    const double edges[] = {0.0, 10.0, 30.0, 100.0};
    histo_t *src = histo_create_variable(3, edges, HISTO_FLAG_NONE);
    histo_fill_bin(src, 0, 10.0);
    histo_fill_bin(src, 1, 30.0);
    histo_fill_bin(src, 2, 60.0);

    /* Prenormalize to 100.0 (percentage) */
    histo_t *cdf_pct = histo_cdf(src, 100.0);
    TEST_ASSERT_NOT_NULL(cdf_pct);
    TEST_ASSERT_EQUAL(HISTO_BIN_VARIABLE, histo_bin_type(cdf_pct));

    double c0 = 0.0, c1 = 0.0, c2 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(cdf_pct, 0, &c0));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(cdf_pct, 1, &c1));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(cdf_pct, 2, &c2));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.0, c0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 40.0, c1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 100.0, c2);

    /* Non-positive prenormalization defaults to 1.0 */
    histo_t *cdf_def = histo_cdf(src, 0.0);
    TEST_ASSERT_NOT_NULL(cdf_def);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(cdf_def, 2, &c2));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, c2);

    /* NULL source */
    TEST_ASSERT_NULL(histo_cdf(NULL, 1.0));

    histo_destroy(src);
    histo_destroy(cdf_pct);
    histo_destroy(cdf_def);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_add_subtract);
    RUN_TEST(test_multiply_divide);
    RUN_TEST(test_scale_normalize);
    RUN_TEST(test_rebin);
    RUN_TEST(test_slice);
    RUN_TEST(test_cdf);
    RUN_TEST(test_scale_exact_moments_invariance);
    RUN_TEST(test_multiply_divide_exact_moments);
    RUN_TEST(test_rebin_variable_bins);
    RUN_TEST(test_slice_variable_bins);
    RUN_TEST(test_cdf_variable_and_prenormalization);
    return UNITY_END();
}
