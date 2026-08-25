/*
 * Unit tests for declarative CLI option parsing framework (cli_opt).
 * Tests all argument parsing behaviors: short flags, bundled flags, long flags,
 * attached values (-n50, -n=50, --bins=50), detached values (-n 50, --bins 50),
 * boolean negations (--no-x), double-dash (--) positional separators, aliases,
 * type conversions, missing values, negative number arguments, and error detection.
 */

#include "unity.h"
#include "cli_opt.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

void test_cli_opt_flags_and_bundling(void) {
    bool flag_a = false;
    bool flag_b = false;
    bool flag_c = false;
    bool flag_long = false;

    cli_opt_spec_t specs[] = {
        {'a', "all", NULL, CLI_OPT_TYPE_BOOL, &flag_a, NULL, 0, NULL, "All flag", NULL},
        {'b', "binary", NULL, CLI_OPT_TYPE_BOOL, &flag_b, NULL, 0, NULL, "Binary flag", NULL},
        {'c', "count", NULL, CLI_OPT_TYPE_BOOL, &flag_c, NULL, 0, NULL, "Count flag", NULL},
        {0, "verbose", NULL, CLI_OPT_TYPE_BOOL, &flag_long, NULL, 0, NULL, "Verbose flag", NULL}
    };

    cli_opt_parser_t parser;
    cli_opt_init(&parser, specs, sizeof(specs)/sizeof(specs[0]), "test-cmd", "[OPTIONS]", "Testing flags");

    char *argv[] = {"test-cmd", "-abc", "--verbose"};
    int rc = cli_opt_parse(&parser, 3, argv, 1);

    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(flag_a);
    TEST_ASSERT_TRUE(flag_b);
    TEST_ASSERT_TRUE(flag_c);
    TEST_ASSERT_TRUE(flag_long);
    TEST_ASSERT_EQUAL_INT(0, parser.num_positionals);

    cli_opt_free(&parser);
}

void test_cli_opt_short_attached_and_detached(void) {
    uint32_t bins = 0;
    const char *format = NULL;

    cli_opt_spec_t specs[] = {
        {'n', "bins", NULL, CLI_OPT_TYPE_UINT32, &bins, NULL, 0, "N", "Number of bins", "50"},
        {'f', "format", NULL, CLI_OPT_TYPE_STRING, &format, NULL, 0, "FMT", "Output format", "table"}
    };

    /* 1. Attached without '=': -n50 */
    {
        bins = 0; format = NULL;
        cli_opt_parser_t parser;
        cli_opt_init(&parser, specs, 2, "test-cmd", NULL, NULL);
        char *argv[] = {"test-cmd", "-n50", "-fjson"};
        TEST_ASSERT_EQUAL_INT(0, cli_opt_parse(&parser, 3, argv, 1));
        TEST_ASSERT_EQUAL_UINT32(50, bins);
        TEST_ASSERT_EQUAL_STRING("json", format);
        cli_opt_free(&parser);
    }

    /* 2. Attached with '=': -n=100 */
    {
        bins = 0; format = NULL;
        cli_opt_parser_t parser;
        cli_opt_init(&parser, specs, 2, "test-cmd", NULL, NULL);
        char *argv[] = {"test-cmd", "-n=100", "-f=tsv"};
        TEST_ASSERT_EQUAL_INT(0, cli_opt_parse(&parser, 3, argv, 1));
        TEST_ASSERT_EQUAL_UINT32(100, bins);
        TEST_ASSERT_EQUAL_STRING("tsv", format);
        cli_opt_free(&parser);
    }

    /* 3. Detached: -n 200 -f binary */
    {
        bins = 0; format = NULL;
        cli_opt_parser_t parser;
        cli_opt_init(&parser, specs, 2, "test-cmd", NULL, NULL);
        char *argv[] = {"test-cmd", "-n", "200", "-f", "binary"};
        TEST_ASSERT_EQUAL_INT(0, cli_opt_parse(&parser, 5, argv, 1));
        TEST_ASSERT_EQUAL_UINT32(200, bins);
        TEST_ASSERT_EQUAL_STRING("binary", format);
        cli_opt_free(&parser);
    }

    /* 4. Flag bundled before option with attached value: -wn75 */
    {
        bool weights = false;
        bins = 0;
        cli_opt_spec_t specs_w[] = {
            {'w', "weights", NULL, CLI_OPT_TYPE_BOOL, &weights, NULL, 0, NULL, "Weights", NULL},
            {'n', "bins", NULL, CLI_OPT_TYPE_UINT32, &bins, NULL, 0, "N", "Number of bins", "50"}
        };
        cli_opt_parser_t parser;
        cli_opt_init(&parser, specs_w, 2, "test-cmd", NULL, NULL);
        char *argv[] = {"test-cmd", "-wn75"};
        TEST_ASSERT_EQUAL_INT(0, cli_opt_parse(&parser, 2, argv, 1));
        TEST_ASSERT_TRUE(weights);
        TEST_ASSERT_EQUAL_UINT32(75, bins);
        cli_opt_free(&parser);
    }
}

void test_cli_opt_long_options_and_aliases(void) {
    const char *palette = NULL;
    double min_val = 0.0;
    double max_val = 0.0;

    cli_opt_spec_t specs[] = {
        {'p', "palette", "colormap", CLI_OPT_TYPE_STRING, &palette, NULL, 0, "NAME", "Palette", "viridis"},
        {0, "min", NULL, CLI_OPT_TYPE_DOUBLE, &min_val, NULL, 0, "X", "Min bound", NULL},
        {0, "max", NULL, CLI_OPT_TYPE_DOUBLE, &max_val, NULL, 0, "X", "Max bound", NULL}
    };

    /* 1. Long options with '=' */
    {
        palette = NULL; min_val = 0.0; max_val = 0.0;
        cli_opt_parser_t parser;
        cli_opt_init(&parser, specs, 3, "test-cmd", NULL, NULL);
        char *argv[] = {"test-cmd", "--palette=plasma", "--min=12.5", "--max=99.9"};
        TEST_ASSERT_EQUAL_INT(0, cli_opt_parse(&parser, 4, argv, 1));
        TEST_ASSERT_EQUAL_STRING("plasma", palette);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 12.5, min_val);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 99.9, max_val);
        cli_opt_free(&parser);
    }

    /* 2. Long options detached */
    {
        palette = NULL; min_val = 0.0; max_val = 0.0;
        cli_opt_parser_t parser;
        cli_opt_init(&parser, specs, 3, "test-cmd", NULL, NULL);
        char *argv[] = {"test-cmd", "--palette", "turbo", "--min", "1.23", "--max", "4.56"};
        TEST_ASSERT_EQUAL_INT(0, cli_opt_parse(&parser, 7, argv, 1));
        TEST_ASSERT_EQUAL_STRING("turbo", palette);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.23, min_val);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.56, max_val);
        cli_opt_free(&parser);
    }

    /* 3. Alias: --colormap */
    {
        palette = NULL;
        cli_opt_parser_t parser;
        cli_opt_init(&parser, specs, 3, "test-cmd", NULL, NULL);
        char *argv[] = {"test-cmd", "--colormap=inferno"};
        TEST_ASSERT_EQUAL_INT(0, cli_opt_parse(&parser, 2, argv, 1));
        TEST_ASSERT_EQUAL_STRING("inferno", palette);
        cli_opt_free(&parser);
    }
}

void test_cli_opt_boolean_negation(void) {
    bool stats = true;
    bool sumw2 = true;
    bool autorange = false;

    cli_opt_spec_t specs[] = {
        {0, "stats", NULL, CLI_OPT_TYPE_BOOL, &stats, NULL, 0, NULL, "Show stats", "on"},
        {0, "sumw2", NULL, CLI_OPT_TYPE_BOOL, &sumw2, NULL, 0, NULL, "Track sumw2", "on"},
        {'a', "auto-range", NULL, CLI_OPT_TYPE_BOOL, &autorange, NULL, 0, NULL, "Auto range", "off"}
    };

    /* Invert flags with --no- prefix */
    cli_opt_parser_t parser;
    cli_opt_init(&parser, specs, 3, "test-cmd", NULL, NULL);
    char *argv[] = {"test-cmd", "--no-stats", "--no-sumw2", "--no-auto-range"};
    TEST_ASSERT_EQUAL_INT(0, cli_opt_parse(&parser, 4, argv, 1));
    TEST_ASSERT_FALSE(stats);
    TEST_ASSERT_FALSE(sumw2);
    TEST_ASSERT_FALSE(autorange);
    cli_opt_free(&parser);
}

void test_cli_opt_negative_numbers_and_positionals(void) {
    double min_val = 0.0;
    double max_val = 0.0;

    cli_opt_spec_t specs[] = {
        {0, "min", NULL, CLI_OPT_TYPE_DOUBLE, &min_val, NULL, 0, "X", "Min bound", NULL},
        {0, "max", NULL, CLI_OPT_TYPE_DOUBLE, &max_val, NULL, 0, "X", "Max bound", NULL}
    };

    /* Negative number detached */
    cli_opt_parser_t parser;
    cli_opt_init(&parser, specs, 2, "test-cmd", NULL, NULL);
    char *argv[] = {"test-cmd", "--min", "-100.5", "--max", "-10.2", "file_a.txt", "file_b.txt"};
    TEST_ASSERT_EQUAL_INT(0, cli_opt_parse(&parser, 7, argv, 1));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -100.5, min_val);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -10.2, max_val);
    TEST_ASSERT_EQUAL_INT(2, parser.num_positionals);
    TEST_ASSERT_EQUAL_STRING("file_a.txt", parser.positionals[0]);
    TEST_ASSERT_EQUAL_STRING("file_b.txt", parser.positionals[1]);
    cli_opt_free(&parser);
}

void test_cli_opt_double_dash_terminator(void) {
    bool flag = false;
    cli_opt_spec_t specs[] = {
        {'v', "verbose", NULL, CLI_OPT_TYPE_BOOL, &flag, NULL, 0, NULL, "Verbose", NULL}
    };

    cli_opt_parser_t parser;
    cli_opt_init(&parser, specs, 1, "test-cmd", NULL, NULL);
    char *argv[] = {"test-cmd", "-v", "--", "-not-a-flag", "--also-positional", "file.dat"};
    TEST_ASSERT_EQUAL_INT(0, cli_opt_parse(&parser, 6, argv, 1));
    TEST_ASSERT_TRUE(flag);
    TEST_ASSERT_EQUAL_INT(3, parser.num_positionals);
    TEST_ASSERT_EQUAL_STRING("-not-a-flag", parser.positionals[0]);
    TEST_ASSERT_EQUAL_STRING("--also-positional", parser.positionals[1]);
    TEST_ASSERT_EQUAL_STRING("file.dat", parser.positionals[2]);
    cli_opt_free(&parser);
}

void test_cli_opt_interspersed_positionals(void) {
    uint32_t width = 0;
    const char *format = NULL;

    cli_opt_spec_t specs[] = {
        {'W', "width", NULL, CLI_OPT_TYPE_UINT32, &width, NULL, 0, "COLS", "Width", NULL},
        {'f', "format", NULL, CLI_OPT_TYPE_STRING, &format, NULL, 0, "FMT", "Format", NULL}
    };

    cli_opt_parser_t parser;
    cli_opt_init(&parser, specs, 2, "test-cmd", NULL, NULL);
    char *argv[] = {"test-cmd", "pos1.bin", "-W", "120", "pos2.bin", "--format=json", "pos3.bin"};
    TEST_ASSERT_EQUAL_INT(0, cli_opt_parse(&parser, 7, argv, 1));
    TEST_ASSERT_EQUAL_UINT32(120, width);
    TEST_ASSERT_EQUAL_STRING("json", format);
    TEST_ASSERT_EQUAL_INT(3, parser.num_positionals);
    TEST_ASSERT_EQUAL_STRING("pos1.bin", parser.positionals[0]);
    TEST_ASSERT_EQUAL_STRING("pos2.bin", parser.positionals[1]);
    TEST_ASSERT_EQUAL_STRING("pos3.bin", parser.positionals[2]);
    cli_opt_free(&parser);
}

static int parse_custom_range(const char *opt_name, const char *val, void *data, char *err_buf, size_t err_size) {
    (void)opt_name;
    double *range = (double *)data;
    if (!val || sscanf(val, "%lf:%lf", &range[0], &range[1]) != 2) {
        snprintf(err_buf, err_size, "Range must be in MIN:MAX format (e.g. 0:100)");
        return 1;
    }
    return 0;
}

void test_cli_opt_custom_callback_and_types(void) {
    int64_t i64_val = 0;
    uint64_t u64_val = 0;
    char delim = ' ';
    double range[2] = {0.0, 0.0};

    cli_opt_spec_t specs[] = {
        {0, "offset", NULL, CLI_OPT_TYPE_INT64, &i64_val, NULL, 0, "N", "Offset", NULL},
        {0, "samples", NULL, CLI_OPT_TYPE_UINT64, &u64_val, NULL, 0, "N", "Samples", NULL},
        {'d', "delimiter", NULL, CLI_OPT_TYPE_CHAR, &delim, NULL, 0, "CHAR", "Delimiter", NULL},
        {0, "range", NULL, CLI_OPT_TYPE_CALLBACK, range, parse_custom_range, 0, "MIN:MAX", "Range", NULL}
    };

    cli_opt_parser_t parser;
    cli_opt_init(&parser, specs, 4, "test-cmd", NULL, NULL);
    char *argv[] = {"test-cmd", "--offset=-1234567890123", "--samples=9876543210123", "-d;", "--range=10.5:20.5"};
    TEST_ASSERT_EQUAL_INT(0, cli_opt_parse(&parser, 5, argv, 1));
    TEST_ASSERT_EQUAL_INT64((int64_t)-1234567890123LL, i64_val);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)9876543210123ULL, u64_val);
    TEST_ASSERT_EQUAL_CHAR(';', delim);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.5, range[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 20.5, range[1]);
    cli_opt_free(&parser);
}

void test_cli_opt_error_handling(void) {
    uint32_t bins = 50;
    double range[2] = {0.0, 0.0};

    cli_opt_spec_t specs[] = {
        {'n', "bins", NULL, CLI_OPT_TYPE_UINT32, &bins, NULL, CLI_OPT_FLAG_REQUIRED, "N", "Number of bins", "50"},
        {0, "range", NULL, CLI_OPT_TYPE_CALLBACK, range, parse_custom_range, 0, "MIN:MAX", "Range", NULL}
    };

    /* 1. Unknown long option */
    {
        cli_opt_parser_t parser;
        cli_opt_init(&parser, specs, 2, "test-cmd", NULL, NULL);
        char *argv[] = {"test-cmd", "--nonexistent-option"};
        TEST_ASSERT_EQUAL_INT(1, cli_opt_parse(&parser, 2, argv, 1));
        TEST_ASSERT_NOT_NULL(strstr(cli_opt_error(&parser), "Unknown option '--nonexistent-option'"));
        cli_opt_free(&parser);
    }

    /* 2. Unknown short option */
    {
        cli_opt_parser_t parser;
        cli_opt_init(&parser, specs, 2, "test-cmd", NULL, NULL);
        char *argv[] = {"test-cmd", "-z"};
        TEST_ASSERT_EQUAL_INT(1, cli_opt_parse(&parser, 2, argv, 1));
        TEST_ASSERT_NOT_NULL(strstr(cli_opt_error(&parser), "Unknown option '-z'"));
        cli_opt_free(&parser);
    }

    /* 3. Missing argument at end of argv */
    {
        cli_opt_parser_t parser;
        cli_opt_init(&parser, specs, 2, "test-cmd", NULL, NULL);
        char *argv[] = {"test-cmd", "--bins"};
        TEST_ASSERT_EQUAL_INT(1, cli_opt_parse(&parser, 2, argv, 1));
        TEST_ASSERT_NOT_NULL(strstr(cli_opt_error(&parser), "requires an argument"));
        cli_opt_free(&parser);
    }

    /* 4. Invalid integer value */
    {
        cli_opt_parser_t parser;
        cli_opt_init(&parser, specs, 2, "test-cmd", NULL, NULL);
        char *argv[] = {"test-cmd", "--bins=abc"};
        TEST_ASSERT_EQUAL_INT(1, cli_opt_parse(&parser, 2, argv, 1));
        TEST_ASSERT_NOT_NULL(strstr(cli_opt_error(&parser), "Invalid unsigned integer"));
        cli_opt_free(&parser);
    }

    /* 5. Custom callback error */
    {
        cli_opt_parser_t parser;
        cli_opt_init(&parser, specs, 2, "test-cmd", NULL, NULL);
        char *argv[] = {"test-cmd", "-n50", "--range=invalid_range"};
        TEST_ASSERT_EQUAL_INT(1, cli_opt_parse(&parser, 3, argv, 1));
        TEST_ASSERT_NOT_NULL(strstr(cli_opt_error(&parser), "MIN:MAX"));
        cli_opt_free(&parser);
    }

    /* 6. Required option missing */
    {
        cli_opt_parser_t parser;
        cli_opt_init(&parser, specs, 2, "test-cmd", NULL, NULL);
        char *argv[] = {"test-cmd", "data.txt"};
        TEST_ASSERT_EQUAL_INT(1, cli_opt_parse(&parser, 2, argv, 1));
        TEST_ASSERT_NOT_NULL(strstr(cli_opt_error(&parser), "Option '--bins' is required"));
        cli_opt_free(&parser);
    }
}

void test_cli_opt_help_detection_and_printing(void) {
    bool flag = false;
    cli_opt_spec_t specs[] = {
        {'a', "all", NULL, CLI_OPT_TYPE_BOOL, &flag, NULL, 0, NULL, "Include all metrics", NULL}
    };

    cli_opt_parser_t parser;
    cli_opt_init(&parser, specs, 1, "histo-test", "[OPTIONS] [FILE...]", "Test command summary.");

    char *argv_help[] = {"histo-test", "--help"};
    TEST_ASSERT_EQUAL_INT(-1, cli_opt_parse(&parser, 2, argv_help, 1));
    TEST_ASSERT_TRUE(parser.help_requested);

    char *argv_h[] = {"histo-test", "-h"};
    TEST_ASSERT_EQUAL_INT(-1, cli_opt_parse(&parser, 2, argv_h, 1));
    TEST_ASSERT_TRUE(parser.help_requested);

    /* Test printing help to devnull or tmpfile */
    FILE *tmp = tmpfile();
    TEST_ASSERT_NOT_NULL(tmp);
    cli_opt_print_help(&parser, tmp);
    rewind(tmp);
    char buf[1024];
    size_t r = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[r] = '\0';
    fclose(tmp);

    TEST_ASSERT_NOT_NULL(strstr(buf, "Usage: histo-test [OPTIONS] [FILE...]"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Test command summary."));
    TEST_ASSERT_NOT_NULL(strstr(buf, "-a, --all"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "-h, --help"));

    cli_opt_free(&parser);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cli_opt_flags_and_bundling);
    RUN_TEST(test_cli_opt_short_attached_and_detached);
    RUN_TEST(test_cli_opt_long_options_and_aliases);
    RUN_TEST(test_cli_opt_boolean_negation);
    RUN_TEST(test_cli_opt_negative_numbers_and_positionals);
    RUN_TEST(test_cli_opt_double_dash_terminator);
    RUN_TEST(test_cli_opt_interspersed_positionals);
    RUN_TEST(test_cli_opt_custom_callback_and_types);
    RUN_TEST(test_cli_opt_error_handling);
    RUN_TEST(test_cli_opt_help_detection_and_printing);
    return UNITY_END();
}
