#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "unity.h"
#include "histo/histo.h"

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


int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_welford_negative_weights);
    RUN_TEST(test_subtract_moments_consistency);
    RUN_TEST(test_quantile_discontinuity);
    return UNITY_END();
}
