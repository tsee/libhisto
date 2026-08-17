#include "cli_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

static void print_stats_usage(void) {
    printf("Usage: histo-stats [OPTIONS] [HISTOGRAM_FILE...]\n");
    printf("       histo stats [OPTIONS] [HISTOGRAM_FILE...]\n\n");
    printf("Displays comprehensive statistical summary, moments, and robust metrics.\n\n");
    printf("Options:\n");
    printf("  -f, --format=<FMT>       Output format: table (default), json, tsv\n");
    printf("  -a, --all                Compute all extended higher moments and peak metrics\n");
    printf("  -h, --help               Show this help message\n");
}

static void print_histogram_stats(const histo_t *h, const char *fmt) {
    if (!h) return;

    double total_w = histo_total_weight(h);
    uint64_t n_entries = histo_num_entries(h);
    uint32_t nbins = histo_nbins(h);
    double range_min = 0.0, range_max = 0.0;
    histo_range(h, &range_min, &range_max);

    double mean = 0.0, var = 0.0, sdev = 0.0;
    double skew = 0.0, kurt = 0.0, ex_kurt = 0.0;
    double q05 = 0.0, q25 = 0.0, median = 0.0, q75 = 0.0, q95 = 0.0, iqr = 0.0, mad = 0.0;
    double t_mean = 0.0, w_mean = 0.0;
    uint32_t mode_bin = 0;
    double mode_cont = 0.0, fwhm = 0.0, rms = 0.0;

    if (total_w > 0.0) {
        histo_mean(h, &mean);
        histo_variance(h, &var);
        histo_std_dev(h, &sdev);
        histo_skewness(h, &skew);
        histo_kurtosis(h, &kurt);
        histo_excess_kurtosis(h, &ex_kurt);

        histo_quantile(h, 0.05, &q05);
        histo_quantile(h, 0.25, &q25);
        histo_median(h, &median);
        histo_quantile(h, 0.75, &q75);
        histo_quantile(h, 0.95, &q95);
        histo_iqr(h, &iqr);
        histo_mad(h, &mad);
        histo_trimmed_mean(h, 0.05, 0.95, &t_mean);
        histo_winsorized_mean(h, 0.05, 0.95, &w_mean);

        histo_mode_bin(h, &mode_bin);
        histo_mode_continuous(h, &mode_cont);
        histo_fwhm(h, &fwhm);
        histo_rms(h, &rms);
    }

    if (strcmp(fmt, "json") == 0) {
        printf("{\n");
        printf("  \"nbins\": %u,\n", nbins);
        printf("  \"min\": %.8g,\n", range_min);
        printf("  \"max\": %.8g,\n", range_max);
        printf("  \"entries\": %llu,\n", (unsigned long long)n_entries);
        printf("  \"total_weight\": %.8g,\n", total_w);
        printf("  \"underflow\": %.8g,\n", histo_underflow(h));
        printf("  \"overflow\": %.8g,\n", histo_overflow(h));
        printf("  \"nan_count\": %llu,\n", (unsigned long long)histo_nan_count(h));
        printf("  \"mean\": %.8g,\n", mean);
        printf("  \"variance\": %.8g,\n", var);
        printf("  \"std_dev\": %.8g,\n", sdev);
        printf("  \"skewness\": %.8g,\n", skew);
        printf("  \"kurtosis\": %.8g,\n", kurt);
        printf("  \"excess_kurtosis\": %.8g,\n", ex_kurt);
        printf("  \"median\": %.8g,\n", median);
        printf("  \"q25\": %.8g,\n", q25);
        printf("  \"q75\": %.8g,\n", q75);
        printf("  \"iqr\": %.8g,\n", iqr);
        printf("  \"mad\": %.8g,\n", mad);
        printf("  \"trimmed_mean\": %.8g,\n", t_mean);
        printf("  \"winsorized_mean\": %.8g,\n", w_mean);
        printf("  \"mode_bin\": %u,\n", mode_bin);
        printf("  \"mode_continuous\": %.8g,\n", mode_cont);
        printf("  \"fwhm\": %.8g,\n", fwhm);
        printf("  \"rms\": %.8g\n", rms);
        printf("}\n");
    } else if (strcmp(fmt, "tsv") == 0) {
        printf("metric\tvalue\n");
        printf("nbins\t%u\n", nbins);
        printf("min\t%.8g\n", range_min);
        printf("max\t%.8g\n", range_max);
        printf("entries\t%llu\n", (unsigned long long)n_entries);
        printf("total_weight\t%.8g\n", total_w);
        printf("mean\t%.8g\n", mean);
        printf("variance\t%.8g\n", var);
        printf("std_dev\t%.8g\n", sdev);
        printf("skewness\t%.8g\n", skew);
        printf("kurtosis\t%.8g\n", kurt);
        printf("excess_kurtosis\t%.8g\n", ex_kurt);
        printf("median\t%.8g\n", median);
        printf("iqr\t%.8g\n", iqr);
        printf("mad\t%.8g\n", mad);
        printf("trimmed_mean\t%.8g\n", t_mean);
        printf("winsorized_mean\t%.8g\n", w_mean);
        printf("mode_continuous\t%.8g\n", mode_cont);
        printf("fwhm\t%.8g\n", fwhm);
        printf("rms\t%.8g\n", rms);
    } else { /* Human-readable table */
        printf("===============================================================\n");
        printf("                    HISTOGRAM SUMMARY REPORT                   \n");
        printf("===============================================================\n");
        printf(" Geometry & Counts:\n");
        printf("   Bins:            %-12u Range: [%.4f, %.4f)\n", nbins, range_min, range_max);
        printf("   Entries:         %-12llu Total Weight: %.4g\n", (unsigned long long)n_entries, total_w);
        printf("   Underflow:       %-12.4g Overflow:     %.4g\n", histo_underflow(h), histo_overflow(h));
        printf("   Non-Finite / NaN:%-12llu\n\n", (unsigned long long)histo_nan_count(h));

        printf(" Central Moments & Shape:\n");
        printf("   Mean:            %-12.6g Std Deviation: %.6g\n", mean, sdev);
        printf("   Variance:        %-12.6g RMS:           %.6g\n", var, rms);
        printf("   Skewness:        %-12.6g Kurtosis:      %.6g\n", skew, kurt);
        printf("   Excess Kurtosis: %-12.6g\n\n", ex_kurt);

        printf(" Robust Non-Parametric Metrics:\n");
        printf("   Median (Q50):    %-12.6g IQR (Q75-Q25): %.6g\n", median, iqr);
        printf("   Q25:             %-12.6g Q75:           %.6g\n", q25, q75);
        printf("   MAD:             %-12.6g Trimmed Mean:  %.6g\n", mad, t_mean);
        printf("   Winsorized Mean: %-12.6g\n\n", w_mean);

        printf(" Peak & Width Estimation:\n");
        printf("   Mode (Bin):      %-12u Mode (Continuous): %.6g\n", mode_bin, mode_cont);
        printf("   FWHM:            %-12.6g\n", fwhm);
        printf("===============================================================\n");
    }
}

int cmd_stats_main(int argc, char **argv) {
    const char *fmt = "table";
    int file_start = argc;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_stats_usage();
            return 0;
        } else if (strncmp(arg, "-f=", 3) == 0) {
            fmt = arg + 3;
        } else if (strcmp(arg, "-f") == 0 && i + 1 < argc) {
            fmt = argv[++i];
        } else if (strncmp(arg, "--format=", 9) == 0) {
            fmt = arg + 9;
        } else if (strcmp(arg, "-a") == 0 || strcmp(arg, "--all") == 0) {
            /* all is default */
        } else if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "Unknown option '%s'. Run 'histo-stats --help' for usage.\n", arg);
            return 1;
        } else {
            file_start = i;
            break;
        }
    }

    int num_files = argc - file_start;
    const char *default_files[] = {"-"};
    const char **files = (num_files > 0) ? (const char **)(argv + file_start) : default_files;
    int nfiles = (num_files > 0) ? num_files : 1;

    for (int f = 0; f < nfiles; ++f) {
        histo_t *h = NULL;
        if (cli_read_histogram_from_file(files[f], &h) == HISTO_OK) {
            print_histogram_stats(h, fmt);
            histo_destroy(h);
        } else {
            fprintf(stderr, "Error: Failed to read histogram from '%s'\n", files[f]);
        }
    }

    return 0;
}
