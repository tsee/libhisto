#include "histo/sketch.h"
#include "histo/histo.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>

void test_sketch_basic() {
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

void test_sketch_merge() {
    histo_sketch_t *s1 = histo_sketch_create(0.01, 1000);
    histo_sketch_t *s2 = histo_sketch_create(0.01, 1000);
    
    for (int i = 1; i <= 100; i++) histo_sketch_insert(s1, i);
    for (int i = 101; i <= 200; i++) histo_sketch_insert(s2, i);
    
    histo_sketch_merge(s1, s2);
    assert(histo_sketch_num_entries(s1) == 200);
    
    histo_sketch_destroy(s1);
    histo_sketch_destroy(s2);
}

void test_sketch_serialization() {
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

int main() {
    test_sketch_basic();
    test_sketch_merge();
    test_sketch_serialization();
    printf("All sketch tests passed.\n");
    return 0;
}

