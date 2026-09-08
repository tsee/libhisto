/*
 * LibFuzzer target for binary wire format roundtrip idempotence invariants.
 */

#include "histo/histo.h"
#include "histo/histo2d.h"
#include "histo/sketch.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size == 0) {
        return 0;
    }

    /* 1. 1D Histogram Roundtrip Invariant */
    histo_t *h1 = NULL;
    if (histo_deserialize_binary(data, size, &h1) == HISTO_OK && h1) {
        void *bufA = NULL;
        size_t szA = 0;
        if (histo_serialize_binary(h1, &bufA, &szA) == HISTO_OK && bufA) {
            histo_t *h2 = NULL;
            if (histo_deserialize_binary(bufA, szA, &h2) == HISTO_OK && h2) {
                void *bufB = NULL;
                size_t szB = 0;
                if (histo_serialize_binary(h2, &bufB, &szB) == HISTO_OK && bufB) {
                    /* Idempotent wire format assertion */
                    if (szA != szB || memcmp(bufA, bufB, szA) != 0) {
                        abort();
                    }
                    histo_free_buffer(bufB);
                }
                histo_destroy(h2);
            }
            histo_free_buffer(bufA);
        }
        histo_destroy(h1);
    }

    /* 2. 2D Histogram Roundtrip Invariant */
    histo2d_t *h2d_1 = NULL;
    if (histo2d_deserialize_binary(data, size, &h2d_1) == HISTO_OK && h2d_1) {
        void *bufA = NULL;
        size_t szA = 0;
        if (histo2d_serialize_binary_alloc(h2d_1, &bufA, &szA) == HISTO_OK && bufA) {
            histo2d_t *h2d_2 = NULL;
            if (histo2d_deserialize_binary(bufA, szA, &h2d_2) == HISTO_OK && h2d_2) {
                void *bufB = NULL;
                size_t szB = 0;
                if (histo2d_serialize_binary_alloc(h2d_2, &bufB, &szB) == HISTO_OK && bufB) {
                    if (szA != szB || memcmp(bufA, bufB, szA) != 0) {
                        abort();
                    }
                    histo_free_buffer(bufB);
                }
                histo2d_destroy(h2d_2);
            }
            histo_free_buffer(bufA);
        }
        histo2d_destroy(h2d_1);
    }

    /* 3. DDSketch Roundtrip Invariant */
    histo_sketch_t *sk1 = NULL;
    if (histo_sketch_deserialize_binary(data, size, &sk1) == HISTO_OK && sk1) {
        void *bufA = NULL;
        size_t szA = 0;
        if (histo_sketch_serialize_binary(sk1, &bufA, &szA) == HISTO_OK && bufA) {
            histo_sketch_t *sk2 = NULL;
            if (histo_sketch_deserialize_binary(bufA, szA, &sk2) == HISTO_OK && sk2) {
                void *bufB = NULL;
                size_t szB = 0;
                if (histo_sketch_serialize_binary(sk2, &bufB, &szB) == HISTO_OK && bufB) {
                    if (szA != szB || memcmp(bufA, bufB, szA) != 0) {
                        abort();
                    }
                    histo_free_buffer(bufB);
                }
                histo_sketch_destroy(sk2);
            }
            histo_free_buffer(bufA);
        }
        histo_sketch_destroy(sk1);
    }

    return 0;
}
