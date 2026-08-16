#include "unity.h"
#include "histo/histo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

/* Helper to convert a V2 buffer to V1 buffer for testing */
static void make_v1_blob(uint8_t *buf, size_t size) {
    if (size >= 10) {
        buf[8] = 1;
        buf[9] = 0;
    }
}

void test_migration_uniform_unweighted(void) {
    histo_t *h = histo_create_uniform(10, 0.0, 10.0, 0);
    TEST_ASSERT_NOT_NULL(h);
    histo_fill(h, 5.5);
    histo_fill(h, -1.0);
    histo_fill(h, 11.0);

    void *v2_buf = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_serialize_binary(h, &v2_buf, &size));

    uint8_t *v1_buf = malloc(size);
    TEST_ASSERT_NOT_NULL(v1_buf);
    memcpy(v1_buf, v2_buf, size);
    make_v1_blob(v1_buf, size);

    void *migrated_buf = NULL;
    size_t migrated_size = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_migrate_binary(v1_buf, size, &migrated_buf, &migrated_size));
    TEST_ASSERT_EQUAL_UINT(size, migrated_size);
    TEST_ASSERT_EQUAL_UINT8(2, ((uint8_t*)migrated_buf)[8]);

    histo_t *h2 = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_deserialize_binary(v1_buf, size, &h2));
    TEST_ASSERT_NOT_NULL(h2);

    TEST_ASSERT_EQUAL_UINT64(1, histo_num_entries(h2));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, histo_underflow(h2));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, histo_overflow(h2));

    histo_destroy(h);
    histo_destroy(h2);
    histo_free_buffer(v2_buf);
    free(v1_buf);
    histo_free_buffer(migrated_buf);
}

void test_migration_weighted_sumw2(void) {
    histo_t *h = histo_create_uniform(5, 0.0, 5.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);
    histo_fill_w(h, 2.5, 2.0);

    void *v2_buf = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_serialize_binary(h, &v2_buf, &size));

    uint8_t *v1_buf = malloc(size);
    TEST_ASSERT_NOT_NULL(v1_buf);
    memcpy(v1_buf, v2_buf, size);
    make_v1_blob(v1_buf, size);

    histo_t *h2 = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_deserialize_binary(v1_buf, size, &h2));
    TEST_ASSERT_NOT_NULL(h2);

    double sumw2 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_sum_w2(h2, 2, &sumw2));
    TEST_ASSERT_EQUAL_DOUBLE(4.0, sumw2);

    histo_destroy(h);
    histo_destroy(h2);
    histo_free_buffer(v2_buf);
    free(v1_buf);
}

void test_migration_exact_moments(void) {
    histo_t *h = histo_create_uniform(5, 0.0, 5.0, HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);
    histo_fill(h, 1.0);
    histo_fill(h, 3.0);

    void *v2_buf = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_serialize_binary(h, &v2_buf, &size));

    uint8_t *v1_buf = malloc(size);
    TEST_ASSERT_NOT_NULL(v1_buf);
    memcpy(v1_buf, v2_buf, size);
    make_v1_blob(v1_buf, size);

    histo_t *h2 = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_deserialize_binary(v1_buf, size, &h2));
    TEST_ASSERT_NOT_NULL(h2);

    double mean = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(h2, &mean));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, mean);

    histo_destroy(h);
    histo_destroy(h2);
    histo_free_buffer(v2_buf);
    free(v1_buf);
}

void test_migration_variable(void) {
    double edges[] = {0.0, 1.0, 10.0, 100.0};
    histo_t *h = histo_create_variable(3, edges, 0);
    TEST_ASSERT_NOT_NULL(h);
    histo_fill(h, 5.0);

    void *v2_buf = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_serialize_binary(h, &v2_buf, &size));

    uint8_t *v1_buf = malloc(size);
    TEST_ASSERT_NOT_NULL(v1_buf);
    memcpy(v1_buf, v2_buf, size);
    make_v1_blob(v1_buf, size);

    histo_t *h2 = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_deserialize_binary(v1_buf, size, &h2));
    TEST_ASSERT_NOT_NULL(h2);

    double c = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_bin_content(h2, 1, &c));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, c);

    histo_destroy(h);
    histo_destroy(h2);
    histo_free_buffer(v2_buf);
    free(v1_buf);
}

void test_migration_edge_cases(void) {
    histo_t *h = histo_create_uniform(5, 0.0, 5.0, 0);
    TEST_ASSERT_NOT_NULL(h);
    void *v2_buf = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_serialize_binary(h, &v2_buf, &size));

    uint8_t *buf = malloc(size);
    TEST_ASSERT_NOT_NULL(buf);
    memcpy(buf, v2_buf, size);

    /* V0 - invalid */
    buf[8] = 0; buf[9] = 0;
    histo_t *h_err = NULL;
    TEST_ASSERT_EQUAL(HISTO_ERR_DESERIALIZATION, histo_deserialize_binary(buf, size, &h_err));
    TEST_ASSERT_NULL(h_err);

    /* V99 - invalid */
    buf[8] = 99; buf[9] = 0;
    TEST_ASSERT_EQUAL(HISTO_ERR_DESERIALIZATION, histo_deserialize_binary(buf, size, &h_err));
    TEST_ASSERT_NULL(h_err);

    /* Corrupt magic V1 */
    make_v1_blob(buf, size);
    buf[0] = 0x00;
    TEST_ASSERT_EQUAL(HISTO_ERR_DESERIALIZATION, histo_deserialize_binary(buf, size, &h_err));
    TEST_ASSERT_NULL(h_err);

    /* Restore magic, truncate buffer */
    buf[0] = 0x89;
    TEST_ASSERT_EQUAL(HISTO_ERR_DESERIALIZATION, histo_deserialize_binary(buf, 200, &h_err));
    TEST_ASSERT_NULL(h_err);

    histo_destroy(h);
    histo_free_buffer(v2_buf);
    free(buf);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_migration_uniform_unweighted);
    RUN_TEST(test_migration_weighted_sumw2);
    RUN_TEST(test_migration_exact_moments);
    RUN_TEST(test_migration_variable);
    RUN_TEST(test_migration_edge_cases);
    return UNITY_END();
}
