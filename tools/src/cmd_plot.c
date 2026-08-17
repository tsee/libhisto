#include "cli_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <ctype.h>

static void print_plot_usage(void) {
    printf("Usage: histo-plot [OPTIONS] [HISTOGRAM_FILE...]\n");
    printf("       histo plot [OPTIONS] [HISTOGRAM_FILE...]\n\n");
    printf("Renders 1D histograms as beautiful ASCII / Unicode terminal charts.\n\n");
    printf("Display Options:\n");
    printf("  -W, --width=<COLS>       Plot width in characters (default: auto terminal width)\n");
    printf("  -s, --style=<STYLE>      Glyph style: blocks (default), ascii, shaded\n");
    printf("  -c, --color=<MODE>       Color mode: auto (default), always, never\n");
    printf("  -l, --log                Use logarithmic scale for bar lengths\n");
    printf("  -e, --errors             Display error bars (when sum_w2 is tracked)\n");
    printf("      --stats              Show full statistical summary header/footer (default: ON)\n");
    printf("      --no-stats           Suppress statistical summary header\n");
    printf("      --title=<TITLE>      Set custom plot title\n\n");
    printf("Live Streaming / Watch Mode:\n");
    printf("  -w, --watch              Continuously render incoming snapshots from stream\n");
    printf("      --clear              Clear entire screen between updates\n");
    printf("  -h, --help               Show this help message\n");
}

/* Unicode horizontal sub-bin fractions (1/8ths) */
static const char *const UNICODE_BLOCKS[9] = {
    " ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█"
};

static void get_ansi_gradient_color(double fraction, char *out_ansi, size_t max_len) {
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    /* Smooth 24-bit TrueColor gradient: Blue -> Cyan -> Green -> Yellow -> Red */
    int r = 0, g = 0, b = 0;
    if (fraction < 0.25) {
        double t = fraction / 0.25;
        r = 0;
        g = (int)(t * 200.0);
        b = 255;
    } else if (fraction < 0.5) {
        double t = (fraction - 0.25) / 0.25;
        r = 0;
        g = 200 + (int)(t * 55.0);
        b = (int)((1.0 - t) * 255.0);
    } else if (fraction < 0.75) {
        double t = (fraction - 0.5) / 0.25;
        r = (int)(t * 255.0);
        g = 255;
        b = 0;
    } else {
        double t = (fraction - 0.75) / 0.25;
        r = 255;
        g = (int)((1.0 - t) * 255.0);
        b = 0;
    }
    snprintf(out_ansi, max_len, "\033[38;2;%d;%d;%dm", r, g, b);
}

static void render_histogram_console(const histo_t *h, int term_width, const char *style,
                                     bool use_color, bool log_scale, bool show_errors,
                                     bool show_stats, const char *title) {
    if (!h) return;

    uint32_t nbins = histo_nbins(h);
    double total_w = histo_total_weight(h);
    uint64_t n_fills = histo_num_entries(h);

    /* Find max bin content for scaling */
    double max_content = 0.0;
    int max_bounds_len = 0;
    int max_count_len = 0;

    for (uint32_t i = 0; i < nbins; ++i) {
        double lower = 0.0, upper = 0.0, content = 0.0;
        histo_bin_bounds(h, i, &lower, &upper);
        histo_bin_content(h, i, &content);
        if (content > max_content) max_content = content;

        char b_tmp[64];
        int blen = snprintf(b_tmp, sizeof(b_tmp), "[%6.2f, %6.2f)", lower, upper);
        if (blen > max_bounds_len) max_bounds_len = blen;

        char c_tmp[64];
        int clen;
        if (content == floor(content) && content >= 0.0 && content < 1e12) {
            clen = snprintf(c_tmp, sizeof(c_tmp), "%.0f", content);
        } else {
            clen = snprintf(c_tmp, sizeof(c_tmp), "%.4g", content);
        }
        if (clen > max_count_len) max_count_len = clen;
    }

    if (max_bounds_len < 16) max_bounds_len = 16;
    if (max_count_len < 6) max_count_len = 6;

    /* Print Title & Header */
    if (title && *title) {
        char tot_str[32];
        snprintf(tot_str, sizeof(tot_str), (total_w == floor(total_w) && total_w >= 0.0 && total_w < 1e12) ? "%.0f" : "%.4g", total_w);
        printf("\033[1m%s\033[0m (Entries: %llu, Total Weight: %s)\n", title, (unsigned long long)n_fills, tot_str);
    }

    if (show_stats && total_w > 0.0) {
        double mean = 0.0, sdev = 0.0, med = 0.0, iqr = 0.0, mode = 0.0;
        histo_mean(h, &mean);
        histo_std_dev(h, &sdev);
        histo_median(h, &med);
        histo_iqr(h, &iqr);
        histo_mode_continuous(h, &mode);

        char s_mean[32], s_sdev[32], s_med[32], s_iqr[32], s_mode[32];
        snprintf(s_mean, sizeof(s_mean), "Mean: %-7.4g", mean);
        snprintf(s_sdev, sizeof(s_sdev), "StdDev: %-7.4g", sdev);
        snprintf(s_med, sizeof(s_med), "Median: %-7.4g", med);
        snprintf(s_iqr, sizeof(s_iqr), "IQR: %-7.4g", iqr);
        snprintf(s_mode, sizeof(s_mode), "Mode: %-7.4g", mode);

        /* Inner content visual width:
         * 1 (space) + 13 (s_mean) + 3 (" │ ") + 15 (s_sdev) + 3 (" │ ") +
         * 15 (s_med) + 3 (" │ ") + 12 (s_iqr) + 3 (" │ ") + 13 (s_mode) + 1 (space) = 82 */
        int inner_width = 82;

        if (strcmp(style, "ascii") == 0) {
            printf("+-- Statistics ");
            for (int k = 0; k < inner_width - 14; ++k) putchar('-');
            printf("+\n");

            printf("| %-13s | %-15s | %-15s | %-12s | %-13s |\n",
                   s_mean, s_sdev, s_med, s_iqr, s_mode);

            printf("+");
            for (int k = 0; k < inner_width; ++k) putchar('-');
            printf("+\n");
        } else {
            printf("┌─ Statistics ");
            for (int k = 0; k < inner_width - 13; ++k) printf("─");
            printf("┐\n");

            printf("│ %-13s │ %-15s │ %-15s │ %-12s │ %-13s │\n",
                   s_mean, s_sdev, s_med, s_iqr, s_mode);

            printf("└");
            for (int k = 0; k < inner_width; ++k) printf("─");
            printf("┘\n");
        }
    }

    /* Calculate column widths for bounds and counts */
    int prefix_width = max_bounds_len + max_count_len + 6; /* "%-*s │ %*s │ " */
    int bar_max_chars = term_width - prefix_width - 2;
    if (bar_max_chars < 10) bar_max_chars = 10;

    double max_scaled = max_content;
    if (log_scale && max_content > 0.0) {
        max_scaled = log10(max_content + 1.0);
    }

    /* Render each bin */
    for (uint32_t i = 0; i < nbins; ++i) {
        double lower = 0.0, upper = 0.0, content = 0.0, err = 0.0;
        histo_bin_bounds(h, i, &lower, &upper);
        histo_bin_content(h, i, &content);
        if (show_errors) {
            histo_bin_error(h, i, &err);
        }

        /* Bin bounds and count formatting */
        char bounds_str[64];
        snprintf(bounds_str, sizeof(bounds_str), "[%6.2f, %6.2f)", lower, upper);

        char count_str[64];
        if (content == floor(content) && content >= 0.0 && content < 1e12) {
            snprintf(count_str, sizeof(count_str), "%.0f", content);
        } else {
            snprintf(count_str, sizeof(count_str), "%.4g", content);
        }

        printf("%-*s │ %*s │ ", max_bounds_len, bounds_str, max_count_len, count_str);

        if (max_scaled <= 0.0 || content <= 0.0) {
            printf("\n");
            continue;
        }

        double val_scaled = log_scale ? log10(content + 1.0) : content;
        double frac_total = val_scaled / max_scaled;
        double bar_len_exact = frac_total * (double)bar_max_chars;

        char color_ansi[32] = "";
        if (use_color) {
            get_ansi_gradient_color(frac_total, color_ansi, sizeof(color_ansi));
            printf("%s", color_ansi);
        }

        if (strcmp(style, "ascii") == 0) {
            int full_chars = (int)bar_len_exact;
            for (int k = 0; k < full_chars; ++k) {
                putchar('#');
            }
            if (show_errors && err > 0.0 && full_chars < bar_max_chars) {
                printf(" ±%.2g", err);
            }
        } else if (strcmp(style, "shaded") == 0) {
            int full_chars = (int)bar_len_exact;
            for (int k = 0; k < full_chars; ++k) {
                printf("█");
            }
        } else { /* blocks with 1/8th sub-character precision */
            int full_chars = (int)bar_len_exact;
            int remainder_eighths = (int)((bar_len_exact - (double)full_chars) * 8.0);
            if (remainder_eighths < 0) remainder_eighths = 0;
            if (remainder_eighths > 8) remainder_eighths = 8;

            for (int k = 0; k < full_chars; ++k) {
                printf("█");
            }
            if (remainder_eighths > 0 && full_chars < bar_max_chars) {
                printf("%s", UNICODE_BLOCKS[remainder_eighths]);
            }
            if (show_errors && err > 0.0) {
                if (use_color) printf("\033[0m\033[90m");
                printf(" ╎±%.2g╎", err);
            }
        }

        if (use_color) {
            printf("\033[0m");
        }
        printf("\n");
    }

    /* Print out-of-range footer */
    double uflow = histo_underflow(h);
    double oflow = histo_overflow(h);
    uint64_t n_nan = histo_nan_count(h);

    char uflow_str[32], oflow_str[32], tot_str[32];
    snprintf(uflow_str, sizeof(uflow_str), (uflow == floor(uflow) && uflow >= 0.0 && uflow < 1e12) ? "%.0f" : "%.4g", uflow);
    snprintf(oflow_str, sizeof(oflow_str), (oflow == floor(oflow) && oflow >= 0.0 && oflow < 1e12) ? "%.0f" : "%.4g", oflow);
    snprintf(tot_str, sizeof(tot_str), (total_w == floor(total_w) && total_w >= 0.0 && total_w < 1e12) ? "%.0f" : "%.4g", total_w);

    printf(" Underflow: %s │ In-Range: %s │ Overflow: %s │ Non-Finite/NaN: %llu\n",
           uflow_str, tot_str, oflow_str, (unsigned long long)n_nan);
}

int cmd_plot_main(int argc, char **argv) {
    int term_width = cli_get_terminal_width(80);
    const char *style = "blocks";
    const char *color_mode = "auto";
    bool log_scale = false;
    bool show_errors = false;
    bool show_stats = true;
    bool watch_mode = false;
    bool clear_screen = false;
    const char *title = NULL;

    int file_start = argc;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_plot_usage();
            return 0;
        } else if (strncmp(arg, "-W=", 3) == 0) {
            term_width = atoi(arg + 3);
        } else if (strcmp(arg, "-W") == 0 && i + 1 < argc) {
            term_width = atoi(argv[++i]);
        } else if (strncmp(arg, "--width=", 8) == 0) {
            term_width = atoi(arg + 8);
        } else if (strncmp(arg, "-s=", 3) == 0) {
            style = arg + 3;
        } else if (strcmp(arg, "-s") == 0 && i + 1 < argc) {
            style = argv[++i];
        } else if (strncmp(arg, "--style=", 8) == 0) {
            style = arg + 8;
        } else if (strncmp(arg, "-c=", 3) == 0) {
            color_mode = arg + 3;
        } else if (strcmp(arg, "-c") == 0 && i + 1 < argc) {
            color_mode = argv[++i];
        } else if (strncmp(arg, "--color=", 8) == 0) {
            color_mode = arg + 8;
        } else if (strcmp(arg, "-l") == 0 || strcmp(arg, "--log") == 0) {
            log_scale = true;
        } else if (strcmp(arg, "-e") == 0 || strcmp(arg, "--errors") == 0) {
            show_errors = true;
        } else if (strcmp(arg, "--stats") == 0) {
            show_stats = true;
        } else if (strcmp(arg, "--no-stats") == 0) {
            show_stats = false;
        } else if (strcmp(arg, "-w") == 0 || strcmp(arg, "--watch") == 0) {
            watch_mode = true;
        } else if (strcmp(arg, "--clear") == 0) {
            clear_screen = true;
        } else if (strncmp(arg, "--title=", 8) == 0) {
            title = arg + 8;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "Unknown option '%s'. Run 'histo-plot --help' for usage.\n", arg);
            return 1;
        } else {
            file_start = i;
            break;
        }
    }

    bool use_color = false;
    if (strcmp(color_mode, "always") == 0) {
        use_color = true;
    } else if (strcmp(color_mode, "never") == 0) {
        use_color = false;
    } else {
        use_color = cli_is_stdout_tty();
    }

    int num_files = argc - file_start;
    const char *default_files[] = {"-"};
    const char **files = (num_files > 0) ? (const char **)(argv + file_start) : default_files;
    int nfiles = (num_files > 0) ? num_files : 1;

    for (int f = 0; f < nfiles; ++f) {
        FILE *in_fp = NULL;
        if (strcmp(files[f], "-") == 0) {
            in_fp = stdin;
        } else {
            in_fp = fopen(files[f], "rb");
            if (!in_fp) {
                fprintf(stderr, "Error: Cannot open file '%s'\n", files[f]);
                continue;
            }
        }

        cli_input_format_t fmt = cli_detect_stream_format(in_fp);
        if (fmt == CLI_INPUT_BINARY_HISTO || fmt == CLI_INPUT_JSON_HISTO) {
            histo_t *h = NULL;
            if (watch_mode) {
                while (cli_read_histogram_from_stream(in_fp, &h) == HISTO_OK) {
                    if (clear_screen) {
                        printf("\033[2J\033[H");
                    } else {
                        printf("\033[H");
                    }
                    render_histogram_console(h, term_width, style, use_color, log_scale, show_errors, show_stats, title);
                    histo_destroy(h);
                    h = NULL;
                }
            } else {
                if (cli_read_histogram_from_stream(in_fp, &h) == HISTO_OK) {
                    render_histogram_console(h, term_width, style, use_color, log_scale, show_errors, show_stats, title);
                    histo_destroy(h);
                } else {
                    fprintf(stderr, "Error: Failed to deserialize histogram from '%s'\n", files[f]);
                }
            }
        } else {
            /* Raw numbers / text: ingest into auto-ranged histogram on the fly */
            char line[2048];
            size_t count = 0, cap = 1024;
            double *samples = (double *)malloc(cap * sizeof(double));
            while (fgets(line, sizeof(line), in_fp)) {
                char *p = line;
                while (*p && isspace((unsigned char)*p)) p++;
                if (*p == '\0' || *p == '#') continue;
                char *endp = NULL;
                double val = strtod(p, &endp);
                if (endp != p) {
                    if (count >= cap) {
                        cap *= 2;
                        samples = (double *)realloc(samples, cap * sizeof(double));
                    }
                    samples[count++] = val;
                }
            }

            if (count > 0) {
                double rmin = samples[0], rmax = samples[0];
                for (size_t i = 1; i < count; ++i) {
                    if (samples[i] < rmin) rmin = samples[i];
                    if (samples[i] > rmax) rmax = samples[i];
                }
                if (fabs(rmax - rmin) < 1e-12) {
                    rmin -= 1.0;
                    rmax += 1.0;
                } else {
                    rmax += (rmax - rmin) * 1e-6;
                }
                histo_t *h = histo_create_uniform(20, rmin, rmax, HISTO_FLAG_TRACK_SUMW2);
                if (h) {
                    histo_fill_n(h, count, samples, NULL);
                    render_histogram_console(h, term_width, style, use_color, log_scale, show_errors, show_stats, title);
                    histo_destroy(h);
                }
            }
            free(samples);
        }

        if (in_fp != stdin) fclose(in_fp);
    }

    return 0;
}
