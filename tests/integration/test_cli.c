#include "unity.h"
#include "histo/histo.h"
#include "histo/version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>

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

void test_cli_execution_pipelines(void) {
    /* Test command-line help flags */
    char *help_argv[] = {"histo", "--help"};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fill_main(2, help_argv));
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_plot_main(2, help_argv));
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_stats_main(2, help_argv));
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_cmp_main(2, help_argv));

    /* Create temporary data file */
    const char *data_path = "/tmp/test_cli_data.txt";
    FILE *fp = fopen(data_path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 1; i <= 50; ++i) {
        fprintf(fp, "%d %f\n", i, 1.0);
    }
    fclose(fp);

    /* Test histo-fill writing binary output */
    const char *out_bin = "/tmp/test_cli_out.bin";
    char *fill_bin_argv[] = {
        "histo-fill", "--bins=10", "--min=0", "--max=50", "-w",
        "-o", "binary", "-f", (char *)out_bin, (char *)data_path
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fill_main(10, fill_bin_argv));

    /* Test histo-stats on binary output */
    char *stats_argv[] = {"histo-stats", "-f", "json", (char *)out_bin};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_stats_main(4, stats_argv));

    /* Test histo-plot on binary output */
    char *plot_argv[] = {"histo-plot", "-s", "ascii", "--no-stats", (char *)out_bin};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_plot_main(5, plot_argv));

    /* Test histo-plot with sparkline mode */
    char *sparkline_argv[] = {"histo-plot", "-S", (char *)out_bin};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_plot_main(3, sparkline_argv));

    char *sparkline_ascii_argv[] = {"histo-plot", "--sparkline", "-s", "ascii", "--no-stats", (char *)out_bin};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_plot_main(6, sparkline_ascii_argv));

    /* Create second dataset for comparison */
    const char *out_bin2 = "/tmp/test_cli_out2.bin";
    char *fill_bin_argv2[] = {
        "histo-fill", "--bins=10", "--min=0", "--max=50",
        "-o", "binary", "-f", (char *)out_bin2, (char *)data_path
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fill_main(9, fill_bin_argv2));

    /* Test histo-cmp */
    char *cmp_argv[] = {"histo-cmp", "-f", "json", (char *)out_bin, (char *)out_bin2};
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
    const char *gauss_path = "/tmp/test_cli_gauss.txt";
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
    char *fit_default_argv[] = {"histo-fit", (char *)gauss_path};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(2, fit_default_argv));

    /* Test JSON fit */
    char *fit_json_argv[] = {"histo-fit", "--model=gaussian", "-j", (char *)gauss_path};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(4, fit_json_argv));

    /* Test quiet fit */
    char *fit_quiet_argv[] = {"histo-fit", "-q", (char *)gauss_path};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(3, fit_quiet_argv));

    /* Test plot fit */
    char *fit_plot_argv[] = {"histo-fit", "-p", (char *)gauss_path};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(3, fit_plot_argv));

    /* Test constrained fit with bounds and fix-param */
    char *fit_bounds_argv[] = {
        "histo-fit", "--bounds=2=1.0:10.0", "--fix-param=1=50.0", (char *)gauss_path
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(4, fit_bounds_argv));

    /* Test other models on serialized histogram */
    const char *out_hist = "/tmp/test_cli_fit_h.json";
    char *fill_argv[] = {
        "histo-fill", "--bins=20", "--min=20", "--max=80",
        "-o", "json", "-f", (char *)out_hist, (char *)gauss_path
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fill_main(9, fill_argv));

    char *fit_poly_argv[] = {"histo-fit", "-m", "polynomial", "-d", "2", (char *)out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(6, fit_poly_argv));

    char *fit_exp_argv[] = {"histo-fit", "-m", "exponential", (char *)out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(4, fit_exp_argv));

    char *fit_bw_argv[] = {"histo-fit", "-m", "breit-wigner", (char *)out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(4, fit_bw_argv));

    char *fit_mle_argv[] = {"histo-fit", "--mle", (char *)out_hist};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fit_main(3, fit_mle_argv));

    /* Error handling */
    char *fit_err_argv[] = {"histo-fit", "-m", "invalid_model_name", (char *)out_hist};
    optind = 1; TEST_ASSERT_NOT_EQUAL_INT(0, cmd_fit_main(4, fit_err_argv));

    remove(gauss_path);
    remove(out_hist);
}

void test_cli_2d_fill_and_delimiters(void) {
    /* 1. Comma-separated CSV */
    const char *csv_path = "/tmp/test_cli_2d.csv";
    FILE *fp = fopen(csv_path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 0; i < 100; ++i) {
        fprintf(fp, "%.2f,%.2f\n", (double)(i % 10), (double)(i / 10));
    }
    fclose(fp);

    const char *out_2d_json = "/tmp/test_cli_2d.json";
    char *fill_csv_argv[] = {
        "histo-fill", "--2d", "--xbins", "10", "--xmin", "0", "--xmax", "10",
        "--ybins", "10", "--ymin", "0", "--ymax", "10",
        "-o", "json", "-f", (char *)out_2d_json, (char *)csv_path
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fill_main((int)(sizeof(fill_csv_argv) / sizeof(fill_csv_argv[0])), fill_csv_argv));

    /* 2. Tab-separated TSV with weights */
    const char *tsv_path = "/tmp/test_cli_2d.tsv";
    fp = fopen(tsv_path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 0; i < 100; ++i) {
        fprintf(fp, "%.2f\t%.2f\t%.2f\n", (double)(i % 10), (double)(i / 10), 2.0);
    }
    fclose(fp);

    const char *out_2d_bin = "/tmp/test_cli_2d.bin";
    char *fill_tsv_argv[] = {
        "histo-fill", "--2d", "-w", "-o", "binary", "-f", (char *)out_2d_bin, (char *)tsv_path
    };
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_fill_main((int)(sizeof(fill_tsv_argv) / sizeof(fill_tsv_argv[0])), fill_tsv_argv));

    /* Test histo-plot on 2D binary file */
    char *plot_2d_argv[] = {"histo-plot", (char *)out_2d_bin};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_plot_main((int)(sizeof(plot_2d_argv) / sizeof(plot_2d_argv[0])), plot_2d_argv));

    /* 3. Semicolon-separated file */
    const char *semi_path = "/tmp/test_cli_2d.semi";
    fp = fopen(semi_path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 0; i < 50; ++i) {
        fprintf(fp, "%d;%d;%f\n", i, i * 2, 1.5);
    }
    fclose(fp);

    char *fill_semi_argv[] = {
        "histo-fill", "--2d", "--auto-range", "-d", ";", "-o", "json", (char *)semi_path
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

void test_cli_top_help(void) {
    char *top_help_argv[] = {"histo-top", "--help"};
    optind = 1; TEST_ASSERT_EQUAL_INT(0, cmd_top_main(2, top_help_argv));
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
    RUN_TEST(test_cli_top_help);
    return UNITY_END();
}

