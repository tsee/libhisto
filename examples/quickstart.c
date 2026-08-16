#include <stdio.h>
#include <math.h>
#include "histo/histo.h"

int main(void) {
    printf("=== libhisto Quickstart Example ===\n\n");

    /* 1. Create a uniform histogram with 10 bins on [0, 100] */
    histo_t *h = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    if (!h) {
        fprintf(stderr, "Failed to create histogram\n");
        return 1;
    }

    /* 2. Ingest samples */
    printf("Filling 1000 pseudo-Gaussian samples...\n");
    for (int i = 0; i < 1000; ++i) {
        /* Deterministic sample centered at 50 with spread ~15 */
        double u1 = (double)((i * 37) % 1000) / 1000.0;
        double u2 = (double)((i * 73) % 1000) / 1000.0;
        if (u1 <= 0.0) u1 = 0.001;
        double z = sqrt(-2.0 * log(u1)) * cos(2.0 * 3.1415926535 * u2);
        double sample = 50.0 + z * 15.0;
        histo_fill(h, sample);
    }

    /* 3. Compute statistics */
    histo_stats_t stats;
    if (histo_get_stats(h, &stats) == HISTO_OK) {
        printf("\nHistogram Statistics:\n");
        printf("  - Total entries : %llu\n", (unsigned long long)stats.n_entries);
        printf("  - Total weight  : %.2f\n", stats.total_weight);
        printf("  - Mean          : %.3f\n", stats.mean);
        printf("  - Std Dev       : %.3f\n", stats.std_dev);
        printf("  - Median        : %.3f\n", stats.median);
        printf("  - Min Sample    : %.3f\n", stats.min);
        printf("  - Max Sample    : %.3f\n", stats.max);
    }

    /* 4. Print ASCII bin distribution */
    printf("\nBin Distribution:\n");
    for (uint32_t i = 0; i < histo_nbins(h); ++i) {
        double low = 0.0, high = 0.0, count = 0.0, err = 0.0;
        histo_bin_bounds(h, i, &low, &high);
        histo_bin_content(h, i, &count);
        histo_bin_error(h, i, &err);

        printf("  [%5.1f, %5.1f): %6.1f +- %4.1f | ", low, high, count, err);
        int bar_len = (int)(count / 10.0);
        for (int b = 0; b < bar_len; ++b) putchar('#');
        putchar('\n');
    }

    /* 5. Serialization to Binary Format */
    void *buf = NULL;
    size_t size = 0;
    if (histo_serialize_binary(h, &buf, &size) == HISTO_OK) {
        printf("\nSerialized to %zu bytes canonical Little-Endian binary format.\n", size);

        /* Deserialize back */
        histo_t *reloaded = NULL;
        if (histo_deserialize_binary(buf, size, &reloaded) == HISTO_OK) {
            printf("Successfully deserialized back: total weight = %.2f\n", histo_total_weight(reloaded));
            histo_destroy(reloaded);
        }
        histo_free_buffer(buf);
    }

    /* 6. Clean up */
    histo_destroy(h);
    printf("\nDone! Clean destruction with 0 leaks.\n");
    return 0;

}
