#include "histo/histo.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_SAMPLES 10000000 /* 10 Million samples */

static inline uint32_t simple_prng(uint32_t *state) {
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

int main(void) {
    printf("=====================================================\n");
    printf(" libhisto Performance Benchmark (10M Operations)\n");
    printf("=====================================================\n\n");

    /* Allocate test data */
    double *data = (double *)malloc(NUM_SAMPLES * sizeof(double));
    double *weights = (double *)malloc(NUM_SAMPLES * sizeof(double));
    if (!data || !weights) {
        fprintf(stderr, "Failed to allocate benchmark data buffers\n");
        return 1;
    }

    uint32_t rng = 1337;
    for (size_t i = 0; i < NUM_SAMPLES; ++i) {
        data[i] = (double)(simple_prng(&rng) % 100000) * 0.001; /* [0.0, 100.0) */
        weights[i] = 1.0;
    }

    /* 1. Benchmark Uniform Fill */
    {
        histo_t *h = histo_create_uniform(100, 0.0, 100.0, HISTO_FLAG_NONE);
        clock_t start = clock();
        for (size_t i = 0; i < NUM_SAMPLES; ++i) {
            histo_fill(h, data[i]);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double mops = ((double)NUM_SAMPLES / elapsed_sec) / 1e6;
        printf("1. Uniform Single Fill (histo_fill):\n");
        printf("   - Time: %.3f s | Throughput: %.2f Mops/s (%.2f ns/op)\n\n",
               elapsed_sec, mops, (elapsed_sec / NUM_SAMPLES) * 1e9);
        histo_destroy(h);
    }

    /* 2. Benchmark Uniform Fill with sum_w2 tracking */
    {
        histo_t *h = histo_create_uniform(100, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
        clock_t start = clock();
        for (size_t i = 0; i < NUM_SAMPLES; ++i) {
            histo_fill_w(h, data[i], weights[i]);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double mops = ((double)NUM_SAMPLES / elapsed_sec) / 1e6;
        printf("2. Uniform Weighted Fill (histo_fill_w + sum_w2):\n");
        printf("   - Time: %.3f s | Throughput: %.2f Mops/s (%.2f ns/op)\n\n",
               elapsed_sec, mops, (elapsed_sec / NUM_SAMPLES) * 1e9);
        histo_destroy(h);
    }

    /* 3. Benchmark Batch Fill */
    {
        histo_t *h = histo_create_uniform(100, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
        clock_t start = clock();
        histo_fill_n(h, NUM_SAMPLES, data, weights);
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double mops = ((double)NUM_SAMPLES / elapsed_sec) / 1e6;
        printf("3. Batch Ingestion (histo_fill_n):\n");
        printf("   - Time: %.3f s | Throughput: %.2f Mops/s (%.2f ns/op)\n\n",
               elapsed_sec, mops, (elapsed_sec / NUM_SAMPLES) * 1e9);
        histo_destroy(h);
    }

    /* 4. Benchmark Variable Bin Binary Search Fill */
    {
        double edges[101];
        for (int i = 0; i <= 100; ++i) {
            edges[i] = (double)i;
        }
        histo_t *h = histo_create_variable(100, edges, HISTO_FLAG_NONE);
        clock_t start = clock();
        for (size_t i = 0; i < NUM_SAMPLES; ++i) {
            histo_fill(h, data[i]);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double mops = ((double)NUM_SAMPLES / elapsed_sec) / 1e6;
        printf("4. Variable Bin Fill (Binary Search 100 bins):\n");
        printf("   - Time: %.3f s | Throughput: %.2f Mops/s (%.2f ns/op)\n\n",
               elapsed_sec, mops, (elapsed_sec / NUM_SAMPLES) * 1e9);
        histo_destroy(h);
    }

    /* 5. Benchmark Binary Serialization */
    {
        histo_t *h = histo_create_uniform(1000, 0.0, 1000.0, HISTO_FLAG_TRACK_SUMW2);
        for (int i = 0; i < 1000; ++i) {
            histo_fill_bin(h, (uint32_t)i, 5.0);
        }
        void *buf = NULL;
        size_t size = 0;
        clock_t start = clock();
        for (int i = 0; i < 100000; ++i) {
            histo_serialize_binary(h, &buf, &size);
            histo_free_buffer(buf);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double mops = ((double)100000 / elapsed_sec) / 1e3;
        printf("5. Binary Serialization (1000 bins, 100k cycles):\n");
        printf("   - Time: %.3f s | Throughput: %.2f k-ser/s\n\n",
               elapsed_sec, mops);
        histo_destroy(h);
    }

    free(data);
    free(weights);
    printf("=====================================================\n");
    return 0;
}
