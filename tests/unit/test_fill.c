#include "unity.h"
#include "histo/histo.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

/* Test uniform boundary-guarded bin lookups */
void test_uniform_find_bin_boundaries(void) {
    /* Range [0, 1] with 3 bins: [0, 1/3), [1/3, 2/3), [2/3, 1) */
    histo_t *h = histo_create_uniform(3, 0.0, 1.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    int64_t bin = -999;

    /* Lower boundary of bin 0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 0.0, &bin));
    TEST_ASSERT_EQUAL_INT64(0, bin);

    /* Inside bin 0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 0.2, &bin));
    TEST_ASSERT_EQUAL_INT64(0, bin);

    /* Exact lower boundary of bin 1: 1/3 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 1.0 / 3.0, &bin));
    TEST_ASSERT_EQUAL_INT64(1, bin);

    /* Exact lower boundary of bin 2: 2/3 (Tests precision truncation guard!) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 2.0 / 3.0, &bin));
    TEST_ASSERT_EQUAL_INT64(2, bin);

    /* Upper boundary of range: 1.0 -> Overflow (bin index = 3) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 1.0, &bin));
    TEST_ASSERT_EQUAL_INT64(3, bin);

    /* Out of range: Overflow */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 5.0, &bin));
    TEST_ASSERT_EQUAL_INT64(3, bin);

    /* Out of range: Underflow */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, -0.01, &bin));
    TEST_ASSERT_EQUAL_INT64(-1, bin);

    histo_destroy(h);
}

/* Test variable binary search bin lookups */
void test_variable_find_bin(void) {
    const double edges[] = {-10.0, -2.0, 0.0, 5.0, 100.0};
    histo_t *h = histo_create_variable(4, edges, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    int64_t bin = -999;

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, -10.0, &bin));
    TEST_ASSERT_EQUAL_INT64(0, bin);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, -2.0, &bin));
    TEST_ASSERT_EQUAL_INT64(1, bin);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 0.0, &bin));
    TEST_ASSERT_EQUAL_INT64(2, bin);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 5.0, &bin));
    TEST_ASSERT_EQUAL_INT64(3, bin);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 99.9, &bin));
    TEST_ASSERT_EQUAL_INT64(3, bin);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 100.0, &bin));
    TEST_ASSERT_EQUAL_INT64(4, bin); /* Overflow */

    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, -10.0001, &bin));
    TEST_ASSERT_EQUAL_INT64(-1, bin); /* Underflow */

    histo_destroy(h);
}

/* Test IEEE-754 edge cases: NaN, Inf, -0.0 */
void test_ieee754_edge_cases(void) {
    histo_t *h = histo_create_uniform(5, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);

    /* NaN value rejection */
    TEST_ASSERT_EQUAL(HISTO_ERR_NON_FINITE, histo_fill(h, NAN));
    TEST_ASSERT_EQUAL_UINT64(1, histo_nan_count(h));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, histo_total_weight(h));
    TEST_ASSERT_EQUAL_UINT64(0, histo_num_entries(h));

    /* NaN weight rejection */
    TEST_ASSERT_EQUAL(HISTO_ERR_NON_FINITE, histo_fill_w(h, 5.0, NAN));
    TEST_ASSERT_EQUAL_UINT64(2, histo_nan_count(h));

    /* +Inf routed to overflow */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_w(h, INFINITY, 2.5));
    TEST_ASSERT_EQUAL_DOUBLE(2.5, histo_overflow(h));

    /* -Inf routed to underflow */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_w(h, -INFINITY, 1.5));
    TEST_ASSERT_EQUAL_DOUBLE(1.5, histo_underflow(h));

    /* -0.0 treated identically to +0.0 (lands in bin 0) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill(h, -0.0));
    TEST_ASSERT_EQUAL_UINT64(1, histo_num_entries(h));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, histo_total_weight(h));

    int64_t bin = -1;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, -0.0, &bin));
    TEST_ASSERT_EQUAL_INT64(0, bin);

    histo_destroy(h);
}

/* Test direct fill by bin index */
void test_fill_bin(void) {
    histo_t *h = histo_create_uniform(4, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_bin(h, 0, 3.0));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_bin(h, 3, 2.0));
    TEST_ASSERT_EQUAL_DOUBLE(5.0, histo_total_weight(h));
    TEST_ASSERT_EQUAL_UINT64(2, histo_num_entries(h));

    /* Out of range bin index */
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo_fill_bin(h, 4, 1.0));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_fill_bin(NULL, 0, 1.0));

    histo_destroy(h);
}

/* Test strided batch ingestion */
typedef struct Sample {
    int id;
    double value;
    double weight;
    char tag[8];
} Sample;

void test_fill_strided_and_batch(void) {
    histo_t *h = histo_create_uniform(5, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    Sample data[4] = {
        {1, 1.0, 1.5, "A"},
        {2, 3.0, 2.0, "B"},
        {3, NAN, 1.0, "C"}, /* Embedded NaN */
        {4, 7.0, 0.5, "D"}
    };

    /* Ingest strided with embedded NaN */
    histo_status_t st = histo_fill_strided(
        h, 4,
        &data[0].value, sizeof(Sample),
        &data[0].weight, sizeof(Sample)
    );

    TEST_ASSERT_EQUAL(HISTO_WARN_NON_FINITE, st);
    TEST_ASSERT_EQUAL_UINT64(1, histo_nan_count(h));
    TEST_ASSERT_EQUAL_UINT64(3, histo_num_entries(h));
    TEST_ASSERT_EQUAL_DOUBLE(4.0, histo_total_weight(h)); /* 1.5 + 2.0 + 0.5 */

    /* Contiguous fill_n */
    double x_vals[3] = {0.5, 5.5, 9.5};
    double w_vals[3] = {1.0, 1.0, 1.0};
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_n(h, 3, x_vals, w_vals));
    TEST_ASSERT_EQUAL_UINT64(6, histo_num_entries(h));
    TEST_ASSERT_EQUAL_DOUBLE(7.0, histo_total_weight(h));

    histo_destroy(h);
}

/* Test fill_n with unit weights (weights == NULL) on histogram tracking sum_w2 */
void test_fill_n_sumw2_unit_weights(void) {
    histo_t *h = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    /* Samples: -5.0 (underflow), 5.0 (bin 0), 5.0 (bin 0), 15.0 (bin 1), 45.0 (overflow), NAN (rejected) */
    double x_vals[6] = {-5.0, 5.0, 5.0, 15.0, 45.0, NAN};
    histo_status_t st = histo_fill_n(h, 6, x_vals, NULL);
    TEST_ASSERT_EQUAL(HISTO_WARN_NON_FINITE, st);

    TEST_ASSERT_EQUAL_UINT64(1, histo_nan_count(h));
    TEST_ASSERT_EQUAL_UINT64(3, histo_num_entries(h));
    TEST_ASSERT_EQUAL_DOUBLE(3.0, histo_total_weight(h));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, histo_underflow(h));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, histo_overflow(h));

    double content0 = 0.0, sumw2_0 = 0.0, err0 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 0, &content0));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_sum_w2(h, 0, &sumw2_0));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h, 0, &err0));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, content0);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, sumw2_0); /* 1^2 + 1^2 = 2 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, sqrt(2.0), err0);

    double content1 = 0.0, sumw2_1 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 1, &content1));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_sum_w2(h, 1, &sumw2_1));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, content1);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, sumw2_1);

    histo_destroy(h);
}

/* Test fill_bin with exact moments flag enabled */
void test_fill_bin_exact_moments(void) {
    histo_t *h = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);

    /* Bin 0: center 5.0, weight 10.0 */
    /* Bin 2: center 25.0, weight 10.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_bin(h, 0, 10.0));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_bin(h, 2, 10.0));

    double mean = 0.0, var = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h, &mean));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 15.0, mean);

    /* Variance: ((5-15)^2 * 10 + (25-15)^2 * 10) / 20 = (1000 + 1000) / 20 = 100.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_variance(h, &var));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 100.0, var);

    histo_destroy(h);
}

/* Test strided fill with 0 stride defaults and NULL array bounds */
void test_fill_strided_zero_stride(void) {
    histo_t *h = histo_create_uniform(5, 0.0, 10.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    double x_vals[3] = {1.0, 3.0, 5.0};
    /* Stride 0 should default to sizeof(double) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_strided(h, 3, x_vals, 0, NULL, 0));
    TEST_ASSERT_EQUAL_UINT64(3, histo_num_entries(h));
    TEST_ASSERT_EQUAL_DOUBLE(3.0, histo_total_weight(h));

    /* Empty batch n = 0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_strided(h, 0, NULL, 0, NULL, 0));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_n(h, 0, NULL, NULL));

    /* Invalid pointer with n > 0 */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_fill_strided(h, 5, NULL, 0, NULL, 0));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_fill_n(h, 5, NULL, NULL));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_fill_strided(NULL, 3, x_vals, 0, NULL, 0));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_fill_n(NULL, 3, x_vals, NULL));

    histo_destroy(h);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_uniform_find_bin_boundaries);
    RUN_TEST(test_variable_find_bin);
    RUN_TEST(test_ieee754_edge_cases);
    RUN_TEST(test_fill_bin);
    RUN_TEST(test_fill_strided_and_batch);
    RUN_TEST(test_fill_n_sumw2_unit_weights);
    RUN_TEST(test_fill_bin_exact_moments);
    RUN_TEST(test_fill_strided_zero_stride);
    return UNITY_END();
}
