#include "unity.h"
#include "histo/histo.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

/* Test uniform histogram creation with valid parameters */
void test_create_uniform_valid(void) {
    histo_t *h = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_EQUAL_UINT32(10, histo_nbins(h));
    TEST_ASSERT_EQUAL(HISTO_BIN_UNIFORM, histo_bin_type(h));

    double min_val = 0.0, max_val = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_range(h, &min_val, &max_val));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, min_val);
    TEST_ASSERT_EQUAL_DOUBLE(100.0, max_val);

    histo_destroy(h);
}

/* Test uniform histogram creation edge cases and invalid parameters */
void test_create_uniform_invalid_params(void) {
    /* Zero bins */
    TEST_ASSERT_NULL(histo_create_uniform(0, 0.0, 10.0, HISTO_FLAG_NONE));

    /* Exceeds maximum allowed bins */
    TEST_ASSERT_NULL(histo_create_uniform(HISTO_MAX_NBINS + 1, 0.0, 10.0, HISTO_FLAG_NONE));

    /* min == max */
    TEST_ASSERT_NULL(histo_create_uniform(10, 5.0, 5.0, HISTO_FLAG_NONE));

    /* min > max (inverted range) */
    TEST_ASSERT_NULL(histo_create_uniform(10, 10.0, 0.0, HISTO_FLAG_NONE));

    /* Non-finite coordinates */
    TEST_ASSERT_NULL(histo_create_uniform(10, NAN, 10.0, HISTO_FLAG_NONE));
    TEST_ASSERT_NULL(histo_create_uniform(10, 0.0, NAN, HISTO_FLAG_NONE));
    TEST_ASSERT_NULL(histo_create_uniform(10, -INFINITY, 10.0, HISTO_FLAG_NONE));
    TEST_ASSERT_NULL(histo_create_uniform(10, 0.0, INFINITY, HISTO_FLAG_NONE));

    /* Extreme range overflow (max - min) overflows to +Inf */
    TEST_ASSERT_NULL(histo_create_uniform(10, -1.0e308, 1.0e308, HISTO_FLAG_NONE));
}

/* Test variable histogram creation with valid monotonic edges */
void test_create_variable_valid(void) {
    const double edges[] = {-10.0, -2.5, 0.0, 5.0, 100.0};
    const uint32_t nbins = 4;

    histo_t *h = histo_create_variable(nbins, edges, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_EQUAL_UINT32(4, histo_nbins(h));
    TEST_ASSERT_EQUAL(HISTO_BIN_VARIABLE, histo_bin_type(h));

    double min_val = 0.0, max_val = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_range(h, &min_val, &max_val));
    TEST_ASSERT_EQUAL_DOUBLE(-10.0, min_val);
    TEST_ASSERT_EQUAL_DOUBLE(100.0, max_val);

    histo_destroy(h);
}

/* Test variable histogram creation invalid and non-monotonic edges */
void test_create_variable_invalid_params(void) {
    /* NULL edges */
    TEST_ASSERT_NULL(histo_create_variable(5, NULL, HISTO_FLAG_NONE));

    /* Zero bins */
    const double edges_valid[] = {0.0, 1.0};
    TEST_ASSERT_NULL(histo_create_variable(0, edges_valid, HISTO_FLAG_NONE));

    /* Non-monotonic: duplicate edge */
    const double edges_dup[] = {0.0, 1.0, 1.0, 3.0};
    TEST_ASSERT_NULL(histo_create_variable(3, edges_dup, HISTO_FLAG_NONE));

    /* Non-monotonic: decreasing edge */
    const double edges_dec[] = {0.0, 5.0, 4.0, 10.0};
    TEST_ASSERT_NULL(histo_create_variable(3, edges_dec, HISTO_FLAG_NONE));

    /* Non-finite edge values */
    const double edges_nan[] = {0.0, NAN, 10.0};
    TEST_ASSERT_NULL(histo_create_variable(2, edges_nan, HISTO_FLAG_NONE));

    const double edges_inf[] = {-INFINITY, 0.0, 10.0};
    TEST_ASSERT_NULL(histo_create_variable(2, edges_inf, HISTO_FLAG_NONE));
}

/* Test destroy on NULL is safe */
void test_destroy_null(void) {
    histo_destroy(NULL); /* Should not crash */
}

/* Test cloning uniform and variable histograms */
void test_clone_and_empty_clone(void) {
    /* Uniform clone */
    histo_t *u = histo_create_uniform(5, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(u);

    histo_t *u_clone = histo_clone(u, false);
    TEST_ASSERT_NOT_NULL(u_clone);
    TEST_ASSERT_EQUAL_UINT32(5, histo_nbins(u_clone));
    TEST_ASSERT_EQUAL(HISTO_BIN_UNIFORM, histo_bin_type(u_clone));

    histo_t *u_empty = histo_clone(u, true);
    TEST_ASSERT_NOT_NULL(u_empty);
    TEST_ASSERT_EQUAL_UINT32(5, histo_nbins(u_empty));

    histo_destroy(u);
    histo_destroy(u_clone);
    histo_destroy(u_empty);

    /* Variable clone */
    const double edges[] = {1.0, 2.0, 4.0, 8.0};
    histo_t *v = histo_create_variable(3, edges, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(v);

    histo_t *v_clone = histo_clone(v, false);
    TEST_ASSERT_NOT_NULL(v_clone);
    TEST_ASSERT_EQUAL_UINT32(3, histo_nbins(v_clone));
    TEST_ASSERT_EQUAL(HISTO_BIN_VARIABLE, histo_bin_type(v_clone));

    histo_destroy(v);
    histo_destroy(v_clone);

    /* NULL clone */
    TEST_ASSERT_NULL(histo_clone(NULL, false));
}

/* Test reset function */
void test_reset(void) {
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_reset(NULL));

    histo_t *h = histo_create_uniform(10, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_reset(h));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, histo_total_weight(h));
    TEST_ASSERT_EQUAL_UINT64(0, histo_num_entries(h));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, histo_underflow(h));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, histo_overflow(h));
    TEST_ASSERT_EQUAL_UINT64(0, histo_nan_count(h));

    histo_destroy(h);
}

/* Test status string lookup */
void test_status_strings(void) {
    TEST_ASSERT_NOT_NULL(histo_status_str(HISTO_OK));
    TEST_ASSERT_NOT_NULL(histo_status_str(HISTO_ERR_INVALID_ARG));
    TEST_ASSERT_NOT_NULL(histo_status_str(HISTO_WARN_NON_FINITE));
    TEST_ASSERT_NOT_NULL(histo_status_str((histo_status_t)999));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_uniform_valid);
    RUN_TEST(test_create_uniform_invalid_params);
    RUN_TEST(test_create_variable_valid);
    RUN_TEST(test_create_variable_invalid_params);
    RUN_TEST(test_destroy_null);
    RUN_TEST(test_clone_and_empty_clone);
    RUN_TEST(test_reset);
    RUN_TEST(test_status_strings);
    return UNITY_END();
}
