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

void test_cli_error_handling(void) {
    /* Invalid arguments to subcommands */
    char *fill_err_argv[] = {"histo-fill", "--invalid-flag"};
    optind = 1; TEST_ASSERT_NOT_EQUAL_INT(0, cmd_fill_main(2, fill_err_argv));

    char *stats_err_argv[] = {"histo-stats", "nonexistent_file.bin"};
    optind = 1; TEST_ASSERT_NOT_EQUAL_INT(0, cmd_stats_main(2, stats_err_argv));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_json_serialization_roundtrip_uniform);
    RUN_TEST(test_json_serialization_roundtrip_variable);
    RUN_TEST(test_json_corrupted_payload);
    RUN_TEST(test_cli_execution_pipelines);
    RUN_TEST(test_cli_error_handling);
    return UNITY_END();
}
