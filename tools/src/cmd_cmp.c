#include "cli_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

static void print_cmp_usage(void) {
    printf("Usage: histo-cmp [OPTIONS] <HISTO_A> <HISTO_B>\n");
    printf("       histo cmp [OPTIONS] <HISTO_A> <HISTO_B>\n\n");
    printf("Compares two histograms and computes statistical distance metrics and compatibility.\n\n");
    printf("Options:\n");
    printf("  -f, --format=<FMT>       Output format: table (default), json, tsv\n");
    printf("  -h, --help               Show this help message\n");
}

int cmd_cmp_main(int argc, char **argv) {
    const char *fmt = "table";
    int file_start = argc;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_cmp_usage();
            return 0;
        } else if (strncmp(arg, "-f=", 3) == 0) {
            fmt = arg + 3;
        } else if (strcmp(arg, "-f") == 0 && i + 1 < argc) {
            fmt = argv[++i];
        } else if (strncmp(arg, "--format=", 9) == 0) {
            fmt = arg + 9;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "Unknown option '%s'. Run 'histo-cmp --help' for usage.\n", arg);
            return 1;
        } else {
            file_start = i;
            break;
        }
    }

    if (argc - file_start < 2) {
        fprintf(stderr, "Error: Two histogram input paths are required.\n\n");
        print_cmp_usage();
        return 1;
    }

    const char *path1 = argv[file_start];
    const char *path2 = argv[file_start + 1];

    histo_t *h1 = NULL;
    histo_t *h2 = NULL;

    if (cli_read_histogram_from_file(path1, &h1) != HISTO_OK) {
        fprintf(stderr, "Error: Failed to read histogram from '%s'\n", path1);
        return 1;
    }
    if (cli_read_histogram_from_file(path2, &h2) != HISTO_OK) {
        fprintf(stderr, "Error: Failed to read histogram from '%s'\n", path2);
        histo_destroy(h1);
        return 1;
    }

    double chi2 = 0.0, ks = 0.0, wass = 0.0, kl_12 = 0.0, kl_21 = 0.0, bhatt = 0.0;
    uint32_t ndf = 0;

    histo_status_t st = histo_cmp_chi2(h1, h2, &chi2, &ndf);
    if (st == HISTO_ERR_INCOMPATIBLE) {
        fprintf(stderr, "Error: Histograms have incompatible binning geometry (bin count, range, or edges mismatch).\n");
        histo_destroy(h1);
        histo_destroy(h2);
        return 1;
    }

    histo_cmp_ks(h1, h2, &ks);
    histo_cmp_wasserstein_1d(h1, h2, &wass);
    histo_cmp_kl_divergence(h1, h2, &kl_12);
    histo_cmp_kl_divergence(h2, h1, &kl_21);
    histo_cmp_bhattacharyya(h1, h2, &bhatt);

    if (strcmp(fmt, "json") == 0) {
        printf("{\n");
        printf("  \"h1_entries\": %llu,\n", (unsigned long long)histo_num_entries(h1));
        printf("  \"h2_entries\": %llu,\n", (unsigned long long)histo_num_entries(h2));
        printf("  \"h1_weight\": %.8g,\n", histo_total_weight(h1));
        printf("  \"h2_weight\": %.8g,\n", histo_total_weight(h2));
        printf("  \"chi2\": %.8g,\n", chi2);
        printf("  \"ndf\": %u,\n", ndf);
        if (ndf > 0) printf("  \"chi2_ndf\": %.8g,\n", chi2 / (double)ndf);
        printf("  \"kolmogorov_smirnov_d\": %.8g,\n", ks);
        printf("  \"wasserstein_1d\": %.8g,\n", wass);
        printf("  \"kl_divergence_1_to_2\": %.8g,\n", kl_12);
        printf("  \"kl_divergence_2_to_1\": %.8g,\n", kl_21);
        printf("  \"bhattacharyya_distance\": %.8g\n", bhatt);
        printf("}\n");
    } else if (strcmp(fmt, "tsv") == 0) {
        printf("metric\tvalue\n");
        printf("chi2\t%.8g\n", chi2);
        printf("ndf\t%u\n", ndf);
        if (ndf > 0) printf("chi2_ndf\t%.8g\n", chi2 / (double)ndf);
        printf("kolmogorov_smirnov_d\t%.8g\n", ks);
        printf("wasserstein_1d\t%.8g\n", wass);
        printf("kl_divergence_1_to_2\t%.8g\n", kl_12);
        printf("kl_divergence_2_to_1\t%.8g\n", kl_21);
        printf("bhattacharyya_distance\t%.8g\n", bhatt);
    } else {
        printf("===============================================================\n");
        printf("              TWO-DISTRIBUTION COMPARISON REPORT               \n");
        printf("===============================================================\n");
        printf(" Source A: %s (Entries: %llu, Weight: %.4g)\n", path1,
               (unsigned long long)histo_num_entries(h1), histo_total_weight(h1));
        printf(" Source B: %s (Entries: %llu, Weight: %.4g)\n\n", path2,
               (unsigned long long)histo_num_entries(h2), histo_total_weight(h2));

        printf(" Hypothesis Testing & Compatibility:\n");
        printf("   Chi-Square (chi^2):       %-12.6g NDF: %u\n", chi2, ndf);
        if (ndf > 0) {
            printf("   chi^2 / NDF:             %-12.6g\n", chi2 / (double)ndf);
        }
        printf("   Kolmogorov-Smirnov (D):  %-12.6g (Max vertical CDF difference)\n\n", ks);

        printf(" Statistical Distance Metrics:\n");
        printf("   1D Wasserstein (EMD):    %-12.6g (L1 CDF transportation cost)\n", wass);
        printf("   KL Divergence (A || B):  %-12.6g nats\n", kl_12);
        printf("   KL Divergence (B || A):  %-12.6g nats\n", kl_21);
        printf("   Bhattacharyya Distance:  %-12.6g\n", bhatt);
        printf("===============================================================\n");
    }

    histo_destroy(h1);
    histo_destroy(h2);
    return 0;
}
