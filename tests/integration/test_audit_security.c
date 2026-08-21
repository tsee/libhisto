/*
 * Security audit tests: integer overflows, buffer limits, and corrupt input.
 */

#include "unity.h"
#include "histo/histo.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

void test_security_null_pointers(void) {
    histo_destroy(NULL);
    TEST_ASSERT_NULL(histo_clone(NULL, true));
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_INVALID_ARG, histo_reset(NULL));
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_INVALID_ARG, histo_fill(NULL, 1.0));
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_INVALID_ARG, histo_fill_w(NULL, 1.0, 2.0));
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_INVALID_ARG, histo_fill_bin(NULL, 0, 1.0));
    
    double out_min = 0.0, out_max = 0.0;
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_INVALID_ARG, histo_range(NULL, &out_min, &out_max));
    
    histo_t *h = histo_create_uniform(10, 0.0, 1.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);
    
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_INVALID_ARG, histo_range(h, NULL, NULL));
    
    size_t out_size = 0;
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_INVALID_ARG, histo_serialize_binary(NULL, NULL, &out_size));
    
    histo_destroy(h);
}

void test_security_deserialize_tampered_header(void) {
    histo_t *h = histo_create_uniform(10, 0.0, 10.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);
    histo_fill(h, 5.0);
    
    void *buf = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(HISTO_OK, histo_serialize_binary(h, &buf, &size));
    TEST_ASSERT_NOT_NULL(buf);
    
    // Test truncation
    histo_t *h_out = NULL;
    for (size_t trunc_size = 0; trunc_size < size; trunc_size++) {
        histo_status_t status = histo_deserialize_binary(buf, trunc_size, &h_out);
        TEST_ASSERT_EQUAL_INT(HISTO_ERR_DESERIALIZATION, status);
        TEST_ASSERT_NULL(h_out);
    }
    
    // Tamper with nbins (try to cause integer overflow during payload calculation)
    uint8_t *tampered_buf = malloc(size);
    TEST_ASSERT_NOT_NULL(tampered_buf);
    memcpy(tampered_buf, buf, size);
    
    // Assuming nbins is at some offset, let's just spray large integers
    // Actually, libhisto binary format typically starts with magic bytes, version, then metadata.
    // By fuzzing the first 128 bytes, we can try to trigger memory allocation failures or OOMs.
    for (size_t i = 8; i < (size < 64 ? size : 64); i += 4) {
        memcpy(tampered_buf, buf, size);
        uint32_t bad_val = 0xFFFFFFFF;
        memcpy(tampered_buf + i, &bad_val, sizeof(uint32_t));
        
        histo_status_t status = histo_deserialize_binary(tampered_buf, size, &h_out);
        if (status == HISTO_OK) {
            histo_destroy(h_out);
            h_out = NULL;
        } else {
            TEST_ASSERT_NULL(h_out);
        }
    }
    
    free(tampered_buf);
    histo_free_buffer(buf);
    histo_destroy(h);
}

void test_security_unaligned_memory(void) {
    histo_t *h = histo_create_uniform(10, 0.0, 10.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);
    
    void *buf = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(HISTO_OK, histo_serialize_binary(h, &buf, &size));
    
    // Allocate a larger buffer to force unaligned access
    uint8_t *unaligned_buf = malloc(size + 1);
    TEST_ASSERT_NOT_NULL(unaligned_buf);
    
    // Shift by 1 byte
    memcpy(unaligned_buf + 1, buf, size);
    
    histo_t *h_out = NULL;
    histo_status_t status = histo_deserialize_binary(unaligned_buf + 1, size, &h_out);
    TEST_ASSERT_EQUAL_INT(HISTO_OK, status);
    TEST_ASSERT_NOT_NULL(h_out);
    
    histo_destroy(h_out);
    free(unaligned_buf);
    histo_free_buffer(buf);
    histo_destroy(h);
}

void test_security_max_allocations(void) {
    // Check if it gracefully handles extremely large allocations
    // HISTO_MAX_NBINS is 100,000,000. That means bins + sum_w2 = ~1.6GB
    // That could pass on modern systems, but let's test if it handles it.
    histo_t *h = histo_create_uniform(HISTO_MAX_NBINS, 0.0, 1.0, HISTO_FLAG_TRACK_SUMW2);
    if (h != NULL) {
        // If it succeeded, fill should work safely
        histo_fill(h, 0.5);
        histo_destroy(h);
    }
    
    // Test beyond max bins
    histo_t *h_bad = histo_create_uniform(HISTO_MAX_NBINS + 1, 0.0, 1.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NULL(h_bad);
}

void test_security_incompatible_clone(void) {
    histo_t *h1 = histo_create_uniform(10, 0.0, 10.0, HISTO_FLAG_NONE);
    histo_t *h2 = histo_clone(h1, true);
    
    TEST_ASSERT_NOT_NULL(h2);
    
    // They should be identical in structure, but let's test memory disjointedness
    histo_fill(h1, 5.0);
    double c1, c2;
    histo_bin_content(h1, 5, &c1);
    histo_bin_content(h2, 5, &c2);
    
    TEST_ASSERT_EQUAL_DOUBLE(1.0, c1);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, c2);
    
    histo_destroy(h1);
    histo_destroy(h2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_security_null_pointers);
    RUN_TEST(test_security_deserialize_tampered_header);
    RUN_TEST(test_security_unaligned_memory);
    RUN_TEST(test_security_max_allocations);
    RUN_TEST(test_security_incompatible_clone);
    return UNITY_END();
}
