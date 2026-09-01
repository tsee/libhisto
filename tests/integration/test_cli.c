/*
 * Integration tests for CLI subcommands, input pipelines, and JSON output.
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "unity.h"
#include "histo/histo.h"
#include "histo/histo2d.h"
#include "histo/version.h"
#include "cli_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

/* Forward declarations of command entrypoints if linking tools object */
extern int cmd_fill_main(int argc, char **argv);
extern int cmd_plot_main(int argc, char **argv);
extern int cmd_stats_main(int argc, char **argv);
extern int cmd_cmp_main(int argc, char **argv);
extern int cmd_top_main(int argc, char **argv);

void test_json_serialization_roundtrip_uniform(void) {
    histo_t *h = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);

    for (int i = 1; i <= 100; ++i) {
        histo_fill_w(h, (double)i, 1.5);
    }
    histo_fill(h, -5.0); /* Underflow */
    histo_fill(h, 105.0); /* Overflow */
    histo_fill(h, NAN); /* Non-finite */

    char *json = NULL;
    histo_status_t st = histo_serialize_json(h, &json);
    TEST_ASSERT_EQUAL_INT(HISTO_OK, st);
    TEST_ASSERT_NOT_NULL(strstr(json, "\"schema\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"libhisto-v2\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"nbins\""));

    histo_t *deser = NULL;
    st = histo_deserialize_json(json, &deser);
    TEST_ASSERT_EQUAL_INT(HISTO_OK, st);
    TEST_ASSERT_NOT_NULL(deser);

    TEST_ASSERT_EQUAL_UINT32(histo_nbins(h), histo_nbins(deser));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, histo_total_weight(h), histo_total_weight(deser));
    TEST_ASSERT_EQUAL_UINT64(histo_num_entries(h), histo_num_entries(deser));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, histo_underflow(h), histo_underflow(deser));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, histo_overflow(h), histo_overflow(deser));
    TEST_ASSERT_EQUAL_UINT64(histo_nan_count(h), histo_nan_count(deser));

    double m1 = 0.0, m2 = 0.0;
    histo_mean(h, &m1);
    histo_mean(deser, &m2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, m1, m2);

    for (uint32_t i = 0; i < 10; ++i) {
        double c1 = 0.0, c2 = 0.0;
        histo_bin_content(h, i, &c1);
        histo_bin_content(deser, i, &c2);
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, c1, c2);
    }

    histo_free_buffer(json);
    histo_destroy(h);
    histo_destroy(deser);
}

void test_json_serialization_roundtrip_variable(void) {
    double edges[] = {0.0, 5.0, 15.0, 30.0, 60.0, 100.0};
    histo_t *h = histo_create_variable(5, edges, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    histo_fill(h, 2.0);
    histo_fill_w(h, 10.0, 3.0);
    histo_fill_w(h, 25.0, 2.5);
    histo_fill(h, 50.0);
    histo_fill(h, 80.0);

    char *json = NULL;
    histo_status_t st = histo_serialize_json(h, &json);
    TEST_ASSERT_EQUAL_INT(HISTO_OK, st);
    TEST_ASSERT_NOT_NULL(json);

    histo_t *deser = NULL;
    st = histo_deserialize_json(json, &deser);
    TEST_ASSERT_EQUAL_INT(HISTO_OK, st);
    TEST_ASSERT_NOT_NULL(deser);
    TEST_ASSERT_EQUAL_INT(HISTO_BIN_VARIABLE, histo_bin_type(deser));
    TEST_ASSERT_EQUAL_UINT32(5, histo_nbins(deser));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, histo_total_weight(h), histo_total_weight(deser));

    histo_free_buffer(json);
    histo_destroy(h);
    histo_destroy(deser);
}

void test_json_corrupted_payload(void) {
    histo_t *h = NULL;
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_INVALID_ARG, histo_deserialize_json(NULL, &h));
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_DESERIALIZATION, histo_deserialize_json("{}", &h));
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_DESERIALIZATION, histo_deserialize_json("{\"nbins\": 0}", &h));
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_DESERIALIZATION, histo_deserialize_json("not valid json", &h));

    /* Missing mandatory keys */
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_DESERIALIZATION, histo_deserialize_json("{\"schema\": \"libhisto-v2\", \"nbins\": 10}", &h));
    /* Invalid types (string instead of array/number) */
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_DESERIALIZATION, histo_deserialize_json("{\"schema\": \"libhisto-v2\", \"bin_type\": 0, \"nbins\": \"10\", \"edges\": []}", &h));
    /* Truncated JSON */
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_DESERIALIZATION, histo_deserialize_json("{\"schema\": \"libhisto-v2\", \"nbins\": ", &h));
}

static unsigned int g_cli_tmp_counter = 0;

static void get_unique_tmp_path(char *buf, size_t bufsz, const char *tag, const char *ext) {
    snprintf(buf, bufsz, "/tmp/histo_test_cli_%d_%u_%s%s", (int)getpid(), ++g_cli_tmp_counter, tag, ext ? ext : "");
}

void test_cli_execution_pipelines(void) {
    /* Test command-line help flags */
    char *help_argv[] = {"histo", "--help"};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fill_main(2, help_argv));
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_plot_main(2, help_argv));
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_stats_main(2, help_argv));
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_cmp_main(2, help_argv));

    /* Create temporary data file */
    char data_path[256];
    get_unique_tmp_path(data_path, sizeof(data_path), "cli_exec_data", ".txt");
    FILE *fp = fopen(data_path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 1; i <= 50; ++i) {
        fprintf(fp, "%d %f\n", i, 1.0);
    }
    fclose(fp);

    /* Test histo-fill writing binary output */
    char out_bin[256];
    get_unique_tmp_path(out_bin, sizeof(out_bin), "cli_exec_out", ".bin");
    char *fill_bin_argv[] = {
        "histo-fill", "--bins=10", "--min=0", "--max=50", "-w",
        "-o", "binary", "-f", out_bin, data_path
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fill_main(10, fill_bin_argv));

    /* Test histo-stats on binary output */
    char *stats_argv[] = {"histo-stats", "-f", "json", out_bin};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_stats_main(4, stats_argv));

    /* Test histo-plot on binary output */
    char *plot_argv[] = {"histo-plot", "-s", "ascii", "--no-stats", out_bin};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_plot_main(5, plot_argv));

    /* Test histo-plot with sparkline mode */
    char *sparkline_argv[] = {"histo-plot", "-S", out_bin};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_plot_main(3, sparkline_argv));

    char *sparkline_ascii_argv[] = {"histo-plot", "--sparkline", "-s", "ascii", "--no-stats", out_bin};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_plot_main(6, sparkline_ascii_argv));

    /* Create second dataset for comparison */
    char out_bin2[256];
    get_unique_tmp_path(out_bin2, sizeof(out_bin2), "cli_exec_out2", ".bin");
    char *fill_bin_argv2[] = {
        "histo-fill", "--bins=10", "--min=0", "--max=50",
        "-o", "binary", "-f", out_bin2, data_path
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fill_main(9, fill_bin_argv2));

    /* Test histo-cmp */
    char *cmp_argv[] = {"histo-cmp", "-f", "json", out_bin, out_bin2};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_cmp_main(5, cmp_argv));

    remove(data_path);
    remove(out_bin);
    remove(out_bin2);
}

extern int cmd_fit_main(int argc, char **argv);

void test_cli_fit_pipeline(void) {
    char *help_argv[] = {"histo-fit", "--help"};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(2, help_argv));

    /* Create sample gaussian data */
    char gauss_path[256];
    get_unique_tmp_path(gauss_path, sizeof(gauss_path), "cli_fit_gauss", ".txt");
    FILE *fp = fopen(gauss_path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 0; i < 500; ++i) {
        double u1 = (double)(i + 1) / 501.0;
        double u2 = (double)((i * 37) % 500 + 1) / 501.0;
        double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * 3.1415926535 * u2);
        double val = 50.0 + 5.0 * z0;
        fprintf(fp, "%.6f\n", val);
    }
    fclose(fp);

    /* Test default fit (Gaussian) */
    char *fit_default_argv[] = {"histo-fit", gauss_path};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(2, fit_default_argv));

    /* Test JSON fit */
    char *fit_json_argv[] = {"histo-fit", "--model=gaussian", "-j", gauss_path};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(4, fit_json_argv));

    /* Test quiet fit */
    char *fit_quiet_argv[] = {"histo-fit", "-q", gauss_path};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(3, fit_quiet_argv));

    /* Test plot fit */
    char *fit_plot_argv[] = {"histo-fit", "-p", gauss_path};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(3, fit_plot_argv));

    /* Test constrained fit with bounds and fix-param */
    char *fit_bounds_argv[] = {
        "histo-fit", "--bounds=2=1.0:10.0", "--fix-param=1=50.0", gauss_path
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(4, fit_bounds_argv));

    /* Test other models on serialized histogram */
    char out_hist[256];
    get_unique_tmp_path(out_hist, sizeof(out_hist), "cli_fit_h", ".json");
    char *fill_argv[] = {
        "histo-fill", "--bins=20", "--min=20", "--max=80",
        "-o", "json", "-f", out_hist, gauss_path
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fill_main(9, fill_argv));

    char *fit_poly_argv[] = {"histo-fit", "-m", "polynomial", "-d", "2", out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(6, fit_poly_argv));

    char *fit_exp_argv[] = {"histo-fit", "-m", "exponential", out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(4, fit_exp_argv));

    char *fit_bw_argv[] = {"histo-fit", "-m", "breit-wigner", out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(4, fit_bw_argv));

    char *fit_logn_argv[] = {"histo-fit", "-m", "lognormal", "-j", out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(5, fit_logn_argv));

    char *fit_gpoly_argv[] = {"histo-fit", "-m", "gauss+linear", out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(4, fit_gpoly_argv));

    char *fit_weibull_argv[] = {"histo-fit", "-m", "weibull", out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(4, fit_weibull_argv));

    char *fit_gamma_argv[] = {"histo-fit", "-m", "gamma", out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(4, fit_gamma_argv));

    char *fit_poisson_argv[] = {"histo-fit", "-m", "poisson", out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(4, fit_poisson_argv));

    char *fit_laplace_argv[] = {"histo-fit", "-m", "laplace", out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(4, fit_laplace_argv));

    char *fit_mle_argv[] = {"histo-fit", "--mle", out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(3, fit_mle_argv));

    /* Error handling */
    char *fit_err_argv[] = {"histo-fit", "-m", "invalid_model_name", out_hist};
    optind = 1; TEST_ASSERT_NOT_EQUAL_INT(0, cmd_fit_main(4, fit_err_argv));

    remove(gauss_path);
    remove(out_hist);
}

void test_cli_2d_fill_and_delimiters(void) {
    /* 1. Comma-separated CSV */
    char csv_path[256];
    get_unique_tmp_path(csv_path, sizeof(csv_path), "cli_2d", ".csv");
    FILE *fp = fopen(csv_path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 0; i < 100; ++i) {
        fprintf(fp, "%.2f,%.2f\n", (double)(i % 10), (double)(i / 10));
    }
    fclose(fp);

    char out_2d_json[256];
    get_unique_tmp_path(out_2d_json, sizeof(out_2d_json), "cli_2d", ".json");
    char *fill_csv_argv[] = {
        "histo-fill", "--2d", "--xbins", "10", "--xmin", "0", "--xmax", "10",
        "--ybins", "10", "--ymin", "0", "--ymax", "10",
        "-o", "json", "-f", out_2d_json, csv_path
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fill_main((int)(sizeof(fill_csv_argv) / sizeof(fill_csv_argv[0])), fill_csv_argv));

    /* 2. Tab-separated TSV with weights */
    char tsv_path[256];
    get_unique_tmp_path(tsv_path, sizeof(tsv_path), "cli_2d", ".tsv");
    fp = fopen(tsv_path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 0; i < 100; ++i) {
        fprintf(fp, "%.2f\t%.2f\t%.2f\n", (double)(i % 10), (double)(i / 10), 2.0);
    }
    fclose(fp);

    char out_2d_bin[256];
    get_unique_tmp_path(out_2d_bin, sizeof(out_2d_bin), "cli_2d", ".bin");
    char *fill_tsv_argv[] = {
        "histo-fill", "--2d", "-w", "-o", "binary", "-f", out_2d_bin, tsv_path
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fill_main((int)(sizeof(fill_tsv_argv) / sizeof(fill_tsv_argv[0])), fill_tsv_argv));

    /* Test histo-plot on 2D binary file */
    char *plot_2d_argv[] = {"histo-plot", out_2d_bin};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_plot_main((int)(sizeof(plot_2d_argv) / sizeof(plot_2d_argv[0])), plot_2d_argv));

    /* 3. Semicolon-separated file */
    char semi_path[256];
    get_unique_tmp_path(semi_path, sizeof(semi_path), "cli_2d", ".semi");
    fp = fopen(semi_path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 0; i < 50; ++i) {
        fprintf(fp, "%d;%d;%f\n", i, i * 2, 1.5);
    }
    fclose(fp);

    char *fill_semi_argv[] = {
        "histo-fill", "--2d", "--auto-range", "-d", ";", "-o", "json", semi_path
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fill_main((int)(sizeof(fill_semi_argv) / sizeof(fill_semi_argv[0])), fill_semi_argv));


    remove(csv_path);
    remove(out_2d_json);
    remove(tsv_path);
    remove(out_2d_bin);
    remove(semi_path);
}

void test_cli_error_handling(void) {
    char *fill_err_argv[] = {"histo-fill", "--invalid-flag"};
    optind = 1; TEST_ASSERT_NOT_EQUAL_INT(0, cmd_fill_main(2, fill_err_argv));

    char *stats_err_argv[] = {"histo-stats", "nonexistent_file.bin"};
    optind = 1; TEST_ASSERT_NOT_EQUAL_INT(0, cmd_stats_main(2, stats_err_argv));
}

void test_cli_plot_palettes(void) {
    char tmp_1d[256];
    char tmp_2d[256];
    get_unique_tmp_path(tmp_1d, sizeof(tmp_1d), "cli_palette_1d", ".json");
    get_unique_tmp_path(tmp_2d, sizeof(tmp_2d), "cli_palette_2d", ".json");

    /* Generate sample 1D and 2D histograms */
    histo_t *h1d = histo_create_uniform(20, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    for (int i = 0; i < 100; ++i) histo_fill(h1d, (double)i);
    char *json_1d = NULL;
    histo_serialize_json(h1d, &json_1d);
    FILE *f1 = fopen(tmp_1d, "w");
    fputs(json_1d, f1);
    fclose(f1);
    histo_free_buffer(json_1d);
    histo_destroy(h1d);

    histo2d_t *h2d = histo2d_create_uniform(10, 0.0, 10.0, 10, 0.0, 10.0, HISTO_FLAG_NONE);
    for (int i = 0; i < 50; ++i) histo2d_fill(h2d, (double)(i % 10), (double)(i / 5));
    char *json_2d = NULL;
    size_t sz_2d = 0;
    histo2d_serialize_json_alloc(h2d, &json_2d, &sz_2d);
    FILE *f2 = fopen(tmp_2d, "w");
    fputs(json_2d, f2);
    fclose(f2);
    histo_free_buffer(json_2d);
    histo2d_destroy(h2d);

    /* Test all 8 palettes on 1D */
    const char *palettes[] = {
        "viridis", "plasma", "inferno", "magma", "turbo", "cividis", "grayscale", "rainbow"
    };

    for (size_t i = 0; i < sizeof(palettes) / sizeof(palettes[0]); ++i) {
        char pal_opt[64];
        snprintf(pal_opt, sizeof(pal_opt), "--palette=%s", palettes[i]);
        char *plot_1d_argv[] = {"histo-plot", "-c", "always", pal_opt, tmp_1d};
        optind = 1;
        TEST_ASSERT_EQUAL_INT(0, cmd_plot_main((int)(sizeof(plot_1d_argv) / sizeof(plot_1d_argv[0])), plot_1d_argv));

        /* Test on 2D */
        char *plot_2d_argv[] = {"histo-plot", "-c", "always", pal_opt, tmp_2d};
        optind = 1;
        TEST_ASSERT_EQUAL_INT(0, cmd_plot_main((int)(sizeof(plot_2d_argv) / sizeof(plot_2d_argv[0])), plot_2d_argv));
    }

    /* Test --colormap alias */
    char *plot_alias_argv[] = {"histo-plot", "-c", "always", "--colormap=plasma", tmp_1d};
    optind = 1;
    TEST_ASSERT_EQUAL_INT(0, cmd_plot_main((int)(sizeof(plot_alias_argv) / sizeof(plot_alias_argv[0])), plot_alias_argv));

    remove(tmp_1d);
    remove(tmp_2d);
}

void test_cli_top_help(void) {
    char *top_help_argv[] = {"histo-top", "--help"};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_top_main(2, top_help_argv));

    char *top_bins_help_argv[] = {
        "histo-top", "--bins", "50", "--min", "0", "--max", "100",
        "--window", "100000", "--decay", "0.05", "--compact",
        "--autorange-threshold", "0.01", "--scale-input", "0.001",
        "--help"
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_top_main((int)(sizeof(top_bins_help_argv)/sizeof(top_bins_help_argv[0])), top_bins_help_argv));

    char *top_reservoir_argv[] = {
        "histo-top", "-n50", "--reservoir=50000", "-H", "-S1e-3", "--threshold=0.02", "--binary-f64", "--help"
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_top_main((int)(sizeof(top_reservoir_argv)/sizeof(top_reservoir_argv[0])), top_reservoir_argv));

    char *top_binary_alias_argv[] = {
        "histo-top", "--binary", "--help"
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_top_main((int)(sizeof(top_binary_alias_argv)/sizeof(top_binary_alias_argv[0])), top_binary_alias_argv));
}

void test_cli_space_separated_args(void) {
    char tmp_input[256];
    char tmp_out_json[256];
    get_unique_tmp_path(tmp_input, sizeof(tmp_input), "cli_space_args_in", ".txt");
    get_unique_tmp_path(tmp_out_json, sizeof(tmp_out_json), "cli_space_args_out", ".json");

    FILE *fp = fopen(tmp_input, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 0; i < 20; ++i) {
        fprintf(fp, "%d\n", i);
    }
    fclose(fp);

    /* Test histo-fill with space-separated --bins 20 --min 0 --max 20 -o json */
    char *fill_argv[] = {
        "histo-fill", "--bins", "20", "--min", "0", "--max", "20", "-o", "json", "-f", tmp_out_json, tmp_input
    };
    optind = 1;
    TEST_ASSERT_EQUAL_INT(0, cmd_fill_main((int)(sizeof(fill_argv) / sizeof(fill_argv[0])), fill_argv));

    /* Test histo-stats with space-separated --format json */
    char *stats_argv[] = {
        "histo-stats", "--format", "json", tmp_out_json
    };
    optind = 1;
    TEST_ASSERT_EQUAL_INT(0, cmd_stats_main((int)(sizeof(stats_argv) / sizeof(stats_argv[0])), stats_argv));

    /* Test histo-plot with space-separated --width 100 --style blocks --color never */
    char *plot_argv[] = {
        "histo-plot", "--width", "100", "--style", "blocks", "--color", "never", tmp_out_json
    };
    optind = 1;
    TEST_ASSERT_EQUAL_INT(0, cmd_plot_main((int)(sizeof(plot_argv) / sizeof(plot_argv[0])), plot_argv));

    /* Test histo-cmp with space-separated --format json */
    char *cmp_argv[] = {
        "histo-cmp", "--format", "json", tmp_out_json, tmp_out_json
    };
    optind = 1;
    TEST_ASSERT_EQUAL_INT(0, cmd_cmp_main((int)(sizeof(cmp_argv) / sizeof(cmp_argv[0])), cmp_argv));

    remove(tmp_input);
    remove(tmp_out_json);
}

extern int histo_cli_main(int argc, char **argv, FILE *out, FILE *err);

void test_cli_attached_and_bundled_options(void) {
    char tmp_in[256];
    char tmp_out[256];
    get_unique_tmp_path(tmp_in, sizeof(tmp_in), "cli_attached_in", ".txt");
    get_unique_tmp_path(tmp_out, sizeof(tmp_out), "cli_attached_out", ".json");

    FILE *fp = fopen(tmp_in, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 0; i < 20; ++i) {
        fprintf(fp, "%d %f\n", i, 1.5);
    }
    fclose(fp);

    /* 1. histo-fill with attached short options: -n10 -w -ojson -f <file> */
    char *fill_argv[] = {
        "histo-fill", "-n10", "--min=0", "--max=20", "-w", "-ojson", "-f", tmp_out, tmp_in
    };
    TEST_ASSERT_EQUAL_INT(0, cmd_fill_main((int)(sizeof(fill_argv) / sizeof(fill_argv[0])), fill_argv));

    /* 2. histo-plot with attached short options */
    char *plot_argv[] = {
        "histo-plot", "-W100", "-sascii", "--no-stats", tmp_out
    };
    TEST_ASSERT_EQUAL_INT(0, cmd_plot_main((int)(sizeof(plot_argv) / sizeof(plot_argv[0])), plot_argv));

    /* 3. histo-stats with attached short option: -fjson */
    char *stats_argv[] = {
        "histo-stats", "-fjson", tmp_out
    };
    TEST_ASSERT_EQUAL_INT(0, cmd_stats_main((int)(sizeof(stats_argv) / sizeof(stats_argv[0])), stats_argv));

    /* 4. histo-cmp with attached short option: -fjson */
    char *cmp_argv[] = {
        "histo-cmp", "-fjson", tmp_out, tmp_out
    };
    TEST_ASSERT_EQUAL_INT(0, cmd_cmp_main((int)(sizeof(cmp_argv) / sizeof(cmp_argv[0])), cmp_argv));

    remove(tmp_in);
    remove(tmp_out);
}

void test_cli_interspersed_positionals(void) {
    char tmp_in[256];
    char tmp_out[256];
    get_unique_tmp_path(tmp_in, sizeof(tmp_in), "cli_inter_in", ".txt");
    get_unique_tmp_path(tmp_out, sizeof(tmp_out), "cli_inter_out", ".json");

    FILE *fp = fopen(tmp_in, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 0; i < 10; ++i) fprintf(fp, "%d\n", i);
    fclose(fp);

    /* Flags before, between, and after positional arguments */
    char *fill_argv[] = {
        "histo-fill", "--min", "0", tmp_in, "--max", "10", "--output=json", "-f", tmp_out, "-n", "10"
    };
    TEST_ASSERT_EQUAL_INT(0, cmd_fill_main((int)(sizeof(fill_argv) / sizeof(fill_argv[0])), fill_argv));

    /* histo-stats with flags after positional argument */
    char *stats_argv[] = {
        "histo-stats", tmp_out, "--format", "json"
    };
    TEST_ASSERT_EQUAL_INT(0, cmd_stats_main((int)(sizeof(stats_argv) / sizeof(stats_argv[0])), stats_argv));

    /* histo-cmp with flags between positional arguments */
    char *cmp_argv[] = {
        "histo-cmp", tmp_out, "--format=json", tmp_out
    };
    TEST_ASSERT_EQUAL_INT(0, cmd_cmp_main((int)(sizeof(cmp_argv) / sizeof(cmp_argv[0])), cmp_argv));

    remove(tmp_in);
    remove(tmp_out);
}

void test_cli_double_dash_and_negatives(void) {
    char tmp_in[256];
    char tmp_out[256];
    get_unique_tmp_path(tmp_in, sizeof(tmp_in), "cli_neg_in", ".txt");
    get_unique_tmp_path(tmp_out, sizeof(tmp_out), "cli_neg_out", ".json");

    FILE *fp = fopen(tmp_in, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = -50; i < -10; ++i) fprintf(fp, "%d\n", i);
    fclose(fp);

    /* Negative number bounds passed detached and attached */
    char *fill_argv[] = {
        "histo-fill", "--bins", "10", "--min", "-60.0", "--max", "-5.0", "-o", "json", "-f", tmp_out, "--", tmp_in
    };
    TEST_ASSERT_EQUAL_INT(0, cmd_fill_main((int)(sizeof(fill_argv) / sizeof(fill_argv[0])), fill_argv));

    /* Double dash positional terminator */
    char *stats_argv[] = {
        "histo-stats", "--format=json", "--", tmp_out
    };
    TEST_ASSERT_EQUAL_INT(0, cmd_stats_main((int)(sizeof(stats_argv) / sizeof(stats_argv[0])), stats_argv));

    remove(tmp_in);
    remove(tmp_out);
}

void test_cli_exhaustive_error_cases(void) {
    /* Unknown options across all commands */
    char *fill_unk[] = {"histo-fill", "--unknown-flag-123"};
    TEST_ASSERT_NOT_EQUAL_INT(0, cmd_fill_main(2, fill_unk));

    char *plot_unk[] = {"histo-plot", "--unknown-flag-123"};
    TEST_ASSERT_NOT_EQUAL_INT(0, cmd_plot_main(2, plot_unk));

    char *stats_unk[] = {"histo-stats", "--unknown-flag-123"};
    TEST_ASSERT_NOT_EQUAL_INT(0, cmd_stats_main(2, stats_unk));

    char *cmp_unk[] = {"histo-cmp", "--unknown-flag-123"};
    TEST_ASSERT_NOT_EQUAL_INT(0, cmd_cmp_main(2, cmp_unk));

    char *fit_unk[] = {"histo-fit", "--unknown-flag-123"};
    TEST_ASSERT_NOT_EQUAL_INT(0, cmd_fit_main(2, fit_unk));

    char *top_unk[] = {"histo-top", "--unknown-flag-123"};
    TEST_ASSERT_NOT_EQUAL_INT(0, cmd_top_main(2, top_unk));

    /* Missing arguments */
    char *fill_miss[] = {"histo-fill", "--bins"};
    TEST_ASSERT_NOT_EQUAL_INT(0, cmd_fill_main(2, fill_miss));

    char *plot_miss[] = {"histo-plot", "--width"};
    TEST_ASSERT_NOT_EQUAL_INT(0, cmd_plot_main(2, plot_miss));

    char *stats_miss[] = {"histo-stats", "--format"};
    TEST_ASSERT_NOT_EQUAL_INT(0, cmd_stats_main(2, stats_miss));

    char *cmp_miss[] = {"histo-cmp", "--format"};
    TEST_ASSERT_NOT_EQUAL_INT(0, cmd_cmp_main(2, cmp_miss));

    /* Invalid type conversions */
    char *fill_type_err[] = {"histo-fill", "--bins=abc"};
    TEST_ASSERT_NOT_EQUAL_INT(0, cmd_fill_main(2, fill_type_err));

    char *plot_type_err[] = {"histo-plot", "--width=xyz"};
    TEST_ASSERT_NOT_EQUAL_INT(0, cmd_plot_main(2, plot_type_err));
}

void test_cli_dispatcher_and_help(void) {
    char *disp_help[] = {"histo", "--help"};
    TEST_ASSERT_EQUAL_INT(0, histo_cli_main(2, disp_help, stdout, stderr));

    char *disp_ver[] = {"histo", "--version"};
    TEST_ASSERT_EQUAL_INT(0, histo_cli_main(2, disp_ver, stdout, stderr));

    char *disp_v[] = {"histo", "-v"};
    TEST_ASSERT_EQUAL_INT(0, histo_cli_main(2, disp_v, stdout, stderr));

    char *disp_fill_help[] = {"histo", "fill", "--help"};
    TEST_ASSERT_EQUAL_INT(0, histo_cli_main(3, disp_fill_help, stdout, stderr));

    char *disp_plot_help[] = {"histo", "plot", "--help"};
    TEST_ASSERT_EQUAL_INT(0, histo_cli_main(3, disp_plot_help, stdout, stderr));

    char *disp_stats_help[] = {"histo", "stats", "--help"};
    TEST_ASSERT_EQUAL_INT(0, histo_cli_main(3, disp_stats_help, stdout, stderr));

    char *disp_cmp_help[] = {"histo", "cmp", "--help"};
    TEST_ASSERT_EQUAL_INT(0, histo_cli_main(3, disp_cmp_help, stdout, stderr));

    char *disp_fit_help[] = {"histo", "fit", "--help"};
    TEST_ASSERT_EQUAL_INT(0, histo_cli_main(3, disp_fit_help, stdout, stderr));

    char *disp_unk_cmd[] = {"histo", "unknown_cmd_xyz"};
    TEST_ASSERT_NOT_EQUAL_INT(0, histo_cli_main(2, disp_unk_cmd, stdout, stderr));
}

void test_cli_scale_input(void) {
    char data_path[] = "/tmp/histo_scale_test_XXXXXX";
    int fd = mkstemp(data_path);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    FILE *fp = fdopen(fd, "w");
    TEST_ASSERT_NOT_NULL(fp);

    /* 1000000 ns, 2000000 ns, 3000000 ns */
    fprintf(fp, "1000000\n2000000\n3000000\n");
    fclose(fp);

    char out_path[] = "/tmp/histo_scale_out_XXXXXX";
    int fd_out = mkstemp(out_path);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd_out);
    close(fd_out);

    char *fill_args[] = {
        "histo-fill",
        "--scale-input=1e-3",
        "--auto-range",
        "--bins=10",
        "--output=json",
        "-f", out_path,
        data_path
    };
    TEST_ASSERT_EQUAL_INT(0, cmd_fill_main(8, fill_args));

    histo_t *h = NULL;
    TEST_ASSERT_EQUAL_INT(HISTO_OK, cli_read_histogram_from_file(out_path, &h));
    TEST_ASSERT_NOT_NULL(h);

    double mean = 0.0;
    histo_mean(h, &mean);
    /* 1000, 2000, 3000 scaled down from 1e6, 2e6, 3e6 with 1e-3 => mean should be ~2000 */
    TEST_ASSERT_DOUBLE_WITHIN(50.0, 2000.0, mean);

    histo_destroy(h);
    unlink(data_path);
    unlink(out_path);
}

void test_cli_bpftrace_pipeline(void) {
    char bpf_path[] = "/tmp/histo_bpf_test_XXXXXX";
    int fd = mkstemp(bpf_path);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    FILE *fp = fdopen(fd, "w");
    TEST_ASSERT_NOT_NULL(fp);

    fprintf(fp,
        "@vfs_read_latency:\n"
        "[0]                    10 |@@@@@                                   |\n"
        "[1]                     5 |@@                                      |\n"
        "[2, 4)                 20 |@@@@@@@@@@                              |\n"
        "[4, 8)                 80 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|\n"
        "[8, 16)                40 |@@@@@@@@@@@@@@@@@@@@                    |\n"
        "[16, 32)               15 |@@@@@@@                                 |\n"
        "[32, 64)                2 |@                                       |\n"
        "[64, 128)               0 |                                        |\n"
        "[128, 256)              1 |@                                       |\n"
    );
    fclose(fp);

    /* Test 1: histo stats on bpftrace output */
    char *stats_args[] = {"histo-stats", "--format=json", bpf_path};
    TEST_ASSERT_EQUAL_INT(0, cmd_stats_main(3, stats_args));

    /* Test 2: histo plot on bpftrace output */
    char *plot_args[] = {"histo-plot", "--style=blocks", bpf_path};
    TEST_ASSERT_EQUAL_INT(0, cmd_plot_main(3, plot_args));

    /* Test 3: histo fit on bpftrace output */
    char *fit_args[] = {"histo-fit", "--model=gaussian", bpf_path};
    TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(3, fit_args));

    /* Test 4: histo fill on bpftrace output to produce JSON */
    char json_out[] = "/tmp/histo_bpf_json_XXXXXX";
    int fd_json = mkstemp(json_out);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd_json);
    close(fd_json);

    char *fill_args[] = {"histo-fill", "--output=json", "-f", json_out, bpf_path};
    TEST_ASSERT_EQUAL_INT(0, cmd_fill_main(5, fill_args));

    histo_t *deser = NULL;
    TEST_ASSERT_EQUAL_INT(HISTO_OK, cli_read_histogram_from_file(json_out, &deser));
    TEST_ASSERT_NOT_NULL(deser);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 173.0, histo_total_weight(deser));
    histo_destroy(deser);

    /* Test 5: histo cmp on two bpftrace files */
    char *cmp_args[] = {"histo-cmp", bpf_path, bpf_path};
    TEST_ASSERT_EQUAL_INT(0, cmd_cmp_main(3, cmp_args));

    unlink(bpf_path);
    unlink(json_out);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_json_serialization_roundtrip_uniform);
    RUN_TEST(test_json_serialization_roundtrip_variable);
    RUN_TEST(test_json_corrupted_payload);
    RUN_TEST(test_cli_execution_pipelines);
    RUN_TEST(test_cli_fit_pipeline);
    RUN_TEST(test_cli_2d_fill_and_delimiters);
    RUN_TEST(test_cli_error_handling);
    RUN_TEST(test_cli_plot_palettes);
    RUN_TEST(test_cli_top_help);
    RUN_TEST(test_cli_space_separated_args);
    RUN_TEST(test_cli_attached_and_bundled_options);
    RUN_TEST(test_cli_interspersed_positionals);
    RUN_TEST(test_cli_double_dash_and_negatives);
    RUN_TEST(test_cli_exhaustive_error_cases);
    RUN_TEST(test_cli_dispatcher_and_help);
    RUN_TEST(test_cli_scale_input);
    RUN_TEST(test_cli_bpftrace_pipeline);
    return UNITY_END();
}



