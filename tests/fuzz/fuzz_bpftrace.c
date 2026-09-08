/*
 * LibFuzzer target for bpftrace ASCII histogram parsing and stream detection.
 */

#include "histo/histo.h"
#include "cli_common.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size == 0 || size > 65536) {
        return 0;
    }

    char *buf = (char *)malloc(size + 1);
    if (!buf) {
        return 0;
    }
    memcpy(buf, data, size);
    buf[size] = '\0';

    histo_t *h = NULL;
    histo_status_t st = cli_parse_bpftrace_histogram_str(buf, &h);
    if (st == HISTO_OK && h != NULL) {
        uint32_t nbins = histo_nbins(h);
        double total_w = histo_total_weight(h);
        uint64_t n_fills = histo_num_entries(h);
        (void)total_w;
        (void)n_fills;

        if (nbins > 0) {
            double c = 0.0, low = 0.0, high = 0.0;
            histo_bin_content(h, 0, &c);
            histo_bin_bounds(h, 0, &low, &high);
            histo_bin_content(h, nbins - 1, &c);
            histo_bin_bounds(h, nbins - 1, &low, &high);
            (void)c;
            (void)low;
            (void)high;
        }

        double mean_v = 0.0, var_v = 0.0;
        histo_mean(h, &mean_v);
        histo_variance(h, &var_v);
        (void)mean_v;
        (void)var_v;

        histo_destroy(h);
    }

    free(buf);
    return 0;
}
