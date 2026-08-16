#include "unity.h"
#include "histo/histo.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

/* Test binary serialization and deserialization roundtrip for uniform histogram */
void test_binary_roundtrip_uniform(void) {
    histo_t *src = histo_create_uniform(5, 0.0, 50.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(src);

    histo_fill_w(src, 5.0, 10.0);
    histo_fill_w(src, 15.0, 20.0);
    histo_fill_w(src, 25.0, 30.0);
    histo_fill_w(src, -5.0, 2.0);   /* Underflow */
    histo_fill_w(src, 100.0, 4.0);  /* Overflow */
    histo_fill(src, NAN);          /* NaN */

    /* 1. Allocate & serialize */
    void *buf = NULL;
    size_t buf_size = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_serialize_binary(src, &buf, &buf_size));
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_TRUE(buf_size >= 256);

    /* 2. Deserialize */
    histo_t *dst = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_deserialize_binary(buf, buf_size, &dst));
    TEST_ASSERT_NOT_NULL(dst);

    /* 3. Verify exact match */
    TEST_ASSERT_EQUAL_UINT32(5, histo_nbins(dst));
    TEST_ASSERT_EQUAL(HISTO_BIN_UNIFORM, histo_bin_type(dst));
    TEST_ASSERT_EQUAL_DOUBLE(60.0, histo_total_weight(dst));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, histo_underflow(dst));
    TEST_ASSERT_EQUAL_DOUBLE(4.0, histo_overflow(dst));
    TEST_ASSERT_EQUAL_UINT64(1, histo_nan_count(dst));

    double content_src = 0.0, content_dst = 0.0;
    double err_src = 0.0, err_dst = 0.0;
    for (uint32_t i = 0; i < 5; ++i) {
        histo_bin_content(src, i, &content_src);
        histo_bin_content(dst, i, &content_dst);
        TEST_ASSERT_EQUAL_DOUBLE(content_src, content_dst);

        histo_bin_error(src, i, &err_src);
        histo_bin_error(dst, i, &err_dst);
        TEST_ASSERT_EQUAL_DOUBLE(err_src, err_dst);
    }

    double mean_src = 0.0, mean_dst = 0.0;
    double var_src = 0.0, var_dst = 0.0;
    histo_mean(src, &mean_src);
    histo_mean(dst, &mean_dst);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, mean_src, mean_dst);

    histo_variance(src, &var_src);
    histo_variance(dst, &var_dst);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, var_src, var_dst);

    /* 4. Caller-provided buffer test */
    size_t req_size = histo_serialize_binary_size(src);
    TEST_ASSERT_EQUAL(buf_size, req_size);
    void *caller_buf = malloc(req_size);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_serialize_binary_into(src, caller_buf, req_size));

    histo_t *dst2 = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_deserialize_binary(caller_buf, req_size, &dst2));
    TEST_ASSERT_NOT_NULL(dst2);
    TEST_ASSERT_EQUAL_DOUBLE(histo_total_weight(src), histo_total_weight(dst2));

    free(caller_buf);
    histo_destroy(dst2);
    histo_free_buffer(buf);
    histo_destroy(src);
    histo_destroy(dst);
}

/* Test binary roundtrip for variable bin histogram */
void test_binary_roundtrip_variable(void) {
    const double edges[] = {-100.0, -10.0, 0.0, 25.0, 500.0};
    histo_t *src = histo_create_variable(4, edges, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(src);

    histo_fill_w(src, -50.0, 5.0);
    histo_fill_w(src, 10.0, 15.0);

    void *buf = NULL;
    size_t buf_size = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_serialize_binary(src, &buf, &buf_size));

    histo_t *dst = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_deserialize_binary(buf, buf_size, &dst));
    TEST_ASSERT_NOT_NULL(dst);

    TEST_ASSERT_EQUAL_UINT32(4, histo_nbins(dst));
    TEST_ASSERT_EQUAL(HISTO_BIN_VARIABLE, histo_bin_type(dst));
    TEST_ASSERT_EQUAL_DOUBLE(20.0, histo_total_weight(dst));

    double low = 0.0, high = 0.0;
    histo_bin_bounds(dst, 0, &low, &high);
    TEST_ASSERT_EQUAL_DOUBLE(-100.0, low);
    TEST_ASSERT_EQUAL_DOUBLE(-10.0, high);

    histo_free_buffer(buf);
    histo_destroy(src);
    histo_destroy(dst);
}

/* Test corrupted binary buffer handling */
void test_binary_deserialization_errors(void) {
    histo_t *dst = NULL;

    /* NULL inputs */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_deserialize_binary(NULL, 100, &dst));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_deserialize_binary("1234", 4, NULL));

    /* Truncated buffer (< 256 bytes) */
    uint8_t short_buf[100] = {0};
    TEST_ASSERT_EQUAL(HISTO_ERR_DESERIALIZATION, histo_deserialize_binary(short_buf, sizeof(short_buf), &dst));

    /* Corrupted magic */
    uint8_t fake_buf[256] = {0};
    memcpy(fake_buf, "INVALID!", 8);
    TEST_ASSERT_EQUAL(HISTO_ERR_DESERIALIZATION, histo_deserialize_binary(fake_buf, sizeof(fake_buf), &dst));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_roundtrip_uniform);
    RUN_TEST(test_binary_roundtrip_variable);
    RUN_TEST(test_binary_deserialization_errors);
    return UNITY_END();
}

