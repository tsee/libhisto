/*
 * LibFuzzer target for DDSketch binary deserialization and state corruption.
 */

#include "histo/sketch.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size == 0) {
        return 0;
    }

    histo_sketch_t *sketch = NULL;
    histo_status_t st = histo_sketch_deserialize_binary(data, size, &sketch);
    if (st == HISTO_OK && sketch != NULL) {
        double min_v = histo_sketch_min(sketch);
        double max_v = histo_sketch_max(sketch);
        double total_w = histo_sketch_total_weight(sketch);
        uint64_t n_entries = histo_sketch_num_entries(sketch);
        (void)min_v; (void)max_v; (void)total_w; (void)n_entries;

        /* Quantile queries across distribution boundaries */
        double q_val = 0.0;
        histo_sketch_quantile(sketch, 0.0, &q_val);
        histo_sketch_quantile(sketch, 0.01, &q_val);
        histo_sketch_quantile(sketch, 0.5, &q_val);
        histo_sketch_quantile(sketch, 0.99, &q_val);
        histo_sketch_quantile(sketch, 1.0, &q_val);
        histo_sketch_quantile(sketch, -0.5, &q_val); /* Should fail safely */
        histo_sketch_quantile(sketch, 1.5, &q_val);  /* Should fail safely */

        /* Insert sample mutations */
        histo_sketch_insert(sketch, 42.0);
        histo_sketch_insert_w(sketch, -100.5, 3.5);
        histo_sketch_insert_w(sketch, 0.0, 1.0);

        /* Binary serialization */
        void *out_buf = NULL;
        size_t out_sz = 0;
        if (histo_sketch_serialize_binary(sketch, &out_buf, &out_sz) == HISTO_OK && out_buf) {
            histo_sketch_t *sketch2 = NULL;
            if (histo_sketch_deserialize_binary(out_buf, out_sz, &sketch2) == HISTO_OK && sketch2) {
                histo_sketch_destroy(sketch2);
            }
            free(out_buf);
        }

        /* Merge test with another sketch */
        histo_sketch_t *other = histo_sketch_create(0.01, 1024);
        if (other) {
            histo_sketch_insert(other, 123.456);
            histo_sketch_merge(sketch, other);
            histo_sketch_destroy(other);
        }

        histo_sketch_destroy(sketch);
    }

    return 0;
}
