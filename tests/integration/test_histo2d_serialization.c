/*
 * Integration tests for 2D histogram binary and JSON serialization roundtrips.
 */

#include "unity.h"
#include "histo/histo2d.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

void test_histo2d_binary_roundtrip_uniform(void) {
    histo2d_t *h = histo2d_create_uniform(5, 0.0, 10.0, 4, -2.0, 2.0,
                                          HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);

    histo2d_fill_w(h, 2.0, 0.5, 3.5);
    histo2d_fill_w(h, 8.0, -1.0, 1.5);
    /* Out of bounds guards */
    histo2d_fill_w(h, -5.0, 5.0, 2.0);  /* North-West */
    histo2d_fill_w(h, 15.0, 0.0, 4.0);  /* East */

    void *buf = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_serialize_binary_alloc(h, &buf, &size));
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_TRUE(size >= 256);

    histo2d_t *loaded = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_deserialize_binary(buf, size, &loaded));
    TEST_ASSERT_NOT_NULL(loaded);

    /* Verify metadata & counts */
    TEST_ASSERT_EQUAL_UINT32(5, histo2d_nbins_x(loaded));
    TEST_ASSERT_EQUAL_UINT32(4, histo2d_nbins_y(loaded));
    TEST_ASSERT_EQUAL_UINT64(2, histo2d_num_entries(loaded));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, histo2d_total_weight(loaded));

    /* Verify cell contents */
    double c1, c2, w2;
    histo2d_bin_content(loaded, 1, 2, &c1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.5, c1);
    histo2d_bin_sum_w2(loaded, 1, 2, &w2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.5 * 3.5, w2);

    histo2d_bin_content(loaded, 4, 1, &c2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.5, c2);

    /* Verify guards */
    double gw;
    uint64_t gc;
    histo2d_region_content(loaded, HISTO2D_REGION_NORTH_WEST, &gw, &gc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, gw);
    TEST_ASSERT_EQUAL_UINT64(1, gc);

    histo2d_region_content(loaded, HISTO2D_REGION_EAST, &gw, &gc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, gw);
    TEST_ASSERT_EQUAL_UINT64(1, gc);

    free(buf);
    histo2d_destroy(h);
    histo2d_destroy(loaded);
}

void test_histo2d_binary_roundtrip_variable(void) {
    double xedges[] = {0.0, 1.0, 5.0, 10.0};
    double yedges[] = {-10.0, 0.0, 10.0, 20.0};
    histo2d_t *h = histo2d_create_variable(3, xedges, 3, yedges, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    histo2d_fill_w(h, 2.5, 5.0, 12.0);

    void *buf = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_serialize_binary_alloc(h, &buf, &size));
    TEST_ASSERT_NOT_NULL(buf);

    histo2d_t *loaded = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_deserialize_binary(buf, size, &loaded));
    TEST_ASSERT_NOT_NULL(loaded);

    histo2d_axis_t ax, ay;
    histo2d_axis_x(loaded, &ax);
    histo2d_axis_y(loaded, &ay);
    TEST_ASSERT_EQUAL(HISTO_BIN_VARIABLE, ax.type);
    TEST_ASSERT_EQUAL(HISTO_BIN_VARIABLE, ay.type);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, ax.edges[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, ax.edges[2]);

    double c;
    histo2d_bin_content(loaded, 1, 1, &c);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 12.0, c);

    free(buf);
    histo2d_destroy(h);
    histo2d_destroy(loaded);
}

void test_histo2d_binary_corrupt_rejection(void) {
    histo2d_t *h = histo2d_create_uniform(4, 0.0, 4.0, 4, 0.0, 4.0, HISTO_FLAG_NONE);
    void *buf = NULL;
    size_t size = 0;
    histo2d_serialize_binary_alloc(h, &buf, &size);
    histo2d_destroy(h);

    histo2d_t *out = NULL;
    /* 1. Truncated buffer */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo2d_deserialize_binary(buf, 100, &out));

    /* 2. Corrupted Magic */
    uint8_t *corrupted = (uint8_t*)malloc(size);
    memcpy(corrupted, buf, size);
    corrupted[0] = 0xFF;
    TEST_ASSERT_EQUAL(HISTO_ERR_DESERIALIZATION, histo2d_deserialize_binary(corrupted, size, &out));

    /* 3. Version mismatch */
    memcpy(corrupted, buf, size);
    corrupted[8] = 99; /* Version 99 */
    TEST_ASSERT_EQUAL(HISTO_ERR_DESERIALIZATION, histo2d_deserialize_binary(corrupted, size, &out));

    free(buf);
    free(corrupted);
}

void test_histo2d_json_roundtrip(void) {
    double yedges[] = {0.0, 10.0, 50.0, 100.0};
    histo2d_t *h = histo2d_create_uniform_variable(3, 0.0, 30.0, 3, yedges,
                                                   HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);

    histo2d_fill_w(h, 5.0, 25.0, 4.0);
    histo2d_fill_w(h, 15.0, 5.0, 2.0);

    char *json_str = NULL;
    size_t json_size = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_serialize_json_alloc(h, &json_str, &json_size));
    TEST_ASSERT_NOT_NULL(json_str);
    TEST_ASSERT_TRUE(strstr(json_str, "libhisto2d-v1") != NULL);

    histo2d_t *loaded = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_deserialize_json(json_str, &loaded));
    TEST_ASSERT_NOT_NULL(loaded);

    TEST_ASSERT_EQUAL_UINT32(3, histo2d_nbins_x(loaded));
    TEST_ASSERT_EQUAL_UINT32(3, histo2d_nbins_y(loaded));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 6.0, histo2d_total_weight(loaded));

    double c1, c2;
    histo2d_bin_content(loaded, 0, 1, &c1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, c1);
    histo2d_bin_content(loaded, 1, 0, &c2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, c2);

    free(json_str);
    histo2d_destroy(h);
    histo2d_destroy(loaded);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_histo2d_binary_roundtrip_uniform);
    RUN_TEST(test_histo2d_binary_roundtrip_variable);
    RUN_TEST(test_histo2d_binary_corrupt_rejection);
    RUN_TEST(test_histo2d_json_roundtrip);
    return UNITY_END();
}
