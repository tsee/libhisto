/*
 * Declarative, table-driven command-line argument parser for libhisto CLI.
 * Strict ISO C99, zero external dependencies, thread-safe, and portable across GCC/Clang/MSVC.
 */

#ifndef HISTO_CLI_OPT_H
#define HISTO_CLI_OPT_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if defined(_WIN32)
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CLI_OPT_TYPE_BOOL,          /* Boolean flag: target is bool* */
    CLI_OPT_TYPE_STRING,        /* String value: target is const char** */
    CLI_OPT_TYPE_INT,           /* Signed integer: target is int* */
    CLI_OPT_TYPE_UINT32,        /* Unsigned 32-bit integer: target is uint32_t* */
    CLI_OPT_TYPE_INT64,         /* Signed 64-bit integer: target is int64_t* */
    CLI_OPT_TYPE_UINT64,        /* Unsigned 64-bit integer: target is uint64_t* */
    CLI_OPT_TYPE_DOUBLE,        /* Double-precision float: target is double* */
    CLI_OPT_TYPE_CHAR,          /* Single character: target is char* */
    CLI_OPT_TYPE_CALLBACK       /* Custom callback parser */
} cli_opt_type_t;

/* Option modifier flags */
enum {
    CLI_OPT_FLAG_NONE         = 0,
    CLI_OPT_FLAG_REQUIRED     = (1 << 0), /* Error if option is not provided */
    CLI_OPT_FLAG_SET_TRUE     = (1 << 1), /* For BOOL: sets *target = true (default behavior) */
    CLI_OPT_FLAG_SET_FALSE    = (1 << 2), /* For BOOL: sets *target = false (e.g. --no-stats) */
    CLI_OPT_FLAG_HIDDEN       = (1 << 3), /* Do not display in generated help output */
    CLI_OPT_FLAG_OPTIONAL_ARG = (1 << 4), /* Value is optional (e.g. --auto-bins[=RULE]) */
    CLI_OPT_FLAG_WAS_SET      = (1 << 5)  /* Internal: tracks if option was specified */
};

/* Custom parser callback signature: returns 0 on success, non-zero on error */
typedef int (*cli_opt_cb_t)(const char *opt_name, const char *val, void *data, char *err_buf, size_t err_size);

typedef struct {
    char short_name;            /* e.g. 'n' for -n, or 0 if none */
    const char *long_name;      /* e.g. "bins" for --bins, or NULL */
    const char *alias;          /* e.g. "colormap" alias for "palette", or NULL */
    cli_opt_type_t type;        /* Value type */
    void *target;               /* Pointer to target variable, or data for callback */
    cli_opt_cb_t callback;      /* Callback function if type == CLI_OPT_TYPE_CALLBACK */
    int flags;                  /* Bitwise OR of CLI_OPT_FLAG_* */
    const char *val_name;       /* Meta-variable name for help, e.g. "N", "FILE", "FMT" */
    const char *description;    /* Help description string */
    const char *def_val_desc;   /* Optional default value text for help, e.g. "50", "table" */
} cli_opt_spec_t;

/* Option parser state context */
typedef struct {
    const cli_opt_spec_t *specs;
    size_t num_specs;
    const char *prog_name;      /* e.g. "histo-fill" or "histo fill" */
    const char *usage_args;     /* e.g. "[OPTIONS] [FILE...]" */
    const char *summary;        /* Command description / summary */

    const char **positionals;   /* Collected non-option arguments */
    int num_positionals;
    int positionals_cap;

    char err_msg[256];          /* Error description buffer */
    bool has_error;
    bool help_requested;
} cli_opt_parser_t;

/* Initialize parser context */
void cli_opt_init(cli_opt_parser_t *parser,
                  const cli_opt_spec_t *specs,
                  size_t num_specs,
                  const char *prog_name,
                  const char *usage_args,
                  const char *summary);

/* Parse argv arguments. Returns 0 on success, 1 on parse error, -1 if --help or -h was requested */
int cli_opt_parse(cli_opt_parser_t *parser, int argc, char **argv, int start_idx);

/* Print auto-generated help message to out */
void cli_opt_print_help(const cli_opt_parser_t *parser, FILE *out);

/* Get error message if cli_opt_parse returned 1 */
const char *cli_opt_error(const cli_opt_parser_t *parser);

/* Free internal allocations */
void cli_opt_free(cli_opt_parser_t *parser);

#ifdef __cplusplus
}
#endif

#endif /* HISTO_CLI_OPT_H */
