/*
 * Unit tests for bpftrace ASCII histogram parser and geometry reconstruction.
 */

#include "unity.h"
#include "histo/histo.h"
#include "cli_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

void test_bpftrace_parser_power_of_two(void) {
    const char *sample =
        "@vfs_read_latency:\n"
        "[0]                    2 |@@@@@@                                  |\n"
        "[1]                    1 |@@@                                     |\n"
        "[2, 4)                 4 |@@@@@@@@@@@@                            |\n"
        "[4, 8)                12 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@    |\n"
        "[8, 16)                7 |@@@@@@@@@@@@@@@@@@@                     |\n"
        "[16, 32)               3 |@@@@@@@@@                               |\n"
        "[32, 64)               1 |@@@                                     |\n";

    histo_t *h = NULL;
    histo_status_t st = cli_parse_bpftrace_histogram_str(sample, &h);
    TEST_ASSERT_EQUAL_INT(HISTO_OK, st);
    TEST_ASSERT_NOT_NULL(h);

    TEST_ASSERT_EQUAL_UINT32(7, histo_nbins(h));
    TEST_ASSERT_EQUAL_INT(HISTO_BIN_VARIABLE, histo_bin_type(h));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 30.0, histo_total_weight(h));
    TEST_ASSERT_EQUAL_UINT64(7, histo_num_entries(h));

    double c0 = 0, c3 = 0, c6 = 0;
    histo_bin_content(h, 0, &c0);
    histo_bin_content(h, 3, &c3);
    histo_bin_content(h, 6, &c6);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, c0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 12.0, c3);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, c6);

    double low = 0, high = 0;
    histo_bin_bounds(h, 0, &low, &high);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, low);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, high);

    histo_bin_bounds(h, 2, &low, &high);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, low);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, high);

    histo_bin_bounds(h, 6, &low, &high);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 32.0, low);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 64.0, high);

    histo_destroy(h);
}

void test_bpftrace_parser_linear(void) {
    const char *sample =
        "@bytes:\n"
        "[0, 10)               10 |@@@@@@@@@@@@@@@@@@@@                    |\n"
        "[10, 20)              25 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|\n"
        "[20, 30)               5 |@@@@@@@@@@                              |\n"
        "[30, 40)               2 |@@@@                                    |\n"
        "[40, 50)               0 |                                        |\n";

    histo_t *h = NULL;
    histo_status_t st = cli_parse_bpftrace_histogram_str(sample, &h);
    TEST_ASSERT_EQUAL_INT(HISTO_OK, st);
    TEST_ASSERT_NOT_NULL(h);

    TEST_ASSERT_EQUAL_UINT32(5, histo_nbins(h));
    TEST_ASSERT_EQUAL_INT(HISTO_BIN_UNIFORM, histo_bin_type(h));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 42.0, histo_total_weight(h));

    double min_v = 0, max_v = 0;
    histo_range(h, &min_v, &max_v);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, min_v);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 50.0, max_v);

    histo_destroy(h);
}

void test_bpftrace_parser_unit_multipliers(void) {
    const char *sample =
        "@network_latency:\n"
        "[256, 512)            10 |@@@@@                                   |\n"
        "[512, 1K)             20 |@@@@@@@@@@                              |\n"
        "[1K, 2K)              40 |@@@@@@@@@@@@@@@@@@@@                    |\n"
        "[2K, 4K)              15 |@@@@@@@                                 |\n"
        "[4K, 8K)               0 |                                        |\n"
        "[8K, 16K)              0 |                                        |\n"
        "[16K, 32K)             5 |@@                                      |\n"
        "[32K, 64K)             0 |                                        |\n"
        "[64K, 128K)            0 |                                        |\n"
        "[128K, 256K)           0 |                                        |\n"
        "[256K, 512K)           0 |                                        |\n"
        "[512K, 1M)             2 |@                                       |\n"
        "[1M, 2M)               1 |@                                       |\n";

    histo_t *h = NULL;
    histo_status_t st = cli_parse_bpftrace_histogram_str(sample, &h);
    TEST_ASSERT_EQUAL_INT(HISTO_OK, st);
    TEST_ASSERT_NOT_NULL(h);

    TEST_ASSERT_EQUAL_UINT32(13, histo_nbins(h));

    double low = 0, high = 0;
    histo_bin_bounds(h, 1, &low, &high);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 512.0, low);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1024.0, high);

    histo_bin_bounds(h, 2, &low, &high);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1024.0, low);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2048.0, high);

    histo_bin_bounds(h, 11, &low, &high);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 524288.0, low);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1048576.0, high);

    histo_bin_bounds(h, 12, &low, &high);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1048576.0, low);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2097152.0, high);

    histo_destroy(h);
}

void test_bpftrace_parser_bcc_style(void) {
    const char *sample =
        "     nsecs               : count     distribution\n"
        "         0 -> 1          : 5        |*****                                   |\n"
        "         2 -> 3          : 10       |**********                              |\n"
        "         4 -> 7          : 20       |********************                    |\n"
        "         8 -> 15         : 40       |****************************************|\n"
        "        16 -> 31         : 15       |***************                         |\n";

    histo_t *h = NULL;
    histo_status_t st = cli_parse_bpftrace_histogram_str(sample, &h);
    TEST_ASSERT_EQUAL_INT(HISTO_OK, st);
    TEST_ASSERT_NOT_NULL(h);

    TEST_ASSERT_EQUAL_UINT32(5, histo_nbins(h));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 90.0, histo_total_weight(h));

    double low = 0, high = 0;
    histo_bin_bounds(h, 0, &low, &high);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, low);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, high);

    histo_bin_bounds(h, 4, &low, &high);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 16.0, low);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 32.0, high);

    histo_destroy(h);
}

void test_bpftrace_parser_invalid_cases(void) {
    histo_t *h = NULL;
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_INVALID_ARG, cli_parse_bpftrace_histogram_str(NULL, &h));
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_INVALID_ARG, cli_parse_bpftrace_histogram_str("sample", NULL));
    TEST_ASSERT_EQUAL_INT(HISTO_ERR_DESERIALIZATION, cli_parse_bpftrace_histogram_str("just random text\nno buckets\n", &h));
    TEST_ASSERT_NULL(h);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bpftrace_parser_power_of_two);
    RUN_TEST(test_bpftrace_parser_linear);
    RUN_TEST(test_bpftrace_parser_unit_multipliers);
    RUN_TEST(test_bpftrace_parser_bcc_style);
    RUN_TEST(test_bpftrace_parser_invalid_cases);
    return UNITY_END();
}
