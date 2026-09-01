/*
 * Unit tests for DDSketch quantile streaming sketches and accuracy bounds.
 */

#include "histo/sketch.h"
#include "histo/histo.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>

static void test_sketch_basic(void) {
    histo_sketch_t *s = histo_sketch_create(0.01, 1000);
    assert(s != NULL);
    
    histo_sketch_insert(s, 1.0);
    histo_sketch_insert(s, 2.0);
    histo_sketch_insert(s, 3.0);
    histo_sketch_insert(s, -1.0);
    histo_sketch_insert(s, 0.0);
    
    double q;
    histo_sketch_quantile(s, 0.5, &q);
    
    assert(histo_sketch_num_entries(s) == 5);
    
    histo_sketch_destroy(s);
}

static void test_sketch_merge(void) {
    histo_sketch_t *s1 = histo_sketch_create(0.01, 1000);
    histo_sketch_t *s2 = histo_sketch_create(0.01, 1000);
    
    for (int i = 1; i <= 100; i++) histo_sketch_insert(s1, i);
    for (int i = 101; i <= 200; i++) histo_sketch_insert(s2, i);
    
    histo_sketch_merge(s1, s2);
    assert(histo_sketch_num_entries(s1) == 200);
    
    histo_sketch_destroy(s1);
    histo_sketch_destroy(s2);
}

static void test_sketch_serialization(void) {
    histo_sketch_t *s = histo_sketch_create(0.01, 1000);
    assert(s != NULL);

    for (int i = -50; i <= 50; i++) {
        histo_sketch_insert(s, (double)i * 1.5);
    }

    void *buf = NULL;
    size_t size = 0;
    histo_status_t status = histo_sketch_serialize_binary(s, &buf, &size);
    assert(status == HISTO_OK);
    assert(buf != NULL);
    assert(size > 128);

    histo_sketch_t *restored = NULL;
    status = histo_sketch_deserialize_binary(buf, size, &restored);
    assert(status == HISTO_OK);
    (void)status;
    assert(restored != NULL);

    assert(histo_sketch_num_entries(restored) == histo_sketch_num_entries(s));
    assert(fabs(histo_sketch_total_weight(restored) - histo_sketch_total_weight(s)) < 1e-9);
    assert(fabs(histo_sketch_min(restored) - histo_sketch_min(s)) < 1e-9);
    assert(fabs(histo_sketch_max(restored) - histo_sketch_max(s)) < 1e-9);

    double q_orig, q_restored;
    histo_sketch_quantile(s, 0.5, &q_orig);
    histo_sketch_quantile(restored, 0.5, &q_restored);
    assert(fabs(q_orig - q_restored) < 1e-9);

    histo_free_buffer(buf);
    histo_sketch_destroy(s);
    histo_sketch_destroy(restored);
}

static void test_sketch_fractional_weights(void) {
    histo_sketch_t *s = histo_sketch_create(0.01, 1000);
    assert(s != NULL);

    /* Total weight = 0.1 + 0.2 + 0.1 + 0.1 = 0.5 (< 1.0) */
    histo_sketch_insert_w(s, 10.0, 0.1);
    histo_sketch_insert_w(s, 20.0, 0.2);
    histo_sketch_insert_w(s, 30.0, 0.1);
    histo_sketch_insert_w(s, 40.0, 0.1);

    assert(fabs(histo_sketch_total_weight(s) - 0.5) < 1e-9);

    double q_min = 0.0, q_25 = 0.0, q_50 = 0.0, q_75 = 0.0, q_max = 0.0;
    histo_status_t st0 = histo_sketch_quantile(s, 0.0, &q_min);
    histo_status_t st1 = histo_sketch_quantile(s, 0.25, &q_25);
    histo_status_t st2 = histo_sketch_quantile(s, 0.50, &q_50);
    histo_status_t st3 = histo_sketch_quantile(s, 0.75, &q_75);
    histo_status_t st4 = histo_sketch_quantile(s, 1.0, &q_max);

    assert(st0 == HISTO_OK && st1 == HISTO_OK && st2 == HISTO_OK && st3 == HISTO_OK && st4 == HISTO_OK);
    (void)st0; (void)st1; (void)st2; (void)st3; (void)st4;

    assert(fabs(q_min - 10.0) < 1e-9);
    assert(fabs(q_max - 40.0) < 1e-9);
    /* q_25 should resolve around 20.0, q_50 around 20.0, q_75 around 30.0 */
    assert(q_25 >= 10.0 * 0.98 && q_25 <= 20.0 * 1.02);
    assert(q_50 >= 20.0 * 0.98 && q_50 <= 20.0 * 1.02);
    assert(q_75 >= 30.0 * 0.98 && q_75 <= 40.0 * 1.02);
    (void)q_min; (void)q_25; (void)q_50; (void)q_75; (void)q_max;

    histo_sketch_destroy(s);
}

static void test_sketch_negative_symmetry(void) {
    histo_sketch_t *s = histo_sketch_create(0.005, 1000);
    assert(s != NULL);

    /* Insert dense symmetric points across [-100, 100] */
    for (int i = 1; i <= 200; ++i) {
        double v = (double)i * 0.5;
        histo_sketch_insert(s, -v);
        histo_sketch_insert(s, v);
    }

    double q_low = 0.0, q_high = 0.0, q_med = 0.0;
    /* 10th percentile vs 90th percentile, and 50th percentile (median) */
    histo_status_t st_low = histo_sketch_quantile(s, 0.10, &q_low);
    histo_status_t st_high = histo_sketch_quantile(s, 0.90, &q_high);
    histo_status_t st_med = histo_sketch_quantile(s, 0.50, &q_med);

    assert(st_low == HISTO_OK && st_high == HISTO_OK && st_med == HISTO_OK);
    (void)st_low; (void)st_high; (void)st_med;

    /* Check symmetry: q_low ≈ -q_high within relative tolerance */
    assert(q_low < 0.0 && q_high > 0.0);
    double rel_diff = fabs(fabs(q_low) - q_high) / q_high;
    assert(rel_diff < 0.02);

    /* Median should be close to 0 */
    assert(fabs(q_med) < 1.0);

    (void)rel_diff; (void)q_low; (void)q_high; (void)q_med;

    histo_sketch_destroy(s);
}

int main(void) {
    test_sketch_basic();
    test_sketch_merge();
    test_sketch_serialization();
    test_sketch_fractional_weights();
    test_sketch_negative_symmetry();
    printf("All sketch tests passed.\n");
    return 0;
}

