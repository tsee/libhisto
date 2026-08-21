/*
 * Comprehensive benchmark suite measuring throughput of libhisto operations.
 */

#include "histo/histo.h"
#include "histo/sketch.h"
#include <stdio.h>
#include "histo/histo2d.h"
#include <stdlib.h>
#include <time.h>

#define NUM_SAMPLES 10000000     /* 10 Million samples */
#define AGG_ITERATIONS 10000     /* 10k iterations on 10k bins (100 Million bin operations) */
#define FAST_ITERATIONS 1000000  /* 1 Million iterations for fast accessors */

static inline uint32_t simple_prng(uint32_t *state) {
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

int main(void) {
    printf("=====================================================\n");
    printf(" libhisto Comprehensive Performance Benchmark\n");
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
        printf("1. Uniform Single Fill (histo_fill, 10M ops):\n");
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
        printf("2. Uniform Weighted Fill (histo_fill_w + sum_w2, 10M ops):\n");
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
        printf("3. Batch Ingestion (histo_fill_n, 10M ops):\n");
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
        printf("4. Variable Bin Fill (Binary Search 100 bins, 10M ops):\n");
        printf("   - Time: %.3f s | Throughput: %.2f Mops/s (%.2f ns/op)\n\n",
               elapsed_sec, mops, (elapsed_sec / NUM_SAMPLES) * 1e9);
        histo_destroy(h);
    }

    /* Setup populated histogram for aggregate benchmarks (10,000 bins) */
    histo_t *h_agg = histo_create_uniform(10000, 0.0, 10000.0, HISTO_FLAG_TRACK_SUMW2);
    for (uint32_t i = 0; i < 10000; ++i) {
        histo_fill_bin(h_agg, i, 1.0 + (double)(i % 50));
    }

    /* 5. Benchmark Statistical Moments (Two-Pass on 10,000 bins) */
    {
        double mean = 0.0, var = 0.0;
        clock_t start = clock();
        for (int i = 0; i < AGG_ITERATIONS; ++i) {
            histo_mean(h_agg, &mean);
            histo_variance(h_agg, &var);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double kops = ((double)AGG_ITERATIONS / elapsed_sec) / 1e3;
        printf("5. Two-Pass Statistical Moments (Mean + Variance, 10k bins, 10k calls):\n");
        printf("   - Time: %.3f s | Throughput: %.2f k-queries/s (%.2f µs/query)\n\n",
               elapsed_sec, kops, (elapsed_sec / AGG_ITERATIONS) * 1e6);
    }

    /* 6. Benchmark Quantile and Median Evaluation */
    {
        double q25 = 0.0, median = 0.0, q75 = 0.0;
        clock_t start = clock();
        for (int i = 0; i < AGG_ITERATIONS; ++i) {
            histo_quantile(h_agg, 0.25, &q25);
            histo_median(h_agg, &median);
            histo_quantile(h_agg, 0.75, &q75);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double kops = ((double)AGG_ITERATIONS / elapsed_sec) / 1e3;
        printf("6. Quantile & Median Evaluation (Q25 + Q50 + Q75, 10k bins, 10k calls):\n");
        printf("   - Time: %.3f s | Throughput: %.2f k-queries/s (%.2f µs/query)\n\n",
               elapsed_sec, kops, (elapsed_sec / AGG_ITERATIONS) * 1e6);
    }

    /* 7. Benchmark Range Integral */
    {
        double integral = 0.0;
        clock_t start = clock();
        for (int i = 0; i < FAST_ITERATIONS; ++i) {
            histo_integral(h_agg, 1000, 8000, &integral);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double mops = ((double)FAST_ITERATIONS / elapsed_sec) / 1e6;
        printf("7. Range Integral (7,000-bin slice, 1M calls):\n");
        printf("   - Time: %.3f s | Throughput: %.2f Mops/s (%.2f ns/op)\n\n",
               elapsed_sec, mops, (elapsed_sec / FAST_ITERATIONS) * 1e9);
    }

    /* 8. Benchmark Element-Wise Vector Arithmetic (histo_add) */
    {
        histo_t *h_dest = histo_clone(h_agg, false);
        clock_t start = clock();
        for (int i = 0; i < AGG_ITERATIONS; ++i) {
            histo_add(h_dest, h_agg);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double kops = ((double)AGG_ITERATIONS / elapsed_sec) / 1e3;
        printf("8. Vector Histogram Addition (histo_add, 10k bins, 10k calls):\n");
        printf("   - Time: %.3f s | Throughput: %.2f k-ops/s (%.2f µs/op)\n\n",
               elapsed_sec, kops, (elapsed_sec / AGG_ITERATIONS) * 1e6);
        histo_destroy(h_dest);
    }

    /* 9. Benchmark Vector Scaling & Normalization */
    {
        histo_t *h_dest = histo_clone(h_agg, false);
        clock_t start = clock();
        for (int i = 0; i < AGG_ITERATIONS; ++i) {
            histo_scale(h_dest, 1.001);
            histo_normalize(h_dest, 1000.0);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double kops = ((double)AGG_ITERATIONS / elapsed_sec) / 1e3;
        printf("9. Vector Scaling & Normalization (10k bins, 10k calls):\n");
        printf("   - Time: %.3f s | Throughput: %.2f k-ops/s (%.2f µs/op)\n\n",
               elapsed_sec, kops, (elapsed_sec / AGG_ITERATIONS) * 1e6);
        histo_destroy(h_dest);
    }

    /* 10. Benchmark CDF Generation */
    {
        clock_t start = clock();
        for (int i = 0; i < AGG_ITERATIONS; ++i) {
            histo_t *cdf = histo_cdf(h_agg, 1.0);
            histo_destroy(cdf);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double kops = ((double)AGG_ITERATIONS / elapsed_sec) / 1e3;
        printf("10. Cumulative Distribution Function (CDF generation, 10k bins, 10k calls):\n");
        printf("    - Time: %.3f s | Throughput: %.2f k-ops/s (%.2f µs/op)\n\n",
               elapsed_sec, kops, (elapsed_sec / AGG_ITERATIONS) * 1e6);
    }

    /* 11. Benchmark Binary Serialization */
    {
        void *buf = NULL;
        size_t size = 0;
        clock_t start = clock();
        for (int i = 0; i < AGG_ITERATIONS; ++i) {
            histo_serialize_binary(h_agg, &buf, &size);
            histo_free_buffer(buf);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double kops = ((double)AGG_ITERATIONS / elapsed_sec) / 1e3;
        printf("11. Binary Serialization (10k bins, 10k cycles):\n");
        printf("    - Time: %.3f s | Throughput: %.2f k-ser/s (%.2f µs/ser)\n\n",
               elapsed_sec, kops, (elapsed_sec / AGG_ITERATIONS) * 1e6);
    }

    /* 12. Benchmark Higher-Order Moments & Shape Metrics */
    {
        double skew = 0.0, kurt = 0.0, mode = 0.0, fwhm = 0.0, rms = 0.0;
        clock_t start = clock();
        for (int i = 0; i < AGG_ITERATIONS; ++i) {
            histo_skewness(h_agg, &skew);
            histo_kurtosis(h_agg, &kurt);
            histo_mode_continuous(h_agg, &mode);
            histo_fwhm(h_agg, &fwhm);
            histo_rms(h_agg, &rms);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double kops = ((double)AGG_ITERATIONS / elapsed_sec) / 1e3;
        printf("12. Higher-Order Moments & Peak Analysis (Skew, Kurt, Mode, FWHM, RMS, 10k bins, 10k calls):\n");
        printf("    - Time: %.3f s | Throughput: %.2f k-evals/s (%.2f µs/eval)\n\n",
               elapsed_sec, kops, (elapsed_sec / AGG_ITERATIONS) * 1e6);
    }

    /* 13. Benchmark Robust Dispersion (IQR & MAD) */
    {
        double iqr = 0.0, mad = 0.0, t_mean = 0.0, w_mean = 0.0;
        clock_t start = clock();
        for (int i = 0; i < AGG_ITERATIONS; ++i) {
            histo_iqr(h_agg, &iqr);
            histo_mad(h_agg, &mad);
            histo_trimmed_mean(h_agg, 0.1, 0.9, &t_mean);
            histo_winsorized_mean(h_agg, 0.1, 0.9, &w_mean);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double kops = ((double)AGG_ITERATIONS / elapsed_sec) / 1e3;
        printf("13. Robust Dispersion & Trimming (IQR, MAD, Trimmed & Winsorized Mean, 10k bins, 10k calls):\n");
        printf("    - Time: %.3f s | Throughput: %.2f k-evals/s (%.2f µs/eval)\n\n",
               elapsed_sec, kops, (elapsed_sec / AGG_ITERATIONS) * 1e6);
    }

    /* 14. Benchmark Two-Sample Comparison (Chi2, KS, Wasserstein, Bhattacharyya) */
    {
        histo_t *h_other = histo_clone(h_agg, false);
        histo_scale(h_other, 1.05);
        double chi2 = 0.0, ks = 0.0, wass = 0.0, bhatt = 0.0;
        uint32_t ndf = 0;

        clock_t start = clock();
        for (int i = 0; i < AGG_ITERATIONS; ++i) {
            histo_cmp_chi2(h_agg, h_other, &chi2, &ndf);
            histo_cmp_ks(h_agg, h_other, &ks);
            histo_cmp_wasserstein_1d(h_agg, h_other, &wass);
            histo_cmp_bhattacharyya(h_agg, h_other, &bhatt);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double kops = ((double)AGG_ITERATIONS / elapsed_sec) / 1e3;
        printf("14. Two-Sample Statistical Distances (Chi2, KS, Wasserstein, Bhattacharyya, 10k bins, 10k calls):\n");
        printf("    - Time: %.3f s | Throughput: %.2f k-cmps/s (%.2f µs/cmp)\n\n",
               elapsed_sec, kops, (elapsed_sec / AGG_ITERATIONS) * 1e6);
        histo_destroy(h_other);
    }

    /* 15. Benchmark DDSketch Quantile Sketch (Insertion & Queries) */
    {
        histo_sketch_t *sketch = histo_sketch_create(0.01, 10000);
        clock_t start = clock();
        for (size_t i = 0; i < NUM_SAMPLES; ++i) {
            histo_sketch_insert(sketch, data[i]);
        }
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double mops = ((double)NUM_SAMPLES / elapsed_sec) / 1e6;
        printf("15a. DDSketch Insertion (alpha=0.01, 10M ops):\n");
        printf("    - Time: %.3f s | Throughput: %.2f Mops/s (%.2f ns/op)\n\n",
               elapsed_sec, mops, (elapsed_sec / NUM_SAMPLES) * 1e9);
               
        double q_val = 0.0;
        start = clock();
        for (int i = 0; i < AGG_ITERATIONS; ++i) {
            histo_sketch_quantile(sketch, 0.5, &q_val);
            histo_sketch_quantile(sketch, 0.9, &q_val);
            histo_sketch_quantile(sketch, 0.99, &q_val);
        }
        end = clock();
        elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double kops = ((double)AGG_ITERATIONS / elapsed_sec) / 1e3;
        printf("15b. DDSketch Quantiles (P50, P90, P99, 10k calls):\n");
        printf("    - Time: %.3f s | Throughput: %.2f k-queries/s (%.2f µs/query)\n\n",
               elapsed_sec, kops, (elapsed_sec / AGG_ITERATIONS) * 1e6);
               
        histo_sketch_destroy(sketch);
    }

    /* 16. Benchmark 2D Batch Ingestion */
    {
        histo2d_t *h2 = histo2d_create_uniform(100, 0.0, 100.0, 100, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
        double *ydata = (double *)malloc(NUM_SAMPLES * sizeof(double));
        for (size_t i = 0; i < NUM_SAMPLES; ++i) {
            ydata[i] = data[i]; // just reuse data for y for simplicity
        }
        clock_t start = clock();
        histo2d_fill_n(h2, NUM_SAMPLES, data, ydata, weights);
        clock_t end = clock();
        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double mops = ((double)NUM_SAMPLES / elapsed_sec) / 1e6;
        printf("16. 2D Batch Ingestion (histo2d_fill_n, 10M ops):\n");
        printf("    - Time: %.3f s | Throughput: %.2f Mops/s (%.2f ns/op)\n\n",
               elapsed_sec, mops, (elapsed_sec / NUM_SAMPLES) * 1e9);
        histo2d_destroy(h2);
        free(ydata);
    }

    histo_destroy(h_agg);
    free(data);
    free(weights);
    printf("=====================================================\n");
    return 0;
}
