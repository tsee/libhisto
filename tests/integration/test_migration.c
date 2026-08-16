#include "histo/histo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

/* Helper to convert a V2 buffer to V1 buffer for testing */
static void make_v1_blob(uint8_t *buf, size_t size) {
    if (size >= 10) {
        buf[8] = 1;
        buf[9] = 0;
    }
}

static void test_migration_uniform_unweighted() {
    histo_t *h = histo_create_uniform(10, 0.0, 10.0, 0);
    assert(h != NULL);
    histo_fill(h, 5.5);
    histo_fill(h, -1.0);
    histo_fill(h, 11.0);

    void *v2_buf = NULL;
    size_t size = 0;
    assert(histo_serialize_binary(h, &v2_buf, &size) == HISTO_OK);

    uint8_t *v1_buf = malloc(size);
    memcpy(v1_buf, v2_buf, size);
    make_v1_blob(v1_buf, size);

    void *migrated_buf = NULL;
    size_t migrated_size = 0;
    assert(histo_migrate_binary(v1_buf, size, &migrated_buf, &migrated_size) == HISTO_OK);
    assert(migrated_size == size);
    assert(((uint8_t*)migrated_buf)[8] == 2);

    histo_t *h2 = NULL;
    assert(histo_deserialize_binary(v1_buf, size, &h2) == HISTO_OK);
    
    printf("h2 entries: %lu\n", (unsigned long)histo_num_entries(h2));
    assert(histo_num_entries(h2) == 1);
    assert(histo_underflow(h2) == 1.0);
    assert(histo_overflow(h2) == 1.0);

    histo_destroy(h);
    histo_destroy(h2);
    histo_free_buffer(v2_buf);
    free(v1_buf);
    histo_free_buffer(migrated_buf);
    printf("Passed %s\n", __func__);
}

static void test_migration_weighted_sumw2() {
    histo_t *h = histo_create_uniform(5, 0.0, 5.0, HISTO_FLAG_TRACK_SUMW2);
    histo_fill_w(h, 2.5, 2.0);
    
    void *v2_buf = NULL;
    size_t size = 0;
    histo_serialize_binary(h, &v2_buf, &size);
    
    uint8_t *v1_buf = malloc(size);
    memcpy(v1_buf, v2_buf, size);
    make_v1_blob(v1_buf, size);
    
    histo_t *h2 = NULL;
    assert(histo_deserialize_binary(v1_buf, size, &h2) == HISTO_OK);
    
    double sumw2;
    histo_bin_sum_w2(h2, 2, &sumw2);
    assert(sumw2 == 4.0);
    
    histo_destroy(h);
    histo_destroy(h2);
    histo_free_buffer(v2_buf);
    free(v1_buf);
    printf("Passed %s\n", __func__);
}

static void test_migration_exact_moments() {
    histo_t *h = histo_create_uniform(5, 0.0, 5.0, HISTO_FLAG_EXACT_MOMENTS);
    histo_fill(h, 1.0);
    histo_fill(h, 3.0);
    
    void *v2_buf = NULL;
    size_t size = 0;
    histo_serialize_binary(h, &v2_buf, &size);
    
    uint8_t *v1_buf = malloc(size);
    memcpy(v1_buf, v2_buf, size);
    make_v1_blob(v1_buf, size);
    
    histo_t *h2 = NULL;
    assert(histo_deserialize_binary(v1_buf, size, &h2) == HISTO_OK);
    
    double mean;
    histo_mean(h2, &mean);
    assert(mean == 2.0);
    
    histo_destroy(h);
    histo_destroy(h2);
    histo_free_buffer(v2_buf);
    free(v1_buf);
    printf("Passed %s\n", __func__);
}

static void test_migration_variable() {
    double edges[] = {0.0, 1.0, 10.0, 100.0};
    histo_t *h = histo_create_variable(3, edges, 0);
    histo_fill(h, 5.0);
    
    void *v2_buf = NULL;
    size_t size = 0;
    histo_serialize_binary(h, &v2_buf, &size);
    
    uint8_t *v1_buf = malloc(size);
    memcpy(v1_buf, v2_buf, size);
    make_v1_blob(v1_buf, size);
    
    histo_t *h2 = NULL;
    assert(histo_deserialize_binary(v1_buf, size, &h2) == HISTO_OK);
    
    double c;
    histo_bin_content(h2, 1, &c);
    assert(c == 1.0);
    
    histo_destroy(h);
    histo_destroy(h2);
    histo_free_buffer(v2_buf);
    free(v1_buf);
    printf("Passed %s\n", __func__);
}

static void test_migration_edge_cases() {
    histo_t *h = histo_create_uniform(5, 0.0, 5.0, 0);
    void *v2_buf = NULL;
    size_t size = 0;
    histo_serialize_binary(h, &v2_buf, &size);
    
    uint8_t *buf = malloc(size);
    memcpy(buf, v2_buf, size);
    
    /* V0 - invalid */
    buf[8] = 0; buf[9] = 0;
    histo_t *h_err = NULL;
    assert(histo_deserialize_binary(buf, size, &h_err) == HISTO_ERR_DESERIALIZATION);
    
    /* V99 - invalid */
    buf[8] = 99; buf[9] = 0;
    assert(histo_deserialize_binary(buf, size, &h_err) == HISTO_ERR_DESERIALIZATION);
    
    /* Corrupt magic V1 */
    make_v1_blob(buf, size);
    buf[0] = 0x00;
    assert(histo_deserialize_binary(buf, size, &h_err) == HISTO_ERR_DESERIALIZATION);
    
    /* Restore magic, truncate buffer */
    buf[0] = 0x89;
    assert(histo_deserialize_binary(buf, 200, &h_err) == HISTO_ERR_DESERIALIZATION);
    
    histo_destroy(h);
    histo_free_buffer(v2_buf);
    free(buf);
    printf("Passed %s\n", __func__);
}

int main() {
    test_migration_uniform_unweighted();
    test_migration_weighted_sumw2();
    test_migration_exact_moments();
    test_migration_variable();
    test_migration_edge_cases();
    printf("All migration tests passed.\n");
    return 0;
}
