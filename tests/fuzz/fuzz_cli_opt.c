/*
 * LibFuzzer target for table-driven CLI option parsing and tokenization.
 */

#include "cli_opt.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_FUZZ_ARGS 64

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size == 0 || size > 32768) {
        return 0;
    }

    /* Tokenize input byte stream into argv array */
    char *buf = (char *)malloc(size + 1);
    if (!buf) return 0;
    memcpy(buf, data, size);
    buf[size] = '\0';

    char *argv[MAX_FUZZ_ARGS + 1];
    argv[0] = "fuzz_cli_opt";
    int argc = 1;

    char *p = buf;
    while (*p && argc < MAX_FUZZ_ARGS) {
        while (*p && (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r')) {
            p++;
        }
        if (!*p) break;

        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\n' && *p != '\t' && *p != '\r') {
            p++;
        }
        if (*p) {
            *p++ = '\0';
        }
    }
    argv[argc] = NULL;

    /* Define comprehensive CLI option specification using exact struct layout */
    int opt_bins = 50;
    double opt_min = 0.0;
    double opt_max = 100.0;
    bool opt_verbose = false;
    bool opt_exact = false;
    const char *opt_format = NULL;

    const cli_opt_spec_t specs[] = {
        { 'b', "bins",    NULL, CLI_OPT_TYPE_INT,    &opt_bins,    NULL, CLI_OPT_FLAG_NONE, "N",    "Bin count",            "50" },
        { 0,   "min",     NULL, CLI_OPT_TYPE_DOUBLE, &opt_min,     NULL, CLI_OPT_FLAG_NONE, "VAL",  "Minimum range",        "0.0" },
        { 0,   "max",     NULL, CLI_OPT_TYPE_DOUBLE, &opt_max,     NULL, CLI_OPT_FLAG_NONE, "VAL",  "Maximum range",        "100.0" },
        { 'v', "verbose", NULL, CLI_OPT_TYPE_BOOL,   &opt_verbose, NULL, CLI_OPT_FLAG_NONE, NULL,   "Verbose output",       NULL },
        { 'e', "exact",   NULL, CLI_OPT_TYPE_BOOL,   &opt_exact,   NULL, CLI_OPT_FLAG_NONE, NULL,   "Exact moments",        NULL },
        { 'f', "format",  NULL, CLI_OPT_TYPE_STRING, &opt_format,  NULL, CLI_OPT_FLAG_NONE, "FMT",  "Serialization format", "binary" },
    };
    size_t num_specs = sizeof(specs) / sizeof(specs[0]);

    cli_opt_parser_t parser;
    cli_opt_init(&parser, specs, num_specs, "fuzz_cli_opt", "[OPTIONS]", "Fuzz CLI options");

    (void)cli_opt_parse(&parser, argc, argv, 1);

    (void)opt_bins;
    (void)opt_min;
    (void)opt_max;
    (void)opt_verbose;
    (void)opt_exact;
    (void)opt_format;
    (void)cli_opt_error(&parser);

    cli_opt_free(&parser);
    free(buf);
    return 0;
}
