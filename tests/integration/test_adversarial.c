#include "unity.h"
#include "histo/histo.h"
#include "histo/histo2d.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

/* Test subnormals and denormals */
void test_subnormal_numbers(void) {
    histo_t *h = histo_create_uniform(10, 0.0, 1.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);

    double subnormal_val = 1e-315; /* Subnormal double */
    double subnormal_weight = 1e-315;

    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_w(h, subnormal_val, subnormal_weight));
    TEST_ASSERT_EQUAL_UINT64(1, histo_num_entries(h));

    double content = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 0, &content));
    TEST_ASSERT_TRUE(content > 0.0);

    histo_destroy(h);

}

/* Test boundary oscillations with machine precision epsilon */
void test_boundary_epsilon_oscillations(void) {
    histo_t *h = histo_create_uniform(5, 0.0, 50.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    double eps = 1e-12;
    int64_t bin = -1;

    /* Left side of bin boundary 10.0 (inside bin 0) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 10.0 - eps, &bin));
    TEST_ASSERT_EQUAL_INT64(0, bin);

    /* Exact bin boundary 10.0 (inside bin 1) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 10.0, &bin));
    TEST_ASSERT_EQUAL_INT64(1, bin);

    /* Right side of bin boundary 10.0 (inside bin 1) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 10.0 + eps, &bin));
    TEST_ASSERT_EQUAL_INT64(1, bin);

    /* Exact preceding float before upper edge 50.0 (inside bin 4) */
    double just_before_50 = nextafter(50.0, 0.0);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, just_before_50, &bin));
    TEST_ASSERT_EQUAL_INT64(4, bin);

    /* Exact upper edge 50.0 (overflow) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_find_bin(h, 50.0, &bin));
    TEST_ASSERT_EQUAL_INT64(5, bin);

    histo_destroy(h);
}


/* Test large bin allocation (100,000 bins) */
void test_large_histogram_stress(void) {
    const uint32_t nbins = 100000;
    histo_t *h = histo_create_uniform(nbins, 0.0, 100000.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    for (uint32_t i = 0; i < nbins; i += 100) {
        histo_fill_bin(h, i, 1.0);
    }
    TEST_ASSERT_EQUAL_DOUBLE(1000.0, histo_total_weight(h));

    histo_destroy(h);
}

static inline uint32_t simple_lcg(uint32_t *state) {
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

/* Fuzz test binary deserializer with pseudo-random corrupted buffers */
void test_binary_fuzz_resilience(void) {
    histo_t *src = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    for (int i = 0; i < 100; ++i) {
        histo_fill(src, (double)i);
    }

    void *orig_buf = NULL;
    size_t orig_size = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_serialize_binary(src, &orig_buf, &orig_size));
    TEST_ASSERT_NOT_NULL(orig_buf);

    uint8_t *corrupt_buf = (uint8_t *)malloc(orig_size);
    TEST_ASSERT_NOT_NULL(corrupt_buf);

    /* Perform 1000 randomized corruptions across headers and payloads */
    uint32_t prng_state = 42;
    for (int iter = 0; iter < 1000; ++iter) {
        memcpy(corrupt_buf, orig_buf, orig_size);

        /* Flip random byte at random position */
        size_t pos = (size_t)(simple_lcg(&prng_state) % orig_size);
        uint8_t val = (uint8_t)(simple_lcg(&prng_state) & 0xFF);
        corrupt_buf[pos] ^= (val == 0 ? 0xFF : val);

        /* Deserialization must NEVER crash or cause memory corruption */
        histo_t *h_corrupt = NULL;
        histo_status_t st = histo_deserialize_binary(corrupt_buf, orig_size, &h_corrupt);
        if (st == HISTO_OK && h_corrupt != NULL) {
            histo_destroy(h_corrupt);
        }
    }


    free(corrupt_buf);
    histo_free_buffer(orig_buf);
    histo_destroy(src);
}

/* Test numerical extremes: huge ranges and zero variance */
void test_extreme_ranges_and_variance(void) {
    histo_t *h = histo_create_uniform(10, -1e300, 1e300, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);

    /* Identical samples to test near-zero variance floor */
    for (int i = 0; i < 100; ++i) {
        histo_fill(h, 0.5e300);
    }
    
    double var = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_variance(h, &var));
    TEST_ASSERT_DOUBLE_WITHIN(1e280, 0.0, var); /* Variance should be close to 0 */
    
    double mean = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h, &mean));
    TEST_ASSERT_DOUBLE_WITHIN(1e285, 0.5e300, mean);

    /* Underflow extreme */
    histo_fill(h, -2e300);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, histo_underflow(h));

    histo_destroy(h);
}

/* Test negative weights (e.g. background subtraction) */
void test_negative_weights(void) {
    histo_t *h = histo_create_uniform(5, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    histo_fill_w(h, 5.0, 10.0);
    histo_fill_w(h, 5.0, -4.0);

    double content = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h, 2, &content));
    TEST_ASSERT_EQUAL_DOUBLE(6.0, content);

    double err = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_error(h, 2, &err));
    TEST_ASSERT_EQUAL_DOUBLE(sqrt(10.0 * 10.0 + (-4.0) * (-4.0)), err);

    histo_destroy(h);
}

/* Test massive sample stream (10^7 = 10,000,000 samples) */
void test_massive_stream_10m(void) {
    const size_t batch_sz = 10000;
    const size_t num_batches = 1000; /* 10,000 * 1,000 = 10,000,000 */
    double *batch = (double *)malloc(batch_sz * sizeof(double));
    TEST_ASSERT_NOT_NULL(batch);

    for (size_t i = 0; i < batch_sz; ++i) {
        batch[i] = (double)(i % 100);
    }

    histo_t *h = histo_create_uniform(100, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);

    for (size_t b = 0; b < num_batches; ++b) {
        TEST_ASSERT_EQUAL(HISTO_OK, histo_fill_n(h, batch_sz, batch, NULL));
    }

    TEST_ASSERT_EQUAL_UINT64(10000000ULL, histo_num_entries(h));
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, 10000000.0, histo_total_weight(h));

    double mean = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h, &mean));
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 49.5, mean);

    double var = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_variance(h, &var));
    /* Variance of uniform discrete 0..99 is (100^2 - 1) / 12 = 9999 / 12 = 833.25 */
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 833.25, var);

    histo_destroy(h);
    free(batch);
}

/* Test comprehensive matrix of IEEE-754 special values */
void test_all_ieee754_specials_matrix(void) {
    histo_t *h = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);

    /* 1. Signed zeroes */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill(h, +0.0));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill(h, -0.0));

    /* 2. NaNs */
    TEST_ASSERT_EQUAL(HISTO_ERR_NON_FINITE, histo_fill(h, NAN));
    TEST_ASSERT_EQUAL(HISTO_ERR_NON_FINITE, histo_fill(h, -NAN));

    /* 3. Infinities */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill(h, INFINITY));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill(h, -INFINITY));

    /* 4. Subnormals and Extremes */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill(h, DBL_MIN));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill(h, nextafter(0.0, 1.0)));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill(h, DBL_MAX));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill(h, -DBL_MAX));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fill(h, nextafter(DBL_MAX, 0.0)));

    /* Verify NaN counter accounted for exactly 2 NaNs */
    TEST_ASSERT_EQUAL_UINT64(2, histo_nan_count(h));

    /* Verify underflow accounts for -INFINITY, -DBL_MAX */
    TEST_ASSERT_TRUE(histo_underflow(h) >= 2.0);

    /* Verify overflow accounts for INFINITY, DBL_MAX, nextafter */
    TEST_ASSERT_TRUE(histo_overflow(h) >= 3.0);

    histo_destroy(h);
}

/* Fuzz test 2D binary and JSON deserialization resilience */
void test_corrupt_2d_and_json_deserialization(void) {
    histo2d_t *src2d = histo2d_create_uniform(8, 0.0, 80.0, 8, 0.0, 80.0,
                                              HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    for (int i = 0; i < 50; ++i) {
        histo2d_fill(src2d, (double)(i % 80), (double)(i % 80));
    }

    void *bin_buf = NULL;
    size_t bin_sz = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_serialize_binary_alloc(src2d, &bin_buf, &bin_sz));
    TEST_ASSERT_NOT_NULL(bin_buf);

    uint8_t *corrupt = (uint8_t *)malloc(bin_sz);
    TEST_ASSERT_NOT_NULL(corrupt);

    uint32_t rng = 777;
    for (int iter = 0; iter < 500; ++iter) {
        memcpy(corrupt, bin_buf, bin_sz);
        size_t pos = (size_t)(simple_lcg(&rng) % bin_sz);
        uint8_t val = (uint8_t)(simple_lcg(&rng) & 0xFF);
        corrupt[pos] ^= (val == 0 ? 0xAA : val);

        histo2d_t *h_corrupt = NULL;
        histo_status_t st = histo2d_deserialize_binary(corrupt, bin_sz, &h_corrupt);
        if (st == HISTO_OK && h_corrupt) {
            histo2d_destroy(h_corrupt);
        }
    }

    free(corrupt);
    histo_free_buffer(bin_buf);
    histo2d_destroy(src2d);

    /* Test corrupt JSON strings */
    const char *bad_jsons[] = {
        "",
        "{",
        "{\"type\": \"histo2d\"",
        "{\"version\": 99999}",
        "{\"type\": \"histo2d\", \"x_nbins\": -5}",
        "{\"type\": \"histo2d\", \"bins\": [\"corrupt\", null]}",
        "{\"type\": \"histogram\", \"nbins\": 1000000000}",
        "{\"type\": \"histogram\", \"range\": [50.0, 10.0]}"
    };

    for (size_t j = 0; j < sizeof(bad_jsons) / sizeof(bad_jsons[0]); ++j) {
        histo_t *h1 = NULL;
        histo2d_t *h2 = NULL;
        histo_status_t st1 = histo_deserialize_json(bad_jsons[j], &h1);
        if (st1 == HISTO_OK && h1) histo_destroy(h1);

        histo_status_t st2 = histo2d_deserialize_json(bad_jsons[j], &h2);
        if (st2 == HISTO_OK && h2) histo2d_destroy(h2);
    }
}

/* Test degenerate range creations */
void test_degenerate_range_creations(void) {
    /* 1D */
    TEST_ASSERT_NULL(histo_create_uniform(0, 0.0, 10.0, 0));
    TEST_ASSERT_NULL(histo_create_uniform(10, 10.0, 0.0, 0)); /* min > max */
    TEST_ASSERT_NULL(histo_create_uniform(10, 5.0, 5.0, 0));   /* min == max */
    TEST_ASSERT_NULL(histo_create_uniform(10, NAN, 10.0, 0));
    TEST_ASSERT_NULL(histo_create_uniform(10, 0.0, INFINITY, 0));
    TEST_ASSERT_NULL(histo_create_uniform(HISTO_MAX_NBINS + 1, 0.0, 10.0, 0));

    /* 2D */
    TEST_ASSERT_NULL(histo2d_create_uniform(0, 0.0, 10.0, 10, 0.0, 10.0, 0));
    TEST_ASSERT_NULL(histo2d_create_uniform(10, 0.0, 10.0, 0, 0.0, 10.0, 0));
    TEST_ASSERT_NULL(histo2d_create_uniform(10, 10.0, 0.0, 10, 0.0, 10.0, 0));
    TEST_ASSERT_NULL(histo2d_create_uniform(10, 0.0, 10.0, 10, 10.0, 0.0, 0));
    TEST_ASSERT_NULL(histo2d_create_uniform(10, NAN, 10.0, 10, 0.0, 10.0, 0));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_subnormal_numbers);
    RUN_TEST(test_boundary_epsilon_oscillations);
    RUN_TEST(test_large_histogram_stress);
    RUN_TEST(test_binary_fuzz_resilience);
    RUN_TEST(test_extreme_ranges_and_variance);
    RUN_TEST(test_negative_weights);
    RUN_TEST(test_massive_stream_10m);
    RUN_TEST(test_all_ieee754_specials_matrix);
    RUN_TEST(test_corrupt_2d_and_json_deserialization);
    RUN_TEST(test_degenerate_range_creations);
    return UNITY_END();
}

