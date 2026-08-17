#include "histo/sketch.h"
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

int main() {
    test_sketch_basic();
    test_sketch_merge();
    printf("All sketch tests passed.\n");
    return 0;
}
