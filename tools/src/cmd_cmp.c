/*
 * CLI subcommand histo cmp: two-sample statistical distance computations.
 */

#include "cli_common.h"
#include "cli_opt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

int histo_cli_cmp(int argc, char **argv, FILE *out, FILE *err) {
    if (!out) out = stdout;
    if (!err) err = stderr;

    const char *fmt = "table";

    const cli_opt_spec_t specs[] = {
        {'f', "format", NULL, CLI_OPT_TYPE_STRING, &fmt, NULL, 0, "FMT",
         "Output format: table (default), json, tsv", "table"}
    };

    cli_opt_parser_t parser;
    cli_opt_init(&parser, specs, sizeof(specs) / sizeof(specs[0]),
                 "histo-cmp", "[OPTIONS] <HISTO_A> <HISTO_B>",
                 "Compares two histograms and computes statistical distance metrics and compatibility.");

    int rc = cli_opt_parse(&parser, argc, argv, 1);
    if (rc < 0) {
        cli_opt_print_help(&parser, out);
        cli_opt_free(&parser);
        return 0;
    }
    if (rc > 0) {
        fprintf(err, "%s\n", cli_opt_error(&parser));
        cli_opt_free(&parser);
        return 1;
    }

    if (parser.num_positionals < 2) {
        fprintf(err, "Error: Two histogram input paths are required.\n\n");
        cli_opt_print_help(&parser, err);
        cli_opt_free(&parser);
        return 1;
    }

    const char *path1 = parser.positionals[0];
    const char *path2 = parser.positionals[1];

    histo_t *h1 = NULL;
    histo_t *h2 = NULL;

    if (cli_read_histogram_from_file(path1, &h1) != HISTO_OK) {
        fprintf(err, "Error: Failed to read first histogram from '%s'\n", path1);
        cli_opt_free(&parser);
        return 1;
    }
    if (cli_read_histogram_from_file(path2, &h2) != HISTO_OK) {
        fprintf(err, "Error: Failed to read second histogram from '%s'\n", path2);
        histo_destroy(h1);
        cli_opt_free(&parser);
        return 1;
    }

    double chi2 = 0.0;
    uint32_t ndf = 0;
    double ks = 0.0;
    double w1 = 0.0;
    double kl = 0.0;
    double bhat = 0.0;

    histo_cmp_chi2(h1, h2, &chi2, &ndf);
    histo_cmp_ks(h1, h2, &ks);
    histo_cmp_wasserstein_1d(h1, h2, &w1);
    histo_cmp_kl_divergence(h1, h2, &kl);
    histo_cmp_bhattacharyya(h1, h2, &bhat);

    if (strcmp(fmt, "json") == 0) {
        fprintf(out, "{\n");
        fprintf(out, "  \"file_a\": \"%s\",\n", path1);
        fprintf(out, "  \"file_b\": \"%s\",\n", path2);
        fprintf(out, "  \"chi2\": %.6g,\n", chi2);
        fprintf(out, "  \"ndf\": %u,\n", ndf);
        fprintf(out, "  \"ks_distance\": %.6g,\n", ks);
        fprintf(out, "  \"wasserstein_1d\": %.6g,\n", w1);
        fprintf(out, "  \"kl_divergence\": %.6g,\n", kl);
        fprintf(out, "  \"bhattacharyya\": %.6g\n", bhat);
        fprintf(out, "}\n");
    } else if (strcmp(fmt, "tsv") == 0) {
        fprintf(out, "Metric\tValue\n");
        fprintf(out, "Chi2\t%.8g\n", chi2);
        fprintf(out, "NDF\t%u\n", ndf);
        fprintf(out, "KS\t%.8g\n", ks);
        fprintf(out, "Wasserstein\t%.8g\n", w1);
        fprintf(out, "KL\t%.8g\n", kl);
        fprintf(out, "Bhattacharyya\t%.8g\n", bhat);
    } else {
        fprintf(out, "===============================================================\n");
        fprintf(out, "  Two-Sample Histogram Compatibility & Distance Analysis\n");
        fprintf(out, "===============================================================\n");
        fprintf(out, "  Dataset A:        %s\n", path1);
        fprintf(out, "  Dataset B:        %s\n", path2);
        fprintf(out, "---------------------------------------------------------------\n");
        fprintf(out, "  Chi-Square (Chi2):      %-12.6g  NDF: %u\n", chi2, ndf);
        if (ndf > 0) {
            fprintf(out, "  Chi2 / NDF:             %-12.6g\n", chi2 / (double)ndf);
        }
        fprintf(out, "  Kolmogorov-Smirnov (KS):%-12.6g\n", ks);
        fprintf(out, "  Wasserstein (EMD 1D):   %-12.6g\n", w1);
        fprintf(out, "  KL Divergence (A || B): %-12.6g\n", kl);
        fprintf(out, "  Bhattacharyya Distance: %-12.6g\n", bhat);
        fprintf(out, "===============================================================\n");
    }

    histo_destroy(h1);
    histo_destroy(h2);
    cli_opt_free(&parser);
    return 0;
}
